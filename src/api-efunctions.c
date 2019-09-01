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
int (*do_join)(aClient *cptr, aClient *sptr, int parc, char *parv[]);
void (*join_channel)(aChannel *chptr, aClient *cptr, aClient *sptr, MessageTag *mtags, int flags);
int (*can_join)(aClient *cptr, aClient *sptr, aChannel *chptr, char *key, char *parv[]);
void (*do_mode)(aChannel *chptr, aClient *cptr, aClient *sptr, MessageTag *mtags, int parc, char *parv[], time_t sendts, int samode);
void (*set_mode)(aChannel *chptr, aClient *cptr, int parc, char *parv[], u_int *pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], int bounce);
int (*m_umode)(aClient *cptr, aClient *sptr, MessageTag *mtags, int parc, char *parv[]);
int (*register_user)(aClient *cptr, aClient *sptr, char *nick, char *username, char *umode, char *virthost, char *ip);
int (*tkl_hash)(unsigned int c);
char (*tkl_typetochar)(int type);
int (*tkl_chartotype)(char c);
char *(*tkl_type_string)(aTKline *tk);
aTKline *(*tkl_add_serverban)(int type, char *usermask, char *hostmask, char *reason, char *setby,
                              time_t expire_at, time_t set_at, int soft, int flags);
aTKline *(*tkl_add_nameban)(int type, char *name, int hold, char *reason, char *setby,
                            time_t expire_at, time_t set_at, int flags);
aTKline *(*tkl_add_spamfilter)(int type, unsigned short target, unsigned short action, aMatch *match, char *setby,
                               time_t expire_at, time_t set_at,
                               time_t spamf_tkl_duration, char *spamf_tkl_reason,
                               int flags);
aTKline *(*tkl_del_line)(aTKline *tkl);
void (*tkl_check_local_remove_shun)(aTKline *tmp);
int (*find_tkline_match)(aClient *cptr, int skip_soft);
int (*find_shun)(aClient *cptr);
int(*find_spamfilter_user)(aClient *sptr, int flags);
aTKline *(*find_qline)(aClient *cptr, char *nick, int *ishold);
aTKline *(*find_tkline_match_zap)(aClient *cptr);
void (*tkl_stats)(aClient *cptr, int type, char *para);
void (*tkl_synch)(aClient *sptr);
int (*m_tkl)(aClient *cptr, aClient *sptr, MessageTag *mtags, int parc, char *parv[]);
int (*place_host_ban)(aClient *sptr, int action, char *reason, long duration);
int (*run_spamfilter)(aClient *sptr, char *str_in, int type, char *target, int flags, aTKline **rettk);
int (*join_viruschan)(aClient *sptr, aTKline *tk, int type);
void (*send_list)(aClient *cptr);
unsigned char *(*StripColors)(unsigned char *text);
const char *(*StripControlCodes)(unsigned char *text);
void (*spamfilter_build_user_string)(char *buf, char *nick, aClient *acptr);
int (*is_silenced)(aClient *sptr, aClient *acptr);
void (*send_protoctl_servers)(aClient *sptr, int response);
int (*verify_link)(aClient *cptr, aClient *sptr, char *servername, ConfigItem_link **link_out);
void (*introduce_user)(aClient *to, aClient *acptr);
void (*send_server_message)(aClient *sptr);
void (*broadcast_md_client)(ModDataInfo *mdi, aClient *acptr, ModData *md);
void (*broadcast_md_channel)(ModDataInfo *mdi, aChannel *chptr, ModData *md);
void (*broadcast_md_member)(ModDataInfo *mdi, aChannel *chptr, Member *m, ModData *md);
void (*broadcast_md_membership)(ModDataInfo *mdi, aClient *acptr, Membership *m, ModData *md);
int (*check_banned)(aClient *cptr, int exitflags);
int (*check_deny_version)(aClient *cptr, char *software, int protocol, char *flags);
void (*broadcast_md_client_cmd)(aClient *except, aClient *sender, aClient *acptr, char *varname, char *value);
void (*broadcast_md_channel_cmd)(aClient *except, aClient *sender, aChannel *chptr, char *varname, char *value);
void (*broadcast_md_member_cmd)(aClient *except, aClient *sender, aChannel *chptr, aClient *acptr, char *varname, char *value);
void (*broadcast_md_membership_cmd)(aClient *except, aClient *sender, aClient *acptr, aChannel *chptr, char *varname, char *value);
void (*send_moddata_client)(aClient *srv, aClient *acptr);
void (*send_moddata_channel)(aClient *srv, aChannel *chptr);
void (*send_moddata_members)(aClient *srv);
void (*broadcast_moddata_client)(aClient *acptr);
int (*match_user)(char *rmask, aClient *acptr, int options);
void (*userhost_changed)(aClient *sptr);
void (*userhost_save_current)(aClient *sptr);
void (*send_join_to_local_users)(aClient *sptr, aChannel *chptr, MessageTag *mtags);
int (*do_nick_name)(char *nick);
int (*do_remote_nick_name)(char *nick);
char *(*charsys_get_current_languages)(void);
void (*broadcast_sinfo)(aClient *acptr, aClient *to, aClient *except);
void (*parse_message_tags)(aClient *cptr, char **str, MessageTag **mtag_list);
extern void parse_message_tags_default_handler(aClient *cptr, char **str, MessageTag **mtag_list);
char *(*mtags_to_string)(MessageTag *m, aClient *acptr);
extern char *mtags_to_string_default_handler(MessageTag *m, aClient *acptr);
int (*can_send)(aClient *cptr, aChannel *chptr, char **msgtext, char **errmsg, int notice);
void (*broadcast_md_globalvar)(ModDataInfo *mdi, ModData *md);
void (*broadcast_md_globalvar_cmd)(aClient *except, aClient *sender, char *varname, char *value);
int (*tkl_ip_hash)(char *ip);
int (*tkl_ip_hash_type)(int type);
void (*sendnotice_tkl_del)(char *removed_by, aTKline *tkl);
void (*sendnotice_tkl_add)(aTKline *tkl);
void (*free_tkl)(aTKline *tkl);
aTKline *(*find_tkl_serverban)(int type, char *usermask, char *hostmask, int softban);
aTKline *(*find_tkl_nameban)(int type, char *name, int hold);
aTKline *(*find_tkl_spamfilter)(int type, char *match_string, unsigned short action, unsigned short target);

Efunction *EfunctionAddMain(Module *module, EfunctionType eftype, int (*func)(), void (*vfunc)(), void *(*pvfunc)(), char *(*cfunc)())
{
	Efunction *p;

	if (!module || !(module->options & MOD_OPT_OFFICIAL))
	{
		if (module)
			module->errorcode = MODERR_INVALID;
		return NULL;
	}
	
	p = MyMallocEx(sizeof(Efunction));
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
		ModuleObject *cbobj = MyMallocEx(sizeof(ModuleObject));
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
						MyFree(cbobj);
						break;
					}
				}
			}
			MyFree(p);
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
	efunction_table[what].name = strdup(name);
	efunction_table[what].funcptr = func;
	efunction_table[what].deffunc = default_func;
}

void efunctions_init(void)
{
	efunc_init_function(EFUNC_DO_JOIN, do_join, NULL);
	efunc_init_function(EFUNC_JOIN_CHANNEL, join_channel, NULL);
	efunc_init_function(EFUNC_CAN_JOIN, can_join, NULL);
	efunc_init_function(EFUNC_DO_MODE, do_mode, NULL);
	efunc_init_function(EFUNC_SET_MODE, set_mode, NULL);
	efunc_init_function(EFUNC_M_UMODE, m_umode, NULL);
	efunc_init_function(EFUNC_REGISTER_USER, register_user, NULL);
	efunc_init_function(EFUNC_TKL_HASH, tkl_hash, NULL);
	efunc_init_function(EFUNC_TKL_TYPETOCHAR, tkl_typetochar, NULL);
	efunc_init_function(EFUNC_TKL_ADD_SERVERBAN, tkl_add_serverban, NULL);
	efunc_init_function(EFUNC_TKL_DEL_LINE, tkl_del_line, NULL);
	efunc_init_function(EFUNC_TKL_CHECK_LOCAL_REMOVE_SHUN, tkl_check_local_remove_shun, NULL);
	efunc_init_function(EFUNC_FIND_TKLINE_MATCH, find_tkline_match, NULL);
	efunc_init_function(EFUNC_FIND_SHUN, find_shun, NULL);
	efunc_init_function(EFUNC_FIND_SPAMFILTER_USER, find_spamfilter_user, NULL);
	efunc_init_function(EFUNC_FIND_QLINE, find_qline, NULL);
	efunc_init_function(EFUNC_FIND_TKLINE_MATCH_ZAP, find_tkline_match_zap, NULL);
	efunc_init_function(EFUNC_TKL_STATS, tkl_stats, NULL);
	efunc_init_function(EFUNC_TKL_SYNCH, tkl_synch, NULL);
	efunc_init_function(EFUNC_M_TKL, m_tkl, NULL);
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
	efunc_init_function(EFUNC_FIND_TKL_NAMEBAN, find_tkl_nameban, NULL);
	efunc_init_function(EFUNC_FIND_TKL_SPAMFILTER, find_tkl_spamfilter, NULL);
}
