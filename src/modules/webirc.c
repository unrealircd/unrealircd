/*
 * IRC - Internet Relay Chat, src/modules/webirc.c
 * WebIRC / CGI:IRC Support
 * (C) Copyright 2006-.. Bram Matthys (Syzop) and the UnrealIRCd team
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

/* Module header */
ModuleHeader MOD_HEADER
= {
	"webirc",
	"5.0",
	"WebIRC/CGI:IRC Support",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Global variables */
ModDataInfo *webirc_md = NULL;

/* Forward declarations */
CMD_FUNC(cmd_webirc);
int webirc_local_pass(Client *client, const char *password);
const char *webirc_md_serialize(ModData *m);
void webirc_md_unserialize(const char *str, ModData *m);
void webirc_md_free(ModData *md);
int webirc_secure_connect(Client *client);

#define IsWEBIRC(x)			(moddata_client(x, webirc_md).l)
#define IsWEBIRCSecure(x)	(moddata_client(x, webirc_md).l == 2)
#define ClearWEBIRC(x)		do { moddata_client(x, webirc_md).l = 0; } while(0)
#define SetWEBIRC(x)		do { moddata_client(x, webirc_md).l = 1; } while(0)
#define SetWEBIRCSecure(x)	do { moddata_client(x, webirc_md).l = 2; } while(0)

#define MSG_WEBIRC "WEBIRC"

/** Called upon module init */
MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "webirc";
	mreq.type = MODDATATYPE_CLIENT;
	mreq.serialize = webirc_md_serialize;
	mreq.unserialize = webirc_md_unserialize;
	mreq.free = webirc_md_free;
	mreq.sync = MODDATA_SYNC_EARLY;
	webirc_md = ModDataAdd(modinfo->handle, mreq);
	if (!webirc_md)
	{
		config_error("could not register webirc moddata");
		return MOD_FAILED;
	}

	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_PASS, 0, webirc_local_pass);
	HookAdd(modinfo->handle, HOOKTYPE_SECURE_CONNECT, 0, webirc_secure_connect);

	CommandAdd(modinfo->handle, MSG_WEBIRC, cmd_webirc, MAXPARA, CMD_UNREGISTERED);
		
	return MOD_SUCCESS;
}

/** Called upon module load */
MOD_LOAD()
{
	return MOD_SUCCESS;
}

/** Called upon unload */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

const char *webirc_md_serialize(ModData *m)
{
	static char buf[32];
	if (m->i == 0)
		return NULL; /* not set */
	snprintf(buf, sizeof(buf), "%d", m->i);
	return buf;
}

void webirc_md_unserialize(const char *str, ModData *m)
{
	m->i = atoi(str);
}

void webirc_md_free(ModData *md)
{
	/* we have nothing to free actually, but we must set to zero */
	md->l = 0;
}

ConfigItem_proxy *find_webirc(Client *client, const char *password, ProxyType type, char **errorstr)
{
	ConfigItem_proxy *e;
	char *error = NULL;

	for (e = conf_proxy; e; e = e->next)
	{
		if ((e->type == type) && user_allowed_by_security_group(client, e->mask))
		{
			if (type == PROXY_WEBIRC)
			{
				/* Check password */
				if (!Auth_Check(client, e->auth, password))
					error = "CGI:IRC -- Invalid password";
				else
					return e; /* Found matching block, return straight away */
			} else
			if (type == PROXY_WEBIRC_PASS) {
				return e; /* The PROXY_WEBIRC_PASS type has no password checking */
			}
		}
	}

	if (error)
		*errorstr = error; /* Invalid password (this error was delayed) */
	else
		*errorstr = "CGI:IRC -- No access"; /* No match found */

	return NULL;
}

#define WEBIRC_STRING     "WEBIRC_"
#define WEBIRC_STRINGLEN  (sizeof(WEBIRC_STRING)-1)

/* Does the CGI:IRC host spoofing work */
void dowebirc(Client *client, const char *ip, const char *host, const char *options)
{
	char oldip[64];
	char scratch[64];

	if (IsWEBIRC(client))
	{
		exit_client(client, NULL, "Double CGI:IRC request (already identified)");
		return;
	}

	if (host && !strcmp(ip, host))
		host = NULL; /* host did not resolve, make it NULL */

	/* STEP 1: Update client->local->ip
	   inet_pton() returns 1 on success, 0 on bad input, -1 on bad AF */
	if (!is_valid_ip(ip))
	{
		/* then we have an invalid IP */
		exit_client(client, NULL, "Invalid IP address");
		return;
	}

	/* STEP 2: Update GetIP() */
	strlcpy(oldip, client->ip, sizeof(oldip));
	safe_strdup(client->ip, ip);
		
	/* STEP 3: Update client->local->hostp */
	/* (free old) */
	if (client->local->hostp)
	{
		unreal_free_hostent(client->local->hostp);
		client->local->hostp = NULL;
	}
	/* (create new) */
	if (host && valid_host(host, 1))
		client->local->hostp = unreal_create_hostent(host, client->ip);

	/* STEP 4: Update sockhost
	   Make sure that if this any IPv4 address is _not_ prefixed with
	   "::ffff:" by using Inet_ia2p().
	 */
	// Hmm I ignored above warning. May be bad during transition period.
	strlcpy(client->local->sockhost, client->ip, sizeof(client->local->sockhost));

	SetWEBIRC(client);

	if (options)
	{
		char optionsbuf[BUFSIZE];
		char *name, *p = NULL, *p2;
		strlcpy(optionsbuf, options, sizeof(optionsbuf));
		for (name = strtoken(&p, optionsbuf, " "); name; name = strtoken(&p, NULL, " "))
		{
			p2 = strchr(name, '=');
			if (p2)
				*p2 = '\0';
			if (!strcmp(name, "secure") && IsSecure(client))
			{
				/* The entire [client]--[webirc gw]--[server] chain is secure */
				SetWEBIRCSecure(client);
			}
		}
	}

	RunHook(HOOKTYPE_IP_CHANGE, client, oldip);
}

/* WEBIRC <pass> "cgiirc" <hostname> <ip> [:option1 [option2...]]*/
CMD_FUNC(cmd_webirc)
{
	const char *ip, *host, *password, *options;
	ConfigItem_proxy *e;
	char *error = NULL;

	if ((parc < 5) || BadPtr(parv[4]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "WEBIRC");
		return;
	}

	password = parv[1];
	host = !DONT_RESOLVE ? parv[3] : parv[4];
	ip = parv[4];
	options = parv[5]; /* can be NULL */

	/* Check if allowed host */
	e = find_webirc(client, password, PROXY_WEBIRC, &error);
	if (!e)
	{
		exit_client(client, NULL, error);
		return;
	}

	/* And do our job.. */
	dowebirc(client, ip, host, options);
}

int webirc_local_pass(Client *client, const char *password)
{
	if (!strncmp(password, WEBIRC_STRING, WEBIRC_STRINGLEN))
	{
		char buf[512];
		char *ip, *host;
		ConfigItem_proxy *e;
		char *error = NULL;

		/* Work on a copy as we may trash it */
		strlcpy(buf, password, sizeof(buf));
		e = find_webirc(client, NULL, PROXY_WEBIRC_PASS, &error);
		if (e)
		{
			/* Ok now we got that sorted out, proceed:
			 * Syntax: WEBIRC_<ip>_<resolvedhostname>
			 * The <resolvedhostname> has been checked ip->host AND host->ip by CGI:IRC itself
			 * already so we trust it.
			 */
			ip = buf + WEBIRC_STRINGLEN;
			host = strchr(ip, '_');
			if (!host)
			{
				exit_client(client, NULL, "Invalid CGI:IRC IP received");
				return HOOK_DENY;
			}
			*host++ = '\0';
		
			dowebirc(client, ip, host, NULL);
			return HOOK_DENY;
		}
		/* fallthrough if not in webirc block.. */
	}

	return HOOK_CONTINUE; /* not webirc */
}

/** Called from register_user() right after setting user +z */
int webirc_secure_connect(Client *client)
{
	/* Remove secure mode (-z) if the WEBIRC gateway did not ensure
	 * us that their [client]--[webirc gateway] connection is also
	 * secure (eg: using https)
	 */
	if (IsWEBIRC(client) && IsSecureConnect(client) && !IsWEBIRCSecure(client))
		client->umodes &= ~UMODE_SECURE;
	return 0;
}
