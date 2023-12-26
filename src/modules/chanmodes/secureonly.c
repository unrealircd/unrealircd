/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/secureonly.c
 * Only allow secure users to join UnrealIRCd Module (Channel Mode +z)
 * (C) Copyright 2014 Travis McArthur (Heero) and the UnrealIRCd team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"chanmodes/secureonly",
	"4.2",
	"Channel Mode +z",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_SECUREONLY;

#define IsSecureOnly(channel)    (channel->mode.mode & EXTCMODE_SECUREONLY)

int secureonly_check_join(Client *client, Channel *channel, const char *key, char **errmsg);
int secureonly_channel_sync (Channel *channel, int merge, int removetheirs, int nomode);
int secureonly_check_secure(Channel *channel);
int secureonly_check_sajoin(Client *target, Channel *channel, Client *requester);
int secureonly_pre_local_join(Client *client, Channel *channel, const char *key);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'z';
	req.is_ok = extcmode_default_requirechop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_SECUREONLY);

	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_JOIN, 0, secureonly_pre_local_join);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_JOIN, 0, secureonly_check_join);
	HookAdd(modinfo->handle, HOOKTYPE_CHANNEL_SYNCED, 0, secureonly_channel_sync);
	HookAdd(modinfo->handle, HOOKTYPE_IS_CHANNEL_SECURE, 0, secureonly_check_secure);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SAJOIN, 0, secureonly_check_sajoin);


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


/** Kicks all insecure users on a +z channel
 * Returns 1 if the channel was destroyed as a result of this.
 */
static int secureonly_kick_insecure_users(Channel *channel)
{
	Member *member, *mb2;
	Client *client;
	int i = 0;
	Hook *h;
	char *comment = "Insecure user not allowed on secure channel (+z)";

	if (!IsSecureOnly(channel))
		return 0;

	for (member = channel->members; member; member = mb2)
	{
		mb2 = member->next;
		client = member->client;
		if (MyUser(client) && !IsSecureConnect(client) && !IsULine(client))
		{
			char *prefix = NULL;
			MessageTag *mtags = NULL;

			if (invisible_user_in_channel(client, channel))
			{
				/* Send only to chanops */
				prefix = "ho";
			}

			new_message(&me, NULL, &mtags);

			RunHook(HOOKTYPE_LOCAL_KICK, &me, &me, client, channel, mtags, comment);

			sendto_channel(channel, &me, client,
				       prefix, 0,
				       SEND_LOCAL, mtags,
				       ":%s KICK %s %s :%s",
				       me.name, channel->name, client->name, comment);

			sendto_prefix_one(client, &me, mtags, ":%s KICK %s %s :%s", me.name, channel->name, client->name, comment);

			sendto_server(NULL, 0, 0, mtags, ":%s KICK %s %s :%s", me.id, channel->name, client->id, comment);

			free_message_tags(mtags);

			if (remove_user_from_channel(client, channel, 0) == 1)
				return 1; /* channel was destroyed */
		}
	}
	return 0;
}

int secureonly_check_join(Client *client, Channel *channel, const char *key, char **errmsg)
{
	Link *lp;

	if (IsSecureOnly(channel) && !(client->umodes & UMODE_SECURE))
	{
		if (ValidatePermissionsForPath("channel:override:secureonly",client,NULL,channel,NULL))
		{
			/* if the channel is +z we still allow an ircop to bypass it
			 * if they are invited.
			 */
			if (is_invited(client, channel))
				return HOOK_CONTINUE;
		}
		*errmsg = STR_ERR_SECUREONLYCHAN;
		return ERR_SECUREONLYCHAN;
	}
	return 0;
}

int secureonly_check_secure(Channel *channel)
{
	if (IsSecureOnly(channel))
	{
		return 1;
	}

	return 0;
}

int secureonly_channel_sync(Channel *channel, int merge, int removetheirs, int nomode)
{
	if (!merge && !removetheirs && !nomode)
		return secureonly_kick_insecure_users(channel); /* may return 1, meaning channel is destroyed */
	return 0;
}

int secureonly_check_sajoin(Client *target, Channel *channel, Client *requester)
{
	if (IsSecureOnly(channel) && !IsSecure(target))
	{
		sendnotice(requester, "You cannot SAJOIN %s to %s because the channel is +z and the user is not connected via TLS",
			target->name, channel->name);
		return HOOK_DENY;
	}

	return HOOK_CONTINUE;
}

/* Special check for +z in set::modes-on-join. Needs to be done early.
 * Perhaps one day this will be properly handled in the core so this can be removed.
 */
int secureonly_pre_local_join(Client *client, Channel *channel, const char *key)
{
	if ((channel->users == 0) && (MODES_ON_JOIN & EXTCMODE_SECUREONLY) && !IsSecure(client) && !IsOper(client))
	{
		sendnumeric(client, ERR_SECUREONLYCHAN, channel->name);
		return HOOK_DENY;
	}
	return HOOK_CONTINUE;
}
