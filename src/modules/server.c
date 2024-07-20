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

typedef struct ConfigItem_deny_link ConfigItem_deny_link;
struct ConfigItem_deny_link {
	ConfigItem_deny_link *prev, *next;
	ConfigFlag_except flag;
	ConfigItem_mask  *mask;
	CRuleNode *rule; /**< parsed crule */
	char *prettyrule; /**< human printable version */
	char *reason; /**< Reason for the deny link */
};

/* Forward declarations */
void server_config_setdefaults(cfgstruct *cfg);
void server_config_free();
int server_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int server_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
EVENT(server_autoconnect);
EVENT(server_handshake_timeout);
void send_channel_modes_sjoin3(Client *to, Channel *channel);
CMD_FUNC(cmd_server);
CMD_FUNC(cmd_sid);
ConfigItem_link *_verify_link(Client *client);
void _send_protoctl_servers(Client *client, int response);
void _send_server_message(Client *client);
void _introduce_user(Client *to, Client *acptr);
int _check_deny_version(Client *cptr, char *software, int protocol, char *flags);
void _broadcast_sinfo(Client *acptr, Client *to, Client *except);
int server_sync(Client *cptr, ConfigItem_link *conf, int incoming);
void tls_link_notification_verify(Client *acptr, ConfigItem_link *aconf);
void server_generic_free(ModData *m);
int server_post_connect(Client *client);
void _connect_server(ConfigItem_link *aconf, Client *by, struct hostent *hp);
static int connect_server_helper(ConfigItem_link *, Client *);
int _is_services_but_not_ulined(Client *client);
const char *_check_deny_link(ConfigItem_link *link, int auto_connect);
int server_stats_denylink_all(Client *client, const char *para);
int server_stats_denylink_auto(Client *client, const char *para);

/* Global variables */
static cfgstruct cfg;
static char *last_autoconnect_server = NULL;
static ConfigItem_deny_link *conf_deny_link = NULL;

ModuleHeader MOD_HEADER
  = {
	"server",
	"5.0",
	"command /server", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_PROTOCTL_SERVERS, _send_protoctl_servers);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_SERVER_MESSAGE, _send_server_message);
	EfunctionAddPVoid(modinfo->handle, EFUNC_VERIFY_LINK, TO_PVOIDFUNC(_verify_link));
	EfunctionAddVoid(modinfo->handle, EFUNC_INTRODUCE_USER, _introduce_user);
	EfunctionAdd(modinfo->handle, EFUNC_CHECK_DENY_VERSION, _check_deny_version);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_SINFO, _broadcast_sinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_CONNECT_SERVER, _connect_server);
	EfunctionAdd(modinfo->handle, EFUNC_IS_SERVICES_BUT_NOT_ULINED, _is_services_but_not_ulined);
	EfunctionAddConstString(modinfo->handle, EFUNC_CHECK_DENY_LINK, _check_deny_link);
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
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, server_stats_denylink_all);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, server_stats_denylink_auto);
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
	server_config_free();
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

void server_config_free(void)
{
	ConfigItem_deny_link *d, *d_next;

	for (d = conf_deny_link; d; d = d_next)
	{
		d_next = d->next;
		unreal_delete_masks(d->mask);
		safe_crule_free(d->rule);
		safe_free(d->prettyrule);
		safe_free(d->reason);
		DelListItem(d, conf_deny_link);
		safe_free(d);
	}
	conf_deny_link = NULL;
}

int server_config_test_set_server_linking(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->value)
		{
			config_error("%s:%i: blank set::server-linking::%s without value",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
			continue;
		} else
		if (!strcmp(cep->name, "autoconnect-strategy"))
		{
			if (autoconnect_strategy_strtoval(cep->value) < 0)
			{
				config_error("%s:%i: set::server-linking::autoconnect-strategy: invalid value '%s'. "
				             "Should be one of: parallel",
				             cep->file->filename, cep->line_number, cep->value);
				errors++;
				continue;
			}
		} else
		if (!strcmp(cep->name, "connect-timeout"))
		{
			long v = config_checkval(cep->value, CFG_TIME);
			if ((v < 5) || (v > 30))
			{
				config_error("%s:%i: set::server-linking::connect-timeout should be between 5 and 60 seconds",
					cep->file->filename, cep->line_number);
				errors++;
				continue;
			}
		} else
		if (!strcmp(cep->name, "handshake-timeout"))
		{
			long v = config_checkval(cep->value, CFG_TIME);
			if ((v < 10) || (v > 120))
			{
				config_error("%s:%i: set::server-linking::handshake-timeout should be between 10 and 120 seconds",
					cep->file->filename, cep->line_number);
				errors++;
				continue;
			}
		} else
		{
			config_error("%s:%i: unknown directive set::server-linking::%s",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
			continue;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int server_config_run_set_server_linking(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "autoconnect-strategy"))
		{
			cfg.autoconnect_strategy = autoconnect_strategy_strtoval(cep->value);
		} else
		if (!strcmp(cep->name, "connect-timeout"))
		{
			cfg.connect_timeout = config_checkval(cep->value, CFG_TIME);
		} else
		if (!strcmp(cep->name, "handshake-timeout"))
		{
			cfg.handshake_timeout = config_checkval(cep->value, CFG_TIME);
		}
	}

	return 1;
}

int server_config_test_deny_link(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
        int errors = 0;
        ConfigEntry *cep;
	char has_mask = 0, has_rule = 0, has_type = 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->items)
		{
			if (config_is_blankorempty(cep, "deny link"))
			{
				errors++;
				continue;
			}
			else if (!strcmp(cep->name, "mask"))
			{
				has_mask = 1;
			} else if (!strcmp(cep->name, "rule"))
			{
				int val = 0;
				if (has_rule)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "deny link::rule");
					continue;
				}
				has_rule = 1;
				if ((val = crule_test(cep->value)))
				{
					config_error("%s:%i: deny link::rule contains an invalid expression: %s",
						cep->file->filename,
						cep->line_number,
						crule_errstring(val));
					errors++;
				}
			}
			else if (!strcmp(cep->name, "type"))
			{
				if (has_type)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "deny link::type");
					continue;
				}
				has_type = 1;
				if (!strcmp(cep->value, "auto"))
				;
				else if (!strcmp(cep->value, "all"))
				;
				else {
					config_status("%s:%i: unknown deny link type",
					cep->file->filename, cep->line_number);
					errors++;
				}
			} else if (!strcmp(cep->name, "reason"))
			{
			}
			else
			{
				config_error_unknown(cep->file->filename,
					cep->line_number, "deny link", cep->name);
				errors++;
			}
		}
		else
		{
			// Sections
			if (!strcmp(cep->name, "mask"))
			{
				if (cep->value || cep->items)
					has_mask = 1;
			}
			else
			{
				config_error_unknown(cep->file->filename,
					cep->line_number, "deny link", cep->name);
				errors++;
				continue;
			}
		}
	}
	if (!has_mask)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"deny link::mask");
		errors++;
	}
	if (!has_rule)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"deny link::rule");
		errors++;
	}
	if (!has_type)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"deny link::type");
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int server_config_run_deny_link(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;
	ConfigItem_deny_link *deny;

	deny = safe_alloc(sizeof(ConfigItem_deny_link));

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "mask"))
		{
			unreal_add_masks(&deny->mask, cep);
		}
		else if (!strcmp(cep->name, "rule"))
		{
			deny->rule = crule_parse(cep->value);
			safe_strdup(deny->prettyrule, cep->value);
		}
		else if (!strcmp(cep->name, "reason"))
		{
			safe_strdup(deny->reason, cep->value);
		}
		else if (!strcmp(cep->name, "type")) {
			if (!strcmp(cep->value, "all"))
				deny->flag.type = CRULE_ALL;
			else if (!strcmp(cep->value, "auto"))
				deny->flag.type = CRULE_AUTO;
		}
	}

	/* Set a default reason, if needed */
	if (!deny->reason)
		safe_strdup(deny->reason, "Denied");

	AddListItem(deny, conf_deny_link);
	return 1;
}

int server_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	if ((type == CONFIG_SET) && !strcmp(ce->name, "server-linking"))
		return server_config_test_set_server_linking(cf, ce, type, errs);

	if ((type == CONFIG_DENY) && !strcmp(ce->value, "link"))
		return server_config_test_deny_link(cf, ce, type, errs);

	return 0; /* not for us */
}

int server_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	if ((type == CONFIG_SET) && ce && !strcmp(ce->name, "server-linking"))
		return server_config_run_set_server_linking(cf, ce, type);

	if ((type == CONFIG_DENY) && !strcmp(ce->value, "link"))
		return server_config_run_deny_link(cf, ce, type);

	return 0; /* not for us */
}

int server_needs_linking(ConfigItem_link *aconf)
{
	Client *client;
	ConfigItem_class *class;

	/* We're only interested in autoconnect blocks that also have
	 * a valid link::outgoing configuration. We also ignore
	 * temporary link blocks (not that they should exist...).
	 */
	if (!(aconf->outgoing.options & CONNECT_AUTO) ||
	    (!aconf->outgoing.hostname && !aconf->outgoing.file) ||
	    (aconf->flag.temporary == 1))
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
	if (check_deny_link(aconf, 1))
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

		connect_server(aconf, NULL, NULL);
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
		if (client->server && *client->server->by && client->local->creationtime &&
		    (IsConnecting(client) || IsTLSConnectHandshake(client) || !IsSynched(client)))
		{
			return 1;
		}
	}

	list_for_each_entry(client, &server_list, special_node)
	{
		if (client->server && *client->server->by && client->local->creationtime &&
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
	connect_server(aconf, NULL, NULL);
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
		if (!client->server || !*client->server->by || !client->local->creationtime)
			continue;

		/* Handle set::server-linking::connect-timeout */
		if ((IsConnecting(client) || IsTLSConnectHandshake(client)) &&
		    ((TStime() - client->local->creationtime) >= cfg.connect_timeout))
		{
			/* If this is a connect timeout to an outgoing server then notify ops & log it */
			unreal_log(ULOG_INFO, "link", "LINK_CONNECT_TIMEOUT", client,
			           "Connect timeout while trying to link to server '$client' ($client.ip)");

			exit_client(client, NULL, "Connection timeout");
			continue;
		}

		/* Handle set::server-linking::handshake-timeout */
		if ((TStime() - client->local->creationtime) >= cfg.handshake_timeout)
		{
			/* If this is a handshake timeout to an outgoing server then notify ops & log it */
			unreal_log(ULOG_INFO, "link", "LINK_HANDSHAKE_TIMEOUT", client,
			           "Connect handshake timeout while trying to link to server '$client' ($client.ip)");

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

	sendto_one(client, NULL, "PROTOCTL EAUTH=%s,%d,%s%s,UnrealIRCd-%s",
		me.name, UnrealProtocol, serveropts, extraflags ? extraflags : "", buildid);
		
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
	if (client->server && client->server->flags.server_sent)
	{
#ifdef DEBUGMODE
		abort();
#endif
		return;
	}

	sendto_one(client, NULL, "SERVER %s 1 :U%d-%s%s-%s %s",
		me.name, UnrealProtocol, serveropts, extraflags ? extraflags : "", me.id, me.info);

	if (client->server)
		client->server->flags.server_sent = 1;
}

#define LINK_DEFAULT_ERROR_MSG "Link denied (No link block found with your server name or link::incoming::mask did not match)"

/** Verify server link.
 * This does authentication and authorization checks.
 * @param client The client which issued the command
 * @returns On successfull authentication, the link block is returned. On failure NULL is returned (client has been killed!).
 */
ConfigItem_link *_verify_link(Client *client)
{
	ConfigItem_link *link, *orig_link;
	Client *acptr = NULL, *ocptr = NULL;
	ConfigItem_ban *bconf;

	/* We set the sockhost here so you can have incoming masks based on hostnames.
	 * Perhaps a bit late to do it here, but does anyone care?
	 */
	if (client->local->hostp && client->local->hostp->h_name)
		set_sockhost(client, client->local->hostp->h_name);

	if (!client->local->passwd)
	{
		unreal_log(ULOG_ERROR, "link", "LINK_DENIED_NO_PASSWORD", client,
			   "Link with server $client.details denied: No password provided. Protocol error.");
		exit_client(client, NULL, "Missing password");
		return NULL;
	}

	if (client->server && client->server->conf)
	{
		/* This is an outgoing connect so we already know what link block we are
		 * dealing with. It's the one in: client->server->conf
		 */

		/* Actually we still need to double check the servername to avoid confusion. */
		if (strcasecmp(client->name, client->server->conf->servername))
		{
			unreal_log(ULOG_ERROR, "link", "LINK_DENIED_SERVERNAME_MISMATCH", client,
			           "Link with server $client.details denied: "
			           "Outgoing connect from link block '$link_block' but server "
			           "introduced itself as '$client'. Server name mismatch.",
			           log_data_link_block(client->server->conf));
			exit_client_fmt(client, NULL, "Servername (%s) does not match name in my link block (%s)",
			                client->name, client->server->conf->servername);
			return NULL;
		}
		link = client->server->conf;
		goto skip_host_check;
	} else {
		/* Hunt the linkblock down ;) */
		link = find_link(client->name);
	}
	
	if (!link)
	{
		unreal_log(ULOG_ERROR, "link", "LINK_DENIED_UNKNOWN_SERVER", client,
		           "Link with server $client.details denied: No link block named '$client'");
		exit_client(client, NULL, LINK_DEFAULT_ERROR_MSG);
		return NULL;
	}
	
	if (!link->incoming.match)
	{
		unreal_log(ULOG_ERROR, "link", "LINK_DENIED_NO_INCOMING", client,
		           "Link with server $client.details denied: Link block exists, but there is no link::incoming::match set.",
		           log_data_link_block(link));
		exit_client(client, NULL, LINK_DEFAULT_ERROR_MSG);
		return NULL;
	}

	orig_link = link;
	if (!user_allowed_by_security_group(client, link->incoming.match))
	{
		unreal_log(ULOG_ERROR, "link", "LINK_DENIED_INCOMING_MASK_MISMATCH", client,
		           "Link with server $client.details denied: Server is in link block but link::incoming::mask didn't match",
		           log_data_link_block(orig_link));
		exit_client(client, NULL, LINK_DEFAULT_ERROR_MSG);
		return NULL;
	}

skip_host_check:
	/* Try to authenticate the server... */
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
			unreal_log(ULOG_ERROR, "link", "LINK_DENIED_AUTH_FAILED", client,
			           "Link with server $client.details denied: Authentication failed: $auth_failure_msg",
			           log_data_string("auth_failure_msg", "different password types on both sides of the link\n"
			                                               "Read https://www.unrealircd.org/docs/FAQ#auth-fail-mixed for more information"),
			           log_data_link_block(link));
		} else
		if (link->auth->type == AUTHTYPE_SPKIFP)
		{
			unreal_log(ULOG_ERROR, "link", "LINK_DENIED_AUTH_FAILED", client,
			           "Link with server $client.details denied: Authentication failed: $auth_failure_msg",
			           log_data_string("auth_failure_msg", "spkifp mismatch"),
			           log_data_link_block(link));
		} else
		if (link->auth->type == AUTHTYPE_TLS_CLIENTCERT)
		{
			unreal_log(ULOG_ERROR, "link", "LINK_DENIED_AUTH_FAILED", client,
			           "Link with server $client.details denied: Authentication failed: $auth_failure_msg",
			           log_data_string("auth_failure_msg", "tlsclientcert mismatch"),
			           log_data_link_block(link));
		} else
		if (link->auth->type == AUTHTYPE_TLS_CLIENTCERTFP)
		{
			unreal_log(ULOG_ERROR, "link", "LINK_DENIED_AUTH_FAILED", client,
			           "Link with server $client.details denied: Authentication failed: $auth_failure_msg",
			           log_data_string("auth_failure_msg", "certfp mismatch"),
			           log_data_link_block(link));
		} else
		{
			unreal_log(ULOG_ERROR, "link", "LINK_DENIED_AUTH_FAILED", client,
			           "Link with server $client.details denied: Authentication failed: $auth_failure_msg",
			           log_data_string("auth_failure_msg", "bad password"),
			           log_data_link_block(link));
		}
		exit_client(client, NULL, "Link denied (Authentication failed)");
		return NULL;
	}

	/* Verify the TLS certificate (if requested) */
	if (link->verify_certificate)
	{
		char *errstr = NULL;

		if (!IsTLS(client))
		{
			unreal_log(ULOG_ERROR, "link", "LINK_DENIED_VERIFY_CERTIFICATE_FAILED", client,
			           "Link with server $client.details denied: verify-certificate failed: $certificate_failure_msg",
			           log_data_string("certificate_failure_msg", "not using TLS"),
			           log_data_link_block(link));
			exit_client(client, NULL, "Link denied (Not using TLS)");
			return NULL;
		}
		if (!verify_certificate(client->local->ssl, link->servername, &errstr))
		{
			unreal_log(ULOG_ERROR, "link", "LINK_DENIED_VERIFY_CERTIFICATE_FAILED", client,
			           "Link with server $client.details denied: verify-certificate failed: $certificate_failure_msg",
			           log_data_string("certificate_failure_msg", errstr),
			           log_data_link_block(link));
			exit_client(client, NULL, "Link denied (Certificate verification failed)");
			return NULL;
		}
	}

	if ((bconf = find_ban(NULL, client->name, CONF_BAN_SERVER)))
	{
		unreal_log(ULOG_ERROR, "link", "LINK_DENIED_SERVER_BAN", client,
		           "Link with server $client.details denied: "
		           "Server is banned ($ban_reason)",
		           log_data_string("ban_reason", bconf->reason),
		           log_data_link_block(link));
		exit_client_fmt(client, NULL, "Banned server: %s", bconf->reason);
		return NULL;
	}

	if (link->class->clients + 1 > link->class->maxclients)
	{
		unreal_log(ULOG_ERROR, "link", "LINK_DENIED_CLASS_FULL", client,
		           "Link with server $client.details denied: "
		           "class '$link_block.class' is full",
		           log_data_link_block(link));
		exit_client(client, NULL, "Full class");
		return NULL;
	}
	if (!IsLocalhost(client) && (iConf.plaintext_policy_server == POLICY_DENY) && !IsSecure(client))
	{
		unreal_log(ULOG_ERROR, "link", "LINK_DENIED_NO_TLS", client,
		           "Link with server $client.details denied: "
		           "Server needs to use TLS (set::plaintext-policy::server is 'deny')\n"
		           "See https://www.unrealircd.org/docs/FAQ#server-requires-tls",
		           log_data_link_block(link));
		exit_client(client, NULL, "Servers need to use TLS (set::plaintext-policy::server is 'deny')");
		return NULL;
	}
	if (IsSecure(client) && (iConf.outdated_tls_policy_server == POLICY_DENY) && outdated_tls_client(client))
	{
		unreal_log(ULOG_ERROR, "link", "LINK_DENIED_OUTDATED_TLS", client,
		           "Link with server $client.details denied: "
		           "Server is using an outdated TLS protocol or cipher ($tls_cipher) and set::outdated-tls-policy::server is 'deny'.\n"
		           "See https://www.unrealircd.org/docs/FAQ#server-outdated-tls",
		           log_data_link_block(link),
			   log_data_string("tls_cipher", tls_get_cipher(client)));
		exit_client(client, NULL, "Server using outdates TLS protocol or cipher (set::outdated-tls-policy::server is 'deny')");
		return NULL;
	}
	/* This one is at the end, because it causes us to delink another server,
	 * so we want to be (reasonably) sure that this one will succeed before
	 * breaking the other one.
	 */
	if ((acptr = find_server(client->name, NULL)))
	{
		if (IsMe(acptr))
		{
			unreal_log(ULOG_ERROR, "link", "LINK_DENIED_SERVER_EXISTS", client,
			           "Link with server $client.details denied: "
			           "Server is trying to link with my name ($me_name)",
			           log_data_string("me_name", me.name),
			           log_data_link_block(link));
			exit_client(client, NULL, "Server Exists (server trying to link with same name as myself)");
			return NULL;
		} else {
			unreal_log(ULOG_ERROR, "link", "LINK_DROPPED_REINTRODUCED", client,
				   "Link with server $client.details causes older link "
				   "with same server via $existing_client.server.uplink to be dropped.",
				   log_data_client("existing_client", acptr),
			           log_data_link_block(link));
			exit_client_ex(acptr, client->direction, NULL, "Old link dropped, resyncing");
		}
	}

	return link;
}

/** Server command. Only for locally connected servers!!.
 * parv[1] = server name
 * parv[2] = hop count
 * parv[3] = server description, may include protocol version and other stuff too (VL)
 */
CMD_FUNC(cmd_server)
{
	const char *servername = NULL;	/* Pointer for servername */
	char *ch = NULL;	/* */
	char descbuf[BUFSIZE];
	int  hop = 0;
	char info[REALLEN + 61];
	ConfigItem_link *aconf = NULL;
	char *flags = NULL, *protocol = NULL, *inf = NULL, *num = NULL;
	int incoming;
	const char *err;

	if (IsUser(client))
	{
		sendnumeric(client, ERR_ALREADYREGISTRED);
		sendnotice(client, "*** Sorry, but your IRC program doesn't appear to support changing servers.");
		return;
	}

	if (parc < 4 || (!*parv[3]))
	{
		exit_client(client, NULL,  "Not enough SERVER parameters");
		return;
	}

	servername = parv[1];

	/* Remote 'SERVER' command is not possible on a 100% SID network */
	if (!MyConnect(client))
	{
		unreal_log(ULOG_ERROR, "link", "LINK_OLD_PROTOCOL", client,
		           "Server link $client tried to introduce $servername using SERVER command. "
		           "Server is using an old and unsupported protocol from UnrealIRCd 3.2.x or earlier. "
		           "See https://www.unrealircd.org/docs/FAQ#old-server-protocol",
		           log_data_string("servername", servername));
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
		exit_client(client, NULL, "Bogus server name");
		return;
	}

	if (!client->local->passwd)
	{
		exit_client(client, NULL, "Missing password");
		return;
	}

	/* We set the client->name early here, even though it is not authenticated yet.
	 * Reason is that it makes the notices and logging more useful.
	 * This should be safe as it is not in the server linked list yet or hash table.
	 * CMTSRV941 -- Syzop.
	 */
	strlcpy(client->name, servername, sizeof(client->name));

	if (!(aconf = verify_link(client)))
		return; /* Rejected */

	/* From this point the server is authenticated, so we can be more verbose
	 * with notices to ircops and in exit_client() and such.
	 */


	if (strlen(client->id) != 3)
	{
		unreal_log(ULOG_ERROR, "link", "LINK_OLD_PROTOCOL", client,
		           "Server link $servername rejected. Server is using an old and unsupported protocol from UnrealIRCd 3.2.x or earlier. "
		           "See https://www.unrealircd.org/docs/FAQ#old-server-protocol",
		           log_data_string("servername", servername));
		exit_client(client, NULL, "Server using old unsupported protocol from UnrealIRCd 3.2.x or earlier. "
		                          "See https://www.unrealircd.org/docs/FAQ#old-server-protocol");
		return;
	}

	hop = atol(parv[2]);
	if (hop != 1)
	{
		unreal_log(ULOG_ERROR, "link", "LINK_DENIED_INVALID_HOPCOUNT", client,
		           "Server link $servername rejected. Directly linked server provided a hopcount of $hopcount, while 1 was expected.",
		           log_data_string("servername", servername),
		           log_data_integer("hopcount", hop));
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

	if ((err = check_deny_link(aconf, 0)))
	{
		unreal_log(ULOG_ERROR, "link", "LINK_DENIED_DENY_LINK_BLOCK", client,
			   "Server link $servername rejected by deny link { } block: $reason",
			   log_data_string("servername", servername),
			   log_data_string("reason", err));
		exit_client_fmt(client, NULL, "Disallowed by connection rule: %s", err);
		return;
	}

	if (aconf->options & CONNECT_QUARANTINE)
		SetQuarantined(client);

	ircsnprintf(descbuf, sizeof descbuf, "Server: %s", servername);
	fd_desc(client->local->fd, descbuf);

	incoming = IsUnknown(client) ? 1 : 0;

	if (client->local->passwd)
		safe_free(client->local->passwd);

	/* Set up server structure */
	free_pending_net(client);
	SetServer(client);
	irccounts.me_servers++;
	irccounts.servers++;
	irccounts.unknown--;
	list_move(&client->client_node, &global_server_list);
	list_move(&client->lclient_node, &lclient_list);
	list_add(&client->special_node, &server_list);

	if (find_uline(client->name))
	{
		if (client->server && client->server->features.software && !strncmp(client->server->features.software, "UnrealIRCd-", 11))
		{
			unreal_log(ULOG_ERROR, "link", "BAD_ULINES", client,
			           "Bad ulines! Server $client matches your ulines { } block, but this server "
			           "is an UnrealIRCd server. UnrealIRCd servers should never be ulined as it "
			           "causes security issues. Ulines should only be added for services! "
			           "See https://www.unrealircd.org/docs/FAQ#bad-ulines.");
			exit_client(client, NULL, "Bad ulines. See https://www.unrealircd.org/docs/FAQ#bad-ulines");
		}
		SetULine(client);
	}

	find_or_add(client->name);

	if (IsSecure(client))
	{
		unreal_log(ULOG_INFO, "link", "SERVER_LINKED", client,
		           "Server linked: $me -> $client [secure: $tls_cipher]",
		           log_data_string("tls_cipher", tls_get_cipher(client)),
		           log_data_client("me", &me));
		tls_link_notification_verify(client, aconf);
	}
	else
	{
		unreal_log(ULOG_INFO, "link", "SERVER_LINKED", client,
		           "Server linked: $me -> $client",
		           log_data_client("me", &me));
		/* Print out a warning if linking to a non-TLS server unless it's localhost.
		 * Yeah.. there are still other cases when non-TLS links are fine (eg: local IP
		 * of the same machine), we won't bother with detecting that. -- Syzop
		 */
		if (!IsLocalhost(client) && (iConf.plaintext_policy_server == POLICY_WARN))
		{
			unreal_log(ULOG_WARNING, "link", "LINK_WARNING_NO_TLS", client,
				   "Link with server $client.details is unencrypted (not TLS). "
				   "We highly recommend to use TLS for server linking. "
				   "See https://www.unrealircd.org/docs/Linking_servers",
				   log_data_link_block(aconf));
		}
		if (IsSecure(client) && (iConf.outdated_tls_policy_server == POLICY_WARN) && outdated_tls_client(client))
		{
			unreal_log(ULOG_WARNING, "link", "LINK_WARNING_OUTDATED_TLS", client,
				   "Link with server $client.details is using an outdated "
				   "TLS protocol or cipher ($tls_cipher).",
				   log_data_link_block(aconf),
				   log_data_string("tls_cipher", tls_get_cipher(client)));
		}
	}

	add_to_client_hash_table(client->name, client);
	/* doesnt duplicate client->server if allocted this struct already */
	make_server(client);
	client->uplink = &me;
	if (!client->server->conf)
		client->server->conf = aconf; /* Only set serv->conf to aconf if not set already! Bug #0003913 */
	if (incoming)
		client->server->conf->refcount++;
	client->server->conf->class->clients++;
	client->local->class = client->server->conf->class;

	server_sync(client, aconf, incoming);
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
	int hop;
	const char *servername = parv[1];
	Client *direction = client->direction; /* lazy, since this function may be removed soon */

	/* Only allow this command from server sockets */
	if (!IsServer(client->direction))
	{
		sendnumeric(client, ERR_NOTFORUSERS, "SID");
		return;
	}

	if (parc < 4 || BadPtr(parv[3]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SID");
		return;
	}

	/* The SID check is done early because we do all the killing by SID,
	 * so we want to know if that won't work first.
	 */
	if (!valid_sid(parv[3]))
	{
		unreal_log(ULOG_ERROR, "link", "REMOTE_LINK_DENIED_INVALID_SID", client,
			   "Denied remote server $servername which was introduced by $client: "
			   "Invalid SID.",
			   log_data_string("servername", servername),
			   log_data_string("sid", parv[3]));
		/* Since we cannot SQUIT via SID (since it is invalid), this gives
		 * us huge doubts about the accuracy of the uplink, so in this case
		 * we terminate the entire uplink.
		 */
		exit_client(client, NULL, "Trying to introduce a server with an invalid SID");
		return;
	}

	/* Check if server already exists... */
	if ((acptr = find_server(servername, NULL)))
	{
		/* Found. Bad. Quit. */

		if (IsMe(acptr))
		{
			/* This should never happen, not even due to a race condition.
			 * We cannot send SQUIT here either since it is unclear what
			 * side would be squitted.
			 * As said, not really important, as this does not happen anyway.
			 */
			unreal_log(ULOG_ERROR, "link", "REMOTE_LINK_DENIED_DUPLICATE_SERVER_IS_ME", client,
			           "Denied remote server $servername which was introduced by $client: "
			           "Server is using our servername, this should be impossible!",
			           log_data_string("servername", servername));
			sendto_one(client, NULL, "ERROR: Server %s exists (it's me!)", me.name);
			exit_client(client, NULL, "Server Exists");
			return;
		}

		unreal_log(ULOG_ERROR, "link", "REMOTE_LINK_DENIED_DUPLICATE_SERVER", client,
			   "Denied remote server $servername which was introduced by $client: "
			   "Already linked via $existing_client.server.uplink.",
			   log_data_string("servername", servername),
			   log_data_client("existing_client", acptr));
		// FIXME: oldest should die.
		// FIXME: code below looks wrong, it checks direction TS instead of anything else
		acptr = acptr->direction;
		ocptr = (direction->local->creationtime > acptr->local->creationtime) ? acptr : direction;
		acptr = (direction->local->creationtime > acptr->local->creationtime) ? direction : acptr;
		// FIXME: Wait, this kills entire acptr? Without sending SQUIT even :D
		exit_client(acptr, NULL, "Server Exists");
		return;
	}

	if ((acptr = find_client(parv[3], NULL)))
	{
		unreal_log(ULOG_ERROR, "link", "LINK_DENIED_DUPLICATE_SID_SERVER", client,
			   "Denied server $servername with SID $sid: Server with SID $existing_client.id ($existing_client) is already linked.",
			   log_data_string("servername", servername),
			   log_data_string("sid", parv[3]),
			   log_data_client("existing_client", acptr));
		sendto_one(client, NULL, "SQUIT %s :Server with this SID (%s) already exists (%s)", parv[3], parv[3], acptr->name);
		return;
	}

	/* Check deny server { } */
	if ((bconf = find_ban(NULL, servername, CONF_BAN_SERVER)))
	{
		unreal_log(ULOG_ERROR, "link", "REMOTE_LINK_DENIED_SERVER_BAN", client,
		           "Denied remote server $servername which was introduced by $client: "
		           "Server is banned ($ban_reason)",
		           log_data_string("servername", servername),
		           log_data_string("ban_reason", bconf->reason));
		/* Before UnrealIRCd 6 this would SQUIT the server who introduced
		 * this server. That seems a bit of an overreaction, so we now
		 * send a SQUIT instead.
		 */
		sendto_one(client, NULL, "SQUIT %s :Banned server: %s", parv[3], bconf->reason);
		return;
	}

	/* OK, let us check the data now */
	if (!valid_server_name(servername))
	{
		unreal_log(ULOG_ERROR, "link", "REMOTE_LINK_DENIED_INVALID_SERVERNAME", client,
			   "Denied remote server $servername which was introduced by $client: "
			   "Invalid server name.",
			   log_data_string("servername", servername));
		sendto_one(client, NULL, "SQUIT %s :Invalid servername", parv[3]);
		return;
	}

	hop = atoi(parv[2]);
	if (hop < 2)
	{
		unreal_log(ULOG_ERROR, "link", "REMOTE_LINK_DENIED_INVALID_HOP_COUNT", client,
			   "Denied remote server $servername which was introduced by $client: "
			   "Invalid server name.",
			   log_data_string("servername", servername),
			   log_data_integer("hop_count", hop));
		sendto_one(client, NULL, "SQUIT %s :Invalid hop count (%d)", parv[3], hop);
		return;
	}

	if (!client->direction->server->conf)
	{
		unreal_log(ULOG_ERROR, "link", "BUG_LOST_CONFIG", client,
			   "[BUG] Lost link conf record for link $direction.",
			   log_data_client("direction", direction));
		exit_client(client->direction, NULL, "BUG: lost link configuration");
		return;
	}

	aconf = client->direction->server->conf;

	if (!aconf->hub)
	{
		unreal_log(ULOG_ERROR, "link", "REMOTE_LINK_DENIED_NO_HUB", client,
			   "Denied remote server $servername which was introduced by $client: "
			   "Server may not introduce this server ($direction is not a hub).",
			   log_data_string("servername", servername),
			   log_data_client("direction", client->direction));
		sendto_one(client, NULL, "SQUIT %s :Server is not permitted to be a hub: %s",
			parv[3], client->direction->name);
		return;
	}

	if (!match_simple(aconf->hub, servername))
	{
		unreal_log(ULOG_ERROR, "link", "REMOTE_LINK_DENIED_NO_MATCHING_HUB", client,
			   "Denied remote server $servername which was introduced by $client: "
			   "Server may not introduce this server ($direction hubmask does not allow it).",
			   log_data_string("servername", servername),
			   log_data_client("direction", client->direction));
		sendto_one(client, NULL, "SQUIT %s :Hub config for %s does not allow introducing this server",
			parv[3], client->direction->name);
		return;
	}

	if (aconf->leaf)
	{
		if (!match_simple(aconf->leaf, servername))
		{
			unreal_log(ULOG_ERROR, "link", "REMOTE_LINK_DENIED_NO_MATCHING_LEAF", client,
				   "Denied remote server $servername which was introduced by $client: "
				   "Server may not introduce this server ($direction leaf config does not allow it).",
				   log_data_string("servername", servername),
				   log_data_client("direction", client->direction));
			sendto_one(client, NULL, "SQUIT %s :Leaf config for %s does not allow introducing this server",
				parv[3], client->direction->name);
			return;
		}
	}

	if (aconf->leaf_depth && (hop > aconf->leaf_depth))
	{
		unreal_log(ULOG_ERROR, "link", "REMOTE_LINK_DENIED_LEAF_DEPTH", client,
			   "Denied remote server $servername which was introduced by $client: "
			   "Server may not introduce this server ($direction leaf depth config does not allow it).",
			   log_data_string("servername", servername),
			   log_data_client("direction", client->direction));
		sendto_one(client, NULL, "SQUIT %s :Leaf depth config for %s does not allow introducing this server",
			parv[3], client->direction->name);
		return;
	}

	/* All approved, add the server */
	acptr = make_client(direction, find_server(client->name, direction));
	strlcpy(acptr->name, servername, sizeof(acptr->name));
	acptr->hopcount = hop;
	strlcpy(acptr->id, parv[3], sizeof(acptr->id));
	strlcpy(acptr->info, parv[parc - 1], sizeof(acptr->info));
	make_server(acptr);
	SetServer(acptr);
	/* If this server is U-lined, or the parent is, then mark it as U-lined */
	if (IsULine(client) || find_uline(acptr->name))
		SetULine(acptr);
	irccounts.servers++;
	find_or_add(acptr->name);
	add_client_to_list(acptr);
	add_to_client_hash_table(acptr->name, acptr);
	add_to_id_hash_table(acptr->id, acptr);
	list_move(&acptr->client_node, &global_server_list);

	if (IsULine(client->direction) || IsSynched(client->direction))
	{
		/* Log these (but don't show when still syncing) */
		unreal_log(ULOG_INFO, "link", "SERVER_LINKED_REMOTE", acptr,
			   "Server linked: $client -> $other_server",
			   log_data_client("other_server", client));
	}

	RunHook(HOOKTYPE_SERVER_CONNECT, acptr);

	sendto_server(client, 0, 0, NULL, ":%s SID %s %d %s :%s",
		    acptr->uplink->id, acptr->name, hop + 1, acptr->id, acptr->info);

	RunHook(HOOKTYPE_POST_SERVER_CONNECT, acptr);
}

void _introduce_user(Client *to, Client *acptr)
{
	char buf[512];

	build_umode_string(acptr, 0, SEND_UMODES, buf);

	sendto_one_nickcmd(to, NULL, acptr, buf);
	
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

	if (acptr->server->features.chanmodes[0])
	{
		snprintf(chanmodes, sizeof(chanmodes), "%s,%s,%s,%s",
			 acptr->server->features.chanmodes[0],
			 acptr->server->features.chanmodes[1],
			 acptr->server->features.chanmodes[2],
			 acptr->server->features.chanmodes[3]);
	} else {
		strlcpy(chanmodes, "*", sizeof(chanmodes));
	}

	snprintf(buf, sizeof(buf), "%lld %d %s %s %s :%s",
		      (long long)acptr->server->boottime,
		      acptr->server->features.protocol,
		      SafeStr(acptr->server->features.usermodes),
		      chanmodes,
		      SafeStr(acptr->server->features.nickchars),
		      SafeStr(acptr->server->features.software));

	if (to)
	{
		/* Targetted to one server */
		sendto_one(to, NULL, ":%s SINFO %s", acptr->id, buf);
	} else {
		/* Broadcast (except one side...) */
		sendto_server(except, 0, 0, NULL, ":%s SINFO %s", acptr->id, buf);
	}
}

/** Sync all information with server 'client'.
 * Eg: users, channels, everything.
 * @param client	The newly linked in server
 * @param aconf		The link block that belongs to this server
 * @note This function (via cmd_server) is called from both sides, so
 *       from the incoming side and the outgoing side.
 */
int server_sync(Client *client, ConfigItem_link *aconf, int incoming)
{
	Client *acptr;

	if (incoming)
	{
		/* If this is an incomming connection, then we have just received
		 * their stuff and now send our PASS, PROTOCTL and SERVER messages back.
		 */
		if (!IsEAuth(client)) /* if eauth'd then we already sent the passwd */
			sendto_one(client, NULL, "PASS :%s", (aconf->auth->type == AUTHTYPE_PLAINTEXT) ? aconf->auth->data : "*");

		send_proto(client, aconf);
		send_server_message(client);
	}

	RunHook(HOOKTYPE_SERVER_CONNECT, client);

	/* Broadcast new server to the rest of the network */
	sendto_server(client, 0, 0, NULL, ":%s SID %s 2 %s :%s",
		    client->uplink->id, client->name, client->id, client->info);

	/* Broadcast the just-linked-in featureset to other servers on our side */
	broadcast_sinfo(client, NULL, client);

	/* Send moddata of &me (if any, likely minimal) */
	send_moddata_client(client, &me);

	list_for_each_entry_reverse(acptr, &global_server_list, client_node)
	{
		/* acptr->direction == acptr for acptr == client */
		if (acptr->direction == client)
			continue;

		if (IsServer(acptr))
		{
			sendto_one(client, NULL, ":%s SID %s %d %s :%s",
			    acptr->uplink->id,
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
			if (acptr->server->flags.synced)
				sendto_one(client, NULL, ":%s EOS", acptr->id);
			/* Send SINFO of our servers to their side */
			broadcast_sinfo(acptr, client, NULL);
			send_moddata_client(client, acptr); /* send moddata of server 'acptr' (if any, likely minimal) */
		}
	}

	/* Synching nick information */
	list_for_each_entry_reverse(acptr, &client_list, client_node)
	{
		/* acptr->direction == acptr for acptr == client */
		if (acptr->direction == client)
			continue;
		if (IsUser(acptr))
			introduce_user(client, acptr);
	}
	/*
	   ** Last, pass all channels plus statuses
	 */
	{
		Channel *channel;
		for (channel = channels; channel; channel = channel->nextch)
		{
			send_channel_modes_sjoin3(client, channel);
			if (channel->topic_time)
				sendto_one(client, NULL, "TOPIC %s %s %lld :%s",
				    channel->name, channel->topic_nick,
				    (long long)channel->topic_time, channel->topic);
			send_moddata_channel(client, channel);
		}
	}
	
	/* Send ModData for all member(ship) structs */
	send_moddata_members(client);
	
	/* pass on TKLs */
	tkl_sync(client);

	RunHook(HOOKTYPE_SERVER_SYNC, client);

	sendto_one(client, NULL, "NETINFO %i %lld %i %s 0 0 0 :%s",
	    irccounts.global_max, (long long)TStime(), UnrealProtocol,
	    CLOAK_KEY_CHECKSUM,
	    NETWORK_NAME);

	/* Send EOS (End Of Sync) to the just linked server... */
	sendto_one(client, NULL, ":%s EOS", me.id);
	RunHook(HOOKTYPE_POST_SERVER_CONNECT, client);
	return 0;
}

void tls_link_notification_verify(Client *client, ConfigItem_link *aconf)
{
	const char *spki_fp;
	const char *tls_fp;
	char *errstr = NULL;
	int verify_ok;

	if (!MyConnect(client) || !client->local->ssl || !aconf)
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

	tls_fp = moddata_client_get(client, "certfp");
	spki_fp = spki_fingerprint(client);
	if (!tls_fp || !spki_fp)
		return; /* wtf ? */

	/* Only bother the user if we are linking to UnrealIRCd 4.0.16+,
	 * since only for these versions we can give precise instructions.
	 */
	if (!client->server || client->server->features.protocol < 4016)
		return;


	verify_ok = verify_certificate(client->local->ssl, aconf->servername, &errstr);
	if (errstr && strstr(errstr, "not valid for hostname"))
	{
		unreal_log(ULOG_INFO, "link", "HINT_VERIFY_LINK", client,
		          "You may want to consider verifying this server link.\n"
		          "More information about this can be found on https://www.unrealircd.org/Link_verification\n"
		          "Unfortunately the certificate of server '$client' has a name mismatch:\n"
		          "$tls_verify_error\n"
		          "This isn't a fatal error but it will prevent you from using verify-certificate yes;",
		          log_data_link_block(aconf),
		          log_data_string("tls_verify_error", errstr));
	} else
	if (!verify_ok)
	{
		unreal_log(ULOG_INFO, "link", "HINT_VERIFY_LINK", client,
		          "You may want to consider verifying this server link.\n"
		          "More information about this can be found on https://www.unrealircd.org/Link_verification\n"
		          "In short: in the configuration file, change the 'link $client {' block to use this as a password:\n"
		          "password \"$spki_fingerprint\" { spkifp; };\n"
		          "And follow the instructions on the other side of the link as well (which will be similar, but will use a different hash)",
		          log_data_link_block(aconf),
		          log_data_string("spki_fingerprint", spki_fp));
	} else
	{
		unreal_log(ULOG_INFO, "link", "HINT_VERIFY_LINK", client,
		          "You may want to consider verifying this server link.\n"
		          "More information about this can be found on https://www.unrealircd.org/Link_verification\n"
		          "In short: in the configuration file, add the following to your 'link $client {' block:\n"
		          "verify-certificate yes;\n"
		          "Alternatively, you could use SPKI fingerprint verification. Then change the password in the link block to be:\n"
		          "password \"$spki_fingerprint\" { spki_fp; };",
		          log_data_link_block(aconf),
		          log_data_string("spki_fingerprint", spki_fp));
	}
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
	char modebuf[BUFSIZE], parabuf[BUFSIZE];

	if (*channel->name != '#')
		return;

	nomode = 0;
	nopara = 0;
	members = channel->members;

	/* First we'll send channel, channel modes and members and status */

	*modebuf = *parabuf = '\0';
	channel_modes(to, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), channel, 1);

	/* Strip final space if needed */
	if (*parabuf && (parabuf[strlen(parabuf)-1] == ' '))
		parabuf[strlen(parabuf)-1] = '\0';

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
		    (long long)channel->creationtime, channel->name);
	}
	if (nopara && !nomode)
	{
		ircsnprintf(buf, sizeof(buf),
		    ":%s SJOIN %lld %s %s :", me.id,
		    (long long)channel->creationtime, channel->name, modebuf);
	}
	if (!nopara && !nomode)
	{
		ircsnprintf(buf, sizeof(buf),
		    ":%s SJOIN %lld %s %s %s :", me.id,
		    (long long)channel->creationtime, channel->name, modebuf, parabuf);
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
		p = mystpcpy(tbuf, modes_to_sjoin_prefix(lp->member_modes)); /* eg @+ */
		p = mystpcpy(p, lp->client->id); /* nick (well, id) */
		*p++ = ' ';
		*p = '\0';

		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(to, mtags, "%s", buf);
			sent++;
			ircsnprintf(buf, sizeof(buf),
			    ":%s SJOIN %lld %s :", me.id,
			    (long long)channel->creationtime, channel->name);
			prebuflen = strlen(buf);
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
			ircsnprintf(buf, sizeof(buf),
			    ":%s SJOIN %lld %s :", me.id,
			    (long long)channel->creationtime, channel->name);
			prebuflen = strlen(buf);
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
			ircsnprintf(buf, sizeof(buf),
			    ":%s SJOIN %lld %s :", me.id,
			    (long long)channel->creationtime, channel->name);
			prebuflen = strlen(buf);
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
			ircsnprintf(buf, sizeof(buf),
			    ":%s SJOIN %lld %s :", me.id,
			    (long long)channel->creationtime, channel->name);
			prebuflen = strlen(buf);
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
		&& !strcmp(last_autoconnect_server, client->name))
	{
		last_autoconnect_server = NULL;
	}
	return 0;
}

/** Start an outgoing connection to a server, for server linking.
 * @param aconf		Configuration attached to this server
 * @param by		The user initiating the connection (can be NULL)
 * @param hp		The address to connect to.
 */
void _connect_server(ConfigItem_link *aconf, Client *by, struct hostent *hp)
{
	Client *client;

	if (!aconf->outgoing.hostname && !aconf->outgoing.file)
	{
		/* Actually the caller should make sure that this doesn't happen,
		 * so this error may never be triggered:
		 */
		unreal_log(ULOG_ERROR, "link", "LINK_ERROR_NO_OUTGOING", NULL,
		           "Connect to $link_block failed: link block is for incoming only (no link::outgoing::hostname or link::outgoing::file set)",
		           log_data_link_block(aconf));
		return;
	}
		
	if (!hp)
	{
		/* Remove "cache" */
		safe_free(aconf->connect_ip);
	}
	/*
	 * If we dont know the IP# for this host and itis a hostname and
	 * not a ip# string, then try and find the appropriate host record.
	 */
	if (!aconf->connect_ip && !aconf->outgoing.file)
	{
		if (is_valid_ip(aconf->outgoing.hostname))
		{
			/* link::outgoing::hostname is an IP address. No need to resolve host. */
			safe_strdup(aconf->connect_ip, aconf->outgoing.hostname);
		} else
		{
			/* It's a hostname, let the resolver look it up. */
			int ipv4_explicit_bind = 0;

			if (aconf->outgoing.bind_ip && (is_valid_ip(aconf->outgoing.bind_ip) == 4))
				ipv4_explicit_bind = 1;
			
			/* We need this 'aconf->refcount++' or else there's a race condition between
			 * starting resolving the host and the result of the resolver (we could
			 * REHASH in that timeframe) leading to an invalid (freed!) 'aconf'.
			 * -- Syzop, bug #0003689.
			 */
			aconf->refcount++;
			unrealdns_gethostbyname_link(aconf->outgoing.hostname, aconf, ipv4_explicit_bind);
			unreal_log(ULOG_INFO, "link", "LINK_RESOLVING", NULL,
				   "Resolving hostname $link_block.hostname...",
				   log_data_link_block(aconf));
			/* Going to resolve the hostname, in the meantime we return (asynchronous operation) */
			return;
		}
	}
	client = make_client(NULL, &me);
	client->local->hostp = hp;
	/*
	 * Copy these in so we have something for error detection.
	 */
	strlcpy(client->name, aconf->servername, sizeof(client->name));
	strlcpy(client->local->sockhost, aconf->outgoing.hostname ? aconf->outgoing.hostname : aconf->outgoing.file, HOSTLEN + 1);

	if (!connect_server_helper(aconf, client))
	{
		fd_close(client->local->fd);
		--OpenFiles;
		client->local->fd = -2;
		free_client(client);
		/* Fatal error */
		return;
	}
	/* The socket has been connected or connect is in progress. */
	make_server(client);
	client->server->conf = aconf;
	client->server->conf->refcount++;
	if (by && IsUser(by))
		strlcpy(client->server->by, by->name, sizeof(client->server->by));
	else
		strlcpy(client->server->by, "AutoConn.", sizeof client->server->by);
	SetConnecting(client);
	SetOutgoing(client);
	irccounts.unknown++;
	list_add(&client->lclient_node, &unknown_list);
	set_sockhost(client, aconf->outgoing.hostname ? aconf->outgoing.hostname : "127.0.0.1");
	add_client_to_list(client);

	if (aconf->outgoing.options & CONNECT_TLS)
	{
		SetTLSConnectHandshake(client);
		fd_setselect(client->local->fd, FD_SELECT_WRITE, unreal_tls_client_handshake, client);
	}
	else
		fd_setselect(client->local->fd, FD_SELECT_WRITE, completed_connection, client);

	unreal_log(ULOG_INFO, "link", "LINK_CONNECTING", client,
		   aconf->outgoing.file
		   ? "Trying to activate link with server $client ($link_block.file)..."
		   : "Trying to activate link with server $client ($link_block.ip:$link_block.port)...",
		   log_data_link_block(aconf));
}

/** Helper function for connect_server() to prepare the actual bind()'ing and connect().
 * This will also take care of logging/sending error messages.
 * @param aconf		Configuration entry of the server.
 * @param client	The client entry that we will use and fill in.
 * @returns 1 on success, 0 on failure.
 */
static int connect_server_helper(ConfigItem_link *aconf, Client *client)
{
	char *bindip;
	char buf[BUFSIZE];

	if (!aconf->connect_ip && !aconf->outgoing.file)
	{
		unreal_log(ULOG_ERROR, "link", "LINK_ERROR_NOIP", client,
		           "Connect to $client failed: no IP address or file to connect to",
		           log_data_link_block(aconf));
		return 0; /* handled upstream or shouldn't happen */
	}

	if (aconf->outgoing.file)
		SetUnixSocket(client);
	else if (strchr(aconf->connect_ip, ':'))
		client->local->socket_type = SOCKET_TYPE_IPV6;

	if (!set_client_ip(client, aconf->connect_ip ? aconf->connect_ip : "127.0.0.1"))
		return 0; /* Killed (would be odd) */
	
	snprintf(buf, sizeof buf, "Outgoing connection: %s", get_client_name(client, TRUE));
	client->local->fd = fd_socket(IsUnixSocket(client) ? AF_UNIX : (IsIPV6(client) ? AF_INET6 : AF_INET), SOCK_STREAM, 0, buf);
	if (client->local->fd < 0)
	{
		if (ERRNO == P_EMFILE)
		{
			unreal_log(ULOG_ERROR, "link", "LINK_ERROR_MAXCLIENTS", client,
				   "Connect to $client failed: no more sockets available",
				   log_data_link_block(aconf));
			return 0;
		}
		unreal_log(ULOG_ERROR, "link", "LINK_ERROR_SOCKET", client,
			   "Connect to $client failed: could not create socket: $socket_error",
			   log_data_socket_error(-1),
			   log_data_link_block(aconf));
		return 0;
	}
	if (++OpenFiles >= maxclients)
	{
		unreal_log(ULOG_ERROR, "link", "LINK_ERROR_MAXCLIENTS", client,
			   "Connect to $client failed: no more connections available",
			   log_data_link_block(aconf));
		return 0;
	}

	set_sockhost(client, aconf->outgoing.hostname ? aconf->outgoing.hostname : "127.0.0.1");

	if (!aconf->outgoing.bind_ip && iConf.link_bindip)
		bindip = iConf.link_bindip;
	else
		bindip = aconf->outgoing.bind_ip;

	if (bindip && strcmp("*", bindip))
	{
		if (!unreal_bind(client->local->fd, bindip, 0, client->local->socket_type))
		{
			unreal_log(ULOG_ERROR, "link", "LINK_ERROR_SOCKET_BIND", client,
				   "Connect to $client failed: could not bind socket to $link_block.bind_ip: $socket_error -- "
				   "Your link::outgoing::bind-ip is probably incorrect.",
				   log_data_socket_error(client->local->fd),
				   log_data_link_block(aconf));
			return 0;
		}
	}

	set_sock_opts(client->local->fd, client, client->local->socket_type);

	if (!unreal_connect(client->local->fd,
			    aconf->outgoing.file ? aconf->outgoing.file : client->ip,
			    aconf->outgoing.port, client->local->socket_type))
	{
			unreal_log(ULOG_ERROR, "link", "LINK_ERROR_CONNECT", client,
				   aconf->outgoing.file
				   ? "Connect to $client ($link_block.file) failed: $socket_error"
				   : "Connect to $client ($link_block.ip:$link_block.port) failed: $socket_error",
				   log_data_socket_error(client->local->fd),
				   log_data_link_block(aconf));
		return 0;
	}

	return 1;
}

int _is_services_but_not_ulined(Client *client)
{
	if (!client->server || !client->server->features.software || !*client->name)
		return 0; /* cannot detect software version or name not available yet */

	if (our_strcasestr(client->server->features.software, "anope") ||
	    our_strcasestr(client->server->features.software, "atheme"))
	{
		if (!find_uline(client->name))
		{
			unreal_log(ULOG_ERROR, "link", "LINK_NO_ULINES", client,
			           "Server $client is a services server ($software). "
			           "However, server $me does not have $client in the ulines { } block, "
			           "which is required for services servers. "
			           "See https://www.unrealircd.org/docs/Ulines_block",
			           log_data_client("me", &me),
			           log_data_string("software", client->server->features.software));
			return 1; /* Is services AND no ulines { } entry */
		}
	}
	return 0;
}

/** Check if this link should be denied due to deny link { } configuration
 * @param link		The link block
 * @param auto_connect	Set this to 1 if this is called from auto connect code
 *			(it will then check both CRULE_AUTO + CRULE_ALL)
 *			set it to 0 otherwise (will not check CRULE_AUTO blocks).
 * @returns The deny block if the server should be denied, or NULL if no deny block.
 */
const char *_check_deny_link(ConfigItem_link *link, int auto_connect)
{
	ConfigItem_deny_link *d;
	crule_context context;

	for (d = conf_deny_link; d; d = d->next)
	{
		if ((auto_connect == 0) && (d->flag.type == CRULE_AUTO))
			continue;
		if (unreal_mask_match_string(link->servername, d->mask) &&
		    crule_eval(NULL, d->rule))
		{
			return d->reason;
		}
	}
	return NULL;
}

int server_stats_denylink_all(Client *client, const char *para)
{
	ConfigItem_deny_link *links;
	ConfigItem_mask *m;

	if (!para || !(!strcmp(para, "D") || !strcasecmp(para, "denylinkall")))
		return 0;

	for (links = conf_deny_link; links; links = links->next)
	{
		if (links->flag.type == CRULE_ALL)
		{
			for (m = links->mask; m; m = m->next)
				sendnumeric(client, RPL_STATSDLINE, 'D', m->mask, links->prettyrule);
		}
	}

	return 1;
}

int server_stats_denylink_auto(Client *client, const char *para)
{
	ConfigItem_deny_link *links;
	ConfigItem_mask *m;

	if (!para || !(!strcmp(para, "d") || !strcasecmp(para, "denylinkauto")))
		return 0;

	for (links = conf_deny_link; links; links = links->next)
	{
		if (links->flag.type == CRULE_AUTO)
		{
			for (m = links->mask; m; m = m->next)
				sendnumeric(client, RPL_STATSDLINE, 'd', m->mask, links->prettyrule);
		}
	}

	return 1;
}
