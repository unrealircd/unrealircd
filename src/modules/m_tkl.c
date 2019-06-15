/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_tkl.c
 *   TKL Commands and TKL Layer
 *   (C) 1999-2019 The UnrealIRCd Team
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
/* This is all for getrusage and friends.. taken from src/s_debug.c so should be safe. */
#ifdef HPUX
# include <sys/syscall.h>
# define getrusage(a,b) syscall(SYS_GETRUSAGE, a, b)
#endif
#ifdef GETRUSAGE_2
# ifdef _SOLARIS
#  include <sys/time.h>
#  ifdef RUSAGEH
#   include <sys/rusage.h>
#  endif
# endif
# include <sys/resource.h>
#else
#  ifdef TIMES_2
#   include <sys/times.h>
#  endif
#endif

CMD_FUNC(m_gline);
CMD_FUNC(m_shun);
CMD_FUNC(m_tempshun);
CMD_FUNC(m_gzline);
CMD_FUNC(m_tkline);
CMD_FUNC(m_tzline);
CMD_FUNC(m_spamfilter);
int m_tkl_line(aClient *cptr, aClient *sptr, int parc, char *parv[], char* type);

int _tkl_hash(unsigned int c);
char _tkl_typetochar(int type);
int _tkl_chartotype(char c);
char *_tkl_type_string(aTKline *tk);
aTKline *_tkl_add_line(int type, char *usermask, char *hostmask, char *reason, char *setby,
    TS expire_at, TS set_at, TS spamf_tkl_duration, char *spamf_tkl_reason, MatchType match_type, int soft, int flags);
void _tkl_del_line(aTKline *tkl);
static void _tkl_check_local_remove_shun(aTKline *tmp);
void _tkl_expire(aTKline * tmp);
EVENT(_tkl_check_expire);
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
int tkl_ip_hash(char *ip);
int tkl_ip_hash_type(char type);

extern MODVAR char zlinebuf[BUFSIZE];
extern MODVAR aTKline *tklines[TKLISTLEN];
extern int MODVAR spamf_ugly_vchanoverride;


/* Place includes here */
#define MSG_GLINE "GLINE"
#define MSG_SHUN "SHUN"
#define MSG_GZLINE "GZLINE"
#define MSG_KLINE "KLINE"
#define MSG_ZLINE "ZLINE"
#define MSG_SPAMFILTER	"SPAMFILTER"
#define MSG_TEMPSHUN "TEMPSHUN"

ModuleInfo *TklModInfo;

ModuleHeader MOD_HEADER(m_tkl)
= {
	"tkl",	/* Name of module */
	"4.2", /* Version */
	"commands /gline etc", /* Short description of module */
	"3.2-b8-1",
	NULL 
};

MOD_TEST(m_tkl)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	TklModInfo = modinfo;
	EfunctionAdd(modinfo->handle, EFUNC_TKL_HASH, _tkl_hash);
	EfunctionAdd(modinfo->handle, EFUNC_TKL_TYPETOCHAR, TO_INTFUNC(_tkl_typetochar));
	EfunctionAdd(modinfo->handle, EFUNC_TKL_CHARTOTYPE, TO_INTFUNC(_tkl_chartotype));
	EfunctionAddPChar(modinfo->handle, EFUNC_TKL_TYPE_STRING, _tkl_type_string);
	EfunctionAddPVoid(modinfo->handle, EFUNC_TKL_ADD_LINE, TO_PVOIDFUNC(_tkl_add_line));
	EfunctionAddVoid(modinfo->handle, EFUNC_TKL_DEL_LINE, _tkl_del_line);
	EfunctionAddVoid(modinfo->handle, EFUNC_TKL_CHECK_LOCAL_REMOVE_SHUN, _tkl_check_local_remove_shun);
	EfunctionAddVoid(modinfo->handle, EFUNC_TKL_EXPIRE, _tkl_expire);
	EfunctionAddVoid(modinfo->handle, EFUNC_TKL_CHECK_EXPIRE, _tkl_check_expire);
	EfunctionAdd(modinfo->handle, EFUNC_FIND_TKLINE_MATCH, _find_tkline_match);
	EfunctionAdd(modinfo->handle, EFUNC_FIND_SHUN, _find_shun);
	EfunctionAdd(modinfo->handle, EFUNC_FIND_SPAMFILTER_USER, _find_spamfilter_user);
	EfunctionAddPVoid(modinfo->handle, EFUNC_FIND_QLINE, TO_PVOIDFUNC(_find_qline));
	EfunctionAddPVoid(modinfo->handle, EFUNC_FIND_TKLINE_MATCH_ZAP, TO_PVOIDFUNC(_find_tkline_match_zap));
	EfunctionAddVoid(modinfo->handle, EFUNC_TKL_STATS, _tkl_stats);
	EfunctionAddVoid(modinfo->handle, EFUNC_TKL_SYNCH, _tkl_synch);
	EfunctionAdd(modinfo->handle, EFUNC_M_TKL, _m_tkl);
	EfunctionAdd(modinfo->handle, EFUNC_PLACE_HOST_BAN, _place_host_ban);
	EfunctionAdd(modinfo->handle, EFUNC_DOSPAMFILTER, _run_spamfilter);
	EfunctionAdd(modinfo->handle, EFUNC_DOSPAMFILTER_VIRUSCHAN, _join_viruschan);
	EfunctionAddVoid(modinfo->handle, EFUNC_SPAMFILTER_BUILD_USER_STRING, _spamfilter_build_user_string);
	EfunctionAdd(modinfo->handle, EFUNC_MATCH_USER, _match_user);
	return MOD_SUCCESS;
}

/* This is called on module init, before Server Ready */
MOD_INIT(m_tkl)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, MSG_GLINE, m_gline, 3, M_OPER);
	CommandAdd(modinfo->handle, MSG_SHUN, m_shun, 3, M_OPER);
	CommandAdd(modinfo->handle, MSG_TEMPSHUN, m_tempshun, 2, M_OPER);
	CommandAdd(modinfo->handle, MSG_ZLINE, m_tzline, 3, M_OPER);
	CommandAdd(modinfo->handle, MSG_KLINE, m_tkline, 3, M_OPER);
	CommandAdd(modinfo->handle, MSG_GZLINE, m_gzline, 3, M_OPER);
	CommandAdd(modinfo->handle, MSG_SPAMFILTER, m_spamfilter, 7, M_OPER);
	CommandAdd(modinfo->handle, MSG_TKL, _m_tkl, MAXPARA, M_OPER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_tkl)
{
	EventAdd(TklModInfo->handle, "tklexpire", 5, 0, tkl_check_expire, NULL);
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_tkl)
{
	return MOD_SUCCESS;
}

/** Return unique spamfilter id for aTKline */
char *spamfilter_id(aTKline *tk)
{
	static char buf[128];

	snprintf(buf, sizeof(buf), "%p", (void *)tk);
	return buf;
}

/*
** m_gline (oper function - /TKL takes care of distribution)
** /gline [+|-]u@h mask time :reason
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

CMD_FUNC(m_tkline)
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

CMD_FUNC(m_tzline)
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

/*
** m_tkl_line (oper function - /TKL takes care of distribution)
** /gline [+|-]u@h mask time :reason
**
** parv[1] = [+|-]u@h mask
** parv[2] = for how long
** parv[3] = reason
*/

int m_tkl_line(aClient *cptr, aClient *sptr, int parc, char *parv[], char* type)
{
	TS secs;
	int whattodo = 0;	/* 0 = add  1 = del */
	TS i;
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
		NULL,			/*5  setby */
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
			sendnotice(sptr, "[error] For (weird) technical reasons you cannot start the host with a ':', sorry");
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
				ircsnprintf(mo, sizeof(mo), "%li", DEFAULT_BANTIME + TStime());
			else
				ircsnprintf(mo, sizeof(mo), "%li", secs); /* "0" */
		}
		else
			ircsnprintf(mo, sizeof(mo), "%li", secs + TStime());
		ircsnprintf(mo2, sizeof(mo2), "%li", TStime());
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

int spamfilter_usage(aClient *sptr)
{
	sendnotice(sptr, "Use: /spamfilter [add|del|remove|+|-] [-simple|-regex|-posix] [type] [action] [tkltime] [tklreason] [regex]");
	sendnotice(sptr, "See '/helpop ?spamfilter' for more information.");
	sendnotice(sptr, "For an easy way to remove an existing spamfilter, use '/spamfilter del' without additional parameters");
	return 0;
}

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
		NULL,		/*  5 setby */
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
	tkllayer[3] = spamfilter_target_inttostring(tk->subtype); /* target(s) */
	mo[0] = banact_valtochar(tk->ptr.spamf->action);
	mo[1] = '\0';
	tkllayer[4] = mo; /* action */
	tkllayer[5] = make_nick_user_host(sptr->name, sptr->user->username, GetHost(sptr));
	tkllayer[8] = "-";
	tkllayer[9] = "-";
	tkllayer[10] = unreal_match_method_valtostr(tk->ptr.spamf->expr->type); /* matching type */
	tkllayer[11] = tk->reason; /* regex */
	ircsnprintf(mo2, sizeof(mo2), "%li", TStime());
	tkllayer[7] = mo2; /* deletion time */

	m_tkl(&me, &me, NULL, 12, tkllayer);

	return 0;
}

/** /spamfilter [add|del|remove|+|-] [match-type] [type] [action] [tkltime] [reason] [regex]
 *                   1                    2         3        4        5        6        7
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
		NULL,		/*  5 setby */
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
		ircsnprintf(mo, sizeof(mo), "%li", SPAMFILTER_BAN_TIME);
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
	 * :servername 229 destname F <target> <action> <num> <num> <num> <reason> <setby> :<regex>
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
		ircsnprintf(mo2, sizeof(mo2), "%li", TStime());
		tkllayer[7] = mo2;
	}

	m_tkl(&me, &me, NULL, 12, tkllayer);

	return 0;
}

/** tkl hash method.
 * NOTE1: the input value 'c' is assumed to be in range a-z or A-Z!
 * NOTE2: don't blindly change the hashmethod, some things depend on
 *        'z'/'Z' getting the same bucket.
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
		if (type & TKL_NICK)
			return 'Q';
	} else {
		if (type & TKL_KILL)
			return 'k';
		if (type & TKL_ZAP)
			return 'z';
		if (type & TKL_SPAMF)
			return 'f';
		if (type & TKL_NICK)
			return 'q';
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
			return TKL_NICK|TKL_GLOBAL;
		case 'k':
			return TKL_KILL;
		case 'z':
			return TKL_ZAP;
		case 'f':
			return TKL_SPAMF;
		case 'q':
			return TKL_NICK;
		default:
			return 0;
	}
	/* NOTREACHED */
}

/** Used for finding out which element of the tkl_ip hash table is used (primary element) */
int tkl_ip_hash(char *ip)
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

/** Used for finding out which tkl_ip hash table needs to be used (secondary element).
 * NOTE: Returns -1 for types that are never on the TKL ip hash table, such as spamfilter.
 *       This can be used by the caller as a quick way to find out if the type is supported.
 */
int tkl_ip_hash_type(char type)
{
	if ((type == 'Z') || (type == 'z'))
		return 0;
	else if (type == 'G')
		return 1;
	else if (type == 'k')
		return 2;
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

/*
 *  type =  TKL_*
 *	usermask@hostmask
 *	reason
 *	setby = whom set it
 *	expire_at = when to expire - 0 if not to expire
 *	set_at    = was set at
 *  spamf_tkl_duration = duration of *line placed by spamfilter [1]
 *  spamf_tkl_reason = escaped reason field for *lines placed by spamfilter [1]
 *
 *  [1]: only relevant for spamfilters, else ignored (eg 0, NULL).
*/

aTKline *_tkl_add_line(int type, char *usermask, char *hostmask, char *reason, char *setby,
                       TS expire_at, TS set_at,
                       TS spamf_tkl_duration, char *spamf_tkl_reason, MatchType match_type,
                       int soft, int flags)
{
	aTKline *tkl;
	int index, index2;
	aMatch *m = NULL;

	/* Pre-allocate etc check for spamfilters that fail to compile.
	 * This could happen if for example the regex engine supports
	 * a feature on server X, but not * on server Y.
	 */
	if (type & TKL_SPAMF)
	{
		char *err = NULL;
		m = unreal_create_match(match_type, reason, &err);
		if (!m)
		{
			sendto_realops("[TKL ERROR] ERROR: Spamfilter was added but did not compile. ERROR='%s', Spamfilter='%s'",
				err, reason);
			return NULL;
		}
	}

	tkl = MyMallocEx(sizeof(aTKline));

	tkl->type = type;
	tkl->flags = flags;
	tkl->expire_at = expire_at;
	tkl->set_at = set_at;
	strlcpy(tkl->usermask, usermask, sizeof(tkl->usermask));
	tkl->hostmask = strdup(hostmask);
	tkl->reason = strdup(reason);
	tkl->setby = strdup(setby);
	if (type & TKL_SPAMF)
	{
		/* Need to set some additional flags like 'targets' and 'action'.. */
		tkl->subtype = spamfilter_gettargets(usermask, NULL);
		tkl->ptr.spamf = MyMallocEx(sizeof(Spamfilter));
		tkl->ptr.spamf->expr = m;
		tkl->ptr.spamf->action = banact_chartoval(*hostmask);
		tkl->expire_at = 0; /* temporary spamfilters are NOT supported! (makes no sense) */
		if (!spamf_tkl_reason)
		{
			/* no exttkl support, use default values... */
			tkl->ptr.spamf->tkl_duration = SPAMFILTER_BAN_TIME;
			tkl->ptr.spamf->tkl_reason = strdup(unreal_encodespace(SPAMFILTER_BAN_REASON));
		} else {
			tkl->ptr.spamf->tkl_duration = spamf_tkl_duration;
			tkl->ptr.spamf->tkl_reason = strdup(spamf_tkl_reason); /* already encoded */
		}
		if (tkl->subtype & SPAMF_USER)
			loop.do_bancheck_spamf_user = 1;
		if (tkl->subtype & SPAMF_AWAY)
			loop.do_bancheck_spamf_away = 1;

	} else {
		if (soft)
			tkl->subtype = TKL_SUBTYPE_SOFT;
	}

	/* For ip hash table TKL's... */
	index = tkl_ip_hash_type(tkl_typetochar(type));
	if (index >= 0)
	{
		index2 = tkl_ip_hash(tkl->hostmask);
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

void _tkl_del_line(aTKline *tkl)
{
	int index, index2;
	int found = 0;

	/* Try to find it in the ip TKL hash table first (if this applies) */
	index = tkl_ip_hash_type(tkl_typetochar(tkl->type));
	if (index >= 0)
	{
		index2 = tkl_ip_hash(tkl->hostmask);
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

	/* Free the entry */
	MyFree(tkl->hostmask);
	MyFree(tkl->reason);
	MyFree(tkl->setby);
	if (tkl->type & TKL_SPAMF && tkl->ptr.spamf)
	{
		unreal_delete_match(tkl->ptr.spamf->expr);
		if (tkl->ptr.spamf->tkl_reason)
			MyFree(tkl->ptr.spamf->tkl_reason);
		MyFree(tkl->ptr.spamf);
	}
	MyFree(tkl);
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

				if ((*tmp->hostmask >= '0') && (*tmp->hostmask <= '9'))
					is_ip = 1;
				else
					is_ip = 0;

				if (is_ip == 0 ?
				    (!match(tmp->hostmask, chost) && !match(tmp->usermask, cname)) : 
				    (!match(tmp->hostmask, chost) || !match(tmp->hostmask, cip))
				    && !match(tmp->usermask, cname))
				{
					/*
					  before blindly marking this user as un-shunned, we need to check
					  if the user is under any other existing shuns. (#0003906)
					  Unfortunately, this requires crazy amounts of indentation ;-).

					  This enumeration code is based off of _tkl_stats()
					 */
					keep_shun = 0;
					for(tk = tklines[tkl_hash('s')]; tk && !keep_shun; tk = tk->next)
						if(tk != tmp && !match(tk->usermask, cname))
						{
							if ((*tk->hostmask >= '0') && (*tk->hostmask <= '9')
							    /* the hostmask is an IP */
							    && (!match(tk->hostmask, chost) || !match(tk->hostmask, cip)))
								keep_shun = 1;
							else
								/* the hostmask is not an IP */
								if (!match(tk->hostmask, chost) && !match(tk->usermask, cname))
									keep_shun = 1;
						}

					if(!keep_shun)
					{
						ClearShunned(acptr);
#ifdef SHUN_NOTICES
						sendnotice(acptr, "*** You are no longer shunned");
#endif
					}
				}
			}
	}
}

void _tkl_expire(aTKline * tmp)
{
	char *whattype = tkl_type_string(tmp);

	if (!tmp)
		return;

	whattype[0] = 0;

	if ((tmp->expire_at == 0) || (tmp->expire_at > TStime()))
	{
		sendto_ops("tkl_expire(): expire for not-yet-expired tkline %s@%s",
		           tmp->usermask, tmp->hostmask);
		return;
	}
	if (!(tmp->type & TKL_NICK))
	{
		sendto_snomask(SNO_TKL,
		    "*** Expiring %s (%s@%s) made by %s (Reason: %s) set %li seconds ago",
		    whattype, tmp->usermask, tmp->hostmask, tmp->setby, tmp->reason,
		    TStime() - tmp->set_at);
		ircd_log
		    (LOG_TKL, "Expiring %s (%s@%s) made by %s (Reason: %s) set %li seconds ago",
		    whattype, tmp->usermask, tmp->hostmask, tmp->setby, tmp->reason,
		    TStime() - tmp->set_at);
	}
	else if (!(*tmp->usermask == 'H')) /* Q-Line but not a hold */
	{
		sendto_snomask(SNO_TKL,
			"*** Expiring %s (%s) made by %s (Reason: %s) set %li seconds ago",
			whattype, tmp->hostmask, tmp->setby, tmp->reason, 
			TStime() - tmp->set_at);
		ircd_log
			(LOG_TKL, "Expiring %s (%s) made by %s (Reason: %s) set %li seconds ago",
			whattype, tmp->hostmask, tmp->setby, tmp->reason, TStime() - tmp->set_at);
	}
	if (tmp->type & TKL_SHUN)
		tkl_check_local_remove_shun(tmp);

	RunHook5(HOOKTYPE_TKL_DEL, NULL, NULL, tmp, 0, NULL);
	tkl_del_line(tmp);
}

EVENT(_tkl_check_expire)
{
	aTKline *tkl, *next;
	TS nowtime;
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
					tkl_expire(tkl);
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
				tkl_expire(tkl);
			}
		}
	}
}

/** Helper function for find_tkline_match() */
int find_tkline_match_matcher(aClient *cptr, int skip_soft, aTKline *tkl)
{
	char uhost[NICKLEN+HOSTLEN+1];
	ConfigItem_except *excepts;
	int match_type = 0;
	Hook *hook;

	if ((tkl->type & TKL_SHUN) || (tkl->type & TKL_SPAMF) || (tkl->type & TKL_NICK))
		return 0;

	if (skip_soft && (tkl->subtype & TKL_SUBTYPE_SOFT))
		return 0;

	snprintf(uhost, sizeof(uhost), "%s@%s", tkl->usermask, tkl->hostmask);

	if (match_user(uhost, cptr, MATCH_CHECK_REAL))
	{
		/* If hard-ban, or soft-ban&unauthenticated.. */
		if (!(tkl->subtype & TKL_SUBTYPE_SOFT) ||
		    ((tkl->subtype & TKL_SUBTYPE_SOFT) && !IsLoggedIn(cptr)))
		{
			/* Found match. Now check for exception... */
			if (((tkl->type & TKL_KILL) || (tkl->type & TKL_ZAP)) && !(tkl->type & TKL_GLOBAL))
				match_type = CONF_EXCEPT_BAN;
			else
				match_type = CONF_EXCEPT_TKL;

			for (excepts = conf_except; excepts; excepts = excepts->next)
			{
				if (excepts->flag.type != match_type || (match_type == CONF_EXCEPT_TKL &&
					excepts->type != tkl->type))
					continue;

				if (match_user(excepts->mask, cptr, MATCH_CHECK_REAL))
					return 0; /* exempt by except block */
			}
			for (hook = Hooks[HOOKTYPE_TKL_EXCEPT]; hook; hook = hook->next)
			{
				if (hook->func.intfunc(cptr, tkl) > 0)
					return 0; /* exempt by hook */
			}
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
			return banned_client(cptr, "G-Lined", tkl->reason, 1, 0);
		else
			return banned_client(cptr, "K-Lined", tkl->reason, 0, 0);
	}
	if (tkl->type & TKL_ZAP)
	{
		ircstp->is_ref++;
		return banned_client(cptr, "Z-Lined", tkl->reason, (tkl->type & TKL_GLOBAL)?1:0, 0);
	}

	return 3;
}

int _find_shun(aClient *cptr)
{
	aTKline *lp;
	ConfigItem_except *excepts;
	int match_type = 0;
	Hook *hook;
	int banned = 0;

	if (IsServer(cptr) || IsMe(cptr))
		return -1;

	if (IsShunned(cptr))
		return 1;

	if (ValidatePermissionsForPath("immune:server-ban:shun",cptr,NULL,NULL,NULL))
		return 1;

	for (lp = tklines[tkl_hash('s')]; lp; lp = lp->next)
	{
		char uhost[NICKLEN+HOSTLEN+1];

		if (!(lp->type & TKL_SHUN))
			continue;

		snprintf(uhost, sizeof(uhost), "%s@%s", lp->usermask, lp->hostmask);

		if (match_user(uhost, cptr, MATCH_CHECK_REAL))
		{
			/* If hard-ban, or soft-ban&unauthenticated.. */
			if (!(lp->subtype & TKL_SUBTYPE_SOFT) ||
			    ((lp->subtype & TKL_SUBTYPE_SOFT) && !IsLoggedIn(cptr)))
			{
				/* Found match. Now check for exception... */
				banned = 1;
				match_type = CONF_EXCEPT_TKL;
				for (excepts = conf_except; excepts; excepts = excepts->next)
				{
					if (excepts->flag.type != match_type || (match_type == CONF_EXCEPT_TKL &&
						excepts->type != lp->type))
						continue;

					if (match_user(excepts->mask, cptr, MATCH_CHECK_REAL))
					{
						banned = 0; /* exempt by except block */
						break;
					}
				}
				for (hook = Hooks[HOOKTYPE_TKL_EXCEPT]; hook; hook = hook->next)
				{
					if (hook->func.intfunc(cptr, lp) > 0)
					{
						banned = 0; /* exempt by hook */
						break;
					}
				}
				if (banned)
					break;
			}
		}
	}

	if (!banned)
		return 1;

	SetShunned(cptr);
	return 2;
}

char *SpamfilterMagicHost(char *i)
{
	static char buf[256];

	if (!strchr(i, ':'))
		return i;

	/* otherwise, it's IPv6.. prepend it with [ and append a ] */
	ircsnprintf(buf, sizeof(buf), "[%s]", i);
	return buf;
}

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

int spamfilter_check_users(aTKline *tk)
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
			if (!unreal_match(tk->ptr.spamf->expr, spamfilter_user))
				continue; /* No match */

			/* matched! */
			ircsnprintf(buf, sizeof(buf), "[Spamfilter] %s!%s@%s matches filter '%s': [%s: '%s'] [%s]",
				acptr->name, acptr->user->username, acptr->user->realhost,
				tk->reason,
				"user", spamfilter_user,
				unreal_decodespace(tk->ptr.spamf->tkl_reason));

			sendto_snomask_global(SNO_SPAMF, "%s", buf);
			ircd_log(LOG_SPAMFILTER, "%s", buf);
			RunHook6(HOOKTYPE_LOCAL_SPAMFILTER, acptr, spamfilter_user, spamfilter_user, SPAMF_USER, NULL, tk);
			matches++;
		}
	}

	return matches;
}

int spamfilter_check_all_users(aClient *from, aTKline *tk)
{
	char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64]; /* n!u@h:r */
	int matches = 0;
	aClient *acptr;

	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (IsPerson(acptr))
		{
			spamfilter_build_user_string(spamfilter_user, acptr->name, acptr);
			if (!unreal_match(tk->ptr.spamf->expr, spamfilter_user))
				continue; /* No match */

			/* matched! */
			sendnotice(from, "[Spamfilter] %s!%s@%s matches filter '%s': [%s: '%s'] [%s]",
				acptr->name, acptr->user->username, acptr->user->realhost,
				tk->reason,
				"user", spamfilter_user,
				unreal_decodespace(tk->ptr.spamf->tkl_reason));
			matches++;
		}
	}

	return matches;
}
/*
 * #*ble* will match with #bbleh
 * *ble* will NOT match with #bbleh, will with bbleh
 */
aTKline *_find_qline(aClient *cptr, char *nick, int *ishold)
{
	aTKline *lp;
	int	points = 0;
	ConfigItem_except *excepts;
	*ishold = 0;
	if (IsServer(cptr) || IsMe(cptr))
		return NULL;

	for (lp = tklines[tkl_hash('q')]; lp; lp = lp->next)
	{
		points = 0;

		if (!(lp->type & TKL_NICK))
			continue;

		if (((*lp->hostmask == '#' && *nick == '#') || (*lp->hostmask != '#' && *nick != '#')) && !match(lp->hostmask, nick))
		{
			points = 1;
			break;
		}
	}

	if (points != 1)
		return NULL;

	/* It's a services hold */
	if (*lp->usermask == 'H')
	{
		*ishold = 1;
		return lp;
	}

	for (excepts = conf_except; excepts; excepts = excepts->next)
	{
		if (excepts->flag.type != CONF_EXCEPT_TKL || excepts->type != TKL_NICK)
			continue;
		if (match_user(excepts->mask, cptr, MATCH_CHECK_REAL))
			return NULL; /* exempt */
	}

	return lp;
}

/** Helper function for find_tkline_match_zap() */
aTKline *find_tkline_match_zap_matcher(aClient *cptr, aTKline *tkl)
{
	ConfigItem_except *excepts;
	Hook *hook;

	if ((tkl->type & TKL_ZAP) && match_user(tkl->hostmask, cptr, MATCH_CHECK_IP))
	{
		for (excepts = conf_except; excepts; excepts = excepts->next)
		{
			/* This used to be:
			 * if (excepts->flag.type != CONF_EXCEPT_TKL || excepts->type != tkl->type)
			 * It now checks for 'except ban', hope this is what most people want,
			 * it is at least the same as in find_tkline_match, which is how it currently
			 * is when a user is connected. -- Syzop/20081221
			 */
			if (excepts->flag.type != CONF_EXCEPT_BAN)
				continue;
			if (match_user(excepts->mask, cptr, MATCH_CHECK_IP))
				return NULL; /* exempt */
		}
		for (hook = Hooks[HOOKTYPE_TKL_EXCEPT]; hook; hook = hook->next)
			if (hook->func.intfunc(cptr, tkl) > 0)
				return NULL; /* exempt */

		return tkl;
	}
	return NULL;
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
	char *setby;
} TKLFlag;

static void parse_tkl_para(char *para, TKLFlag *flag)
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
				if (flag->setby || !(tmp = strtok(NULL, " ")))
					continue;
				if (what == '+')
					flag->flags |= BY_SETBY;
				else
					flag->flags |= NOT_BY_SETBY;
				flag->setby = tmp;
				break;
		}
	}
}

void tkl_stats_matcher(aClient *cptr, int type, char *para, TKLFlag *tklflags, aTKline *tk)
{
	if (!BadPtr(para))
	{
		if (tklflags->flags & BY_MASK)
		{
			if (tk->type & TKL_NICK)
			{
				if (match(tklflags->mask, tk->hostmask))
					return;
			}
			else if (match(tklflags->mask, make_user_host(tk->usermask, tk->hostmask)))
				return;
		}
		if (tklflags->flags & NOT_BY_MASK)
		{
			if (tk->type & TKL_NICK)
			{
				if (!match(tklflags->mask, tk->hostmask))
					return;
			}
			else if (!match(tklflags->mask, make_user_host(tk->usermask, tk->hostmask)))
				return;
		}
		if (tklflags->flags & BY_REASON)
			if (match(tklflags->reason, tk->reason))
				return;
		if (tklflags->flags & NOT_BY_REASON)
			if (!match(tklflags->reason, tk->reason))
				return;
		if (tklflags->flags & BY_SETBY)
			if (match(tklflags->setby, tk->setby))
				return;
		if (tklflags->flags & NOT_BY_SETBY)
			if (!match(tklflags->setby, tk->setby))
				return;
	}
	if (tk->type == (TKL_KILL | TKL_GLOBAL))
	{
		sendnumeric(cptr, RPL_STATSGLINE, 'G',
			   (tk->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   tk->usermask, tk->hostmask,
			   (tk->expire_at != 0) ? (tk->expire_at - TStime()) : 0,
			   (TStime() - tk->set_at), tk->setby, tk->reason);
	}
	if (tk->type == (TKL_ZAP | TKL_GLOBAL))
	{
		sendnumeric(cptr, RPL_STATSGLINE, 'Z',
			   (tk->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   tk->usermask, tk->hostmask,
			   (tk->expire_at != 0) ? (tk->expire_at - TStime()) : 0,
			   (TStime() - tk->set_at), tk->setby, tk->reason);
	}
	if (tk->type == (TKL_SHUN | TKL_GLOBAL))
	{
		sendnumeric(cptr, RPL_STATSGLINE, 's',
			   (tk->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   tk->usermask, tk->hostmask,
			   (tk->expire_at != 0) ? (tk->expire_at - TStime()) : 0,
			   (TStime() - tk->set_at), tk->setby, tk->reason);
	}
	if (tk->type == (TKL_KILL))
	{
		sendnumeric(cptr, RPL_STATSGLINE, 'K',
			   (tk->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   tk->usermask, tk->hostmask,
			   (tk->expire_at != 0) ? (tk->expire_at - TStime()) : 0,
			   (TStime() - tk->set_at), tk->setby, tk->reason);
	}
	if (tk->type == (TKL_ZAP))
	{
		sendnumeric(cptr, RPL_STATSGLINE, 'z',
			   (tk->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   tk->usermask, tk->hostmask,
			   (tk->expire_at != 0) ? (tk->expire_at - TStime()) : 0,
			   (TStime() - tk->set_at), tk->setby, tk->reason);
	}
	if (tk->type & TKL_SPAMF)
	{
		sendnumeric(cptr, RPL_STATSSPAMF,
			(tk->type & TKL_GLOBAL) ? 'F' : 'f',
			unreal_match_method_valtostr(tk->ptr.spamf->expr->type),
			spamfilter_target_inttostring(tk->subtype),
			banact_valtostring(tk->ptr.spamf->action),
			(tk->expire_at != 0) ? (tk->expire_at - TStime()) : 0,
			TStime() - tk->set_at,
			tk->ptr.spamf->tkl_duration, tk->ptr.spamf->tkl_reason,
			tk->setby,
			tk->reason);
		if (para && !strcasecmp(para, "del"))
		{
			char *hash = spamfilter_id(tk);
			if (tk->type & TKL_GLOBAL)
			{
				sendtxtnumeric(cptr, "To delete this spamfilter, use /SPAMFILTER del %s", hash);
				sendtxtnumeric(cptr, "-");
			} else {
				sendtxtnumeric(cptr, "This spamfilter is stored in the configuration file and cannot be removed with /SPAMFILTER del");
				sendtxtnumeric(cptr, "-");
			}
		}
	}
	if (tk->type & TKL_NICK)
	{
		sendnumeric(cptr, RPL_STATSQLINE, (tk->type & TKL_GLOBAL) ? 'Q' : 'q',
			tk->hostmask, (tk->expire_at != 0) ? (tk->expire_at - TStime()) : 0,
			TStime() - tk->set_at, tk->setby, tk->reason);
	}
}

void _tkl_stats(aClient *cptr, int type, char *para)
{
	aTKline *tk;
	TKLFlag tklflags;
	int index, index2;

	if (!BadPtr(para))
		parse_tkl_para(para, &tklflags);

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

void tkl_synch_send_entry(aClient *sptr, aTKline *tkl)
{
	char typ;

	if (!(tkl->type & TKL_GLOBAL))
		return; /* nothing to sync */

	typ = tkl_typetochar(tkl->type);

	if ((tkl->type & TKL_SPAMF) && (sptr->local->proto & PROTO_TKLEXT2))
	{
		sendto_one(sptr, NULL, ":%s TKL + %c %s %s %s %li %li %li %s %s :%s", me.name,
			   typ,
			   tkl->usermask, tkl->hostmask, tkl->setby,
			   tkl->expire_at, tkl->set_at,
			   tkl->ptr.spamf->tkl_duration, tkl->ptr.spamf->tkl_reason,
			   unreal_match_method_valtostr(tkl->ptr.spamf->expr->type),
			   tkl->reason);
	} else
	if ((tkl->type & TKL_SPAMF) && (sptr->local->proto & PROTO_TKLEXT))
	{
		sendto_one(sptr, NULL, ":%s TKL + %c %s %s %s %li %li %li %s :%s", me.name,
			   typ,
			   tkl->usermask, tkl->hostmask, tkl->setby,
			   tkl->expire_at, tkl->set_at,
			   tkl->ptr.spamf->tkl_duration, tkl->ptr.spamf->tkl_reason,
			   tkl->reason);
	} else
	{
		/* All other types (non-spamfilter)... */
		sendto_one(sptr, NULL, ":%s TKL + %c %s%s %s %s %li %li :%s", me.name,
			   typ,
			   (tkl->subtype & TKL_SUBTYPE_SOFT) ? "%" : "",
			   *tkl->usermask ? tkl->usermask : "*", tkl->hostmask, tkl->setby,
			   tkl->expire_at, tkl->set_at, tkl->reason);
	}
}

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
				tkl_synch_send_entry(sptr, tkl);
			}
		}
	}

	/* Then, regular entries.. */
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			tkl_synch_send_entry(sptr, tkl);
		}
	}
}

/** Show TKL type as a string (used when adding/removing) */
char *_tkl_type_string(aTKline *tk)
{
	static char txt[256];

	*txt = '\0';

	if (!(tk->type & TKL_SPAMF) && (tk->subtype == TKL_SUBTYPE_SOFT))
		strlcpy(txt, "Soft ", sizeof(txt));

	switch (tk->type)
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
		case TKL_NICK | TKL_GLOBAL:
			strlcat(txt, "Global Q-Line", sizeof(txt));
			break;
		case TKL_NICK:
			strlcat(txt, "Q-Line", sizeof(txt));
			break;
		case TKL_SPAMF:
			strlcat(txt, "Local Spamfilter", sizeof(txt));
			break;
		case TKL_SPAMF | TKL_GLOBAL:
			strlcat(txt, "Spamfilter", sizeof(txt));
			break;
		default:
			strlcat(txt, "Unknown *-Line", sizeof(txt));
	}

	return txt;
}

/** Find a TKL entry by type and user@mask (and some more) */
aTKline *find_tkline(int type, int softban, char *usermask, char *hostmask, char *spamfilter)
{
	char tpe = tkl_typetochar(type);
	aTKline *head, *tk;

	head = tkl_find_head(tpe, hostmask, tklines[tkl_hash(tpe)]);
	for (tk = head; tk; tk = tk->next)
	{
		if (tk->type == type)
		{
			if (type & TKL_NICK)
			{
				if (!stricmp(tk->hostmask, hostmask))
					return tk;
			} else
			if (type & TKL_SPAMF)
			{
				if (!strcmp(tk->hostmask, hostmask) && !strcmp(tk->usermask, usermask) &&
				    !stricmp(tk->reason, spamfilter))
				{
					return tk;
				}
			} else /* all other types... */
			if (!stricmp(tk->hostmask, hostmask) && !stricmp(tk->usermask, usermask))
			{
				/* And an extra check for soft/hard ban mismatches.. */
				if ((tk->subtype & TKL_SUBTYPE_SOFT) == softban)
					return tk;
			}
		}
	}
	return NULL; /* Not found */
}

/** Add a TKL using the TKL layer. See m_tkl for parv[] and protocol documentation. */
CMD_FUNC(m_tkl_add)
{
	aTKline *tk;
	int type;
	char gmt[256], gmt2[256];
	TS expiry_1, setat_1, spamf_tklduration = 0;
	MatchType spamf_match_method = MATCH_PCRE_REGEX; /* default */
	char *reason = NULL, *timeret;
	int softban = 0;

	/* we rely on servers to be failsafe.. */
	if (!IsServer(sptr) && !IsMe(sptr))
		return 0;

	if (parc < 9)
		return 0;

	/* In case of a soft ban, strip the percent sign early,
	 * so parv[3] (username) is really the username without any prefix.
	 * Set the 'softban' flag if this is the case.
	 */
	if ((parc > 3) && (parv[3][0] == '%'))
	{
		softban = 1;
		parv[3] = parv[3]+1;
	}

	type = tkl_chartotype(parv[2][0]);
	if (!type)
		return 0;

	expiry_1 = atol(parv[6]);
	setat_1 = atol(parv[7]);
	reason = parv[8];

	if (expiry_1 < 0)
	{
		sendto_realops("Invalid TKL entry from %s, negative expire time (%ld) -- not added. Clock on other server incorrect?",
			sptr->name, (long)expiry_1);
		return 0;
	}

	if (setat_1 < 0)
	{
		sendto_realops("Invalid TKL entry from %s, negative set-at time (%ld) -- not added. Clock on other server incorrect?",
			sptr->name, (long)setat_1);
		return 0;
	}

	if ((type & TKL_SPAMF) && (parc >= 11))
	{
		if (parc >= 12)
		{
			reason = parv[11];
			if (!strcasecmp(parv[10], "posix"))
			{
				sendto_realops("Ignoring spamfilter from %s. Spamfilter is of type 'posix' (TRE) which "
					       "is not supported in UnrealIRCd 5. Suggestion: upgrade the other server.",
					       sptr->name);
				return 0;
			}
			spamf_match_method = unreal_match_method_strtoval(parv[10]);
			if (spamf_match_method == 0)
			{
				sendto_realops("Ignoring spamfilter from %s with unknown match type '%s'",
					sptr->name, parv[10]);
				return 0;
			}
		} else {
			/* Actually this only happens for 3.2.x or lower */
			reason = parv[10];
			sendto_realops("Ignoring spamfilter from %s. Spamfilter is of type 'posix' (TRE) which "
				       "is not supported in UnrealIRCd 5. Suggestion: upgrade the other server.",
				       sptr->name);
			return 0;
		}
		spamf_tklduration = config_checkval(parv[8], CFG_TIME); /* was: atol(parv[8]); */
	}

	/* Search for existing entry... */
	tk = find_tkline(type, softban, parv[3], parv[4], reason);
	if (tk)
	{
		/* The *LINE already exists.
		 * This could be an update to things like reason / duration / ..
		 */

		if (type & TKL_NICK)
		{
			/* for sqline: usermask = H overrides */
			/* FIXME: wait.. is this true? it would mean a temporary
			 * hold from nickserv could cause 'H' to be set and then
			 * on un-hold it would remove our *line ?
			 */
			if (*parv[3] == 'H')
				*tk->usermask = 'H';
		}

		if ((setat_1 < tk->set_at) || (expiry_1 != tk->expire_at) ||
		    strcmp(tk->reason, reason) || strcmp(tk->setby, parv[5]))
		{
			/* here's how it goes:
			 * set_at: oldest wins
			 * expire_at: longest wins
			 * reason: highest strcmp wins
			 * setby: highest strcmp wins
			 * We broadcast the result of this back to all servers except
			 * cptr's direction, because cptr will do the same thing and
			 * send it back to his servers (except us)... no need for a
			 * double networkwide flood ;p. -- Syzop
			 */
			tk->set_at = MIN(tk->set_at, setat_1);

			if (!tk->expire_at || !expiry_1)
				tk->expire_at = 0;
			else
				tk->expire_at = MAX(tk->expire_at, expiry_1);

			if (strcmp(tk->reason, reason) < 0)
				safestrdup(tk->reason, reason);

			if (strcmp(tk->setby, parv[5]) < 0)
				safestrdup(tk->setby, parv[5]);

			/* TODO: improve this notice */
			if (tk->type & TKL_NICK)
			{
				if (*tk->usermask != 'H')
					sendto_snomask(SNO_TKL, "tkl update for %s/reason='%s'/by=%s/set=%ld/expire=%ld [causedby: %s]",
						tk->hostmask, tk->reason, tk->setby, tk->set_at, tk->expire_at, sptr->name);
			} else {
				sendto_snomask(SNO_TKL, "tkl update for %s@%s/reason='%s'/by=%s/set=%ld/expire=%ld [causedby: %s]",
					tk->usermask, tk->hostmask, tk->reason, tk->setby, tk->set_at, tk->expire_at, sptr->name);
			}

			if ((parc == 11) && (type & TKL_SPAMF))
			{
				/* I decided to only send updates to OPT_TKLEXT in this case,
				 * it's pretty useless to send it also to OPT_NOT_TKLEXT because
				 * spamfilter entries are permanent (no expire time), the only stuff
				 * that can differ for non-opt is the 'setby' and 'setat' field...
				 */

				/* FIXME check if this works correctly with TKLEXT2 ? */
				sendto_server(cptr, PROTO_TKLEXT, 0, NULL,
					":%s TKL %s %s %s %s %s %ld %ld %ld %s :%s", sptr->name,
					parv[1], parv[2], parv[3], parv[4],
					tk->setby, tk->expire_at, tk->set_at, tk->ptr.spamf->tkl_duration,
					tk->ptr.spamf->tkl_reason, tk->reason);
			} 
			else if (type & TKL_GLOBAL)
			{
				sendto_server(cptr, 0, 0, NULL,
					":%s TKL %s %s %s%s %s %s %ld %ld :%s", sptr->name,
					parv[1], parv[2], (softban?"%":""), parv[3], parv[4],
					tk->setby, tk->expire_at, tk->set_at, tk->reason);
			}
		}
		return 0;
	}

	/* Validate set and expiry time */
	timeret = short_date(setat_1);
	if (!timeret)
	{
		sendto_realops("Invalid TKL entry from %s, set-at time is out of range (%ld) -- not added. Clock on other server incorrect or bogus entry.",
			sptr->name, (long)setat_1);
		return 0;
	}
	strlcpy(gmt, timeret, sizeof(gmt));

	timeret = short_date(expiry_1);
	if (!timeret)
	{
		sendto_realops("Invalid TKL entry from %s, expiry time is out of range (%ld) -- not added. Clock on other server incorrect or bogus entry.",
			sptr->name, (long)expiry_1);
		return 0;
	}
	strlcpy(gmt2, timeret, sizeof(gmt2));

	/* Actually add the TKL entry */
	if ((type & TKL_SPAMF) && (parc >= 11))
	{
		tk = tkl_add_line(type, parv[3], parv[4], reason, parv[5],
				  expiry_1, setat_1, spamf_tklduration, parv[9],
				  spamf_match_method, 0, 0);
	} else {
		tk = tkl_add_line(type, parv[3], parv[4], reason, parv[5],
				  expiry_1, setat_1, 0, NULL,
				  0, softban, 0);
	}

	if (!tk)
		return 0; /* ERROR on allocate or something else... */

	RunHook5(HOOKTYPE_TKL_ADD, cptr, sptr, tk, parc, parv);

	if (type & TKL_SPAMF)
	{
		char buf[512];

		ircsnprintf(buf, sizeof(buf),
			   "Spamfilter added: '%s' [target: %s] [action: %s] [reason: %s] on %s GMT (from %s)",
			   reason, parv[3], banact_valtostring(banact_chartoval(*parv[4])),
			   parc >= 10 ? unreal_decodespace(parv[9]) : SPAMFILTER_BAN_REASON,
			   gmt, parv[5]);

		sendto_snomask(SNO_TKL, "*** %s", buf);
		ircd_log(LOG_TKL, "%s", buf);

		if (tk && (tk->ptr.spamf->action == BAN_ACT_WARN) && (tk->subtype & SPAMF_USER))
			spamfilter_check_users(tk);
	} else {
		char buf[512];
		char *tkl_type_str = tkl_type_string(tk); /* eg: "K-Line" */

		if (expiry_1 != 0)
		{
			if (type & TKL_NICK)
			{
				if (*parv[3] != 'H')
					ircsnprintf(buf, sizeof(buf), "%s added for %s on %s GMT (from %s to expire at %s GMT: %s)",
						tkl_type_str, parv[4], gmt, parv[5], gmt2, reason);
			} else {
				ircsnprintf(buf, sizeof(buf), "%s added for %s@%s on %s GMT (from %s to expire at %s GMT: %s)",
					tkl_type_str, parv[3], parv[4], gmt, parv[5], gmt2, reason);
			}
		} else {
			if (type & TKL_NICK)
			{
				if (*parv[3] != 'H')
					ircsnprintf(buf, sizeof(buf), "Permanent %s added for %s on %s GMT (from %s: %s)",
						tkl_type_str, parv[4], gmt, parv[5], reason);
			} else {
				ircsnprintf(buf, sizeof(buf), "Permanent %s added for %s@%s on %s GMT (from %s: %s)",
					tkl_type_str, parv[3], parv[4], gmt, parv[5], reason);
			}
		}

		if (!((type & TKL_NICK) && *parv[3] == 'H'))
		{
			sendto_snomask(SNO_TKL, "*** %s", buf);
			ircd_log(LOG_TKL, "%s", buf);
		}
	}

	/* Ban checking executes during run loop for efficiency */
	loop.do_bancheck = 1;

	if (type & TKL_GLOBAL)
	{
		if ((parc == 12) && (type & TKL_SPAMF))
		{
			/* Oooooh.. so many flavours ! */
			sendto_server(cptr, PROTO_TKLEXT2, 0, NULL,
				":%s TKL %s %s %s %s %s %s %s %s %s %s :%s", sptr->name,
				parv[1], parv[2], parv[3], parv[4], parv[5],
				parv[6], parv[7], parv[8], parv[9], parv[10], parv[11]);
		} else
		if ((parc == 11) && (type & TKL_SPAMF))
		{
			sendto_server(cptr, PROTO_TKLEXT, 0, NULL,
				":%s TKL %s %s %s %s %s %s %s %s %s :%s", sptr->name,
				parv[1], parv[2], parv[3], parv[4], parv[5],
				parv[6], parv[7], parv[8], parv[9], parv[10]);
			sendto_server(cptr, 0, PROTO_TKLEXT, NULL,
				":%s TKL %s %s %s %s %s %s %s :%s", sptr->name,
				parv[1], parv[2], parv[3], parv[4], parv[5],
				parv[6], parv[7], parv[10]);
		} else {
			sendto_server(cptr, 0, 0, NULL,
				":%s TKL %s %s %s%s %s %s %s %s :%s", sptr->name,
				parv[1], parv[2], (softban?"%":""), parv[3], parv[4], parv[5],
				parv[6], parv[7], parv[8]);
		}
	} /* TKL_GLOBAL */

	return 0;
}

/** Delete a TKL using the TKL layer. See m_tkl for parv[] and protocol documentation. */
CMD_FUNC(m_tkl_del)
{
	aTKline *tk;
	int type;
	char gmt[256];
	char *reason = NULL, *timeret;
	int softban = 0;
	char *tkl_type_str;

	if (!IsServer(sptr) && !IsMe(sptr))
		return 0;

	if (parc < 6)
		return 0;

	/* In case of a soft ban, strip the percent sign early,
	 * so parv[3] (username) is really the username without any prefix.
	 * Set the 'softban' flag if this is the case.
	 */
	if ((parc > 3) && (parv[3][0] == '%'))
	{
		softban = 1;
		parv[3] = parv[3]+1;
	}

	type = tkl_chartotype(parv[2][0]);
	if (type == 0)
	{
		return 0;
	}
	if (type & TKL_SPAMF)
	{
		if (parc < 9)
		{
			sendto_realops("[BUG] m_tkl called with bogus spamfilter removal request [f/F], from=%s, parc=%d",
				       sptr->name, parc);
			return 0; /* bogus */
		}
		if (parc >= 12)
			reason = parv[11];
		else if (parc >= 11)
			reason = parv[10];
		else
			reason = parv[8];
	}

	tk = find_tkline(type, softban, parv[3], parv[4], reason);
	if (!tk)
		return 0; /* Item not found */

	if (tk->flags & TKL_FLAG_CONFIG)
		return 0; /* Item is in the configuration file (persistent) */

	tkl_type_str = tkl_type_string(tk); /* eg: "K-Line" */
	timeret = short_date(tk->set_at);
	if (!timeret)
		timeret = "???"; /* invalid time, but allow us to remove it anyway */
	strlcpy(gmt, timeret, sizeof(gmt));

	/* broadcast remove msg to opers... */
	if (type & TKL_NICK)
	{
		if (!(*parv[3] == 'H'))
		{
			sendto_snomask(SNO_TKL, "%s removed %s %s (set at %s - reason: %s)",
				parv[5], tkl_type_str, tk->hostmask, gmt, tk->reason);
			ircd_log(LOG_TKL, "%s removed %s %s (set at %s - reason: %s)",
				parv[5], tkl_type_str, tk->hostmask, gmt, tk->reason);
		}
	}
	else if (type & TKL_SPAMF)
	{
		sendto_snomask(SNO_TKL, "%s removed Spamfilter '%s' (set at %s)",
			       parv[5], tk->reason, gmt);
		ircd_log(LOG_TKL, "%s removed Spamfilter '%s' (set at %s)",
			 parv[5], tk->reason, gmt);
	} else {
		sendto_snomask(SNO_TKL,
			       "%s removed %s %s@%s (set at %s - reason: %s)",
			       parv[5], tkl_type_str, tk->usermask,
			       tk->hostmask, gmt, tk->reason);
		ircd_log(LOG_TKL, "%s removed %s %s@%s (set at %s - reason: %s)",
			 parv[5], tkl_type_str, tk->usermask, tk->hostmask,
			 gmt, tk->reason);
	}

	if (type & TKL_SHUN)
		tkl_check_local_remove_shun(tk);

	RunHook5(HOOKTYPE_TKL_DEL, cptr, sptr, tk, parc, parv);

	/* Broadcast removal to all other servers */
	if (type & TKL_GLOBAL)
	{
		if ((parc > 8) && (type & TKL_SPAMF))
		{
			/* Spamfilter... */
			sendto_server(cptr, 0, 0, NULL, ":%s TKL %s %s %s %s %s %s %s :%s",
				      sptr->name, parv[1], parv[2], parv[3], parv[4], parv[5],
				      parv[6], parv[7], reason);
		} else {
			/* Any other TKL (eg: gline) */
			sendto_server(cptr, 0, 0, NULL, ":%s TKL %s %s %s%s %s %s",
				      sptr->name, parv[1], parv[2], (softban?"%":""), parv[3], parv[4], parv[5]);
		}
	}

	tkl_del_line(tk);
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
 *           add:      remove:    spamfilter:    spamfilter+TKLEXT  spamfilter+TKLEXT2  sqline:
 * parv[ 1]: +         -          +/-            +                  +                   +/-
 * parv[ 2]: type      type       type           type               type                type
 * parv[ 3]: user      user       target         target             target              hold
 * parv[ 4]: host      host       action         action             action              host
 * parv[ 5]: setby     removedby  (un)setby      setby              setby               setby
 * parv[ 6]: expire_at            expire_at (0)  expire_at (0)      expire_at (0)       expire_at
 * parv[ 7]: set_at               set_at         set_at             set_at              set_at
 * parv[ 8]: reason               regex          tkl duration       tkl duration        reason
 * parv[ 9]:                                     tkl reason [A]     tkl reason [A]
 * parv[10]:                                     regex              match-type [B]
 * parv[11]:                                                        match-string [C]
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
				ircsnprintf(mo, sizeof(mo), "%li", duration + TStime());
			ircsnprintf(mo2, sizeof(mo2), "%li", TStime());
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

	/* First, see if the action field differs... */
	if (one->ptr.spamf->action != two->ptr.spamf->action)
	{
		/* We can simply compare the action. Highest (strongest) wins. */
		if (one->ptr.spamf->action > two->ptr.spamf->action)
			return one;
		else
			return two;
	}

	/* Ok, try comparing the regex then.. */
	n = strcmp(one->reason, two->reason);
	if (n < 0)
		return one;
	if (n > 0)
		return two;

	/* Hmm.. regex is identical. Try the 'reason' field. */
	n = strcmp(one->ptr.spamf->tkl_reason, two->ptr.spamf->tkl_reason);
	if (n < 0)
		return one;
	if (n > 0)
		return two;

	/* Hmm.. 'reason' is identical as well.
	 * Make a final decision, could still be identical but would be unlikely.
	 */
	return (one->subtype > two->subtype) ? one : two;
}

/** Checks if 'target' is on the spamfilter exception list.
 * RETURNS 1 if found in list, 0 if not.
 */
static int target_is_spamexcept(char *target)
{
	SpamExcept *e;

	for (e = iConf.spamexcept; e; e = e->next)
	{
		if (!match(e->name, target))
			return 1;
	}
	return 0;
}

int _join_viruschan(aClient *sptr, aTKline *tk, int type)
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
		SPAMFILTER_VIRUSCHAN, unreal_decodespace(tk->ptr.spamf->tkl_reason));

	chptr = find_channel(SPAMFILTER_VIRUSCHAN, NULL);
	if (chptr)
	{
		MessageTag *mtags = NULL;
		ircsnprintf(chbuf, sizeof(chbuf), "@%s", chptr->chname);
		ircsnprintf(buf, sizeof(buf), "[Spamfilter] %s matched filter '%s' [%s] [%s]",
			sptr->name, tk->reason, cmdname_by_spamftarget(type),
			unreal_decodespace(tk->ptr.spamf->tkl_reason));
		new_message(&me, NULL, &mtags);
		sendto_channel(chptr, &me, NULL, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
		               0, SEND_ALL|SKIP_DEAF, mtags,
		               ":%s NOTICE %s :%s", me.name, chbuf, buf);
		free_mtags(mtags);
	}
	SetVirus(sptr);
	return 0;
}

/** run_spamfilter: executes the spamfilter onto the string.
 * @param str		The text (eg msg text, notice text, part text, quit text, etc
 * @param type		The spamfilter type (SPAMF_*)
 * @param target	The target as a text string (can be NULL, eg: for away)
 * @param flags		Any flags (SPAMFLAG_*)
 * @param rettk		Pointer to an aTKLline struct, _used for special circumstances only_
 * RETURN VALUE:
 * 0 if not matched, non-0 if it should be blocked.
 * Return value can be FLUSH_BUFFER (-2) which means 'sptr' is
 * _NOT_ valid anymore so you should return immediately
 * (like from m_message, m_part, m_quit, etc).
 */
 
int _run_spamfilter(aClient *sptr, char *str_in, int type, char *target, int flags, aTKline **rettk)
{
	aTKline *tk;
	aTKline *winner_tk = NULL;
	char *str;
	int ret = -1;
#ifdef SPAMFILTER_DETECTSLOW
	struct rusage rnow, rprev;
	long ms_past;
#endif

	if (rettk)
		*rettk = NULL; /* initialize to NULL */

	if (type == SPAMF_USER)
		str = str_in;
	else
		str = (char *)StripControlCodes(str_in);

	/* (note: using sptr->user check here instead of IsPerson()
	 * due to SPAMF_USER where user isn't marked as client/person yet.
	 */
	if (!sptr->user || ValidatePermissionsForPath("immune:server-ban:spamfilter",sptr,NULL,NULL,NULL) || IsULine(sptr))
		return 0;

	for (tk = tklines[tkl_hash('F')]; tk; tk = tk->next)
	{
		if (!(tk->subtype & type))
			continue;

		if ((flags & SPAMFLAG_NOWARN) && (tk->ptr.spamf->action == BAN_ACT_WARN))
			continue;

		/* If the action is 'soft' (for non-logged in users only) then
		 * don't bother running the spamfilter if the user is logged in.
		 */
		if (IsSoftBanAction(tk->ptr.spamf->action) && IsLoggedIn(sptr))
			continue;

#ifdef SPAMFILTER_DETECTSLOW
		memset(&rnow, 0, sizeof(rnow));
		memset(&rprev, 0, sizeof(rnow));

		getrusage(RUSAGE_SELF, &rprev);
#endif

		ret = unreal_match(tk->ptr.spamf->expr, str);

#ifdef SPAMFILTER_DETECTSLOW
		getrusage(RUSAGE_SELF, &rnow);

		ms_past = ((rnow.ru_utime.tv_sec - rprev.ru_utime.tv_sec) * 1000) +
		          ((rnow.ru_utime.tv_usec - rprev.ru_utime.tv_usec) / 1000);

		if ((SPAMFILTER_DETECTSLOW_FATAL > 0) && (ms_past > SPAMFILTER_DETECTSLOW_FATAL))
		{
			sendto_realops("[Spamfilter] WARNING: Too slow spamfilter detected (took %ld msec to execute) "
			               "-- spamfilter will be \002REMOVED!\002: %s", ms_past, tk->reason);
			tkl_del_line(tk);
			return 0; /* Act as if it didn't match, even if it did.. it's gone now anyway.. */
		} else
		if ((SPAMFILTER_DETECTSLOW_WARN > 0) && (ms_past > SPAMFILTER_DETECTSLOW_WARN))
		{
			sendto_realops("[Spamfilter] WARNING: SLOW Spamfilter detected (took %ld msec to execute): %s",
				ms_past, tk->reason);
		}
#endif

		if (ret)
		{
			/* We have a match! */
			char buf[1024];
			char targetbuf[48];

			if (target) {
				targetbuf[0] = ' ';
				strlcpy(targetbuf+1, target, sizeof(targetbuf)-1); /* cut it off */
			} else
				targetbuf[0] = '\0';

			/* Hold on.. perhaps it's on the exceptions list... */
			if (!winner_tk && target && target_is_spamexcept(target))
				return 0; /* No problem! */

			ircsnprintf(buf, sizeof(buf), "[Spamfilter] %s!%s@%s matches filter '%s': [%s%s: '%s'] [%s]",
				sptr->name, sptr->user->username, sptr->user->realhost,
				tk->reason,
				cmdname_by_spamftarget(type), targetbuf, str,
				unreal_decodespace(tk->ptr.spamf->tkl_reason));

			sendto_snomask_global(SNO_SPAMF, "%s", buf);
			ircd_log(LOG_SPAMFILTER, "%s", buf);
			RunHook6(HOOKTYPE_LOCAL_SPAMFILTER, sptr, str, str_in, type, target, tk);

			/* If we should stop after the first match, we end here... */
			if (SPAMFILTER_STOP_ON_FIRST_MATCH)
			{
				winner_tk = tk;
				break;
			}

			/* Otherwise.. we set 'winner_tk' to the spamfilter with the strongest action. */
			if (!winner_tk)
				winner_tk = tk;
			else
				winner_tk = choose_winning_spamfilter(tk, winner_tk);

			/* and continue.. */
		}
	}

	tk = winner_tk;

	if (!tk)
		return 0; /* NOMATCH, we are done */

	/* Spamfilter matched, take action: */

	if ((tk->ptr.spamf->action == BAN_ACT_BLOCK) || (tk->ptr.spamf->action == BAN_ACT_SOFT_BLOCK))
	{
		switch(type)
		{
			case SPAMF_USERMSG:
			case SPAMF_USERNOTICE:
				sendnotice(sptr, "Message to %s blocked: %s",
					target, unreal_decodespace(tk->ptr.spamf->tkl_reason));
				break;
			case SPAMF_CHANMSG:
			case SPAMF_CHANNOTICE:
				sendto_one(sptr, NULL, ":%s 404 %s %s :Message blocked: %s",
					me.name, sptr->name, target,
					unreal_decodespace(tk->ptr.spamf->tkl_reason));
				break;
			case SPAMF_DCC:
				sendnotice(sptr, "DCC to %s blocked: %s",
					target, unreal_decodespace(tk->ptr.spamf->tkl_reason));
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
					target, unreal_decodespace(tk->ptr.spamf->tkl_reason));
				break;
			default:
				break;
		}
		return -1;
	} else
	if ((tk->ptr.spamf->action == BAN_ACT_WARN) || (tk->ptr.spamf->action == BAN_ACT_SOFT_WARN))
	{
		if ((type != SPAMF_USER) && (type != SPAMF_QUIT))
			sendnumeric(sptr, RPL_SPAMCMDFWD, cmdname_by_spamftarget(type),
				unreal_decodespace(tk->ptr.spamf->tkl_reason));
		return 0;
	} else
	if ((tk->ptr.spamf->action == BAN_ACT_DCCBLOCK) || (tk->ptr.spamf->action == BAN_ACT_SOFT_DCCBLOCK))
	{
		if (type == SPAMF_DCC)
		{
			sendnotice(sptr, "DCC to %s blocked: %s",
				target, unreal_decodespace(tk->ptr.spamf->tkl_reason));
			sendnotice(sptr, "*** You have been blocked from sending files, reconnect to regain permission to send files");
			sptr->flags |= FLAGS_DCCBLOCK;
		}
		return -1;
	} else
	if ((tk->ptr.spamf->action == BAN_ACT_VIRUSCHAN) || (tk->ptr.spamf->action == BAN_ACT_SOFT_VIRUSCHAN))
	{
		if (IsVirus(sptr)) /* Already tagged */
			return 0;

		/* There's a race condition for SPAMF_USER, so 'rettk' is used for SPAMF_USER
		 * when a user is currently connecting and filters are checked:
		 */
		if (!IsClient(sptr))
		{
			if (rettk)
				*rettk = tk;
			return -5;
		}

		join_viruschan(sptr, tk, type);
		return -5;
	} else
		return place_host_ban(sptr, tk->ptr.spamf->action,
			unreal_decodespace(tk->ptr.spamf->tkl_reason), tk->ptr.spamf->tkl_duration);

	return 0; /* NOTREACHED */
}

/* compares the first 'mask' bits. returns 1 if equal, 0 if not.
 * taken from atheme
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
			if (match(nmask, acptr->name) != 0)
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
			if (match(umask, client_username) != 0)
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
		if (hostname && (match(hmask, hostname) == 0))
			return 1; /* MATCH: visible host */
	}

	/**** Check cloaked host ****/
	if (options & MATCH_CHECK_CLOAKED_HOST)
	{
		if (acptr->user && (match(hmask, acptr->user->cloakedhost) == 0))
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
			if (acptr->ip && (match(hmask, acptr->ip) == 0))
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
		if (hostname && (match(hmask, hostname) == 0))
			return 1; /* MATCH: hostname match */
	}

	return 0; /* NOMATCH: nothing of the above matched */
}
