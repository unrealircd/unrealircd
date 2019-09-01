/*
 * Unreal Internet Relay Chat Daemon, src/modules/m_tkl.c
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

/* Forward declarations */
int tkl_config_test_spamfilter(ConfigFile *, ConfigEntry *, int, int *);
int tkl_config_run_spamfilter(ConfigFile *, ConfigEntry *, int);
int tkl_config_test_ban(ConfigFile *, ConfigEntry *, int, int *);
int tkl_config_run_ban(ConfigFile *, ConfigEntry *, int);
int tkl_config_test_except(ConfigFile *, ConfigEntry *, int, int *);
int tkl_config_run_except(ConfigFile *, ConfigEntry *, int);
CMD_FUNC(m_gline);
CMD_FUNC(m_shun);
CMD_FUNC(m_tempshun);
CMD_FUNC(m_gzline);
CMD_FUNC(m_kline);
CMD_FUNC(m_zline);
CMD_FUNC(m_spamfilter);
CMD_FUNC(m_eline);
int m_tkl_line(aClient *cptr, aClient *sptr, int parc, char *parv[], char* type);
int _tkl_hash(unsigned int c);
char _tkl_typetochar(int type);
int _tkl_chartotype(char c);
char *_tkl_type_string(aTKline *tk);
aTKline *_tkl_add_serverban(int type, char *usermask, char *hostmask, char *reason, char *set_by,
                            time_t expire_at, time_t set_at, int soft, int flags);
aTKline *_tkl_add_banexception(int type, char *usermask, char *hostmask, char *reason, char *set_by,
                               time_t expire_at, time_t set_at, int soft, char *bantypes, int flags);
aTKline *_tkl_add_nameban(int type, char *name, int hold, char *reason, char *set_by,
                          time_t expire_at, time_t set_at, int flags);
aTKline *_tkl_add_spamfilter(int type, unsigned short target, unsigned short action, aMatch *match, char *set_by,
                             time_t expire_at, time_t set_at,
                             time_t spamf_tkl_duration, char *spamf_tkl_reason,
                             int flags);
void _sendnotice_tkl_del(char *removed_by, aTKline *tkl);
void _sendnotice_tkl_add(aTKline *tkl);
void _free_tkl(aTKline *tkl);
void _tkl_del_line(aTKline *tkl);
static void _tkl_check_local_remove_shun(aTKline *tmp);
void tkl_expire_entry(aTKline * tmp);
EVENT(tkl_check_expire);
int _find_tkline_match(aClient *cptr, int skip_soft);
int _find_shun(aClient *cptr);
int _find_spamfilter_user(aClient *sptr, int flags);
aTKline *_find_qline(aClient *cptr, char *nick, int *ishold);
aTKline *_find_tkline_match_zap(aClient *cptr);
void _tkl_stats(aClient *cptr, int type, char *para);
void _tkl_synch(aClient *sptr);
CMD_FUNC(_m_tkl);
int _place_host_ban(aClient *sptr, int action, char *reason, long duration);
int _run_spamfilter(aClient *sptr, char *str_in, int type, char *target, int flags, aTKline **rettk);
int _join_viruschan(aClient *sptr, aTKline *tk, int type);
void _spamfilter_build_user_string(char *buf, char *nick, aClient *acptr);
int _match_user(char *rmask, aClient *acptr, int options);
int _tkl_ip_hash(char *ip);
int _tkl_ip_hash_type(int type);
aTKline *_find_tkl_serverban(int type, char *usermask, char *hostmask, int softban);
aTKline *_find_tkl_banexception(int type, char *usermask, char *hostmask, int softban);
aTKline *_find_tkl_nameban(int type, char *name, int hold);
aTKline *_find_tkl_spamfilter(int type, char *match_string, unsigned short action, unsigned short target);
int _find_tkl_exception(int ban_type, aClient *cptr);

/* Externals (only for us :D) */
extern int MODVAR spamf_ugly_vchanoverride;

ModuleHeader MOD_HEADER(tkl)
= {
	"tkl",
	"5.0",
	"Server ban commands such as /GLINE, /SPAMFILTER, etc.",
	"UnrealIRCd Team",
	"unrealircd-5",
};

MOD_TEST(tkl)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkl_config_test_spamfilter);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkl_config_test_ban);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkl_config_test_except);
	EfunctionAdd(modinfo->handle, EFUNC_TKL_HASH, _tkl_hash);
	EfunctionAdd(modinfo->handle, EFUNC_TKL_TYPETOCHAR, TO_INTFUNC(_tkl_typetochar));
	EfunctionAdd(modinfo->handle, EFUNC_TKL_CHARTOTYPE, TO_INTFUNC(_tkl_chartotype));
	EfunctionAddPChar(modinfo->handle, EFUNC_TKL_TYPE_STRING, _tkl_type_string);
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
	EfunctionAddVoid(modinfo->handle, EFUNC_TKL_SYNCH, _tkl_synch);
	EfunctionAdd(modinfo->handle, EFUNC_M_TKL, _m_tkl);
	EfunctionAdd(modinfo->handle, EFUNC_PLACE_HOST_BAN, _place_host_ban);
	EfunctionAdd(modinfo->handle, EFUNC_DOSPAMFILTER, _run_spamfilter);
	EfunctionAdd(modinfo->handle, EFUNC_DOSPAMFILTER_VIRUSCHAN, _join_viruschan);
	EfunctionAddVoid(modinfo->handle, EFUNC_SPAMFILTER_BUILD_USER_STRING, _spamfilter_build_user_string);
	EfunctionAdd(modinfo->handle, EFUNC_MATCH_USER, _match_user);
	EfunctionAdd(modinfo->handle, EFUNC_TKL_IP_HASH, _tkl_ip_hash);
	EfunctionAdd(modinfo->handle, EFUNC_TKL_IP_HASH_TYPE, _tkl_ip_hash_type);
	EfunctionAddVoid(modinfo->handle, EFUNC_SENDNOTICE_TKL_ADD, _sendnotice_tkl_add);
	EfunctionAddVoid(modinfo->handle, EFUNC_SENDNOTICE_TKL_DEL, _sendnotice_tkl_del);
	EfunctionAdd(modinfo->handle, EFUNC_FIND_TKL_EXCEPTION, _find_tkl_exception);
	return MOD_SUCCESS;
}

MOD_INIT(tkl)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkl_config_run_spamfilter);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkl_config_run_ban);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkl_config_run_except);
	CommandAdd(modinfo->handle, "GLINE", m_gline, 3, M_OPER);
	CommandAdd(modinfo->handle, "SHUN", m_shun, 3, M_OPER);
	CommandAdd(modinfo->handle, "TEMPSHUN", m_tempshun, 2, M_OPER);
	CommandAdd(modinfo->handle, "ZLINE", m_zline, 3, M_OPER);
	CommandAdd(modinfo->handle, "KLINE", m_kline, 3, M_OPER);
	CommandAdd(modinfo->handle, "GZLINE", m_gzline, 3, M_OPER);
	CommandAdd(modinfo->handle, "SPAMFILTER", m_spamfilter, 7, M_OPER);
	CommandAdd(modinfo->handle, "ELINE", m_eline, 4, M_OPER);
	CommandAdd(modinfo->handle, "TKL", _m_tkl, MAXPARA, M_OPER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(tkl)
{
	EventAdd(modinfo->handle, "tklexpire", 5, 0, tkl_check_expire, NULL);
	return MOD_SUCCESS;
}

MOD_UNLOAD(tkl)
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
	if ((type != CONFIG_MAIN) || strcmp(ce->ce_varname, "spamfilter"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "target"))
		{
			if (has_target)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "spamfilter::target");
				continue;
			}
			has_target = 1;
			if (cep->ce_vardata)
			{
				if (!spamfilter_getconftargets(cep->ce_vardata))
				{
					config_error("%s:%i: unknown spamfiler target type '%s'",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
					errors++;
				}
			}
			else if (cep->ce_entries)
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (!spamfilter_getconftargets(cepp->ce_varname))
					{
						config_error("%s:%i: unknown spamfiler target type '%s'",
							cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, cepp->ce_varname);
						errors++;
					}
				}
			}
			else
			{
				config_error_empty(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "spamfilter", cep->ce_varname);
				errors++;
			}
			continue;
		}
		if (!cep->ce_vardata)
		{
			config_error_empty(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"spamfilter", cep->ce_varname);
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "reason"))
		{
			if (has_reason)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "spamfilter::reason");
				continue;
			}
			has_reason = 1;
			reason = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "match"))
		{
			if (has_match)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "spamfilter::match");
				continue;
			}
			has_match = 1;
			match = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "action"))
		{
			if (has_action)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "spamfilter::action");
				continue;
			}
			has_action = 1;
			if (!banact_stringtoval(cep->ce_vardata))
			{
				config_error("%s:%i: spamfilter::action has unknown action type '%s'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "ban-time"))
		{
			if (has_bantime)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "spamfilter::ban-time");
				continue;
			}
			has_bantime = 1;
		}
		else if (!strcmp(cep->ce_varname, "match-type"))
		{
			if (has_match_type)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "spamfilter::match-type");
				continue;
			}
			if (!strcasecmp(cep->ce_vardata, "posix"))
			{
				config_error("%s:%i: this spamfilter uses match-type 'posix' which is no longer supported. "
				             "You must switch over to match-type 'regex' instead. "
				             "See https://www.unrealircd.org/docs/FAQ#spamfilter-posix-deprecated",
				             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
				errors++;
				*errs = errors;
				return -1; /* return now, otherwise there will be issues */
			}
			match_type = unreal_match_method_strtoval(cep->ce_vardata);
			if (match_type == 0)
			{
				config_error("%s:%i: spamfilter::match-type: unknown match type '%s', "
				             "should be one of: 'simple', 'regex' or 'posix'",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				             cep->ce_vardata);
				errors++;
				continue;
			}
			has_match_type = 1;
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"spamfilter", cep->ce_varname);
			errors++;
			continue;
		}
	}

	if (match && match_type)
	{
		aMatch *m;
		char *err;

		m = unreal_create_match(match_type, match, &err);
		if (!m)
		{
			config_error("%s:%i: spamfilter::match contains an invalid regex: %s",
				ce->ce_fileptr->cf_filename,
				ce->ce_varlinenum,
				err);
			errors++;
		} else
		{
			unreal_delete_match(m);
		}
	}

	if (!has_match)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"spamfilter::match");
		errors++;
	}
	if (!has_target)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"spamfilter::target");
		errors++;
	}
	if (!has_action)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"spamfilter::action");
		errors++;
	}
	if (match && reason && (strlen(match) + strlen(reason) > 505))
	{
		config_error("%s:%i: spamfilter block problem: match + reason field are together over 505 bytes, "
		             "please choose a shorter regex or reason",
		             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	if (!has_match_type)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"spamfilter::match-type");
		errors++;
	}

	if (!has_match_type && !has_match && has_action && has_target)
	{
		need_34_upgrade = 1;
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
	aTKline *tk;
	char *word = NULL, *reason = NULL;
	time_t bantime = (SPAMFILTER_BAN_TIME ? SPAMFILTER_BAN_TIME : 86400);
	char *banreason = "<internally added by ircd>";
	int action = 0, target = 0;
	int match_type = 0;
	aMatch *m;

	/* We are only interested in spamfilter { } blocks */
	if ((type != CONFIG_MAIN) || strcmp(ce->ce_varname, "spamfilter"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "match"))
		{
			word = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "target"))
		{
			if (cep->ce_vardata)
				target = spamfilter_getconftargets(cep->ce_vardata);
			else
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
					target |= spamfilter_getconftargets(cepp->ce_varname);
			}
		}
		else if (!strcmp(cep->ce_varname, "action"))
		{
			action = banact_stringtoval(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "reason"))
		{
			reason = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "ban-time"))
		{
			bantime = config_checkval(cep->ce_vardata, CFG_TIME);
		}
		else if (!strcmp(cep->ce_varname, "match-type"))
		{
			match_type = unreal_match_method_strtoval(cep->ce_vardata);
		}
	}

	m = unreal_create_match(match_type, word, NULL);
	tkl_add_spamfilter(TKL_SPAMF,
	                    target,
	                    action,
	                    m,
	                    me.name,
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

	if (strcmp(ce->ce_vardata, "nick") && strcmp(ce->ce_vardata, "user") &&
	    strcmp(ce->ce_vardata, "ip"))
	{
		return 0;
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (config_is_blankorempty(cep, "ban"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "mask"))
		{
			if (has_mask)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "ban::mask");
				continue;
			}
			has_mask = 1;
		}
		else if (!strcmp(cep->ce_varname, "reason"))
		{
			if (has_reason)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "ban::reason");
				continue;
			}
			has_reason = 1;
		}
		else
		{
			config_error("%s:%i: unknown directive ban %s::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				ce->ce_vardata,
				cep->ce_varname);
			errors++;
		}
	}

	if (!has_mask)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"ban::mask");
		errors++;
	}

	if (!has_reason)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
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

	if (strcmp(ce->ce_vardata, "nick") && strcmp(ce->ce_vardata, "user") &&
	    strcmp(ce->ce_vardata, "ip"))
	{
		return 0;
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask"))
		{
			char buf[512], *p;
			strlcpy(buf, cep->ce_vardata, sizeof(buf));
			p = strchr(buf, '@');
			if (p)
			{
				*p++ = '\0';
				usermask = strdup(buf);
				hostmask = strdup(p);
			} else {
				hostmask = strdup(cep->ce_vardata);
			}
		} else
		if (!strcmp(cep->ce_varname, "reason"))
		{
			reason = strdup(cep->ce_vardata);
		}
	}

	if (!usermask)
		usermask = strdup("*");

	if (!reason)
		reason = strdup("-");

	if (!strcmp(ce->ce_vardata, "nick"))
		tkltype = TKL_NAME;
	else if (!strcmp(ce->ce_vardata, "user"))
		tkltype = TKL_KILL;
	else if (!strcmp(ce->ce_vardata, "ip"))
		tkltype = TKL_ZAP;
	else
		abort(); /* impossible */

	if (TKLIsNameBanType(tkltype))
		tkl_add_nameban(tkltype, hostmask, 0, reason, "-config-", 0, TStime(), TKL_FLAG_CONFIG);
	else if (TKLIsServerBanType(tkltype))
		tkl_add_serverban(tkltype, usermask, hostmask, reason, "-config-", 0, TStime(), 0, TKL_FLAG_CONFIG);

	safefree(usermask);
	safefree(hostmask);
	safefree(reason);
	return 1;
}

int tkl_config_test_except(ConfigFile *cf, ConfigEntry *ce, int configtype, int *errs)
{
	ConfigEntry *cep;
	ConfigItem_except *ca;
	Hook *h;
	char *bantypes = NULL;
	int errors = 0;

	/* We are only interested in except { } blocks */
	if (configtype != CONFIG_EXCEPT)
		return 0;

	/* These are the types that we handle */
	if (strcmp(ce->ce_vardata, "ban") && strcmp(ce->ce_vardata, "throttle") &&
	    strcmp(ce->ce_vardata, "tkl") && strcmp(ce->ce_vardata, "blacklist") &&
	    strcmp(ce->ce_vardata, "spamfilter"))
	{
		return 0;
	}

	if (!strcmp(ce->ce_vardata, "tkl"))
	{
		config_error("%s:%d: except tkl { } has been renamed to except ban { }",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		config_status("Please rename your block in the configuration file.");
		*errs = 1;
		return -1;
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask"))
		{
			// TODO: verify mask, can be both a list or just 1 directly
		} else
		if (!strcmp(cep->ce_varname, "type"))
		{
			// TODO: verify type, can be both a list or just 1 directly
		} else {
			config_error_unknown(cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, "except", cep->ce_varname);
			errors++;
			continue;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

void config_create_tkl_except(char *mask, char *bantypes)
{
	char *usermask = NULL;
	char *hostmask = NULL;
	int soft = 0;
	char buf[256], *p;

	if (*mask == '%')
	{
		soft = 1;
		mask++;
	}
	strlcpy(buf, mask, sizeof(buf));
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

	if ((*usermask == ':') || (*hostmask == ':'))
	{
		config_error("Cannot add illegal ban '%s': for a given user@host neither"
		             "user nor host may start with a : character (semicolon)", mask);
		return;
	}

	tkl_add_banexception(TKL_EXCEPTION, usermask, hostmask, "Added in configuration file",
	                     "-config-", 0, TStime(), soft, bantypes, TKL_FLAG_CONFIG);
}

void tkl_config_run_except_add_bantype(char *bantypes, size_t bantypeslen, char *name)
{
	if (!strcmp(name, "kline"))
		strlcat(bantypes, "k", sizeof(bantypes));
	// etc etc.. but probably in a different way ;)
}

int tkl_config_run_except(ConfigFile *cf, ConfigEntry *ce, int configtype)
{
	ConfigEntry *cep, *cepp;
	ConfigItem_except *ca;
	Hook *h;
	char *default_bantypes = NULL;
	char bantypes[64];

	/* We are only interested in except { } blocks */
	if (configtype != CONFIG_EXCEPT)
		return 0;

	/* These are the types that we handle */
	if (strcmp(ce->ce_vardata, "ban") && strcmp(ce->ce_vardata, "throttle") &&
	    strcmp(ce->ce_vardata, "blacklist") &&
	    strcmp(ce->ce_vardata, "spamfilter"))
	{
		return 0;
	}

	*bantypes = '\0';

	/* First configure all the types */
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "type"))
		{
			if (cep->ce_entries)
			{
				/* type { x; y; z; }; */
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
					tkl_config_run_except_add_bantype(bantypes, sizeof(bantypes), cepp->ce_varname);
			} else
			if (cep->ce_vardata)
			{
				/* type x; */
				tkl_config_run_except_add_bantype(bantypes, sizeof(bantypes), cep->ce_vardata);
			}
		}
	}

	if (!*bantypes)
	{
		/* Default setting if no 'type' is specified: */
		if (!strcmp(ce->ce_vardata, "ban"))
			strlcpy(bantypes, "kgzZs", sizeof(bantypes));
		else if (!strcmp(ce->ce_vardata, "throttle"))
			strlcpy(bantypes, "t", sizeof(bantypes));
		else if (!strcmp(ce->ce_vardata, "blacklist"))
			strlcpy(bantypes, "b", sizeof(bantypes));
		else if (!strcmp(ce->ce_vardata, "spamfilter"))
			strlcpy(bantypes, "f", sizeof(bantypes));
		else
			abort(); /* someone can't code */
	}

	/* Now walk through all mask entries */
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask"))
		{
			if (cep->ce_entries)
			{
				/* mask { *@1.1.1.1; *@2.2.2.2; *@3.3.3.3; }; */
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
					config_create_tkl_except(cepp->ce_varname, bantypes);
			} else
			if (cep->ce_vardata)
			{
				/* mask *@1.1.1.1; */
				config_create_tkl_except(cep->ce_vardata, bantypes);
			}
		}
	}

	return 1;
}

/** Return unique spamfilter id for aTKline */
char *spamfilter_id(aTKline *tk)
{
	static char buf[128];

	snprintf(buf, sizeof(buf), "%p", (void *)tk);
	return buf;
}

/** GLINE - Global kline.
** Syntax: /gline [+|-]u@h mask time :reason
**
** parv[1] = [+|-]u@h mask
** parv[2] = for how long
** parv[3] = reason
*/
CMD_FUNC(m_gline)
{
	if (IsServer(sptr))
		return 0;
	if (!ValidatePermissionsForPath("server-ban:gline",sptr,NULL,NULL,NULL))

	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if (parc == 1)
	{
		char *parv[3];
		parv[0] = NULL;
		parv[1] = "gline";
		parv[2] = NULL;
		return do_cmd(sptr, sptr, recv_mtags, "STATS", 2, parv);
	}

	return m_tkl_line(cptr, sptr, parc, parv, "G");
}

/** GZLINE - Global zline.
 */
CMD_FUNC(m_gzline)
{
	if (IsServer(sptr))
		return 0;

	if (!ValidatePermissionsForPath("server-ban:zline:global",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if (parc == 1)
	{
		char *parv[3];
		parv[0] = NULL;
		parv[1] = "gline"; /* (there's no /STATS gzline, it's included in /STATS gline output) */
		parv[2] = NULL;
		return do_cmd(sptr, sptr, recv_mtags, "STATS", 2, parv);
	}

	return m_tkl_line(cptr, sptr, parc, parv, "Z");

}

/** SHUN - Shun a user so it can no longer execute any meaningful commands.
 */
CMD_FUNC(m_shun)
{
	if (IsServer(sptr))
		return 0;

	if (!ValidatePermissionsForPath("server-ban:shun",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if (parc == 1)
	{
		char *parv[3];
		parv[0] = NULL;
		parv[1] = "shun";
		parv[2] = NULL;
		return do_cmd(sptr, sptr, recv_mtags, "STATS", 2, parv);
	}

	return m_tkl_line(cptr, sptr, parc, parv, "s");

}

/** TEMPSHUN - Temporarily shun a user so it can no longer execute
 *  any meaningful commands - until the user disconnects (session only).
 */
CMD_FUNC(m_tempshun)
{
	aClient *acptr;
	char *comment = ((parc > 2) && !BadPtr(parv[2])) ? parv[2] : "no reason";
	char *name;
	int remove = 0;

	if (MyClient(sptr) && (!ValidatePermissionsForPath("server-ban:shun:temporary",sptr,NULL,NULL,NULL)))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}
	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "TEMPSHUN");
		return 0;
	}
	if (parv[1][0] == '+')
		name = parv[1]+1;
	else if (parv[1][0] == '-')
	{
		name = parv[1]+1;
		remove = 1;
	} else
		name = parv[1];

	acptr = find_person(name, NULL);
	if (!acptr)
	{
		sendnumeric(sptr, ERR_NOSUCHNICK, name);
		return 0;
	}
	if (!MyClient(acptr))
	{
		sendto_one(acptr->from, NULL, ":%s TEMPSHUN %s :%s",
			sptr->name, parv[1], comment);
	} else {
		char buf[1024];
		if (!remove)
		{
			if (IsShunned(acptr))
			{
				sendnotice(sptr, "User '%s' already shunned", acptr->name);
			} else if (ValidatePermissionsForPath("immune:server-ban:shun",acptr,NULL,NULL,NULL))
			{
				sendnotice(sptr, "You cannot tempshun '%s' because (s)he is an oper with 'immune:server-ban:shun' privilege", acptr->name);
			} else
			{
				SetShunned(acptr);
				ircsnprintf(buf, sizeof(buf), "Temporary shun added on user %s (%s@%s) by %s [%s]",
					acptr->name, acptr->user->username, acptr->user->realhost,
					sptr->name, comment);
				sendto_snomask_global(SNO_TKL, "%s", buf);
			}
		} else {
			if (!IsShunned(acptr))
			{
				sendnotice(sptr, "User '%s' is not shunned", acptr->name);
			} else {
				ClearShunned(acptr);
				ircsnprintf(buf, sizeof(buf), "Removed temporary shun on user %s (%s@%s) by %s",
					acptr->name, acptr->user->username, acptr->user->realhost,
					sptr->name);
				sendto_snomask_global(SNO_TKL, "%s", buf);
			}
		}
	}
	return 0;
}

/** KLINE - Kill line (ban user from local server)
 */
CMD_FUNC(m_kline)
{
	if (IsServer(sptr))
		return 0;

	if (!ValidatePermissionsForPath("server-ban:kline:local:add",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if (parc == 1)
	{
		char *parv[3];
		parv[0] = NULL;
		parv[1] = "kline";
		parv[2] = NULL;
		return do_cmd(sptr, sptr, recv_mtags, "STATS", 2, parv);
	}

	if (!ValidatePermissionsForPath("server-ban:kline:remove",sptr,NULL,NULL,NULL) && *parv[1] == '-')
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	return m_tkl_line(cptr, sptr, parc, parv, "k");
}

/** Generate stats for '/GLINE -stats' and such */
void tkl_general_stats(aClient *sptr)
{
	int index, index2;
	aTKline *tkl;
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
				sendnotice(sptr, "Slot %d:%d has %d item(s)", index, index2, subtotal);
			total += subtotal;
		}
	}
	sendnotice(sptr, "Hashed TKL items: %d item(s)", total);

	/* Now normal entries.. */
	subtotal = 0;
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
			subtotal++;
	}
	sendnotice(sptr, "Standard TKL items: %d item(s)", subtotal);
	total += subtotal;
	sendnotice(sptr, "Grand total TKL items: %d item(s)", total);
}

/** ZLINE - Kill a user as soon as it tries to connect to the server.
 * This happens before any DNS/ident lookups have been done and
 * before any data has been processed (including no SSL/TLS handshake, etc.)
 */
CMD_FUNC(m_zline)
{
	if (IsServer(sptr))
		return 0;

	if (!ValidatePermissionsForPath("server-ban:zline:local:add",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if (parc == 1)
	{
		char *parv[3];
		parv[0] = NULL;
		parv[1] = "kline"; /* (there's no /STATS zline, it's included in /STATS kline output) */
		parv[2] = NULL;
		return do_cmd(sptr, sptr, recv_mtags, "STATS", 2, parv);
	}

	if ((parc > 1) && !BadPtr(parv[1]) && !strcasecmp(parv[1], "-stats"))
	{
		/* Print some statistics */
		tkl_general_stats(sptr);
		return 0;
	}

	return m_tkl_line(cptr, sptr, parc, parv, "z");

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

/** Intermediate layer between user functions such as KLINE/GLINE
 * and the TKL layer (m_tkl).
 * This allows us doing some syntax checking and other helpful
 * things that are the same for many types of *LINES.
 */
int m_tkl_line(aClient *cptr, aClient *sptr, int parc, char *parv[], char *type)
{
	time_t secs;
	int whattodo = 0;	/* 0 = add  1 = del */
	time_t i;
	aClient *acptr = NULL;
	char *mask = NULL;
	char mo[1024], mo2[1024];
	char *p, *usermask, *hostmask;
	char *tkllayer[10] = {
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

	if (parc == 1)
		return 0; /* shouldn't happen */

	mask = parv[1];
	if (*mask == '-')
	{
		whattodo = 1;
		mask++;
	}
	else if (*mask == '+')
	{
		whattodo = 0;
		mask++;
	}

	if (strchr(mask, '!'))
	{
		sendnotice(sptr, "[error] Cannot have '!' in masks.");
		return 0;
	}
	if (*mask == ':')
	{
		sendnotice(sptr, "[error] Mask cannot start with a ':'.");
		return 0;
	}
	if (strchr(mask, ' '))
		return 0;

	/* Check if it's a softban */
	if (*mask == '%')
	{
		if (!strchr("kGs", *type))
		{
			sendnotice(sptr, "The %% prefix (soft ban) is only available for KLINE, GLINE and SHUN."
			                 "For technical reasons this will not work for (G)ZLINE.");
			return 0;
		}
	}
	/* Check if it's a hostmask and legal .. */
	p = strchr(mask, '@');
	if (p) {
		if ((p == mask) || !p[1])
		{
			sendnotice(sptr, "Error: no user@host specified");
			return 0;
		}
		usermask = strtok(mask, "@");
		hostmask = strtok(NULL, "");
		if (BadPtr(hostmask)) {
			if (BadPtr(usermask)) {
				return 0;
			}
			hostmask = usermask;
			usermask = "*";
		}
		if (*hostmask == ':')
		{
			sendnotice(sptr, "[error] For technical reasons you cannot start the host with a ':', sorry");
			return 0;
		}
		if (((*type == 'z') || (*type == 'Z')) && !whattodo)
		{
			/* It's a (G)ZLINE, make sure the user isn't specyfing a HOST.
			 * Just a warning in 3.2.3, but an error in 3.2.4.
			 */
			if (strcmp(usermask, "*"))
			{
				sendnotice(sptr, "ERROR: (g)zlines must be placed at \037*\037@ipmask, not \037user\037@ipmask. This is "
				                 "because (g)zlines are processed BEFORE dns and ident lookups are done. "
				                 "If you want to use usermasks, use a KLINE/GLINE instead.");
				return -1;
			}
			for (p=hostmask; *p; p++)
				if (isalpha(*p) && !isxdigit(*p))
				{
					sendnotice(sptr, "ERROR: (g)zlines must be placed at *@\037IPMASK\037, not *@\037HOSTMASK\037 "
					                 "(so for example *@192.168.* is ok, but *@*.aol.com is not). "
					                 "This is because (g)zlines are processed BEFORE dns and ident lookups are done. "
					                 "If you want to use hostmasks instead of ipmasks, use a KLINE/GLINE instead.");
					return -1;
				}
		}
		/* set 'p' right for later... */
		p = hostmask-1;
	}
	else
	{
		/* It's seemingly a nick .. let's see if we can find the user */
		if ((acptr = find_person(mask, NULL)))
		{
			usermask = "*";
			if ((*type == 'z') || (*type == 'Z'))
			{
				/* Fill in IP */
				hostmask = GetIP(acptr);
				if (!hostmask)
				{
					sendnotice(sptr, "Could not get IP for user '%s'", acptr->name);
					return 0;
				}
			} else {
				/* Fill in host */
				hostmask = acptr->user->realhost;
			}
			p = hostmask - 1;
		}
		else
		{
			sendnumeric(sptr, ERR_NOSUCHNICK, mask);
			return 0;
		}
	}
	if (!whattodo && ban_too_broad(usermask, hostmask))
	{
		sendnotice(sptr, "*** [error] Too broad mask");
		return 0;
	}

	secs = 0;

	if (whattodo == 0 && (parc > 3))
	{
		secs = atime(parv[2]);
		if (secs < 0)
		{
			sendnotice(sptr, "*** [error] The time you specified is out of range!");
			return 0;
		}
	}
	tkllayer[1] = whattodo == 0 ? "+" : "-";
	tkllayer[2] = type;
	tkllayer[3] = usermask;
	tkllayer[4] = hostmask;
	tkllayer[5] = make_nick_user_host(sptr->name, sptr->user->username, GetHost(sptr));
	if (whattodo == 0)
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
			sendnotice(sptr, "*** [error] The time you specified is out of range");
			return 0;
		}

		/* call the tkl layer .. */
		m_tkl(&me, &me, NULL, 9, tkllayer);
	}
	else
	{
		/* call the tkl layer .. */
		m_tkl(&me, &me, NULL, 6, tkllayer);

	}
	return 0;
}

int eline_syntax(aClient *sptr)
{
	sendnotice(sptr, " Syntax: /ELINE <user@host> <bantypes> <expiry-time> <reason>");
	sendnotice(sptr, "Valid bantypes are:");
	sendnotice(sptr, "k: K-Line     g: G-Line");
	sendnotice(sptr, "z: Z-Line     Z: Global Z-Line");
	sendnotice(sptr, "q: Q-Line");
	sendnotice(sptr, "s: Shun");
	sendnotice(sptr, "f: Spamfilter");
	sendnotice(sptr, "t: Throttling");
	sendnotice(sptr, "b: Blacklist checking");
	sendnotice(sptr, "Example: /ELINE *@unrealircd.org kgzZ 0 This user is exempt");
	sendnotice(sptr, "-");
	sendnotice(sptr, "To get a list of all current ELINEs, type: /STATS exceptban");
	return 0;
}

/** Check if any of the specified types require the
 * exception to be placed on *@ip rather than
 * user@host or *@host. For eg zlines.
 */
int eline_type_requires_ip(char *bantypes)
{
	if (strchr(bantypes, 'z') || strchr(bantypes, 'Z') || strchr(bantypes, 't') || strchr(bantypes, 'b'))
		return 1;
	return 0;
}

CMD_FUNC(m_eline)
{
	time_t secs = 0;
	int add = 1;
	time_t i;
	aClient *acptr = NULL;
	char *mask = NULL;
	char mo[1024], mo2[1024];
	char *p, *usermask, *hostmask, *bantypes=NULL, *reason;
	char *tkllayer[11] = {
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
	struct tm *t;

	if (IsServer(sptr))
		return 0;

	if (!ValidatePermissionsForPath("server-ban:eline",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	/* For del we need at least:
	 * ELINE -user@host
	 * The 'add' case is checked later.
	 */
	if ((parc < 2) || BadPtr(parv[1]))
		return eline_syntax(sptr);

	mask = parv[1];
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
			return eline_syntax(sptr);
		bantypes = parv[2];
		reason = parv[4];
	}

	if (strchr(mask, '!'))
	{
		sendnotice(sptr, "[error] Cannot have '!' in masks.");
		return 0;
	}
	if (*mask == ':')
	{
		sendnotice(sptr, "[error] Mask cannot start with a ':'.");
		return 0;
	}
	if (strchr(mask, ' '))
		return 0;

	/* Check if it's a hostmask and legal .. */
	p = strchr(mask, '@');
	if (p)
	{
		if ((p == mask) || !p[1])
		{
			sendnotice(sptr, "Error: no user@host specified");
			return 0;
		}
		usermask = strtok(mask, "@");
		hostmask = strtok(NULL, "");
		if (BadPtr(hostmask)) {
			if (BadPtr(usermask)) {
				return 0;
			}
			hostmask = usermask;
			usermask = "*";
		}
		if (*hostmask == ':')
		{
			sendnotice(sptr, "[error] For technical reasons you cannot start the host with a ':', sorry");
			return 0;
		}
		if (add && eline_type_requires_ip(bantypes))
		{
			/* Trying to exempt a user from a (G)ZLINE,
			 * make sure the user isn't specifying a host then.
			 */
			if (strcmp(usermask, "*"))
			{
				sendnotice(sptr, "ERROR: Ban exceptions with type z/Z/t/b need to be placed at \037*\037@ipmask, not \037user\037@ipmask. "
				                 "This is because checking (g)zlines, throttling and blacklists is done BEFORE any dns and ident lookups.");
				return -1;
			}
			for (p=hostmask; *p; p++)
				if (isalpha(*p) && !isxdigit(*p))
				{
					sendnotice(sptr, "ERROR: Ban exceptions with type z/Z/t/b need to be placed at *@\037ipmask\037, not *@\037hostmask\037. "
					                 "(so for example *@192.168.* is ok, but *@*.aol.com is not). "
				                         "This is because checking (g)zlines, throttling and blacklists is done BEFORE any dns and ident lookups.");
					return -1;
				}
		}
	}
	else
	{
		/* It's seemingly a nick .. let's see if we can find the user */
		if ((acptr = find_person(mask, NULL)))
		{
			usermask = "*";
			hostmask = GetIP(acptr);
			if (!hostmask)
			{
				sendnotice(sptr, "Could not get IP for user '%s'", acptr->name);
				return 0;
			}
		}
		else
		{
			sendnumeric(sptr, ERR_NOSUCHNICK, mask);
			return 0;
		}
	}

	if (add)
	{
		secs = atime(parv[3]);
		if ((secs <= 0) && (*parv[3] != '0'))
		{
			sendnotice(sptr, "*** [error] The expiry time you specified is out of range!");
			return eline_syntax(sptr);
		}
	}

	tkllayer[1] = add ? "+" : "-";
	tkllayer[2] = "E";
	tkllayer[3] = usermask;
	tkllayer[4] = hostmask;
	tkllayer[5] = make_nick_user_host(sptr->name, sptr->user->username, GetHost(sptr));

	if (add)
	{
		/* Add ELINE */
		if (secs == 0)
			ircsnprintf(mo, sizeof(mo), "%lld", (long long)secs); /* "0" */
		else
			ircsnprintf(mo, sizeof(mo), "%lld", (long long)(secs + TStime()));
		ircsnprintf(mo2, sizeof(mo2), "%lld", (long long)TStime());
		tkllayer[6] = mo;
		tkllayer[7] = mo2;
		tkllayer[8] = bantypes;
		tkllayer[9] = reason;
		/* call the tkl layer .. */
		m_tkl(&me, &me, NULL, 10, tkllayer);
	}
	else
	{
		/* Remove ELINE */
		/* call the tkl layer .. */
		m_tkl(&me, &me, NULL, 10, tkllayer);

	}
	return 0;
}



/** Helper function for m_spamfilter, explaining usage. */
int spamfilter_usage(aClient *sptr)
{
	sendnotice(sptr, "Use: /spamfilter [add|del|remove|+|-] [-simple|-regex|-posix] [type] [action] [tkltime] [tklreason] [regex]");
	sendnotice(sptr, "See '/helpop ?spamfilter' for more information.");
	sendnotice(sptr, "For an easy way to remove an existing spamfilter, use '/spamfilter del' without additional parameters");
	return 0;
}

/** Helper function for m_spamfilter, explaining usage has changed. */
int spamfilter_new_usage(aClient *cptr, aClient *sptr, char *parv[])
{
	sendnotice(sptr, "Unknown match-type '%s'. Must be one of: -regex (new fast PCRE regexes), "
	                 "-posix (old unreal 3.2.x posix regexes) or "
	                 "-simple (simple text with ? and * wildcards)",
	                 parv[2]);

	if (*parv[2] != '-')
		sendnotice(sptr, "Using the old 3.2.x /SPAMFILTER syntax? Note the new -regex/-posix/-simple field!!");

	return spamfilter_usage(cptr);
} 

/** Delete a spamfilter by ID (the ID can be obtained via '/SPAMFILTER del' */
int spamfilter_del_by_id(aClient *sptr, char *id)
{
	int index;
	aTKline *tk;
	int found = 0;
	char mo[32], mo2[32];
	char *tkllayer[13] = {
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
		sendnotice(sptr, "Sorry, no spamfilter found with that ID. Did you run '/spamfilter del' to get the appropriate id?");
		return 0;
	}

	/* Spamfilter found. Now fill the tkllayer */
	tkllayer[1] = "-";
	tkllayer[3] = spamfilter_target_inttostring(tk->ptr.spamfilter->target); /* target(s) */
	mo[0] = banact_valtochar(tk->ptr.spamfilter->action);
	mo[1] = '\0';
	tkllayer[4] = mo; /* action */
	tkllayer[5] = make_nick_user_host(sptr->name, sptr->user->username, GetHost(sptr));
	tkllayer[8] = "-";
	tkllayer[9] = "-";
	tkllayer[10] = unreal_match_method_valtostr(tk->ptr.spamfilter->match->type); /* matching type */
	tkllayer[11] = tk->ptr.spamfilter->match->str; /* regex */
	ircsnprintf(mo2, sizeof(mo2), "%lld", (long long)TStime());
	tkllayer[7] = mo2; /* deletion time */

	m_tkl(&me, &me, NULL, 12, tkllayer);

	return 0;
}

/** Spamfilter to fight spam, advertising, worms and other bad things on IRC.
 * See https://www.unrealircd.org/docs/Spamfilter for general documentation.
 *
 * /SPAMFILTER [add|del|remove|+|-] [match-type] [type] [action] [tkltime] [reason] [regex]
 *                   1                    2         3       4        5        6        7
 */
CMD_FUNC(m_spamfilter)
{
	int whattodo = 0;	/* 0 = add  1 = del */
	char mo[32], mo2[32];
	char *tkllayer[13] = {
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
	aMatch *m;
	int match_type = 0;
	char *err = NULL;

	if (IsServer(sptr))
		return 0;

	if (!ValidatePermissionsForPath("server-ban:spamfilter",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if (parc == 1)
	{
		char *parv[3];
		parv[0] = NULL;
		parv[1] = "spamfilter";
		parv[2] = NULL;
		return do_cmd(sptr, sptr, recv_mtags, "STATS", 2, parv);
	}

	if ((parc <= 3) && !strcmp(parv[1], "del"))
	{
		if (!parv[2])
		{
			/* Show STATS with appropriate SPAMFILTER del command */
			char *parv[5];
			parv[0] = NULL;
			parv[1] = "spamfilter";
			parv[2] = me.name;
			parv[3] = "del";
			parv[4] = NULL;
			return do_cmd(sptr, sptr, recv_mtags, "STATS", 4, parv);
		}
		return spamfilter_del_by_id(sptr, parv[2]);
	}

	if ((parc == 7) && (*parv[2] != '-'))
		return spamfilter_new_usage(cptr,sptr,parv);

	if ((parc < 8) || BadPtr(parv[7]))
		return spamfilter_usage(sptr);

	/* parv[1]: [add|del|+|-]
	 * parv[2]: match-type
	 * parv[3]: type
	 * parv[4]: action
	 * parv[5]: tkl time
	 * parv[6]: tkl reason (or block reason..)
	 * parv[7]: regex
	 */
	if (!strcasecmp(parv[1], "add") || !strcmp(parv[1], "+"))
		whattodo = 0;
	else if (!strcasecmp(parv[1], "del") || !strcmp(parv[1], "-") || !strcasecmp(parv[1], "remove"))
		whattodo = 1;
	else
	{
		sendnotice(sptr, "1st parameter invalid");
		return spamfilter_usage(sptr);
	}

	if ((whattodo == 0) && !strcasecmp(parv[2]+1, "posix"))
	{
		sendnotice(sptr, "ERROR: Spamfilter type 'posix' is DEPRECATED. You must use type 'regex' instead.");
		sendnotice(sptr, "See https://www.unrealircd.org/docs/FAQ#spamfilter-posix-deprecated");
		return 0;
	}

	match_type = unreal_match_method_strtoval(parv[2]+1);
	if (!match_type)
	{
		return spamfilter_new_usage(cptr,sptr,parv);
	}

	targets = spamfilter_gettargets(parv[3], sptr);
	if (!targets)
		return spamfilter_usage(sptr);

	strlcpy(targetbuf, spamfilter_target_inttostring(targets), sizeof(targetbuf));

	action = banact_stringtoval(parv[4]);
	if (!action)
	{
		sendnotice(sptr, "Invalid 'action' field (%s)", parv[4]);
		return spamfilter_usage(sptr);
	}
	actionbuf[0] = banact_valtochar(action);
	actionbuf[1] = '\0';

	if (whattodo == 0)
	{
		/* now check the regex / match field... */
		m = unreal_create_match(match_type, parv[7], &err);
		if (!m)
		{
			sendnotice(sptr, "Error in regex '%s': %s", parv[7], err);
			return 0;
		}
		unreal_delete_match(m);
	}

	tkllayer[1] = whattodo ? "-" : "+";
	tkllayer[3] = targetbuf;
	tkllayer[4] = actionbuf;
	tkllayer[5] = make_nick_user_host(sptr->name, sptr->user->username, GetHost(sptr));

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
	if ((n > 500) && (whattodo == 0))
	{
		sendnotice(sptr, "Sorry, spamfilter too long. You'll either have to trim down the "
		                 "reason or the regex (exceeded by %d bytes)", n - 500);
		return 0;
	}

	if (whattodo == 0)
	{
		ircsnprintf(mo2, sizeof(mo2), "%lld", (long long)TStime());
		tkllayer[7] = mo2;
	}

	m_tkl(&me, &me, NULL, 12, tkllayer);

	return 0;
}

/** tkl hash method.
 * @param c   The tkl type character, see tkl_typetochar().
 * @notes     The input value 'c' is assumed to be in range a-z or A-Z!
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
		sendto_realops("[BUG] tkl_hash() called with out of range parameter (c = '%c') !!!", c);
		ircd_log(LOG_ERROR, "[BUG] tkl_hash() called with out of range parameter (c = '%c') !!!", c);
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
	if (type & TKL_GLOBAL)
	{
		if (type & TKL_KILL)
			return 'G';
		if (type & TKL_ZAP)
			return 'Z';
		if (type & TKL_SHUN)
			return 's';
		if (type & TKL_SPAMF)
			return 'F';
		if (type & TKL_NAME)
			return 'Q';
		if (type & TKL_EXCEPTION)
			return 'E';
	} else {
		if (type & TKL_KILL)
			return 'k';
		if (type & TKL_ZAP)
			return 'z';
		if (type & TKL_SPAMF)
			return 'f';
		if (type & TKL_NAME)
			return 'q';
		if (type & TKL_EXCEPTION)
			return 'e';
	}
	sendto_realops("[BUG]: tkl_typetochar(): unknown type 0x%x !!!", type);
	ircd_log(LOG_ERROR, "[BUG] tkl_typetochar(): unknown type 0x%x !!!", type);
	return 0;
}

/** tkl character to tkl type
 * Returns 0 if invalid type.
 */
int _tkl_chartotype(char c)
{
	switch(c)
	{
		case 'G':
			return TKL_KILL|TKL_GLOBAL;
		case 'Z':
			return TKL_ZAP|TKL_GLOBAL;
		case 's':
			return TKL_SHUN|TKL_GLOBAL;
		case 'F':
			return TKL_SPAMF|TKL_GLOBAL;
		case 'Q':
			return TKL_NAME|TKL_GLOBAL;
		case 'E':
			return TKL_EXCEPTION|TKL_GLOBAL;
		case 'k':
			return TKL_KILL;
		case 'z':
			return TKL_ZAP;
		case 'f':
			return TKL_SPAMF;
		case 'q':
			return TKL_NAME;
		case 'e':
			return TKL_EXCEPTION;
		default:
			return 0;
	}
	/* NOTREACHED */
}

int tkl_banexception_chartotype(char c)
{
	int ret = _tkl_chartotype(c);
	if (ret == 0)
	{
		if (c == 't')
			ret = TKL_THROTTLE;
		else if (c == 'b')
			ret = TKL_BLACKLIST;
	}
	return ret;
}

int tkl_banexception_matches_type(aTKline *except, int bantype)
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
int tkl_ip_hash_tkl(aTKline *tkl)
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
aTKline *tkl_find_head(char type, char *hostmask, aTKline *def)
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
aTKline *_tkl_add_spamfilter(int type, unsigned short target, unsigned short action, aMatch *match, char *set_by,
                             time_t expire_at, time_t set_at,
                             time_t tkl_duration, char *tkl_reason,
                             int flags)
{
	aTKline *tkl;
	int index;

	if (!(type & TKL_SPAMF))
		abort();

	tkl = MyMallocEx(sizeof(aTKline));
	/* First the common fields */
	tkl->type = type;
	tkl->flags = flags;
	tkl->set_at = set_at;
	tkl->set_by = strdup(set_by);
	tkl->expire_at = expire_at;
	/* Then the spamfilter fields */
	tkl->ptr.spamfilter = MyMallocEx(sizeof(Spamfilter));
	tkl->ptr.spamfilter->target = target;
	tkl->ptr.spamfilter->action = action;
	tkl->ptr.spamfilter->match = match;
	tkl->ptr.spamfilter->tkl_reason = strdup(tkl_reason);
	tkl->ptr.spamfilter->tkl_duration = tkl_duration;

	if (tkl->ptr.spamfilter->target & SPAMF_USER)
		loop.do_bancheck_spamf_user = 1;
	if (tkl->ptr.spamfilter->target & SPAMF_AWAY)
		loop.do_bancheck_spamf_away = 1;

	/* Spamfilters go via the normal TKL list... */
	index = tkl_hash(tkl_typetochar(type));
	AddListItem(tkl, tklines[index]);

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
 * @notes
 * Be sure not to call this function for spamfilters,
 * qlines or exempts, which have their own function!
 */
aTKline *_tkl_add_serverban(int type, char *usermask, char *hostmask, char *reason, char *set_by,
                           time_t expire_at, time_t set_at, int soft, int flags)
{
	aTKline *tkl;
	int index, index2;

	if (!TKLIsServerBanType(type))
		abort();

	tkl = MyMallocEx(sizeof(aTKline));
	/* First the common fields */
	tkl->type = type;
	tkl->flags = flags;
	tkl->set_at = set_at;
	tkl->set_by = strdup(set_by);
	tkl->expire_at = expire_at;
	/* Now the server ban fields */
	tkl->ptr.serverban = MyMallocEx(sizeof(ServerBan));
	tkl->ptr.serverban->usermask = strdup(usermask);
	tkl->ptr.serverban->hostmask = strdup(hostmask);
	if (soft)
		tkl->ptr.serverban->subtype = TKL_SUBTYPE_SOFT;
	tkl->ptr.serverban->reason = strdup(reason);

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
 * @notes
 * Be sure not to call this function for spamfilters,
 * qlines or exempts, which have their own function!
 */
aTKline *_tkl_add_banexception(int type, char *usermask, char *hostmask, char *reason, char *set_by,
                               time_t expire_at, time_t set_at, int soft, char *bantypes, int flags)
{
	aTKline *tkl;
	int index, index2;

	if (!TKLIsBanExceptionType(type))
		abort();

	tkl = MyMallocEx(sizeof(aTKline));
	/* First the common fields */
	tkl->type = type;
	tkl->flags = flags;
	tkl->set_at = set_at;
	tkl->set_by = strdup(set_by);
	tkl->expire_at = expire_at;
	/* Now the ban except fields */
	tkl->ptr.banexception = MyMallocEx(sizeof(BanException));
	tkl->ptr.banexception->usermask = strdup(usermask);
	tkl->ptr.banexception->hostmask = strdup(hostmask);
	if (soft)
		tkl->ptr.banexception->subtype = TKL_SUBTYPE_SOFT;
	tkl->ptr.banexception->bantypes = strdup(bantypes);
	tkl->ptr.banexception->reason = strdup(reason);

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
 * @notes
 * Be sure not to call this function for spamfilters,
 * qlines or exempts, which have their own function!
 */
aTKline *_tkl_add_nameban(int type, char *name, int hold, char *reason, char *set_by,
                          time_t expire_at, time_t set_at, int flags)
{
	aTKline *tkl;
	int index, index2;

	if (!TKLIsNameBanType(type))
		abort();

	tkl = MyMallocEx(sizeof(aTKline));
	/* First the common fields */
	tkl->type = type;
	tkl->flags = flags;
	tkl->set_at = set_at;
	tkl->set_by = strdup(set_by);
	tkl->expire_at = expire_at;
	/* Now the name ban fields */
	tkl->ptr.nameban = MyMallocEx(sizeof(ServerBan));
	tkl->ptr.nameban->name = strdup(name);
	tkl->ptr.nameban->hold = hold;
	tkl->ptr.nameban->reason = strdup(reason);

	/* Name bans go via the normal TKL list.. */
	index = tkl_hash(tkl_typetochar(type));
	AddListItem(tkl, tklines[index]);

	return tkl;
}


/** Free a TKL entry but do not remove from the list.
 * (this assumes that it was not added yet or is already removed)
 * Most people will use tkl_del_line() instead.
 */
void _free_tkl(aTKline *tkl)
{
	/* Free the entry */
	/* First, the common fields */
	safefree(tkl->set_by);
	/* Now the type specific fields */
	if (TKLIsServerBan(tkl) && tkl->ptr.serverban)
	{
		safefree(tkl->ptr.serverban->usermask);
		safefree(tkl->ptr.serverban->hostmask);
		safefree(tkl->ptr.serverban->reason);
		MyFree(tkl->ptr.serverban);
	} else
	if (TKLIsNameBan(tkl) && tkl->ptr.nameban)
	{
		safefree(tkl->ptr.nameban->name);
		safefree(tkl->ptr.nameban->reason);
		MyFree(tkl->ptr.nameban);
	} else
	if (TKLIsSpamfilter(tkl) && tkl->ptr.spamfilter)
	{
		/* Spamfilter */
		safefree(tkl->ptr.spamfilter->tkl_reason);
		if (tkl->ptr.spamfilter->match)
			unreal_delete_match(tkl->ptr.spamfilter->match);
		MyFree(tkl->ptr.spamfilter);
	}
	MyFree(tkl);
}

/** Delete a TKL entry from the list and free it.
 * @param tkl The TKL entry.
 */
void _tkl_del_line(aTKline *tkl)
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
}

/*
 * tkl_check_local_remove_shun:
 * removes shun from currently connected users affected by tmp.
 */
// TODO / FIXME: audit this function, it looks crazy
void _tkl_check_local_remove_shun(aTKline *tmp)
{
	long i;
	char *chost, *cname, *cip;
	int is_ip;
	aClient *acptr;

	aTKline *tk;
	int keep_shun;

	for (i = 0; i <= 5; i++)
	{
		list_for_each_entry(acptr, &lclient_list, lclient_node)
			if (MyClient(acptr) && IsShunned(acptr))
			{
				chost = acptr->local->sockhost;
				cname = acptr->user->username;

				cip = GetIP(acptr);

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
						if(tk != tmp && match_simple(tk->ptr.serverban->usermask, cname))
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

					if(!keep_shun)
					{
						ClearShunned(acptr);
					}
				}
			}
	}
}

/** Deal with expiration of a specific TKL entry.
 * This is a helper function for tkl_check_expire().
 */
void tkl_expire_entry(aTKline *tkl)
{
	char *whattype = tkl_type_string(tkl);

	if (!tkl)
		return;

	if (tkl->type & TKL_SPAMF)
	{
		/* Impossible */
	} else
	if (TKLIsServerBan(tkl))
	{
		sendto_snomask(SNO_TKL,
		    "*** Expiring %s (%s@%s) made by %s (Reason: %s) set %lld seconds ago",
		    whattype, tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask, tkl->set_by, tkl->ptr.serverban->reason,
		    (long long)(TStime() - tkl->set_at));
		ircd_log
		    (LOG_TKL, "Expiring %s (%s@%s) made by %s (Reason: %s) set %lld seconds ago",
		    whattype, tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask, tkl->set_by, tkl->ptr.serverban->reason,
		    (long long)(TStime() - tkl->set_at));
	}
	else if (TKLIsNameBan(tkl))
	{
		if (!tkl->ptr.nameban->hold)
		{
			sendto_snomask(SNO_TKL,
				"*** Expiring %s (%s) made by %s (Reason: %s) set %lld seconds ago",
				whattype, tkl->ptr.nameban->name, tkl->set_by, tkl->ptr.nameban->reason,
				(long long)(TStime() - tkl->set_at));
			ircd_log
				(LOG_TKL, "Expiring %s (%s) made by %s (Reason: %s) set %lld seconds ago",
				whattype, tkl->ptr.nameban->name, tkl->set_by, tkl->ptr.nameban->reason,
				(long long)(TStime() - tkl->set_at));
		}
	}

	if (tkl->type & TKL_SHUN)
		tkl_check_local_remove_shun(tkl);

	RunHook3(HOOKTYPE_TKL_DEL, NULL, NULL, tkl);
	tkl_del_line(tkl);
}

/** Regularly check TKL entries for expiration */
EVENT(tkl_check_expire)
{
	aTKline *tkl, *next;
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
static int find_tkl_exception_matcher(aClient *cptr, int ban_type, aTKline *except_tkl)
{
	char uhost[NICKLEN+HOSTLEN+1];
	Hook *hook;

	if (!TKLIsBanException(except_tkl))
		return 0;

	if (!tkl_banexception_matches_type(except_tkl, ban_type))
		return 0;

	snprintf(uhost, sizeof(uhost), "%s@%s",
	         except_tkl->ptr.banexception->usermask, except_tkl->ptr.banexception->hostmask);

	if (match_user(uhost, cptr, MATCH_CHECK_REAL))
	{
		if (!(except_tkl->ptr.banexception->subtype & TKL_SUBTYPE_SOFT))
			return 1; /* hard ban exempt */
		if ((except_tkl->ptr.banexception->subtype & TKL_SUBTYPE_SOFT) && IsLoggedIn(cptr))
			return 1; /* soft ban exempt - only matches if user is logged in */
	}

	return 0; /* not found */
}

/** Search for TKL Exceptions for this user.
 * @param ban_type The ban type to check, normally ban_tkl->type.
 * @param cptr     The user
 * @returns 1 if ban exempt, 0 if not.
 * @notes
 * If you have a TKL ban that matched, say, 'ban_tkl'.
 * Then you call this function like this:
 * if (find_tkl_exception(ban_tkl->type, cptr))
 *     return 0; // User is exempt
 * [.. continue and ban the user..]
 */
int _find_tkl_exception(int ban_type, aClient *cptr)
{
	aTKline *tkl, *ret;
	int index, index2;
	Hook *hook;

	if (IsServer(cptr) || IsMe(cptr))
		return 1;

	/* First, the TKL ip hash table entries.. */
	index = tkl_ip_hash_type('e');
	index2 = tkl_ip_hash(GetIP(cptr));
	if (index2 >= 0)
	{
		for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
		{
			if (find_tkl_exception_matcher(cptr, ban_type, tkl))
				return 1; /* exempt */
		}
	}

	/* If not banned (yet), then check regular entries.. */
	for (tkl = tklines[tkl_hash('e')]; tkl; tkl = tkl->next)
	{
			if (find_tkl_exception_matcher(cptr, ban_type, tkl))
				return 1; /* exempt */
	}

	// FIXME: REMOVE CONF_EXCEPT_TKL plz and CONF_EXCEPT_BAN

	for (hook = Hooks[HOOKTYPE_TKL_EXCEPT]; hook; hook = hook->next)
	{
		if (hook->func.intfunc(cptr, ban_type) > 0)
			return 1; /* exempt by hook */
	}
	return 0; /* Not exempt */
}

/** Helper function for find_tkline_match() */
int find_tkline_match_matcher(aClient *cptr, int skip_soft, aTKline *tkl)
{
	char uhost[NICKLEN+HOSTLEN+1];
	ConfigItem_except *excepts;
	Hook *hook;

	if (!TKLIsServerBan(tkl) || (tkl->type & TKL_SHUN))
		return 0;

	if (skip_soft && (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT))
		return 0;

	snprintf(uhost, sizeof(uhost), "%s@%s", tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask);

	if (match_user(uhost, cptr, MATCH_CHECK_REAL))
	{
		/* If hard-ban, or soft-ban&unauthenticated.. */
		if (!(tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ||
		    ((tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) && !IsLoggedIn(cptr)))
		{
			/* Found match. Now check for exception... */
			if (find_tkl_exception(tkl->type, cptr))
				return 0; /* exempted */
			return 1; /* banned */
		}
	}

	return 0; /* no match */
}

/** Check if user matches a *LINE. If so, kill the user.
 * @retval <0 if client is banned (user is killed, don't touch 'cptr' anymore),
 *         otherwise the client is not banned (either no match or on an exception list).
 */
int _find_tkline_match(aClient *cptr, int skip_soft)
{
	aTKline *tkl;
	int banned = 0;
	int index, index2;

	if (IsServer(cptr) || IsMe(cptr))
		return -1;

	/* First, the TKL ip hash table entries.. */
	index2 = tkl_ip_hash(GetIP(cptr));
	if (index2 >= 0)
	{
		for (index = 0; index < TKLIPHASHLEN1; index++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				banned = find_tkline_match_matcher(cptr, skip_soft, tkl);
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
				banned = find_tkline_match_matcher(cptr, skip_soft, tkl);
				if (banned)
					break;
			}
			if (banned)
				break;
		}
	}

	if (!banned)
		return 1;

	/* User is banned... */

	RunHookReturnInt2(HOOKTYPE_FIND_TKLINE_MATCH, cptr, tkl, !=99);

	if (tkl->type & TKL_KILL)
	{
		ircstp->is_ref++;
		if (tkl->type & TKL_GLOBAL)
			return banned_client(cptr, "G-Lined", tkl->ptr.serverban->reason, 1, 0);
		else
			return banned_client(cptr, "K-Lined", tkl->ptr.serverban->reason, 0, 0);
	}
	if (tkl->type & TKL_ZAP)
	{
		ircstp->is_ref++;
		return banned_client(cptr, "Z-Lined", tkl->ptr.serverban->reason, (tkl->type & TKL_GLOBAL)?1:0, 0);
	}

	return 3;
}

/** Check if user is shunned. Returns 2 in such a case (FIXME: why 2 ?) */
int _find_shun(aClient *cptr)
{
	aTKline *tkl;
	ConfigItem_except *excepts;
	int match_type = 0;
	Hook *hook;

	if (IsServer(cptr) || IsMe(cptr))
		return -1;

	if (IsShunned(cptr))
		return 1;

	if (ValidatePermissionsForPath("immune:server-ban:shun",cptr,NULL,NULL,NULL))
		return 1;

	for (tkl = tklines[tkl_hash('s')]; tkl; tkl = tkl->next)
	{
		char uhost[NICKLEN+HOSTLEN+1];

		if (!(tkl->type & TKL_SHUN))
			continue;

		snprintf(uhost, sizeof(uhost), "%s@%s", tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask);

		if (match_user(uhost, cptr, MATCH_CHECK_REAL))
		{
			/* If hard-ban, or soft-ban&unauthenticated.. */
			if (!(tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ||
			    ((tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) && !IsLoggedIn(cptr)))
			{
				/* Found match. Now check for exception... */
				if (find_tkl_exception(TKL_SHUN, cptr))
					return 1;
				SetShunned(cptr);
				return 2; /* Shunned */
			}
		}
	}

	return 1; /* No match */
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
 * @param buf   The buffer used for storage, the size of
 *              which should be at least NICKLEN+USERLEN+HOSTLEN+1.
 * @param nick  The nickname (because acptr can be nick-changing).
 * @param acptr The affected client.
 */
void _spamfilter_build_user_string(char *buf, char *nick, aClient *acptr)
{
	snprintf(buf, NICKLEN+USERLEN+HOSTLEN+1, "%s!%s@%s:%s",
		nick, acptr->user->username, SpamfilterMagicHost(acptr->user->realhost), acptr->info);
}


/** Checks if the user matches a spamfilter of type 'u' (user,
 * nick!user@host:realname ban).
 * Written by: Syzop
 * Assumes: only call for clients, possible assume on local clients [?]
 * Return values: see run_spamfilter()
 */
int _find_spamfilter_user(aClient *sptr, int flags)
{
	char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64]; /* n!u@h:r */

	if (ValidatePermissionsForPath("immune:server-ban:spamfilter",sptr,NULL,NULL,NULL))
		return 0;

	spamfilter_build_user_string(spamfilter_user, sptr->name, sptr);
	return run_spamfilter(sptr, spamfilter_user, SPAMF_USER, NULL, flags, NULL);
}

/** Check a spamfilter against all local users and print a message.
 * This is only used for the 'warn' action (BAN_ACT_WARN).
 */
int spamfilter_check_users(aTKline *tkl)
{
	char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64]; /* n!u@h:r */
	char buf[1024];
	int matches = 0;
	aClient *acptr;

	list_for_each_entry_reverse(acptr, &lclient_list, lclient_node)
	{
		if (MyClient(acptr))
		{
			spamfilter_build_user_string(spamfilter_user, acptr->name, acptr);
			if (!unreal_match(tkl->ptr.spamfilter->match, spamfilter_user))
				continue; /* No match */

			/* matched! */
			ircsnprintf(buf, sizeof(buf), "[Spamfilter] %s!%s@%s matches filter '%s': [%s: '%s'] [%s]",
				acptr->name, acptr->user->username, acptr->user->realhost,
				tkl->ptr.spamfilter->match->str,
				"user", spamfilter_user,
				unreal_decodespace(tkl->ptr.spamfilter->tkl_reason));

			sendto_snomask_global(SNO_SPAMF, "%s", buf);
			ircd_log(LOG_SPAMFILTER, "%s", buf);
			RunHook6(HOOKTYPE_LOCAL_SPAMFILTER, acptr, spamfilter_user, spamfilter_user, SPAMF_USER, NULL, tkl);
			matches++;
		}
	}

	return matches;
}

/** Similarly to previous, but match against all global users.
 * FUNCTION IS UNUSED !!
 */
int spamfilter_check_all_users(aClient *from, aTKline *tkl)
{
	char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64]; /* n!u@h:r */
	int matches = 0;
	aClient *acptr;

	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (IsPerson(acptr))
		{
			spamfilter_build_user_string(spamfilter_user, acptr->name, acptr);
			if (!unreal_match(tkl->ptr.spamfilter->match, spamfilter_user))
				continue; /* No match */

			/* matched! */
			sendnotice(from, "[Spamfilter] %s!%s@%s matches filter '%s': [%s: '%s'] [%s]",
				acptr->name, acptr->user->username, acptr->user->realhost,
				tkl->ptr.spamfilter->match->str,
				"user", spamfilter_user,
				unreal_decodespace(tkl->ptr.spamfilter->tkl_reason));
			matches++;
		}
	}

	return matches;
}

/** Check if the nick or channel name is banned (Q-Line).
 * @param cptr     The possibly affected user.
 * @param name     The nick or channel to check.
 * @param is_hold  This will be SET (so OUT) if it's a services hold.
 *
 * @notes Special handling:
 * #*ble* will match with #bbleh
 * *ble* will NOT match with #bbleh, will with bbleh
 */
aTKline *_find_qline(aClient *cptr, char *name, int *ishold)
{
	aTKline *tkl;
	int	points = 0;
	ConfigItem_except *excepts;
	*ishold = 0;

	if (IsServer(cptr) || IsMe(cptr))
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

	if (find_tkl_exception(TKL_NAME, cptr))
		return NULL; /* exempt */

	return tkl;
}

/** Helper function for find_tkline_match_zap() */
aTKline *find_tkline_match_zap_matcher(aClient *cptr, aTKline *tkl)
{
	ConfigItem_except *excepts;
	Hook *hook;

	if (!(tkl->type & TKL_ZAP))
		return NULL;

	if (match_user(tkl->ptr.serverban->hostmask, cptr, MATCH_CHECK_IP))
	{
		if (find_tkl_exception(TKL_ZAP, cptr))
			return NULL; /* exempt */
		return tkl; /* banned */
	}

	return NULL; /* no match */
}

/** Find matching (G)ZLINE, if any.
 * Note: function prototype changed as per UnrealIRCd 4.2.0.
 * @retval The (G)Z-Line that matched, or NULL if no such ban was found.
 */
aTKline *_find_tkline_match_zap(aClient *cptr)
{
	aTKline *tkl, *ret;
	int index, index2;

	if (IsServer(cptr) || IsMe(cptr))
		return NULL;

	/* First, the TKL ip hash table entries.. */
	index = tkl_ip_hash_type('z');
	index2 = tkl_ip_hash(GetIP(cptr));
	if (index2 >= 0)
	{
		for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
		{
			ret = find_tkline_match_zap_matcher(cptr, tkl);
			if (ret)
				return ret;
		}
	}

	/* If not banned (yet), then check regular entries.. */
	for (tkl = tklines[tkl_hash('z')]; tkl; tkl = tkl->next)
	{
		ret = find_tkline_match_zap_matcher(cptr, tkl);
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
	char *mask;
	char *reason;
	char *set_by;
} TKLFlag;

/** Parse STATS tkl parameters.
 * TODO: I don't think this is documented anywhere? Or underdocumented at least.
 */
static void parse_stats_params(char *para, TKLFlag *flag)
{
	static char paratmp[512]; /* <- copy of para, because it gets fragged by strtok() */
	char *flags, *tmp;
	char what = '+';

	bzero(flag, sizeof(TKLFlag));
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
void tkl_stats_matcher(aClient *cptr, int type, char *para, TKLFlag *tklflags, aTKline *tkl)
{
	/* This handles the selection */
	if (!BadPtr(para))
	{
		if (tklflags->flags & BY_SETBY)
			if (!match_simple(tklflags->set_by, tkl->set_by))
				return;
		if (tklflags->flags & NOT_BY_SETBY)
			if (match_simple(tklflags->set_by, tkl->set_by))
				return;
		if (TKLIsServerBan(tkl))
		{
			if (tklflags->flags & BY_MASK)
			{
				if (tkl->type & TKL_NAME)
				{
					if (!match_simple(tklflags->mask, tkl->ptr.nameban->name))
						return;
				}
				else if (!match_simple(tklflags->mask, make_user_host(tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask)))
					return;
			}
			if (tklflags->flags & NOT_BY_MASK)
			{
				if (tkl->type & TKL_NAME)
				{
					if (match_simple(tklflags->mask, tkl->ptr.nameban->name))
						return;
				}
				else if (match_simple(tklflags->mask, make_user_host(tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask)))
					return;
			}
			if (tklflags->flags & BY_REASON)
				if (!match_simple(tklflags->reason, tkl->ptr.serverban->reason))
					return;
			if (tklflags->flags & NOT_BY_REASON)
				if (match_simple(tklflags->reason, tkl->ptr.serverban->reason))
					return;
		}
	}

	/* If we are still here, then we will send the STATS entry */

	if (tkl->type == (TKL_KILL | TKL_GLOBAL))
	{
		sendnumeric(cptr, RPL_STATSGLINE, 'G',
			   (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask,
			   (tkl->expire_at != 0) ? (tkl->expire_at - TStime()) : 0,
			   (TStime() - tkl->set_at), tkl->set_by, tkl->ptr.serverban->reason);
	}
	if (tkl->type == (TKL_ZAP | TKL_GLOBAL))
	{
		sendnumeric(cptr, RPL_STATSGLINE, 'Z',
			   (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask,
			   (tkl->expire_at != 0) ? (tkl->expire_at - TStime()) : 0,
			   (TStime() - tkl->set_at), tkl->set_by, tkl->ptr.serverban->reason);
	}
	if (tkl->type == (TKL_SHUN | TKL_GLOBAL))
	{
		sendnumeric(cptr, RPL_STATSGLINE, 's',
			   (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask,
			   (tkl->expire_at != 0) ? (tkl->expire_at - TStime()) : 0,
			   (TStime() - tkl->set_at), tkl->set_by, tkl->ptr.serverban->reason);
	}
	if (tkl->type == (TKL_KILL))
	{
		sendnumeric(cptr, RPL_STATSGLINE, 'K',
			   (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask,
			   (tkl->expire_at != 0) ? (tkl->expire_at - TStime()) : 0,
			   (TStime() - tkl->set_at), tkl->set_by, tkl->ptr.serverban->reason);
	}
	if (tkl->type == (TKL_ZAP))
	{
		sendnumeric(cptr, RPL_STATSGLINE, 'z',
			   (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask,
			   (tkl->expire_at != 0) ? (tkl->expire_at - TStime()) : 0,
			   (TStime() - tkl->set_at), tkl->set_by, tkl->ptr.serverban->reason);
	}
	if (tkl->type & TKL_SPAMF)
	{
		sendnumeric(cptr, RPL_STATSSPAMF,
			(tkl->type & TKL_GLOBAL) ? 'F' : 'f',
			unreal_match_method_valtostr(tkl->ptr.spamfilter->match->type),
			spamfilter_target_inttostring(tkl->ptr.spamfilter->target),
			banact_valtostring(tkl->ptr.spamfilter->action),
			(tkl->expire_at != 0) ? (tkl->expire_at - TStime()) : 0,
			TStime() - tkl->set_at,
			tkl->ptr.spamfilter->tkl_duration, tkl->ptr.spamfilter->tkl_reason,
			tkl->set_by,
			tkl->ptr.spamfilter->match->str);
		if (para && !strcasecmp(para, "del"))
		{
			char *hash = spamfilter_id(tkl);
			if (tkl->type & TKL_GLOBAL)
			{
				sendtxtnumeric(cptr, "To delete this spamfilter, use /SPAMFILTER del %s", hash);
				sendtxtnumeric(cptr, "-");
			} else {
				sendtxtnumeric(cptr, "This spamfilter is stored in the configuration file and cannot be removed with /SPAMFILTER del");
				sendtxtnumeric(cptr, "-");
			}
		}
	}
	if (tkl->type & TKL_NAME)
	{
		sendnumeric(cptr, RPL_STATSQLINE, (tkl->type & TKL_GLOBAL) ? 'Q' : 'q',
			tkl->ptr.nameban->name, (tkl->expire_at != 0) ? (tkl->expire_at - TStime()) : 0,
			TStime() - tkl->set_at, tkl->set_by, tkl->ptr.nameban->reason);
	}
}

/* TKL Stats. This is used by /STATS gline and all the others */
void _tkl_stats(aClient *cptr, int type, char *para)
{
	aTKline *tk;
	TKLFlag tklflags;
	int index, index2;

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
				tkl_stats_matcher(cptr, type, para, &tklflags, tk);
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
			tkl_stats_matcher(cptr, type, para, &tklflags, tk);
		}
	}

	if ((type == (TKL_SPAMF|TKL_GLOBAL)) && (!para || strcasecmp(para, "del")))
	{
		/* If requesting spamfilter stats and not spamfilter del, then suggest it. */
		sendnotice(cptr, "Tip: if you are looking for an easy way to remove a spamfilter, run '/SPAMFILTER del'.");
	}
}

/** Synchronize a TKL entry with the other server.
 * @param sender  The sender (eg: &me).
 * @param to      The remote server.
 * @param tkl     The TKL entry.
 */
void tkl_synch_send_entry(int add, aClient *sender, aClient *to, aTKline *tkl)
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
		sendto_one(to, NULL, ":%s TKL %c %c %s %s %s %lld %lld %lld %s %s :%s", sender->name,
			   add ? '+' : '-',
			   typ,
			   spamfilter_target_inttostring(tkl->ptr.spamfilter->target),
			   banact_valtostring(tkl->ptr.spamfilter->action),
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
		sendto_ops_and_log("[BUG] tkl_synch_send_entry() called, but unknown type %d/'%c'",
			tkl->type, typ);
		abort();
	}
}

/** Broadcast a TKL entry.
 * @param sender  The sender, eg &me
 * @param skip    The client to skip, eg cptr or NULL.
 * @param tkl     The TKL entry to synchronize with the other servers.
 */
void tkl_broadcast_entry(int add, aClient *sender, aClient *skip, aTKline *tkl)
{
	aClient *cptr;

	list_for_each_entry(cptr, &server_list, special_node)
	{
		if (skip && cptr == skip->from)
			continue;

		tkl_synch_send_entry(add, sender, cptr, tkl);
	}
}

/** Synchronize all TKL entries with this server.
 * @param sptr The server to synchronize with.
 */
void _tkl_synch(aClient *sptr)
{
	aTKline *tkl;
	int index, index2;

	/* First, hashed entries.. */
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				tkl_synch_send_entry(1, &me, sptr, tkl);
			}
		}
	}

	/* Then, regular entries.. */
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			tkl_synch_send_entry(1, &me, sptr, tkl);
		}
	}
}

/** Show TKL type as a string (used when adding/removing) */
char *_tkl_type_string(aTKline *tkl)
{
	static char txt[256];

	*txt = '\0';

	if (TKLIsServerBan(tkl) && (tkl->ptr.serverban->subtype == TKL_SUBTYPE_SOFT))
		strlcpy(txt, "Soft ", sizeof(txt));

	switch (tkl->type)
	{
		case TKL_KILL:
			strlcat(txt, "K-Line", sizeof(txt));
			break;
		case TKL_ZAP:
			strlcat(txt, "Z-Line", sizeof(txt));
			break;
		case TKL_KILL | TKL_GLOBAL:
			strlcat(txt, "G-Line", sizeof(txt));
			break;
		case TKL_ZAP | TKL_GLOBAL:
			strlcat(txt, "Global Z-Line", sizeof(txt));
			break;
		case TKL_SHUN | TKL_GLOBAL:
			strlcat(txt, "Shun", sizeof(txt));
			break;
		case TKL_NAME | TKL_GLOBAL:
			strlcat(txt, "Global Q-Line", sizeof(txt));
			break;
		case TKL_NAME:
			strlcat(txt, "Q-Line", sizeof(txt));
			break;
		case TKL_SPAMF:
			strlcat(txt, "Local Spamfilter", sizeof(txt));
			break;
		case TKL_SPAMF | TKL_GLOBAL:
			strlcat(txt, "Spamfilter", sizeof(txt));
			break;
		case TKL_EXCEPTION:
			strlcat(txt, "Local Exception", sizeof(txt));
			break;
		case TKL_EXCEPTION | TKL_GLOBAL:
			strlcat(txt, "Exception", sizeof(txt));
			break;
		default:
			strlcat(txt, "Unknown *-Line", sizeof(txt));
	}

	return txt;
}

/** Find a server ban TKL - only used to prevent duplicates and for deletion */
aTKline *_find_tkl_serverban(int type, char *usermask, char *hostmask, int softban)
{
	char tpe = tkl_typetochar(type);
	aTKline *head, *tkl;

	if (!TKLIsServerBanType(type))
		abort();

	head = tkl_find_head(tpe, hostmask, tklines[tkl_hash(tpe)]);
	for (tkl = head; tkl; tkl = tkl->next)
	{
		if (tkl->type == type)
		{
			if (!stricmp(tkl->ptr.serverban->hostmask, hostmask) &&
			    !stricmp(tkl->ptr.serverban->usermask, usermask))
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
aTKline *_find_tkl_banexception(int type, char *usermask, char *hostmask, int softban)
{
	char tpe = tkl_typetochar(type);
	aTKline *head, *tkl;

	if (!TKLIsBanExceptionType(type))
		abort();

	head = tkl_find_head(tpe, hostmask, tklines[tkl_hash(tpe)]);
	for (tkl = head; tkl; tkl = tkl->next)
	{
		if (tkl->type == type)
		{
			if (!stricmp(tkl->ptr.banexception->hostmask, hostmask) &&
			    !stricmp(tkl->ptr.banexception->usermask, usermask))
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
aTKline *_find_tkl_nameban(int type, char *name, int hold)
{
	char tpe = tkl_typetochar(type);
	aTKline *tkl;

	if (!TKLIsNameBanType(type))
		abort();

	for (tkl = tklines[tkl_hash(tpe)]; tkl; tkl = tkl->next)
	{
		if ((tkl->type == type) && !stricmp(tkl->ptr.nameban->name, name))
			return tkl;
	}
	return NULL; /* Not found */
}

/** Find a spamfilter TKL - only used to prevent duplicates and for deletion */
aTKline *_find_tkl_spamfilter(int type, char *match_string, unsigned short action, unsigned short target)
{
	char tpe = tkl_typetochar(type);
	aTKline *tkl;

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
void _sendnotice_tkl_add(aTKline *tkl)
{
	char buf[512];
	char set_at[128];
	char expire_at[128];
	char *tkl_type_str; /**< Eg: "K-Line" */

	/* Don't show notices for temporary nick holds (issued by services) */
	if (TKLIsNameBan(tkl) && tkl->ptr.nameban->hold)
		return;

	tkl_type_str = tkl_type_string(tkl);

	*buf = *set_at = *expire_at = '\0';
	short_date(tkl->set_at, set_at);
	if (tkl->expire_at > 0)
		short_date(tkl->expire_at, expire_at);

	if (TKLIsServerBan(tkl))
	{
		if (tkl->expire_at != 0)
		{
			ircsnprintf(buf, sizeof(buf), "%s added for %s%s@%s on %s GMT (from %s to expire at %s GMT: %s)",
				tkl_type_str,
				(tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
				tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask,
				set_at, tkl->set_by, expire_at, tkl->ptr.serverban->reason);
		} else {
			ircsnprintf(buf, sizeof(buf), "Permanent %s added for %s%s@%s on %s GMT (from %s: %s)",
				tkl_type_str,
				(tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
				tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask,
				set_at, tkl->set_by, tkl->ptr.serverban->reason);
		}
	} else
	if (TKLIsNameBan(tkl))
	{
		if (tkl->expire_at > 0)
		{
			ircsnprintf(buf, sizeof(buf), "%s added for %s on %s GMT (from %s to expire at %s GMT: %s)",
				tkl_type_str, tkl->ptr.nameban->name, set_at, tkl->set_by, expire_at, tkl->ptr.nameban->reason);
		} else {
			ircsnprintf(buf, sizeof(buf), "Permanent %s added for %s on %s GMT (from %s: %s)",
				tkl_type_str, tkl->ptr.nameban->name, set_at, tkl->set_by, tkl->ptr.nameban->reason);
		}
	} else
	if (TKLIsSpamfilter(tkl))
	{
		/* Spamfilter */
		ircsnprintf(buf, sizeof(buf),
		            "Spamfilter added: '%s' [target: %s] [action: %s] [reason: %s] on %s GMT (from %s)",
		            tkl->ptr.spamfilter->match->str,
		            spamfilter_target_inttostring(tkl->ptr.spamfilter->target),
		            banact_valtostring(tkl->ptr.spamfilter->action),
		            unreal_decodespace(tkl->ptr.spamfilter->tkl_reason),
		            set_at,
		            tkl->set_by);
	} else
	if (TKLIsBanException(tkl))
	{
		if (tkl->expire_at != 0)
		{
			ircsnprintf(buf, sizeof(buf), "%s added for %s%s@%s for types '%s' on %s GMT (from %s to expire at %s GMT: %s)",
				tkl_type_str,
				(tkl->ptr.banexception->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
				tkl->ptr.banexception->usermask, tkl->ptr.banexception->hostmask,
				tkl->ptr.banexception->bantypes,
				set_at, tkl->set_by, expire_at, tkl->ptr.banexception->reason);
		} else {
			ircsnprintf(buf, sizeof(buf), "Permanent %s added for %s%s@%s for types '%s' on %s GMT (from %s: %s)",
				tkl_type_str,
				(tkl->ptr.banexception->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
				tkl->ptr.banexception->usermask, tkl->ptr.banexception->hostmask,
				tkl->ptr.banexception->bantypes,
				set_at, tkl->set_by, tkl->ptr.banexception->reason);
		}
	} else
	{
		ircsnprintf(buf, sizeof(buf), "[BUG] %s added but type unhandled in sendnotice_tkl_add()!!!", tkl_type_str);
	}

	sendto_snomask(SNO_TKL, "*** %s", buf);
	ircd_log(LOG_TKL, "%s", buf);
}

/** Send a notice to opers about the TKL that is being deleted */
void _sendnotice_tkl_del(char *removed_by, aTKline *tkl)
{
	char buf[512];
	char set_at[128];
	char *tkl_type_str;

	/* Don't show notices for temporary nick holds (issued by services) */
	if (TKLIsNameBan(tkl) && tkl->ptr.nameban->hold)
		return;

	tkl_type_str = tkl_type_string(tkl); /* eg: "K-Line" */

	*buf = *set_at = '\0';
	short_date(tkl->set_at, set_at);

	if (TKLIsServerBan(tkl))
	{
		ircsnprintf(buf, sizeof(buf),
			       "%s removed %s %s@%s (set at %s - reason: %s)",
			       removed_by, tkl_type_str,
			       tkl->ptr.serverban->usermask, tkl->ptr.serverban->hostmask,
			       set_at, tkl->ptr.serverban->reason);
	} else
	if (TKLIsNameBan(tkl))
	{
		ircsnprintf(buf, sizeof(buf),
			"%s removed %s %s (set at %s - reason: %s)",
			removed_by, tkl_type_str, tkl->ptr.nameban->name, set_at, tkl->ptr.nameban->reason);
	} else
	if (TKLIsSpamfilter(tkl))
	{
		ircsnprintf(buf, sizeof(buf),
			"%s removed Spamfilter '%s' (set at %s)",
			removed_by, tkl->ptr.spamfilter->match->str, set_at);
	} else
	if (TKLIsBanException(tkl))
	{
		ircsnprintf(buf, sizeof(buf),
			       "%s removed exception on %s@%s (set at %s - reason: %s)",
			       removed_by,
			       tkl->ptr.banexception->usermask, tkl->ptr.banexception->hostmask,
			       set_at, tkl->ptr.banexception->reason);
	} else
	{
		ircsnprintf(buf, sizeof(buf), "[BUG] %s added but type unhandled in sendnotice_tkl_del()!!!!!", tkl_type_str);
	}

	sendto_snomask(SNO_TKL, "*** %s", buf);
	ircd_log(LOG_TKL, "%s", buf);
}

/** Add a TKL using the TKL layer. See m_tkl for parv[] and protocol documentation. */
CMD_FUNC(m_tkl_add)
{
	aTKline *tkl;
	int type;
	time_t expire_at, set_at;
	char *set_by;
	char tkl_entry_exists = 0;

	/* we rely on servers to be failsafe.. */
	if (!IsServer(sptr) && !IsMe(sptr))
		return 0;

	if (parc < 9)
		return 0;

	type = tkl_chartotype(parv[2][0]);
	if (!type)
		return 0;

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
		sendto_realops("Invalid TKL entry from %s, set-at time is out of range (%lld) -- not added. Clock on other server incorrect or bogus entry.",
			sptr->name, (long long)set_at);
		return 0;
	}
	if ((expire_at < 0) || !short_date(expire_at, NULL))
	{
		sendto_realops("Invalid TKL entry from %s, expiry time is out of range (%lld) -- not added. Clock on other server incorrect or bogus entry.",
			sptr->name, (long long)expire_at);
		return 0;
	}

	/* Now comes type-specific validation
	 * and we check if the TKL entry already exists and needs updating too.
	 */

	if (TKLIsServerBanType(type))
	{
		/* Validate server ban TKL fields */
		int softban = 0;
		char *usermask = parv[3];
		char *hostmask = parv[4];
		char *reason = parv[8];

		/* Some simple validation on usermask and hostmask:
		 * may not contain an @. Yeah, some services or self-written
		 * linked servers are known to have sent this in the past.
		 */
		if (strchr(usermask, '@') || strchr(hostmask, '@'))
		{
			sendto_realops("Ignoring TKL entry %s@%s from %s. "
			               "Invalid usermask '%s' or hostmask '%s'.",
			               usermask, hostmask, sptr->name, usermask, hostmask);
			return 0;
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
		char *usermask = parv[3];
		char *hostmask = parv[4];
		char *bantypes = parv[8];
		char *reason;

		if (parc < 10)
			return 0;

		reason = parv[9];

		/* Some simple validation on usermask and hostmask:
		 * may not contain an @. Yeah, some services or self-written
		 * linked servers are known to have sent this in the past.
		 */
		if (strchr(usermask, '@') || strchr(hostmask, '@'))
		{
			sendto_realops("Ignoring TKL exception entry %s@%s from %s. "
			               "Invalid usermask '%s' or hostmask '%s'.",
			               usermask, hostmask, sptr->name, usermask, hostmask);
			return 0;
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
		char *name = parv[4];
		char *reason = parv[8];

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
		char *match_string;
		aMatch *m; /* compiled match_string */
		time_t tkl_duration;
		char *tkl_reason;
		unsigned short action;
		unsigned short target;
		/* helper variables */
		char *err;

		if (parc < 12)
		{
			sendto_realops("Ignoring spamfilter from %s. Running very old UnrealIRCd protocol (3.2.X?)", sptr->name);
			return 0;
		}

		match_string = parv[11];

		if (!strcasecmp(parv[10], "posix"))
		{
			sendto_realops("Ignoring spamfilter from %s. Spamfilter is of type 'posix' (TRE) which "
				       "is not supported in UnrealIRCd 5. Suggestion: upgrade the other server.",
				       sptr->name);
			return 0;
		}
		match_method = unreal_match_method_strtoval(parv[10]);
		if (match_method == 0)
		{
			sendto_realops("Ignoring spamfilter '%s' from %s with unknown match type '%s'",
				match_string, sptr->name, parv[10]);
			return 0;
		}

		if (!(target = spamfilter_gettargets(parv[3], NULL)))
		{
			sendto_realops("Ignoring spamfilter '%s' from %s with unknown target type '%s'",
				match_string, sptr->name, parv[3]);
			return 0;
		}

		if (!(action = banact_chartoval(*parv[4])))
		{
			sendto_realops("Ignoring spamfilter '%s' from %s with unknown action type '%s'",
				match_string, sptr->name, parv[4]);
			return 0;
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
				sendto_realops("[TKL ERROR] ERROR: Trying to add a spamfilter which does not compile. "
					       " ERROR='%s', Spamfilter='%s', from='%s'",
					       err, match_string, sptr->name);
				return 0;
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
		return 0;

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
			 * cptr's direction, because cptr will do the same thing and
			 * send it back to his servers (except us)... no need for a
			 * double networkwide flood ;p. -- Syzop
			 */
			tkl->set_at = MIN(tkl->set_at, set_at);

			if (!tkl->expire_at || !expire_at)
				tkl->expire_at = 0;
			else
				tkl->expire_at = MAX(tkl->expire_at, expire_at);

			if (strcmp(tkl->set_by, parv[5]) < 0)
				safestrdup(tkl->set_by, parv[5]);

			if (type & TKL_GLOBAL)
				tkl_broadcast_entry(1, sptr, cptr, tkl);
		}
		return 0;
	}

	/* Below this line we will only use 'tkl'. No parc/parv reading anymore. */

	RunHook3(HOOKTYPE_TKL_ADD, cptr, sptr, tkl);

	sendnotice_tkl_add(tkl);

	/* spamfilter 'warn' action is special */
	if ((tkl->type & TKL_SPAMF) && (tkl->ptr.spamfilter->action == BAN_ACT_WARN) && (tkl->ptr.spamfilter->target & SPAMF_USER))
		spamfilter_check_users(tkl);

	/* Ban checking executes during run loop for efficiency */
	loop.do_bancheck = 1;

	if (type & TKL_GLOBAL)
		tkl_broadcast_entry(1, sptr, cptr, tkl);

	return 0;
}

/** Delete a TKL using the TKL layer. See m_tkl for parv[] and protocol documentation. */
CMD_FUNC(m_tkl_del)
{
	aTKline *tkl;
	int type;
	char *removed_by;

	if (!IsServer(sptr) && !IsMe(sptr))
		return 0;

	if (parc < 6)
		return 0;

	type = tkl_chartotype(parv[2][0]);
	if (type == 0)
		return 0;

	removed_by = parv[5];

	if (TKLIsServerBanType(type))
	{
		char *usermask = parv[3];
		char *hostmask = parv[4];
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
		char *usermask = parv[3];
		char *hostmask = parv[4];
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
		char *name = parv[4];

		if (*parv[3] == 'H')
			hold = 1;
		tkl = find_tkl_nameban(type, name, hold);
	}
	else if (TKLIsSpamfilterType(type))
	{
		char *match_string;
		unsigned short target;
		unsigned short action;

		if (parc < 9)
		{
			sendto_realops("[BUG] m_tkl called with bogus spamfilter removal request [f/F], from=%s, parc=%d",
				       sptr->name, parc);
			return 0; /* bogus */
		}
		if (parc >= 12)
			match_string = parv[11];
		else if (parc >= 11)
			match_string = parv[10];
		else
			match_string = parv[8];

		if (!(target = spamfilter_gettargets(parv[3], NULL)))
		{
			sendto_realops("Ignoring spamfilter deletion request for '%s' from %s with unknown target type '%s'",
				match_string, sptr->name, parv[3]);
			return 0;
		}

		if (!(action = banact_chartoval(*parv[4])))
		{
			sendto_realops("Ignoring spamfilter deletion request for '%s' from %s with unknown action type '%s'",
				match_string, sptr->name, parv[4]);
			return 0;
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
		return 0; /* Item not found, nothing to remove. */

	if (tkl->flags & TKL_FLAG_CONFIG)
		return 0; /* Item is in the configuration file (persistent) */

	/* broadcast remove msg to opers... */
	sendnotice_tkl_del(removed_by, tkl);

	if (type & TKL_SHUN)
		tkl_check_local_remove_shun(tkl);

	RunHook3(HOOKTYPE_TKL_DEL, cptr, sptr, tkl);

	if (type & TKL_GLOBAL)
		tkl_broadcast_entry(0, sptr, cptr, tkl);


	if (TKLIsBanException(tkl))
	{
		/* Since an exception has been removed we have to re-check if
		 * any connected user is now matched by a ban.
		 * Set flag here, actual checking takes place in main loop.
		 */
		loop.do_bancheck = 1;
	}

	tkl_del_line(tkl);

	return 0;
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
 *     if m_tkl is called internally].
 * [B] match-type must be one of: regex, simple, posix.
 * [C] Could be a regex or a regular string with wildcards, depending on [B]
 */
CMD_FUNC(_m_tkl)
{
	if (!IsServer(sptr) && !IsOper(sptr) && !IsMe(sptr))
		return 0;

	if (parc < 2)
		return 0;

	switch (*parv[1])
	{
		case '+':
			return m_tkl_add(cptr, sptr, recv_mtags, parc, parv);
		case '-':
			return m_tkl_del(cptr, sptr, recv_mtags, parc, parv);
		default:
			break;
	}

	return 0;
}

/** Take an action on the user, such as banning or killing.
 * @author Bram Matthys (Syzop), 2003-present
 * @param sptr     The client which is affected.
 * @param action   The type of ban (one of BAN_ACT_*).
 * @param reason   The ban reason.
 * @param duration The ban duration in seconds.
 * @note This function assumes that sptr is a locally connected user.
 * @retval -1 in case of block/tempshun.
 * @retval -5 in case of kill/zline/gline/etc (-5 = FLUSH_BUFFER).
 *            one should no longer read from 'sptr' as the client
 *            has been freed.
 * @retval 0  no action is taken, the user is exempted.
 */
int _place_host_ban(aClient *sptr, int action, char *reason, long duration)
{
	/* If this is a soft action and the user is logged in, then the ban does not apply.
	 * NOTE: Actually in such a case it would be better if place_host_ban() would not
	 * be called at all. Or at least, the caller should not take any action
	 * (eg: the message should be delivered, the user may connect, etc..)
	 * The following is more like secondary protection in case the caller forgets...
	 */
	if (IsSoftBanAction(action) && IsLoggedIn(sptr))
		return 0;

	RunHookReturnInt4(HOOKTYPE_PLACE_HOST_BAN, sptr, action, reason, duration, !=99);

	switch(action)
	{
		case BAN_ACT_TEMPSHUN:
			/* We simply mark this connection as shunned and do not add a ban record */
			sendto_snomask(SNO_TKL, "Temporary shun added at user %s (%s@%s) [%s]",
				sptr->name,
				sptr->user ? sptr->user->username : "unknown",
				sptr->user ? sptr->user->realhost : GetIP(sptr),
				reason);
			SetShunned(sptr);
			break;
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
			char *tkllayer[9] = {
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

			strlcpy(ip, GetIP(sptr), sizeof(ip));

			if (iConf.ban_include_username && (action != BAN_ACT_ZLINE) && (action != BAN_ACT_GZLINE))
			{
				if (sptr->user)
					strlcpy(user, sptr->user->username, sizeof(user));
				else
					strlcpy(user, "*", sizeof(user));
				tkllayer[3] = user;
			}

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
			tkllayer[4] = ip;
			tkllayer[5] = me.name;
			if (!duration)
				strlcpy(mo, "0", sizeof(mo)); /* perm */
			else
				ircsnprintf(mo, sizeof(mo), "%lld", (long long)(duration + TStime()));
			ircsnprintf(mo2, sizeof(mo2), "%lld", (long long)TStime());
			tkllayer[6] = mo;
			tkllayer[7] = mo2;
			tkllayer[8] = reason;
			m_tkl(&me, &me, NULL, 9, tkllayer);
			if ((action == BAN_ACT_SHUN) || (action == BAN_ACT_SOFT_SHUN))
			{
				find_shun(sptr);
				return -1;
			} else
				return find_tkline_match(sptr, 0);
		}
		case BAN_ACT_SOFT_KILL:
		case BAN_ACT_KILL:
		default:
			return exit_client(sptr, sptr, sptr, NULL, reason);
	}
	return -1;
}

/** This function compares two spamfilters ('one' and 'two') and will return
 * a 'winner' based on which one has the strongest action.
 * If both have equal action then some additional logic is applied simply
 * to ensure we (almost) always return the same winner regardless of the
 * order of the spamfilters (which may differ between servers).
 */
aTKline *choose_winning_spamfilter(aTKline *one, aTKline *two)
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
static int target_is_spamexcept(char *target)
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
 * @param sptr  The user that was doing something bad.
 * @param tk    The TKL entry that matched this user.
 * @param type  The spamfilter type (SPAMF_*)
 *              TODO: Looks redundant?
 */
int _join_viruschan(aClient *sptr, aTKline *tkl, int type)
{
	char *xparv[3], chbuf[CHANNELLEN + 16], buf[2048];
	aChannel *chptr;
	int ret;

	snprintf(buf, sizeof(buf), "0,%s", SPAMFILTER_VIRUSCHAN);
	xparv[0] = sptr->name;
	xparv[1] = buf;
	xparv[2] = NULL;

	/* RECURSIVE CAUTION in case we ever add blacklisted chans */
	spamf_ugly_vchanoverride = 1;
	ret = do_cmd(sptr, sptr, NULL, "JOIN", 2, xparv);
	spamf_ugly_vchanoverride = 0;

	if (ret == FLUSH_BUFFER)
		return FLUSH_BUFFER; /* don't ask me how we could have died... */

	sendnotice(sptr, "You are now restricted to talking in %s: %s",
		SPAMFILTER_VIRUSCHAN, unreal_decodespace(tkl->ptr.spamfilter->tkl_reason));

	chptr = find_channel(SPAMFILTER_VIRUSCHAN, NULL);
	if (chptr)
	{
		MessageTag *mtags = NULL;
		ircsnprintf(chbuf, sizeof(chbuf), "@%s", chptr->chname);
		ircsnprintf(buf, sizeof(buf), "[Spamfilter] %s matched filter '%s' [%s] [%s]",
			sptr->name, tkl->ptr.spamfilter->match->str, cmdname_by_spamftarget(type),
			unreal_decodespace(tkl->ptr.spamfilter->tkl_reason));
		new_message(&me, NULL, &mtags);
		sendto_channel(chptr, &me, NULL, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
		               0, SEND_ALL|SKIP_DEAF, mtags,
		               ":%s NOTICE %s :%s", me.name, chbuf, buf);
		free_message_tags(mtags);
	}
	SetVirus(sptr);
	return 0;
}

/** run_spamfilter: executes the spamfilter on the input string.
 * @param str		The text (eg msg text, notice text, part text, quit text, etc
 * @param target	The spamfilter target (SPAMF_*)
 * @param destination	The destination as a text string (eg: "somenick", can be NULL.. eg for away)
 * @param flags		Any flags (SPAMFLAG_*)
 * @param rettkl	Pointer to an aTKLline struct, _used for special circumstances only_
 * RETURN VALUE:
 * 0 if not matched, non-0 if it should be blocked.
 * Return value can be FLUSH_BUFFER (-2) which means 'sptr' is
 * _NOT_ valid anymore so you should return immediately
 * (like from m_message, m_part, m_quit, etc).
 */
int _run_spamfilter(aClient *sptr, char *str_in, int target, char *destination, int flags, aTKline **rettkl)
{
	aTKline *tkl;
	aTKline *winner_tkl = NULL;
	char *str;
	int ret = -1;
	char *reason = NULL;
#ifdef SPAMFILTER_DETECTSLOW
	struct rusage rnow, rprev;
	long ms_past;
#endif

	if (rettkl)
		*rettkl = NULL; /* initialize to NULL */

	if (target == SPAMF_USER)
		str = str_in;
	else
		str = (char *)StripControlCodes(str_in);

	/* (note: using sptr->user check here instead of IsPerson()
	 * due to SPAMF_USER where user isn't marked as client/person yet.
	 */
	if (!sptr->user || ValidatePermissionsForPath("immune:server-ban:spamfilter",sptr,NULL,NULL,NULL) || IsULine(sptr))
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
		if (IsSoftBanAction(tkl->ptr.spamfilter->action) && IsLoggedIn(sptr))
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
			sendto_realops("[Spamfilter] WARNING: Too slow spamfilter detected (took %ld msec to execute) "
			               "-- spamfilter will be \002REMOVED!\002: %s", ms_past, tkl->ptr.spamfilter->match->str);
			tkl_del_line(tkl);
			return 0; /* Act as if it didn't match, even if it did.. it's gone now anyway.. */
		} else
		if ((SPAMFILTER_DETECTSLOW_WARN > 0) && (ms_past > SPAMFILTER_DETECTSLOW_WARN))
		{
			sendto_realops("[Spamfilter] WARNING: SLOW Spamfilter detected (took %ld msec to execute): %s",
				ms_past, tkl->ptr.spamfilter->match->str);
		}
#endif

		if (ret)
		{
			/* We have a match! */
			char buf[1024];
			char destinationbuf[48];

			if (destination) {
				destinationbuf[0] = ' ';
				strlcpy(destinationbuf+1, destination, sizeof(destinationbuf)-1); /* cut it off */
			} else
				destinationbuf[0] = '\0';

			/* Hold on.. perhaps it's on the exceptions list... */
			if (!winner_tkl && destination && target_is_spamexcept(destination))
				return 0; /* No problem! */

			ircsnprintf(buf, sizeof(buf), "[Spamfilter] %s!%s@%s matches filter '%s': [%s%s: '%s'] [%s]",
				sptr->name, sptr->user->username, sptr->user->realhost,
				tkl->ptr.spamfilter->match->str,
				cmdname_by_spamftarget(target), destinationbuf, str,
				unreal_decodespace(tkl->ptr.spamfilter->tkl_reason));

			sendto_snomask_global(SNO_SPAMF, "%s", buf);
			ircd_log(LOG_SPAMFILTER, "%s", buf);
			RunHook6(HOOKTYPE_LOCAL_SPAMFILTER, sptr, str, str_in, target, destination, tkl);

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
				sendnotice(sptr, "Message to %s blocked: %s", destination, reason);
				break;
			case SPAMF_CHANMSG:
			case SPAMF_CHANNOTICE:
				sendto_one(sptr, NULL, ":%s 404 %s %s :Message blocked: %s",
					me.name, sptr->name, destination, reason);
				break;
			case SPAMF_DCC:
				sendnotice(sptr, "DCC to %s blocked: %s", destination, reason);
				break;
			case SPAMF_AWAY:
				/* hack to deal with 'after-away-was-set-filters' */
				if (sptr->user->away && !strcmp(str_in, sptr->user->away))
				{
					/* free away & broadcast the unset */
					MyFree(sptr->user->away);
					sptr->user->away = NULL;
					sendto_server(sptr, 0, 0, NULL, ":%s AWAY", sptr->name);
				}
				break;
			case SPAMF_TOPIC:
				//...
				sendnotice(sptr, "Setting of topic on %s to that text is blocked: %s",
					destination, reason);
				break;
			default:
				break;
		}
		return -1;
	} else
	if ((tkl->ptr.spamfilter->action == BAN_ACT_WARN) || (tkl->ptr.spamfilter->action == BAN_ACT_SOFT_WARN))
	{
		if ((target != SPAMF_USER) && (target != SPAMF_QUIT))
			sendnumeric(sptr, RPL_SPAMCMDFWD, cmdname_by_spamftarget(target), reason);
		return 0;
	} else
	if ((tkl->ptr.spamfilter->action == BAN_ACT_DCCBLOCK) || (tkl->ptr.spamfilter->action == BAN_ACT_SOFT_DCCBLOCK))
	{
		if (target == SPAMF_DCC)
		{
			sendnotice(sptr, "DCC to %s blocked: %s", destination, reason);
			sendnotice(sptr, "*** You have been blocked from sending files, reconnect to regain permission to send files");
			sptr->flags |= FLAGS_DCCBLOCK;
		}
		return -1;
	} else
	if ((tkl->ptr.spamfilter->action == BAN_ACT_VIRUSCHAN) || (tkl->ptr.spamfilter->action == BAN_ACT_SOFT_VIRUSCHAN))
	{
		if (IsVirus(sptr)) /* Already tagged */
			return 0;

		/* There's a race condition for SPAMF_USER, so 'rettk' is used for SPAMF_USER
		 * when a user is currently connecting and filters are checked:
		 */
		if (!IsClient(sptr))
		{
			if (rettkl)
				*rettkl = tkl;
			return -5;
		}

		join_viruschan(sptr, tkl, target);
		return -5;
	} else
		return place_host_ban(sptr, tkl->ptr.spamfilter->action, reason, tkl->ptr.spamfilter->tkl_duration);

	return 0; /* NOTREACHED */
}

/** CIDR function to compare the first 'mask' bits.
 * @returns 1 if equal, 0 if not.
 * @notes Taken from atheme
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
int _match_user(char *rmask, aClient *acptr, int options)
{
	char mask[NICKLEN+USERLEN+HOSTLEN+8];
	char clientip[IPSZ], maskip[IPSZ];
	char *p = NULL;
	char *nmask = NULL, *umask = NULL, *hmask = NULL;
	int cidr = -1; /* CIDR length, -1 for no CIDR */

	strlcpy(mask, rmask, sizeof(mask));

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
			if (!match_simple(nmask, acptr->name))
				return 0; /* NOMATCH: nick mask did not match */
		}
	}

	if (!(options & (MATCH_MASK_IS_HOST)))
	{
		p = strchr(p ? p : mask, '@');
		if (p)
		{
			char *client_username = (acptr->user && *acptr->user->username) ? acptr->user->username : acptr->username;

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
		char *hostname = acptr->user ? GetHost(acptr) : (MyClient(acptr) ? acptr->local->sockhost : NULL);
		if (hostname && match_simple(hmask, hostname))
			return 1; /* MATCH: visible host */
	}

	/**** Check cloaked host ****/
	if (options & MATCH_CHECK_CLOAKED_HOST)
	{
		if (acptr->user && match_simple(hmask, acptr->user->cloakedhost))
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
			if (acptr->ip && match_simple(hmask, acptr->ip))
				return 1; /* MATCH (IP with wildcards) */
		} else 
		if (strchr(hmask, ':'))
		{
			/* IPv6 hostmask */

			/* We can actually return here on match/nomatch as we don't need to check the
			 * virtual host and things like that since ':' can never be in a hostname.
			 */
			if (!acptr->ip || !strchr(acptr->ip, ':'))
				return 0; /* NOMATCH: hmask is IPv6 address and client is not IPv6 */
			if (!inet_pton6(acptr->ip, clientip))
				return 0; /* NOMATCH: unusual failure */
			if (!inet_pton6(hmask, maskip))
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
			if (acptr->ip && inet_pton4(acptr->ip, clientip) && inet_pton4(hmask, maskip))
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
		char *hostname = acptr->user ? acptr->user->realhost : (MyClient(acptr) ? acptr->local->sockhost : NULL);
		if (hostname && match_simple(hmask, hostname))
			return 1; /* MATCH: hostname match */
	}

	return 0; /* NOMATCH: nothing of the above matched */
}
