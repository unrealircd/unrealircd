/************************************************************************
 *   IRC - Internet Relay Chat, src/api-efunctions.c
 *   (C) 2003-2019 Bram Matthys (Syzop) and the UnrealIRCd Team
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

/* Types */
typedef struct {
	char *name;
	void **funcptr;
	void *deffunc;
} EfunctionsList;

/* Variables */
static Efunction *Efunctions[MAXEFUNCTIONS]; /* Efunction objects (used for rehashing) */
static EfunctionsList efunction_table[MAXEFUNCTIONS];

/* Efuncs */
void (*do_join)(Client *client, int parc, const char *parv[]);
void (*join_channel)(Channel *channel, Client *client, MessageTag *mtags, const char *member_modes);
int (*can_join)(Client *client, Channel *channel, const char *key, char **errmsg);
void (*do_mode)(Channel *channel, Client *client, MessageTag *mtags, int parc, const char *parv[], time_t sendts, int samode);
MultiLineMode *(*set_mode)(Channel *channel, Client *client, int parc, const char *parv[], u_int *pcount,
                           char pvar[MAXMODEPARAMS][MODEBUFLEN + 3]);
void (*set_channel_mode)(Channel *channel, char *modes, char *parameters);
void (*cmd_umode)(Client *client, MessageTag *mtags, int parc, const char *parv[]);
int (*register_user)(Client *client);
int (*tkl_hash)(unsigned int c);
char (*tkl_typetochar)(int type);
int (*tkl_chartotype)(char c);
const char *(*tkl_type_string)(TKL *tk);
const char *(*tkl_type_config_string)(TKL *tk);
char *(*tkl_uhost)(TKL *tkl, char *buf, size_t buflen, int options);
TKL *(*tkl_add_serverban)(int type, const char *usermask, const char *hostmask, const char *reason, const char *setby,
                              time_t expire_at, time_t set_at, int soft, int flags);
TKL *(*tkl_add_nameban)(int type, const char *name, int hold, const char *reason, const char *setby,
                            time_t expire_at, time_t set_at, int flags);
TKL *(*tkl_add_spamfilter)(int type, unsigned short target, unsigned short action, Match *match, const char *setby,
                               time_t expire_at, time_t set_at,
                               time_t spamf_tkl_duration, const char *spamf_tkl_reason,
                               int flags);
TKL *(*tkl_add_banexception)(int type, const char *usermask, const char *hostmask, const char *reason, const char *set_by,
                                time_t expire_at, time_t set_at, int soft, const char *bantypes, int flags);
TKL *(*tkl_del_line)(TKL *tkl);
void (*tkl_check_local_remove_shun)(TKL *tmp);
int (*find_tkline_match)(Client *client, int skip_soft);
int (*find_shun)(Client *client);
int(*find_spamfilter_user)(Client *client, int flags);
TKL *(*find_qline)(Client *client, const char *nick, int *ishold);
TKL *(*find_tkline_match_zap)(Client *client);
void (*tkl_stats)(Client *client, int type, const char *para, int *cnt);
void (*tkl_sync)(Client *client);
void (*cmd_tkl)(Client *client, MessageTag *mtags, int parc, const char *parv[]);
int (*place_host_ban)(Client *client, BanAction action, const char *reason, long duration);
int (*match_spamfilter)(Client *client, const char *str_in, int type, const char *cmd, const char *target, int flags, TKL **rettk);
int (*match_spamfilter_mtags)(Client *client, MessageTag *mtags, const char *cmd);
int (*join_viruschan)(Client *client, TKL *tk, int type);
const char *(*StripColors)(const char *text);
const char *(*StripControlCodes)(const char *text);
void (*spamfilter_build_user_string)(char *buf, const char *nick, Client *client);
void (*send_protoctl_servers)(Client *client, int response);
int (*verify_link)(Client *client, ConfigItem_link **link_out);
void (*introduce_user)(Client *to, Client *client);
void (*send_server_message)(Client *client);
void (*broadcast_md_client)(ModDataInfo *mdi, Client *client, ModData *md);
void (*broadcast_md_channel)(ModDataInfo *mdi, Channel *channel, ModData *md);
void (*broadcast_md_member)(ModDataInfo *mdi, Channel *channel, Member *m, ModData *md);
void (*broadcast_md_membership)(ModDataInfo *mdi, Client *client, Membership *m, ModData *md);
int (*check_banned)(Client *client, int exitflags);
int (*check_deny_version)(Client *client, const char *software, int protocol, const char *flags);
void (*broadcast_md_client_cmd)(Client *except, Client *sender, Client *acptr, const char *varname, const char *value);
void (*broadcast_md_channel_cmd)(Client *except, Client *sender, Channel *channel, const char *varname, const char *value);
void (*broadcast_md_member_cmd)(Client *except, Client *sender, Channel *channel, Client *acptr, const char *varname, const char *value);
void (*broadcast_md_membership_cmd)(Client *except, Client *sender, Client *acptr, Channel *channel, const char *varname, const char *value);
void (*moddata_add_s2s_mtags)(Client *client, MessageTag **mtags);
void (*moddata_extract_s2s_mtags)(Client *client, MessageTag *mtags);
void (*send_moddata_client)(Client *srv, Client *client);
void (*send_moddata_channel)(Client *srv, Channel *channel);
void (*send_moddata_members)(Client *srv);
void (*broadcast_moddata_client)(Client *client);
int (*match_user)(const char *rmask, Client *client, int options);
void (*userhost_changed)(Client *client);
void (*userhost_save_current)(Client *client);
void (*send_join_to_local_users)(Client *client, Channel *channel, MessageTag *mtags);
int (*do_nick_name)(char *nick);
int (*do_remote_nick_name)(char *nick);
const char *(*charsys_get_current_languages)(void);
void (*broadcast_sinfo)(Client *client, Client *to, Client *except);
void (*connect_server)(ConfigItem_link *aconf, Client *by, struct hostent *hp);
void (*parse_message_tags)(Client *client, char **str, MessageTag **mtag_list);
const char *(*mtags_to_string)(MessageTag *m, Client *client);
int (*can_send_to_channel)(Client *client, Channel *channel, const char **msgtext, const char **errmsg, int notice);
void (*broadcast_md_globalvar)(ModDataInfo *mdi, ModData *md);
void (*broadcast_md_globalvar_cmd)(Client *except, Client *sender, const char *varname, const char *value);
int (*tkl_ip_hash)(const char *ip);
int (*tkl_ip_hash_type)(int type);
void (*sendnotice_tkl_del)(const char *removed_by, TKL *tkl);
void (*sendnotice_tkl_add)(TKL *tkl);
void (*free_tkl)(TKL *tkl);
TKL *(*find_tkl_serverban)(int type, const char *usermask, const char *hostmask, int softban);
TKL *(*find_tkl_banexception)(int type, const char *usermask, const char *hostmask, int softban);
TKL *(*find_tkl_nameban)(int type, const char *name, int hold);
TKL *(*find_tkl_spamfilter)(int type, const char *match_string, unsigned short action, unsigned short target);
int (*find_tkl_exception)(int ban_type, Client *client);
int (*is_silenced)(Client *client, Client *acptr);
int (*del_silence)(Client *client, const char *mask);
int (*add_silence)(Client *client, const char *mask, int senderr);
void *(*labeled_response_save_context)(void);
void (*labeled_response_set_context)(void *ctx);
void (*labeled_response_force_end)(void);
void (*kick_user)(MessageTag *mtags, Channel *channel, Client *client, Client *victim, const char *comment);
int (*watch_add)(const char *nick, Client *client, int flags);
int (*watch_del)(const char *nick, Client *client, int flags);
int (*watch_del_list)(Client *client, int flags);
Watch *(*watch_get)(const char *nick);
int (*watch_check)(Client *client, int reply, int (*watch_notify)(Client *client, Watch *watch, Link *lp, int event));
void (*do_unreal_log_remote_deliver)(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, const char *json_serialized);
char *(*get_chmodes_for_user)(Client *client, const char *flags);
WhoisConfigDetails (*whois_get_policy)(Client *client, Client *target, const char *name);
int (*make_oper)(Client *client, const char *operblock_name, const char *operclass, ConfigItem_class *clientclass, long modes, const char *snomask, const char *vhost);

Efunction *EfunctionAddMain(Module *module, EfunctionType eftype, int (*func)(), void (*vfunc)(), void *(*pvfunc)(), char *(*stringfunc)(), const char *(*conststringfunc)())
{
	Efunction *p;

	if (!module || !(module->options & MOD_OPT_OFFICIAL))
	{
		if (module)
			module->errorcode = MODERR_INVALID;
		return NULL;
	}
	
	p = safe_alloc(sizeof(Efunction));
	if (func)
		p->func.intfunc = func;
	if (vfunc)
		p->func.voidfunc = vfunc;
	if (pvfunc)
		p->func.pvoidfunc = pvfunc;
	if (stringfunc)
		p->func.stringfunc = stringfunc;
	if (conststringfunc)
		p->func.conststringfunc = conststringfunc;
	p->type = eftype;
	p->owner = module;
	AddListItem(p, Efunctions[eftype]);
	if (module)
	{
		ModuleObject *cbobj = safe_alloc(sizeof(ModuleObject));
		cbobj->object.efunction = p;
		cbobj->type = MOBJ_EFUNCTION;
		AddListItem(cbobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	return p;
}

Efunction *EfunctionDel(Efunction *cb)
{
	Efunction *p, *q;

	for (p = Efunctions[cb->type]; p; p = p->next)
	{
		if (p == cb)
		{
			q = p->next;
			DelListItem(p, Efunctions[cb->type]);
			if (*efunction_table[cb->type].funcptr == p)
				*efunction_table[cb->type].funcptr = NULL;
			if (p->owner)
			{
				ModuleObject *cbobj;
				for (cbobj = p->owner->objects; cbobj; cbobj = cbobj->next)
				{
					if ((cbobj->type == MOBJ_EFUNCTION) && (cbobj->object.efunction == p))
					{
						DelListItem(cbobj, cb->owner->objects);
						safe_free(cbobj);
						break;
					}
				}
			}
			safe_free(p);
			return q;
		}
	}
	return NULL;
}

static int num_efunctions(EfunctionType eftype)
{
	Efunction *e;
	int cnt = 0;

#ifdef DEBUGMODE
	if ((eftype < 0) || (eftype >= MAXEFUNCTIONS))
		abort();
#endif

	for (e = Efunctions[eftype]; e; e = e->next)
		if (!e->willberemoved)
			cnt++;
			
	return cnt;
}


/** Ensure that all efunctions are present. */
int efunctions_check(void)
{
	int i, n, errors=0;

	for (i=0; i < MAXEFUNCTIONS; i++)
		if (efunction_table[i].name)
		{
			n = num_efunctions(i);
			if ((n != 1) && (errors > 10))
			{
				config_error("[--efunction errors truncated to prevent flooding--]");
				break;
			}
			if ((n < 1) && !efunction_table[i].deffunc)
			{
				config_error("ERROR: efunction '%s' not found, you probably did not "
				             "load all required modules! (hint: see modules.default.conf)",
				             efunction_table[i].name);
				errors++;
			} else
			if (n > 1)
			{
				config_error("ERROR: efunction '%s' was found %d times, perhaps you "
				             "loaded a module multiple times??",
				             efunction_table[i].name, n);
				errors++;
			}
		}
	return errors ? -1 : 0;
}

void efunctions_switchover(void)
{
	Efunction *e;
	int i;

	/* Now set the real efunction, and tag the new one
	 * as 'willberemoved' if needed.
	 */

	for (i=0; i < MAXEFUNCTIONS; i++)
	{
		int found = 0;
		for (e = Efunctions[i]; e; e = e->next)
		{
			if (e->willberemoved)
				continue;
			if (!efunction_table[i].funcptr)
			{
				unreal_log(ULOG_FATAL, "module", "BUG_EFUNCTIONS_SWITCHOVER", NULL,
				           "[BUG] efunctions_switchover(): someone forgot to initialize the function table for efunc $efunction_number",
				           log_data_integer("efunction_number", i));
				abort();
			}
			*efunction_table[i].funcptr = e->func.voidfunc;  /* This is the new one. */
			if (!(e->owner->options & MOD_OPT_PERM))
				e->willberemoved = 1;
			found = 1;
			break;
		}
		if (!found)
		{
			if (efunction_table[i].deffunc)
				*efunction_table[i].funcptr = efunction_table[i].deffunc;
		}
	}
}

#define efunc_init_function(what, func, default_func) efunc_init_function_(what, #func, (void *)&func, (void *)default_func)

void efunc_init_function_(EfunctionType what, char *name, void *func, void *default_func)
{
	if (what >= MAXEFUNCTIONS)
	{
		/* increase MAXEFUNCTIONS if you ever encounter that --k4be */
		unreal_log(ULOG_FATAL, "module", "BUG_EFUNC_INIT_FUNCTION_TOO_MANY", NULL,
		           "Too many efunctions! ($efunctions_request > $efunctions_max)",
		           log_data_integer("efunctions_request", what),
		           log_data_integer("efunctions_max", MAXEFUNCTIONS));
		abort();
	}
	safe_strdup(efunction_table[what].name, name);
	efunction_table[what].funcptr = func;
	efunction_table[what].deffunc = default_func;
}

void efunctions_init(void)
{
	memset(&efunction_table, 0, sizeof(efunction_table));
	efunc_init_function(EFUNC_DO_JOIN, do_join, NULL);
	efunc_init_function(EFUNC_JOIN_CHANNEL, join_channel, NULL);
	efunc_init_function(EFUNC_CAN_JOIN, can_join, NULL);
	efunc_init_function(EFUNC_DO_MODE, do_mode, NULL);
	efunc_init_function(EFUNC_SET_MODE, set_mode, NULL);
	efunc_init_function(EFUNC_SET_CHANNEL_MODE, set_channel_mode, NULL);
	efunc_init_function(EFUNC_CMD_UMODE, cmd_umode, NULL);
	efunc_init_function(EFUNC_REGISTER_USER, register_user, NULL);
	efunc_init_function(EFUNC_TKL_HASH, tkl_hash, NULL);
	efunc_init_function(EFUNC_TKL_TYPETOCHAR, tkl_typetochar, NULL);
	efunc_init_function(EFUNC_TKL_ADD_SERVERBAN, tkl_add_serverban, NULL);
	efunc_init_function(EFUNC_TKL_ADD_BANEXCEPTION, tkl_add_banexception, NULL);
	efunc_init_function(EFUNC_TKL_DEL_LINE, tkl_del_line, NULL);
	efunc_init_function(EFUNC_TKL_CHECK_LOCAL_REMOVE_SHUN, tkl_check_local_remove_shun, NULL);
	efunc_init_function(EFUNC_FIND_TKLINE_MATCH, find_tkline_match, NULL);
	efunc_init_function(EFUNC_FIND_SHUN, find_shun, NULL);
	efunc_init_function(EFUNC_FIND_SPAMFILTER_USER, find_spamfilter_user, NULL);
	efunc_init_function(EFUNC_FIND_QLINE, find_qline, NULL);
	efunc_init_function(EFUNC_FIND_TKLINE_MATCH_ZAP, find_tkline_match_zap, NULL);
	efunc_init_function(EFUNC_TKL_STATS, tkl_stats, NULL);
	efunc_init_function(EFUNC_TKL_SYNCH, tkl_sync, NULL);
	efunc_init_function(EFUNC_CMD_TKL, cmd_tkl, NULL);
	efunc_init_function(EFUNC_PLACE_HOST_BAN, place_host_ban, NULL);
	efunc_init_function(EFUNC_MATCH_SPAMFILTER, match_spamfilter, NULL);
	efunc_init_function(EFUNC_MATCH_SPAMFILTER_MTAGS, match_spamfilter_mtags, NULL);
	efunc_init_function(EFUNC_JOIN_VIRUSCHAN, join_viruschan, NULL);
	efunc_init_function(EFUNC_STRIPCOLORS, StripColors, NULL);
	efunc_init_function(EFUNC_STRIPCONTROLCODES, StripControlCodes, NULL);
	efunc_init_function(EFUNC_SPAMFILTER_BUILD_USER_STRING, spamfilter_build_user_string, NULL);
	efunc_init_function(EFUNC_SEND_PROTOCTL_SERVERS, send_protoctl_servers, NULL);
	efunc_init_function(EFUNC_VERIFY_LINK, verify_link, NULL);
	efunc_init_function(EFUNC_SEND_SERVER_MESSAGE, send_server_message, NULL);
	efunc_init_function(EFUNC_BROADCAST_MD_CLIENT, broadcast_md_client, NULL);
	efunc_init_function(EFUNC_BROADCAST_MD_CHANNEL, broadcast_md_channel, NULL);
	efunc_init_function(EFUNC_BROADCAST_MD_MEMBER, broadcast_md_member, NULL);
	efunc_init_function(EFUNC_BROADCAST_MD_MEMBERSHIP, broadcast_md_membership, NULL);
	efunc_init_function(EFUNC_CHECK_BANNED, check_banned, NULL);
	efunc_init_function(EFUNC_INTRODUCE_USER, introduce_user, NULL);
	efunc_init_function(EFUNC_CHECK_DENY_VERSION, check_deny_version, NULL);
	efunc_init_function(EFUNC_BROADCAST_MD_CLIENT_CMD, broadcast_md_client_cmd, NULL);
	efunc_init_function(EFUNC_BROADCAST_MD_CHANNEL_CMD, broadcast_md_channel_cmd, NULL);
	efunc_init_function(EFUNC_BROADCAST_MD_MEMBER_CMD, broadcast_md_member_cmd, NULL);
	efunc_init_function(EFUNC_BROADCAST_MD_MEMBERSHIP_CMD, broadcast_md_membership_cmd, NULL);
	efunc_init_function(EFUNC_MODDATA_ADD_S2S_MTAGS, moddata_add_s2s_mtags, NULL);
	efunc_init_function(EFUNC_MODDATA_EXTRACT_S2S_MTAGS, moddata_extract_s2s_mtags, NULL);
	efunc_init_function(EFUNC_SEND_MODDATA_CLIENT, send_moddata_client, NULL);
	efunc_init_function(EFUNC_SEND_MODDATA_CHANNEL, send_moddata_channel, NULL);
	efunc_init_function(EFUNC_SEND_MODDATA_MEMBERS, send_moddata_members, NULL);
	efunc_init_function(EFUNC_BROADCAST_MODDATA_CLIENT, broadcast_moddata_client, NULL);
	efunc_init_function(EFUNC_MATCH_USER, match_user, NULL);
	efunc_init_function(EFUNC_USERHOST_SAVE_CURRENT, userhost_save_current, NULL);
	efunc_init_function(EFUNC_USERHOST_CHANGED, userhost_changed, NULL);
	efunc_init_function(EFUNC_SEND_JOIN_TO_LOCAL_USERS, send_join_to_local_users, NULL);
	efunc_init_function(EFUNC_DO_NICK_NAME, do_nick_name, NULL);
	efunc_init_function(EFUNC_DO_REMOTE_NICK_NAME, do_remote_nick_name, NULL);
	efunc_init_function(EFUNC_CHARSYS_GET_CURRENT_LANGUAGES, charsys_get_current_languages, NULL);
	efunc_init_function(EFUNC_BROADCAST_SINFO, broadcast_sinfo, NULL);
	efunc_init_function(EFUNC_CONNECT_SERVER, connect_server, NULL);
	efunc_init_function(EFUNC_PARSE_MESSAGE_TAGS, parse_message_tags, &parse_message_tags_default_handler);
	efunc_init_function(EFUNC_MTAGS_TO_STRING, mtags_to_string, &mtags_to_string_default_handler);
	efunc_init_function(EFUNC_TKL_CHARTOTYPE, tkl_chartotype, NULL);
	efunc_init_function(EFUNC_TKL_TYPE_STRING, tkl_type_string, NULL);
	efunc_init_function(EFUNC_TKL_TYPE_CONFIG_STRING, tkl_type_config_string, NULL);
	efunc_init_function(EFUNC_CAN_SEND_TO_CHANNEL, can_send_to_channel, NULL);
	efunc_init_function(EFUNC_BROADCAST_MD_GLOBALVAR, broadcast_md_globalvar, NULL);
	efunc_init_function(EFUNC_BROADCAST_MD_GLOBALVAR_CMD, broadcast_md_globalvar_cmd, NULL);
	efunc_init_function(EFUNC_TKL_IP_HASH, tkl_ip_hash, NULL);
	efunc_init_function(EFUNC_TKL_IP_HASH_TYPE, tkl_ip_hash_type, NULL);
	efunc_init_function(EFUNC_TKL_ADD_NAMEBAN, tkl_add_nameban, NULL);
	efunc_init_function(EFUNC_TKL_ADD_SPAMFILTER, tkl_add_spamfilter, NULL);
	efunc_init_function(EFUNC_SENDNOTICE_TKL_ADD, sendnotice_tkl_add, NULL);
	efunc_init_function(EFUNC_SENDNOTICE_TKL_DEL, sendnotice_tkl_del, NULL);
	efunc_init_function(EFUNC_FREE_TKL, free_tkl, NULL);
	efunc_init_function(EFUNC_FIND_TKL_SERVERBAN, find_tkl_serverban, NULL);
	efunc_init_function(EFUNC_FIND_TKL_BANEXCEPTION, find_tkl_banexception, NULL);
	efunc_init_function(EFUNC_FIND_TKL_NAMEBAN, find_tkl_nameban, NULL);
	efunc_init_function(EFUNC_FIND_TKL_SPAMFILTER, find_tkl_spamfilter, NULL);
	efunc_init_function(EFUNC_FIND_TKL_EXCEPTION, find_tkl_exception, NULL);
	efunc_init_function(EFUNC_ADD_SILENCE, add_silence, add_silence_default_handler);
	efunc_init_function(EFUNC_DEL_SILENCE, del_silence, del_silence_default_handler);
	efunc_init_function(EFUNC_IS_SILENCED, is_silenced, is_silenced_default_handler);
	efunc_init_function(EFUNC_LABELED_RESPONSE_SAVE_CONTEXT, labeled_response_save_context, labeled_response_save_context_default_handler);
	efunc_init_function(EFUNC_LABELED_RESPONSE_SET_CONTEXT, labeled_response_set_context, labeled_response_set_context_default_handler);
	efunc_init_function(EFUNC_LABELED_RESPONSE_FORCE_END, labeled_response_force_end, labeled_response_force_end_default_handler);
	efunc_init_function(EFUNC_KICK_USER, kick_user, NULL);
	efunc_init_function(EFUNC_WATCH_ADD, watch_add, NULL);
	efunc_init_function(EFUNC_WATCH_DEL, watch_del, NULL);
	efunc_init_function(EFUNC_WATCH_DEL_LIST, watch_del_list, NULL);
	efunc_init_function(EFUNC_WATCH_GET, watch_get, NULL);
	efunc_init_function(EFUNC_WATCH_CHECK, watch_check, NULL);
	efunc_init_function(EFUNC_TKL_UHOST, tkl_uhost, NULL);
	efunc_init_function(EFUNC_DO_UNREAL_LOG_REMOTE_DELIVER, do_unreal_log_remote_deliver, do_unreal_log_remote_deliver_default_handler);
	efunc_init_function(EFUNC_GET_CHMODES_FOR_USER, get_chmodes_for_user, NULL);
	efunc_init_function(EFUNC_WHOIS_GET_POLICY, whois_get_policy, NULL);
	efunc_init_function(EFUNC_MAKE_OPER, make_oper, make_oper_default_handler);
}
