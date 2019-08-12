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

ModuleHeader MOD_HEADER(stats)
  = {
	"stats",
	"5.0",
	"command /stats", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(stats)
{
	CommandAdd(modinfo->handle, MSG_STATS, m_stats, 3, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(stats)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(stats)
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
	sendnumeric(sptr, RPL_STATSHELP, "/Stats flags:");
	sendnumeric(sptr, RPL_STATSHELP, "B - banversion - Send the ban version list");
	sendnumeric(sptr, RPL_STATSHELP, "b - badword - Send the badwords list");
	sendnumeric(sptr, RPL_STATSHELP, "C - link - Send the link block list");
	sendnumeric(sptr, RPL_STATSHELP, "d - denylinkauto - Send the deny link (auto) block list");
	sendnumeric(sptr, RPL_STATSHELP, "D - denylinkall - Send the deny link (all) block list");
	sendnumeric(sptr, RPL_STATSHELP, "e - exceptthrottle - Send the except throttle block list");
	sendnumeric(sptr, RPL_STATSHELP, "E - exceptban - Send the except ban and except tkl block list");
	sendnumeric(sptr, RPL_STATSHELP, "f - spamfilter - Send the spamfilter list");
	sendnumeric(sptr, RPL_STATSHELP, "F - denydcc - Send the deny dcc and allow dcc block lists");
	sendnumeric(sptr, RPL_STATSHELP, "G - gline - Send the gline and gzline list");
	sendnumeric(sptr, RPL_STATSHELP, "  Extended flags: [+/-mrs] [mask] [reason] [setby]");
	sendnumeric(sptr, RPL_STATSHELP, "   m Return glines matching/not matching the specified mask");
	sendnumeric(sptr, RPL_STATSHELP, "   r Return glines with a reason matching/not matching the specified reason");
	sendnumeric(sptr, RPL_STATSHELP, "   s Return glines set by/not set by clients matching the specified name");
	sendnumeric(sptr, RPL_STATSHELP, "I - allow - Send the allow block list");
	sendnumeric(sptr, RPL_STATSHELP, "j - officialchans - Send the offical channels list");
	sendnumeric(sptr, RPL_STATSHELP, "K - kline - Send the ban user/ban ip/except ban block list");
	sendnumeric(sptr, RPL_STATSHELP, "l - linkinfo - Send link information");
	sendnumeric(sptr, RPL_STATSHELP, "L - linkinfoall - Send all link information");
	sendnumeric(sptr, RPL_STATSHELP, "M - command - Send list of how many times each command was used");
	sendnumeric(sptr, RPL_STATSHELP, "n - banrealname - Send the ban realname block list");
	sendnumeric(sptr, RPL_STATSHELP, "O - oper - Send the oper block list");
	sendnumeric(sptr, RPL_STATSHELP, "P - port - Send information about ports");
	sendnumeric(sptr, RPL_STATSHELP, "q - bannick - Send the ban nick block list");
	sendnumeric(sptr, RPL_STATSHELP, "Q - sqline - Send the global qline list");
	sendnumeric(sptr, RPL_STATSHELP, "r - chanrestrict - Send the channel deny/allow block list");
#ifdef DEBUGMODE
	sendnumeric(sptr, RPL_STATSHELP, "R - usage - Send usage information");
#endif
	sendnumeric(sptr, RPL_STATSHELP, "S - set - Send the set block list");
	sendnumeric(sptr, RPL_STATSHELP, "s - shun - Send the shun list");
	sendnumeric(sptr, RPL_STATSHELP, "  Extended flags: [+/-mrs] [mask] [reason] [setby]");
	sendnumeric(sptr, RPL_STATSHELP, "   m Return shuns matching/not matching the specified mask");
	sendnumeric(sptr, RPL_STATSHELP, "   r Return shuns with a reason matching/not matching the specified reason");
	sendnumeric(sptr, RPL_STATSHELP, "   s Return shuns set by/not set by clients matching the specified name");
	sendnumeric(sptr, RPL_STATSHELP, "t - tld - Send the tld block list");
	sendnumeric(sptr, RPL_STATSHELP, "T - traffic - Send traffic information");
	sendnumeric(sptr, RPL_STATSHELP, "u - uptime - Send the server uptime and connection count");
	sendnumeric(sptr, RPL_STATSHELP, "U - uline - Send the ulines block list");
	sendnumeric(sptr, RPL_STATSHELP, "v - denyver - Send the deny version block list");
	sendnumeric(sptr, RPL_STATSHELP, "V - vhost - Send the vhost block list");
	sendnumeric(sptr, RPL_STATSHELP, "W - fdtable - Send the FD table listing");
	sendnumeric(sptr, RPL_STATSHELP, "X - notlink - Send the list of servers that are not current linked");
	sendnumeric(sptr, RPL_STATSHELP, "Y - class - Send the class block list");
	sendnumeric(sptr, RPL_STATSHELP, "Z - mem - Send memory usage information");
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
	for (os = iConf.oper_only_stats_ext; os; os = os->next)
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
	for (os = iConf.oper_only_stats_ext; os; os = os->next)
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
		if (hunt_server(cptr, sptr, recv_mtags, ":%s STATS %s :%s", 2, parc, parv) != HUNTED_ISME)
			return 0;
	}
	else if (parc == 4 && parv[2][0] != '+' && parv[2][0] != '-')
	{
		if (hunt_server(cptr, sptr, recv_mtags, ":%s STATS %s %s %s", 2, parc, parv) != HUNTED_ISME)
			return 0;
	}
	if (parc < 2 || !*parv[1])
	{
		stats_help(sptr);
		sendnumeric(sptr, RPL_ENDOFSTATS, '*');
		return 0;
	}

	/* Decide if we are looking for 1 char or a string */
	if (parv[1][0] && !parv[1][1])
	{
		if (!ValidatePermissionsForPath("server:info:stats",sptr,NULL,NULL,NULL) && stats_operonly_short(parv[1][0]))
		{
			sendnumeric(sptr, ERR_NOPRIVILEGES);	
			return 0;
		}	
		/* Old style, we can use a binary search here */
		stat = stats_binary_search(parv[1][0]);
	}
	else
	{
		if (!ValidatePermissionsForPath("server:info:stats",sptr,NULL,NULL,NULL) && stats_operonly_long(parv[1]))
		{
			sendnumeric(sptr, ERR_NOPRIVILEGES);	
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
			if (!ValidatePermissionsForPath("server:info:stats",sptr,NULL,NULL,NULL) && stats_operonly_long(stat->longflag))
			{
				sendnumeric(sptr, ERR_NOPRIVILEGES);
				return 0;
			}
		}
		/* It was a long flag, so check oper only on short flags */
		else
		{
			if (!ValidatePermissionsForPath("server:info:stats",sptr,NULL,NULL,NULL) && stats_operonly_short(stat->flag))
			{
				sendnumeric(sptr, ERR_NOPRIVILEGES);	
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
		sendnumeric(sptr, RPL_ENDOFSTATS, stat->flag);
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
		sendnumeric(sptr, RPL_ENDOFSTATS, '*');
		return 0;
	}
	return 0;
}

int stats_banversion(aClient *sptr, char *para)
{
	ConfigItem_ban *bans;
	for (bans = conf_ban; bans; bans = bans->next)
	{
		if (bans->flag.type != CONF_BAN_VERSION)
			continue;
		sendnumeric(sptr, RPL_STATSBANVER,
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
	for (link_p = conf_link; link_p; link_p = link_p->next)
	{
		sendnumericfmt(sptr, RPL_STATSCLINE, "C - * %s %i %s %s%s%s",
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
			sendnumericfmt(sptr, RPL_STATSHLINE, "H %s * %s",
				link_p->hub, link_p->servername);
		else if (link_p->leaf)
			sendnumericfmt(sptr, RPL_STATSLLINE, "L %s * %s %d",
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

	for (links = conf_deny_link; links; links = links->next)
	{
		if (links->flag.type == CRULE_ALL)
			sendnumeric(sptr, RPL_STATSDLINE,
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
	for (excepts = conf_except; excepts; excepts = excepts->next)
	{
		if (excepts->flag.type == CONF_EXCEPT_BAN)
			sendnumeric(sptr, RPL_STATSKLINE, "E", excepts->mask, "");
		else if (excepts->flag.type == CONF_EXCEPT_TKL)
			sendnumeric(sptr, RPL_STATSEXCEPTTKL, tkl_typetochar(excepts->type), excepts->mask);
	}
	return 0;
}

int stats_allow(aClient *sptr, char *para)
{
	ConfigItem_allow *allows;
	for (allows = conf_allow; allows; allows = allows->next)
		sendnumeric(sptr, RPL_STATSILINE, allows->ip, allows->hostname, allows->maxperip, 
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
			sendnumeric(sptr, RPL_STATSCOMMANDS, mptr->cmd,
				mptr->count, mptr->bytes);
#else
			sendnumeric(sptr, RPL_STATSCOMMANDS, mptr->cmd,
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

	for (oper_p = conf_oper; oper_p; oper_p = oper_p->next)
	{
		for (m = oper_p->mask; m; m = m->next)
		{
	  		sendnumeric(sptr, RPL_STATSOLINE, 
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

	for (listener = conf_listen; listener != NULL; listener = listener->next)
	{
		if (!(listener->options & LISTENER_BOUND))
			continue;
		if ((listener->options & LISTENER_SERVERSONLY) && !ValidatePermissionsForPath("server:info:stats",sptr,NULL,NULL,NULL))
			continue;
		sendnotice(sptr, "*** Listener on %s:%i (%s): has %i client(s), options: %s %s",
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

	sendnumericfmt(sptr, RPL_STATSDEBUG, "accepts %u refused %u", sp->is_ac, sp->is_ref);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "unknown commands %u prefixes %u", sp->is_unco, sp->is_unpf);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "nick collisions %u unknown closes %u", sp->is_kill, sp->is_ni);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "wrong direction %u empty %u", sp->is_wrdi, sp->is_empt);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "numerics seen %u mode fakes %u", sp->is_num, sp->is_fake);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "auth successes %u fails %u", sp->is_asuc, sp->is_abad);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "local connections %u udp packets %u", sp->is_loc, sp->is_udp);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "Client Server");
	sendnumericfmt(sptr, RPL_STATSDEBUG, "connected %u %u", sp->is_cl, sp->is_sv);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "bytes sent %ld.%huK %ld.%huK",
		sp->is_cks, sp->is_cbs, sp->is_sks, sp->is_sbs);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "bytes recv %ld.%huK %ld.%huK",
	    sp->is_ckr, sp->is_cbr, sp->is_skr, sp->is_sbr);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "time connected %ld %ld",
	    sp->is_cti, sp->is_sti);

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

		sendnumericfmt(sptr, RPL_STATSDEBUG,
			"fd %3d, desc '%s', read-hdl %p, write-hdl %p, cbdata %p",
			fde->fd, fde->desc, fde->read_callback, fde->write_callback, fde->data);
	}

	return 0;
}

int stats_uline(aClient *sptr, char *para)
{
	ConfigItem_ulines *ulines;
	for (ulines = conf_ulines; ulines; ulines = ulines->next)
		sendnumeric(sptr, RPL_STATSULINE, ulines->servername);
	return 0;	
}
int stats_vhost(aClient *sptr, char *para)
{
	ConfigItem_mask *m;
	ConfigItem_vhost *vhosts;

	for (vhosts = conf_vhost; vhosts; vhosts = vhosts->next)
	{
		for (m = vhosts->mask; m; m = m->next)
		{
			sendtxtnumeric(sptr, "vhost %s%s%s %s %s", vhosts->virtuser ? vhosts->virtuser : "", vhosts->virtuser ? "@" : "",
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

	if (!ValidatePermissionsForPath("server:info:stats",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
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

	sendnumericfmt(sptr, RPL_STATSDEBUG, "Client Local %d(%ld) Remote %d(%ld)",
	    lc, lcm, rc, rcm);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "Users %d(%ld) Invites %d(%ld)",
	    us, (long)(us * sizeof(anUser)),
	    usi, (long)(usi * sizeof(Link)));
	sendnumericfmt(sptr, RPL_STATSDEBUG, "User channels %d(%ld) Aways %d(%ld)",
	    usc, (long)(usc * sizeof(Link)), aw, awm);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "WATCH headers %d(%ld) entries %d(%ld)",
	    wlh, wlhm, wle, (long)(wle * sizeof(Link)));
	sendnumericfmt(sptr, RPL_STATSDEBUG, "Attached confs %d(%ld)",
	    lcc, (long)(lcc * sizeof(Link)));

	totcl = lcm + rcm + us * sizeof(anUser) + usc * sizeof(Link) + awm;
	totcl += lcc * sizeof(Link) + usi * sizeof(Link) + wlhm;
	totcl += wle * sizeof(Link);

	sendnumericfmt(sptr, RPL_STATSDEBUG, "Conflines %d(%ld)", co, com);

	sendnumericfmt(sptr, RPL_STATSDEBUG, "Classes %d(%ld)",
		StatsZ.classes, StatsZ.classesmem);

	sendnumericfmt(sptr, RPL_STATSDEBUG, "Channels %d(%ld) Bans %d(%ld)",
	    ch, chm, chb, chbm);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "Channel members %d(%ld) invite %d(%ld)",
	    chu, (long)(chu * sizeof(Link)),
	    chi, (long)(chi * sizeof(Link)));

	totch = chm + chbm + chu * sizeof(Link) + chi * sizeof(Link);

	sendnumericfmt(sptr, RPL_STATSDEBUG, "Whowas users %d(%ld) away %d(%ld)",
	    wwu, (long)(wwu * sizeof(anUser)),
	    wwa, wwam);
	sendnumericfmt(sptr, RPL_STATSDEBUG, "Whowas array %d(%ld)",
	    NICKNAMEHISTORYLENGTH, wwm);

	totww = wwu * sizeof(anUser) + wwam + wwm;

	sendnumericfmt(sptr, RPL_STATSDEBUG,
	    "Hash: client %d(%ld) chan %d(%ld) watch %d(%ld)",
	    NICK_HASH_TABLE_SIZE,
	    (long)(sizeof(struct list_head) * NICK_HASH_TABLE_SIZE),
	    CHAN_HASH_TABLE_SIZE,
	    (long)(sizeof(aChannel *) * CHAN_HASH_TABLE_SIZE), WATCH_HASH_TABLE_SIZE,
	    (long)(sizeof(aWatch *) * WATCH_HASH_TABLE_SIZE));

	for (link = freelink; link; link = link->next)
		fl++;
	sendnumericfmt(sptr, RPL_STATSDEBUG, "Link blocks free %d(%ld) total %d(%ld)",
	    fl, (long)(fl * sizeof(Link)),
	    flinks, (long)(flinks * sizeof(Link)));

/*	rm = cres_mem(sptr,sptr->name); */
	rm = 0; /* syzop: todo ?????????? */

	tot = totww + totch + totcl + com + cl * sizeof(aClass) + db + rm;
	tot += fl * sizeof(Link);
	tot += sizeof(struct list_head) * NICK_HASH_TABLE_SIZE;
	tot += sizeof(aChannel *) * CHAN_HASH_TABLE_SIZE;
	tot += sizeof(aWatch *) * WATCH_HASH_TABLE_SIZE;

	sendnumericfmt(sptr, RPL_STATSDEBUG, "Total: ww %ld ch %ld cl %ld co %ld db %ld",
	    totww, totch, totcl, com, db);
#if !defined(_WIN32) && !defined(_AMIGA)
#ifdef __alpha
	sendnumericfmt(sptr, RPL_STATSDEBUG, "TOTAL: %d sbrk(0)-etext: %u",
	    tot,
	    (u_int)sbrk((size_t)0) - (u_int)sbrk0);
#else
	sendnumericfmt(sptr, RPL_STATSDEBUG, "TOTAL: %ld sbrk(0)-etext: %lu",
	    tot,
	    (u_long)sbrk((size_t)0) - (u_long)sbrk0);

#endif
#else
	sendnumericfmt(sptr, RPL_STATSDEBUG, "TOTAL: %lu", tot);
#endif
	return 0;
}

int stats_denylinkauto(aClient *sptr, char *para)
{
	ConfigItem_deny_link *links;

	for (links = conf_deny_link; links; links = links->next)
	{
		if (links->flag.type == CRULE_AUTO)
			sendnumeric(sptr, RPL_STATSDLINE,
			'd', links->mask, links->prettyrule);
	}
	return 0;
}

int stats_exceptthrottle(aClient *sptr, char *para)
{
	ConfigItem_except *excepts;
	for (excepts = conf_except; excepts; excepts = excepts->next)
		if (excepts->flag.type == CONF_EXCEPT_THROTTLE)
			sendnumeric(sptr, RPL_STATSELINE, excepts->mask);
	return 0;
}

int stats_denydcc(aClient *sptr, char *para)
{
	ConfigItem_deny_dcc *denytmp;
	ConfigItem_allow_dcc *allowtmp;
	char *filemask, *reason;
	char a = 0;

	for (denytmp = conf_deny_dcc; denytmp; denytmp = denytmp->next)
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
		sendtxtnumeric(sptr, "d %c %c %s %s", (denytmp->flag.type == DCCDENY_SOFT) ? 's' : 'h',
			a, filemask, reason);
	}
	for (allowtmp = conf_allow_dcc; allowtmp; allowtmp = allowtmp->next)
	{
		filemask = BadPtr(allowtmp->filename) ? "<NULL>" : allowtmp->filename;
		if (allowtmp->flag.type2 == CONF_BAN_TYPE_CONF)
			a = 'c';
		if (allowtmp->flag.type2 == CONF_BAN_TYPE_AKILL)
			a = 's';
		if (allowtmp->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
			a = 'o';
		/* <a> <s|h> <howadded> <filemask> */
		sendtxtnumeric(sptr, "a %c %c %s", (allowtmp->flag.type == DCCDENY_SOFT) ? 's' : 'h',
			a, filemask);
	}
	return 0;
}

int stats_kline(aClient *sptr, char *para)
{
	ConfigItem_except *excepts;

	tkl_stats(sptr, TKL_KILL, NULL);
	tkl_stats(sptr, TKL_ZAP, NULL);

	for (excepts = conf_except; excepts; excepts = excepts->next)
	{
		if (excepts->flag.type == CONF_EXCEPT_BAN)
			sendnumeric(sptr, RPL_STATSKLINE, "E", excepts->mask, "");
	}
	return 0;
}

int stats_banrealname(aClient *sptr, char *para)
{
	ConfigItem_ban *bans;
	for (bans = conf_ban; bans; bans = bans->next)
	{
		if (bans->flag.type == CONF_BAN_REALNAME)
		{
			sendnumeric(sptr, RPL_STATSNLINE, bans->mask, bans->reason
				? bans->reason : "<no reason>");
		}
	}
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
	for (dchans = conf_deny_channel; dchans; dchans = dchans->next)
	{
		sendtxtnumeric(sptr, "deny %s %c %s", dchans->channel, dchans->warn ? 'w' : '-', dchans->reason);
	}
  	for (achans = conf_allow_channel; achans; achans = achans->next)
  	{
		sendtxtnumeric(sptr, "allow %s", achans->channel);
	}
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

	for (x = conf_offchans; x; x = x->next)
	{
		sendtxtnumeric(sptr, "%s %s", x->chname, x->topic ? x->topic : "");
	}
	return 0;
}

#define SafePrint(x)   ((x) ? (x) : "")

int stats_set(aClient *sptr, char *para)
{
	char *uhallow;

	if (!ValidatePermissionsForPath("server:info:stats",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	sendtxtnumeric(sptr, "*** Configuration Report ***");
	sendtxtnumeric(sptr, "network-name: %s", ircnetwork);
	sendtxtnumeric(sptr, "default-server: %s", defserv);
	if (SERVICES_NAME)
	{
		sendtxtnumeric(sptr, "services-server: %s", SERVICES_NAME);
	}
	if (STATS_SERVER)
	{
		sendtxtnumeric(sptr, "stats-server: %s", STATS_SERVER);
	}
	if (SASL_SERVER)
	{
		sendtxtnumeric(sptr, "sasl-server: %s", SASL_SERVER);
	}
	sendtxtnumeric(sptr, "hiddenhost-prefix: %s", hidden_host);
	sendtxtnumeric(sptr, "help-channel: %s", helpchan);
	sendtxtnumeric(sptr, "cloak-keys: %s", CLOAK_KEYCRC);
	sendtxtnumeric(sptr, "kline-address: %s", KLINE_ADDRESS);
	if (GLINE_ADDRESS)
		sendtxtnumeric(sptr, "gline-address: %s", GLINE_ADDRESS);
	sendtxtnumeric(sptr, "modes-on-connect: %s", get_modestr(CONN_MODES));
	sendtxtnumeric(sptr, "modes-on-oper: %s", get_modestr(OPER_MODES));
	*modebuf = *parabuf = 0;
	chmode_str(&iConf.modes_on_join, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf));
	sendtxtnumeric(sptr, "modes-on-join: %s %s", modebuf, parabuf);
	sendtxtnumeric(sptr, "nick-length: %i", iConf.nick_length);
	sendtxtnumeric(sptr, "snomask-on-oper: %s", OPER_SNOMASK);
	if (OPER_ONLY_STATS)
	{
		char *longflags = stats_operonly_long_to_short();
		sendtxtnumeric(sptr, "oper-only-stats: %s%s", OPER_ONLY_STATS, longflags ? longflags : "");
	}
	if (RESTRICT_USERMODES)
		sendtxtnumeric(sptr, "restrict-usermodes: %s", RESTRICT_USERMODES);
	if (RESTRICT_CHANNELMODES)
		sendtxtnumeric(sptr, "restrict-channelmodes: %s", RESTRICT_CHANNELMODES);
	if (RESTRICT_EXTENDEDBANS)
		sendtxtnumeric(sptr, "restrict-extendedbans: %s", RESTRICT_EXTENDEDBANS);
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
		sendtxtnumeric(sptr, "allow-userhost-change: %s", uhallow);
	sendtxtnumeric(sptr, "hide-ban-reason: %d", HIDE_BAN_REASON);
	sendtxtnumeric(sptr, "anti-spam-quit-message-time: %s", pretty_time_val(ANTI_SPAM_QUIT_MSG_TIME));
	sendtxtnumeric(sptr, "channel-command-prefix: %s", CHANCMDPFX ? CHANCMDPFX : "`");
	sendtxtnumeric(sptr, "ssl::certificate: %s", SafePrint(iConf.ssl_options->certificate_file));
	sendtxtnumeric(sptr, "ssl::key: %s", SafePrint(iConf.ssl_options->key_file));
	sendtxtnumeric(sptr, "ssl::trusted-ca-file: %s", SafePrint(iConf.ssl_options->trusted_ca_file));
	sendtxtnumeric(sptr, "ssl::options: %s", iConf.ssl_options->options & SSLFLAG_FAILIFNOCERT ? "FAILIFNOCERT" : "");
	sendtxtnumeric(sptr, "options::show-opermotd: %d", SHOWOPERMOTD);
	sendtxtnumeric(sptr, "options::hide-ulines: %d", HIDE_ULINES);
	sendtxtnumeric(sptr, "options::identd-check: %d", IDENT_CHECK);
	sendtxtnumeric(sptr, "options::fail-oper-warn: %d", FAILOPER_WARN);
	sendtxtnumeric(sptr, "options::show-connect-info: %d", SHOWCONNECTINFO);
	sendtxtnumeric(sptr, "options::no-connect-ssl-info: %d", NOCONNECTSSLINFO);
	sendtxtnumeric(sptr, "options::dont-resolve: %d", DONT_RESOLVE);
	sendtxtnumeric(sptr, "options::mkpasswd-for-everyone: %d", MKPASSWD_FOR_EVERYONE);
	sendtxtnumeric(sptr, "options::allow-insane-bans: %d", ALLOW_INSANE_BANS);
	sendtxtnumeric(sptr, "options::allow-part-if-shunned: %d", ALLOW_PART_IF_SHUNNED);
	sendtxtnumeric(sptr, "maxchannelsperuser: %i", MAXCHANNELSPERUSER);
	sendtxtnumeric(sptr, "ping-warning: %i seconds", PINGWARNING);
	sendtxtnumeric(sptr, "auto-join: %s", AUTO_JOIN_CHANS ? AUTO_JOIN_CHANS : "0");
	sendtxtnumeric(sptr, "oper-auto-join: %s", OPER_AUTO_JOIN_CHANS ? OPER_AUTO_JOIN_CHANS : "0");
	sendtxtnumeric(sptr, "static-quit: %s", STATIC_QUIT ? STATIC_QUIT : "<none>");	
	sendtxtnumeric(sptr, "static-part: %s", STATIC_PART ? STATIC_PART : "<none>");	
	sendtxtnumeric(sptr, "who-limit: %d", WHOLIMIT);
	sendtxtnumeric(sptr, "silence-limit: %d", SILENCE_LIMIT);
	if (DNS_BINDIP)
		sendtxtnumeric(sptr, "dns::bind-ip: %s", DNS_BINDIP);
	sendtxtnumeric(sptr, "ban-version-tkl-time: %s", pretty_time_val(BAN_VERSION_TKL_TIME));
	if (LINK_BINDIP)
		sendtxtnumeric(sptr, "link::bind-ip: %s", LINK_BINDIP);
	sendtxtnumeric(sptr, "anti-flood::connect-flood: %d per %s", THROTTLING_COUNT, pretty_time_val(THROTTLING_PERIOD));
	sendtxtnumeric(sptr, "anti-flood::unknown-flood-bantime: %s", pretty_time_val(UNKNOWN_FLOOD_BANTIME));
	sendtxtnumeric(sptr, "anti-flood::unknown-flood-amount: %ldKB", UNKNOWN_FLOOD_AMOUNT);
	if (AWAY_PERIOD)
	{
		sendtxtnumeric(sptr, "anti-flood::away-flood: %d per %s", AWAY_COUNT, pretty_time_val(AWAY_PERIOD));
	}
	sendtxtnumeric(sptr, "anti-flood::nick-flood: %d per %s", NICK_COUNT, pretty_time_val(NICK_PERIOD));
	sendtxtnumeric(sptr, "ident::connect-timeout: %s", pretty_time_val(IDENT_CONNECT_TIMEOUT));
	sendtxtnumeric(sptr, "ident::read-timeout: %s", pretty_time_val(IDENT_READ_TIMEOUT));
	sendtxtnumeric(sptr, "spamfilter::ban-time: %s", pretty_time_val(SPAMFILTER_BAN_TIME));
	sendtxtnumeric(sptr, "spamfilter::ban-reason: %s", SPAMFILTER_BAN_REASON);
	sendtxtnumeric(sptr, "spamfilter::virus-help-channel: %s", SPAMFILTER_VIRUSCHAN);
	if (SPAMFILTER_EXCEPT)
		sendtxtnumeric(sptr, "spamfilter::except: %s", SPAMFILTER_EXCEPT);
	sendtxtnumeric(sptr, "check-target-nick-bans: %s", CHECK_TARGET_NICK_BANS ? "yes" : "no");
	sendtxtnumeric(sptr, "plaintext-policy::user: %s", policy_valtostr(iConf.plaintext_policy_user));
	sendtxtnumeric(sptr, "plaintext-policy::oper: %s", policy_valtostr(iConf.plaintext_policy_oper));
	sendtxtnumeric(sptr, "plaintext-policy::server: %s", policy_valtostr(iConf.plaintext_policy_server));
	sendtxtnumeric(sptr, "outdated-tls-policy::user: %s", policy_valtostr(iConf.outdated_tls_policy_user));
	sendtxtnumeric(sptr, "outdated-tls-policy::oper: %s", policy_valtostr(iConf.outdated_tls_policy_oper));
	sendtxtnumeric(sptr, "outdated-tls-policy::server: %s", policy_valtostr(iConf.outdated_tls_policy_server));
	RunHook2(HOOKTYPE_STATS, sptr, "S");
	return 1;
}

int stats_tld(aClient *sptr, char *para)
{
	ConfigItem_tld *tld;

	for (tld = conf_tld; tld; tld = tld->next)
	{
		sendnumeric(sptr, RPL_STATSTLINE,
			tld->mask, tld->motd_file, tld->rules_file ? 
			tld->rules_file : "none");
	}

	return 0;
}

int stats_uptime(aClient *sptr, char *para)
{
	time_t tmpnow;

	tmpnow = TStime() - me.local->since;
	sendnumeric(sptr, RPL_STATSUPTIME,
	    tmpnow / 86400, (tmpnow / 3600) % 24, (tmpnow / 60) % 60,
	    tmpnow % 60);
	sendnumeric(sptr, RPL_STATSCONN,
	    max_connection_count, IRCstats.me_max);
	return 0;
}

int stats_denyver(aClient *sptr, char *para)
{
	ConfigItem_deny_version *versions;
	for (versions = conf_deny_version; versions; versions = versions->next)
	{
		sendnumeric(sptr, RPL_STATSVLINE,
			versions->version, versions->flags, versions->mask);
	}
	return 0;
}

int stats_notlink(aClient *sptr, char *para)
{
	ConfigItem_link *link_p;

	for (link_p = conf_link; link_p; link_p = link_p->next)
	{
		if (!find_server_quick(link_p->servername))
		{
			sendnumeric(sptr, RPL_STATSXLINE, link_p->servername,
				link_p->outgoing.port);
		}
	}
	return 0;
}

int stats_class(aClient *sptr, char *para)
{
	ConfigItem_class *classes;

	for (classes = conf_class; classes; classes = classes->next)
	{
		sendnumeric(sptr, RPL_STATSYLINE, classes->name, classes->pingfreq, classes->connfreq,
			classes->maxclients, classes->sendq, classes->recvq ? classes->recvq : DEFAULT_RECVQ);
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
	static char Sformat[] = "SendQ SendM SendBytes RcveM RcveBytes Open_since :Idle";
	static char Lformat[] = "%s%s %u %u %u %u %u %u :%u";
#else
	static char Sformat[] = "SendQ SendM SendBytes RcveM RcveBytes Open_since CPU :Idle";
	static char Lformat[] = "%s%s %u %u %u %u %u %u %s";
	char pbuf[96];		/* Should be enough for to ints */
#endif
	int remote = 0;
	int wilds = 0;
	int doall = 0;
	int showports = ValidatePermissionsForPath("server:info:stats",sptr,NULL,NULL,NULL);
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
	sendnumericfmt(sptr, RPL_STATSLINKINFO, "%s", Sformat);
	if (!MyClient(sptr))
	{
		remote = 1;
		wilds = 0;
	}

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsInvisible(acptr) && (doall || wilds) &&
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
		if (ValidatePermissionsForPath("server:info:stats",sptr,NULL,NULL,NULL))
		{
			sendnumericfmt(sptr, RPL_STATSLINKINFO, Lformat,
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
		}
		else if (!strchr(acptr->name, '.'))
			sendnumericfmt(sptr, RPL_STATSLINKINFO, Lformat,
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
			sendnotice(sptr, "Server %s is %s",
				acptr->name, acptr->serv->flags.synced ? "SYNCED" : "NOT SYNCED!!");
	}
#endif
	return 0;
}
