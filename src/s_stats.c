#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "version.h"
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <time.h>
#include "h.h"
#include "proto.h"
#include <string.h>

extern int  max_connection_count;
extern char modebuf[MAXMODEPARAMS*2+1], parabuf[504];
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
int stats_badwords(aClient *, char *);
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
int stats_zip(aClient *, char *);
int stats_officialchannels(aClient *, char *);

#define SERVER_AS_PARA 0x1
#define FLAGS_AS_PARA 0x2

struct statstab {
	char flag;
	char *longflag;
	int (*func)(aClient *sptr, char *para);
	int options;
};

//TODO:
// module help
// update docs
// update module docs 

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
	{ 'Q', "bannick",	stats_bannick,		0 		},
	{ 'R', "usage",		stats_usage,		0 		},
	{ 'S', "set",		stats_set,		0		},
	{ 'T', "traffic",	stats_traffic,		0 		},
	{ 'U', "uline",		stats_uline,		0 		},
	{ 'V', "vhost", 	stats_vhost,		0 		},
	{ 'X', "notlink",	stats_notlink,		0 		},	
	{ 'Y', "class",		stats_class,		0 		},	
	{ 'Z', "mem",		stats_mem,		0 		},
	{ 'b', "badword", 	stats_badwords,		0 		},
	{ 'c', "link", 		stats_links,		0 		},
	{ 'd', "denylinkauto",	stats_denylinkauto,	0 		},
	{ 'e', "exceptthrottle",stats_exceptthrottle,	0		},
	{ 'f', "denydcc",	stats_denydcc,		0		},	
	{ 'g', "gline",		stats_gline,		FLAGS_AS_PARA	},
	{ 'h', "link", 		stats_links,		0 		},
	{ 'j', "officialchans", stats_officialchannels, 0 },
	{ 'k', "kline",		stats_kline,		0 		},
	{ 'l', "linkinfo",	stats_linkinfo,		SERVER_AS_PARA 	},
	{ 'n', "banrealname",	stats_banrealname,	0 		},
	{ 'o', "oper",		stats_oper,		0 		},
	{ 'q', "sqline",	stats_sqline,		0 		},
	{ 'r', "chanrestrict",	stats_chanrestrict,	0 		},
	{ 's', "shun",		stats_shun,		FLAGS_AS_PARA	},
	{ 't', "tld",		stats_tld,		0 		},
	{ 'u', "uptime",	stats_uptime,		0 		},
	{ 'v', "denyver",	stats_denyver,		0 		},
	{ 'x', "notlink",	stats_notlink,		0 		},	
	{ 'y', "class",		stats_class,		0 		},
	{ 'z', "zip",		stats_zip,		0 		},
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

inline struct statstab *stats_binary_search(char c) {
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

inline struct statstab *stats_search(char *s) {
	int i;
	for (i = 0; StatsTable[i].flag; i++)
		if (!stats_compare(StatsTable[i].longflag,s))
			return &StatsTable[i];
	return NULL;
}

inline char *stats_combine_parv(char *p1, char *p2)
{
	static char buf[BUFSIZE+1];
	strcpy(buf, p1);
	strcat(buf, " ");
	strcat(buf, p2);
	return buf;
}

inline void stats_help(aClient *sptr)
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
		"e - excepthrottle - Send the except trottle block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"E - exceptban - Send the except ban block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"F - denydcc - Send the deny dcc block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"G - gline - Send the gline list");
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
		"P - port - Send information about ports");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"q - sqline - Send the SQLINE list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"Q - bannick - Send the ban nick block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"r - chanrestrict - Send the channel deny/allow block list");
#ifdef DEBUGMODE
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"R - usage - Send usage information");
#endif
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"t - tld - Send the tld block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"T - traffic - Send traffic information");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"u - uptime - Send the server uptime and connection count");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"U - uline - Send the ulines block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"v - Send the deny version block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"V - Send the vhost block list");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"X - notlink - Send the list of servers that are not current linked");
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"Y - class - Send the class block list");
#ifdef ZIP_LINKS
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"z - zip - Send compression information about ziplinked servers");
#endif
	sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, sptr->name,
		"Z - mem - Send memory usage information");
}

inline int stats_operonly_short(char c)
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
		l =='f' || l == 'i' || l == 'h')
	{
		if (islower(c) && strchr(OPER_ONLY_STATS, toupper(c)))
			return 1;
		else if (isupper(c) && strchr(OPER_ONLY_STATS, tolower(c)))
			return 1;
	}
	/* Hack for c/C/H/h */
	if (l == 'c')
		if (strpbrk(OPER_ONLY_STATS, "hH"))
			return 1;
	else if (l == 'h')
		if (strpbrk(OPER_ONLY_STATS, "cC"))
			return 1;
	return 0;
}

inline int stats_operonly_long(char *s)
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
inline char *stats_operonly_long_to_short()
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
		if (hunt_server_token(cptr, sptr, MSG_STATS, TOK_STATS, "%s :%s", 2, parc,
		    parv) != HUNTED_ISME)
			return 0;
	}
	else if (parc == 4 && parv[2][0] != '+' && parv[2][0] != '-')
	{
		if (hunt_server_token(cptr, sptr, MSG_STATS, TOK_STATS, "%s %s %s", 2, parc,
			parv) != HUNTED_ISME)
				return 0;
	}
	if (parc < 2 || !*parv[1])
	{
		stats_help(sptr);
		sendto_one(sptr, rpl_str(RPL_ENDOFSTATS), me.name, parv[0], '*');
		return 0;
	}

	/* Decide if we are looking for 1 char or a string */
	if (parv[1][0] && !parv[1][1])
	{
		if (!IsAnOper(sptr) && stats_operonly_short(parv[1][0]))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);	
			return 0;
		}	
		/* Old style, we can use a binary search here */
		stat = stats_binary_search(parv[1][0]);
	}
	else
	{
		if (!IsAnOper(sptr) && stats_operonly_long(parv[1]))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);	
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
			if (!IsAnOper(sptr) && stats_operonly_long(stat->longflag))
			{
				sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
				return 0;
			}
		}
		/* It was a long flag, so check oper only on short flags */
		else
		{
			if (!IsAnOper(sptr) && stats_operonly_short(stat->flag))
			{
				sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);	
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
		sendto_one(sptr, rpl_str(RPL_ENDOFSTATS), me.name, parv[0], stat->flag);
		sendto_snomask(SNO_EYES, "Stats \'%c\' requested by %s (%s@%s)",
			stat->flag, sptr->name, sptr->user->username, GetHost(sptr));
	}
	else
	{
		stats_help(sptr);
		sendto_one(sptr, rpl_str(RPL_ENDOFSTATS), me.name, parv[0], '*');
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
	for (link_p = conf_link; link_p; link_p = (ConfigItem_link *) link_p->next)
	{
		sendto_one(sptr, ":%s 213 %s C %s@%s * %s %i %s %s%s%s%s%s",
			me.name, sptr->name, IsOper(sptr) ? link_p->username : "*",
			IsOper(sptr) ? link_p->hostname : "*", link_p->servername,
			link_p->port,
			link_p->class->name,
			(link_p->options & CONNECT_AUTO) ? "a" : "",
			(link_p->options & CONNECT_SSL) ? "S" : "",
			(link_p->options & CONNECT_ZIP) ? "z" : "",
			(link_p->options & CONNECT_NODNSCACHE) ? "d" : "",
			(link_p->options & CONNECT_NOHOSTCHECK) ? "h" : "");
		if (link_p->hubmask)
			sendto_one(sptr, ":%s 244 %s H %s * %s",
				me.name, sptr->name, link_p->hubmask,
				link_p->servername);
		else if (link_p->leafmask)
			sendto_one(sptr, ":%s 241 %s L %s * %s %d",
				me.name, sptr->name,
				link_p->leafmask, link_p->servername, link_p->leafdepth);
	}
	return 0;
}

int stats_denylinkall(aClient *sptr, char *para)
{
	ConfigItem_deny_link *links;

	for (links = conf_deny_link; links; links = (ConfigItem_deny_link *) links->next) 
	{
		if (links->flag.type == CRULE_ALL)
			sendto_one(sptr, rpl_str(RPL_STATSDLINE), me.name, sptr->name,
			"D", links->mask, links->prettyrule);
	}
	return 0;
}

int stats_gline(aClient *sptr, char *para)
{
	tkl_stats(sptr, TKL_GLOBAL|TKL_KILL, para);
	tkl_stats(sptr, TKL_GLOBAL|TKL_ZAP, para);
	return 0;
}

int stats_exceptban(aClient *sptr, char *para)
{
	ConfigItem_except *excepts;
	for (excepts = conf_except; excepts; excepts = (ConfigItem_except *) excepts->next)
		if (excepts->flag.type == 1)
			sendto_one(sptr, rpl_str(RPL_STATSKLINE), me.name,
				sptr->name, "E", excepts->mask, "");
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
	for (i = 0; i < 256; i++)
		for (mptr = TokenHash[i]; mptr; mptr = mptr->next)
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
	ConfigItem_oper_from *from;
	for (oper_p = conf_oper; oper_p; oper_p = (ConfigItem_oper *) oper_p->next)
	{
		if(!oper_p->from)
			sendto_one(sptr, rpl_str(RPL_STATSOLINE),
		  		me.name, sptr->name, 
		  		'O', "(none)", oper_p->name,
		  		oflagstr(oper_p->oflags),
		  		oper_p->class->name ? oper_p->class->name : "");
		else
			for (from = (ConfigItem_oper_from *) oper_p->from; from; from = (ConfigItem_oper_from *) from->next)
		  		sendto_one(sptr, rpl_str(RPL_STATSOLINE),
		  			me.name, sptr->name, 
		  			'O', from->name, oper_p->name,
		  			oflagstr(oper_p->oflags),
		  			oper_p->class->name? oper_p->class->name : "");
	}
	return 0;
}

static char *stats_port_helper(aClient *listener)
{
static char buf[256];
	buf[0] = '\0';
	if (listener->umodes & LISTENER_CLIENTSONLY)
		strcat(buf, "clientsonly ");
	if (listener->umodes & LISTENER_SERVERSONLY)
		strcat(buf, "serversonly ");
	if (listener->umodes & LISTENER_JAVACLIENT)
		strcat(buf, "java ");
	if (listener->umodes & LISTENER_SSL)
		strcat(buf, "SSL ");
	return buf;
}

int stats_port(aClient *sptr, char *para)
{
	int i;
	aClient *acptr;
	for (i = 0; i <= LastSlot; i++)
	{
		if (!(acptr = local[i]))
			continue;
	  	if (!IsListening(acptr))
	  		continue;
	  	sendto_one(sptr, ":%s %s %s :*** Listener on %s:%i, clients %i. is %s %s",
	  		me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name,
	  		((ConfigItem_listen *)acptr->class)->ip,
			((ConfigItem_listen *)acptr->class)->port,
			((ConfigItem_listen *)acptr->class)->clients,
			((ConfigItem_listen *)acptr->class)->flag.temporary ? "TEMPORARY" : "PERM",
			stats_port_helper(acptr));
	}
	return 0;
}

int stats_bannick(aClient *sptr, char *para)
{
	ConfigItem_ban *bans;

	for (bans = conf_ban; bans; bans = (ConfigItem_ban *)bans->next) 
		if (bans->flag.type == CONF_BAN_NICK && (bans->flag.type2 != CONF_BAN_TYPE_AKILL))
			sendto_one(sptr, rpl_str(RPL_STATSQLINE),
				me.name, sptr->name,  bans->reason, bans->mask);
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
	for (i = 0; i <= LastSlot; i++)
	{
		if (!(acptr = local[i]))
			continue;
		if (IsServer(acptr))
		{
			sp->is_sbs += acptr->sendB;
			sp->is_sbr += acptr->receiveB;
			sp->is_sks += acptr->sendK;
			sp->is_skr += acptr->receiveK;
			sp->is_sti += now - acptr->firsttime;
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
			sp->is_cbs += acptr->sendB;
			sp->is_cbr += acptr->receiveB;
			sp->is_cks += acptr->sendK;
			sp->is_ckr += acptr->receiveK;
			sp->is_cti += now - acptr->firsttime;
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
#ifndef NO_FDLIST
	sendto_one(sptr,
	    ":%s %d %s :incoming rate %0.2f kb/s - outgoing rate %0.2f kb/s",
	    me.name, RPL_STATSDEBUG, sptr->name, currentrate, currentrate2);
#endif
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
	ConfigItem_oper_from *from;
	ConfigItem_vhost *vhosts;
	for(vhosts = conf_vhost; vhosts; vhosts = (ConfigItem_vhost *) vhosts->next) 
	{
		for (from = (ConfigItem_oper_from *)vhosts->from; from; from = (ConfigItem_oper_from *)from->next) 
			sendto_one(sptr, ":%s %i %s :vhost %s%s%s %s %s", me.name, RPL_TEXT, sptr->name,
			     vhosts->virtuser ? vhosts->virtuser : "", vhosts->virtuser ? "@" : "",
			     vhosts->virthost, vhosts->login, from->name);
	}
	return 0;
}

int stats_mem(aClient *sptr, char *para)
{
	extern aChannel *channel;
	extern int flinks;
	extern Link *freelink;
	extern MemoryInfo StatsZ;

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

	if (!IsAnOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	count_whowas_memory(&wwu, &wwam);
	count_watch_memory(&wlh, &wlhm);
	wwm = sizeof(aName) * NICKNAMEHISTORYLENGTH;

	for (acptr = client; acptr; acptr = acptr->next)
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
	}

	sendto_one(sptr, ":%s %d %s :Client Local %d(%ld) Remote %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, lc, lcm, rc, rcm);
	sendto_one(sptr, ":%s %d %s :Users %d(%d) Invites %d(%d)",
	    me.name, RPL_STATSDEBUG, sptr->name, us, us * sizeof(anUser), usi,
	    usi * sizeof(Link));
	sendto_one(sptr, ":%s %d %s :User channels %d(%d) Aways %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, usc, usc * sizeof(Link), aw, awm);
	sendto_one(sptr, ":%s %d %s :WATCH headers %d(%ld) entries %d(%d)",
	    me.name, RPL_STATSDEBUG, sptr->name, wlh, wlhm, wle, wle * sizeof(Link));
	sendto_one(sptr, ":%s %d %s :Attached confs %d(%d)",
	    me.name, RPL_STATSDEBUG, sptr->name, lcc, lcc * sizeof(Link));

	totcl = lcm + rcm + us * sizeof(anUser) + usc * sizeof(Link) + awm;
	totcl += lcc * sizeof(Link) + usi * sizeof(Link) + wlhm;
	totcl += wle * sizeof(Link);

	sendto_one(sptr, ":%s %d %s :Conflines %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, co, com);

	sendto_one(sptr, ":%s %d %s :Classes %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, StatsZ.classes, StatsZ.classesmem);

	sendto_one(sptr, ":%s %d %s :Channels %d(%ld) Bans %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, ch, chm, chb, chbm);
	sendto_one(sptr, ":%s %d %s :Channel members %d(%d) invite %d(%d)",
	    me.name, RPL_STATSDEBUG, sptr->name, chu, chu * sizeof(Link),
	    chi, chi * sizeof(Link));

	totch = chm + chbm + chu * sizeof(Link) + chi * sizeof(Link);

	sendto_one(sptr, ":%s %d %s :Whowas users %d(%d) away %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, wwu, wwu * sizeof(anUser),
	    wwa, wwam);
	sendto_one(sptr, ":%s %d %s :Whowas array %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, NICKNAMEHISTORYLENGTH, wwm);

	totww = wwu * sizeof(anUser) + wwam + wwm;

	sendto_one(sptr,
	    ":%s %d %s :Hash: client %d(%d) chan %d(%d) watch %d(%d)", me.name,
	    RPL_STATSDEBUG, sptr->name, U_MAX, sizeof(aHashEntry) * U_MAX, CH_MAX,
	    sizeof(aHashEntry) * CH_MAX, WATCHHASHSIZE,
	    sizeof(aWatch *) * WATCHHASHSIZE);
	db = dbufblocks * sizeof(dbufbuf);
	sendto_one(sptr, ":%s %d %s :Dbuf blocks %d(%ld)",
	    me.name, RPL_STATSDEBUG, sptr->name, dbufblocks, db);

	link = freelink;
	while ((link = link->next))
		fl++;
	fl++;
	sendto_one(sptr, ":%s %d %s :Link blocks free %d(%d) total %d(%d)",
	    me.name, RPL_STATSDEBUG, sptr->name, fl, fl * sizeof(Link),
	    flinks, flinks * sizeof(Link));

	rm = cres_mem(sptr,sptr->name);

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
	sendto_one(sptr, ":%s %d %s :TOTAL: %d",
	    me.name, RPL_STATSDEBUG, sptr->name, tot);
#endif
	return 0;
}

int stats_badwords(aClient *sptr, char *para)
{
#ifdef STRIPBADWORDS
	  ConfigItem_badword *words;

	  for (words = conf_badword_channel; words; words = (ConfigItem_badword *) words->next) {
 #ifdef FAST_BADWORD_REPLACE
		  sendto_one(sptr, ":%s %i %s :c %c %s%s%s %s",
		      me.name, RPL_TEXT, sptr->name, words->type & BADW_TYPE_REGEX ? 'R' : 'F',
		      (words->type & BADW_TYPE_FAST_L) ? "*" : "", words->word,
		      (words->type & BADW_TYPE_FAST_R) ? "*" : "",
		      words->action == BADWORD_REPLACE ? 
		      (words->replace ? words->replace : "<censored>") : "");
 #else
		  sendto_one(sptr, ":%s %i %s :c %s %s", me.name, RPL_TEXT,
			sptr->name,  words->word, words->action == BADWORD_REPLACE ? 
			(words->replace ? words->replace : "<censored>") : "");
 #endif
	  }
	  for (words = conf_badword_message; words; words = (ConfigItem_badword *) words->next) {
 #ifdef FAST_BADWORD_REPLACE
		  sendto_one(sptr, ":%s %i %s :m %c %s%s%s %s",
		      me.name, RPL_TEXT, sptr->name, words->type & BADW_TYPE_REGEX ? 'R' : 'F',
		      (words->type & BADW_TYPE_FAST_L) ? "*" : "", words->word,
		      (words->type & BADW_TYPE_FAST_R) ? "*" : "",
		      words->action == BADWORD_REPLACE ? 
		      (words->replace ? words->replace : "<censored>") : "");
 #else
		  sendto_one(sptr, ":%s %i %s :m %s %s", me.name, RPL_TEXT, sptr->name,
			words->word, words->action == BADWORD_REPLACE ? 
			(words->replace ? words->replace : "<censored>") : "");

 #endif
	  }
	  for (words = conf_badword_quit; words; words = (ConfigItem_badword *) words->next) {
 #ifdef FAST_BADWORD_REPLACE
		  sendto_one(sptr, ":%s %i %s :q %c %s%s%s %s",
		      me.name, RPL_TEXT, sptr->name, words->type & BADW_TYPE_REGEX ? 'R' : 'F',
		      (words->type & BADW_TYPE_FAST_L) ? "*" : "", words->word,
		      (words->type & BADW_TYPE_FAST_R) ? "*" : "",
		      words->action == BADWORD_REPLACE ? 
		      (words->replace ? words->replace : "<censored>") : "");
 #else
		  sendto_one(sptr, ":%s %i %s :q %s %s", me.name, RPL_TEXT, sptr->name,
			words->word, words->action == BADWORD_REPLACE ? 
			(words->replace ? words->replace : "<censored>") : "");

 #endif
	  }
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
			"d", links->mask, links->prettyrule);
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
	ConfigItem_deny_dcc *tmp;
	char *filemask, *reason;
	char a = 0;

	for (tmp = conf_deny_dcc; tmp; tmp = (ConfigItem_deny_dcc *) tmp->next)
	{
		filemask = BadPtr(tmp->filename) ? "<NULL>" : tmp->filename;
		reason = BadPtr(tmp->reason) ? "<NULL>" : tmp->reason;
		if (tmp->flag.type2 == CONF_BAN_TYPE_CONF)
			a = 'c';
		if (tmp->flag.type2 == CONF_BAN_TYPE_AKILL)
			a = 's';
		if (tmp->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
			a = 'o';
		sendto_one(sptr, ":%s %i %s :%c %s %s", me.name, RPL_TEXT,
		    sptr->name, a, filemask, reason);
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
	for (excepts = conf_except; excepts; excepts = (ConfigItem_except *)excepts->next) {
		if (excepts->flag.type == 1)
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
	ConfigItem_ban *bans;

	for (bans = conf_ban; bans; bans = (ConfigItem_ban *)bans->next) 
		if (bans->flag.type == CONF_BAN_NICK && (bans->flag.type2 == CONF_BAN_TYPE_AKILL))
			sendto_one(sptr, rpl_str(RPL_SQLINE_NICK),
				me.name, sptr->name, bans->mask, bans->reason ? bans->reason
				: "No Reason");
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

	if (!IsAnOper(sptr))
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
	sendto_one(sptr, ":%s %i %s :stats-server: %s", me.name, RPL_TEXT,
	    sptr->name, STATS_SERVER);
	sendto_one(sptr, ":%s %i %s :hiddenhost-prefix: %s", me.name, RPL_TEXT,
	    sptr->name, hidden_host);
	sendto_one(sptr, ":%s %i %s :help-channel: %s", me.name, RPL_TEXT,
	    sptr->name, helpchan);
	sendto_one(sptr, ":%s %i %s :cloak-keys: %lX", me.name, RPL_TEXT, sptr->name,
		CLOAK_KEYCRC);
	sendto_one(sptr, ":%s %i %s :kline-address: %s", me.name, RPL_TEXT,
	    sptr->name, KLINE_ADDRESS);
	sendto_one(sptr, ":%s %i %s :modes-on-connect: %s", me.name, RPL_TEXT,
	    sptr->name, get_modestr(CONN_MODES));
	sendto_one(sptr, ":%s %i %s :modes-on-oper: %s", me.name, RPL_TEXT,
	    sptr->name, get_modestr(OPER_MODES));
	*modebuf = *parabuf = 0;
	chmode_str(iConf.modes_on_join, modebuf, parabuf);
	sendto_one(sptr, ":%s %i %s :modes-on-join: %s %s", me.name, RPL_TEXT,
		sptr->name, modebuf, parabuf);
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
	switch (UHOST_ALLOWED)
	{
		case UHALLOW_ALWAYS:
			uhallow = "always";
			break;
		case UHALLOW_NEVER:
			uhallow = "never";
			break;
		case UHALLOW_NOCHANS:
			uhallow = "not-on-channels";
			break;
		case UHALLOW_REJOIN:
			uhallow = "force-rejoin";
			break;
	}
	sendto_one(sptr, ":%s %i %s :anti-spam-quit-message-time: %s", me.name, RPL_TEXT, 
		sptr->name, pretty_time_val(ANTI_SPAM_QUIT_MSG_TIME));
	sendto_one(sptr, ":%s %i %s :channel-command-prefix: %s", me.name, RPL_TEXT, sptr->name, CHANCMDPFX ? CHANCMDPFX : "`");
#ifdef USE_SSL
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
#endif

	sendto_one(sptr, ":%s %i %s :options::show-opermotd: %d", me.name, RPL_TEXT,
	    sptr->name, SHOWOPERMOTD);
	sendto_one(sptr, ":%s %i %s :options::hide-ulines: %d", me.name, RPL_TEXT,
	    sptr->name, HIDE_ULINES);
	sendto_one(sptr, ":%s %i %s :options::webtv-support: %d", me.name, RPL_TEXT,
	    sptr->name, WEBTV_SUPPORT);
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
	sendto_one(sptr, ":%s %i %s :who-limit: %d", me.name, RPL_TEXT,
		sptr->name, WHOLIMIT);
	sendto_one(sptr, ":%s %i %s :dns::timeout: %s", me.name, RPL_TEXT,
	    sptr->name, pretty_time_val(HOST_TIMEOUT));
	sendto_one(sptr, ":%s %i %s :dns::retries: %d", me.name, RPL_TEXT,
	    sptr->name, HOST_RETRIES);
	sendto_one(sptr, ":%s %i %s :dns::nameserver: %s", me.name, RPL_TEXT,
	    sptr->name, NAME_SERVER);
	sendto_one(sptr, ":%s %i %s :ban-version-tkl-time: %s", me.name, RPL_TEXT,
	    sptr->name, pretty_time_val(BAN_VERSION_TKL_TIME));
#ifdef THROTTLING
	sendto_one(sptr, ":%s %i %s :throttle::period: %s", me.name, RPL_TEXT,
			sptr->name, THROTTLING_PERIOD ? pretty_time_val(THROTTLING_PERIOD) : "disabled");
	sendto_one(sptr, ":%s %i %s :throttle::connections: %d", me.name, RPL_TEXT,
			sptr->name, THROTTLING_COUNT ? THROTTLING_COUNT : -1);
#endif
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
#ifdef NEWCHFLOODPROT
	sendto_one(sptr, ":%s %i %s :modef-default-unsettime: %hd", me.name, RPL_TEXT,
			sptr->name, (unsigned short)MODEF_DEFAULT_UNSETTIME);
	sendto_one(sptr, ":%s %i %s :modef-max-unsettime: %hd", me.name, RPL_TEXT,
			sptr->name, (unsigned short)MODEF_MAX_UNSETTIME);
#endif
	sendto_one(sptr, ":%s %i %s :hosts::global: %s", me.name, RPL_TEXT,
	    sptr->name, oper_host);
	sendto_one(sptr, ":%s %i %s :hosts::admin: %s", me.name, RPL_TEXT,
	    sptr->name, admin_host);
	sendto_one(sptr, ":%s %i %s :hosts::local: %s", me.name, RPL_TEXT,
	    sptr->name, locop_host);
	sendto_one(sptr, ":%s %i %s :hosts::servicesadmin: %s", me.name, RPL_TEXT,
	    sptr->name, sadmin_host);
	sendto_one(sptr, ":%s %i %s :hosts::netadmin: %s", me.name, RPL_TEXT,
	    sptr->name, netadmin_host);
	sendto_one(sptr, ":%s %i %s :hosts::coadmin: %s", me.name, RPL_TEXT,
	    sptr->name, coadmin_host);
	sendto_one(sptr, ":%s %i %s :hosts::host-on-oper-up: %i", me.name, RPL_TEXT, sptr->name,
	    iNAH);
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

	tmpnow = TStime() - me.since;
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
				link_p->port);
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
	}
	return 0;
}

int stats_zip(aClient *sptr, char *para)
{
#ifdef ZIP_LINKS
	int i;
	aClient *acptr;
	for (i=0; i <= LastSlot; i++)
	{
		if (!(acptr = local[i]))
			continue;
		if (!IsServer(acptr) || !IsZipped(acptr))
			continue;
		if (acptr->zip->in->total_out && acptr->zip->out->total_in)
		{
			sendto_one(sptr,
				":%s %i %s :Zipstats for link to %s (compresslevel %d): decompressed (in): %01lu/%01lu (%3.1f%%), compressed (out): %01lu/%01lu (%3.1f%%)",
				me.name, RPL_TEXT, sptr->name, get_client_name(acptr, TRUE),
				acptr->serv->conf->compression_level ? 
				acptr->serv->conf->compression_level : ZIP_DEFAULT_LEVEL,
				acptr->zip->in->total_in, acptr->zip->in->total_out,
				(100.0*(float)acptr->zip->in->total_in) /(float)acptr->zip->in->total_out,
				acptr->zip->out->total_in, acptr->zip->out->total_out,
				(100.0*(float)acptr->zip->out->total_out) /(float)acptr->zip->out->total_in);
		} 
		else 
			sendto_one(sptr, ":%s %i %s :Zipstats for link to %s: unavailable", 
				me.name, RPL_TEXT, sptr->name);
	}
#endif
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
	int showports = IsAnOper(sptr);
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
	for (i = 0; i <= LastSlot; i++)
	{
		if (!(acptr = local[i]))
			continue;
		if (IsInvisible(acptr) && (doall || wilds) &&
			!(MyConnect(sptr) && IsOper(sptr)) &&
			!IsAnOper(acptr) && (acptr != sptr))
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
		ircsprintf(pbuf, "%d :%d", acptr->cputime,
		      (acptr->user && MyConnect(acptr)) ?
		      TStime() - acptr->last : 0);
#endif
		if (IsOper(sptr))
		{
			sendto_one(sptr, Lformat, me.name,
				RPL_STATSLINKINFO, sptr->name, 
				all ?
				(get_client_name2(acptr, showports)) :
				(get_client_name(acptr, FALSE)),
				get_cptr_status(acptr),
				(int)DBufLength(&acptr->sendQ),
				(int)acptr->sendM, (int)acptr->sendK,
				(int)acptr->receiveM,
				(int)acptr->receiveK,
			 	TStime() - acptr->firsttime,
#ifndef DEBUGMODE
				(acptr->user && MyConnect(acptr)) ?
				TStime() - acptr->last : 0);
#else
				pbuf);
#endif
			if (!IsServer(acptr) && !IsMe(acptr) && IsAnOper(acptr) && sptr != acptr)
				sendto_one(acptr,
					":%s %s %s :*** %s did a /stats L on you! IP may have been shown",
					me.name, IsWebTV(acptr) ? "PRIVMSG" : "NOTICE", 
					acptr->name, sptr->name);
		}
		else if (!strchr(acptr->name, '.'))
			sendto_one(sptr, Lformat, me.name,
				RPL_STATSLINKINFO, sptr->name,
				IsHidden(acptr) ? acptr->name :
				all ?	/* Potvin - PreZ */
				get_client_name2(acptr, showports) :
				get_client_name(acptr, FALSE),
				get_cptr_status(acptr),
				(int)DBufLength(&acptr->sendQ),
				(int)acptr->sendM, (int)acptr->sendK,
				(int)acptr->receiveM,
				(int)acptr->receiveK,
				TStime() - acptr->firsttime,
#ifndef DEBUGMODE
				(acptr->user && MyConnect(acptr)) ?
				TStime() - acptr->last : 0);
#else
				pbuf);
#endif
	}
#ifdef DEBUGMODE
	for (acptr = client; acptr; acptr = acptr->next)
	{
		if (IsServer(acptr))
			sendto_one(sptr, ":%s NOTICE %s :Server %s is %s",
				me.name, sptr->name, acptr->name, acptr->serv->flags.synced ? "SYNCED" : "NOT SYNCED!!");
	}
#endif
	return 0;
}
