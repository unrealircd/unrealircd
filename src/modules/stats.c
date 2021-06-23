/*
 *   IRC - Internet Relay Chat, src/modules/stats.c
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

CMD_FUNC(cmd_stats);

#define MSG_STATS 	"STATS"

ModuleHeader MOD_HEADER
  = {
	"stats",
	"5.0",
	"command /stats",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_STATS, cmd_stats, 3, CMD_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

extern MODVAR int  max_connection_count;
extern char *get_client_name2(Client *, int);

int stats_banversion(Client *, char *);
int stats_links(Client *, char *);
int stats_denylinkall(Client *, char *);
int stats_gline(Client *, char *);
int stats_except(Client *, char *);
int stats_allow(Client *, char *);
int stats_command(Client *, char *);
int stats_oper(Client *, char *);
int stats_port(Client *, char *);
int stats_bannick(Client *, char *);
int stats_traffic(Client *, char *);
int stats_uline(Client *, char *);
int stats_vhost(Client *, char *);
int stats_denylinkauto(Client *, char *);
int stats_kline(Client *, char *);
int stats_banrealname(Client *, char *);
int stats_sqline(Client *, char *);
int stats_linkinfoint(Client *, char *, int);
int stats_linkinfo(Client *, char *);
int stats_linkinfoall(Client *, char *);
int stats_chanrestrict(Client *, char *);
int stats_shun(Client *, char *);
int stats_set(Client *, char *);
int stats_tld(Client *, char *);
int stats_uptime(Client *, char *);
int stats_denyver(Client *, char *);
int stats_notlink(Client *, char *);
int stats_class(Client *, char *);
int stats_officialchannels(Client *, char *);
int stats_spamfilter(Client *, char *);
int stats_fdtable(Client *, char *);

#define SERVER_AS_PARA 0x1
#define FLAGS_AS_PARA 0x2

struct statstab {
	char flag;
	char *longflag;
	int (*func)(Client *client, char *para);
	int options;
};

/* Must be listed lexicographically */
/* Long flags must be lowercase */
struct statstab StatsTable[] = {
	{ 'B', "banversion",	stats_banversion,	0		},
	{ 'C', "link", 		stats_links,		0 		},
	{ 'D', "denylinkall",	stats_denylinkall,	0		},
	{ 'G', "gline",		stats_gline,		FLAGS_AS_PARA	},
	{ 'H', "link",	 	stats_links,		0 		},
	{ 'I', "allow",		stats_allow,		0 		},
	{ 'K', "kline",		stats_kline,		0 		},
	{ 'L', "linkinfoall",	stats_linkinfoall,	SERVER_AS_PARA	},
	{ 'M', "command",	stats_command,		0 		},
	{ 'O', "oper",		stats_oper,		0 		},
	{ 'P', "port",		stats_port,		0 		},
	{ 'Q', "sqline",	stats_sqline,		FLAGS_AS_PARA 	},
	{ 'S', "set",		stats_set,		0		},
	{ 'T', "traffic",	stats_traffic,		0 		},
	{ 'U', "uline",		stats_uline,		0 		},
	{ 'V', "vhost", 	stats_vhost,		0 		},
	{ 'W', "fdtable",       stats_fdtable,          0               },
	{ 'X', "notlink",	stats_notlink,		0 		},
	{ 'Y', "class",		stats_class,		0 		},
	{ 'c', "link", 		stats_links,		0 		},
	{ 'd', "denylinkauto",	stats_denylinkauto,	0 		},
	{ 'e', "except",	stats_except,		0 		},
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

static inline void stats_help(Client *client)
{
	sendnumeric(client, RPL_STATSHELP, "/Stats flags:");
	sendnumeric(client, RPL_STATSHELP, "B - banversion - Send the ban version list");
	sendnumeric(client, RPL_STATSHELP, "b - badword - Send the badwords list");
	sendnumeric(client, RPL_STATSHELP, "C - link - Send the link block list");
	sendnumeric(client, RPL_STATSHELP, "d - denylinkauto - Send the deny link (auto) block list");
	sendnumeric(client, RPL_STATSHELP, "D - denylinkall - Send the deny link (all) block list");
	sendnumeric(client, RPL_STATSHELP, "e - except - Send the ban exception list (ELINEs and in config))");
	sendnumeric(client, RPL_STATSHELP, "f - spamfilter - Send the spamfilter list");
	sendnumeric(client, RPL_STATSHELP, "F - denydcc - Send the deny dcc and allow dcc block lists");
	sendnumeric(client, RPL_STATSHELP, "G - gline - Send the gline and gzline list");
	sendnumeric(client, RPL_STATSHELP, "  Extended flags: [+/-mrs] [mask] [reason] [setby]");
	sendnumeric(client, RPL_STATSHELP, "   m Return glines matching/not matching the specified mask");
	sendnumeric(client, RPL_STATSHELP, "   r Return glines with a reason matching/not matching the specified reason");
	sendnumeric(client, RPL_STATSHELP, "   s Return glines set by/not set by clients matching the specified name");
	sendnumeric(client, RPL_STATSHELP, "I - allow - Send the allow block list");
	sendnumeric(client, RPL_STATSHELP, "j - officialchans - Send the offical channels list");
	sendnumeric(client, RPL_STATSHELP, "K - kline - Send the ban user/ban ip/except ban block list");
	sendnumeric(client, RPL_STATSHELP, "l - linkinfo - Send link information");
	sendnumeric(client, RPL_STATSHELP, "L - linkinfoall - Send all link information");
	sendnumeric(client, RPL_STATSHELP, "M - command - Send list of how many times each command was used");
	sendnumeric(client, RPL_STATSHELP, "n - banrealname - Send the ban realname block list");
	sendnumeric(client, RPL_STATSHELP, "O - oper - Send the oper block list");
	sendnumeric(client, RPL_STATSHELP, "P - port - Send information about ports");
	sendnumeric(client, RPL_STATSHELP, "q - bannick - Send the ban nick block list");
	sendnumeric(client, RPL_STATSHELP, "Q - sqline - Send the global qline list");
	sendnumeric(client, RPL_STATSHELP, "r - chanrestrict - Send the channel deny/allow block list");
	sendnumeric(client, RPL_STATSHELP, "S - set - Send the set block list");
	sendnumeric(client, RPL_STATSHELP, "s - shun - Send the shun list");
	sendnumeric(client, RPL_STATSHELP, "  Extended flags: [+/-mrs] [mask] [reason] [setby]");
	sendnumeric(client, RPL_STATSHELP, "   m Return shuns matching/not matching the specified mask");
	sendnumeric(client, RPL_STATSHELP, "   r Return shuns with a reason matching/not matching the specified reason");
	sendnumeric(client, RPL_STATSHELP, "   s Return shuns set by/not set by clients matching the specified name");
	sendnumeric(client, RPL_STATSHELP, "t - tld - Send the tld block list");
	sendnumeric(client, RPL_STATSHELP, "T - traffic - Send traffic information");
	sendnumeric(client, RPL_STATSHELP, "u - uptime - Send the server uptime and connection count");
	sendnumeric(client, RPL_STATSHELP, "U - uline - Send the ulines block list");
	sendnumeric(client, RPL_STATSHELP, "v - denyver - Send the deny version block list");
	sendnumeric(client, RPL_STATSHELP, "V - vhost - Send the vhost block list");
	sendnumeric(client, RPL_STATSHELP, "W - fdtable - Send the FD table listing");
	sendnumeric(client, RPL_STATSHELP, "X - notlink - Send the list of servers that are not current linked");
	sendnumeric(client, RPL_STATSHELP, "Y - class - Send the class block list");
}

static inline int allow_user_stats_short(char c)
{
	char l;
	if (!ALLOW_USER_STATS)
		return 0;
	if (strchr(ALLOW_USER_STATS, c))
		return 1;
	l = tolower(c);
	/* Hack for the flags that are case insensitive */
	if (l == 'o' || l == 'y' || l == 'k' || l == 'g' || l == 'x' || l == 'c' ||
		l =='f' || l == 'i' || l == 'h' || l == 'm')
	{
		if (islower(c) && strchr(ALLOW_USER_STATS, toupper(c)))
			return 1;
		else if (isupper(c) && strchr(ALLOW_USER_STATS, tolower(c)))
			return 1;
	}
	/* Hack for c/C/H/h */
	if (l == 'c')
	{
		if (strpbrk(ALLOW_USER_STATS, "hH"))
			return 1;
	} else if (l == 'h')
		if (strpbrk(ALLOW_USER_STATS, "cC"))
			return 1;
	return 0;
}

static inline int allow_user_stats_long(char *s)
{
	OperStat *os;
	for (os = iConf.allow_user_stats_ext; os; os = os->next)
	{
		if (!strcasecmp(os->flag, s))
			return 1;
	}
	return 0;
}

/* This is pretty slow, but it isn't used often so it isn't a big deal */
static inline char *allow_user_stats_long_to_short()
{
	static char buffer[BUFSIZE+1];
	int i = 0;
	OperStat *os;
	for (os = iConf.allow_user_stats_ext; os; os = os->next)
	{
		struct statstab *stat = stats_search(os->flag);
		if (!stat)
			continue;
		if (!strchr(ALLOW_USER_STATS, stat->flag))
			buffer[i++] = stat->flag;
	}
	buffer[i] = 0;
	return buffer;
}

CMD_FUNC(cmd_stats)
{
	struct statstab *stat;
	char flags[2];

	if (parc == 3 && parv[2][0] != '+' && parv[2][0] != '-')
	{
		if (hunt_server(client, recv_mtags, ":%s STATS %s :%s", 2, parc, parv) != HUNTED_ISME)
			return;
	}
	else if (parc == 4 && parv[2][0] != '+' && parv[2][0] != '-')
	{
		if (hunt_server(client, recv_mtags, ":%s STATS %s %s %s", 2, parc, parv) != HUNTED_ISME)
			return;
	}
	if (parc < 2 || !*parv[1])
	{
		stats_help(client);
		sendnumeric(client, RPL_ENDOFSTATS, '*');
		return;
	}

	/* Decide if we are looking for 1 char or a string */
	if (parv[1][0] && !parv[1][1])
	{
		if (!ValidatePermissionsForPath("server:info:stats",client,NULL,NULL,NULL) && !allow_user_stats_short(parv[1][0]))
		{
			sendnumeric(client, ERR_NOPRIVILEGES);
			return;
		}
		/* Old style, we can use a binary search here */
		stat = stats_binary_search(parv[1][0]);
	}
	else
	{
		if (!ValidatePermissionsForPath("server:info:stats",client,NULL,NULL,NULL) && !allow_user_stats_long(parv[1]))
		{
			sendnumeric(client, ERR_NOPRIVILEGES);
			return;
		}
		/* New style, search the hard way */
		stat = stats_search(parv[1]);
	}

	if (!stat)
	{
		/* Not found. Perhaps a module provides it? */
		Hook *h;
		int found = 0, n;
		for (h = Hooks[HOOKTYPE_STATS]; h; h = h->next)
		{
			n = (*(h->func.intfunc))(client, parv[1]);
			if (n == 1)
				found = 1;
		}
		if (!found)
			stats_help(client);
		sendnumeric(client, RPL_ENDOFSTATS, '*');
		return;
	}

	flags[0] = stat->flag;
	flags[1] = '\0';

	if (stat->options & FLAGS_AS_PARA)
	{
		if (parc > 2 && (parv[2][0] == '+' || parv[2][0] == '-'))
		{
			if (parc > 3)
				stat->func(client, stats_combine_parv(parv[2],parv[3]));
			else
				stat->func(client, parv[2]);
		}
		else if (parc > 3)
			stat->func(client, parv[3]);
		else
			stat->func(client, NULL);
	}
	else if (stat->options & SERVER_AS_PARA)
	{
		if (parc > 2)
			stat->func(client, parv[2]);
		else
			stat->func(client, NULL);
	}
	else
		stat->func(client, NULL);

	/* Modules can append data:
	 * ('STATS S' already has special code for this that
	 *  maintains certain ordering, so not included here)
	 */
	if (stat->flag != 'S')
	{
		RunHook2(HOOKTYPE_STATS, client, flags);
	}

	sendnumeric(client, RPL_ENDOFSTATS, stat->flag);

	if (!IsULine(client))
		sendto_snomask(SNO_EYES, "Stats \'%c\' requested by %s (%s@%s)",
			stat->flag, client->name, client->user->username, GetHost(client));
	else
		sendto_snomask(SNO_JUNK, "Stats \'%c\' requested by %s (%s@%s) [ulined]",
			stat->flag, client->name, client->user->username, GetHost(client));
}

int stats_banversion(Client *client, char *para)
{
	ConfigItem_ban *bans;
	for (bans = conf_ban; bans; bans = bans->next)
	{
		if (bans->flag.type != CONF_BAN_VERSION)
			continue;
		sendnumeric(client, RPL_STATSBANVER,
			bans->mask, bans->reason ? bans->reason : "No Reason");
	}
	return 0;
}

int stats_links(Client *client, char *para)
{
	ConfigItem_link *link_p;
#ifdef DEBUGMODE
	Client *acptr;
#endif
	for (link_p = conf_link; link_p; link_p = link_p->next)
	{
		sendnumericfmt(client, RPL_STATSCLINE, "C - * %s %i %s %s%s%s",
			link_p->servername,
			link_p->outgoing.port,
			link_p->class->name,
			(link_p->outgoing.options & CONNECT_AUTO) ? "a" : "",
			(link_p->outgoing.options & CONNECT_TLS) ? "S" : "",
			(link_p->flag.temporary == 1) ? "T" : "");
#ifdef DEBUGMODE
		sendnotice(client, "%s (%p) has refcount %d",
			link_p->servername, link_p, link_p->refcount);
#endif
		if (link_p->hub)
			sendnumericfmt(client, RPL_STATSHLINE, "H %s * %s",
				link_p->hub, link_p->servername);
		else if (link_p->leaf)
			sendnumericfmt(client, RPL_STATSLLINE, "L %s * %s %d",
				link_p->leaf, link_p->servername, link_p->leaf_depth);
		// TODO: send incoming allow list? (for opers only)
	}
#ifdef DEBUGMODE
	list_for_each_entry(acptr, &client_list, client_node)
		if (MyConnect(acptr) && acptr->serv && !IsMe(acptr))
		{
			if (!acptr->serv->conf)
				sendnotice(client, "client '%s' (%p) has NO CONF attached (? :P)",
					acptr->name, acptr);
			else
				sendnotice(client, "client '%s' (%p) has conf %p attached, refcount: %d, temporary: %s",
					acptr->name, acptr,
					acptr->serv->conf,
					acptr->serv->conf->refcount,
					acptr->serv->conf->flag.temporary ? "YES" : "NO");
		}
#endif
	return 0;
}

int stats_denylinkall(Client *client, char *para)
{
	ConfigItem_deny_link *links;

	for (links = conf_deny_link; links; links = links->next)
	{
		if (links->flag.type == CRULE_ALL)
			sendnumeric(client, RPL_STATSDLINE,
			'D', links->mask, links->prettyrule);
	}
	return 0;
}

int stats_gline(Client *client, char *para)
{
	int cnt = 0;
	tkl_stats(client, TKL_GLOBAL|TKL_KILL, para, &cnt);
	tkl_stats(client, TKL_GLOBAL|TKL_ZAP, para, &cnt);
	return 0;
}

int stats_spamfilter(Client *client, char *para)
{
	int cnt = 0;
	tkl_stats(client, TKL_SPAMF, para, &cnt);
	tkl_stats(client, TKL_GLOBAL|TKL_SPAMF, para, &cnt);
	return 0;
}

int stats_except(Client *client, char *para)
{
	int cnt = 0;
	tkl_stats(client, TKL_EXCEPTION, para, &cnt);
	tkl_stats(client, TKL_EXCEPTION|TKL_GLOBAL, para, &cnt);
	return 0;
}

int stats_allow(Client *client, char *para)
{
	ConfigItem_allow *allows;
	ConfigItem_mask *m;

	for (allows = conf_allow; allows; allows = allows->next)
	{
		for (m = allows->mask; m; m = m->next)
		{
			sendnumeric(client, RPL_STATSILINE,
				    m->mask, "-",
				    allows->maxperip,
				    allows->global_maxperip,
				    allows->class->name,
				    allows->server ? allows->server : defserv,
				    allows->port ? allows->port : 6667);
		}
	}
	return 0;
}

int stats_command(Client *client, char *para)
{
	int i;
	RealCommand *mptr;
	for (i = 0; i < 256; i++)
		for (mptr = CommandHash[i]; mptr; mptr = mptr->next)
			if (mptr->count)
#ifndef DEBUGMODE
			sendnumeric(client, RPL_STATSCOMMANDS, mptr->cmd,
				mptr->count, mptr->bytes);
#else
			sendnumeric(client, RPL_STATSCOMMANDS, mptr->cmd,
				mptr->count, mptr->bytes,
				mptr->lticks, mptr->lticks / CLOCKS_PER_SEC,
				mptr->rticks, mptr->rticks / CLOCKS_PER_SEC);
#endif

	return 0;
}

int stats_oper(Client *client, char *para)
{
	ConfigItem_oper *oper_p;
	ConfigItem_mask *m;

	for (oper_p = conf_oper; oper_p; oper_p = oper_p->next)
	{
		for (m = oper_p->mask; m; m = m->next)
		{
	  		sendnumeric(client, RPL_STATSOLINE,
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
	    (listener->options & LISTENER_TLS)?         "tls ": "",
	    !(listener->options & LISTENER_TLS)?        "plaintext ": "");
	return buf;
}

int stats_port(Client *client, char *para)
{
	ConfigItem_listen *listener;

	for (listener = conf_listen; listener != NULL; listener = listener->next)
	{
		if (!(listener->options & LISTENER_BOUND))
			continue;
		if ((listener->options & LISTENER_SERVERSONLY) && !ValidatePermissionsForPath("server:info:stats",client,NULL,NULL,NULL))
			continue;
		sendnotice(client, "*** Listener on %s:%i (%s): has %i client(s), options: %s %s",
		           listener->ip,
		           listener->port,
		           listener->ipv6 ? "IPv6" : "IPv4",
		           listener->clients,
		           stats_port_helper(listener),
		           listener->flag.temporary ? "[TEMPORARY]" : "");
	}
	return 0;
}

int stats_bannick(Client *client, char *para)
{
	int cnt = 0;
	tkl_stats(client, TKL_NAME, para, &cnt);
	tkl_stats(client, TKL_GLOBAL|TKL_NAME, para, &cnt);
	return 0;
}

int stats_traffic(Client *client, char *para)
{
	Client *acptr;
	IRCStatistics *sp;
	IRCStatistics tmp;
	time_t now = TStime();

	sp = &tmp;
	memcpy(sp, &ircstats, sizeof(IRCStatistics));

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
		else if (IsUser(acptr))
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

	sendnumericfmt(client, RPL_STATSDEBUG, "accepts %u refused %u", sp->is_ac, sp->is_ref);
	sendnumericfmt(client, RPL_STATSDEBUG, "unknown commands %u prefixes %u", sp->is_unco, sp->is_unpf);
	sendnumericfmt(client, RPL_STATSDEBUG, "nick collisions %u unknown closes %u", sp->is_kill, sp->is_ni);
	sendnumericfmt(client, RPL_STATSDEBUG, "wrong direction %u empty %u", sp->is_wrdi, sp->is_empt);
	sendnumericfmt(client, RPL_STATSDEBUG, "numerics seen %u mode fakes %u", sp->is_num, sp->is_fake);
	sendnumericfmt(client, RPL_STATSDEBUG, "auth successes %u fails %u", sp->is_asuc, sp->is_abad);
	sendnumericfmt(client, RPL_STATSDEBUG, "local connections %u udp packets %u", sp->is_loc, sp->is_udp);
	sendnumericfmt(client, RPL_STATSDEBUG, "Client Server");
	sendnumericfmt(client, RPL_STATSDEBUG, "connected %u %u", sp->is_cl, sp->is_sv);
	sendnumericfmt(client, RPL_STATSDEBUG, "bytes sent %ld.%huK %ld.%huK",
		sp->is_cks, sp->is_cbs, sp->is_sks, sp->is_sbs);
	sendnumericfmt(client, RPL_STATSDEBUG, "bytes recv %ld.%huK %ld.%huK",
	    sp->is_ckr, sp->is_cbr, sp->is_skr, sp->is_sbr);
	sendnumericfmt(client, RPL_STATSDEBUG, "time connected %lld %lld",
	    (long long)sp->is_cti, (long long)sp->is_sti);

	return 0;
}

int stats_fdtable(Client *client, char *para)
{
	int i;

	for (i = 0; i < MAXCONNECTIONS; i++)
	{
		FDEntry *fde = &fd_table[i];

		if (!fde->is_open)
			continue;

		sendnumericfmt(client, RPL_STATSDEBUG,
			"fd %3d, desc '%s', read-hdl %p, write-hdl %p, cbdata %p",
			fde->fd, fde->desc, fde->read_callback, fde->write_callback, fde->data);
	}

	return 0;
}

int stats_uline(Client *client, char *para)
{
	ConfigItem_ulines *ulines;
	for (ulines = conf_ulines; ulines; ulines = ulines->next)
		sendnumeric(client, RPL_STATSULINE, ulines->servername);
	return 0;
}
int stats_vhost(Client *client, char *para)
{
	ConfigItem_mask *m;
	ConfigItem_vhost *vhosts;

	for (vhosts = conf_vhost; vhosts; vhosts = vhosts->next)
	{
		for (m = vhosts->mask; m; m = m->next)
		{
			sendtxtnumeric(client, "vhost %s%s%s %s %s", vhosts->virtuser ? vhosts->virtuser : "", vhosts->virtuser ? "@" : "",
			     vhosts->virthost, vhosts->login, m->mask);
		}
	}
	return 0;
}

int stats_denylinkauto(Client *client, char *para)
{
	ConfigItem_deny_link *links;

	for (links = conf_deny_link; links; links = links->next)
	{
		if (links->flag.type == CRULE_AUTO)
			sendnumeric(client, RPL_STATSDLINE,
			'd', links->mask, links->prettyrule);
	}
	return 0;
}

int stats_kline(Client *client, char *para)
{
	int cnt = 0;
	tkl_stats(client, TKL_KILL, NULL, &cnt);
	tkl_stats(client, TKL_ZAP, NULL, &cnt);
	return 0;
}

int stats_banrealname(Client *client, char *para)
{
	ConfigItem_ban *bans;
	for (bans = conf_ban; bans; bans = bans->next)
	{
		if (bans->flag.type == CONF_BAN_REALNAME)
		{
			sendnumeric(client, RPL_STATSNLINE, bans->mask, bans->reason
				? bans->reason : "<no reason>");
		}
	}
	return 0;
}

int stats_sqline(Client *client, char *para)
{
	int cnt = 0;
	tkl_stats(client, TKL_NAME|TKL_GLOBAL, para, &cnt);
	return 0;
}

int stats_chanrestrict(Client *client, char *para)
{
	ConfigItem_deny_channel *dchans;
	ConfigItem_allow_channel *achans;
	for (dchans = conf_deny_channel; dchans; dchans = dchans->next)
	{
		sendtxtnumeric(client, "deny %s %c %s", dchans->channel, dchans->warn ? 'w' : '-', dchans->reason);
	}
  	for (achans = conf_allow_channel; achans; achans = achans->next)
  	{
		sendtxtnumeric(client, "allow %s", achans->channel);
	}
	return 0;
}

int stats_shun(Client *client, char *para)
{
	int cnt = 0;
	tkl_stats(client, TKL_GLOBAL|TKL_SHUN, para, &cnt);
	return 0;
}

/* should this be moved to a seperate stats flag? */
int stats_officialchannels(Client *client, char *para)
{
	ConfigItem_offchans *x;

	for (x = conf_offchans; x; x = x->next)
	{
		sendtxtnumeric(client, "%s %s", x->chname, x->topic ? x->topic : "");
	}
	return 0;
}

#define SafePrint(x)   ((x) ? (x) : "")

/** Helper for stats_set() */
static void stats_set_anti_flood(Client *client, FloodSettings *f)
{
	int i;

	for (i=0; floodoption_names[i]; i++)
	{
		if (i == FLD_CONVERSATIONS)
		{
			sendtxtnumeric(client, "anti-flood::%s::%s: %d users, new user every %s",
				f->name, floodoption_names[i],
				(int)f->limit[i], pretty_time_val(f->period[i]));
		}
		if (i == FLD_LAG_PENALTY)
		{
			sendtxtnumeric(client, "anti-flood::%s::lag-penalty: %d msec",
				f->name, (int)f->period[i]);
			sendtxtnumeric(client, "anti-flood::%s::lag-penalty-bytes: %d",
				f->name,
				f->limit[i] == INT_MAX ? 0 : (int)f->limit[i]);
		}
		else
		{
			sendtxtnumeric(client, "anti-flood::%s::%s: %d per %s",
				f->name, floodoption_names[i],
				(int)f->limit[i], pretty_time_val(f->period[i]));
		}
	}
}

int stats_set(Client *client, char *para)
{
	char *uhallow;
	SecurityGroup *s;
	FloodSettings *f;

	if (!ValidatePermissionsForPath("server:info:stats",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return 0;
	}

	sendtxtnumeric(client, "*** Configuration Report ***");
	sendtxtnumeric(client, "network-name: %s", ircnetwork);
	sendtxtnumeric(client, "default-server: %s", defserv);
	if (SERVICES_NAME)
	{
		sendtxtnumeric(client, "services-server: %s", SERVICES_NAME);
	}
	if (STATS_SERVER)
	{
		sendtxtnumeric(client, "stats-server: %s", STATS_SERVER);
	}
	if (SASL_SERVER)
	{
		sendtxtnumeric(client, "sasl-server: %s", SASL_SERVER);
	}
	sendtxtnumeric(client, "hiddenhost-prefix: %s", hidden_host);
	sendtxtnumeric(client, "help-channel: %s", helpchan);
	sendtxtnumeric(client, "cloak-keys: %s", CLOAK_KEYCRC);
	sendtxtnumeric(client, "kline-address: %s", KLINE_ADDRESS);
	if (GLINE_ADDRESS)
		sendtxtnumeric(client, "gline-address: %s", GLINE_ADDRESS);
	sendtxtnumeric(client, "modes-on-connect: %s", get_usermode_string_raw(CONN_MODES));
	sendtxtnumeric(client, "modes-on-oper: %s", get_usermode_string_raw(OPER_MODES));
	*modebuf = *parabuf = 0;
	chmode_str(&iConf.modes_on_join, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf));
	sendtxtnumeric(client, "modes-on-join: %s %s", modebuf, parabuf);
	if (iConf.min_nick_length)
		sendtxtnumeric(client, "min-nick-length: %i", iConf.min_nick_length);
	sendtxtnumeric(client, "nick-length: %i", iConf.nick_length);
	sendtxtnumeric(client, "snomask-on-oper: %s", OPER_SNOMASK);
	if (ALLOW_USER_STATS)
	{
		char *longflags = allow_user_stats_long_to_short();
		sendtxtnumeric(client, "allow-user-stats: %s%s", ALLOW_USER_STATS, longflags ? longflags : "");
	}
	if (RESTRICT_USERMODES)
		sendtxtnumeric(client, "restrict-usermodes: %s", RESTRICT_USERMODES);
	if (RESTRICT_CHANNELMODES)
		sendtxtnumeric(client, "restrict-channelmodes: %s", RESTRICT_CHANNELMODES);
	if (RESTRICT_EXTENDEDBANS)
		sendtxtnumeric(client, "restrict-extendedbans: %s", RESTRICT_EXTENDEDBANS);
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
		sendtxtnumeric(client, "allow-userhost-change: %s", uhallow);
	sendtxtnumeric(client, "hide-ban-reason: %d", HIDE_BAN_REASON);
	sendtxtnumeric(client, "anti-spam-quit-message-time: %s", pretty_time_val(ANTI_SPAM_QUIT_MSG_TIME));
	sendtxtnumeric(client, "channel-command-prefix: %s", CHANCMDPFX ? CHANCMDPFX : "`");
	sendtxtnumeric(client, "tls::certificate: %s", SafePrint(iConf.tls_options->certificate_file));
	sendtxtnumeric(client, "tls::key: %s", SafePrint(iConf.tls_options->key_file));
	sendtxtnumeric(client, "tls::trusted-ca-file: %s", SafePrint(iConf.tls_options->trusted_ca_file));
	sendtxtnumeric(client, "tls::options: %s", iConf.tls_options->options & TLSFLAG_FAILIFNOCERT ? "FAILIFNOCERT" : "");
	sendtxtnumeric(client, "options::show-opermotd: %d", SHOWOPERMOTD);
	sendtxtnumeric(client, "options::hide-ulines: %d", HIDE_ULINES);
	sendtxtnumeric(client, "options::identd-check: %d", IDENT_CHECK);
	sendtxtnumeric(client, "options::fail-oper-warn: %d", FAILOPER_WARN);
	sendtxtnumeric(client, "options::show-connect-info: %d", SHOWCONNECTINFO);
	sendtxtnumeric(client, "options::no-connect-tls-info: %d", NOCONNECTTLSLINFO);
	sendtxtnumeric(client, "options::dont-resolve: %d", DONT_RESOLVE);
	sendtxtnumeric(client, "options::mkpasswd-for-everyone: %d", MKPASSWD_FOR_EVERYONE);
	sendtxtnumeric(client, "options::allow-insane-bans: %d", ALLOW_INSANE_BANS);
	sendtxtnumeric(client, "options::allow-part-if-shunned: %d", ALLOW_PART_IF_SHUNNED);
	sendtxtnumeric(client, "maxchannelsperuser: %i", MAXCHANNELSPERUSER);
	sendtxtnumeric(client, "ping-warning: %i seconds", PINGWARNING);
	sendtxtnumeric(client, "auto-join: %s", AUTO_JOIN_CHANS ? AUTO_JOIN_CHANS : "0");
	sendtxtnumeric(client, "oper-auto-join: %s", OPER_AUTO_JOIN_CHANS ? OPER_AUTO_JOIN_CHANS : "0");
	sendtxtnumeric(client, "static-quit: %s", STATIC_QUIT ? STATIC_QUIT : "<none>");
	sendtxtnumeric(client, "static-part: %s", STATIC_PART ? STATIC_PART : "<none>");
	sendtxtnumeric(client, "who-limit: %d", WHOLIMIT);
	sendtxtnumeric(client, "silence-limit: %d", SILENCE_LIMIT);
	if (DNS_BINDIP)
		sendtxtnumeric(client, "dns::bind-ip: %s", DNS_BINDIP);
	sendtxtnumeric(client, "ban-version-tkl-time: %s", pretty_time_val(BAN_VERSION_TKL_TIME));
	if (LINK_BINDIP)
		sendtxtnumeric(client, "link::bind-ip: %s", LINK_BINDIP);
	sendtxtnumeric(client, "anti-flood::connect-flood: %d per %s", THROTTLING_COUNT, pretty_time_val(THROTTLING_PERIOD));
	sendtxtnumeric(client, "anti-flood::handshake-data-flood::amount: %ld bytes", iConf.handshake_data_flood_amount);
	sendtxtnumeric(client, "anti-flood::handshake-data-flood::ban-action: %s", banact_valtostring(iConf.handshake_data_flood_ban_action));
	sendtxtnumeric(client, "anti-flood::handshake-data-flood::ban-time: %s", pretty_time_val(iConf.handshake_data_flood_ban_time));

	/* set::anti-flood */
	for (s = securitygroups; s; s = s->next)
		if ((f = find_floodsettings_block(s->name)))
			stats_set_anti_flood(client, f);
	f = find_floodsettings_block("unknown-users");
	stats_set_anti_flood(client, f);

	//if (AWAY_PERIOD)
	//	sendtxtnumeric(client, "anti-flood::away-flood: %d per %s", AWAY_COUNT, pretty_time_val(AWAY_PERIOD));
	//sendtxtnumeric(client, "anti-flood::nick-flood: %d per %s", NICK_COUNT, pretty_time_val(NICK_PERIOD));
	sendtxtnumeric(client, "handshake-timeout: %s", pretty_time_val(iConf.handshake_timeout));
	sendtxtnumeric(client, "sasl-timeout: %s", pretty_time_val(iConf.sasl_timeout));
	sendtxtnumeric(client, "ident::connect-timeout: %s", pretty_time_val(IDENT_CONNECT_TIMEOUT));
	sendtxtnumeric(client, "ident::read-timeout: %s", pretty_time_val(IDENT_READ_TIMEOUT));
	sendtxtnumeric(client, "spamfilter::ban-time: %s", pretty_time_val(SPAMFILTER_BAN_TIME));
	sendtxtnumeric(client, "spamfilter::ban-reason: %s", SPAMFILTER_BAN_REASON);
	sendtxtnumeric(client, "spamfilter::virus-help-channel: %s", SPAMFILTER_VIRUSCHAN);
	if (SPAMFILTER_EXCEPT)
		sendtxtnumeric(client, "spamfilter::except: %s", SPAMFILTER_EXCEPT);
	sendtxtnumeric(client, "check-target-nick-bans: %s", CHECK_TARGET_NICK_BANS ? "yes" : "no");
	sendtxtnumeric(client, "plaintext-policy::user: %s", policy_valtostr(iConf.plaintext_policy_user));
	sendtxtnumeric(client, "plaintext-policy::oper: %s", policy_valtostr(iConf.plaintext_policy_oper));
	sendtxtnumeric(client, "plaintext-policy::server: %s", policy_valtostr(iConf.plaintext_policy_server));
	sendtxtnumeric(client, "outdated-tls-policy::user: %s", policy_valtostr(iConf.outdated_tls_policy_user));
	sendtxtnumeric(client, "outdated-tls-policy::oper: %s", policy_valtostr(iConf.outdated_tls_policy_oper));
	sendtxtnumeric(client, "outdated-tls-policy::server: %s", policy_valtostr(iConf.outdated_tls_policy_server));
	RunHook2(HOOKTYPE_STATS, client, "S");
#ifndef _WIN32
	sendtxtnumeric(client, "This server can handle %d concurrent sockets (%d clients + %d reserve)",
		maxclients+CLIENTS_RESERVE, maxclients, CLIENTS_RESERVE);
#endif
	return 1;
}

int stats_tld(Client *client, char *para)
{
	ConfigItem_tld *tld;

	for (tld = conf_tld; tld; tld = tld->next)
	{
		sendnumeric(client, RPL_STATSTLINE,
			tld->mask, tld->motd_file, tld->rules_file ?
			tld->rules_file : "none");
	}

	return 0;
}

int stats_uptime(Client *client, char *para)
{
	time_t tmpnow;

	tmpnow = TStime() - me.local->since;
	sendnumeric(client, RPL_STATSUPTIME,
	    tmpnow / 86400, (tmpnow / 3600) % 24, (tmpnow / 60) % 60,
	    tmpnow % 60);
	sendnumeric(client, RPL_STATSCONN,
	    max_connection_count, irccounts.me_max);
	return 0;
}

int stats_denyver(Client *client, char *para)
{
	ConfigItem_deny_version *versions;
	for (versions = conf_deny_version; versions; versions = versions->next)
	{
		sendnumeric(client, RPL_STATSVLINE,
			versions->version, versions->flags, versions->mask);
	}
	return 0;
}

int stats_notlink(Client *client, char *para)
{
	ConfigItem_link *link_p;

	for (link_p = conf_link; link_p; link_p = link_p->next)
	{
		if (!find_server_quick(link_p->servername))
		{
			sendnumeric(client, RPL_STATSXLINE, link_p->servername,
				link_p->outgoing.port);
		}
	}
	return 0;
}

int stats_class(Client *client, char *para)
{
	ConfigItem_class *classes;

	for (classes = conf_class; classes; classes = classes->next)
	{
		sendnumeric(client, RPL_STATSYLINE, classes->name, classes->pingfreq, classes->connfreq,
			classes->maxclients, classes->sendq, classes->recvq ? classes->recvq : DEFAULT_RECVQ);
#ifdef DEBUGMODE
		sendnotice(client, "class '%s' has clients=%d, xrefcount=%d",
			classes->name, classes->clients, classes->xrefcount);
#endif
	}
	return 0;
}

int stats_linkinfo(Client *client, char *para)
{
	return stats_linkinfoint(client, para, 0);
}

int stats_linkinfoall(Client *client, char *para)
{
	return stats_linkinfoint(client, para, 1);
}

int stats_linkinfoint(Client *client, char *para, int all)
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
	int showports = ValidatePermissionsForPath("server:info:stats",client,NULL,NULL,NULL);
	Client *acptr;
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
		else if (match_simple(para, me.name))
			doall = 1;
		if (strchr(para, '*') || strchr(para, '?'))
			wilds = 1;
	}
	else
		para = me.name;
	sendnumericfmt(client, RPL_STATSLINKINFO, "%s", Sformat);
	if (!MyUser(client))
	{
		remote = 1;
		wilds = 0;
	}

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsInvisible(acptr) && (doall || wilds) &&
			!IsOper(acptr) && (acptr != client))
			continue;
		if (remote && doall && !IsServer(acptr) && !IsMe(acptr))
			continue;
		if (remote && !doall && IsServer(acptr))
			continue;
		if (!doall && wilds && !match_simple(para, acptr->name))
			continue;
		if (!(para && (IsServer(acptr) || IsListening(acptr))) &&
		    !(doall || wilds) &&
		    mycmp(para, acptr->name))
		{
			continue;
		}

#ifdef DEBUGMODE
		ircsnprintf(pbuf, sizeof(pbuf), "%ld :%ld", (long)acptr->local->cputime,
		      (long)(acptr->user && MyConnect(acptr)) ? TStime() - acptr->local->last : 0);
#endif
		if (ValidatePermissionsForPath("server:info:stats",client,NULL,NULL,NULL))
		{
			sendnumericfmt(client, RPL_STATSLINKINFO, Lformat,
				all ?
				(get_client_name2(acptr, showports)) :
				(get_client_name(acptr, FALSE)),
				get_client_status(acptr),
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
			sendnumericfmt(client, RPL_STATSLINKINFO, Lformat,
				IsHidden(acptr) ? acptr->name :
				all ?	/* Potvin - PreZ */
				get_client_name2(acptr, showports) :
				get_client_name(acptr, FALSE),
				get_client_status(acptr),
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
			sendnotice(client, "Server %s is %s",
				acptr->name, acptr->serv->flags.synced ? "SYNCED" : "NOT SYNCED!!");
	}
#endif
	return 0;
}
