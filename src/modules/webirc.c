/*
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
ModuleHeader MOD_HEADER = {
    "webirc",
    "5.0",
    "WebIRC/CGI:IRC Support",
    "UnrealIRCd Team",
    "unrealircd-6",
};

/** The type of WEBIRC client (in practice: secure or not).
 * Do not change these numbers, they are used in S2S.
 */
typedef enum WEBIRCType {
	WEBIRC_TYPE_WEBIRC = 1,        /**< Via WEBIRC */
	WEBIRC_TYPE_WEBIRC_SECURE = 2, /**< Via WEBIRC and gateway indicated the client is securely connected */
} WEBIRCType;

typedef struct WEBIRCUser {
	WEBIRCType type; /**< One of WEBIRC_TYPE_* (secure or not)  */
	char *name;      /**< Name of the proxy { } block. NULL for remote clients on older servers. */
	char *ip;        /**< IP of the gateway. NULL for remote clients on older servers. */
	char *host;      /**< Hostname of the gateway (and otherwise the IP). NULL for remote clients on older servers. */
} WEBIRCUser;

/* Global variables */
ModDataInfo *webirc_md = NULL;

/* Forward declarations */
CMD_FUNC(cmd_webirc);
int webirc_local_pass(Client *client, const char *password);
const char *webirc_md_serialize(ModData *m);
void webirc_md_unserialize(const char *str, ModData *m);
void webirc_md_free(ModData *md);
int webirc_secure_connect(Client *client);
int webirc_whois(Client *client, Client *target, NameValuePrioList **list);

#define WEBIRC(x)         ((WEBIRCUser *)moddata_client(x, webirc_md).ptr)
#define IsWEBIRC(x)       (WEBIRC(x) != NULL)
#define IsWEBIRCSecure(x) (WEBIRC(x) && (WEBIRC(x)->type == WEBIRC_TYPE_WEBIRC_SECURE))

#define MSG_WEBIRC "WEBIRC"

/** Called upon module init */
MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "webirc";
	mreq.type = MODDATATYPE_CLIENT;
	mreq.priority = -999997;
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
	HookAdd(modinfo->handle, HOOKTYPE_WHOIS, 0, webirc_whois);

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
	static char buf[512];
	WEBIRCUser *w = m->ptr;

	if (!w)
		return NULL; /* not set */

	if (!w->name && !w->ip && !w->host)
		snprintf(buf, sizeof(buf), "%d", (int)w->type);
	else
		snprintf(buf, sizeof(buf), "%d;%s;%s;%s", (int)w->type,
		         w->name ? w->name : "", w->ip ? w->ip : "", w->host ? w->host : "");
	return buf;
}

void webirc_md_unserialize(const char *str, ModData *m)
{
	WEBIRCUser *w;
	char buf[512];
	char *type, *name, *ip, *host, *p = NULL;
	int t;

	if (m->ptr)
		webirc_md_free(m);
	if (BadPtr(str))
		return; /* empty/freed */

	strlcpy(buf, str, sizeof(buf));
	type = strtoken_noskip(&p, buf, ";");
	t = atoi(type);
	if (t <= 0)
		return;

	m->ptr = w = safe_alloc(sizeof(WEBIRCUser));
	w->type = t;

	name = strtoken_noskip(&p, NULL, ";");
	ip = strtoken_noskip(&p, NULL, ";");
	host = strtoken_noskip(&p, NULL, ";");
	if (!BadPtr(name) && security_group_valid_name(name))
		safe_strdup(w->name, name);
	if (!BadPtr(ip) && is_valid_ip(ip))
		safe_strdup(w->ip, ip);
	if (!BadPtr(host) && (valid_host(host, 1) || is_valid_ip(host)))
		safe_strdup(w->host, host);
}

void webirc_md_free(ModData *md)
{
	WEBIRCUser *w = md->ptr;

	if (w)
	{
		safe_free(w->name);
		safe_free(w->host);
		safe_free(w->ip);
		safe_free(md->ptr);
	}
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
			} else if (type == PROXY_WEBIRC_PASS)
			{
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

#define WEBIRC_STRING    "WEBIRC_"
#define WEBIRC_STRINGLEN (sizeof(WEBIRC_STRING) - 1)

/* Does the CGI:IRC host spoofing work */
void dowebirc(Client *client, ConfigItem_proxy *proxy, const char *ip, const char *host, const char *options)
{
	char gateway_ip[IPLEN + 1];
	char gateway_host[HOSTLEN + 1];
	WEBIRCUser *w;

	if (IsWEBIRC(client))
	{
		exit_client(client, NULL, "Double CGI:IRC request (already identified)");
		return;
	}

	strlcpy(gateway_ip, client->ip, sizeof(gateway_ip));
	if (client->local->hostp && client->local->hostp->h_name)
		strlcpy(gateway_host, client->local->hostp->h_name, sizeof(gateway_host));
	else
		strlcpy(gateway_host, client->ip, sizeof(gateway_host));

	if (host && !strcmp(ip, host))
		host = NULL; /* host did not resolve, make it NULL */

	if (!set_client_ip(client, ip))
		return; /* Failed, client exited */

	/* Update client->local->hostp */
	/* (free old) */
	if (client->local->hostp)
	{
		unreal_free_hostent(client->local->hostp);
		client->local->hostp = NULL;
	}
	/* (create new) */
	if (host && valid_host(host, 1))
		client->local->hostp = unreal_create_hostent(host, client->ip);

	/* Update sockhost */
	set_sockhost(client, client->ip);

	moddata_client(client, webirc_md).ptr = w = safe_alloc(sizeof(WEBIRCUser));
	w->type = WEBIRC_TYPE_WEBIRC;
	safe_strdup(w->name, proxy->name);
	safe_strdup(w->ip, gateway_ip);
	safe_strdup(w->host, gateway_host);

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
				w->type = WEBIRC_TYPE_WEBIRC_SECURE;
			}
		}
	}
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
	dowebirc(client, e, ip, host, options);
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

			dowebirc(client, e, ip, host, NULL);
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

/** Show the WEBIRC gateway in WHOIS */
int webirc_whois(Client *client, Client *target, NameValuePrioList **list)
{
	WhoisConfigDetails policy = whois_get_policy(client, target, "webirc");
	WEBIRCUser *w = WEBIRC(target);
	const char *ip = "*", *host = "*";

	if (!w || (policy <= WHOIS_CONFIG_DETAILS_NONE))
		return 0;

	if (policy == WHOIS_CONFIG_DETAILS_FULL)
	{
		if (w->ip)
			ip = w->ip;
		if (w->host)
			host = w->host;
	}

	if (w->name)
	{
		add_nvplist_numeric_fmt(list, 40001, "webirc", client, RPL_WHOISGATEWAY,
		                        "%s %s %s :is connecting via WebIRC gateway %s",
		                        target->name, host, ip, w->name);
	} else
	{
		/* Servers older than 6.2.8 didn't communicate name/ip/host in S2S: */
		add_nvplist_numeric(list, 40001, "webirc", client, RPL_WHOISGATEWAY,
		                    target->name, host, ip, "is connecting via a WebIRC gateway");
	}
	return 0;
}
