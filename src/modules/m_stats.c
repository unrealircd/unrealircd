/*
 *   IRC - Internet Relay Chat, src/modules/m_stats.c
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

CMD_FUNC(m_stats);

#define MSG_STATS 	"STATS"	

ModuleHeader MOD_HEADER(m_stats)
  = {
	"m_stats",
	"4.0",
	"command /stats", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_stats)
{
	CommandAdd(modinfo->handle, MSG_STATS, m_stats, 3, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_stats)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_stats)
{
	return MOD_SUCCESS;
}

extern MODVAR int  max_connection_count;
extern char *get_client_name2(aClient *, int);

int stats_banversion(aClient *, char *);
int stats_links(aClient *, char *);
int stats_denylinkall(aClient *, char *);
int stats_gline(aClient *, char *);
int stats_exceptban(aClient *, char *);
int stats_allow(aClient *, char *);
int stats_command(aClient *, char *);
int stats_oper(aClient *, char *);
int stats_port(aClient *, char *);
int stats_bannick(aClient *, char *);
int stats_usage(aClient *, char *);
int stats_traffic(aClient *, char *);
int stats_uline(aClient *, char *);
int stats_vhost(aClient *, char *);
int stats_mem(aClient *, char *);
int stats_denylinkauto(aClient *, char *);
int stats_exceptthrottle(aClient *, char *);
int stats_denydcc(aClient *, char *);
int stats_kline(aClient *, char *);
int stats_banrealname(aClient *, char *);
int stats_sqline(aClient *, char *);
int stats_linkinfoint(aClient *, char *, int);
int stats_linkinfo(aClient *, char *);
int stats_linkinfoall(aClient *, char *);
int stats_chanrestrict(aClient *, char *);
int stats_shun(aClient *, char *);
int stats_set(aClient *, char *);
int stats_tld(aClient *, char *);
int stats_uptime(aClient *, char *);
int stats_denyver(aClient *, char *);
int stats_notlink(aClient *, char *);
int stats_class(aClient *, char *);
int stats_officialchannels(aClient *, char *);
int stats_spamfilter(aClient *, char *);
int stats_fdtable(aClient *, char *);

#define SERVER_AS_PARA 0x1
#define FLAGS_AS_PARA 0x2

struct statstab {
	char flag;
	char *longflag;
	int (*func)(aClient *sptr, char *para);
	int options;
};

/* Must be listed lexicographically */
/* Long flags must be lowercase */
struct statstab StatsTable[] = {
	{ 'B', "banversion",	stats_banversion,	0		},
	{ 'C', "link", 		stats_links,		0 		},
	{ 'D', "denylinkall",	stats_denylinkall,	0		},
	{ 'E', "exceptban",	stats_exceptban,	0 		},
	{ 'F', "denydcc",	stats_denydcc,		0 		},
	{ 'G', "gline",		stats_gline,		FLAGS_AS_PARA	},
	{ 'H', "link",	 	stats_links,		0 		},	
	{ 'I', "allow",		stats_allow,		0 		},
	{ 'K', "kline",		stats_kline,		0 		},
	{ 'L', "linkinfoall",	stats_linkinfoall,	SERVER_AS_PARA	},
	{ 'M', "command",	stats_command,		0 		},
	{ 'O', "oper",		stats_oper,		0 		},
	{ 'P', "port",		stats_port,		0 		},
	{ 'Q', "sqline",	stats_sqline,		FLAGS_AS_PARA 	},
	{ 'R', "usage",		stats_usage,		0 		},
	{ 'S', "set",		stats_set,		0		},
	{ 'T', "traffic",	stats_traffic,		0 		},
	{ 'U', "uline",		stats_uline,		0 		},
	{ 'V', "vhost", 	stats_vhost,		0 		},
	{ 'W', "fdtable",       stats_fdtable,          0               },
	{ 'X', "notlink",	stats_notlink,		0 		},	
	{ 'Y', "class",		stats_class,		0 		},	
	{ 'Z', "mem",		stats_mem,		0 		},
	{ 'c', "link", 		stats_links,		0 		},
	{ 'd', "denylinkauto",	stats_denylinkauto,	0 		},
	{ 'e', "exceptthrottle",stats_exceptthrottle,	0		},
	{ 'f', "spamfilter",	stats_spamfilter,	FLAGS_AS_PARA	},	
	{ 'g', "gline",		stats_gline,		FLAGS_AS_PARA	},
	{ 'h', "link", 		stats_links,		0 		},
	{ 'j', "officialchans", stats_officialchannels, 0 		},
	{ 'k', "kline",		stats_kline,		0 		},
	{ 'l', "linkinfo",	stats_linkinfo,		SERVER_AS_PARA 	},
	{ 'm', "command",	stats_command,		0 		},
	{ 'n', "banrealname",	stats_banrealname,	0 		},
	{ 'o', "oper",		stats_oper,		0 		},
	{ 'q', "bannick",	stats_bannick,		FLAGS_AS_PARA	},
	{ 'r', "chanrestrict",	stats_chanrestrict,	0 		},
	{ 's', "shun",		stats_shun,		FLAGS_AS_PARA	},
	{ 't', "tld",		stats_tld,		0 		},
	{ 'u', "uptime",	stats_uptime,		0 		},
	{ 'v', "denyver",	stats_denyver,		0 		},
	{ 'x', "notlink",	stats_notlink,		0 		},	
	{ 'y', "class",		stats_class,		0 		},
	{ 0, 	NULL, 		NULL, 			0		}
};

int stats_compare(char *s1, char *s2)
{
	/* The long stats flags are always lowercase */
	while (*s1 == tolower(*s2))
	{
		if (*s1 == 0)
			return 0;
		s1++;
		s2++;
	}
	return 1;
}	

static inline struct statstab *stats_binary_search(char c) {
	int start = 0;
	int stop = sizeof(StatsTable)/sizeof(StatsTable[0])-1;
	int mid;
	while (start <= stop) {
		mid = (start+stop)/2;
		if (c < StatsTable[mid].flag) 
			stop = mid-1;
		else if (StatsTable[mid].flag == c) 
			return &StatsTable[mid];
		else
			start = mid+1;
	}
	return NULL;
}

static inline struct statstab *stats_search(char *s) {
	int i;
	for (i = 0; StatsTable[i].flag; i++)
		if (!stats_compare(StatsTable[i].longflag,s))
			return &StatsTable[i];
	return NULL;
}

static inline char *stats_combine_parv(char *p1, char *p2)
{
	static char buf[BUFSIZE+1];
        ircsnprintf(buf, sizeof(buf), "%s %s", p1, p2);
	return buf;
}

static inline void stats_help(aClient *sptr)
{
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name, "/Stats flags:");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"B - banversion - Send the ban version list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"b - badword - Send the badwords list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"C - link - Send the link block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"d - denylinkauto - Send the deny link (auto) block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"D - denylinkall - Send the deny link (all) block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"e - exceptthrottle - Send the except throttle block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"E - exceptban - Send the except ban and except tkl block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"f - spamfilter - Send the spamfilter list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"F - denydcc - Send the deny dcc and allow dcc block lists");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"G - gline - Send the gline and gzline list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"  Extended flags: [+/-mrs] [mask] [reason] [setby]");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"   m Return glines matching/not matching the specified mask");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"   r Return glines with a reason matching/not matching the specified reason");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"   s Return glines set by/not set by clients matching the specified name");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"I - allow - Send the allow block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"j - officialchans - Send the offical channels list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"K - kline - Send the ban user/ban ip/except ban block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"l - linkinfo - Send link information");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"L - linkinfoall - Send all link information");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"M - command - Send list of how many times each command was used");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"n - banrealname - Send the ban realname block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"O - oper - Send the oper block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"P - port - Send information about ports");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"q - bannick - Send the ban nick block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"Q - sqline - Send the global qline list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"r - chanrestrict - Send the channel deny/allow block list");
#ifdef DEBUGMODE
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"R - usage - Send usage information");
#endif
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"S - set - Send the set block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"s - shun - Send the shun list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"  Extended flags: [+/-mrs] [mask] [reason] [setby]");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"   m Return shuns matching/not matching the specified mask");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"   r Return shuns with a reason matching/not matching the specified reason");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"   s Return shuns set by/not set by clients matching the specified name");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"t - tld - Send the tld block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"T - traffic - Send traffic information");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"u - uptime - Send the server uptime and connection count");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"U - uline - Send the ulines block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"v - denyver - Send the deny version block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"V - vhost - Send the vhost block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"W - fdtable - Send the FD table listing");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"X - notlink - Send the list of servers that are not current linked");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"Y - class - Send the class block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"Z - mem - Send memory usage information");
}

static inline int stats_operonly_short(char c)
{
	char l;
	if (!OPER_ONLY_STATS)
		return 0;
	if (*OPER_ONLY_STATS == '*')
		return 1;
	if (strchr(OPER_ONLY_STATS, c))
		return 1;
	l = tolower(c);
	/* Hack for the flags that are case insensitive */
	if (l == 'o' || l == 'y' || l == 'k' || l == 'g' || l == 'x' || l == 'c' || 
		l =='f' || l == 'i' || l == 'h' || l == 'm')
	{
		if (islower(c) && strchr(OPER_ONLY_STATS, toupper(c)))
			return 1;
		else if (isupper(c) && strchr(OPER_ONLY_STATS, tolower(c)))
			return 1;
	}
	/* Hack for c/C/H/h */
	if (l == 'c')
	{
		if (strpbrk(OPER_ONLY_STATS, "hH"))
			return 1;
	} else if (l == 'h')
		if (strpbrk(OPER_ONLY_STATS, "cC"))
			return 1;
	return 0;
}

static inline int stats_operonly_long(char *s)
{
	OperStat *os;
	for (os = iConf.oper_only_stats_ext; os; os = (OperStat *)os->next)
	{
		if (!stricmp(os->flag, s))
			return 1;
	}
	return 0;
}

/* This is pretty slow, but it isn't used often so it isn't a big deal */
static inline char *stats_operonly_long_to_short()
{
	static char buffer[BUFSIZE+1];
	int i = 0;
	OperStat *os;
	for (os = iConf.oper_only_stats_ext; os; os = (OperStat *)os->next)
	{
		struct statstab *stat = stats_search(os->flag);
		if (!stat)
			continue;
		if (!strchr(OPER_ONLY_STATS, stat->flag))
			buffer[i++] = stat->flag;
	}
	buffer[i] = 0;
	return buffer;
}

CMD_FUNC(m_stats)
{
	struct statstab *stat;

	if (parc == 3 && parv[2][0] != '+' && parv[2][0] != '-')
	{
		if (hunt_server(cptr, sptr, ":%s STATS %s :%s", 2, parc, parv) != HUNTED_ISME)
			return 0;
	}
	else if (parc == 4 && parv[2][0] != '+' && parv[2][0] != '-')
	{
		if (hunt_server(cptr, sptr, ":%s STATS %s %s %s", 2, parc, parv) != HUNTED_ISME)
			return 0;
	}
	if (parc < 2 || !*parv[1])
	{
		stats_help(sptr);
		sendto_one(sptr, rpl_str(RPL_ENDOFSTATS), me.name, sptr->name, '*');
		return 0;
	}

	/* Decide if we are looking for 1 char or a string */
	if (parv[1][0] && !parv[1][1])
	{
		if (!ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL) && stats_operonly_short(parv[1][0]))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);	
			return 0;
		}	
		/* Old style, we can use a binary search here */
		stat = stats_binary_search(parv[1][0]);
	}
	else
	{
		if (!ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL) && stats_operonly_long(parv[1]))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);	
			return 0;
		}
		/* New style, search the hard way */
		stat = stats_search(parv[1]);
	}
	if (stat)
	{
		/* It was a short flag, so check oper only on long flags */
		if (!parv[1][1])
		{
			if (!ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL) && stats_operonly_long(stat->longflag))
			{
				sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
				return 0;
			}
		}
		/* It was a long flag, so check oper only on short flags */
		else
		{
			if (!ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL) && stats_operonly_short(stat->flag))
			{
				sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);	
				return 0;
			}
		}
		if (stat->options & FLAGS_AS_PARA)
		{
			if (parc > 2 && (parv[2][0] == '+' || parv[2][0] == '-'))
			{
				if (parc > 3)
					stat->func(sptr, stats_combine_parv(parv[2],parv[3]));
				else
					stat->func(sptr, parv[2]);
			}
			else if (parc > 3)
				stat->func(sptr, parv[3]);
			else
				stat->func(sptr, NULL);
		}
		else if (stat->options & SERVER_AS_PARA)
		{
			if (parc > 2)
				stat->func(sptr, parv[2]);
			else
				stat->func(sptr, NULL);
		}
		else
			stat->func(sptr, NULL);
		sendto_one(sptr, rpl_str(RPL_ENDOFSTATS), me.name, sptr->name, stat->flag);
		if (!IsULine(sptr))
			sendto_snomask(SNO_EYES, "Stats \'%c\' requested by %s (%s@%s)",
				stat->flag, sptr->name, sptr->user->username, GetHost(sptr));
		else
			sendto_snomask(SNO_JUNK, "Stats \'%c\' requested by %s (%s@%s) [ulined]",
				stat->flag, sptr->name, sptr->user->username, GetHost(sptr));
	}
	else
	{
		stats_help(sptr);
		sendto_one(sptr, rpl_str(RPL_ENDOFSTATS), me.name, sptr->name, '*');
		return 0;
	}
	return 0;
}

int stats_banversion(aClient *sptr, char *para)
{
	ConfigItem_ban *bans;
	for (bans = conf_ban; bans; bans = (ConfigItem_ban *)bans->next)
	{
		if (bans->flag.type != CONF_BAN_VERSION)
			continue;
		sendto_one(sptr, rpl_str(RPL_STATSBANVER), me.name, sptr->name,
			bans->mask, bans->reason ? bans->reason : "No Reason");
	}
	return 0;
}

int stats_links(aClient *sptr, char *para)
{
	ConfigItem_link *link_p;
#ifdef DEBUGMODE
	aClient *acptr;
#endif
	for (link_p = conf_link; link_p; link_p = (ConfigItem_link *) link_p->next)
	{
		sendto_one(sptr, ":%s 213 %s C - * %s %i %s %s%s%s",
			me.name, sptr->name, /* user@host no longer shown as we allow multiple and split out/in etc */
			link_p->servername,
			link_p->outgoing.port,
			link_p->class->name,
			(link_p->outgoing.options & CONNECT_AUTO) ? "a" : "",
			(link_p->outgoing.options & CONNECT_SSL) ? "S" : "",
			(link_p->flag.temporary == 1) ? "T" : "");
#ifdef DEBUGMODE
		sendnotice(sptr, "%s (%p) has refcount %d",
			link_p->servername, link_p, link_p->refcount);
#endif
		if (link_p->hub)
			sendto_one(sptr, ":%s 244 %s H %s * %s",
				me.name, sptr->name, link_p->hub,
				link_p->servername);
		else if (link_p->leaf)
			sendto_one(sptr, ":%s 241 %s L %s * %s %d",
				me.name, sptr->name,
				link_p->leaf, link_p->servername, link_p->leaf_depth);
		// TODO: send incoming allow list? (for opers only)
	}
#ifdef DEBUGMODE
	list_for_each_entry(acptr, &client_list, client_node)
		if (MyConnect(acptr) && acptr->serv && !IsMe(acptr))
		{
			if (!acptr->serv->conf)
				sendnotice(sptr, "client '%s' (%p) has NO CONF attached (? :P)",
					acptr->name, acptr);
			else
				sendnotice(sptr, "client '%s' (%p) has conf %p attached, refcount: %d, temporary: %s",
					acptr->name, acptr,
					acptr->serv->conf,
					acptr->serv->conf->refcount,
					acptr->serv->conf->flag.temporary ? "YES" : "NO");
		}
#endif
	return 0;
}

int stats_denylinkall(aClient *sptr, char *para)
{
	ConfigItem_deny_link *links;

	for (links = conf_deny_link; links; links = (ConfigItem_deny_link *) links->next) 
	{
		if (links->flag.type == CRULE_ALL)
			sendto_one(sptr, rpl_str(RPL_STATSDLINE), me.name, sptr->name,
			'D', links->mask, links->prettyrule);
	}
	return 0;
}

int stats_gline(aClient *sptr, char *para)
{
	tkl_stats(sptr, TKL_GLOBAL|TKL_KILL, para);
	tkl_stats(sptr, TKL_GLOBAL|TKL_ZAP, para);
	return 0;
}

int stats_spamfilter(aClient *sptr, char *para)
{
	tkl_stats(sptr, TKL_SPAMF, para);
	tkl_stats(sptr, TKL_GLOBAL|TKL_SPAMF, para);
	return 0;
}

int stats_exceptban(aClient *sptr, char *para)
{
	ConfigItem_except *excepts;
	for (excepts = conf_except; excepts; excepts = (ConfigItem_except *) excepts->next)
	{
		if (excepts->flag.type == CONF_EXCEPT_BAN)
			sendto_one(sptr, rpl_str(RPL_STATSKLINE), me.name,
				sptr->name, "E", excepts->mask, "");
		else if (excepts->flag.type == CONF_EXCEPT_TKL)
			sendto_one(sptr, rpl_str(RPL_STATSEXCEPTTKL), me.name,
				sptr->name, tkl_typetochar(excepts->type), excepts->mask);
	}
	return 0;
}

int stats_allow(aClient *sptr, char *para)
{
	ConfigItem_allow *allows;
	for (allows = conf_allow; allows; allows = (ConfigItem_allow *) allows->next) 
		sendto_one(sptr, rpl_str(RPL_STATSILINE), me.name,
			sptr->name, allows->ip, allows->hostname, allows->maxperip, 
			allows->class->name, allows->server ? allows->server 
			: defserv, allows->port ? allows->port : 6667);
	return 0;
}

int stats_command(aClient *sptr, char *para)
{
	int i;
	aCommand *mptr;
	for (i = 0; i < 256; i++)
		for (mptr = CommandHash[i]; mptr; mptr = mptr->next)
			if (mptr->count)
#ifndef DEBUGMODE
			sendto_one(sptr, rpl_str(RPL_STATSCOMMANDS),
				me.name, sptr->name, mptr->cmd,
				mptr->count, mptr->bytes);
#else
			sendto_one(sptr, rpl_str(RPL_STATSCOMMANDS),
				me.name, sptr->name, mptr->cmd,
				mptr->count, mptr->bytes,
				mptr->lticks, mptr->lticks / CLOCKS_PER_SEC,
				mptr->rticks, mptr->rticks / CLOCKS_PER_SEC);
#endif

	return 0;
}	

int stats_oper(aClient *sptr, char *para)
{
	ConfigItem_oper *oper_p;
	ConfigItem_mask *m;

	for (oper_p = conf_oper; oper_p; oper_p = (ConfigItem_oper *) oper_p->next)
	{
		for (m = oper_p->mask; m; m = m->next)
		{
	  		sendto_one(sptr, rpl_str(RPL_STATSOLINE),
	  			me.name, sptr->name, 
	  			'O', m->mask, oper_p->name,
	  			"-",
	  			oper_p->class->name? oper_p->class->name : "");
		}
	}
	return 0;
}

static char *stats_port_helper(ConfigItem_listen *listener)
{
	static char buf[256];

	ircsnprintf(buf, sizeof(buf), "%s%s%s%s",
	    (listener->options & LISTENER_CLIENTSONLY)? "clientsonly ": "",
	    (listener->options & LISTENER_SERVERSONLY)? "serversonly ": "",
	    (listener->options & LISTENER_SSL)?         "ssl ": "",
	    !(listener->options & LISTENER_SSL)?        "plaintext ": "");
	return buf;
}

int stats_port(aClient *sptr, char *para)
{
	int i;
	ConfigItem_listen *listener;

	for (listener = conf_listen; listener != NULL; listener = (ConfigItem_listen *) listener->next)
	{
		if (!(listener->options & LISTENER_BOUND))
			continue;
		if ((listener->options & LISTENER_SERVERSONLY) && !ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL))
			continue;
		sendto_one(sptr, ":%s NOTICE %s :*** Listener on %s:%i (%s): has %i client(s), options: %s %s",
		           me.name, sptr->name,
		           listener->ip,
		           listener->port,
		           listener->ipv6 ? "IPv6" : "IPv4",
		           listener->clients,
		           stats_port_helper(listener),
		           listener->flag.temporary ? "[TEMPORARY]" : "");
	}
	return 0;
}

int stats_bannick(aClient *sptr, char *para)
{
	tkl_stats(sptr, TKL_NICK, para);
	return 0;
}

int stats_usage(aClient *sptr, char *para)
{
#ifdef DEBUGMODE
	send_usage(sptr, sptr->name);
#endif
	return 0;
}

int stats_traffic(aClient *sptr, char *para)
{
	aClient *acptr;
	int  i;
	struct stats *sp;
	struct stats tmp;
	time_t now = TStime();

	sp = &tmp;
	bcopy((char *)ircstp, (char *)sp, sizeof(*sp));

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsServer(acptr))
		{
			sp->is_sbs += acptr->local->sendB;
			sp->is_sbr += acptr->local->receiveB;
			sp->is_sks += acptr->local->sendK;
			sp->is_skr += acptr->local->receiveK;
			sp->is_sti += now - acptr->local->firsttime;
			sp->is_sv++;
			if (sp->is_sbs > 1023)
			{
				sp->is_sks += (sp->is_sbs >> 10);
				sp->is_sbs &= 0x3ff;
			}
			if (sp->is_sbr > 1023)
			{
				sp->is_skr += (sp->is_sbr >> 10);
				sp->is_sbr &= 0x3ff;
			}
		}
		else if (IsClient(acptr))
		{
			sp->is_cbs += acptr->local->sendB;
			sp->is_cbr += acptr->local->receiveB;
			sp->is_cks += acptr->local->sendK;
			sp->is_ckr += acptr->local->receiveK;
			sp->is_cti += now - acptr->local->firsttime;
			sp->is_cl++;
			if (sp->is_cbs > 1023)
			{
				sp->is_cks += (sp->is_cbs >> 10);
				sp->is_cbs &= 0x3ff;
			}
			if (sp->is_cbr > 1023)
			{
				sp->is_ckr += (sp->is_cbr >> 10);
				sp->is_cbr &= 0x3ff;
			}
		}
		else if (IsUnknown(acptr))
			sp->is_ni++;
	}

	sendto_one(sptr, ":%s %d %s :accepts %u refused %u",
	    me.name, RPL_STATSDEBUG, sptr->name, sp->is_ac, sp->is_ref);
	sendto_one(sptr, ":%s %d %s :unknown commands %u prefixes %u",
	    me.name, RPL_STATSDEBUG, sptr->name, sp->is_unco, sp->is_unpf);
	sendto_one(sptr, ":%s %d %s :nick collisions %u unknown closes %u",
	    me.name, RPL_STATSDEBUG, sptr->name, sp->is_kill, sp->is_ni);
	sendto_one(sptr, ":%s %d %s :wrong direction %u empty %u",
	    me.name, RPL_STATSDEBUG, sptr->name, sp->is_wrdi, sp->is_empt);
	sendto_one(sptr, ":%s %d %s :numerics seen %u mode fakes %u",
	    me.name, RPL_STATSDEBUG, sptr->name, sp->is_num, sp->is_fake);
	sendto_one(sptr, ":%s %d %s :auth successes %u fails %u",
	    me.name, RPL_STATSDEBUG, sptr->name, sp->is_asuc, sp->is_abad);
	sendto_one(sptr, ":%s %d %s :local connections %u udp packets %u",
	    me.name, RPL_STATSDEBUG, sptr->name, sp->is_loc, sp->is_udp);
	sendto_one(sptr, ":%s %d %s :Client Server",
	    me.name, RPL_STATSDEBUG, sptr->name);
	sendto_one(sptr, ":%s %d %s :connected %u %u",
	    me.name, RPL_STATSDEBUG, sptr->name, sp->is_cl, sp->is_sv);
	sendto_one(sptr, ":%s %d %s :bytes sent %ld.%huK %ld.%huK",
	    me.name, RPL_STATSDEBUG, sptr->name,
	    sp->is_cks, sp->is_cbs, sp->is_sks, sp->is_sbs);
	sendto_one(sptr, ":%s %d %s :bytes recv %ld.%huK %ld.%huK",
	    me.name, RPL_STATSDEBUG, sptr->name,
	    sp->is_ckr, sp->is_cbr, sp->is_skr, sp->is_sbr);
	sendto_one(sptr, ":%s %d %s :time connected %ld %ld",
	    me.name, RPL_STATSDEBUG, sptr->name, sp->is_cti, sp->is_sti);

	return 0;
}

int stats_fdtable(aClient *sptr, char *para)
{
	int i;

	for (i = 0; i < MAXCONNECTIONS; i++)
	{
		FDEntry *fde = &fd_table[i];

		if (!fde->is_open)
			continue;

		sendto_one(sptr,
			":%s %d %s :fd %3d, desc '%s', read-hdl %p, write-hdl %p, cbdata %p",
			me.name, RPL_STATSDEBUG, sptr->name,
			fde->fd, fde->desc, fde->read_callback, fde->write_callback, fde->data);
	}

	return 0;
}

int stats_uline(aClient *sptr, char *para)
{
	ConfigItem_ulines *ulines;
	for (ulines = conf_ulines; ulines; ulines = (ConfigItem_ulines *) ulines->next)
		sendto_one(sptr, rpl_str(RPL_STATSULINE), me.name,
			sptr->name, ulines->servername);
	return 0;	
}
int stats_vhost(aClient *sptr, char *para)
{
	ConfigItem_mask *m;
	ConfigItem_vhost *vhosts;

	for (vhosts = conf_vhost; vhosts; vhosts = (ConfigItem_vhost *) vhosts->next) 
	{
		for (m = vhosts->mask; m; m = m->next)
		{
			sendto_one(sptr, ":%s %i %s :vhost %s%s%s %s %s", me.name, RPL_TEXT, sptr->name,
			     vhosts->virtuser ? vhosts->virtuser : "", vhosts->virtuser ? "@" : "",
			     vhosts->virthost, vhosts->login, m->mask);
		}
	}
	return 0;
}

int stats_mem(aClient *sptr, char *para)
{
	extern MODVAR int flinks;
	extern MODVAR Link *freelink;
	extern MODVAR MemoryInfo StatsZ;

	aClient *acptr;
	Ban *ban;
	Link *link;
	aChannel *chptr;

	int  lc = 0,		/* local clients */
	     ch = 0,		/* channels */
	     lcc = 0,		/* local client conf links */
	     rc = 0,		/* remote clients */
	     us = 0,		/* user structs */
	     chu = 0,		/* channel users */
	     chi = 0,		/* channel invites */
	     chb = 0,		/* channel bans */
	     wwu = 0,		/* whowas users */
	     fl = 0,		/* free links */
	     cl = 0,		/* classes */
	     co = 0;		/* conf lines */

	int  usi = 0,		/* users invited */
	     usc = 0,		/* users in channels */
	     aw = 0,		/* aways set */
	     wwa = 0,		/* whowas aways */
	     wlh = 0,		/* watchlist headers */
	     wle = 0;		/* watchlist entries */

	u_long chm = 0,		/* memory used by channels */
	     chbm = 0,		/* memory used by channel bans */
	     lcm = 0,		/* memory used by local clients */
	     rcm = 0,		/* memory used by remote clients */
	     awm = 0,		/* memory used by aways */
	     wwam = 0,		/* whowas away memory used */
	     wwm = 0,		/* whowas array memory used */
	     com = 0,		/* memory used by conf lines */
	     wlhm = 0,		/* watchlist memory used */
	     db = 0,		/* memory used by dbufs */
	     rm = 0,		/* res memory used */
	     totcl = 0, totch = 0, totww = 0, tot = 0;

	if (!ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	count_whowas_memory(&wwu, &wwam);
	count_watch_memory(&wlh, &wlhm);
	wwm = sizeof(aName) * NICKNAMEHISTORYLENGTH;

	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (MyConnect(acptr))
		{
			lc++;
			/*for (link = acptr->confs; link; link = link->next)
				lcc++;
			wle += acptr->notifies;*/
			
		}
		else
			rc++;
		if (acptr->user)
		{
			Membership *mb;
			us++;
			for (link = acptr->user->invited; link;
			    link = link->next)
				usi++;
			for (mb = acptr->user->channel; mb;
			    mb = mb->next)
				usc++;
			if (acptr->user->away)
			{
				aw++;
				awm += (strlen(acptr->user->away) + 1);
			}
		}
	}
	lcm = lc * CLIENT_LOCAL_SIZE;
	rcm = rc * CLIENT_REMOTE_SIZE;

	for (chptr = channel; chptr; chptr = chptr->nextch)
	{
		Member *member;
		
		ch++;
		chm += (strlen(chptr->chname) + sizeof(aChannel));
		for (member = chptr->members; member; member = member->next)
			chu++;
		for (link = chptr->invites; link; link = link->next)
			chi++;
		for (ban = chptr->banlist; ban; ban = ban->next)
		{
			chb++;
			chbm += (strlen(ban->banstr) + 1 +
			    strlen(ban->who) + 1 + sizeof(Ban));
		}
		for (ban = chptr->exlist; ban; ban = ban->next)
		{
			chb++;
			chbm += (strlen(ban->banstr) + 1 +
			    strlen(ban->who) + 1 + sizeof(Ban));
		}
		for (ban = chptr->invexlist; ban; ban = ban->next)
		{
			chb++;
			chbm += (strlen(ban->banstr) + 1 +
			    strlen(ban->who) + 1 + sizeof(Ban));
		}
	}

	sendto_one(sptr, ":%s %d %s :Client Local %d(%ld) Remote %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, lc, lcm, rc, rcm);
	sendto_one(sptr, ":%s %d %s :Users %d(%ld) Invites %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, us, (long)(us * sizeof(anUser)),
	    usi, (long)(usi * sizeof(Link)));
	sendto_one(sptr, ":%s %d %s :User channels %d(%ld) Aways %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, usc, (long)(usc * sizeof(Link)), aw, awm);
	sendto_one(sptr, ":%s %d %s :WATCH headers %d(%ld) entries %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, wlh, wlhm, wle, (long)(wle * sizeof(Link)));
	sendto_one(sptr, ":%s %d %s :Attached confs %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, lcc, (long)(lcc * sizeof(Link)));

	totcl = lcm + rcm + us * sizeof(anUser) + usc * sizeof(Link) + awm;
	totcl += lcc * sizeof(Link) + usi * sizeof(Link) + wlhm;
	totcl += wle * sizeof(Link);

	sendto_one(sptr, ":%s %d %s :Conflines %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, co, com);

	sendto_one(sptr, ":%s %d %s :Classes %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, StatsZ.classes, StatsZ.classesmem);

	sendto_one(sptr, ":%s %d %s :Channels %d(%ld) Bans %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, ch, chm, chb, chbm);
	sendto_one(sptr, ":%s %d %s :Channel members %d(%ld) invite %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, chu, (long)(chu * sizeof(Link)),
	    chi, (long)(chi * sizeof(Link)));

	totch = chm + chbm + chu * sizeof(Link) + chi * sizeof(Link);

	sendto_one(sptr, ":%s %d %s :Whowas users %d(%ld) away %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, 
	    wwu, (long)(wwu * sizeof(anUser)),
	    wwa, wwam);
	sendto_one(sptr, ":%s %d %s :Whowas array %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, NICKNAMEHISTORYLENGTH, wwm);

	totww = wwu * sizeof(anUser) + wwam + wwm;

	sendto_one(sptr,
	    ":%s %d %s :Hash: client %d(%ld) chan %d(%ld) watch %d(%ld)", me.name,
	    RPL_STATSDEBUG, sptr->name, U_MAX,
	    (long)(sizeof(aHashEntry) * U_MAX), CH_MAX,
	    (long)(sizeof(aHashEntry) * CH_MAX), WATCHHASHSIZE,
	    (long)(sizeof(aWatch *) * WATCHHASHSIZE));

	for (link = freelink; link; link = link->next)
		fl++;
	sendto_one(sptr, ":%s %d %s :Link blocks free %d(%ld) total %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name,
	    fl, (long)(fl * sizeof(Link)),
	    flinks, (long)(flinks * sizeof(Link)));

/*	rm = cres_mem(sptr,sptr->name); */
	rm = 0; /* syzop: todo ?????????? */

	tot = totww + totch + totcl + com + cl * sizeof(aClass) + db + rm;
	tot += fl * sizeof(Link);
	tot += sizeof(aHashEntry) * U_MAX;
	tot += sizeof(aHashEntry) * CH_MAX;
	tot += sizeof(aWatch *) * WATCHHASHSIZE;

	sendto_one(sptr, ":%s %d %s :Total: ww %ld ch %ld cl %ld co %ld db %ld",
	    me.name, RPL_STATSDEBUG, sptr->name, totww, totch, totcl, com, db);
#if !defined(_WIN32) && !defined(_AMIGA)
#ifdef __alpha
	sendto_one(sptr, ":%s %d %s :TOTAL: %d sbrk(0)-etext: %u",
	    me.name, RPL_STATSDEBUG, sptr->name, tot,
	    (u_int)sbrk((size_t)0) - (u_int)sbrk0);
#else
	sendto_one(sptr, ":%s %d %s :TOTAL: %ld sbrk(0)-etext: %lu",
	    me.name, RPL_STATSDEBUG, sptr->name, tot,
	    (u_long)sbrk((size_t)0) - (u_long)sbrk0);

#endif
#else
	sendto_one(sptr, ":%s %d %s :TOTAL: %lu",
	    me.name, RPL_STATSDEBUG, sptr->name, tot);
#endif
	return 0;
}

int stats_denylinkauto(aClient *sptr, char *para)
{
	ConfigItem_deny_link *links;

	for (links = conf_deny_link; links; links = (ConfigItem_deny_link *) links->next) 
	{
		if (links->flag.type == CRULE_AUTO)
			sendto_one(sptr, rpl_str(RPL_STATSDLINE), me.name, sptr->name,
			'd', links->mask, links->prettyrule);
	}
	return 0;
}

int stats_exceptthrottle(aClient *sptr, char *para)
{
	ConfigItem_except *excepts;
	for (excepts = conf_except; excepts; excepts = (ConfigItem_except *) excepts->next)
		if (excepts->flag.type == CONF_EXCEPT_THROTTLE)
			sendto_one(sptr, rpl_str(RPL_STATSELINE),
				me.name, sptr->name, excepts->mask);
	return 0;
}

int stats_denydcc(aClient *sptr, char *para)
{
	ConfigItem_deny_dcc *denytmp;
	ConfigItem_allow_dcc *allowtmp;
	char *filemask, *reason;
	char a = 0;

	for (denytmp = conf_deny_dcc; denytmp; denytmp = (ConfigItem_deny_dcc *) denytmp->next)
	{
		filemask = BadPtr(denytmp->filename) ? "<NULL>" : denytmp->filename;
		reason = BadPtr(denytmp->reason) ? "<NULL>" : denytmp->reason;
		if (denytmp->flag.type2 == CONF_BAN_TYPE_CONF)
			a = 'c';
		if (denytmp->flag.type2 == CONF_BAN_TYPE_AKILL)
			a = 's';
		if (denytmp->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
			a = 'o';
		/* <d> <s|h> <howadded> <filemask> <reason> */
		sendto_one(sptr, ":%s %i %s :d %c %c %s %s", me.name, RPL_TEXT,
			sptr->name,
			(denytmp->flag.type == DCCDENY_SOFT) ? 's' : 'h',
			a, filemask, reason);
	}
	for (allowtmp = conf_allow_dcc; allowtmp; allowtmp = (ConfigItem_allow_dcc *) allowtmp->next)
	{
		filemask = BadPtr(allowtmp->filename) ? "<NULL>" : allowtmp->filename;
		if (allowtmp->flag.type2 == CONF_BAN_TYPE_CONF)
			a = 'c';
		if (allowtmp->flag.type2 == CONF_BAN_TYPE_AKILL)
			a = 's';
		if (allowtmp->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
			a = 'o';
		/* <a> <s|h> <howadded> <filemask> */
		sendto_one(sptr, ":%s %i %s :a %c %c %s", me.name, RPL_TEXT,
			sptr->name,
			(allowtmp->flag.type == DCCDENY_SOFT) ? 's' : 'h',
			a, filemask);
	}
	return 0;
}

int stats_kline(aClient *sptr, char *para)
{
	ConfigItem_ban *bans;
	ConfigItem_except *excepts;
	char type[2];
  	for (bans = conf_ban; bans; bans = (ConfigItem_ban *)bans->next) {
		if (bans->flag.type == CONF_BAN_USER) {
			if (bans->flag.type2 == CONF_BAN_TYPE_CONF)
				type[0] = 'K';
			type[1] = '\0';
			sendto_one(sptr, rpl_str(RPL_STATSKLINE),
		 		me.name, sptr->name, type, bans->mask, bans->reason
				? bans->reason : "<no reason>");
		}
		else if (bans->flag.type == CONF_BAN_IP) {
			if (bans->flag.type2 == CONF_BAN_TYPE_CONF)
				type[0] = 'Z';
			else if (bans->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
				type[0] = 'z';
			type[1] = '\0';
			sendto_one(sptr, rpl_str(RPL_STATSKLINE),
				me.name, sptr->name, type, bans->mask, bans->reason 
				? bans->reason : "<no reason>");
		}
	}
	tkl_stats(sptr, TKL_KILL, NULL);
	tkl_stats(sptr, TKL_ZAP, NULL);
	for (excepts = conf_except; excepts; excepts = (ConfigItem_except *)excepts->next) 
	{
		if (excepts->flag.type == CONF_EXCEPT_BAN)
			sendto_one(sptr, rpl_str(RPL_STATSKLINE),
				me.name, sptr->name, "E", excepts->mask, "");
	}
	return 0;
}

int stats_banrealname(aClient *sptr, char *para)
{
	ConfigItem_ban *bans;
	for (bans = conf_ban; bans; bans = (ConfigItem_ban *)bans->next) 
		if (bans->flag.type == CONF_BAN_REALNAME)
			sendto_one(sptr, rpl_str(RPL_STATSNLINE),
				me.name, sptr->name, bans->mask, bans->reason
				? bans->reason : "<no reason>");
	return 0;
}

int stats_sqline(aClient *sptr, char *para)
{
	tkl_stats(sptr, TKL_NICK|TKL_GLOBAL, para);
	return 0;
}

int stats_chanrestrict(aClient *sptr, char *para)
{
	ConfigItem_deny_channel *dchans;
	ConfigItem_allow_channel *achans;
	for (dchans = conf_deny_channel; dchans; dchans = (ConfigItem_deny_channel *) dchans->next) 
		sendto_one(sptr, ":%s %i %s :deny %s %c %s", me.name, RPL_TEXT, sptr->name,
			dchans->channel, dchans->warn ? 'w' : '-', dchans->reason);
  	for (achans = conf_allow_channel; achans; achans = (ConfigItem_allow_channel *) achans->next) 
		sendto_one(sptr, ":%s %i %s :allow %s", me.name, RPL_TEXT, sptr->name,
			achans->channel);
	return 0;
}

int stats_shun(aClient *sptr, char *para)
{
	tkl_stats(sptr, TKL_GLOBAL|TKL_SHUN, para);
	return 0;
}

/* should this be moved to a seperate stats flag? */
int stats_officialchannels(aClient *sptr, char *para)
{
ConfigItem_offchans *x;
	for (x = conf_offchans; x; x = (ConfigItem_offchans *)x->next)
		sendto_one(sptr, ":%s %i %s :%s %s",
			me.name, RPL_TEXT, sptr->name, x->chname, x->topic ? x->topic : "");
	return 0;
}

int stats_set(aClient *sptr, char *para)
{
	char *uhallow;

	if (!ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	sendto_one(sptr, ":%s %i %s :*** Configuration Report ***",
	    me.name, RPL_TEXT, sptr->name);
	sendto_one(sptr, ":%s %i %s :network-name: %s", me.name, RPL_TEXT,
	    sptr->name, ircnetwork);
	sendto_one(sptr, ":%s %i %s :default-server: %s", me.name, RPL_TEXT,
	    sptr->name, defserv);
	sendto_one(sptr, ":%s %i %s :services-server: %s", me.name, RPL_TEXT,
	    sptr->name, SERVICES_NAME);
	if (STATS_SERVER)
	{
		sendto_one(sptr, ":%s %i %s :stats-server: %s", me.name, RPL_TEXT,
		    sptr->name, STATS_SERVER);
	}
	sendto_one(sptr, ":%s %i %s :hiddenhost-prefix: %s", me.name, RPL_TEXT,
	    sptr->name, hidden_host);
	sendto_one(sptr, ":%s %i %s :help-channel: %s", me.name, RPL_TEXT,
	    sptr->name, helpchan);
	sendto_one(sptr, ":%s %i %s :cloak-keys: %s", me.name, RPL_TEXT, sptr->name,
		CLOAK_KEYCRC);
	sendto_one(sptr, ":%s %i %s :kline-address: %s", me.name, RPL_TEXT,
	    sptr->name, KLINE_ADDRESS);
	if (GLINE_ADDRESS)
		sendto_one(sptr, ":%s %i %s :gline-address: %s", me.name, RPL_TEXT,
		    sptr->name, GLINE_ADDRESS);
	sendto_one(sptr, ":%s %i %s :modes-on-connect: %s", me.name, RPL_TEXT,
	    sptr->name, get_modestr(CONN_MODES));
	sendto_one(sptr, ":%s %i %s :modes-on-oper: %s", me.name, RPL_TEXT,
	    sptr->name, get_modestr(OPER_MODES));
	*modebuf = *parabuf = 0;
	chmode_str(&iConf.modes_on_join, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf));
	sendto_one(sptr, ":%s %i %s :modes-on-join: %s %s", me.name, RPL_TEXT,
		sptr->name, modebuf, parabuf);
	sendto_one(sptr, ":%s %i %s :nick-length: %i", me.name, RPL_TEXT,
		sptr->name, iConf.nicklen);
	sendto_one(sptr, ":%s %i %s :snomask-on-oper: %s", me.name, RPL_TEXT,
	    sptr->name, OPER_SNOMASK);
	sendto_one(sptr, ":%s %i %s :snomask-on-connect: %s", me.name, RPL_TEXT,
	    sptr->name, CONNECT_SNOMASK ? CONNECT_SNOMASK : "+");
	if (OPER_ONLY_STATS)
	{
		char *longflags = stats_operonly_long_to_short();
		sendto_one(sptr, ":%s %i %s :oper-only-stats: %s%s", me.name, RPL_TEXT,
			sptr->name, OPER_ONLY_STATS, longflags ? longflags : "");
	}
	if (RESTRICT_USERMODES)
		sendto_one(sptr, ":%s %i %s :restrict-usermodes: %s", me.name, RPL_TEXT,
			sptr->name, RESTRICT_USERMODES);
	if (RESTRICT_CHANNELMODES)
		sendto_one(sptr, ":%s %i %s :restrict-channelmodes: %s", me.name, RPL_TEXT,
			sptr->name, RESTRICT_CHANNELMODES);
	if (RESTRICT_EXTENDEDBANS)
		sendto_one(sptr, ":%s %i %s :restrict-extendedbans: %s", me.name, RPL_TEXT,
			sptr->name, RESTRICT_EXTENDEDBANS);
	switch (UHOST_ALLOWED)
	{
		case UHALLOW_NEVER:
			uhallow = "never";
			break;
		case UHALLOW_NOCHANS:
			uhallow = "not-on-channels";
			break;
		case UHALLOW_REJOIN:
			uhallow = "force-rejoin";
			break;
		case UHALLOW_ALWAYS:
		default:
			uhallow = "always";
			break;
	}
	if (uhallow)
		sendto_one(sptr, ":%s %i %s :allow-userhost-change: %s", me.name, RPL_TEXT,
			sptr->name, uhallow);
	sendto_one(sptr, ":%s %i %s :hide-ban-reason: %d", me.name, RPL_TEXT,
	    sptr->name, HIDE_BAN_REASON);
	sendto_one(sptr, ":%s %i %s :anti-spam-quit-message-time: %s", me.name, RPL_TEXT, 
		sptr->name, pretty_time_val(ANTI_SPAM_QUIT_MSG_TIME));
	sendto_one(sptr, ":%s %i %s :channel-command-prefix: %s", me.name, RPL_TEXT, sptr->name, CHANCMDPFX ? CHANCMDPFX : "`");
	sendto_one(sptr, ":%s %i %s :ssl::egd: %s", me.name, RPL_TEXT,
		sptr->name, EGD_PATH ? EGD_PATH : (USE_EGD ? "1" : "0"));
	sendto_one(sptr, ":%s %i %s :ssl::certificate: %s", me.name, RPL_TEXT,
		sptr->name, SSL_SERVER_CERT_PEM);
	sendto_one(sptr, ":%s %i %s :ssl::key: %s", me.name, RPL_TEXT,
		sptr->name, SSL_SERVER_KEY_PEM);
	sendto_one(sptr, ":%s %i %s :ssl::trusted-ca-file: %s", me.name, RPL_TEXT, sptr->name,
	 iConf.trusted_ca_file ? iConf.trusted_ca_file : "<none>");
	sendto_one(sptr, ":%s %i %s :ssl::options: %s %s %s", me.name, RPL_TEXT, sptr->name,
		iConf.ssl_options & SSLFLAG_FAILIFNOCERT ? "FAILIFNOCERT" : "",
		iConf.ssl_options & SSLFLAG_VERIFYCERT ? "VERIFYCERT" : "",
		iConf.ssl_options & SSLFLAG_DONOTACCEPTSELFSIGNED ? "DONOTACCEPTSELFSIGNED" : "");

	sendto_one(sptr, ":%s %i %s :options::show-opermotd: %d", me.name, RPL_TEXT,
	    sptr->name, SHOWOPERMOTD);
	sendto_one(sptr, ":%s %i %s :options::hide-ulines: %d", me.name, RPL_TEXT,
	    sptr->name, HIDE_ULINES);
	sendto_one(sptr, ":%s %i %s :options::identd-check: %d", me.name, RPL_TEXT,
	    sptr->name, IDENT_CHECK);
	sendto_one(sptr, ":%s %i %s :options::fail-oper-warn: %d", me.name, RPL_TEXT,
	    sptr->name, FAILOPER_WARN);
	sendto_one(sptr, ":%s %i %s :options::show-connect-info: %d", me.name, RPL_TEXT,
	    sptr->name, SHOWCONNECTINFO);
	sendto_one(sptr, ":%s %i %s :options::dont-resolve: %d", me.name, RPL_TEXT,
	    sptr->name, DONT_RESOLVE);
	sendto_one(sptr, ":%s %i %s :options::mkpasswd-for-everyone: %d", me.name, RPL_TEXT,
	    sptr->name, MKPASSWD_FOR_EVERYONE);
	sendto_one(sptr, ":%s %i %s :options::allow-insane-bans: %d", me.name, RPL_TEXT,
	    sptr->name, ALLOW_INSANE_BANS);
	sendto_one(sptr, ":%s %i %s :options::allow-part-if-shunned: %d", me.name, RPL_TEXT,
	    sptr->name, ALLOW_PART_IF_SHUNNED);
	sendto_one(sptr, ":%s %i %s :maxchannelsperuser: %i", me.name, RPL_TEXT,
	    sptr->name, MAXCHANNELSPERUSER);
	sendto_one(sptr, ":%s %i %s :auto-join: %s", me.name, RPL_TEXT,
	    sptr->name, AUTO_JOIN_CHANS ? AUTO_JOIN_CHANS : "0");
	sendto_one(sptr, ":%s %i %s :oper-auto-join: %s", me.name,
	    RPL_TEXT, sptr->name, OPER_AUTO_JOIN_CHANS ? OPER_AUTO_JOIN_CHANS : "0");
	sendto_one(sptr, ":%s %i %s :static-quit: %s", me.name, 
		RPL_TEXT, sptr->name, STATIC_QUIT ? STATIC_QUIT : "<none>");	
	sendto_one(sptr, ":%s %i %s :static-part: %s", me.name, 
		RPL_TEXT, sptr->name, STATIC_PART ? STATIC_PART : "<none>");	
	sendto_one(sptr, ":%s %i %s :who-limit: %d", me.name, RPL_TEXT,
		sptr->name, WHOLIMIT);
	sendto_one(sptr, ":%s %i %s :silence-limit: %d", me.name, RPL_TEXT,
		sptr->name, SILENCE_LIMIT);
	if (DNS_BINDIP)
		sendto_one(sptr, ":%s %i %s :dns::bind-ip: %s", me.name, RPL_TEXT,
		    sptr->name, DNS_BINDIP);
	sendto_one(sptr, ":%s %i %s :ban-version-tkl-time: %s", me.name, RPL_TEXT,
	    sptr->name, pretty_time_val(BAN_VERSION_TKL_TIME));
	if (LINK_BINDIP)
		sendto_one(sptr, ":%s %i %s :link::bind-ip: %s", me.name, RPL_TEXT,
		    sptr->name, LINK_BINDIP);
	sendto_one(sptr, ":%s %i %s :throttle::period: %s", me.name, RPL_TEXT,
			sptr->name, THROTTLING_PERIOD ? pretty_time_val(THROTTLING_PERIOD) : "disabled");
	sendto_one(sptr, ":%s %i %s :throttle::connections: %d", me.name, RPL_TEXT,
			sptr->name, THROTTLING_COUNT ? THROTTLING_COUNT : -1);
	sendto_one(sptr, ":%s %i %s :anti-flood::unknown-flood-bantime: %s", me.name, RPL_TEXT,
			sptr->name, pretty_time_val(UNKNOWN_FLOOD_BANTIME));
	sendto_one(sptr, ":%s %i %s :anti-flood::unknown-flood-amount: %ldKB", me.name, RPL_TEXT,
			sptr->name, UNKNOWN_FLOOD_AMOUNT);
#ifdef NO_FLOOD_AWAY
	if (AWAY_PERIOD)
	{
		sendto_one(sptr, ":%s %i %s :anti-flood::away-flood: %d per %s", me.name, RPL_TEXT, 
			sptr->name, AWAY_COUNT, pretty_time_val(AWAY_PERIOD));
	}
#endif
	sendto_one(sptr, ":%s %i %s :anti-flood::nick-flood: %d per %s", me.name, RPL_TEXT, 
		sptr->name, NICK_COUNT, pretty_time_val(NICK_PERIOD));
	sendto_one(sptr, ":%s %i %s :ident::connect-timeout: %s", me.name, RPL_TEXT,
			sptr->name, pretty_time_val(IDENT_CONNECT_TIMEOUT));
	sendto_one(sptr, ":%s %i %s :ident::read-timeout: %s", me.name, RPL_TEXT,
			sptr->name, pretty_time_val(IDENT_READ_TIMEOUT));
	sendto_one(sptr, ":%s %i %s :modef-default-unsettime: %hd", me.name, RPL_TEXT,
			sptr->name, (unsigned short)MODEF_DEFAULT_UNSETTIME);
	sendto_one(sptr, ":%s %i %s :modef-max-unsettime: %hd", me.name, RPL_TEXT,
			sptr->name, (unsigned short)MODEF_MAX_UNSETTIME);
	sendto_one(sptr, ":%s %i %s :spamfilter::ban-time: %s", me.name, RPL_TEXT,
		sptr->name, pretty_time_val(SPAMFILTER_BAN_TIME));
	sendto_one(sptr, ":%s %i %s :spamfilter::ban-reason: %s", me.name, RPL_TEXT,
		sptr->name, SPAMFILTER_BAN_REASON);
	sendto_one(sptr, ":%s %i %s :spamfilter::virus-help-channel: %s", me.name, RPL_TEXT,
		sptr->name, SPAMFILTER_VIRUSCHAN);
	if (SPAMFILTER_EXCEPT)
		sendto_one(sptr, ":%s %i %s :spamfilter::except: %s", me.name, RPL_TEXT,
			sptr->name, SPAMFILTER_EXCEPT);
	sendto_one(sptr, ":%s %i %s :check-target-nick-bans: %s", me.name, RPL_TEXT,
		sptr->name, CHECK_TARGET_NICK_BANS ? "yes" : "no");
	RunHook2(HOOKTYPE_STATS, sptr, "S");
	return 1;
}

int stats_tld(aClient *sptr, char *para)
{
	ConfigItem_tld *tld;
	for (tld = conf_tld; tld; tld = (ConfigItem_tld *) tld->next)
		sendto_one(sptr, rpl_str(RPL_STATSTLINE), me.name, sptr->name, 
			tld->mask, tld->motd_file, tld->rules_file ? 
			tld->rules_file : "none");
	return 0;
}

int stats_uptime(aClient *sptr, char *para)
{
	time_t tmpnow;

	tmpnow = TStime() - me.local->since;
	sendto_one(sptr, rpl_str(RPL_STATSUPTIME), me.name, sptr->name,
	    tmpnow / 86400, (tmpnow / 3600) % 24, (tmpnow / 60) % 60,
	    tmpnow % 60);
	sendto_one(sptr, rpl_str(RPL_STATSCONN), me.name, sptr->name,
	    max_connection_count, IRCstats.me_max);
	return 0;
}

int stats_denyver(aClient *sptr, char *para)
{
	ConfigItem_deny_version *versions;
	for (versions = conf_deny_version; versions; versions = (ConfigItem_deny_version *) versions->next) 
		sendto_one(sptr, rpl_str(RPL_STATSVLINE), me.name, sptr->name,
			versions->version, versions->flags, versions->mask);
	return 0;
}

int stats_notlink(aClient *sptr, char *para)
{
	ConfigItem_link *link_p;

	for (link_p = conf_link; link_p; link_p = (ConfigItem_link *) link_p->next)
	{
		if (!find_server_quick(link_p->servername))
			sendto_one(sptr, rpl_str(RPL_STATSXLINE),
				me.name, sptr->name, link_p->servername,
				link_p->outgoing.port);
	}
	return 0;
}

int stats_class(aClient *sptr, char *para)
{
	ConfigItem_class *classes;
	for (classes = conf_class; classes; classes = (ConfigItem_class *) classes->next) 
	{
		sendto_one(sptr, rpl_str(RPL_STATSYLINE),
			me.name, sptr->name, classes->name, classes->pingfreq, classes->connfreq,
			classes->maxclients, classes->sendq, classes->recvq ? classes->recvq : CLIENT_FLOOD);
#ifdef DEBUGMODE
		sendnotice(sptr, "class '%s' has clients=%d, xrefcount=%d",
			classes->name, classes->clients, classes->xrefcount);
#endif
	}
	return 0;
}

int stats_linkinfo(aClient *sptr, char *para)
{
	return stats_linkinfoint(sptr, para, 0);
}

int stats_linkinfoall(aClient *sptr, char *para)
{
	return stats_linkinfoint(sptr, para, 1);
}

int stats_linkinfoint(aClient *sptr, char *para, int all)
{
#ifndef DEBUGMODE
	static char Sformat[] =
	    ":%s %d %s SendQ SendM SendBytes RcveM RcveBytes Open_since :Idle";
	static char Lformat[] = ":%s %d %s %s%s %u %u %u %u %u %u :%u";
#else
	static char Sformat[] =
	    ":%s %d %s SendQ SendM SendBytes RcveM RcveBytes Open_since CPU :Idle";
	static char Lformat[] = ":%s %d %s %s%s %u %u %u %u %u %u %s";
	char pbuf[96];		/* Should be enough for to ints */
#endif
	int remote = 0;
	int wilds = 0;
	int doall = 0;
	int showports = ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL);
	int i;
	aClient *acptr;
	/*
	 * send info about connections which match, or all if the
	 * mask matches me.name.  Only restrictions are on those who
	 * are invisible not being visible to 'foreigners' who use
	 * a wild card based search to list it.
	 */
	if (para)
	{
		if (!mycmp(para, me.name))
			doall = 2;
		else if (match(para, me.name) == 0)
			doall = 1;
		if (index(para, '*') || index(para, '?'))
			wilds = 1;
	}
	else
		para = me.name;
	sendto_one(sptr, Sformat, me.name, RPL_STATSLINKINFO, sptr->name);
	if (!MyClient(sptr))
	{
		remote = 1;
		wilds = 0;
	}

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsInvisible(acptr) && (doall || wilds) &&
			!ValidatePermissionsForPath("stats:viewinvisible",sptr,NULL,NULL,NULL) &&
			!IsOper(acptr) && (acptr != sptr))
			continue;
		if (remote && doall && !IsServer(acptr) && !IsMe(acptr))
			continue;
		if (remote && !doall && IsServer(acptr))
			continue;
		if (!doall && wilds && match(para, acptr->name))
			continue;
		if (!(para && (IsServer(acptr)
			|| (acptr->flags & FLAGS_LISTEN))) && !(doall
			|| wilds) && mycmp(para, acptr->name))
			continue;

#ifdef DEBUGMODE
		ircsnprintf(pbuf, sizeof(pbuf), "%ld :%ld", (long)acptr->local->cputime,
		      (long)(acptr->user && MyConnect(acptr)) ? TStime() - acptr->local->last : 0);
#endif
		if (ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL))
		{
			sendto_one(sptr, Lformat, me.name,
				RPL_STATSLINKINFO, sptr->name, 
				all ?
				(get_client_name2(acptr, showports)) :
				(get_client_name(acptr, FALSE)),
				get_cptr_status(acptr),
				(int)DBufLength(&acptr->local->sendQ),
				(int)acptr->local->sendM, (int)acptr->local->sendK,
				(int)acptr->local->receiveM,
				(int)acptr->local->receiveK,
			 	TStime() - acptr->local->firsttime,
#ifndef DEBUGMODE
				(acptr->user && MyConnect(acptr)) ?
				TStime() - acptr->local->last : 0);
#else
				pbuf);
#endif
			if (!IsServer(acptr) && !IsMe(acptr) && ValidatePermissionsForPath("privacy",acptr,NULL,NULL,NULL) && sptr != acptr)
				sendto_one(acptr,
					":%s NOTICE %s :*** %s did a /stats L on you! IP may have been shown",
					me.name, acptr->name, sptr->name);
		}
		else if (!strchr(acptr->name, '.'))
			sendto_one(sptr, Lformat, me.name,
				RPL_STATSLINKINFO, sptr->name,
				IsHidden(acptr) ? acptr->name :
				all ?	/* Potvin - PreZ */
				get_client_name2(acptr, showports) :
				get_client_name(acptr, FALSE),
				get_cptr_status(acptr),
				(int)DBufLength(&acptr->local->sendQ),
				(int)acptr->local->sendM, (int)acptr->local->sendK,
				(int)acptr->local->receiveM,
				(int)acptr->local->receiveK,
				TStime() - acptr->local->firsttime,
#ifndef DEBUGMODE
				(acptr->user && MyConnect(acptr)) ?
				TStime() - acptr->local->last : 0);
#else
				pbuf);
#endif
	}
#ifdef DEBUGMODE
	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (IsServer(acptr))
			sendto_one(sptr, ":%s NOTICE %s :Server %s is %s",
				me.name, sptr->name, acptr->name, acptr->serv->flags.synced ? "SYNCED" : "NOT SYNCED!!");
	}
#endif
	return 0;
}
