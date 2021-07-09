/*
 *   IRC - Internet Relay Chat, src/modules/server.c
 *   (C) 2004-present The UnrealIRCd Team
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

/* Definitions */
typedef enum AutoConnectStrategy {
	AUTOCONNECT_PARALLEL = 0,
	AUTOCONNECT_SEQUENTIAL = 1,
	AUTOCONNECT_SEQUENTIAL_FALLBACK = 2
} AutoConnectStrategy;

typedef struct cfgstruct cfgstruct;
struct cfgstruct {
	AutoConnectStrategy autoconnect_strategy;
	long connect_timeout;
	long handshake_timeout;
};

/* Forward declarations */
void server_config_setdefaults(cfgstruct *cfg);
int server_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int server_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
EVENT(server_autoconnect);
EVENT(server_handshake_timeout);
void send_channel_modes_sjoin3(Client *to, Channel *channel);
CMD_FUNC(cmd_server);
CMD_FUNC(cmd_sid);
int _verify_link(Client *client, char *servername, ConfigItem_link **link_out);
void _send_protoctl_servers(Client *client, int response);
void _send_server_message(Client *client);
void _introduce_user(Client *to, Client *acptr);
int _check_deny_version(Client *cptr, char *software, int protocol, char *flags);
void _broadcast_sinfo(Client *acptr, Client *to, Client *except);
int server_sync(Client *cptr, ConfigItem_link *conf);
void server_generic_free(ModData *m);
int server_post_connect(Client *client);


/* Global variables */
static char buf[BUFSIZE];
static cfgstruct cfg;
static char *last_autoconnect_server = NULL;

ModuleHeader MOD_HEADER
  = {
	"server",
	"5.0",
	"command /server", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_PROTOCTL_SERVERS, _send_protoctl_servers);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_SERVER_MESSAGE, _send_server_message);
	EfunctionAdd(modinfo->handle, EFUNC_VERIFY_LINK, _verify_link);
	EfunctionAddVoid(modinfo->handle, EFUNC_INTRODUCE_USER, _introduce_user);
	EfunctionAdd(modinfo->handle, EFUNC_CHECK_DENY_VERSION, _check_deny_version);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_SINFO, _broadcast_sinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, server_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	LoadPersistentPointer(modinfo, last_autoconnect_server, server_generic_free);
	server_config_setdefaults(&cfg);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, server_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_POST_SERVER_CONNECT, 0, server_post_connect);
	CommandAdd(modinfo->handle, "SERVER", cmd_server, MAXPARA, CMD_UNREGISTERED|CMD_SERVER);
	CommandAdd(modinfo->handle, "SID", cmd_sid, MAXPARA, CMD_SERVER);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	EventAdd(modinfo->handle, "server_autoconnect", server_autoconnect, NULL, 2000, 0);
	EventAdd(modinfo->handle, "server_handshake_timeout", server_handshake_timeout, NULL, 1000, 0);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	SavePersistentPointer(modinfo, last_autoconnect_server);
	return MOD_SUCCESS;
}

/** Convert 'str' to a AutoConnectStrategy value.
 * @param str	The string, eg "parallel"
 * @returns a valid AutoConnectStrategy value or -1 if not found.
 */
AutoConnectStrategy autoconnect_strategy_strtoval(char *str)
{
	if (!strcmp(str, "parallel"))
		return AUTOCONNECT_PARALLEL;
	if (!strcmp(str, "sequential"))
		return AUTOCONNECT_SEQUENTIAL;
	if (!strcmp(str, "sequential-fallback"))
		return AUTOCONNECT_SEQUENTIAL_FALLBACK;
	return -1;
}

/** Convert an AutoConnectStrategy value to a string.
 * @param val	The value to convert to a string
 * @returns a string, such as "parallel".
 */
char *autoconnect_strategy_valtostr(AutoConnectStrategy val)
{
	switch (val)
	{
		case AUTOCONNECT_PARALLEL:
			return "parallel";
		case AUTOCONNECT_SEQUENTIAL:
			return "sequential";
		case AUTOCONNECT_SEQUENTIAL_FALLBACK:
			return "sequential-fallback";
		default:
			return "???";
	}
}

void server_config_setdefaults(cfgstruct *cfg)
{
	cfg->autoconnect_strategy = AUTOCONNECT_SEQUENTIAL;
	cfg->connect_timeout = 10;
	cfg->handshake_timeout = 20;
}

int server_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::server-linking.. */
	if (!ce || strcmp(ce->ce_varname, "server-linking"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: blank set::server-linking::%s without value",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		} else
		if (!strcmp(cep->ce_varname, "autoconnect-strategy"))
		{
			if (autoconnect_strategy_strtoval(cep->ce_vardata) < 0)
			{
				config_error("%s:%i: set::server-linking::autoconnect-strategy: invalid value '%s'. "
				             "Should be one of: parallel",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				errors++;
				continue;
			}
		} else
		if (!strcmp(cep->ce_varname, "connect-timeout"))
		{
			long v = config_checkval(cep->ce_vardata, CFG_TIME);
			if ((v < 5) || (v > 30))
			{
				config_error("%s:%i: set::server-linking::connect-timeout should be between 5 and 60 seconds",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				continue;
			}
		} else
		if (!strcmp(cep->ce_varname, "handshake-timeout"))
		{
			long v = config_checkval(cep->ce_vardata, CFG_TIME);
			if ((v < 10) || (v > 120))
			{
				config_error("%s:%i: set::server-linking::handshake-timeout should be between 10 and 120 seconds",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				continue;
			}
		} else
		{
			config_error("%s:%i: unknown directive set::server-linking::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int server_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::server-linking.. */
	if (!ce || strcmp(ce->ce_varname, "server-linking"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "autoconnect-strategy"))
		{
			cfg.autoconnect_strategy = autoconnect_strategy_strtoval(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "connect-timeout"))
		{
			cfg.connect_timeout = config_checkval(cep->ce_vardata, CFG_TIME);
		} else
		if (!strcmp(cep->ce_varname, "handshake-timeout"))
		{
			cfg.handshake_timeout = config_checkval(cep->ce_vardata, CFG_TIME);
		}
	}
	return 1;
}

int server_needs_linking(ConfigItem_link *aconf)
{
	ConfigItem_deny_link *deny;
	Client *client;
	ConfigItem_class *class;

	/* We're only interested in autoconnect blocks that are valid. Also, we ignore temporary link blocks. */
	if (!(aconf->outgoing.options & CONNECT_AUTO) || !aconf->outgoing.hostname || (aconf->flag.temporary == 1))
		return 0;

	class = aconf->class;

	/* Never do more than one connection attempt per <connfreq> seconds (for the same server) */
	if ((aconf->hold > TStime()))
		return 0;

	aconf->hold = TStime() + class->connfreq;

	client = find_client(aconf->servername, NULL);
	if (client)
		return 0; /* Server already connected (or connecting) */

	if (class->clients >= class->maxclients)
		return 0; /* Class is full */

	/* Check connect rules to see if we're allowed to try the link */
	for (deny = conf_deny_link; deny; deny = deny->next)
		if (unreal_mask_match_string(aconf->servername, deny->mask) && crule_eval(deny->rule))
			return 0;

	/* Yes, this server is a linking candidate */
	return 1;
}

void server_autoconnect_parallel(void)
{
	ConfigItem_link *aconf;

	for (aconf = conf_link; aconf; aconf = aconf->next)
	{
		if (!server_needs_linking(aconf))
			continue;

		if (connect_server(aconf, NULL, NULL) == 0)
		{
			sendto_ops_and_log("Trying to activate link with server %s[%s]...",
				aconf->servername, aconf->outgoing.hostname);
		}
	}
}

/** Find first (valid) autoconnect server in link blocks.
 * This function should not be used directly. It is a helper function
 * for find_next_autoconnect_server().
 */
ConfigItem_link *find_first_autoconnect_server(void)
{
	ConfigItem_link *aconf;

	for (aconf = conf_link; aconf; aconf = aconf->next)
	{
		if (!server_needs_linking(aconf))
			continue;
		return aconf; /* found! */
	}
	return NULL; /* none */
}

/** Find next server that we should try to autoconnect to.
 * Taking into account that we last tried server 'current'.
 * @param current	Server the previous autoconnect attempt was made to
 * @returns A link block, or NULL if no servers are suitable.
 */
ConfigItem_link *find_next_autoconnect_server(char *current)
{
	ConfigItem_link *aconf;

	/* If the current autoconnect server is NULL then
	 * just find whichever valid server is first.
	 */
	if (current == NULL)
		return find_first_autoconnect_server();

	/* Next code is a bit convulted, it would have
	 * been easier if conf_link was a circular list ;)
	 */

	/* Otherwise, walk the list up to 'current' */
	for (aconf = conf_link; aconf; aconf = aconf->next)
	{
		if (!strcmp(aconf->servername, current))
			break;
	}

	/* If the 'current' server dissapeared, then let's
	 * just pick the first one from the list.
	 * It is a rare event to have the link { } block
	 * removed of a server that we just happened to
	 * try to link to before, so we can afford to do
	 * it this way.
	 */
	if (!aconf)
		return find_first_autoconnect_server();

	/* Check the remainder for the list, in other words:
	 * check all servers after 'current' if they are
	 * ready for an outgoing connection attempt...
	 */
	for (aconf = aconf->next; aconf; aconf = aconf->next)
	{
		if (!server_needs_linking(aconf))
			continue;
		return aconf; /* found! */
	}

	/* If we get here then there are no valid servers
	 * after 'current', so now check for before 'current'
	 * (and including 'current', since we may
	 *  have to autoconnect to that one again,
	 *  eg if it is the only autoconnect server)...
	 */
	for (aconf = conf_link; aconf; aconf = aconf->next)
	{
		if (!server_needs_linking(aconf))
		{
			if (!strcmp(aconf->servername, current))
				break; /* need to stop here */
			continue;
		}
		return aconf; /* found! */
	}

	return NULL; /* none */
}

/** Check if we are currently connecting to a server (outgoing).
 * This function takes into account not only an outgoing TCP/IP connect
 * or TLS handshake, but also if we are 'somewhat connected' to that
 * server but have not completed the full sync, eg we may still need
 * to receive SIDs or other sync data.
 * NOTE: This implicitly assumes that outgoing links only go to
 *       servers that will (eventually) send "EOS".
 *       Should be a reasonable assumption given that in nearly all
 *       cases we only connect to UnrealIRCd servers for the outgoing
 *       case, as services are "always" incoming links.
 * @returns 1 if an outgoing link is in progress, 0 if not.
 */
int current_outgoing_link_in_process(void)
{
	Client *client;

	list_for_each_entry(client, &unknown_list, lclient_node)
	{
		if (client->serv && *client->serv->by && client->local->firsttime &&
		    (IsConnecting(client) || IsTLSConnectHandshake(client) || !IsSynched(client)))
		{
			return 1;
		}
	}

	list_for_each_entry(client, &server_list, special_node)
	{
		if (client->serv && *client->serv->by && client->local->firsttime &&
		    (IsConnecting(client) || IsTLSConnectHandshake(client) || !IsSynched(client)))
		{
			return 1;
		}
	}

	return 0;
}

void server_autoconnect_sequential(void)
{
	ConfigItem_link *aconf;

	if (current_outgoing_link_in_process())
		return;

	/* We are currently not in the process of doing an outgoing connect,
	 * let's see if we need to connect to somewhere...
	 */
	aconf = find_next_autoconnect_server(last_autoconnect_server);
	if (aconf == NULL)
		return; /* No server to connect to at this time */

	/* Start outgoing link attempt */
	safe_strdup(last_autoconnect_server, aconf->servername);
	if (connect_server(aconf, NULL, NULL) == 0)
	{
		sendto_ops_and_log("Trying to activate link with server %s[%s]...",
			aconf->servername, aconf->outgoing.hostname);
	}
}

/** Perform autoconnect to servers that are not linked yet. */
EVENT(server_autoconnect)
{
	switch (cfg.autoconnect_strategy)
	{
		case AUTOCONNECT_PARALLEL:
			server_autoconnect_parallel();
			break;
		case AUTOCONNECT_SEQUENTIAL:
		/* Fallback is the same as sequential but we reset last_autoconnect_server on connect */
		case AUTOCONNECT_SEQUENTIAL_FALLBACK:
			server_autoconnect_sequential();
			break;
	}
}

EVENT(server_handshake_timeout)
{
	Client *client, *next;

	list_for_each_entry_safe(client, next, &unknown_list, lclient_node)
	{
		/* We are only interested in outgoing server connects */
		if (!client->serv || !*client->serv->by || !client->local->firsttime)
			continue;

		/* Handle set::server-linking::connect-timeout */
		if ((IsConnecting(client) || IsTLSConnectHandshake(client)) &&
		    ((TStime() - client->local->firsttime) >= cfg.connect_timeout))
		{
			/* If this is a connect timeout to an outgoing server then notify ops & log it */
			sendto_ops_and_log("Connect timeout while trying to link to server '%s' (%s)",
			                   client->name, client->ip?client->ip:"<unknown ip>");

			exit_client(client, NULL, "Connection timeout");
			continue;
		}

		/* Handle set::server-linking::handshake-timeout */
		if ((TStime() - client->local->firsttime) >= cfg.handshake_timeout)
		{
			/* If this is a handshake timeout to an outgoing server then notify ops & log it */
			sendto_ops_and_log("Connection handshake timeout while trying to link to server '%s' (%s)",
			                   client->name, client->ip?client->ip:"<unknown ip>");

			exit_client(client, NULL, "Handshake Timeout");
			continue;
		}
	}
}

/** Check deny version { } blocks.
 * @param cptr		Client (a server)
 * @param software	Software version in use (can be NULL)
 * @param protoctol	UnrealIRCd protocol version in use (can be 0)
 * @param flags		Server flags (hardly ever used, can be NULL)
 * @returns 1 if link is denied (client is already killed), 0 if not.
 */
int _check_deny_version(Client *cptr, char *software, int protocol, char *flags)
{
	ConfigItem_deny_version *vlines;
	
	for (vlines = conf_deny_version; vlines; vlines = vlines->next)
	{
		if (match_simple(vlines->mask, cptr->name))
			break;
	}
	
	if (vlines)
	{
		char *proto = vlines->version;
		char *vflags = vlines->flags;
		int result = 0, i;
		switch (*proto)
		{
			case '<':
				proto++;
				if (protocol < atoi(proto))
					result = 1;
				break;
			case '>':
				proto++;
				if (protocol > atoi(proto))
					result = 1;
				break;
			case '=':
				proto++;
				if (protocol == atoi(proto))
					result = 1;
				break;
			case '!':
				proto++;
				if (protocol != atoi(proto))
					result = 1;
				break;
			default:
				if (protocol == atoi(proto))
					result = 1;
				break;
		}
		if (protocol == 0 || *proto == '*')
			result = 0;

		if (result)
		{
			exit_client(cptr, NULL, "Denied by deny version { } block");
			return 0;
		}

		if (flags)
		{
			for (i = 0; vflags[i]; i++)
			{
				if (vflags[i] == '!')
				{
					i++;
					if (strchr(flags, vflags[i])) {
						result = 1;
						break;
					}
				}
				else if (!strchr(flags, vflags[i]))
				{
						result = 1;
						break;
				}
			}

			if (*vflags == '*' || !strcmp(flags, "0"))
				result = 0;
		}

		if (result)
		{
			exit_client(cptr, NULL, "Denied by deny version { } block");
			return 0;
		}
	}
	
	return 1;
}

/** Send our PROTOCTL SERVERS=x,x,x,x stuff.
 * When response is set, it will be PROTOCTL SERVERS=*x,x,x (mind the asterisk).
 */
void _send_protoctl_servers(Client *client, int response)
{
	char buf[512];
	Client *acptr;
	int sendit = 1;

	sendto_one(client, NULL, "PROTOCTL EAUTH=%s,%d,%s%s,%s",
		me.name, UnrealProtocol, serveropts, extraflags ? extraflags : "", version);
		
	ircsnprintf(buf, sizeof(buf), "PROTOCTL SERVERS=%s", response ? "*" : "");

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s,", acptr->id);
		sendit = 1;
		if (strlen(buf) > sizeof(buf)-12)
		{
			if (buf[strlen(buf)-1] == ',')
				buf[strlen(buf)-1] = '\0';
			sendto_one(client, NULL, "%s", buf);
			/* We use the asterisk here too for continuation lines */
			ircsnprintf(buf, sizeof(buf), "PROTOCTL SERVERS=*");
			sendit = 0;
		}
	}
	
	/* Remove final comma (if any) */
	if (buf[strlen(buf)-1] == ',')
		buf[strlen(buf)-1] = '\0';

	if (sendit)
		sendto_one(client, NULL, "%s", buf);
}

void _send_server_message(Client *client)
{
	if (client->serv && client->serv->flags.server_sent)
	{
#ifdef DEBUGMODE
		abort();
#endif
		return;
	}

	sendto_one(client, NULL, "SERVER %s 1 :U%d-%s%s-%s %s",
		me.name, UnrealProtocol, serveropts, extraflags ? extraflags : "", me.id, me.info);

	if (client->serv)
		client->serv->flags.server_sent = 1;
}


/** Verify server link.
 * This does authentication and authorization checks.
 * @param cptr The client directly connected to us (cptr).
 * @param client The client which (originally) issued the server command (client).
 * @param servername The server name provided by the client.
 * @param link_out Pointer-to-pointer-to-link block. Will be set when auth OK. Caller may pass NULL if he doesn't care.
 * @returns This function returns 1 on successful authentication, 0 otherwise - in which case the client has been killed.
 */
int _verify_link(Client *client, char *servername, ConfigItem_link **link_out)
{
	char xerrmsg[256];
	ConfigItem_link *link;
	char *inpath = get_client_name(client, TRUE);
	Client *acptr = NULL, *ocptr = NULL;
	ConfigItem_ban *bconf;

	/* We set the sockhost here so you can have incoming masks based on hostnames.
	 * Perhaps a bit late to do it here, but does anyone care?
	 */
	if (client->local->hostp && client->local->hostp->h_name)
		set_sockhost(client, client->local->hostp->h_name);

	if (link_out)
		*link_out = NULL;
	
	strcpy(xerrmsg, "No matching link configuration");

	if (!client->local->passwd)
	{
		sendto_one(client, NULL, "ERROR :Missing password");
		exit_client(client, NULL, "Missing password");
		return 0;
	}

	/* First check if the server is in the list */
	if (!servername) {
		strcpy(xerrmsg, "Null servername");
		goto errlink;
	}
	
	if (client->serv && client->serv->conf)
	{
		/* This is an outgoing connect so we already know what link block we are
		 * dealing with. It's the one in: client->serv->conf
		 */

		/* Actually we still need to double check the servername to avoid confusion. */
		if (strcasecmp(servername, client->serv->conf->servername))
		{
			ircsnprintf(xerrmsg, sizeof(xerrmsg), "Outgoing connect from link block '%s' but server "
				"introduced himself as '%s'. Server name mismatch.",
				client->serv->conf->servername,
				servername);

			sendto_one(client, NULL, "ERROR :%s", xerrmsg);
			sendto_ops_and_log("Outgoing link aborted to %s(%s@%s) (%s) %s",
				client->serv->conf->servername, client->ident, client->local->sockhost, xerrmsg, inpath);
			exit_client(client, NULL, xerrmsg);
			return 0;
		}
		link = client->serv->conf;
		goto skip_host_check;
	} else {
		/* Hunt the linkblock down ;) */
		for(link = conf_link; link; link = link->next)
			if (match_simple(link->servername, servername))
				break;
	}
	
	if (!link)
	{
		ircsnprintf(xerrmsg, sizeof(xerrmsg), "No link block named '%s'", servername);
		goto errlink;
	}
	
	if (!link->incoming.mask)
	{
		ircsnprintf(xerrmsg, sizeof(xerrmsg), "Link block '%s' exists but has no link::incoming::mask", servername);
		goto errlink;
	}

	link = find_link(servername, client);

	if (!link)
	{
		ircsnprintf(xerrmsg, sizeof(xerrmsg), "Server is in link block but link::incoming::mask didn't match");
errlink:
		/* Send the "simple" error msg to the server */
		sendto_one(client, NULL,
		    "ERROR :Link denied (No link block found named '%s' or link::incoming::mask did not match your IP %s) %s",
		    servername, GetIP(client), inpath);
		/* And send the "verbose" error msg only to locally connected ircops */
		sendto_ops_and_log("Link denied for %s(%s@%s) (%s) %s",
		    servername, client->ident, client->local->sockhost, xerrmsg, inpath);
		exit_client(client, NULL, "Link denied (No link block found with your server name or link::incoming::mask did not match)");
		return 0;
	}

skip_host_check:
	/* Now for checking passwords */
	if (!Auth_Check(client, link->auth, client->local->passwd))
	{
		/* Let's help admins a bit with a good error message in case
		 * they mix different authentication systems (plaintext password
		 * vs an "TLS Auth type" like spkifp/tlsclientcert/tlsclientcertfp).
		 * The 'if' statement below is a bit complex but it consists of 2 things:
		 * 1. Check if our side expects a plaintext password but we did not receive one
		 * 2. Check if our side expects a non-plaintext password but we did receive one
		 */
		if (((link->auth->type == AUTHTYPE_PLAINTEXT) && client->local->passwd && !strcmp(client->local->passwd, "*")) ||
		    ((link->auth->type != AUTHTYPE_PLAINTEXT) && client->local->passwd && strcmp(client->local->passwd, "*")))
		{
			sendto_ops_and_log("Link denied for '%s' (Authentication failed due to different password types on both sides of the link) %s",
				servername, inpath);
			sendto_ops_and_log("Read https://www.unrealircd.org/docs/FAQ#auth-fail-mixed for more information");
		} else
		if (link->auth->type == AUTHTYPE_SPKIFP)
		{
			sendto_ops_and_log("Link denied for '%s' (Authentication failed [spkifp mismatch]) %s",
				servername, inpath);
		} else
		if (link->auth->type == AUTHTYPE_TLS_CLIENTCERT)
		{
			sendto_ops_and_log("Link denied for '%s' (Authentication failed [tlsclientcert mismatch]) %s",
				servername, inpath);
		} else
		if (link->auth->type == AUTHTYPE_TLS_CLIENTCERTFP)
		{
			sendto_ops_and_log("Link denied for '%s' (Authentication failed [tlsclientcertfp mismatch]) %s",
				servername, inpath);
		} else
		{
			sendto_ops_and_log("Link denied for '%s' (Authentication failed [Bad password?]) %s",
				servername, inpath);
		}
		sendto_one(client, NULL,
		    "ERROR :Link '%s' denied (Authentication failed) %s",
		    servername, inpath);
		exit_client(client, NULL, "Link denied (Authentication failed)");
		return 0;
	}

	/* Verify the TLS certificate (if requested) */
	if (link->verify_certificate)
	{
		char *errstr = NULL;

		if (!IsTLS(client))
		{
			sendto_one(client, NULL,
				"ERROR :Link '%s' denied (Not using SSL/TLS) %s",
				servername, inpath);
			sendto_ops_and_log("Link denied for '%s' (Not using SSL/TLS and verify-certificate is on) %s",
				servername, inpath);
			exit_client(client, NULL, "Link denied (Not using SSL/TLS)");
			return 0;
		}
		if (!verify_certificate(client->local->ssl, link->servername, &errstr))
		{
			sendto_one(client, NULL,
				"ERROR :Link '%s' denied (Certificate verification failed) %s",
				servername, inpath);
			sendto_ops_and_log("Link denied for '%s' (Certificate verification failed) %s",
				servername, inpath);
			sendto_ops_and_log("Reason for certificate verification failure: %s", errstr);
			exit_client(client, NULL, "Link denied (Certificate verification failed)");
			return 0;
		}
	}

	/*
	 * Third phase, we check that the server does not exist
	 * already
	 */
	if ((acptr = find_server(servername, NULL)))
	{
		/* Found. Bad. Quit. */

		if (IsMe(acptr))
		{
			sendto_ops_and_log("Link %s rejected, server trying to link with my name (%s)",
				get_client_name(client, TRUE), me.name);
			sendto_one(client, NULL, "ERROR: Server %s exists (it's me!)", me.name);
			exit_client(client, NULL, "Server Exists");
			return 0;
		}

		acptr = acptr->direction;
		ocptr = (client->local->firsttime > acptr->local->firsttime) ? acptr : client;
		acptr = (client->local->firsttime > acptr->local->firsttime) ? client : acptr;
		sendto_one(acptr, NULL,
		    "ERROR :Server %s already exists from %s",
		    servername,
		    (ocptr->direction ? ocptr->direction->name : "<nobody>"));
		sendto_ops_and_log
		    ("Link %s cancelled, server %s already exists from %s",
		    get_client_name(acptr, TRUE), servername,
		    (ocptr->direction ? ocptr->direction->name : "<nobody>"));
		exit_client(acptr, NULL, "Server Exists");
		return 0;
	}
	if ((bconf = find_ban(NULL, servername, CONF_BAN_SERVER)))
	{
		sendto_ops_and_log
			("Cancelling link %s, banned server",
			get_client_name(client, TRUE));
		sendto_one(client, NULL, "ERROR :Banned server (%s)", bconf->reason ? bconf->reason : "no reason");
		exit_client(client, NULL, "Banned server");
		return 0;
	}
	if (link->class->clients + 1 > link->class->maxclients)
	{
		sendto_ops_and_log("Cancelling link %s, full class",
				get_client_name(client, TRUE));
		exit_client(client, NULL, "Full class");
		return 0;
	}
	if (!IsLocalhost(client) && (iConf.plaintext_policy_server == POLICY_DENY) && !IsSecure(client))
	{
		sendto_one(client, NULL, "ERROR :Servers need to use SSL/TLS (set::plaintext-policy::server is 'deny')");
		sendto_ops_and_log("Rejected insecure server %s. See https://www.unrealircd.org/docs/FAQ#ERROR:_Servers_need_to_use_SSL.2FTLS", client->name);
		exit_client(client, NULL, "Servers need to use SSL/TLS (set::plaintext-policy::server is 'deny')");
		return 0;
	}
	if (IsSecure(client) && (iConf.outdated_tls_policy_server == POLICY_DENY) && outdated_tls_client(client))
	{
		sendto_one(client, NULL, "ERROR :Server is using an outdated SSL/TLS protocol or cipher (set::outdated-tls-policy::server is 'deny')");
		sendto_ops_and_log("Rejected server %s using outdated %s. See https://www.unrealircd.org/docs/FAQ#server-outdated-tls", tls_get_cipher(client->local->ssl), client->name);
		exit_client(client, NULL, "Server using outdates SSL/TLS protocol or cipher (set::outdated-tls-policy::server is 'deny')");
		return 0;
	}
	if (link_out)
		*link_out = link;
	return 1;
}

/** Server command. Only for locally connected servers!!.
 * parv[1] = server name
 * parv[2] = hop count
 * parv[3] = server description, may include protocol version and other stuff too (VL)
 */
CMD_FUNC(cmd_server)
{
	char *servername = NULL;	/* Pointer for servername */
	char *ch = NULL;	/* */
	char descbuf[BUFSIZE];
	int  hop = 0;
	char info[REALLEN + 61];
	ConfigItem_link *aconf = NULL;
	ConfigItem_deny_link *deny;
	char *flags = NULL, *protocol = NULL, *inf = NULL, *num = NULL;

	if (IsUser(client))
	{
		sendnumeric(client, ERR_ALREADYREGISTRED);
		sendnotice(client, "*** Sorry, but your IRC program doesn't appear to support changing servers.");
		return;
	}

	if (parc < 4 || (!*parv[3]))
	{
		sendto_one(client, NULL, "ERROR :Not enough SERVER parameters");
		exit_client(client, NULL,  "Not enough parameters");
		return;
	}

	servername = parv[1];

	/* Remote 'SERVER' command is not possible on a 100% SID network */
	if (!MyConnect(client))
	{
		char buf[256];
		sendto_umode_global(UMODE_OPER, "Server %s introduced %s which is using old unsupported protocol from UnrealIRCd 3.2.x or earlier. " 
		                                "See https://www.unrealircd.org/docs/FAQ#old-server-protocol",
		                                client->direction->name, servername);
		exit_client(client->direction, NULL, "Introduced another server with unsupported protocol");
		return;
	}

	if (client->local->listener && (client->local->listener->options & LISTENER_CLIENTSONLY))
	{
		exit_client(client, NULL, "This port is for clients only");
		return;
	}

	if (!valid_server_name(servername))
	{
		sendto_one(client, NULL, "ERROR :Bogus server name (%s)", servername);
		sendto_snomask
		    (SNO_JUNK,
		    "WARNING: Bogus server name (%s) from %s (maybe just a fishy client)",
		    servername, get_client_name(client, TRUE));
		exit_client(client, NULL, "Bogus server name");
		return;
	}

	if (!client->local->passwd)
	{
		sendto_one(client, NULL, "ERROR :Missing password");
		exit_client(client, NULL, "Missing password");
		return;
	}

	if (!verify_link(client, servername, &aconf))
		return; /* Rejected */

	/* From this point the server is authenticated, so we can be more verbose
	 * with notices to ircops and in exit_client() and such.
	 */

	strlcpy(client->name, servername, sizeof(client->name));

	if (strlen(client->id) != 3)
	{
		sendto_umode_global(UMODE_OPER, "Server %s is using old unsupported protocol from UnrealIRCd 3.2.x or earlier. " 
		                                "See https://www.unrealircd.org/docs/FAQ#old-server-protocol",
		                                servername);
		ircd_log(LOG_ERROR, "Server using old unsupported protocol from UnrealIRCd 3.2.x or earlier. "
		                    "See https://www.unrealircd.org/docs/FAQ#old-server-protocol");
		exit_client(client, NULL, "Server using old unsupported protocol from UnrealIRCd 3.2.x or earlier. "
		                          "See https://www.unrealircd.org/docs/FAQ#old-server-protocol");
		return;
	}

	hop = atol(parv[2]);
	if (hop != 1)
	{
		sendto_umode_global(UMODE_OPER, "Directly linked server %s provided a hopcount of %d, while 1 was expected",
		                                servername, hop);
		exit_client(client, NULL, "Invalid SERVER message, hop count must be 1");
		return;
	}
	client->hopcount = hop;

	strlcpy(info, parv[parc - 1], sizeof(info));

	/* Parse "VL" data in description */
	if (SupportVL(client))
	{
		char tmp[REALLEN + 61];
		inf = protocol = flags = num = NULL;
		strlcpy(tmp, info, sizeof(tmp)); /* work on a copy */

		/* We are careful here to allow invalid syntax or missing
		 * stuff, which mean that particular variable will stay NULL.
		 */

		protocol = strtok(tmp, "-");
		if (protocol)
			flags = strtok(NULL, "-");
		if (flags)
			num = strtok(NULL, " ");
		if (num)
			inf = strtok(NULL, "");
		if (inf)
		{
			strlcpy(client->info, inf[0] ? inf : "server", sizeof(client->info)); /* set real description */

			if (!_check_deny_version(client, NULL, atoi(protocol), flags))
				return; /* Rejected */
		} else {
			strlcpy(client->info, info[0] ? info : "server", sizeof(client->info));
		}
	} else {
		strlcpy(client->info, info[0] ? info : "server", sizeof(client->info));
	}

	/* Process deny server { } restrictions */
	for (deny = conf_deny_link; deny; deny = deny->next)
	{
		if (deny->flag.type == CRULE_ALL && unreal_mask_match_string(servername, deny->mask)
			&& crule_eval(deny->rule))
		{
			sendto_ops_and_log("Refused connection from %s. Rejected by deny link { } block.",
				get_client_host(client));
			exit_client(client, NULL, "Disallowed by connection rule");
			return;
		}
	}

	if (aconf->options & CONNECT_QUARANTINE)
		SetQuarantined(client);

	ircsnprintf(descbuf, sizeof descbuf, "Server: %s", servername);
	fd_desc(client->local->fd, descbuf);

	server_sync(client, aconf);
}

/** Remote server command (SID).
 * parv[1] = server name
 * parv[2] = hop count (always >1)
 * parv[3] = SID
 * parv[4] = server description
 */
CMD_FUNC(cmd_sid)
{
	Client *acptr, *ocptr;
	ConfigItem_link	*aconf;
	ConfigItem_ban *bconf;
	int 	hop;
	char	*servername = parv[1];
	Client *cptr = client->direction; /* lazy, since this function may be removed soon */

	/* Only allow this command from server sockets */
	if (!IsServer(client->direction))
	{
		sendnumeric(client, ERR_NOTFORUSERS, "SID");
		return;
	}

	if (parc < 4 || BadPtr(parv[3]))
	{
		sendto_one(client, NULL, "ERROR :Not enough SID parameters");
		return;
	}

	/* Check if server already exists... */
	if ((acptr = find_server(servername, NULL)))
	{
		/* Found. Bad. Quit. */

		if (IsMe(acptr))
		{
			sendto_ops_and_log("Link %s rejected, server trying to link with my name (%s)",
				get_client_name(client, TRUE), me.name);
			sendto_one(client, NULL, "ERROR: Server %s exists (it's me!)", me.name);
			exit_client(client, NULL, "Server Exists");
			return;
		}

		// FIXME: verify this code:
		acptr = acptr->direction;
		ocptr = (cptr->local->firsttime > acptr->local->firsttime) ? acptr : cptr;
		acptr = (cptr->local->firsttime > acptr->local->firsttime) ? cptr : acptr;
		sendto_one(acptr, NULL,
		    "ERROR :Server %s already exists from %s",
		    servername,
		    (ocptr->direction ? ocptr->direction->name : "<nobody>"));
		sendto_ops_and_log
		    ("Link %s cancelled, server %s already exists from %s",
		    get_client_name(acptr, TRUE), servername,
		    (ocptr->direction ? ocptr->direction->name : "<nobody>"));
		exit_client(acptr, NULL, "Server Exists");
		return;
	}

	/* Check deny server { } */
	if ((bconf = find_ban(NULL, servername, CONF_BAN_SERVER)))
	{
		sendto_ops_and_log("Cancelling link %s, banned server %s",
			get_client_name(cptr, TRUE), servername);
		sendto_one(cptr, NULL, "ERROR :Banned server (%s)", bconf->reason ? bconf->reason : "no reason");
		exit_client(cptr, NULL, "Brought in banned server");
		return;
	}

	/* OK, let us check the data now */
	if (!valid_server_name(servername))
	{
		sendto_ops_and_log("Link %s introduced server with bad server name '%s' -- disconnecting",
		                   client->name, servername);
		exit_client(cptr, NULL, "Introduced server with bad server name");
		return;
	}

	hop = atol(parv[2]);
	if (hop < 2)
	{
		sendto_ops_and_log("Server %s introduced server %s with hop count of %d, while >1 was expected",
		                   client->name, servername, hop);
		exit_client(cptr, NULL, "ERROR :Invalid hop count");
		return;
	}

	if (!valid_sid(parv[3]))
	{
		sendto_ops_and_log("Server %s introduced server %s with invalid SID '%s' -- disconnecting",
		                   client->name, servername, parv[3]);
		exit_client(cptr, NULL, "ERROR :Invalid SID");
		return;
	}

	if (!cptr->serv->conf)
	{
		sendto_ops_and_log("Internal error: lost conf for %s!!, dropping link", cptr->name);
		exit_client(cptr, NULL, "Internal error: lost configuration");
		return;
	}

	aconf = cptr->serv->conf;

	if (!aconf->hub)
	{
		sendto_ops_and_log("Link %s cancelled, is Non-Hub but introduced Leaf %s",
			cptr->name, servername);
		exit_client(cptr, NULL, "Non-Hub Link");
		return;
	}

	if (!match_simple(aconf->hub, servername))
	{
		sendto_ops_and_log("Link %s cancelled, linked in %s, which hub config disallows",
			cptr->name, servername);
		exit_client(cptr, NULL, "Not matching hub configuration");
		return;
	}

	if (aconf->leaf)
	{
		if (!match_simple(aconf->leaf, servername))
		{
			sendto_ops_and_log("Link %s(%s) cancelled, disallowed by leaf configuration",
				cptr->name, servername);
			exit_client(cptr, NULL, "Disallowed by leaf configuration");
			return;
		}
	}

	if (aconf->leaf_depth && (hop > aconf->leaf_depth))
	{
		sendto_ops_and_log("Link %s(%s) cancelled, too deep depth",
			cptr->name, servername);
		exit_client(cptr, NULL, "Too deep link depth (leaf)");
		return;
	}

	/* All approved, add the server */
	acptr = make_client(cptr, find_server(client->name, cptr));
	strlcpy(acptr->name, servername, sizeof(acptr->name));
	acptr->hopcount = hop;
	strlcpy(acptr->id, parv[3], sizeof(acptr->id));
	strlcpy(acptr->info, parv[parc - 1], sizeof(acptr->info));
	make_server(acptr);
	acptr->serv->up = find_or_add(acptr->srvptr->name);
	SetServer(acptr);
	ircd_log(LOG_SERVER, "SERVER %s (from %s)", acptr->name, acptr->srvptr->name);
	/* If this server is U-lined, or the parent is, then mark it as U-lined */
	if (IsULine(client) || find_uline(acptr->name))
		SetULine(acptr);
	irccounts.servers++;
	find_or_add(acptr->name);
	add_client_to_list(acptr);
	add_to_client_hash_table(acptr->name, acptr);
	add_to_id_hash_table(acptr->id, acptr);
	list_move(&acptr->client_node, &global_server_list);

	RunHook(HOOKTYPE_SERVER_CONNECT, acptr);

	sendto_server(client, 0, 0, NULL, ":%s SID %s %d %s :%s",
		    acptr->srvptr->id, acptr->name, hop + 1, acptr->id, acptr->info);

	RunHook(HOOKTYPE_POST_SERVER_CONNECT, acptr);
}

void _introduce_user(Client *to, Client *acptr)
{
	build_umode_string(acptr, 0, SEND_UMODES, buf);

	sendto_one_nickcmd(to, acptr, buf);
	
	send_moddata_client(to, acptr);

	if (acptr->user->away)
		sendto_one(to, NULL, ":%s AWAY :%s", acptr->id, acptr->user->away);

	if (acptr->user->swhois)
	{
		SWhois *s;
		for (s = acptr->user->swhois; s; s = s->next)
		{
			if (CHECKSERVERPROTO(to, PROTO_EXTSWHOIS))
			{
				sendto_one(to, NULL, ":%s SWHOIS %s + %s %d :%s",
					me.id, acptr->name, s->setby, s->priority, s->line);
			} else
			{
				sendto_one(to, NULL, ":%s SWHOIS %s :%s",
					me.id, acptr->name, s->line);
			}
		}
	}
}

void tls_link_notification_verify(Client *acptr, ConfigItem_link *aconf)
{
	char *spki_fp;
	char *tls_fp;
	char *errstr = NULL;
	int verify_ok;

	if (!MyConnect(acptr) || !acptr->local->ssl || !aconf)
		return;

	if ((aconf->auth->type == AUTHTYPE_TLS_CLIENTCERT) ||
	    (aconf->auth->type == AUTHTYPE_TLS_CLIENTCERTFP) ||
	    (aconf->auth->type == AUTHTYPE_SPKIFP))
	{
		/* Link verified by certificate or SPKI */
		return;
	}

	if (aconf->verify_certificate)
	{
		/* Link verified by trust chain */
		return;
	}

	tls_fp = moddata_client_get(acptr, "certfp");
	spki_fp = spki_fingerprint(acptr);
	if (!tls_fp || !spki_fp)
		return; /* wtf ? */

	/* Only bother the user if we are linking to UnrealIRCd 4.0.16+,
	 * since only for these versions we can give precise instructions.
	 */
	if (!acptr->serv || acptr->serv->features.protocol < 4016)
		return;

	sendto_realops("You may want to consider verifying this server link.");
	sendto_realops("More information about this can be found on https://www.unrealircd.org/Link_verification");

	verify_ok = verify_certificate(acptr->local->ssl, aconf->servername, &errstr);
	if (errstr && strstr(errstr, "not valid for hostname"))
	{
		sendto_realops("Unfortunately the certificate of server '%s' has a name mismatch:", acptr->name);
		sendto_realops("%s", errstr);
		sendto_realops("This isn't a fatal error but it will prevent you from using verify-certificate yes;");
	} else
	if (!verify_ok)
	{
		sendto_realops("In short: in the configuration file, change the 'link %s {' block to use this as a password:", acptr->name);
		sendto_realops("password \"%s\" { spkifp; };", spki_fp);
		sendto_realops("And follow the instructions on the other side of the link as well (which will be similar, but will use a different hash)");
	} else
	{
		sendto_realops("In short: in the configuration file, add the following to your 'link %s {' block:", acptr->name);
		sendto_realops("verify-certificate yes;");
		sendto_realops("Alternatively, you could use SPKI fingerprint verification. Then change the password in the link block to be:");
		sendto_realops("password \"%s\" { spkifp; };", spki_fp);
	}
}

#define SafeStr(x)    ((x && *(x)) ? (x) : "*")

/** Broadcast SINFO.
 * @param cptr   The server to send the information about.
 * @param to     The server to send the information TO (NULL for broadcast).
 * @param except The direction NOT to send to.
 * This function takes into account that the server may not
 * provide all of the detailed info. If any information is
 * absent we will send 0 for numbers and * for NULL strings.
 */
void _broadcast_sinfo(Client *acptr, Client *to, Client *except)
{
	char chanmodes[128], buf[512];

	if (acptr->serv->features.chanmodes[0])
	{
		snprintf(chanmodes, sizeof(chanmodes), "%s,%s,%s,%s",
			 acptr->serv->features.chanmodes[0],
			 acptr->serv->features.chanmodes[1],
			 acptr->serv->features.chanmodes[2],
			 acptr->serv->features.chanmodes[3]);
	} else {
		strlcpy(chanmodes, "*", sizeof(chanmodes));
	}

	snprintf(buf, sizeof(buf), "%lld %d %s %s %s :%s",
		      (long long)acptr->serv->boottime,
		      acptr->serv->features.protocol,
		      SafeStr(acptr->serv->features.usermodes),
		      chanmodes,
		      SafeStr(acptr->serv->features.nickchars),
		      SafeStr(acptr->serv->features.software));

	if (to)
	{
		/* Targetted to one server */
		sendto_one(to, NULL, ":%s SINFO %s", acptr->id, buf);
	} else {
		/* Broadcast (except one side...) */
		sendto_server(except, 0, 0, NULL, ":%s SINFO %s", acptr->id, buf);
	}
}

int	server_sync(Client *cptr, ConfigItem_link *aconf)
{
	char		*inpath = get_client_name(cptr, TRUE);
	Client		*acptr;
	int incoming = IsUnknown(cptr) ? 1 : 0;

	ircd_log(LOG_SERVER, "SERVER %s", cptr->name);

	if (cptr->local->passwd)
	{
		safe_free(cptr->local->passwd);
		cptr->local->passwd = NULL;
	}
	if (incoming)
	{
		/* If this is an incomming connection, then we have just received
		 * their stuff and now send our stuff back.
		 */
		if (!IsEAuth(cptr)) /* if eauth'd then we already sent the passwd */
			sendto_one(cptr, NULL, "PASS :%s", (aconf->auth->type == AUTHTYPE_PLAINTEXT) ? aconf->auth->data : "*");

		send_proto(cptr, aconf);
		send_server_message(cptr);
	}

	/* Set up server structure */
	free_pending_net(cptr);
	SetServer(cptr);
	irccounts.me_servers++;
	irccounts.servers++;
	irccounts.unknown--;
	list_move(&cptr->client_node, &global_server_list);
	list_move(&cptr->lclient_node, &lclient_list);
	list_add(&cptr->special_node, &server_list);
	if (find_uline(cptr->name))
	{
		if (cptr->serv && cptr->serv->features.software && !strncmp(cptr->serv->features.software, "UnrealIRCd-", 11))
		{
			sendto_realops("\002WARNING:\002 Bad ulines! It seems your server is misconfigured: "
			               "your ulines { } block is matching an UnrealIRCd server (%s). "
			               "This is not correct and will cause security issues. "
			               "ULines should only be added for services! "
			               "See https://www.unrealircd.org/docs/FAQ#bad-ulines",
			               cptr->name);
		}
		SetULine(cptr);
	}
	find_or_add(cptr->name);
	if (IsSecure(cptr))
	{
		sendto_umode_global(UMODE_OPER,
			"(\2link\2) Secure link %s -> %s established (%s)",
			me.name, inpath, tls_get_cipher(cptr->local->ssl));
		tls_link_notification_verify(cptr, aconf);
	}
	else
	{
		sendto_umode_global(UMODE_OPER,
			"(\2link\2) Link %s -> %s established",
			me.name, inpath);
		/* Print out a warning if linking to a non-TLS server unless it's localhost.
		 * Yeah.. there are still other cases when non-TLS links are fine (eg: local IP
		 * of the same machine), we won't bother with detecting that. -- Syzop
		 */
		if (!IsLocalhost(cptr) && (iConf.plaintext_policy_server == POLICY_WARN))
		{
			sendto_realops("\002WARNING:\002 This link is unencrypted (not SSL/TLS). We highly recommend to use "
			               "SSL/TLS for server linking. See https://www.unrealircd.org/docs/Linking_servers");
		}
		if (IsSecure(cptr) && (iConf.outdated_tls_policy_server == POLICY_WARN) && outdated_tls_client(cptr))
		{
			sendto_realops("\002WARNING:\002 This link is using an outdated SSL/TLS protocol or cipher (%s).",
			               tls_get_cipher(cptr->local->ssl));
		}
	}
	add_to_client_hash_table(cptr->name, cptr);
	/* doesnt duplicate cptr->serv if allocted this struct already */
	make_server(cptr);
	cptr->serv->up = me.name;
	cptr->srvptr = &me;
	if (!cptr->serv->conf)
		cptr->serv->conf = aconf; /* Only set serv->conf to aconf if not set already! Bug #0003913 */
	if (incoming)
	{
		cptr->serv->conf->refcount++;
		Debug((DEBUG_ERROR, "reference count for %s (%s) is now %d",
			cptr->name, cptr->serv->conf->servername, cptr->serv->conf->refcount));
	}
	cptr->serv->conf->class->clients++;
	cptr->local->class = cptr->serv->conf->class;
	RunHook(HOOKTYPE_SERVER_CONNECT, cptr);

	/* Broadcast new server to the rest of the network */
	sendto_server(cptr, 0, 0, NULL, ":%s SID %s 2 %s :%s",
		    cptr->srvptr->id, cptr->name, cptr->id, cptr->info);

	/* Broadcast the just-linked-in featureset to other servers on our side */
	broadcast_sinfo(cptr, NULL, cptr);

	/* Send moddata of &me (if any, likely minimal) */
	send_moddata_client(cptr, &me);

	list_for_each_entry_reverse(acptr, &global_server_list, client_node)
	{
		/* acptr->direction == acptr for acptr == cptr */
		if (acptr->direction == cptr)
			continue;

		if (IsServer(acptr))
		{
			sendto_one(cptr, NULL, ":%s SID %s %d %s :%s",
			    acptr->srvptr->id,
			    acptr->name, acptr->hopcount + 1,
			    acptr->id, acptr->info);

			/* Also signal to the just-linked server which
			 * servers are fully linked.
			 * Now you might ask yourself "Why don't we just
			 * assume every server you get during link phase
			 * is fully linked?", well.. there's a race condition
			 * if 2 servers link (almost) at the same time,
			 * then you would think the other one is fully linked
			 * while in fact he was not.. -- Syzop.
			 */
			if (acptr->serv->flags.synced)
			{
				sendto_one(cptr, NULL, ":%s EOS", acptr->id);
#ifdef DEBUGMODE
				ircd_log(LOG_ERROR, "[EOSDBG] server_sync: sending to uplink '%s' with src %s...",
					cptr->name, acptr->name);
#endif
			}
			/* Send SINFO of our servers to their side */
			broadcast_sinfo(acptr, cptr, NULL);
			send_moddata_client(cptr, acptr); /* send moddata of server 'acptr' (if any, likely minimal) */
		}
	}

	/* Synching nick information */
	list_for_each_entry_reverse(acptr, &client_list, client_node)
	{
		/* acptr->direction == acptr for acptr == cptr */
		if (acptr->direction == cptr)
			continue;
		if (IsUser(acptr))
			introduce_user(cptr, acptr);
	}
	/*
	   ** Last, pass all channels plus statuses
	 */
	{
		Channel *channel;
		for (channel = channels; channel; channel = channel->nextch)
		{
			send_channel_modes_sjoin3(cptr, channel);
			if (channel->topic_time)
				sendto_one(cptr, NULL, "TOPIC %s %s %lld :%s",
				    channel->chname, channel->topic_nick,
				    (long long)channel->topic_time, channel->topic);
			send_moddata_channel(cptr, channel);
		}
	}
	
	/* Send ModData for all member(ship) structs */
	send_moddata_members(cptr);
	
	/* pass on TKLs */
	tkl_sync(cptr);

	RunHook(HOOKTYPE_SERVER_SYNC, cptr);

	sendto_one(cptr, NULL, "NETINFO %i %lld %i %s 0 0 0 :%s",
	    irccounts.global_max, (long long)TStime(), UnrealProtocol,
	    CLOAK_KEYCRC,
	    ircnetwork);

	/* Send EOS (End Of Sync) to the just linked server... */
	sendto_one(cptr, NULL, ":%s EOS", me.id);
#ifdef DEBUGMODE
	ircd_log(LOG_ERROR, "[EOSDBG] server_sync: sending to justlinked '%s' with src ME...",
			cptr->name);
#endif
	RunHook(HOOKTYPE_POST_SERVER_CONNECT, cptr);
	return 0;
}

/** This will send "to" a full list of the modes for channel channel,
 *
 * Half of it recoded by Syzop: the whole buffering and size checking stuff
 * looked weird and just plain inefficient. We now fill up our send-buffer
 * really as much as we can, without causing any overflows of course.
 */
void send_channel_modes_sjoin3(Client *to, Channel *channel)
{
	MessageTag *mtags = NULL;
	Member *members;
	Member *lp;
	Ban *ban;
	short nomode, nopara;
	char tbuf[512]; /* work buffer, for temporary data */
	char buf[1024]; /* send buffer */
	char *bufptr; /* points somewhere in 'buf' */
	char *p; /* points to somewhere in 'tbuf' */
	int prebuflen = 0; /* points to after the <sjointoken> <TS> <chan> <fixmodes> <fixparas <..>> : part */
	int sent = 0; /* we need this so we send at least 1 message about the channel (eg if +P and no members, no bans, #4459) */

	if (*channel->chname != '#')
		return;

	nomode = 0;
	nopara = 0;
	members = channel->members;

	/* First we'll send channel, channel modes and members and status */

	*modebuf = *parabuf = '\0';
	channel_modes(to, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), channel, 1);

	if (!modebuf[1])
		nomode = 1;
	if (!(*parabuf))
		nopara = 1;

	/* Generate a new message (including msgid).
	 * Due to the way SJOIN works, we will use the same msgid for
	 * multiple SJOIN messages to servers. Rest assured that clients
	 * will never see these duplicate msgid's though. They
	 * will see a 'special' version instead with a suffix.
	 */
	new_message(&me, NULL, &mtags);

	if (nomode && nopara)
	{
		ircsnprintf(buf, sizeof(buf),
		    ":%s SJOIN %lld %s :", me.id,
		    (long long)channel->creationtime, channel->chname);
	}
	if (nopara && !nomode)
	{
		ircsnprintf(buf, sizeof(buf),
		    ":%s SJOIN %lld %s %s :", me.id,
		    (long long)channel->creationtime, channel->chname, modebuf);
	}
	if (!nopara && !nomode)
	{
		ircsnprintf(buf, sizeof(buf),
		    ":%s SJOIN %lld %s %s %s :", me.id,
		    (long long)channel->creationtime, channel->chname, modebuf, parabuf);
	}

	prebuflen = strlen(buf);
	bufptr = buf + prebuflen;

	/* RULES:
	 * - Use 'tbuf' as a working buffer, use 'p' to advance in 'tbuf'.
	 *   Thus, be sure to do a 'p = tbuf' at the top of the loop.
	 * - When one entry has been build, check if strlen(buf) + strlen(tbuf) > BUFSIZE - 8,
	 *   if so, do not concat but send the current result (buf) first to the server
	 *   and reset 'buf' to only the prebuf part (all until the ':').
	 *   Then, in both cases, concat 'tbuf' to 'buf' and continue
	 * - Be sure to ALWAYS zero terminate (*p = '\0') when the entry has been build.
	 * - Be sure to add a space after each entry ;)
	 *
	 * For a more illustrated view, take a look at the first for loop, the others
	 * are pretty much the same.
	 *
	 * Follow these rules, and things would be smooth and efficient (network-wise),
	 * if you ignore them, expect crashes and/or heap corruption, aka: HELL.
	 * You have been warned.
	 *
	 * Side note: of course things would be more efficient if the prebuf thing would
	 * not be sent every time, but that's another story
	 *      -- Syzop
	 */

	for (lp = members; lp; lp = lp->next)
	{
		p = tbuf;
		if (lp->flags & MODE_CHANOP)
			*p++ = '@';
		if (lp->flags & MODE_VOICE)
			*p++ = '+';
		if (lp->flags & MODE_HALFOP)
			*p++ = '%';
		if (lp->flags & MODE_CHANOWNER)
			*p++ = '*';
		if (lp->flags & MODE_CHANADMIN)
			*p++ = '~';

		p = mystpcpy(p, lp->client->id);
		*p++ = ' ';
		*p = '\0';

		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(to, mtags, "%s", buf);
			sent++;
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	for (ban = channel->banlist; ban; ban = ban->next)
	{
		p = tbuf;
		if (SupportSJSBY(to))
			p += add_sjsby(p, ban->who, ban->when);
		*p++ = '&';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(to, mtags, "%s", buf);
			sent++;
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	for (ban = channel->exlist; ban; ban = ban->next)
	{
		p = tbuf;
		if (SupportSJSBY(to))
			p += add_sjsby(p, ban->who, ban->when);
		*p++ = '"';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(to, mtags, "%s", buf);
			sent++;
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	for (ban = channel->invexlist; ban; ban = ban->next)
	{
		p = tbuf;
		if (SupportSJSBY(to))
			p += add_sjsby(p, ban->who, ban->when);
		*p++ = '\'';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(to, mtags, "%s", buf);
			sent++;
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	if (buf[prebuflen] || !sent)
		sendto_one(to, mtags, "%s", buf);

	free_message_tags(mtags);
}

void server_generic_free(ModData *m)
{
	safe_free(m->ptr);
}

int server_post_connect(Client *client) {
	if (cfg.autoconnect_strategy == AUTOCONNECT_SEQUENTIAL_FALLBACK && last_autoconnect_server
		&& !strcmp(last_autoconnect_server, client->serv->conf->servername))
	{
		last_autoconnect_server = NULL;
	}
	return 0;
}
