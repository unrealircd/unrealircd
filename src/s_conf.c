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
#include "h.h"
#include "inet.h"
#include "proto.h"
#ifdef _WIN32
#undef GLOBH
#endif

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

typedef struct _conf_namevalue NameValue;
struct _conf_namevalue
{
	long	flag;
	char	*name;
};


/* Config commands */

static int	_conf_admin		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_me		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_files		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_oper		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_operclass		(ConfigFile *conf, ConfigEntry *ce);
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

/*
 * Validation commands
*/

static int	_test_admin		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_me		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_files		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_oper		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_operclass		(ConfigFile *conf, ConfigEntry *ce);
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
static int	_test_deny		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_allow_channel	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_allow_dcc		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_loadmodule	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_log		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_alias		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_help		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_offchans		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_spamfilter	(ConfigFile *conf, ConfigEntry *ce);

/* This MUST be alphabetized */
static ConfigCommand _ConfigCommands[] = {
	{ "admin", 		_conf_admin,		_test_admin 	},
	{ "alias",		_conf_alias,		_test_alias	},
	{ "allow",		_conf_allow,		_test_allow	},
	{ "ban", 		_conf_ban,		_test_ban	},
	{ "class", 		_conf_class,		_test_class	},
	{ "deny",		_conf_deny,		_test_deny	},
	{ "drpass",		_conf_drpass,		_test_drpass	},
	{ "except",		_conf_except,		_test_except	},
	{ "files",		_conf_files,		_test_files	},
	{ "help",		_conf_help,		_test_help	},
	{ "include",		NULL,	  		_test_include	},
	{ "link", 		_conf_link,		_test_link	},
	{ "listen", 		_conf_listen,		_test_listen	},
	{ "loadmodule",		NULL,		 	_test_loadmodule},
	{ "log",		_conf_log,		_test_log	},
	{ "me", 		_conf_me,		_test_me	},
	{ "official-channels", 		_conf_offchans,		_test_offchans	},
	{ "oper", 		_conf_oper,		_test_oper	},
	{ "operclass",		_conf_operclass,	_test_operclass	},
	{ "set",		_conf_set,		_test_set	},
	{ "spamfilter",	_conf_spamfilter,	_test_spamfilter	},
	{ "tld",		_conf_tld,		_test_tld	},
	{ "ulines",		_conf_ulines,		_test_ulines	},
	{ "vhost", 		_conf_vhost,		_test_vhost	},
};

/* This MUST be alphabetized */
static NameValue _ListenerFlags[] = {
	{ LISTENER_CLIENTSONLY,  "clientsonly"},
	{ LISTENER_DEFER_ACCEPT, "defer-accept"},
	{ LISTENER_SERVERSONLY,  "serversonly"},
	{ LISTENER_SSL, 	 "ssl"},
	{ LISTENER_NORMAL, 	 "standard"},
};

/* This MUST be alphabetized */
static NameValue _LinkFlags[] = {
	{ CONNECT_AUTO,	"autoconnect" },
	{ CONNECT_INSECURE,	"insecure" },
	{ CONNECT_QUARANTINE, "quarantine"},
	{ CONNECT_SSL, "ssl" },
};

/* This MUST be alphabetized */
static NameValue _LogFlags[] = {
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
static NameValue ExceptTklFlags[] = {
	{ 0, "all" },
	{ TKL_GLOBAL|TKL_KILL,	"gline" },
	{ TKL_GLOBAL|TKL_NICK,	"gqline" },
	{ TKL_GLOBAL|TKL_ZAP,	"gzline" },
	{ TKL_NICK,		"qline" },
	{ TKL_GLOBAL|TKL_SHUN,	"shun" }
};

/* This MUST be alphabetized */
static NameValue _SSLFlags[] = {
	{ SSLFLAG_FAILIFNOCERT, "fail-if-no-clientcert" },
	{ SSLFLAG_DONOTACCEPTSELFSIGNED, "no-self-signed" },
	{ SSLFLAG_NOSTARTTLS, "no-starttls" },
	{ SSLFLAG_VERIFYCERT, "verify-certificate" },
};

struct {
	unsigned conf_me : 1;
	unsigned conf_admin : 1;
	unsigned conf_listen : 1;
} requiredstuff;
struct SetCheck settings;
/*
 * Utilities
*/

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
extern void add_entropy_configfile(struct stat *st, char *buf);
extern void unload_all_unused_snomasks(void);
extern void unload_all_unused_umodes(void);
extern void unload_all_unused_extcmodes(void);

extern int charsys_test_language(char *name);
extern void charsys_add_language(char *name);
extern void charsys_reset_pretest(void);
int charsys_postconftest(void);
void charsys_finish(void);
int reloadable_perm_module_unloaded(void);

/*
 * Config parser (IRCd)
*/
int			init_conf(char *rootconf, int rehash);
int			load_conf(char *filename, const char *original_path);
void			config_rehash();
int			config_run();
/*
 * Configuration linked lists
*/
ConfigItem_me		*conf_me = NULL;
ConfigItem_files	*conf_files = NULL;
ConfigItem_class 	*conf_class = NULL;
ConfigItem_class	*default_class = NULL;
ConfigItem_admin 	*conf_admin = NULL;
ConfigItem_admin	*conf_admin_tail = NULL;
ConfigItem_drpass	*conf_drpass = NULL;
ConfigItem_ulines	*conf_ulines = NULL;
ConfigItem_tld		*conf_tld = NULL;
ConfigItem_oper		*conf_oper = NULL;
ConfigItem_operclass	*conf_operclass = NULL;
ConfigItem_listen	*conf_listen = NULL;
ConfigItem_allow	*conf_allow = NULL;
ConfigItem_except	*conf_except = NULL;
ConfigItem_vhost	*conf_vhost = NULL;
ConfigItem_link		*conf_link = NULL;
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
ConfigItem_offchans	*conf_offchans = NULL;

MODVAR aConfiguration		iConf;
MODVAR aConfiguration		tempiConf;
MODVAR ConfigFile		*conf = NULL;
MODVAR int ipv6_disabled = 0;
MODVAR aClient *remote_rehash_client = NULL;

MODVAR int			config_error_flag = 0;
int			config_verbose = 0;

MODVAR int need_34_upgrade = 0;

void add_include(const char *filename, const char *included_from, int included_from_line);
#ifdef USE_LIBCURL
void add_remote_include(const char *, const char *, int, const char *, const char *included_from, int included_from_line);
void update_remote_include(ConfigItem_include *inc, const char *file, int, const char *errorbuf);
int remote_include(ConfigEntry *ce);
#endif
void unload_notloaded_includes(void);
void load_includes(void);
void unload_loaded_includes(void);
int rehash_internal(aClient *cptr, aClient *sptr, int sig);


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
	*c = '-';
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
		parambuf = strdup(params);
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
						param = Channelmode_Table[i].conv_param(param, NULL);
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
	}
	if (parambuf)
		free(parambuf);
}

void chmode_str(struct ChMode *modes, char *mbuf, char *pbuf, size_t mbuf_size, size_t pbuf_size)
{
	aCtab *tab;
	int i;

        if (!(mbuf_size && pbuf_size)) return;

	*pbuf = 0;
	*mbuf++ = '+';
	if (--mbuf_size == 0) return;
	for (tab = &cFlagTab[0]; tab->mode; tab++)
	{
		if (modes->mode & tab->mode)
		{
			if (!tab->parameters) {
				*mbuf++ = tab->flag;
				if (!--mbuf_size) {
					*--mbuf=0;
					break;
				}
                        }
		}
	}
	for (i=0; i <= Channelmode_highest; i++)
	{
		if (!(Channelmode_Table[i].flag))
			continue;

		if (modes->extmodes & Channelmode_Table[i].mode)
		{
			if (mbuf_size) {
				*mbuf++ = Channelmode_Table[i].flag;
				if (!--mbuf_size) {
					*--mbuf=0;
					break;
				}
			}
			if (Channelmode_Table[i].paracount)
			{
				strncat(pbuf, modes->extparams[i], pbuf_size-1);
				pbuf_size-=strlen(modes->extparams[i]);
				if (!pbuf_size) break;
				strncat(pbuf, " ", pbuf_size-1);
				if (!--pbuf_size) break;
			}
		}
	}
	*mbuf=0;
}

int channellevel_to_int(char *s)
{
	/* Requested at http://bugs.unrealircd.org/view.php?id=3852 */
	if (!strcmp(s, "none"))
		return CHFL_DEOPPED;
	if (!strcmp(s, "voice"))
		return CHFL_VOICE;
	if (!strcmp(s, "halfop"))
		return CHFL_HALFOP;
	if (!strcmp(s, "op") || !strcmp(s, "chanop"))
		return CHFL_CHANOP;
	if (!strcmp(s, "protect") || !strcmp(s, "chanprot"))
#ifdef PREFIX_AQ
		return CHFL_CHANPROT;
#else
		return CHFL_CHANOP|CHFL_CHANPROT;
#endif
	if (!strcmp(s, "owner") || !strcmp(s, "chanowner"))
#ifdef PREFIX_AQ
		return CHFL_CHANOWNER;
#else
		return CHFL_CHANOP|CHFL_CHANOWNER;
#endif

	return 0; /* unknown or unsupported */
}

/* Channel flag (eg: CHFL_CHANOWNER) to SJOIN symbol (eg: *).
 * WARNING: Do not confuse SJOIN symbols with prefixes in /NAMES!
 */
char *chfl_to_sjoin_symbol(int s)
{
	switch(s)
	{
		case CHFL_VOICE:
			return "+";
		case CHFL_HALFOP:
			return "%";
		case CHFL_CHANOP:
			return "@";
		case CHFL_CHANPROT:
#ifdef PREFIX_AQ
			return "~";
#else
			return "~@";
#endif
		case CHFL_CHANOWNER:
#ifdef PREFIX_AQ
			return "*";
#else
			return "*@";
#endif
		case CHFL_DEOPPED:
		default:
			return "";
	}
	/* NOT REACHED */
}

char chfl_to_chanmode(int s)
{
	switch(s)
	{
		case CHFL_VOICE:
			return 'v';
		case CHFL_HALFOP:
			return 'h';
		case CHFL_CHANOP:
			return 'o';
		case CHFL_CHANPROT:
			return 'a';
		case CHFL_CHANOWNER:
			return 'q';
		case CHFL_DEOPPED:
		default:
			return '\0';
	}
	/* NOT REACHED */
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
		close(fd);
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
	add_entropy_configfile(&sb, buf);
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

/** Remove quotes so that 'hello \"all\" \\ lala' becomes 'hello "all" \ lala' */
void unreal_delquotes(char *i)
{
	char *o;

	for (o = i; *i; i++)
	{
		if (*i == '\\')
		{
			if ((i[1] == '\\') || (i[1] == '"'))
			{
				i++; /* skip original \ */
				if (*i == '\0')
					break;
			}
		}
		*o++ = *i;
	}
	*o = '\0';
}

/* This is the internal parser, made by Chris Behrens & Fred Jacobs */
static ConfigFile *config_parse(char *filename, char *confdata)
{
	char		*ptr;
	char		*start;
	int		linenumber = 1;
	int errors = 0;
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
					config_error("%s:%i: New section start detected on line %d but the section has no name. "
					             "Sections should start with a name like 'oper {' or 'set {'.",
							filename, linenumber, linenumber);
					errors++;
					continue;
				}
				else if (curce->ce_entries)
				{
					config_error("%s:%i: New section start but previous section did not end properly. "
					             "Check line %d and the line(s) before, you are likely missing a '};' there.\n",
							filename, linenumber, linenumber);
					errors++;
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
					config_error("%s:%i: Missing semicolon (';') before close brace. Check line %d and the line(s) before.\n",
						filename, linenumber, linenumber);
					config_entry_free(curce);
					config_free(curcf);
					errors++;
					return NULL;
				}
				else if (!cursection)
				{
					config_error("%s:%i: You have a close brace ('};') too many. "
					              "Check line %d AND the lines above it from the previous block.\n",
						filename, linenumber, linenumber);
					errors++;
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
						config_error("%s:%i Comment on line %d does not end\n",
							filename, commentstart, commentstart);
						errors++;
						config_entry_free(curce);
						config_free(curcf);
						return NULL;
					}
				}
				break;
			case '\"':
				if (curce && curce->ce_varlinenum != linenumber && cursection)
				{
					config_error("%s:%i: Missing semicolon (';') at end of line. "
					             "Line %d must end with a ; character\n",
						filename, curce->ce_varlinenum, curce->ce_varlinenum);
					errors++;

					*lastce = curce;
					lastce = &(curce->ce_next);
					curce->ce_fileposend = (ptr - confdata);
					curce = NULL;
				}

				start = ++ptr;
				for(;*ptr;ptr++)
				{
					if (*ptr == '\\')
					{
						if ((ptr[1] == '\\') || (ptr[1] == '"'))
						{
							/* \\ or \" in config file (escaped) */
							ptr++; /* skip */
							continue;
						}
					}
					else if ((*ptr == '\"') || (*ptr == '\n'))
						break;
				}
				if (!*ptr || (*ptr == '\n'))
				{
					config_error("%s:%i: Unterminated quote found\n",
							filename, linenumber);
					errors++;
					config_entry_free(curce);
					config_free(curcf);
					return NULL;
				}
				if (curce)
				{
					if (curce->ce_vardata)
					{
						config_error("%s:%i: Extra data detected. Perhaps missing a ';' or one too many?\n",
							filename, linenumber);
						errors++;
					}
					else
					{
						curce->ce_vardata = MyMalloc(ptr-start+1);
						strlcpy(curce->ce_vardata, start, ptr-start+1);
						unreal_delquotes(curce->ce_vardata);
					}
				}
				else
				{
					curce = MyMalloc(sizeof(ConfigEntry));
					memset(curce, 0, sizeof(ConfigEntry));
					curce->ce_varname = MyMalloc((ptr-start)+1);
					strlcpy(curce->ce_varname, start, ptr-start+1);
					unreal_delquotes(curce->ce_varname);
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
						config_error("%s: End of file reached but directive or block at line %i did not end properly. "
									 "Perhaps a missing ; (semicolon) somewhere?\n",
							filename, curce->ce_varlinenum);
					else if (cursection)
						config_error("%s: End of file reached but the section which starts at line %i did never end properly. "
									 "Perhaps a missing }; ?\n",
								filename, cursection->ce_sectlinenum);
					else
						config_error("%s: Unexpected end of file. Some line or block did not end properly. "
						             "Look for any missing } and };\n", filename);
					errors++;
					config_entry_free(curce);
					config_free(curcf);
					return NULL;
				}
				if (curce)
				{
					if (curce->ce_vardata)
					{
						config_error("%s:%i: Extra data detected. Check for a missing ; character at or around line %d\n",
							filename, linenumber, linenumber-1);
						errors++;
					}
					else
					{
						curce->ce_vardata = MyMalloc(ptr-start+1);
						strlcpy(curce->ce_vardata, start, ptr-start+1);
					}
				}
				else
				{
					curce = MyMalloc(sizeof(ConfigEntry));
					memset(curce, 0, sizeof(ConfigEntry));
					curce->ce_varname = MyMalloc((ptr-start)+1);
					strlcpy(curce->ce_varname, start, ptr-start+1);
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
		config_error("%s: End of file reached but directive or block at line %i did not end properly. "
		             "Perhaps a missing ; (semicolon) somewhere?\n",
			filename, curce->ce_varlinenum);
		errors++;
		config_entry_free(curce);
	}
	else if (cursection)
	{
		config_error("%s: End of file reached but the section which starts at line %i did never end properly. "
		             "Perhaps a missing }; ?\n",
				filename, cursection->ce_sectlinenum);
		errors++;
	}

	if (errors)
	{
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
	vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
#ifdef _WIN32
	if (!loop.ircd_booted)
		win_log("[error] %s", buffer);
#endif
	ircd_log(LOG_ERROR, "config error: %s", buffer);
	sendto_realops("error: %s", buffer);
	if (remote_rehash_client)
		sendto_one(remote_rehash_client, ":%s NOTICE %s :error: %s", me.name, remote_rehash_client->name, buffer);
	/* We cannot live with this */
	config_error_flag = 1;
}

void config_error_missing(const char *filename, int line, const char *entry)
{
	config_error("%s:%d: %s is missing", filename, line, entry);
}

void config_error_unknown(const char *filename, int line, const char *block,
	const char *entry)
{
	config_error("%s:%d: Unknown directive '%s::%s'", filename, line, block, entry);
}

void config_error_unknownflag(const char *filename, int line, const char *block,
	const char *entry)
{
	config_error("%s:%d: Unknown %s flag '%s'", filename, line, block, entry);
}

void config_error_unknownopt(const char *filename, int line, const char *block,
	const char *entry)
{
	config_error("%s:%d: Unknown %s option '%s'", filename, line, block, entry);
}

void config_error_noname(const char *filename, int line, const char *block)
{
	config_error("%s:%d: %s block has no name", filename, line, block);
}

void config_error_blank(const char *filename, int line, const char *block)
{
	config_error("%s:%d: Blank %s entry", filename, line, block);
}

void config_error_empty(const char *filename, int line, const char *block,
	const char *entry)
{
	config_error("%s:%d: %s::%s specified without a value",
		filename, line, block, entry);
}

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
#ifdef _WIN32
	if (!loop.ircd_booted)
		win_log("* %s", buffer);
#endif
	ircd_log(LOG_ERROR, "%s", buffer);
	sendto_realops("%s", buffer);
	if (remote_rehash_client)
		sendto_one(remote_rehash_client, ":%s NOTICE %s :%s", me.name, remote_rehash_client->name, buffer);
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
#ifdef _WIN32
	if (!loop.ircd_booted)
		win_log("[warning] %s", buffer);
#endif
	ircd_log(LOG_ERROR, "[warning] %s", buffer);
	sendto_realops("[warning] %s", buffer);
	if (remote_rehash_client)
		sendto_one(remote_rehash_client, ":%s NOTICE %s :[warning] %s", me.name, remote_rehash_client->name, buffer);
}

void config_warn_duplicate(const char *filename, int line, const char *entry)
{
	config_warn("%s:%d: Duplicate %s directive", filename, line, entry);
}

/* returns 1 if the test fails */
int config_test_openfile(ConfigEntry *cep, int flags, mode_t mode, const char *entry, int fatal, int allow_url)
{
	int fd;

	if(!cep->ce_vardata)
	{
		if(fatal)
			config_error("%s:%i: %s: <no file specified>: no file specified",
				     cep->ce_fileptr->cf_filename,
				     cep->ce_varlinenum,
				     entry);
		else

			config_warn("%s:%i: %s: <no file specified>: no file specified",
				    cep->ce_fileptr->cf_filename,
				    cep->ce_varlinenum,
				    entry);
		return 1;
	}

	/* There's not much checking that can be done for asynchronously downloaded files */
#ifdef USE_LIBCURL
	if(url_is_valid(cep->ce_vardata))
	{
		if(allow_url)
			return 0;

		/* but we can check if a URL is used wrongly :-) */
		config_warn("%s:%i: %s: %s: URL used where not allowed",
			    cep->ce_fileptr->cf_filename,
			    cep->ce_varlinenum,
			    entry, cep->ce_vardata);
		if(fatal)
			return 1;
		else
			return 0;
	}
#endif /* USE_LIBCURL */

	/*
	 * Make sure that files are created with the correct mode. This is
	 * because we don't feel like unlink()ing them...which would require
	 * stat()ing them to make sure that we don't delete existing ones
	 * and that we deal with all of the bugs that come with complexity.
	 * The only files we may be creating are the tunefile and pidfile so far.
	 */
	if(flags & O_CREAT)
		fd = open(cep->ce_vardata, flags, mode);
	else
		fd = open(cep->ce_vardata, flags);
	if(fd == -1)
	{
		if(fatal)
			config_error("%s:%i: %s: %s: %s",
				     cep->ce_fileptr->cf_filename,
				     cep->ce_varlinenum,
				     entry,
				     cep->ce_vardata,
				     strerror(errno));
		else
			config_warn("%s:%i: %s: %s: %s",
				     cep->ce_fileptr->cf_filename,
				     cep->ce_varlinenum,
				     entry,
				     cep->ce_vardata,
				     strerror(errno));
		return 1;
	}
	close(fd);
	return 0;
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
#ifdef _WIN32
	if (!loop.ircd_booted)
		win_log("* %s", buffer);
#endif
	sendto_realops("%s", buffer);
}

int config_is_blankorempty(ConfigEntry *cep, const char *block)
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
	safefree(i->kline_address);
	safefree(i->gline_address);
	safefree(i->auto_join_chans);
	safefree(i->oper_auto_join_chans);
	safefree(i->oper_only_stats);
	safefree(i->channel_command_prefix);
	safefree(i->oper_snomask);
	safefree(i->user_snomask);
	safefree(i->egd_path);
	safefree(i->static_quit);
	safefree(i->x_server_cert_pem);
	safefree(i->x_server_key_pem);
	safefree(i->x_server_cipher_list);
	safefree(i->trusted_ca_file);
	safefree(i->restrict_usermodes);
	safefree(i->restrict_channelmodes);
	safefree(i->restrict_extendedbans);
	safefree(i->network.x_ircnetwork);
	safefree(i->network.x_ircnet005);
	safefree(i->network.x_defserv);
	safefree(i->network.x_services_name);
	safefree(i->network.x_hidden_host);
	safefree(i->network.x_prefix_quit);
	safefree(i->network.x_helpchan);
	safefree(i->network.x_stats_server);
	safefree(i->spamfilter_ban_reason);
	safefree(i->spamfilter_virus_help_channel);
	safefree(i->spamexcept_line);
	safefree(i->timesynch_server);
	safefree(i->link_bindip);
}

int	config_test();

void config_setdefaultsettings(aConfiguration *i)
{
	char tmp[512];

	i->unknown_flood_amount = 4;
	i->unknown_flood_bantime = 600;
	i->oper_snomask = strdup(SNO_DEFOPER);
	i->ident_read_timeout = 30;
	i->ident_connect_timeout = 3;
	i->nick_count = 3; i->nick_period = 60; /* nickflood protection: max 3 per 60s */
#ifdef NO_FLOOD_AWAY
	i->away_count = 4; i->away_period = 120; /* awayflood protection: max 4 per 120s */
#endif
	i->throttle_count = 3; i->throttle_period = 60; /* throttle protection: max 3 per 60s */
	i->modef_default_unsettime = 0;
	i->modef_max_unsettime = 60; /* 1 hour seems enough :p */
	i->ban_version_tkl_time = 86400; /* 1d */
	i->spamfilter_ban_time = 86400; /* 1d */
	i->spamfilter_ban_reason = strdup("Spam/advertising");
	i->spamfilter_virus_help_channel = strdup("#help");
	i->spamfilter_detectslow_warn = 250;
	i->spamfilter_detectslow_fatal = 500;
	i->spamfilter_stop_on_first_match = 1;
	i->maxdccallow = 10;
	i->channel_command_prefix = strdup("`!.");
	i->check_target_nick_bans = 1;
	i->maxbans = 60;
	i->maxbanlength = 2048;
	i->timesynch_enabled = 1;
	i->timesynch_timeout = 3;
	i->timesynch_server = strdup("193.67.79.202,192.43.244.18,128.250.36.3"); /* nlnet (EU), NIST (US), uni melbourne (AU). All open acces, nonotify, nodns. */
	i->level_on_join = CHFL_CHANOP;
	i->watch_away_notification = 1;
	i->new_linking_protocol = 1;
	i->uhnames = 1;
	i->ping_cookie = 1;
	i->default_ipv6_clone_mask = 64;
	i->nicklen = NICKLEN;
	i->link_bindip = strdup("*");
	i->oper_only_stats = strdup("*");
	snprintf(tmp, sizeof(tmp), "%s/ssl/server.cert.pem", CONFDIR);
	i->x_server_cert_pem = strdup(tmp);
	snprintf(tmp, sizeof(tmp), "%s/ssl/server.key.pem", CONFDIR);
	i->x_server_key_pem = strdup(tmp);
	if (!ipv6_capable())
		DISABLE_IPV6 = 1;
	i->network.x_prefix_quit = strdup("Quit");
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
		if (UHNAMES_ENABLED)
		{
			IsupportAdd(NULL, "UHNAMES", NULL);
		} else {
			Isupport *hunted = IsupportFind("UHNAMES");
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
	ca->logfd = -1;
	AddListItem(ca, conf_log);
}

int isanyserverlinked(void)
{
	return !list_empty(&server_list);
}

void applymeblock(void)
{
	if (!conf_me)
		return; /* uh-huh? */

	/* Info text may always change, just wouldn't show up on other servers, that's all.. */
	strlcpy(me.info, conf_me->info, sizeof(me.info));

	/* Name can only be set once (on boot) */
	if (!*me.name)
		strlcpy(me.name, conf_me->name, sizeof(me.name));

	if (!*me.id)
		strlcpy(me.id, conf_me->sid, sizeof(me.id));
}

void upgrade_conf_to_34(void)
{
	config_error("******************************************************************");
	config_error("This *seems* an UnrealIRCd 3.2.x configuration file.");

#ifdef _WIN32
	if (!IsService)
		config_error("In next screen you will be prompted to automatically upgrade the configuration file(s).");
	else
	{
		config_error("We offer a configuration file converter to convert 3.2.x conf's to 4.0, however this "
		             "is not available when running as a service. If you want to use it, make UnrealIRCd "
		             "run in GUI mode by running 'unreal uninstall'. Then start UnrealIRCd.exe and when "
		             "it prompts you to convert the configuration click 'Yes'. Check if UnrealIRCd boots properly. "
		             "Once everything is looking good you can run 'unreal install' to make UnrealIRCd run "
		             "as a service again."); /* TODO: make this unnecessary :D */
	}
#else
	config_error("To upgrade it to the new 4.0 format, run: ./unrealircd upgrade-conf");
#endif

	config_error("******************************************************************");
	/* TODO: win32 may require a different error */
}

int	init_conf(char *rootconf, int rehash)
{
	char *old_pid_file = NULL;

	config_status("Loading IRCd configuration..");
	if (conf)
	{
		config_error("%s:%i - Someone forgot to clean up", __FILE__, __LINE__);
		return -1;
	}
	bzero(&tempiConf, sizeof(iConf));
	bzero(&settings, sizeof(settings));
	bzero(&requiredstuff, sizeof(requiredstuff));
	config_setdefaultsettings(&tempiConf);
	/*
	 * the rootconf must be listed in the conf_include for include
	 * recursion prevention code and sanity checking code to be
	 * made happy :-). Think of it as us implicitly making an
	 * in-memory config file that looks like:
	 *
	 * include "unrealircd.conf";
	 */
	add_include(rootconf, "[thin air]", -1);
	if (load_conf(rootconf, rootconf) > 0)
	{
		charsys_reset_pretest();
		if ((config_test() < 0) || (callbacks_check() < 0) || (efunctions_check() < 0) ||
		    (charsys_postconftest() < 0) || ssl_used_in_config_but_unavail() || reloadable_perm_module_unloaded())
		{
			config_error("IRCd configuration failed to pass testing");
#ifdef _WIN32
			if (!rehash)
				win_error();
#endif
			Unload_all_testing_modules();
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
			safestrdup(old_pid_file, conf_files->pid_file);
			unrealdns_delasyncconnects();
			config_rehash();
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
			unload_loaded_includes();
		}
		load_includes();
		Init_all_testing_modules();
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
		if (old_pid_file && strcmp(old_pid_file, conf_files->pid_file))
		{
			sendto_ops("pidfile is being rewritten to %s, please delete %s",
				   conf_files->pid_file,
				   old_pid_file);
			write_pidfile();
		}
		safefree(old_pid_file);
	}
	else
	{
		config_error("IRCd configuration failed to load");
		Unload_all_testing_modules();
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
		module_loadall();
		RunHook0(HOOKTYPE_REHASH_COMPLETE);
	}
	do_weird_shun_stuff();
	if (!conf_log)
		make_default_logblock();
	config_status("Configuration loaded without any problems.");
	return 0;
}

/**
 * Processes filename as part of the IRCd's configuration.
 *
 * One _must_ call add_include() or add_remote_include() before
 * calling load_conf(). This way, include recursion may be detected
 * and reported to the user as an error instead of causing the IRCd to
 * hang in an infinite recursion, eat up memory, and eventually
 * overflow its stack ;-). (reported by warg).
 *
 * This function will set INCLUDE_USED on the config_include list
 * entry if the config file loaded without error.
 *
 * @param filename the file where the conf may be read from
 * @param original_path the path or URL used to refer to this file.
 *        (mostly to support remote includes' URIs for recursive include detection).
 * @return 1 on success, a negative number on error
 */
int	load_conf(char *filename, const char *original_path)
{
	ConfigFile 	*cfptr, *cfptr2, **cfptr3;
	ConfigEntry 	*ce;
	ConfigItem_include *inc, *my_inc;
	int ret;
	int fatal_ret;
	int counter;

	if (config_verbose > 0)
		config_status("Loading config file %s ..", filename);

	need_34_upgrade = 0;

	/*
	 * Check if we're accidentally including a file a second
	 * time. We should expect to find one entry in this list: the
	 * entry for our current file.
	 */
	counter = 0;
	my_inc = NULL;
	for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next)
	{
		/*
		 * ignore files which were part of a _previous_
		 * successful rehash.
		 */
		if (!(inc->flag.type & INCLUDE_NOTLOADED))
			continue;

		if (!counter)
			my_inc = inc;

		if (!strcmp(filename, inc->file))
		{
			counter ++;
			continue;
		}
#ifdef _WIN32
		if (!stricmp(filename, inc->file))
		{
			counter ++;
			continue;
		}
#endif
#ifdef USE_LIBCURL
		if (inc->url && !strcmp(original_path, inc->url))
		{
			counter ++;
			continue;
		}
#endif
	}
	if (counter < 1 || !my_inc)
	{
		/*
		 * The following is simply for debugging/[sanity
		 * checking]. To make sure that functions call
		 * add_include() or add_remote_include() before
		 * calling us.
		 */
		config_error("I don't have a record for %s being included."
			     " Perhaps someone forgot to call add_include()?",
			     filename);
		abort();
	}
	if (counter > 1 || my_inc->flag.type & INCLUDE_USED)
	{
		config_error("%s:%d:include: Config file %s has been loaded before %d time."
			     " You may include each file only once.",
			     my_inc->included_from, my_inc->included_from_line,
			     filename, counter - 1);
		return -1;
	}
	/* end include recursion checking code */

	if ((cfptr = config_load(filename)))
	{
		for (cfptr3 = &conf, cfptr2 = conf; cfptr2; cfptr2 = cfptr2->cf_next)
			cfptr3 = &cfptr2->cf_next;
		*cfptr3 = cfptr;

		/* Load modules */
		if (config_verbose > 1)
			config_status("Loading modules in %s", filename);

		fatal_ret = 0;
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
			if (!strcmp(ce->ce_varname, "loadmodule"))
			{
				 ret = _conf_loadmodule(cfptr, ce);
				 if (ret < fatal_ret)
				 	fatal_ret = ret; /* lowest wins */
			}
		ret = fatal_ret;
		if (need_34_upgrade)
			upgrade_conf_to_34();
		if (ret < 0)
			return ret;

		/* Load includes */
		if (config_verbose > 1)
			config_status("Searching through %s for include files..", filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
			if (!strcmp(ce->ce_varname, "include"))
			{
				 ret = _conf_include(cfptr, ce);
				 if (need_34_upgrade)
				 	upgrade_conf_to_34();
				 if (ret < 0)
					 	return ret;
			}
		my_inc->flag.type |= INCLUDE_USED;
		return 1;
	}
	else
	{
		config_error("Could not load config file %s", filename);
#ifdef _WIN32
		if (!strcmp(filename, "conf/unrealircd.conf"))
		{
			if (file_exists("unrealircd.conf"))
			{
				config_error("Note that 'unrealircd.conf' now belongs in the 'conf' subdirectory! (So move it to there)");
			} else {
				config_error("New to UnrealIRCd? Be sure to read https://www.unrealircd.org/docs/Installing_%%28Windows%%29");
			}
		}
#endif
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
	ConfigItem_listen	 	*listen_ptr;
	ConfigItem_tld			*tld_ptr;
	ConfigItem_vhost		*vhost_ptr;
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

	for (admin_ptr = conf_admin; admin_ptr; admin_ptr = (ConfigItem_admin *)next)
	{
		next = (ListStruct *)admin_ptr->next;
		safefree(admin_ptr->line);
		DelListItem(admin_ptr, conf_admin);
		MyFree(admin_ptr);
	}

	for (oper_ptr = conf_oper; oper_ptr; oper_ptr = (ConfigItem_oper *)next)
	{
		ConfigItem_mask *oper_mask;
		SWhois *s, *s_next;
		next = (ListStruct *)oper_ptr->next;
		safefree(oper_ptr->name);
		safefree(oper_ptr->snomask);
		safefree(oper_ptr->operclass);
		safefree(oper_ptr->vhost);
		Auth_DeleteAuthStruct(oper_ptr->auth);
		unreal_delete_masks(oper_ptr->mask);
		DelListItem(oper_ptr, conf_oper);
		for (s = oper_ptr->swhois; s; s = s_next)
		{
			s_next = s->next;
			safefree(s->line);
			safefree(s->setby);
			MyFree(s);
		}
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
		safefree(uline_ptr->servername);
		DelListItem(uline_ptr, conf_ulines);
		MyFree(uline_ptr);
	}
	for (allow_ptr = conf_allow; allow_ptr; allow_ptr = (ConfigItem_allow *) next)
	{
		next = (ListStruct *)allow_ptr->next;
		safefree(allow_ptr->ip);
		safefree(allow_ptr->hostname);
		Auth_DeleteAuthStruct(allow_ptr->auth);
		DelListItem(allow_ptr, conf_allow);
		MyFree(allow_ptr);
	}
	for (except_ptr = conf_except; except_ptr; except_ptr = (ConfigItem_except *) next)
	{
		next = (ListStruct *)except_ptr->next;
		safefree(except_ptr->mask);
		DelListItem(except_ptr, conf_except);
		MyFree(except_ptr);
	}
	for (ban_ptr = conf_ban; ban_ptr; ban_ptr = (ConfigItem_ban *) next)
	{
		next = (ListStruct *)ban_ptr->next;
		if (ban_ptr->flag.type2 == CONF_BAN_TYPE_CONF || ban_ptr->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
		{
			safefree(ban_ptr->mask);
			safefree(ban_ptr->reason);
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
		next = (ListStruct *)tld_ptr->next;
		safefree(tld_ptr->motd_file);
		safefree(tld_ptr->rules_file);
		safefree(tld_ptr->smotd_file);
		safefree(tld_ptr->opermotd_file);
		safefree(tld_ptr->botmotd_file);

		free_motd(&tld_ptr->motd);
		free_motd(&tld_ptr->rules);
		free_motd(&tld_ptr->smotd);
		free_motd(&tld_ptr->opermotd);
		free_motd(&tld_ptr->botmotd);

		DelListItem(tld_ptr, conf_tld);
		MyFree(tld_ptr);
	}
	for (vhost_ptr = conf_vhost; vhost_ptr; vhost_ptr = (ConfigItem_vhost *) next)
	{
		ConfigItem_mask *vhost_mask;
		SWhois *s, *s_next;

		next = (ListStruct *)vhost_ptr->next;

		safefree(vhost_ptr->login);
		Auth_DeleteAuthStruct(vhost_ptr->auth);
		safefree(vhost_ptr->virthost);
		safefree(vhost_ptr->virtuser);
		unreal_delete_masks(vhost_ptr->mask);
		for (s = vhost_ptr->swhois; s; s = s_next)
		{
			s_next = s->next;
			safefree(s->line);
			safefree(s->setby);
			MyFree(s);
		}
		DelListItem(vhost_ptr, conf_vhost);
		MyFree(vhost_ptr);
	}

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
			safefree(deny_dcc_ptr->filename);
			safefree(deny_dcc_ptr->reason);
			DelListItem(deny_dcc_ptr, conf_deny_dcc);
			MyFree(deny_dcc_ptr);
		}
	}
	for (deny_link_ptr = conf_deny_link; deny_link_ptr; deny_link_ptr = (ConfigItem_deny_link *) next) {
		next = (ListStruct *)deny_link_ptr->next;
		safefree(deny_link_ptr->prettyrule);
		safefree(deny_link_ptr->mask);
		crule_free(&deny_link_ptr->rule);
		DelListItem(deny_link_ptr, conf_deny_link);
		MyFree(deny_link_ptr);
	}
	for (deny_version_ptr = conf_deny_version; deny_version_ptr; deny_version_ptr = (ConfigItem_deny_version *) next) {
		next = (ListStruct *)deny_version_ptr->next;
		safefree(deny_version_ptr->mask);
		safefree(deny_version_ptr->version);
		safefree(deny_version_ptr->flags);
		DelListItem(deny_version_ptr, conf_deny_version);
		MyFree(deny_version_ptr);
	}
	for (deny_channel_ptr = conf_deny_channel; deny_channel_ptr; deny_channel_ptr = (ConfigItem_deny_channel *) next)
	{
		next = (ListStruct *)deny_channel_ptr->next;
		safefree(deny_channel_ptr->redirect);
		safefree(deny_channel_ptr->channel);
		safefree(deny_channel_ptr->reason);
		safefree(deny_channel_ptr->class);
		DelListItem(deny_channel_ptr, conf_deny_channel);
		MyFree(deny_channel_ptr);
	}

	for (allow_channel_ptr = conf_allow_channel; allow_channel_ptr; allow_channel_ptr = (ConfigItem_allow_channel *) next)
	{
		next = (ListStruct *)allow_channel_ptr->next;
		safefree(allow_channel_ptr->channel);
		safefree(allow_channel_ptr->class);
		DelListItem(allow_channel_ptr, conf_allow_channel);
		MyFree(allow_channel_ptr);
	}
	for (allow_dcc_ptr = conf_allow_dcc; allow_dcc_ptr; allow_dcc_ptr = (ConfigItem_allow_dcc *)next)
	{
		next = (ListStruct *)allow_dcc_ptr->next;
		if (allow_dcc_ptr->flag.type2 == CONF_BAN_TYPE_CONF)
		{
			safefree(allow_dcc_ptr->filename);
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
		safefree(conf_drpass);
	}
	for (log_ptr = conf_log; log_ptr; log_ptr = (ConfigItem_log *)next) {
		next = (ListStruct *)log_ptr->next;
		if (log_ptr->logfd != -1)
			fd_close(log_ptr->logfd);
		safefree(log_ptr->file);
		DelListItem(log_ptr, conf_log);
		MyFree(log_ptr);
	}
	for (alias_ptr = conf_alias; alias_ptr; alias_ptr = (ConfigItem_alias *)next) {
		aCommand *cmptr = find_Command(alias_ptr->alias, 0, 0);
		ConfigItem_alias_format *fmt;
		next = (ListStruct *)alias_ptr->next;
		safefree(alias_ptr->nick);
		if (cmptr)
			del_Command(alias_ptr->alias, cmptr->func);
		safefree(alias_ptr->alias);
		if (alias_ptr->format && (alias_ptr->type == ALIAS_COMMAND)) {
			for (fmt = (ConfigItem_alias_format *) alias_ptr->format; fmt; fmt = (ConfigItem_alias_format *) next2)
			{
				next2 = (ListStruct *)fmt->next;
				safefree(fmt->format);
				safefree(fmt->nick);
				safefree(fmt->parameters);
				unreal_delete_match(fmt->expr);
				DelListItem(fmt, alias_ptr->format);
				MyFree(fmt);
			}
		}
		DelListItem(alias_ptr, conf_alias);
		MyFree(alias_ptr);
	}
	for (help_ptr = conf_help; help_ptr; help_ptr = (ConfigItem_help *)next) {
		aMotdLine *text;
		next = (ListStruct *)help_ptr->next;
		safefree(help_ptr->command);
		while (help_ptr->text) {
			text = help_ptr->text->next;
			safefree(help_ptr->text->line);
			safefree(help_ptr->text);
			help_ptr->text = text;
		}
		DelListItem(help_ptr, conf_help);
		MyFree(help_ptr);
	}
	for (os_ptr = iConf.oper_only_stats_ext; os_ptr; os_ptr = (OperStat *)next)
	{
		next = (ListStruct *)os_ptr->next;
		safefree(os_ptr->flag);
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
		safefree(of_ptr->topic);
		MyFree(of_ptr);
	}
	conf_offchans = NULL;

	for (i = 0; i < EXTCMODETABLESZ; i++)
	{
		if (iConf.modes_on_join.extparams[i])
			free(iConf.modes_on_join.extparams[i]);
	}
	
	/*
	  reset conf_files -- should this be in its own function? no, because
	  it's only used here
	 */
	safefree(conf_files->motd_file);
	safefree(conf_files->smotd_file);
	safefree(conf_files->opermotd_file);
	safefree(conf_files->svsmotd_file);
	safefree(conf_files->botmotd_file);
	safefree(conf_files->rules_file);
	safefree(conf_files->pid_file);
	safefree(conf_files->tune_file);
	/*
	   Don't free conf_files->pid_file here; the old value is used to determine if
	   the pidfile location has changed and write_pidfile() needs to be called
	   again.
	*/
	safefree(conf_files);
	conf_files = NULL;
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
	if (!settings.has_services_server)
		Error("set::services-server is missing");
	if (!settings.has_default_server)
		Error("set::default-server is missing");
	if (!settings.has_network_name)
		Error("set::network-name is missing");
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
	ConfigItem_allow *allow;

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
	/*
	 * transfer default values from set::ipv6_clones_mask into
	 * each individual allow block. If other similar things like
	 * this stack up here, perhaps this shoul be moved to another
	 * function.
	 */
	for(allow = conf_allow; allow; allow = (ConfigItem_allow *)allow->next)
		if(!allow->ipv6_clone_mask)
			allow->ipv6_clone_mask = tempiConf.default_ipv6_clone_mask;

	close_listeners();
	listen_cleanup();
	close_listeners();
	loop.do_bancheck = 1;
	free_iConf(&iConf);
	bcopy(&tempiConf, &iConf, sizeof(aConfiguration));
	bzero(&tempiConf, sizeof(aConfiguration));

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


	/* initialize conf_files with defaults if the block isn't set: */
	if(!conf_files)
	  _conf_files(NULL, NULL);

	if (errors > 0)
	{
		config_error("%i fatal errors encountered", errors);
	}
	return (errors > 0 ? -1 : 1);
}


NameValue *config_binary_flags_search(NameValue *table, char *cmd, int size) {
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

	need_34_upgrade = 0;

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

	if (need_34_upgrade)
	{
		upgrade_conf_to_34();
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

ConfigItem_operclass *Find_operclass(char *name)
{
	ConfigItem_operclass *p;
	if (!name)
		return NULL;

	for (p = conf_operclass; p; p= (ConfigItem_operclass *) p->next)
	{
		if (!strcmp(name,p->classStruct->name))
			return (p);
	}
	return NULL;
}

int count_oper_sessions(char *name)
{
	int count = 0;
	aClient *cptr;

	list_for_each_entry(cptr, &oper_list, special_node)
	{
		if (cptr->user->operlogin != NULL && !strcmp(cptr->user->operlogin, name))
			count++;
	}

	return count;
}

ConfigItem_listen *Find_listen(char *ipmask, int port, int ipv6)
{
	ConfigItem_listen	*p;

	if (!ipmask)
		return NULL;

	for (p = conf_listen; p; p = (ConfigItem_listen *) p->next)
	{
		if (p->ipv6 != ipv6)
			continue;

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


ConfigItem_except *Find_except(aClient *sptr, short type)
{
	ConfigItem_except *excepts;

	for(excepts = conf_except; excepts; excepts =(ConfigItem_except *) excepts->next)
	{
		if (excepts->flag.type == type)
		{
			if (match_user(excepts->mask, sptr, MATCH_CHECK_REAL))
				return excepts;
		}
	}
	return NULL;
}

ConfigItem_tld *Find_tld(aClient *cptr)
{
	ConfigItem_tld *tld;

	for (tld = conf_tld; tld; tld = (ConfigItem_tld *) tld->next)
	{
		if (match_user(tld->mask, cptr, MATCH_CHECK_REAL))
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


ConfigItem_link *Find_link(char *servername, aClient *acptr)
{
	ConfigItem_link	*link;

	for (link = conf_link; link; link = (ConfigItem_link *)link->next)
	{
		if (!match(link->servername, servername) && unreal_mask_match(acptr, link->incoming.mask))
		{
		    return link;
		}
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
				if (match_user(ban->mask, sptr, MATCH_CHECK_REAL))
				{
					/* Person got a exception */
					if ((type == CONF_BAN_USER || type == CONF_BAN_IP)
					    && Find_except(sptr, CONF_EXCEPT_BAN))
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
				if (match_user(ban->mask, sptr, MATCH_CHECK_REAL))
				{
					/* Person got a exception */
					if (Find_except(sptr, type))
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
		if (aconf->auth && !cptr->local->passwd && aconf->flags.nopasscont)
			continue;
		if (aconf->flags.ssl && !IsSecure(cptr))
			continue;
		if (hp && hp->h_name)
		{
			hname = hp->h_name;
			strlcpy(fullname, hname, sizeof(fullname));
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
				strlcpy(uhost, username, sizeof(uhost));
			else
				strlcpy(uhost, cptr->username, sizeof(uhost));
			(void)strlcat(uhost, "@", sizeof(uhost));
		}
		else
			*uhost = '\0';
		strlcat(uhost, sockhost, sizeof(uhost));
		/* Check the IP */
		if (match_user(aconf->ip, cptr, MATCH_CHECK_IP))
			goto attach;

		/* Hmm, localhost is a special case, hp == NULL and sockhost contains
		 * 'localhost' instead of an ip... -- Syzop. */
		if (!strcmp(sockhost, "localhost"))
		{
			if (index(aconf->hostname, '@'))
			{
				if (aconf->flags.noident)
					strlcpy(uhost, username, sizeof(uhost));
				else
					strlcpy(uhost, cptr->username, sizeof(uhost));
				strlcat(uhost, "@localhost", sizeof(uhost));
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
			strlcpy(uhost, fullname, sizeof(uhost));
		else
			strlcpy(uhost, sockhost, sizeof(uhost));
		set_sockhost(cptr, uhost);

		if (aconf->maxperip)
		{
			aClient *acptr, *acptr2;

			ii = 1;
			list_for_each_entry_safe(acptr, acptr2, &lclient_list, lclient_node)
			{
				if (!strcmp(GetIP(acptr), GetIP(cptr)))
				{
					ii++;
					if (ii > aconf->maxperip)
					{
						exit_client(cptr, cptr, &me,
							"Too many connections from your IP");
						return -5;	/* Already got too many with that ip# */
					}
				}
			}
		}
		if ((i = Auth_Check(cptr, aconf->auth, cptr->local->passwd)) == -1)
		{
			exit_client(cptr, cptr, &me,
				"Password mismatch");
			return -5;
		}
		if ((i == 2) && (cptr->local->passwd))
		{
			MyFree(cptr->local->passwd);
			cptr->local->passwd = NULL;
		}
		if (!((aconf->class->clients + 1) > aconf->class->maxclients))
		{
			cptr->local->class = aconf->class;
			cptr->local->class->clients++;
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
ConfigItem_deny_channel *Find_channel_allowed(aClient *cptr, char *name)
{
	ConfigItem_deny_channel *dchannel;
	ConfigItem_allow_channel *achannel;

	for (dchannel = conf_deny_channel; dchannel; dchannel = (ConfigItem_deny_channel *)dchannel->next)
	{
		if (!match(dchannel->channel, name) && (dchannel->class ? !strcmp(cptr->local->class->name, dchannel->class) : 1))
			break;
	}
	if (dchannel)
	{
		for (achannel = conf_allow_channel; achannel; achannel = (ConfigItem_allow_channel *)achannel->next)
		{
			if (!match(achannel->channel, name) && (achannel->class ? !strcmp(cptr->local->class->name, achannel->class) : 1))
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
		snprintf(buf, sizeof(buf), "%ld day%s ", timeval/86400, timeval/86400 != 1 ? "s" : "");
	if ((timeval/3600) % 24)
		snprintf(buf, sizeof(buf), "%s%ld hour%s ", buf, (timeval/3600)%24, (timeval/3600)%24 != 1 ? "s" : "");
	if ((timeval/60)%60)
		snprintf(buf, sizeof(buf), "%s%ld minute%s ", buf, (timeval/60)%60, (timeval/60)%60 != 1 ? "s" : "");
	if ((timeval%60))
		snprintf(buf, sizeof(buf), "%s%ld second%s", buf, timeval%60, timeval%60 != 1 ? "s" : "");
	return buf;
}

/* This converts a relative path to an absolute path, but only if necessary. */
void convert_to_absolute_path(char **path, char *reldir)
{
	char *s;
	
	if (!*path || !**path)
		return; /* NULL or empty */
	
	if (strstr(*path, "://"))
		return; /* URL: don't touch */
	
	if ((**path == '/') || (**path == '\\'))
		return; /* already absolute path */
	
	if (!strncmp(*path, reldir, strlen(reldir)))
		return; /* already contains reldir */
	
	s = MyMallocEx(strlen(reldir) + strlen(*path) + 2);
	sprintf(s, "%s/%s", reldir, *path); /* safe, see line above */
	MyFree(*path);
	*path = s;
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

	if (!strcmp(ce->ce_vardata, "help.conf"))
		need_34_upgrade = 1;

	convert_to_absolute_path(&ce->ce_vardata, CONFDIR);

#ifdef USE_LIBCURL
	if (url_is_valid(ce->ce_vardata))
		return remote_include(ce);
#endif
#if !defined(_WIN32) && !defined(_AMIGA) && !defined(OSXTIGER) && DEFAULT_PERMISSIONS != 0
	(void)chmod(ce->ce_vardata, DEFAULT_PERMISSIONS);
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
		add_include(files.gl_pathv[i], ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		ret = load_conf(files.gl_pathv[i], files.gl_pathv[i]);
		if (ret < 0)
		{
			globfree(&files);
			return ret;
		}
	}
	globfree(&files);
#elif defined(_WIN32)
	bzero(cPath,MAX_PATH);
	if (strchr(ce->ce_vardata, '/') || strchr(ce->ce_vardata, '\\')) {
		strlcpy(cPath,ce->ce_vardata,MAX_PATH);
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
		strcpy(path, cPath);
		strcat(path, FindData.cFileName);

		add_include(path, ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		ret = load_conf(path, path);
		free(path);

	}
	else
	{
		add_include(FindData.cFileName, ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		ret = load_conf(FindData.cFileName, FindData.cFileName);
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

			add_include(path, ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			ret = load_conf(path, path);
			free(path);
			if (ret < 0)
				break;
		}
		else
		{
			add_include(FindData.cFileName, ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			ret = load_conf(FindData.cFileName, FindData.cFileName);
		}
	}
	FindClose(hFind);
	if (ret < 0)
		return ret;
#else
	add_include(ce->ce_vardata, ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
	ret = load_conf(ce->ce_vardata, ce->ce_vardata);
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
		safestrdup(ca->line, cep->ce_varname);
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
			safestrdup(conf_me->name, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "info"))
		{
			safestrdup(conf_me->info, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "sid"))
		{
			safestrdup(conf_me->sid, cep->ce_vardata);
		}
	}
	return 1;
}

int	_test_me(ConfigFile *conf, ConfigEntry *ce)
{
	char has_name = 0, has_info = 0, has_sid = 0;
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
		else if (!strcmp(cep->ce_varname, "numeric"))
		{
			config_error("%s:%i: me::numeric has been removed, you must now specify a Server ID (SID) instead. "
			             "Edit your configuration file and change 'numeric' to 'sid' and make up "
			             "a server id of exactly 3 characters, starting with a digit, eg: \"001\" or \"0AB\".",
			             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
		}
		else if (!strcmp(cep->ce_varname, "sid"))
		{
			if (has_sid)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "me::sid");
				continue;
			}
			has_sid = 1;

			if (strlen(cep->ce_vardata) != 3)
			{
				config_error("%s:%i: me::sid must be 3 characters long and begin with a number",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}

			if (!isdigit(*cep->ce_vardata))
			{
				config_error("%s:%i: me::sid must be 3 characters long and begin with a number",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
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
	if (!has_sid)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "me::sid");
		errors++;
	}
	requiredstuff.conf_me = 1;
	return errors;
}

/*
 * The files {} block
 */
int	_conf_files(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;

	if (!conf_files)
	{
		conf_files = MyMallocEx(sizeof(ConfigItem_files));

		/* set defaults */
		conf_files->motd_file = strdup(MPATH);
		conf_files->rules_file = strdup(RPATH);
		conf_files->smotd_file = strdup(SMPATH);
		conf_files->botmotd_file = strdup(BPATH);
		conf_files->opermotd_file = strdup(OPATH);
		conf_files->svsmotd_file = strdup(VPATH);

		conf_files->pid_file = strdup(IRCD_PIDFILE);
		conf_files->tune_file = strdup(IRCDTUNE);

		/* we let actual files get read in later by the motd caching mechanism */
	}
	/*
	 * hack to allow initialization of conf_files (above) when there is no files block in
	 * CPATH. The caller calls _conf_files(NULL, NULL); to do this. We return here because
	 * the for loop's initialization of cep would segfault otherwise. We return 1 because
	 * if config_run() calls us with a NULL ce, it's got a bug...but we can't detect that.
	 */
	if(!ce)
	  return 1;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "motd"))
			safestrdup(conf_files->motd_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "shortmotd"))
			safestrdup(conf_files->smotd_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "opermotd"))
			safestrdup(conf_files->opermotd_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "svsmotd"))
			safestrdup(conf_files->svsmotd_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "botmotd"))
			safestrdup(conf_files->botmotd_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "rules"))
			safestrdup(conf_files->rules_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "tunefile"))
			safestrdup(conf_files->tune_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "pidfile"))
			safestrdup(conf_files->pid_file, cep->ce_vardata);
	}
	return 1;
}

int	_test_files(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int	    errors = 0;
	char has_motd = 0, has_smotd = 0, has_rules = 0;
	char has_botmotd = 0, has_opermotd = 0, has_svsmotd = 0;
	char has_pidfile = 0, has_tunefile = 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		/* files::motd */
		if (!strcmp(cep->ce_varname, "motd"))
		{
			if (has_motd)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "files::motd");
				continue;
			}
			config_test_openfile(cep, O_RDONLY, 0, "files::motd", 0, 1);
			has_motd = 1;
		}
		/* files::smotd */
		else if (!strcmp(cep->ce_varname, "shortmotd"))
		{
			if (has_smotd)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "files::shortmotd");
				continue;
			}
			config_test_openfile(cep, O_RDONLY, 0, "files::shortmotd", 0, 1);
			has_smotd = 1;
		}
		/* files::rules */
		else if (!strcmp(cep->ce_varname, "rules"))
		{
			if (has_rules)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "files::rules");
				continue;
			}
			config_test_openfile(cep, O_RDONLY, 0, "files::rules", 0, 1);
			has_rules = 1;
		}
		/* files::botmotd */
		else if (!strcmp(cep->ce_varname, "botmotd"))
		{
			if (has_botmotd)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "files::botmotd");
				continue;
			}
			config_test_openfile(cep, O_RDONLY, 0, "files::botmotd", 0, 1);
			has_botmotd = 1;
		}
		/* files::opermotd */
		else if (!strcmp(cep->ce_varname, "opermotd"))
		{
			if (has_opermotd)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "files::opermotd");
				continue;
			}
			config_test_openfile(cep, O_RDONLY, 0, "files::opermotd", 0, 1);
			has_opermotd = 1;
		}
		/* files::svsmotd
		 * This config stuff should somehow be inside of modules/m_svsmotd.c!!!... right?
		 */
		else if (!strcmp(cep->ce_varname, "svsmotd"))
		{
			if (has_svsmotd)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "files::svsmotd");
				continue;
			}
			/* svsmotd can't be a URL because we have to be able to write to it */
			config_test_openfile(cep, O_RDONLY, 0, "files::svsmotd", 0, 0);
			has_svsmotd = 1;
		}
		/* files::pidfile */
		else if (!strcmp(cep->ce_varname, "pidfile"))
		{
			if (has_pidfile)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "files::pidfile");
				continue;
			}

			errors += config_test_openfile(cep, O_WRONLY | O_CREAT, 0600, "files::pidfile", 1, 0);
			has_pidfile = 1;
		}
		/* files::tunefile */
		else if (!strcmp(cep->ce_varname, "tunefile"))
		{
			if (has_tunefile)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "files::tunefile");
				continue;
			}
			errors += config_test_openfile(cep, O_RDWR | O_CREAT, 0600, "files::tunefile", 1, 0);
			has_tunefile = 1;
		}
		/* <random directive here> */
		else
		{
			config_error("%s:%d: Unknown directive: \"%s\" in files {}", cep->ce_fileptr->cf_filename,
				     cep->ce_varlinenum, cep->ce_varname);
			errors ++;
		}
	}
	return errors;
}

/*
 * The operclass {} block parser
 */

OperClassACLEntry* _conf_parseACLEntry(ConfigEntry *ce)
{
	ConfigEntry *cep;
	OperClassACLEntry *entry = NULL;
	entry = MyMallocEx(sizeof(OperClassACLEntry));

	if (!strcmp(ce->ce_varname,"allow"))
		entry->type = OPERCLASSENTRY_ALLOW;
	else
		entry->type = OPERCLASSENTRY_DENY;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		OperClassACLEntryVar *var = MyMallocEx(sizeof(OperClassACLEntryVar));
		var->name = strdup(cep->ce_varname);
		if (cep->ce_vardata)
		{
			var->value = strdup(cep->ce_vardata);
		}
		AddListItem(var,entry->variables);
	}

	return entry;
}

OperClassACL* _conf_parseACL(char* name, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	OperClassACL *acl = NULL;
	acl = MyMallocEx(sizeof(OperClassACL));
	acl->name = strdup(name);
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "deny") || !strcmp(cep->ce_varname, "allow"))
		{
			OperClassACLEntry *entry = _conf_parseACLEntry(cep);
			AddListItem(entry,acl->entries);
		}
		else {
			OperClassACL *subAcl = _conf_parseACL(cep->ce_varname,cep);
			AddListItem(subAcl,acl->acls);
		}
	}

	return acl;
}

int	_conf_operclass(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	ConfigItem_operclass *operClass = NULL;
	operClass = MyMallocEx(sizeof(ConfigItem_operclass));
	operClass->classStruct = MyMallocEx(sizeof(OperClass));
	operClass->classStruct->name = strdup(ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "parent"))
		{
			operClass->classStruct->ISA = strdup(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "privileges"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				OperClassACL *acl = _conf_parseACL(cepp->ce_varname,cepp);
				AddListItem(acl,operClass->classStruct->acls);
			}
		}
	}

	AddListItem(operClass, conf_operclass);
	return 1;
}

int 	_test_operclass(ConfigFile *conf, ConfigEntry *ce)
{
	char has_privileges = 0, has_parent = 0;
	ConfigEntry *cep;
	ConfigEntry *cepp;
	NameValue *ofp;
	int	errors = 0;

	if (!ce->ce_vardata)
	{
		config_error_noname(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "operclass");
		errors++;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"operclass");
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "parent"))
		{
			if (has_parent)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "operclass::parent");
				continue;
			}
			has_parent = 1;
			continue;
		}
		if (!strcmp(cep->ce_varname, "privileges"))
		{
			if (has_privileges)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, "oper::privileges");
				continue;
			}
			has_privileges = 1;
			continue;
		}
		/* Regular variables */
		if (!cep->ce_entries)
		{
			if (!strcmp(cep->ce_varname, "privileges") && !cep->ce_vardata)
			{
				config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					"operclass::parent");
				errors++;
				continue;
			}

			else
			{
				config_error_unknown(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "operclass", cep->ce_varname);
				errors++;
				continue;
			}
		}

		/* Sections */
		else
		{
			/* No that's not a typo, if it isn't privileges, we explode */
			if (strcmp(cep->ce_varname, "privileges"))
			{
				config_error_unknown(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "operclass", cep->ce_varname);
				errors++;
				continue;
			}
		}
	}

	if (!has_privileges)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"oper::privileges");
		errors++;
	}

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

	oper =  MyMallocEx(sizeof(ConfigItem_oper));
	oper->name = strdup(ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "operclass"))
			oper->operclass = strdup(cep->ce_vardata);
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
		else if (!strcmp(cep->ce_varname, "swhois"))
		{
			SWhois *s;
			if (cep->ce_entries)
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					s = MyMallocEx(sizeof(SWhois));
					s->line = strdup(cepp->ce_varname);
					s->setby = strdup("oper");
					AddListItem(s, oper->swhois);
				}
			} else
			if (cep->ce_vardata)
			{
				s = MyMallocEx(sizeof(SWhois));
				s->line = strdup(cep->ce_vardata);
				s->setby = strdup("oper");
				AddListItem(s, oper->swhois);
			}
		}
		else if (!strcmp(cep->ce_varname, "snomask"))
		{
			safestrdup(oper->snomask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "modes"))
		{
			oper->modes = set_usermode(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "require-modes"))
		{
			oper->require_modes = set_usermode(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "maxlogins"))
		{
			oper->maxlogins = atoi(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "mask"))
		{
			unreal_add_masks(&oper->mask, cep);
		}
		else if (!strcmp(cep->ce_varname, "vhost"))
		{
			safestrdup(oper->vhost, cep->ce_vardata);
		}
	}
	AddListItem(oper, conf_oper);
	return 1;
}

int	_test_oper(ConfigFile *conf, ConfigEntry *ce)
{
	char has_class = 0, has_password = 0, has_swhois = 0, has_snomask = 0;
	char has_modes = 0, has_require_modes = 0, has_mask = 0, has_maxlogins = 0;
	char has_operclass = 0, has_vhost = 0;
	int oper_flags = 0;
	ConfigEntry *cep;
	ConfigEntry *cepp;
	NameValue *ofp;
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
		/* Regular variables */
		if (!cep->ce_entries)
		{
			if (config_is_blankorempty(cep, "oper"))
			{
				errors++;
				continue;
			}
			/* oper::password */
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

				if (ce->ce_vardata && cep->ce_vardata &&
					!strcmp(ce->ce_vardata, "bobsmith") &&
					!strcmp(cep->ce_vardata, "test"))
				{
					config_error("%s:%i: please change the the name and password of the "
								 "default 'bobsmith' oper block",
								 ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
					errors++;
				}
				continue;
			}
			/* oper::operclass */
			else if (!strcmp(cep->ce_varname, "operclass"))
			{
				if (has_operclass)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "oper::operclass");
					continue;
				}
				has_operclass = 1;
				continue;
			} 
			/* oper::class */
			else if (!strcmp(cep->ce_varname, "class"))
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
			}
			/* oper::vhost */
			else if (!strcmp(cep->ce_varname, "vhost"))
			{
				if (has_vhost)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "oper::vhost");
					continue;
				}
				has_vhost = 1;
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
			/* oper::require-modes */
			else if (!strcmp(cep->ce_varname, "require-modes"))
			{
				char *p;
				for (p = cep->ce_vardata; *p; p++)
					if (strchr("oOaANC", *p))
					{
						config_warn("%s:%i: oper::require-modes probably shouldn't include mode '%c'",
							cep->ce_fileptr->cf_filename, cep->ce_varlinenum, *p);
					}
				if (has_require_modes)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "oper::require-modes");
					continue;
				}
				has_require_modes = 1;
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
				config_error("%s:%i: oper::flags no longer exists. UnrealIRCd 4 uses a new style oper block.",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				need_34_upgrade = 1;
			}
			else if (!strcmp(cep->ce_varname, "mask"))
			{
				if (cep->ce_vardata || cep->ce_entries)
					has_mask = 1;
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
				config_error("%s:%i: oper::flags no longer exists. UnrealIRCd 4 uses a new style oper block.",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				need_34_upgrade = 1;
				continue;
			}
			/* oper::from {} */
			else if (!strcmp(cep->ce_varname, "from"))
			{
				config_error("%s:%i: oper::from::userhost is now called oper::mask",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				need_34_upgrade = 1;
				continue;
			}
			else if (!strcmp(cep->ce_varname, "swhois"))
			{
				/* ok */
			}
			else if (!strcmp(cep->ce_varname, "mask"))
			{
				if (cep->ce_vardata || cep->ce_entries)
					has_mask = 1;
			}
			else if (!strcmp(cep->ce_varname, "password"))
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
	if (!has_mask)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"oper::mask");
		errors++;
	}
	if (!has_class)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"oper::class");
		errors++;
	}
	if (!has_operclass)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"oper::operclass");
		need_34_upgrade = 1;
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
		safestrdup(class->name, ce->ce_vardata);
		isnew = 1;
	}
	else
	{
		isnew = 0;
		class->flag.temporary = 0;
		class->options = 0; /* RESET OPTIONS */
	}
	safestrdup(class->name, ce->ce_vardata);

	class->connfreq = 15; /* default */

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "pingfreq"))
			class->pingfreq = config_checkval(cep->ce_vardata,CFG_TIME);
		else if (!strcmp(cep->ce_varname, "connfreq"))
			class->connfreq = config_checkval(cep->ce_vardata,CFG_TIME);
		else if (!strcmp(cep->ce_varname, "maxclients"))
			class->maxclients = atol(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "sendq"))
			class->sendq = config_checkval(cep->ce_vardata,CFG_SIZE);
		else if (!strcmp(cep->ce_varname, "recvq"))
			class->recvq = config_checkval(cep->ce_vardata,CFG_SIZE);
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
		return 1;
	}
	if (!strcasecmp(ce->ce_vardata, "default"))
	{
		config_error("%s:%d: Class cannot be named 'default', this class name is reserved for internal use.",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
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
			int v = config_checkval(cep->ce_vardata,CFG_TIME);
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
			l = config_checkval(cep->ce_vardata,CFG_TIME);
			if ((l < 5) || (l > 604800))
			{
				config_error("%s:%i: class::connfreq with illegal value (must be >5 and <7d)",
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
			l = config_checkval(cep->ce_vardata,CFG_SIZE);
			if ((l <= 0) || (l > 2000000000))
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
			l = config_checkval(cep->ce_vardata,CFG_SIZE);
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
		safestrdup(ca->servername, cep->ce_varname);
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
			read_motd(cep->ce_vardata, &ca->motd);
		}
		else if (!strcmp(cep->ce_varname, "shortmotd"))
		{
			ca->smotd_file = strdup(cep->ce_vardata);
			read_motd(cep->ce_vardata, &ca->smotd);
		}
		else if (!strcmp(cep->ce_varname, "opermotd"))
		{
			ca->opermotd_file = strdup(cep->ce_vardata);
			read_motd(cep->ce_vardata, &ca->opermotd);
		}
		else if (!strcmp(cep->ce_varname, "botmotd"))
		{
			ca->botmotd_file = strdup(cep->ce_vardata);
			read_motd(cep->ce_vardata, &ca->botmotd);
		}
		else if (!strcmp(cep->ce_varname, "rules"))
		{
			ca->rules_file = strdup(cep->ce_vardata);
			read_motd(cep->ce_vardata, &ca->rules);
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
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
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
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
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
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
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
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
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
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
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
	char *ip;
	int start=0, end=0, port, isnew;
	int tmpflags =0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "ip"))
		{
			ip = cep->ce_vardata;
		} else
		if (!strcmp(cep->ce_varname, "port"))
		{
			port_range(cep->ce_vardata, &start, &end);
			if ((start < 0) || (start > 65535) || (end < 0) || (end > 65535))
				return -1; /* this is already validated in _test_listen, but okay.. */
		} else
		if (!strcmp(cep->ce_varname, "options"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				NameValue *ofp;
				if ((ofp = config_binary_flags_search(_ListenerFlags, cepp->ce_varname, ARRAY_SIZEOF(_ListenerFlags))))
					tmpflags |= ofp->flag;
			}
		}
	}
	for (port = start; port <= end; port++)
	{
		/* First deal with IPv4 */
		if (!strchr(ip, ':'))
		{
			if (!(listen = Find_listen(ip, port, 0)))
			{
				listen = MyMallocEx(sizeof(ConfigItem_listen));
				listen->ip = strdup(ip);
				listen->port = port;
				listen->fd = -1;
				listen->ipv6 = 0;
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

		/* Then deal with IPv6 (if available/enabled) */
		if (!DISABLE_IPV6)
		{
			if (strchr(ip, ':') || (*ip == '*'))
			{
				if (!(listen = Find_listen(ip, port, 1)))
				{
					listen = MyMallocEx(sizeof(ConfigItem_listen));
					listen->ip = strdup(ip);
					listen->port = port;
					listen->fd = -1;
					listen->ipv6 = 1;
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
		}
	}
	return 1;
}

int	_test_listen(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	int errors = 0;
	char has_ip = 0, has_port = 0, has_options = 0;

	if (ce->ce_vardata)
	{
		config_error("%s:%i: listen block has a new syntax, see https://www.unrealircd.org/docs/Listen_block",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			
		need_34_upgrade = 1;
		return 1;
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
				NameValue *ofp;
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
						cepp->ce_varlinenum, "listen::options", cepp->ce_varname);
					errors++;
					continue;
				}
			}
		}
		else
		if (!cep->ce_vardata)
		{
			config_error_empty(cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, "listen", cep->ce_varname);
			errors++;
			continue;
		} else
		if (!strcmp(cep->ce_varname, "ip"))
		{
			has_ip = 1;
			
			if (strcmp(cep->ce_vardata, "*") && !is_valid_ip(cep->ce_vardata))
			{
				config_error("%s:%i: listen: illegal listen::ip (%s). Must be either '*' or contain a valid IP.",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				return 1;
			}
		} else
		if (!strcmp(cep->ce_varname, "host"))
		{
			config_error("%s:%i: listen: unknown option listen::host, did you mean listen::ip?",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
		} else
		if (!strcmp(cep->ce_varname, "port"))
		{
			int start = 0, end = 0;

			has_port = 1;

			port_range(cep->ce_vardata, &start, &end);
			if (start == end)
			{
				if ((start < 0) || (start > 65535))
				{
					config_error("%s:%i: listen: illegal port (must be 0..65535)",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					return 1;
				}
			}
			else
			{
				if (end < start)
				{
					config_error("%s:%i: listen: illegal port range end value is less than starting value",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					return 1;
				}
				if (end - start >= 100)
				{
					config_error("%s:%i: listen: you requested port %d-%d, that's %d ports "
						"(and thus consumes %d sockets) this is probably not what you want.",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, start, end,
						end - start + 1, end - start + 1);
					return 1;
				}
				if ((start < 0) || (start > 65535) || (end < 0) || (end > 65535))
				{
					config_error("%s:%i: listen: illegal port range values must be between 0 and 65535",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					return 1;
				}
			}
		} else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"listen", cep->ce_varname);
			errors++;
			continue;
		}
	}
	
	if (!has_ip)
	{
		config_error("%s:%d: listen block requires an listen::ip",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}

	if (!has_port)
	{
		config_error("%s:%d: listen block requires an listen::port",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}

	requiredstuff.conf_listen = 1;
	return errors;
}


int	_conf_allow(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	ConfigItem_allow *allow;
	Hook *h;

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
		else if (!strcmp(cep->ce_varname, "ipv6-clone-mask"))
		{
			/*
			 * If this item isn't set explicitly by the
			 * user, the value will temporarily be
			 * zero. Defaults are applied in config_run().
			 */
			allow->ipv6_clone_mask = atoi(cep->ce_vardata);
		}
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

	if (!allow->hostname)
		allow->hostname = strdup("*@NOMATCH");

	if (!allow->ip)
		allow->ip = strdup("*@NOMATCH");

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
	int hostname_possible_silliness = 0;

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
		else if (!strcmp(cep->ce_varname, "ipv6-clone-mask"))
		{
			/* keep this in sync with _test_set() */
			int ipv6mask;
			ipv6mask = atoi(cep->ce_vardata);
			if (ipv6mask == 0)
			{
				config_error("%s:%d: allow::ipv6-clone-mask given a value of zero. This cannnot be correct, as it would treat all IPv6 hosts as one host.",
					     cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
			if (ipv6mask > 128)
			{
				config_error("%s:%d: set::default-ipv6-clone-mask was set to %d. The maximum value is 128.",
					     cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					     ipv6mask);
				errors++;
			}
			if (ipv6mask <= 32)
			{
				config_warn("%s:%d: allow::ipv6-clone-mask was given a very small value.",
					    cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
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
			if (!strcmp(cep->ce_vardata, "*@*") || !strcmp(cep->ce_vardata, "*"))
				hostname_possible_silliness = 1;
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

	if (!has_ip && !has_hostname)
	{
		config_error("%s:%d: allow block needs an allow::ip or allow::hostname",
				 ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}

	if (has_ip && has_hostname)
	{
		config_warn("%s:%d: allow block has both allow::ip and allow::hostname which is no longer permitted.",
		            ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		need_34_upgrade = 1;
	} else
	if (hostname_possible_silliness)
	{
		config_warn("%s:%d: allow block contains 'hostname *;'. This means means that users "
		            "without a valid hostname (unresolved IP's) will be unable to connect. "
		            "You most likely want to use 'ip *;' instead.",
		            ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
	}

	if (!has_class)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"allow::class");
		errors++;
	}

	if (!has_maxperip)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"allow::maxperip");
		errors++;
	}
	return errors;
}

int	_conf_allow_channel(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_allow_channel 	*allow = NULL;
	ConfigEntry 	    	*cep;
	char *class = NULL;

	/* First, search for ::class, if any */
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "class"))
			class = cep->ce_vardata;
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "channel"))
		{
			/* This way, we permit multiple ::channel items in one allow block */
			allow = MyMallocEx(sizeof(ConfigItem_allow_channel));
			safestrdup(allow->channel, cep->ce_vardata);
			if (class)
				safestrdup(allow->class, class);
			AddListItem(allow, conf_allow_channel);
		}
	}
	return 1;
}

int	_test_allow_channel(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry		*cep;
	int			errors = 0;
	char			has_channel = 0, has_class = 0;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (config_is_blankorempty(cep, "allow channel"))
		{
			errors++;
			continue;
		}

		if (!strcmp(cep->ce_varname, "channel"))
		{
			has_channel = 1;
		}
		else if (!strcmp(cep->ce_varname, "class"))
		{

			if (has_class)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "allow channel::class");
				continue;
			}
			has_class = 1;
		}
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
			safestrdup(allow->filename, cep->ce_vardata);
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

void create_tkl_except_ii(char *mask, char *type)
{
	ConfigItem_except *ca;
	NameValue *opf;
	ca = MyMallocEx(sizeof(ConfigItem_except));
	ca->mask = strdup(mask);

	opf = config_binary_flags_search(ExceptTklFlags, type, ARRAY_SIZEOF(ExceptTklFlags));
	ca->type = opf->flag;
	ca->flag.type = CONF_EXCEPT_TKL;
	AddListItem(ca, conf_except);
}

void create_tkl_except(char *mask, char *type)
{
	if (!strcmp(type, "all"))
	{
		/* Special treatment */
		int i;
		for (i = 0; i < ARRAY_SIZEOF(ExceptTklFlags); i++)
			if (ExceptTklFlags[i].flag)
				create_tkl_except_ii(mask, ExceptTklFlags[i].name);
	}
	else
		create_tkl_except_ii(mask, type);
}

int     _conf_except(ConfigFile *conf, ConfigEntry *ce)
{

	ConfigEntry *cep;
	ConfigItem_except *ca;
	Hook *h;

	if (!strcmp(ce->ce_vardata, "ban")) {
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "mask")) {
				ca = MyMallocEx(sizeof(ConfigItem_except));
				ca->mask = strdup(cep->ce_vardata);
				ca->flag.type = CONF_EXCEPT_BAN;
				AddListItem(ca, conf_except);
			}
			else {
			}
		}
	}
	else if (!strcmp(ce->ce_vardata, "throttle")) {
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "mask")) {
				ca = MyMallocEx(sizeof(ConfigItem_except));
				ca->mask = strdup(cep->ce_vardata);
				ca->flag.type = CONF_EXCEPT_THROTTLE;
				AddListItem(ca, conf_except);
			}
			else {
			}
		}

	}
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
	ConfigItem_mask *mask;
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
		else if (!strcmp(cep->ce_varname, "mask"))
		{
			unreal_add_masks(&vhost->mask, cep);
		}
		else if (!strcmp(cep->ce_varname, "swhois"))
		{
			SWhois *s;
			if (cep->ce_entries)
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					s = MyMallocEx(sizeof(SWhois));
					s->line = strdup(cepp->ce_varname);
					s->setby = strdup("vhost");
					AddListItem(s, vhost->swhois);
				}
			} else
			if (cep->ce_vardata)
			{
				s = MyMallocEx(sizeof(SWhois));
				s->line = strdup(cep->ce_vardata);
				s->setby = strdup("vhost");
				AddListItem(s, vhost->swhois);
			}
		}
	}
	AddListItem(vhost, conf_vhost);
	return 1;
}

int	_test_vhost(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	ConfigEntry *cep;
	char has_vhost = 0, has_login = 0, has_password = 0, has_swhois = 0, has_mask = 0;

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

			config_error("%s:%i: vhost::from::userhost is now called oper::mask",
						 cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
			need_34_upgrade = 1;
			continue;
		}
		else if (!strcmp(cep->ce_varname, "mask"))
		{
			has_mask = 1;
		}
		else if (!strcmp(cep->ce_varname, "swhois"))
		{
			/* multiple is ok */
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
	if (!has_mask)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"vhost::mask");
		errors++;
	}
	// TODO: 3.2.x -> 4.x upgrading hints
	return errors;
}

int _conf_spamfilter(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	aTKline *nl = MyMallocEx(sizeof(aTKline));
	char *word = NULL, *reason = NULL, *bantime = NULL;
	int action = 0, target = 0;
	char has_reason = 0, has_bantime = 0;
	int match_type = 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "match"))
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
		else if (!strcmp(cep->ce_varname, "match-type"))
		{
			match_type = unreal_match_method_strtoval(cep->ce_vardata);
		}
	}
	nl->type = TKL_SPAMF;
	nl->expire_at = 0;
	nl->set_at = TStime();

	strlcpy(nl->usermask, spamfilter_target_inttostring(target), sizeof(nl->usermask));
	nl->subtype = target;

	nl->setby = BadPtr(me.name) ? NULL : strdup(me.name); /* Hmm! */
	nl->ptr.spamf = MyMallocEx(sizeof(Spamfilter));
	nl->ptr.spamf->expr = unreal_create_match(match_type, word, NULL);
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
	char *match = NULL, *reason = NULL;
	char has_target = 0, has_match = 0, has_action = 0, has_reason = 0, has_bantime = 0, has_match_type = 0;
	int match_type = 0;

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

	return errors;
}

int     _conf_help(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_help *ca;
	aMotdLine *last = NULL, *temp;
	ca = MyMallocEx(sizeof(ConfigItem_help));

	if (!ce->ce_vardata)
		ca->command = NULL;
	else
		ca->command = strdup(ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		temp = MyMallocEx(sizeof(aMotdLine));
		temp->line = strdup(cep->ce_varname);
		temp->next = NULL;
		if (!last)
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
	NameValue *ofp = NULL;

	ca = MyMallocEx(sizeof(ConfigItem_log));
	ca->logfd = -1;
	safestrdup(ca->file, ce->ce_vardata);

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
	int fd, errors = 0;
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

	/* Convert to absolute path (if needed) unless it's "syslog" */
	if (strcmp(ce->ce_vardata, "syslog"))
		convert_to_absolute_path(&ce->ce_vardata, LOGDIR);

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
	if ((fd = fd_fileopen(ce->ce_vardata, O_WRONLY|O_CREAT)) == -1)
	{
		config_error("%s:%i: Couldn't open logfile (%s) for writing: %s",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata, strerror(errno));
		errors++;
	}
	else
		fd_close(fd);

	return errors;
}

int	_conf_link(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp, *ceppp;
	ConfigItem_link *link = NULL;
	NameValue *ofp;

	link = (ConfigItem_link *) MyMallocEx(sizeof(ConfigItem_link));
	link->servername = strdup(ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "incoming"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "mask"))
				{
					unreal_add_masks(&link->incoming.mask, cepp);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "outgoing"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "bind-ip"))
					safestrdup(link->outgoing.bind_ip, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "hostname"))
					safestrdup(link->outgoing.hostname, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "port"))
					link->outgoing.port = atoi(cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "options"))
				{
					/* TODO: options still need to be split */
					link->outgoing.options = 0;
					for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
					{
						if ((ofp = config_binary_flags_search(_LinkFlags, ceppp->ce_varname, ARRAY_SIZEOF(_LinkFlags))))
							link->outgoing.options |= ofp->flag;
					}
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "password"))
			link->auth = Auth_ConvertConf2AuthStruct(cep);
		else if (!strcmp(cep->ce_varname, "hub"))
			safestrdup(link->hub, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "leaf"))
			safestrdup(link->leaf, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "leaf-depth") || !strcmp(cep->ce_varname, "leafdepth"))
			link->leaf_depth = atoi(cep->ce_vardata);
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
		else if (!strcmp(cep->ce_varname, "ciphers"))
			link->ciphers = strdup(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "options"))
		{
			link->options = 0;
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if ((ofp = config_binary_flags_search(_LinkFlags, cepp->ce_varname, ARRAY_SIZEOF(_LinkFlags))))
					link->options |= ofp->flag;
			}
		}
	}

	/* The default is 'hub *', unless you specify leaf or hub manually. */
	if (!link->hub && !link->leaf)
		link->hub = strdup("*");

	AddListItem(link, conf_link);
	return 0;
}

/** Helper function for erroring on duplicate items.
 * TODO: make even more friendy for dev's?
 */
int config_detect_duplicate(int *var, ConfigEntry *ce, int *errors)
{
	if (*var)
	{
		config_error("%s:%d: Duplicate %s directive",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_varname);
		(*errors)++;
		return 1;
	} else {
		*var = 1;
	}
	return 0;
}

int	_test_link(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp, *ceppp;
	NameValue *ofp;
	int errors = 0;

	int has_incoming = 0, has_incoming_mask = 0, has_outgoing = 0;
	int has_outgoing_bind_ip = 0, has_outgoing_hostname = 0, has_outgoing_port = 0;
	int has_outgoing_options = 0, has_hub = 0, has_leaf = 0, has_leaf_depth = 0;
	int has_password = 0, has_class = 0, has_ciphers = 0, has_options = 0;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: link without servername. Expected: link servername { ... }",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;

	}

	if (!strchr(ce->ce_vardata, '.'))
	{
		config_error("%s:%i: link: bogus server name. Expected: link servername { ... }",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "incoming"))
		{
			config_detect_duplicate(&has_incoming, cep, &errors);
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "mask"))
				{
					if (cepp->ce_vardata || cepp->ce_entries)
						has_incoming_mask = 1;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "outgoing"))
		{
			config_detect_duplicate(&has_outgoing, cep, &errors);
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "bind-ip"))
				{
					config_detect_duplicate(&has_outgoing_bind_ip, cepp, &errors);
					// todo: ipv4 vs ipv6
				}
				else if (!strcmp(cepp->ce_varname, "hostname"))
				{
					config_detect_duplicate(&has_outgoing_hostname, cepp, &errors);
					if (strchr(cepp->ce_vardata, '*') || strchr(cepp->ce_vardata, '?'))
					{
						config_error("%s:%i: hostname in link::outgoing(!) cannot contain wildcards",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
					}
				}
				else if (!strcmp(cepp->ce_varname, "port"))
				{
					config_detect_duplicate(&has_outgoing_port, cepp, &errors);
				}
				else if (!strcmp(cepp->ce_varname, "options"))
				{
					config_detect_duplicate(&has_outgoing_options, cepp, &errors);
					for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
					{
						if (!strcmp(ceppp->ce_varname, "autoconnect"))
							;
						else if (!strcmp(ceppp->ce_varname, "ssl"))
							;
						else if (!strcmp(ceppp->ce_varname, "insecure"))
							;
						else
						{
							config_error_unknownopt(ceppp->ce_fileptr->cf_filename,
								ceppp->ce_varlinenum, "link::outgoing", ceppp->ce_varname);
							errors++;
						}
						// TODO: validate more options (?) and use list rather than code here...
					}
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "password"))
		{
			config_detect_duplicate(&has_password, cep, &errors);
			if (Auth_CheckError(cep) < 0)
			{
				errors++;
			} else {
				anAuthStruct *auth = Auth_ConvertConf2AuthStruct(cep);
				/* hm. would be nicer if handled @auth-system I think. ah well.. */
				if ((auth->type != AUTHTYPE_PLAINTEXT) && (auth->type != AUTHTYPE_SSL_CLIENTCERT) &&
				    (auth->type != AUTHTYPE_SSL_CLIENTCERTFP))
				{
					config_error("%s:%i: password in link block should be plaintext OR should be the "
					             "SSL fingerprint of the remote link (=better)",
					             /* TODO: mention some faq or wiki item for more information */
					             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					errors++;
				}
				Auth_DeleteAuthStruct(auth);
			}
		}
		else if (!strcmp(cep->ce_varname, "hub"))
		{
			config_detect_duplicate(&has_hub, cep, &errors);
		}
		else if (!strcmp(cep->ce_varname, "leaf"))
		{
			config_detect_duplicate(&has_leaf, cep, &errors);
		}
		else if (!strcmp(cep->ce_varname, "leaf-depth") || !strcmp(cep->ce_varname, "leafdepth"))
		{
			config_detect_duplicate(&has_leaf_depth, cep, &errors);
		}
		else if (!strcmp(cep->ce_varname, "class"))
		{
			config_detect_duplicate(&has_class, cep, &errors);
		}
		else if (!strcmp(cep->ce_varname, "ciphers"))
		{
			config_detect_duplicate(&has_ciphers, cep, &errors);
		}
		else if (!strcmp(cep->ce_varname, "options"))
		{
			config_detect_duplicate(&has_options, cep, &errors);
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "quarantine"))
					;
				else
				{
					config_error("%s:%d: link::options only has one possible option ('quarantine', rarely used). "
					             "Option '%s' is unrecognized. "
					             "Perhaps you meant to set an outgoing option in link::outgoing::options instead?",
					             cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, cepp->ce_varname);
					errors++;
				}
			}
		}
	}

	if (!has_incoming && !has_outgoing)
	{
		config_error("%s:%d: link block needs at least an incoming or outgoing section.",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
		need_34_upgrade = 1;
	}

	if (has_incoming)
	{
		/* If we have an incoming sub-block then we need at least 'mask' and 'password' */
		if (!has_incoming_mask)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "link::incoming::mask");
			errors++;
		}
	}

	if (has_outgoing)
	{
		/* If we have an outgoing sub-block then we need at least a hostname and port */
		if (!has_outgoing_hostname)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "link::outgoing::hostname");
			errors++;
		}
		if (!has_outgoing_port)
		{
			config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "link::outgoing::port");
			errors++;
		}
	}

	/* The only other generic options that are required are 'class' and 'password' */
	if (!has_password)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "link::password");
		errors++;
	}
	if (!has_class)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"link::class");
		errors++;
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
	NameValue 	*ofl = NULL;
	Hook *h;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "kline-address")) {
			safestrdup(tempiConf.kline_address, cep->ce_vardata);
		}
		if (!strcmp(cep->ce_varname, "gline-address")) {
			safestrdup(tempiConf.gline_address, cep->ce_vardata);
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
			safestrdup(tempiConf.oper_snomask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "snomask-on-connect")) {
			safestrdup(tempiConf.user_snomask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "level-on-join")) {
			tempiConf.level_on_join = channellevel_to_int(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "static-quit")) {
			safestrdup(tempiConf.static_quit, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "static-part")) {
			safestrdup(tempiConf.static_part, cep->ce_vardata);
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
			safestrdup(tempiConf.auto_join_chans, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "oper-auto-join")) {
			safestrdup(tempiConf.oper_auto_join_chans, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "check-target-nick-bans")) {
			tempiConf.check_target_nick_bans = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "ping-cookie")) {
			tempiConf.ping_cookie = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "watch-away-notification")) {
			tempiConf.watch_away_notification = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "uhnames")) {
			tempiConf.uhnames = config_checkval(cep->ce_vardata, CFG_YESNO);
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
			safestrdup(tempiConf.channel_command_prefix, cep->ce_vardata);
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
			safestrdup(tempiConf.restrict_extendedbans, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "new-linking-protocol")) {
			tempiConf.new_linking_protocol = atoi(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "anti-spam-quit-message-time")) {
			tempiConf.anti_spam_quit_message_time = config_checkval(cep->ce_vardata,CFG_TIME);
		}
		else if (!strcmp(cep->ce_varname, "oper-only-stats")) {
			if (!cep->ce_entries)
			{
				safestrdup(tempiConf.oper_only_stats, cep->ce_vardata);
			}
			else
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					OperStat *os = MyMallocEx(sizeof(OperStat));
					safestrdup(os->flag, cepp->ce_varname);
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
				ircsnprintf(tmpbuf, sizeof(tmpbuf), "#:%s", cep->ce_vardata);
				IsupportSetValue(IsupportFind("CHANLIMIT"), tmpbuf);
			}
		}
		else if (!strcmp(cep->ce_varname, "maxdccallow")) {
			tempiConf.maxdccallow = atoi(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "network-name")) {
			char *tmp;
			safestrdup(tempiConf.network.x_ircnetwork, cep->ce_vardata);
			for (tmp = cep->ce_vardata; *cep->ce_vardata; cep->ce_vardata++) {
				if (*cep->ce_vardata == ' ')
					*cep->ce_vardata='-';
			}
			safestrdup(tempiConf.network.x_ircnet005, tmp);
			if (loop.ircd_booted)
				IsupportSetValue(IsupportFind("NETWORK"), tmp);
			cep->ce_vardata = tmp;
		}
		else if (!strcmp(cep->ce_varname, "default-server")) {
			safestrdup(tempiConf.network.x_defserv, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "services-server")) {
			safestrdup(tempiConf.network.x_services_name, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "sasl-server")) {
			safestrdup(tempiConf.network.x_sasl_server, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "stats-server")) {
			safestrdup(tempiConf.network.x_stats_server, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "help-channel")) {
			safestrdup(tempiConf.network.x_helpchan, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "hiddenhost-prefix")) {
			safestrdup(tempiConf.network.x_hidden_host, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "hide-ban-reason")) {
			tempiConf.hide_ban_reason = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "prefix-quit")) {
			if (!strcmp(cep->ce_vardata, "0") || !strcmp(cep->ce_vardata, "no"))
				safefree(tempiConf.network.x_prefix_quit);
			else
				safestrdup(tempiConf.network.x_prefix_quit, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "link")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "bind-ip")) {
					safestrdup(tempiConf.link_bindip, cepp->ce_vardata);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "dns")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "bind-ip")) {
					safestrdup(tempiConf.dns_bindip, cepp->ce_vardata);
				}
			}
		}
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
				else if (!strcmp(cepp->ce_varname, "connect-flood"))
				{
					int cnt, period;
					config_parse_flood(cepp->ce_vardata, &cnt, &period);
					tempiConf.throttle_count = cnt;
					tempiConf.throttle_period = period;
				}
				else
				{
					for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
					{
						int value = (*(h->func.intfunc))(conf,cepp,CONFIG_SET_ANTI_FLOOD);
						if (value == 1)
							break;
					}
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "options")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "hide-ulines")) {
					tempiConf.hide_ulines = 1;
				}
				else if (!strcmp(cepp->ce_varname, "flat-map")) {
					tempiConf.flat_map = 1;
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
				else if (!strcmp(cepp->ce_varname, "allow-insane-bans")) {
					tempiConf.allow_insane_bans = 1;
				}
				else if (!strcmp(cepp->ce_varname, "allow-part-if-shunned")) {
					tempiConf.allow_part_if_shunned = 1;
				}
				else if (!strcmp(cepp->ce_varname, "disable-cap")) {
					tempiConf.disable_cap = 1;
				}
				else if (!strcmp(cepp->ce_varname, "disable-ipv6")) {
					/* other code handles this */
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
					safestrdup(tempiConf.timesynch_server, cepp->ce_vardata);
			}
		}
		else if (!strcmp(cep->ce_varname, "spamfilter"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "ban-time"))
					tempiConf.spamfilter_ban_time = config_checkval(cepp->ce_vardata,CFG_TIME);
				else if (!strcmp(cepp->ce_varname, "ban-reason"))
					safestrdup(tempiConf.spamfilter_ban_reason, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "virus-help-channel"))
					safestrdup(tempiConf.spamfilter_virus_help_channel, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "virus-help-channel-deny"))
					tempiConf.spamfilter_vchan_deny = config_checkval(cepp->ce_vardata,CFG_YESNO);
				else if (!strcmp(cepp->ce_varname, "except"))
				{
					char *name, *p;
					SpamExcept *e;
					safestrdup(tempiConf.spamexcept_line, cepp->ce_vardata);
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
				else if (!strcmp(cepp->ce_varname, "stop-on-first-match"))
				{
					tempiConf.spamfilter_stop_on_first_match = config_checkval(cepp->ce_vardata, CFG_YESNO);
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
		else if (!strcmp(cep->ce_varname, "modef-default-unsettime")) {
			int v = atoi(cep->ce_vardata);
			tempiConf.modef_default_unsettime = (unsigned char)v;
		}
		else if (!strcmp(cep->ce_varname, "modef-max-unsettime")) {
			int v = atoi(cep->ce_vardata);
			tempiConf.modef_max_unsettime = (unsigned char)v;
		}
		else if (!strcmp(cep->ce_varname, "nick-length")) {
			int v = atoi(cep->ce_vardata);
			tempiConf.nicklen = v;
			if (loop.ircd_booted)
				IsupportSetValue(IsupportFind("NICKLEN"), cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "ssl")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "egd")) {
					tempiConf.use_egd = 1;
					if (cepp->ce_vardata)
						tempiConf.egd_path = strdup(cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "server-cipher-list"))
				{
					safestrdup(tempiConf.x_server_cipher_list, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "dh"))
				{
					safestrdup(tempiConf.x_dh_pem, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "certificate"))
				{
					convert_to_absolute_path(&cepp->ce_vardata, CONFDIR);
					safestrdup(tempiConf.x_server_cert_pem, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "key"))
				{
					convert_to_absolute_path(&cepp->ce_vardata, CONFDIR);
					safestrdup(tempiConf.x_server_key_pem, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "trusted-ca-file"))
				{
					convert_to_absolute_path(&cepp->ce_vardata, CONFDIR);
					safestrdup(tempiConf.trusted_ca_file, cepp->ce_vardata);
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
						ofl = config_binary_flags_search(_SSLFlags, ceppp->ce_varname, ARRAY_SIZEOF(_SSLFlags));
						if (ofl) /* this should always be true */
							tempiConf.ssl_options |= ofl->flag;
					}
					if (tempiConf.ssl_options & SSLFLAG_DONOTACCEPTSELFSIGNED)
						if (!(tempiConf.ssl_options & SSLFLAG_VERIFYCERT))
							tempiConf.ssl_options |= SSLFLAG_VERIFYCERT;
				}

			}
		}
		else if (!strcmp(cep->ce_varname, "default-ipv6-clone-mask"))
		{
			tempiConf.default_ipv6_clone_mask = atoi(cep->ce_vardata);
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
	long		templong;
	int		tempi;
	int	    errors = 0;
	Hook	*h;
#define CheckNull(x) if ((!(x)->ce_vardata) || (!(*((x)->ce_vardata)))) { config_error("%s:%i: missing parameter", (x)->ce_fileptr->cf_filename, (x)->ce_varlinenum); errors++; continue; }
#define CheckNullAllowEmpty(x) if ((!(x)->ce_vardata)) { config_error("%s:%i: missing parameter", (x)->ce_fileptr->cf_filename, (x)->ce_varlinenum); errors++; continue; }
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
					case 'k':
					case 'l':
						config_error("%s:%i: set::modes-on-join may not contain +%c",
							cep->ce_fileptr->cf_filename, cep->ce_varlinenum, *c);
						errors++;
						break;
				}
			}
			set_channelmodes(cep->ce_vardata, &temp, 1);
			if (temp.mode & MODE_SECRET && temp.mode & MODE_PRIVATE)
			{
				config_error("%s:%i: set::modes-on-join has both +s and +p",
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
				config_error("%s:%i: set::level-on-join: unknown value '%s', should be one of: none, voice, halfop, op, protect, owner",
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
			config_error("%s:%i: set::pingpong-warning no longer exists (the warning is always off)",
			             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			need_34_upgrade = 1;
			errors++;
		}
		else if (!strcmp(cep->ce_varname, "ping-cookie")) {
			CheckNull(cep);
			CheckDuplicate(cep, ping_cookie, "ping-cookie");
		}
		else if (!strcmp(cep->ce_varname, "watch-away-notification")) {
			CheckNull(cep);
			CheckDuplicate(cep, watch_away_notification, "watch-away-notification");
		}
		else if (!strcmp(cep->ce_varname, "uhnames")) {
			CheckNull(cep);
			CheckDuplicate(cep, uhnames, "uhnames");
		}
		else if (!strcmp(cep->ce_varname, "channel-command-prefix")) {
			CheckNullAllowEmpty(cep);
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
		else if (!strcmp(cep->ce_varname, "sasl-server")) {
			CheckNull(cep);
			CheckDuplicate(cep, sasl_server, "sasl-server");
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
		else if (!strcmp(cep->ce_varname, "hide-ban-reason")) {
			CheckDuplicate(cep, hide_ban_reason, "hide-ban-reason");
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
		else if (!strcmp(cep->ce_varname, "new-linking-protocol"))
		{
			CheckDuplicate(cep, new_linking_protocol, "new-linking-protocol");
			CheckNull(cep);
		}
		else if (!strcmp(cep->ce_varname, "link")) {
					for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
						CheckNull(cepp);
						if (!strcmp(cepp->ce_varname, "bind-ip")) {
							CheckDuplicate(cepp, link_bind_ip, "link::bind-ip");
							if (strcmp(cepp->ce_vardata, "*"))
							{
								if (!is_valid_ip(cepp->ce_vardata))
								{
									config_error("%s:%i: set::link::bind-ip (%s) is not a valid IP",
										cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
										cepp->ce_vardata);
									errors++;
									continue;
								}
							}
						}
					}
		}
		else if (!strcmp(cep->ce_varname, "dns")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "nameserver") ||
				    !strcmp(cepp->ce_varname, "timeout") ||
				    !strcmp(cepp->ce_varname, "retries"))
				{
					config_error("%s:%i: set::dns::%s no longer exist in UnrealIRCd 4. "
					             "Please remove it from your configuration file.",
						cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, cepp->ce_varname);
					errors++;
				} else
				if (!strcmp(cepp->ce_varname, "bind-ip")) {
					CheckDuplicate(cepp, dns_bind_ip, "dns::bind-ip");
					if (strcmp(cepp->ce_vardata, "*"))
					{
						if (!is_valid_ip(cepp->ce_vardata))
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
		else if (!strcmp(cep->ce_varname, "throttle")) {
			config_error("%s:%i: set::throttle has been renamed. you now use "
			             "set::anti-flood::connect-flood <connections>:<period>. "
			             "Or just remove the throttle block and you get the default "
			             "of 3 per 60 seconds.",
			             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
			need_34_upgrade = 1;
			continue;
		}
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
						config_error("%s:%i: set::anti-flood::nick-flood error. Syntax is '<count>:<period>' (eg 5:60), "
						             "count should be 1-255, period should be greater than 4",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
					}
				}
				else if (!strcmp(cepp->ce_varname, "connect-flood"))
				{
					int cnt, period;
					CheckDuplicate(cepp, anti_flood_connect_flood, "anti-flood::connect-flood");
					if (!config_parse_flood(cepp->ce_vardata, &cnt, &period) ||
					    (cnt < 1) || (cnt > 255) || (period < 1) || (period > 3600))
					{
						config_error("%s:%i: set::anti-flood::connect-flood: Syntax is '<count>:<period>' (eg 5:60), "
						             "count should be 1-255, period should be 1-3600",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
					}
				}
				else
				{
					/* hmm.. I don't like this method. but I just quickly copied it from CONFIG_ALLOW for now... */
					int used = 0;
					Hook *h;
					for (h = Hooks[HOOKTYPE_CONFIGTEST]; h; h = h->next)
					{
						int value, errs = 0;
						if (h->owner && !(h->owner->flags & MODFLAG_TESTING)
							&& !(h->owner->options & MOD_OPT_PERM))
							continue;
						value = (*(h->func.intfunc))(conf,cepp,CONFIG_SET_ANTI_FLOOD,&errs);
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
					{
						config_error_unknownopt(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, "set::anti-flood",
							cepp->ce_varname);
						errors++;
					}
					continue;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "options")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "hide-ulines"))
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
				else if (!strcmp(cepp->ce_varname, "allow-insane-bans")) {
					CheckDuplicate(cepp, options_allow_insane_bans, "options::allow-insane-bans");
				}
				else if (!strcmp(cepp->ce_varname, "allow-part-if-shunned")) {
					CheckDuplicate(cepp, options_allow_part_if_shunned, "options::allow-part-if-shunned");
				}
				else if (!strcmp(cepp->ce_varname, "disable-cap")) {
					CheckDuplicate(cepp, options_disable_cap, "options::disable-cap");
				}
				else if (!strcmp(cepp->ce_varname, "disable-ipv6")) {
					CheckDuplicate(cepp, options_disable_ipv6, "options::disable-ipv6");
					DISABLE_IPV6 = 1; /* ugly ugly. needs to be done here because at conf runtime is too late. */
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
			config_error("%s:%i: set::hosts has been removed in UnrealIRCd 4. You can use oper::vhost now.",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
			need_34_upgrade = 1;
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
				if (!strcmp(cepp->ce_varname, "stop-on-first-match"))
				{
				} else
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
		else if (!strcmp(cep->ce_varname, "nick-length")) {
			int v;
			CheckDuplicate(cep, nicklen, "nick-length");
			CheckNull(cep);
			v = atoi(cep->ce_vardata);
			if ((v <= 0) || (v > NICKLEN))
			{
				config_error("%s:%i: set::nick-length: value '%d' out of range (should be 1-%d)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, v, NICKLEN);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "ssl")) {
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
				else if (!strcmp(cepp->ce_varname, "dh"))
				{
					CheckNull(cepp);
					CheckDuplicate(cep, ssl_dh, "ssl::dh");
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
						if (!config_binary_flags_search(_SSLFlags, ceppp->ce_varname, ARRAY_SIZEOF(_SSLFlags)))
						{
							config_error("%s:%i: unknown SSL flag '%s'",
								     ceppp->ce_fileptr->cf_filename,
								     ceppp->ce_varlinenum, ceppp->ce_varname);
							errors ++;
						}
				}
				else
				{
					config_error("%s:%i: unknown directive set::ssl::%s",
						cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
						cepp->ce_varname);
					errors++;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "default-ipv6-clone-mask"))
		{
			/* keep this in sync with _test_allow() */
			int ipv6mask;
			ipv6mask = atoi(cep->ce_vardata);
			if (ipv6mask == 0)
			{
				config_error("%s:%d: set::default-ipv6-clone-mask given a value of zero. This cannnot be correct, as it would treat all IPv6 hosts as one host.",
					     cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
			if (ipv6mask > 128)
			{
				config_error("%s:%d: set::default-ipv6-clone-mask was set to %d. The maximum value is 128.",
					     cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					     ipv6mask);
				errors++;
			}
			if (ipv6mask <= 32)
			{
				config_warn("%s:%d: set::default-ipv6-clone-mask was given a very small value.",
					    cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			}
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
	char *ret;
	if (!ce->ce_vardata)
	{
		config_status("%s:%i: loadmodule without filename",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	if (strstr(ce->ce_vardata, "commands.so") || strstr(ce->ce_vardata, "commands.dll"))
	{
		config_error("%s:%i: You are trying to load the 'commands' module, this is no longer supported. "
		             "Fix this by editing your configuration file: remove the loadmodule line for commands and add the following line instead: "
		             "include \"modules.default.conf\";",
		             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		need_34_upgrade = 1;
		return -1;
	}
	if (strstr(ce->ce_vardata, "modules/cloak") && !strcmp(conf->cf_filename, "modules.conf"))
	{
		config_error("You seem to have an include for 'modules.conf'.");
		config_error("If you have this because you are upgrading from 3.4-alpha3 to");
		config_error("UnrealIRCd 4 then please change the include \"modules.conf\";");
		config_error("into an include \"modules.default.conf\"; (probably in your");
		config_error("conf/unrealircd.conf). Yeah, we changed the file name.");
		// TODO ^: silly win32 wrapping prevents this from being displayed otherwise. PLZ FIX! !
		/* let it continue to load anyway? */
	}
	if ((ret = Module_Create(ce->ce_vardata))) {
		config_status("%s:%i: loadmodule %s: failed to load: %s",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata, ret);
		return -1;
	}
	return 1;
}

int	_test_loadmodule(ConfigFile *conf, ConfigEntry *ce)
{
	return 0;
}

void start_listeners(void)
{
	ConfigItem_listen 	*listenptr;
	int failed = 0, ports_bound = 0;
	char boundmsg_ipv4[512], boundmsg_ipv6[512];
	
	*boundmsg_ipv4 = *boundmsg_ipv6 = '\0';

	for (listenptr = conf_listen; listenptr; listenptr = (ConfigItem_listen *) listenptr->next)
	{
		/* Try to bind to any ports that are not yet bound and not marked as temporary */
		if (!(listenptr->options & LISTENER_BOUND) && !listenptr->flag.temporary)
		{
			if (add_listener2(listenptr) == -1)
			{
				ircd_log(LOG_ERROR, "Failed to bind to %s:%i", listenptr->ip, listenptr->port);
				failed = 1;
			} else {
				if (loop.ircd_booted)
				{
					ircd_log(LOG_ERROR, "UnrealIRCd is now also listening on %s:%d (%s)%s",
						listenptr->ip, listenptr->port,
						listenptr->ipv6 ? "IPv6" : "IPv4",
						listenptr->options & LISTENER_SSL ? " (SSL)" : "");
				} else {
					if (listenptr->ipv6)
						snprintf(boundmsg_ipv6+strlen(boundmsg_ipv6), sizeof(boundmsg_ipv6)-strlen(boundmsg_ipv6),
							"%s:%d%s, ", listenptr->ip, listenptr->port,
							listenptr->options & LISTENER_SSL ? "(SSL)" : "");
					else
						snprintf(boundmsg_ipv4+strlen(boundmsg_ipv4), sizeof(boundmsg_ipv4)-strlen(boundmsg_ipv4),
							"%s:%d%s, ", listenptr->ip, listenptr->port,
							listenptr->options & LISTENER_SSL ? "(SSL)" : "");
				}
			}
		}

		/* NOTE: do not merge this with code above (nor in an else block),
		 * as add_listener2() affects this flag.
		 */
		if (listenptr->options & LISTENER_BOUND)
			ports_bound++;
	}

	if (ports_bound == 0)
	{
		ircd_log(LOG_ERROR, "IRCd could not listen on any ports. If you see 'Address already in use' errors "
		                    "above then most likely the IRCd is already running (or something else is using the "
		                    "specified ports). If you are sure the IRCd is not running then verify your "
		                    "listen blocks, maybe you have to bind to a specific IP rather than \"*\".");
		exit(-1);
	}
	
	if (failed && !loop.ircd_booted)
	{
		ircd_log(LOG_ERROR, "Could not listen on all specified addresses/ports. See errors above. "
		                    "Please fix your listen { } blocks and/or make sure no other programs "
		                    "are listening on the same port.");
		exit(-1);
	}

	if (!loop.ircd_booted)
	{
		if (strlen(boundmsg_ipv4) > 2)
			boundmsg_ipv4[strlen(boundmsg_ipv4)-2] = '\0';
		if (strlen(boundmsg_ipv6) > 2)
			boundmsg_ipv6[strlen(boundmsg_ipv6)-2] = '\0';

		ircd_log(LOG_ERROR, "UnrealIRCd is now listening on the following addresses/ports:");
		ircd_log(LOG_ERROR, "IPv4: %s", *boundmsg_ipv4 ? boundmsg_ipv4 : "<none>");
		ircd_log(LOG_ERROR, "IPv6: %s", *boundmsg_ipv6 ? boundmsg_ipv6 : "<none>");
	}
}

/* Actually use configuration */
void run_configuration(void)
{
	start_listeners();
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
		del_Command(ce->ce_vardata, cmptr->func);
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
	safestrdup(alias->alias, ce->ce_vardata);
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "format")) {
			format = MyMallocEx(sizeof(ConfigItem_alias_format));
			safestrdup(format->format, cep->ce_vardata);
			format->expr = unreal_create_match(MATCH_PCRE_REGEX, cep->ce_vardata, NULL);
			if (!format->expr)
				abort(); /* Impossible due to _test_alias earlier */
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "nick") ||
				    !strcmp(cepp->ce_varname, "target") ||
				    !strcmp(cepp->ce_varname, "command")) {
					safestrdup(format->nick, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "parameters")) {
					safestrdup(format->parameters, cepp->ce_vardata);
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
			safestrdup(alias->nick, cep->ce_vardata);
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
		safestrdup(alias->nick, alias->alias);
	}
	CommandAdd(NULL, alias->alias, (void *)m_alias, 1, M_USER|M_ALIAS);
	
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
			char *err = NULL;
			aMatch *expr;
			char has_type = 0, has_target = 0, has_parameters = 0;

			has_format = 1;
			expr = unreal_create_match(MATCH_PCRE_REGEX, cep->ce_vardata, &err);
			if (!expr)
			{
				config_error("%s:%i: alias::format contains an invalid regex: %s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, err);
				config_error("Upgrading from 3.2.x to UnrealIRCd 4? Note that regex changed from POSIX Regex "
				             "to PCRE Regex!"); /* TODO: refer to some url ? */
			} else {
				unreal_delete_match(expr);
			}

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
			safestrdup(deny->filename, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "reason"))
		{
			safestrdup(deny->reason, cep->ce_vardata);
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
			safestrdup(deny->reason, "Possible infected virus file");
		else
			safestrdup(deny->reason, "Possible executable content");
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
			safestrdup(deny->channel, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "redirect"))
		{
			safestrdup(deny->redirect, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "reason"))
		{
			safestrdup(deny->reason, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "warn"))
		{
			deny->warn = config_checkval(cep->ce_vardata,CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "class"))
		{
			safestrdup(deny->class, cep->ce_vardata);
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
			safestrdup(deny->mask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "rule"))
		{
			deny->rule = (char *)crule_parse(cep->ce_vardata);
			safestrdup(deny->prettyrule, cep->ce_vardata);
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
			safestrdup(deny->mask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "version"))
		{
			safestrdup(deny->version, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "flags"))
		{
			safestrdup(deny->flags, cep->ce_vardata);
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
		char has_channel = 0, has_warn = 0, has_reason = 0, has_redirect = 0, has_class = 0;
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
			else if (!strcmp(cep->ce_varname, "class"))
			{
				if (has_class)
				{
					config_warn_duplicate(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny channel::class");
					continue;
				}
				has_class = 1;
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
static void conf_download_complete(const char *url, const char *file, const char *errorbuf, int cached, void *inc_key)
{
	ConfigItem_include *inc;
	if (!loop.ircd_rehashing)
		return;

	/*
	  use inc_key to find the correct include block. This
	  should be cheaper than using the full URL.
	 */
	for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next)
	{
		if ( inc_key != (void *)inc )
			continue;
		if (!(inc->flag.type & INCLUDE_REMOTE))
			continue;
		if (inc->flag.type & INCLUDE_NOTLOADED)
			continue;
		if (stricmp(url, inc->url))
			continue;

		inc->flag.type &= ~INCLUDE_DLQUEUED;
		break;
	}
	if (!inc)
	{
		ircd_log(LOG_ERROR, "Downloaded remote include which matches no include statement.");
		return;
	}

	if (!file && !cached)
		update_remote_include(inc, file, 0, errorbuf); /* DOWNLOAD FAILED */
	else
	{
		char *urlfile = url_getfilename(url);
		char *file_basename = unreal_getfilename(urlfile);
		char *tmp = unreal_mktemp(TMPDIR, file_basename ? file_basename : "download.conf");
		free(urlfile);

		if (cached)
		{
			unreal_copyfileex(inc->file, tmp, 1);
#ifdef REMOTEINC_SPECIALCACHE
			unreal_copyfileex(inc->file, unreal_mkcache(url), 0);
#endif
			update_remote_include(inc, tmp, 0, NULL);
		}
		else
		{
			/*
			  copy/hardlink file to another file because our caller will
			  remove(file).
			*/
			unreal_copyfileex(file, tmp, 1);
			update_remote_include(inc, tmp, 0, NULL);
#ifdef REMOTEINC_SPECIALCACHE
			unreal_copyfileex(file, unreal_mkcache(url), 0);
#endif
		}
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

		/*
		  use (void *)inc as the key for finding which
		  include block conf_download_complete() should use.
		*/
		download_file_async(inc->url, modtime, conf_download_complete, (void *)inc);
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
	extcmodes_check_for_changes();
	loop.ircd_rehashing = 0;
	remote_rehash_client = NULL;
	return 1;
}

void link_cleanup(ConfigItem_link *link_ptr)
{
	safefree(link_ptr->servername);
	unreal_delete_masks(link_ptr->incoming.mask);
	Auth_DeleteAuthStruct(link_ptr->auth);
	safefree(link_ptr->outgoing.bind_ip);
	safefree(link_ptr->outgoing.hostname);
	safefree(link_ptr->hub);
	safefree(link_ptr->leaf);
	safefree(link_ptr->ciphers);
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

void delete_classblock(ConfigItem_class *class_ptr)
{
	Debug((DEBUG_ERROR, "delete_classblock: deleting %s, clients=%d, xrefcount=%d",
		class_ptr->name, class_ptr->clients, class_ptr->xrefcount));
	safefree(class_ptr->name);
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
			safefree(listen_ptr->ip);
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

/**
 * Non-asynchronous remote inclusion to give a user better feedback
 * when first starting his IRCd.
 *
 * The asynchronous friend is rehash() which merely queues remote
 * includes for download using download_file_async().
 */
int remote_include(ConfigEntry *ce)
{
	char *errorbuf = NULL;
	char *url = ce->ce_vardata;
	char *file = find_remote_include(url, &errorbuf);
	int ret;
	if (!loop.ircd_rehashing || (loop.ircd_rehashing && !file && !errorbuf))
	{
		char *error;
		if (config_verbose > 0)
			config_status("Downloading %s", url);
		file = download_file(url, &error);
		if (!file)
		{
#ifdef REMOTEINC_SPECIALCACHE
			if (has_cached_version(url))
			{
				config_warn("%s:%i: include: error downloading '%s': %s -- using cached version instead.",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					url, error);
				file = strdup(unreal_mkcache(url));
				/* Let it pass to load_conf()... */
			} else {
#endif
				config_error("%s:%i: include: error downloading '%s': %s",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					 url, error);
				return -1;
#ifdef REMOTEINC_SPECIALCACHE
			}
#endif
		} else {
#ifdef REMOTEINC_SPECIALCACHE
			unreal_copyfileex(file, unreal_mkcache(url), 0);
#endif
		}
		add_remote_include(file, url, 0, NULL, ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		ret = load_conf(file, url);
		free(file);
		return ret;
	}
	else
	{
		if (errorbuf)
		{
#ifdef REMOTEINC_SPECIALCACHE
			if (has_cached_version(url))
			{
				config_warn("%s:%i: include: error downloading '%s': %s -- using cached version instead.",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					url, errorbuf);
				/* Let it pass to load_conf()... */
				file = strdup(unreal_mkcache(url));
			} else {
#endif
				config_error("%s:%i: include: error downloading '%s': %s",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					url, errorbuf);
				return -1;
#ifdef REMOTEINC_SPECIALCACHE
			}
#endif
		}
		if (config_verbose > 0)
			config_status("Loading %s from download", url);
		add_remote_include(file, url, 0, NULL, ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		ret = load_conf(file, url);
		return ret;
	}
	return 0;
}
#endif

/**
 * Add an item to the conf_include list for the specified file.
 *
 * Checks for whether or not we're performing recursive includes
 * belong in conf_load() because that function is able to return an
 * error code. Any checks in here will end up being ignored by callers
 * and thus will gain us nothing.
 *
 * @param file path to the include file.
 */
void add_include(const char *file, const char *included_from, int included_from_line)
{
	ConfigItem_include *inc;

	inc = MyMallocEx(sizeof(ConfigItem_include));
	inc->file = strdup(file);
	inc->flag.type = INCLUDE_NOTLOADED;
	inc->included_from = strdup(included_from);
	inc->included_from_line = included_from_line;
	AddListItem(inc, conf_include);
}

#ifdef USE_LIBCURL
/**
 * Adds a remote include entry to the config_include list.
 *
 * This is to be called whenever the included_from and
 * included_from_line parameters are known. This means that during a
 * rehash when downloads are done asynchronously, you call this with
 * the inclued_from and included_from_line information. After the
 * download is complete and you know there it is stored in the FS,
 * call update_remote_include().
 */
void add_remote_include(const char *file, const char *url, int flags, const char *errorbuf, const char *included_from, int included_from_line)
{
	ConfigItem_include *inc;

	/* we rely on MyMallocEx() zeroing the ConfigItem_include */
	inc = MyMallocEx(sizeof(ConfigItem_include));
	if (included_from)
	{
		inc->included_from = strdup(included_from);
		inc->included_from_line = included_from_line;
	}
	inc->url = strdup(url);

	update_remote_include(inc, file, INCLUDE_NOTLOADED|INCLUDE_REMOTE|flags, errorbuf);
	AddListItem(inc, conf_include);
}

/**
 * Update certain information in a remote include's config_include list entry.
 *
 * @param file the place on disk where the downloaded remote include
 *        may be found
 * @param flags additional flags to set on the config_include entry
 * @param errorbuf non-NULL if there were errors encountered in
 *        downloading. The error will be stored into the config_include
 *        entry.
 */
void update_remote_include(ConfigItem_include *inc, const char *file, int flags, const char *errorbuf)
{
	/*
	 * file may be NULL when errorbuf is non-NULL and vice-versa.
	 */
	if (file)
		inc->file = strdup(file);
	inc->flag.type |= flags;

	if (errorbuf)
		inc->errorbuf = strdup(errorbuf);
}
#endif

/**
 * Clean up conf_include after a rehash fails because of a
 * configuration file error.
 *
 * Duplicates some in unload_loaded_include().
 */
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
				/* Delete the file, but only if it's not a cached version */
				if (strncmp(inc->file, CACHEDIR, strlen(CACHEDIR)))
				{
					remove(inc->file);
				}
				free(inc->url);
				if (inc->errorbuf)
					free(inc->errorbuf);
			}
#endif
			free(inc->file);
			free(inc->included_from);
			DelListItem(inc, conf_include);
			free(inc);
		}
	}
}

/**
 * Clean up conf_include after a successful rehash to make way for
 * load_includes().
 */
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
				/* Delete the file, but only if it's not a cached version */
				if (strncmp(inc->file, CACHEDIR, strlen(CACHEDIR)))
				{
					remove(inc->file);
				}
				free(inc->url);
				if (inc->errorbuf)
					free(inc->errorbuf);
			}
#endif
			free(inc->file);
			free(inc->included_from);
			DelListItem(inc, conf_include);
			free(inc);
		}
	}
}

/**
 * Mark loaded includes as loaded by removing the INCLUDE_NOTLOADED
 * flag. Meant to be called only after calling
 * unload_loaded_includes().
 */
void load_includes(void)
{
	ConfigItem_include *inc;

	/* Doing this for all the includes should actually be faster
	 * than only doing it for includes that are not-loaded
	 */
	for (inc = conf_include; inc; inc = (ConfigItem_include *)inc->next)
		inc->flag.type &= ~INCLUDE_NOTLOADED;
}

/** Check if an important SSL option is used in config. Only checks link and listen at this time.
 * This will generate a warning on boot & REHASH.
 * If booting, the IRCd will stop.
 */
int ssl_used_in_config_but_unavail(void)
{
	int errors = 0;
	ConfigItem_link *link;
	ConfigItem_listen *listener;

	if (ctx_server && ctx_client)
		return 0; /* everything is functional */

	for (listener = conf_listen; listener; listener = (ConfigItem_listen *)listener->next)
		if (listener->options & LISTENER_SSL)
		{
			config_error("Listen block %s:%d is configured to use SSL, however SSL is unavailable due to an earlier error (certificate/key not loaded?)", listener->ip, listener->port);
			errors++;
		}

	for (link = conf_link; link; link = (ConfigItem_link *)link->next)
		if (link->options & CONNECT_SSL)
		{
			config_error("Link block %s is configured to use SSL, however SSL is unavailable due to an earlier error (certificate/key not loaded?)", link->servername);
			errors++;
		}

	return (errors ? 1 : 0);
}

/** Check if the user attempts to unload (eg: by commenting out) a module
 * that is currently loaded and is tagged as MOD_OPT_PERM_RELOADABLE
 * (in other words: a module that allows re-loading but not un-loading)
 */
int reloadable_perm_module_unloaded(void)
{
Module *m, *m2;
extern Module *Modules;
int ret = 0;

	for (m = Modules; m; m = m->next)
	{
		if ((m->options & MOD_OPT_PERM_RELOADABLE) && (m->flags & MODFLAG_LOADED))
		{
			/* For each module w/MOD_OPT_PERM_RELOADABLE that is currently fully loaded... */
			int found = 0;
			for (m2 = Modules; m2; m2 = m2->next)
			{
				if ((m != m2) && !strcmp(m->header->name, m2->header->name))
					found = 1;
			}
			if (!found)
			{
				config_error("Attempt to unload module '%s' is not permitted. Module is permanent and reloadable only.", m->header->name);
				ret = 1;
				/* we don't return straight away so the user gets to see all errors and not just one */
			}
		}
	}
	return ret;
}
