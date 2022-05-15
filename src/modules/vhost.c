/*
 *   Unreal Internet Relay Chat Daemon, src/modules/vhost.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
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

CMD_FUNC(cmd_vhost);

/* Place includes here */
#define MSG_VHOST       "VHOST"

ModuleHeader MOD_HEADER
  = {
	"vhost",	/* Name of module */
	"5.0", /* Version */
	"command /vhost", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_VHOST, cmd_vhost, MAXPARA, CMD_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD()
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
MOD_UNLOAD()
{
	return MOD_SUCCESS;	
}

CMD_FUNC(cmd_vhost)
{
	ConfigItem_vhost *vhost;
	char login[HOSTLEN+1];
	const char *password;
	char olduser[USERLEN+1];

	if (!MyUser(client))
		return;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "VHOST");
		return;

	}

	/* cut-off too long login names. HOSTLEN is arbitrary, we just don't want our
	 * error messages to be cut off because the user is sending huge login names.
	 */
	strlcpy(login, parv[1], sizeof(login));

	password = (parc > 2) ? parv[2] : "";

	if (!(vhost = find_vhost(login)))
	{
		unreal_log(ULOG_WARNING, "vhost", "VHOST_FAILED", client,
		           "Failed VHOST attempt by $client.details [reason: $reason] [vhost-block: $vhost_block]",
		           log_data_string("reason", "Vhost block not found"),
		           log_data_string("fail_type", "UNKNOWN_VHOST_NAME"),
		           log_data_string("vhost_block", login));
		sendnotice(client, "*** [\2vhost\2] Login for %s failed - password incorrect", login);
		return;
	}
	
	if (!user_allowed_by_security_group(client, vhost->match))
	{
		unreal_log(ULOG_WARNING, "vhost", "VHOST_FAILED", client,
		           "Failed VHOST attempt by $client.details [reason: $reason] [vhost-block: $vhost_block]",
		           log_data_string("reason", "Host does not match"),
		           log_data_string("fail_type", "NO_HOST_MATCH"),
		           log_data_string("vhost_block", login));
		sendnotice(client, "*** No vHost lines available for your host");
		return;
	}

	if (!Auth_Check(client, vhost->auth, password))
	{
		unreal_log(ULOG_WARNING, "vhost", "VHOST_FAILED", client,
		           "Failed VHOST attempt by $client.details [reason: $reason] [vhost-block: $vhost_block]",
		           log_data_string("reason", "Authentication failed"),
		           log_data_string("fail_type", "AUTHENTICATION_FAILED"),
		           log_data_string("vhost_block", login));
		sendnotice(client, "*** [\2vhost\2] Login for %s failed - password incorrect", login);
		return;
	}

	/* Authentication passed, but.. there could still be other restrictions: */
	switch (UHOST_ALLOWED)
	{
		case UHALLOW_NEVER:
			if (MyUser(client))
			{
				sendnotice(client, "*** /vhost is disabled");
				return;
			}
			break;
		case UHALLOW_ALWAYS:
			break;
		case UHALLOW_NOCHANS:
			if (MyUser(client) && client->user->joined)
			{
				sendnotice(client, "*** /vhost can not be used while you are on a channel");
				return;
			}
			break;
		case UHALLOW_REJOIN:
			/* join sent later when the host has been changed */
			break;
	}

	/* All checks passed, now let's go ahead and change the host */

	userhost_save_current(client);

	safe_strdup(client->user->virthost, vhost->virthost);
	if (vhost->virtuser)
	{
		strlcpy(olduser, client->user->username, sizeof(olduser));
		strlcpy(client->user->username, vhost->virtuser, sizeof(client->user->username));
		sendto_server(client, 0, 0, NULL, ":%s SETIDENT %s", client->id,
		    client->user->username);
	}
	client->umodes |= UMODE_HIDE;
	client->umodes |= UMODE_SETHOST;
	sendto_server(client, 0, 0, NULL, ":%s SETHOST %s", client->id, client->user->virthost);
	sendto_one(client, NULL, ":%s MODE %s :+tx", client->name, client->name);
	if (vhost->swhois)
	{
		SWhois *s;
		for (s = vhost->swhois; s; s = s->next)
			swhois_add(client, "vhost", -100, s->line, &me, NULL);
	}
	sendnotice(client, "*** Your vhost is now %s%s%s",
		vhost->virtuser ? vhost->virtuser : "",
		vhost->virtuser ? "@" : "",
		vhost->virthost);

	if (vhost->virtuser)
	{
		/* virtuser@virthost */
		unreal_log(ULOG_INFO, "vhost", "VHOST_SUCCESS", client,
			   "$client.details is now using vhost $virtuser@$virthost [vhost-block: $vhost_block]",
			   log_data_string("virtuser", vhost->virtuser),
			   log_data_string("virthost", vhost->virthost),
			   log_data_string("vhost_block", login));
	} else {
		/* just virthost */
		unreal_log(ULOG_INFO, "vhost", "VHOST_SUCCESS", client,
			   "$client.details is now using vhost $virthost [vhost-block: $vhost_block]",
			   log_data_string("virthost", vhost->virthost),
			   log_data_string("vhost_block", login));
	}

	userhost_changed(client);
}
