/*
 *   Unreal Internet Relay Chat Daemon, src/s_conf.c
 *   (C) 1998-2000 Chris Behrens & Fred Jacobs (comstud, moogle)
 *   (C) 2000-2002 Carsten V. Munk and the UnrealIRCd Team
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
#include "struct.h"
#include "url.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "channel.h"
#include "macros.h"
#include <fcntl.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/wait.h>
#else
#include <io.h>
#endif
#include <sys/stat.h>
#ifdef __hpux
#include "inet.h"
#endif
#if defined(PCS) || defined(AIX) || defined(SVR3)
#include <time.h>
#endif
#include <string.h>
#ifdef GLOBH
#include <glob.h>
#endif
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#include "h.h"
#include "inet.h"
#include "proto.h"
#ifdef _WIN32
#undef GLOBH
#endif
#include "badwords.h"

#define ircstrdup(x,y) do { if (x) MyFree(x); if (!y) x = NULL; else x = strdup(y); } while(0)
#define ircfree(x) do { if (x) MyFree(x); x = NULL; } while(0)

/* 
 * Some typedefs..
*/
typedef struct _confcommand ConfigCommand;
struct	_confcommand
{
	char	*name;
	int	(*conffunc)(ConfigFile *conf, ConfigEntry *ce);
	int 	(*testfunc)(ConfigFile *conf, ConfigEntry *ce);
};

typedef struct _conf_operflag OperFlag;
struct _conf_operflag
{
	long	flag;
	char	*name;
};


/* Config commands */

static int	_conf_admin		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_me		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_oper		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_class		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_drpass		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_ulines		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_include		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_tld		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_listen		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_allow		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_except		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_vhost		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_link		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_ban		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_set		(ConfigFile *conf, ConfigEntry *ce);
#ifdef STRIPBADWORDS
static int	_conf_badword		(ConfigFile *conf, ConfigEntry *ce);
#endif
static int	_conf_deny		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_deny_dcc		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_deny_link		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_deny_channel	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_deny_version	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_allow_channel	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_allow_dcc		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_loadmodule	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_log		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_alias		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_help		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_offchans		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_spamfilter	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_cgiirc	(ConfigFile *conf, ConfigEntry *ce);

/* 
 * Validation commands 
*/

static int	_test_admin		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_me		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_oper		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_class		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_drpass		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_ulines		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_include		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_tld		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_listen		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_allow		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_except		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_vhost		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_link		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_ban		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_set		(ConfigFile *conf, ConfigEntry *ce);
#ifdef STRIPBADWORDS
static int	_test_badword		(ConfigFile *conf, ConfigEntry *ce);
#endif
static int	_test_deny		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_allow_channel	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_allow_dcc		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_loadmodule	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_log		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_alias		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_help		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_offchans		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_spamfilter	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_cgiirc	(ConfigFile *conf, ConfigEntry *ce);
 
/* This MUST be alphabetized */
static ConfigCommand _ConfigCommands[] = {
	{ "admin", 		_conf_admin,		_test_admin 	},
	{ "alias",		_conf_alias,		_test_alias	},
	{ "allow",		_conf_allow,		_test_allow	},
#ifdef STRIPBADWORDS
	{ "badword",		_conf_badword,		_test_badword	},
#endif
	{ "ban", 		_conf_ban,		_test_ban	},
	{ "cgiirc", 	_conf_cgiirc,	_test_cgiirc	},
	{ "class", 		_conf_class,		_test_class	},
	{ "deny",		_conf_deny,		_test_deny	},
	{ "drpass",		_conf_drpass,		_test_drpass	},
	{ "except",		_conf_except,		_test_except	},
	{ "help",		_conf_help,		_test_help	},
	{ "include",		NULL,	  		_test_include	},
	{ "link", 		_conf_link,		_test_link	},
	{ "listen", 		_conf_listen,		_test_listen	},
	{ "loadmodule",		NULL,		 	_test_loadmodule},
	{ "log",		_conf_log,		_test_log	},
	{ "me", 		_conf_me,		_test_me	},
	{ "official-channels", 		_conf_offchans,		_test_offchans	},
	{ "oper", 		_conf_oper,		_test_oper	},
	{ "set",		_conf_set,		_test_set	},
	{ "spamfilter",	_conf_spamfilter,	_test_spamfilter	},
	{ "tld",		_conf_tld,		_test_tld	},
	{ "ulines",		_conf_ulines,		_test_ulines	},
	{ "vhost", 		_conf_vhost,		_test_vhost	},
};

static int _OldOperFlags[] = {
	OFLAG_LOCAL, 'o',
	OFLAG_GLOBAL, 'O',
	OFLAG_REHASH, 'r',
	OFLAG_DIE, 'D',
	OFLAG_RESTART, 'R',
	OFLAG_HELPOP, 'h',
	OFLAG_GLOBOP, 'g',
	OFLAG_WALLOP, 'w',
	OFLAG_LOCOP, 'l',
	OFLAG_LROUTE, 'c',
	OFLAG_GROUTE, 'L',
	OFLAG_LKILL, 'k',
	OFLAG_GKILL, 'K',
	OFLAG_KLINE, 'b',
	OFLAG_UNKLINE, 'B',
	OFLAG_LNOTICE, 'n',
	OFLAG_GNOTICE, 'G',
	OFLAG_ADMIN_, 'A',
	OFLAG_SADMIN_, 'a',
	OFLAG_NADMIN, 'N',
	OFLAG_COADMIN, 'C',
	OFLAG_ZLINE, 'z',
	OFLAG_WHOIS, 'W',
	OFLAG_HIDE, 'H',
	OFLAG_TKL, 't',
	OFLAG_GZL, 'Z',
	OFLAG_OVERRIDE, 'v',
	OFLAG_UMODEQ, 'q',
	OFLAG_DCCDENY, 'd',
	OFLAG_ADDLINE, 'X',
	0, 0
};

/* This MUST be alphabetized */
static OperFlag _OperFlags[] = {
	{ OFLAG_ADMIN_,		"admin"},
	{ OFLAG_ADDLINE,	"can_addline"},
	{ OFLAG_DCCDENY,	"can_dccdeny"},
	{ OFLAG_DIE,		"can_die" },
	{ OFLAG_TKL,		"can_gkline"},
	{ OFLAG_GKILL,		"can_globalkill" },
	{ OFLAG_GNOTICE,	"can_globalnotice" },
	{ OFLAG_GROUTE,		"can_globalroute" },
	{ OFLAG_GLOBOP,         "can_globops" },
	{ OFLAG_GZL,		"can_gzline"},
	{ OFLAG_KLINE,		"can_kline" },
	{ OFLAG_LKILL,		"can_localkill" },
	{ OFLAG_LNOTICE,	"can_localnotice" },
	{ OFLAG_LROUTE,		"can_localroute" },
	{ OFLAG_OVERRIDE,	"can_override" },
	{ OFLAG_REHASH,		"can_rehash" },
	{ OFLAG_RESTART,        "can_restart" },
	{ OFLAG_UMODEQ,		"can_setq" },
	{ OFLAG_UNKLINE,	"can_unkline" },
	{ OFLAG_WALLOP,         "can_wallops" },
	{ OFLAG_ZLINE,		"can_zline"},
	{ OFLAG_COADMIN_,	"coadmin"},
	{ OFLAG_HIDE,		"get_host"},
	{ OFLAG_WHOIS,		"get_umodew"},
	{ OFLAG_GLOBAL,		"global" },
	{ OFLAG_HELPOP,         "helpop" },
	{ OFLAG_LOCAL,		"local" },
	{ OFLAG_LOCOP,		"locop"},
	{ OFLAG_NADMIN,		"netadmin"},
	{ OFLAG_SADMIN_,	"services-admin"},
};

/* This MUST be alphabetized */
static OperFlag _ListenerFlags[] = {
	{ LISTENER_CLIENTSONLY, "clientsonly"},
	{ LISTENER_JAVACLIENT, 	"java"},
	{ LISTENER_MASK, 	"mask"},
	{ LISTENER_REMOTEADMIN, "remoteadmin"},
	{ LISTENER_SERVERSONLY, "serversonly"},
	{ LISTENER_SSL, 	"ssl"},
	{ LISTENER_NORMAL, 	"standard"},
};

/* This MUST be alphabetized */
static OperFlag _LinkFlags[] = {
	{ CONNECT_AUTO,	"autoconnect" },
	{ CONNECT_NODNSCACHE, "nodnscache" },
	{ CONNECT_NOHOSTCHECK, "nohostcheck" },
	{ CONNECT_QUARANTINE, "quarantine"},
	{ CONNECT_SSL,	"ssl"		  },
	{ CONNECT_ZIP,	"zip"		  },
};

/* This MUST be alphabetized */
static OperFlag _LogFlags[] = {
	{ LOG_CHGCMDS, "chg-commands" },
	{ LOG_CLIENT, "connects" },
	{ LOG_ERROR, "errors" },
	{ LOG_KILL, "kills" },
	{ LOG_KLINE, "kline" },
	{ LOG_OPER, "oper" },
	{ LOG_OVERRIDE, "oper-override" },
	{ LOG_SACMDS, "sadmin-commands" },
	{ LOG_SERVER, "server-connects" },
	{ LOG_SPAMFILTER, "spamfilter" },
	{ LOG_TKL, "tkl" },
};

/* This MUST be alphabetized */
static OperFlag ExceptTklFlags[] = {
	{ TKL_GLOBAL|TKL_KILL,	"gline" },
	{ TKL_GLOBAL|TKL_NICK,	"gqline" },
	{ TKL_GLOBAL|TKL_ZAP,	"gzline" },
	{ TKL_NICK,		"qline" },
	{ TKL_GLOBAL|TKL_SHUN,	"shun" }
};

#ifdef USE_SSL
/* This MUST be alphabetized */
static OperFlag _SSLFlags[] = {
	{ SSLFLAG_FAILIFNOCERT, "fail-if-no-clientcert" },
	{ SSLFLAG_DONOTACCEPTSELFSIGNED, "no-self-signed" },
	{ SSLFLAG_VERIFYCERT, "verify-certificate" },
};
#endif

struct {
	unsigned conf_me : 1;
	unsigned conf_admin : 1;
	unsigned conf_listen : 1;
} requiredstuff;
struct SetCheck settings;
/*
 * Utilities
*/

void	ipport_seperate(char *string, char **ip, char **port);
void	port_range(char *string, int *start, int *end);
long	config_checkval(char *value, unsigned short flags);

/*
 * Parser
*/

ConfigFile		*config_load(char *filename);
void			config_free(ConfigFile *cfptr);
static ConfigFile 	*config_parse(char *filename, char *confdata);
static void 		config_entry_free(ConfigEntry *ceptr);
ConfigEntry		*config_find_entry(ConfigEntry *ce, char *name);
/*
 * Error handling
*/

void			config_warn(char *format, ...);
void 			config_error(char *format, ...);
void 			config_status(char *format, ...);
void 			config_progress(char *format, ...);

#ifdef _WIN32
extern void 	win_log(char *format, ...);
extern void		win_error();
#endif
extern void add_entropy_configfile(struct stat st, char *buf);
extern void unload_all_unused_snomasks();
extern void unload_all_unused_umodes();
extern void unload_all_unused_extcmodes(void);

extern int charsys_test_language(char *name);
extern void charsys_add_language(char *name);
extern void charsys_reset_pretest(void);
int charsys_postconftest(void);
void charsys_finish(void);
void delete_cgiircblock(ConfigItem_cgiirc *e);

/*
 * Config parser (IRCd)
*/
int			init_conf(char *rootconf, int rehash);
int			load_conf(char *filename);
void			config_rehash();
int			config_run();
/*
 * Configuration linked lists
*/
ConfigItem_me		*conf_me = NULL;
ConfigItem_class 	*conf_class = NULL;
ConfigItem_class	*default_class = NULL;
ConfigItem_admin 	*conf_admin = NULL;
ConfigItem_admin	*conf_admin_tail = NULL;
ConfigItem_drpass	*conf_drpass = NULL;
ConfigItem_ulines	*conf_ulines = NULL;
ConfigItem_tld		*conf_tld = NULL;
ConfigItem_oper		*conf_oper = NULL;
ConfigItem_listen	*conf_listen = NULL;
ConfigItem_allow	*conf_allow = NULL;
ConfigItem_except	*conf_except = NULL;
ConfigItem_vhost	*conf_vhost = NULL;
ConfigItem_link		*conf_link = NULL;
ConfigItem_cgiirc	*conf_cgiirc = NULL;
ConfigItem_ban		*conf_ban = NULL;
ConfigItem_deny_dcc     *conf_deny_dcc = NULL;
ConfigItem_deny_channel *conf_deny_channel = NULL;
ConfigItem_allow_channel *conf_allow_channel = NULL;
ConfigItem_allow_dcc    *conf_allow_dcc = NULL;
ConfigItem_deny_link	*conf_deny_link = NULL;
ConfigItem_deny_version *conf_deny_version = NULL;
ConfigItem_log		*conf_log = NULL;
ConfigItem_alias	*conf_alias = NULL;
ConfigItem_include	*conf_include = NULL;
ConfigItem_help		*conf_help = NULL;
#ifdef STRIPBADWORDS
ConfigItem_badword	*conf_badword_channel = NULL;
ConfigItem_badword      *conf_badword_message = NULL;
ConfigItem_badword	*conf_badword_quit = NULL;
#endif
ConfigItem_offchans	*conf_offchans = NULL;

aConfiguration		iConf;
MODVAR aConfiguration		tempiConf;
MODVAR ConfigFile		*conf = NULL;

MODVAR int			config_error_flag = 0;
int			config_verbose = 0;

void add_include(char *);
#ifdef USE_LIBCURL
void add_remote_include(char *, char *, int, char *);
int remote_include(ConfigEntry *ce);
#endif
void unload_notloaded_includes(void);
void load_includes(void);
void unload_loaded_includes(void);
int rehash_internal(aClient *cptr, aClient *sptr, int sig);


/* Pick out the ip address and the port number from a string.
 * The string syntax is:  ip:port.  ip must be enclosed in brackets ([]) if its an ipv6
 * address because they contain colon (:) separators.  The ip part is optional.  If the string
 * contains a single number its assumed to be a port number.
 *
 * Returns with ip pointing to the ip address (if one was specified), a "*" (if only a port 
 * was specified), or an empty string if there was an error.  port is returned pointing to the 
 * port number if one was specified, otherwise it points to a empty string.
 */
void ipport_seperate(char *string, char **ip, char **port)
{
	char *f;
	
	/* assume failure */
	*ip = *port = "";

	/* sanity check */
	if (string && strlen(string) > 0)
	{
		/* handle ipv6 type of ip address */
		if (*string == '[')
		{
			if ((f = strrchr(string, ']')))
			{
				*ip = string + 1;	/* skip [ */
				*f = '\0';			/* terminate the ip string */
				/* next char must be a : if a port was specified */
				if (*++f == ':')
				{
					*port = ++f;
				}
			}
		}
		/* handle ipv4 and port */
		else if ((f = strchr(string, ':')))
		{
			/* we found a colon... we may have ip:port or just :port */
			if (f == string)
			{
				/* we have just :port */
				*ip = "*";
			}
			else
			{
				/* we have ip:port */
				*ip = string;
				*f = '\0';
			}
			*port = ++f;
		}
		/* no ip was specified, just a port number */
		else if (!strcmp(string, my_itoa(atoi(string))))
		{
			*ip = "*";
			*port = string;
		}
	}
}

void port_range(char *string, int *start, int *end)
{
	char *c = strchr(string, '-');
	if (!c)
	{
		int tmp = atoi(string);
		*start = tmp;
		*end = tmp;
		return;
	}
	*c = '\0';
	*start = atoi(string);
	*end = atoi((c+1));
}

/** Parses '5:60s' config values.
 * orig: original string
 * times: pointer to int, first value (before the :)
 * period: pointer to int, second value (after the :) in seconds
 * RETURNS: 0 for parse error, 1 if ok.
 * REMARK: times&period should be ints!
 */
int config_parse_flood(char *orig, int *times, int *period)
{
char *x;

	*times = *period = 0;
	x = strchr(orig, ':');
	/* 'blah', ':blah', '1:' */
	if (!x || (x == orig) || (*(x+1) == '\0'))
		return 0;

	*x = '\0';
	*times = atoi(orig);
	*period = config_checkval(x+1, CFG_TIME);
	*x = ':'; /* restore */
	return 1;
}

long config_checkval(char *orig, unsigned short flags) {
	char *value;
	char *text;
	long ret = 0;

	value = strdup(orig);

	if (flags == CFG_YESNO) {
		for (text = value; *text; text++) {
			if (!isalnum(*text))
				continue;
			if (tolower(*text) == 'y' || (tolower(*text) == 'o' &&
			    tolower(*(text+1)) == 'n') || *text == '1' || tolower(*text) == 't') {
				ret = 1;
				break;
			}
		}
	}
	else if (flags == CFG_SIZE) {
		int mfactor = 1;
		char *sz;
		for (text = value; *text; text++) {
			if (isalpha(*text)) {
				if (tolower(*text) == 'k') 
					mfactor = 1024;
				else if (tolower(*text) == 'm') 
					mfactor = 1048576;
				else if (tolower(*text) == 'g') 
					mfactor = 1073741824;
				else 
					mfactor = 1;
				sz = text;
				while (isalpha(*text))
					text++;

				*sz-- = 0;
				while (sz-- > value && *sz) {
					if (isspace(*sz)) 
						*sz = 0;
					if (!isdigit(*sz)) 
						break;
				}
				ret += atoi(sz+1)*mfactor;
				if (*text == '\0') {
					text++;
					break;
				}
			}
		}
		mfactor = 1;
		sz = text;
		sz--;
		while (sz-- > value) {
			if (isspace(*sz)) 
				*sz = 0;
			if (!isdigit(*sz)) 
				break;
		}
		ret += atoi(sz+1)*mfactor;
	}
	else if (flags == CFG_TIME) {
		int mfactor = 1;
		char *sz;
		for (text = value; *text; text++) {
			if (isalpha(*text)) {
				if (tolower(*text) == 'w')
					mfactor = 604800;	
				else if (tolower(*text) == 'd') 
					mfactor = 86400;
				else if (tolower(*text) == 'h') 
					mfactor = 3600;
				else if (tolower(*text) == 'm') 
					mfactor = 60;
				else 
					mfactor = 1;
				sz = text;
				while (isalpha(*text))
					text++;

				*sz-- = 0;
				while (sz-- > value && *sz) {
					if (isspace(*sz)) 
						*sz = 0;
					if (!isdigit(*sz)) 
						break;
				}
				ret += atoi(sz+1)*mfactor;
				if (*text == '\0') {
					text++;
					break;
				}
			}
		}
		mfactor = 1;
		sz = text;
		sz--;
		while (sz-- > value) {
			if (isspace(*sz)) 
				*sz = 0;
			if (!isdigit(*sz)) 
				break;
		}
		ret += atoi(sz+1)*mfactor;
	}
	free(value);
	return ret;
}

int iplist_onlist(IPList *iplist, char *ip)
{
IPList *e;

	for (e = iplist; e; e = e->next)
		if (!match(e->mask, ip))
			return 1;
	return 0;
}

void set_channelmodes(char *modes, struct ChMode *store, int warn)
{
	aCtab *tab;
	char *params = strchr(modes, ' ');
	char *parambuf = NULL;
	char *param = NULL;
	char *save = NULL;
	
	warn = 0; // warn is broken
	
	if (params)
	{
		params++;
		parambuf = MyMalloc(strlen(params)+1);
		strcpy(parambuf, params);
		param = strtoken(&save, parambuf, " ");
	}		

	for (; *modes && *modes != ' '; modes++)
	{
		if (*modes == '+')
			continue;
		if (*modes == '-')
		/* When a channel is created it has no modes, so just ignore if the
		 * user asks us to unset anything -- codemastr 
		 */
		{
			while (*modes && *modes != '+')
				modes++;
			continue;
		}
		switch (*modes)
		{
			case 'f':
			{
#ifdef NEWCHFLOODPROT
				char *myparam = param;

				ChanFloodProt newf;
				
				memset(&newf, 0, sizeof(newf));
				if (!myparam)
					break;
				/* Go to next parameter */
				param = strtoken(&save, NULL, " ");

				if (myparam[0] != '[')
				{
					if (warn)
						config_status("set::modes-on-join: please use the new +f format: '10:5' becomes '[10t]:5' "
					                  "and '*10:5' becomes '[10t#b]:5'.");
				} else
				{
					char xbuf[256], c, a, *p, *p2, *x = xbuf+1;
					int v;
					unsigned short breakit;
					unsigned char r;
					
					/* '['<number><1 letter>[optional: '#'+1 letter],[next..]']'':'<number> */
					strlcpy(xbuf, myparam, sizeof(xbuf));
					p2 = strchr(xbuf+1, ']');
					if (!p2)
						break;
					*p2 = '\0';
					if (*(p2+1) != ':')
						break;
					breakit = 0;
					for (x = strtok(xbuf+1, ","); x; x = strtok(NULL, ","))
					{
						/* <number><1 letter>[optional: '#'+1 letter] */
						p = x;
						while(isdigit(*p)) { p++; }
						if ((*p == '\0') ||
						    !((*p == 'c') || (*p == 'j') || (*p == 'k') ||
						    (*p == 'm') || (*p == 'n') || (*p == 't')))
							break;
						c = *p;
						*p = '\0';
						v = atoi(x);
						if ((v < 1) || (v > 999)) /* out of range... */
							break;
						p++;
						a = '\0';
						r = 0;
						if (*p != '\0')
						{
							if (*p == '#')
							{
								p++;
								a = *p;
								p++;
								if (*p != '\0')
								{
									int tv;
									tv = atoi(p);
									if (tv <= 0)
										tv = 0; /* (ignored) */
									if (tv > 255)
										tv = 255; /* set to max */
									r = tv;
								}
							}
						}

						switch(c)
						{
							case 'c':
								newf.l[FLD_CTCP] = v;
								if ((a == 'm') || (a == 'M'))
									newf.a[FLD_CTCP] = a;
								else
									newf.a[FLD_CTCP] = 'C';
								newf.r[FLD_CTCP] = r;
								break;
							case 'j':
								newf.l[FLD_JOIN] = v;
								if (a == 'R')
									newf.a[FLD_JOIN] = a;
								else
									newf.a[FLD_JOIN] = 'i';
								newf.r[FLD_JOIN] = r;
								break;
							case 'k':
								newf.l[FLD_KNOCK] = v;
								newf.a[FLD_KNOCK] = 'K';
								newf.r[FLD_KNOCK] = r;
								break;
							case 'm':
								newf.l[FLD_MSG] = v;
								if (a == 'M')
									newf.a[FLD_MSG] = a;
								else
									newf.a[FLD_MSG] = 'm';
								newf.r[FLD_MSG] = r;
								break;
							case 'n':
								newf.l[FLD_NICK] = v;
								newf.a[FLD_NICK] = 'N';
								newf.r[FLD_NICK] = r;
								break;
							case 't':
								newf.l[FLD_TEXT] = v;
								if (a == 'b')
									newf.a[FLD_TEXT] = 'b';
								/** newf.r[FLD_TEXT] ** not supported */
								break;
							default:
								breakit=1;
								break;
						}
						if (breakit)
							break;
					} /* for strtok.. */
					if (breakit)
						break;
					/* parse 'per' */
					p2++;
					if (*p2 != ':')
						break;
					p2++;
					if (!*p2)
						break;
					v = atoi(p2);
					if ((v < 1) || (v > 999)) /* 'per' out of range */
						break;
					newf.per = v;
					/* Is anything turned on? (to stop things like '+f []:15' */
					breakit = 1;
					for (v=0; v < NUMFLD; v++)
						if (newf.l[v])
							breakit=0;
					if (breakit)
						break;
					
					/* w00t, we passed... */
					memcpy(&store->floodprot, &newf, sizeof(newf));
					store->mode |= MODE_FLOODLIMIT;
					break;
				}
#else
				char *myparam = param;
				char kmode = 0;
				char *xp;
				int msgs=0, per=0;
				int hascolon = 0;
				if (!myparam)
					break;
				/* Go to next parameter */
				param = strtoken(&save, NULL, " ");

				if (*myparam == '*')
					kmode = 1;
				for (xp = myparam; *xp; xp++)
				{
					if (*xp == ':')
					{
						hascolon++;
						continue;
					}
					if (((*xp < '0') || (*xp > '9')) && *xp != '*')
						break;
					if (*xp == '*' && *myparam != '*')
						break;
				}
				if (hascolon != 1)
					break;
				xp = strchr(myparam, ':');
					*xp = 0;
				msgs = atoi((*myparam == '*') ? (myparam+1) : myparam);
				xp++;
				per = atoi(xp);
				xp--;
				*xp = ':';
				if (msgs == 0 || msgs > 500 || per == 0 || per > 500)
					break;
				store->msgs = msgs;
				store->per = per;
				store->kmode = kmode; 					     
				store->mode |= MODE_FLOODLIMIT;
#endif
				break;
			}
			default:
				for (tab = &cFlagTab[0]; tab->mode; tab++)
				{
					if (tab->flag == *modes)
					{
						if (tab->parameters)
						{
							/* INCOMPATIBLE */
							break;
						}
						store->mode |= tab->mode;
						break;
					}
				}
#ifdef EXTCMODE
				/* Try extcmodes */
				if (!tab->mode)
				{
					int i;
					for (i=0; i <= Channelmode_highest; i++)
					{
						if (!(Channelmode_Table[i].flag))
							continue;
						if (*modes == Channelmode_Table[i].flag)
						{
							if (Channelmode_Table[i].paracount)
							{
								if (!param)
									break;
								param = Channelmode_Table[i].conv_param(param);
								if (!param)
									break; /* invalid parameter fmt, do not set mode. */
								store->extparams[i] = strdup(param);
								/* Get next parameter */
								param = strtoken(&save, NULL, " ");
							}
							store->extmodes |= Channelmode_Table[i].mode;
							break;
						}
					}
				}
#endif
		}
	}
	if (parambuf)
		free(parambuf);
}

void chmode_str(struct ChMode modes, char *mbuf, char *pbuf)
{
	aCtab *tab;
	int i;
	*pbuf = 0;
	*mbuf++ = '+';
	for (tab = &cFlagTab[0]; tab->mode; tab++)
	{
		if (modes.mode & tab->mode)
		{
			if (!tab->parameters)
				*mbuf++ = tab->flag;
		}
	}
#ifdef EXTCMODE
	for (i=0; i <= Channelmode_highest; i++)
	{
		if (!(Channelmode_Table[i].flag))
			continue;
	
		if (modes.extmodes & Channelmode_Table[i].mode)
		{
			*mbuf++ = Channelmode_Table[i].flag;
			if (Channelmode_Table[i].paracount)
			{
				strcat(pbuf, modes.extparams[i]);
				strcat(pbuf, " ");
			}
		}
	}
#endif
#ifdef NEWCHFLOODPROT
	if (modes.floodprot.per)
	{
		*mbuf++ = 'f';
		strcat(pbuf, channel_modef_string(&modes.floodprot));
	}
#else
	if (modes.per)
	{
		*mbuf++ = 'f';
		if (modes.kmode)
			strcat(pbuf, "*");
		strcat(pbuf, my_itoa(modes.msgs));
		strcat(pbuf, ":");
		strcat(pbuf, my_itoa(modes.per));
	}
#endif
	*mbuf++=0;
}

int channellevel_to_int(char *s)
{
	if (!strcmp(s, "none"))
		return CHFL_DEOPPED;
	if (!strcmp(s, "op") || !strcmp(s, "chanop"))
		return CHFL_CHANOP;
	return 0; /* unknown or unsupported */
}

ConfigFile *config_load(char *filename)
{
	struct stat sb;
	int			fd;
	int			ret;
	char		*buf = NULL;
	ConfigFile	*cfptr;

#ifndef _WIN32
	fd = open(filename, O_RDONLY);
#else
	fd = open(filename, O_RDONLY|O_BINARY);
#endif
	if (fd == -1)
	{
		config_error("Couldn't open \"%s\": %s\n", filename, strerror(errno));
		return NULL;
	}
	if (fstat(fd, &sb) == -1)
	{
		config_error("Couldn't fstat \"%s\": %s\n", filename, strerror(errno));
		close(fd);
		return NULL;
	}
	if (!sb.st_size)
	{
		/* Workaround for empty files */
		cfptr = config_parse(filename, " ");
		return cfptr;
	}
	buf = MyMalloc(sb.st_size+1);
	if (buf == NULL)
	{
		config_error("Out of memory trying to load \"%s\"\n", filename);
		close(fd);
		return NULL;
	}
	ret = read(fd, buf, sb.st_size);
	if (ret != sb.st_size)
	{
		config_error("Error reading \"%s\": %s\n", filename,
			ret == -1 ? strerror(errno) : strerror(EFAULT));
		free(buf);
		close(fd);
		return NULL;
	}
	/* Just me or could this cause memory corrupted when ret <0 ? */
	buf[ret] = '\0';
	close(fd);
	add_entropy_configfile(sb, buf);
	cfptr = config_parse(filename, buf);
	free(buf);
	return cfptr;
}

void config_free(ConfigFile *cfptr)
{
	ConfigFile	*nptr;

	for(;cfptr;cfptr=nptr)
	{
		nptr = cfptr->cf_next;
		if (cfptr->cf_entries)
			config_entry_free(cfptr->cf_entries);
		if (cfptr->cf_filename)
			free(cfptr->cf_filename);
		free(cfptr);
	}
}

/* This is the internal parser, made by Chris Behrens & Fred Jacobs */
static ConfigFile *config_parse(char *filename, char *confdata)
{
	char		*ptr;
	char		*start;
	int		linenumber = 1;
	ConfigEntry	*curce;
	ConfigEntry	**lastce;
	ConfigEntry	*cursection;

	ConfigFile	*curcf;
	ConfigFile	*lastcf;

	lastcf = curcf = MyMalloc(sizeof(ConfigFile));
	memset(curcf, 0, sizeof(ConfigFile));
	curcf->cf_filename = strdup(filename);
	lastce = &(curcf->cf_entries);
	curce = NULL;
	cursection = NULL;
	/* Replace \r's with spaces .. ugly ugly -Stskeeps */
	for (ptr=confdata; *ptr; ptr++)
		if (*ptr == '\r')
			*ptr = ' ';

	for(ptr=confdata;*ptr;ptr++)
	{
		switch(*ptr)
		{
			case ';':
				if (!curce)
				{
					config_status("%s:%i Ignoring extra semicolon\n",
						filename, linenumber);
					break;
				}
				*lastce = curce;
				lastce = &(curce->ce_next);
				curce->ce_fileposend = (ptr - confdata);
				curce = NULL;
				break;
			case '{':
				if (!curce)
				{
					config_status("%s:%i: No name for section start\n",
							filename, linenumber);
					continue;
				}
				else if (curce->ce_entries)
				{
					config_status("%s:%i: Ignoring extra section start\n",
							filename, linenumber);
					continue;
				}
				curce->ce_sectlinenum = linenumber;
				lastce = &(curce->ce_entries);
				cursection = curce;
				curce = NULL;
				break;
			case '}':
				if (curce)
				{
					config_error("%s:%i: Missing semicolon before close brace\n",
						filename, linenumber);
					config_entry_free(curce);
					config_free(curcf);

					return NULL;
				}
				else if (!cursection)
				{
					config_status("%s:%i: Ignoring extra close brace\n",
						filename, linenumber);
					continue;
				}
				curce = cursection;
				cursection->ce_fileposend = (ptr - confdata);
				cursection = cursection->ce_prevlevel;
				if (!cursection)
					lastce = &(curcf->cf_entries);
				else
					lastce = &(cursection->ce_entries);
				for(;*lastce;lastce = &((*lastce)->ce_next))
					continue;
				break;
			case '#':
				ptr++;
				while(*ptr && (*ptr != '\n'))
					 ptr++;
				if (!*ptr)
					break;
				ptr--;
				continue;
			case '/':
				if (*(ptr+1) == '/')
				{
					ptr += 2;
					while(*ptr && (*ptr != '\n'))
						ptr++;
					if (!*ptr)
						break;
					ptr--; /* grab the \n on next loop thru */
					continue;
				}
				else if (*(ptr+1) == '*')
				{
					int commentstart = linenumber;
					int commentlevel = 1;

					for(ptr+=2;*ptr;ptr++)
					{
						if ((*ptr == '/') && (*(ptr+1) == '*'))
						{
							commentlevel++;
							ptr++;
						}

						else if ((*ptr == '*') && (*(ptr+1) == '/'))
						{
							commentlevel--;
							ptr++;
						}

						else if (*ptr == '\n')
							linenumber++;

						if (!commentlevel)
							break;
					}
					if (!*ptr)
					{
						config_error("%s:%i Comment on this line does not end\n",
							filename, commentstart);
						config_entry_free(curce);
						config_free(curcf);
						return NULL;
					}
				}
				break;
			case '\"':
				if (curce && curce->ce_varlinenum != linenumber && cursection)
				{
					config_warn("%s:%i: Missing semicolon at end of line\n",
						filename, curce->ce_varlinenum);
					
					*lastce = curce;
					lastce = &(curce->ce_next);
					curce->ce_fileposend = (ptr - confdata);
					curce = NULL;
				}

				start = ++ptr;
				for(;*ptr;ptr++)
				{
					if ((*ptr == '\\'))
					{
					
						if (*(ptr+1) == '\\' || *(ptr+1) == '\"')
						{
							char *tptr = ptr;
							while((*tptr = *(tptr+1)))
								tptr++;
						}
					}
					else if ((*ptr == '\"') || (*ptr == '\n'))
						break;
				}
				if (!*ptr || (*ptr == '\n'))
				{
					config_error("%s:%i: Unterminated quote found\n",
							filename, linenumber);
					config_entry_free(curce);
					config_free(curcf);
					return NULL;
				}
				if (curce)
				{
					if (curce->ce_vardata)
					{
						config_status("%s:%i: Ignoring extra data\n",
							filename, linenumber);
					}
					else
					{
						curce->ce_vardata = MyMalloc(ptr-start+1);
						strncpy(curce->ce_vardata, start, ptr-start);
						curce->ce_vardata[ptr-start] = '\0';
					}
				}
				else
				{
					curce = MyMalloc(sizeof(ConfigEntry));
					memset(curce, 0, sizeof(ConfigEntry));
					curce->ce_varname = MyMalloc((ptr-start)+1);
					strncpy(curce->ce_varname, start, ptr-start);
					curce->ce_varname[ptr-start] = '\0';
					curce->ce_varlinenum = linenumber;
					curce->ce_fileptr = curcf;
					curce->ce_prevlevel = cursection;
					curce->ce_fileposstart = (start - confdata);
				}
				break;
			case '\n':
				linenumber++;
				/* fall through */
			case '\t':
			case ' ':
			case '=':
			case '\r':
				break;
			default:
				if ((*ptr == '*') && (*(ptr+1) == '/'))
				{
					config_status("%s:%i Ignoring extra end comment\n",
						filename, linenumber);
					ptr++;
					break;
				}
				start = ptr;
				for(;*ptr;ptr++)
				{
					if ((*ptr == ' ') || (*ptr == '=') || (*ptr == '\t') || (*ptr == '\n') || (*ptr == ';'))
						break;
				}
				if (!*ptr)
				{
					if (curce)
						config_error("%s: Unexpected EOF for variable starting at %i\n",
							filename, curce->ce_varlinenum);
					else if (cursection) 
						config_error("%s: Unexpected EOF for section starting at %i\n",
							filename, cursection->ce_sectlinenum);
					else
						config_error("%s: Unexpected EOF.\n", filename);
					config_entry_free(curce);
					config_free(curcf);
					return NULL;
				}
				if (curce)
				{
					if (curce->ce_vardata)
					{
						config_status("%s:%i: Ignoring extra data\n",
							filename, linenumber);
					}
					else
					{
						curce->ce_vardata = MyMalloc(ptr-start+1);
						strncpy(curce->ce_vardata, start, ptr-start);
						curce->ce_vardata[ptr-start] = '\0';
					}
				}
				else
				{
					curce = MyMalloc(sizeof(ConfigEntry));
					memset(curce, 0, sizeof(ConfigEntry));
					curce->ce_varname = MyMalloc((ptr-start)+1);
					strncpy(curce->ce_varname, start, ptr-start);
					curce->ce_varname[ptr-start] = '\0';
					curce->ce_varlinenum = linenumber;
					curce->ce_fileptr = curcf;
					curce->ce_prevlevel = cursection;
					curce->ce_fileposstart = (start - confdata);
				}
				if ((*ptr == ';') || (*ptr == '\n'))
					ptr--;
				break;
		} /* switch */
		if (!*ptr) /* This IS possible. -- Syzop */
			break;
	} /* for */
	if (curce)
	{
		config_error("%s: Unexpected EOF for variable starting on line %i\n",
			filename, curce->ce_varlinenum);
		config_entry_free(curce);
		config_free(curcf);
		return NULL;
	}
	else if (cursection)
	{
		config_error("%s: Unexpected EOF for section starting on line %i\n",
				filename, cursection->ce_sectlinenum);
		config_free(curcf);
		return NULL;
	}
	return curcf;
}

static void config_entry_free(ConfigEntry *ceptr)
{
	ConfigEntry	*nptr;

	for(;ceptr;ceptr=nptr)
	{
		nptr = ceptr->ce_next;
		if (ceptr->ce_entries)
			config_entry_free(ceptr->ce_entries);
		if (ceptr->ce_varname)
			free(ceptr->ce_varname);
		if (ceptr->ce_vardata)
			free(ceptr->ce_vardata);
		free(ceptr);
	}
}

ConfigEntry		*config_find_entry(ConfigEntry *ce, char *name)
{
	ConfigEntry *cep;
	
	for (cep = ce; cep; cep = cep->ce_next)
		if (cep->ce_varname && !strcmp(cep->ce_varname, name))
			break;
	return cep;
}

void config_error(char *format, ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	if (!loop.ircd_booted)
#ifndef _WIN32
		fprintf(stderr, "[error] %s\n", buffer);
#else
		win_log("[error] %s", buffer);
#endif
	else
		ircd_log(LOG_ERROR, "config error: %s", buffer);
	sendto_realops("error: %s", buffer);
	/* We cannot live with this */
	config_error_flag = 1;
}

static void inline config_error_missing(const char *filename, int line, const char *entry)
{
	config_error("%s:%d: %s is missing", filename, line, entry);
}

static void inline config_error_unknown(const char *filename, int line, const char *block, 
	const char *entry)
{
	config_error("%s:%d: Unknown directive '%s::%s'", filename, line, block, entry);
}

static void inline config_error_unknownflag(const char *filename, int line, const char *block,
	const char *entry)
{
	config_error("%s:%d: Unknown %s flag '%s'", filename, line, block, entry);
}

static void inline config_error_unknownopt(const char *filename, int line, const char *block,
	const char *entry)
{
	config_error("%s:%d: Unknown %s option '%s'", filename, line, block, entry);
}

static void inline config_error_noname(const char *filename, int line, const char *block)
{
	config_error("%s:%d: %s block has no name", filename, line, block);
}

static void inline config_error_blank(const char *filename, int line, const char *block)
{
	config_error("%s:%d: Blank %s entry", filename, line, block);
}

static void inline config_error_empty(const char *filename, int line, const char *block, 
	const char *entry)
{
	config_error("%s:%d: %s::%s specified without a value",
		filename, line, block, entry);
}

/* Like above */
void config_status(char *format, ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsnprintf(buffer, 1023, format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	if (!loop.ircd_booted)
#ifndef _WIN32
		fprintf(stderr, "* %s\n", buffer);
#else
		win_log("* %s", buffer);
#endif
	sendto_realops("%s", buffer);
}

void config_warn(char *format, ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsnprintf(buffer, 1023, format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	if (!loop.ircd_booted)
#ifndef _WIN32
		fprintf(stderr, "[warning] %s\n", buffer);
#else
		win_log("[warning] %s", buffer);
#endif
	sendto_realops("[warning] %s", buffer);
}

static void inline config_warn_duplicate(const char *filename, int line, const char *entry)
{
	config_warn("%s:%d: Duplicate %s directive", filename, line, entry);
}

void config_progress(char *format, ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsnprintf(buffer, 1023, format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	if (!loop.ircd_booted)
#ifndef _WIN32
		fprintf(stderr, "* %s\n", buffer);
#else
		win_log("* %s", buffer);
#endif
	sendto_realops("%s", buffer);
}

static int inline config_is_blankorempty(ConfigEntry *cep, const char *block)
{
	if (!cep->ce_varname)
	{
		config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum, block);
		return 1;
	}
	if (!cep->ce_vardata)
	{
		config_error_empty(cep->ce_fileptr->cf_filename, cep->ce_varlinenum, block,
			cep->ce_varname);
		return 1;
	}
	return 0;
}

ConfigCommand *config_binary_search(char *cmd) {
	int start = 0;
	int stop = ARRAY_SIZEOF(_ConfigCommands)-1;
	int mid;
	while (start <= stop) {
		mid = (start+stop)/2;
		if (smycmp(cmd,_ConfigCommands[mid].name) < 0) {
			stop = mid-1;
		}
		else if (strcmp(cmd,_ConfigCommands[mid].name) == 0) {
			return &_ConfigCommands[mid];
		}
		else
			start = mid+1;
	}
	return NULL;
}

void	free_iConf(aConfiguration *i)
{
	ircfree(i->name_server);
	ircfree(i->kline_address);
	ircfree(i->gline_address);
	ircfree(i->auto_join_chans);
	ircfree(i->oper_auto_join_chans);
	ircfree(i->oper_only_stats);
	ircfree(i->channel_command_prefix);
	ircfree(i->oper_snomask);
	ircfree(i->user_snomask);
	ircfree(i->egd_path);
	ircfree(i->static_quit);
#ifdef USE_SSL
	ircfree(i->x_server_cert_pem);
	ircfree(i->x_server_key_pem);
	ircfree(i->x_server_cipher_list);
	ircfree(i->trusted_ca_file);
#endif	
	ircfree(i->restrict_usermodes);
	ircfree(i->restrict_channelmodes);
	ircfree(i->restrict_extendedbans);
	ircfree(i->network.x_ircnetwork);
	ircfree(i->network.x_ircnet005);	
	ircfree(i->network.x_defserv);
	ircfree(i->network.x_services_name);
	ircfree(i->network.x_oper_host);
	ircfree(i->network.x_admin_host);
	ircfree(i->network.x_locop_host);	
	ircfree(i->network.x_sadmin_host);
	ircfree(i->network.x_netadmin_host);
	ircfree(i->network.x_coadmin_host);
	ircfree(i->network.x_hidden_host);
	ircfree(i->network.x_prefix_quit);
	ircfree(i->network.x_helpchan);
	ircfree(i->network.x_stats_server);
	ircfree(i->spamfilter_ban_reason);
	ircfree(i->spamfilter_virus_help_channel);
	ircfree(i->spamexcept_line);
}

int	config_test();

void config_setdefaultsettings(aConfiguration *i)
{
	i->unknown_flood_amount = 4;
	i->unknown_flood_bantime = 600;
	i->oper_snomask = strdup(SNO_DEFOPER);
	i->ident_read_timeout = 30;
	i->ident_connect_timeout = 10;
	i->nick_count = 3; i->nick_period = 60; /* nickflood protection: max 3 per 60s */
#ifdef NO_FLOOD_AWAY
	i->away_count = 4; i->away_period = 120; /* awayflood protection: max 4 per 120s */
#endif
#ifdef NEWCHFLOODPROT
	i->modef_default_unsettime = 0;
	i->modef_max_unsettime = 60; /* 1 hour seems enough :p */
#endif
	i->ban_version_tkl_time = 86400; /* 1d */
	i->spamfilter_ban_time = 86400; /* 1d */
	i->spamfilter_ban_reason = strdup("Spam/advertising");
	i->spamfilter_virus_help_channel = strdup("#help");
	i->spamfilter_detectslow_warn = 250;
	i->spamfilter_detectslow_fatal = 500;
	i->maxdccallow = 10;
	i->channel_command_prefix = strdup("`!.");
	i->check_target_nick_bans = 1;
	i->maxbans = 60;
	i->maxbanlength = 2048;
	i->timesynch_enabled = 1;
	i->timesynch_timeout = 3;
	i->timesynch_server = strdup("193.67.79.202,192.43.244.18,128.250.36.3"); /* nlnet (EU), NIST (US), uni melbourne (AU). All open acces, nonotify, nodns. */
	i->name_server = strdup("127.0.0.1"); /* default, especially needed for w2003+ in some rare cases */
	i->level_on_join = CHFL_CHANOP;
	i->watch_away_notification = 1;
}

/* 1: needed for set::options::allow-part-if-shunned,
 * we can't just make it M_SHUN and do a ALLOW_PART_IF_SHUNNED in
 * m_part itself because that will also block internal calls (like sapart). -- Syzop
 * 2: now also used by spamfilter entries added by config...
 * we got a chicken-and-egg problem here.. antries added without reason or ban-time
 * field should use the config default (set::spamfilter::ban-reason/ban-time) but
 * this isn't (or might not) be known yet when parsing spamfilter entries..
 * so we do a VERY UGLY mass replace here.. unless someone else has a better idea.
 */
static void do_weird_shun_stuff()
{
aCommand *cmptr;
aTKline *tk;
char *encoded;

	if ((cmptr = find_Command_simple("PART")))
	{
		if (ALLOW_PART_IF_SHUNNED)
			cmptr->flags |= M_SHUN;
		else
			cmptr->flags &= ~M_SHUN;
	}

	encoded = unreal_encodespace(SPAMFILTER_BAN_REASON);
	if (!encoded)
		abort(); /* hack to trace 'impossible' bug... */
	for (tk = tklines[tkl_hash('q')]; tk; tk = tk->next)
	{
		if (tk->type != TKL_NICK)
			continue;
		if (!tk->setby)
		{
			if (me.name[0] != '\0')
				tk->setby = strdup(me.name);
			else
				tk->setby = strdup(conf_me->name ? conf_me->name : "~server~");
		}
	}

	for (tk = tklines[tkl_hash('f')]; tk; tk = tk->next)
	{
		if (tk->type != TKL_SPAMF)
			continue; /* global entry or something else.. */
		if (!strcmp(tk->ptr.spamf->tkl_reason, "<internally added by ircd>"))
		{
			MyFree(tk->ptr.spamf->tkl_reason);
			tk->ptr.spamf->tkl_reason = strdup(encoded);
			tk->ptr.spamf->tkl_duration = SPAMFILTER_BAN_TIME;
		}
		/* This one is even more ugly, but our config crap is VERY confusing :[ */
		if (!tk->setby)
		{
			if (me.name[0] != '\0')
				tk->setby = strdup(me.name);
			else
				tk->setby = strdup(conf_me->name ? conf_me->name : "~server~");
		}
	}
	if (loop.ircd_booted) /* only has to be done for rehashes, api-isupport takes care of boot */
	{
		if (WATCH_AWAY_NOTIFICATION)
		{
			IsupportAdd(NULL, "WATCHOPTS", "A");
		} else {
			Isupport *hunted = IsupportFind("WATCHOPTS");
			if (hunted)
				IsupportDel(hunted);
		}
	}
}

static void make_default_logblock(void)
{
ConfigItem_log *ca = MyMallocEx(sizeof(ConfigItem_log));

	config_status("No log { } block found -- using default: errors will be logged to 'ircd.log'");

	ca->file = strdup("ircd.log");
	ca->flags |= LOG_ERROR;
	AddListItem(ca, conf_log);
}

int isanyserverlinked(void)
{
int i;
aClient *acptr;

	for (i = LastSlot; i >= 0; i--)
		if ((acptr = local[i]) && (acptr != &me) && IsServer(acptr))
			return 1;

	return 0;
}

void applymeblock(void)
{
	if (!conf_me || !me.serv)
		return; /* uh-huh? */
	
	/* Numeric change? */
	if (conf_me->numeric != me.serv->numeric)
	{
		/* Can we apply ? */
		if (!isanyserverlinked())
		{
			me.serv->numeric = conf_me->numeric;
		} else {
			config_warn("me::numeric: Numeric change detected, but change cannot be applied "
			            "due to being linked to other servers. Unlink all servers and /REHASH to "
			            "try again.");
		}
	}
}

int	init_conf(char *rootconf, int rehash)
{
	config_status("Loading IRCd configuration ..");
	if (conf)
	{
		config_error("%s:%i - Someone forgot to clean up", __FILE__, __LINE__);
		return -1;
	}
	bzero(&tempiConf, sizeof(iConf));
	bzero(&settings, sizeof(settings));
	bzero(&requiredstuff, sizeof(requiredstuff));
	config_setdefaultsettings(&tempiConf);
	if (load_conf(rootconf) > 0)
	{
		charsys_reset_pretest();
		if ((config_test() < 0) || (callbacks_check() < 0) || (efunctions_check() < 0) ||
		    (charsys_postconftest() < 0))
		{
			config_error("IRCd configuration failed to pass testing");
#ifdef _WIN32
			if (!rehash)
				win_error();
#endif
#ifndef STATIC_LINKING
			Unload_all_testing_modules();
#endif
			unload_notloaded_includes();
			config_free(conf);
			conf = NULL;
			free_iConf(&tempiConf);
			return -1;
		}
		callbacks_switchover();
		efunctions_switchover();
		if (rehash)
		{
			Hook *h;
			unrealdns_delasyncconnects();
			config_rehash();
#ifndef STATIC_LINKING
			Unload_all_loaded_modules();

			/* Notify permanent modules of the rehash */
			for (h = Hooks[HOOKTYPE_REHASH]; h; h = h->next)
		        {
				if (!h->owner)
					continue;
				if (!(h->owner->options & MOD_OPT_PERM))
					continue;
				(*(h->func.intfunc))();
			}
#else
			RunHook0(HOOKTYPE_REHASH);
#endif
			unload_loaded_includes();
		}
		load_includes();
#ifndef STATIC_LINKING
		Init_all_testing_modules();
#else
		if (!rehash) {
			ModuleInfo ModCoreInfo;
			ModCoreInfo.size = sizeof(ModuleInfo);
			ModCoreInfo.module_load = 0;
			ModCoreInfo.handle = NULL;
			l_commands_Init(&ModCoreInfo);
		}
#endif
		charsys_reset();
		if (config_run() < 0)
		{
			config_error("Bad case of config errors. Server will now die. This really shouldn't happen");
#ifdef _WIN32
			if (!rehash)
				win_error();
#endif
			abort();
		}
		charsys_finish();
		applymeblock();
	}
	else	
	{
		config_error("IRCd configuration failed to load");
#ifndef STATIC_LINKING
		Unload_all_testing_modules();
#endif
		unload_notloaded_includes();
		config_free(conf);
		conf = NULL;
		free_iConf(&tempiConf);
#ifdef _WIN32
		if (!rehash)
			win_error();
#endif
		return -1;
	}
	config_free(conf);
	conf = NULL;
	if (rehash)
	{
#ifndef STATIC_LINKING
		module_loadall(0);
#endif
		RunHook0(HOOKTYPE_REHASH_COMPLETE);
	}
	do_weird_shun_stuff();
	if (!conf_log)
		make_default_logblock();
	nextconnect = TStime() + 1; /* check for autoconnects */
	config_status("Configuration loaded without any problems ..");
	return 0;
}

int	load_conf(char *filename)
{
	ConfigFile 	*cfptr, *cfptr2, **cfptr3;
	ConfigEntry 	*ce;
	int		ret;

	if (config_verbose > 0)
		config_status("Loading config file %s ..", filename);
	if ((cfptr = config_load(filename)))
	{
		for (cfptr3 = &conf, cfptr2 = conf; cfptr2; cfptr2 = cfptr2->cf_next)
			cfptr3 = &cfptr2->cf_next;
		*cfptr3 = cfptr;
		if (config_verbose > 1)
			config_status("Loading modules in %s", filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
			if (!strcmp(ce->ce_varname, "loadmodule"))
			{
				 ret = _conf_loadmodule(cfptr, ce);
				 if (ret < 0) 
					 	return ret;
			}
		if (config_verbose > 1)
			config_status("Searching through %s for include files..", filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
			if (!strcmp(ce->ce_varname, "include"))
			{
				 ret = _conf_include(cfptr, ce);
				 if (ret < 0) 
					 	return ret;
			}
		return 1;
	}
	else
	{
		config_error("Could not load config file %s", filename);
		return -1;
	}	
}

void	config_rehash()
{
	ConfigItem_oper			*oper_ptr;
	ConfigItem_class 		*class_ptr;
	ConfigItem_ulines 		*uline_ptr;
	ConfigItem_allow 		*allow_ptr;
	ConfigItem_except 		*except_ptr;
	ConfigItem_ban 			*ban_ptr;
	ConfigItem_link 		*link_ptr;
	ConfigItem_cgiirc 		*cgiirc_ptr;
	ConfigItem_listen	 	*listen_ptr;
	ConfigItem_tld			*tld_ptr;
	ConfigItem_vhost		*vhost_ptr;
	ConfigItem_badword		*badword_ptr;
	ConfigItem_deny_dcc		*deny_dcc_ptr;
	ConfigItem_allow_dcc		*allow_dcc_ptr;
	ConfigItem_deny_link		*deny_link_ptr;
	ConfigItem_deny_channel		*deny_channel_ptr;
	ConfigItem_allow_channel	*allow_channel_ptr;
	ConfigItem_admin		*admin_ptr;
	ConfigItem_deny_version		*deny_version_ptr;
	ConfigItem_log			*log_ptr;
	ConfigItem_alias		*alias_ptr;
	ConfigItem_help			*help_ptr;
	ConfigItem_offchans		*of_ptr;
	OperStat 			*os_ptr;
	ListStruct 	*next, *next2;
	aTKline *tk, *tk_next;
	SpamExcept *spamex_ptr;
	int i;

	USE_BAN_VERSION = 0;
	/* clean out stuff that we don't use */	
	for (admin_ptr = conf_admin; admin_ptr; admin_ptr = (ConfigItem_admin *)next)
	{
		next = (ListStruct *)admin_ptr->next;
		ircfree(admin_ptr->line);
		DelListItem(admin_ptr, conf_admin);
		MyFree(admin_ptr);
	}
	/* wipe the fckers out ..*/
	for (oper_ptr = conf_oper; oper_ptr; oper_ptr = (ConfigItem_oper *)next)
	{
		ConfigItem_oper_from *oper_from;
		next = (ListStruct *)oper_ptr->next;
		ircfree(oper_ptr->name);
		ircfree(oper_ptr->swhois);
		ircfree(oper_ptr->snomask);
		Auth_DeleteAuthStruct(oper_ptr->auth);
		for (oper_from = (ConfigItem_oper_from *) oper_ptr->from; oper_from; oper_from = (ConfigItem_oper_from *) next2)
		{
			next2 = (ListStruct *)oper_from->next;
			ircfree(oper_from->name);
			if (oper_from->netmask)
			{
				MyFree(oper_from->netmask);
			}
			DelListItem(oper_from, oper_ptr->from);
			MyFree(oper_from);
		}
		DelListItem(oper_ptr, conf_oper);
		MyFree(oper_ptr);
	}
	for (link_ptr = conf_link; link_ptr; link_ptr = (ConfigItem_link *) next)
	{
		next = (ListStruct *)link_ptr->next;
		if (link_ptr->refcount == 0)
		{
			Debug((DEBUG_ERROR, "s_conf: deleting block %s (refcount 0)", link_ptr->servername));
			delete_linkblock(link_ptr);
		}
		else
		{
			Debug((DEBUG_ERROR, "s_conf: marking block %s (refcount %d) as temporary",
				link_ptr->servername, link_ptr->refcount));
			link_ptr->flag.temporary = 1;
		}
	}
	for (class_ptr = conf_class; class_ptr; class_ptr = (ConfigItem_class *) next)
	{
		next = (ListStruct *)class_ptr->next;
		if (class_ptr->flag.permanent == 1)
			continue;
		class_ptr->flag.temporary = 1;
		/* We'll wipe it out when it has no clients */
		if (!class_ptr->clients && !class_ptr->xrefcount)
		{
			delete_classblock(class_ptr);
		}
	}
	for (uline_ptr = conf_ulines; uline_ptr; uline_ptr = (ConfigItem_ulines *) next)
	{
		next = (ListStruct *)uline_ptr->next;
		/* We'll wipe it out when it has no clients */
		ircfree(uline_ptr->servername);
		DelListItem(uline_ptr, conf_ulines);
		MyFree(uline_ptr);
	}
	for (allow_ptr = conf_allow; allow_ptr; allow_ptr = (ConfigItem_allow *) next)
	{
		next = (ListStruct *)allow_ptr->next;
		ircfree(allow_ptr->ip);
		ircfree(allow_ptr->hostname);
		if (allow_ptr->netmask)
			MyFree(allow_ptr->netmask);
		Auth_DeleteAuthStruct(allow_ptr->auth);
		DelListItem(allow_ptr, conf_allow);
		MyFree(allow_ptr);
	}
	for (except_ptr = conf_except; except_ptr; except_ptr = (ConfigItem_except *) next)
	{
		next = (ListStruct *)except_ptr->next;
		ircfree(except_ptr->mask);
		if (except_ptr->netmask)
			MyFree(except_ptr->netmask);
		DelListItem(except_ptr, conf_except);
		MyFree(except_ptr);
	}
	for (ban_ptr = conf_ban; ban_ptr; ban_ptr = (ConfigItem_ban *) next)
	{
		next = (ListStruct *)ban_ptr->next;
		if (ban_ptr->flag.type2 == CONF_BAN_TYPE_CONF || ban_ptr->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
		{
			ircfree(ban_ptr->mask);
			ircfree(ban_ptr->reason);
			if (ban_ptr->netmask)
				MyFree(ban_ptr->netmask);
			DelListItem(ban_ptr, conf_ban);
			MyFree(ban_ptr);
		}
	}
	for (listen_ptr = conf_listen; listen_ptr; listen_ptr = (ConfigItem_listen *)listen_ptr->next)
	{
		listen_ptr->flag.temporary = 1;
	}
	for (tld_ptr = conf_tld; tld_ptr; tld_ptr = (ConfigItem_tld *) next)
	{
		aMotd *motd;
		next = (ListStruct *)tld_ptr->next;
		ircfree(tld_ptr->motd_file);
		ircfree(tld_ptr->rules_file);
		ircfree(tld_ptr->smotd_file);
		ircfree(tld_ptr->opermotd_file);
		ircfree(tld_ptr->botmotd_file);
		if (!tld_ptr->flag.motdptr) {
			while (tld_ptr->motd) {
				motd = tld_ptr->motd->next;
				ircfree(tld_ptr->motd->line);
				ircfree(tld_ptr->motd);
				tld_ptr->motd = motd;
			}
		}
		if (!tld_ptr->flag.rulesptr) {
			while (tld_ptr->rules) {
				motd = tld_ptr->rules->next;
				ircfree(tld_ptr->rules->line);
				ircfree(tld_ptr->rules);
				tld_ptr->rules = motd;
			}
		}
		while (tld_ptr->smotd) {
			motd = tld_ptr->smotd->next;
			ircfree(tld_ptr->smotd->line);
			ircfree(tld_ptr->smotd);
			tld_ptr->smotd = motd;
		}
		while (tld_ptr->opermotd) {
			motd = tld_ptr->opermotd->next;
			ircfree(tld_ptr->opermotd->line);
			ircfree(tld_ptr->opermotd);
			tld_ptr->opermotd = motd;
		}
		while (tld_ptr->botmotd) {
			motd = tld_ptr->botmotd->next;
			ircfree(tld_ptr->botmotd->line);
			ircfree(tld_ptr->botmotd);
			tld_ptr->botmotd = motd;
		}
		DelListItem(tld_ptr, conf_tld);
		MyFree(tld_ptr);
	}
	for (vhost_ptr = conf_vhost; vhost_ptr; vhost_ptr = (ConfigItem_vhost *) next)
	{
		ConfigItem_oper_from *vhost_from;
		
		next = (ListStruct *)vhost_ptr->next;
		
		ircfree(vhost_ptr->login);
		Auth_DeleteAuthStruct(vhost_ptr->auth);
		ircfree(vhost_ptr->virthost);
		ircfree(vhost_ptr->virtuser);
		for (vhost_from = (ConfigItem_oper_from *) vhost_ptr->from; vhost_from;
			vhost_from = (ConfigItem_oper_from *) next2)
		{
			next2 = (ListStruct *)vhost_from->next;
			ircfree(vhost_from->name);
			DelListItem(vhost_from, vhost_ptr->from);
			MyFree(vhost_from);
		}
		DelListItem(vhost_ptr, conf_vhost);
		MyFree(vhost_ptr);
	}

#ifdef STRIPBADWORDS
	for (badword_ptr = conf_badword_channel; badword_ptr;
		badword_ptr = (ConfigItem_badword *) next) {
		next = (ListStruct *)badword_ptr->next;
		ircfree(badword_ptr->word);
		if (badword_ptr->replace)
			ircfree(badword_ptr->replace);
		regfree(&badword_ptr->expr);
		DelListItem(badword_ptr, conf_badword_channel);
		MyFree(badword_ptr);
	}
	for (badword_ptr = conf_badword_message; badword_ptr;
		badword_ptr = (ConfigItem_badword *) next) {
		next = (ListStruct *)badword_ptr->next;
		ircfree(badword_ptr->word);
		if (badword_ptr->replace)
			ircfree(badword_ptr->replace);
		regfree(&badword_ptr->expr);
		DelListItem(badword_ptr, conf_badword_message);
		MyFree(badword_ptr);
	}
	for (badword_ptr = conf_badword_quit; badword_ptr;
		badword_ptr = (ConfigItem_badword *) next) {
		next = (ListStruct *)badword_ptr->next;
		ircfree(badword_ptr->word);
		if (badword_ptr->replace)
			ircfree(badword_ptr->replace);
		regfree(&badword_ptr->expr);
		DelListItem(badword_ptr, conf_badword_quit);
		MyFree(badword_ptr);
	}
#endif
	/* Clean up local spamfilter entries... */
	for (tk = tklines[tkl_hash('f')]; tk; tk = tk_next)
	{
		if (tk->type == TKL_SPAMF)
			tk_next = tkl_del_line(tk);
		else /* global spamfilter.. don't touch! */
			tk_next = tk->next;
	}

	for (tk = tklines[tkl_hash('q')]; tk; tk = tk_next)
	{
		if (tk->type == TKL_NICK)
			tk_next = tkl_del_line(tk);
		else 
			tk_next = tk->next;
	}

	for (deny_dcc_ptr = conf_deny_dcc; deny_dcc_ptr; deny_dcc_ptr = (ConfigItem_deny_dcc *)next)
	{
		next = (ListStruct *)deny_dcc_ptr->next;
		if (deny_dcc_ptr->flag.type2 == CONF_BAN_TYPE_CONF)
		{
			ircfree(deny_dcc_ptr->filename);
			ircfree(deny_dcc_ptr->reason);
			DelListItem(deny_dcc_ptr, conf_deny_dcc);
			MyFree(deny_dcc_ptr);
		}
	}
	for (deny_link_ptr = conf_deny_link; deny_link_ptr; deny_link_ptr = (ConfigItem_deny_link *) next) {
		next = (ListStruct *)deny_link_ptr->next;
		ircfree(deny_link_ptr->prettyrule);
		ircfree(deny_link_ptr->mask);
		crule_free(&deny_link_ptr->rule);
		DelListItem(deny_link_ptr, conf_deny_link);
		MyFree(deny_link_ptr);
	}
	for (deny_version_ptr = conf_deny_version; deny_version_ptr; deny_version_ptr = (ConfigItem_deny_version *) next) {
		next = (ListStruct *)deny_version_ptr->next;
		ircfree(deny_version_ptr->mask);
		ircfree(deny_version_ptr->version);
		ircfree(deny_version_ptr->flags);
		DelListItem(deny_version_ptr, conf_deny_version);
		MyFree(deny_version_ptr);
	}
	for (deny_channel_ptr = conf_deny_channel; deny_channel_ptr; deny_channel_ptr = (ConfigItem_deny_channel *) next)
	{
		next = (ListStruct *)deny_channel_ptr->next;
		ircfree(deny_channel_ptr->redirect);
		ircfree(deny_channel_ptr->channel);
		ircfree(deny_channel_ptr->reason);
		DelListItem(deny_channel_ptr, conf_deny_channel);
		MyFree(deny_channel_ptr);
	}

	for (allow_channel_ptr = conf_allow_channel; allow_channel_ptr; allow_channel_ptr = (ConfigItem_allow_channel *) next)
	{
		next = (ListStruct *)allow_channel_ptr->next;
		ircfree(allow_channel_ptr->channel);
		DelListItem(allow_channel_ptr, conf_allow_channel);
		MyFree(allow_channel_ptr);
	}
	for (allow_dcc_ptr = conf_allow_dcc; allow_dcc_ptr; allow_dcc_ptr = (ConfigItem_allow_dcc *)next)
	{
		next = (ListStruct *)allow_dcc_ptr->next;
		if (allow_dcc_ptr->flag.type2 == CONF_BAN_TYPE_CONF)
		{
			ircfree(allow_dcc_ptr->filename);
			DelListItem(allow_dcc_ptr, conf_allow_dcc);
			MyFree(allow_dcc_ptr);
		}
	}

	if (conf_drpass)
	{
		Auth_DeleteAuthStruct(conf_drpass->restartauth);
		conf_drpass->restartauth = NULL;
		Auth_DeleteAuthStruct(conf_drpass->dieauth);
		conf_drpass->dieauth = NULL;
		ircfree(conf_drpass);
	}
	for (log_ptr = conf_log; log_ptr; log_ptr = (ConfigItem_log *)next) {
		next = (ListStruct *)log_ptr->next;
		ircfree(log_ptr->file);
		DelListItem(log_ptr, conf_log);
		MyFree(log_ptr);
	}
	for (alias_ptr = conf_alias; alias_ptr; alias_ptr = (ConfigItem_alias *)next) {
		aCommand *cmptr = find_Command(alias_ptr->alias, 0, 0);
		ConfigItem_alias_format *fmt;
		next = (ListStruct *)alias_ptr->next;		
		ircfree(alias_ptr->nick);
		del_Command(alias_ptr->alias, NULL, cmptr->func);
		ircfree(alias_ptr->alias);
		if (alias_ptr->format && (alias_ptr->type == ALIAS_COMMAND)) {
			for (fmt = (ConfigItem_alias_format *) alias_ptr->format; fmt; fmt = (ConfigItem_alias_format *) next2)
			{
				next2 = (ListStruct *)fmt->next;
				ircfree(fmt->format);
				ircfree(fmt->nick);
				ircfree(fmt->parameters);
				regfree(&fmt->expr);
				DelListItem(fmt, alias_ptr->format);
				MyFree(fmt);
			}
		}
		DelListItem(alias_ptr, conf_alias);
		MyFree(alias_ptr);
	}
	for (help_ptr = conf_help; help_ptr; help_ptr = (ConfigItem_help *)next) {
		aMotd *text;
		next = (ListStruct *)help_ptr->next;
		ircfree(help_ptr->command);
		while (help_ptr->text) {
			text = help_ptr->text->next;
			ircfree(help_ptr->text->line);
			ircfree(help_ptr->text);
			help_ptr->text = text;
		}
		DelListItem(help_ptr, conf_help);
		MyFree(help_ptr);
	}
	for (os_ptr = iConf.oper_only_stats_ext; os_ptr; os_ptr = (OperStat *)next)
	{
		next = (ListStruct *)os_ptr->next;
		ircfree(os_ptr->flag);
		MyFree(os_ptr);
	}
	iConf.oper_only_stats_ext = NULL;
	for (spamex_ptr = iConf.spamexcept; spamex_ptr; spamex_ptr = (SpamExcept *)next)
	{
		next = (ListStruct *)spamex_ptr->next;
		MyFree(spamex_ptr);
	}
	iConf.spamexcept = NULL;
	for (of_ptr = conf_offchans; of_ptr; of_ptr = (ConfigItem_offchans *)next)
	{
		next = (ListStruct *)of_ptr->next;
		ircfree(of_ptr->topic);
		MyFree(of_ptr);
	}
	conf_offchans = NULL;
	
#ifdef EXTCMODE
	for (i = 0; i < EXTCMODETABLESZ; i++)
	{
		if (iConf.modes_on_join.extparams[i])
			free(iConf.modes_on_join.extparams[i]);
	}
#endif

	for (cgiirc_ptr = conf_cgiirc; cgiirc_ptr; cgiirc_ptr = (ConfigItem_cgiirc *) next)
	{
		next = (ListStruct *)cgiirc_ptr->next;
		delete_cgiircblock(cgiirc_ptr);
	}
}

int	config_post_test()
{
#define Error(x) { config_error((x)); errors++; }
	int 	errors = 0;
	Hook *h;
	
	if (!requiredstuff.conf_me)
		Error("me {} block is missing");
	if (!requiredstuff.conf_admin)
		Error("admin {} block is missing");
	if (!requiredstuff.conf_listen)
		Error("listen {} block is missing");
	if (!settings.has_kline_address)
		Error("set::kline-address is missing");
	if (!settings.has_maxchannelsperuser)
		Error("set::maxchannelsperuser is missing");
#if 0
	if (!settings.has_dns_nameserver)
		Error("set::dns::nameserver is missing");
	if (!settings.has_dns_timeout)
		Error("set::dns::timeout is missing");
	if (!settings.has_dns_retries)
		Error("set::dns::retries is missing");
#endif
	if (!settings.has_services_server)
		Error("set::services-server is missing");
	if (!settings.has_default_server)
		Error("set::default-server is missing");
	if (!settings.has_network_name)
		Error("set::network-name is missing");
	if (!settings.has_hosts_global)
		Error("set::hosts::global is missing");
	if (!settings.has_hosts_admin)
		Error("set::hosts::admin is missing");
	if (!settings.has_hosts_servicesadmin)
		Error("set::hosts::servicesadmin is missing");
	if (!settings.has_hosts_netadmin)
		Error("set::hosts::netadmin is missing");
	if (!settings.has_hosts_coadmin)
		Error("set::hosts::coadmin is missing");
	if (!settings.has_help_channel)
		Error("set::help-channel is missing");
	if (!settings.has_hiddenhost_prefix)
		Error("set::hiddenhost-prefix is missing");
		
	for (h = Hooks[HOOKTYPE_CONFIGPOSTTEST]; h; h = h->next) 
	{
		int value, errs = 0;
		if (h->owner && !(h->owner->flags & MODFLAG_TESTING) &&
		                !(h->owner->options & MOD_OPT_PERM))
			continue;
		value = (*(h->func.intfunc))(&errs);
		if (value == -1)
		{
			errors += errs;
			break;
		}
		if (value == -2)
			errors += errs;
	}
	return errors;	
}

int	config_run()
{
	ConfigEntry 	*ce;
	ConfigFile	*cfptr;
	ConfigCommand	*cc;
	int		errors = 0;
	Hook *h;
	for (cfptr = conf; cfptr; cfptr = cfptr->cf_next)
	{
		if (config_verbose > 1)
			config_status("Running %s", cfptr->cf_filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
		{
			if ((cc = config_binary_search(ce->ce_varname))) {
				if ((cc->conffunc) && (cc->conffunc(cfptr, ce) < 0))
					errors++;
			}
			else
			{
				int value;
				for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
				{
					value = (*(h->func.intfunc))(cfptr,ce,CONFIG_MAIN);
					if (value == 1)
						break;
				}
			}
		}
	}

	close_listeners();
	listen_cleanup();
	close_listeners();
	loop.do_bancheck = 1;
	free_iConf(&iConf);
	bcopy(&tempiConf, &iConf, sizeof(aConfiguration));
	bzero(&tempiConf, sizeof(aConfiguration));
#ifdef THROTTLING
	{
		EventInfo eInfo;
		long v;
		eInfo.flags = EMOD_EVERY;
		if (!THROTTLING_PERIOD)
			v = 120;
		else
		{
			v = THROTTLING_PERIOD/2;
			if (v > 5)
				v = 5; /* accuracy, please */
		}
		eInfo.every = v;
		EventMod(EventFind("bucketcleaning"), &eInfo);
	}
#endif

	if (errors > 0)
	{
		config_error("%i fatal errors encountered", errors);
	}
	return (errors > 0 ? -1 : 1);
}


OperFlag *config_binary_flags_search(OperFlag *table, char *cmd, int size) {
	int start = 0;
	int stop = size-1;
	int mid;
	while (start <= stop) {
		mid = (start+stop)/2;

		if (smycmp(cmd,table[mid].name) < 0) {
			stop = mid-1;
		}
		else if (strcmp(cmd,table[mid].name) == 0) {
			return &(table[mid]);
		}
		else
			start = mid+1;
	}
	return NULL;
}


int	config_test()
{
	ConfigEntry 	*ce;
	ConfigFile	*cfptr;
	ConfigCommand	*cc;
	int		errors = 0;
	Hook *h;

	for (cfptr = conf; cfptr; cfptr = cfptr->cf_next)
	{
		if (config_verbose > 1)
			config_status("Testing %s", cfptr->cf_filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
		{
			if (!ce->ce_varname)
			{
				config_error("%s:%i: %s:%i: null ce->ce_varname",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					__FILE__, __LINE__);
				return -1;
			}
			if ((cc = config_binary_search(ce->ce_varname))) {
				if (cc->testfunc)
					errors += (cc->testfunc(cfptr, ce));
			}
			else 
			{
				int used = 0;
				for (h = Hooks[HOOKTYPE_CONFIGTEST]; h; h = h->next) 
				{
					int value, errs = 0;
					if (h->owner && !(h->owner->flags & MODFLAG_TESTING)
					    && !(h->owner->options & MOD_OPT_PERM))


						continue;
					value = (*(h->func.intfunc))(cfptr,ce,CONFIG_MAIN,&errs);
					if (value == 2)
						used = 1;
					if (value == 1)
					{
						used = 1;
						break;
					}
					if (value == -1)
					{
						used = 1;
						errors += errs;
						break;
					}
					if (value == -2) 
					{
						used = 1;
						errors += errs;
					}
						
				}
				if (!used)
					config_status("%s:%i: unknown directive %s", 
						ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
						ce->ce_varname);
			}
		}
	}
	errors += config_post_test();
	if (errors > 0)
	{
		config_error("%i errors encountered", errors);
	}
	return (errors > 0 ? -1 : 1);
}

/*
 * Service functions
*/

ConfigItem_deny_dcc	*Find_deny_dcc(char *name)
{
	ConfigItem_deny_dcc	*p;

	if (!name)
		return NULL;

	for (p = conf_deny_dcc; p; p = (ConfigItem_deny_dcc *) p->next)
	{
		if (!match(name, p->filename))
			return (p);
	}
	return NULL;
}

ConfigItem_alias *Find_alias(char *name) {
	ConfigItem_alias *alias;

	if (!name)
		return NULL;

	for (alias = conf_alias; alias; alias = (ConfigItem_alias *)alias->next) {
		if (!stricmp(alias->alias, name))
			return alias;
	}
	return NULL;
}

ConfigItem_class	*Find_class(char *name)
{
	ConfigItem_class	*p;

	if (!name)
		return NULL;

	for (p = conf_class; p; p = (ConfigItem_class *) p->next)
	{
		if (!strcmp(name, p->name))
			return (p);
	}
	return NULL;
}

ConfigItem_oper	*Find_oper(char *name)
{
	ConfigItem_oper	*p;

	if (!name)
		return NULL;

	for (p = conf_oper; p; p = (ConfigItem_oper *) p->next)
	{
		if (!strcmp(name, p->name))
			return (p);
	}
	return NULL;
}

int count_oper_sessions(char *name)
{
int i, count = 0;
aClient *cptr;

#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
int j;
	for (i = oper_fdlist.entry[j = 1]; j <= oper_fdlist.last_entry; i = oper_fdlist.entry[++j])
#endif
		if ((cptr = local[i]) && IsPerson(cptr) && IsAnOper(cptr) &&
		    cptr->user && cptr->user->operlogin && !strcmp(cptr->user->operlogin,name))
			count++;

	return count;
}

ConfigItem_listen	*Find_listen(char *ipmask, int port)
{
	ConfigItem_listen	*p;

	if (!ipmask)
		return NULL;

	for (p = conf_listen; p; p = (ConfigItem_listen *) p->next)
	{
		if (!match(p->ip, ipmask) && (port == p->port))
			return (p);
		if (!match(ipmask, p->ip) && (port == p->port))
			return (p);
	}
	return NULL;
}

ConfigItem_ulines *Find_uline(char *host) {
	ConfigItem_ulines *ulines;

	if (!host)
		return NULL;

	for(ulines = conf_ulines; ulines; ulines =(ConfigItem_ulines *) ulines->next) {
		if (!stricmp(host, ulines->servername))
			return ulines;
	}
	return NULL;
}


ConfigItem_except *Find_except(aClient *sptr, char *host, short type) {
	ConfigItem_except *excepts;

	if (!host)
		return NULL;

	for(excepts = conf_except; excepts; excepts =(ConfigItem_except *) excepts->next) {
		if (excepts->flag.type == type)
		{
			if (match_ip(sptr->ip, host, excepts->mask, excepts->netmask))
				return excepts;
		}
	}
	return NULL;
}

ConfigItem_tld *Find_tld(aClient *cptr, char *uhost) {
	ConfigItem_tld *tld;

	if (!uhost || !cptr)
		return NULL;

	for(tld = conf_tld; tld; tld = (ConfigItem_tld *) tld->next)
	{
		if (!match(tld->mask, uhost))
		{
			if ((tld->options & TLD_SSL) && !IsSecure(cptr))
				continue;
			if ((tld->options & TLD_REMOTE) && MyClient(cptr))
				continue;
			return tld;
		}
	}
	return NULL;
}


ConfigItem_link *Find_link(char *username,
			   char *hostname,
			   char *ip,
			   char *servername)
{
	ConfigItem_link	*link;

	if (!username || !hostname || !servername || !ip)
		return NULL;

	for(link = conf_link; link; link = (ConfigItem_link *) link->next)
	{
		if (!match(link->servername, servername) &&
		    !match(link->username, username) &&
		    (!match(link->hostname, hostname) || !match(link->hostname, ip)))
			return link;
	}
	return NULL;

}

/* ugly ugly ugly */
int match_ip46(char *a, char *b)
{
#ifdef INET6
	if (!strncmp(a, "::ffff:", 7) && !strcmp(a+7, b))
		return 0; // match
#endif
	return 1; //nomatch
}

ConfigItem_cgiirc *Find_cgiirc(char *username, char *hostname, char *ip, CGIIRCType type)
{
ConfigItem_cgiirc *e;

	if (!username || !hostname || !ip)
		return NULL;

	for (e = conf_cgiirc; e; e = (ConfigItem_cgiirc *)e->next)
	{
		if ((e->type == type) && (!e->username || !match(e->username, username)) &&
		    (!match(e->hostname, hostname) || !match(e->hostname, ip) || !match_ip46(e->hostname, ip)))
			return e;
	}

	return NULL;
}

ConfigItem_ban 	*Find_ban(aClient *sptr, char *host, short type)
{
	ConfigItem_ban *ban;

	/* Check for an except ONLY if we find a ban, makes it
	 * faster since most users will not have a ban so excepts
	 * don't need to be searched -- codemastr
	 */

	for (ban = conf_ban; ban; ban = (ConfigItem_ban *) ban->next)
	{
		if (ban->flag.type == type)
		{
			if (sptr)
			{
				if (match_ip(sptr->ip, host, ban->mask, ban->netmask))
				{
					/* Person got a exception */
					if ((type == CONF_BAN_USER || type == CONF_BAN_IP)
					    && Find_except(sptr, host, CONF_EXCEPT_BAN))
						return NULL;
					return ban;
				}
			}
			else if (!match(ban->mask, host)) /* We don't worry about exceptions */
				return ban;
		}
	}
	return NULL;
}

ConfigItem_ban 	*Find_banEx(aClient *sptr, char *host, short type, short type2)
{
	ConfigItem_ban *ban;

	/* Check for an except ONLY if we find a ban, makes it
	 * faster since most users will not have a ban so excepts
	 * don't need to be searched -- codemastr
	 */

	for (ban = conf_ban; ban; ban = (ConfigItem_ban *) ban->next)
	{
		if ((ban->flag.type == type) && (ban->flag.type2 == type2))
		{
			if (sptr)
			{
				if (match_ip(sptr->ip, host, ban->mask, ban->netmask)) {
					/* Person got a exception */
					if (Find_except(sptr, host, type))
						return NULL;
					return ban;
				}
			}
			else if (!match(ban->mask, host)) /* We don't worry about exceptions */
				return ban;
		}
	}
	return NULL;
}

int	AllowClient(aClient *cptr, struct hostent *hp, char *sockhost, char *username)
{
	ConfigItem_allow *aconf;
	char *hname;
	int  i, ii = 0;
	static char uhost[HOSTLEN + USERLEN + 3];
	static char fullname[HOSTLEN + 1];

	for (aconf = conf_allow; aconf; aconf = (ConfigItem_allow *) aconf->next)
	{
		if (!aconf->hostname || !aconf->ip)
			goto attach;
		if (aconf->auth && !cptr->passwd && aconf->flags.nopasscont)
			continue;
		if (aconf->flags.ssl && !IsSecure(cptr))
			continue;
		if (hp && hp->h_name)
		{
			hname = hp->h_name;
			strncpyzt(fullname, hname, sizeof(fullname));
			add_local_domain(fullname, HOSTLEN - strlen(fullname));
			Debug((DEBUG_DNS, "a_il: %s->%s", sockhost, fullname));
			if (index(aconf->hostname, '@'))
			{
				if (aconf->flags.noident)
					strlcpy(uhost, username, sizeof(uhost));
				else
					strlcpy(uhost, cptr->username, sizeof(uhost));
				strlcat(uhost, "@", sizeof(uhost));
			}
			else
				*uhost = '\0';
			strlcat(uhost, fullname, sizeof(uhost));
			if (!match(aconf->hostname, uhost))
				goto attach;
		}

		if (index(aconf->ip, '@'))
		{
			if (aconf->flags.noident)
				strncpyzt(uhost, username, sizeof(uhost));
			else
				strncpyzt(uhost, cptr->username, sizeof(uhost));
			(void)strlcat(uhost, "@", sizeof(uhost));
		}
		else
			*uhost = '\0';
		strlcat(uhost, sockhost, sizeof(uhost));
		/* Check the IP */
		if (match_ip(cptr->ip, uhost, aconf->ip, aconf->netmask))
			goto attach;

		/* Hmm, localhost is a special case, hp == NULL and sockhost contains
		 * 'localhost' instead of an ip... -- Syzop. */
		if (!strcmp(sockhost, "localhost"))
		{
			if (index(aconf->hostname, '@'))
			{
				if (aconf->flags.noident)
					strcpy(uhost, username);
				else
					strcpy(uhost, cptr->username);
				strcat(uhost, "@localhost");
			}
			else
				strcpy(uhost, "localhost");

			if (!match(aconf->hostname, uhost))
				goto attach;
		}
		
		continue;
	      attach:
/*		if (index(uhost, '@'))  now flag based -- codemastr */
		if (!aconf->flags.noident)
			cptr->flags |= FLAGS_DOID;
		if (!aconf->flags.useip && hp) 
			strncpyzt(uhost, fullname, sizeof(uhost));
		else
			strncpyzt(uhost, sockhost, sizeof(uhost));
		get_sockhost(cptr, uhost);
		/* FIXME */
		if (aconf->maxperip)
		{
			ii = 1;
			for (i = LastSlot; i >= 0; i--)
				if (local[i] && MyClient(local[i]) &&
#ifndef INET6
				    local[i]->ip.S_ADDR == cptr->ip.S_ADDR)
#else
				    !bcmp(local[i]->ip.S_ADDR, cptr->ip.S_ADDR, sizeof(cptr->ip.S_ADDR)))
#endif
				{
					ii++;
					if (ii > aconf->maxperip)
					{
						exit_client(cptr, cptr, &me,
							"Too many connections from your IP");
						return -5;	/* Already got one with that ip# */
					}
				}
		}
		if ((i = Auth_Check(cptr, aconf->auth, cptr->passwd)) == -1)
		{
			exit_client(cptr, cptr, &me,
				"Password mismatch");
			return -5;
		}
		if ((i == 2) && (cptr->passwd))
		{
			MyFree(cptr->passwd);
			cptr->passwd = NULL;
		}
		if (!((aconf->class->clients + 1) > aconf->class->maxclients))
		{
			cptr->class = aconf->class;
			cptr->class->clients++;
		}
		else
		{
			sendto_one(cptr, rpl_str(RPL_REDIR), me.name, cptr->name, aconf->server ? aconf->server : defserv, aconf->port ? aconf->port : 6667);
			return -3;
		}
		return 0;
	}
	return -1;
}

ConfigItem_vhost *Find_vhost(char *name) {
	ConfigItem_vhost *vhost;

	for (vhost = conf_vhost; vhost; vhost = (ConfigItem_vhost *)vhost->next) {
		if (!strcmp(name, vhost->login))
			return vhost;
	}
	return NULL;
}


/** returns NULL if allowed and struct if denied */
ConfigItem_deny_channel *Find_channel_allowed(char *name)
{
	ConfigItem_deny_channel *dchannel;
	ConfigItem_allow_channel *achannel;

	for (dchannel = conf_deny_channel; dchannel; dchannel = (ConfigItem_deny_channel *)dchannel->next)
	{
		if (!match(dchannel->channel, name))
			break;
	}
	if (dchannel)
	{
		for (achannel = conf_allow_channel; achannel; achannel = (ConfigItem_allow_channel *)achannel->next)
		{
			if (!match(achannel->channel, name))
				break;
		}
		if (achannel)
			return NULL;
		else
			return (dchannel);
	}
	return NULL;
}

void init_dynconf(void)
{
	bzero(&iConf, sizeof(iConf));
	bzero(&tempiConf, sizeof(iConf));
}

char *pretty_time_val(long timeval)
{
	static char buf[512];

	if (timeval == 0)
		return "0";

	buf[0] = 0;

	if (timeval/86400)
		sprintf(buf, "%ld day%s ", timeval/86400, timeval/86400 != 1 ? "s" : "");
	if ((timeval/3600) % 24)
		sprintf(buf, "%s%ld hour%s ", buf, (timeval/3600)%24, (timeval/3600)%24 != 1 ? "s" : "");
	if ((timeval/60)%60)
		sprintf(buf, "%s%ld minute%s ", buf, (timeval/60)%60, (timeval/60)%60 != 1 ? "s" : "");
	if ((timeval%60))
		sprintf(buf, "%s%ld second%s", buf, timeval%60, timeval%60 != 1 ? "s" : "");
	return buf;
}

/*
 * Actual config parser funcs
*/

int	_conf_include(ConfigFile *conf, ConfigEntry *ce)
{
	int	ret = 0;
#ifdef GLOBH
	glob_t files;
	int i;
#elif defined(_WIN32)
	HANDLE hFind;
	WIN32_FIND_DATA FindData;
	char cPath[MAX_PATH], *cSlash = NULL, *path;
#endif
	if (!ce->ce_vardata)
	{
		config_status("%s:%i: include: no filename given",
			ce->ce_fileptr->cf_filename,
			ce->ce_varlinenum);
		return -1;
	}
#ifdef USE_LIBCURL
	if (url_is_valid(ce->ce_vardata))
		return remote_include(ce);
#endif
#if !defined(_WIN32) && !defined(_AMIGA) && !defined(OSXTIGER) && DEFAULT_PERMISSIONS != 0
	chmod(ce->ce_vardata, DEFAULT_PERMISSIONS);
#endif
#ifdef GLOBH
#if defined(__OpenBSD__) && defined(GLOB_LIMIT)
	glob(ce->ce_vardata, GLOB_NOSORT|GLOB_NOCHECK|GLOB_LIMIT, NULL, &files);
#else
	glob(ce->ce_vardata, GLOB_NOSORT|GLOB_NOCHECK, NULL, &files);
#endif
	if (!files.gl_pathc) {
		globfree(&files);
		config_status("%s:%i: include %s: invalid file given",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);
		return -1;
	}	
	for (i = 0; i < files.gl_pathc; i++) {
		ret = load_conf(files.gl_pathv[i]);
		if (ret < 0)
		{
			globfree(&files);
			return ret;
		}
		add_include(files.gl_pathv[i]);
	}
	globfree(&files);
#elif defined(_WIN32)
	bzero(cPath,MAX_PATH);
	if (strchr(ce->ce_vardata, '/') || strchr(ce->ce_vardata, '\\')) {
		strncpyzt(cPath,ce->ce_vardata,MAX_PATH);
		cSlash=cPath+strlen(cPath);
		while(*cSlash != '\\' && *cSlash != '/' && cSlash > cPath)
			cSlash--; 
		*(cSlash+1)=0;
	}
	if ( (hFind = FindFirstFile(ce->ce_vardata, &FindData)) == INVALID_HANDLE_VALUE )
	{
		config_status("%s:%i: include %s: invalid file given",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);
		return -1;
	}
	if (cPath) {
		path = MyMalloc(strlen(cPath) + strlen(FindData.cFileName)+1);
		strcpy(path,cPath);
		strcat(path,FindData.cFileName);
		ret = load_conf(path);
		if (ret >= 0)
			add_include(path);
		free(path);

	}
	else
	{
		ret = load_conf(FindData.cFileName);
		if (ret >= 0)
			add_include(FindData.cFileName);
	}
	if (ret < 0)
	{
		FindClose(hFind);
		return ret;
	}

	ret = 0;
	while (FindNextFile(hFind, &FindData) != 0) {
		if (cPath) {
			path = MyMalloc(strlen(cPath) + strlen(FindData.cFileName)+1);
			strcpy(path,cPath);
			strcat(path,FindData.cFileName);
			ret = load_conf(path);
			if (ret >= 0)
			{
				add_include(path);
				free(path);
			}
			else
			{
				free(path);
				break;
			}
		}
		else
		{
			ret = load_conf(FindData.cFileName);
			if (ret >= 0)
				add_include(FindData.cFileName);
		}
	}
	FindClose(hFind);
	if (ret < 0)
		return ret;
#else
	ret = load_conf(ce->ce_vardata);
	if (ret >= 0)
		add_include(ce->ce_vardata);
	return ret;
#endif
	return 1;
}

int	_test_include(ConfigFile *conf, ConfigEntry *ce)
{
	return 0;
}

int	_conf_admin(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_admin *ca;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		ca = MyMallocEx(sizeof(ConfigItem_admin));
		if (!conf_admin)
			conf_admin_tail = ca;
		ircstrdup(ca->line, cep->ce_varname);
		AddListItem(ca, conf_admin);
	}
	return 1;
}

int	_test_admin(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int 	    errors = 0;

	if (requiredstuff.conf_admin)
	{
		config_warn_duplicate(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "admin");
		return 0;
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: blank admin item",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum);
			errors++;
			continue;
		}
	}
	requiredstuff.conf_admin = 1;
	return errors;
}

int	_conf_me(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;

	if (!conf_me)
		conf_me = MyMallocEx(sizeof(ConfigItem_me));

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "name"))
		{
			ircstrdup(conf_me->name, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "info"))
		{
			ircstrdup(conf_me->info, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "numeric"))
		{
			conf_me->numeric = atol(cep->ce_vardata);
		}
	}
	return 1;
}

int	_test_me(ConfigFile *conf, ConfigEntry *ce)
{
	char has_name = 0, has_info = 0, has_numeric = 0;
	ConfigEntry *cep;
	int	    errors = 0;

	if (requiredstuff.conf_me)
	{
		config_warn_duplicate(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "me");
		return 0;
	}
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (config_is_blankorempty(cep, "me"))
			continue;

		/* me::name */
		if (!strcmp(cep->ce_varname, "name"))
		{
			if (has_name)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "me::name");
				continue;
			}
			has_name = 1;
			if (!strchr(cep->ce_vardata, '.'))
			{
				config_error("%s:%i: illegal me::name, must be fully qualified hostname",
					cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum);
				errors++;
			}
			if (!valid_host(cep->ce_vardata))
			{
				config_error("%s:%i: illegal me::name contains invalid character(s) [only a-z, 0-9, _, -, . are allowed]",
					cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum);
				errors++;
			}
			if (strlen(cep->ce_vardata) > HOSTLEN)
			{
				config_error("%s:%i: illegal me::name, must be less or equal to %i characters",
					cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, HOSTLEN);
				errors++;
			}
		}
		/* me::info */
		else if (!strcmp(cep->ce_varname, "info"))
		{
			char *p;
			char valid = 0;
			if (has_info)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "me::info");
				continue;
			}
			has_info = 1;
			if (strlen(cep->ce_vardata) > (REALLEN-1))
			{
				config_error("%s:%i: too long me::info, must be max. %i characters",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, 
					REALLEN-1);
				errors++;
			}
		
			/* Valid me::info? Any data except spaces is ok */
			for (p=cep->ce_vardata; *p; p++)
			{
				if (*p != ' ')
				{
					valid = 1;
					break;
				}
			}
			if (!valid)
			{
				config_error("%s:%i: empty me::info, should be a server description.",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		/* me::numeric */
		else if (!strcmp(cep->ce_varname, "numeric"))
		{
			long l;

			has_numeric = 1;
			l = atol(cep->ce_vardata);
			if ((l < 0) || (l > 254))
			{
				config_error("%s:%i: illegal me::numeric error (must be between 0 and 254)",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum);
				errors++;
			}
		}
		/* Unknown entry */
		else
		{
			config_error_unknown(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, 
				"me", cep->ce_varname);
			errors++;
		}
	}
	if (!has_name)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "me::name");
		errors++;
	}
	if (!has_info)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "me::info");
		errors++;
	}
	if (!has_numeric)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, 
			"me::numeric");
		errors++;
	}
	requiredstuff.conf_me = 1;
	return errors;
}

/*
 * The oper {} block parser
*/

int	_conf_oper(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	ConfigItem_oper *oper = NULL;
	ConfigItem_oper_from *from;
	OperFlag *ofp = NULL;
	struct irc_netmask tmp;

	oper =  MyMallocEx(sizeof(ConfigItem_oper));
	oper->name = strdup(ce->ce_vardata);
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "password"))
			oper->auth = Auth_ConvertConf2AuthStruct(cep);
		else if (!strcmp(cep->ce_varname, "class"))
		{
			oper->class = Find_class(cep->ce_vardata);
			if (!oper->class || (oper->class->flag.temporary == 1))
			{
				config_status("%s:%i: illegal oper::class, unknown class '%s' using default of class 'default'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_vardata);
				oper->class = default_class;
			}
		}
		else if (!strcmp(cep->ce_varname, "flags"))
		{
			if (!cep->ce_entries)
			{
				char *m = "*";
				int *i, flag;

				for (m = (*cep->ce_vardata) ? cep->ce_vardata : m; *m; m++) 
				{
					for (i = _OldOperFlags; (flag = *i); i += 2)
						if (*m == (char)(*(i + 1))) 
						{
							oper->oflags |= flag;
							break;
						}
				}
			}
			else
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if ((ofp = config_binary_flags_search(_OperFlags, cepp->ce_varname, ARRAY_SIZEOF(_OperFlags)))) 
						oper->oflags |= ofp->flag;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "swhois"))
		{
			ircstrdup(oper->swhois, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "snomask"))
		{
			ircstrdup(oper->snomask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "modes"))
		{
			oper->modes = set_usermode(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "maxlogins"))
		{
			oper->maxlogins = atoi(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "from"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "userhost"))
				{
					from = MyMallocEx(sizeof(ConfigItem_oper_from));
					ircstrdup(from->name, cepp->ce_vardata);
					tmp.type = parse_netmask(from->name, &tmp);
					if (tmp.type != HM_HOST)
					{
						from->netmask = MyMallocEx(sizeof(struct irc_netmask));
						bcopy(&tmp, from->netmask, sizeof(struct irc_netmask));
					}
					AddListItem(from, oper->from);
				}
			}
		}
	}
	AddListItem(oper, conf_oper);
	return 1;
}

int	_test_oper(ConfigFile *conf, ConfigEntry *ce)
{
	char has_class = 0, has_password = 0, has_flags = 0, has_swhois = 0, has_snomask = 0;
	char has_modes = 0, has_from = 0, has_maxlogins = 0;
	int oper_flags = 0;
	ConfigEntry *cep;
	ConfigEntry *cepp;
	OperFlag *ofp;
	int	errors = 0;

	if (!ce->ce_vardata)
	{
		config_error_noname(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "oper");
		errors++;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"oper");
			errors++; 
			continue;
		}
		if (!strcmp(cep->ce_varname, "password"))
		{
			if (has_password)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "oper::password");
				continue;
			}
			has_password = 1;
			if (Auth_CheckError(cep) < 0)
				errors++;
			continue;
		}
		/* Regular variables */
		if (!cep->ce_entries)
		{
			if (config_is_blankorempty(cep, "oper"))
			{
				errors++; 
				continue;
			}
			/* oper::class */
			if (!strcmp(cep->ce_varname, "class"))
			{
				if (has_class)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename, 
						cep->ce_varlinenum, "oper::class");
					continue;
				}
				has_class = 1;
			}
			/* oper::swhois */
			else if (!strcmp(cep->ce_varname, "swhois")) 
			{
				if (has_swhois)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "oper::swhois");
					continue;
				}
				has_swhois = 1;
			}
			/* oper::snomask */
			else if (!strcmp(cep->ce_varname, "snomask")) 
			{
				if (has_snomask)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "oper::snomask");
					continue;
				}
				has_snomask = 1;
			}
			/* oper::modes */
			else if (!strcmp(cep->ce_varname, "modes")) 
			{
				char *p;
				for (p = cep->ce_vardata; *p; p++)
					if (strchr("oOaANCrzS", *p))
					{
						config_error("%s:%i: oper::modes may not include mode '%c'",
							cep->ce_fileptr->cf_filename, cep->ce_varlinenum, *p);
						errors++;
					}
				if (has_modes)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "oper::modes");
					continue;
				}
				has_modes = 1;
			}
			/* oper::maxlogins */
			else if (!strcmp(cep->ce_varname, "maxlogins"))
			{
				int l;

				if (has_maxlogins)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "oper::maxlogins");
					continue;
				}
				has_maxlogins = 1;

				l = atoi(cep->ce_vardata);
				if ((l < 0) || (l > 5000))
				{
					config_error("%s:%i: oper::maxlogins: value out of range (%d) should be 0-5000",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, l);
					errors++; 
					continue;
				}
			}
			/* oper::flags */
			else if (!strcmp(cep->ce_varname, "flags"))
			{
				if (has_flags)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "oper::flags");
					continue;
				}
				has_flags = 1;
			}
			else
			{
				config_error_unknown(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "oper", cep->ce_varname);
				errors++;
				continue;
			}
		}
		/* Sections */
		else
		{
			/* oper::flags {} */
			if (!strcmp(cep->ce_varname, "flags"))
			{
				if (has_flags)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "oper::flags");
					continue;
				}
				has_flags = 1;
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (!cepp->ce_varname)
					{
						config_error_empty(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, "oper::flags",
							cep->ce_varname);
						errors++; 
						continue;
					}
					if (!(ofp = config_binary_flags_search(_OperFlags, cepp->ce_varname, ARRAY_SIZEOF(_OperFlags)))) {
						config_error_unknownflag(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, "oper",
							cepp->ce_varname);
						errors++; 
					} else
						oper_flags |= ofp->flag;
				}
				continue;
			}
			/* oper::from {} */
			else if (!strcmp(cep->ce_varname, "from"))
			{
				if (has_from)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "oper::from");
					continue;
				}
				has_from = 1;
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (config_is_blankorempty(cepp, "oper::from"))
					{
						errors++;
						continue;
					}
					/* Unknown Entry */
					if (strcmp(cepp->ce_varname, "userhost"))
					{
						config_error_unknown(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, "oper::from",
							cepp->ce_varname);
						errors++;
						continue;
					}
				}
				continue;
			}
			else
			{
				config_error_unknown(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "oper", cep->ce_varname);
				errors++; 
				continue;
			}
		}
	}
	if (!has_password)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"oper::password");
		errors++;
	}	
	if (!has_from)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"oper::from");
		errors++;
	}	
	if (!has_flags)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"oper::flags");
		errors++;
	} else {
		/* Check oper flags -- warning needed only (autoconvert) */
		if (!(oper_flags & (OFLAG_GROUTE|OFLAG_GKILL|OFLAG_GNOTICE)) &&
		    (oper_flags & (OFLAG_GZL|OFLAG_TKL|OFLAG_OVERRIDE)))
		{
			config_warn("%s:%i: oper::oflags: can_gzline/can_gkline/can_override (global privileges) "
			            "are incompatible with local oper -- user will be globop",
			            ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		}
	}
	if (!has_class)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"oper::class");
		errors++;
	}
	return errors;
	
}

/*
 * The class {} block parser
*/
int	_conf_class(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cep2;
	ConfigItem_class *class;
	unsigned char isnew = 0;

	if (!(class = Find_class(ce->ce_vardata)))
	{
		class = MyMallocEx(sizeof(ConfigItem_class));
		ircstrdup(class->name, ce->ce_vardata);
		isnew = 1;
	}
	else
	{
		isnew = 0;
		class->flag.temporary = 0;
		class->options = 0; /* RESET OPTIONS */
	}
	ircstrdup(class->name, ce->ce_vardata);

	class->connfreq = 60; /* default */

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "pingfreq"))
			class->pingfreq = atol(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "connfreq"))
			class->connfreq = atol(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "maxclients"))
			class->maxclients = atol(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "sendq"))
			class->sendq = atol(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "recvq"))
			class->recvq = atol(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "options"))
		{
			for (cep2 = cep->ce_entries; cep2; cep2 = cep2->ce_next)
				if (!strcmp(cep2->ce_varname, "nofakelag"))
					class->options |= CLASS_OPT_NOFAKELAG;
		}
	}
	if (isnew)
		AddListItem(class, conf_class);
	return 1;
}

int	_test_class(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry 	*cep, *cep2;
	int		errors = 0;
	char has_pingfreq = 0, has_connfreq = 0, has_maxclients = 0, has_sendq = 0;
	char has_recvq = 0;

	if (!ce->ce_vardata)
	{
		config_error_noname(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "class");
		errors++;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "options"))
		{
			for (cep2 = cep->ce_entries; cep2; cep2 = cep2->ce_next)
			{
#ifdef FAKELAG_CONFIGURABLE
				if (!strcmp(cep2->ce_varname, "nofakelag"))
					;
				else
#endif
				{
					config_error("%s:%d: Unknown option '%s' in class::options",
						cep2->ce_fileptr->cf_filename, cep2->ce_varlinenum, cep2->ce_varname);
					errors++;
				}
			}
		}
		else if (config_is_blankorempty(cep, "class"))
		{
			errors++;
			continue;
		}
		/* class::pingfreq */
		else if (!strcmp(cep->ce_varname, "pingfreq"))
		{
			int v = atol(cep->ce_vardata);
			if (has_pingfreq)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "class::pingfreq");
				continue;
			}
			has_pingfreq = 1;
			if ((v < 30) || (v > 600))
			{
				config_error("%s:%i: class::pingfreq should be a reasonable value (30-600)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				continue;
			}
		} 
		/* class::maxclients */
		else if (!strcmp(cep->ce_varname, "maxclients"))
		{
			long l;
			if (has_maxclients)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "class::maxclients");
				continue;
			}
			has_maxclients = 1;
			l = atol(cep->ce_vardata);
			if ((l < 1) || (l > 1000000))
			{
				config_error("%s:%i: class::maxclients with illegal value",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		/* class::connfreq */
		else if (!strcmp(cep->ce_varname, "connfreq"))
		{
			long l;
			if (has_connfreq)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "class::connfreq");
				continue;
			}
			has_connfreq = 1;
			l = atol(cep->ce_vardata);
			if ((l < 10) || (l > 604800))
			{
				config_error("%s:%i: class::connfreq with illegal value (must be >10 and <7d)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		/* class::sendq */
		else if (!strcmp(cep->ce_varname, "sendq"))
		{
			long l;
			if (has_sendq)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "class::sendq");
				continue;
			}
			has_sendq = 1;
			l = atol(cep->ce_vardata);
			if ((l < 0) || (l > 2000000000))
			{
				config_error("%s:%i: class::sendq with illegal value",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		/* class::recvq */
		else if (!strcmp(cep->ce_varname, "recvq"))
		{
			long l;
			if (has_recvq)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "class::recvq");
				continue;
			}
			has_recvq = 1;
			l = atol(cep->ce_vardata);
			if ((l < 512) || (l > 32768))
			{
				config_error("%s:%i: class::recvq with illegal value (must be >512 and <32k)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		/* Unknown */
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"class", cep->ce_varname);
			errors++;
			continue;
		}
	}
	if (!has_pingfreq)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"class::pingfreq");
		errors++;
	}
	if (!has_maxclients)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"class::maxclients");
		errors++;
	}
	if (!has_sendq)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"class::sendq");
		errors++;
	}
	
	return errors;
}

int     _conf_drpass(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;

	if (!conf_drpass) 
	{
		conf_drpass =  MyMallocEx(sizeof(ConfigItem_drpass));
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "restart"))
		{
			if (conf_drpass->restartauth)
				Auth_DeleteAuthStruct(conf_drpass->restartauth);
			
			conf_drpass->restartauth = Auth_ConvertConf2AuthStruct(cep);
		}
		else if (!strcmp(cep->ce_varname, "die"))
		{
			if (conf_drpass->dieauth)
				Auth_DeleteAuthStruct(conf_drpass->dieauth);
			
			conf_drpass->dieauth = Auth_ConvertConf2AuthStruct(cep);
		}
	}
	return 1;
}

int     _test_drpass(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int errors = 0;
	char has_restart = 0, has_die = 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (config_is_blankorempty(cep, "drpass"))
		{
			errors++;
			continue;
		}
		/* drpass::restart */
		if (!strcmp(cep->ce_varname, "restart"))
		{
			if (has_restart)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "drpass::restart");
				continue;
			}
			has_restart = 1;
			if (Auth_CheckError(cep) < 0)
				errors++;
			continue;
		}
		/* drpass::die */
		else if (!strcmp(cep->ce_varname, "die"))
		{
			if (has_die)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "drpass::die");
				continue;
			}
			has_die = 1;
			if (Auth_CheckError(cep) < 0)
				errors++;
			continue;
		}
		/* Unknown */
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"drpass", cep->ce_varname);
			errors++;
			continue;
		}
	}
	return errors;
}

/*
 * The ulines {} block parser
*/
int	_conf_ulines(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_ulines *ca;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		ca = MyMallocEx(sizeof(ConfigItem_ulines));
		ircstrdup(ca->servername, cep->ce_varname);
		AddListItem(ca, conf_ulines);
	}
	return 1;
}

int	_test_ulines(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int 	    errors = 0;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, "ulines");
			errors++;
			continue;
		}
	}
	return errors;
}

int     _conf_tld(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_tld *ca;

	ca = MyMallocEx(sizeof(ConfigItem_tld));

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask"))
			ca->mask = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "motd"))
		{
			ca->motd_file = strdup(cep->ce_vardata);
			ca->motd = read_file_ex(cep->ce_vardata, NULL, &ca->motd_tm);
		}
		else if (!strcmp(cep->ce_varname, "shortmotd"))
		{
			ca->smotd_file = strdup(cep->ce_vardata);
			ca->smotd = read_file_ex(cep->ce_vardata, NULL, &ca->smotd_tm);
		}
		else if (!strcmp(cep->ce_varname, "opermotd"))
		{
			ca->opermotd_file = strdup(cep->ce_vardata);
			ca->opermotd = read_file(cep->ce_vardata, NULL);
		}
		else if (!strcmp(cep->ce_varname, "botmotd"))
		{
			ca->botmotd_file = strdup(cep->ce_vardata);
			ca->botmotd = read_file(cep->ce_vardata, NULL);
		}
		else if (!strcmp(cep->ce_varname, "rules"))
		{
			ca->rules_file = strdup(cep->ce_vardata);
			ca->rules = read_file(cep->ce_vardata, NULL);
		}
		else if (!strcmp(cep->ce_varname, "options"))
		{
			ConfigEntry *cepp;
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "ssl"))
					ca->options |= TLD_SSL;
				else if (!strcmp(cepp->ce_varname, "remote"))
					ca->options |= TLD_REMOTE;
			}
		}
		else if (!strcmp(cep->ce_varname, "channel"))
			ca->channel = strdup(cep->ce_vardata);
	}
	AddListItem(ca, conf_tld);
	return 1;
}

int     _test_tld(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int	    errors = 0;
	int	    fd = -1;
	char has_mask = 0, has_motd = 0, has_rules = 0, has_shortmotd = 0, has_channel = 0;
	char has_opermotd = 0, has_botmotd = 0, has_options = 0;

        for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"tld");
			errors++;
			continue;
		}
		if (!cep->ce_vardata && strcmp(cep->ce_varname, "options"))
		{
			config_error_empty(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"tld", cep->ce_varname);
			errors++;
			continue;
		}
		/* tld::mask */
		if (!strcmp(cep->ce_varname, "mask")) 
		{
			if (has_mask)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "tld::mask");
				continue;
			}
			has_mask = 1;
		}
		/* tld::motd */
		else if (!strcmp(cep->ce_varname, "motd")) 
		{
			if (has_motd)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "tld::motd");
				continue;
			}
			has_motd = 1;
			if (((fd = open(cep->ce_vardata, O_RDONLY)) == -1))
			{
				config_error("%s:%i: tld::motd: %s: %s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_vardata, strerror(errno));
				errors++;
			}
			else
				close(fd);
		}
		/* tld::rules */
		else if (!strcmp(cep->ce_varname, "rules"))
		{
			if (has_rules)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "tld::rules");
				continue;
			}
			has_rules = 1;
			if (((fd = open(cep->ce_vardata, O_RDONLY)) == -1))
			{
				config_error("%s:%i: tld::rules: %s: %s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_vardata, strerror(errno));
				errors++;
			}
			else
				close(fd);
		}
		/* tld::channel */
		else if (!strcmp(cep->ce_varname, "channel"))
		{
			if (has_channel)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "tld::channel");
				continue;
			}
			has_channel = 1;
		}
		/* tld::shortmotd */
		else if (!strcmp(cep->ce_varname, "shortmotd"))
		{
			if (has_shortmotd)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "tld::shortmotd");
				continue;
			}
			has_shortmotd = 1;
			if (((fd = open(cep->ce_vardata, O_RDONLY)) == -1))
			{
				config_error("%s:%i: tld::shortmotd: %s: %s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_vardata, strerror(errno));
				errors++;
			}
			else
				close(fd);
		}
		/* tld::opermotd */
		else if (!strcmp(cep->ce_varname, "opermotd"))
		{
			if (has_opermotd)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "tld::opermotd");
				continue;
			}
			has_opermotd = 1;
			if (((fd = open(cep->ce_vardata, O_RDONLY)) == -1))
			{
				config_error("%s:%i: tld::opermotd: %s: %s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_vardata, strerror(errno));
				errors++;
			}
			else
				close(fd);
		}
		/* tld::botmotd */
		else if (!strcmp(cep->ce_varname, "botmotd"))
		{
			if (has_botmotd)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "tld::botmotd");
				continue;
			}
			has_botmotd = 1;
			if (((fd = open(cep->ce_vardata, O_RDONLY)) == -1))
			{
				config_error("%s:%i: tld::botmotd: %s: %s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_vardata, strerror(errno));
				errors++;
			}
			else
				close(fd);
		}
		/* tld::options */
		else if (!strcmp(cep->ce_varname, "options")) {
			ConfigEntry *cep2;

			if (has_options)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "tld::options");
				continue;
			}
			has_options = 1;

			for (cep2 = cep->ce_entries; cep2; cep2 = cep2->ce_next)
			{
				if (!cep2->ce_varname)
				{
					config_error_blank(cep2->ce_fileptr->cf_filename,
						cep2->ce_varlinenum, "tld::options");
					continue;
				}
				if (strcmp(cep2->ce_varname, "ssl") && 
					strcmp(cep2->ce_varname, "remote")) 
				{
					config_error_unknownopt(cep2->ce_fileptr->cf_filename,
						cep2->ce_varlinenum, "tld", cep2->ce_varname);
					errors++;
				}
			}
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"tld", cep->ce_varname);
			errors++;
			continue;
		}
	}
	if (!has_mask)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"tld::mask");
		errors++;
	}
	if (!has_motd)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"tld::motd");
		errors++;
	}
	if (!has_rules)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"tld::rules");
		errors++;
	}
	return errors;
}

int	_conf_listen(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	ConfigItem_listen *listen = NULL;
	OperFlag    *ofp;
	char	    copy[256];
	char	    *ip;
	char	    *port;
	int	    start, end, iport, isnew;
	int tmpflags =0;

	strcpy(copy, ce->ce_vardata);
	/* Seriously cheap hack to make listen <port> work -Stskeeps */
	ipport_seperate(copy, &ip, &port);
	if (!ip || !*ip)
	{
		return -1;
	}
	if (strchr(ip, '*') && strcmp(ip, "*"))
	{
		return -1;
	}
	if (!port || !*port)
	{
		return -1;
	}
	port_range(port, &start, &end);
	if ((start < 0) || (start > 65535) || (end < 0) || (end > 65535))
	{
		return -1;
	}
	end++;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "options"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if ((ofp = config_binary_flags_search(_ListenerFlags, cepp->ce_varname, ARRAY_SIZEOF(_ListenerFlags))))
					tmpflags |= ofp->flag;
			}
		}
	}
#ifndef USE_SSL
	tmpflags &= ~LISTENER_SSL;
#endif
	for (iport = start; iport < end; iport++)
	{
		if (!(listen = Find_listen(ip, iport)))
		{
			listen = MyMallocEx(sizeof(ConfigItem_listen));
			listen->ip = strdup(ip);
			listen->port = iport;
			isnew = 1;
		} else
			isnew = 0;

		if (listen->options & LISTENER_BOUND)
			tmpflags |= LISTENER_BOUND;

		listen->options = tmpflags;
		if (isnew)
			AddListItem(listen, conf_listen);
		listen->flag.temporary = 0;
	}
	return 1;
}

int	_test_listen(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	char	    copy[256];
	char	    *ip;
	char	    *port;
	int	    start, end;
	int	    errors = 0;
	char has_options = 0;
	OperFlag    *ofp;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: listen without ip:port",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}

	strcpy(copy, ce->ce_vardata);
	/* Seriously cheap hack to make listen <port> work -Stskeeps */
	ipport_seperate(copy, &ip, &port);
	if (!ip || !*ip)
	{
		config_error("%s:%i: listen: illegal ip:port mask",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	if (strchr(ip, '*') && strcmp(ip, "*"))
	{
		config_error("%s:%i: listen: illegal ip, (mask, and not '*')",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	if (!port || !*port)
	{
		config_error("%s:%i: listen: missing port in mask",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
#ifdef INET6
	if ((strlen(ip) > 6) && !strchr(ip, ':') && isdigit(ip[strlen(ip)-1]))
	{
		char crap[32];
		if (inet_pton(AF_INET, ip, crap) != 0)
		{
			char ipv6buf[128];
			snprintf(ipv6buf, sizeof(ipv6buf), "[::ffff:%s]:%s", ip, port);
			ce->ce_vardata = strdup(ipv6buf);
		} else {
		/* Insert IPv6 validation here */
			config_error("%s:%i: listen: '%s' looks like it might be IPv4, but is not a valid address.",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum, ip);
			return 1;
		}
	}
#endif
	port_range(port, &start, &end);
	if (start == end)
	{
		if ((start < 0) || (start > 65535))
		{
			config_error("%s:%i: listen: illegal port (must be 0..65535)",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return 1;
		}
	}
	else 
	{
		if (end < start)
		{
			config_error("%s:%i: listen: illegal port range end value is less than starting value",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return 1;
		}
		if (end - start >= 100)
		{
			config_error("%s:%i: listen: you requested port %d-%d, that's %d ports "
				"(and thus consumes %d sockets) this is probably not what you want.",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum, start, end,
				end - start + 1, end - start + 1);
			return 1;
		}
		if ((start < 0) || (start > 65535) || (end < 0) || (end > 65535))
		{
			config_error("%s:%i: listen: illegal port range values must be between 0 and 65535",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return 1;
		}
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"listen");
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "options"))
		{
			if (has_options)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "listen::options");
				continue;
			}
			has_options = 1;
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!cepp->ce_varname)
				{
					config_error_blank(cepp->ce_fileptr->cf_filename, 
						cepp->ce_varlinenum, "listen::options");
					errors++;
					continue;
				}
				if (!(ofp = config_binary_flags_search(_ListenerFlags, cepp->ce_varname, ARRAY_SIZEOF(_ListenerFlags))))
				{
					config_error_unknownopt(cepp->ce_fileptr->cf_filename, 
						cepp->ce_varlinenum, "class", cepp->ce_varname);
					errors++;
					continue;
				}
#ifndef USE_SSL
				else if (ofp->flag & LISTENER_SSL)
				{
					config_warn("%s:%i: listen with SSL flag enabled on a non SSL compile",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				}
#endif
			}
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"listen", cep->ce_varname);
			errors++;
			continue;
		}

	}
	requiredstuff.conf_listen = 1;
	return errors;
}


int	_conf_allow(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	ConfigItem_allow *allow;
	Hook *h;
	struct irc_netmask tmp;
	if (ce->ce_vardata)
	{
		if (!strcmp(ce->ce_vardata, "channel"))
			return (_conf_allow_channel(conf, ce));
		else if (!strcmp(ce->ce_vardata, "dcc"))
			return (_conf_allow_dcc(conf, ce));
		else
		{
			int value;
			for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
			{
				value = (*(h->func.intfunc))(conf,ce,CONFIG_ALLOW);
				if (value == 1)
					break;
			}
			return 0;
		}
	}
	allow = MyMallocEx(sizeof(ConfigItem_allow));
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "ip"))
		{
			allow->ip = strdup(cep->ce_vardata);
			/* CIDR */
			tmp.type = parse_netmask(allow->ip, &tmp);
			if (tmp.type != HM_HOST)
			{
				allow->netmask = MyMallocEx(sizeof(struct irc_netmask));
				bcopy(&tmp, allow->netmask, sizeof(struct irc_netmask));
			}
		}
		else if (!strcmp(cep->ce_varname, "hostname"))
			allow->hostname = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "password"))
			allow->auth = Auth_ConvertConf2AuthStruct(cep);
		else if (!strcmp(cep->ce_varname, "class"))
		{
			allow->class = Find_class(cep->ce_vardata);
			if (!allow->class || (allow->class->flag.temporary == 1))
			{
				config_status("%s:%i: illegal allow::class, unknown class '%s' using default of class 'default'",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum,
					cep->ce_vardata);
					allow->class = default_class;
			}
		}
		else if (!strcmp(cep->ce_varname, "maxperip"))
			allow->maxperip = atoi(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "redirect-server"))
			allow->server = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "redirect-port"))
			allow->port = atoi(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "options"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) 
			{
				if (!strcmp(cepp->ce_varname, "noident"))
					allow->flags.noident = 1;
				else if (!strcmp(cepp->ce_varname, "useip")) 
					allow->flags.useip = 1;
				else if (!strcmp(cepp->ce_varname, "ssl")) 
					allow->flags.ssl = 1;
				else if (!strcmp(cepp->ce_varname, "nopasscont")) 
					allow->flags.nopasscont = 1;
			}
		}
	}
	AddListItem(allow, conf_allow);
	return 1;
}

int	_test_allow(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	int		errors = 0;
	Hook *h;
	char has_ip = 0, has_hostname = 0, has_maxperip = 0, has_password = 0, has_class = 0;
	char has_redirectserver = 0, has_redirectport = 0, has_options = 0;
	
	if (ce->ce_vardata)
	{
		if (!strcmp(ce->ce_vardata, "channel"))
			return (_test_allow_channel(conf, ce));
		else if (!strcmp(ce->ce_vardata, "dcc"))
			return (_test_allow_dcc(conf, ce));
		else
		{
			int used = 0;
			for (h = Hooks[HOOKTYPE_CONFIGTEST]; h; h = h->next) 
			{
				int value, errs = 0;
				if (h->owner && !(h->owner->flags & MODFLAG_TESTING)
				    && !(h->owner->options & MOD_OPT_PERM))
					continue;
				value = (*(h->func.intfunc))(conf,ce,CONFIG_ALLOW,&errs);
				if (value == 2)
					used = 1;
				if (value == 1)
				{
					used = 1;
					break;
				}
				if (value == -1)
				{
					used = 1;
					errors += errs;
					break;
				}
				if (value == -2)
				{
					used = 1;
					errors += errs;
				}
			}
			if (!used) {
				config_error("%s:%i: allow item with unknown type",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
				return 1;
			}
			return errors;
		}
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (strcmp(cep->ce_varname, "options") && config_is_blankorempty(cep, "allow"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "ip"))
		{
			if (has_ip)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "allow::ip");
				continue;
			}
			has_ip = 1;
		} 
		else if (!strcmp(cep->ce_varname, "maxperip"))
		{
			int v = atoi(cep->ce_vardata);
			if (has_maxperip)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "allow::maxperip");
				continue;
			}
			has_maxperip = 1;
			if ((v <= 0) || (v > 65535))
			{
				config_error("%s:%i: allow::maxperip with illegal value (must be 1-65535)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "hostname"))
		{
			if (has_hostname)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "allow::hostname");
				continue;
			}
			has_hostname = 1;
		} 
		else if (!strcmp(cep->ce_varname, "password"))
		{
			if (has_password)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "allow::password");
				continue;
			}
			has_password = 1;
			/* some auth check stuff? */
			if (Auth_CheckError(cep) < 0)
				errors++;
		}
		else if (!strcmp(cep->ce_varname, "class"))
		{
			if (has_class)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "allow::class");
				continue;
			}
			has_class = 1;
		}
		else if (!strcmp(cep->ce_varname, "redirect-server"))
		{
			if (has_redirectserver)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "allow::redirect-server");
				continue;
			}
			has_redirectserver = 1;
		}
		else if (!strcmp(cep->ce_varname, "redirect-port"))
		{
			if (has_redirectport)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "allow::redirect-port");
				continue;
			}
			has_redirectport = 1;
		}
		else if (!strcmp(cep->ce_varname, "options"))
		{
			if (has_options)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "allow::options");
				continue;
			}
			has_options = 1;
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "noident"))
				{}
				else if (!strcmp(cepp->ce_varname, "useip")) 
				{}
				else if (!strcmp(cepp->ce_varname, "ssl")) 
				{}
				else if (!strcmp(cepp->ce_varname, "nopasscont")) 
				{}
				else
				{
					config_error_unknownopt(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "allow", cepp->ce_varname);
					errors++;
				}
			}
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"allow", cep->ce_varname);
			errors++;
			continue;
		}
	}
	if (!has_ip)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"allow::ip");
		errors++;
	}
	if (!has_hostname)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"allow::hostname");
		errors++;
	}
	if (!has_class)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"allow::class");
		errors++;
	}
	return errors;
}

int	_conf_allow_channel(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_allow_channel 	*allow = NULL;
	ConfigEntry 	    	*cep;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "channel"))
		{
			allow = MyMallocEx(sizeof(ConfigItem_allow_channel));
			ircstrdup(allow->channel, cep->ce_vardata);
			AddListItem(allow, conf_allow_channel);
		}
	}
	return 1;
}

int	_test_allow_channel(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry		*cep;
	int			errors = 0;
	char			has_channel = 0;	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (config_is_blankorempty(cep, "allow channel"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "channel"))
			has_channel = 1;
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"allow channel", cep->ce_varname);
			errors++;
		}
	}
	if (!has_channel)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"allow channel::channel");
		errors++;
	}
	return errors;
}

int	_conf_allow_dcc(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_allow_dcc *allow = NULL;
	ConfigEntry *cep;

	allow = MyMallocEx(sizeof(ConfigItem_allow_dcc));
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "filename"))
			ircstrdup(allow->filename, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "soft"))
		{
			int x = config_checkval(cep->ce_vardata,CFG_YESNO);
			if (x)
				allow->flag.type = DCCDENY_SOFT;
		}
	}
	AddListItem(allow, conf_allow_dcc);
	return 1;
}

int	_test_allow_dcc(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int errors = 0, has_filename = 0, has_soft = 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (config_is_blankorempty(cep, "allow dcc"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "filename"))
		{
			if (has_filename)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "allow dcc::filename");
				continue;
			}				
			has_filename = 1;
		}
		else if (!strcmp(cep->ce_varname, "soft"))
		{
			if (has_soft)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "allow dcc::soft");
				continue;
			}
			has_soft = 1;
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"allow dcc", cep->ce_varname);
			errors++;
		}
	}
	if (!has_filename)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"allow dcc::filename");
		errors++;
	}
	return errors;
}

void create_tkl_except(char *mask, char *type)
{
	ConfigItem_except *ca;
	struct irc_netmask tmp;
	OperFlag *opf;
	ca = MyMallocEx(sizeof(ConfigItem_except));
	ca->mask = strdup(mask);
	
	opf = config_binary_flags_search(ExceptTklFlags, type, ARRAY_SIZEOF(ExceptTklFlags));
	ca->type = opf->flag;
	
	if (ca->type & TKL_KILL || ca->type & TKL_ZAP || ca->type & TKL_SHUN)
	{
		tmp.type = parse_netmask(ca->mask, &tmp);
		if (tmp.type != HM_HOST)
		{
			ca->netmask = MyMallocEx(sizeof(struct irc_netmask));
			bcopy(&tmp, ca->netmask, sizeof(struct irc_netmask));
		}
	}
	ca->flag.type = CONF_EXCEPT_TKL;
	AddListItem(ca, conf_except);
}

int     _conf_except(ConfigFile *conf, ConfigEntry *ce)
{

	ConfigEntry *cep;
	ConfigItem_except *ca;
	Hook *h;
	struct irc_netmask tmp;

	if (!strcmp(ce->ce_vardata, "ban")) {
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "mask")) {
				ca = MyMallocEx(sizeof(ConfigItem_except));
				ca->mask = strdup(cep->ce_vardata);
				tmp.type = parse_netmask(ca->mask, &tmp);
				if (tmp.type != HM_HOST)
				{
					ca->netmask = MyMallocEx(sizeof(struct irc_netmask));
					bcopy(&tmp, ca->netmask, sizeof(struct irc_netmask));
				}
				ca->flag.type = CONF_EXCEPT_BAN;
				AddListItem(ca, conf_except);
			}
			else {
			}
		}
	}
#ifdef THROTTLING
	else if (!strcmp(ce->ce_vardata, "throttle")) {
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "mask")) {
				ca = MyMallocEx(sizeof(ConfigItem_except));
				ca->mask = strdup(cep->ce_vardata);
				tmp.type = parse_netmask(ca->mask, &tmp);
				if (tmp.type != HM_HOST)
				{
					ca->netmask = MyMallocEx(sizeof(struct irc_netmask));
					bcopy(&tmp, ca->netmask, sizeof(struct irc_netmask));
				}
				ca->flag.type = CONF_EXCEPT_THROTTLE;
				AddListItem(ca, conf_except);
			}
			else {
			}
		}

	}
#endif
	else if (!strcmp(ce->ce_vardata, "tkl")) {
		ConfigEntry *mask = NULL, *type = NULL;
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "mask"))
				mask = cep;
			else if (!strcmp(cep->ce_varname, "type"))
				type = cep;
		}
		if (type->ce_vardata)
			create_tkl_except(mask->ce_vardata, type->ce_vardata);
		else
		{
			ConfigEntry *cepp;
			for (cepp = type->ce_entries; cepp; cepp = cepp->ce_next)
				create_tkl_except(mask->ce_vardata, cepp->ce_varname);
		}
	}
	else {
		int value;
		for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
		{
			value = (*(h->func.intfunc))(conf,ce,CONFIG_EXCEPT);
			if (value == 1)
				break;
		}
	}
	return 1;
}

int     _test_except(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int	    errors = 0;
	Hook *h;
	char has_mask = 0;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: except without type",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}

	if (!strcmp(ce->ce_vardata, "ban")) 
	{
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (config_is_blankorempty(cep, "except ban"))
			{
				errors++;
				continue;
			}
			if (!strcmp(cep->ce_varname, "mask"))
			{
				if (has_mask)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename, 
						cep->ce_varlinenum, "except ban::mask");
					continue;
				}
				has_mask = 1;
			}
			else
			{
				config_error_unknown(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "except ban", cep->ce_varname);
				errors++;
				continue;
			}
		}
		if (!has_mask)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"except ban::mask");
			errors++;
		}
		return errors;
	}
#ifdef THROTTLING
	else if (!strcmp(ce->ce_vardata, "throttle")) {
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (config_is_blankorempty(cep, "except throttle"))
			{
				errors++;
				continue;
			}
			if (!strcmp(cep->ce_varname, "mask"))
			{
				if (has_mask)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename, 
						cep->ce_varlinenum, "except throttle::mask");
					continue;
				}
				has_mask = 1;
			}
			else
			{
				config_error_unknown(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "except throttle", cep->ce_varname);
				errors++;
				continue;
			}
		}
		if (!has_mask)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"except throttle::mask");
			errors++;
		}
		return errors;
	}
#endif
	else if (!strcmp(ce->ce_vardata, "tkl")) {
		char has_type = 0;

		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!cep->ce_varname)
			{
				config_error_blank(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "except tkl");
				errors++;
				continue;
			}
			if (!strcmp(cep->ce_varname, "mask"))
			{
				if (!cep->ce_vardata)
				{
					config_error_empty(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "except tkl", "mask");
					errors++;
					continue;
				}
				if (has_mask)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "except tkl::mask");
					continue;
				}
				has_mask = 1;
			}
			else if (!strcmp(cep->ce_varname, "type"))
			{
				if (has_type)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "except tkl::type");
					continue;
				}					
				if (cep->ce_vardata)
				{
					if (!strcmp(cep->ce_vardata, "tkline") || 
					    !strcmp(cep->ce_vardata, "tzline"))
					{
						config_error("%s:%i: except tkl of type %s is"
							     " deprecated. Use except ban {}"
							     " instead", 
							     cep->ce_fileptr->cf_filename,
							     cep->ce_varlinenum, 
							     cep->ce_vardata);
						errors++;
					}
					if (!config_binary_flags_search(ExceptTklFlags, 
					     cep->ce_vardata, ARRAY_SIZEOF(ExceptTklFlags)))
					{
						config_error("%s:%i: unknown except tkl type %s",
							     cep->ce_fileptr->cf_filename, 
							     cep->ce_varlinenum,
							     cep->ce_vardata);
						return 1;
					}
				}
				else if (cep->ce_entries)
				{
					ConfigEntry *cepp;
					for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
					{
						if (!strcmp(cepp->ce_varname, "tkline") || 
						    !strcmp(cepp->ce_varname, "tzline"))
						{
							config_error("%s:%i: except tkl of type %s is"
								     " deprecated. Use except ban {}"
								     " instead", 
								     cepp->ce_fileptr->cf_filename,
								     cepp->ce_varlinenum, 
								     cepp->ce_varname);
							errors++;
						}
						if (!config_binary_flags_search(ExceptTklFlags, 
						     cepp->ce_varname, ARRAY_SIZEOF(ExceptTklFlags)))
						{
							config_error("%s:%i: unknown except tkl type %s",
								     cepp->ce_fileptr->cf_filename, 
								     cepp->ce_varlinenum,
								     cepp->ce_varname);
							return 1;
						}
					}
				}
				else
				{
					config_error_empty(cep->ce_fileptr->cf_filename, 
						cep->ce_varlinenum, "except tkl", "type");
					errors++;
					continue;
				}
				has_type = 1;
			}
			else
			{
				config_error_unknown(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "except tkl", cep->ce_varname);
				errors++;
				continue;
			}
		}
		if (!has_mask)
		{
			config_error("%s:%i: except tkl without mask item",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return 1;
		}
		if (!has_type)
		{
			config_error("%s:%i: except tkl without type item",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return 1;
		}
		return errors;
	}
	else {
		int used = 0;
		for (h = Hooks[HOOKTYPE_CONFIGTEST]; h; h = h->next) 
		{
			int value, errs = 0;
			if (h->owner && !(h->owner->flags & MODFLAG_TESTING)
			    && !(h->owner->options & MOD_OPT_PERM))
				continue;
			value = (*(h->func.intfunc))(conf,ce,CONFIG_EXCEPT,&errs);
			if (value == 2)
				used = 1;
			if (value == 1)
			{
				used = 1;
				break;
			}
			if (value == -1)
			{
				used = 1;
				errors += errs;
				break;
			}
			if (value == -2)
			{
				used = 1;
				errors += errs;
			}
		}
		if (!used) {
			config_error("%s:%i: unknown except type %s",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum, 
				ce->ce_vardata);
			return 1;
		}
	}
	return errors;
}

/*
 * vhost {} block parser
*/
int	_conf_vhost(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_vhost *vhost;
	ConfigItem_oper_from *from;
	ConfigEntry *cep, *cepp;
	vhost = MyMallocEx(sizeof(ConfigItem_vhost));

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "vhost"))
		{
			char *user, *host;
			user = strtok(cep->ce_vardata, "@");
			host = strtok(NULL, "");
			if (!host)
				vhost->virthost = strdup(user);
			else 
			{
				vhost->virtuser = strdup(user);
				vhost->virthost = strdup(host);
			}
		}
		else if (!strcmp(cep->ce_varname, "login"))
			vhost->login = strdup(cep->ce_vardata);	
		else if (!strcmp(cep->ce_varname, "password"))
			vhost->auth = Auth_ConvertConf2AuthStruct(cep);
		else if (!strcmp(cep->ce_varname, "from"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "userhost"))
				{
					from = MyMallocEx(sizeof(ConfigItem_oper_from));
					ircstrdup(from->name, cepp->ce_vardata);
					AddListItem(from, vhost->from);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "swhois"))
			vhost->swhois = strdup(cep->ce_vardata);
	}
	AddListItem(vhost, conf_vhost);
	return 1;
}

int	_test_vhost(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	ConfigEntry *cep;
	char has_vhost = 0, has_login = 0, has_password = 0, has_swhois = 0, has_from = 0;
	char has_userhost = 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, "vhost");
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "vhost"))
		{
			char *at, *tmp, *host;
			if (has_vhost)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "vhost::vhost");
				continue;
			}
			has_vhost = 1;
			if (!cep->ce_vardata)
			{
				config_error_empty(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "vhost", "vhost");
				errors++;
				continue;
			}	
			if ((at = strchr(cep->ce_vardata, '@')))
			{
				for (tmp = cep->ce_vardata; tmp != at; tmp++)
				{
					if (*tmp == '~' && tmp == cep->ce_vardata)
						continue;
					if (!isallowed(*tmp))
						break;
				}
				if (tmp != at)
				{
					config_error("%s:%i: vhost::vhost contains an invalid ident",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					errors++;
				}
				host = at+1;
			}
			else
				host = cep->ce_vardata;
			if (!*host)
			{
				config_error("%s:%i: vhost::vhost does not have a host set",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
			else
			{
				if (!valid_host(host))
				{
					config_error("%s:%i: vhost::vhost contains an invalid host",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					errors++;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "login"))
		{
			if (has_login)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "vhost::login");
			}
			has_login = 1;
			if (!cep->ce_vardata)
			{
				config_error_empty(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "vhost", "login");
				errors++;
				continue;
			}	
		}
		else if (!strcmp(cep->ce_varname, "password"))
		{
			if (has_password)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "vhost::password");
			}
			has_password = 1;
			if (!cep->ce_vardata)
			{
				config_error_empty(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "vhost", "password");
				errors++;
				continue;
			}	
			if (Auth_CheckError(cep) < 0)
				errors++;
		}
		else if (!strcmp(cep->ce_varname, "from"))
		{
			ConfigEntry *cepp;

			if (has_from)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "vhost::from");
				continue;
			}
			has_from = 1;
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (config_is_blankorempty(cepp, "vhost::from"))
				{
					errors++;
					continue;
				}
				if (!strcmp(cepp->ce_varname, "userhost"))
					has_userhost = 1;
				else
				{
					config_error_unknown(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "vhost::from",
						cepp->ce_varname);
					errors++;
					continue;	
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "swhois"))
		{
			if (has_swhois)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "vhost::swhois");
				continue;
			}
			has_swhois = 1;
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"vhost", cep->ce_varname);
			errors++;
		}
	}
	if (!has_vhost)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"vhost::vhost");
		errors++;
	}
	if (!has_login)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"vhost::login");
		errors++;
		
	}
	if (!has_password)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"vhost::password");
		errors++;
	}
	if (!has_from)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"vhost::from");
		errors++;
	}
	if (!has_userhost)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"vhost::userhost");
		errors++;
	}
	return errors;
}

#ifdef STRIPBADWORDS

static ConfigItem_badword *copy_badword_struct(ConfigItem_badword *ca, int regex, int regflags)
{
	ConfigItem_badword *x = MyMalloc(sizeof(ConfigItem_badword));
	memcpy(x, ca, sizeof(ConfigItem_badword));
	x->word = strdup(ca->word);
	if (ca->replace)
		x->replace = strdup(ca->replace);
	if (regex) 
	{
		memset(&x->expr, 0, sizeof(regex_t));
		regcomp(&x->expr, x->word, regflags);
	}
	return x;
}

int     _conf_badword(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *word = NULL;
	ConfigItem_badword *ca;
	char *tmp;
	short regex = 0;
	int regflags = 0;
#ifdef FAST_BADWORD_REPLACE
	int ast_l = 0, ast_r = 0;
#endif

	ca = MyMallocEx(sizeof(ConfigItem_badword));
	ca->action = BADWORD_REPLACE;
	regflags = REG_ICASE|REG_EXTENDED;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "action"))
		{
			if (!strcmp(cep->ce_vardata, "block"))
			{
				ca->action = BADWORD_BLOCK;
				/* If it is set to just block, then we don't need to worry about
				 * replacements 
				 */
				regflags |= REG_NOSUB;
			}
		}
		else if (!strcmp(cep->ce_varname, "replace"))
		{
			ircstrdup(ca->replace, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "word"))
			word = cep;
	}
#ifdef FAST_BADWORD_REPLACE
	/* The fast badwords routine can do: "blah" "*blah" "blah*" and "*blah*",
	 * in all other cases use regex.
	 */
	for (tmp = word->ce_vardata; *tmp; tmp++) {
		if (!isalnum(*tmp) && !(*tmp >= 128)) {
			if ((word->ce_vardata == tmp) && (*tmp == '*')) {
				ast_l = 1; /* Asterisk at the left */
				continue;
			}
			if ((*(tmp + 1) == '\0') && (*tmp == '*')) {
				ast_r = 1; /* Asterisk at the right */
				continue;
			}
			regex = 1;
			break;
		}
	}
	if (regex) 
	{
		ca->type = BADW_TYPE_REGEX;
		ircstrdup(ca->word, word->ce_vardata);
		regcomp(&ca->expr, ca->word, regflags);
	}
	else
	{
		char *tmpw;
		ca->type = BADW_TYPE_FAST;
		ca->word = tmpw = MyMalloc(strlen(word->ce_vardata) - ast_l - ast_r + 1);
		/* Copy except for asterisks */
		for (tmp = word->ce_vardata; *tmp; tmp++)
			if (*tmp != '*')
				*tmpw++ = *tmp;
		*tmpw = '\0';
		if (ast_l)
			ca->type |= BADW_TYPE_FAST_L;
		if (ast_r)
			ca->type |= BADW_TYPE_FAST_R;
	}
#else
	for (tmp = word->ce_vardata; *tmp; tmp++)
	{
		if (!isalnum(*tmp) && !(*tmp >= 128))
		{
			regex = 1;
			break;
		}
	}
	if (regex)
	{
		ircstrdup(ca->word, word->ce_vardata);
	}
	else
	{
		ca->word = MyMalloc(strlen(word->ce_vardata) + strlen(PATTERN) -1);
		ircsprintf(ca->word, PATTERN, word->ce_vardata);
	}
	/* Yes this is called twice, once in test, and once here, but it is still MUCH
	   faster than calling it each time a message is received like before. -- codemastr
	 */
	regcomp(&ca->expr, ca->word, regflags);
#endif
	if (!strcmp(ce->ce_vardata, "channel"))
		AddListItem(ca, conf_badword_channel);
	else if (!strcmp(ce->ce_vardata, "message"))
		AddListItem(ca, conf_badword_message);
	else if (!strcmp(ce->ce_vardata, "quit"))
		AddListItem(ca, conf_badword_quit);
	else if (!strcmp(ce->ce_vardata, "all"))
	{
		AddListItem(ca, conf_badword_channel);
		AddListItem(copy_badword_struct(ca,regex,regflags), conf_badword_message);
		AddListItem(copy_badword_struct(ca,regex,regflags), conf_badword_quit);
	}
	return 1;
}

int _test_badword(ConfigFile *conf, ConfigEntry *ce) 
{ 
	int errors = 0;
	ConfigEntry *cep;
	char has_word = 0, has_replace = 0, has_action = 0, action = 'r';

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: badword without type",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	else if (strcmp(ce->ce_vardata, "channel") && strcmp(ce->ce_vardata, "message") && 
	         strcmp(ce->ce_vardata, "quit") && strcmp(ce->ce_vardata, "all")) {
			config_error("%s:%i: badword with unknown type",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (config_is_blankorempty(cep, "badword"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "word"))
		{
			char *errbuf;
			if (has_word)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "badword::word");
				continue;
			}
			has_word = 1;
			if ((errbuf = unreal_checkregex(cep->ce_vardata,1,1)))
			{
				config_error("%s:%i: badword::%s contains an invalid regex: %s",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum,
					cep->ce_varname, errbuf);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "replace"))
		{
			if (has_replace)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "badword::replace");
				continue;
			}
			has_replace = 1;
		}
		else if (!strcmp(cep->ce_varname, "action"))
		{
			if (has_action)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "badword::action");
				continue;
			}
			has_action = 1;
			if (!strcmp(cep->ce_vardata, "replace"))
				action = 'r';
			else if (!strcmp(cep->ce_vardata, "block"))
				action = 'b';
			else
			{
				config_error("%s:%d: Unknown badword::action '%s'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_vardata);
				errors++;
			}
				
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"badword", cep->ce_varname);
			errors++;
		}
	}

	if (!has_word)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"badword::word");
		errors++;
	}
	if (has_action)
	{
		if (has_replace && action == 'b')
		{
			config_error("%s:%i: badword::action is block but badword::replace exists",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			errors++;
		}
	}
	return errors; 
}
#endif

int _conf_spamfilter(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	aTKline *nl = MyMallocEx(sizeof(aTKline));
	char *word = NULL, *reason = NULL, *bantime = NULL;
	int action = 0, target = 0;
	char has_reason = 0, has_bantime = 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "regex"))
		{
			nl->reason = strdup(cep->ce_vardata);

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
			nl->hostmask = strdup(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "reason"))
		{
			has_reason = 1;
			reason = cep->ce_vardata;
		}
		else if (!strcmp(cep->ce_varname, "ban-time"))
		{
			has_bantime = 1;
			bantime = cep->ce_vardata;
		}
	}
	nl->type = TKL_SPAMF;
	nl->expire_at = 0;
	nl->set_at = TStime();

	strncpyzt(nl->usermask, spamfilter_target_inttostring(target), sizeof(nl->usermask));
	nl->subtype = target;

	nl->setby = BadPtr(me.name) ? NULL : strdup(me.name); /* Hmm! */
	nl->ptr.spamf = unreal_buildspamfilter(word);
	nl->ptr.spamf->action = action;

	if (has_reason && reason)
		nl->ptr.spamf->tkl_reason = strdup(unreal_encodespace(reason));
	else
		nl->ptr.spamf->tkl_reason = strdup("<internally added by ircd>");

	if (has_bantime)
		nl->ptr.spamf->tkl_duration = config_checkval(bantime, CFG_TIME);
	else
		nl->ptr.spamf->tkl_duration = (SPAMFILTER_BAN_TIME ? SPAMFILTER_BAN_TIME : 86400);
		
	AddListItem(nl, tklines[tkl_hash('f')]);
	return 1;
}

int _test_spamfilter(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	int errors = 0;
	char *regex = NULL, *reason = NULL;
	char has_target = 0, has_regex = 0, has_action = 0, has_reason = 0, has_bantime = 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"spamfilter");
			errors++;
			continue;
		}
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
					if (!cepp->ce_varname)
					{
						config_error_blank(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, 
							"spamfilter::target");
						errors++;
						continue;
					}
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
		else if (!strcmp(cep->ce_varname, "regex"))
		{
			char *errbuf;
			if (has_regex)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "spamfilter::regex");
				continue;
			}
			has_regex = 1;
			if ((errbuf = unreal_checkregex(cep->ce_vardata,0,0)))
			{
				config_error("%s:%i: spamfilter::regex contains an invalid regex: %s",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum,
					errbuf);
				errors++;
				continue;
			}
			regex = cep->ce_vardata;
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
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"spamfilter", cep->ce_varname);
			errors++;
			continue;
		}
	}

	if (!has_regex)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"spamfilter::regex");
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
	if (regex && reason && (strlen(regex) + strlen(reason) > 505))
	{
		config_error("%s:%i: spamfilter block problem: regex + reason field are together over 505 bytes, "
		             "please choose a shorter regex or reason",
		             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}

	return errors;
}

int     _conf_help(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_help *ca;
	aMotd *last = NULL, *temp;
	ca = MyMallocEx(sizeof(ConfigItem_help));

	if (!ce->ce_vardata)
		ca->command = NULL;
	else
		ca->command = strdup(ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		temp = MyMalloc(sizeof(aMotd));
		temp->line = strdup(cep->ce_varname);
		temp->next = NULL;
		if (!ca->text)
			ca->text = temp;
		else
			last->next = temp;
		last = temp;
	}
	AddListItem(ca, conf_help);
	return 1;

}

int _test_help(ConfigFile *conf, ConfigEntry *ce) { 
	int errors = 0;
	ConfigEntry *cep;
	if (!ce->ce_entries)
	{
		config_error("%s:%i: empty help block", 
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"help");
			errors++;
			continue;
		}
	}
	return errors; 
}

int     _conf_log(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	ConfigItem_log *ca;
	OperFlag *ofp = NULL;

	ca = MyMallocEx(sizeof(ConfigItem_log));
	ircstrdup(ca->file, ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "maxsize")) 
		{
			ca->maxsize = config_checkval(cep->ce_vardata,CFG_SIZE);
		}
		else if (!strcmp(cep->ce_varname, "flags")) 
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if ((ofp = config_binary_flags_search(_LogFlags, cepp->ce_varname, ARRAY_SIZEOF(_LogFlags)))) 
					ca->flags |= ofp->flag;
			}
		}
	}
	AddListItem(ca, conf_log);
	return 1;

}

int _test_log(ConfigFile *conf, ConfigEntry *ce) { 
	int errors = 0;
	ConfigEntry *cep, *cepp;
	char has_flags = 0, has_maxsize = 0;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: log block without filename", 
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	if (!ce->ce_entries)
	{
		config_error("%s:%i: empty log block", 
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"log");
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "flags")) 
		{
			if (has_flags)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "log::flags");
				continue;
			}
			has_flags = 1;
			if (!cep->ce_entries)
			{
				config_error_empty(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "log", cep->ce_varname);
				errors++;
				continue;
			}
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!cepp->ce_varname)
				{
					config_error_blank(cepp->ce_fileptr->cf_filename, 
						cepp->ce_varlinenum, "log::flags");
					errors++;
					continue;
				}
				if (!config_binary_flags_search(_LogFlags, cepp->ce_varname, ARRAY_SIZEOF(_LogFlags))) 
				{
					config_error_unknownflag(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "log", cepp->ce_varname);
					errors++; 
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "maxsize")) 
		{
			if (has_maxsize)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "log::maxsize");
				continue;
			}
			has_maxsize = 1;
			if (!cep->ce_vardata)
			{
				config_error_empty(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "log", cep->ce_varname);
				errors++;
			}
		}
		else 
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"log", cep->ce_varname);
			errors++;
			continue;
		}
	}
	if (!has_flags)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"log::flags");
		errors++;
	}
	return errors; 
}


int	_conf_link(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	ConfigItem_link *link = NULL;
	OperFlag    *ofp;

	link = (ConfigItem_link *) MyMallocEx(sizeof(ConfigItem_link));
	link->servername = strdup(ce->ce_vardata);
	/* ugly, but it works. if it fails, we know _test_link failed miserably */
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "username"))
			link->username = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "hostname"))
			link->hostname = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "bind-ip"))
			link->bindip = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "port"))
			link->port = atol(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "password-receive"))
			link->recvauth = Auth_ConvertConf2AuthStruct(cep);
		else if (!strcmp(cep->ce_varname, "password-connect"))
			link->connpwd = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "class"))
		{
			link->class = Find_class(cep->ce_vardata);
			if (!link->class || (link->class->flag.temporary == 1))
			{
				config_status("%s:%i: illegal link::class, unknown class '%s' using default of class 'default'",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum,
					cep->ce_vardata);
				link->class = default_class;
			}
			link->class->xrefcount++;
		}
		else if (!strcmp(cep->ce_varname, "options"))
		{
			link->options = 0;
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if ((ofp = config_binary_flags_search(_LinkFlags, cepp->ce_varname, ARRAY_SIZEOF(_LinkFlags)))) 
					link->options |= ofp->flag;
			}
		}
		else if (!strcmp(cep->ce_varname, "hub"))
			link->hubmask = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "leaf"))
			link->leafmask = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "leafdepth"))
			link->leafdepth = atol(cep->ce_vardata);
#ifdef USE_SSL
		else if (!strcmp(cep->ce_varname, "ciphers"))
			link->ciphers = strdup(cep->ce_vardata);
#endif
#ifdef ZIP_LINKS
		else if (!strcmp(cep->ce_varname, "compression-level"))
			link->compression_level = atoi(cep->ce_vardata);
#endif
	}
	AddListItem(link, conf_link);
	return 0;
}

int	_test_link(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry	*cep, *cepp;
	OperFlag 	*ofp;
	int		errors = 0;
	char has_username = 0, has_hostname = 0, has_bindip = 0, has_port = 0;
	char has_passwordreceive = 0, has_passwordconnect = 0, has_class = 0;
	char has_hub = 0, has_leaf = 0, has_leafdepth = 0, has_ciphers = 0;
	char has_options = 0;
	char has_autoconnect = 0;
	char has_hostname_wildcards = 0;
#ifdef ZIP_LINKS
	char has_compressionlevel = 0;
#endif
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: link without servername",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;

	}
	if (!strchr(ce->ce_vardata, '.'))
	{
		config_error("%s:%i: link: bogus server name",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"link");
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "options"))
		{
			if (has_options)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::options");
				continue;
			}
			has_options = 1;
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!cepp->ce_varname)
				{
					config_error_blank(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "link::options");
					errors++; 
					continue;
				}
				if (!(ofp = config_binary_flags_search(_LinkFlags, cepp->ce_varname, ARRAY_SIZEOF(_LinkFlags)))) 
				{
					config_error_unknownopt(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "link", cepp->ce_varname);
					errors++;
					continue;
				}
#ifndef USE_SSL
				if (ofp->flag == CONNECT_SSL)
				{
					config_error("%s:%i: link %s with SSL option enabled on a non-SSL compile",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, ce->ce_vardata);
					errors++;
				}
#endif
#ifndef ZIP_LINKS
				if (ofp->flag == CONNECT_ZIP)
				{
					config_error("%s:%i: link %s with ZIP option enabled on a non-ZIP compile",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, ce->ce_vardata);
					errors++;
				}
#endif				
				if (ofp->flag == CONNECT_AUTO)
				{
					has_autoconnect = 1;
				}
			}
			continue;
		}
		if (!cep->ce_vardata)
		{
			config_error_empty(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"link", cep->ce_varname);
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "username"))
		{
			if (has_username)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::username");
				continue;
			}
			has_username = 1;
		}
		else if (!strcmp(cep->ce_varname, "hostname"))
		{
			if (has_hostname)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::hostname");
				continue;
			}
			has_hostname = 1;
#ifdef INET6
			/* I'm nice... I'll help those poor ipv6 users. -- Syzop */
			/* [ not null && len>6 && has not a : in it && last character is a digit ] */
			if (cep->ce_vardata && (strlen(cep->ce_vardata) > 6) && !strchr(cep->ce_vardata, ':') &&
			    isdigit(cep->ce_vardata[strlen(cep->ce_vardata)-1]))
			{
				char crap[32];
				if (inet_pton(AF_INET, cep->ce_vardata, crap) != 0)
				{
					char ipv6buf[48];
					snprintf(ipv6buf, sizeof(ipv6buf), "::ffff:%s", cep->ce_vardata);
					cep->ce_vardata = strdup(ipv6buf);
				} else {
				/* Insert IPv6 validation here */
					config_error( "%s:%i: listen: '%s' looks like "
						"it might be IPv4, but is not a valid address.",
						ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
						cep->ce_vardata);
					errors++;
				}
			}
#endif
			if (strchr(cep->ce_vardata, '*') != NULL || strchr(cep->ce_vardata, '?'))
			{
				has_hostname_wildcards = 1;
			}
		}
		else if (!strcmp(cep->ce_varname, "bind-ip"))
		{
			if (has_bindip)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::bind-ip");
				continue;
			}
			has_bindip = 1;
		}
		else if (!strcmp(cep->ce_varname, "port"))
		{
			if (has_port)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::port");
				continue;
			}
			has_port = 1;
		}
		else if (!strcmp(cep->ce_varname, "password-receive"))
		{
			if (has_passwordreceive)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::password-receive");
				continue;
			}
			has_passwordreceive = 1;
			if (Auth_CheckError(cep) < 0)
				errors++;
		}
		else if (!strcmp(cep->ce_varname, "password-connect"))
		{
			if (has_passwordconnect)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::password-connect");
				continue;
			}
			has_passwordconnect = 1;
			if (cep->ce_entries)
			{
				config_error("%s:%i: link::password-connect cannot be encrypted",
					     ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "class"))
		{
			if (has_class)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::class");
				continue;
			}
			has_class = 1;
		}
		else if (!strcmp(cep->ce_varname, "hub"))
		{
			if (has_hub)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::hub");
				continue;
			}
			has_hub = 1;
		}
		else if (!strcmp(cep->ce_varname, "leaf"))
		{
			if (has_leaf)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::leaf");
				continue;
			}
			has_leaf = 1;
		}
		else if (!strcmp(cep->ce_varname, "leafdepth"))
		{
			if (has_leafdepth)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::leafdepth");
				continue;
			}
			has_leafdepth = 1;
		}
		else if (!strcmp(cep->ce_varname, "ciphers"))
		{
			if (has_ciphers)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::ciphers");
				continue;
			}
			has_ciphers = 1;
		}
#ifdef ZIP_LINKS
		else if (!strcmp(cep->ce_varname, "compression-level"))
		{
			if (has_compressionlevel)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "link::compression-level");
				continue;
			}
			has_compressionlevel = 1;
			if ((atoi(cep->ce_vardata) < 1) || (atoi(cep->ce_vardata) > 9))
			{
				config_error("%s:%i: compression-level should be in range 1..9",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
#endif
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"link", cep->ce_varname);
			errors++;
		}
	}
	if (!has_username)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"link::username");
		errors++;
	}
	if (!has_hostname)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"link::hostname");
		errors++;
	}
	if (!has_port)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"link::port");
		errors++;
	}
	if (!has_passwordreceive)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"link::password-receive");
		errors++;
	}
	if (!has_passwordconnect)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"link::password-connect");
		errors++;
	}
	if (!has_class)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"link::class");
		errors++;
	}
	if (has_autoconnect && has_hostname_wildcards)
	{
		config_error("%s:%i: link block with autoconnect and wildcards (* and/or ? in hostname)",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	if (errors > 0)
		return errors;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "options")) 
		{
			continue;
		}
	}
	return errors;
		
}

int	_conf_cgiirc(ConfigFile *conf, ConfigEntry *ce)
{
ConfigEntry *cep;
ConfigEntry *cepp;
ConfigItem_cgiirc *cgiirc = NULL;

	cgiirc = (ConfigItem_cgiirc *) MyMallocEx(sizeof(ConfigItem_cgiirc));

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "username"))
			cgiirc->username = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "hostname"))
			cgiirc->hostname = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "password"))
			cgiirc->auth = Auth_ConvertConf2AuthStruct(cep);
		else if (!strcmp(cep->ce_varname, "type"))
		{
			if (!strcmp(cep->ce_vardata, "webirc"))
				cgiirc->type = CGIIRC_WEBIRC;
			else if (!strcmp(cep->ce_vardata, "old"))
				cgiirc->type = CGIIRC_PASS;
			else
				abort();
		}
	}
	AddListItem(cgiirc, conf_cgiirc);
	return 0;
}

int	_test_cgiirc(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry	*cep, *cepp;
	OperFlag 	*ofp;
	int		errors = 0;
	char has_username = 0; /* dup checking only, not mandatory */
	char has_type     = 0; /* mandatory */
	char has_hostname = 0; /* mandatory */
	char has_password = 0; /* mandatory */
	CGIIRCType type;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum, "cgiirc");
			errors++;
			continue;
		}
		if (!cep->ce_vardata)
		{
			config_error_empty(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"cgiirc", cep->ce_varname);
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "username"))
		{
			if (has_username)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "cgiirc::username");
				continue;
			}
			has_username = 1;
		}
		else if (!strcmp(cep->ce_varname, "hostname"))
		{
			if (has_hostname)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "cgiirc::hostname");
				continue;
			}
			has_hostname = 1;
#ifdef INET6
			/* I'm nice... I'll help those poor ipv6 users. -- Syzop */
			/* [ not null && len>6 && has not a : in it && last character is a digit ] */
			if (cep->ce_vardata && (strlen(cep->ce_vardata) > 6) && !strchr(cep->ce_vardata, ':') &&
			    isdigit(cep->ce_vardata[strlen(cep->ce_vardata)-1]))
			{
				char crap[32];
				if (inet_pton(AF_INET, cep->ce_vardata, crap) != 0)
				{
					char ipv6buf[48];
					snprintf(ipv6buf, sizeof(ipv6buf), "::ffff:%s", cep->ce_vardata);
					cep->ce_vardata = strdup(ipv6buf);
				} else {
				/* Insert IPv6 validation here */
					config_error( "%s:%i: cgiirc::hostname: '%s' looks like "
						"it might be IPv4, but is not a valid address.",
						ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
						cep->ce_vardata);
					errors++;
				}
			}
#endif
		}
		else if (!strcmp(cep->ce_varname, "password"))
		{
			if (has_password)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "cgiirc::password");
				continue;
			}
			has_password = 1;
			if (Auth_CheckError(cep) < 0)
				errors++;
		}
		else if (!strcmp(cep->ce_varname, "type"))
		{
			if (has_type)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "cgiirc::type");
			}
			has_type = 1;
			if (!strcmp(cep->ce_vardata, "webirc"))
				type = CGIIRC_WEBIRC;
			else if (!strcmp(cep->ce_vardata, "old"))
				type = CGIIRC_PASS;
			else
			{
				config_error("%s:%i: unknown cgiirc::type '%s', should be either 'webirc' or 'old'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				errors++;
			}
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"cgiirc", cep->ce_varname);
			errors++;
		}
	}
	if (!has_hostname)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"cgiirc::hostname");
		errors++;
	}
	if (!has_type)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"cgiirc::type");
		errors++;
	} else
	{
		if (!has_password && (type == CGIIRC_WEBIRC))
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"cgiirc::password");
			errors++;
		} else
		if (has_password && (type == CGIIRC_PASS))
		{
			config_error("%s:%i: cgiirc block has type set to 'old' but has a password set. "
			             "Passwords are not used with type 'old'. Either remove the password or "
			             "use the 'webirc' method instead.",
			             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			errors++;
		}
	}

	return errors;
}

int     _conf_ban(ConfigFile *conf, ConfigEntry *ce)
{

	ConfigEntry *cep;
	ConfigItem_ban *ca;
	Hook *h;

	ca = MyMallocEx(sizeof(ConfigItem_ban));
	if (!strcmp(ce->ce_vardata, "nick"))
	{
		aTKline *nl = MyMallocEx(sizeof(aTKline));
		nl->type = TKL_NICK;
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "mask"))
				nl->hostmask = strdup(cep->ce_vardata);
			else if (!strcmp(cep->ce_varname, "reason"))
				nl->reason = strdup(cep->ce_vardata);
		}
		strcpy(nl->usermask, "*");
		AddListItem(nl, tklines[tkl_hash('q')]);
		free(ca);
		return 0;
	}
	else if (!strcmp(ce->ce_vardata, "ip"))
		ca->flag.type = CONF_BAN_IP;
	else if (!strcmp(ce->ce_vardata, "server"))
		ca->flag.type = CONF_BAN_SERVER;
	else if (!strcmp(ce->ce_vardata, "user"))
		ca->flag.type = CONF_BAN_USER;
	else if (!strcmp(ce->ce_vardata, "realname"))
		ca->flag.type = CONF_BAN_REALNAME;
	else if (!strcmp(ce->ce_vardata, "version"))
	{
		ca->flag.type = CONF_BAN_VERSION;
		tempiConf.use_ban_version = 1;
	}
	else {
		int value;
		free(ca); /* ca isn't used, modules have their own list. */
		for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
		{
			value = (*(h->func.intfunc))(conf,ce,CONFIG_BAN);
			if (value == 1)
				break;
		}
		return 0;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask"))
		{
			ca->mask = strdup(cep->ce_vardata);
			if (ca->flag.type == CONF_BAN_IP || ca->flag.type == CONF_BAN_USER)
			{
				struct irc_netmask tmp;
				tmp.type = parse_netmask(ca->mask, &tmp);
				if (tmp.type != HM_HOST)
				{
					ca->netmask = MyMallocEx(sizeof(struct irc_netmask));
					bcopy(&tmp, ca->netmask, sizeof(struct irc_netmask));
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "reason"))
			ca->reason = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "action"))
			ca ->action = banact_stringtoval(cep->ce_vardata);
	}
	AddListItem(ca, conf_ban);
	return 0;
}

int     _test_ban(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int	    errors = 0;
	Hook *h;
	char type = 0;
	char has_mask = 0, has_action = 0, has_reason = 0;
	
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: ban without type",	
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	if (!strcmp(ce->ce_vardata, "nick"))
	{}
	else if (!strcmp(ce->ce_vardata, "ip"))
	{}
	else if (!strcmp(ce->ce_vardata, "server"))
	{}
	else if (!strcmp(ce->ce_vardata, "user"))
	{}
	else if (!strcmp(ce->ce_vardata, "realname"))
	{}
	else if (!strcmp(ce->ce_vardata, "version"))
		type = 'v';
	else
	{
		int used = 0;
		for (h = Hooks[HOOKTYPE_CONFIGTEST]; h; h = h->next) 
		{
			int value, errs = 0;
			if (h->owner && !(h->owner->flags & MODFLAG_TESTING)
			    && !(h->owner->options & MOD_OPT_PERM))
				continue;
			value = (*(h->func.intfunc))(conf,ce,CONFIG_BAN, &errs);
			if (value == 2)
				used = 1;
			if (value == 1)
			{
				used = 1;
				break;
			}
			if (value == -1)
			{
				used = 1;
				errors += errs;
				break;
			}
			if (value == -2)
			{
				used = 1;
				errors += errs;
			}
		}
		if (!used) {
			config_error("%s:%i: unknown ban type %s",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				ce->ce_vardata);
			return 1;
		}
		return errors;
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
		else if (!strcmp(cep->ce_varname, "action"))
		{
			if (has_action)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "ban::action");
			}
			has_action = 1;
			if (!banact_stringtoval(cep->ce_vardata))
			{
				config_error("%s:%i: ban::action has unknown action type '%s'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, 
					cep->ce_vardata);
				errors++;
			}
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
	if (has_action && type != 'v')
	{
		config_error("%s:%d: ban::action specified even though type is not 'version'",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	return errors;	
}

int	_conf_set(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp, *ceppp;
	OperFlag 	*ofl = NULL;
	Hook *h;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "kline-address")) {
			ircstrdup(tempiConf.kline_address, cep->ce_vardata);
		}
		if (!strcmp(cep->ce_varname, "gline-address")) {
			ircstrdup(tempiConf.gline_address, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "modes-on-connect")) {
			tempiConf.conn_modes = (long) set_usermode(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "modes-on-oper")) {
			tempiConf.oper_modes = (long) set_usermode(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "modes-on-join")) {
			set_channelmodes(cep->ce_vardata, &tempiConf.modes_on_join, 0);
		}
		else if (!strcmp(cep->ce_varname, "snomask-on-oper")) {
			ircstrdup(tempiConf.oper_snomask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "snomask-on-connect")) {
			ircstrdup(tempiConf.user_snomask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "level-on-join")) {
			tempiConf.level_on_join = channellevel_to_int(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "static-quit")) {
			ircstrdup(tempiConf.static_quit, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "static-part")) {
			ircstrdup(tempiConf.static_part, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "who-limit")) {
			tempiConf.who_limit = atol(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "maxbans")) {
			tempiConf.maxbans = atol(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "maxbanlength")) {
			tempiConf.maxbanlength = atol(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "silence-limit")) {
			tempiConf.silence_limit = atol(cep->ce_vardata);
			if (loop.ircd_booted)
				IsupportSetValue(IsupportFind("SILENCE"), cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "auto-join")) {
			ircstrdup(tempiConf.auto_join_chans, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "oper-auto-join")) {
			ircstrdup(tempiConf.oper_auto_join_chans, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "check-target-nick-bans")) {
			tempiConf.check_target_nick_bans = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "pingpong-warning")) {
			tempiConf.pingpong_warning = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "watch-away-notification")) {
			tempiConf.watch_away_notification = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "allow-userhost-change")) {
			if (!stricmp(cep->ce_vardata, "always"))
				tempiConf.userhost_allowed = UHALLOW_ALWAYS;
			else if (!stricmp(cep->ce_vardata, "never"))
				tempiConf.userhost_allowed = UHALLOW_NEVER;
			else if (!stricmp(cep->ce_vardata, "not-on-channels"))
				tempiConf.userhost_allowed = UHALLOW_NOCHANS;
			else
				tempiConf.userhost_allowed = UHALLOW_REJOIN;
		}
		else if (!strcmp(cep->ce_varname, "allowed-nickchars")) {
			for (cepp = cep->ce_entries; cepp; cepp=cepp->ce_next)
				charsys_add_language(cepp->ce_varname);
		}
		else if (!strcmp(cep->ce_varname, "channel-command-prefix")) {
			ircstrdup(tempiConf.channel_command_prefix, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "restrict-usermodes")) {
			int i;
			char *p = MyMalloc(strlen(cep->ce_vardata) + 1), *x = p;
			/* The data should be something like 'Gw' or something,
			 * but just in case users use '+Gw' then ignore the + (and -).
			 */
			for (i=0; i < strlen(cep->ce_vardata); i++)
				if ((cep->ce_vardata[i] != '+') && (cep->ce_vardata[i] != '-'))
					*x++ = cep->ce_vardata[i];
			*x = '\0';
			tempiConf.restrict_usermodes = p;
		}
		else if (!strcmp(cep->ce_varname, "restrict-channelmodes")) {
			int i;
			char *p = MyMalloc(strlen(cep->ce_vardata) + 1), *x = p;
			/* The data should be something like 'GL' or something,
			 * but just in case users use '+GL' then ignore the + (and -).
			 */
			for (i=0; i < strlen(cep->ce_vardata); i++)
				if ((cep->ce_vardata[i] != '+') && (cep->ce_vardata[i] != '-'))
					*x++ = cep->ce_vardata[i];
			*x = '\0';
			tempiConf.restrict_channelmodes = p;
		}
		else if (!strcmp(cep->ce_varname, "restrict-extendedbans")) {
			ircstrdup(tempiConf.restrict_extendedbans, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "anti-spam-quit-message-time")) {
			tempiConf.anti_spam_quit_message_time = config_checkval(cep->ce_vardata,CFG_TIME);
		}
		else if (!strcmp(cep->ce_varname, "oper-only-stats")) {
			if (!cep->ce_entries)
			{
				ircstrdup(tempiConf.oper_only_stats, cep->ce_vardata);
			}
			else
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					OperStat *os = MyMallocEx(sizeof(OperStat));
					ircstrdup(os->flag, cepp->ce_varname);
					AddListItem(os, tempiConf.oper_only_stats_ext);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "maxchannelsperuser")) {
			tempiConf.maxchannelsperuser = atoi(cep->ce_vardata);
			if (loop.ircd_booted)
			{
				char tmpbuf[512];
				IsupportSetValue(IsupportFind("MAXCHANNELS"), cep->ce_vardata);
				ircsprintf(tmpbuf, "#:%s", cep->ce_vardata);
				IsupportSetValue(IsupportFind("CHANLIMIT"), tmpbuf);
			}
		}
		else if (!strcmp(cep->ce_varname, "maxdccallow")) {
			tempiConf.maxdccallow = atoi(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "network-name")) {
			char *tmp;
			ircstrdup(tempiConf.network.x_ircnetwork, cep->ce_vardata);
			for (tmp = cep->ce_vardata; *cep->ce_vardata; cep->ce_vardata++) {
				if (*cep->ce_vardata == ' ')
					*cep->ce_vardata='-';
			}
			ircstrdup(tempiConf.network.x_ircnet005, tmp);
			if (loop.ircd_booted)
				IsupportSetValue(IsupportFind("NETWORK"), tmp);
			cep->ce_vardata = tmp;
		}
		else if (!strcmp(cep->ce_varname, "default-server")) {
			ircstrdup(tempiConf.network.x_defserv, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "services-server")) {
			ircstrdup(tempiConf.network.x_services_name, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "stats-server")) {
			ircstrdup(tempiConf.network.x_stats_server, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "help-channel")) {
			ircstrdup(tempiConf.network.x_helpchan, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "hiddenhost-prefix")) {
			ircstrdup(tempiConf.network.x_hidden_host, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "prefix-quit")) {
			if (*cep->ce_vardata == '0')
			{
				ircstrdup(tempiConf.network.x_prefix_quit, "");
			}
			else
				ircstrdup(tempiConf.network.x_prefix_quit, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "dns")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "timeout")) {
					tempiConf.host_timeout = config_checkval(cepp->ce_vardata,CFG_TIME);
				}
				else if (!strcmp(cepp->ce_varname, "retries")) {
					tempiConf.host_retries = config_checkval(cepp->ce_vardata,CFG_TIME);
				}
				else if (!strcmp(cepp->ce_varname, "nameserver")) {
					ircstrdup(tempiConf.name_server, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "bind-ip")) {
					ircstrdup(tempiConf.dns_bindip, cepp->ce_vardata);
				}
			}
		}
#ifdef THROTTLING
		else if (!strcmp(cep->ce_varname, "throttle")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "period")) 
					tempiConf.throttle_period = config_checkval(cepp->ce_vardata,CFG_TIME);
				else if (!strcmp(cepp->ce_varname, "connections"))
					tempiConf.throttle_count = atoi(cepp->ce_vardata);
			}
		}
#endif
		else if (!strcmp(cep->ce_varname, "anti-flood")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "unknown-flood-bantime")) 
					tempiConf.unknown_flood_bantime = config_checkval(cepp->ce_vardata,CFG_TIME);
				else if (!strcmp(cepp->ce_varname, "unknown-flood-amount"))
					tempiConf.unknown_flood_amount = atol(cepp->ce_vardata);
#ifdef NO_FLOOD_AWAY
				else if (!strcmp(cepp->ce_varname, "away-count"))
					tempiConf.away_count = atol(cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "away-period"))
					tempiConf.away_period = config_checkval(cepp->ce_vardata, CFG_TIME);
				else if (!strcmp(cepp->ce_varname, "away-flood"))
				{
					int cnt, period;
					config_parse_flood(cepp->ce_vardata, &cnt, &period);
					tempiConf.away_count = cnt;
					tempiConf.away_period = period;
				}
#endif
				else if (!strcmp(cepp->ce_varname, "nick-flood"))
				{
					int cnt, period;
					config_parse_flood(cepp->ce_vardata, &cnt, &period);
					tempiConf.nick_count = cnt;
					tempiConf.nick_period = period;
				}

			}
		}
		else if (!strcmp(cep->ce_varname, "options")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "webtv-support")) {
					tempiConf.webtv_support = 1;
				}
				else if (!strcmp(cepp->ce_varname, "hide-ulines")) {
					tempiConf.hide_ulines = 1;
				}
				else if (!strcmp(cepp->ce_varname, "flat-map")) {
					tempiConf.flat_map = 1;
				}
				else if (!strcmp(cepp->ce_varname, "no-stealth")) {
					tempiConf.no_oper_hiding = 1;
				}
				else if (!strcmp(cepp->ce_varname, "show-opermotd")) {
					tempiConf.som = 1;
				}
				else if (!strcmp(cepp->ce_varname, "identd-check")) {
					tempiConf.ident_check = 1;
				}
				else if (!strcmp(cepp->ce_varname, "fail-oper-warn")) {
					tempiConf.fail_oper_warn = 1;
				}
				else if (!strcmp(cepp->ce_varname, "show-connect-info")) {
					tempiConf.show_connect_info = 1;
				}
				else if (!strcmp(cepp->ce_varname, "dont-resolve")) {
					tempiConf.dont_resolve = 1;
				}
				else if (!strcmp(cepp->ce_varname, "mkpasswd-for-everyone")) {
					tempiConf.mkpasswd_for_everyone = 1;
				}
				else if (!strcmp(cepp->ce_varname, "allow-part-if-shunned")) {
					tempiConf.allow_part_if_shunned = 1;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "hosts")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "local")) {
					ircstrdup(tempiConf.network.x_locop_host, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "global")) {
					ircstrdup(tempiConf.network.x_oper_host, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "coadmin")) {
					ircstrdup(tempiConf.network.x_coadmin_host, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "admin")) {
					ircstrdup(tempiConf.network.x_admin_host, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "servicesadmin")) {
					ircstrdup(tempiConf.network.x_sadmin_host, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "netadmin")) {
					ircstrdup(tempiConf.network.x_netadmin_host, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "host-on-oper-up")) {
					tempiConf.network.x_inah = config_checkval(cepp->ce_vardata,CFG_YESNO);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "cloak-keys"))
		{
			for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
			{
				int value;
				value = (*(h->func.intfunc))(conf, cep, CONFIG_CLOAKKEYS);
				if (value == 1)
					break;
			}
		}
		else if (!strcmp(cep->ce_varname, "ident"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "connect-timeout"))
					tempiConf.ident_connect_timeout = config_checkval(cepp->ce_vardata,CFG_TIME);
				if (!strcmp(cepp->ce_varname, "read-timeout"))
					tempiConf.ident_read_timeout = config_checkval(cepp->ce_vardata,CFG_TIME);
			}
		}
		else if (!strcmp(cep->ce_varname, "timesync") || !strcmp(cep->ce_varname, "timesynch"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "enabled"))
					tempiConf.timesynch_enabled = config_checkval(cepp->ce_vardata,CFG_YESNO);
				else if (!strcmp(cepp->ce_varname, "timeout"))
					tempiConf.timesynch_timeout = config_checkval(cepp->ce_vardata,CFG_TIME);
				else if (!strcmp(cepp->ce_varname, "server"))
					ircstrdup(tempiConf.timesynch_server, cepp->ce_vardata);
			}
		}
		else if (!strcmp(cep->ce_varname, "spamfilter"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "ban-time"))
					tempiConf.spamfilter_ban_time = config_checkval(cepp->ce_vardata,CFG_TIME);
				else if (!strcmp(cepp->ce_varname, "ban-reason"))
					ircstrdup(tempiConf.spamfilter_ban_reason, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "virus-help-channel"))
					ircstrdup(tempiConf.spamfilter_virus_help_channel, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "virus-help-channel-deny"))
					tempiConf.spamfilter_vchan_deny = config_checkval(cepp->ce_vardata,CFG_YESNO);
				else if (!strcmp(cepp->ce_varname, "except"))
				{
					char *name, *p;
					SpamExcept *e;
					ircstrdup(tempiConf.spamexcept_line, cepp->ce_vardata);
					for (name = strtoken(&p, cepp->ce_vardata, ","); name; name = strtoken(&p, NULL, ","))
					{
						if (*name == ' ')
							name++;
						if (*name)
						{
							e = MyMallocEx(sizeof(SpamExcept) + strlen(name));
							strcpy(e->name, name);
							AddListItem(e, tempiConf.spamexcept);
						}
					}
				}
				else if (!strcmp(cepp->ce_varname, "detect-slow-warn"))
				{
					tempiConf.spamfilter_detectslow_warn = atol(cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "detect-slow-fatal"))
				{
					tempiConf.spamfilter_detectslow_fatal = atol(cepp->ce_vardata);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "default-bantime"))
		{
			tempiConf.default_bantime = config_checkval(cep->ce_vardata,CFG_TIME);
		}
		else if (!strcmp(cep->ce_varname, "ban-version-tkl-time"))
		{
			tempiConf.ban_version_tkl_time = config_checkval(cep->ce_vardata,CFG_TIME);
		}
#ifdef NEWCHFLOODPROT
		else if (!strcmp(cep->ce_varname, "modef-default-unsettime")) {
			int v = atoi(cep->ce_vardata);
			tempiConf.modef_default_unsettime = (unsigned char)v;
		}
		else if (!strcmp(cep->ce_varname, "modef-max-unsettime")) {
			int v = atoi(cep->ce_vardata);
			tempiConf.modef_max_unsettime = (unsigned char)v;
		}
#endif
		else if (!strcmp(cep->ce_varname, "ssl")) {
#ifdef USE_SSL
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "egd")) {
					tempiConf.use_egd = 1;
					if (cepp->ce_vardata)
						tempiConf.egd_path = strdup(cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "server-cipher-list"))
				{
					ircstrdup(tempiConf.x_server_cipher_list, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "certificate"))
				{
					ircstrdup(tempiConf.x_server_cert_pem, cepp->ce_vardata);	
				}
				else if (!strcmp(cepp->ce_varname, "key"))
				{
					ircstrdup(tempiConf.x_server_key_pem, cepp->ce_vardata);	
				}
				else if (!strcmp(cepp->ce_varname, "trusted-ca-file"))
				{
					ircstrdup(tempiConf.trusted_ca_file, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "renegotiate-bytes"))
				{
					tempiConf.ssl_renegotiate_bytes = config_checkval(cepp->ce_vardata, CFG_SIZE);
				}
				else if (!strcmp(cepp->ce_varname, "renegotiate-timeout"))
				{
					tempiConf.ssl_renegotiate_timeout = config_checkval(cepp->ce_vardata, CFG_TIME);
				}
				else if (!strcmp(cepp->ce_varname, "options"))
				{
					tempiConf.ssl_options = 0;
					for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
					{
						for (ofl = _SSLFlags; ofl->name; ofl++)
						{
							if (!strcmp(ceppp->ce_varname, ofl->name))
							{	
								tempiConf.ssl_options |= ofl->flag;
								break;
							}
						}
					}
					if (tempiConf.ssl_options & SSLFLAG_DONOTACCEPTSELFSIGNED)
						if (!tempiConf.ssl_options & SSLFLAG_VERIFYCERT)
							tempiConf.ssl_options |= SSLFLAG_VERIFYCERT;
				}	
				
			}
#endif
		}
		else 
		{
			int value;
			for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
			{
				value = (*(h->func.intfunc))(conf,cep,CONFIG_SET);
				if (value == 1)
					break;
			}
		}
	}
	return 0;
}

int	_test_set(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp, *ceppp;
	OperFlag 	*ofl = NULL;
	long		templong;
	int		tempi;
	int	    errors = 0;
	Hook	*h;
#define CheckNull(x) if ((!(x)->ce_vardata) || (!(*((x)->ce_vardata)))) { config_error("%s:%i: missing parameter", (x)->ce_fileptr->cf_filename, (x)->ce_varlinenum); errors++; continue; }
#define CheckDuplicate(cep, name, display) if (settings.has_##name) { config_warn_duplicate((cep)->ce_fileptr->cf_filename, cep->ce_varlinenum, "set::" display); continue; } else settings.has_##name = 1

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, "set");
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "kline-address")) {
			CheckNull(cep);
			CheckDuplicate(cep, kline_address, "kline-address");
			if (!strchr(cep->ce_vardata, '@') && !strchr(cep->ce_vardata, ':'))
			{
				config_error("%s:%i: set::kline-address must be an e-mail or an URL",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				continue;
			}
			else if (!match("*@unrealircd.com", cep->ce_vardata) || !match("*@unrealircd.org",cep->ce_vardata) || !match("unreal-*@lists.sourceforge.net",cep->ce_vardata)) 
			{
				config_error("%s:%i: set::kline-address may not be an UnrealIRCd Team address",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++; continue;
			}
		}
		else if (!strcmp(cep->ce_varname, "gline-address")) {
			CheckNull(cep);
			CheckDuplicate(cep, gline_address, "gline-address");
			if (!strchr(cep->ce_vardata, '@') && !strchr(cep->ce_vardata, ':'))
			{
				config_error("%s:%i: set::gline-address must be an e-mail or an URL",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				continue;
			}
			else if (!match("*@unrealircd.com", cep->ce_vardata) || !match("*@unrealircd.org",cep->ce_vardata) || !match("unreal-*@lists.sourceforge.net",cep->ce_vardata)) 
			{
				config_error("%s:%i: set::gline-address may not be an UnrealIRCd Team address",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++; continue;
			}
		}
		else if (!strcmp(cep->ce_varname, "modes-on-connect")) {
			char *p;
			CheckNull(cep);
			CheckDuplicate(cep, modes_on_connect, "modes-on-connect");
			for (p = cep->ce_vardata; *p; p++)
				if (strchr("oOaANCrzSgHhqtW", *p))
				{
					config_error("%s:%i: set::modes-on-connect may not include mode '%c'",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, *p);
					errors++;
				}
			templong = (long) set_usermode(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "modes-on-join")) {
			char *c;
			struct ChMode temp;
			bzero(&temp, sizeof(temp));
			CheckNull(cep);
			CheckDuplicate(cep, modes_on_join, "modes-on-join");
			for (c = cep->ce_vardata; *c; c++)
			{
				if (*c == ' ')
					break; /* don't check the parameter ;p */
				switch (*c)
				{
					case 'q':
					case 'a':
					case 'o':
					case 'h':
					case 'v':
					case 'b':
					case 'e':
					case 'I':
					case 'O':
					case 'A':
					case 'z':
					case 'l':
					case 'k':
					case 'L':
						config_error("%s:%i: set::modes-on-join contains +%c", 
							cep->ce_fileptr->cf_filename, cep->ce_varlinenum, *c);
						errors++;
						break;
				}
			}
			set_channelmodes(cep->ce_vardata, &temp, 1);
			if (temp.mode & MODE_NOKNOCK && !(temp.mode & MODE_INVITEONLY))
			{
				config_error("%s:%i: set::modes-on-join has +K but not +i",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
			if (temp.mode & MODE_NOCOLOR && temp.mode & MODE_STRIP)
			{
				config_error("%s:%i: set::modes-on-join has +c and +S",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
			if (temp.mode & MODE_SECRET && temp.mode & MODE_PRIVATE)
			{
				config_error("%s:%i: set::modes-on-join has +s and +p",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
			
		}
		else if (!strcmp(cep->ce_varname, "modes-on-oper")) {
			char *p;
			CheckNull(cep);
			CheckDuplicate(cep, modes_on_oper, "modes-on-oper");
			for (p = cep->ce_vardata; *p; p++)
				if (strchr("oOaANCrzS", *p))
				{
					config_error("%s:%i: set::modes-on-oper may not include mode '%c'",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, *p);
					errors++;
				}
			templong = (long) set_usermode(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "snomask-on-oper")) {
			CheckNull(cep);
			CheckDuplicate(cep, snomask_on_oper, "snomask-on-oper");
		}
		else if (!strcmp(cep->ce_varname, "snomask-on-connect")) {
			CheckNull(cep);
			CheckDuplicate(cep, snomask_on_connect, "snomask-on-connect");
		}
		else if (!strcmp(cep->ce_varname, "level-on-join")) {
			char *p;
			CheckNull(cep);
			CheckDuplicate(cep, level_on_join, "level-on-join");
			if (!channellevel_to_int(cep->ce_vardata))
			{
				config_error("%s:%i: set::level-on-join: unknown value '%s', should be one of: none, op",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "static-quit")) {
			CheckNull(cep);
			CheckDuplicate(cep, static_quit, "static-quit");
		}
		else if (!strcmp(cep->ce_varname, "static-part")) {
			CheckNull(cep);
			CheckDuplicate(cep, static_part, "static-part");
		}
		else if (!strcmp(cep->ce_varname, "who-limit")) {
			CheckNull(cep);
			CheckDuplicate(cep, who_limit, "who-limit");
		}
		else if (!strcmp(cep->ce_varname, "maxbans")) {
			CheckNull(cep);
			CheckDuplicate(cep, maxbans, "maxbans");
		}
		else if (!strcmp(cep->ce_varname, "maxbanlength")) {
			CheckNull(cep);
			CheckDuplicate(cep, maxbanlength, "maxbanlength");
		}
		else if (!strcmp(cep->ce_varname, "silence-limit")) {
			CheckNull(cep);
			CheckDuplicate(cep, silence_limit, "silence-limit");
		}
		else if (!strcmp(cep->ce_varname, "auto-join")) {
			CheckNull(cep);
			CheckDuplicate(cep, auto_join, "auto-join");
		}
		else if (!strcmp(cep->ce_varname, "oper-auto-join")) {
			CheckNull(cep);
			CheckDuplicate(cep, oper_auto_join, "oper-auto-join");
		}
		else if (!strcmp(cep->ce_varname, "check-target-nick-bans")) {
			CheckNull(cep);
			CheckDuplicate(cep, check_target_nick_bans, "check-target-nick-bans");
		}
		else if (!strcmp(cep->ce_varname, "pingpong-warning")) {
			CheckNull(cep);
			CheckDuplicate(cep, pingpong_warning, "pingpong-warning");
		}
		else if (!strcmp(cep->ce_varname, "watch-away-notification")) {
			CheckNull(cep);
			CheckDuplicate(cep, watch_away_notification, "watch-away-notification");
		}
		else if (!strcmp(cep->ce_varname, "channel-command-prefix")) {
			CheckNull(cep);
			CheckDuplicate(cep, channel_command_prefix, "channel-command-prefix");
		}
		else if (!strcmp(cep->ce_varname, "allow-userhost-change")) {
			CheckNull(cep);
			CheckDuplicate(cep, allow_userhost_change, "allow-userhost-change");
			if (stricmp(cep->ce_vardata, "always") && 
			    stricmp(cep->ce_vardata, "never") &&
			    stricmp(cep->ce_vardata, "not-on-channels") &&
			    stricmp(cep->ce_vardata, "force-rejoin"))
			{
				config_error("%s:%i: set::allow-userhost-change is invalid",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum);
				errors++;
				continue;
			}
		}
		else if (!strcmp(cep->ce_varname, "allowed-nickchars")) {
			if (cep->ce_vardata)
			{
				config_error("%s:%i: set::allowed-nickchars: please use 'allowed-nickchars { name; };' "
				             "and not 'allowed-nickchars name;'",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				continue;
			}
			for (cepp = cep->ce_entries; cepp; cepp=cepp->ce_next)
			{
				if (!charsys_test_language(cepp->ce_varname))
				{
					config_error("%s:%i: set::allowed-nickchars: Unknown (sub)language '%s'",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cepp->ce_varname);
					errors++;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "anti-spam-quit-message-time")) {
			CheckNull(cep);
			CheckDuplicate(cep, anti_spam_quit_message_time, "anti-spam-quit-message-time");
		}
		else if (!strcmp(cep->ce_varname, "oper-only-stats")) {
			CheckDuplicate(cep, oper_only_stats, "oper-only-stats");
			if (!cep->ce_entries)
			{
				CheckNull(cep);
			}
			else
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (!cepp->ce_varname)
						config_error("%s:%i: blank set::oper-only-stats item",
							cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "maxchannelsperuser")) {
			CheckNull(cep);
			CheckDuplicate(cep, maxchannelsperuser, "maxchannelsperuser");
			tempi = atoi(cep->ce_vardata);
			if (tempi < 1)
			{
				config_error("%s:%i: set::maxchannelsperuser must be > 0",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum);
				errors++;
				continue;
			}
		}
		else if (!strcmp(cep->ce_varname, "maxdccallow")) {
			CheckNull(cep);
			CheckDuplicate(cep, maxdccallow, "maxdccallow");
		}
		else if (!strcmp(cep->ce_varname, "network-name")) {
			char *p;
			CheckNull(cep);
			CheckDuplicate(cep, network_name, "network-name");
			for (p = cep->ce_vardata; *p; p++)
				if ((*p < ' ') || (*p > '~'))
				{
					config_error("%s:%i: set::network-name can only contain ASCII characters 33-126. Invalid character = '%c'",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, *p);
					errors++;
					break;
				}
		}
		else if (!strcmp(cep->ce_varname, "default-server")) {
			CheckNull(cep);
			CheckDuplicate(cep, default_server, "default-server");
		}
		else if (!strcmp(cep->ce_varname, "services-server")) {
			CheckNull(cep);
			CheckDuplicate(cep, services_server, "services-server");
		}
		else if (!strcmp(cep->ce_varname, "stats-server")) {
			CheckNull(cep);
			CheckDuplicate(cep, stats_server, "stats-server");
		}
		else if (!strcmp(cep->ce_varname, "help-channel")) {
			CheckNull(cep);
			CheckDuplicate(cep, help_channel, "help-channel");
		}
		else if (!strcmp(cep->ce_varname, "hiddenhost-prefix")) {
			CheckNull(cep);
			CheckDuplicate(cep, hiddenhost_prefix, "hiddenhost-prefix");
			if (strchr(cep->ce_vardata, ' ') || (*cep->ce_vardata == ':'))
			{
				config_error("%s:%i: set::hiddenhost-prefix must not contain spaces or be prefixed with ':'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				continue;
			}
		}
		else if (!strcmp(cep->ce_varname, "prefix-quit")) {
			CheckNull(cep);
			CheckDuplicate(cep, prefix_quit, "prefix-quit");
		}
		else if (!strcmp(cep->ce_varname, "restrict-usermodes"))
		{
			CheckNull(cep);
			CheckDuplicate(cep, restrict_usermodes, "restrict-usermodes");
			if (cep->ce_varname) {
				int warn = 0;
				char *p;
				for (p = cep->ce_vardata; *p; p++)
					if ((*p == '+') || (*p == '-'))
						warn = 1;
				if (warn) {
					config_status("%s:%i: warning: set::restrict-usermodes: should only contain modechars, no + or -.\n",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "restrict-channelmodes"))
		{
			CheckNull(cep);
			CheckDuplicate(cep, restrict_channelmodes, "restrict-channelmodes");
			if (cep->ce_varname) {
				int warn = 0;
				char *p;
				for (p = cep->ce_vardata; *p; p++)
					if ((*p == '+') || (*p == '-'))
						warn = 1;
				if (warn) {
					config_status("%s:%i: warning: set::restrict-channelmodes: should only contain modechars, no + or -.\n",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "restrict-extendedbans"))
		{
			CheckDuplicate(cep, restrict_extendedbans, "restrict-extendedbans");
			CheckNull(cep);
		}
		else if (!strcmp(cep->ce_varname, "dns")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "timeout")) {
					CheckDuplicate(cepp, dns_timeout, "dns::timeout");
				}
				else if (!strcmp(cepp->ce_varname, "retries")) {
					CheckDuplicate(cepp, dns_retries, "dns::retries");
				}
				else if (!strcmp(cepp->ce_varname, "nameserver")) {
					struct in_addr in;
					CheckDuplicate(cepp, dns_nameserver, "dns::nameserver");
					
					in.s_addr = inet_addr(cepp->ce_vardata);
					if (strcmp((char *)inet_ntoa(in), cepp->ce_vardata))
					{
						config_error("%s:%i: set::dns::nameserver (%s) is not a valid IP",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
							cepp->ce_vardata);
						errors++;
						continue;
					}
				}
				else if (!strcmp(cepp->ce_varname, "bind-ip")) {
					struct in_addr in;
					CheckDuplicate(cepp, dns_bind_ip, "dns::bind-ip");
					if (strcmp(cepp->ce_vardata, "*"))
					{
						in.s_addr = inet_addr(cepp->ce_vardata);
						if (strcmp((char *)inet_ntoa(in), cepp->ce_vardata))
						{
							config_error("%s:%i: set::dns::bind-ip (%s) is not a valid IP",
								cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
								cepp->ce_vardata);
							errors++;
							continue;
						}
					}
				}
				else
				{
					config_error_unknownopt(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::dns", 
						cepp->ce_varname);
						errors++;
				}
			}
		}
#ifdef THROTTLING
		else if (!strcmp(cep->ce_varname, "throttle")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "period")) {
					int x = config_checkval(cepp->ce_vardata,CFG_TIME);
					CheckDuplicate(cepp, throttle_period, "throttle::period");
					if (x > 86400*7)
					{
						config_error("%s:%i: insane set::throttle::period value",
							cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum);
						errors++;
						continue;
					}
				}
				else if (!strcmp(cepp->ce_varname, "connections")) {
					int x = atoi(cepp->ce_vardata);
					CheckDuplicate(cepp, throttle_connections, "throttle::connections");
					if ((x < 1) || (x > 127))
					{
						config_error("%s:%i: set::throttle::connections out of range, should be 1-127",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
						continue;
					}
				}
				else
				{
					config_error_unknownopt(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::throttle",
						cepp->ce_varname);
					errors++;
					continue;
				}
			}
		}
#endif
		else if (!strcmp(cep->ce_varname, "anti-flood")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "unknown-flood-bantime")) 
				{
					CheckDuplicate(cepp, anti_flood_unknown_flood_bantime, "anti-flood::unknown-flood-bantime");
				}
				else if (!strcmp(cepp->ce_varname, "unknown-flood-amount")) {
					CheckDuplicate(cepp, anti_flood_unknown_flood_amount, "anti-flood::unknown-flood-amount");
				}
#ifdef NO_FLOOD_AWAY
				else if (!strcmp(cepp->ce_varname, "away-count")) {
					int temp = atol(cepp->ce_vardata);
					CheckDuplicate(cepp, anti_flood_away_count, "anti-flood::away-count");
					if (temp < 1 || temp > 255)
					{
						config_error("%s:%i: set::anti-flood::away-count must be between 1 and 255",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
					}
				}
				else if (!strcmp(cepp->ce_varname, "away-period")) {
					int temp = config_checkval(cepp->ce_vardata, CFG_TIME);
					CheckDuplicate(cepp, anti_flood_away_period, "anti-flood::away-period");
					if (temp < 10)
					{
						config_error("%s:%i: set::anti-flood::away-period must be greater than 9",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
					}
				}
				else if (!strcmp(cepp->ce_varname, "away-flood"))
				{
					int cnt, period;
					if (settings.has_anti_flood_away_period)
					{
						config_warn("%s:%d: set::anti-flood::away-flood overrides set::anti-flood::away-period",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						continue;
					}
					if (settings.has_anti_flood_away_count)
					{
						config_warn("%s:%d: set::anti-flood::away-flood overrides set::anti-flood::away-count",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						continue;
					}
					settings.has_anti_flood_away_period = 1;
					settings.has_anti_flood_away_count = 1;
					if (!config_parse_flood(cepp->ce_vardata, &cnt, &period) ||
					    (cnt < 1) || (cnt > 255) || (period < 10))
					{
						config_error("%s:%i: set::anti-flood::away-flood error. Syntax is '<count>:<period>' (eg 5:60), "
						             "count should be 1-255, period should be greater than 9",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
					}
				}
#endif
				else if (!strcmp(cepp->ce_varname, "nick-flood"))
				{
					int cnt, period;
					CheckDuplicate(cepp, anti_flood_nick_flood, "anti-flood::nick-flood");
					if (!config_parse_flood(cepp->ce_vardata, &cnt, &period) ||
					    (cnt < 1) || (cnt > 255) || (period < 5))
					{
						config_error("%s:%i: set::anti-flood::away-flood error. Syntax is '<count>:<period>' (eg 5:60), "
						             "count should be 1-255, period should be greater than 4",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
					}
				}
				else
				{
					config_error_unknownopt(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::anti-flood",
						cepp->ce_varname);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "options")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "webtv-support")) 
				{
					CheckDuplicate(cepp, options_webtv_support, "options::webtv-support");
				}
				else if (!strcmp(cepp->ce_varname, "hide-ulines")) 
				{
					CheckDuplicate(cepp, options_hide_ulines, "options::hide-ulines");
				}
				else if (!strcmp(cepp->ce_varname, "flat-map")) {
					CheckDuplicate(cepp, options_flat_map, "options::flat-map");
				}
				else if (!strcmp(cepp->ce_varname, "show-opermotd")) {
					CheckDuplicate(cepp, options_show_opermotd, "options::show-opermotd");
				}
				else if (!strcmp(cepp->ce_varname, "identd-check")) {
					CheckDuplicate(cepp, options_identd_check, "options::identd-check");
				}
				else if (!strcmp(cepp->ce_varname, "fail-oper-warn")) {
					CheckDuplicate(cepp, options_fail_oper_warn, "options::fail-oper-warn");
				}
				else if (!strcmp(cepp->ce_varname, "show-connect-info")) {
					CheckDuplicate(cepp, options_show_connect_info, "options::show-connect-info");
				}
				else if (!strcmp(cepp->ce_varname, "dont-resolve")) {
					CheckDuplicate(cepp, options_dont_resolve, "options::dont-resolve");
				}
				else if (!strcmp(cepp->ce_varname, "mkpasswd-for-everyone")) {
					CheckDuplicate(cepp, options_mkpasswd_for_everyone, "options::mkpasswd-for-everyone");
				}
				else if (!strcmp(cepp->ce_varname, "allow-part-if-shunned")) {
					CheckDuplicate(cepp, options_allow_part_if_shunned, "options::allow-part-if-shunned");
				}
				else
				{
					config_error_unknownopt(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::options",
						cepp->ce_varname);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "hosts")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				char *c, *host;
				if (!cepp->ce_vardata)
				{
					config_error_empty(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::hosts",
						cepp->ce_varname);
					errors++;
					continue;
				} 
				if (!strcmp(cepp->ce_varname, "local")) {
					CheckDuplicate(cepp, hosts_local, "hosts::local");
				}
				else if (!strcmp(cepp->ce_varname, "global")) {
					CheckDuplicate(cepp, hosts_global, "hosts::global");
				}
				else if (!strcmp(cepp->ce_varname, "coadmin")) {
					CheckDuplicate(cepp, hosts_coadmin, "hosts::coadmin");
				}
				else if (!strcmp(cepp->ce_varname, "admin")) {
					CheckDuplicate(cepp, hosts_admin, "hosts::admin");
				}
				else if (!strcmp(cepp->ce_varname, "servicesadmin")) {
					CheckDuplicate(cepp, hosts_servicesadmin, "hosts::servicesadmin");
				}
				else if (!strcmp(cepp->ce_varname, "netadmin")) {
					CheckDuplicate(cepp, hosts_netadmin, "hosts::netadmin");
				}
				else if (!strcmp(cepp->ce_varname, "host-on-oper-up")) {
					CheckDuplicate(cepp, hosts_host_on_oper_up, "hosts::host-on-oper-up");
				}
				else
				{
					config_error_unknown(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::hosts", cepp->ce_varname);
					errors++;
					continue;

				}
				if ((c = strchr(cepp->ce_vardata, '@')))
				{
					char *tmp;
					if (!(*(c+1)) || (c-cepp->ce_vardata) > USERLEN ||
					    c == cepp->ce_vardata)
					{
						config_error("%s:%i: illegal value for set::hosts::%s",
							     cepp->ce_fileptr->cf_filename,
							     cepp->ce_varlinenum, 
							     cepp->ce_varname);
						errors++;
						continue;
					}
					for (tmp = cepp->ce_vardata; tmp != c; tmp++)
					{
						if (*tmp == '~' && tmp == cepp->ce_vardata)
							continue;
						if (!isallowed(*tmp))
							break;
					}
					if (tmp != c)
					{
						config_error("%s:%i: illegal value for set::hosts::%s",
							     cepp->ce_fileptr->cf_filename,
							     cepp->ce_varlinenum, 
							     cepp->ce_varname);
						errors++;
						continue;
					}
					host = c+1;
				}
				else
					host = cepp->ce_vardata;
				if (strlen(host) > HOSTLEN)
				{
					config_error("%s:%i: illegal value for set::hosts::%s",
						     cepp->ce_fileptr->cf_filename,
						     cepp->ce_varlinenum, 
						     cepp->ce_varname);
					errors++;
					continue;
				}
				for (; *host; host++)
				{
					if (!isallowed(*host) && *host != ':')
					{
						config_error("%s:%i: illegal value for set::hosts::%s",
							     cepp->ce_fileptr->cf_filename,
							     cepp->ce_varlinenum, 
							     cepp->ce_varname);
						errors++;
						continue;
					}
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "cloak-keys"))
		{
			CheckDuplicate(cep, cloak_keys, "cloak-keys");
			for (h = Hooks[HOOKTYPE_CONFIGTEST]; h; h = h->next)
			{
				int value, errs = 0;
				if (h->owner && !(h->owner->flags & MODFLAG_TESTING)
				    && !(h->owner->options & MOD_OPT_PERM))
					continue;
				value = (*(h->func.intfunc))(conf, cep, CONFIG_CLOAKKEYS, &errs);

				if (value == 1)
					break;
				if (value == -1)
				{
					errors += errs;
					break;
				}
				if (value == -2) 
					errors += errs;
			}
		}
		else if (!strcmp(cep->ce_varname, "scan")) {
			config_status("%s:%i: set::scan: WARNING: scanner support has been removed, "
			    "use BOPM instead: http://www.blitzed.org/bopm/ (*NIX) / http://vulnscan.org/winbopm/ (Windows)",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
		}
		else if (!strcmp(cep->ce_varname, "ident")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				int is_ok = 0;
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "connect-timeout"))
				{
					is_ok = 1;
					CheckDuplicate(cepp, ident_connect_timeout, "ident::connect-timeout");
				}
				else if (!strcmp(cepp->ce_varname, "read-timeout"))
				{
					is_ok = 1;
					CheckDuplicate(cepp, ident_read_timeout, "ident::read-timeout");
				}
				if (is_ok)
				{
					int v = config_checkval(cepp->ce_vardata,CFG_TIME);
					if ((v > 60) || (v < 1))
					{
						config_error("%s:%i: set::ident::%s value out of range (%d), should be between 1 and 60.",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, cepp->ce_varname, v);
						errors++;
						continue;
					}
				} else {
					config_error_unknown(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::ident",
						cepp->ce_varname);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "timesync") || !strcmp(cep->ce_varname, "timesynch")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "enabled"))
				{
				}
				else if (!strcmp(cepp->ce_varname, "timeout"))
				{
					int v = config_checkval(cepp->ce_vardata,CFG_TIME);
					if ((v > 5) || (v < 1))
					{
						config_error("%s:%i: set::timesync::%s value out of range (%d), should be between 1 and 5 (higher=unreliable).",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, cepp->ce_varname, v);
						errors++;
						continue;
					}
				} else if (!strcmp(cepp->ce_varname, "server"))
				{
				} else {
					config_error_unknown(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::timesync",
						cepp->ce_varname);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "spamfilter")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "ban-time"))
				{
					long x;
					CheckDuplicate(cepp, spamfilter_ban_time, "spamfilter::ban-time");
					x = config_checkval(cepp->ce_vardata,CFG_TIME);
					if ((x < 0) > (x > 2000000000))
					{
						config_error("%s:%i: set::spamfilter:ban-time: value '%ld' out of range",
							cep->ce_fileptr->cf_filename, cep->ce_varlinenum, x);
						errors++;
						continue;
					}
				} else
				if (!strcmp(cepp->ce_varname, "ban-reason"))
				{ 
					CheckDuplicate(cepp, spamfilter_ban_reason, "spamfilter::ban-reason");

				} 
				else if (!strcmp(cepp->ce_varname, "virus-help-channel"))
				{
					CheckDuplicate(cepp, spamfilter_virus_help_channel, "spamfilter::virus-help-channel");
					if ((cepp->ce_vardata[0] != '#') || (strlen(cepp->ce_vardata) > CHANNELLEN))
					{
						config_error("%s:%i: set::spamfilter:virus-help-channel: "
						             "specified channelname is too long or contains invalid characters (%s)",
						             cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
						             cepp->ce_vardata);
						errors++;
						continue;
					}
				} else 
				if (!strcmp(cepp->ce_varname, "virus-help-channel-deny"))
				{ 
					CheckDuplicate(cepp, spamfilter_virus_help_channel_deny, "spamfilter::virus-help-channel-deny");
				} else
				if (!strcmp(cepp->ce_varname, "except"))
				{ 
					CheckDuplicate(cepp, spamfilter_except, "spamfilter::except");
				} else
#ifdef SPAMFILTER_DETECTSLOW
				if (!strcmp(cepp->ce_varname, "detect-slow-warn"))
				{ 
				} else
				if (!strcmp(cepp->ce_varname, "detect-slow-fatal"))
				{ 
				} else
#endif
				{
					config_error_unknown(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::spamfilter",
						cepp->ce_varname);
					errors++;
					continue;
				}
			}
		}
/* TODO: FIX THIS */
		else if (!strcmp(cep->ce_varname, "default-bantime"))
		{
			long x;
			CheckDuplicate(cep, default_bantime, "default-bantime");
			CheckNull(cep);
			x = config_checkval(cep->ce_vardata,CFG_TIME);
			if ((x < 0) > (x > 2000000000))
			{
				config_error("%s:%i: set::default-bantime: value '%ld' out of range",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, x);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "ban-version-tkl-time")) {
			long x;
			CheckDuplicate(cep, ban_version_tkl_time, "ban-version-tkl-time");
			CheckNull(cep);
			x = config_checkval(cep->ce_vardata,CFG_TIME);
			if ((x < 0) > (x > 2000000000))
			{
				config_error("%s:%i: set::ban-version-tkl-time: value '%ld' out of range",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, x);
				errors++;
			}
		}
#ifdef NEWCHFLOODPROT
		else if (!strcmp(cep->ce_varname, "modef-default-unsettime")) {
			int v;
			CheckDuplicate(cep, modef_default_unsettime, "modef-default-unsettime");
			CheckNull(cep);
			v = atoi(cep->ce_vardata);
			if ((v <= 0) || (v > 255))
			{
				config_error("%s:%i: set::modef-default-unsettime: value '%d' out of range (should be 1-255)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, v);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "modef-max-unsettime")) {
			int v;
			CheckDuplicate(cep, modef_max_unsettime, "modef-max-unsettime");
			CheckNull(cep);
			v = atoi(cep->ce_vardata);
			if ((v <= 0) || (v > 255))
			{
				config_error("%s:%i: set::modef-max-unsettime: value '%d' out of range (should be 1-255)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, v);
				errors++;
			}
		}
#endif
		else if (!strcmp(cep->ce_varname, "ssl")) {
#ifdef USE_SSL
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "egd")) {
					CheckDuplicate(cep, ssl_egd, "ssl::egd");
				}
				else if (!strcmp(cepp->ce_varname, "renegotiate-timeout"))
				{
					CheckDuplicate(cep, renegotiate_timeout, "ssl::renegotiate-timeout");
				}
				else if (!strcmp(cepp->ce_varname, "renegotiate-bytes"))
				{
					CheckDuplicate(cep, renegotiate_bytes, "ssl::renegotiate-bytes");
				}
				else if (!strcmp(cepp->ce_varname, "server-cipher-list"))
				{
					CheckNull(cepp);
					CheckDuplicate(cep, ssl_server_cipher_list, "ssl::server-cipher-list");
				}
				else if (!strcmp(cepp->ce_varname, "certificate"))
				{
					CheckNull(cepp);
					CheckDuplicate(cep, ssl_certificate, "ssl::certificate");
				}
				else if (!strcmp(cepp->ce_varname, "key"))
				{
					CheckNull(cepp);
					CheckDuplicate(cep, ssl_key, "ssl::key");
				}
				else if (!strcmp(cepp->ce_varname, "trusted-ca-file"))
				{
					CheckNull(cepp);
					CheckDuplicate(cep, ssl_trusted_ca_file, "ssl::trusted-ca-file");
				}
				else if (!strcmp(cepp->ce_varname, "options"))
				{
					CheckDuplicate(cep, ssl_options, "ssl::options");
					for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
					{
						for (ofl = _SSLFlags; ofl->name; ofl++)
						{
							if (!strcmp(ceppp->ce_varname, ofl->name))
							{	
								break;
							}
						}
					}
					if (ofl && !ofl->name)
					{
						config_error("%s:%i: unknown SSL flag '%s'",
							ceppp->ce_fileptr->cf_filename, 
							ceppp->ce_varlinenum, ceppp->ce_varname);
					}
				}	
				
			}
#endif
		}
		else
		{
			int used = 0;
			for (h = Hooks[HOOKTYPE_CONFIGTEST]; h; h = h->next) 
			{
				int value, errs = 0;
				if (h->owner && !(h->owner->flags & MODFLAG_TESTING) &&
				                !(h->owner->options & MOD_OPT_PERM))
					continue;
				value = (*(h->func.intfunc))(conf,cep,CONFIG_SET, &errs);
				if (value == 2)
					used = 1;
				if (value == 1)
				{
					used = 1;
					break;
				}
				if (value == -1)
				{
					used = 1;
					errors += errs;
					break;
				}
				if (value == -2)
				{
					used = 1;
					errors += errs;
				}
			}
			if (!used) {
				config_error("%s:%i: unknown directive set::%s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_varname);
				errors++;
			}
		}
	}
	return errors;
}

int	_conf_loadmodule(ConfigFile *conf, ConfigEntry *ce)
{
#ifdef GLOBH
	glob_t files;
	int i;
#elif defined(_WIN32)
	HANDLE hFind;
	WIN32_FIND_DATA FindData;
	char cPath[MAX_PATH], *cSlash = NULL, *path;
#endif
	char *ret;
	if (!ce->ce_vardata)
	{
		config_status("%s:%i: loadmodule without filename",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
#ifdef GLOBH
#if defined(__OpenBSD__) && defined(GLOB_LIMIT)
	glob(ce->ce_vardata, GLOB_NOSORT|GLOB_NOCHECK|GLOB_LIMIT, NULL, &files);
#else
	glob(ce->ce_vardata, GLOB_NOSORT|GLOB_NOCHECK, NULL, &files);
#endif
	if (!files.gl_pathc) {
		globfree(&files);
		config_status("%s:%i: loadmodule %s: failed to load",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);
		return -1;
	}	
	for (i = 0; i < files.gl_pathc; i++) {
		if ((ret = Module_Create(files.gl_pathv[i]))) {
			config_status("%s:%i: loadmodule %s: failed to load: %s",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				files.gl_pathv[i], ret);
			return -1;
		}
	}
	globfree(&files);
#elif defined(_WIN32)
	bzero(cPath,MAX_PATH);
	if (strchr(ce->ce_vardata, '/') || strchr(ce->ce_vardata, '\\')) {
		strncpyzt(cPath,ce->ce_vardata,MAX_PATH);
		cSlash=cPath+strlen(cPath);
		while(*cSlash != '\\' && *cSlash != '/' && cSlash > cPath)
			cSlash--; 
		*(cSlash+1)=0;
	}
	hFind = FindFirstFile(ce->ce_vardata, &FindData);
	if (!FindData.cFileName || hFind == INVALID_HANDLE_VALUE) {
		config_status("%s:%i: loadmodule %s: failed to load",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);
		FindClose(hFind);
		return -1;
	}

	if (cPath) {
		path = MyMalloc(strlen(cPath) + strlen(FindData.cFileName)+1);
		strcpy(path,cPath);
		strcat(path,FindData.cFileName);
		if ((ret = Module_Create(path))) {
			config_status("%s:%i: loadmodule %s: failed to load: %s",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				path, ret);
			free(path);
			return -1;
		}
		free(path);
	}
	else
	{
		if ((ret = Module_Create(FindData.cFileName))) {
			config_status("%s:%i: loadmodule %s: failed to load: %s",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				FindData.cFileName, ret);
			return -1;
		}
	}
	while (FindNextFile(hFind, &FindData) != 0) {
		if (cPath) {
			path = MyMalloc(strlen(cPath) + strlen(FindData.cFileName)+1);
			strcpy(path,cPath);
			strcat(path,FindData.cFileName);		
			if ((ret = Module_Create(path)))
			{
				config_status("%s:%i: loadmodule %s: failed to load: %s",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					FindData.cFileName, ret);
				free(path);
				return -1;
			}
			free(path);
		}
		else
		{
			if ((ret = Module_Create(FindData.cFileName)))
			{
				config_status("%s:%i: loadmodule %s: failed to load: %s",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					FindData.cFileName, ret);
				return -1;
			}
		}
	}
	FindClose(hFind);
#else
	if ((ret = Module_Create(ce->ce_vardata))) {
			config_status("%s:%i: loadmodule %s: failed to load: %s",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				ce->ce_vardata, ret);
				return -1;
	}
#endif
	return 1;
}

int	_test_loadmodule(ConfigFile *conf, ConfigEntry *ce)
{
	return 0;
}

/*
 * Actually use configuration
*/

void	run_configuration(void)
{
	ConfigItem_listen 	*listenptr;

	for (listenptr = conf_listen; listenptr; listenptr = (ConfigItem_listen *) listenptr->next)
	{
		if (!(listenptr->options & LISTENER_BOUND))
		{
			if (add_listener2(listenptr) == -1)
			{
				ircd_log(LOG_ERROR, "Failed to bind to %s:%i", listenptr->ip, listenptr->port);
			}
				else
			{
			}
		}
		else
		{
			if (listenptr->listener)
			{
				listenptr->listener->umodes = 
					(listenptr->options & ~LISTENER_BOUND) ? listenptr->options : LISTENER_NORMAL;
				listenptr->listener->umodes |= LISTENER_BOUND;
			}
		}
	}
}

int	_conf_offchans(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		ConfigItem_offchans *of = MyMallocEx(sizeof(ConfigItem_offchans));
		strlcpy(of->chname, cep->ce_varname, CHANNELLEN+1);
		for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
		{
			if (!strcmp(cepp->ce_varname, "topic"))
				of->topic = strdup(cepp->ce_vardata);
		}
		AddListItem(of, conf_offchans);
	}
	return 0;
}

int	_test_offchans(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	ConfigEntry *cep, *cep2;
	char checkchan[CHANNELLEN + 1];
	
	if (!ce->ce_entries)
	{
		config_error("%s:%i: empty official-channels block", 
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (strlen(cep->ce_varname) > CHANNELLEN)
		{
			config_error("%s:%i: official-channels: '%s' name too long (max %d characters).",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname, CHANNELLEN);
			errors++;
			continue;
		}
		strcpy(checkchan, cep->ce_varname); /* safe */
		clean_channelname(checkchan);
		if (strcmp(checkchan, cep->ce_varname) || (*cep->ce_varname != '#'))
		{
			config_error("%s:%i: official-channels: '%s' is not a valid channel name.",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		}
		for (cep2 = cep->ce_entries; cep2; cep2 = cep2->ce_next)
		{
			if (!cep2->ce_vardata)
			{
				config_error_empty(cep2->ce_fileptr->cf_filename, 
					cep2->ce_varlinenum, "official-channels", 
					cep2->ce_varname);
				errors++;
				continue;
			}
			if (!strcmp(cep2->ce_varname, "topic"))
			{
				if (strlen(cep2->ce_vardata) > TOPICLEN)
				{
					config_error("%s:%i: official-channels::%s: topic too long (max %d characters).",
						cep2->ce_fileptr->cf_filename, cep2->ce_varlinenum, cep->ce_varname, TOPICLEN);
					errors++;
					continue;
				}
			} else {
				config_error_unknown(cep2->ce_fileptr->cf_filename, 
					cep2->ce_varlinenum, "official-channels", 
					cep2->ce_varname);
				errors++;
				continue;
			}
		}
	}
	return errors;
}

int	_conf_alias(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_alias *alias = NULL;
	ConfigItem_alias_format *format;
	ConfigEntry 	    	*cep, *cepp;
	aCommand *cmptr;

	if ((cmptr = find_Command(ce->ce_vardata, 0, M_ALIAS)))
		del_Command(ce->ce_vardata, NULL, cmptr->func);
	if (find_Command_simple(ce->ce_vardata))
	{
		config_warn("%s:%i: Alias '%s' would conflict with command (or server token) '%s', alias not added.",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata, ce->ce_vardata);
		return 0;
	}
	if ((alias = Find_alias(ce->ce_vardata)))
		DelListItem(alias, conf_alias);
	alias = MyMallocEx(sizeof(ConfigItem_alias));
	ircstrdup(alias->alias, ce->ce_vardata);
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "format")) {
			format = MyMallocEx(sizeof(ConfigItem_alias_format));
			ircstrdup(format->format, cep->ce_vardata);
			regcomp(&format->expr, cep->ce_vardata, REG_ICASE|REG_EXTENDED);
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "nick") ||
				    !strcmp(cepp->ce_varname, "target") ||
				    !strcmp(cepp->ce_varname, "command")) {
					ircstrdup(format->nick, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "parameters")) {
					ircstrdup(format->parameters, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "type")) {
					if (!strcmp(cepp->ce_vardata, "services"))
						format->type = ALIAS_SERVICES;
					else if (!strcmp(cepp->ce_vardata, "stats"))
						format->type = ALIAS_STATS;
					else if (!strcmp(cepp->ce_vardata, "normal"))
						format->type = ALIAS_NORMAL;
					else if (!strcmp(cepp->ce_vardata, "channel"))
						format->type = ALIAS_CHANNEL;
					else if (!strcmp(cepp->ce_vardata, "real"))
						format->type = ALIAS_REAL;
				}
			}
			AddListItem(format, alias->format);
		}		
				
		else if (!strcmp(cep->ce_varname, "nick") || !strcmp(cep->ce_varname, "target")) 
		{
			ircstrdup(alias->nick, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "type")) {
			if (!strcmp(cep->ce_vardata, "services"))
				alias->type = ALIAS_SERVICES;
			else if (!strcmp(cep->ce_vardata, "stats"))
				alias->type = ALIAS_STATS;
			else if (!strcmp(cep->ce_vardata, "normal"))
				alias->type = ALIAS_NORMAL;
			else if (!strcmp(cep->ce_vardata, "channel"))
				alias->type = ALIAS_CHANNEL;
			else if (!strcmp(cep->ce_vardata, "command"))
				alias->type = ALIAS_COMMAND;
		}
		else if (!strcmp(cep->ce_varname, "spamfilter"))
			alias->spamfilter = config_checkval(cep->ce_vardata, CFG_YESNO);
	}
	if (BadPtr(alias->nick) && alias->type != ALIAS_COMMAND) {
		ircstrdup(alias->nick, alias->alias); 
	}
	add_CommandX(alias->alias, NULL, m_alias, 1, M_USER|M_ALIAS);
	AddListItem(alias, conf_alias);
	return 0;
}


int _test_alias(ConfigFile *conf, ConfigEntry *ce) { 
	int errors = 0;
	ConfigEntry *cep, *cepp;
	char has_type = 0, has_target = 0, has_format = 0;
	char type = 0;

	if (!ce->ce_entries)
	{
		config_error("%s:%i: empty alias block", 
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	if (!ce->ce_vardata) 
	{
		config_error("%s:%i: alias without name", 
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	else if (!find_Command(ce->ce_vardata, 0, M_ALIAS) && find_Command(ce->ce_vardata, 0, 0)) {
		config_status("%s:%i: %s is an existing command, can not add alias",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum, ce->ce_vardata);
		errors++;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (config_is_blankorempty(cep, "alias"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "format")) {
			int errorcode, errorbufsize;
			char *errorbuf;
			regex_t expr;
			char has_type = 0, has_target = 0, has_parameters = 0;

			has_format = 1;
			errorcode = regcomp(&expr, cep->ce_vardata, REG_ICASE|REG_EXTENDED);
                        if (errorcode > 0)
                        {
                                errorbufsize = regerror(errorcode, &expr, NULL, 0)+1;
                                errorbuf = MyMalloc(errorbufsize);
                                regerror(errorcode, &expr, errorbuf, errorbufsize);
                                config_error("%s:%i: alias::format contains an invalid regex: %s",
 					cep->ce_fileptr->cf_filename,
 					cep->ce_varlinenum,
 					errorbuf);
                                errors++;
                                free(errorbuf);
                        }
			regfree(&expr);	
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (config_is_blankorempty(cepp, "alias::format"))
				{
					errors++;
					continue;
				}
				if (!strcmp(cepp->ce_varname, "nick") ||
				    !strcmp(cepp->ce_varname, "command") ||
				    !strcmp(cepp->ce_varname, "target"))
				{
					if (has_target)
					{
						config_warn_duplicate(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, 
							"alias::format::target");
						continue;
					}
					has_target = 1;
				}
				else if (!strcmp(cepp->ce_varname, "type"))
				{
					if (has_type)
					{
						config_warn_duplicate(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, 
							"alias::format::type");
						continue;
					}
					has_type = 1;
					if (!strcmp(cepp->ce_vardata, "services"))
						;
					else if (!strcmp(cepp->ce_vardata, "stats"))
						;
					else if (!strcmp(cepp->ce_vardata, "normal"))
						;
					else if (!strcmp(cepp->ce_vardata, "channel"))
						;
					else if (!strcmp(cepp->ce_vardata, "real"))
						;
					else 
					{
						config_error("%s:%i: unknown alias type",
						cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
					}
				}
				else if (!strcmp(cepp->ce_varname, "parameters")) 
				{
					if (has_parameters)
					{
						config_warn_duplicate(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, 
							"alias::format::parameters");
						continue;
					}
					has_parameters = 1;
				}
				else 
				{
					config_error_unknown(cepp->ce_fileptr->cf_filename, 
						cepp->ce_varlinenum, "alias::format",
						cepp->ce_varname);
					errors++;
				}
			}
			if (!has_target)
			{
				config_error_missing(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "alias::format::target");
				errors++;
			}
			if (!has_type)
			{
				config_error_missing(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "alias::format::type");
				errors++;
			}
			if (!has_parameters)
			{
				config_error_missing(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "alias::format::parameters");
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "nick") || !strcmp(cep->ce_varname, "target")) 
		{
			if (has_target)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "alias::target");
				continue;
			}
			has_target = 1;
		}
		else if (!strcmp(cep->ce_varname, "type")) {
			if (has_type)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "alias::type");
				continue;
			}
			has_type = 1;
			if (!strcmp(cep->ce_vardata, "services"))
				;
			else if (!strcmp(cep->ce_vardata, "stats"))
				;
			else if (!strcmp(cep->ce_vardata, "normal"))
				;
			else if (!strcmp(cep->ce_vardata, "channel"))
				;
			else if (!strcmp(cep->ce_vardata, "command"))
				type = 'c';
			else {
				config_error("%s:%i: unknown alias type",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "spamfilter"))
			;
		else {
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"alias", cep->ce_varname);
			errors++;
		}
	}
	if (!has_type)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"alias::type");
		errors++;
	}
	if (!has_format && type == 'c')
	{
		config_error("%s:%d: alias::type is 'command' but no alias::format was specified",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	else if (has_format && type != 'c')
	{
		config_error("%s:%d: alias::format specified when type is not 'command'",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	return errors; 
}

int	_conf_deny(ConfigFile *conf, ConfigEntry *ce)
{
Hook *h;

	if (!strcmp(ce->ce_vardata, "dcc"))
		_conf_deny_dcc(conf, ce);
	else if (!strcmp(ce->ce_vardata, "channel"))
		_conf_deny_channel(conf, ce);
	else if (!strcmp(ce->ce_vardata, "link"))
		_conf_deny_link(conf, ce);
	else if (!strcmp(ce->ce_vardata, "version"))
		_conf_deny_version(conf, ce);
	else
	{
		int value;
		for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
		{
			value = (*(h->func.intfunc))(conf,ce,CONFIG_DENY);
			if (value == 1)
				break;
		}
		return 0;
	}
	return 0;
}

int	_conf_deny_dcc(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_deny_dcc 	*deny = NULL;
	ConfigEntry 	    	*cep;

	deny = MyMallocEx(sizeof(ConfigItem_deny_dcc));
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "filename"))
		{
			ircstrdup(deny->filename, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "reason"))
		{
			ircstrdup(deny->reason, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "soft"))
		{
			int x = config_checkval(cep->ce_vardata,CFG_YESNO);
			if (x == 1)
				deny->flag.type = DCCDENY_SOFT;
		}
	}
	if (!deny->reason)
	{
		if (deny->flag.type == DCCDENY_HARD)
			ircstrdup(deny->reason, "Possible infected virus file");
		else
			ircstrdup(deny->reason, "Possible executable content");
	}
	AddListItem(deny, conf_deny_dcc);
	return 0;
}

int	_conf_deny_channel(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_deny_channel 	*deny = NULL;
	ConfigEntry 	    	*cep;

	deny = MyMallocEx(sizeof(ConfigItem_deny_channel));
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "channel"))
		{
			ircstrdup(deny->channel, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "redirect"))
		{
			ircstrdup(deny->redirect, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "reason"))
		{
			ircstrdup(deny->reason, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "warn"))
		{
			deny->warn = config_checkval(cep->ce_vardata,CFG_YESNO);
		}
	}
	AddListItem(deny, conf_deny_channel);
	return 0;
}
int	_conf_deny_link(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_deny_link 	*deny = NULL;
	ConfigEntry 	    	*cep;

	deny = MyMallocEx(sizeof(ConfigItem_deny_link));
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask"))
		{
			ircstrdup(deny->mask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "rule"))
		{
			deny->rule = (char *)crule_parse(cep->ce_vardata);
			ircstrdup(deny->prettyrule, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "type")) {
			if (!strcmp(cep->ce_vardata, "all"))
				deny->flag.type = CRULE_ALL;
			else if (!strcmp(cep->ce_vardata, "auto"))
				deny->flag.type = CRULE_AUTO;
		}
	}
	AddListItem(deny, conf_deny_link);
	return 0;
}

int	_conf_deny_version(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_deny_version *deny = NULL;
	ConfigEntry 	    	*cep;

	deny = MyMallocEx(sizeof(ConfigItem_deny_version));
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask"))
		{
			ircstrdup(deny->mask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "version"))
		{
			ircstrdup(deny->version, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "flags"))
		{
			ircstrdup(deny->flags, cep->ce_vardata);
		}
	}
	AddListItem(deny, conf_deny_version);
	return 0;
}

int     _test_deny(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int	    errors = 0;
	Hook	*h;
	
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: deny without type",	
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	if (!strcmp(ce->ce_vardata, "dcc"))
	{
		char has_filename = 0, has_reason = 0, has_soft = 0;
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (config_is_blankorempty(cep, "deny dcc"))
			{
				errors++;
				continue;
			}
			if (!strcmp(cep->ce_varname, "filename"))
			{
				if (has_filename)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny dcc::filename");
					continue;
				}
				has_filename = 1;
			}
			else if (!strcmp(cep->ce_varname, "reason"))
			{
				if (has_reason)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny dcc::reason");
					continue;
				}
				has_reason = 1;
			}
			else if (!strcmp(cep->ce_varname, "soft"))
			{
				if (has_soft)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny dcc::soft");
					continue;
				}
				has_soft = 1;
			}
			else 
			{
				config_error_unknown(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "deny dcc", cep->ce_varname);
				errors++;
			}
		}
		if (!has_filename)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"deny dcc::filename");
			errors++;
		}
		if (!has_reason)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"deny dcc::reason");
			errors++;
		}
	}
	else if (!strcmp(ce->ce_vardata, "channel"))
	{
		char has_channel = 0, has_warn = 0, has_reason = 0, has_redirect = 0;
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (config_is_blankorempty(cep, "deny channel"))
			{
				errors++;
				continue;
			}
			if (!strcmp(cep->ce_varname, "channel"))
			{
				if (has_channel)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny channel::channel");
					continue;
				}
				has_channel = 1;
			}
			else if (!strcmp(cep->ce_varname, "redirect"))
			{
				if (has_redirect)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny channel::redirect");
					continue;
				}
				has_redirect = 1;
			}
			else if (!strcmp(cep->ce_varname, "reason"))
			{
				if (has_reason)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny channel::reason");
					continue;
				}
				has_reason = 1;
			}
			else if (!strcmp(cep->ce_varname, "warn"))
			{
				if (has_warn)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny channel::warn");
					continue;
				}
				has_warn = 1;
			}
			else 
			{
				config_error_unknown(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "deny channel", cep->ce_varname);
				errors++;
			}
		}
		if (!has_channel)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"deny channel::channel");
			errors++;
		}
		if (!has_reason)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"deny channel::reason");
			errors++;
		}
	}
	else if (!strcmp(ce->ce_vardata, "link"))
	{
		char has_mask = 0, has_rule = 0, has_type = 0;
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (config_is_blankorempty(cep, "deny link"))
			{
				errors++;
				continue;
			}
			if (!strcmp(cep->ce_varname, "mask"))
			{
				if (has_mask)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny link::mask");
					continue;
				}
				has_mask = 1;
			}
			else if (!strcmp(cep->ce_varname, "rule"))
			{
				int val = 0;
				if (has_rule)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny link::rule");
					continue;
				}
				has_rule = 1;
				if ((val = crule_test(cep->ce_vardata)))
				{
					config_error("%s:%i: deny link::rule contains an invalid expression: %s",
						cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum,
						crule_errstring(val));
					errors++;
				}
			}
			else if (!strcmp(cep->ce_varname, "type"))
			{
				if (has_type)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny link::type");
					continue;
				}
				has_type = 1;
				if (!strcmp(cep->ce_vardata, "auto"))
				;
				else if (!strcmp(cep->ce_vardata, "all"))
				;
				else {
					config_status("%s:%i: unknown deny link type",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					errors++;
				}
			}	
			else 
			{
				config_error_unknown(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "deny link", cep->ce_varname);
				errors++;
			}
		}
		if (!has_mask)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"deny link::mask");
			errors++;
		}	
		if (!has_rule)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"deny link::rule");
			errors++;
		}
		if (!has_type)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"deny link::type");
			errors++;
		}
	}
	else if (!strcmp(ce->ce_vardata, "version"))
	{
		char has_mask = 0, has_version = 0, has_flags = 0;
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (config_is_blankorempty(cep, "deny version"))
			{
				errors++;
				continue;
			}
			if (!strcmp(cep->ce_varname, "mask"))
			{
				if (has_mask)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny version::mask");
					continue;
				}
				has_mask = 1;
			}
			else if (!strcmp(cep->ce_varname, "version"))
			{
				if (has_version)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny version::version");
					continue;
				}
				has_version = 1;
			}
			else if (!strcmp(cep->ce_varname, "flags"))
			{
				if (has_flags)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny version::flags");
					continue;
				}
				has_flags = 1;
			}
			else 
			{
				config_error_unknown(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "deny version", cep->ce_varname);
				errors++;
			}
		}
		if (!has_mask)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"deny version::mask");
			errors++;
		}
		if (!has_version)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"deny version::version");
			errors++;
		}
		if (!has_flags)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"deny version::flags");
			errors++;
		}
	}
	else
	{
		int used = 0;
		for (h = Hooks[HOOKTYPE_CONFIGTEST]; h; h = h->next) 
		{
			int value, errs = 0;
			if (h->owner && !(h->owner->flags & MODFLAG_TESTING)
			    && !(h->owner->options & MOD_OPT_PERM))
				continue;
			value = (*(h->func.intfunc))(conf,ce,CONFIG_DENY, &errs);
			if (value == 2)
				used = 1;
			if (value == 1)
			{
				used = 1;
				break;
			}
			if (value == -1)
			{
				used = 1;
				errors += errs;
				break;
			}
			if (value == -2)
			{
				used = 1;
				errors += errs;
			}
		}
		if (!used) {
			config_error("%s:%i: unknown deny type %s",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				ce->ce_vardata);
			return 1;
		}
		return errors;
	}

	return errors;	
}

#ifdef USE_LIBCURL
static void conf_download_complete(char *url, char *file, char *errorbuf, int cached)
{
	ConfigItem_include *inc;
	if (!loop.ircd_rehashing)
	{
		remove(file);
		return;
	}
	for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next)
	{
		if (!(inc->flag.type & INCLUDE_REMOTE))
			continue;
		if (inc->flag.type & INCLUDE_NOTLOADED)
			continue;
		if (!stricmp(url, inc->url))
		{
			inc->flag.type &= ~INCLUDE_DLQUEUED;
			break;
		}
	}
	if (!file && !cached)
		add_remote_include(file, url, 0, errorbuf);
	else
	{
		if (cached)
		{
			char *urlfile = url_getfilename(url);
			char *file = unreal_getfilename(urlfile);
			char *tmp = unreal_mktemp("tmp", file);
			unreal_copyfileex(inc->file, tmp, 1);
			add_remote_include(tmp, url, 0, NULL);
			free(urlfile);
		}
		else
			add_remote_include(file, url, 0, NULL);
	}
	for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next)
	{
		if (inc->flag.type & INCLUDE_DLQUEUED)
			return;
	}
	rehash_internal(loop.rehash_save_cptr, loop.rehash_save_sptr, loop.rehash_save_sig);
}
#endif

int     rehash(aClient *cptr, aClient *sptr, int sig)
{
#ifdef USE_LIBCURL
	ConfigItem_include *inc;
	char found_remote = 0;
	if (loop.ircd_rehashing)
	{
		if (!sig)
			sendto_one(sptr, ":%s NOTICE %s :A rehash is already in progress",
				me.name, sptr->name);
		return 0;
	}

	loop.ircd_rehashing = 1;
	loop.rehash_save_cptr = cptr;
	loop.rehash_save_sptr = sptr;
	loop.rehash_save_sig = sig;
	for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next)
	{
		time_t modtime;
		if (!(inc->flag.type & INCLUDE_REMOTE))
			continue;

		if (inc->flag.type & INCLUDE_NOTLOADED)
			continue;
		found_remote = 1;
		modtime = unreal_getfilemodtime(inc->file);
		inc->flag.type |= INCLUDE_DLQUEUED;
		download_file_async(inc->url, modtime, conf_download_complete);
	}
	if (!found_remote)
		return rehash_internal(cptr, sptr, sig);
	return 0;
#else
	loop.ircd_rehashing = 1;
	return rehash_internal(cptr, sptr, sig);
#endif
}

int	rehash_internal(aClient *cptr, aClient *sptr, int sig)
{
	flush_connections(&me);
	if (sig == 1)
	{
		sendto_ops("Got signal SIGHUP, reloading %s file", configfile);
#ifdef	ULTRIX
		if (fork() > 0)
			exit(0);
		write_pidfile();
#endif
	}
	loop.ircd_rehashing = 1; /* double checking.. */
	if (init_conf(configfile, 1) == 0)
		run_configuration();
	if (sig == 1)
		reread_motdsandrules();
	unload_all_unused_snomasks();
	unload_all_unused_umodes();
	unload_all_unused_extcmodes();
	loop.ircd_rehashing = 0;	
	return 1;
}

void	link_cleanup(ConfigItem_link *link_ptr)
{
	ircfree(link_ptr->servername);
	ircfree(link_ptr->username);
	ircfree(link_ptr->bindip);
	ircfree(link_ptr->hostname);
	ircfree(link_ptr->hubmask);
	ircfree(link_ptr->leafmask);
	ircfree(link_ptr->connpwd);
#ifdef USE_SSL
	ircfree(link_ptr->ciphers);
#endif
	Auth_DeleteAuthStruct(link_ptr->recvauth);
	link_ptr->recvauth = NULL;
}

void delete_linkblock(ConfigItem_link *link_ptr)
{
	Debug((DEBUG_ERROR, "delete_linkblock: deleting %s, refcount=%d",
		link_ptr->servername, link_ptr->refcount));
	if (link_ptr->class)
	{
		link_ptr->class->xrefcount--;
		/* Perhaps the class is temporary too and we need to free it... */
		if (link_ptr->class->flag.temporary && 
		    !link_ptr->class->clients && !link_ptr->class->xrefcount)
		{
			delete_classblock(link_ptr->class);
			link_ptr->class = NULL;
		}
	}
	link_cleanup(link_ptr);
	DelListItem(link_ptr, conf_link);
	MyFree(link_ptr);
}

void delete_cgiircblock(ConfigItem_cgiirc *e)
{
	Debug((DEBUG_ERROR, "delete_cgiircblock: deleting %s", e->hostname));
	if (e->auth)
		Auth_DeleteAuthStruct(e->auth);
	ircfree(e->hostname);
	ircfree(e->username);
	DelListItem(e, conf_cgiirc);
	MyFree(e);
}

void delete_classblock(ConfigItem_class *class_ptr)
{
	Debug((DEBUG_ERROR, "delete_classblock: deleting %s, clients=%d, xrefcount=%d",
		class_ptr->name, class_ptr->clients, class_ptr->xrefcount));
	ircfree(class_ptr->name);
	DelListItem(class_ptr, conf_class);
	MyFree(class_ptr);
}

void	listen_cleanup()
{
	int	i = 0;
	ConfigItem_listen *listen_ptr;
	ListStruct *next;
	for (listen_ptr = conf_listen; listen_ptr; listen_ptr = (ConfigItem_listen *)next)
	{
		next = (ListStruct *)listen_ptr->next;
		if (listen_ptr->flag.temporary && !listen_ptr->clients)
		{
			ircfree(listen_ptr->ip);
			DelListItem(listen_ptr, conf_listen);
			MyFree(listen_ptr);
			i++;
		}
	}
	if (i)
		close_listeners();
}

#ifdef USE_LIBCURL
char *find_remote_include(char *url, char **errorbuf)
{
	ConfigItem_include *inc;
	for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next)
	{
		if (!(inc->flag.type & INCLUDE_NOTLOADED))
			continue;
		if (!(inc->flag.type & INCLUDE_REMOTE))
			continue;
		if (!stricmp(url, inc->url))
		{
			*errorbuf = inc->errorbuf;
			return inc->file;
		}
	}
	return NULL;
}

char *find_loaded_remote_include(char *url)
{
	ConfigItem_include *inc;
	for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next)
	{
		if ((inc->flag.type & INCLUDE_NOTLOADED))
			continue;
		if (!(inc->flag.type & INCLUDE_REMOTE))
			continue;
		if (!stricmp(url, inc->url))
			return inc->file;
	}
	return NULL;
}	

int remote_include(ConfigEntry *ce)
{
	char *errorbuf = NULL;
	char *file = find_remote_include(ce->ce_vardata, &errorbuf);
	int ret;
	if (!loop.ircd_rehashing || (loop.ircd_rehashing && !file && !errorbuf))
	{
		char *error;
		if (config_verbose > 0)
			config_status("Downloading %s", ce->ce_vardata);
		file = download_file(ce->ce_vardata, &error);
		if (!file)
		{
			config_error("%s:%i: include: error downloading '%s': %s",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				 ce->ce_vardata, error);
			return -1;
		}
		else
		{
			if ((ret = load_conf(file)) >= 0)
				add_remote_include(file, ce->ce_vardata, INCLUDE_USED, NULL);
			free(file);
			return ret;
		}
	}
	else
	{
		if (errorbuf)
		{
			config_error("%s:%i: include: error downloading '%s': %s",
                                ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
                                ce->ce_vardata, errorbuf);
			return -1;
		}
		if (config_verbose > 0)
			config_status("Loading %s from download", ce->ce_vardata);
		if ((ret = load_conf(file)) >= 0)
			add_remote_include(file, ce->ce_vardata, INCLUDE_USED, NULL);
		return ret;
	}
	return 0;
}
#endif
		
void add_include(char *file)
{
	ConfigItem_include *inc;
	if (!stricmp(file, CPATH))
		return;

	for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next)
	{
		if (!(inc->flag.type & INCLUDE_NOTLOADED))
			continue;
		if (inc->flag.type & INCLUDE_REMOTE)
			continue;
		if (!stricmp(file, inc->file))
			return;
	}
	inc = MyMallocEx(sizeof(ConfigItem_include));
	inc->file = strdup(file);
	inc->flag.type = INCLUDE_NOTLOADED|INCLUDE_USED;
	AddListItem(inc, conf_include);
}

#ifdef USE_LIBCURL
void add_remote_include(char *file, char *url, int flags, char *errorbuf)
{
	ConfigItem_include *inc;

	for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next)
	{
		if (!(inc->flag.type & INCLUDE_REMOTE))
			continue;
		if (!(inc->flag.type & INCLUDE_NOTLOADED))
			continue;
		if (!stricmp(url, inc->url))
		{
			inc->flag.type |= flags;
			return;
		}
	}

	inc = MyMallocEx(sizeof(ConfigItem_include));
	if (file)
		inc->file = strdup(file);
	inc->url = strdup(url);
	inc->flag.type = (INCLUDE_NOTLOADED|INCLUDE_REMOTE|flags);
	if (errorbuf)
		inc->errorbuf = strdup(errorbuf);
	AddListItem(inc, conf_include);
}
#endif

void unload_notloaded_includes(void)
{
	ConfigItem_include *inc, *next;

	for (inc = conf_include; inc; inc = next)
	{
		next = (ConfigItem_include *)inc->next;
		if ((inc->flag.type & INCLUDE_NOTLOADED) || !(inc->flag.type & INCLUDE_USED))
		{
#ifdef USE_LIBCURL
			if (inc->flag.type & INCLUDE_REMOTE)
			{
				remove(inc->file);
				free(inc->url);
				if (inc->errorbuf)
					free(inc->errorbuf);
			}
#endif
			free(inc->file);
			DelListItem(inc, conf_include);
			free(inc);
		}
	}
}

void unload_loaded_includes(void)
{
	ConfigItem_include *inc, *next;

	for (inc = conf_include; inc; inc = next)
	{
		next = (ConfigItem_include *)inc->next;
		if (!(inc->flag.type & INCLUDE_NOTLOADED) || !(inc->flag.type & INCLUDE_USED))
		{
#ifdef USE_LIBCURL
			if (inc->flag.type & INCLUDE_REMOTE)
			{
				remove(inc->file);
				free(inc->url);
				if (inc->errorbuf)
					free(inc->errorbuf);
			}
#endif
			free(inc->file);
			DelListItem(inc, conf_include);
			free(inc);
		}
	}
}
			
void load_includes(void)
{
	ConfigItem_include *inc;

	/* Doing this for all the modules should actually be faster
	 * than only doing it for modules that are not-loaded
	 */
	for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next)
		inc->flag.type &= ~INCLUDE_NOTLOADED; 
}
