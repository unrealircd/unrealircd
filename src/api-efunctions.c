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
int (*do_join)(Client *cptr, Client *sptr, int parc, char *parv[]);
void (*join_channel)(Channel *chptr, Client *cptr, Client *sptr, MessageTag *mtags, int flags);
int (*can_join)(Client *cptr, Client *sptr, Channel *chptr, char *key, char *parv[]);
void (*do_mode)(Channel *chptr, Client *cptr, Client *sptr, MessageTag *mtags, int parc, char *parv[], time_t sendts, int samode);
void (*set_mode)(Channel *chptr, Client *cptr, int parc, char *parv[], u_int *pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], int bounce);
int (*cmd_umode)(Client *cptr, Client *sptr, MessageTag *mtags, int parc, char *parv[]);
int (*register_user)(Client *cptr, Client *sptr, char *nick, char *username, char *umode, char *virthost, char *ip);
int (*tkl_hash)(unsigned int c);
char (*tkl_typetochar)(int type);
int (*tkl_chartotype)(char c);
char *(*tkl_type_string)(TKL *tk);
TKL *(*tkl_add_serverban)(int type, char *usermask, char *hostmask, char *reason, char *setby,
                              time_t expire_at, time_t set_at, int soft, int flags);
TKL *(*tkl_add_nameban)(int type, char *name, int hold, char *reason, char *setby,
                            time_t expire_at, time_t set_at, int flags);
TKL *(*tkl_add_spamfilter)(int type, unsigned short target, unsigned short action, Match *match, char *setby,
                               time_t expire_at, time_t set_at,
                               time_t spamf_tkl_duration, char *spamf_tkl_reason,
                               int flags);
TKL *(*tkl_add_banexception)(int type, char *usermask, char *hostmask, char *reason, char *set_by,
                                time_t expire_at, time_t set_at, int soft, char *bantypes, int flags);
TKL *(*tkl_del_line)(TKL *tkl);
void (*tkl_check_local_remove_shun)(TKL *tmp);
int (*find_tkline_match)(Client *cptr, int skip_soft);
int (*find_shun)(Client *cptr);
int(*find_spamfilter_user)(Client *sptr, int flags);
TKL *(*find_qline)(Client *cptr, char *nick, int *ishold);
TKL *(*find_tkline_match_zap)(Client *cptr);
void (*tkl_stats)(Client *cptr, int type, char *para);
void (*tkl_synch)(Client *sptr);
int (*cmd_tkl)(Client *cptr, Client *sptr, MessageTag *mtags, int parc, char *parv[]);
int (*place_host_ban)(Client *sptr, BanAction action, char *reason, long duration);
int (*run_spamfilter)(Client *sptr, char *str_in, int type, char *target, int flags, TKL **rettk);
int (*join_viruschan)(Client *sptr, TKL *tk, int type);
void (*send_list)(Client *cptr);
unsigned char *(*StripColors)(unsigned char *text);
const char *(*StripControlCodes)(unsigned char *text);
void (*spamfilter_build_user_string)(char *buf, char *nick, Client *acptr);
int (*is_silenced)(Client *sptr, Client *acptr);
void (*send_protoctl_servers)(Client *sptr, int response);
int (*verify_link)(Client *cptr, Client *sptr, char *servername, ConfigItem_link **link_out);
void (*introduce_user)(Client *to, Client *acptr);
void (*send_server_message)(Client *sptr);
void (*broadcast_md_client)(ModDataInfo *mdi, Client *acptr, ModData *md);
void (*broadcast_md_channel)(ModDataInfo *mdi, Channel *chptr, ModData *md);
void (*broadcast_md_member)(ModDataInfo *mdi, Channel *chptr, Member *m, ModData *md);
void (*broadcast_md_membership)(ModDataInfo *mdi, Client *acptr, Membership *m, ModData *md);
int (*check_banned)(Client *cptr, int exitflags);
int (*check_deny_version)(Client *cptr, char *software, int protocol, char *flags);
void (*broadcast_md_client_cmd)(Client *except, Client *sender, Client *acptr, char *varname, char *value);
void (*broadcast_md_channel_cmd)(Client *except, Client *sender, Channel *chptr, char *varname, char *value);
void (*broadcast_md_member_cmd)(Client *except, Client *sender, Channel *chptr, Client *acptr, char *varname, char *value);
void (*broadcast_md_membership_cmd)(Client *except, Client *sender, Client *acptr, Channel *chptr, char *varname, char *value);
void (*send_moddata_client)(Client *srv, Client *acptr);
void (*send_moddata_channel)(Client *srv, Channel *chptr);
void (*send_moddata_members)(Client *srv);
void (*broadcast_moddata_client)(Client *acptr);
int (*match_user)(char *rmask, Client *acptr, int options);
void (*userhost_changed)(Client *sptr);
void (*userhost_save_current)(Client *sptr);
void (*send_join_to_local_users)(Client *sptr, Channel *chptr, MessageTag *mtags);
int (*do_nick_name)(char *nick);
int (*do_remote_nick_name)(char *nick);
char *(*charsys_get_current_languages)(void);
void (*broadcast_sinfo)(Client *acptr, Client *to, Client *except);
void (*parse_message_tags)(Client *cptr, char **str, MessageTag **mtag_list);
extern void parse_message_tags_default_handler(Client *cptr, char **str, MessageTag **mtag_list);
char *(*mtags_to_string)(MessageTag *m, Client *acptr);
extern char *mtags_to_string_default_handler(MessageTag *m, Client *acptr);
int (*can_send)(Client *cptr, Channel *chptr, char **msgtext, char **errmsg, int notice);
void (*broadcast_md_globalvar)(ModDataInfo *mdi, ModData *md);
void (*broadcast_md_globalvar_cmd)(Client *except, Client *sender, char *varname, char *value);
int (*tkl_ip_hash)(char *ip);
int (*tkl_ip_hash_type)(int type);
void (*sendnotice_tkl_del)(char *removed_by, TKL *tkl);
void (*sendnotice_tkl_add)(TKL *tkl);
void (*free_tkl)(TKL *tkl);
TKL *(*find_tkl_serverban)(int type, char *usermask, char *hostmask, int softban);
TKL *(*find_tkl_banexception)(int type, char *usermask, char *hostmask, int softban);
TKL *(*find_tkl_nameban)(int type, char *name, int hold);
TKL *(*find_tkl_spamfilter)(int type, char *match_string, unsigned short action, unsigned short target);
int (*find_tkl_exception)(int ban_type, Client *cptr);

Efunction *EfunctionAddMain(Module *module, EfunctionType eftype, int (*func)(), void (*vfunc)(), void *(*pvfunc)(), char *(*cfunc)())
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
	if (cfunc)
		p->func.pcharfunc = cfunc;
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
				ircd_log(LOG_ERROR, "[BUG] efunctions_switchover(): someone forgot to initialize the function table for efunc %d", i);
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

#define efunc_init_function(what, func, default_func) efunc_init_function_(what, #func, (void *)&func, default_func)

void efunc_init_function_(EfunctionType what, char *name, void *func, void *default_func)
{
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
	efunc_init_function(EFUNC_TKL_SYNCH, tkl_synch, NULL);
	efunc_init_function(EFUNC_CMD_TKL, cmd_tkl, NULL);
	efunc_init_function(EFUNC_PLACE_HOST_BAN, place_host_ban, NULL);
	efunc_init_function(EFUNC_DOSPAMFILTER, run_spamfilter, NULL);
	efunc_init_function(EFUNC_DOSPAMFILTER_VIRUSCHAN, join_viruschan, NULL);
	efunc_init_function(EFUNC_SEND_LIST, send_list, NULL);
	efunc_init_function(EFUNC_STRIPCOLORS, StripColors, NULL);
	efunc_init_function(EFUNC_STRIPCONTROLCODES, StripControlCodes, NULL);
	efunc_init_function(EFUNC_SPAMFILTER_BUILD_USER_STRING, spamfilter_build_user_string, NULL);
	efunc_init_function(EFUNC_IS_SILENCED, is_silenced, NULL);
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
	efunc_init_function(EFUNC_PARSE_MESSAGE_TAGS, parse_message_tags, &parse_message_tags_default_handler);
	efunc_init_function(EFUNC_MTAGS_TO_STRING, mtags_to_string, &mtags_to_string_default_handler);
	efunc_init_function(EFUNC_TKL_CHARTOTYPE, tkl_chartotype, NULL);
	efunc_init_function(EFUNC_TKL_TYPE_STRING, tkl_type_string, NULL);
	efunc_init_function(EFUNC_CAN_SEND, can_send, NULL);
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
}
