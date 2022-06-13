/*
 *   IRC - Internet Relay Chat, src/modules/sajoin.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(cmd_sajoin);

#define MSG_SAJOIN 	"SAJOIN"	

ModuleHeader MOD_HEADER
  = {
	"sajoin",
	"5.0",
	"command /sajoin", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SAJOIN, cmd_sajoin, MAXPARA, CMD_USER|CMD_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
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

static void log_sajoin(Client *client, Client *target, const char *channels)
{
	unreal_log(ULOG_INFO, "sacmds", "SAJOIN_COMMAND", client, "SAJOIN: $client used SAJOIN to make $target join $channels",
		   log_data_client("target", target),
		   log_data_string("channels", channels));
}

/* cmd_sajoin() - Lamego - Wed Jul 21 20:04:48 1999
   Copied off PTlink IRCd (C) PTlink coders team.
   Coded for Sadmin by Stskeeps
   also Modified by NiQuiL (niquil@programmer.net)
	parv[1] - nick to make join
	parv[2] - channel(s) to join
*/
CMD_FUNC(cmd_sajoin)
{
	Client *target;
	char request[BUFSIZE];
	char jbuf[BUFSIZE];
	int did_anything = 0;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("SAJOIN");

	if (parc < 3)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SAJOIN");
		return;
	}

	if (!(target = find_user(parv[1], NULL)))
	{
		sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
		return;
	}

	/* Is this user disallowed from operating on this victim at all? */
	if (!IsULine(client) && !ValidatePermissionsForPath("sacmd:sajoin",client,target,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	/* Broadcast so other servers can log it appropriately as an SAJOIN */
	sendto_server(client, 0, 0, recv_mtags, ":%s SAJOIN %s %s", client->id, target->id, parv[2]);

	/* If it's not for our client, then simply pass on the message... */
	if (!MyUser(target))
	{
		log_sajoin(client, target, parv[2]);
		return;
	}

	/* 'target' is our client... */

	/* Can't this just use do_join() or something with a parameter to bypass some checks?
	 * This duplicate code is damn ugly. Ah well..
	 */
	{
		char *name, *p = NULL;
		int parted = 0;
	
		*jbuf = 0;

		/* Now works like cmd_join */
		strlcpy(request, parv[2], sizeof(request));
		for (name = strtoken(&p, request, ","); name; name = strtoken(&p, NULL, ","))
		{
			Channel *channel;
			Membership *lp;
			char mode = '\0';
			char prefix = '\0';

			if (++ntargets > maxtargets)
			{
				sendnumeric(client, ERR_TOOMANYTARGETS, name, maxtargets, "SAJOIN");
				break;
			}

			mode = prefix_to_mode(*name);
			if (mode)
			{
				prefix = *name;
				name++; /* skip the prefix */
			}

			if (strlen(name) > CHANNELLEN)
			{
				sendnotice(client, "Channel name too long: %s", name);
				continue;
			}

			if (*name == '0' && !atoi(name) && !mode)
			{
				strlcpy(jbuf, "0", sizeof(jbuf));
				parted = 1;
				continue;
			}

			if (!valid_channelname(name))
			{
				send_invalid_channelname(client, name);
				continue;
			}

			channel = make_channel(name);

			/* If this _specific_ channel is not permitted, skip it */
			if (!IsULine(client) && !ValidatePermissionsForPath("sacmd:sajoin",client,target,channel,NULL))
			{
				sendnumeric(client, ERR_NOPRIVILEGES);
				continue;
			}

			if (!parted && channel && (lp = find_membership_link(target->user->channel, channel)))
			{
				sendnumeric(client, ERR_USERONCHANNEL, name, target->name);
				continue;
			}
			if (*jbuf)
				strlcat(jbuf, ",", sizeof(jbuf));
			if (prefix)
				strlcat_letter(jbuf, prefix, sizeof(jbuf));
			strlcat(jbuf, name, sizeof(jbuf));
		}
		if (!*jbuf)
			return;

		strlcpy(request, jbuf, sizeof(request));
		*jbuf = 0;
		for (name = strtoken(&p, request, ","); name; name = strtoken(&p, NULL, ","))
		{
			MessageTag *mtags = NULL;
			const char *member_modes;
			Channel *channel;
			Membership *lp;
			Hook *h;
			int i = 0;
			char mode = '\0';
			char prefix = '\0';

			mode = prefix_to_mode(*name);
			if (mode != '\0')
			{
				/* Yup, it was a real prefix. */
				prefix = *name;
				name++;
			}

			if (*name == '0' && !atoi(name) && !mode)
			{
				/* Rewritten so to generate a PART for each channel to servers,
				 * so the same msgid is used for each part on all servers. -- Syzop
				 */
				did_anything = 1;
				while ((lp = target->user->channel))
				{
					MessageTag *mtags = NULL;
					channel = lp->channel;

					new_message(target, NULL, &mtags);
					sendto_channel(channel, target, NULL, 0, 0, SEND_LOCAL, mtags,
					               ":%s PART %s :%s",
					               target->name, channel->name, "Left all channels");
					sendto_server(NULL, 0, 0, mtags, ":%s PART %s :Left all channels", target->name, channel->name);
					if (MyConnect(target))
						RunHook(HOOKTYPE_LOCAL_PART, target, channel, mtags, "Left all channels");
					free_message_tags(mtags);
					remove_user_from_channel(target, channel, 0);
				}
				strlcpy(jbuf, "0", sizeof(jbuf));
				continue;
			}
			member_modes = (ChannelExists(name)) ? "" : LEVEL_ON_JOIN;
			channel = make_channel(name);
			if (channel && (lp = find_membership_link(target->user->channel, channel)))
				continue;

			i = HOOK_CONTINUE;
			for (h = Hooks[HOOKTYPE_CAN_SAJOIN]; h; h = h->next)
			{
				i = (*(h->func.intfunc))(target,channel,client);
				if (i != HOOK_CONTINUE)
					break;
			}

			if (i == HOOK_DENY)
				continue; /* process next channel */

			/* Generate a new message without inheritance.
			 * We can do this because we are the server that
			 * will send a JOIN for each channel due to this loop.
			 * Each with their own unique msgid.
			 */
			new_message(target, NULL, &mtags);
			join_channel(channel, target, mtags, member_modes);
			if (prefix)
			{
				char *modes;
				const char *mode_args[3];

				opermode = 0;
				sajoinmode = 1;

				modes = safe_alloc(2);
				modes[0] = mode;

				mode_args[0] = modes;
				mode_args[1] = target->name;
				mode_args[2] = 0;

				do_mode(channel, target, NULL, 3, mode_args, 0, 1);

				sajoinmode = 0;
				safe_free(modes);
			}
			free_message_tags(mtags);
			did_anything = 1;
			if (*jbuf)
				strlcat(jbuf, ",", sizeof jbuf);
			strlcat(jbuf, name, sizeof jbuf);
		}
		
		if (did_anything)
		{
			sendnotice(target, "*** You were forced to join %s", jbuf);
			log_sajoin(client, target, jbuf);
		}
	}
}
