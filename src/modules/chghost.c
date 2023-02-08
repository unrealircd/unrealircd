/*
 *   IRC - Internet Relay Chat, src/modules/chghost.c
 *   (C) 1999-2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

#define MSG_CHGHOST 	"CHGHOST"

CMD_FUNC(cmd_chghost);
void _userhost_save_current(Client *client);
void _userhost_changed(Client *client);

long CAP_CHGHOST = 0L;

ModuleHeader MOD_HEADER
  = {
	"chghost",	/* Name of module */
	"5.0", /* Version */
	"/chghost", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_USERHOST_SAVE_CURRENT, _userhost_save_current);
	EfunctionAddVoid(modinfo->handle, EFUNC_USERHOST_CHANGED, _userhost_changed);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ClientCapabilityInfo c;

	CommandAdd(modinfo->handle, MSG_CHGHOST, cmd_chghost, MAXPARA, CMD_USER|CMD_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&c, 0, sizeof(c));
	c.name = "chghost";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_CHGHOST);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
	
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;	
}


static char remember_nick[NICKLEN+1];
static char remember_user[USERLEN+1];
static char remember_host[HOSTLEN+1];

/** Save current nick/user/host. Used later by userhost_changed(). */
void _userhost_save_current(Client *client)
{
	strlcpy(remember_nick, client->name, sizeof(remember_nick));
	strlcpy(remember_user, client->user->username, sizeof(remember_user));
	strlcpy(remember_host, GetHost(client), sizeof(remember_host));
}

/** User/Host changed for user.
 * Note that userhost_save_current() needs to be called before this
 * to save the old username/hostname.
 * This userhost_changed() function deals with notifying local clients
 * about the user/host change by sending PART+JOIN+MODE if
 * set::allow-userhost-change force-rejoin is in use,
 * and it wills end "CAP chghost" to such capable clients.
 * It will also deal with bumping fakelag for the user since a user/host
 * change is costly, doesn't matter if it was self-induced or not.
 *
 * Please call this function for any user/host change by doing:
 * userhost_save_current(client);
 * << change username or hostname here >>
 * userhost_changed(client);
 */
void _userhost_changed(Client *client)
{
	Membership *channels;
	Member *lp;
	Client *acptr;
	int impact = 0;
	char buf[512];
	long CAP_EXTENDED_JOIN = ClientCapabilityBit("extended-join");

	if (strcmp(remember_nick, client->name))
	{
		unreal_log(ULOG_ERROR, "main", "BUG_USERHOST_CHANGED", client,
		           "[BUG] userhost_changed() was called but without calling userhost_save_current() first! Affected user: $client\n"
		           "Please report above bug on https://bugs.unrealircd.org/");
		return; /* We cannot safely process this request anymore */
	}

	/* It's perfectly acceptable to call us even if the userhost didn't change. */
	if (!strcmp(remember_user, client->user->username) && !strcmp(remember_host, GetHost(client)))
		return; /* Nothing to do */

	/* Most of the work is only necessary for set::allow-userhost-change force-rejoin */
	if (UHOST_ALLOWED == UHALLOW_REJOIN)
	{
		/* Walk through all channels of this user.. */
		for (channels = client->user->channel; channels; channels = channels->next)
		{
			Channel *channel = channels->channel;
			char *modes;
			char partbuf[512]; /* PART */
			char joinbuf[512]; /* JOIN */
			char exjoinbuf[512]; /* JOIN (for CAP extended-join) */
			char modebuf[512]; /* MODE (if any) */
			int chanops_only = invisible_user_in_channel(client, channel);

			modebuf[0] = '\0';

			/* If the user is banned, don't send any rejoins, it would only be annoying */
			if (is_banned(client, channel, BANCHK_JOIN, NULL, NULL))
				continue;

			/* Prepare buffers for PART, JOIN, MODE */
			ircsnprintf(partbuf, sizeof(partbuf), ":%s!%s@%s PART %s :%s",
						remember_nick, remember_user, remember_host,
						channel->name,
						"Changing host");

			ircsnprintf(joinbuf, sizeof(joinbuf), ":%s!%s@%s JOIN %s",
						client->name, client->user->username, GetHost(client), channel->name);

			ircsnprintf(exjoinbuf, sizeof(exjoinbuf), ":%s!%s@%s JOIN %s %s :%s",
				client->name, client->user->username, GetHost(client), channel->name,
				IsLoggedIn(client) ? client->user->account : "*",
				client->info);

			modes = get_chmodes_for_user(client, channels->member_modes);
			if (!BadPtr(modes))
				ircsnprintf(modebuf, sizeof(modebuf), ":%s MODE %s %s", me.name, channel->name, modes);

			for (lp = channel->members; lp; lp = lp->next)
			{
				acptr = lp->client;

				if (acptr == client)
					continue; /* skip self */

				if (!MyConnect(acptr))
					continue; /* only locally connected clients */

				if (chanops_only && !check_channel_access_member(lp, "hoaq"))
					continue; /* skip non-ops if requested to (used for mode +D) */

				if (HasCapabilityFast(acptr, CAP_CHGHOST))
					continue; /* we notify 'CAP chghost' users in a different way, so don't send it here. */

				impact++;

				/* FIXME: if a client does not have the "chghost" cap then
				 * here we will not generate a proper new message, probably
				 * needs to be fixed... I skipped doing it for now.
				 */
				sendto_one(acptr, NULL, "%s", partbuf);

				if (HasCapabilityFast(acptr, CAP_EXTENDED_JOIN))
					sendto_one(acptr, NULL, "%s", exjoinbuf);
				else
					sendto_one(acptr, NULL, "%s", joinbuf);

				if (*modebuf)
					sendto_one(acptr, NULL, "%s", modebuf);
			}
		}
	}

	/* Now deal with "CAP chghost" clients.
	 * This only needs to be sent one per "common channel".
	 * This would normally call sendto_common_channels_local_butone() but the user already
	 * has the new user/host.. so we do it here..
	 */
	ircsnprintf(buf, sizeof(buf), ":%s!%s@%s CHGHOST %s %s",
	            remember_nick, remember_user, remember_host,
	            client->user->username,
	            GetHost(client));
	current_serial++;
	for (channels = client->user->channel; channels; channels = channels->next)
	{
		for (lp = channels->channel->members; lp; lp = lp->next)
		{
			acptr = lp->client;
			if (MyUser(acptr) && HasCapabilityFast(acptr, CAP_CHGHOST) &&
			    (acptr->local->serial != current_serial) && (client != acptr))
			{
				/* FIXME: send mtag */
				sendto_one(acptr, NULL, "%s", buf);
				acptr->local->serial = current_serial;
			}
		}
	}
	
	RunHook(HOOKTYPE_USERHOST_CHANGE, client, remember_user, remember_host);

	if (MyUser(client))
	{
		/* We take the liberty of sending the CHGHOST to the impacted user as
		 * well. This makes things easy for client coders.
		 * (Note that this cannot be merged with the for loop from 15 lines up
		 *  since the user may not be in any channels)
		 */
		if (HasCapabilityFast(client, CAP_CHGHOST))
			sendto_one(client, NULL, "%s", buf);

		if (MyUser(client))
			sendnumeric(client, RPL_HOSTHIDDEN, GetHost(client));

		/* A userhost change always generates the following network traffic:
		 * server to server traffic, CAP "chghost" notifications, and
		 * possibly PART+JOIN+MODE if force-rejoin had work to do.
		 * We give the user a penalty so they don't flood...
		 */
		if (impact)
			add_fake_lag(client, 7000); /* Resulted in rejoins and such. */
		else
			add_fake_lag(client, 4000); /* No rejoins */
	}
}

/* 
 * cmd_chghost - 12/07/1999 (two months after I made SETIDENT) - Stskeeps
 * :prefix CHGHOST <nick> <new hostname>
 * parv[1] - target user
 * parv[2] - hostname
 *
*/

CMD_FUNC(cmd_chghost)
{
	Client *target;

	if (MyUser(client) && !ValidatePermissionsForPath("client:set:host",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc < 3) || BadPtr(parv[2]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "CHGHOST");
		return;
	}

	if (strlen(parv[2]) > (HOSTLEN))
	{
		sendnotice(client, "*** ChgName Error: Requested hostname too long -- rejected.");
		return;
	}

	if (!valid_host(parv[2], 0))
	{
		sendnotice(client, "*** /ChgHost Error: A hostname may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them");
		return;
	}

	if (parv[2][0] == ':')
	{
		sendnotice(client, "*** A hostname cannot start with ':'");
		return;
	}

	target = find_client(parv[1], NULL);
	if (!MyUser(client) && !target && (target = find_server_by_uid(parv[1])))
	{
		/* CHGHOST for a UID that is not online.
		 * Let's assume it may not YET be online and forward the message to
		 * the remote server and stop processing ourselves.
		 * That server will then handle pre-registered processing of the
		 * CHGHOST and later communicate the host when the user actually
		 * comes online in the UID message.
		 */
		sendto_one(target, recv_mtags, ":%s CHGHOST %s %s", client->id, parv[1], parv[2]);
		return;
	}

	if (!target || !target->user)
	{
		sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
		return;
	}

	if (!strcmp(GetHost(target), parv[2]))
	{
		sendnotice(client, "*** /ChgHost Error: requested host is same as current host.");
		return;
	}

	userhost_save_current(target);

	switch (UHOST_ALLOWED)
	{
		case UHALLOW_NEVER:
			if (MyUser(client))
			{
				sendnumeric(client, ERR_DISABLED, "CHGHOST",
					"This command is disabled on this server");
				return;
			}
			break;
		case UHALLOW_ALWAYS:
			break;
		case UHALLOW_NOCHANS:
			if (IsUser(target) && MyUser(client) && target->user->joined)
			{
				sendnotice(client, "*** /ChgHost can not be used while %s is on a channel", target->name);
				return;
			}
			break;
		case UHALLOW_REJOIN:
			/* rejoin sent later when the host has been changed */
			break;
	}

	if (!IsULine(client))
	{
		unreal_log(ULOG_INFO, "chgcmds", "CHGHOST_COMMAND", client,
		           "CHGHOST: $client changed the virtual hostname of $target.details to be $new_hostname",
		           log_data_string("change_type", "hostname"),
			   log_data_client("target", target),
		           log_data_string("new_hostname", parv[2]));
	}

	target->umodes |= UMODE_HIDE;
	target->umodes |= UMODE_SETHOST;

	/* Send to other servers too, unless the client is still in the registration phase (SASL) */
	if (IsUser(target))
		sendto_server(client, 0, 0, NULL, ":%s CHGHOST %s %s", client->id, target->id, parv[2]);

	safe_strdup(target->user->virthost, parv[2]);
	userhost_changed(target);
}
