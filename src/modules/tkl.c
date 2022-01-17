/*
 * Unreal Internet Relay Chat Daemon, src/modules/tkl.c
 * TKL Commands: server bans, spamfilters, etc.
 * (C) 1999-2019 Bram Matthys and The UnrealIRCd Team
 *
 * See file AUTHORS in IRC package for additional names of
 * the programmers.
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
	"tkl",
	"5.0",
	"Server ban commands such as /GLINE, /SPAMFILTER, etc.",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
int tkl_config_test_spamfilter(ConfigFile *, ConfigEntry *, int, int *);
int tkl_config_run_spamfilter(ConfigFile *, ConfigEntry *, int);
int tkl_config_test_ban(ConfigFile *, ConfigEntry *, int, int *);
int tkl_config_run_ban(ConfigFile *, ConfigEntry *, int);
int tkl_config_test_except(ConfigFile *, ConfigEntry *, int, int *);
int tkl_config_run_except(ConfigFile *, ConfigEntry *, int);
int tkl_config_test_set(ConfigFile *, ConfigEntry *, int, int *);
int tkl_config_run_set(ConfigFile *, ConfigEntry *, int);
int tkl_ip_change(Client *client, const char *oldip);
CMD_FUNC(cmd_gline);
CMD_FUNC(cmd_shun);
CMD_FUNC(cmd_tempshun);
CMD_FUNC(cmd_gzline);
CMD_FUNC(cmd_kline);
CMD_FUNC(cmd_zline);
CMD_FUNC(cmd_spamfilter);
CMD_FUNC(cmd_eline);
void cmd_tkl_line(Client *client, int parc, const char *parv[], char *type);
int _tkl_hash(unsigned int c);
char _tkl_typetochar(int type);
int _tkl_chartotype(char c);
int tkl_banexception_chartotype(char c);
char *_tkl_type_string(TKL *tk);
char *_tkl_type_config_string(TKL *tk);
char *tkl_banexception_configname_to_chars(char *name);
TKL *_tkl_add_serverban(int type, char *usermask, char *hostmask, char *reason, char *set_by,
                            time_t expire_at, time_t set_at, int soft, int flags);
TKL *_tkl_add_banexception(int type, char *usermask, char *hostmask, char *reason, char *set_by,
                               time_t expire_at, time_t set_at, int soft, char *bantypes, int flags);
TKL *_tkl_add_nameban(int type, char *name, int hold, char *reason, char *set_by,
                          time_t expire_at, time_t set_at, int flags);
TKL *_tkl_add_spamfilter(int type, unsigned short target, BanAction action, Match *match, char *set_by,
                             time_t expire_at, time_t set_at,
                             time_t spamf_tkl_duration, char *spamf_tkl_reason,
                             int flags);
void _sendnotice_tkl_del(char *removed_by, TKL *tkl);
void _sendnotice_tkl_add(TKL *tkl);
void _free_tkl(TKL *tkl);
void _tkl_del_line(TKL *tkl);
static void _tkl_check_local_remove_shun(TKL *tmp);
char *_tkl_uhost(TKL *tkl, char *buf, size_t buflen, int options);
void tkl_expire_entry(TKL * tmp);
EVENT(tkl_check_expire);
int _find_tkline_match(Client *client, int skip_soft);
int _find_shun(Client *client);
int _find_spamfilter_user(Client *client, int flags);
TKL *_find_qline(Client *client, char *nick, int *ishold);
TKL *_find_tkline_match_zap(Client *client);
void _tkl_stats(Client *client, int type, const char *para, int *cnt);
void _tkl_sync(Client *client);
CMD_FUNC(_cmd_tkl);
int _place_host_ban(Client *client, BanAction action, char *reason, long duration);
int _match_spamfilter(Client *client, const char *str_in, int type, const char *cmd, const char *target, int flags, TKL **rettk);
int _match_spamfilter_mtags(Client *client, MessageTag *mtags, char *cmd);
int check_mtag_spamfilters_present(void);
int _join_viruschan(Client *client, TKL *tk, int type);
void _spamfilter_build_user_string(char *buf, char *nick, Client *client);
int _match_user(const char *rmask, Client *client, int options);
int _match_user_extended_server_ban(const char *banstr, Client *client);
void ban_target_to_tkl_layer(BanTarget ban_target, BanAction action, Client *client, const char **tkl_username, const char **tkl_hostname);
int _tkl_ip_hash(char *ip);
int _tkl_ip_hash_type(int type);
TKL *_find_tkl_serverban(int type, char *usermask, char *hostmask, int softban);
TKL *_find_tkl_banexception(int type, char *usermask, char *hostmask, int softban);
TKL *_find_tkl_nameban(int type, char *name, int hold);
TKL *_find_tkl_spamfilter(int type, char *match_string, BanAction action, unsigned short target);
int _find_tkl_exception(int ban_type, Client *client);
static void add_default_exempts(void);
int parse_extended_server_ban(const char *mask_in, Client *client, char **error, int skip_checking, char *buf1, size_t buf1len, char *buf2, size_t buf2len);

/* Externals (only for us :D) */
extern int MODVAR spamf_ugly_vchanoverride;

typedef struct TKLTypeTable TKLTypeTable;
struct TKLTypeTable
{
	char *config_name;        /**< The name as used in the configuration file */
	char letter;              /**< The letter ised in the TKL S2S command */
	int type;                 /**< TKL_xxx, optionally OR'ed with TKL_GLOBAL */
	char *log_name;           /**< Used for logging and server notices */
	unsigned tkltype:1;       /**< Is a type available in cmd_tkl() and friends */
	unsigned exceptiontype:1; /**< Is a type available for exceptions */
	unsigned needip:1;        /**< When using this exempt option, only IP addresses are permitted (processed before DNS/ident lookups etc) */
};

/** This table which defines all TKL types and TKL exception types.
 * If you wonder about the messy order: gline/kline/gzline/zline
 * are at the top for performance reasons. They make up 99% of the TKLs.
 *
 * IMPORTANT IF YOU ARE ADDING A NEW TYPE TO THIS TABLE:
 * - also update eline_syntax()
 * - update help.conf (HELPOP ELINE)
 * - more?
 */
TKLTypeTable tkl_types[] = {
	/* <config name> <letter> <TKL_xxx type>               <logging name> <tkl option?> <exempt option?> <need ip address?> */
	{ "gline",                'G', TKL_KILL       | TKL_GLOBAL, "G-Line",               1, 1, 0 },
	{ "kline",                'k', TKL_KILL,                    "K-Line",               1, 1, 0 },
	{ "gzline",               'Z', TKL_ZAP        | TKL_GLOBAL, "Global Z-Line",        1, 1, 1 },
	{ "zline",                'z', TKL_ZAP,                     "Z-Line",               1, 1, 1 },
	{ "spamfilter",           'F', TKL_SPAMF      | TKL_GLOBAL, "Spamfilter",           1, 1, 0 },
	{ "qline",                'Q', TKL_NAME       | TKL_GLOBAL, "Q-Line",               1, 1, 0 },
	{ "except",               'E', TKL_EXCEPTION  | TKL_GLOBAL, "Exception",            1, 0, 0 },
	{ "shun",                 's', TKL_SHUN       | TKL_GLOBAL, "Shun",                 1, 1, 0 },
	{ "local-qline",          'q', TKL_NAME,                    "Local Q-Line",         1, 0, 0 },
	{ "local-exception",      'e', TKL_EXCEPTION,               "Local Exception",      1, 0, 0 },
	{ "local-spamfilter",     'f', TKL_SPAMF,                   "Local Spamfilter",     1, 0, 0 },
	{ "blacklist",            'b', TKL_BLACKLIST,               "Blacklist",            0, 1, 1 },
	{ "connect-flood",        'c', TKL_CONNECT_FLOOD,           "Connect flood",        0, 1, 1 },
	{ "maxperip",             'm', TKL_MAXPERIP,                "Max-per-IP",           0, 1, 0 },
	{ "handshake-data-flood", 'd', TKL_HANDSHAKE_DATA_FLOOD,    "Handshake data flood", 0, 1, 1 },
	{ "antirandom",           'r', TKL_ANTIRANDOM,              "Antirandom",           0, 1, 0 },
	{ "antimixedutf8",        '8', TKL_ANTIMIXEDUTF8,           "Antimixedutf8",        0, 1, 0 },
	{ "ban-version",          'v', TKL_BAN_VERSION,             "Ban Version",          0, 1, 0 },
	{ NULL,                   '\0', 0,                          NULL,                   0, 0, 0 },
};
#define ALL_VALID_EXCEPTION_TYPES "kline, gline, zline, gzline, spamfilter, shun, qline, blacklist, connect-flood, handshake-data-flood, antirandom, antimixedutf8, ban-version"

int max_stats_matches = 1000;
int mtag_spamfilters_present = 0; /**< Are any spamfilters with type SPAMF_MTAG present? */

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkl_config_test_spamfilter);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkl_config_test_ban);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkl_config_test_except);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkl_config_test_set);
	EfunctionAdd(modinfo->handle, EFUNC_TKL_HASH, _tkl_hash);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
	EfunctionAdd(modinfo->handle, EFUNC_TKL_TYPETOCHAR, TO_INTFUNC(_tkl_typetochar));
	EfunctionAdd(modinfo->handle, EFUNC_TKL_CHARTOTYPE, TO_INTFUNC(_tkl_chartotype));
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
	EfunctionAddString(modinfo->handle, EFUNC_TKL_TYPE_STRING, _tkl_type_string);
	EfunctionAddString(modinfo->handle, EFUNC_TKL_TYPE_CONFIG_STRING, _tkl_type_config_string);
	EfunctionAddPVoid(modinfo->handle, EFUNC_TKL_ADD_SERVERBAN, TO_PVOIDFUNC(_tkl_add_serverban));
	EfunctionAddPVoid(modinfo->handle, EFUNC_TKL_ADD_BANEXCEPTION, TO_PVOIDFUNC(_tkl_add_banexception));
	EfunctionAddPVoid(modinfo->handle, EFUNC_TKL_ADD_NAMEBAN, TO_PVOIDFUNC(_tkl_add_nameban));
	EfunctionAddPVoid(modinfo->handle, EFUNC_TKL_ADD_SPAMFILTER, TO_PVOIDFUNC(_tkl_add_spamfilter));
	EfunctionAddVoid(modinfo->handle, EFUNC_TKL_DEL_LINE, _tkl_del_line);
	EfunctionAddVoid(modinfo->handle, EFUNC_FREE_TKL, _free_tkl);
	EfunctionAddVoid(modinfo->handle, EFUNC_TKL_CHECK_LOCAL_REMOVE_SHUN, _tkl_check_local_remove_shun);
	EfunctionAdd(modinfo->handle, EFUNC_FIND_TKLINE_MATCH, _find_tkline_match);
	EfunctionAdd(modinfo->handle, EFUNC_FIND_SHUN, _find_shun);
	EfunctionAdd(modinfo->handle, EFUNC_FIND_SPAMFILTER_USER, _find_spamfilter_user);
	EfunctionAddPVoid(modinfo->handle, EFUNC_FIND_QLINE, TO_PVOIDFUNC(_find_qline));
	EfunctionAddPVoid(modinfo->handle, EFUNC_FIND_TKLINE_MATCH_ZAP, TO_PVOIDFUNC(_find_tkline_match_zap));
	EfunctionAddPVoid(modinfo->handle, EFUNC_FIND_TKL_SERVERBAN, TO_PVOIDFUNC(_find_tkl_serverban));
	EfunctionAddPVoid(modinfo->handle, EFUNC_FIND_TKL_BANEXCEPTION, TO_PVOIDFUNC(_find_tkl_banexception));
	EfunctionAddPVoid(modinfo->handle, EFUNC_FIND_TKL_NAMEBAN, TO_PVOIDFUNC(_find_tkl_nameban));
	EfunctionAddPVoid(modinfo->handle, EFUNC_FIND_TKL_SPAMFILTER, TO_PVOIDFUNC(_find_tkl_spamfilter));
	EfunctionAddVoid(modinfo->handle, EFUNC_TKL_STATS, _tkl_stats);
	EfunctionAddVoid(modinfo->handle, EFUNC_TKL_SYNCH, _tkl_sync);
	EfunctionAddVoid(modinfo->handle, EFUNC_CMD_TKL, _cmd_tkl);
	EfunctionAdd(modinfo->handle, EFUNC_PLACE_HOST_BAN, _place_host_ban);
	EfunctionAdd(modinfo->handle, EFUNC_MATCH_SPAMFILTER, _match_spamfilter);
	EfunctionAdd(modinfo->handle, EFUNC_MATCH_SPAMFILTER_MTAGS, _match_spamfilter_mtags);
	EfunctionAdd(modinfo->handle, EFUNC_JOIN_VIRUSCHAN, _join_viruschan);
	EfunctionAddVoid(modinfo->handle, EFUNC_SPAMFILTER_BUILD_USER_STRING, _spamfilter_build_user_string);
	EfunctionAdd(modinfo->handle, EFUNC_MATCH_USER, _match_user);
	EfunctionAdd(modinfo->handle, EFUNC_TKL_IP_HASH, _tkl_ip_hash);
	EfunctionAdd(modinfo->handle, EFUNC_TKL_IP_HASH_TYPE, _tkl_ip_hash_type);
	EfunctionAddVoid(modinfo->handle, EFUNC_SENDNOTICE_TKL_ADD, _sendnotice_tkl_add);
	EfunctionAddVoid(modinfo->handle, EFUNC_SENDNOTICE_TKL_DEL, _sendnotice_tkl_del);
	EfunctionAdd(modinfo->handle, EFUNC_FIND_TKL_EXCEPTION, _find_tkl_exception);
	EfunctionAddString(modinfo->handle, EFUNC_TKL_UHOST, _tkl_uhost);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkl_config_run_spamfilter);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkl_config_run_ban);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkl_config_run_except);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkl_config_run_set);
	HookAdd(modinfo->handle, HOOKTYPE_IP_CHANGE, 2000000000, tkl_ip_change);
	CommandAdd(modinfo->handle, "GLINE", cmd_gline, 3, CMD_OPER);
	CommandAdd(modinfo->handle, "SHUN", cmd_shun, 3, CMD_OPER);
	CommandAdd(modinfo->handle, "TEMPSHUN", cmd_tempshun, 2, CMD_OPER);
	CommandAdd(modinfo->handle, "ZLINE", cmd_zline, 3, CMD_OPER);
	CommandAdd(modinfo->handle, "KLINE", cmd_kline, 3, CMD_OPER);
	CommandAdd(modinfo->handle, "GZLINE", cmd_gzline, 3, CMD_OPER);
	CommandAdd(modinfo->handle, "SPAMFILTER", cmd_spamfilter, 7, CMD_OPER);
	CommandAdd(modinfo->handle, "ELINE", cmd_eline, 4, CMD_OPER);
	CommandAdd(modinfo->handle, "TKL", _cmd_tkl, MAXPARA, CMD_OPER|CMD_SERVER);
	add_default_exempts();
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	check_mtag_spamfilters_present();
	EventAdd(modinfo->handle, "tklexpire", tkl_check_expire, NULL, 5000, 0);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/** Test a spamfilter { } block in the configuration file */
int tkl_config_test_spamfilter(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep, *cepp;
	int errors = 0;
	char *match = NULL, *reason = NULL;
	char has_target = 0, has_match = 0, has_action = 0, has_reason = 0, has_bantime = 0, has_match_type = 0;
	int match_type = 0;

	/* We are only interested in spamfilter { } blocks */
	if ((type != CONFIG_MAIN) || strcmp(ce->name, "spamfilter"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "target"))
		{
			if (has_target)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "spamfilter::target");
				continue;
			}
			has_target = 1;
			if (cep->value)
			{
				if (!spamfilter_getconftargets(cep->value))
				{
					config_error("%s:%i: unknown spamfiler target type '%s'",
						cep->file->filename, cep->line_number, cep->value);
					errors++;
				}
			}
			else if (cep->items)
			{
				for (cepp = cep->items; cepp; cepp = cepp->next)
				{
					if (!spamfilter_getconftargets(cepp->name))
					{
						config_error("%s:%i: unknown spamfiler target type '%s'",
							cepp->file->filename,
							cepp->line_number, cepp->name);
						errors++;
					}
				}
			}
			else
			{
				config_error_empty(cep->file->filename,
					cep->line_number, "spamfilter", cep->name);
				errors++;
			}
			continue;
		}
		if (!cep->value)
		{
			config_error_empty(cep->file->filename, cep->line_number,
				"spamfilter", cep->name);
			errors++;
			continue;
		}
		if (!strcmp(cep->name, "reason"))
		{
			if (has_reason)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "spamfilter::reason");
				continue;
			}
			has_reason = 1;
			reason = cep->value;
		}
		else if (!strcmp(cep->name, "match"))
		{
			if (has_match)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "spamfilter::match");
				continue;
			}
			has_match = 1;
			match = cep->value;
		}
		else if (!strcmp(cep->name, "action"))
		{
			if (has_action)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "spamfilter::action");
				continue;
			}
			has_action = 1;
			if (!banact_stringtoval(cep->value))
			{
				config_error("%s:%i: spamfilter::action has unknown action type '%s'",
					cep->file->filename, cep->line_number, cep->value);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "ban-time"))
		{
			if (has_bantime)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "spamfilter::ban-time");
				continue;
			}
			has_bantime = 1;
		}
		else if (!strcmp(cep->name, "match-type"))
		{
			if (has_match_type)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "spamfilter::match-type");
				continue;
			}
			if (!strcasecmp(cep->value, "posix"))
			{
				config_error("%s:%i: this spamfilter uses match-type 'posix' which is no longer supported. "
				             "You must switch over to match-type 'regex' instead. "
				             "See https://www.unrealircd.org/docs/FAQ#spamfilter-posix-deprecated",
				             ce->file->filename, ce->line_number);
				errors++;
				*errs = errors;
				return -1; /* return now, otherwise there will be issues */
			}
			match_type = unreal_match_method_strtoval(cep->value);
			if (match_type == 0)
			{
				config_error("%s:%i: spamfilter::match-type: unknown match type '%s', "
				             "should be one of: 'simple', 'regex' or 'posix'",
				             cep->file->filename, cep->line_number,
				             cep->value);
				errors++;
				continue;
			}
			has_match_type = 1;
		}
		else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"spamfilter", cep->name);
			errors++;
			continue;
		}
	}

	if (match && match_type)
	{
		Match *m;
		char *err;

		m = unreal_create_match(match_type, match, &err);
		if (!m)
		{
			config_error("%s:%i: spamfilter::match contains an invalid regex: %s",
				ce->file->filename,
				ce->line_number,
				err);
			errors++;
		} else
		{
			unreal_delete_match(m);
		}
	}

	if (!has_match)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"spamfilter::match");
		errors++;
	}
	if (!has_target)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"spamfilter::target");
		errors++;
	}
	if (!has_action)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"spamfilter::action");
		errors++;
	}
	if (match && reason && (strlen(match) + strlen(reason) > 505))
	{
		config_error("%s:%i: spamfilter block problem: match + reason field are together over 505 bytes, "
		             "please choose a shorter regex or reason",
		             ce->file->filename, ce->line_number);
		errors++;
	}
	if (!has_match_type)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"spamfilter::match-type");
		errors++;
	}

	if (match && !strcmp(match, "^LOL! //echo -a \\$\\(\\$decode\\(.+,m\\),[0-9]\\)$"))
	{
		config_warn("*** IMPORTANT ***");
		config_warn("You have old examples in your spamfilter.conf. "
		             "We suggest you to edit this file and replace the examples.");
		config_warn("Please read https://www.unrealircd.org/docs/FAQ#old-spamfilter-conf !!!");
		config_warn("*****************");
	}
	*errs = errors;
	return errors ? -1 : 1;
}

/** Process a spamfilter { } block in the configuration file */
int tkl_config_run_spamfilter(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	char *word = NULL;
	time_t bantime = tempiConf.spamfilter_ban_time;
	char *banreason = tempiConf.spamfilter_ban_reason;
	int action = 0, target = 0;
	int match_type = 0;
	Match *m;

	/* We are only interested in spamfilter { } blocks */
	if ((type != CONFIG_MAIN) || strcmp(ce->name, "spamfilter"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "match"))
		{
			word = cep->value;
		}
		else if (!strcmp(cep->name, "target"))
		{
			if (cep->value)
				target = spamfilter_getconftargets(cep->value);
			else
			{
				for (cepp = cep->items; cepp; cepp = cepp->next)
					target |= spamfilter_getconftargets(cepp->name);
			}
		}
		else if (!strcmp(cep->name, "action"))
		{
			action = banact_stringtoval(cep->value);
		}
		else if (!strcmp(cep->name, "reason"))
		{
			banreason = cep->value;
		}
		else if (!strcmp(cep->name, "ban-time"))
		{
			bantime = config_checkval(cep->value, CFG_TIME);
		}
		else if (!strcmp(cep->name, "match-type"))
		{
			match_type = unreal_match_method_strtoval(cep->value);
		}
	}

	m = unreal_create_match(match_type, word, NULL);
	tkl_add_spamfilter(TKL_SPAMF,
	                    target,
	                    action,
	                    m,
	                    "-config-",
	                    0,
	                    TStime(),
	                    bantime,
	                    banreason,
	                    TKL_FLAG_CONFIG);
	return 1;
}

/** Test a ban { } block in the configuration file */
int tkl_config_test_ban(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;
	char has_mask = 0, has_reason = 0;

	/* We are only interested in ban { } blocks */
	if (type != CONFIG_BAN)
		return 0;

	if (strcmp(ce->value, "nick") && strcmp(ce->value, "user") &&
	    strcmp(ce->value, "ip"))
	{
		return 0;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (config_is_blankorempty(cep, "ban"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->name, "mask"))
		{
			if (has_mask)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "ban::mask");
				continue;
			}
			has_mask = 1;
		}
		else if (!strcmp(cep->name, "reason"))
		{
			if (has_reason)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "ban::reason");
				continue;
			}
			has_reason = 1;
		}
		else
		{
			config_error("%s:%i: unknown directive ban %s::%s",
				cep->file->filename, cep->line_number,
				ce->value,
				cep->name);
			errors++;
		}
	}

	if (!has_mask)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"ban::mask");
		errors++;
	}

	if (!has_reason)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"ban::reason");
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

/** Process a ban { } block in the configuration file */
int tkl_config_run_ban(ConfigFile *cf, ConfigEntry *ce, int configtype)
{
	ConfigEntry *cep;
	char *usermask = NULL;
	char *hostmask = NULL;
	char *reason = NULL;
	int tkltype;

	/* We are only interested in ban { } blocks */
	if (configtype != CONFIG_BAN)
		return 0;

	if (strcmp(ce->value, "nick") && strcmp(ce->value, "user") &&
	    strcmp(ce->value, "ip"))
	{
		return 0;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "mask"))
		{
			if (is_extended_server_ban(cep->value))
			{
				char mask1buf[512], mask2buf[512];
				char *err = NULL;

				if (!parse_extended_server_ban(cep->value, NULL, &err, 0, mask1buf, sizeof(mask1buf), mask2buf, sizeof(mask2buf)))
				{
					config_warn("%s:%d: Could not add extended server ban '%s': %s",
						cep->file->filename, cep->line_number, cep->value, err);
					goto tcrb_end;
				}
				safe_strdup(usermask, mask1buf);
				safe_strdup(hostmask, mask2buf);
			} else
			{
				char buf[512];
				char *p;

				strlcpy(buf, cep->value, sizeof(buf));
				p = strchr(buf, '@');
				if (p)
				{
					*p++ = '\0';
					safe_strdup(usermask, buf);
					safe_strdup(hostmask, p);
				} else {
					safe_strdup(hostmask, cep->value);
				}
			}
		} else
		if (!strcmp(cep->name, "reason"))
		{
			safe_strdup(reason, cep->value);
		}
	}

	if (!usermask)
		safe_strdup(usermask, "*");

	if (!reason)
		safe_strdup(reason, "-");

	if (!strcmp(ce->value, "nick"))
		tkltype = TKL_NAME;
	else if (!strcmp(ce->value, "user"))
		tkltype = TKL_KILL;
	else if (!strcmp(ce->value, "ip"))
		tkltype = TKL_ZAP;
	else
		abort(); /* impossible */

	if (TKLIsNameBanType(tkltype))
		tkl_add_nameban(tkltype, hostmask, 0, reason, "-config-", 0, TStime(), TKL_FLAG_CONFIG);
	else if (TKLIsServerBanType(tkltype))
		tkl_add_serverban(tkltype, usermask, hostmask, reason, "-config-", 0, TStime(), 0, TKL_FLAG_CONFIG);

tcrb_end:
	safe_free(usermask);
	safe_free(hostmask);
	safe_free(reason);
	return 1;
}

int tkl_config_test_except(ConfigFile *cf, ConfigEntry *ce, int configtype, int *errs)
{
	ConfigEntry *cep, *cepp;
	int errors = 0;
	int has_mask = 0;

	/* We are only interested in except { } blocks */
	if (configtype != CONFIG_EXCEPT)
		return 0;

	/* These are the types that we handle */
	if (strcmp(ce->value, "ban") && strcmp(ce->value, "throttle") &&
	    strcmp(ce->value, "tkl") && strcmp(ce->value, "blacklist") &&
	    strcmp(ce->value, "spamfilter"))
	{
		return 0;
	}

	if (!strcmp(ce->value, "tkl"))
	{
		config_error("%s:%d: except tkl { } has been renamed to except ban { }",
			ce->file->filename, ce->line_number);
		config_status("Please rename your block in the configuration file.");
		*errs = 1;
		return -1;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "mask"))
		{
			if (cep->items)
			{
				/* mask { *@1.1.1.1; *@2.2.2.2; *@3.3.3.3; }; */
				for (cepp = cep->items; cepp; cepp = cepp->next)
				{
					if (!cepp->name)
					{
						config_error_empty(cepp->file->filename,
							cepp->line_number, "except ban", "mask");
						errors++;
						continue;
					}
					has_mask = 1;
				}
			} else
			if (cep->value)
			{
				/* mask *@1.1.1.1; */
				if (!cep->value)
				{
					config_error_empty(cep->file->filename,
						cep->line_number, "except ban", "mask");
					errors++;
					continue;
				}
				has_mask = 1;
			}
		} else
		if (!strcmp(cep->name, "type"))
		{
			if (cep->items)
			{
				/* type { x; y; z; }; */
				for (cepp = cep->items; cepp; cepp = cepp->next)
					if (!tkl_banexception_configname_to_chars(cepp->name))
					{
						config_error("%s:%d: except ban::type '%s' unknown. Must be one of: %s",
							cepp->file->filename, cepp->line_number, cepp->name,
							ALL_VALID_EXCEPTION_TYPES);
						errors++;
					}
			} else
			if (cep->value)
			{
				/* type x; */
				if (!tkl_banexception_configname_to_chars(cep->value))
				{
					config_error("%s:%d: except ban::type '%s' unknown. Must be one of: %s",
						cep->file->filename, cep->line_number, cep->value,
						ALL_VALID_EXCEPTION_TYPES);
					errors++;
				}
			}
		} else {
			config_error_unknown(cep->file->filename,
				cep->line_number, "except", cep->name);
			errors++;
			continue;
		}
	}

	if (!has_mask)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"except ban::mask");
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

void config_create_tkl_except(char *mask, char *bantypes)
{
	char *usermask = NULL;
	char *hostmask = NULL;
	int soft = 0;
	char buf[256];
	char mask1buf[512];
	char mask2buf[512];
	char *p;

	if (*mask == '%')
	{
		soft = 1;
		mask++;
	}
	strlcpy(buf, mask, sizeof(buf));
	if (is_extended_server_ban(buf))
	{
		char *err = NULL;
		if (!parse_extended_server_ban(buf, NULL, &err, 0, mask1buf, sizeof(mask1buf), mask2buf, sizeof(mask2buf)))
		{
			config_warn("Could not add extended server ban '%s': %s", buf, err);
			return;
		}
		usermask = mask1buf;
		hostmask = mask2buf;
	} else
	{
		p = strchr(buf, '@');
		if (!p)
		{
			usermask = "*";
			hostmask = buf;
		} else {
			*p++ = '\0';
			usermask = buf;
			hostmask = p;
		}
	}

	if ((*usermask == ':') || (*hostmask == ':'))
	{
		config_error("Cannot add illegal ban '%s': for a given user@host - neither "
		             "user nor host may start with a : character (semicolon)", mask);
		return;
	}

	tkl_add_banexception(TKL_EXCEPTION, usermask, hostmask, "Added in configuration file",
	                     "-config-", 0, TStime(), soft, bantypes, TKL_FLAG_CONFIG);
}

int tkl_config_run_except(ConfigFile *cf, ConfigEntry *ce, int configtype)
{
	ConfigEntry *cep, *cepp;
	char bantypes[64];

	/* We are only interested in except { } blocks */
	if (configtype != CONFIG_EXCEPT)
		return 0;

	/* These are the types that we handle */
	if (strcmp(ce->value, "ban") && strcmp(ce->value, "throttle") &&
	    strcmp(ce->value, "blacklist") &&
	    strcmp(ce->value, "spamfilter"))
	{
		return 0;
	}

	*bantypes = '\0';

	/* First configure all the types */
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "type"))
		{
			if (cep->items)
			{
				/* type { x; y; z; }; */
				for (cepp = cep->items; cepp; cepp = cepp->next)
				{
					char *str = tkl_banexception_configname_to_chars(cepp->name);
					strlcat(bantypes, str, sizeof(bantypes));
				}
			} else
			if (cep->value)
			{
				/* type x; */
				char *str = tkl_banexception_configname_to_chars(cep->value);
				strlcat(bantypes, str, sizeof(bantypes));
			}
		}
	}

	if (!*bantypes)
	{
		/* Default setting if no 'type' is specified: */
		if (!strcmp(ce->value, "ban"))
			strlcpy(bantypes, "kGzZs", sizeof(bantypes));
		else if (!strcmp(ce->value, "throttle"))
			strlcpy(bantypes, "c", sizeof(bantypes));
		else if (!strcmp(ce->value, "blacklist"))
			strlcpy(bantypes, "b", sizeof(bantypes));
		else if (!strcmp(ce->value, "spamfilter"))
			strlcpy(bantypes, "f", sizeof(bantypes));
		else
			abort(); /* someone can't code */
	}

	/* Now walk through all mask entries */
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "mask"))
		{
			if (cep->items)
			{
				/* mask { *@1.1.1.1; *@2.2.2.2; *@3.3.3.3; }; */
				for (cepp = cep->items; cepp; cepp = cepp->next)
					config_create_tkl_except(cepp->name, bantypes);
			} else
			if (cep->value)
			{
				/* mask *@1.1.1.1; */
				config_create_tkl_except(cep->value, bantypes);
			}
		}
	}

	return 1;
}

int tkl_config_test_set(ConfigFile *cf, ConfigEntry *ce, int configtype, int *errs)
{
	int errors = 0;

	/* We are only interested in set { } blocks */
	if (configtype != CONFIG_SET)
		return 0;

	if (!strcmp(ce->name, "max-stats-matches"))
	{
		if (!ce->value)
		{
			config_error("%s:%i: set::max-stats-matches: no value specified",
				ce->file->filename, ce->line_number);
			errors++;
		}
		// allow any other value, including 0 and negative.
		*errs = errors;
		return errors ? -1 : 1;
	}
	return 0;
}

int tkl_config_run_set(ConfigFile *cf, ConfigEntry *ce, int configtype)
{
	/* We are only interested in set { } blocks */
	if (configtype != CONFIG_SET)
		return 0;

	if (!strcmp(ce->name, "max-stats-matches"))
	{
		max_stats_matches = atoi(ce->value);
		return 1;
	}

	return 0;
}

/** Return unique spamfilter id for TKL */
char *spamfilter_id(TKL *tk)
{
	static char buf[128];

	snprintf(buf, sizeof(buf), "%p", (void *)tk);
	return buf;
}

int tkl_ip_change(Client *client, const char *oldip)
{
	check_banned(client, 0);
	return 0;
}

/** GLINE - Global kline.
** Syntax: /gline [+|-]u@h mask time :reason
**
** parv[1] = [+|-]u@h mask
** parv[2] = for how long
** parv[3] = reason
*/
CMD_FUNC(cmd_gline)
{
	if (IsServer(client))
		return;

	if (!ValidatePermissionsForPath("server-ban:gline",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (parc == 1)
	{
		const char *parv[3];
		parv[0] = NULL;
		parv[1] = "gline";
		parv[2] = NULL;
		do_cmd(client, recv_mtags, "STATS", 2, parv);
	} else
	{
		cmd_tkl_line(client, parc, parv, "G");
	}
}

/** GZLINE - Global zline.
 */
CMD_FUNC(cmd_gzline)
{
	if (IsServer(client))
		return;

	if (!ValidatePermissionsForPath("server-ban:zline:global",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (parc == 1)
	{
		const char *parv[3];
		parv[0] = NULL;
		parv[1] = "gline"; /* (there's no /STATS gzline, it's included in /STATS gline output) */
		parv[2] = NULL;
		do_cmd(client, recv_mtags, "STATS", 2, parv);
	} else {
		cmd_tkl_line(client, parc, parv, "Z");
	}
}

/** SHUN - Shun a user so it can no longer execute any meaningful commands.
 */
CMD_FUNC(cmd_shun)
{
	if (IsServer(client))
		return;

	if (!ValidatePermissionsForPath("server-ban:shun",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (parc == 1)
	{
		const char *parv[3];
		parv[0] = NULL;
		parv[1] = "shun";
		parv[2] = NULL;
		do_cmd(client, recv_mtags, "STATS", 2, parv);
	} else {
		cmd_tkl_line(client, parc, parv, "s");
	}
}

/** TEMPSHUN - Temporarily shun a user so it can no longer execute
 *  any meaningful commands - until the user disconnects (session only).
 */
CMD_FUNC(cmd_tempshun)
{
	Client *target;
	const char *comment = ((parc > 2) && !BadPtr(parv[2])) ? parv[2] : "no reason";
	const char *name;
	int remove = 0;

	if (MyUser(client) && (!ValidatePermissionsForPath("server-ban:shun:temporary",client,NULL,NULL,NULL)))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}
	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "TEMPSHUN");
		return;
	}
	if (parv[1][0] == '+')
		name = parv[1]+1;
	else if (parv[1][0] == '-')
	{
		name = parv[1]+1;
		remove = 1;
	} else
		name = parv[1];

	target = find_user(name, NULL);
	if (!target)
	{
		sendnumeric(client, ERR_NOSUCHNICK, name);
		return;
	}
	if (!MyUser(target))
	{
		sendto_one(target, NULL, ":%s TEMPSHUN %c%s :%s",
		           client->id, remove ? '-' : '+', target->id, comment);
	} else {
		char buf[1024];
		if (!remove)
		{
			if (IsShunned(target))
			{
				sendnotice(client, "User '%s' already shunned", target->name);
			} else if (ValidatePermissionsForPath("immune:server-ban:shun",target,NULL,NULL,NULL))
			{
				sendnotice(client, "You cannot tempshun '%s' because (s)he is an oper with 'immune:server-ban:shun' privilege", target->name);
			} else
			{
				SetShunned(target);
				unreal_log(ULOG_INFO, "tkl", "TKL_ADD_TEMPSHUN", client,
					   "Temporary shun added on user $target.details [reason: $shun_reason] [by: $client]",
					   log_data_string("shun_reason", comment),
					   log_data_client("target", target));
			}
		} else {
			if (!IsShunned(target))
			{
				sendnotice(client, "User '%s' is not shunned", target->name);
			} else {
				ClearShunned(target);
				unreal_log(ULOG_INFO, "tkl", "TKL_DEL_TEMPSHUN", client,
					   "Temporary shun removed from user $target.details [by: $client]",
					   log_data_client("target", target));
			}
		}
	}
}

/** KLINE - Kill line (ban user from local server)
 */
CMD_FUNC(cmd_kline)
{
	if (IsServer(client))
		return;

	if (!ValidatePermissionsForPath("server-ban:kline:local:add",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (parc == 1)
	{
		const char *parv[3];
		parv[0] = NULL;
		parv[1] = "kline";
		parv[2] = NULL;
		do_cmd(client, recv_mtags, "STATS", 2, parv);
		return;
	}

	if (!ValidatePermissionsForPath("server-ban:kline:remove",client,NULL,NULL,NULL) && *parv[1] == '-')
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	cmd_tkl_line(client, parc, parv, "k");
}

/** Generate stats for '/GLINE -stats' and such */
void tkl_general_stats(Client *client)
{
	int index, index2;
	TKL *tkl;
	int total = 0;
	int subtotal;

	/* First, hashed entries.. */
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			subtotal = 0;
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
				subtotal++;
			if (subtotal > 0)
				sendnotice(client, "Slot %d:%d has %d item(s)", index, index2, subtotal);
			total += subtotal;
		}
	}
	sendnotice(client, "Hashed TKL items: %d item(s)", total);

	/* Now normal entries.. */
	subtotal = 0;
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
			subtotal++;
	}
	sendnotice(client, "Standard TKL items: %d item(s)", subtotal);
	total += subtotal;
	sendnotice(client, "Grand total TKL items: %d item(s)", total);
}

/** ZLINE - Kill a user as soon as it tries to connect to the server.
 * This happens before any DNS/ident lookups have been done and
 * before any data has been processed (including no TLS handshake, etc.)
 */
CMD_FUNC(cmd_zline)
{
	if (IsServer(client))
		return;

	if (!ValidatePermissionsForPath("server-ban:zline:local:add",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (parc == 1)
	{
		const char *parv[3];
		parv[0] = NULL;
		parv[1] = "kline"; /* (there's no /STATS zline, it's included in /STATS kline output) */
		parv[2] = NULL;
		do_cmd(client, recv_mtags, "STATS", 2, parv);
		return;
	}

	if ((parc > 1) && !BadPtr(parv[1]) && !strcasecmp(parv[1], "-stats"))
	{
		/* Print some statistics */
		tkl_general_stats(client);
		return;
	}

	cmd_tkl_line(client, parc, parv, "z");
}

/** Check if a ban is placed with a too broad mask (like '*') */
int ban_too_broad(char *usermask, char *hostmask)
{
	char *p;
	int cnt = 0;

	/* Scary config setting. Hmmm. */
	if (ALLOW_INSANE_BANS)
		return 0;

	/* Allow things like clone@*, dsfsf@*, etc.. */
	if (!strchr(usermask, '*') && !strchr(usermask, '?'))
		return 0;

	/* If it's a CIDR, then check /mask first.. */
	p = strchr(hostmask, '/');
	if (p)
	{
		int cidrlen = atoi(p+1);
		if (strchr(hostmask, ':'))
		{
			if (cidrlen < 48)
				return 1; /* too broad IPv6 CIDR mask */
		} else {
			if (cidrlen < 16)
				return 1; /* too broad IPv4 CIDR mask */
		}
	}

	/* Must at least contain 4 non-wildcard/non-dot characters.
	 * This will deal with non-CIDR and hosts, but any correct
	 * CIDR mask will also pass this test (which is fine).
	 */
	for (p = hostmask; *p; p++)
		if (*p != '*' && *p != '.' && *p != '?')
			cnt++;

	if (cnt >= 4)
		return 0;

	return 1;
}

/** Ugly function, only meant to be called by cmd_tkl_line() */
static int xline_exists(char *type, char *usermask, char *hostmask)
{
	char *umask = usermask;
	int softban = 0;
	int tpe = tkl_chartotype(type[0]);

	if (*umask == '%')
	{
		umask++;
		softban = 1;
	}

	return find_tkl_serverban(tpe, umask, hostmask, softban) ? 1 : 0;
}

/** Parse an extended server ban such as ~S:aabbccddetc..
 * Used for both syntax checking and to split it into userbuf/hostbuf for TKL protocol.
 * @param mask_in	The input mask (eg: ~S:aabbccddetc)
 * @param client	Client doing the request (used to send errors), can be NULL.
 * @param error		Pointer to set to the error buffer (must be set!)
 * @param skip_checking	Set this to 1 if coming from a remote user/server to skip the .is_ok() check.
 *                      Note that a .conv_param() call can still fail.
 * @param buf1		Buffer to store the extban starter in (eg "~S:") -- can be NULL if you don't need it
 * @param buf1len	Length of buf1
 * @param buf2		Buffer to store the extban remainder in (eg "aabbccddetc") -- can be NULL if you don't need it
 * @param buf2len	Length of buf2
 * @returns 1 if the server ban is acceptable. The ban will then be stored in buf1/buf2 (unless those
 *            were set to NULL by the caller). On failure we return 0 and 'error' is set appropriately.
 */
int parse_extended_server_ban(const char *mask_in, Client *client, char **error, int skip_checking, char *buf1, size_t buf1len, char *buf2, size_t buf2len)
{
	const char *nextbanstr = NULL;
	Extban *extban;
	const char *str;
	char *p;
	BanContext *b = NULL;
	char    mask[USERLEN + NICKLEN + HOSTLEN + 32]; // same as extban_conv_param_nuh_or_extban()
	char newmask[USERLEN + NICKLEN + HOSTLEN + 32];
	char soft_ban = 0;

	*error = NULL;
	if (buf1 && buf2)
		*buf1 = *buf2 = '\0';

	/* Work on a copy */
	if (*mask_in == '%')
	{
		strlcpy(mask, mask_in+1, sizeof(mask));
		soft_ban = 1;
	} else {
		strlcpy(mask, mask_in, sizeof(mask));
	}

	extban = findmod_by_bantype(mask, &nextbanstr);
	if (!extban || !(extban->options & EXTBOPT_TKL))
	{
		*error = "Invalid or unsupported extended server ban requested. Valid types are for example ~a, ~r, ~S.";
		goto fail_parse_extended_server_ban;
	}

	b = safe_alloc(sizeof(BanContext));
	b->client = client;
	b->banstr = nextbanstr;
	b->is_ok_check = EXBCHK_PARAM;
	b->what = MODE_ADD;
	b->ban_type = EXBTYPE_TKL;

	/* Run .is_ok() for the extban. This check is skipped if coming from a remote user/server */
	if (skip_checking == 0)
	{
		if (extban->is_ok && !extban->is_ok(b))
		{
			*error = "Invalid extended server ban";
			goto fail_parse_extended_server_ban;
		}
	}

	b->banstr = nextbanstr;
	str = extban->conv_param(b, extban);
	if (!str)
	{
		*error = "Invalid extended server ban";
		goto fail_parse_extended_server_ban;
	}
	str = prefix_with_extban(str, b, extban, newmask, sizeof(newmask));
	if (str == NULL)
	{
		*error = "Unexpected error (1)";
		goto fail_parse_extended_server_ban;
	}

	p = strchr(newmask, ':');
	if (!p)
	{
		*error = "Unexpected error (2)";
		goto fail_parse_extended_server_ban;
	}

	if (p[1] == ':')
	{
		*error = "For technical reasons you cannot use a double : at the beginning of an extended server ban (eg ~a::xyz)";
		goto fail_parse_extended_server_ban;
	}

	if (!p[1])
	{
		*error = "Empty / too short extended server ban";
		goto fail_parse_extended_server_ban;
	}

	/* Now convert the result into two buffers for TKL protocol usage */
	if (buf1 && buf2)
	{
		char save;
		p++;
		save = *p;
		*p = '\0';
		/* First buffer is eg ~S: or %~S: */
		snprintf(buf1, buf1len, "%s%s",
		         soft_ban ? "%" : "",
		         newmask);
		*p = save;
		strlcpy(buf2, p, buf2len); /* eg 1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef */
	}
	safe_free(b);
	return 1;

fail_parse_extended_server_ban:
	safe_free(b);
	return 0;
}


/** Intermediate layer between user functions such as KLINE/GLINE
 * and the TKL layer (cmd_tkl).
 * This allows us doing some syntax checking and other helpful
 * things that are the same for many types of *LINES.
 */
void cmd_tkl_line(Client *client, int parc, const char *parv[], char *type)
{
	time_t secs;
	int add = 1;
	time_t i;
	Client *acptr = NULL;
	char maskbuf[BUFSIZE];
	char *mask;
	char mo[64], mo2[64];
	char mask1buf[BUFSIZE];
	char mask2buf[BUFSIZE];
	char *p, *usermask, *hostmask;
	const char *tkllayer[10] = {
		me.name,		/*0  server.name */
		NULL,			/*1  +|- */
		NULL,			/*2  G   */
		NULL,			/*3  user */
		NULL,			/*4  host */
		NULL,			/*5  set_by */
		"0",			/*6  expire_at */
		NULL,			/*7  set_at */
		"no reason",	/*8  reason */
		NULL
	};
	struct tm *t;

	if ((parc == 1) || BadPtr(parv[1]))
		return; /* shouldn't happen */

	strlcpy(maskbuf, parv[1], sizeof(maskbuf));
	mask = maskbuf;
	if (*mask == '-')
	{
		add = 0;
		mask++;
	}
	else if (*mask == '+')
	{
		add = 1;
		mask++;
	}

	if ((*mask != '~') && strchr(mask, '!'))
	{
		sendnotice(client, "[error] Cannot have '!' in masks.");
		return;
	}
	if (*mask == ':')
	{
		sendnotice(client, "[error] Mask cannot start with a ':'.");
		return;
	}
	if (strchr(mask, ' '))
		return;

	/* Check if it's a softban */
	if (*mask == '%')
	{
		if (!strchr("kGs", *type))
		{
			sendnotice(client, "The %% prefix (soft ban) is only available for KLINE, GLINE and SHUN."
			                 "For technical reasons this will not work for (G)ZLINE.");
			return;
		}
	}

	/* Check if it's an extended server ban */
	if (is_extended_server_ban(mask))
	{
		char *err;

		if (!parse_extended_server_ban(mask, client, &err, 0, mask1buf, sizeof(mask1buf), mask2buf, sizeof(mask2buf)))
		{
			/* If adding, reject it */
			if (add)
			{
				sendnotice(client, "ERROR: %s", err);
				return;
			} else
			{
				/* Always allow any removal attempt... */
				char *p;
				char save;
				p = strchr(mask, ':');
				p++;
				save = *p;
				*p = '\0';
				strlcpy(mask1buf, mask, sizeof(mask1buf));
				*p = save;
				strlcpy(mask2buf, p, sizeof(mask2buf));
				/* fallthrough */
			}
		}
		if (add && ((*type == 'z') || (*type == 'Z')))
		{
			sendnotice(client, "ERROR: (g)zlines must be placed at *@\037IPMASK\037. "
					   "Extended server bans don't work here because (g)zlines are processed"
					   "BEFORE dns and ident lookups are done and before reading any client data. "
					   "If you want to use extended server bans then use a KLINE/GLINE instead.");
			return;
		}
		usermask = mask1buf; /* eg ~S: */
		hostmask = mask2buf; /* eg 1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef */
	} else
	{
		/* Check if it's a hostmask and legal .. */
		p = strchr(mask, '@');
		if (p) {
			if ((p == mask) || !p[1])
			{
				sendnotice(client, "Error: no user@host specified");
				return;
			}
			usermask = strtok(mask, "@");
			hostmask = strtok(NULL, "");
			if (BadPtr(hostmask)) {
				if (BadPtr(usermask)) {
					return;
				}
				hostmask = usermask;
				usermask = "*";
			}
			if (*hostmask == ':')
			{
				sendnotice(client, "[error] For technical reasons you cannot start the host with a ':', sorry");
				return;
			}
			if (add && ((*type == 'z') || (*type == 'Z')))
			{
				/* It's a (G)ZLINE, make sure the user isn't specyfing a HOST.
				 * Just a warning in 3.2.3, but an error in 3.2.4.
				 */
				if (strcmp(usermask, "*"))
				{
					sendnotice(client, "ERROR: (g)zlines must be placed at \037*\037@ipmask, not \037user\037@ipmask. This is "
							 "because (g)zlines are processed BEFORE dns and ident lookups are done. "
							 "If you want to use usermasks, use a KLINE/GLINE instead.");
					return;
				}
				for (p=hostmask; *p; p++)
					if (isalpha(*p) && !isxdigit(*p))
					{
						sendnotice(client, "ERROR: (g)zlines must be placed at *@\037IPMASK\037, not *@\037HOSTMASK\037 "
								 "(so for example *@192.168.* is ok, but *@*.aol.com is not). "
								 "This is because (g)zlines are processed BEFORE dns and ident lookups are done. "
								 "If you want to use hostmasks instead of ipmasks, use a KLINE/GLINE instead.");
						return;
					}
			}
		}
		else
		{
			/* It's seemingly a nick .. let's see if we can find the user */
			if ((acptr = find_user(mask, NULL)))
			{
				BanAction action = BAN_ACT_KLINE; // just a dummy default
				if ((*type == 'z') || (*type == 'Z'))
					action = BAN_ACT_ZLINE; // to indicate zline (no hostname, no dns, etc)
				ban_target_to_tkl_layer(iConf.manual_ban_target, action, acptr, (const char **)&usermask, (const char **)&hostmask);
			}
			else
			{
				sendnumeric(client, ERR_NOSUCHNICK, mask);
				return;
			}
		}
	}

	if (add && ban_too_broad(usermask, hostmask))
	{
		sendnotice(client, "*** [error] Too broad mask");
		return;
	}

	secs = 0;

	if (add && (parc > 3))
	{
		secs = config_checkval(parv[2], CFG_TIME);
		if (secs < 0)
		{
			sendnotice(client, "*** [error] The time you specified is out of range!");
			return;
		}
	}
	tkllayer[1] = add ? "+" : "-";
	tkllayer[2] = type;
	tkllayer[3] = usermask;
	tkllayer[4] = hostmask;
	tkllayer[5] = make_nick_user_host(client->name, client->user->username, GetHost(client));
	if (add)
	{
		if (secs == 0)
		{
			if (DEFAULT_BANTIME && (parc <= 3))
				ircsnprintf(mo, sizeof(mo), "%lld", (long long)(DEFAULT_BANTIME + TStime()));
			else
				ircsnprintf(mo, sizeof(mo), "%lld", (long long)secs); /* "0" */
		}
		else
			ircsnprintf(mo, sizeof(mo), "%lld", (long long)(secs + TStime()));
		ircsnprintf(mo2, sizeof(mo2), "%lld", (long long)TStime());
		tkllayer[6] = mo;
		tkllayer[7] = mo2;
		if (parc > 3) {
			tkllayer[8] = parv[3];
		} else if (parc > 2) {
			tkllayer[8] = parv[2];
		}
		/* Blerghhh... */
		i = atol(mo);
		t = gmtime(&i);
		if (!t)
		{
			sendnotice(client, "*** [error] The time you specified is out of range");
			return;
		}

		/* Some stupid checking */
		if (xline_exists(type, usermask, hostmask))
		{
			sendnotice(client, "ERROR: Ban for %s@%s already exists.", usermask, hostmask);
			return;
		}

		/* call the tkl layer .. */
		cmd_tkl(&me, NULL, 9, tkllayer);
	}
	else
	{
		/* call the tkl layer .. */
		cmd_tkl(&me, NULL, 6, tkllayer);

	}
}

void eline_syntax(Client *client)
{
	sendnotice(client, " Syntax: /ELINE <user@host> <bantypes> <expiry-time> <reason>");
	sendnotice(client, "     Or: /ELINE <extserverban> <bantypes> <expiry-time> <reason>");
	sendnotice(client, "Valid bantypes are:");
	sendnotice(client, "k: K-Line     G: G-Line");
	sendnotice(client, "z: Z-Line     Z: Global Z-Line");
	sendnotice(client, "Q: Q-Line");
	sendnotice(client, "s: Shun");
	sendnotice(client, "F: Spamfilter");
	sendnotice(client, "b: Blacklist checking");
	sendnotice(client, "c: Connect flood (bypass set::anti-flood::connect-flood))");
	sendnotice(client, "d: Handshake data flood (no ZLINE on too much data before registration)");
	sendnotice(client, "m: Bypass allow::maxperip restriction");
	sendnotice(client, "r: Bypass antirandom module");
	sendnotice(client, "8: Bypass antimixedutf8 module");
	sendnotice(client, "v: Bypass ban version { } blocks");
	sendnotice(client, "Examples:");
	sendnotice(client, "/ELINE *@unrealircd.org kGF 0 This user is exempt");
	sendnotice(client, "/ELINE ~S:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef kGF 0 Trusted user with this certificate fingerprint");
	sendnotice(client, "-");
	sendnotice(client, "To get a list of all current ELINEs, type: /STATS except");
}

/** Check if any of the specified types require the
 * exception to be placed on *@ip rather than
 * user@host or *@host. For eg zlines.
 */
TKLTypeTable *eline_type_requires_ip(const char *bantypes)
{
	int i;

	for (i=0; tkl_types[i].config_name; i++)
		if (tkl_types[i].needip && strchr(bantypes, tkl_types[i].letter))
			return &tkl_types[i];
	return NULL;
}

/** Checks a string to see if it contains invalid ban exception types */
int contains_invalid_server_ban_exception_type(const char *str, char *c)
{
	const char *p;
	for (p = str; *p; p++)
	{
		if (!tkl_banexception_chartotype(*p))
		{
			*c = *p;
			return 1;
		}
	}
	return 0;
}

CMD_FUNC(cmd_eline)
{
	time_t secs = 0;
	int add = 1;
	Client *acptr = NULL;
	char *mask = NULL;
	char mo[64], mo2[64];
	char maskbuf[BUFSIZE];
	char mask1buf[BUFSIZE];
	char mask2buf[BUFSIZE];
	const char *p, *bantypes=NULL, *reason=NULL;
	char *usermask, *hostmask;
	const char *tkllayer[11] = {
		me.name,		/*0  server.name */
		NULL,			/*1  +|- */
		NULL,			/*2  E   */
		NULL,			/*3  user */
		NULL,			/*4  host */
		NULL,			/*5  set_by */
		"0",			/*6  expire_at */
		"-",			/*7  set_at */
		"-",			/*8  ban types */
		"-",			/*9  reason */
		NULL
	};
	TKLTypeTable *t;

	if (IsServer(client))
		return;

	if (!ValidatePermissionsForPath("server-ban:eline",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	/* For del we need at least:
	 * ELINE -user@host
	 * The 'add' case is checked later.
	 */
	if ((parc < 2) || BadPtr(parv[1]))
	{
		eline_syntax(client);
		return;
	}

	strlcpy(maskbuf, parv[1], sizeof(maskbuf));
	mask = maskbuf;
	if (*mask == '-')
	{
		add = 0;
		mask++;
	}
	else if (*mask == '+')
	{
		add = 1;
		mask++;
	}

	/* For add we need more:
	 * ELINE user@host bantypes expiry :reason
	 */
	if (add)
	{
		if ((parc < 5) || BadPtr(parv[4]))
		{
			eline_syntax(client);
			return;
		}
		bantypes = parv[2];
		reason = parv[4];
	}

	if ((*mask != '~') && strchr(mask, '!'))
	{
		sendnotice(client, "[error] Cannot have '!' in masks.");
		return;
	}
	if (*mask == ':')
	{
		sendnotice(client, "[error] Mask cannot start with a ':'.");
		return;
	}
	if (strchr(mask, ' '))
		return;

	/* Check if it's an extended server ban */
	if (is_extended_server_ban(mask))
	{
		char *err;
		if (!parse_extended_server_ban(mask, client, &err, 0, mask1buf, sizeof(mask1buf), mask2buf, sizeof(mask2buf)))
		{
			/* If adding, reject it */
			if (add)
			{
				sendnotice(client, "ERROR: %s", err);
				return;
			} else
			{
				/* Always allow any removal attempt... */
				char *p;
				char save;
				p = strchr(mask, ':');
				p++;
				save = *p;
				*p = '\0';
				strlcpy(mask1buf, mask, sizeof(mask1buf));
				*p = save;
				strlcpy(mask2buf, p, sizeof(mask2buf));
				/* fallthrough */
			}
		}
		if (add && (t = eline_type_requires_ip(bantypes)))
		{
			sendnotice(client, "ERROR: Ban exception with type '%c' does not work on extended server bans. "
					   "This is because checking for %s takes places BEFORE "
					   "extended bans can be checked.", t->letter, t->log_name);
			return;
		}
		usermask = mask1buf; /* eg ~S: */
		hostmask = mask2buf; /* eg 1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef */
	} else
	{
		/* Check if it's a hostmask and legal .. */
		p = strchr(mask, '@');
		if (p)
		{
			if ((p == mask) || !p[1])
			{
				sendnotice(client, "Error: no user@host specified");
				return;
			}
			usermask = strtok(mask, "@");
			hostmask = strtok(NULL, "");
			if (BadPtr(hostmask)) {
				if (BadPtr(usermask)) {
					return;
				}
				hostmask = usermask;
				usermask = "*";
			}
			if (*hostmask == ':')
			{
				sendnotice(client, "[error] For technical reasons you cannot start the host with a ':', sorry");
				return;
			}
			if (add && ((t = eline_type_requires_ip(bantypes))))
			{
				/* Trying to exempt a user from a (G)ZLINE,
				 * make sure the user isn't specifying a host then.
				 */
				if (strcmp(usermask, "*"))
				{
					sendnotice(client, "ERROR: Ban exception with type '%c' need to be placed at \037*\037@ipmask, not \037user\037@ipmask. "
					                   "This is because checking %s takes places (possibly) BEFORE any dns and ident lookups.",
					                   t->letter,
					                   t->log_name);
					return;
				}
				for (p=hostmask; *p; p++)
				{
					if (isalpha(*p) && !isxdigit(*p))
					{
						sendnotice(client, "ERROR: Ban exception with type '%c' needs to be placed at *@\037ipmask\037, not *@\037hostmask\037. "
						                   "(so for example *@192.168.* is OK, but *@*.aol.com is not). "
						                   "This is because checking %s takes places (possibly) BEFORE any dns and ident lookups.",
						                   t->letter,
						                   t->log_name);
						return;
					}
				}
			}
		}
		else
		{
			/* It's seemingly a nick .. let's see if we can find the user */
			if ((acptr = find_user(mask, NULL)))
			{
				BanAction action = BAN_ACT_KLINE; // just a dummy default
				if (add && eline_type_requires_ip(bantypes))
					action = BAN_ACT_ZLINE; // to indicate zline (no hostname, no dns, etc)
				ban_target_to_tkl_layer(iConf.manual_ban_target, action, acptr, (const char **)&usermask, (const char **)&hostmask);
			}
			else
			{
				sendnumeric(client, ERR_NOSUCHNICK, mask);
				return;
			}
		}
	}

	if (add)
	{
		secs = config_checkval(parv[3], CFG_TIME);
		if ((secs <= 0) && (*parv[3] != '0'))
		{
			sendnotice(client, "*** [error] The expiry time you specified is out of range!");
			eline_syntax(client);
			return;
		}
	}

	tkllayer[1] = add ? "+" : "-";
	tkllayer[2] = "E";
	tkllayer[3] = usermask;
	tkllayer[4] = hostmask;
	tkllayer[5] = make_nick_user_host(client->name, client->user->username, GetHost(client));

	if (add)
	{
		char c;
		/* Add ELINE */
		if (secs == 0)
			ircsnprintf(mo, sizeof(mo), "%lld", (long long)secs); /* "0" */
		else
			ircsnprintf(mo, sizeof(mo), "%lld", (long long)(secs + TStime()));
		ircsnprintf(mo2, sizeof(mo2), "%lld", (long long)TStime());
		tkllayer[6] = mo;
		tkllayer[7] = mo2;
		tkllayer[8] = bantypes;
		if (contains_invalid_server_ban_exception_type(bantypes, &c))
		{
			sendnotice(client, "ERROR: bantype '%c' is unrecognized (in '%s'). "
			                   "Note that the bantypes are case sensitive. "
			                   "Type /ELINE to see a list of all possible bantypes.",
			                   c, bantypes);
			return;
		}
		tkllayer[9] = reason;
		/* call the tkl layer .. */
		cmd_tkl(&me, NULL, 10, tkllayer);
	}
	else
	{
		/* Remove ELINE */
		/* call the tkl layer .. */
		cmd_tkl(&me, NULL, 10, tkllayer);

	}
}


/** Helper function for cmd_spamfilter, explaining usage. */
void spamfilter_usage(Client *client)
{
	sendnotice(client, "Use: /spamfilter [add|del|remove|+|-] [-simple|-regex] [type] [action] [tkltime] [tklreason] [regex]");
	sendnotice(client, "See '/helpop ?spamfilter' for more information.");
	sendnotice(client, "For an easy way to remove an existing spamfilter, use '/spamfilter del' without additional parameters");
}

/** Helper function for cmd_spamfilter, explaining usage has changed. */
void spamfilter_new_usage(Client *client, const char *parv[])
{
	sendnotice(client, "Unknown match-type '%s'. Must be one of: -regex (new fast PCRE regexes) or "
	                 "-simple (simple text with ? and * wildcards)",
	                 parv[2]);

	if (*parv[2] != '-')
		sendnotice(client, "Using the old 3.2.x /SPAMFILTER syntax? Note the new -regex/-simple field!!");

	spamfilter_usage(client);
}

/** Delete a spamfilter by ID (the ID can be obtained via '/SPAMFILTER del' */
void spamfilter_del_by_id(Client *client, const char *id)
{
	int index;
	TKL *tk;
	int found = 0;
	char mo[32], mo2[32];
	const char *tkllayer[13] = {
		me.name,	/*  0 server.name */
		NULL,		/*  1 +|- */
		"F",		/*  2 F   */
		NULL,		/*  3 usermask (targets) */
		NULL,		/*  4 hostmask (action) */
		NULL,		/*  5 set_by */
		"0",		/*  6 expire_at */
		"0",		/*  7 set_at */
		"",			/*  8 tkl time */
		"",			/*  9 tkl reason */
		"",			/* 10 match method */
		"",			/* 11 regex */
		NULL
	};

	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tk = tklines[index]; tk; tk = tk->next)
		{
			if (((tk->type & (TKL_GLOBAL|TKL_SPAMF)) == (TKL_GLOBAL|TKL_SPAMF)) && !strcmp(spamfilter_id(tk), id))
			{
				found = 1;
				break;
			}
		}
		if (found)
			break; /* break outer loop */
	}

	if (!tk)
	{
		sendnotice(client, "Sorry, no spamfilter found with that ID. Did you run '/spamfilter del' to get the appropriate id?");
		return;
	}

	/* Spamfilter found. Now fill the tkllayer */
	tkllayer[1] = "-";
	tkllayer[3] = spamfilter_target_inttostring(tk->ptr.spamfilter->target); /* target(s) */
	mo[0] = banact_valtochar(tk->ptr.spamfilter->action);
	mo[1] = '\0';
	tkllayer[4] = mo; /* action */
	tkllayer[5] = make_nick_user_host(client->name, client->user->username, GetHost(client));
	tkllayer[8] = "-";
	tkllayer[9] = "-";
	tkllayer[10] = unreal_match_method_valtostr(tk->ptr.spamfilter->match->type); /* matching type */
	tkllayer[11] = tk->ptr.spamfilter->match->str; /* regex */
	ircsnprintf(mo2, sizeof(mo2), "%lld", (long long)TStime());
	tkllayer[7] = mo2; /* deletion time */

	cmd_tkl(&me, NULL, 12, tkllayer);
}

/** Spamfilter to fight spam, advertising, worms and other bad things on IRC.
 * See https://www.unrealircd.org/docs/Spamfilter for general documentation.
 *
 * /SPAMFILTER [add|del|remove|+|-] [match-type] [type] [action] [tkltime] [reason] [regex]
 *                   1                    2         3       4        5        6        7
 */
CMD_FUNC(cmd_spamfilter)
{
	int add = 1;
	char mo[32], mo2[32];
	const char *tkllayer[13] = {
		me.name,	/*  0 server.name */
		NULL,		/*  1 +|- */
		"F",		/*  2 F   */
		NULL,		/*  3 usermask (targets) */
		NULL,		/*  4 hostmask (action) */
		NULL,		/*  5 set_by */
		"0",		/*  6 expire_at */
		"0",		/*  7 set_at */
		"",			/*  8 tkl time */
		"",			/*  9 tkl reason */
		"",			/* 10 match method */
		"",			/* 11 regex */
		NULL
	};
	int targets = 0, action = 0;
	char targetbuf[64], actionbuf[2];
	char reason[512];
	int n;
	Match *m;
	int match_type = 0;
	char *err = NULL;

	if (IsServer(client))
		return;

	if (!ValidatePermissionsForPath("server-ban:spamfilter",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (parc == 1)
	{
		const char *parv[3];
		parv[0] = NULL;
		parv[1] = "spamfilter";
		parv[2] = NULL;
		do_cmd(client, recv_mtags, "STATS", 2, parv);
		return;
	}

	if ((parc <= 3) && !strcmp(parv[1], "del"))
	{
		if (!parv[2])
		{
			/* Show STATS with appropriate SPAMFILTER del command */
			const char *parv[5];
			parv[0] = NULL;
			parv[1] = "spamfilter";
			parv[2] = me.name;
			parv[3] = "del";
			parv[4] = NULL;
			do_cmd(client, recv_mtags, "STATS", 4, parv);
			return;
		}
		spamfilter_del_by_id(client, parv[2]);
		return;
	}

	if ((parc == 7) && (*parv[2] != '-'))
	{
		spamfilter_new_usage(client,parv);
		return;
	}

	if ((parc < 8) || BadPtr(parv[7]))
	{
		spamfilter_usage(client);
		return;
	}

	/* parv[1]: [add|del|+|-]
	 * parv[2]: match-type
	 * parv[3]: type
	 * parv[4]: action
	 * parv[5]: tkl time
	 * parv[6]: tkl reason (or block reason..)
	 * parv[7]: regex
	 */
	if (!strcasecmp(parv[1], "add") || !strcmp(parv[1], "+"))
		add = 1;
	else if (!strcasecmp(parv[1], "del") || !strcmp(parv[1], "-") || !strcasecmp(parv[1], "remove"))
		add = 0;
	else
	{
		sendnotice(client, "1st parameter invalid");
		spamfilter_usage(client);
		return;
	}

	if (add && !strcasecmp(parv[2]+1, "posix"))
	{
		sendnotice(client, "ERROR: Spamfilter type 'posix' is DEPRECATED. You must use type 'regex' instead.");
		sendnotice(client, "See https://www.unrealircd.org/docs/FAQ#spamfilter-posix-deprecated");
		return;
	}

	match_type = unreal_match_method_strtoval(parv[2]+1);
	if (!match_type)
	{
		spamfilter_new_usage(client, parv);
		return;
	}

	targets = spamfilter_gettargets(parv[3], client);
	if (!targets)
	{
		spamfilter_usage(client);
		return;
	}

	strlcpy(targetbuf, spamfilter_target_inttostring(targets), sizeof(targetbuf));

	action = banact_stringtoval(parv[4]);
	if (!action)
	{
		sendnotice(client, "Invalid 'action' field (%s)", parv[4]);
		spamfilter_usage(client);
		return;
	}
	actionbuf[0] = banact_valtochar(action);
	actionbuf[1] = '\0';

	if (add)
	{
		/* now check the regex / match field... */
		m = unreal_create_match(match_type, parv[7], &err);
		if (!m)
		{
			sendnotice(client, "Error in regex '%s': %s", parv[7], err);
			return;
		}
		unreal_delete_match(m);
	}

	tkllayer[1] = add ? "+" : "-";
	tkllayer[3] = targetbuf;
	tkllayer[4] = actionbuf;
	tkllayer[5] = make_nick_user_host(client->name, client->user->username, GetHost(client));

	if (parv[5][0] == '-')
	{
		ircsnprintf(mo, sizeof(mo), "%lld", (long long)SPAMFILTER_BAN_TIME);
		tkllayer[8] = mo;
	}
	else
		tkllayer[8] = parv[5];

	if (parv[6][0] == '-')
		strlcpy(reason, unreal_encodespace(SPAMFILTER_BAN_REASON), sizeof(reason));
	else
		strlcpy(reason, parv[6], sizeof(reason));

	tkllayer[9] = reason;
	tkllayer[10] = parv[2]+1; /* +1 to skip the '-' */
	tkllayer[11] = parv[7];

	/* SPAMFILTER LENGTH CHECK.
	 * We try to limit it here so '/stats f' output shows ok, output of that is:
	 * :servername 229 destname F <target> <action> <num> <num> <num> <reason> <set_by> :<regex>
	 * : ^NICKLEN       ^ NICKLEN                                       ^check   ^check   ^check
	 * And for the other fields (and spacing/etc) we count on max 40 characters.
	 * We also do >500 instead of >510, since that looks cleaner ;).. so actually we count
	 * on 50 characters for the rest... -- Syzop
	 */
	n = strlen(reason) + strlen(parv[7]) + strlen(tkllayer[6]) + (NICKLEN * 2) + 40;
	if ((n > 500) && add)
	{
		sendnotice(client, "Sorry, spamfilter too long. You'll either have to trim down the "
		                 "reason or the regex (exceeded by %d bytes)", n - 500);
		return;
	}

	if (add)
	{
		ircsnprintf(mo2, sizeof(mo2), "%lld", (long long)TStime());
		tkllayer[7] = mo2;
	}

	cmd_tkl(&me, NULL, 12, tkllayer);
}

/** tkl hash method.
 * @param c   The tkl type character, see tkl_typetochar().
 * @note      The input value 'c' is assumed to be in range a-z or A-Z!
 *            Also, don't blindly change the hashmethod here, some things
 *            depend on 'z' and 'Z' ending up in the same bucket.
 */
int _tkl_hash(unsigned int c)
{
#ifdef DEBUGMODE
	if ((c >= 'a') && (c <= 'z'))
		return c-'a';
	else if ((c >= 'A') && (c <= 'Z'))
		return c-'A';
	else {
		unreal_log(ULOG_ERROR, "bug", "TKL_HASH_INVALID", NULL,
		           "tkl_hash() called with out of range parameter (c = '$tkl_char') !!!",
		           log_data_char("tkl_char", c));
		return 0;
	}
#else
	return (isupper(c) ? c-'A' : c-'a');
#endif
}

/** tkl type to tkl character.
 * NOTE: type is assumed to be valid.
 */
char _tkl_typetochar(int type)
{
	int i;
	for (i=0; tkl_types[i].config_name; i++)
		if ((tkl_types[i].type == type) && tkl_types[i].tkltype)
			return tkl_types[i].letter;
	unreal_log(ULOG_ERROR, "bug", "TKL_TYPETOCHAR_INVALID", NULL,
	           "tkl_typetochar(): unknown type $tkl_type!!!",
	           log_data_integer("tkl_type", type));
	return 0;
}

/** tkl character to tkl type
 * Returns 0 if invalid type.
 */
int _tkl_chartotype(char c)
{
	int i;
	for (i=0; tkl_types[i].config_name; i++)
		if ((tkl_types[i].letter == c) && tkl_types[i].tkltype)
			return tkl_types[i].type;
	return 0;
}

int tkl_banexception_chartotype(char c)
{
	int i;
	for (i=0; tkl_types[i].config_name; i++)
		if ((tkl_types[i].letter == c) && tkl_types[i].exceptiontype)
			return tkl_types[i].type;
	return 0;
}

char *tkl_banexception_configname_to_chars(char *name)
{
	static char buf[128];
	int i;

	if (!strcasecmp(name, "all"))
	{
		/* 'all' means everything except qline: */
		char *p = buf;
		for (i=0; tkl_types[i].config_name; i++)
		{
			if (tkl_types[i].exceptiontype && !(tkl_types[i].type & TKL_NAME))
				*p++ = tkl_types[i].letter;
		}
		*p = '\0';
		return buf;
	}

	for (i=0; tkl_types[i].config_name; i++)
	{
		if (!strcasecmp(name, tkl_types[i].config_name) && tkl_types[i].exceptiontype)
		{
			buf[0] = tkl_types[i].letter;
			buf[1] = '\0';
			return buf;
		}
	}
	return NULL;
}

/** Show TKL type as a string (used when adding/removing) */
char *_tkl_type_string(TKL *tkl)
{
	static char txt[256];
	int i;

	*txt = '\0';

	if (TKLIsServerBan(tkl) && (tkl->ptr.serverban->subtype == TKL_SUBTYPE_SOFT))
		strlcpy(txt, "Soft ", sizeof(txt));

	for (i=0; tkl_types[i].config_name; i++)
	{
		if ((tkl_types[i].type == tkl->type) && tkl_types[i].tkltype)
		{
			strlcat(txt, tkl_types[i].log_name, sizeof(txt));
			return txt;
		}
	}

	strlcpy(txt, "Unknown *-Line", sizeof(txt));
	return txt;
}

/** Short config string, lowercase alnum with possibly hyphens (eg: 'kline') */
char *_tkl_type_config_string(TKL *tkl)
{
	int i;

	for (i=0; tkl_types[i].config_name; i++)
		if ((tkl_types[i].type == tkl->type) && tkl_types[i].tkltype)
			return tkl_types[i].config_name;

	return "???";
}

int tkl_banexception_matches_type(TKL *except, int bantype)
{
	char *p;
	int extype;

	if (!TKLIsBanException(except))
		abort();

	for (p = except->ptr.banexception->bantypes; *p; p++)
	{
		extype = tkl_banexception_chartotype(*p);
		if ((extype & TKL_SPAMF) || (extype & TKL_SHUN) || (extype & TKL_NAME))
		{
			/* For spamfilter, shun and qline we don't care
			 * whether they are global or not. That would only
			 * be confusing to the admin.
			 */
			extype &= ~TKL_GLOBAL;
			if (bantype & extype)
				return 1;
		} else {
			/* Rest requires an exact match */
			if (bantype == extype)
				return 1;
		}
	}

	return 0;
}

/** Used for finding out which element of the tkl_ip hash table is used (primary element) */
int _tkl_ip_hash(char *ip)
{
	char ipbuf[64], *p;

	for (p = ip; *p; p++)
	{
		if ((*p == '?') || (*p == '*') || (*p == '/'))
			return -1; /* not an entry suitable for the ip hash table */
	}
	if (inet_pton(AF_INET, ip, &ipbuf) == 1)
	{
		/* IPv4 */
		unsigned int v = (ipbuf[0] << 24) +
		                 (ipbuf[1] << 16) +
		                 (ipbuf[2] << 8)  +
		                 ipbuf[3];
		return v % TKLIPHASHLEN2;
	} else
	if (inet_pton(AF_INET6, ip, &ipbuf) == 1)
	{
		/* IPv6 (only upper 64 bits) */
		unsigned int v1 = (ipbuf[0] << 24) +
		                 (ipbuf[1] << 16) +
		                 (ipbuf[2] << 8)  +
		                 ipbuf[3];
		unsigned int v2 = (ipbuf[4] << 24) +
		                 (ipbuf[5] << 16) +
		                 (ipbuf[6] << 8)  +
		                 ipbuf[7];
		return (v1 ^ v2) % TKLIPHASHLEN2;
	} else
	{
		return -1;
	}
}

// TODO: consider efunc
int tkl_ip_hash_tkl(TKL *tkl)
{
	if (TKLIsServerBan(tkl))
		return tkl_ip_hash(tkl->ptr.serverban->hostmask);
	if (TKLIsBanException(tkl))
		return tkl_ip_hash(tkl->ptr.banexception->hostmask);
	return -1;
}

/** Used for finding out which tkl_ip hash table needs to be used (secondary element).
 * NOTE: Returns -1 for types that are never on the TKL ip hash table, such as spamfilter.
 *       This can be used by the caller as a quick way to find out if the type is supported.
 */
int _tkl_ip_hash_type(int type)
{
	if ((type == 'Z') || (type == 'z'))
		return 0;
	else if (type == 'G')
		return 1;
	else if (type == 'k')
		return 2;
	else if ((type == 'e') || (type == 'E'))
		return 3;
	else
		return -1;
}

/* Find the appropriate list 'head' that we need to iterate.
 * This is simply a helper that is used at 3 places and I hate duplicate code.
 * NOTE: this function may return NULL.
 */
TKL *tkl_find_head(char type, char *hostmask, TKL *def)
{
	int index, index2;

	/* First, check ip hash table TKL's... */
	index = tkl_ip_hash_type(type);
	if (index >= 0)
	{
		index2 = tkl_ip_hash(hostmask);
		if (index2 >= 0)
		{
			/* iterate tklines_ip_hash[index][index2] */
			return tklines_ip_hash[index][index2];
		}
	}
	/* Fallback to the default */
	return def;
}

/** Add a spamfilter entry to the list.
 * @param type                TKL_SPAMF or TKL_SPAMF|TKL_GLOBAL.
 * @param target              The spamfilter target (SPAMF_*)
 * @param action              The spamfilter action (BAN_ACT_*)
 * @param match               The match (this struct may contain a regex for example)
 * @param set_by              Who (or what) set the ban
 * @param expire_at           When will the ban expire (0 for permanent)
 * @param set_at              When was the ban set
 * @param spamf_tkl_duration  When will the ban placed by spamfilter expire
 * @param spamf_tkl_reason    What is the reason for bans placed by spamfilter
 * @param flags               Any TKL_FLAG_* (TKL_FLAG_CONFIG, etc..)
 * @returns                   The TKL entry, or NULL in case of a problem,
 *                            such as a regex failing to compile, memory problem, ..
 */
TKL *_tkl_add_spamfilter(int type, unsigned short target, BanAction action, Match *match, char *set_by,
                             time_t expire_at, time_t set_at,
                             time_t tkl_duration, char *tkl_reason,
                             int flags)
{
	TKL *tkl;
	int index;

	if (!(type & TKL_SPAMF))
		abort();

	tkl = safe_alloc(sizeof(TKL));
	/* First the common fields */
	tkl->type = type;
	tkl->flags = flags;
	tkl->set_at = set_at;
	safe_strdup(tkl->set_by, set_by);
	tkl->expire_at = expire_at;
	/* Then the spamfilter fields */
	tkl->ptr.spamfilter = safe_alloc(sizeof(Spamfilter));
	tkl->ptr.spamfilter->target = target;
	tkl->ptr.spamfilter->action = action;
	tkl->ptr.spamfilter->match = match;
	safe_strdup(tkl->ptr.spamfilter->tkl_reason, tkl_reason);
	tkl->ptr.spamfilter->tkl_duration = tkl_duration;

	if (tkl->ptr.spamfilter->target & SPAMF_USER)
		loop.do_bancheck_spamf_user = 1;
	if (tkl->ptr.spamfilter->target & SPAMF_AWAY)
		loop.do_bancheck_spamf_away = 1;

	/* Spamfilters go via the normal TKL list... */
	index = tkl_hash(tkl_typetochar(type));
	AddListItem(tkl, tklines[index]);

	if (target & SPAMF_MTAG)
		mtag_spamfilters_present = 1;

	return tkl;
}

/** Add a server ban TKL entry.
 * @param type                The TKL type, one of TKL_*,
 *                            optionally OR'ed with TKL_GLOBAL.
 * @param usermask            The user mask
 * @param hostmask            The host mask
 * @param reason              The reason for the ban
 * @param set_by              Who (or what) set the ban
 * @param expire_at           When will the ban expire (0 for permanent)
 * @param set_at              When was the ban set
 * @param soft                Whether it's a soft-ban
 * @param flags               Any TKL_FLAG_* (TKL_FLAG_CONFIG, etc..)
 * @returns                   The TKL entry, or NULL in case of a problem,
 *                            such as a regex failing to compile, memory problem, ..
 * @note
 * Be sure not to call this function for spamfilters,
 * qlines or exempts, which have their own function!
 */
TKL *_tkl_add_serverban(int type, char *usermask, char *hostmask, char *reason, char *set_by,
                           time_t expire_at, time_t set_at, int soft, int flags)
{
	TKL *tkl;
	int index, index2;

	if (!TKLIsServerBanType(type))
		abort();

	tkl = safe_alloc(sizeof(TKL));
	/* First the common fields */
	tkl->type = type;
	tkl->flags = flags;
	tkl->set_at = set_at;
	safe_strdup(tkl->set_by, set_by);
	tkl->expire_at = expire_at;
	/* Now the server ban fields */
	tkl->ptr.serverban = safe_alloc(sizeof(ServerBan));
	safe_strdup(tkl->ptr.serverban->usermask, usermask);
	safe_strdup(tkl->ptr.serverban->hostmask, hostmask);
	if (soft)
		tkl->ptr.serverban->subtype = TKL_SUBTYPE_SOFT;
	safe_strdup(tkl->ptr.serverban->reason, reason);

	/* For ip hash table TKL's... */
	index = tkl_ip_hash_type(tkl_typetochar(type));
	if (index >= 0)
	{
		index2 = tkl_ip_hash_tkl(tkl);
		if (index2 >= 0)
		{
			AddListItem(tkl, tklines_ip_hash[index][index2]);
			return tkl;
		}
	}

	/* If we get here it's just for our normal list.. */
	index = tkl_hash(tkl_typetochar(type));
	AddListItem(tkl, tklines[index]);

	return tkl;
}

/** Add a ban exception TKL entry.
 * @param type                TKL_EXCEPTION or TKLEXCEPT|TKL_GLOBAL.
 * @param usermask            The user mask
 * @param hostmask            The host mask
 * @param reason              The reason for the ban
 * @param set_by              Who (or what) set the ban
 * @param expire_at           When will the ban expire (0 for permanent)
 * @param set_at              When was the ban set
 * @param soft                Whether it's a soft-ban
 * @param bantypes            The ban types to exempt from
 * @param flags               Any TKL_FLAG_* (TKL_FLAG_CONFIG, etc..)
 * @returns                   The TKL entry, or NULL in case of a problem,
 *                            such as a regex failing to compile, memory problem, ..
 * @note
 * Be sure not to call this function for spamfilters,
 * qlines or exempts, which have their own function!
 */
TKL *_tkl_add_banexception(int type, char *usermask, char *hostmask, char *reason, char *set_by,
                               time_t expire_at, time_t set_at, int soft, char *bantypes, int flags)
{
	TKL *tkl;
	int index, index2;

	if (!TKLIsBanExceptionType(type))
		abort();

	tkl = safe_alloc(sizeof(TKL));
	/* First the common fields */
	tkl->type = type;
	tkl->flags = flags;
	tkl->set_at = set_at;
	safe_strdup(tkl->set_by, set_by);
	tkl->expire_at = expire_at;
	/* Now the ban except fields */
	tkl->ptr.banexception = safe_alloc(sizeof(BanException));
	safe_strdup(tkl->ptr.banexception->usermask, usermask);
	safe_strdup(tkl->ptr.banexception->hostmask, hostmask);
	if (soft)
		tkl->ptr.banexception->subtype = TKL_SUBTYPE_SOFT;
	safe_strdup(tkl->ptr.banexception->bantypes, bantypes);
	safe_strdup(tkl->ptr.banexception->reason, reason);

	/* For ip hash table TKL's... */
	index = tkl_ip_hash_type(tkl_typetochar(type));
	if (index >= 0)
	{
		index2 = tkl_ip_hash_tkl(tkl);
		if (index2 >= 0)
		{
			AddListItem(tkl, tklines_ip_hash[index][index2]);
			return tkl;
		}
	}

	/* If we get here it's just for our normal list.. */
	index = tkl_hash(tkl_typetochar(type));
	AddListItem(tkl, tklines[index]);

	return tkl;
}

/** Add a name ban TKL entry (Q-Line), used for banning nicks and channels.
 * @param type                The TKL type, one of TKL_*,
 *                            optionally OR'ed with TKL_GLOBAL.
 * @param name                The nick or channel to be banned (wildcards accepted)
 * @param hold                Flag to indicate services hold
 * @param reason              The reason for the ban
 * @param set_by              Who (or what) set the ban
 * @param expire_at           When will the ban expire (0 for permanent)
 * @param set_at              When was the ban set
 * @param flags               Any TKL_FLAG_* (TKL_FLAG_CONFIG, etc..)
 * @returns                   The TKL entry, or NULL in case of a problem,
 *                            such as a regex failing to compile, memory problem, ..
 * @note
 * Be sure not to call this function for spamfilters,
 * qlines or exempts, which have their own function!
 */
TKL *_tkl_add_nameban(int type, char *name, int hold, char *reason, char *set_by,
                          time_t expire_at, time_t set_at, int flags)
{
	TKL *tkl;
	int index;

	if (!TKLIsNameBanType(type))
		abort();

	tkl = safe_alloc(sizeof(TKL));
	/* First the common fields */
	tkl->type = type;
	tkl->flags = flags;
	tkl->set_at = set_at;
	safe_strdup(tkl->set_by, set_by);
	tkl->expire_at = expire_at;
	/* Now the name ban fields */
	tkl->ptr.nameban = safe_alloc(sizeof(ServerBan));
	safe_strdup(tkl->ptr.nameban->name, name);
	tkl->ptr.nameban->hold = hold;
	safe_strdup(tkl->ptr.nameban->reason, reason);

	/* Name bans go via the normal TKL list.. */
	index = tkl_hash(tkl_typetochar(type));
	AddListItem(tkl, tklines[index]);

	return tkl;
}


/** Free a TKL entry but do not remove from the list.
 * (this assumes that it was not added yet or is already removed)
 * Most people will use tkl_del_line() instead.
 */
void _free_tkl(TKL *tkl)
{
	/* Free the entry */
	/* First, the common fields */
	safe_free(tkl->set_by);
	/* Now the type specific fields */
	if (TKLIsServerBan(tkl) && tkl->ptr.serverban)
	{
		safe_free(tkl->ptr.serverban->usermask);
		safe_free(tkl->ptr.serverban->hostmask);
		safe_free(tkl->ptr.serverban->reason);
		safe_free(tkl->ptr.serverban);
	} else
	if (TKLIsNameBan(tkl) && tkl->ptr.nameban)
	{
		safe_free(tkl->ptr.nameban->name);
		safe_free(tkl->ptr.nameban->reason);
		safe_free(tkl->ptr.nameban);
	} else
	if (TKLIsSpamfilter(tkl) && tkl->ptr.spamfilter)
	{
		/* Spamfilter */
		safe_free(tkl->ptr.spamfilter->tkl_reason);
		if (tkl->ptr.spamfilter->match)
			unreal_delete_match(tkl->ptr.spamfilter->match);
		safe_free(tkl->ptr.spamfilter);
	} else
	if (TKLIsBanException(tkl) && tkl->ptr.banexception)
	{
		safe_free(tkl->ptr.banexception->usermask);
		safe_free(tkl->ptr.banexception->hostmask);
		safe_free(tkl->ptr.banexception->bantypes);
		safe_free(tkl->ptr.banexception->reason);
		safe_free(tkl->ptr.banexception);
	}
	safe_free(tkl);
}

/** Delete a TKL entry from the list and free it.
 * @param tkl The TKL entry.
 */
void _tkl_del_line(TKL *tkl)
{
	int index, index2;
	int found = 0;

	/* Try to find it in the ip TKL hash table first
	 * (this only applies to server bans)
	 */
	index = tkl_ip_hash_type(tkl_typetochar(tkl->type));
	if (index >= 0)
	{
		index2 = tkl_ip_hash_tkl(tkl);
		if (index2 >= 0)
		{
#if 1
			/* Temporary validation until an rmtkl(?) bug is fixed */
			TKL *d;
			int really_found = 0;
			for (d = tklines_ip_hash[index][index2]; d; d = d->next)
				if (d == tkl)
				{
					really_found = 1;
					break;
				}
			if (!really_found)
			{
				unreal_log(ULOG_FATAL, "tkl", "BUG_TKL_DEL_LINE_HASH", NULL,
				           "[BUG] [Crash] tkl_del_line() for $tkl (type: $tkl.type_string): "
				           "NOT found in tklines_ip_hash. This should never happen!",
				           log_data_tkl("tkl", tkl));
				abort();
			}
#endif
			DelListItem(tkl, tklines_ip_hash[index][index2]);
			found = 1;
		}
	}

	if (!found)
	{
		/* If we get here it's just for our normal list.. */
		index = tkl_hash(tkl_typetochar(tkl->type));
		DelListItem(tkl, tklines[index]);
	}

	/* Finally, free the entry */
	free_tkl(tkl);
	check_mtag_spamfilters_present();
}

/** Add some default ban exceptions - for localhost */
static void add_default_exempts(void)
{
	/* The exempted ban types are only ones that will affect other connections as well,
	 * such as gline, and not policy decissions such as maxperip exempt or bypass qlines.
	 * Currently the list is: gline, kline, gzline, zline, shun, blacklist,
	 *                        connect-flood, handshake-data-flood.
	 */
	tkl_add_banexception(TKL_EXCEPTION, "*", "127.0.0.0/8", "localhost is always exempt",
	                     "-default-", 0, TStime(), 0, "GkZzsbcd", TKL_FLAG_CONFIG);
}

/*
 * tkl_check_local_remove_shun:
 * removes shun from currently connected users affected by tmp.
 */
// TODO / FIXME: audit this function, it looks crazy
void _tkl_check_local_remove_shun(TKL *tmp)
{
	long i;
	char *chost, *cname, *cip;
	int is_ip;
	Client *client;

	TKL *tk;
	int keep_shun;

	for (i = 0; i <= 5; i++)
	{
		list_for_each_entry(client, &lclient_list, lclient_node)
			if (MyUser(client) && IsShunned(client))
			{
				chost = client->local->sockhost;
				cname = client->user->username;

				cip = GetIP(client);

				if ((*tmp->ptr.serverban->hostmask >= '0') && (*tmp->ptr.serverban->hostmask <= '9'))
					is_ip = 1;
				else
					is_ip = 0;

				if (is_ip == 0 ?
				    (match_simple(tmp->ptr.serverban->hostmask, chost) && match_simple(tmp->ptr.serverban->usermask, cname)) :
				    (match_simple(tmp->ptr.serverban->hostmask, chost) || match_simple(tmp->ptr.serverban->hostmask, cip))
				    && match_simple(tmp->ptr.serverban->usermask, cname))
				{
					/*
					  before blindly marking this user as un-shunned, we need to check
					  if the user is under any other existing shuns. (#0003906)
					  Unfortunately, this requires crazy amounts of indentation ;-).

					  This enumeration code is based off of _tkl_stats()
					 */
					keep_shun = 0;
					for(tk = tklines[tkl_hash('s')]; tk && !keep_shun; tk = tk->next)
						if (tk != tmp && match_simple(tk->ptr.serverban->usermask, cname))
						{
							if ((*tk->ptr.serverban->hostmask >= '0') && (*tk->ptr.serverban->hostmask <= '9')
							    /* the hostmask is an IP */
							    && (match_simple(tk->ptr.serverban->hostmask, chost) || match_simple(tk->ptr.serverban->hostmask, cip)))
								keep_shun = 1;
							else
								/* the hostmask is not an IP */
								if (match_simple(tk->ptr.serverban->hostmask, chost) && match_simple(tk->ptr.serverban->usermask, cname))
									keep_shun = 1;
						}

					if (!keep_shun)
					{
						ClearShunned(client);
					}
				}
			}
	}
}


/** This returns something like user@host, or %user@host, or ~a:Trusted
 * that can be used in oper notices like expiring kline, added kline, etc.
 */
#define NO_SOFT_PREFIX	1
char *_tkl_uhost(TKL *tkl, char *buf, size_t buflen, int options)
{
	if (TKLIsServerBan(tkl))
	{
		if (is_extended_server_ban(tkl->ptr.serverban->usermask))
		{
			ircsnprintf(buf, buflen, "%s%s%s",
				(!(options & NO_SOFT_PREFIX) && (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT)) ? "%" : "",
				tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask);
		} else {
			ircsnprintf(buf, buflen, "%s%s@%s",
				(!(options & NO_SOFT_PREFIX) && (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT)) ? "%" : "",
				tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask);
		}
	} else
	if (TKLIsBanException(tkl))
	{
		if (is_extended_server_ban(tkl->ptr.banexception->usermask))
		{
			ircsnprintf(buf, buflen, "%s%s%s",
				(!(options & NO_SOFT_PREFIX) && (tkl->ptr.banexception->subtype & TKL_SUBTYPE_SOFT)) ? "%" : "",
				tkl->ptr.banexception->usermask, tkl->ptr.banexception->hostmask);
		} else {
			ircsnprintf(buf, buflen, "%s%s@%s",
				(!(options & NO_SOFT_PREFIX) && (tkl->ptr.banexception->subtype & TKL_SUBTYPE_SOFT)) ? "%" : "",
				tkl->ptr.banexception->usermask, tkl->ptr.banexception->hostmask);
		}
	} else
		abort();

	return buf;
}

/** Deal with expiration of a specific TKL entry.
 * This is a helper function for tkl_check_expire().
 */
void tkl_expire_entry(TKL *tkl)
{
	if (TKLIsServerBan(tkl))
	{
		unreal_log(ULOG_INFO, "tkl", "TKL_EXPIRE", NULL,
		           "Expiring $tkl.type_string '$tkl' [reason: $tkl.reason] [by: $tkl.set_by] [duration: $tkl.duration_string]",
		           log_data_tkl("tkl", tkl));
	}
	else if (TKLIsNameBan(tkl))
	{
		if (!tkl->ptr.nameban->hold)
		{
			unreal_log(ULOG_INFO, "tkl", "TKL_EXPIRE", NULL,
			           "Expiring $tkl.type_string '$tkl' [reason: $tkl.reason] [by: $tkl.set_by] [duration: $tkl.duration_string]",
				   log_data_tkl("tkl", tkl));
		}
	}
	else if (TKLIsBanException(tkl))
	{
		unreal_log(ULOG_INFO, "tkl", "TKL_EXPIRE", NULL,
			   "Expiring $tkl.type_string '$tkl' [type: $tkl.exception_types] [reason: $tkl.reason] [by: $tkl.set_by] [duration: $tkl.duration_string]",
			   log_data_tkl("tkl", tkl));
	}

	// FIXME: so.. this isn't logged? or what?
	if (tkl->type & TKL_SHUN)
		tkl_check_local_remove_shun(tkl);

	RunHook(HOOKTYPE_TKL_DEL, NULL, tkl);
	tkl_del_line(tkl);
}

/** Regularly check TKL entries for expiration */
EVENT(tkl_check_expire)
{
	TKL *tkl, *next;
	time_t nowtime;
	int index, index2;

	nowtime = TStime();

	/* First, hashed entries.. */
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = next)
			{
				next = tkl->next;
				if (tkl->expire_at <= nowtime && !(tkl->expire_at == 0))
				{
					tkl_expire_entry(tkl);
				}
			}
		}
	}

	/* Now normal entries.. */
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = next)
		{
			next = tkl->next;
			if (tkl->expire_at <= nowtime && !(tkl->expire_at == 0))
			{
				tkl_expire_entry(tkl);
			}
		}
	}
}

/* This is just a helper function for find_tkl_exception() */
static int find_tkl_exception_matcher(Client *client, int ban_type, TKL *except_tkl)
{
	char uhost[NICKLEN+HOSTLEN+1];

	if (!TKLIsBanException(except_tkl))
		return 0;

	if (!tkl_banexception_matches_type(except_tkl, ban_type))
		return 0;

	tkl_uhost(except_tkl, uhost, sizeof(uhost), NO_SOFT_PREFIX);

	if (match_user(uhost, client, MATCH_CHECK_REAL))
	{
		if (!(except_tkl->ptr.banexception->subtype & TKL_SUBTYPE_SOFT))
			return 1; /* hard ban exempt */
		if ((except_tkl->ptr.banexception->subtype & TKL_SUBTYPE_SOFT) && IsLoggedIn(client))
			return 1; /* soft ban exempt - only matches if user is logged in */
	}

	return 0; /* not found */
}

/** Search for TKL Exceptions for this user.
 * @param ban_type   The ban type to check, normally ban_tkl->type.
 * @param client     The user
 * @returns 1 if ban exempt, 0 if not.
 * @note
 * If you have a TKL ban that matched, say, 'ban_tkl'.
 * Then you call this function like this:
 * if (find_tkl_exception(ban_tkl->type, client))
 *     return 0; // User is exempt
 * [.. continue and ban the user..]
 */
int _find_tkl_exception(int ban_type, Client *client)
{
	TKL *tkl;
	int index, index2;
	Hook *hook;

	if (IsServer(client) || IsMe(client))
		return 1;

	/* First, the TKL ip hash table entries.. */
	index = tkl_ip_hash_type('e');
	index2 = tkl_ip_hash(GetIP(client));
	if (index2 >= 0)
	{
		for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
		{
			if (find_tkl_exception_matcher(client, ban_type, tkl))
				return 1; /* exempt */
		}
	}

	/* If not banned (yet), then check regular entries.. */
	for (tkl = tklines[tkl_hash('e')]; tkl; tkl = tkl->next)
	{
			if (find_tkl_exception_matcher(client, ban_type, tkl))
				return 1; /* exempt */
	}

	for (hook = Hooks[HOOKTYPE_TKL_EXCEPT]; hook; hook = hook->next)
	{
		if (hook->func.intfunc(client, ban_type) > 0)
			return 1; /* exempt by hook */
	}
	return 0; /* Not exempt */
}

/** Helper function for find_tkline_match() */
int find_tkline_match_matcher(Client *client, int skip_soft, TKL *tkl)
{
	char uhost[NICKLEN+HOSTLEN+1];

	if (!TKLIsServerBan(tkl) || (tkl->type & TKL_SHUN))
		return 0;

	if (skip_soft && (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT))
		return 0;

	tkl_uhost(tkl, uhost, sizeof(uhost), NO_SOFT_PREFIX);

	if (match_user(uhost, client, MATCH_CHECK_REAL))
	{
		/* If hard-ban, or soft-ban&unauthenticated.. */
		if (!(tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ||
		    ((tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) && !IsLoggedIn(client)))
		{
			/* Found match. Now check for exception... */
			if (find_tkl_exception(tkl->type, client))
				return 0; /* exempted */
			return 1; /* banned */
		}
	}

	return 0; /* no match */
}

/** Check if user matches a *LINE. If so, kill the user.
 * @retval 1 if client is banned, 0 if not
 * @note Do not continue processing if the client is killed (0 return value).
 * @note Return value changed with regards to UnrealIRCd 4!
 */
int _find_tkline_match(Client *client, int skip_soft)
{
	TKL *tkl;
	int banned = 0;
	int index, index2;

	if (IsServer(client) || IsMe(client))
		return 0;

	/* First, the TKL ip hash table entries.. */
	index2 = tkl_ip_hash(GetIP(client));
	if (index2 >= 0)
	{
		for (index = 0; index < TKLIPHASHLEN1; index++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				banned = find_tkline_match_matcher(client, skip_soft, tkl);
				if (banned)
					break;
			}
			if (banned)
				break;
		}
	}

	/* If not banned (yet), then check regular entries.. */
	if (!banned)
	{
		for (index = 0; index < TKLISTLEN; index++)
		{
			for (tkl = tklines[index]; tkl; tkl = tkl->next)
			{
				banned = find_tkline_match_matcher(client, skip_soft, tkl);
				if (banned)
					break;
			}
			if (banned)
				break;
		}
	}

	if (!banned)
		return 0;

	/* User is banned... */

	RunHookReturnInt(HOOKTYPE_FIND_TKLINE_MATCH, !=99, client, tkl);

	if (tkl->type & TKL_KILL)
	{
		ircstats.is_ref++;
		if (tkl->type & TKL_GLOBAL)
			banned_client(client, "G-Lined", tkl->ptr.serverban->reason, 1, 0);
		else
			banned_client(client, "K-Lined", tkl->ptr.serverban->reason, 0, 0);
		return 1; /* killed */
	} else
	if (tkl->type & TKL_ZAP)
	{
		ircstats.is_ref++;
		banned_client(client, "Z-Lined", tkl->ptr.serverban->reason, (tkl->type & TKL_GLOBAL)?1:0, 0);
		return 1; /* killed */
	}

	return 0;
}

/** Check if user is shunned.
 * @param client   Client to check.
 * @returns 1 if shunned, 0 if not.
 */
int _find_shun(Client *client)
{
	TKL *tkl;

	if (IsServer(client) || IsMe(client))
		return 0;

	if (IsShunned(client))
		return 1;

	if (ValidatePermissionsForPath("immune:server-ban:shun",client,NULL,NULL,NULL))
		return 0;

	for (tkl = tklines[tkl_hash('s')]; tkl; tkl = tkl->next)
	{
		char uhost[NICKLEN+HOSTLEN+1];

		if (!(tkl->type & TKL_SHUN))
			continue;

		tkl_uhost(tkl, uhost, sizeof(uhost), NO_SOFT_PREFIX);

		if (match_user(uhost, client, MATCH_CHECK_REAL))
		{
			/* If hard-ban, or soft-ban&unauthenticated.. */
			if (!(tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ||
			    ((tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) && !IsLoggedIn(client)))
			{
				/* Found match. Now check for exception... */
				if (find_tkl_exception(TKL_SHUN, client))
					return 0;
				SetShunned(client);
				return 1;
			}
		}
	}

	return 0;
}

/** Helper function for spamfilter_build_user_string().
 * This ensures IPv6 hosts are in brackets.
 */
char *SpamfilterMagicHost(char *i)
{
	static char buf[256];

	if (!strchr(i, ':'))
		return i;

	/* otherwise, it's IPv6.. prepend it with [ and append a ] */
	ircsnprintf(buf, sizeof(buf), "[%s]", i);
	return buf;
}

/** Build the nick:user@host:realname string
 * @param buf     The buffer used for storage, the size of
 *                which should be at least NICKLEN+USERLEN+HOSTLEN+1.
 * @param nick    The nickname (because client can be nick-changing).
 * @param client  The affected client.
 */
void _spamfilter_build_user_string(char *buf, char *nick, Client *client)
{
	snprintf(buf, NICKLEN+USERLEN+HOSTLEN+1, "%s!%s@%s:%s",
		nick, client->user->username, SpamfilterMagicHost(client->user->realhost), client->info);
}


/** Checks if the user matches a spamfilter of type 'u' (user,
 * nick!user@host:realname ban).
 * Written by: Syzop
 * Assumes: only call for clients, possible assume on local clients [?]
 * Return values: see match_spamfilter()
 */
int _find_spamfilter_user(Client *client, int flags)
{
	char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64]; /* n!u@h:r */

	if (ValidatePermissionsForPath("immune:server-ban:spamfilter",client,NULL,NULL,NULL))
		return 0;

	spamfilter_build_user_string(spamfilter_user, client->name, client);
	return match_spamfilter(client, spamfilter_user, SPAMF_USER, NULL, NULL, flags, NULL);
}

/** Check a spamfilter against all local users and print a message.
 * This is only used for the 'warn' action (BAN_ACT_WARN).
 */
int spamfilter_check_users(TKL *tkl)
{
	char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64]; /* n!u@h:r */
	char buf[1024];
	int matches = 0;
	Client *client;

	list_for_each_entry_reverse(client, &lclient_list, lclient_node)
	{
		if (MyUser(client))
		{
			spamfilter_build_user_string(spamfilter_user, client->name, client);
			if (!unreal_match(tkl->ptr.spamfilter->match, spamfilter_user))
				continue; /* No match */

			/* matched! */
			unreal_log(ULOG_INFO, "tkl", "SPAMFILTER_MATCH", client,
			           "[Spamfilter] $client.details matches filter '$tkl': [cmd: $command: '$str'] [reason: $tkl.reason] [action: $tkl.ban_action]",
				   log_data_tkl("tkl", tkl),
				   log_data_string("command", "USER"),
				   log_data_string("str", spamfilter_user));

			RunHook(HOOKTYPE_LOCAL_SPAMFILTER, client, spamfilter_user, spamfilter_user, SPAMF_USER, NULL, tkl);
			matches++;
		}
	}

	return matches;
}

/** Check if the nick or channel name is banned (Q-Line).
 * @param client   The possibly affected user.
 * @param name     The nick or channel to check.
 * @param is_hold  This will be SET (so OUT) if it's a services hold.
 *
 * @note Special handling:
 * #*ble* will match with #bbleh
 * *ble* will NOT match with #bbleh, will with bbleh
 */
TKL *_find_qline(Client *client, char *name, int *ishold)
{
	TKL *tkl;
	int	points = 0;
	*ishold = 0;

	if (IsServer(client) || IsMe(client))
		return NULL;

	for (tkl = tklines[tkl_hash('q')]; tkl; tkl = tkl->next)
	{
		points = 0;

		if (!TKLIsNameBan(tkl))
			continue;

		if (((*tkl->ptr.nameban->name == '#' && *name == '#') || (*tkl->ptr.nameban->name != '#' && *name != '#'))
		    && match_simple(tkl->ptr.nameban->name, name))
		{
			points = 1;
			break;
		}
	}

	if (points != 1)
		return NULL;

	/* It's a services hold (except bans don't override this) */
	if (tkl->ptr.nameban->hold)
	{
		*ishold = 1;
		return tkl;
	}

	if (find_tkl_exception(TKL_NAME, client))
		return NULL; /* exempt */

	return tkl;
}

/** Helper function for find_tkline_match_zap() */
TKL *find_tkline_match_zap_matcher(Client *client, TKL *tkl)
{
	if (!(tkl->type & TKL_ZAP))
		return NULL;

	if (match_user(tkl->ptr.serverban->hostmask, client, MATCH_CHECK_IP))
	{
		if (find_tkl_exception(TKL_ZAP, client))
			return NULL; /* exempt */
		return tkl; /* banned */
	}

	return NULL; /* no match */
}

/** Find matching (G)ZLINE, if any.
 * Note: function prototype changed as per UnrealIRCd 4.2.0.
 * @retval The (G)Z-Line that matched, or NULL if no such ban was found.
 */
TKL *_find_tkline_match_zap(Client *client)
{
	TKL *tkl, *ret;
	int index, index2;

	if (IsServer(client) || IsMe(client))
		return NULL;

	/* First, the TKL ip hash table entries.. */
	index = tkl_ip_hash_type('z');
	index2 = tkl_ip_hash(GetIP(client));
	if (index2 >= 0)
	{
		for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
		{
			ret = find_tkline_match_zap_matcher(client, tkl);
			if (ret)
				return ret;
		}
	}

	/* If not banned (yet), then check regular entries.. */
	for (tkl = tklines[tkl_hash('z')]; tkl; tkl = tkl->next)
	{
		ret = find_tkline_match_zap_matcher(client, tkl);
		if (ret)
			return ret;
	}

	return NULL;
}

#define BY_MASK 0x1
#define BY_REASON 0x2
#define NOT_BY_MASK 0x4
#define NOT_BY_REASON 0x8
#define BY_SETBY 0x10
#define NOT_BY_SETBY 0x20

typedef struct {
	int flags;
	const char *mask;
	const char *reason;
	const char *set_by;
} TKLFlag;

/** Parse STATS tkl parameters.
 * TODO: I don't think this is documented anywhere? Or underdocumented at least.
 */
static void parse_stats_params(const char *para, TKLFlag *flag)
{
	static char paratmp[512]; /* <- copy of para, because it gets fragged by strtok() */
	char *flags, *tmp;
	char what = '+';

	memset(flag, 0, sizeof(TKLFlag));
	strlcpy(paratmp, para, sizeof(paratmp));
	flags = strtok(paratmp, " ");
	if (!flags)
		return;

	for (; *flags; flags++)
	{
		switch (*flags)
		{
			case '+':
				what = '+';
				break;
			case '-':
				what = '-';
				break;
			case 'm':
				if (flag->mask || !(tmp = strtok(NULL, " ")))
					continue;
				if (what == '+')
					flag->flags |= BY_MASK;
				else
					flag->flags |= NOT_BY_MASK;
				flag->mask = tmp;
				break;
			case 'r':
				if (flag->reason || !(tmp = strtok(NULL, " ")))
					continue;
				if (what == '+')
					flag->flags |= BY_REASON;
				else
					flag->flags |= NOT_BY_REASON;
				flag->reason = tmp;
				break;
			case 's':
				if (flag->set_by || !(tmp = strtok(NULL, " ")))
					continue;
				if (what == '+')
					flag->flags |= BY_SETBY;
				else
					flag->flags |= NOT_BY_SETBY;
				flag->set_by = tmp;
				break;
		}
	}
}

/** Does this TKL entry match the search terms?
 * This is a helper function for tkl_stats().
 */
int tkl_stats_matcher(Client *client, int type, const char *para, TKLFlag *tklflags, TKL *tkl)
{
	/***** First, handle the selection ******/

	if (!BadPtr(para))
	{
		if (tklflags->flags & BY_SETBY)
			if (!match_simple(tklflags->set_by, tkl->set_by))
				return 0;
		if (tklflags->flags & NOT_BY_SETBY)
			if (match_simple(tklflags->set_by, tkl->set_by))
				return 0;
		if (TKLIsServerBan(tkl))
		{
			if (tklflags->flags & BY_MASK)
			{
				if (!match_simple(tklflags->mask, make_user_host(tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask)))
					return 0;
			}
			if (tklflags->flags & NOT_BY_MASK)
			{
				if (match_simple(tklflags->mask, make_user_host(tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask)))
					return 0;
			}
			if (tklflags->flags & BY_REASON)
				if (!match_simple(tklflags->reason, tkl->ptr.serverban->reason))
					return 0;
			if (tklflags->flags & NOT_BY_REASON)
				if (match_simple(tklflags->reason, tkl->ptr.serverban->reason))
					return 0;
		} else
		if (TKLIsNameBan(tkl))
		{
			if (tklflags->flags & BY_MASK)
			{
				if (!match_simple(tklflags->mask, tkl->ptr.nameban->name))
					return 0;
			}
			if (tklflags->flags & NOT_BY_MASK)
			{
				if (match_simple(tklflags->mask, tkl->ptr.nameban->name))
					return 0;
			}
			if (tklflags->flags & BY_REASON)
				if (!match_simple(tklflags->reason, tkl->ptr.nameban->reason))
					return 0;
			if (tklflags->flags & NOT_BY_REASON)
				if (match_simple(tklflags->reason, tkl->ptr.nameban->reason))
					return 0;
		} else
		if (TKLIsBanException(tkl))
		{
			if (tklflags->flags & BY_MASK)
			{
				if (!match_simple(tklflags->mask, make_user_host(tkl->ptr.banexception->usermask, tkl->ptr.banexception->hostmask)))
					return 0;
			}
			if (tklflags->flags & NOT_BY_MASK)
			{
				if (match_simple(tklflags->mask, make_user_host(tkl->ptr.banexception->usermask, tkl->ptr.banexception->hostmask)))
					return 0;
			}
			if (tklflags->flags & BY_REASON)
				if (!match_simple(tklflags->reason, tkl->ptr.banexception->reason))
					return 0;
			if (tklflags->flags & NOT_BY_REASON)
				if (match_simple(tklflags->reason, tkl->ptr.banexception->reason))
					return 0;
		}
	}

	/***** If we are still here then we have a match and will will send the STATS entry */
	if (TKLIsServerBan(tkl))
	{
		char uhostbuf[BUFSIZE];
		char *uhost = tkl_uhost(tkl, uhostbuf, sizeof(uhostbuf), 0);
		if (tkl->type == (TKL_KILL | TKL_GLOBAL))
		{
			sendnumeric(client, RPL_STATSGLINE, 'G', uhost,
				   (tkl->expire_at != 0) ? (long long)(tkl->expire_at - TStime()) : 0,
				   (long long)(TStime() - tkl->set_at), tkl->set_by, tkl->ptr.serverban->reason);
		} else
		if (tkl->type == (TKL_ZAP | TKL_GLOBAL))
		{
			sendnumeric(client, RPL_STATSGLINE, 'Z', uhost,
				   (tkl->expire_at != 0) ? (long long)(tkl->expire_at - TStime()) : 0,
				   (long long)(TStime() - tkl->set_at), tkl->set_by, tkl->ptr.serverban->reason);
		} else
		if (tkl->type == (TKL_SHUN | TKL_GLOBAL))
		{
			sendnumeric(client, RPL_STATSGLINE, 's', uhost,
				   (tkl->expire_at != 0) ? (long long)(tkl->expire_at - TStime()) : 0,
				   (long long)(TStime() - tkl->set_at), tkl->set_by, tkl->ptr.serverban->reason);
		} else
		if (tkl->type == (TKL_KILL))
		{
			sendnumeric(client, RPL_STATSGLINE, 'K', uhost,
				   (tkl->expire_at != 0) ? (long long)(tkl->expire_at - TStime()) : 0,
				   (long long)(TStime() - tkl->set_at), tkl->set_by, tkl->ptr.serverban->reason);
		} else
		if (tkl->type == (TKL_ZAP))
		{
			sendnumeric(client, RPL_STATSGLINE, 'z', uhost,
				   (tkl->expire_at != 0) ? (long long)(tkl->expire_at - TStime()) : 0,
				   (long long)(TStime() - tkl->set_at), tkl->set_by, tkl->ptr.serverban->reason);
		}
	} else
	if (TKLIsSpamfilter(tkl))
	{
		sendnumeric(client, RPL_STATSSPAMF,
			(tkl->type & TKL_GLOBAL) ? 'F' : 'f',
			unreal_match_method_valtostr(tkl->ptr.spamfilter->match->type),
			spamfilter_target_inttostring(tkl->ptr.spamfilter->target),
			banact_valtostring(tkl->ptr.spamfilter->action),
			(tkl->expire_at != 0) ? (long long)(tkl->expire_at - TStime()) : 0,
			(long long)(TStime() - tkl->set_at),
			(long long)tkl->ptr.spamfilter->tkl_duration,
			tkl->ptr.spamfilter->tkl_reason,
			tkl->set_by,
			tkl->ptr.spamfilter->match->str);
		if (para && !strcasecmp(para, "del"))
		{
			char *hash = spamfilter_id(tkl);
			if (tkl->type & TKL_GLOBAL)
			{
				sendtxtnumeric(client, "To delete this spamfilter, use /SPAMFILTER del %s", hash);
				sendtxtnumeric(client, "-");
			} else {
				sendtxtnumeric(client, "This spamfilter is stored in the configuration file and cannot be removed with /SPAMFILTER del");
				sendtxtnumeric(client, "-");
			}
		}
	} else
	if (TKLIsNameBan(tkl))
	{
		sendnumeric(client, RPL_STATSQLINE,
		            (tkl->type & TKL_GLOBAL) ? 'Q' : 'q',
		            tkl->ptr.nameban->name,
		            (tkl->expire_at != 0) ? (long long)(tkl->expire_at - TStime()) : 0,
		            (long long)(TStime() - tkl->set_at),
		            tkl->set_by,
		            tkl->ptr.nameban->reason);
	} else
	if (TKLIsBanException(tkl))
	{
		char uhostbuf[BUFSIZE];
		char *uhost = tkl_uhost(tkl, uhostbuf, sizeof(uhostbuf), 0);
		sendnumeric(client, RPL_STATSEXCEPTTKL, uhost,
			   tkl->ptr.banexception->bantypes,
			   (tkl->expire_at != 0) ? (long long)(tkl->expire_at - TStime()) : 0,
			   (long long)(TStime() - tkl->set_at), tkl->set_by, tkl->ptr.banexception->reason);
	} else
	{
		/* That's weird, unknown TKL type */
		return 0;
	}
	return 1;
}

/* TKL Stats. This is used by /STATS gline and all the others */
void _tkl_stats(Client *client, int type, const char *para, int *cnt)
{
	TKL *tk;
	TKLFlag tklflags;
	int index, index2;

	if ((max_stats_matches > 0) && (*cnt >= max_stats_matches))
		return;

	if (!BadPtr(para))
		parse_stats_params(para, &tklflags);

	/* First the IP hashed entries (if applicable).. */
	index = tkl_ip_hash_type(tkl_typetochar(type));
	if (index >= 0)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tk = tklines_ip_hash[index][index2]; tk; tk = tk->next)
			{
				if (type && tk->type != type)
					continue;
				if (tkl_stats_matcher(client, type, para, &tklflags, tk))
				{
					*cnt += 1;
					if ((max_stats_matches > 0) && (*cnt >= max_stats_matches))
					{
						sendnumeric(client, ERR_TOOMANYMATCHES, "STATS", "too many matches (set::max-stats-matches)");
						sendnotice(client, "Consider searching on something more specific, eg '/STATS gline +m *.nl'. See '/STATS' (without parameters) for help.");
						return;
					}
				}
			}
		}
	}

	/* Then the normal entries... */
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tk = tklines[index]; tk; tk = tk->next)
		{
			if (type && tk->type != type)
				continue;
			if (tkl_stats_matcher(client, type, para, &tklflags, tk))
			{
				*cnt += 1;
				if ((max_stats_matches > 0) && (*cnt >= max_stats_matches))
				{
					sendnumeric(client, ERR_TOOMANYMATCHES, "STATS", "too many matches (set::max-stats-matches)");
					sendnotice(client, "Consider searching on something more specific, eg '/STATS gline +m *.nl'. See '/STATS' (without parameters) for help.");
					return;
				}
			}
		}
	}

	if ((type == (TKL_SPAMF|TKL_GLOBAL)) && (!para || strcasecmp(para, "del")))
	{
		/* If requesting spamfilter stats and not spamfilter del, then suggest it. */
		sendnotice(client, "Tip: if you are looking for an easy way to remove a spamfilter, run '/SPAMFILTER del'.");
	}
}

/** Synchronize a TKL entry with the other server.
 * @param sender  The sender (eg: &me).
 * @param to      The remote server.
 * @param tkl     The TKL entry.
 */
void tkl_sync_send_entry(int add, Client *sender, Client *to, TKL *tkl)
{
	char typ;

	if (!(tkl->type & TKL_GLOBAL))
		return; /* nothing to sync */

	typ = tkl_typetochar(tkl->type);

	if (TKLIsServerBan(tkl))
	{
		sendto_one(to, NULL, ":%s TKL %c %c %s%s %s %s %lld %lld :%s", sender->name,
			   add ? '+' : '-',
			   typ,
			   (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   *tkl->ptr.serverban->usermask ? tkl->ptr.serverban->usermask : "*",
			   tkl->ptr.serverban->hostmask, tkl->set_by,
			   (long long)tkl->expire_at, (long long)tkl->set_at,
			   tkl->ptr.serverban->reason);
	} else
	if (TKLIsNameBan(tkl))
	{
		sendto_one(to, NULL, ":%s TKL %c %c %c %s %s %lld %lld :%s", sender->name,
			   add ? '+' : '-',
			   typ,
			   tkl->ptr.nameban->hold ? 'H' : '*',
			   tkl->ptr.nameban->name,
			   tkl->set_by,
			   (long long)tkl->expire_at, (long long)tkl->set_at,
			   tkl->ptr.nameban->reason);
	} else
	if (TKLIsSpamfilter(tkl))
	{
		sendto_one(to, NULL, ":%s TKL %c %c %s %c %s %lld %lld %lld %s %s :%s", sender->name,
			   add ? '+' : '-',
			   typ,
			   spamfilter_target_inttostring(tkl->ptr.spamfilter->target),
			   banact_valtochar(tkl->ptr.spamfilter->action),
			   tkl->set_by,
			   (long long)tkl->expire_at, (long long)tkl->set_at,
			   (long long)tkl->ptr.spamfilter->tkl_duration, tkl->ptr.spamfilter->tkl_reason,
			   unreal_match_method_valtostr(tkl->ptr.spamfilter->match->type),
			   tkl->ptr.spamfilter->match->str);
	} else
	if (TKLIsBanException(tkl))
	{
		sendto_one(to, NULL, ":%s TKL %c %c %s%s %s %s %lld %lld %s :%s", sender->name,
			   add ? '+' : '-',
			   typ,
			   (tkl->ptr.banexception->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   *tkl->ptr.banexception->usermask ? tkl->ptr.banexception->usermask : "*",
			   tkl->ptr.banexception->hostmask, tkl->set_by,
			   (long long)tkl->expire_at, (long long)tkl->set_at,
			   tkl->ptr.banexception->bantypes,
			   tkl->ptr.banexception->reason);
	} else
	{
		unreal_log(ULOG_FATAL, "tkl", "BUG_TKL_SYNC_SEND_ENTRY", NULL,
			   "[BUG] tkl_sync_send_entry() called for '%s' but unknown type: $tkl.type_string ($tkl_type_int)",
			   log_data_tkl("tkl", tkl),
			   log_data_integer("tkl_type_int", typ));
		abort();
	}
}

/** Broadcast a TKL entry.
 * @param sender  The sender, eg &me
 * @param skip    The client to skip, eg 'client' or NULL.
 * @param tkl     The TKL entry to synchronize with the other servers.
 */
void tkl_broadcast_entry(int add, Client *sender, Client *skip, TKL *tkl)
{
	Client *acptr;

	list_for_each_entry(acptr, &server_list, special_node)
	{
		if (skip && acptr == skip->direction)
			continue;

		tkl_sync_send_entry(add, sender, acptr, tkl);
	}
}

/** Synchronize all TKL entries with this server.
 * @param client The server to synchronize with.
 */
void _tkl_sync(Client *client)
{
	TKL *tkl;
	int index, index2;

	/* First, hashed entries.. */
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				tkl_sync_send_entry(1, &me, client, tkl);
			}
		}
	}

	/* Then, regular entries.. */
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			tkl_sync_send_entry(1, &me, client, tkl);
		}
	}
}

/** Find a server ban TKL - only used to prevent duplicates and for deletion */
TKL *_find_tkl_serverban(int type, char *usermask, char *hostmask, int softban)
{
	char tpe = tkl_typetochar(type);
	TKL *head, *tkl;

	if (!TKLIsServerBanType(type))
		abort();

	head = tkl_find_head(tpe, hostmask, tklines[tkl_hash(tpe)]);
	for (tkl = head; tkl; tkl = tkl->next)
	{
		if (tkl->type == type)
		{
			if (!strcasecmp(tkl->ptr.serverban->hostmask, hostmask) &&
			    !strcasecmp(tkl->ptr.serverban->usermask, usermask))
			{
				/* And an extra check for soft/hard ban mismatches.. */
				if ((tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) == softban)
					return tkl;
			}
		}
	}
	return NULL; /* Not found */
}

/** Find a ban exception TKL - only used to prevent duplicates and for deletion */
TKL *_find_tkl_banexception(int type, char *usermask, char *hostmask, int softban)
{
	char tpe = tkl_typetochar(type);
	TKL *head, *tkl;

	if (!TKLIsBanExceptionType(type))
		abort();

	head = tkl_find_head(tpe, hostmask, tklines[tkl_hash(tpe)]);
	for (tkl = head; tkl; tkl = tkl->next)
	{
		if (tkl->type == type)
		{
			if (!strcasecmp(tkl->ptr.banexception->hostmask, hostmask) &&
			    !strcasecmp(tkl->ptr.banexception->usermask, usermask))
			{
				/* And an extra check for soft/hard ban mismatches.. */
				if ((tkl->ptr.banexception->subtype & TKL_SUBTYPE_SOFT) == softban)
					return tkl;
			}
		}
	}
	return NULL; /* Not found */
}

/** Find a name ban TKL (qline) - only used to prevent duplicates and for deletion */
TKL *_find_tkl_nameban(int type, char *name, int hold)
{
	char tpe = tkl_typetochar(type);
	TKL *tkl;

	if (!TKLIsNameBanType(type))
		abort();

	for (tkl = tklines[tkl_hash(tpe)]; tkl; tkl = tkl->next)
	{
		if ((tkl->type == type) && !strcasecmp(tkl->ptr.nameban->name, name))
			return tkl;
	}
	return NULL; /* Not found */
}

/** Find a spamfilter TKL - only used to prevent duplicates and for deletion */
TKL *_find_tkl_spamfilter(int type, char *match_string, BanAction action, unsigned short target)
{
	char tpe = tkl_typetochar(type);
	TKL *tkl;

	if (!TKLIsSpamfilterType(type))
		abort();

	for (tkl = tklines[tkl_hash(tpe)]; tkl; tkl = tkl->next)
	{
		if ((type == tkl->type) &&
		    !strcmp(match_string, tkl->ptr.spamfilter->match->str) &&
		    (action == tkl->ptr.spamfilter->action) &&
		    (target == tkl->ptr.spamfilter->target))
		{
			return tkl;
		}
	}
	return NULL; /* Not found */
}

/** Send a notice to opers about the TKL that is being added */
void _sendnotice_tkl_add(TKL *tkl)
{
	/* Don't show notices for temporary nick holds (issued by services) */
	if (TKLIsNameBan(tkl) && tkl->ptr.nameban->hold)
		return;

	if (TKLIsServerBan(tkl))
	{
		unreal_log(ULOG_INFO, "tkl", "TKL_ADD", NULL,
			   "$tkl.type_string added: '$tkl' [reason: $tkl.reason] [by: $tkl.set_by] [duration: $tkl.duration_string]",
			   log_data_tkl("tkl", tkl));
	} else
	if (TKLIsNameBan(tkl))
	{
		unreal_log(ULOG_INFO, "tkl", "TKL_ADD", NULL,
			   "$tkl.type_string added: '$tkl' [reason: $tkl.reason] [by: $tkl.set_by] [duration: $tkl.duration_string]",
			   log_data_tkl("tkl", tkl));
	} else
	if (TKLIsSpamfilter(tkl))
	{
		unreal_log(ULOG_INFO, "tkl", "TKL_ADD", NULL,
			   "Spamfilter added: '$tkl' [type: $tkl.match_type] [targets: $tkl.spamfilter_targets] "
			   "[action: $tkl.ban_action] [reason: $tkl.reason] [by: $tkl.set_by]",
			   log_data_tkl("tkl", tkl));
	} else
	if (TKLIsBanException(tkl))
	{
		unreal_log(ULOG_INFO, "tkl", "TKL_ADD", NULL,
			   "$tkl.type_string added: '$tkl' [types: $tkl.exception_types] [by: $tkl.set_by] [duration: $tkl.duration_string]",
			   log_data_tkl("tkl", tkl));
	} else
	{
		unreal_log(ULOG_ERROR, "tkl", "BUG_UNKNOWN_TKL", NULL,
		           "[BUG] TKL added of unknown type, unhandled in sendnotice_tkl_add()!!!!");
	}
}

/** Send a notice to opers about the TKL that is being deleted */
void _sendnotice_tkl_del(char *removed_by, TKL *tkl)
{
	/* Don't show notices for temporary nick holds (issued by services) */
	if (TKLIsNameBan(tkl) && tkl->ptr.nameban->hold)
		return;

	if (TKLIsServerBan(tkl))
	{
		unreal_log(ULOG_INFO, "tkl", "TKL_DEL", NULL,
			   "$tkl.type_string removed: '$tkl' [reason: $tkl.reason] [by: $removed_by] [set at: $tkl.set_at_string]",
			   log_data_tkl("tkl", tkl),
			   log_data_string("removed_by", removed_by));
	} else
	if (TKLIsNameBan(tkl))
	{
		unreal_log(ULOG_INFO, "tkl", "TKL_DEL", NULL,
			   "$tkl.type_string removed: '$tkl' [reason: $tkl.reason] [by: $removed_by] [set at: $tkl.set_at_string]",
			   log_data_tkl("tkl", tkl),
			   log_data_string("removed_by", removed_by));
	} else
	if (TKLIsSpamfilter(tkl))
	{
		unreal_log(ULOG_INFO, "tkl", "TKL_DEL", NULL,
			   "Spamfilter removed: '$tkl' [type: $tkl.match_type] [targets: $tkl.spamfilter_targets] "
			   "[action: $tkl.ban_action] [reason: $tkl.reason] [by: $removed_by] [set at: $tkl.set_at_string]",
			   log_data_tkl("tkl", tkl),
			   log_data_string("removed_by", removed_by));
	} else
	if (TKLIsBanException(tkl))
	{
		unreal_log(ULOG_INFO, "tkl", "TKL_DEL", NULL,
			   "$tkl.type_string removed: '$tkl' [types: $tkl.exception_types] [by: $removed_by] [set at: $tkl.set_at_string]",
			   log_data_tkl("tkl", tkl),
			   log_data_string("removed_by", removed_by));
	} else
	{
		unreal_log(ULOG_ERROR, "tkl", "BUG_UNKNOWN_TKL", NULL,
		           "[BUG] TKL removed of unknown type, unhandled in sendnotice_tkl_del()!!!!");
	}
}

/** Add a TKL using the TKL layer. See cmd_tkl for parv[] and protocol documentation. */
CMD_FUNC(cmd_tkl_add)
{
	TKL *tkl;
	int type;
	time_t expire_at, set_at;
	const char *set_by;
	char tkl_entry_exists = 0;

	/* we rely on servers to be failsafe.. */
	if (!IsServer(client) && !IsMe(client))
		return;

	if (parc < 9)
		return;

	type = tkl_chartotype(parv[2][0]);
	if (!type)
		return;

	/* All TKL types have the following fields in common when adding:
	 * parv[5]: set_by
	 * parv[6]: expire_at
	 * parv[7]: set_at
	 * ... so we validate them here at the beginning.
	 */

	set_by = parv[5];
	expire_at = atol(parv[6]);
	set_at = atol(parv[7]);

	/* Validate set and expiry time */
	if ((set_at < 0) || !short_date(set_at, NULL))
	{
		unreal_log(ULOG_WARNING, "tkl", "TKL_ADD_INVALID", client,
			"Invalid TKL entry from $client: "
			"The set-at time is out of range ($set_at). Clock on other server incorrect or bogus entry.",
			log_data_integer("set_at", set_at));
		return;
	}
	if ((expire_at < 0) || !short_date(expire_at, NULL))
	{
		unreal_log(ULOG_WARNING, "tkl", "TKL_ADD_INVALID", client,
			"Invalid TKL entry from $client: "
			"The expire-at time is out of range ($expire_at). Clock on other server incorrect or bogus entry.",
			log_data_integer("expire_at", expire_at));
		return;
	}

	/* Now comes type-specific validation
	 * and we check if the TKL entry already exists and needs updating too.
	 */

	if (TKLIsServerBanType(type))
	{
		/* Validate server ban TKL fields */
		int softban = 0;
		const char *usermask = parv[3];
		const char *hostmask = parv[4];
		const char *reason = parv[8];

		/* Some simple validation on usermask and hostmask:
		 * may not contain an @. Yeah, some services or self-written
		 * linked servers are known to have sent this in the past.
		 */
		if (strchr(usermask, '@') || strchr(hostmask, '@'))
		{
			unreal_log(ULOG_WARNING, "tkl", "TKL_ADD_INVALID", client,
				"Invalid TKL entry from $client: "
				"Invalid user@host $usermask@$hostmask",
				log_data_string("usermask", usermask),
				log_data_string("hostmask", hostmask));
			return;
		}

		/* In case of a soft ban, strip the percent sign early,
		 * so parv[3] (username) is really the username without any prefix.
		 * Set the 'softban' flag if this is the case.
		 */
		if (*usermask == '%')
		{
			usermask++;
			softban = 1;
		}

		tkl = find_tkl_serverban(type, usermask, hostmask, softban);
		if (tkl)
		{
			tkl_entry_exists = 1;
		} else {
			tkl = tkl_add_serverban(type, usermask, hostmask, reason,
			                        set_by, expire_at, set_at, softban, 0);
		}
	} else
	if (TKLIsBanExceptionType(type))
	{
		/* Validate ban exception TKL fields */
		int softban = 0;
		const char *usermask = parv[3];
		const char *hostmask = parv[4];
		const char *bantypes = parv[8];
		const char *reason;

		if (parc < 10)
			return;

		reason = parv[9];

		/* Some simple validation on usermask and hostmask:
		 * may not contain an @. Yeah, some services or self-written
		 * linked servers are known to have sent this in the past.
		 */
		if (strchr(usermask, '@') || strchr(hostmask, '@'))
		{
			unreal_log(ULOG_WARNING, "tkl", "TKL_ADD_INVALID", client,
				"Invalid TKL entry from $client: "
				"Invalid TKL except user@host $usermask@$hostmask",
				log_data_string("usermask", usermask),
				log_data_string("hostmask", hostmask));
			return;
		}

		/* In case of a soft ban, strip the percent sign early,
		 * so parv[3] (username) is really the username without any prefix.
		 * Set the 'softban' flag if this is the case.
		 */
		if (*usermask == '%')
		{
			usermask++;
			softban = 1;
		}

		/* At this moment we do not validate 'bantypes' since a missing
		 * or wrong type does not cause harm anyway.
		 */
		tkl = find_tkl_banexception(type, usermask, hostmask, softban);
		if (tkl)
		{
			tkl_entry_exists = 1;
		} else {
			tkl = tkl_add_banexception(type, usermask, hostmask, reason,
			                           set_by, expire_at, set_at, softban, bantypes, 0);
		}
	} else
	if (TKLIsNameBanType(type))
	{
		/* Validate name ban TKL fields */
		int hold = 0;
		const char *name = parv[4];
		const char *reason = parv[8];

		if (*parv[3] == 'H')
			hold = 1;

		tkl = find_tkl_nameban(type, name, hold);
		if (tkl)
		{
			tkl_entry_exists = 1;
		} else {
			tkl = tkl_add_nameban(type, name, hold, reason, set_by, expire_at,
			                      set_at, 0);
		}
	} else
	if (TKLIsSpamfilterType(type))
	{
		/* Validate spamfilter-specific TKL fields */
		MatchType match_method;
		const char *match_string;
		Match *m; /* compiled match_string */
		time_t tkl_duration;
		const char *tkl_reason;
		BanAction action;
		unsigned short target;
		/* helper variables */
		char *err;

		if (parc < 12)
		{
			unreal_log(ULOG_WARNING, "tkl", "TKL_ADD_INVALID", client,
				"Invalid TKL entry from $client: "
				"Spamfilter with too few parameters. Running very old UnrealIRCd protocol (3.2.X?)");
			return;
		}

		match_string = parv[11];

		match_method = unreal_match_method_strtoval(parv[10]);
		if (match_method == 0)
		{
			unreal_log(ULOG_WARNING, "tkl", "TKL_ADD_INVALID", client,
				"Invalid TKL entry from $client: "
				"Spamfilter '$spamfilter_string' has unkown match-type '$spamfilter_type'",
				log_data_string("spamfilter_string", match_string),
				log_data_string("spamfilter_type", parv[10]));
			return;
		}

		if (!(target = spamfilter_gettargets(parv[3], NULL)))
		{
			unreal_log(ULOG_WARNING, "tkl", "TKL_ADD_INVALID", client,
				"Invalid TKL entry from $client: "
				"Spamfilter '$spamfilter_string' has unkown targets '$spamfilter_targets'",
				log_data_string("spamfilter_string", match_string),
				log_data_string("spamfilter_targets", parv[3]));
			return;
		}

		if (!(action = banact_chartoval(*parv[4])))
		{
			unreal_log(ULOG_WARNING, "tkl", "TKL_ADD_INVALID", client,
				"Invalid TKL entry from $client: "
				"Spamfilter '$spamfilter_string' has unkown action '$spamfilter_action'",
				log_data_string("spamfilter_string", match_string),
				log_data_string("spamfilter_action", parv[4]));
			return;
		}

		tkl_duration = config_checkval(parv[8], CFG_TIME);
		tkl_reason = parv[9];

		tkl = find_tkl_spamfilter(type, match_string, action, target);

		if (tkl)
		{
			tkl_entry_exists = 1;
		} else {
			m = unreal_create_match(match_method, match_string, &err);
			if (!m)
			{
				unreal_log(ULOG_WARNING, "tkl", "TKL_ADD_INVALID", client,
					"Invalid TKL entry from $client: "
					"Spamfilter '$spamfilter_string': regex does not compile: $spamfilter_regex_error",
					log_data_string("spamfilter_string", match_string),
					log_data_string("spamfilter_regex_error", err));
				return;
			}
			tkl = tkl_add_spamfilter(type, target, action, m, set_by, expire_at, set_at,
			                         tkl_duration, tkl_reason, 0);
		}
	} else
	{
		/* Unhandled, should never happen */
		abort();
	}

	if (!tkl)
		return;

	if (tkl_entry_exists)
	{
		/* Let's see if we need to update the existing entry.
		 * Note that we only update common fields,
		 * which is acceptable to me. -- Syzop
		 */
		if ((set_at < tkl->set_at) || (expire_at != tkl->expire_at) || strcmp(tkl->set_by, parv[5]))
		{
			/* here's how it goes:
			 * set_at: oldest wins
			 * expire_at: longest wins
			 * set_by: highest strcmp wins
			 *
			 * We broadcast the result of this back to all servers except
			 * sptr->direction, because that side will do the same thing and
			 * send it back to his servers (except us)... no need for a
			 * double networkwide flood ;p. -- Syzop
			 */
			tkl->set_at = MIN(tkl->set_at, set_at);

			if (!tkl->expire_at || !expire_at)
				tkl->expire_at = 0;
			else
				tkl->expire_at = MAX(tkl->expire_at, expire_at);

			if (strcmp(tkl->set_by, parv[5]) < 0)
				safe_strdup(tkl->set_by, parv[5]);

			if (type & TKL_GLOBAL)
				tkl_broadcast_entry(1, client, client, tkl);
		}
		return;
	}

	/* Below this line we will only use 'tkl'. No parc/parv reading anymore. */

	RunHook(HOOKTYPE_TKL_ADD, client, tkl);

	sendnotice_tkl_add(tkl);

	/* spamfilter 'warn' action is special */
	if ((tkl->type & TKL_SPAMF) && (tkl->ptr.spamfilter->action == BAN_ACT_WARN) && (tkl->ptr.spamfilter->target & SPAMF_USER))
		spamfilter_check_users(tkl);

	/* Ban checking executes during run loop for efficiency */
	loop.do_bancheck = 1;

	if (type & TKL_GLOBAL)
		tkl_broadcast_entry(1, client, client, tkl);
}

/** Delete a TKL using the TKL layer. See cmd_tkl for parv[] and protocol documentation. */
CMD_FUNC(cmd_tkl_del)
{
	TKL *tkl;
	int type;
	const char *removed_by;

	if (!IsServer(client) && !IsMe(client))
		return;

	if (parc < 6)
		return;

	type = tkl_chartotype(parv[2][0]);
	if (type == 0)
		return;

	removed_by = parv[5];

	if (TKLIsServerBanType(type))
	{
		const char *usermask = parv[3];
		const char *hostmask = parv[4];
		int softban = 0;

		if (*usermask == '%')
		{
			usermask++;
			softban = 1;
		}

		tkl = find_tkl_serverban(type, usermask, hostmask, softban);
	}
	else if (TKLIsBanExceptionType(type))
	{
		const char *usermask = parv[3];
		const char *hostmask = parv[4];
		int softban = 0;
		/* other parameters are ignored */

		if (*usermask == '%')
		{
			usermask++;
			softban = 1;
		}

		tkl = find_tkl_banexception(type, usermask, hostmask, softban);
	}
	else if (TKLIsNameBanType(type))
	{
		int hold = 0;
		const char *name = parv[4];

		if (*parv[3] == 'H')
			hold = 1;
		tkl = find_tkl_nameban(type, name, hold);
	}
	else if (TKLIsSpamfilterType(type))
	{
		const char *match_string;
		unsigned short target;
		BanAction action;

		if (parc < 9)
		{
			unreal_log(ULOG_WARNING, "tkl", "TKL_DEL_INVALID", client,
				"Invalid TKL deletion request from $client: "
				"Spamfilter with too few parameters. Running very old UnrealIRCd protocol (3.2.X?)");
			return; /* bogus */
		}
		if (parc >= 12)
			match_string = parv[11];
		else if (parc >= 11)
			match_string = parv[10];
		else
			match_string = parv[8];

		if (!(target = spamfilter_gettargets(parv[3], NULL)))
		{
			unreal_log(ULOG_WARNING, "tkl", "TKL_DEL_INVALID", client,
				"Invalid TKL deletion request from $client: "
				"Spamfilter '$spamfilter_string' has unkown targets '$spamfilter_targets'",
				log_data_string("spamfilter_string", match_string),
				log_data_string("spamfilter_targets", parv[3]));
			return;
		}

		if (!(action = banact_chartoval(*parv[4])))
		{
			unreal_log(ULOG_WARNING, "tkl", "TKL_DEL_INVALID", client,
				"Invalid TKL deletion request from $client: "
				"Spamfilter '$spamfilter_string' has unkown action '$spamfilter_action'",
				log_data_string("spamfilter_string", match_string),
				log_data_string("spamfilter_action", parv[4]));
			return;
		}
		tkl = find_tkl_spamfilter(type, match_string, action, target);
	} else
	{
		/* This can never happen, unless someone added a TKL type
		 * to UnrealIRCd but forgot to add the removal code :D.
		 */
		abort();
	}

	if (!tkl)
		return; /* Item not found, nothing to remove. */

	if (tkl->flags & TKL_FLAG_CONFIG)
		return; /* Item is in the configuration file (persistent) */

	/* broadcast remove msg to opers... */
	sendnotice_tkl_del(removed_by, tkl);

	if (type & TKL_SHUN)
		tkl_check_local_remove_shun(tkl);

	RunHook(HOOKTYPE_TKL_DEL, client, tkl);

	if (type & TKL_GLOBAL)
	{
		/* This is a bit of a hack for #5629. Will consider real fix post-release. */
		safe_strdup(tkl->set_by, removed_by);
		tkl_broadcast_entry(0, client, client, tkl);
	}

	if (TKLIsBanException(tkl))
	{
		/* Since an exception has been removed we have to re-check if
		 * any connected user is now matched by a ban.
		 * Set flag here, actual checking takes place in main loop.
		 */
		loop.do_bancheck = 1;
	}

	tkl_del_line(tkl);
}

/** TKL command: server to server handling of *LINEs and SPAMFILTERs.
 * HISTORY:
 * This was originall called Timed KLines, but today it's
 * used by various *line types eg: zline, gline, gzline, shun,
 * but also by spamfilter etc...
 * DOCUMENTATION
 * See (also) https://www.unrealircd.org/docs/Server_protocol:TKL_command
 * USAGE:
 * This routine is used both internally by the ircd (to
 * for example add local klines, zlines, etc) and over the
 * network (glines, gzlines, spamfilter, etc).
 *
 *           serverban  serverban  spamfilter      spamfilter         sqline:    ban exception:
 *           add:       remove:    remove in U4:   with TKLEXT2:
 * parv[ 1]: +          -          -               +/-                +/-        +/-
 * parv[ 2]: type       type       type            type               type       type
 * parv[ 3]: user       user       target          target             hold       user
 * parv[ 4]: host       host       action          action             host       host
 * parv[ 5]: set_by     removedby  (un)set_by      set_by/unset_by    set_by     set_by
 * parv[ 6]: expire_at             expire_at (0)   expire_at (0)      expire_at  expire_at
 * parv[ 7]: set_at                set_at          set_at             set_at     set_at
 * parv[ 8]: reason                regex           tkl duration       reason     except_type
 * parv[ 9]:                                       tkl reason [A]                reason
 * parv[10]:                                       match-type [B]
 * parv[11]:                                       match-string [C]
 *
 * [A] tkl reason field must be escaped by caller [eg: use unreal_encodespace()
 *     if cmd_tkl is called internally].
 * [B] match-type must be one of: regex, simple.
 * [C] Could be a regex or a regular string with wildcards, depending on [B]
 */
CMD_FUNC(_cmd_tkl)
{
	if (!IsServer(client) && !IsOper(client) && !IsMe(client))
		return;

	if (parc < 2)
		return;

	switch (*parv[1])
	{
		case '+':
			cmd_tkl_add(client, recv_mtags, parc, parv);
			break;
		case '-':
			cmd_tkl_del(client, recv_mtags, parc, parv);
			break;
		default:
			break;
	}
}

/** Configure the username/hostname TKL layer based on the BAN_TARGET_* configuration */
void ban_target_to_tkl_layer(BanTarget ban_target, BanAction action, Client *client, const char **tkl_username, const char **tkl_hostname)
{
	static char username[USERLEN+1];
	static char hostname[HOSTLEN+8];

	if ((action == BAN_ACT_ZLINE) || (action == BAN_ACT_GZLINE))
		ban_target = BAN_TARGET_IP; /* The only possible choice with ZLINE/GZLINE, other info is unavailable */

	if (ban_target == BAN_TARGET_ACCOUNT)
	{
		if (IsLoggedIn(client) && (*client->user->account != ':'))
		{
			/* Place a ban on ~a:Accountname */
			strlcpy(username, "~a:", sizeof(username));
			strlcpy(hostname, client->user->account, sizeof(hostname));
			*tkl_username = username;
			*tkl_hostname = hostname;
			return;
		}
		ban_target = BAN_TARGET_IP; /* fallback */
	} else
	if (ban_target == BAN_TARGET_CERTFP)
	{
		const char *fp = moddata_client_get(client, "certfp");
		if (fp)
		{
			/* Place a ban on ~S:sha256sumofclientcertificate */
			strlcpy(username, "~S:", sizeof(username));
			strlcpy(hostname, fp, sizeof(hostname));
			*tkl_username = username;
			*tkl_hostname = hostname;
			return;
		}
		ban_target = BAN_TARGET_IP; /* fallback */
	}

	/* Below we deal with the more common choices... */

	/* First, set the username */
	if (((ban_target == BAN_TARGET_USERIP) || (ban_target == BAN_TARGET_USERHOST)) && strcmp(client->ident, "unknown"))
		strlcpy(username, client->ident, sizeof(username));
	else
		strlcpy(username, "*", sizeof(username));

	/* Now set the host-portion of the TKL */
	if (((ban_target == BAN_TARGET_HOST) || (ban_target == BAN_TARGET_USERHOST)) && client->user && *client->user->realhost)
		strlcpy(hostname, client->user->realhost, sizeof(hostname));
	else
		strlcpy(hostname, GetIP(client), sizeof(hostname));

	*tkl_username = username;
	*tkl_hostname = hostname;
}

/** Take an action on the user, such as banning or killing.
 * @author Bram Matthys (Syzop), 2003-present
 * @param client     The client which is affected.
 * @param action   The type of ban (one of BAN_ACT_*).
 * @param reason   The ban reason.
 * @param duration The ban duration in seconds.
 * @note This function assumes that client is a locally connected user.
 * @retval 1 if action is taken, 0 if user is exempted.
 * @note Be sure to check IsDead(client) if return value is 1 and you are
 *       considering to continue processing.
 */
int _place_host_ban(Client *client, BanAction action, char *reason, long duration)
{
	/* If this is a soft action and the user is logged in, then the ban does not apply.
	 * NOTE: Actually in such a case it would be better if place_host_ban() would not
	 * be called at all. Or at least, the caller should not take any action
	 * (eg: the message should be delivered, the user may connect, etc..)
	 * The following is more like secondary protection in case the caller forgets...
	 */
	if (IsSoftBanAction(action) && IsLoggedIn(client))
		return 0;

	switch(action)
	{
		case BAN_ACT_TEMPSHUN:
			/* We simply mark this connection as shunned and do not add a ban record */
			unreal_log(ULOG_INFO, "tkl", "TKL_ADD_TEMPSHUN", &me,
				   "Temporary shun added on user $target.details [reason: $shun_reason] [by: $client]",
				   log_data_string("shun_reason", reason),
				   log_data_client("target", client));
			SetShunned(client);
			return 1;
		case BAN_ACT_GZLINE:
		case BAN_ACT_GLINE:
		case BAN_ACT_SOFT_GLINE:
		case BAN_ACT_ZLINE:
		case BAN_ACT_KLINE:
		case BAN_ACT_SOFT_KLINE:
		case BAN_ACT_SHUN:
		case BAN_ACT_SOFT_SHUN:
		{
			char ip[128], user[USERLEN+3], mo[100], mo2[100];
			const char *tkllayer[9] = {
				me.name,	/*0  server.name */
				"+",		/*1  +|- */
				"?",		/*2  type */
				"*",		/*3  user */
				NULL,		/*4  host */
				NULL,
				NULL,		/*6  expire_at */
				NULL,		/*7  set_at */
				NULL		/*8  reason */
			};

			ban_target_to_tkl_layer(iConf.automatic_ban_target, action, client, &tkllayer[3], &tkllayer[4]);

			/* For soft bans we need to prefix the % in the username */
			if (IsSoftBanAction(action))
			{
				char tmp[USERLEN+3];
				snprintf(tmp, sizeof(tmp), "%%%s", tkllayer[3]);
				strlcpy(user, tmp, sizeof(user));
				tkllayer[3] = user;
			}

			if ((action == BAN_ACT_KLINE) || (action == BAN_ACT_SOFT_KLINE))
				tkllayer[2] = "k";
			else if (action == BAN_ACT_ZLINE)
				tkllayer[2] = "z";
			else if (action == BAN_ACT_GZLINE)
				tkllayer[2] = "Z";
			else if ((action == BAN_ACT_GLINE) || (action == BAN_ACT_SOFT_GLINE))
				tkllayer[2] = "G";
			else if ((action == BAN_ACT_SHUN) || (action == BAN_ACT_SOFT_SHUN))
				tkllayer[2] = "s";
			tkllayer[5] = me.name;
			if (!duration)
				strlcpy(mo, "0", sizeof(mo)); /* perm */
			else
				ircsnprintf(mo, sizeof(mo), "%lld", (long long)(duration + TStime()));
			ircsnprintf(mo2, sizeof(mo2), "%lld", (long long)TStime());
			tkllayer[6] = mo;
			tkllayer[7] = mo2;
			tkllayer[8] = reason;
			cmd_tkl(&me, NULL, 9, tkllayer);
			RunHookReturnInt(HOOKTYPE_PLACE_HOST_BAN, !=99, client, action, reason, duration);
			if ((action == BAN_ACT_SHUN) || (action == BAN_ACT_SOFT_SHUN))
			{
				find_shun(client);
				return 1;
			} /* else.. */
			return find_tkline_match(client, 0);
		}
		case BAN_ACT_SOFT_KILL:
		case BAN_ACT_KILL:
		default:
			RunHookReturnInt(HOOKTYPE_PLACE_HOST_BAN, !=99, client, action, reason, duration);
			exit_client(client, NULL, reason);
			return 1;
	}
	return 0; /* no action taken (weird) */
}

/** This function compares two spamfilters ('one' and 'two') and will return
 * a 'winner' based on which one has the strongest action.
 * If both have equal action then some additional logic is applied simply
 * to ensure we (almost) always return the same winner regardless of the
 * order of the spamfilters (which may differ between servers).
 */
TKL *choose_winning_spamfilter(TKL *one, TKL *two)
{
	int n;

	if (!TKLIsSpamfilter(one) || !TKLIsSpamfilter(two))
		abort();

	/* First, see if the action field differs... */
	if (one->ptr.spamfilter->action != two->ptr.spamfilter->action)
	{
		/* We can simply compare the action. Highest (strongest) wins. */
		if (one->ptr.spamfilter->action > two->ptr.spamfilter->action)
			return one;
		else
			return two;
	}

	/* Ok, try comparing the regex then.. */
	n = strcmp(one->ptr.spamfilter->match->str, two->ptr.spamfilter->match->str);
	if (n < 0)
		return one;
	if (n > 0)
		return two;

	/* Hmm.. regex is identical. Try the 'reason' field. */
	n = strcmp(one->ptr.spamfilter->tkl_reason, two->ptr.spamfilter->tkl_reason);
	if (n < 0)
		return one;
	if (n > 0)
		return two;

	/* Hmm.. 'reason' is identical as well.
	 * Make a final decision, could still be identical but would be unlikely.
	 */
	return (one->ptr.spamfilter->target > two->ptr.spamfilter->target) ? one : two;
}

/** Checks if 'target' is on the spamfilter exception list.
 * RETURNS 1 if found in list, 0 if not.
 */
static int target_is_spamexcept(const char *target)
{
	SpamExcept *e;

	for (e = iConf.spamexcept; e; e = e->next)
	{
		if (match_simple(e->name, target))
			return 1;
	}
	return 0;
}

/** Make user join the virus channel.
 * @param client  The user that was doing something bad.
 * @param tk    The TKL entry that matched this user.
 * @param type  The spamfilter type (SPAMF_*)
 *              TODO: Looks redundant?
 */
int _join_viruschan(Client *client, TKL *tkl, int type)
{
	const char *xparv[3];
	char chbuf[CHANNELLEN + 16], buf[2048];
	Channel *channel;
	int ret;

	snprintf(buf, sizeof(buf), "0,%s", SPAMFILTER_VIRUSCHAN);
	xparv[0] = NULL;
	xparv[1] = buf;
	xparv[2] = NULL;

	/* RECURSIVE CAUTION in case we ever add blacklisted chans */
	spamf_ugly_vchanoverride = 1;
	do_cmd(client, NULL, "JOIN", 2, xparv);
	spamf_ugly_vchanoverride = 0;

	if (IsDead(client))
		return 0; /* killed due to JOIN */

	sendnotice(client, "You are now restricted to talking in %s: %s",
		SPAMFILTER_VIRUSCHAN, unreal_decodespace(tkl->ptr.spamfilter->tkl_reason));

	channel = find_channel(SPAMFILTER_VIRUSCHAN);
	if (channel)
	{
		MessageTag *mtags = NULL;
		ircsnprintf(chbuf, sizeof(chbuf), "@%s", channel->name);
		ircsnprintf(buf, sizeof(buf), "[Spamfilter] %s matched filter '%s' [%s] [%s]",
			client->name, tkl->ptr.spamfilter->match->str, cmdname_by_spamftarget(type),
			unreal_decodespace(tkl->ptr.spamfilter->tkl_reason));
		new_message(&me, NULL, &mtags);
		sendto_channel(channel, &me, NULL, "o",
		               0, SEND_ALL|SKIP_DEAF, mtags,
		               ":%s NOTICE %s :%s", me.name, chbuf, buf);
		free_message_tags(mtags);
	}
	SetVirus(client);
	return 1;
}

/** match_spamfilter: executes the spamfilter on the input string.
 * @param str		The text (eg msg text, notice text, part text, quit text, etc
 * @param target	The spamfilter target (SPAMF_*)
 * @param cmd		The command (eg: "PRIVMSG")
 * @param destination	The destination as a text string (eg: "somenick", can be NULL.. eg for away)
 * @param flags		Any flags (SPAMFLAG_*)
 * @param rettkl	Pointer to an aTKLline struct, _used for special circumstances only_
 * RETURN VALUE:
 * 1 if spamfilter matched and it should be blocked (or client exited), 0 if not matched.
 * In case of 1, be sure to check IsDead(client)..
 */
int _match_spamfilter(Client *client, const char *str_in, int target, const char *cmd, const char *destination, int flags, TKL **rettkl)
{
	TKL *tkl;
	TKL *winner_tkl = NULL;
	const char *str;
	int ret = -1;
	char *reason = NULL;
#ifdef SPAMFILTER_DETECTSLOW
	struct rusage rnow, rprev;
	long ms_past;
#endif

	if (rettkl)
		*rettkl = NULL; /* initialize to NULL */

	if (!cmd)
		cmd = cmdname_by_spamftarget(target);

	if (target == SPAMF_USER)
		str = str_in;
	else
		str = StripControlCodes(str_in);

	/* (note: using client->user check here instead of IsUser()
	 * due to SPAMF_USER where user isn't marked as client/person yet.
	 */
	if (!client->user || ValidatePermissionsForPath("immune:server-ban:spamfilter",client,NULL,NULL,NULL) || IsULine(client))
		return 0;

	/* Client exempt from spamfilter checking?
	 * Let's check that early: going through elines is likely faster than running the regex(es).
	 */
	if (find_tkl_exception(TKL_SPAMF, client))
		return 0;

	for (tkl = tklines[tkl_hash('F')]; tkl; tkl = tkl->next)
	{
		if (!(tkl->ptr.spamfilter->target & target))
			continue;

		if ((flags & SPAMFLAG_NOWARN) && (tkl->ptr.spamfilter->action == BAN_ACT_WARN))
			continue;

		/* If the action is 'soft' (for non-logged in users only) then
		 * don't bother running the spamfilter if the user is logged in.
		 */
		if (IsSoftBanAction(tkl->ptr.spamfilter->action) && IsLoggedIn(client))
			continue;

#ifdef SPAMFILTER_DETECTSLOW
		memset(&rnow, 0, sizeof(rnow));
		memset(&rprev, 0, sizeof(rnow));

		getrusage(RUSAGE_SELF, &rprev);
#endif

		ret = unreal_match(tkl->ptr.spamfilter->match, str);

#ifdef SPAMFILTER_DETECTSLOW
		getrusage(RUSAGE_SELF, &rnow);

		ms_past = ((rnow.ru_utime.tv_sec - rprev.ru_utime.tv_sec) * 1000) +
		          ((rnow.ru_utime.tv_usec - rprev.ru_utime.tv_usec) / 1000);

		if ((SPAMFILTER_DETECTSLOW_FATAL > 0) && (ms_past > SPAMFILTER_DETECTSLOW_FATAL))
		{
			unreal_log(ULOG_ERROR, "tkl", "SPAMFILTER_SLOW_FATAL", NULL,
			           "[Spamfilter] WARNING: Too slow spamfilter detected (took $msec_time msec to execute) "
			           "-- spamfilter will be \002REMOVED!\002: $tkl",
			           log_data_tkl("tkl", tkl),
			           log_data_integer("msec_time", ms_past));
			tkl_del_line(tkl);
			return 0; /* Act as if it didn't match, even if it did.. it's gone now anyway.. */
		} else
		if ((SPAMFILTER_DETECTSLOW_WARN > 0) && (ms_past > SPAMFILTER_DETECTSLOW_WARN))
		{
			unreal_log(ULOG_WARNING, "tkl", "SPAMFILTER_SLOW_WARN", NULL,
			           "[Spamfilter] WARNING: Slow spamfilter detected (took $msec_time msec to execute): $tkl",
			           log_data_tkl("tkl", tkl),
			           log_data_integer("msec_time", ms_past));
		}
#endif

		if (ret)
		{
			/* We have a match! But.. perhaps it's on the exceptions list? */
			if (!winner_tkl && destination && target_is_spamexcept(destination))
				return 0; /* No problem! */

			unreal_log(ULOG_INFO, "tkl", "SPAMFILTER_MATCH", client,
			           "[Spamfilter] $client.details matches filter '$tkl': [cmd: $command$_space$destination: '$str'] [reason: $tkl.reason] [action: $tkl.ban_action]",
				   log_data_tkl("tkl", tkl),
				   log_data_string("command", cmd),
				   log_data_string("_space", destination ? " " : ""),
				   log_data_string("destination", destination ? destination : ""),
				   log_data_string("str", str));

			RunHook(HOOKTYPE_LOCAL_SPAMFILTER, client, str, str_in, target, destination, tkl);

			/* If we should stop after the first match, we end here... */
			if (SPAMFILTER_STOP_ON_FIRST_MATCH)
			{
				winner_tkl = tkl;
				break;
			}

			/* Otherwise.. we set 'winner_tkl' to the spamfilter with the strongest action. */
			if (!winner_tkl)
				winner_tkl = tkl;
			else
				winner_tkl = choose_winning_spamfilter(tkl, winner_tkl);

			/* and continue.. */
		}
	}

	tkl = winner_tkl;

	if (!tkl)
		return 0; /* NOMATCH, we are done */

	/* Spamfilter matched, take action: */

	reason = unreal_decodespace(tkl->ptr.spamfilter->tkl_reason);
	if ((tkl->ptr.spamfilter->action == BAN_ACT_BLOCK) || (tkl->ptr.spamfilter->action == BAN_ACT_SOFT_BLOCK))
	{
		switch(target)
		{
			case SPAMF_USERMSG:
			case SPAMF_USERNOTICE:
			{
				char errmsg[512];
				ircsnprintf(errmsg, sizeof(errmsg), "Message blocked: %s", reason);
				sendnumeric(client, ERR_CANTSENDTOUSER, destination, errmsg);
				break;
			}
			case SPAMF_CHANNOTICE:
				break; /* no replies to notices */
			case SPAMF_CHANMSG:
			{
				sendto_one(client, NULL, ":%s 404 %s %s :Message blocked: %s",
					me.name, client->name, destination, reason);
				break;
			}
			case SPAMF_MTAG:
			{
				sendnumericfmt(client, ERR_CANNOTDOCOMMAND, "%s :Command blocked: %s",
					cmd, reason);
				break;
			}
			case SPAMF_DCC:
			{
				char errmsg[512];
				ircsnprintf(errmsg, sizeof(errmsg), "DCC blocked: %s", reason);
				sendnumeric(client, ERR_CANTSENDTOUSER, destination, errmsg);
				break;
			}
			case SPAMF_AWAY:
				/* hack to deal with 'after-away-was-set-filters' */
				if (client->user->away && !strcmp(str_in, client->user->away))
				{
					/* free away & broadcast the unset */
					safe_free(client->user->away);
					client->user->away = NULL;
					sendto_server(client, 0, 0, NULL, ":%s AWAY", client->id);
				}
				break;
			case SPAMF_TOPIC:
				//...
				sendnotice(client, "Setting of topic on %s to that text is blocked: %s",
					destination, reason);
				break;
			default:
				break;
		}
		return 1;
	} else
	if ((tkl->ptr.spamfilter->action == BAN_ACT_WARN) || (tkl->ptr.spamfilter->action == BAN_ACT_SOFT_WARN))
	{
		if ((target != SPAMF_USER) && (target != SPAMF_QUIT))
			sendnumeric(client, RPL_SPAMCMDFWD, cmd, reason);
		return 0;
	} else
	if ((tkl->ptr.spamfilter->action == BAN_ACT_DCCBLOCK) || (tkl->ptr.spamfilter->action == BAN_ACT_SOFT_DCCBLOCK))
	{
		if (target == SPAMF_DCC)
		{
			sendnotice(client, "DCC to %s blocked: %s", destination, reason);
			sendnotice(client, "*** You have been blocked from sending files, reconnect to regain permission to send files");
			SetDCCBlock(client);
		}
		return 1;
	} else
	if ((tkl->ptr.spamfilter->action == BAN_ACT_VIRUSCHAN) || (tkl->ptr.spamfilter->action == BAN_ACT_SOFT_VIRUSCHAN))
	{
		if (IsVirus(client)) /* Already tagged */
			return 0;

		/* There's a race condition for SPAMF_USER, so 'rettk' is used for SPAMF_USER
		 * when a user is currently connecting and filters are checked:
		 */
		if (!IsUser(client))
		{
			if (rettkl)
				*rettkl = tkl;
			return 1;
		}

		join_viruschan(client, tkl, target);
		return 1;
	} else
	{
		return place_host_ban(client, tkl->ptr.spamfilter->action, reason, tkl->ptr.spamfilter->tkl_duration);
	}

	return 0; /* NOTREACHED */
}

/** Check message-tag spamfilters.
 * @param client	The client
 * @param mtags		Message tags sent by client
 * @param cmd		Command to be executed (can be NULL)
 * @retval Return 1 to stop processing the command (ignore it) or 0 to allow/continue as normal
 */
int _match_spamfilter_mtags(Client *client, MessageTag *mtags, char *cmd)
{
	MessageTag *m;
	char buf[4096];
	char *str;

	/* This is a shortcut: if there are no spamfilters present
	 * on message tags then we can return immediately.
	 * Saves a lot of CPU and it is quite likely too!
	 */
	if (mtag_spamfilters_present == 0)
		return 0;

	for (m = mtags; m; m = m->next)
	{
		if (m->value)
		{
			snprintf(buf, sizeof(buf), "%s=%s", m->name, m->value);
			str = buf;
		} else {
			str = m->name;
		}
		if (match_spamfilter(client, str, SPAMF_MTAG, cmd, NULL, 0, NULL))
			return 1;
	}
	return 0;
}

/** Updates 'mtag_spamfilters_present' based on if any spamfilters
 * are present with the SPAMF_MTAG target.
 */
int check_mtag_spamfilters_present(void)
{
	TKL *tkl;

	for (tkl = tklines[tkl_hash('F')]; tkl; tkl = tkl->next)
	{
		if (tkl->ptr.spamfilter->target & SPAMF_MTAG)
		{
			mtag_spamfilters_present = 1;
			return 1;
		}
	}

	mtag_spamfilters_present = 0;
	return 0;
}

/** CIDR function to compare the first 'mask' bits.
 * @author Taken from atheme
 * @returns 1 if equal, 0 if not.
 */
static int comp_with_mask(void *addr, void *dest, u_int mask)
{
	if (memcmp(addr, dest, mask / 8) == 0)
	{
		int n = mask / 8;
		int m = (0xffff << (8 - (mask % 8)));
		if (mask % 8 == 0 || (((u_char *) addr)[n] & m) == (((u_char *) dest)[n] & m))
		{
			return (1);
		}
	}
	return (0);
}

#define IPSZ 16

/** Match a user against a mask.
 * This will deal with 'nick!user@host', 'user@host' and just 'host'.
 * We try to match the 'host' portion against the client IP, real host, etc...
 * CIDR support is available so 'host' may be like '1.2.0.0/16'.
 * @returns 1 on match, 0 on no match.
 */
int _match_user(const char *rmask, Client *client, int options)
{
	char mask[NICKLEN+USERLEN+HOSTLEN+8];
	char clientip[IPSZ], maskip[IPSZ];
	char *p = NULL;
	char *nmask = NULL, *umask = NULL, *hmask = NULL;
	int cidr = -1; /* CIDR length, -1 for no CIDR */

	strlcpy(mask, rmask, sizeof(mask));

	if ((options & MATCH_CHECK_EXTENDED) &&
	    is_extended_server_ban(mask) &&
	    client->user)
	{
		/* Check user properties / extbans style */
		return _match_user_extended_server_ban(rmask, client);
	}

	if (!(options & MATCH_MASK_IS_UHOST))
	{
		p = strchr(mask, '!');
		if (p)
		{
			*p++ = '\0';
			if (!*mask)
				return 0; /* NOMATCH: '!...' */
			nmask = mask;
			umask = p;

			/* Could just as well check nick right now */
			if (!match_simple(nmask, client->name))
				return 0; /* NOMATCH: nick mask did not match */
		}
	}

	if (!(options & (MATCH_MASK_IS_HOST)))
	{
		p = strchr(p ? p : mask, '@');
		if (p)
		{
			char *client_username = (client->user && *client->user->username) ? client->user->username : client->ident;

			*p++ = '\0';
			if (!*p || !*mask)
				return 0; /* NOMATCH: '...@' or '@...' */
			hmask = p;
			if (!umask)
				umask = mask;

			/* Check user portion right away */
			if (!match_simple(umask, client_username))
				return 0; /* NOMATCH: user mask did not match */
		} else {
			if (nmask)
				return 0; /* NOMATCH: 'abc!def' (or even just 'abc!') */
			hmask = mask;
		}
	} else {
		hmask = mask;
	}

	/* If we get here then we have done checking nick / ident (if it was needed)
	 * and now need to match the 'host' portion.
	 */

	/**** Check visible host ****/
	if (options & MATCH_CHECK_VISIBLE_HOST)
	{
		char *hostname = client->user ? GetHost(client) : (MyUser(client) ? client->local->sockhost : NULL);
		if (hostname && match_simple(hmask, hostname))
			return 1; /* MATCH: visible host */
	}

	/**** Check cloaked host ****/
	if (options & MATCH_CHECK_CLOAKED_HOST)
	{
		if (client->user && match_simple(hmask, client->user->cloakedhost))
			return 1; /* MATCH: cloaked host */
	}

	/**** check on IP ****/
	if (options & MATCH_CHECK_IP)
	{
		p = strchr(hmask, '/');
		if (p)
		{
			*p++ = '\0';
			cidr = atoi(p);
			if (cidr <= 0)
				return 0; /* NOMATCH: invalid CIDR */
		}

		if (strchr(hmask, '?') || strchr(hmask, '*'))
		{
			/* Wildcards */
			if (client->ip && match_simple(hmask, client->ip))
				return 1; /* MATCH (IP with wildcards) */
		} else
		if (strchr(hmask, ':'))
		{
			/* IPv6 hostmask */

			/* We can actually return here on match/nomatch as we don't need to check the
			 * virtual host and things like that since ':' can never be in a hostname.
			 */
			if (!client->ip || !strchr(client->ip, ':'))
				return 0; /* NOMATCH: hmask is IPv6 address and client is not IPv6 */
			if (!inet_pton(AF_INET6, client->ip, clientip))
				return 0; /* NOMATCH: unusual failure */
			if (!inet_pton(AF_INET6, hmask, maskip))
				return 0; /* NOMATCH: invalid IPv6 IP in hostmask */

			if (cidr < 0)
				return comp_with_mask(clientip, maskip, 128); /* MATCH/NOMATCH by exact IP */

			if (cidr > 128)
				return 0; /* NOMATCH: invalid CIDR */

			return comp_with_mask(clientip, maskip, cidr);
		} else
		{
			/* Host is not IPv6 and does not contain wildcards.
			 * So could be a literal IPv4 address or IPv4 CIDR.
			 * NOTE: could also be neither (like a real hostname), so don't return 0 on nomatch,
			 * in that case we should just continue...
			 * The exception is CIDR. If we have CIDR mask then don't bother checking for
			 * virtual hosts and things like that since '/' can never be in a hostname.
			 */
			if (client->ip && inet_pton(AF_INET, client->ip, clientip) && inet_pton(AF_INET, hmask, maskip))
			{
				if (cidr < 0)
				{
					if (comp_with_mask(clientip, maskip, 32))
						return 1; /* MATCH: exact IP */
				}
				else if (cidr > 32)
					return 0; /* NOMATCH: invalid CIDR */
				else
					return comp_with_mask(clientip, maskip, cidr); /* MATCH/NOMATCH by CIDR */
			}
		}
	}

	/**** Check real host ****/
	if (options & MATCH_CHECK_REAL_HOST)
	{
		char *hostname = client->user ? client->user->realhost : (MyUser(client) ? client->local->sockhost : NULL);
		if (hostname && match_simple(hmask, hostname))
			return 1; /* MATCH: hostname match */
	}

	return 0; /* NOMATCH: nothing of the above matched */
}

int _match_user_extended_server_ban(const char *banstr, Client *client)
{
	const char *nextbanstr;
	Extban *extban;
	BanContext *b;
	int ret;

	if (!is_extended_server_ban(banstr))
		return 0; /* we should never have been called */

	extban = findmod_by_bantype(banstr, &nextbanstr);
	if (!extban ||
	    !(extban->options & EXTBOPT_TKL) ||
	    !(extban->is_banned_events & BANCHK_TKL))
	{
		return 0; /* extban not found or of incorrect type (eg ~T) */
	}

	b = safe_alloc(sizeof(BanContext));
	b->client = client;
	b->banstr = nextbanstr;
	b->ban_check_types = BANCHK_TKL;
	ret = extban->is_banned(b);
	safe_free(b);
	return ret;
}
