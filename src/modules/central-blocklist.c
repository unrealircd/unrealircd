/* Central blocklist
 * (C) Copyright 2023 Bram Matthys and The UnrealIRCd Team
 * License: GPLv2
 */

/*** <<<MODULE MANAGER START>>>
module
{
	documentation "https://www.unrealircd.org/docs/Central_Blocklist";

	// This is displayed in './unrealircd module info ..' and also if compilation of the module fails:
	troubleshooting "Please report at https://bugs.unrealircd.org/ if this module fails to compile";

	// Minimum version necessary for this module to work:
	min-unrealircd-version "6.1.2";

	// Maximum version
	max-unrealircd-version "6.*";

	post-install-text {
		"The module is installed. See https://www.unrealircd.org/docs/Central_Blocklist";
		"for the configuration that you need to add. One important aspect is getting";
		"an API Key, which is a process that (as of October 2023) is not open to everyone.";
	}
}
*** <<<MODULE MANAGER END>>>
*/

#include "unrealircd.h"

#ifndef HOOKTYPE_GET_CENTRAL_API_KEY
 #define HOOKTYPE_GET_CENTRAL_API_KEY 199
#endif

ModuleHeader MOD_HEADER
  = {
	"central-blocklist",
	"1.0.5",
	"Check users at central blocklist",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

ModDataInfo *centralblocklist_md = NULL;
Module *cbl_module = NULL;

#define CBL_URL	 "https://centralblocklist.unrealircd-api.org/api/v1"
#define SPAMREPORT_URL	"https://spamreport.unrealircd-api.org/api/spamreport-v1"
#define CBL_TRANSFER_TIMEOUT 10
#define SPAMREPORT_NUM_REMEMBERED_CMDS 10

typedef struct CBLUser CBLUser;
struct CBLUser
{
	json_t *handshake;
	time_t request_sent;
	char request_pending;
	char allowed_in;
	int last_cmds_slot;
	char *last_cmds[SPAMREPORT_NUM_REMEMBERED_CMDS];
};

/* For tracking current HTTPS requests */
typedef struct CBLTransfer CBLTransfer;
struct CBLTransfer
{
	CBLTransfer *prev, *next;
	time_t started;
	NameList *clients;
};

typedef struct ScoreAction ScoreAction;
struct ScoreAction {
	ScoreAction *prev, *next;
	int priority;
	int score;
	BanAction *ban_action;
	char *ban_reason;
	long ban_time;
};

struct cfgstruct {
	char *url;
	char *spamreport_url;
	char *api_key;
	int max_downloads;
	int spamreport;
	SecurityGroup *except;
	ScoreAction *actions;
};

static struct cfgstruct cfg;

struct reqstruct {
	char custom_score_blocks;
};
static struct reqstruct req;

CBLTransfer *cbltransfers = NULL;

/* Forward declarations */
int cbl_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int cbl_config_posttest(int *errs);
int cbl_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
int cbl_packet(Client *from, Client *to, Client *intended_to, char **msg, int *len);
//int cbl_prelocalconnect(Client *client);
int cbl_is_handshake_finished(Client *client);
void cbl_download_complete(const char *url, const char *file, const char *memory, int memory_len, const char *errorbuf, int cached, void *rs_key);
void cbl_mdata_free(ModData *m);
int cbl_start_request(Client *client);
void cbl_cancel_all_transfers(void);
EVENT(centralblocklist_bundle_requests);
EVENT(centralblocklist_timeout_evt);
void cbl_allow(Client *client);
void send_request_for_pending_clients(void);
const char *get_api_key(void);
void set_tag(Client *client, const char *tag, int value);

#define CBLRAW(x)		(moddata_local_client(x, centralblocklist_md).ptr)
#define CBL(x)			((CBLUser *)(moddata_local_client(x, centralblocklist_md).ptr))

#define alloc_cbl_if_needed(x)	do { \
					if (!moddata_local_client(x, centralblocklist_md).ptr) \
					{ \
						CBLUser *u = safe_alloc(sizeof(CBLUser)); \
						u->handshake = json_object(); \
						moddata_local_client(x, centralblocklist_md).ptr = u; \
					} \
				   } while(0)

#define AddScoreAction(item,list) do { item->priority = 0 - item->score; AddListItemPrio(item, list, item->priority); } while(0)

CMD_OVERRIDE_FUNC(cbl_override);
CMD_OVERRIDE_FUNC(cbl_override_spamreport_gather);
CMD_OVERRIDE_FUNC(cbl_override_spamreport_cmd);

static void set_default_score_action(ScoreAction *action)
{
	action->ban_action = banact_value_to_struct(BAN_ACT_KILL);
	action->ban_time = 900;
	safe_strdup(action->ban_reason, "Rejected by central blocklist");
}

/* Default config */
static void init_config(void)
{
	memset(&cfg, 0, sizeof(cfg));
	safe_strdup(cfg.url, CBL_URL);
	safe_strdup(cfg.spamreport_url, SPAMREPORT_URL);
	cfg.max_downloads = 100;
	// default action
	if (!req.custom_score_blocks)
	{
		ScoreAction *action;
		/* score 5+ */
		action = safe_alloc(sizeof(ScoreAction));
		action->score = 5;
		action->ban_action = banact_value_to_struct(BAN_ACT_KLINE);
		action->ban_time = 900; /* 15m */
		safe_strdup(action->ban_reason, "Rejected by central blocklist");
		AddScoreAction(action, cfg.actions);
		/* score 10+ */
		action = safe_alloc(sizeof(ScoreAction));
		action->score = 10;
		action->ban_action = banact_value_to_struct(BAN_ACT_SHUN);
		action->ban_time = 3600; /* 1h */
		safe_strdup(action->ban_reason, "Rejected by central blocklist");
		AddScoreAction(action, cfg.actions);
	}
	// and the default except block
	cfg.except = safe_alloc(sizeof(SecurityGroup));
	cfg.except->reputation_score = 2016; /* 7 days unregged, or 3.5 days identified */
	cfg.except->identified = 1;
	// exception masks
	unreal_add_mask_string(&cfg.except->mask, "*.irccloud.com");
	// exception IPs
#ifndef DEBUGMODE
	add_name_list(cfg.except->ip, "127.0.0.1");
	add_name_list(cfg.except->ip, "192.168.*");
	add_name_list(cfg.except->ip, "10.*");
#endif
}

static void free_config(void)
{
	ScoreAction *s, *s_next;

	for (s = cfg.actions; s; s = s_next)
	{
		s_next = s->next;
		safe_free(s->ban_reason);
		safe_free_all_ban_actions(s->ban_action);
		safe_free(s);
	}
	cfg.actions = NULL;

	free_security_group(cfg.except);
	safe_free(cfg.url);
	safe_free(cfg.spamreport_url);
	safe_free(cfg.api_key);
	memset(&cfg, 0, sizeof(cfg)); /* needed! */
}


MOD_TEST()
{
	memset(&req, 0, sizeof(req));
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, cbl_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, cbl_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	cbl_module = modinfo->handle;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	init_config();

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "central-blocklist-user";
	mreq.type = MODDATATYPE_LOCAL_CLIENT;
	mreq.free = cbl_mdata_free;
	centralblocklist_md = ModDataAdd(modinfo->handle, mreq);
	if (!centralblocklist_md)
	{
		config_error("[central-blocklist] failed adding moddata");
		return MOD_FAILED;
	}
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, cbl_config_run);
	//HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0, cbl_prelocalconnect);
	HookAdd(modinfo->handle, HOOKTYPE_IS_HANDSHAKE_FINISHED, INT_MAX, cbl_is_handshake_finished);
	return MOD_SUCCESS;
}

void do_command_overrides(ModuleInfo *modinfo)
{
	RealCommand *cmd;
	int i;

	for (i = 0; i < 256; i++)
	{
		for (cmd = CommandHash[i]; cmd; cmd = cmd->next)
		{
			if (cmd->flags & CMD_UNREGISTERED)
				CommandOverrideAdd(modinfo->handle, cmd->cmd, -1, cbl_override);
		}
	}
}


MOD_LOAD()
{
	const char *central_api_key;

	central_api_key = get_api_key();
	if (!central_api_key)
	{
		config_warn("The centralblocklist module is inactive because the central api key is not set. "
		            "Acquire a key via https://www.unrealircd.org/central-api/ and then "
		            "make sure the central-api-key module is loaded and set::central-api::api-key set.");
		return MOD_SUCCESS;
	} else {
		safe_strdup(cfg.api_key, central_api_key);
	}

	do_command_overrides(modinfo);

	/* Enable gathering of "last 10 lines" for SPAMREPORT, only if SPAMREPORT is enabled: */
	if (cfg.spamreport)
	{
		CommandOverrideAdd(modinfo->handle, "NICK", -2, cbl_override_spamreport_gather);
		CommandOverrideAdd(modinfo->handle, "PRIVMSG", -2, cbl_override_spamreport_gather);
		CommandOverrideAdd(modinfo->handle, "NOTICE", -2, cbl_override_spamreport_gather);
		CommandOverrideAdd(modinfo->handle, "PART", -2, cbl_override_spamreport_gather);
		CommandOverrideAdd(modinfo->handle, "INVITE", -2, cbl_override_spamreport_gather);
		CommandOverrideAdd(modinfo->handle, "KNOCK", -2, cbl_override_spamreport_gather);
	}

	CommandOverrideAdd(modinfo->handle, "SPAMREPORT", -2, cbl_override_spamreport_cmd);

	EventAdd(modinfo->handle, "centralblocklist_timeout_evt", centralblocklist_timeout_evt, NULL, 1000, 0);
	EventAdd(modinfo->handle, "centralblocklist_bundle_requests", centralblocklist_bundle_requests, NULL, 1000, 0);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	cbl_cancel_all_transfers();
	free_config();
	return MOD_SUCCESS;
}

/** Test the set::central-blocklist configuration */
int cbl_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::central-blocklist.. */
	if (!ce || !ce->name || strcmp(ce->name, "central-blocklist"))
		return 0;
	
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "api-key"))
		{
			config_error("%s:%i: the api-key is no longer configured at this place. "
			             "Remove set::central-blocklist::api-key, load the "
			             "central-api module and put the key in set::central-api::api-key",
			             cep->file->filename, cep->line_number);
			errors++;
		} else
		if (!strcmp(cep->name, "except"))
		{
			test_match_block(cf, cep, &errors);
		} else
		if (!strcmp(cep->name, "score"))
		{
			int v = atoi(cep->value);
			if ((v < 1) || (v > 99))
			{
				config_error("%s:%i: set::central-blocklist::score: must be between 1 - 99 (got: %d)",
					cep->file->filename, cep->line_number, v);
				errors++;
			}
			if (cep->items)
			{
				req.custom_score_blocks = 1;
				for (cepp = cep->items; cepp; cepp = cepp->next)
				{
					if (!strcmp(cepp->name, "ban-action"))
					{
						errors += test_ban_action_config(cepp);
					} else
					if (!strcmp(cepp->name, "ban-reason"))
					{
					} else
					if (!strcmp(cepp->name, "ban-time"))
					{
					} else
					{
						config_error("%s:%i: unknown directive set::central-blocklist::score::%s",
							cepp->file->filename, cepp->line_number, cepp->name);
						errors++;
						continue;
					}
				}
			}
		} else
		if (!cep->value)
		{
			config_error("%s:%i: set::central-blocklist::%s with no value",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		} else
		if (!strcmp(cep->name, "url"))
		{
		} else
		if (!strcmp(cep->name, "spamreport"))
		{
		} else
		if (!strcmp(cep->name, "spamreport-url"))
		{
		} else
		if (!strcmp(cep->name, "max-downloads"))
		{
			int v = atoi(cep->value);
			if ((v < 1) || (v > 500))
			{
				config_error("%s:%i: set::central-blocklist::score: must be between 1 - 500 (got: %d)",
					cep->file->filename, cep->line_number, v);
				errors++;
			}
		} else
		if (!strcmp(cep->name, "ban-action") || !strcmp(cep->name, "ban-reason") || !strcmp(cep->name, "ban-time"))
		{
			config_error("%s:%i: set::central-blocklist: you cannot use ban-action/ban-reason/ban-time here. "
			             "There are now multiple score blocks. "
			             "See https://www.unrealircd.org/docs/Central_Blocklist#Configuration",
			             cep->file->filename, cep->line_number);
			errors++;
		} else
		{
			config_error("%s:%i: unknown directive set::central-blocklist::%s",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
			continue;
		}
	}
	
	*errs = errors;
	return errors ? -1 : 1;
}

int cbl_config_posttest(int *errs)
{
	int errors = 0;

	*errs = errors;
	return errors ? -1 : 1;
}

/* Configure ourselves based on the set::central-blocklist settings */
int cbl_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::central-blocklist.. */
	if (!ce || !ce->name || strcmp(ce->name, "central-blocklist"))
		return 0;
	
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "api-key"))
		{
			safe_strdup(cfg.api_key, cep->value);
		} else
		if (!strcmp(cep->name, "score"))
		{
			if (!cep->items)
			{
				cfg.actions->score = atoi(cep->value);
			} else
			{
				ScoreAction *action = safe_alloc(sizeof(ScoreAction));
				set_default_score_action(action);
				action->score = atoi(cep->value);
				AddScoreAction(action, cfg.actions);

				for (cepp = cep->items; cepp; cepp = cepp->next)
				{
					if (!strcmp(cepp->name, "ban-action"))
					{
						parse_ban_action_config(cepp, &action->ban_action);
					} else
					if (!strcmp(cepp->name, "ban-reason"))
					{
						safe_strdup(action->ban_reason, cepp->value);
					} else
					if (!strcmp(cepp->name, "ban-time"))
					{
						action->ban_time = config_checkval(cepp->value, CFG_TIME);
					}
				}
			}
		} else
		if (!strcmp(cep->name, "url"))
		{
			safe_strdup(cfg.url, cep->value);
		} else
		if (!strcmp(cep->name, "spamreport"))
		{
			cfg.spamreport = config_checkval(cep->value, CFG_YESNO);
		} else
		if (!strcmp(cep->name, "spamreport-url"))
		{
			safe_strdup(cfg.spamreport_url, cep->value);
		} else
		if (!strcmp(cep->name, "max-downloads"))
		{
			cfg.max_downloads = atoi(cep->value);
		} else
		if (!strcmp(cep->name, "ban-action"))
		{
			parse_ban_action_config(cep, &cfg.actions->ban_action);
		} else
		if (!strcmp(cep->name, "ban-reason"))
		{
			safe_strdup(cfg.actions->ban_reason, cep->value);
		} else
		if (!strcmp(cep->name, "ban-time"))
		{
			cfg.actions->ban_time = config_checkval(cep->value, CFG_TIME);
		} else
		if (!strcmp(cep->name, "except"))
		{
			if (cfg.except)
			{
				free_security_group(cfg.except);
				cfg.except = NULL;
			}
			conf_match_block(cf, cep, &cfg.except);
		}
	}
	return 1;
}

CBLTransfer *add_cbl_transfer(NameList *clients)
{
	CBLTransfer *c = safe_alloc(sizeof(CBLTransfer));
	c->started = TStime();
	c->clients = clients;
	AddListItem(c, cbltransfers);
	return c;
}

void del_cbl_transfer(CBLTransfer *c)
{
	free_entire_name_list(c->clients);
	DelListItem(c, cbltransfers);
	safe_free(c);
}

void cbl_cancel_all_transfers(void)
{
	CBLTransfer *c, *c_next;
	Client *client, *client_next;

	for (c = cbltransfers; c; c = c_next)
	{
		json_t *cbl;
		c_next = c->next;
		url_cancel_handle_by_callback_data(c);
		safe_free(c);
	}
	cbltransfers = NULL;

	list_for_each_entry_safe(client, client_next, &unknown_list, lclient_node)
	{
		CBLUser *cbl = CBL(client);

		if (cbl && cbl->request_sent)
		{
			cbl->request_sent = 0;
			cbl->request_pending = 1;
		}
	}
}

EVENT(centralblocklist_timeout_evt)
{
	Client *client, *client_next;

	list_for_each_entry_safe(client, client_next, &unknown_list, lclient_node)
	{
		CBLUser *cbl = CBL(client);

		if (cbl && cbl->request_sent && (TStime() - cbl->request_sent > CBL_TRANSFER_TIMEOUT))
		{
			unreal_log(ULOG_WARNING, "central-blocklist", "CENTRAL_BLOCKLIST_TIMEOUT", client,
				   "Central blocklist too slow to respond. "
				   "Possible problem with infrastructure at unrealircd.org. "
				   "Allowing user $client.details in unchecked.");
			cbl_allow(client);
		}
	}
	/* NOTE: We did not cancel the HTTPS request, so the result may come in later
	 * when the user is already allowed in.
	 */
}

void show_client_json(Client *client)
{
	char *json_serialized;
	json_serialized = json_dumps(CBL(client)->handshake, JSON_COMPACT);

	unreal_log(ULOG_DEBUG, "central-blocklist", "DEBUG_CENTRAL_BLOCKLIST", client,
		   "OUT: $data",
		   log_data_string("data", json_serialized));
	safe_free(json_serialized);
}

void cbl_add_client_info(Client *client)
{
	char buf[BUFSIZE+1];
	json_t *cbl = CBL(client)->handshake;
	json_t *child = json_object();
	const char *str;

	json_object_set_new(cbl, "client", child);

	//// THE FOLLOWING IS TAKEN FROM src/json.c AND MODIFIED /////
	
	/* First the information that is available for ALL client types: */
	json_object_set_new(child, "name", json_string_unreal(client->name));
	json_object_set_new(child, "id", json_string_unreal(client->id));

	/* hostname is available for all, it just depends a bit on whether it is DNS or IP */
	if (client->user && *client->user->realhost)
		json_object_set_new(child, "hostname", json_string_unreal(client->user->realhost));
	else if (client->local && *client->local->sockhost)
		json_object_set_new(child, "hostname", json_string_unreal(client->local->sockhost));
	else
		json_object_set_new(child, "hostname", json_string_unreal(GetIP(client)));

	/* same for ip, is there for all (well, some services pseudo-users may not have one) */
	json_object_set_new(child, "ip", json_string_unreal(client->ip));

	/* client.details is always available: it is nick!user@host, nick@host, server@host
	 * server@ip, or just server.
	 */
	if (client->user)
	{
		snprintf(buf, sizeof(buf), "%s!%s@%s", client->name, client->user->username, client->user->realhost);
		json_object_set_new(child, "details", json_string_unreal(buf));
	} else if (client->ip) {
		if (*client->name)
			snprintf(buf, sizeof(buf), "%s@%s", client->name, client->ip);
		else
			snprintf(buf, sizeof(buf), "[%s]", client->ip);
		json_object_set_new(child, "details", json_string_unreal(buf));
	} else {
		json_object_set_new(child, "details", json_string_unreal(client->name));
	}

	if (client->local && client->local->listener)
		json_object_set_new(child, "server_port", json_integer(client->local->listener->port));
	if (client->local && client->local->port)
		json_object_set_new(child, "client_port", json_integer(client->local->port));

	if (client->user)
	{
		char buf[512];
		const char *str;
		/* client.user */
		json_t *user = json_object();
		json_object_set_new(child, "user", user);

		json_object_set_new(user, "username", json_string_unreal(client->user->username));
		if (!BadPtr(client->info))
			json_object_set_new(user, "realname", json_string_unreal(client->info));
		json_object_set_new(user, "reputation", json_integer(GetReputation(client)));
	}

	if ((str = moddata_client_get(client, "tls_cipher")))
	{
		json_t *tls = json_object();
		json_object_set_new(child, "tls", tls);
		json_object_set_new(tls, "cipher", json_string_unreal(str));
		if (client->local->sni_servername)
			json_object_set_new(tls, "sni_servername", json_string_unreal(client->local->sni_servername));
	}

#ifdef HAVE_TCP_INFO
	if (client->local->fd >= 0)
	{
		socklen_t optlen = sizeof(struct tcp_info);
		struct tcp_info tcp_info;
		optlen = sizeof(tcp_info);
		memset(&tcp_info, 0, sizeof(tcp_info));
		if (getsockopt(client->local->fd, IPPROTO_TCP, TCP_INFO, (void *)&tcp_info, &optlen) == 0)
		{
			json_t *j = json_object();
			json_object_set_new(child, "tcp_info", j);
			json_object_set_new(j, "rtt", json_integer(MAX(tcp_info.tcpi_rtt,1)/1000));
			json_object_set_new(j, "rtt_var", json_integer(MAX(tcp_info.tcpi_rttvar,1)/1000));
#if defined(__FreeBSD__)
			json_object_set_new(j, "pmtu", json_integer(tcp_info.__tcpi_pmtu));
#else
			json_object_set_new(j, "pmtu", json_integer(tcp_info.tcpi_pmtu));
#endif
			json_object_set_new(j, "snd_cwnd", json_integer(tcp_info.tcpi_snd_cwnd));
			json_object_set_new(j, "snd_mss", json_integer(tcp_info.tcpi_snd_mss));
			json_object_set_new(j, "rcv_mss", json_integer(tcp_info.tcpi_rcv_mss));
		}
	}
#endif
}

CMD_OVERRIDE_FUNC(cbl_override)
{
	json_t *cbl;
	json_t *handshake;
	json_t *cmds;
	json_t *item;
	char timebuf[64];
	char number[32];
	char isnick = 0;
	uint32_t nospoof = 0;

	if (!MyConnect(client) ||
	    !IsUnknown(client) ||
	    !strcmp(ovr->command->cmd, "PASS") ||
	    !strcmp(ovr->command->cmd, "WEBIRC") ||
	    !strcmp(ovr->command->cmd, "AUTHENTICATE"))
	{
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}

	alloc_cbl_if_needed(client);
	cbl = CBL(client)->handshake;

	/* Create "handshake" if it does not exist yet */
	handshake = json_object_get(cbl, "handshake");
	if (!handshake)
	{
		handshake = json_object();
		json_object_set_new(cbl, "handshake", handshake);
	}
	/* Create handshake->commands if it does not exist yet */
	cmds = json_object_get(handshake, "commands");
	if (!cmds)
	{
		cmds = json_object();
		json_object_set_new(handshake, "commands", cmds);
	}

	strlcpy(timebuf, timestamp_iso8601_now(), sizeof(timebuf));
	snprintf(number, sizeof(number), "%lld", client->local->traffic.messages_received);

	item = json_object();
	json_object_set_new(item, "time", json_string_unreal(timebuf));
	json_object_set_new(item, "command", json_string_unreal(ovr->command->cmd));
	json_object_set_new(item, "raw", json_string_unreal(backupbuf));
	json_object_set_new(cmds, number, item);

	if (!strcmp(ovr->command->cmd, "NICK"))
	{
		isnick = 1;
		nospoof = client->local->nospoof;
	} else
	if (!strcmp(ovr->command->cmd, "PONG") && (parc > 1) && !BadPtr(parv[1]))
	{
		unsigned long result = strtoul(parv[1], NULL, 16);
		if (client->local->nospoof && (client->local->nospoof == result))
		{
			json_object_del(handshake, "pong_received");
			json_object_set_new(handshake, "pong_received", json_string_unreal(timebuf));
		}
	}
#if UNREAL_VERSION < 0x06010300
	/* Meh... bug in UnrealIRCd <6.1.3 */
	else if (!strcmp(ovr->command->cmd, "CAP") && (parc > 1) && !strcasecmp(parv[1], "END") && !IsUser(client))
	{
		ClearCapability(client, "cap");
		if (is_handshake_finished(client))
			register_user(client);
		return; // we handled it
	}
#endif
	CALL_NEXT_COMMAND_OVERRIDE();
	if (isnick && !IsDead(client) && (nospoof != client->local->nospoof))
	{
		json_object_del(handshake, "ping_sent");
		json_object_set_new(handshake, "ping_sent", json_string_unreal(timebuf));
	}
}

int cbl_start_request(Client *client)
{
	CBLUser *cbl = CBL(client);

	if (cbl->request_sent || cbl->request_pending)
		return 0; /* Handshake is NOT finished yet, HTTP request already in progress */

	if (!json_object_get(cbl->handshake, "client"))
		cbl_add_client_info(client);

	cbl->request_pending = 1;

#ifdef DEBUGMODE
	show_client_json(client);
#endif

	return 0; /* Handshake is NOT finished yet, request will be sent to server */
}

int cbl_is_handshake_finished(Client *client)
{
	if (!CBL(client) || CBL(client)->allowed_in)
		return 1; // something went wrong or we are finished with this, let the user through

	/* Missing something, pretend we are finished and don't handle */
	if (!(client->user && *client->user->username && client->name[0] && IsNotSpoof(client)))
		return 1;

	/* User is exempt */
	if (user_allowed_by_security_group(client, cfg.except))
		return 1;

	return cbl_start_request(client);
}

void cbl_allow(Client *client)
{
	if (CBL(client))
		CBL(client)->allowed_in = 2;

	if (is_handshake_finished(client))
		register_user(client);
}

void set_tag(Client *client, const char *name, int value)
{
	Tag *tag = find_tag(client, name);
	if (tag)
		tag->value = value;
	else
		add_tag(client, name, value);
}

void cbl_handle_response(Client *client, json_t *response)
{
	int spam_score = 0; // spam score, can be negative too
	json_error_t jerr;
	Tag *tag;
	ScoreAction *action;
	json_t *j, *obj;

	spam_score = json_object_get_integer(response, "score", 0);
	set_tag(client, "CBL_SCORE", spam_score);

	obj = json_object_get(response, "set-variables");
	if (obj)
	{
		const char *key;
		json_t *value;
		json_object_foreach(obj, key, value)
		{
			if (!key || !value || !json_is_integer(value))
				continue;
			if (!strcmp(key, "REPUTATION"))
				continue; // reserved variable name (FIXME: not hardcoded)
			set_tag(client, key, json_integer_value(value));
		}
	}

	for (action = cfg.actions; action; action = action->next)
	{
		if (spam_score >= action->score)
		{
			if (highest_ban_action(action->ban_action) <= BAN_ACT_WARN)
			{
				unreal_log(ULOG_INFO, "central-blocklist", "CBL_HIT", client,
					   "CBL: Client $client.details flagged by central-blocklist, but allowed in (score $spam_score)",
					   log_data_integer("spam_score", spam_score));
			} else {
				unreal_log(ULOG_INFO, "central-blocklist", "CBL_HIT_REJECTED_USER", client,
					   "CBL: Client $client.details is rejected by central-blocklist (score $spam_score)",
					   log_data_integer("spam_score", spam_score));
			}
			if (take_action(client, action->ban_action, action->ban_reason, action->ban_time, 0, NULL) <= BAN_ACT_WARN)
				cbl_allow(client);
			return;
		}
	}
	unreal_log(ULOG_DEBUG, "central-blocklist", "DEBUG_CENTRAL_BLOCKLIST", client,
		   "CBL: Client $client.details is allowed (score $spam_score)",
		   log_data_integer("spam_score", spam_score));
	cbl_allow(client);
}

void cbl_error_response(CBLTransfer *transfer, const char *error)
{
	NameList *n;
	Client *client;
	int num = 0;

	for (n = transfer->clients; n; n = n->next)
	{
		client = hash_find_id(n->name, NULL);
		if (!client)
			continue; /* Client disconnected already */
		unreal_log(ULOG_DEBUG, "central-blocklist", "DEBUG_CENTRAL_BLOCKLIST_ERROR", client,
			   "CBL: Client $client.details allowed in due to CBL error: $error",
			   log_data_string("error", error));
		cbl_allow(client);
		num++;
	}
	if (num > 0)
	{
		unreal_log(ULOG_INFO, "central-blocklist", "CENTRAL_BLOCKLIST_ERROR", client,
			   "CBL: Allowed $num_clients client(s) in due to CBL error: $error",
			   log_data_integer("num_clients", num),
			   log_data_string("error", error));
	}
	del_cbl_transfer(transfer);
}

void cbl_download_complete(const char *url, const char *file, const char *memory, int memory_len, const char *errorbuf, int cached, void *rs_key)
{
	CBLTransfer *transfer;
	json_t *result; // complete JSON result
	json_t *responses; // result->responses
	json_error_t jerr;
	const char *str;
	const char *key;
	json_t *value;

	transfer = (CBLTransfer *)rs_key;

	// !!!!! IMPORTANT !!!!!
	//
	// Do NOT 'return' without calling cbl_error_response(transfer)
	//
	// !!!!! IMPORTANT !!!!!

	if (errorbuf || !memory)
	{
		unreal_log(ULOG_DEBUG, "central-blocklist", "DEBUG_CENTRAL_BLOCKLIST", NULL,
		           "CBL ERROR: $error",
		           log_data_string("error", errorbuf ? errorbuf : "No data returned"));
		cbl_error_response(transfer, "error contacting CBL");
		return;
	}

#ifdef DEBUGMODE
	unreal_log(ULOG_DEBUG, "central-blocklist", "DEBUG_CENTRAL_BLOCKLIST", NULL,
	           "CBL Got result: $buf",
	           log_data_string("buf", memory));
#endif

	// NOTE: if we didn't have that debug from above, we could avoid the strlncpy and use json_loadb here
	result = json_loads(memory, JSON_REJECT_DUPLICATES, &jerr);
	if (!result)
	{
		unreal_log(ULOG_DEBUG, "central-blocklist", "DEBUG_CENTRAL_BLOCKLIST", NULL,
		           "CBL ERROR: JSON parse error");
		cbl_error_response(transfer, "invalid CBL response (JSON parse error)");
		return;
	}

	/* Errors are fatal, we display, allow clients in and stop */
	if ((str = json_object_get_string(result, "error")))
	{
		cbl_error_response(transfer, str);
		return;
	}

	/* Warnings are non-fatal, we display it and continue.
	 * These could be used, for example for deprecation warnings
	 * (eg: ancient module version) before we make it a hard
	 * error weeks/months later.
	 */
	if ((str = json_object_get_string(result, "warning")))
	{
		unreal_log(ULOG_WARNING, "central-blocklist", "CENTRAL_BLOCKLIST_WARNING", NULL,
		           "CBL Server gave a warning: $warning",
		           log_data_string("warning", str));
	}

	responses = json_object_get(result, "responses");
	if (!responses)
	{
		json_decref(result);
		cbl_error_response(transfer, "no spam scores calculated for users");
		return; /* Nothing to do */
	}

	/* Now iterate through each */
	json_object_foreach(responses, key, value)
	{
		Client *client;
		if (!key)
			continue; /* Is this even possible? */
		client = hash_find_id(key, NULL);
		if (!client)
			continue; /* Client disconnected already */
		cbl_handle_response(client, value);
	}

	json_decref(result);
	del_cbl_transfer(transfer);
}

void cbl_mdata_free(ModData *m)
{
	CBLUser *cbl = (CBLUser *)m->ptr;

	if (cbl)
	{
		json_decref(cbl->handshake);
		if (cbl->allowed_in == 2)
		{
			int i;
			for (i = 0; i < SPAMREPORT_NUM_REMEMBERED_CMDS; i++)
				safe_free(cbl->last_cmds[i]);
		}
		safe_free(cbl);
		m->ptr = NULL;
	}
}

void send_request_for_pending_clients(void)
{
	Client *client, *next;
	json_t *j, *requests;
	NameValuePrioList *headers = NULL;
	int num;
	char *json_serialized;
	CBLTransfer *c;
	NameList *clientlist = NULL;

	num = downloads_in_progress();
	if (num > cfg.max_downloads)
	{
		unreal_log(ULOG_WARNING, "central-blocklist", "CENTRAL_BLOCKLIST_TOO_MANY_CONCURRENT_REQUESTS", NULL,
			   "Already $num_requests HTTP(S) requests in progress.",
			   log_data_integer("num_requests", num));
		return;
	}

	j = json_object();
	json_object_set_new(j, "server", json_string_unreal(me.name));
	json_object_set_new(j, "module_version", json_string_unreal(cbl_module->header->version));
	requests = json_object();
	json_object_set_new(j, "requests", requests);

	list_for_each_entry_safe(client, next, &unknown_list, lclient_node)
	{
		CBLUser *cbl = CBL(client);
		if (cbl && cbl->request_pending)
		{
			// requests[clientid] => ["client"=>["nick"=>"xyz"...etc...
			json_object_set_new(requests, client->id, json_deep_copy(cbl->handshake));

			cbl->request_pending = 0;
			cbl->request_sent = TStime();
			add_name_list(clientlist, client->id);
		}
	}

	json_serialized = json_dumps(j, JSON_COMPACT);
	if (!json_serialized)
	{
		unreal_log(ULOG_WARNING, "central-blocklist", "CENTRAL_BLOCKLIST_BUG_SERIALIZE", client,
			   "Unable to serialize JSON request. Weird.");
		json_decref(j);
		free_entire_name_list(clientlist);
		return;
	}
	json_decref(j);

	add_nvplist(&headers, 0, "Content-Type", "application/json; charset=utf-8");
	add_nvplist(&headers, 0, "X-API-Key", cfg.api_key);
	c = add_cbl_transfer(clientlist);
	url_start_async(cfg.url, HTTP_METHOD_POST, json_serialized, headers, 0, 0, cbl_download_complete, c, cfg.url, 1);
	safe_free(json_serialized);
	safe_free_nvplist(headers);
}

int cbl_any_pending_clients(void)
{
	Client *client, *next;

	list_for_each_entry_safe(client, next, &unknown_list, lclient_node)
	{
		CBLUser *cbl = CBL(client);
		if (cbl && cbl->request_pending)
			return 1;
	}
	return 0;
}

EVENT(centralblocklist_bundle_requests)
{
	if (cbl_any_pending_clients())
		send_request_for_pending_clients();
}

const char *get_api_key(void)
{
	Hook *h;
	for (h = Hooks[HOOKTYPE_GET_CENTRAL_API_KEY]; h; h = h->next)
		return h->func.conststringfunc();
	return NULL;
}

/** Remember last # commands for SPAMREPORT */
CMD_OVERRIDE_FUNC(cbl_override_spamreport_gather)
{
	if (MyUser(client) && CBL(client) && (CBL(client)->allowed_in == 2))
	{
		char record_cmd = 1;

		if ((!strcmp(ovr->command->cmd, "PRIVMSG") || !strcmp(ovr->command->cmd, "NOTICE")) &&
		    (parc > 2) && !strchr(parv[1], '#'))
		{
			/* This is a private PRIVMSG/NOTICE */
			record_cmd = 0;
		}
		if (record_cmd)
		{
			safe_strdup(CBL(client)->last_cmds[CBL(client)->last_cmds_slot], backupbuf);
			CBL(client)->last_cmds_slot++;
			if (CBL(client)->last_cmds_slot >= SPAMREPORT_NUM_REMEMBERED_CMDS)
				CBL(client)->last_cmds_slot = 0;
		}
	}

	CALL_NEXT_COMMAND_OVERRIDE();
}

void cbl_spamreport(Client *from, Client *client)
{
	json_t *j, *requests, *data, *cmds, *item;
	NameValuePrioList *headers = NULL;
	int num;
	char *json_serialized;
	int i, start;
	char number[16];
	int cnt = 0;

	if (!MyUser(client) || !CBL(client))
		return; /* Only possible if hot-loading */

	num = downloads_in_progress();
	if (num > cfg.max_downloads)
	{
		unreal_log(ULOG_WARNING, "central-blocklist", "CENTRAL_BLOCKLIST_TOO_MANY_CONCURRENT_REQUESTS", NULL,
			   "Already $num_requests HTTP(S) requests in progress.",
			   log_data_integer("num_requests", num));
		return;
	}

	j = json_object();
	json_object_set_new(j, "server", json_string_unreal(me.name));
	json_object_set_new(j, "module_version", json_string_unreal(cbl_module->header->version));
	requests = json_object();
	json_object_set_new(j, "reports", requests);

	data = json_deep_copy(CBL(client)->handshake); /* .. deep copy. */
	json_object_set_new(requests, client->id, data); /* ..and steal reference */
	cmds = json_object();
	json_object_set_new(data, "commands", cmds);
	start = CBL(client)->last_cmds_slot;
	for (i = start; i < SPAMREPORT_NUM_REMEMBERED_CMDS; i++)
	{
		if (CBL(client)->last_cmds[i])
		{
			snprintf(number, sizeof(number), "%d", ++cnt);
			item = json_object();
			json_object_set_new(item, "raw", json_string_unreal(CBL(client)->last_cmds[i]));
			json_object_set_new(cmds, number, item);
		}
	}
	for (i = 0; i < start; i++)
	{
		if (CBL(client)->last_cmds[i])
		{
			snprintf(number, sizeof(number), "%d", ++cnt);
			item = json_object();
			json_object_set_new(item, "raw", json_string_unreal(CBL(client)->last_cmds[i]));
			json_object_set_new(cmds, number, item);
		}
	}

	json_serialized = json_dumps(j, JSON_COMPACT);
	if (!json_serialized)
	{
		unreal_log(ULOG_WARNING, "central-blocklist", "CENTRAL_BLOCKLIST_BUG_SERIALIZE", client,
			   "Unable to serialize JSON request. Weird.");
		json_decref(j);
		return;
	}

	json_decref(j);
	add_nvplist(&headers, 0, "Content-Type", "application/json; charset=utf-8");
	add_nvplist(&headers, 0, "X-API-Key", cfg.api_key);
	url_start_async(cfg.spamreport_url, HTTP_METHOD_POST, json_serialized, headers, 0, 0, download_complete_dontcare, NULL, cfg.spamreport_url, 1);
	safe_free(json_serialized);
	safe_free_nvplist(headers);
}

CMD_OVERRIDE_FUNC(cbl_override_spamreport_cmd)
{
	if (ValidatePermissionsForPath("server-ban:spamreport",client,NULL,NULL,NULL) &&
	    (parc > 1) &&
	    !((parc > 2) && strcasecmp(parv[2], "unrealircd"))
	    )
	{
		Client *target = find_user(parv[1], NULL);
		if (target)
		{
			if (!MyUser(target))
			{
				/* Forward it to other server */
				if (parc > 2)
				{
					sendto_one(target, NULL, ":%s SPAMREPORT %s %s",
					           client->id, parv[1], parv[2]);
				} else {
					sendto_one(target, NULL, ":%s SPAMREPORT %s",
					           client->id, parv[1]);
				}
				return;
			} else {
				/* My client. */
				int n;

				if (!cfg.spamreport)
				{
					if ((parc > 2) && !strcasecmp(parv[2], "unrealircd"))
					{
						sendnotice(client, "Spamreporting to UnrealIRCd is not enabled on this server. "
						                   "To enable, add: set { central-blocklist { spamreport yes; } }");
						return;
					}
					CALL_NEXT_COMMAND_OVERRIDE();
				}

				/* Report to UnrealIRCd first... */
				sendnotice(client, "Sending spam report to UnrealIRCd...");
				cbl_spamreport(client, target);
				if ((parc > 2) && !strcasecmp(parv[2], "unrealircd"))
				{
					sendnotice(client, "Sending spam report to %d target(s)", 1);
					return; /* We are done */
				}

				/* There may be more spamreport blocks: */
				n = spamreport(target, target->ip, NULL, NULL);
				if (n < 0)
					n = 0;
				sendnotice(client, "Sending spam report to %d target(s)", n + 1); /* +1 for unrealircd :D */
				return;
			}
		}
	}
	CALL_NEXT_COMMAND_OVERRIDE();
}
