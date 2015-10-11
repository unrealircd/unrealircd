/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_vhost.c
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

CMD_FUNC(m_vhost);

/* Place includes here */
#define MSG_VHOST       "VHOST"

ModuleHeader MOD_HEADER(m_vhost)
  = {
	"vhost",	/* Name of module */
	"4.0", /* Version */
	"command /vhost", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_vhost)
{
	CommandAdd(modinfo->handle, MSG_VHOST, m_vhost, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_vhost)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
MOD_UNLOAD(m_vhost)
{
	return MOD_SUCCESS;	
}

CMD_FUNC(m_vhost)
{
	ConfigItem_vhost *vhost;
	char *login, *password, host[NICKLEN+USERLEN+HOSTLEN+6], host2[NICKLEN+USERLEN+HOSTLEN+6];
	int	len, length;
	int 	i;

	if (!MyClient(sptr))
		return 0;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "VHOST");
		return 0;

	}

	login = parv[1];
	password = (parc > 2) ? parv[2] : "";

	/* cut-off too long login names. HOSTLEN is arbitrary, we just don't want our
	 * error messages to be cut off because the user is sending huge login names.
	 */
	if (strlen(login) > HOSTLEN)
		login[HOSTLEN] = '\0';

	if (!(vhost = Find_vhost(login)))
	{
		sendto_snomask(SNO_VHOST,
		    "[\2vhost\2] Failed login for vhost %s by %s!%s@%s - incorrect password",
		    login, sptr->name,
		    sptr->user->username,
		    sptr->user->realhost);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** [\2vhost\2] Login for %s failed - password incorrect",
		    me.name, sptr->name, login);
		return 0;
	}
	
	if (!unreal_mask_match(sptr, vhost->mask))
	{
		sendto_snomask(SNO_VHOST,
		    "[\2vhost\2] Failed login for vhost %s by %s!%s@%s - host does not match",
		    login, sptr->name, sptr->user->username, sptr->user->realhost);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** No vHost lines available for your host",
		    me.name, sptr->name);
		return 0;
	}

	i = Auth_Check(cptr, vhost->auth, password);
	if (i > 0)
	{
		char olduser[USERLEN+1];
		
		switch (UHOST_ALLOWED)
		{
			case UHALLOW_NEVER:
				if (MyClient(sptr))
				{
					sendto_one(sptr, ":%s NOTICE %s :*** /vhost is disabled", me.name, sptr->name);
					return 0;
				}
				break;
			case UHALLOW_ALWAYS:
				break;
			case UHALLOW_NOCHANS:
				if (MyClient(sptr) && sptr->user->joined)
				{
					sendto_one(sptr, ":%s NOTICE %s :*** /vhost can not be used while you are on a channel", me.name, sptr->name);
					return 0;
				}
				break;
			case UHALLOW_REJOIN:
				rejoin_leave(sptr);
				/* join sent later when the host has been changed */
				break;
		}
		safestrdup(sptr->user->virthost, vhost->virthost);
		if (vhost->virtuser)
		{
			strcpy(olduser, sptr->user->username);
			strlcpy(sptr->user->username, vhost->virtuser, USERLEN);
			sendto_server(cptr, 0, 0, ":%s SETIDENT %s", sptr->name,
			    sptr->user->username);
		}
		sptr->umodes |= UMODE_HIDE;
		sptr->umodes |= UMODE_SETHOST;
		sendto_server(cptr, 0, 0, ":%s SETHOST %s", sptr->name, sptr->user->virthost);
		sendto_one(sptr, ":%s MODE %s :+tx",
		    sptr->name, sptr->name);
		if (vhost->swhois)
		{
			SWhois *s;
			for (s = vhost->swhois; s; s = s->next)
				swhois_add(sptr, "vhost", -100, s->line, &me, NULL);
		}
		sendto_one(sptr, err_str(RPL_HOSTHIDDEN), me.name, sptr->name, vhost->virthost);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Your vhost is now %s%s%s",
		    me.name, sptr->name, vhost->virtuser ? vhost->virtuser : "", 
			vhost->virtuser ? "@" : "", vhost->virthost);
		sendto_snomask(SNO_VHOST,
		    "[\2vhost\2] %s (%s!%s@%s) is now using vhost %s%s%s",
		    login, sptr->name,
		    vhost->virtuser ? olduser : sptr->user->username,
		    sptr->user->realhost, vhost->virtuser ? vhost->virtuser : "", 
		    	vhost->virtuser ? "@" : "", vhost->virthost);
		if (UHOST_ALLOWED == UHALLOW_REJOIN)
			rejoin_joinandmode(sptr);
		return 0;
	}
	if (i == -1)
	{
		sendto_snomask(SNO_VHOST,
		    "[\2vhost\2] Failed login for vhost %s by %s!%s@%s - incorrect password",
		    login, sptr->name,
		    sptr->user->username,
		    sptr->user->realhost);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** [\2vhost\2] Login for %s failed - password incorrect",
		    me.name, sptr->name, login);
		return 0;
	}
	/* Belay that order, Lt. (upon -2)*/
	
	return 0;	
}
