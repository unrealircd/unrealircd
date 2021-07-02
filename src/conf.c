/*
 *   Unreal Internet Relay Chat Daemon, src/conf.c
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

#include "unrealircd.h"

/*
 * Some typedefs..
*/
typedef struct ConfigCommand ConfigCommand;
struct ConfigCommand
{
	char	*name;
	int	(*conffunc)(ConfigFile *conf, ConfigEntry *ce);
	int 	(*testfunc)(ConfigFile *conf, ConfigEntry *ce);
};

typedef struct NameValue NameValue;
struct NameValue
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
static int	_conf_deny_link		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_deny_channel	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_deny_version	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_require		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_allow_channel	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_loadmodule	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_log		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_alias		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_help		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_offchans		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_sni		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_security_group	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_secret		(ConfigFile *conf, ConfigEntry *ce);

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
static int	_test_require		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_set		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_deny		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_allow_channel	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_loadmodule	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_blacklist_module	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_log		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_alias		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_help		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_offchans		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_sni		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_security_group	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_secret		(ConfigFile *conf, ConfigEntry *ce);

/* This MUST be alphabetized */
static ConfigCommand _ConfigCommands[] = {
	{ "admin", 		_conf_admin,		_test_admin 	},
	{ "alias",		_conf_alias,		_test_alias	},
	{ "allow",		_conf_allow,		_test_allow	},
	{ "ban", 		_conf_ban,		_test_ban	},
	{ "blacklist-module",	NULL,		 	NULL },
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
	{ "official-channels", 	_conf_offchans,		_test_offchans	},
	{ "oper", 		_conf_oper,		_test_oper	},
	{ "operclass",		_conf_operclass,	_test_operclass	},
	{ "require", 		_conf_require,		_test_require	},
	{ "secret",		_conf_secret,		_test_secret	},
	{ "security-group",	_conf_security_group,	_test_security_group	},
	{ "set",		_conf_set,		_test_set	},
	{ "sni",		_conf_sni,		_test_sni	},
	{ "tld",		_conf_tld,		_test_tld	},
	{ "ulines",		_conf_ulines,		_test_ulines	},
	{ "vhost", 		_conf_vhost,		_test_vhost	},
};

/* This MUST be alphabetized */
static NameValue _ListenerFlags[] = {
	{ LISTENER_CLIENTSONLY,  "clientsonly"},
	{ LISTENER_DEFER_ACCEPT, "defer-accept"},
	{ LISTENER_SERVERSONLY,  "serversonly"},
	{ LISTENER_TLS, 	 "ssl"},
	{ LISTENER_NORMAL, 	 "standard"},
	{ LISTENER_TLS, 	 "tls"},
};

/* This MUST be alphabetized */
static NameValue _LinkFlags[] = {
	{ CONNECT_AUTO,	"autoconnect" },
	{ CONNECT_INSECURE,	"insecure" },
	{ CONNECT_QUARANTINE, "quarantine"},
	{ CONNECT_TLS, "ssl" },
	{ CONNECT_TLS, "tls" },
};

/* This MUST be alphabetized */
static NameValue _LogFlags[] = {
	{ LOG_CHGCMDS, "chg-commands" },
	{ LOG_CLIENT, "connects" },
	{ LOG_ERROR, "errors" },
	{ LOG_FLOOD, "flood" },
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
static NameValue _TLSFlags[] = {
	{ TLSFLAG_FAILIFNOCERT, "fail-if-no-clientcert" },
	{ TLSFLAG_DISABLECLIENTCERT, "no-client-certificate" },
	{ TLSFLAG_NOSTARTTLS, "no-starttls" },
};

struct {
	unsigned conf_me : 1;
	unsigned conf_admin : 1;
	unsigned conf_listen : 1;
} requiredstuff;
struct {
	int min, max;
} nicklengths;
struct SetCheck settings;
/*
 * Utilities
*/

void	port_range(char *string, int *start, int *end);
long	config_checkval(char *value, unsigned short flags);

/*
 * Parser
*/

ConfigFile		*config_load(char *filename, char *displayname);
void			config_free(ConfigFile *cfptr);
ConfigFile		*config_parse_with_offset(char *filename, char *confdata, unsigned int line_offset);
ConfigFile	 	*config_parse(char *filename, char *confdata);
ConfigEntry		*config_find_entry(ConfigEntry *ce, char *name);

extern void add_entropy_configfile(struct stat *st, char *buf);
extern void unload_all_unused_snomasks(void);
extern void unload_all_unused_umodes(void);
extern void unload_all_unused_extcmodes(void);
extern void unload_all_unused_caps(void);
extern void unload_all_unused_history_backends(void);

int reloadable_perm_module_unloaded(void);

int tls_tests(void);

/* Conf sub-sub-functions */
void test_tlsblock(ConfigFile *conf, ConfigEntry *cep, int *totalerrors);
void conf_tlsblock(ConfigFile *conf, ConfigEntry *cep, TLSOptions *tlsoptions);
void free_tls_options(TLSOptions *tlsoptions);

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
ConfigItem_sni		*conf_sni = NULL;
ConfigItem_allow	*conf_allow = NULL;
ConfigItem_except	*conf_except = NULL;
ConfigItem_vhost	*conf_vhost = NULL;
ConfigItem_link		*conf_link = NULL;
ConfigItem_ban		*conf_ban = NULL;
ConfigItem_deny_channel *conf_deny_channel = NULL;
ConfigItem_allow_channel *conf_allow_channel = NULL;
ConfigItem_deny_link	*conf_deny_link = NULL;
ConfigItem_deny_version *conf_deny_version = NULL;
ConfigItem_log		*conf_log = NULL;
ConfigItem_alias	*conf_alias = NULL;
ConfigItem_include	*conf_include = NULL;
ConfigItem_blacklist_module	*conf_blacklist_module = NULL;
ConfigItem_help		*conf_help = NULL;
ConfigItem_offchans	*conf_offchans = NULL;
SecurityGroup		*securitygroups = NULL;
Secret			*secrets = NULL;

MODVAR Configuration		iConf;
MODVAR Configuration		tempiConf;
MODVAR ConfigFile		*conf = NULL;
extern NameValueList *config_defines;
MODVAR int ipv6_disabled = 0;
MODVAR Client *remote_rehash_client = NULL;

MODVAR int			config_error_flag = 0;
int			config_verbose = 0;

MODVAR int need_34_upgrade = 0;
int need_operclass_permissions_upgrade = 0;
int have_tls_listeners = 0;
char *port_6667_ip = NULL;

void add_include(const char *filename, const char *included_from, int included_from_line);
#ifdef USE_LIBCURL
void add_remote_include(const char *, const char *, int, const char *, const char *included_from, int included_from_line);
void update_remote_include(ConfigItem_include *inc, const char *file, int, const char *errorbuf);
int remote_include(ConfigEntry *ce);
#endif
void unload_notloaded_includes(void);
void load_includes(void);
void unload_loaded_includes(void);
int rehash_internal(Client *client, int sig);
int is_blacklisted_module(char *name);

/** Return the printable string of a 'cep' location, such as set::something::xyz */
char *config_var(ConfigEntry *cep)
{
	static char buf[256];
	ConfigEntry *e;
	char *elem[16];
	int numel = 0, i;

	if (!cep)
		return "";

	buf[0] = '\0';

	/* First, walk back to the top */
	for (e = cep; e; e = e->ce_prevlevel)
	{
		elem[numel++] = e->ce_varname;
		if (numel == 15)
			break;
	}

	/* Now construct the xxx::yyy::zzz string */
	for (i = numel-1; i >= 0; i--)
	{
		strlcat(buf, elem[i], sizeof(buf));
		if (i > 0)
			strlcat(buf, "::", sizeof(buf));
	}

	return buf;
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

/** Find an anti-flood settings block by name.
 * @param name		The name of the set::anti-flood block
 * @returns The FloodSettings block if found, or NULL if not found.
 */
FloodSettings *find_floodsettings_block_ex(Configuration *conf, const char *name)
{
	FloodSettings *f;

	for (f = conf->floodsettings; f; f = f->next)
		if (!strcmp(f->name, name))
			return f;

	return NULL;
}

/** Find an anti-flood settings block by name.
 * @param name		The name of the set::anti-flood block
 * @returns The FloodSettings block if found, or NULL if not found.
 */
FloodSettings *find_floodsettings_block(const char *name)
{
	return find_floodsettings_block_ex(&iConf, name);
}

/** Check if 'name' is in the array 'list'.
 * @param name		The name to check
 * @param list		The char *list[] with the list of valid names.
 * @returns 1 if found, 0 if not
 * @note The array in list must end in a NULL element!
 */
int text_in_array(const char *name, const char *list[])
{
	int i;

	for (i=0; list[i]; i++)
		if (!strcmp(name, list[i]))
			return 1;

	return 0; /* Not found */
}

int flood_option_is_old(const char *name)
{
	const char *opts[] =
	{
		"max-concurrent-conversations",
		"unknown-flood-amount",
		"unknown-flood-bantime",
		"handshake-data-flood",
		"away-count",
		"away-period",
		"away-flood",
		"nick-flood",
		"join-flood",
		"invite-flood",
		"knock-flood",
		"connect-flood",
		"target-flood",
		NULL
	};

	return text_in_array(name, opts);
}

int flood_option_is_for_everyone(const char *name)
{
	const char *opts[] =
	{
		"connect-flood",
		"handshake-data-flood",
		"unknown-flood",
		"target-flood",
		NULL
	};

	return text_in_array(name, opts);
}

/** Free a FloodSettings struct */
void free_floodsettings(FloodSettings *f)
{
	safe_free(f->name);
	safe_free(f);
}

/** Parses a value like '5:60s' into a flood setting that we can store.
 * @param str		The string to parse (eg: '5:60s')
 * @param settings	The FloodSettings block to store the result in
 * @param opt		The option (eg: FLD_AWAY)
 * @returns 1 if OK, 0 for parse error.
 */
int config_parse_flood_generic(const char *str, Configuration *conf, char *blockname, FloodOption opt)
{
	char buf[64], *p;
	FloodSettings *settings = find_floodsettings_block_ex(conf, blockname);

	/* Create a new anti-flood block if it doesn't exist */
	if (!settings)
	{
		settings = safe_alloc(sizeof(FloodSettings));
		safe_strdup(settings->name, blockname);
		AddListItem(settings, conf->floodsettings);
	}

	if (!strcmp(str, "unlimited") || !strcmp(str, "max"))
	{
		settings->limit[opt] = -1;
		settings->period[opt] = 0;
		return 1;
	}

	/* Work on a copy so we don't destroy 'str' */
	strlcpy(buf, str, sizeof(buf));

	p = strchr(buf, ':');

	/* 'blah', ':blah', '1:' */
	if (!p || (p == buf) || (*(p+1) == '\0'))
		return 0;

	*p++ = '\0';

	settings->limit[opt] = atoi(buf);
	settings->period[opt] = config_checkval(p, CFG_TIME);

	return 1;
}

long config_checkval(char *orig, unsigned short flags) {
	char *value = raw_strdup(orig);
	char *text;
	long ret = 0;

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

				*sz = 0;
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
		sz--; /* -1 because we are PAST the end of the string */
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

				*sz = 0;
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
		sz--; /* -1 because we are PAST the end of the string */
		while (sz-- > value) {
			if (isspace(*sz))
				*sz = 0;
			if (!isdigit(*sz))
				break;
		}
		ret += atoi(sz+1)*mfactor;
	}
	safe_free(value);
	return ret;
}

/** Free configuration setting for set::modes-on-join */
void free_conf_channelmodes(struct ChMode *store)
{
	int i;

	store->mode = 0;
	store->extmodes = 0;
	for (i = 0; i < EXTCMODETABLESZ; i++)
		safe_free(store->extparams[i]);
}

/* Set configuration, used for set::modes-on-join */
void conf_channelmodes(char *modes, struct ChMode *store, int warn)
{
	CoreChannelModeTable *tab;
	char *params = strchr(modes, ' ');
	char *parambuf = NULL;
	char *param = NULL;
	char *save = NULL;

	warn = 0; // warn is broken

	/* Free existing parameters first (no inheritance) */
	free_conf_channelmodes(store);

	if (params)
	{
		params++;
		safe_strdup(parambuf, params);
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
		for (tab = &corechannelmodetable[0]; tab->mode; tab++)
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
						param = Channelmode_Table[i].conv_param(param, NULL, NULL);
						if (!param)
							break; /* invalid parameter fmt, do not set mode. */
						store->extparams[i] = raw_strdup(param);
						/* Get next parameter */
						param = strtoken(&save, NULL, " ");
					}
					store->extmodes |= Channelmode_Table[i].mode;
					break;
				}
			}
		}
	}
	safe_free(parambuf);
}

void chmode_str(struct ChMode *modes, char *mbuf, char *pbuf, size_t mbuf_size, size_t pbuf_size)
{
	CoreChannelModeTable *tab;
	int i;

	if (!(mbuf_size && pbuf_size))
		return;

	*pbuf = 0;
	*mbuf++ = '+';
	if (--mbuf_size == 0) return;
	for (tab = &corechannelmodetable[0]; tab->mode; tab++)
	{
		if (modes->mode & tab->mode)
		{
			if (!tab->parameters)
			{
				*mbuf++ = tab->flag;
				if (!--mbuf_size)
				{
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
			if (mbuf_size)
			{
				*mbuf++ = Channelmode_Table[i].flag;
				if (!--mbuf_size)
				{
					*--mbuf=0;
					break;
				}
			}
			if (Channelmode_Table[i].paracount)
			{
				strlcat(pbuf, modes->extparams[i], pbuf_size);
				strlcat(pbuf, " ", pbuf_size);
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
		return CHFL_CHANADMIN;
#else
		return CHFL_CHANOP|CHFL_CHANADMIN;
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
		case CHFL_CHANADMIN:
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
		case CHFL_CHANADMIN:
			return 'a';
		case CHFL_CHANOWNER:
			return 'q';
		case CHFL_DEOPPED:
		default:
			return '\0';
	}
	/* NOT REACHED */
}

Policy policy_strtoval(char *s)
{
	if (!s)
		return 0;

	if (!strcmp(s, "allow"))
		return POLICY_ALLOW;

	if (!strcmp(s, "warn"))
		return POLICY_WARN;

	if (!strcmp(s, "deny"))
		return POLICY_DENY;

	return 0;
}

char *policy_valtostr(Policy policy)
{
	if (policy == POLICY_ALLOW)
		return "allow";
	if (policy == POLICY_WARN)
		return "warn";
	if (policy == POLICY_DENY)
		return "deny";
	return "???";
}

char policy_valtochar(Policy policy)
{
	if (policy == POLICY_ALLOW)
		return 'a';
	if (policy == POLICY_WARN)
		return 'w';
	if (policy == POLICY_DENY)
		return 'd';
	return '?';
}

AllowedChannelChars allowed_channelchars_strtoval(char *str)
{
	if (!strcmp(str, "ascii"))
		return ALLOWED_CHANNELCHARS_ASCII;
	else if (!strcmp(str, "utf8"))
		return ALLOWED_CHANNELCHARS_UTF8;
	else if (!strcmp(str, "any"))
		return ALLOWED_CHANNELCHARS_ANY;
	return 0;
}

char *allowed_channelchars_valtostr(AllowedChannelChars v)
{
	switch(v)
	{
		case ALLOWED_CHANNELCHARS_ASCII:
			return "ascii";
		case ALLOWED_CHANNELCHARS_UTF8:
			return "utf8";
		case ALLOWED_CHANNELCHARS_ANY:
			return "any";
		default:
			/* Not possible */
			abort();
			return "NOTREACHED"; /* Windows.. */
	}
}

/* Used for set::automatic-ban-target and set::manual-ban-target */
BanTarget ban_target_strtoval(char *str)
{
	if (!strcmp(str, "ip"))
		return BAN_TARGET_IP;
	else if (!strcmp(str, "userip"))
		return BAN_TARGET_USERIP;
	else if (!strcmp(str, "host"))
		return BAN_TARGET_HOST;
	else if (!strcmp(str, "userhost"))
		return BAN_TARGET_USERHOST;
	else if (!strcmp(str, "account"))
		return BAN_TARGET_ACCOUNT;
	else if (!strcmp(str, "certfp"))
		return BAN_TARGET_CERTFP;
	return 0; /* invalid */
}

/* Used for set::automatic-ban-target and set::manual-ban-target */
char *ban_target_valtostr(BanTarget v)
{
	switch(v)
	{
		case BAN_TARGET_IP:
			return "ip";
		case BAN_TARGET_USERIP:
			return "userip";
		case BAN_TARGET_HOST:
			return "host";
		case BAN_TARGET_USERHOST:
			return "userhost";
		case BAN_TARGET_ACCOUNT:
			return "account";
		case BAN_TARGET_CERTFP:
			return "certfp";
		default:
			return "???";
	}
}

HideIdleTimePolicy hideidletime_strtoval(char *str)
{
	if (!strcmp(str, "never"))
		return HIDE_IDLE_TIME_NEVER;
	else if (!strcmp(str, "always"))
		return HIDE_IDLE_TIME_ALWAYS;
	else if (!strcmp(str, "usermode"))
		return HIDE_IDLE_TIME_USERMODE;
	else if (!strcmp(str, "oper-usermode"))
		return HIDE_IDLE_TIME_OPER_USERMODE;
	return 0;
}

char *hideidletime_valtostr(HideIdleTimePolicy v)
{
	switch(v)
	{
		case HIDE_IDLE_TIME_NEVER:
			return "never";
		case HIDE_IDLE_TIME_ALWAYS:
			return "always";
		case HIDE_IDLE_TIME_USERMODE:
			return "usermode";
		case HIDE_IDLE_TIME_OPER_USERMODE:
			return "oper-usermode";
		default:
			return "INVALID";
	}
}

ConfigFile *config_load(char *filename, char *displayname)
{
	struct stat sb;
	int			fd;
	int			ret;
	char		*buf = NULL;
	ConfigFile	*cfptr;

	if (!displayname)
		displayname = filename;

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
	buf = safe_alloc(sb.st_size+1);
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
		safe_free(buf);
		close(fd);
		return NULL;
	}
	/* Just me or could this cause memory corrupted when ret <0 ? */
	buf[ret] = '\0';
	close(fd);
	add_entropy_configfile(&sb, buf);
	cfptr = config_parse(displayname, buf);
	safe_free(buf);
	return cfptr;
}

void config_free(ConfigFile *cfptr)
{
	ConfigFile	*nptr;

	for(;cfptr;cfptr=nptr)
	{
		nptr = cfptr->cf_next;
		if (cfptr->cf_entries)
			config_entry_free_all(cfptr->cf_entries);
		safe_free(cfptr->cf_filename);
		safe_free(cfptr);
	}
}

/** Remove quotes so that 'hello \"all\" \\ lala' becomes 'hello "all" \ lala' */
void unreal_del_quotes(char *i)
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

/** Add quotes to a line, eg some"thing becomes some\"thing - extended version */
int unreal_add_quotes_r(char *i, char *o, size_t len)
{
	if (len == 0)
		return 0;
	
	len--; /* reserve room for nul byte */

	if (len == 0)
	{
		*o = '\0';
		return 0;
	}
	
	for (; *i; i++)
	{
		if ((*i == '"') || (*i == '\\')) /* only " and \ need to be quoted */
		{
			if (len < 2)
				break;
			*o++ = '\\';
			*o++ = *i;
			len -= 2;
		} else
		{
			if (len == 0)
				break;
			*o++ = *i;
			len--;
		}
	}
	*o = '\0';
	
	return 1;
}	

/** Add quotes to a line, eg some"thing becomes some\"thing */
char *unreal_add_quotes(char *str)
{
	static char qbuf[2048];
	
	*qbuf = '\0';
	unreal_add_quotes_r(str, qbuf, sizeof(qbuf));
	return qbuf;
}

ConfigFile *config_parse(char *filename, char *confdata){
	return config_parse_with_offset(filename, confdata, 0);
}

/* This is the internal parser, made by Chris Behrens & Fred Jacobs <2005.
 * Enhanced (or mutilated) by Bram Matthys over the years (2015-2019).
 */
ConfigFile *config_parse_with_offset(char *filename, char *confdata, unsigned int line_offset)
{
	char		*ptr;
	char		*start;
	int		linenumber = 1+line_offset;
	int errors = 0;
	int n;
	ConfigEntry	*curce;
	ConfigEntry	**lastce;
	ConfigEntry	*cursection;
	ConfigFile	*curcf;
	int preprocessor_level = 0;
	ConditionalConfig *cc, *cc_list = NULL;

	curcf = safe_alloc(sizeof(ConfigFile));
	safe_strdup(curcf->cf_filename, filename);
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
					config_entry_free_all(curce);
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
				if (*(ptr+1) != ';')
				{
					/* Simulate closing ; so you can get away with } instead of ugly }; */
					*lastce = curce;
					lastce = &(curce->ce_next);
					curce->ce_fileposend = (ptr - confdata);
					curce = NULL;
				}
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

					for(ptr+=2;*ptr;ptr++)
					{
						if (*ptr == '\n')
						{
							linenumber++;
						} else
						if ((*ptr == '*') && (*(ptr+1) == '/'))
						{
							ptr++;
							break;
						}
					}
					if (!*ptr)
					{
						config_error("%s:%i Comment on line %d does not end\n",
							filename, commentstart, commentstart);
						errors++;
						config_entry_free_all(curce);
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
					config_entry_free_all(curce);
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
						safe_strldup(curce->ce_vardata, start, ptr-start+1);
						preprocessor_replace_defines(&curce->ce_vardata, curce);
						unreal_del_quotes(curce->ce_vardata);
					}
				}
				else
				{
					curce = safe_alloc(sizeof(ConfigEntry));
					curce->ce_varlinenum = linenumber;
					curce->ce_fileptr = curcf;
					curce->ce_prevlevel = cursection;
					curce->ce_fileposstart = (start - confdata);
					safe_strldup(curce->ce_varname, start, ptr-start+1);
					preprocessor_replace_defines(&curce->ce_varname, curce);
					unreal_del_quotes(curce->ce_varname);
					preprocessor_cc_duplicate_list(cc_list, &curce->ce_cond);
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
			case '@':
				/* Preprocessor item, such as @if, @define, etc. */
				start = ptr;
				for (;*ptr; ptr++)
				{
					if (*ptr == '\n')
						break;
				}
				cc = NULL;
				n = parse_preprocessor_item(start, ptr, filename, linenumber, &cc);
				linenumber++;
				if (n == PREPROCESSOR_IF)
				{
					preprocessor_level++;
					cc->priority = preprocessor_level;
					AddListItem(cc, cc_list);
				} else
				if (n == PREPROCESSOR_ENDIF)
				{
					if (preprocessor_level == 0)
					{
						config_error("%s:%i: @endif unexpected. There was no preciding unclosed @if.",
							filename, linenumber);
						errors++;
					}
					preprocessor_cc_free_level(&cc_list, preprocessor_level);
					preprocessor_level--;
				} else
				if (n == PREPROCESSOR_ERROR)
				{
					errors++;
					goto breakout;
				}

				if (!*ptr)
					goto breakout; /* special case, since we don't want the for loop to ptr++ */

				break;
			default:
				if ((*ptr == '*') && (*(ptr+1) == '/'))
				{
					config_status("%s:%i: Ignoring extra end comment\n",
						filename, linenumber);
					config_status("WARNING: Starting with UnrealIRCd 4.2.1 a /*-style comment stops as soon as the first */ is encountered. "
					              "See https://www.unrealircd.org/docs/FAQ#Nesting_comments for more information.");
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
					config_entry_free_all(curce);
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
						safe_strldup(curce->ce_vardata, start, ptr-start+1);
						preprocessor_replace_defines(&curce->ce_vardata, curce);
					}
				}
				else
				{
					curce = safe_alloc(sizeof(ConfigEntry));
					memset(curce, 0, sizeof(ConfigEntry));
					curce->ce_varlinenum = linenumber;
					curce->ce_fileptr = curcf;
					curce->ce_prevlevel = cursection;
					curce->ce_fileposstart = (start - confdata);
					safe_strldup(curce->ce_varname, start, ptr-start+1);
					preprocessor_replace_defines(&curce->ce_varname, curce);
					if (curce->ce_cond)
						abort(); // hmm this can be reached? FIXME!
					preprocessor_cc_duplicate_list(cc_list, &curce->ce_cond);
				}
				if ((*ptr == ';') || (*ptr == '\n'))
					ptr--;
				break;
		} /* switch */
		if (!*ptr) /* This IS possible. -- Syzop */
			break;
	} /* for */
breakout:
	if (curce)
	{
		config_error("%s: End of file reached but directive or block at line %i did not end properly. "
		             "Perhaps a missing ; (semicolon) somewhere?\n",
			filename, curce->ce_varlinenum);
		errors++;
		config_entry_free_all(curce);
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

/** Free a ConfigEntry struct, all it's children, and all it's next entries.
 * Consider calling config_entry_free() instead of this one.. or at least
 * check which one of the two you actually need ;)
 */
void config_entry_free_all(ConfigEntry *ce)
{
	ConfigEntry	*nptr;

	for(;ce;ce=nptr)
	{
		nptr = ce->ce_next;
		if (ce->ce_entries)
			config_entry_free_all(ce->ce_entries);
		safe_free(ce->ce_varname);
		safe_free(ce->ce_vardata);
		if (ce->ce_cond)
			preprocessor_cc_free_list(ce->ce_cond);
		safe_free(ce);
	}
}

/** Free a specific ConfigEntry struct (and it's children).
 * Caller must ensure that the entry is not in the linked list anymore.
 */
void config_entry_free(ConfigEntry *ce)
{
	if (ce->ce_entries)
		config_entry_free_all(ce->ce_entries);
	safe_free(ce->ce_varname);
	safe_free(ce->ce_vardata);
	if (ce->ce_cond)
		preprocessor_cc_free_list(ce->ce_cond);
	safe_free(ce);
}

ConfigEntry *config_find_entry(ConfigEntry *ce, char *name)
{
	ConfigEntry *cep;

	for (cep = ce; cep; cep = cep->ce_next)
		if (cep->ce_varname && !strcmp(cep->ce_varname, name))
			break;
	return cep;
}

void config_error(FORMAT_STRING(const char *format), ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	ircd_log(LOG_ERROR, "config error: %s", buffer);
	sendto_realops("error: %s", buffer);
	if (remote_rehash_client)
		sendnotice(remote_rehash_client, "error: %s", buffer);
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

void config_status(FORMAT_STRING(const char *format), ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsnprintf(buffer, 1023, format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	ircd_log(LOG_ERROR, "%s", buffer);
	sendto_realops("%s", buffer);
	if (remote_rehash_client)
		sendnotice(remote_rehash_client, "%s", buffer);
}

void config_warn(FORMAT_STRING(const char *format), ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsnprintf(buffer, 1023, format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	ircd_log(LOG_ERROR, "[warning] %s", buffer);
	sendto_realops("[warning] %s", buffer);
	if (remote_rehash_client)
		sendnotice(remote_rehash_client, "[warning] %s", buffer);
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
#else
	if (strstr(cep->ce_vardata, "://"))
	{
		config_error("%s:%d: %s: UnrealIRCd was not compiled with remote includes support "
		             "so you cannot use URLs here.",
		             cep->ce_fileptr->cf_filename,
		             cep->ce_varlinenum,
		             entry);
		return 1;
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

int config_is_blankorempty(ConfigEntry *cep, const char *block)
{
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

void	free_iConf(Configuration *i)
{
	FloodSettings *f, *f_next;

	safe_free(i->dns_bindip);
	safe_free(i->link_bindip);
	safe_free(i->kline_address);
	safe_free(i->gline_address);
	safe_free(i->oper_snomask);
	safe_free(i->auto_join_chans);
	safe_free(i->oper_auto_join_chans);
	safe_free(i->allow_user_stats);
	// allow_user_stats_ext is freed elsewhere
	safe_free(i->egd_path);
	safe_free(i->static_quit);
	safe_free(i->static_part);
	free_tls_options(i->tls_options);
	i->tls_options = NULL;
	safe_free(i->tls_options);
	safe_free_multiline(i->plaintext_policy_user_message);
	safe_free_multiline(i->plaintext_policy_oper_message);
	safe_free(i->outdated_tls_policy_user_message);
	safe_free(i->outdated_tls_policy_oper_message);
	safe_free(i->restrict_usermodes);
	safe_free(i->restrict_channelmodes);
	safe_free(i->restrict_extendedbans);
	safe_free(i->channel_command_prefix);
	safe_free(i->spamfilter_ban_reason);
	safe_free(i->spamfilter_virus_help_channel);
	// spamexcept is freed elsewhere
	safe_free(i->spamexcept_line);
	safe_free(i->reject_message_too_many_connections);
	safe_free(i->reject_message_server_full);
	safe_free(i->reject_message_unauthorized);
	safe_free(i->reject_message_kline);
	safe_free(i->reject_message_gline);
	// network struct:
	safe_free(i->network.x_ircnetwork);
	safe_free(i->network.x_ircnet005);
	safe_free(i->network.x_defserv);
	safe_free(i->network.x_services_name);
	safe_free(i->network.x_hidden_host);
	safe_free(i->network.x_prefix_quit);
	safe_free(i->network.x_helpchan);
	safe_free(i->network.x_stats_server);
	safe_free(i->network.x_sasl_server);
	// anti-flood:
	for (f = i->floodsettings; f; f = f_next)
	{
		f_next = f->next;
		free_floodsettings(f);
	}
	i->floodsettings = NULL;
}

int	config_test();

void config_setdefaultsettings(Configuration *i)
{
	char tmp[512];

	safe_strdup(i->oper_snomask, SNO_DEFOPER);
	i->ident_read_timeout = 7;
	i->ident_connect_timeout = 3;
	i->ban_version_tkl_time = 86400; /* 1d */
	i->spamfilter_ban_time = 86400; /* 1d */
	safe_strdup(i->spamfilter_ban_reason, "Spam/advertising");
	safe_strdup(i->spamfilter_virus_help_channel, "#help");
	i->spamfilter_detectslow_warn = 250;
	i->spamfilter_detectslow_fatal = 500;
	i->spamfilter_stop_on_first_match = 1;
	i->maxchannelsperuser = 10;
	i->maxdccallow = 10;
	safe_strdup(i->channel_command_prefix, "`!.");
	conf_channelmodes("+nt", &i->modes_on_join, 0);
	i->conn_modes = set_usermode("+ixw");
	i->check_target_nick_bans = 1;
	i->maxbans = 60;
	i->maxbanlength = 2048;
	i->level_on_join = CHFL_CHANOP;
	i->watch_away_notification = 1;
	i->uhnames = 1;
	i->ping_cookie = 1;
	i->ping_warning = 15; /* default ping warning notices 15 seconds */
	i->default_ipv6_clone_mask = 64;
	nicklengths.min = i->min_nick_length = 0; /* 0 means no minimum required */
	nicklengths.max = i->nick_length = NICKLEN;
	i->topic_length = 360;
	i->away_length = 307;
	i->kick_length = 307;
	i->quit_length = 307;
	safe_strdup(i->link_bindip, "*");
	safe_strdup(i->network.x_hidden_host, "Clk");
	if (!ipv6_capable())
		DISABLE_IPV6 = 1;
	safe_strdup(i->network.x_prefix_quit, "Quit");
	i->max_unknown_connections_per_ip = 3;
	i->handshake_timeout = 30;
	i->sasl_timeout = 15;
	i->handshake_delay = -1;
	i->broadcast_channel_messages = BROADCAST_CHANNEL_MESSAGES_AUTO;

	/* Flood options */
	/* - everyone */
	i->throttle_count = 3; i->throttle_period = 60; /* throttle protection: max 3 per 60s */
	i->handshake_data_flood_amount = 4096;
	i->handshake_data_flood_ban_action = BAN_ACT_ZLINE;
	i->handshake_data_flood_ban_time = 600;
	// (targetflood is in the targetflood module)
	/* - known-users */
	config_parse_flood_generic("3:60", i, "known-users", FLD_NICK); /* NICK flood protection: max 3 per 60s */
	config_parse_flood_generic("3:90", i, "known-users", FLD_JOIN); /* JOIN flood protection: max 3 per 90s */
	config_parse_flood_generic("4:120", i, "known-users", FLD_AWAY); /* AWAY flood protection: max 4 per 120s */
	config_parse_flood_generic("4:60", i, "known-users", FLD_INVITE); /* INVITE flood protection: max 4 per 60s */
	config_parse_flood_generic("4:120", i, "known-users", FLD_KNOCK); /* KNOCK protection: max 4 per 120s */
	config_parse_flood_generic("10:15", i, "known-users", FLD_CONVERSATIONS); /* 10 users, new user every 15s */
	config_parse_flood_generic("180:750", i, "known-users", FLD_LAG_PENALTY); /* 180 bytes / 750 msec */
	/* - unknown-users */
	config_parse_flood_generic("2:60", i, "unknown-users", FLD_NICK); /* NICK flood protection: max 2 per 60s */
	config_parse_flood_generic("2:90", i, "unknown-users", FLD_JOIN); /* JOIN flood protection: max 2 per 90s */
	config_parse_flood_generic("4:120", i, "unknown-users", FLD_AWAY); /* AWAY flood protection: max 4 per 120s */
	config_parse_flood_generic("2:60", i, "unknown-users", FLD_INVITE); /* INVITE flood protection: max 2 per 60s */
	config_parse_flood_generic("2:120", i, "unknown-users", FLD_KNOCK); /* KNOCK protection: max 2 per 120s */
	config_parse_flood_generic("4:15", i, "unknown-users", FLD_CONVERSATIONS); /* 4 users, new user every 15s */
	config_parse_flood_generic("90:1000", i, "unknown-users", FLD_LAG_PENALTY); /* 90 bytes / 1000 msec */

	/* SSL/TLS options */
	i->tls_options = safe_alloc(sizeof(TLSOptions));
	snprintf(tmp, sizeof(tmp), "%s/tls/server.cert.pem", CONFDIR);
	safe_strdup(i->tls_options->certificate_file, tmp);
	snprintf(tmp, sizeof(tmp), "%s/tls/server.key.pem", CONFDIR);
	safe_strdup(i->tls_options->key_file, tmp);
	snprintf(tmp, sizeof(tmp), "%s/tls/curl-ca-bundle.crt", CONFDIR);
	safe_strdup(i->tls_options->trusted_ca_file, tmp);
	safe_strdup(i->tls_options->ciphers, UNREALIRCD_DEFAULT_CIPHERS);
	safe_strdup(i->tls_options->ciphersuites, UNREALIRCD_DEFAULT_CIPHERSUITES);
	i->tls_options->protocols = TLS_PROTOCOL_ALL;
#ifdef HAS_SSL_CTX_SET1_CURVES_LIST
	safe_strdup(i->tls_options->ecdh_curves, UNREALIRCD_DEFAULT_ECDH_CURVES);
#endif
	safe_strdup(i->tls_options->outdated_protocols, "TLSv1,TLSv1.1");
	/* the following may look strange but "AES*" matches all
	 * AES ciphersuites that do not have Forward Secrecy.
	 * Any decent client using AES will use ECDHE-xx-AES.
	 */
	safe_strdup(i->tls_options->outdated_ciphers, "AES*,RC4*,DES*");

	i->plaintext_policy_user = POLICY_ALLOW;
	i->plaintext_policy_oper = POLICY_DENY;
	i->plaintext_policy_server = POLICY_DENY;

	i->outdated_tls_policy_user = POLICY_WARN;
	i->outdated_tls_policy_oper = POLICY_DENY;
	i->outdated_tls_policy_server = POLICY_DENY;

	safe_strdup(i->reject_message_too_many_connections, "Too many connections from your IP");
	safe_strdup(i->reject_message_server_full, "This server is full");
	safe_strdup(i->reject_message_unauthorized, "You are not authorized to connect to this server");
	safe_strdup(i->reject_message_kline, "You are not welcome on this server. $bantype: $banreason. Email $klineaddr for more information.");
	safe_strdup(i->reject_message_gline, "You are not welcome on this network. $bantype: $banreason. Email $glineaddr for more information.");

	i->topic_setter = SETTER_NICK;
	i->ban_setter = SETTER_NICK;
	i->ban_setter_sync = 1;

	i->allowed_channelchars = ALLOWED_CHANNELCHARS_UTF8;

	i->automatic_ban_target = BAN_TARGET_IP;
	i->manual_ban_target = BAN_TARGET_HOST;

	i->hide_idle_time = HIDE_IDLE_TIME_OPER_USERMODE;

	i->who_limit = 100;
}

static void make_default_logblock(void)
{
	ConfigItem_log *ca = safe_alloc(sizeof(ConfigItem_log));

	config_status("No log { } block found -- logging everything to 'ircd.log'");

	safe_strdup(ca->file, "ircd.log");
	convert_to_absolute_path(&ca->file, LOGDIR);
	ca->flags |= LOG_CHGCMDS|LOG_CLIENT|LOG_ERROR|LOG_KILL|LOG_KLINE|LOG_OPER|LOG_OVERRIDE|LOG_SACMDS|LOG_SERVER|LOG_SPAMFILTER|LOG_TKL;
	ca->logfd = -1;
	AddListItem(ca, conf_log);
}

/** Similar to config_setdefaultsettings but this one is applied *AFTER*
 * the entire configuration has been ran (sometimes this is the only way it can be done..).
 * NOTE: iConf is thus already populated with (non-default) values. Only overwrite if necessary!
 */
void postconf_defaults(void)
{
	TKL *tk;
	char *encoded;

	if (!iConf.plaintext_policy_user_message)
	{
		/* The message depends on whether it's reject or warn.. */
		if (iConf.plaintext_policy_user == POLICY_DENY)
			addmultiline(&iConf.plaintext_policy_user_message, "Insecure connection. Please reconnect using SSL/TLS.");
		else if (iConf.plaintext_policy_user == POLICY_WARN)
			addmultiline(&iConf.plaintext_policy_user_message, "WARNING: Insecure connection. Please consider using SSL/TLS.");
	}

	if (!iConf.plaintext_policy_oper_message)
	{
		/* The message depends on whether it's reject or warn.. */
		if (iConf.plaintext_policy_oper == POLICY_DENY)
		{
			addmultiline(&iConf.plaintext_policy_oper_message, "You need to use a secure connection (SSL/TLS) in order to /OPER.");
			addmultiline(&iConf.plaintext_policy_oper_message, "See https://www.unrealircd.org/docs/FAQ#oper-requires-tls");
		}
		else if (iConf.plaintext_policy_oper == POLICY_WARN)
			addmultiline(&iConf.plaintext_policy_oper_message, "WARNING: You /OPER'ed up from an insecure connection. Please consider using SSL/TLS.");
	}

	if (!iConf.outdated_tls_policy_user_message)
	{
		/* The message depends on whether it's reject or warn.. */
		if (iConf.outdated_tls_policy_user == POLICY_DENY)
			safe_strdup(iConf.outdated_tls_policy_user_message, "Your IRC client is using an outdated SSL/TLS protocol or ciphersuite ($protocol-$cipher). Please upgrade your IRC client.");
		else if (iConf.outdated_tls_policy_user == POLICY_WARN)
			safe_strdup(iConf.outdated_tls_policy_user_message, "WARNING: Your IRC client is using an outdated SSL/TLS protocol or ciphersuite ($protocol-$cipher). Please upgrade your IRC client.");
	}

	if (!iConf.outdated_tls_policy_oper_message)
	{
		/* The message depends on whether it's reject or warn.. */
		if (iConf.outdated_tls_policy_oper == POLICY_DENY)
			safe_strdup(iConf.outdated_tls_policy_oper_message, "Your IRC client is using an outdated SSL/TLS protocol or ciphersuite ($protocol-$cipher). Please upgrade your IRC client.");
		else if (iConf.outdated_tls_policy_oper == POLICY_WARN)
			safe_strdup(iConf.outdated_tls_policy_oper_message, "WARNING: Your IRC client is using an outdated SSL/TLS protocol or ciphersuite ($protocol-$cipher). Please upgrade your IRC client.");
	}

	/* We got a chicken-and-egg problem here.. antries added without reason or ban-time
	 * field should use the config default (set::spamfilter::ban-reason/ban-time) but
	 * this isn't (or might not) be known yet when parsing spamfilter entries..
	 * so we do a VERY UGLY mass replace here.. unless someone else has a better idea.
	 */

	encoded = unreal_encodespace(SPAMFILTER_BAN_REASON);
	if (!encoded)
		abort(); /* hack to trace 'impossible' bug... */
	// FIXME: remove this stuff with ~server~, why not just use -config-
	//        which is more meaningful.
	for (tk = tklines[tkl_hash('q')]; tk; tk = tk->next)
	{
		if (tk->type != TKL_NAME)
			continue;
		if (!tk->set_by)
		{
			if (me.name[0] != '\0')
				safe_strdup(tk->set_by, me.name);
			else
				safe_strdup(tk->set_by, conf_me->name ? conf_me->name : "~server~");
		}
	}

	for (tk = tklines[tkl_hash('f')]; tk; tk = tk->next)
	{
		if (tk->type != TKL_SPAMF)
			continue; /* global entry or something else.. */
		if (!strcmp(tk->ptr.spamfilter->tkl_reason, "<internally added by ircd>"))
		{
			safe_strdup(tk->ptr.spamfilter->tkl_reason, encoded);
			tk->ptr.spamfilter->tkl_duration = SPAMFILTER_BAN_TIME;
		}
		/* This one is even more ugly, but our config crap is VERY confusing :[ */
		if (!tk->set_by)
		{
			if (me.name[0] != '\0')
				safe_strdup(tk->set_by, me.name);
			else
				safe_strdup(tk->set_by, conf_me->name ? conf_me->name : "~server~");
		}
	}

	if (!conf_log)
		make_default_logblock();
}

void postconf_fixes(void)
{
	/* If set::topic-setter is set to "nick-user-host" then the
	 * maximum topic length becomes shorter.
	 */
	if ((iConf.topic_setter == SETTER_NICK_USER_HOST) &&
	    (iConf.topic_length > 340))
	{
		config_warn("set::topic-length adjusted from %d to 340, which is the maximum because "
		            "set::topic-setter is set to 'nick-user-host'.", iConf.topic_length);
		iConf.topic_length = 340;
	}
}

/* Needed for set::options::allow-part-if-shunned,
 * we can't just make it CMD_SHUN and do a ALLOW_PART_IF_SHUNNED in
 * cmd_part itself because that will also block internal calls (like sapart). -- Syzop
 */
static void do_weird_shun_stuff()
{
RealCommand *cmptr;

	if ((cmptr = find_command_simple("PART")))
	{
		if (ALLOW_PART_IF_SHUNNED)
			cmptr->flags |= CMD_SHUN;
		else
			cmptr->flags &= ~CMD_SHUN;
	}
}

/** Various things that are done at the very end after the configuration file
 * has been read and almost all values have been set. This is to deal with
 * things like adding a default log { } block if there is none and that kind
 * of things.
 * This function is called by init_conf(), both on boot and on rehash.
 */
void postconf(void)
{
	postconf_defaults();
	postconf_fixes();
	do_weird_shun_stuff();
	isupport_init(); /* for all the 005 values that changed.. */
	tls_check_expiry(NULL);

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
	if (loop.ircd_rehashing)
		reinit_tls();
#endif
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
	else if (strcmp(me.name, conf_me->name))
	{
		config_warn("You changed the servername (me::name). "
		            "This change will NOT be effective unless you restart the IRC Server.");
	}

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
		config_error("We offer a configuration file converter to convert 3.2.x conf's to 4.x, however this "
		             "is not available when running as a service. If you want to use it, make UnrealIRCd "
		             "run in GUI mode by running 'unreal uninstall'. Then start UnrealIRCd.exe and when "
		             "it prompts you to convert the configuration click 'Yes'. Check if UnrealIRCd boots properly. "
		             "Once everything is looking good you can run 'unreal install' to make UnrealIRCd run "
		             "as a service again."); /* TODO: make this unnecessary :D */
	}
#else
	config_error("To upgrade it to the new 4.x format, run: ./unrealircd upgrade-conf");
#endif

	config_error("******************************************************************");
	/* TODO: win32 may require a different error */
}

/** Reset config tests (before running the config test) */
void config_test_reset(void)
{
}

/** Run config test and all post config tests. */
int config_test_all(void)
{
	if ((config_test() < 0) || (callbacks_check() < 0) || (efunctions_check() < 0) ||
	    reloadable_perm_module_unloaded() || !tls_tests())
	{
		return 0;
	}

	special_delayed_unloading();

	return 1;
}

/** Process all loadmodule directives in all includes.
 * This was previously done at the same time as 'include' was called but
 * that was too early now that we have blacklist-module, so moved here.
 * @retval 1 on success, 0 on any failed loadmodule directive.
 */
int config_loadmodules(void)
{
	ConfigFile *cfptr;
	ConfigEntry *ce;
	ConfigItem_blacklist_module *blm, *blm_next;

	int fatal_ret = 0, ret;

	for (cfptr = conf; cfptr; cfptr = cfptr->cf_next)
	{
		if (config_verbose > 1)
			config_status("Testing %s", cfptr->cf_filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
		{
			if (!strcmp(ce->ce_varname, "loadmodule"))
			{
				if (ce->ce_cond)
				{
					config_error("%s:%d: Currently you cannot have a 'loadmodule' statement "
						     "within an @if block, sorry.",
						     ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
					return 0;
				}
				ret = _conf_loadmodule(cfptr, ce);
				if (ret < fatal_ret)
					fatal_ret = ret; /* lowest wins */
			}
		}
	}

	/* Let's free the blacklist-module list here as well */
	for (blm = conf_blacklist_module; blm; blm = blm_next)
	{
		blm_next = blm->next;
		safe_free(blm->name);
		safe_free(blm);
	}
	conf_blacklist_module = NULL;
	/* End of freeing code */

	/* If any loadmodule returned a fatal (-1) error code then we return fail status (0) */
	if (fatal_ret < 0)
		return 0; /* FAIL */
	return 1; /* SUCCESS */
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
	memset(&tempiConf, 0, sizeof(iConf));
	memset(&settings, 0, sizeof(settings));
	memset(&requiredstuff, 0, sizeof(requiredstuff));
	memset(&nicklengths, 0, sizeof(nicklengths));
	config_setdefaultsettings(&tempiConf);
	clicap_pre_rehash();
	free_config_defines();
	/*
	 * the rootconf must be listed in the conf_include for include
	 * recursion prevention code and sanity checking code to be
	 * made happy :-). Think of it as us implicitly making an
	 * in-memory config file that looks like:
	 *
	 * include "unrealircd.conf";
	 */
	add_include(rootconf, "[thin air]", -1);
	if ((load_conf(rootconf, rootconf) > 0) && config_loadmodules())
	{
		preprocessor_resolve_conditionals_all(PREPROCESSOR_PHASE_MODULE);
		config_test_reset();
		if (!config_test_all())
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
		set_targmax_defaults();
		set_security_group_defaults();
		if (rehash)
		{
			Hook *h;
			safe_strdup(old_pid_file, conf_files->pid_file);
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
		if (config_run() < 0)
		{
			config_error("Bad case of config errors. Server will now die. This really shouldn't happen");
#ifdef _WIN32
			if (!rehash)
				win_error();
#endif
			abort();
		}
		applymeblock();
		if (old_pid_file && strcmp(old_pid_file, conf_files->pid_file))
		{
			sendto_ops("pidfile is being rewritten to %s, please delete %s",
				   conf_files->pid_file,
				   old_pid_file);
			write_pidfile();
		}
		safe_free(old_pid_file);
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
	postconf();
	config_status("Configuration loaded.");
	clicap_post_rehash();
	unload_all_unused_mtag_handlers();
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
	int counter;

	if (config_verbose > 0)
		config_status("Loading config file %s ..", filename);

	need_34_upgrade = 0;
	need_operclass_permissions_upgrade = 0;

	/*
	 * Check if we're accidentally including a file a second
	 * time. We should expect to find one entry in this list: the
	 * entry for our current file.
	 */
	counter = 0;
	my_inc = NULL;
	for (inc = conf_include; inc; inc = inc->next)
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
		if (!strcasecmp(filename, inc->file))
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

	if ((cfptr = config_load(filename, NULL)))
	{
		for (cfptr3 = &conf, cfptr2 = conf; cfptr2; cfptr2 = cfptr2->cf_next)
			cfptr3 = &cfptr2->cf_next;
		*cfptr3 = cfptr;

		if (config_verbose > 1)
			config_status("Loading module blacklist in %s", filename);

		preprocessor_resolve_conditionals_ce(&cfptr->cf_entries, PREPROCESSOR_PHASE_INITIAL);

		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
			if (!strcmp(ce->ce_varname, "blacklist-module"))
				 _test_blacklist_module(cfptr, ce);

		/* Load modules */
		if (config_verbose > 1)
			config_status("Loading modules in %s", filename);
		if (need_34_upgrade)
			upgrade_conf_to_34();

		/* Load includes */
		if (config_verbose > 1)
			config_status("Searching through %s for include files..", filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
			if (!strcmp(ce->ce_varname, "include"))
			{
				if (ce->ce_cond)
				{
					config_error("%s:%d: Currently you cannot have an 'include' statement "
					             "within an @if block, sorry. However, you CAN do it the other "
					             "way around, that is: put the @if within the included file itself.",
					             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
					return -1;
				}
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

/** Remove all TKL's that were added by the config file(s).
 * This is done after config passed testing and right before
 * adding the (new) entries.
 */
void remove_config_tkls(void)
{
	TKL *tk, *tk_next;
	int index, index2;

	/* IP hashed TKL list */
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tk = tklines_ip_hash[index][index2]; tk; tk = tk_next)
			{
				tk_next = tk->next;
				if (tk->flags & TKL_FLAG_CONFIG)
					tkl_del_line(tk);
			}
		}
	}

	/* Generic TKL list */
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tk = tklines[index]; tk; tk = tk_next)
		{
			tk_next = tk->next;
			if (tk->flags & TKL_FLAG_CONFIG)
				tkl_del_line(tk);
		}
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
	ConfigItem_deny_link		*deny_link_ptr;
	ConfigItem_deny_channel		*deny_channel_ptr;
	ConfigItem_allow_channel	*allow_channel_ptr;
	ConfigItem_admin		*admin_ptr;
	ConfigItem_deny_version		*deny_version_ptr;
	ConfigItem_log			*log_ptr;
	ConfigItem_alias		*alias_ptr;
	ConfigItem_help			*help_ptr;
	ConfigItem_offchans		*of_ptr;
	ConfigItem_sni			*sni;
	OperStat 			*os_ptr;
	ListStruct 	*next, *next2;
	SpamExcept *spamex_ptr;

	USE_BAN_VERSION = 0;

	for (admin_ptr = conf_admin; admin_ptr; admin_ptr = (ConfigItem_admin *)next)
	{
		next = (ListStruct *)admin_ptr->next;
		safe_free(admin_ptr->line);
		DelListItem(admin_ptr, conf_admin);
		safe_free(admin_ptr);
	}

	for (oper_ptr = conf_oper; oper_ptr; oper_ptr = (ConfigItem_oper *)next)
	{
		SWhois *s, *s_next;
		next = (ListStruct *)oper_ptr->next;
		safe_free(oper_ptr->name);
		safe_free(oper_ptr->snomask);
		safe_free(oper_ptr->operclass);
		safe_free(oper_ptr->vhost);
		Auth_FreeAuthConfig(oper_ptr->auth);
		unreal_delete_masks(oper_ptr->mask);
		DelListItem(oper_ptr, conf_oper);
		for (s = oper_ptr->swhois; s; s = s_next)
		{
			s_next = s->next;
			safe_free(s->line);
			safe_free(s->setby);
			safe_free(s);
		}
		safe_free(oper_ptr);
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
		safe_free(uline_ptr->servername);
		DelListItem(uline_ptr, conf_ulines);
		safe_free(uline_ptr);
	}
	for (allow_ptr = conf_allow; allow_ptr; allow_ptr = (ConfigItem_allow *) next)
	{
		next = (ListStruct *)allow_ptr->next;
		unreal_delete_masks(allow_ptr->mask);
		Auth_FreeAuthConfig(allow_ptr->auth);
		DelListItem(allow_ptr, conf_allow);
		safe_free(allow_ptr);
	}
	for (except_ptr = conf_except; except_ptr; except_ptr = (ConfigItem_except *) next)
	{
		next = (ListStruct *)except_ptr->next;
		safe_free(except_ptr->mask);
		DelListItem(except_ptr, conf_except);
		safe_free(except_ptr);
	}
	/* Free ban realname { }, ban server { } and ban version { } */
	for (ban_ptr = conf_ban; ban_ptr; ban_ptr = (ConfigItem_ban *) next)
	{
		next = (ListStruct *)ban_ptr->next;
		if (ban_ptr->flag.type2 == CONF_BAN_TYPE_CONF || ban_ptr->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
		{
			safe_free(ban_ptr->mask);
			safe_free(ban_ptr->reason);
			DelListItem(ban_ptr, conf_ban);
			safe_free(ban_ptr);
		}
	}
	for (listen_ptr = conf_listen; listen_ptr; listen_ptr = listen_ptr->next)
	{
		listen_ptr->flag.temporary = 1;
	}
	for (tld_ptr = conf_tld; tld_ptr; tld_ptr = (ConfigItem_tld *) next)
	{
		next = (ListStruct *)tld_ptr->next;
		safe_free(tld_ptr->motd_file);
		safe_free(tld_ptr->rules_file);
		safe_free(tld_ptr->smotd_file);
		safe_free(tld_ptr->opermotd_file);
		safe_free(tld_ptr->botmotd_file);

		free_motd(&tld_ptr->motd);
		free_motd(&tld_ptr->rules);
		free_motd(&tld_ptr->smotd);
		free_motd(&tld_ptr->opermotd);
		free_motd(&tld_ptr->botmotd);

		DelListItem(tld_ptr, conf_tld);
		safe_free(tld_ptr);
	}
	for (vhost_ptr = conf_vhost; vhost_ptr; vhost_ptr = (ConfigItem_vhost *) next)
	{
		SWhois *s, *s_next;

		next = (ListStruct *)vhost_ptr->next;

		safe_free(vhost_ptr->login);
		Auth_FreeAuthConfig(vhost_ptr->auth);
		safe_free(vhost_ptr->virthost);
		safe_free(vhost_ptr->virtuser);
		unreal_delete_masks(vhost_ptr->mask);
		for (s = vhost_ptr->swhois; s; s = s_next)
		{
			s_next = s->next;
			safe_free(s->line);
			safe_free(s->setby);
			safe_free(s);
		}
		DelListItem(vhost_ptr, conf_vhost);
		safe_free(vhost_ptr);
	}

	remove_config_tkls();

	for (deny_link_ptr = conf_deny_link; deny_link_ptr; deny_link_ptr = (ConfigItem_deny_link *) next) {
		next = (ListStruct *)deny_link_ptr->next;
		safe_free(deny_link_ptr->prettyrule);
		unreal_delete_masks(deny_link_ptr->mask);
		crule_free(&deny_link_ptr->rule);
		DelListItem(deny_link_ptr, conf_deny_link);
		safe_free(deny_link_ptr);
	}
	for (deny_version_ptr = conf_deny_version; deny_version_ptr; deny_version_ptr = (ConfigItem_deny_version *) next) {
		next = (ListStruct *)deny_version_ptr->next;
		safe_free(deny_version_ptr->mask);
		safe_free(deny_version_ptr->version);
		safe_free(deny_version_ptr->flags);
		DelListItem(deny_version_ptr, conf_deny_version);
		safe_free(deny_version_ptr);
	}
	for (deny_channel_ptr = conf_deny_channel; deny_channel_ptr; deny_channel_ptr = (ConfigItem_deny_channel *) next)
	{
		next = (ListStruct *)deny_channel_ptr->next;
		safe_free(deny_channel_ptr->redirect);
		safe_free(deny_channel_ptr->channel);
		safe_free(deny_channel_ptr->reason);
		safe_free(deny_channel_ptr->class);
		DelListItem(deny_channel_ptr, conf_deny_channel);
		unreal_delete_masks(deny_channel_ptr->mask);
		safe_free(deny_channel_ptr);
	}

	for (allow_channel_ptr = conf_allow_channel; allow_channel_ptr; allow_channel_ptr = (ConfigItem_allow_channel *) next)
	{
		next = (ListStruct *)allow_channel_ptr->next;
		safe_free(allow_channel_ptr->channel);
		safe_free(allow_channel_ptr->class);
		DelListItem(allow_channel_ptr, conf_allow_channel);
		unreal_delete_masks(allow_channel_ptr->mask);
		safe_free(allow_channel_ptr);
	}

	if (conf_drpass)
	{
		Auth_FreeAuthConfig(conf_drpass->restartauth);
		conf_drpass->restartauth = NULL;
		Auth_FreeAuthConfig(conf_drpass->dieauth);
		conf_drpass->dieauth = NULL;
		safe_free(conf_drpass);
	}
	for (log_ptr = conf_log; log_ptr; log_ptr = (ConfigItem_log *)next) {
		next = (ListStruct *)log_ptr->next;
		if (log_ptr->logfd != -1)
			fd_close(log_ptr->logfd);
		safe_free(log_ptr->file);
		DelListItem(log_ptr, conf_log);
		safe_free(log_ptr);
	}
	for (alias_ptr = conf_alias; alias_ptr; alias_ptr = (ConfigItem_alias *)next) {
		RealCommand *cmptr = find_command(alias_ptr->alias, 0);
		ConfigItem_alias_format *fmt;
		next = (ListStruct *)alias_ptr->next;
		safe_free(alias_ptr->nick);
		if (cmptr)
			CommandDelX(NULL, cmptr);
		safe_free(alias_ptr->alias);
		if (alias_ptr->format && (alias_ptr->type == ALIAS_COMMAND)) {
			for (fmt = (ConfigItem_alias_format *) alias_ptr->format; fmt; fmt = (ConfigItem_alias_format *) next2)
			{
				next2 = (ListStruct *)fmt->next;
				safe_free(fmt->format);
				safe_free(fmt->nick);
				safe_free(fmt->parameters);
				unreal_delete_match(fmt->expr);
				DelListItem(fmt, alias_ptr->format);
				safe_free(fmt);
			}
		}
		DelListItem(alias_ptr, conf_alias);
		safe_free(alias_ptr);
	}
	for (help_ptr = conf_help; help_ptr; help_ptr = (ConfigItem_help *)next) {
		MOTDLine *text;
		next = (ListStruct *)help_ptr->next;
		safe_free(help_ptr->command);
		while (help_ptr->text) {
			text = help_ptr->text->next;
			safe_free(help_ptr->text->line);
			safe_free(help_ptr->text);
			help_ptr->text = text;
		}
		DelListItem(help_ptr, conf_help);
		safe_free(help_ptr);
	}
	for (os_ptr = iConf.allow_user_stats_ext; os_ptr; os_ptr = (OperStat *)next)
	{
		next = (ListStruct *)os_ptr->next;
		safe_free(os_ptr->flag);
		safe_free(os_ptr);
	}
	iConf.allow_user_stats_ext = NULL;
	for (spamex_ptr = iConf.spamexcept; spamex_ptr; spamex_ptr = (SpamExcept *)next)
	{
		next = (ListStruct *)spamex_ptr->next;
		safe_free(spamex_ptr);
	}
	iConf.spamexcept = NULL;
	for (of_ptr = conf_offchans; of_ptr; of_ptr = (ConfigItem_offchans *)next)
	{
		next = (ListStruct *)of_ptr->next;
		safe_free(of_ptr->topic);
		safe_free(of_ptr);
	}
	conf_offchans = NULL;

	/* Free sni { } blocks */
	for (sni = conf_sni; sni; sni = (ConfigItem_sni *)next)
	{
	    next = (ListStruct *)sni->next;
	    SSL_CTX_free(sni->ssl_ctx);
	    free_tls_options(sni->tls_options);
	    safe_free(sni->name);
	    safe_free(sni);
	}
	conf_sni = NULL;

	free_conf_channelmodes(&iConf.modes_on_join);

	/*
	  reset conf_files -- should this be in its own function? no, because
	  it's only used here
	 */
	safe_free(conf_files->motd_file);
	safe_free(conf_files->smotd_file);
	safe_free(conf_files->opermotd_file);
	safe_free(conf_files->svsmotd_file);
	safe_free(conf_files->botmotd_file);
	safe_free(conf_files->rules_file);
	safe_free(conf_files->pid_file);
	safe_free(conf_files->tune_file);
	/*
	   Don't free conf_files->pid_file here; the old value is used to determine if
	   the pidfile location has changed and write_pidfile() needs to be called
	   again.
	*/
	safe_free(conf_files);
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
	if (!settings.has_default_server)
		Error("set::default-server is missing");
	if (!settings.has_network_name)
		Error("set::network-name is missing");
	if (!settings.has_help_channel)
		Error("set::help-channel is missing");
	if (nicklengths.min > nicklengths.max)
		Error("set::nick-length is smaller than set::min-nick-length");

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

	/* Stage 1: set block first */
	for (cfptr = conf; cfptr; cfptr = cfptr->cf_next)
	{
		if (config_verbose > 1)
			config_status("Running %s", cfptr->cf_filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
		{
			if (!strcmp(ce->ce_varname, "set"))
			{
				if (_conf_set(cfptr, ce) < 0)
					errors++;
			}
		}
	}

	/* Stage 2: now class blocks */
	for (cfptr = conf; cfptr; cfptr = cfptr->cf_next)
	{
		if (config_verbose > 1)
			config_status("Running %s", cfptr->cf_filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
		{
			if (!strcmp(ce->ce_varname, "class"))
			{
				if (_conf_class(cfptr, ce) < 0)
					errors++;
			}
		}
	}

	/* Stage 3: now all the rest */
	for (cfptr = conf; cfptr; cfptr = cfptr->cf_next)
	{
		if (config_verbose > 1)
			config_status("Running %s", cfptr->cf_filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
		{
			/* These are already processed above (set, class)
			 * or via config_test() (secret).
			 */
			if (!strcmp(ce->ce_varname, "set") ||
			    !strcmp(ce->ce_varname, "class") ||
			    !strcmp(ce->ce_varname, "secret"))
			{
				continue;
			}

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
	for(allow = conf_allow; allow; allow = allow->next)
		if(!allow->ipv6_clone_mask)
			allow->ipv6_clone_mask = tempiConf.default_ipv6_clone_mask;

	/* ^^^ TODO: due to the two-stage model now we can do it in conf_allow again
	 *     and remove it here.
	 */

	close_unbound_listeners();
	listen_cleanup();
	close_unbound_listeners();
	loop.do_bancheck = 1;
	free_iConf(&iConf);
	memcpy(&iConf, &tempiConf, sizeof(iConf));
	memset(&tempiConf, 0, sizeof(tempiConf));
	update_throttling_timer_settings();

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
		/* First test and run the secret { } blocks */
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
		{
			if (!strcmp(ce->ce_varname, "secret"))
			{
				int n = _test_secret(cfptr, ce);
				errors += n;
				if (n == 0)
					_conf_secret(cfptr, ce);
			}
		}
		/* First test the set { } block */
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
		{
			if (!strcmp(ce->ce_varname, "set"))
				errors += _test_set(cfptr, ce);
		}
		/* Now test all the rest */
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
		{
			/* These are already processed, so skip them here.. */
			if (!strcmp(ce->ce_varname, "secret") ||
			    !strcmp(ce->ce_varname, "set"))
			{
				continue;
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
				{
					config_error("%s:%i: unknown directive %s",
						ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
						ce->ce_varname);
					errors++;
					if (strchr(ce->ce_varname, ':'))
					{
						config_error("You cannot use :: in a directive, you have to write them out. "
						             "For example 'set::auto-join #something' needs to be written as: "
						             "set { auto-join \"#something\"; }");
						config_error("See also https://www.unrealircd.org/docs/Set_block#Syntax_used_in_this_documentation");
					}
				}
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

ConfigItem_alias *find_alias(char *name)
{
	ConfigItem_alias *e;

	if (!name)
		return NULL;

	for (e = conf_alias; e; e = e->next)
	{
		if (!strcasecmp(e->alias, name))
			return e;
	}
	return NULL;
}

ConfigItem_class *find_class(char *name)
{
	ConfigItem_class *e;

	if (!name)
		return NULL;

	for (e = conf_class; e; e = e->next)
	{
		if (!strcmp(name, e->name))
			return e;
	}
	return NULL;
}


ConfigItem_oper	*find_oper(char *name)
{
	ConfigItem_oper	*e;

	if (!name)
		return NULL;

	for (e = conf_oper; e; e = e->next)
	{
		if (!strcmp(name, e->name))
			return e;
	}
	return NULL;
}

ConfigItem_operclass *find_operclass(char *name)
{
	ConfigItem_operclass *e;

	if (!name)
		return NULL;

	for (e = conf_operclass; e; e = e->next)
	{
		if (!strcmp(name,e->classStruct->name))
			return e;
	}
	return NULL;
}

int count_oper_sessions(char *name)
{
	int count = 0;
	Client *client;

	list_for_each_entry(client, &oper_list, special_node)
	{
		if (client->user->operlogin != NULL && !strcmp(client->user->operlogin, name))
			count++;
	}

	return count;
}

ConfigItem_listen *find_listen(char *ipmask, int port, int ipv6)
{
	ConfigItem_listen *e;

	if (!ipmask)
		return NULL;

	for (e = conf_listen; e; e = e->next)
		if ((e->ipv6 == ipv6) && (e->port == port) && !strcmp(e->ip, ipmask))
			return e;

	return NULL;
}

/** Find an SNI match.
 * @param name The hostname to look for (eg: irc.xyz.com).
 */
ConfigItem_sni *find_sni(char *name)
{
	ConfigItem_sni *e;

	if (!name)
		return NULL;

	for (e = conf_sni; e; e = e->next)
	{
        if (match_simple(e->name, name))
            return e;
	}
	return NULL;
}

ConfigItem_ulines *find_uline(char *host)
{
	ConfigItem_ulines *ulines;

	if (!host)
		return NULL;

	for(ulines = conf_ulines; ulines; ulines = ulines->next)
	{
		if (!strcasecmp(host, ulines->servername))
			return ulines;
	}
	return NULL;
}


ConfigItem_except *find_except(Client *client, short type)
{
	ConfigItem_except *excepts;

	for(excepts = conf_except; excepts; excepts = excepts->next)
	{
		if (excepts->flag.type == type)
		{
			if (match_user(excepts->mask, client, MATCH_CHECK_REAL))
				return excepts;
		}
	}
	return NULL;
}

ConfigItem_tld *find_tld(Client *client)
{
	ConfigItem_tld *tld;

	for (tld = conf_tld; tld; tld = tld->next)
	{
		if (match_user(tld->mask, client, MATCH_CHECK_REAL))
		{
			if ((tld->options & TLD_TLS) && !IsSecureConnect(client))
				continue;
			if ((tld->options & TLD_REMOTE) && MyUser(client))
				continue;
			return tld;
		}
	}

	return NULL;
}


ConfigItem_link *find_link(char *servername, Client *client)
{
	ConfigItem_link	*link;

	for (link = conf_link; link; link = link->next)
	{
		if (match_simple(link->servername, servername) && unreal_mask_match(client, link->incoming.mask))
		{
		    return link;
		}
	}
	return NULL;
}

/** Find a ban of type CONF_BAN_*, which is currently only
 * CONF_BAN_SERVER, CONF_BAN_VERSION and CONF_BAN_REALNAME
 */
ConfigItem_ban *find_ban(Client *client, char *host, short type)
{
	ConfigItem_ban *ban;

	for (ban = conf_ban; ban; ban = ban->next)
	{
		if (ban->flag.type == type)
		{
			if (client)
			{
				if (match_user(ban->mask, client, MATCH_CHECK_REAL))
					return ban;
			}
			else if (match_simple(ban->mask, host))
				return ban;
		}
	}
	return NULL;
}

/** Find a ban of type CONF_BAN_*, which is currently only
 * CONF_BAN_SERVER, CONF_BAN_VERSION and CONF_BAN_REALNAME
 * This is the extended version, only used by cmd_svsnline.
 */
ConfigItem_ban 	*find_banEx(Client *client, char *host, short type, short type2)
{
	ConfigItem_ban *ban;

	for (ban = conf_ban; ban; ban = ban->next)
	{
		if ((ban->flag.type == type) && (ban->flag.type2 == type2))
		{
			if (client)
			{
				if (match_user(ban->mask, client, MATCH_CHECK_REAL))
					return ban;
			}
			else if (match_simple(ban->mask, host))
				return ban;
		}
	}
	return NULL;
}

ConfigItem_vhost *find_vhost(char *name)
{
	ConfigItem_vhost *vhost;

	for (vhost = conf_vhost; vhost; vhost = vhost->next)
	{
		if (!strcmp(name, vhost->login))
			return vhost;
	}

	return NULL;
}


/** returns NULL if allowed and struct if denied */
ConfigItem_deny_channel *find_channel_allowed(Client *client, char *name)
{
	ConfigItem_deny_channel *dchannel;
	ConfigItem_allow_channel *achannel;

	for (dchannel = conf_deny_channel; dchannel; dchannel = dchannel->next)
	{
		if (match_simple(dchannel->channel, name))
		{
			if (dchannel->class && strcmp(client->local->class->name, dchannel->class))
				continue;
			if (dchannel->mask && !unreal_mask_match(client, dchannel->mask))
				continue;
			break; /* MATCH deny channel { } */
		}
	}

	if (dchannel)
	{
		/* Check exceptions... ('allow channel') */
		for (achannel = conf_allow_channel; achannel; achannel = achannel->next)
		{
			if (match_simple(achannel->channel, name))
			{
				if (achannel->class && strcmp(client->local->class->name, achannel->class))
					continue;
				if (achannel->mask && !unreal_mask_match(client, achannel->mask))
					continue;
				break; /* MATCH allow channel { } */
			}
		}
		if (achannel)
			return NULL; /* Matches an 'allow channel' - so not forbidden */
		else
			return dchannel;
	}
	return NULL;
}

void init_dynconf(void)
{
	memset(&iConf, 0, sizeof(iConf));
	memset(&tempiConf, 0, sizeof(iConf));
}

char *pretty_time_val(long timeval)
{
	static char buf[512];

	if (timeval == 0)
		return "0";

	buf[0] = 0;

	if (timeval/86400)
		snprintf(buf, sizeof(buf), "%ldd", timeval/86400);
	if ((timeval/3600) % 24)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%ldh", (timeval/3600)%24);
	if ((timeval/60)%60)
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%ldm", (timeval/60)%60);
	if ((timeval%60))
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%lds", timeval%60);

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

	s = safe_alloc(strlen(reldir) + strlen(*path) + 2);
	sprintf(s, "%s/%s", reldir, *path); /* safe, see line above */
	safe_free(*path);
	*path = s;
}

/* Similar to convert_to_absolute_path() but returns a duplicated string.
 * Don't forget to free!
 */
char *convert_to_absolute_path_duplicate(char *path, char *reldir)
{
	char *xpath = strdup(path);
	convert_to_absolute_path(&xpath, reldir);
	return xpath;
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
#else
	if (strstr(ce->ce_vardata, "://"))
	{
		config_error("%s:%d: URL specified: %s",
		             ce->ce_fileptr->cf_filename,
		             ce->ce_varlinenum,
		             ce->ce_vardata);
		config_error("UnrealIRCd was not compiled with remote includes support "
		             "so you cannot use URLs. You are suggested to re-run ./Config "
		             "and answer YES to the question about remote includes.");
		return -1;
	}
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
	memset(cPath, 0, MAX_PATH);
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
		path = safe_alloc(strlen(cPath) + strlen(FindData.cFileName)+1);
		strcpy(path, cPath);
		strcat(path, FindData.cFileName);

		add_include(path, ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		ret = load_conf(path, path);
		safe_free(path);

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
			path = safe_alloc(strlen(cPath) + strlen(FindData.cFileName)+1);
			strcpy(path,cPath);
			strcat(path,FindData.cFileName);

			add_include(path, ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			ret = load_conf(path, path);
			safe_free(path);
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
		ca = safe_alloc(sizeof(ConfigItem_admin));
		if (!conf_admin)
			conf_admin_tail = ca;
		safe_strdup(ca->line, cep->ce_varname);
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
		if (strlen(cep->ce_varname) > 500)
		{
			config_error("%s:%i: oversized data in admin block",
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
		conf_me = safe_alloc(sizeof(ConfigItem_me));

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "name"))
		{
			safe_strdup(conf_me->name, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "info"))
		{
			safe_strdup(conf_me->info, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "sid"))
		{
			safe_strdup(conf_me->sid, cep->ce_vardata);
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

			if (!valid_sid(cep->ce_vardata))
			{
				config_error("%s:%i: me::sid must be 3 characters long, begin with a number, "
				             "and the 2nd and 3rd character must be a number or uppercase letter. "
				             "Example: \"001\" and \"0AB\" is good. \"AAA\" and \"0ab\" are bad. ",
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
		conf_files = safe_alloc(sizeof(ConfigItem_files));

		/* set defaults */
		safe_strdup(conf_files->motd_file, MPATH);
		safe_strdup(conf_files->rules_file, RPATH);
		safe_strdup(conf_files->smotd_file, SMPATH);
		safe_strdup(conf_files->botmotd_file, BPATH);
		safe_strdup(conf_files->opermotd_file, OPATH);
		safe_strdup(conf_files->svsmotd_file, VPATH);

		safe_strdup(conf_files->pid_file, IRCD_PIDFILE);
		safe_strdup(conf_files->tune_file, IRCDTUNE);

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
			safe_strdup(conf_files->motd_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "shortmotd"))
			safe_strdup(conf_files->smotd_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "opermotd"))
			safe_strdup(conf_files->opermotd_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "svsmotd"))
			safe_strdup(conf_files->svsmotd_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "botmotd"))
			safe_strdup(conf_files->botmotd_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "rules"))
			safe_strdup(conf_files->rules_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "tunefile"))
			safe_strdup(conf_files->tune_file, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "pidfile"))
			safe_strdup(conf_files->pid_file, cep->ce_vardata);
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
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
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
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
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
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
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
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
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
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
			config_test_openfile(cep, O_RDONLY, 0, "files::opermotd", 0, 1);
			has_opermotd = 1;
		}
		/* files::svsmotd
		 * This config stuff should somehow be inside of modules/svsmotd.c!!!... right?
		 */
		else if (!strcmp(cep->ce_varname, "svsmotd"))
		{
			if (has_svsmotd)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "files::svsmotd");
				continue;
			}
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
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
			convert_to_absolute_path(&cep->ce_vardata, PERMDATADIR);
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
			convert_to_absolute_path(&cep->ce_vardata, PERMDATADIR);
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
	entry = safe_alloc(sizeof(OperClassACLEntry));

	if (!strcmp(ce->ce_varname,"allow"))
		entry->type = OPERCLASSENTRY_ALLOW;
	else
		entry->type = OPERCLASSENTRY_DENY;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		OperClassACLEntryVar *var = safe_alloc(sizeof(OperClassACLEntryVar));
		safe_strdup(var->name, cep->ce_varname);
		if (cep->ce_vardata)
		{
			safe_strdup(var->value, cep->ce_vardata);
		}
		AddListItem(var,entry->variables);
	}

	return entry;
}

OperClassACL* _conf_parseACL(char *name, ConfigEntry *ce)
{
	ConfigEntry *cep;
	OperClassACL *acl = NULL;

	acl = safe_alloc(sizeof(OperClassACL));
	safe_strdup(acl->name, name);

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
	operClass = safe_alloc(sizeof(ConfigItem_operclass));
	operClass->classStruct = safe_alloc(sizeof(OperClass));
	safe_strdup(operClass->classStruct->name, ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "parent"))
		{
			safe_strdup(operClass->classStruct->ISA, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "permissions"))
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

void new_permissions_system(ConfigFile *conf, ConfigEntry *ce)
{
	if (need_operclass_permissions_upgrade)
		return; /* error already shown */

	config_error("%s:%i: UnrealIRCd 4.2.1 and higher have a new operclass permissions system.",
	             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
	config_error("Please see https://www.unrealircd.org/docs/FAQ#New_operclass_permissions");
	config_error("(additional errors regarding this are suppressed)");
	/*
	config_error("First of all, operclass::privileges has been renamed to operclass::permissions.");
	config_error("However, the permissions themselves have also been changed. You cannot simply "
	             "rename 'privileges' to 'permissions' and be done with it! ");
	config_error("See https://www.unrealircd.org/docs/Operclass_permissions for the new list of permissions.");
	config_error("Or just use the default operclasses from operclass.default.conf, then no need to change anything."); */
	need_operclass_permissions_upgrade = 1;
}

int 	_test_operclass(ConfigFile *conf, ConfigEntry *ce)
{
	char has_permissions = 0, has_parent = 0;
	ConfigEntry *cep;
	int	errors = 0;

	if (!ce->ce_vardata)
	{
		config_error_noname(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "operclass");
		errors++;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
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
		} else
		if (!strcmp(cep->ce_varname, "permissions"))
		{
			if (has_permissions)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, "oper::permissions");
				continue;
			}
			has_permissions = 1;
			continue;
		} else
		if (!strcmp(cep->ce_varname, "privileges"))
		{
			new_permissions_system(conf, cep);
			errors++;
			return errors;
		} else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, "operclass", cep->ce_varname);
			errors++;
			continue;
		}
	}

	if (!has_permissions)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"oper::permissions");
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

	oper =  safe_alloc(sizeof(ConfigItem_oper));
	safe_strdup(oper->name, ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "operclass"))
			safe_strdup(oper->operclass, cep->ce_vardata);
		if (!strcmp(cep->ce_varname, "password"))
			oper->auth = AuthBlockToAuthConfig(cep);
		else if (!strcmp(cep->ce_varname, "class"))
		{
			oper->class = find_class(cep->ce_vardata);
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
					s = safe_alloc(sizeof(SWhois));
					safe_strdup(s->line, cepp->ce_varname);
					safe_strdup(s->setby, "oper");
					AddListItem(s, oper->swhois);
				}
			} else
			if (cep->ce_vardata)
			{
				s = safe_alloc(sizeof(SWhois));
				safe_strdup(s->line, cep->ce_vardata);
				safe_strdup(s->setby, "oper");
				AddListItem(s, oper->swhois);
			}
		}
		else if (!strcmp(cep->ce_varname, "snomask"))
		{
			safe_strdup(oper->snomask, cep->ce_vardata);
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
			safe_strdup(oper->vhost, cep->ce_vardata);
		}
	}
	AddListItem(oper, conf_oper);
	return 1;
}

int	_test_oper(ConfigFile *conf, ConfigEntry *ce)
{
	char has_class = 0, has_password = 0, has_snomask = 0;
	char has_modes = 0, has_require_modes = 0, has_mask = 0, has_maxlogins = 0;
	char has_operclass = 0, has_vhost = 0;
	ConfigEntry *cep;
	int errors = 0;

	if (!ce->ce_vardata)
	{
		config_error_noname(ce->ce_fileptr->cf_filename, ce->ce_varlinenum, "oper");
		errors++;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
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
					if (strchr("orzS", *p))
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
					if (strchr("o", *p))
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

	if (!(class = find_class(ce->ce_vardata)))
	{
		class = safe_alloc(sizeof(ConfigItem_class));
		safe_strdup(class->name, ce->ce_vardata);
		isnew = 1;
	}
	else
	{
		isnew = 0;
		class->flag.temporary = 0;
		class->options = 0; /* RESET OPTIONS */
	}
	safe_strdup(class->name, ce->ce_vardata);

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
		conf_drpass =  safe_alloc(sizeof(ConfigItem_drpass));
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "restart"))
		{
			if (conf_drpass->restartauth)
				Auth_FreeAuthConfig(conf_drpass->restartauth);

			conf_drpass->restartauth = AuthBlockToAuthConfig(cep);
		}
		else if (!strcmp(cep->ce_varname, "die"))
		{
			if (conf_drpass->dieauth)
				Auth_FreeAuthConfig(conf_drpass->dieauth);

			conf_drpass->dieauth = AuthBlockToAuthConfig(cep);
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
		ca = safe_alloc(sizeof(ConfigItem_ulines));
		safe_strdup(ca->servername, cep->ce_varname);
		AddListItem(ca, conf_ulines);
	}
	return 1;
}

int	_test_ulines(ConfigFile *conf, ConfigEntry *ce)
{
	/* No check needed */
	return 0;
}

int     _conf_tld(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_tld *ca;

	ca = safe_alloc(sizeof(ConfigItem_tld));

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask"))
			safe_strdup(ca->mask, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "motd"))
		{
			safe_strdup(ca->motd_file, cep->ce_vardata);
			read_motd(cep->ce_vardata, &ca->motd);
		}
		else if (!strcmp(cep->ce_varname, "shortmotd"))
		{
			safe_strdup(ca->smotd_file, cep->ce_vardata);
			read_motd(cep->ce_vardata, &ca->smotd);
		}
		else if (!strcmp(cep->ce_varname, "opermotd"))
		{
			safe_strdup(ca->opermotd_file, cep->ce_vardata);
			read_motd(cep->ce_vardata, &ca->opermotd);
		}
		else if (!strcmp(cep->ce_varname, "botmotd"))
		{
			safe_strdup(ca->botmotd_file, cep->ce_vardata);
			read_motd(cep->ce_vardata, &ca->botmotd);
		}
		else if (!strcmp(cep->ce_varname, "rules"))
		{
			safe_strdup(ca->rules_file, cep->ce_vardata);
			read_motd(cep->ce_vardata, &ca->rules);
		}
		else if (!strcmp(cep->ce_varname, "options"))
		{
			ConfigEntry *cepp;
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "ssl") || !strcmp(cepp->ce_varname, "tls"))
					ca->options |= TLD_TLS;
				else if (!strcmp(cepp->ce_varname, "remote"))
					ca->options |= TLD_REMOTE;
			}
		}
		else if (!strcmp(cep->ce_varname, "channel"))
			safe_strdup(ca->channel, cep->ce_vardata);
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
				if (strcmp(cep2->ce_varname, "ssl") &&
				    strcmp(cep2->ce_varname, "tls") &&
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
	ConfigEntry *tlsconfig = NULL;
	ConfigItem_listen *listen = NULL;
	char *ip = NULL;
	int start=0, end=0, port, isnew;
	int tmpflags =0;
	Hook *h;

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
				{
					tmpflags |= ofp->flag;
				} else {
					for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
					{
						int value = (*(h->func.intfunc))(conf, cepp, CONFIG_LISTEN_OPTIONS);
						if (value == 1)
							break;
					}
				}
			}
		} else
		if (!strcmp(cep->ce_varname, "ssl-options") || !strcmp(cep->ce_varname, "tls-options"))
		{
			tlsconfig = cep;
		} else
		{
			for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
			{
				int value = (*(h->func.intfunc))(conf, cep, CONFIG_LISTEN);
				if (value == 1)
					break;
			}
		}
	}
	for (port = start; port <= end; port++)
	{
		/* First deal with IPv4 */
		if (!strchr(ip, ':'))
		{
			if (!(listen = find_listen(ip, port, 0)))
			{
				listen = safe_alloc(sizeof(ConfigItem_listen));
				safe_strdup(listen->ip, ip);
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

			if (listen->ssl_ctx)
			{
				SSL_CTX_free(listen->ssl_ctx);
				listen->ssl_ctx = NULL;
			}

			if (listen->tls_options)
			{
				free_tls_options(listen->tls_options);
				listen->tls_options = NULL;
			}

			if (tlsconfig)
			{
				listen->tls_options = safe_alloc(sizeof(TLSOptions));
				conf_tlsblock(conf, tlsconfig, listen->tls_options);
				listen->ssl_ctx = init_ctx(listen->tls_options, 1);
			}

			/* For modules that hook CONFIG_LISTEN and CONFIG_LISTEN_OPTIONS.
			 * Yeah, ugly we have this here..
			 * and again about 100 lines down too.
			 */
			for (cep = ce->ce_entries; cep; cep = cep->ce_next)
			{
				if (!strcmp(cep->ce_varname, "ip"))
					;
				else if (!strcmp(cep->ce_varname, "port"))
					;
				else if (!strcmp(cep->ce_varname, "options"))
				{
					for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
					{
						NameValue *ofp;
						if (!config_binary_flags_search(_ListenerFlags, cepp->ce_varname, ARRAY_SIZEOF(_ListenerFlags)))
						{
							for (h = Hooks[HOOKTYPE_CONFIGRUN_EX]; h; h = h->next)
							{
								int value = (*(h->func.intfunc))(conf, cepp, CONFIG_LISTEN_OPTIONS, listen);
								if (value == 1)
									break;
							}
						}
					}
				} else
				if (!strcmp(cep->ce_varname, "ssl-options") || !strcmp(cep->ce_varname, "tls-options"))
					;
				else
				{
					for (h = Hooks[HOOKTYPE_CONFIGRUN_EX]; h; h = h->next)
					{
						int value = (*(h->func.intfunc))(conf, cep, CONFIG_LISTEN, listen);
						if (value == 1)
							break;
					}
				}
			}
		}

		/* Then deal with IPv6 (if available/enabled) */
		if (!DISABLE_IPV6)
		{
			if (strchr(ip, ':') || (*ip == '*'))
			{
				if (!(listen = find_listen(ip, port, 1)))
				{
					listen = safe_alloc(sizeof(ConfigItem_listen));
					safe_strdup(listen->ip, ip);
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

				if (listen->ssl_ctx)
				{
					SSL_CTX_free(listen->ssl_ctx);
					listen->ssl_ctx = NULL;
				}

				if (listen->tls_options)
				{
					free_tls_options(listen->tls_options);
					listen->tls_options = NULL;
				}

				if (tlsconfig)
				{
					listen->tls_options = safe_alloc(sizeof(TLSOptions));
					conf_tlsblock(conf, tlsconfig, listen->tls_options);
					listen->ssl_ctx = init_ctx(listen->tls_options, 1);
				}
				/* For modules that hook CONFIG_LISTEN and CONFIG_LISTEN_OPTIONS.
				 * Yeah, ugly we have this here..
				 */
				for (cep = ce->ce_entries; cep; cep = cep->ce_next)
				{
					if (!strcmp(cep->ce_varname, "ip"))
						;
					else if (!strcmp(cep->ce_varname, "port"))
						;
					else if (!strcmp(cep->ce_varname, "options"))
					{
						for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
						{
							NameValue *ofp;
							if (!config_binary_flags_search(_ListenerFlags, cepp->ce_varname, ARRAY_SIZEOF(_ListenerFlags)))
							{
								for (h = Hooks[HOOKTYPE_CONFIGRUN_EX]; h; h = h->next)
								{
									int value = (*(h->func.intfunc))(conf, cepp, CONFIG_LISTEN_OPTIONS, listen);
									if (value == 1)
										break;
								}
							}
						}
					} else
					if (!strcmp(cep->ce_varname, "ssl-options") || !strcmp(cep->ce_varname, "tls-options"))
						;
					else
					{
						for (h = Hooks[HOOKTYPE_CONFIGRUN_EX]; h; h = h->next)
						{
							int value = (*(h->func.intfunc))(conf, cep, CONFIG_LISTEN, listen);
							if (value == 1)
								break;
						}
					}
				}
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
	char has_ip = 0, has_port = 0, has_options = 0, port_6667 = 0;
	char *ip = NULL;
	Hook *h;

	if (ce->ce_vardata)
	{
		config_error("%s:%i: listen block has a new syntax, see https://www.unrealircd.org/docs/Listen_block",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);

		need_34_upgrade = 1;
		return 1;
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		int used_by_module = 0;

		/* First, check if a module knows about this listen::something */
		for (h = Hooks[HOOKTYPE_CONFIGTEST]; h; h = h->next)
		{
			int value, errs = 0;
			if (h->owner && !(h->owner->flags & MODFLAG_TESTING)
			    && !(h->owner->options & MOD_OPT_PERM))
			{
				continue;
			}
			value = (*(h->func.intfunc))(conf, cep, CONFIG_LISTEN, &errs);
			if (value == 2)
				used_by_module = 1;
			if (value == 1)
			{
				used_by_module = 1;
				break;
			}
			if (value == -1)
			{
				used_by_module = 1;
				errors += errs;
				break;
			}
			if (value == -2)
			{
				used_by_module = 1;
				errors += errs;
			}
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
				if (!(ofp = config_binary_flags_search(_ListenerFlags, cepp->ce_varname, ARRAY_SIZEOF(_ListenerFlags))))
				{
					/* Check if a module knows about this listen::options::something */
					int used_by_module = 0;
					for (h = Hooks[HOOKTYPE_CONFIGTEST]; h; h = h->next)
					{
						int value, errs = 0;
						if (h->owner && !(h->owner->flags & MODFLAG_TESTING)
						    && !(h->owner->options & MOD_OPT_PERM))
						{
							continue;
						}
						value = (*(h->func.intfunc))(conf, cepp, CONFIG_LISTEN_OPTIONS, &errs);
						if (value == 2)
							used_by_module = 1;
						if (value == 1)
						{
							used_by_module = 1;
							break;
						}
						if (value == -1)
						{
							used_by_module = 1;
							errors += errs;
							break;
						}
						if (value == -2)
						{
							used_by_module = 1;
							errors += errs;
						}
					}
					if (!used_by_module)
					{
						config_error_unknownopt(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, "listen::options", cepp->ce_varname);
						errors++;
						continue;
					}
				}
				if (!strcmp(cepp->ce_varname, "ssl") || !strcmp(cepp->ce_varname, "tls"))
					have_tls_listeners = 1; /* for ssl config test */
			}
		}
		else
		if (!strcmp(cep->ce_varname, "ssl-options") || !strcmp(cep->ce_varname, "tls-options"))
		{
			test_tlsblock(conf, cep, &errors);
		}
		else
		if (!cep->ce_vardata)
		{
			if (!used_by_module)
			{
				config_error_empty(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "listen", cep->ce_varname);
				errors++;
			}
			continue; /* always */
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
			ip = cep->ce_vardata;
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
				if ((start < 1) || (start > 65535))
				{
					config_error("%s:%i: listen: illegal port (must be 1..65535)",
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
				if ((start < 1) || (start > 65535) || (end < 1) || (end > 65535))
				{
					config_error("%s:%i: listen: illegal port range values must be between 1 and 65535",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					return 1;
				}
			}

			if ((6667 >= start) && (6667 <= end))
				port_6667 = 1;
		} else
		{
			if (!used_by_module)
			{
				config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					"listen", cep->ce_varname);
				errors++;
			}
			continue; /* always */
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

	if (port_6667)
		safe_strdup(port_6667_ip, ip);

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
	allow = safe_alloc(sizeof(ConfigItem_allow));

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask") || !strcmp(cep->ce_varname, "ip") || !strcmp(cep->ce_varname, "hostname"))
		{
			unreal_add_masks(&allow->mask, cep);
		}
		else if (!strcmp(cep->ce_varname, "password"))
			allow->auth = AuthBlockToAuthConfig(cep);
		else if (!strcmp(cep->ce_varname, "class"))
		{
			allow->class = find_class(cep->ce_vardata);
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
		else if (!strcmp(cep->ce_varname, "global-maxperip"))
			allow->global_maxperip = atoi(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "redirect-server"))
			safe_strdup(allow->server, cep->ce_vardata);
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
				else if (!strcmp(cepp->ce_varname, "ssl") || !strcmp(cepp->ce_varname, "tls"))
					allow->flags.tls = 1;
				else if (!strcmp(cepp->ce_varname, "reject-on-auth-failure"))
					allow->flags.reject_on_auth_failure = 1;
			}
		}
	}

	/* Default: global-maxperip = maxperip+1 */
	if (allow->global_maxperip == 0)
		allow->global_maxperip = allow->maxperip+1;

	/* global-maxperip < maxperip makes no sense */
	if (allow->global_maxperip < allow->maxperip)
		allow->global_maxperip = allow->maxperip;

	AddListItem(allow, conf_allow);
	return 1;
}

int	_test_allow(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	int		errors = 0;
	Hook *h;
	char has_ip = 0, has_hostname = 0, has_mask = 0;
	char has_maxperip = 0, has_global_maxperip = 0, has_password = 0, has_class = 0;
	char has_redirectserver = 0, has_redirectport = 0, has_options = 0;
	int hostname_possible_silliness = 0;

	if (ce->ce_vardata)
	{
		if (!strcmp(ce->ce_vardata, "channel"))
			return (_test_allow_channel(conf, ce));
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
		if (strcmp(cep->ce_varname, "options") &&
		    strcmp(cep->ce_varname, "mask") &&
		    config_is_blankorempty(cep, "allow"))
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
		else if (!strcmp(cep->ce_varname, "mask"))
		{
			has_mask = 1;
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
			if ((v <= 0) || (v > 1000000))
			{
				config_error("%s:%i: allow::maxperip with illegal value (must be 1-1000000)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "global-maxperip"))
		{
			int v = atoi(cep->ce_vardata);
			if (has_global_maxperip)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "allow::global-maxperip");
				continue;
			}
			has_global_maxperip = 1;
			if ((v <= 0) || (v > 1000000))
			{
				config_error("%s:%i: allow::global-maxperip with illegal value (must be 1-1000000)",
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
				else if (!strcmp(cepp->ce_varname, "ssl") || !strcmp(cepp->ce_varname, "tls"))
				{}
				else if (!strcmp(cepp->ce_varname, "reject-on-auth-failure"))
				{}
				else if (!strcmp(cepp->ce_varname, "sasl"))
				{
					config_error("%s:%d: The option allow::options::sasl no longer exists. "
					             "Please use a require authentication { } block instead, which "
					             "is more flexible and provides the same functionality. See "
					             "https://www.unrealircd.org/docs/Require_authentication_block",
					             cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
					errors++;
				}
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

	if (has_mask && (has_ip || has_hostname))
	{
		config_error("%s:%d: The allow block uses allow::mask, but you also have an allow::ip and allow::hostname.",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		config_error("Please delete your allow::ip and allow::hostname entries and/or integrate them into allow::mask");
	} else
	if (has_ip)
	{
		config_warn("%s:%d: The allow block uses allow::mask nowadays. Rename your allow::ip item to allow::mask.",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		config_warn("See https://www.unrealircd.org/docs/FAQ#allow-mask for more information");
	} else
	if (has_hostname)
	{
		config_warn("%s:%d: The allow block uses allow::mask nowadays. Rename your allow::hostname item to allow::mask.",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		config_warn("See https://www.unrealircd.org/docs/FAQ#allow-mask for more information");
	} else
	if (!has_mask)
	{
		config_error("%s:%d: allow block needs an allow::mask",
				 ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}

	if (has_ip && has_hostname)
	{
		config_error("%s:%d: allow block has both allow::ip and allow::hostname, this is no longer permitted.",
		             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		config_error("Please integrate your allow::ip and allow::hostname items into a single allow::mask block");
		need_34_upgrade = 1;
		errors++;
	} else
	if (hostname_possible_silliness)
	{
		config_error("%s:%d: allow block contains 'hostname *;'. This means means that users "
		             "without a valid hostname (unresolved IP's) will be unable to connect. "
		             "You most likely want to use 'mask *;' instead.",
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
	ConfigEntry *mask = NULL;

	/* First, search for ::class, if any */
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "class"))
			class = cep->ce_vardata;
		else if (!strcmp(cep->ce_varname, "mask"))
			mask = cep;
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "channel"))
		{
			/* This way, we permit multiple ::channel items in one allow block */
			allow = safe_alloc(sizeof(ConfigItem_allow_channel));
			safe_strdup(allow->channel, cep->ce_vardata);
			if (class)
				safe_strdup(allow->class, class);
			if (mask)
				unreal_add_masks(&allow->mask, mask);
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
		else if (!strcmp(cep->ce_varname, "mask"))
		{
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

int _conf_except(ConfigFile *conf, ConfigEntry *ce)
{
	Hook *h;
	int value;

	for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
	{
		value = (*(h->func.intfunc))(conf,ce,CONFIG_EXCEPT);
		if (value == 1)
			break;
	}

	return 1;
}

int _test_except(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	Hook *h;
	int used = 0;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: except without type",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}

	if (!strcmp(ce->ce_vardata, "tkl"))
	{
		config_warn("%s:%i: except tkl { } is now called except ban { }. "
		            "Simply rename the block from 'except tkl' to 'except ban' "
		            "to get rid of this warning.",
		            ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		safe_strdup(ce->ce_vardata, "ban"); /* awww */
	}

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

	if (!used)
	{
		config_error("%s:%i: unknown except type %s",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);
		return 1;
	}

	return errors;
}

/*
 * vhost {} block parser
*/
int	_conf_vhost(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_vhost *vhost;
	ConfigEntry *cep, *cepp;
	vhost = safe_alloc(sizeof(ConfigItem_vhost));

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "vhost"))
		{
			char *user, *host;
			user = strtok(cep->ce_vardata, "@");
			host = strtok(NULL, "");
			if (!host)
				safe_strdup(vhost->virthost, user);
			else
			{
				safe_strdup(vhost->virtuser, user);
				safe_strdup(vhost->virthost, host);
			}
		}
		else if (!strcmp(cep->ce_varname, "login"))
			safe_strdup(vhost->login, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "password"))
			vhost->auth = AuthBlockToAuthConfig(cep);
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
					s = safe_alloc(sizeof(SWhois));
					safe_strdup(s->line, cepp->ce_varname);
					safe_strdup(s->setby, "vhost");
					AddListItem(s, vhost->swhois);
				}
			} else
			if (cep->ce_vardata)
			{
				s = safe_alloc(sizeof(SWhois));
				safe_strdup(s->line, cep->ce_vardata);
				safe_strdup(s->setby, "vhost");
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
	char has_vhost = 0, has_login = 0, has_password = 0, has_mask = 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
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

int	_test_sni(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	ConfigEntry *cep;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: sni block needs a name, eg: sni irc.xyz.com {",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "ssl-options") || !strcmp(cep->ce_varname, "tls-options"))
		{
			test_tlsblock(conf, cep, &errors);
		} else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"sni", cep->ce_varname);
			errors++;
			continue;
		}
	}

	return errors;
}

int	_conf_sni(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *tlsconfig = NULL;
	char *name;
	ConfigItem_sni *sni = NULL;

	name = ce->ce_vardata;
	if (!name)
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "ssl-options") || !strcmp(cep->ce_varname, "tls-options"))
		{
			tlsconfig = cep;
		}
	}

	if (!tlsconfig)
		return 0;

	sni = safe_alloc(sizeof(ConfigItem_listen));
	safe_strdup(sni->name, name);
	sni->tls_options = safe_alloc(sizeof(TLSOptions));
	conf_tlsblock(conf, tlsconfig, sni->tls_options);
	sni->ssl_ctx = init_ctx(sni->tls_options, 1);
	AddListItem(sni, conf_sni);

	return 1;
}

int     _conf_help(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_help *ca;
	MOTDLine *last = NULL, *temp;
	ca = safe_alloc(sizeof(ConfigItem_help));

	if (!ce->ce_vardata)
		ca->command = NULL;
	else
		safe_strdup(ca->command, ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		temp = safe_alloc(sizeof(MOTDLine));
		safe_strdup(temp->line, cep->ce_varname);
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
		if (strlen(cep->ce_varname) > 500)
		{
			config_error("%s:%i: oversized help item",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
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

	ca = safe_alloc(sizeof(ConfigItem_log));
	ca->logfd = -1;
	if (strchr(ce->ce_vardata, '%'))
		safe_strdup(ca->filefmt, ce->ce_vardata);
	else
		safe_strdup(ca->file, ce->ce_vardata);

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
	char *fname;

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

	fname = unreal_strftime(ce->ce_vardata);
	if ((fd = fd_fileopen(fname, O_WRONLY|O_CREAT)) == -1)
	{
		config_error("%s:%i: Couldn't open logfile (%s) for writing: %s",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			fname, strerror(errno));
		errors++;
	} else
	{
		fd_close(fd);
	}

	return errors;
}

int	_conf_link(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp, *ceppp;
	ConfigItem_link *link = NULL;
	NameValue *ofp;

	link = safe_alloc(sizeof(ConfigItem_link));
	safe_strdup(link->servername, ce->ce_vardata);

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
					safe_strdup(link->outgoing.bind_ip, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "hostname"))
					safe_strdup(link->outgoing.hostname, cepp->ce_vardata);
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
				else if (!strcmp(cepp->ce_varname, "ssl-options") || !strcmp(cepp->ce_varname, "tls-options"))
				{
					link->tls_options = safe_alloc(sizeof(TLSOptions));
					conf_tlsblock(conf, cepp, link->tls_options);
					link->ssl_ctx = init_ctx(link->tls_options, 0);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "password"))
			link->auth = AuthBlockToAuthConfig(cep);
		else if (!strcmp(cep->ce_varname, "hub"))
			safe_strdup(link->hub, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "leaf"))
			safe_strdup(link->leaf, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "leaf-depth") || !strcmp(cep->ce_varname, "leafdepth"))
			link->leaf_depth = atoi(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "class"))
		{
			link->class = find_class(cep->ce_vardata);
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
		else if (!strcmp(cep->ce_varname, "verify-certificate"))
		{
			link->verify_certificate = config_checkval(cep->ce_vardata, CFG_YESNO);
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
	}

	/* The default is 'hub *', unless you specify leaf or hub manually. */
	if (!link->hub && !link->leaf)
		safe_strdup(link->hub, "*");

	AppendListItem(link, conf_link);
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
	int errors = 0;

	int has_incoming = 0, has_incoming_mask = 0, has_outgoing = 0;
	int has_outgoing_bind_ip = 0, has_outgoing_hostname = 0, has_outgoing_port = 0;
	int has_outgoing_options = 0, has_hub = 0, has_leaf = 0, has_leaf_depth = 0;
	int has_password = 0, has_class = 0, has_options = 0;

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
					else
					if (config_is_blankorempty(cepp, "link::incoming"))
					{
						errors++;
						continue;
					}
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
					if (config_is_blankorempty(cepp, "link::outgoing"))
					{
						errors++;
						continue;
					}
					config_detect_duplicate(&has_outgoing_bind_ip, cepp, &errors);
					// todo: ipv4 vs ipv6
				}
				else if (!strcmp(cepp->ce_varname, "hostname"))
				{
					if (config_is_blankorempty(cepp, "link::outgoing"))
					{
						errors++;
						continue;
					}
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
					if (config_is_blankorempty(cepp, "link::outgoing"))
					{
						errors++;
						continue;
					}
					config_detect_duplicate(&has_outgoing_port, cepp, &errors);
				}
				else if (!strcmp(cepp->ce_varname, "options"))
				{
					config_detect_duplicate(&has_outgoing_options, cepp, &errors);
					for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
					{
						if (!strcmp(ceppp->ce_varname, "autoconnect"))
							;
						else if (!strcmp(ceppp->ce_varname, "ssl") || !strcmp(ceppp->ce_varname, "tls"))
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
				else if (!strcmp(cepp->ce_varname, "ssl-options") || !strcmp(cepp->ce_varname, "tls-options"))
				{
					test_tlsblock(conf, cepp, &errors);
				}
				else
				{
					config_error("%s:%d: Unknown directive '%s'",
					             cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
					             config_var(cepp));
					errors++;
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
				AuthConfig *auth = AuthBlockToAuthConfig(cep);
				/* hm. would be nicer if handled @auth-system I think. ah well.. */
				if ((auth->type != AUTHTYPE_PLAINTEXT) && (auth->type != AUTHTYPE_TLS_CLIENTCERT) &&
				    (auth->type != AUTHTYPE_TLS_CLIENTCERTFP) && (auth->type != AUTHTYPE_SPKIFP))
				{
					config_error("%s:%i: password in link block should be plaintext OR should be the "
					             "SSL or SPKI fingerprint of the remote link (=better)",
					             /* TODO: mention some faq or wiki item for more information */
					             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					errors++;
				}
				Auth_FreeAuthConfig(auth);
			}
		}
		else if (!strcmp(cep->ce_varname, "hub"))
		{
			if (config_is_blankorempty(cep, "link"))
			{
				errors++;
				continue;
			}
			config_detect_duplicate(&has_hub, cep, &errors);
		}
		else if (!strcmp(cep->ce_varname, "leaf"))
		{
			if (config_is_blankorempty(cep, "link"))
			{
				errors++;
				continue;
			}
			config_detect_duplicate(&has_leaf, cep, &errors);
		}
		else if (!strcmp(cep->ce_varname, "leaf-depth") || !strcmp(cep->ce_varname, "leafdepth"))
		{
			if (config_is_blankorempty(cep, "link"))
			{
				errors++;
				continue;
			}
			config_detect_duplicate(&has_leaf_depth, cep, &errors);
		}
		else if (!strcmp(cep->ce_varname, "class"))
		{
			if (config_is_blankorempty(cep, "link"))
			{
				errors++;
				continue;
			}
			config_detect_duplicate(&has_class, cep, &errors);
		}
		else if (!strcmp(cep->ce_varname, "ciphers"))
		{
			config_error("%s:%d: link::ciphers has been moved to link::outgoing::ssl-options::ciphers, "
			             "see https://www.unrealircd.org/docs/FAQ#link::ciphers_no_longer_works",
			             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
		}
		else if (!strcmp(cep->ce_varname, "verify-certificate"))
		{
			if (config_is_blankorempty(cep, "link"))
			{
				errors++;
				continue;
			}
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
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename,
			    cep->ce_varlinenum, "link", cep->ce_varname);
			errors++;
			continue;
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

	ca = safe_alloc(sizeof(ConfigItem_ban));
	if (!strcmp(ce->ce_vardata, "realname"))
		ca->flag.type = CONF_BAN_REALNAME;
	else if (!strcmp(ce->ce_vardata, "server"))
		ca->flag.type = CONF_BAN_SERVER;
	else if (!strcmp(ce->ce_vardata, "version"))
	{
		ca->flag.type = CONF_BAN_VERSION;
		tempiConf.use_ban_version = 1; /* enable CTCP VERSION on connect */
	}
	else {
		int value;
		safe_free(ca); /* ca isn't used, modules have their own list. */
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
			safe_strdup(ca->mask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "reason"))
			safe_strdup(ca->reason, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "action"))
			ca->action = banact_stringtoval(cep->ce_vardata);
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
	else if (!strcmp(ce->ce_vardata, "server"))
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

int _conf_require(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	Hook *h;
	char *usermask = NULL;
	char *hostmask = NULL;
	char *reason = NULL;

	if (strcmp(ce->ce_vardata, "authentication") && strcmp(ce->ce_vardata, "sasl"))
	{
		/* Some other block... run modules... */
		int value;
		for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
		{
			value = (*(h->func.intfunc))(conf,ce,CONFIG_REQUIRE);
			if (value == 1)
				break;
		}
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
				safe_strdup(usermask, buf);
				safe_strdup(hostmask, p);
			} else {
				safe_strdup(hostmask, cep->ce_vardata);
			}
		}
		else if (!strcmp(cep->ce_varname, "reason"))
			safe_strdup(reason, cep->ce_vardata);
	}

	if (!usermask)
		safe_strdup(usermask, "*");

	if (!reason)
		safe_strdup(reason, "-");

	tkl_add_serverban(TKL_KILL, usermask, hostmask, reason, "-config-", 0, TStime(), 1, TKL_FLAG_CONFIG);
	safe_free(usermask);
	safe_free(hostmask);
	safe_free(reason);
	return 0;
}

int _test_require(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int errors = 0;
	Hook *h;
	char has_mask = 0, has_reason = 0;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: require without type, did you mean 'require authentication'?",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	if (!strcmp(ce->ce_vardata, "authentication"))
	{}
	else if (!strcmp(ce->ce_vardata, "sasl"))
	{
		config_warn("%s:%i: the 'require sasl' block is now called 'require authentication'",
		            ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
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
			value = (*(h->func.intfunc))(conf,ce,CONFIG_REQUIRE, &errs);
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
			config_error("%s:%i: unknown require type '%s'",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				ce->ce_vardata);
			return 1;
		}
		return errors;
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (config_is_blankorempty(cep, "require"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "mask"))
		{
			if (has_mask)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "require::mask");
				continue;
			}
			has_mask = 1;
		}
		else if (!strcmp(cep->ce_varname, "reason"))
		{
			if (has_reason)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "require::reason");
				continue;
			}
			has_reason = 1;
		}
	}

	if (!has_mask)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"require::mask");
		errors++;
	}
	if (!has_reason)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"require::reason");
		errors++;
	}
	return errors;
}

#define CheckNull(x) if ((!(x)->ce_vardata) || (!(*((x)->ce_vardata)))) { config_error("%s:%i: missing parameter", (x)->ce_fileptr->cf_filename, (x)->ce_varlinenum); errors++; continue; }
#define CheckNullAllowEmpty(x) if ((!(x)->ce_vardata)) { config_error("%s:%i: missing parameter", (x)->ce_fileptr->cf_filename, (x)->ce_varlinenum); errors++; continue; }
#define CheckDuplicate(cep, name, display) if (settings.has_##name) { config_warn_duplicate((cep)->ce_fileptr->cf_filename, cep->ce_varlinenum, "set::" display); continue; } else settings.has_##name = 1

void test_tlsblock(ConfigFile *conf, ConfigEntry *cep, int *totalerrors)
{
	ConfigEntry *cepp, *ceppp;
	int errors = 0;

	for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
	{
		if (!strcmp(cepp->ce_varname, "renegotiate-timeout"))
		{
		}
		else if (!strcmp(cepp->ce_varname, "renegotiate-bytes"))
		{
		}
		else if (!strcmp(cepp->ce_varname, "ciphers") || !strcmp(cepp->ce_varname, "server-cipher-list"))
		{
			CheckNull(cepp);
		}
		else if (!strcmp(cepp->ce_varname, "ciphersuites"))
		{
			CheckNull(cepp);
		}
		else if (!strcmp(cepp->ce_varname, "ecdh-curves"))
		{
			CheckNull(cepp);
#ifndef HAS_SSL_CTX_SET1_CURVES_LIST
			config_error("ecdh-curves specified but your OpenSSL/LibreSSL library does not "
			             "support setting curves manually by name. Either upgrade to a "
			             "newer library version or remove the 'ecdh-curves' directive "
			             "from your configuration file");
			errors++;
#endif
		}
		else if (!strcmp(cepp->ce_varname, "protocols"))
		{
			char copy[512], *p, *name;
			int v = 0;
			int option;
			char modifier;

			CheckNull(cepp);
			strlcpy(copy, cepp->ce_vardata, sizeof(copy));
			for (name = strtoken(&p, copy, ","); name; name = strtoken(&p, NULL, ","))
			{
				modifier = '\0';
				option = 0;

				if ((*name == '+') || (*name == '-'))
				{
					modifier = *name;
					name++;
				}

				if (!strcasecmp(name, "All"))
					option = TLS_PROTOCOL_ALL;
				else if (!strcasecmp(name, "TLSv1"))
					option = TLS_PROTOCOL_TLSV1;
				else if (!strcasecmp(name, "TLSv1.1"))
					option = TLS_PROTOCOL_TLSV1_1;
				else if (!strcasecmp(name, "TLSv1.2"))
					option = TLS_PROTOCOL_TLSV1_2;
				else if (!strcasecmp(name, "TLSv1.3"))
					option = TLS_PROTOCOL_TLSV1_3;
				else
				{
#ifdef SSL_OP_NO_TLSv1_3
					config_warn("%s:%i: %s: unknown protocol '%s'. "
								 "Valid protocols are: TLSv1,TLSv1.1,TLSv1.2,TLSv1.3",
								 cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, config_var(cepp), name);
#else
					config_warn("%s:%i: %s: unknown protocol '%s'. "
								 "Valid protocols are: TLSv1,TLSv1.1,TLSv1.2",
								 cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, config_var(cepp), name);
#endif
				}

				if (option)
				{
					if (modifier == '\0')
						v = option;
					else if (modifier == '+')
						v |= option;
					else if (modifier == '-')
						v &= ~option;
				}
			}
			if (v == 0)
			{
				config_error("%s:%i: %s: no protocols enabled. Hint: set at least TLSv1.2",
					cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, config_var(cepp));
				errors++;
			}
		}
		else if (!strcmp(cepp->ce_varname, "certificate") ||
		         !strcmp(cepp->ce_varname, "key") ||
		         !strcmp(cepp->ce_varname, "trusted-ca-file"))
		{
			char *path;
			CheckNull(cepp);
			path = convert_to_absolute_path_duplicate(cepp->ce_vardata, CONFDIR);
			if (!file_exists(path))
			{
				config_error("%s:%i: %s: could not open '%s': %s",
					cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, config_var(cepp),
					path, strerror(errno));
				safe_free(path);
				errors++;
			}
			safe_free(path);
		}
		else if (!strcmp(cepp->ce_varname, "dh"))
		{
			/* Support for this undocumented option was silently dropped in 5.0.0.
			 * Since 5.0.7 we print a warning about it, since you never know
			 * someone may still have it configured. -- Syzop
			 */
			config_warn("%s:%d: Not reading DH file '%s'. UnrealIRCd does not support old DH(E), we use modern ECDHE/EECDH. "
			            "Just remove the 'dh' directive from your config file to get rid of this warning.",
				cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
				cepp->ce_vardata ? cepp->ce_vardata : "");
		}
		else if (!strcmp(cepp->ce_varname, "outdated-protocols"))
		{
			char copy[512], *p, *name;
			int v = 0;
			int option;
			char modifier;

			CheckNull(cepp);
			strlcpy(copy, cepp->ce_vardata, sizeof(copy));
			for (name = strtoken(&p, copy, ","); name; name = strtoken(&p, NULL, ","))
			{
				if (!strcasecmp(name, "All"))
					;
				else if (!strcasecmp(name, "TLSv1"))
					;
				else if (!strcasecmp(name, "TLSv1.1"))
					;
				else if (!strcasecmp(name, "TLSv1.2"))
					;
				else if (!strcasecmp(name, "TLSv1.3"))
					;
				else
				{
#ifdef SSL_OP_NO_TLSv1_3
					config_warn("%s:%i: %s: unknown protocol '%s'. "
								 "Valid protocols are: TLSv1,TLSv1.1,TLSv1.2,TLSv1.3",
								 cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, config_var(cepp), name);
#else
					config_warn("%s:%i: %s: unknown protocol '%s'. "
								 "Valid protocols are: TLSv1,TLSv1.1,TLSv1.2",
								 cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, config_var(cepp), name);
#endif
		                }
			}
		}
		else if (!strcmp(cepp->ce_varname, "outdated-ciphers"))
		{
			CheckNull(cepp);
		}
		else if (!strcmp(cepp->ce_varname, "options"))
		{
			for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
				if (!config_binary_flags_search(_TLSFlags, ceppp->ce_varname, ARRAY_SIZEOF(_TLSFlags)))
				{
					config_error("%s:%i: unknown SSL/TLS option '%s'",
							 ceppp->ce_fileptr->cf_filename,
							 ceppp->ce_varlinenum, ceppp->ce_varname);
					errors ++;
				}
		}
		else if (!strcmp(cepp->ce_varname, "sts-policy"))
		{
			int has_port = 0;
			int has_duration = 0;

			for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
			{
				if (!strcmp(ceppp->ce_varname, "port"))
				{
					int port;
					CheckNull(ceppp);
					port = atoi(ceppp->ce_vardata);
					if ((port < 1) || (port > 65535))
					{
						config_error("%s:%i: invalid port number specified in sts-policy::port (%d)",
						             ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum, port);
						errors++;
					}
					has_port = 1;
				}
				else if (!strcmp(ceppp->ce_varname, "duration"))
				{
					long duration;
					CheckNull(ceppp);
					duration = config_checkval(ceppp->ce_vardata, CFG_TIME);
					if (duration < 1)
					{
						config_error("%s:%i: invalid duration specified in sts-policy::duration (%ld seconds)",
						             ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum, duration);
						errors++;
					}
					has_duration = 1;
				}
				else if (!strcmp(ceppp->ce_varname, "preload"))
				{
					CheckNull(ceppp);
				}
			}
			if (!has_port)
			{
				config_error("%s:%i: sts-policy block without port",
				             cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
				errors++;
			}
			if (!has_duration)
			{
				config_error("%s:%i: sts-policy block without duration",
				             cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
				errors++;
			}
		}
		else
		{
			config_error("%s:%i: unknown directive %s",
				cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
				config_var(cepp));
			errors++;
		}
	}

	*totalerrors += errors;
}

void free_tls_options(TLSOptions *tlsoptions)
{
	if (!tlsoptions)
		return;

	safe_free(tlsoptions->certificate_file);
	safe_free(tlsoptions->key_file);
	safe_free(tlsoptions->trusted_ca_file);
	safe_free(tlsoptions->ciphers);
	safe_free(tlsoptions->ciphersuites);
	safe_free(tlsoptions->ecdh_curves);
	safe_free(tlsoptions->outdated_protocols);
	safe_free(tlsoptions->outdated_ciphers);
	memset(tlsoptions, 0, sizeof(TLSOptions));
	safe_free(tlsoptions);
}

void conf_tlsblock(ConfigFile *conf, ConfigEntry *cep, TLSOptions *tlsoptions)
{
	ConfigEntry *cepp, *ceppp;
	NameValue *ofl;

	/* First, inherit settings from set::options::tls */
	if (tlsoptions != tempiConf.tls_options)
	{
		safe_strdup(tlsoptions->certificate_file, tempiConf.tls_options->certificate_file);
		safe_strdup(tlsoptions->key_file, tempiConf.tls_options->key_file);
		safe_strdup(tlsoptions->trusted_ca_file, tempiConf.tls_options->trusted_ca_file);
		tlsoptions->protocols = tempiConf.tls_options->protocols;
		safe_strdup(tlsoptions->ciphers, tempiConf.tls_options->ciphers);
		safe_strdup(tlsoptions->ciphersuites, tempiConf.tls_options->ciphersuites);
		safe_strdup(tlsoptions->ecdh_curves, tempiConf.tls_options->ecdh_curves);
		safe_strdup(tlsoptions->outdated_protocols, tempiConf.tls_options->outdated_protocols);
		safe_strdup(tlsoptions->outdated_ciphers, tempiConf.tls_options->outdated_ciphers);
		tlsoptions->options = tempiConf.tls_options->options;
		tlsoptions->renegotiate_bytes = tempiConf.tls_options->renegotiate_bytes;
		tlsoptions->renegotiate_timeout = tempiConf.tls_options->renegotiate_timeout;
		tlsoptions->sts_port = tempiConf.tls_options->sts_port;
		tlsoptions->sts_duration = tempiConf.tls_options->sts_duration;
		tlsoptions->sts_preload = tempiConf.tls_options->sts_preload;
	}

	/* Now process the options */
	for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
	{
		if (!strcmp(cepp->ce_varname, "ciphers") || !strcmp(cepp->ce_varname, "server-cipher-list"))
		{
			safe_strdup(tlsoptions->ciphers, cepp->ce_vardata);
		}
		else if (!strcmp(cepp->ce_varname, "ciphersuites"))
		{
			safe_strdup(tlsoptions->ciphersuites, cepp->ce_vardata);
		}
		else if (!strcmp(cepp->ce_varname, "ecdh-curves"))
		{
			safe_strdup(tlsoptions->ecdh_curves, cepp->ce_vardata);
		}
		else if (!strcmp(cepp->ce_varname, "protocols"))
		{
			char copy[512], *p, *name;
			int option;
			char modifier;

			strlcpy(copy, cepp->ce_vardata, sizeof(copy));
			tlsoptions->protocols = 0;
			for (name = strtoken(&p, copy, ","); name; name = strtoken(&p, NULL, ","))
			{
				modifier = '\0';
				option = 0;

				if ((*name == '+') || (*name == '-'))
				{
					modifier = *name;
					name++;
				}

				if (!strcasecmp(name, "All"))
					option = TLS_PROTOCOL_ALL;
				else if (!strcasecmp(name, "TLSv1"))
					option = TLS_PROTOCOL_TLSV1;
				else if (!strcasecmp(name, "TLSv1.1"))
					option = TLS_PROTOCOL_TLSV1_1;
				else if (!strcasecmp(name, "TLSv1.2"))
					option = TLS_PROTOCOL_TLSV1_2;
				else if (!strcasecmp(name, "TLSv1.3"))
					option = TLS_PROTOCOL_TLSV1_3;

				if (option)
				{
					if (modifier == '\0')
						tlsoptions->protocols = option;
					else if (modifier == '+')
						tlsoptions->protocols |= option;
					else if (modifier == '-')
						tlsoptions->protocols &= ~option;
				}
			}
		}
		else if (!strcmp(cepp->ce_varname, "certificate"))
		{
			convert_to_absolute_path(&cepp->ce_vardata, CONFDIR);
			safe_strdup(tlsoptions->certificate_file, cepp->ce_vardata);
		}
		else if (!strcmp(cepp->ce_varname, "key"))
		{
			convert_to_absolute_path(&cepp->ce_vardata, CONFDIR);
			safe_strdup(tlsoptions->key_file, cepp->ce_vardata);
		}
		else if (!strcmp(cepp->ce_varname, "trusted-ca-file"))
		{
			convert_to_absolute_path(&cepp->ce_vardata, CONFDIR);
			safe_strdup(tlsoptions->trusted_ca_file, cepp->ce_vardata);
		}
		else if (!strcmp(cepp->ce_varname, "outdated-protocols"))
		{
			safe_strdup(tlsoptions->outdated_protocols, cepp->ce_vardata);
		}
		else if (!strcmp(cepp->ce_varname, "outdated-ciphers"))
		{
			safe_strdup(tlsoptions->outdated_ciphers, cepp->ce_vardata);
		}
		else if (!strcmp(cepp->ce_varname, "renegotiate-bytes"))
		{
			tlsoptions->renegotiate_bytes = config_checkval(cepp->ce_vardata, CFG_SIZE);
		}
		else if (!strcmp(cepp->ce_varname, "renegotiate-timeout"))
		{
			tlsoptions->renegotiate_timeout = config_checkval(cepp->ce_vardata, CFG_TIME);
		}
		else if (!strcmp(cepp->ce_varname, "options"))
		{
			tlsoptions->options = 0;
			for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
			{
				ofl = config_binary_flags_search(_TLSFlags, ceppp->ce_varname, ARRAY_SIZEOF(_TLSFlags));
				if (ofl) /* this should always be true */
					tlsoptions->options |= ofl->flag;
			}
		}
		else if (!strcmp(cepp->ce_varname, "sts-policy"))
		{
			/* We do not inherit ::sts-policy if there is a specific block for this one... */
			tlsoptions->sts_port = 0;
			tlsoptions->sts_duration = 0;
			tlsoptions->sts_preload = 0;
			for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
			{
				if (!strcmp(ceppp->ce_varname, "port"))
					tlsoptions->sts_port = atoi(ceppp->ce_vardata);
				else if (!strcmp(ceppp->ce_varname, "duration"))
					tlsoptions->sts_duration = config_checkval(ceppp->ce_vardata, CFG_TIME);
				else if (!strcmp(ceppp->ce_varname, "preload"))
					tlsoptions->sts_preload = config_checkval(ceppp->ce_vardata, CFG_YESNO);
			}
		}
	}
}

int	_conf_set(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp, *ceppp, *cep4;
	Hook *h;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "kline-address")) {
			safe_strdup(tempiConf.kline_address, cep->ce_vardata);
		}
		if (!strcmp(cep->ce_varname, "gline-address")) {
			safe_strdup(tempiConf.gline_address, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "modes-on-connect")) {
			tempiConf.conn_modes = (long) set_usermode(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "modes-on-oper")) {
			tempiConf.oper_modes = (long) set_usermode(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "modes-on-join")) {
			conf_channelmodes(cep->ce_vardata, &tempiConf.modes_on_join, 0);
		}
		else if (!strcmp(cep->ce_varname, "snomask-on-oper")) {
			safe_strdup(tempiConf.oper_snomask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "level-on-join")) {
			tempiConf.level_on_join = channellevel_to_int(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "static-quit")) {
			safe_strdup(tempiConf.static_quit, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "static-part")) {
			safe_strdup(tempiConf.static_part, cep->ce_vardata);
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
		}
		else if (!strcmp(cep->ce_varname, "auto-join")) {
			safe_strdup(tempiConf.auto_join_chans, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "oper-auto-join")) {
			safe_strdup(tempiConf.oper_auto_join_chans, cep->ce_vardata);
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
			if (!strcasecmp(cep->ce_vardata, "always"))
				tempiConf.userhost_allowed = UHALLOW_ALWAYS;
			else if (!strcasecmp(cep->ce_vardata, "never"))
				tempiConf.userhost_allowed = UHALLOW_NEVER;
			else if (!strcasecmp(cep->ce_vardata, "not-on-channels"))
				tempiConf.userhost_allowed = UHALLOW_NOCHANS;
			else
				tempiConf.userhost_allowed = UHALLOW_REJOIN;
		}
		else if (!strcmp(cep->ce_varname, "channel-command-prefix")) {
			safe_strdup(tempiConf.channel_command_prefix, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "restrict-usermodes")) {
			int i;
			char *p = safe_alloc(strlen(cep->ce_vardata) + 1), *x = p;
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
			char *p = safe_alloc(strlen(cep->ce_vardata) + 1), *x = p;
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
			safe_strdup(tempiConf.restrict_extendedbans, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "anti-spam-quit-message-time")) {
			tempiConf.anti_spam_quit_message_time = config_checkval(cep->ce_vardata,CFG_TIME);
		}
		else if (!strcmp(cep->ce_varname, "allow-user-stats")) {
			if (!cep->ce_entries)
			{
				safe_strdup(tempiConf.allow_user_stats, cep->ce_vardata);
			}
			else
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					OperStat *os = safe_alloc(sizeof(OperStat));
					safe_strdup(os->flag, cepp->ce_varname);
					AddListItem(os, tempiConf.allow_user_stats_ext);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "maxchannelsperuser")) {
			tempiConf.maxchannelsperuser = atoi(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "ping-warning")) {
			tempiConf.ping_warning = atoi(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "maxdccallow")) {
			tempiConf.maxdccallow = atoi(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "max-targets-per-command"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				int v;
				if (!strcmp(cepp->ce_vardata, "max"))
					v = MAXTARGETS_MAX;
				else
					v = atoi(cepp->ce_vardata);
				setmaxtargets(cepp->ce_varname, v);
			}
		}
		else if (!strcmp(cep->ce_varname, "network-name")) {
			char *tmp;
			safe_strdup(tempiConf.network.x_ircnetwork, cep->ce_vardata);
			for (tmp = cep->ce_vardata; *cep->ce_vardata; cep->ce_vardata++) {
				if (*cep->ce_vardata == ' ')
					*cep->ce_vardata='-';
			}
			safe_strdup(tempiConf.network.x_ircnet005, tmp);
			cep->ce_vardata = tmp;
		}
		else if (!strcmp(cep->ce_varname, "default-server")) {
			safe_strdup(tempiConf.network.x_defserv, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "services-server")) {
			safe_strdup(tempiConf.network.x_services_name, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "sasl-server")) {
			safe_strdup(tempiConf.network.x_sasl_server, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "stats-server")) {
			safe_strdup(tempiConf.network.x_stats_server, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "help-channel")) {
			safe_strdup(tempiConf.network.x_helpchan, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "hiddenhost-prefix")) {
			safe_strdup(tempiConf.network.x_hidden_host, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "hide-ban-reason")) {
			tempiConf.hide_ban_reason = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "prefix-quit")) {
			if (!strcmp(cep->ce_vardata, "0") || !strcmp(cep->ce_vardata, "no"))
				safe_free(tempiConf.network.x_prefix_quit);
			else
				safe_strdup(tempiConf.network.x_prefix_quit, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "link")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "bind-ip")) {
					safe_strdup(tempiConf.link_bindip, cepp->ce_vardata);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "dns")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "bind-ip")) {
					safe_strdup(tempiConf.dns_bindip, cepp->ce_vardata);
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "anti-flood")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				int lag_penalty = -1;
				int lag_penalty_bytes = -1;
				for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
				{
					if (!strcmp(ceppp->ce_varname, "handshake-data-flood"))
					{
						for (cep4 = ceppp->ce_entries; cep4; cep4 = cep4->ce_next)
						{
							if (!strcmp(cep4->ce_varname, "amount"))
								tempiConf.handshake_data_flood_amount = config_checkval(cep4->ce_vardata, CFG_SIZE);
							else if (!strcmp(cep4->ce_varname, "ban-time"))
								tempiConf.handshake_data_flood_ban_time = config_checkval(cep4->ce_vardata, CFG_TIME);
							else if (!strcmp(cep4->ce_varname, "ban-action"))
								tempiConf.handshake_data_flood_ban_action = banact_stringtoval(cep4->ce_vardata);
						}
					}
					else if (!strcmp(ceppp->ce_varname, "away-flood"))
					{
						config_parse_flood_generic(ceppp->ce_vardata, &tempiConf, cepp->ce_varname, FLD_AWAY);
					}
					else if (!strcmp(ceppp->ce_varname, "nick-flood"))
					{
						config_parse_flood_generic(ceppp->ce_vardata, &tempiConf, cepp->ce_varname, FLD_NICK);
					}
					else if (!strcmp(ceppp->ce_varname, "join-flood"))
					{
						config_parse_flood_generic(ceppp->ce_vardata, &tempiConf, cepp->ce_varname, FLD_JOIN);
					}
					else if (!strcmp(ceppp->ce_varname, "invite-flood"))
					{
						config_parse_flood_generic(ceppp->ce_vardata, &tempiConf, cepp->ce_varname, FLD_INVITE);
					}
					else if (!strcmp(ceppp->ce_varname, "knock-flood"))
					{
						config_parse_flood_generic(ceppp->ce_vardata, &tempiConf, cepp->ce_varname, FLD_KNOCK);
					}
					else if (!strcmp(ceppp->ce_varname, "lag-penalty"))
					{
						lag_penalty = atoi(ceppp->ce_vardata);
					}
					else if (!strcmp(ceppp->ce_varname, "lag-penalty-bytes"))
					{
						lag_penalty_bytes = config_checkval(ceppp->ce_vardata, CFG_SIZE);
						if (lag_penalty_bytes <= 0)
							lag_penalty_bytes = INT_MAX;
					}
					else if (!strcmp(ceppp->ce_varname, "connect-flood"))
					{
						int cnt, period;
						config_parse_flood(ceppp->ce_vardata, &cnt, &period);
						tempiConf.throttle_count = cnt;
						tempiConf.throttle_period = period;
					}
					if (!strcmp(ceppp->ce_varname, "max-concurrent-conversations"))
					{
						/* We use a hack here to make it fit our storage format */
						char buf[64];
						int users=0;
						long every=0;
						for (cep4 = ceppp->ce_entries; cep4; cep4 = cep4->ce_next)
						{
							if (!strcmp(cep4->ce_varname, "users"))
							{
								users = atoi(cep4->ce_vardata);
							} else
							if (!strcmp(cep4->ce_varname, "new-user-every"))
							{
								every = config_checkval(cep4->ce_vardata, CFG_TIME);
							}
						}
						snprintf(buf, sizeof(buf), "%d:%ld", users, every);
						config_parse_flood_generic(buf, &tempiConf, cepp->ce_varname, FLD_CONVERSATIONS);
					}
					else
					{
						for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
						{
							int value = (*(h->func.intfunc))(conf,ceppp,CONFIG_SET_ANTI_FLOOD);
							if (value == 1)
								break;
						}
					}
				}
				if ((lag_penalty != -1) && (lag_penalty_bytes != -1))
				{
					/* We use a hack here to make it fit our storage format */
					char buf[64];
					snprintf(buf, sizeof(buf), "%d:%d", lag_penalty_bytes, lag_penalty);
					config_parse_flood_generic(buf, &tempiConf, cepp->ce_varname, FLD_LAG_PENALTY);
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
				else if (!strcmp(cepp->ce_varname, "no-connect-tls-info")) {
					tempiConf.no_connect_tls_info = 1;
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
		else if (!strcmp(cep->ce_varname, "spamfilter"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "ban-time"))
					tempiConf.spamfilter_ban_time = config_checkval(cepp->ce_vardata,CFG_TIME);
				else if (!strcmp(cepp->ce_varname, "ban-reason"))
					safe_strdup(tempiConf.spamfilter_ban_reason, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "virus-help-channel"))
					safe_strdup(tempiConf.spamfilter_virus_help_channel, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "virus-help-channel-deny"))
					tempiConf.spamfilter_vchan_deny = config_checkval(cepp->ce_vardata,CFG_YESNO);
				else if (!strcmp(cepp->ce_varname, "except"))
				{
					char *name, *p;
					SpamExcept *e;
					safe_strdup(tempiConf.spamexcept_line, cepp->ce_vardata);
					for (name = strtoken(&p, cepp->ce_vardata, ","); name; name = strtoken(&p, NULL, ","))
					{
						if (*name == ' ')
							name++;
						if (*name)
						{
							e = safe_alloc(sizeof(SpamExcept) + strlen(name));
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
		else if (!strcmp(cep->ce_varname, "min-nick-length")) {
			int v = atoi(cep->ce_vardata);
			tempiConf.min_nick_length = v;
		}
		else if (!strcmp(cep->ce_varname, "nick-length")) {
			int v = atoi(cep->ce_vardata);
			tempiConf.nick_length = v;
		}
		else if (!strcmp(cep->ce_varname, "topic-length")) {
			int v = atoi(cep->ce_vardata);
			tempiConf.topic_length = v;
		}
		else if (!strcmp(cep->ce_varname, "away-length")) {
			int v = atoi(cep->ce_vardata);
			tempiConf.away_length = v;
		}
		else if (!strcmp(cep->ce_varname, "kick-length")) {
			int v = atoi(cep->ce_vardata);
			tempiConf.kick_length = v;
		}
		else if (!strcmp(cep->ce_varname, "quit-length")) {
			int v = atoi(cep->ce_vardata);
			tempiConf.quit_length = v;
		}
		else if (!strcmp(cep->ce_varname, "ssl") || !strcmp(cep->ce_varname, "tls")) {
			/* no need to alloc tempiConf.tls_options since config_defaults() already ensures it exists */
			conf_tlsblock(conf, cep, tempiConf.tls_options);
		}
		else if (!strcmp(cep->ce_varname, "plaintext-policy"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "user"))
					tempiConf.plaintext_policy_user = policy_strtoval(cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "oper"))
					tempiConf.plaintext_policy_oper = policy_strtoval(cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "server"))
					tempiConf.plaintext_policy_server = policy_strtoval(cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "user-message"))
					addmultiline(&tempiConf.plaintext_policy_user_message, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "oper-message"))
					addmultiline(&tempiConf.plaintext_policy_oper_message, cepp->ce_vardata);
			}
		}
		else if (!strcmp(cep->ce_varname, "outdated-tls-policy"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "user"))
					tempiConf.outdated_tls_policy_user = policy_strtoval(cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "oper"))
					tempiConf.outdated_tls_policy_oper = policy_strtoval(cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "server"))
					tempiConf.outdated_tls_policy_server = policy_strtoval(cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "user-message"))
					safe_strdup(tempiConf.outdated_tls_policy_user_message, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "oper-message"))
					safe_strdup(tempiConf.outdated_tls_policy_oper_message, cepp->ce_vardata);
			}
		}
		else if (!strcmp(cep->ce_varname, "default-ipv6-clone-mask"))
		{
			tempiConf.default_ipv6_clone_mask = atoi(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "hide-list")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "deny-channel"))
				{
					tempiConf.hide_list = 1;
					/* if we would expand this later then change this to a bitmask or struct or whatever */
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "max-unknown-connections-per-ip"))
		{
			tempiConf.max_unknown_connections_per_ip = atoi(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "handshake-timeout"))
		{
			tempiConf.handshake_timeout = config_checkval(cep->ce_vardata, CFG_TIME);
		}
		else if (!strcmp(cep->ce_varname, "sasl-timeout"))
		{
			tempiConf.sasl_timeout = config_checkval(cep->ce_vardata, CFG_TIME);
		}
		else if (!strcmp(cep->ce_varname, "handshake-delay"))
		{
			tempiConf.handshake_delay = config_checkval(cep->ce_vardata, CFG_TIME);
		}
		else if (!strcmp(cep->ce_varname, "automatic-ban-target"))
		{
			tempiConf.automatic_ban_target = ban_target_strtoval(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "manual-ban-target"))
		{
			tempiConf.manual_ban_target = ban_target_strtoval(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "reject-message"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "too-many-connections"))
					safe_strdup(tempiConf.reject_message_too_many_connections, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "server-full"))
					safe_strdup(tempiConf.reject_message_server_full, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "unauthorized"))
					safe_strdup(tempiConf.reject_message_unauthorized, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "kline"))
					safe_strdup(tempiConf.reject_message_kline, cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "gline"))
					safe_strdup(tempiConf.reject_message_gline, cepp->ce_vardata);
			}
		}
		else if (!strcmp(cep->ce_varname, "topic-setter"))
		{
			if (!strcmp(cep->ce_vardata, "nick"))
				tempiConf.topic_setter = SETTER_NICK;
			else if (!strcmp(cep->ce_vardata, "nick-user-host"))
				tempiConf.topic_setter = SETTER_NICK_USER_HOST;
		}
		else if (!strcmp(cep->ce_varname, "ban-setter"))
		{
			if (!strcmp(cep->ce_vardata, "nick"))
				tempiConf.ban_setter = SETTER_NICK;
			else if (!strcmp(cep->ce_vardata, "nick-user-host"))
				tempiConf.ban_setter = SETTER_NICK_USER_HOST;
		}
		else if (!strcmp(cep->ce_varname, "ban-setter-sync") || !strcmp(cep->ce_varname, "ban-setter-synch"))
		{
			tempiConf.ban_setter_sync = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "part-instead-of-quit-on-comment-change"))
		{
			tempiConf.part_instead_of_quit_on_comment_change = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "broadcast-channel-messages"))
		{
			if (!strcmp(cep->ce_vardata, "auto"))
				tempiConf.broadcast_channel_messages = BROADCAST_CHANNEL_MESSAGES_AUTO;
			else if (!strcmp(cep->ce_vardata, "always"))
				tempiConf.broadcast_channel_messages = BROADCAST_CHANNEL_MESSAGES_ALWAYS;
			else if (!strcmp(cep->ce_vardata, "never"))
				tempiConf.broadcast_channel_messages = BROADCAST_CHANNEL_MESSAGES_NEVER;
		}
		else if (!strcmp(cep->ce_varname, "allowed-channelchars"))
		{
			tempiConf.allowed_channelchars = allowed_channelchars_strtoval(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "hide-idle-time"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "policy"))
					tempiConf.hide_idle_time = hideidletime_strtoval(cepp->ce_vardata);
			}
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
	ConfigEntry *cep, *cepp, *ceppp, *cep4;
	int tempi;
	int errors = 0;
	Hook *h;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
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
			else if (match_simple("*@unrealircd.com", cep->ce_vardata) || match_simple("*@unrealircd.org",cep->ce_vardata) || match_simple("unreal-*@lists.sourceforge.net",cep->ce_vardata))
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
			else if (match_simple("*@unrealircd.com", cep->ce_vardata) || match_simple("*@unrealircd.org",cep->ce_vardata) || match_simple("unreal-*@lists.sourceforge.net",cep->ce_vardata))
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
				if (strchr("orzSHqtW", *p))
				{
					config_error("%s:%i: set::modes-on-connect may not include mode '%c'",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, *p);
					errors++;
				}
			set_usermode(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "modes-on-join")) {
			char *c;
			struct ChMode temp;
			memset(&temp, 0, sizeof(temp));
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
			conf_channelmodes(cep->ce_vardata, &temp, 1);
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
				if (strchr("orzS", *p))
				{
					config_error("%s:%i: set::modes-on-oper may not include mode '%c'",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, *p);
					errors++;
				}
			set_usermode(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "snomask-on-oper")) {
			CheckNull(cep);
			CheckDuplicate(cep, snomask_on_oper, "snomask-on-oper");
		}
		else if (!strcmp(cep->ce_varname, "level-on-join")) {
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
			if (!config_checkval(cep->ce_vardata,CFG_SIZE))
			{
				config_error("%s:%i: set::who-limit: value must be at least 1",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
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
			if (strcasecmp(cep->ce_vardata, "always") &&
			    strcasecmp(cep->ce_vardata, "never") &&
			    strcasecmp(cep->ce_vardata, "not-on-channels") &&
			    strcasecmp(cep->ce_vardata, "force-rejoin"))
			{
				config_error("%s:%i: set::allow-userhost-change is invalid",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum);
				errors++;
				continue;
			}
		}
		else if (!strcmp(cep->ce_varname, "anti-spam-quit-message-time")) {
			CheckNull(cep);
			CheckDuplicate(cep, anti_spam_quit_message_time, "anti-spam-quit-message-time");
		}
		else if (!strcmp(cep->ce_varname, "oper-only-stats"))
		{
			config_warn("%s:%d: We no longer use a blacklist for stats (set::oper-only-stats) but "
			             "have a whitelist now instead (set::allow-user-stats). ",
			             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			config_warn("Simply delete the oper-only-stats line from your configuration file %s around line %d to get rid of this warning",
			             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;
		}
		else if (!strcmp(cep->ce_varname, "allow-user-stats"))
		{
			CheckDuplicate(cep, allow_user_stats, "allow-user-stats");
			if (!cep->ce_entries)
			{
				CheckNull(cep);
			}
			else
			{
				/* TODO: check the entries for existence?
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
				} */
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
		else if (!strcmp(cep->ce_varname, "ping-warning")) {
			CheckNull(cep);
			CheckDuplicate(cep, ping_warning, "ping-warning");
			tempi = atoi(cep->ce_vardata);
			/* it is pointless to allow setting higher than 170 */
			if (tempi > 170)
			{
				config_error("%s:%i: set::ping-warning must be < 170",
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
		else if (!strcmp(cep->ce_varname, "max-targets-per-command"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				CheckNull(cepp);
				if (!strcasecmp(cepp->ce_varname, "NAMES") || !strcasecmp(cepp->ce_varname, "WHOWAS"))
				{
					if (atoi(cepp->ce_vardata) != 1)
					{
						config_error("%s:%i: set::max-targets-per-command::%s: "
						             "this command is hardcoded at a maximum of 1 "
						             "and cannot be configured to accept more.",
						             cepp->ce_fileptr->cf_filename,
						             cepp->ce_varlinenum,
						             cepp->ce_varname);
						errors++;
					}
				} else
				if (!strcasecmp(cepp->ce_varname, "USERHOST") ||
				    !strcasecmp(cepp->ce_varname, "USERIP") ||
				    !strcasecmp(cepp->ce_varname, "ISON") ||
				    !strcasecmp(cepp->ce_varname, "WATCH"))
				{
					if (strcmp(cepp->ce_vardata, "max"))
					{
						config_error("%s:%i: set::max-targets-per-command::%s: "
						             "this command is hardcoded at a maximum of 'max' "
						             "and cannot be changed. This because it is "
						             "highly discouraged to change it.",
						             cepp->ce_fileptr->cf_filename,
						             cepp->ce_varlinenum,
						             cepp->ce_varname);
						errors++;
					}
				}
				/* Now check the value syntax in general: */
				if (strcmp(cepp->ce_vardata, "max")) /* anything other than 'max'.. */
				{
					int v = atoi(cepp->ce_vardata);
					if ((v < 1) || (v > 20))
					{
						config_error("%s:%i: set::max-targets-per-command::%s: "
						             "value should be 1-20 or 'max'",
						             cepp->ce_fileptr->cf_filename,
						             cepp->ce_varlinenum,
						             cepp->ce_varname);
						errors++;
					}
				}
			}
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
			CheckNull(cep);
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
		else if (!strcmp(cep->ce_varname, "anti-flood"))
		{
			int anti_flood_old = 0;
			int anti_flood_old_and_default = 0;

			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				int has_lag_penalty = 0;
				int has_lag_penalty_bytes = 0;

				/* Test for old options: */
				if (flood_option_is_old(cepp->ce_varname))
				{
					/* Special code if the user is using 100% of the defaults */
					if (cepp->ce_vardata &&
					    ((!strcmp(cepp->ce_varname, "nick-flood") && !strcmp(cepp->ce_vardata, "3:60")) ||
					     (!strcmp(cepp->ce_varname, "connect-flood") && cepp->ce_vardata && !strcmp(cepp->ce_vardata, "3:60")) ||
					     (!strcmp(cepp->ce_varname, "away-flood") && cepp->ce_vardata && !strcmp(cepp->ce_vardata, "4:120"))))
					{
						anti_flood_old_and_default = 1;
					} else
					{
						anti_flood_old = 1;
					}
					continue;
				}

				for (ceppp = cepp->ce_entries; ceppp; ceppp = ceppp->ce_next)
				{
					int everyone = !strcmp(cepp->ce_varname, "everyone") ? 1 : 0;
					int for_everyone = flood_option_is_for_everyone(ceppp->ce_varname);

					if (everyone && !for_everyone)
					{
						config_error("%s:%i: %s cannot be in the set::anti-flood::everyone block. "
						             "You can put it in 'known-users' or 'unknown-users' instead.",
							ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum,
							ceppp->ce_varname);
						errors++;
						continue;
					} else
					if (!everyone && for_everyone)
					{
						config_error("%s:%i: %s must be in the set::anti-flood::everyone block, not anywhere else.",
							ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum,
							ceppp->ce_varname);
						errors++;
						continue;
					}

					/* Now comes the actual config check for each element... */
					if (!strcmp(ceppp->ce_varname, "max-concurrent-conversations"))
					{
						for (cep4 = ceppp->ce_entries; cep4; cep4 = cep4->ce_next)
						{
							CheckNull(cep4);
							if (!strcmp(cep4->ce_varname, "users"))
							{
								int v = atoi(cep4->ce_vardata);
								if ((v < 1) || (v > MAXCCUSERS))
								{
									config_error("%s:%i: set::anti-flood::max-concurrent-conversations::users: "
										     "value should be between 1 and %d",
										     cep4->ce_fileptr->cf_filename, cep4->ce_varlinenum, MAXCCUSERS);
									errors++;
								}
							} else
							if (!strcmp(cep4->ce_varname, "new-user-every"))
							{
								long v = config_checkval(cep4->ce_vardata, CFG_TIME);
								if ((v < 1) || (v > 120))
								{
									config_error("%s:%i: set::anti-flood::max-concurrent-conversations::new-user-every: "
										     "value should be between 1 and 120 seconds",
										     cep4->ce_fileptr->cf_filename, cep4->ce_varlinenum);
									errors++;
								}
							} else
							{
								config_error_unknownopt(cep4->ce_fileptr->cf_filename,
									cep4->ce_varlinenum, "set::anti-flood",
									cep4->ce_varname);
								errors++;
							}
						}
						continue; /* required here, due to checknull directly below */
					}
					else if (!strcmp(ceppp->ce_varname, "unknown-flood-amount") ||
						 !strcmp(ceppp->ce_varname, "unknown-flood-bantime"))
					{
						config_error("%s:%i: set::anti-flood::%s: this setting has been moved. "
							     "See https://www.unrealircd.org/docs/Anti-flood_settings#handshake-data-flood",
							     ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum, ceppp->ce_varname);
						errors++;
						continue;
					}
					else if (!strcmp(ceppp->ce_varname, "handshake-data-flood"))
					{
						for (cep4 = ceppp->ce_entries; cep4; cep4 = cep4->ce_next)
						{
							if (!strcmp(cep4->ce_varname, "amount"))
							{
								long v;
								CheckNull(cep4);
								v = config_checkval(cep4->ce_vardata, CFG_SIZE);
								if (v < 1024)
								{
									config_error("%s:%i: set::anti-flood::handshake-data-flood::amount must be at least 1024 bytes",
										cep4->ce_fileptr->cf_filename, cep4->ce_varlinenum);
									errors++;
								}
							} else
							if (!strcmp(cep4->ce_varname, "ban-action"))
							{
								CheckNull(cep4);
								if (!banact_stringtoval(cep4->ce_vardata))
								{
									config_error("%s:%i: set::anti-flood::handshake-data-flood::ban-action has unknown action type '%s'",
										cep4->ce_fileptr->cf_filename, cep4->ce_varlinenum,
										cep4->ce_vardata);
									errors++;
								}
							} else
							if (!strcmp(cep4->ce_varname, "ban-time"))
							{
								CheckNull(cep4);
							} else
							{
								config_error_unknownopt(cep4->ce_fileptr->cf_filename,
									cep4->ce_varlinenum, "set::anti-flood::handshake-data-flood",
									cep4->ce_varname);
								errors++;
							}
						}
					}
					else if (!strcmp(ceppp->ce_varname, "away-count"))
					{
						int temp = atol(ceppp->ce_vardata);
						CheckNull(ceppp);
						if (temp < 1 || temp > 255)
						{
							config_error("%s:%i: set::anti-flood::away-count must be between 1 and 255",
								ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum);
							errors++;
						}
					}
					else if (!strcmp(ceppp->ce_varname, "away-period"))
					{
						CheckNull(ceppp);
						int temp = config_checkval(ceppp->ce_vardata, CFG_TIME);
						if (temp < 10)
						{
							config_error("%s:%i: set::anti-flood::away-period must be greater than 9",
								ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum);
							errors++;
						}
					}
					else if (!strcmp(ceppp->ce_varname, "away-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);
						if (!config_parse_flood(ceppp->ce_vardata, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 10))
						{
							config_error("%s:%i: set::anti-flood::away-flood error. Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be greater than 9",
								ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum);
							errors++;
						}
					}
					else if (!strcmp(ceppp->ce_varname, "nick-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);
						if (!config_parse_flood(ceppp->ce_vardata, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 5))
						{
							config_error("%s:%i: set::anti-flood::nick-flood error. Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be greater than 4",
								ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum);
							errors++;
						}
					}
					else if (!strcmp(ceppp->ce_varname, "join-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);

						if (!config_parse_flood(ceppp->ce_vardata, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 5))
						{
							config_error("%s:%i: join-flood error. Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be greater than 4",
								ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum);
							errors++;
						}
					}
					else if (!strcmp(ceppp->ce_varname, "invite-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);
						if (!config_parse_flood(ceppp->ce_vardata, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 5))
						{
							config_error("%s:%i: set::anti-flood::invite-flood error. Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be greater than 4",
								ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum);
							errors++;
						}
					}
					else if (!strcmp(ceppp->ce_varname, "knock-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);
						if (!config_parse_flood(ceppp->ce_vardata, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 5))
						{
							config_error("%s:%i: set::anti-flood::knock-flood error. Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be greater than 4",
								ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum);
							errors++;
						}
					}
					else if (!strcmp(ceppp->ce_varname, "lag-penalty"))
					{
						int v;
						CheckNull(ceppp);
						v = atoi(ceppp->ce_vardata);
						has_lag_penalty = 1;
						if ((v < 0) || (v > 10000))
						{
							config_error("%s:%i: set::anti-flood::%s::lag-penalty: value is in milliseconds and should be between 0 and 10000",
								ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum, cepp->ce_varname);
							errors++;
						}
					}
					else if (!strcmp(ceppp->ce_varname, "lag-penalty-bytes"))
					{
						has_lag_penalty_bytes = 1;
						CheckNull(ceppp);
					}
					else if (!strcmp(ceppp->ce_varname, "connect-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);
						if (strcmp(cepp->ce_varname, "everyone"))
						{
							config_error("%s:%i: connect-flood must be in the set::anti-flood::everyone block, not anywhere else.",
								ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum);
							errors++;
							continue;
						}
						if (!config_parse_flood(ceppp->ce_vardata, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 1) || (period > 3600))
						{
							config_error("%s:%i: set::anti-flood::connect-flood: Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be 1-3600",
								ceppp->ce_fileptr->cf_filename, ceppp->ce_varlinenum);
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
							value = (*(h->func.intfunc))(conf,ceppp,CONFIG_SET_ANTI_FLOOD,&errs);
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
							config_error_unknownopt(ceppp->ce_fileptr->cf_filename,
								ceppp->ce_varlinenum, "set::anti-flood",
								ceppp->ce_varname);
							errors++;
						}
						continue;
					}
				}
				if (has_lag_penalty+has_lag_penalty_bytes == 1)
				{
					config_error("%s:%i: set::anti-flood::%s: if you use lag-penalty then you must also add an lag-penalty-bytes item (and vice-versa)",
						cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, cepp->ce_varname);
					errors++;
				}
			}
			/* Now the warnings: */
			if (anti_flood_old == 1)
			{
				config_warn("%s:%d: the set::anti-flood block has been reorganized to be more flexible. "
				            "Your custom anti-flood settings have NOT been read.",
				            cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				config_warn("See https://www.unrealircd.org/docs/Anti-flood_settings for the new block style,");
				config_warn("OR: simply remove all the anti-flood options from the conf to get rid of this "
				            "warning and use the built-in defaults.");
			} else
			if (anti_flood_old_and_default == 1)
			{
				config_warn("%s:%d: the set::anti-flood block has been reorganized to be more flexible.",
					    cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				config_warn("To fix this warning, delete the anti-flood block from your configuration file "
				            "(file %s around line %d), this will make UnrealIRCd use the built-in defaults.",
				            cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				config_warn("If you want to learn more about the new functionality you can visit "
				            "https://www.unrealircd.org/docs/Anti-flood_settings");
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
				else if (!strcmp(cepp->ce_varname, "no-connect-tls-info")) {
					CheckDuplicate(cepp, options_no_connect_tls_info, "options::no-connect-tls-info");
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
		else if (!strcmp(cep->ce_varname, "timesync") || !strcmp(cep->ce_varname, "timesynch"))
		{
			config_warn("%s:%i: Timesync support has been removed from UnrealIRCd. "
			            "Please remove any set::timesync blocks you may have.",
			            cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			config_warn("Use the time synchronization feature of your OS/distro instead!");
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
		else if (!strcmp(cep->ce_varname, "min-nick-length")) {
			int v;
			CheckDuplicate(cep, min_nick_length, "min-nick-length");
			CheckNull(cep);
			v = atoi(cep->ce_vardata);
			if ((v <= 0) || (v > NICKLEN))
			{
				config_error("%s:%i: set::min-nick-length: value '%d' out of range (should be 1-%d)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, v, NICKLEN);
				errors++;
			}
			else
				nicklengths.min = v;
		}
		else if (!strcmp(cep->ce_varname, "nick-length")) {
			int v;
			CheckDuplicate(cep, nick_length, "nick-length");
			CheckNull(cep);
			v = atoi(cep->ce_vardata);
			if ((v <= 0) || (v > NICKLEN))
			{
				config_error("%s:%i: set::nick-length: value '%d' out of range (should be 1-%d)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, v, NICKLEN);
				errors++;
			}
			else
				nicklengths.max = v;
		}
		else if (!strcmp(cep->ce_varname, "topic-length")) {
			int v;
			CheckNull(cep);
			v = atoi(cep->ce_vardata);
			if ((v <= 0) || (v > MAXTOPICLEN))
			{
				config_error("%s:%i: set::topic-length: value '%d' out of range (should be 1-%d)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, v, MAXTOPICLEN);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "away-length")) {
			int v;
			CheckNull(cep);
			v = atoi(cep->ce_vardata);
			if ((v <= 0) || (v > MAXAWAYLEN))
			{
				config_error("%s:%i: set::away-length: value '%d' out of range (should be 1-%d)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, v, MAXAWAYLEN);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "kick-length")) {
			int v;
			CheckNull(cep);
			v = atoi(cep->ce_vardata);
			if ((v <= 0) || (v > MAXKICKLEN))
			{
				config_error("%s:%i: set::kick-length: value '%d' out of range (should be 1-%d)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, v, MAXKICKLEN);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "quit-length")) {
			int v;
			CheckNull(cep);
			v = atoi(cep->ce_vardata);
			if ((v <= 0) || (v > MAXQUITLEN))
			{
				config_error("%s:%i: set::quit-length: value '%d' out of range (should be 1-%d)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, v, MAXQUITLEN);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "ssl") || !strcmp(cep->ce_varname, "tls")) {
			test_tlsblock(conf, cep, &errors);
		}
		else if (!strcmp(cep->ce_varname, "plaintext-policy"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "user") ||
					!strcmp(cepp->ce_varname, "oper") ||
					!strcmp(cepp->ce_varname, "server"))
				{
					Policy policy;
					CheckNull(cepp);
					policy = policy_strtoval(cepp->ce_vardata);
					if (!policy)
					{
						config_error("%s:%i: set::plaintext-policy::%s: needs to be one of: 'allow', 'warn' or 'reject'",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, cepp->ce_varname);
						errors++;
					}
				} else if (!strcmp(cepp->ce_varname, "user-message") ||
				           !strcmp(cepp->ce_varname, "oper-message"))
				{
					CheckNull(cepp);
				} else {
					config_error_unknown(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::plaintext-policy",
						cepp->ce_varname);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "outdated-tls-policy"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "user") ||
					!strcmp(cepp->ce_varname, "oper") ||
					!strcmp(cepp->ce_varname, "server"))
				{
					Policy policy;
					CheckNull(cepp);
					policy = policy_strtoval(cepp->ce_vardata);
					if (!policy)
					{
						config_error("%s:%i: set::outdated-tls-policy::%s: needs to be one of: 'allow', 'warn' or 'reject'",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum, cepp->ce_varname);
						errors++;
					}
				} else if (!strcmp(cepp->ce_varname, "user-message") ||
				           !strcmp(cepp->ce_varname, "oper-message"))
				{
					CheckNull(cepp);
				} else {
					config_error_unknown(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::outdated-tls-policy",
						cepp->ce_varname);
					errors++;
					continue;
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
		else if (!strcmp(cep->ce_varname, "hide-list")) {
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "deny-channel"))
				{
				} else
				{
					config_error_unknown(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::hide-list",
						cepp->ce_varname);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "max-unknown-connections-per-ip")) {
			int v;
			CheckNull(cep);
			v = atoi(cep->ce_vardata);
			if (v < 1)
			{
				config_error("%s:%i: set::max-unknown-connections-per-ip: value should be at least 1.",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "handshake-timeout")) {
			int v;
			CheckNull(cep);
			v = config_checkval(cep->ce_vardata, CFG_TIME);
			if (v < 5)
			{
				config_error("%s:%i: set::handshake-timeout: value should be at least 5 seconds.",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "sasl-timeout")) {
			int v;
			CheckNull(cep);
			v = config_checkval(cep->ce_vardata, CFG_TIME);
			if (v < 5)
			{
				config_error("%s:%i: set::sasl-timeout: value should be at least 5 seconds.",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "handshake-delay"))
		{
			int v;
			CheckNull(cep);
			v = config_checkval(cep->ce_vardata, CFG_TIME);
			if (v >= 10)
			{
				config_error("%s:%i: set::handshake-delay: value should be less than 10 seconds.",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "ban-include-username"))
		{
			config_error("%s:%i: set::ban-include-username is no longer supported. "
			             "Use set { automatic-ban-target userip; }; instead.",
			             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			config_error("See https://www.unrealircd.org/docs/Set_block#set::automatic-ban-target "
			             "for more information and options.");
			errors++;
		}
		else if (!strcmp(cep->ce_varname, "automatic-ban-target"))
		{
			CheckNull(cep);
			if (!ban_target_strtoval(cep->ce_vardata))
			{
				config_error("%s:%i: set::automatic-ban-target: value '%s' is not recognized. "
				             "See https://www.unrealircd.org/docs/Set_block#set::automatic-ban-target",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "manual-ban-target"))
		{
			CheckNull(cep);
			if (!ban_target_strtoval(cep->ce_vardata))
			{
				config_error("%s:%i: set::manual-ban-target: value '%s' is not recognized. "
				             "See https://www.unrealircd.org/docs/Set_block#set::manual-ban-target",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "reject-message"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "password-mismatch"))
					;
				else if (!strcmp(cepp->ce_varname, "too-many-connections"))
					;
				else if (!strcmp(cepp->ce_varname, "server-full"))
					;
				else if (!strcmp(cepp->ce_varname, "unauthorized"))
					;
				else if (!strcmp(cepp->ce_varname, "kline"))
					;
				else if (!strcmp(cepp->ce_varname, "gline"))
					;
				else
				{
					config_error_unknown(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::reject-message",
						cepp->ce_varname);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->ce_varname, "topic-setter"))
		{
			CheckNull(cep);
			if (strcmp(cep->ce_vardata, "nick") && strcmp(cep->ce_vardata, "nick-user-host"))
			{
				config_error("%s:%i: set::topic-setter: value should be 'nick' or 'nick-user-host'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "ban-setter"))
		{
			CheckNull(cep);
			if (strcmp(cep->ce_vardata, "nick") && strcmp(cep->ce_vardata, "nick-user-host"))
			{
				config_error("%s:%i: set::ban-setter: value should be 'nick' or 'nick-user-host'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "ban-setter-sync") || !strcmp(cep->ce_varname, "ban-setter-synch"))
		{
			CheckNull(cep);
		}
		else if (!strcmp(cep->ce_varname, "part-instead-of-quit-on-comment-change"))
		{
			CheckNull(cep);
		}
		else if (!strcmp(cep->ce_varname, "broadcast-channel-messages"))
		{
			CheckNull(cep);
			if (strcmp(cep->ce_vardata, "auto") &&
			    strcmp(cep->ce_vardata, "always") &&
			    strcmp(cep->ce_vardata, "never"))
			{
				config_error("%s:%i: set::broadcast-channel-messages: value should be 'auto', 'always' or 'never'",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "allowed-channelchars"))
		{
			CheckNull(cep);
			if (!allowed_channelchars_strtoval(cep->ce_vardata))
			{
				config_error("%s:%i: set::allowed-channelchars: value should be one of: 'ascii', 'utf8' or 'any'",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "hide-idle-time"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "policy"))
				{
					if (!hideidletime_strtoval(cepp->ce_vardata))
					{
						config_error("%s:%i: set::hide-idle-time::policy: value should be one of: 'never', 'always', 'usermode' or 'oper-usermode'",
							     cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
					}
				}
				else
				{
					config_error_unknown(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::hide-idle-time",
						cepp->ce_varname);
					errors++;
					continue;
				}
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

	if (is_blacklisted_module(ce->ce_vardata))
	{
		/* config_warn("%s:%i: Module '%s' is blacklisted, not loading",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum, ce->ce_vardata); */
		return 1;
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

int	_test_blacklist_module(ConfigFile *conf, ConfigEntry *ce)
{
	char *path;
	ConfigItem_blacklist_module *m;

	if (!ce->ce_vardata)
	{
		config_status("%s:%i: blacklist-module: no module name given to blacklist",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}

	path = Module_TransformPath(ce->ce_vardata);

	/* Is it a good idea to warn about this?
	 * Yes, the user may have made a typo, thinking (s)he blacklisted something
	 *      but due to the typo the blacklist-module is not effective.
	 *  No, the user may have blacklisted a bunch of modules of which not all may
	 *      be installed at the time.
	 * Hmmmmmm.
	 */
	if (!file_exists(path))
	{
		config_warn("%s:%i: blacklist-module for '%s' but module does not exist anyway",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum, ce->ce_vardata);
		/* fallthrough */
	}

	m = safe_alloc(sizeof(ConfigItem_blacklist_module));
	safe_strdup(m->name, ce->ce_vardata);
	AddListItem(m, conf_blacklist_module);

	return 0;
}

int is_blacklisted_module(char *name)
{
	char *path = Module_TransformPath(name);
	ConfigItem_blacklist_module *m;

	for (m = conf_blacklist_module; m; m = m->next)
		if (!strcasecmp(m->name, name) || !strcasecmp(m->name, path))
			return 1;

	return 0;
}

void start_listeners(void)
{
	ConfigItem_listen *listenptr;
	int failed = 0, ports_bound = 0;
	char boundmsg_ipv4[512], boundmsg_ipv6[512];

	*boundmsg_ipv4 = *boundmsg_ipv6 = '\0';

	for (listenptr = conf_listen; listenptr; listenptr = listenptr->next)
	{
		/* Try to bind to any ports that are not yet bound and not marked as temporary */
		if (!(listenptr->options & LISTENER_BOUND) && !listenptr->flag.temporary)
		{
			if (add_listener(listenptr) == -1)
			{
				ircd_log(LOG_ERROR, "Failed to bind to %s:%i", listenptr->ip, listenptr->port);
				failed = 1;
			} else {
				if (loop.ircd_booted)
				{
					ircd_log(LOG_ERROR, "UnrealIRCd is now also listening on %s:%d (%s)%s",
						listenptr->ip, listenptr->port,
						listenptr->ipv6 ? "IPv6" : "IPv4",
						listenptr->options & LISTENER_TLS ? " (SSL/TLS)" : "");
				} else {
					if (listenptr->ipv6)
						snprintf(boundmsg_ipv6+strlen(boundmsg_ipv6), sizeof(boundmsg_ipv6)-strlen(boundmsg_ipv6),
							"%s:%d%s, ", listenptr->ip, listenptr->port,
							listenptr->options & LISTENER_TLS ? "(SSL/TLS)" : "");
					else
						snprintf(boundmsg_ipv4+strlen(boundmsg_ipv4), sizeof(boundmsg_ipv4)-strlen(boundmsg_ipv4),
							"%s:%d%s, ", listenptr->ip, listenptr->port,
							listenptr->options & LISTENER_TLS ? "(SSL/TLS)" : "");
				}
			}
		}

		/* NOTE: do not merge this with code above (nor in an else block),
		 * as add_listener() affects this flag.
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
		ConfigItem_offchans *of = safe_alloc(sizeof(ConfigItem_offchans));
		strlcpy(of->chname, cep->ce_varname, CHANNELLEN+1);
		for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
		{
			if (!strcmp(cepp->ce_varname, "topic"))
				safe_strdup(of->topic, cepp->ce_vardata);
		}
		AddListItem(of, conf_offchans);
	}
	return 0;
}

int	_test_offchans(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	ConfigEntry *cep, *cep2;

	if (!ce->ce_entries)
	{
		config_error("%s:%i: empty official-channels block",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}

	config_warn("set::official-channels is deprecated. It often does not do what you want. "
	            "You're better of creating a channel, setting all modes, topic, etc. to your liking "
	            "and then making the channel permanent (MODE #channel +P). "
	            "The channel will then be stored in a database to preserve it between restarts.");

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (strlen(cep->ce_varname) > CHANNELLEN)
		{
			config_error("%s:%i: official-channels: '%s' name too long (max %d characters).",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname, CHANNELLEN);
			errors++;
			continue;
		}
		if (!valid_channelname(cep->ce_varname))
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
				if (strlen(cep2->ce_vardata) > MAXTOPICLEN)
				{
					config_error("%s:%i: official-channels::%s: topic too long (max %d characters).",
						cep2->ce_fileptr->cf_filename, cep2->ce_varlinenum, cep->ce_varname, MAXTOPICLEN);
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
	RealCommand *cmptr;

	if ((cmptr = find_command(ce->ce_vardata, CMD_ALIAS)))
		CommandDelX(NULL, cmptr);
	if (find_command_simple(ce->ce_vardata))
	{
		config_warn("%s:%i: Alias '%s' would conflict with command (or server token) '%s', alias not added.",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata, ce->ce_vardata);
		return 0;
	}
	if ((alias = find_alias(ce->ce_vardata)))
		DelListItem(alias, conf_alias);
	alias = safe_alloc(sizeof(ConfigItem_alias));
	safe_strdup(alias->alias, ce->ce_vardata);
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "format")) {
			format = safe_alloc(sizeof(ConfigItem_alias_format));
			safe_strdup(format->format, cep->ce_vardata);
			format->expr = unreal_create_match(MATCH_PCRE_REGEX, cep->ce_vardata, NULL);
			if (!format->expr)
				abort(); /* Impossible due to _test_alias earlier */
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
				if (!strcmp(cepp->ce_varname, "nick") ||
				    !strcmp(cepp->ce_varname, "target") ||
				    !strcmp(cepp->ce_varname, "command")) {
					safe_strdup(format->nick, cepp->ce_vardata);
				}
				else if (!strcmp(cepp->ce_varname, "parameters")) {
					safe_strdup(format->parameters, cepp->ce_vardata);
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
			safe_strdup(alias->nick, cep->ce_vardata);
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
		safe_strdup(alias->nick, alias->alias);
	}
	AliasAdd(NULL, alias->alias, cmd_alias, 1, CMD_USER|CMD_ALIAS);

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
	else if (!find_command(ce->ce_vardata, CMD_ALIAS) && find_command(ce->ce_vardata, 0)) {
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
			Match *expr;
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

	if (!strcmp(ce->ce_vardata, "channel"))
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

int	_conf_deny_channel(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_deny_channel 	*deny = NULL;
	ConfigEntry 	    	*cep;

	deny = safe_alloc(sizeof(ConfigItem_deny_channel));
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "channel"))
		{
			safe_strdup(deny->channel, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "redirect"))
		{
			safe_strdup(deny->redirect, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "reason"))
		{
			safe_strdup(deny->reason, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "warn"))
		{
			deny->warn = config_checkval(cep->ce_vardata,CFG_YESNO);
		}
		else if (!strcmp(cep->ce_varname, "class"))
		{
			safe_strdup(deny->class, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "mask"))
		{
			unreal_add_masks(&deny->mask, cep);
		}
	}
	AddListItem(deny, conf_deny_channel);
	return 0;
}
int	_conf_deny_link(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_deny_link 	*deny = NULL;
	ConfigEntry 	    	*cep;

	deny = safe_alloc(sizeof(ConfigItem_deny_link));
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask"))
		{
			unreal_add_masks(&deny->mask, cep);
		}
		else if (!strcmp(cep->ce_varname, "rule"))
		{
			deny->rule = (char *)crule_parse(cep->ce_vardata);
			safe_strdup(deny->prettyrule, cep->ce_vardata);
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

	deny = safe_alloc(sizeof(ConfigItem_deny_version));
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask"))
		{
			safe_strdup(deny->mask, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "version"))
		{
			safe_strdup(deny->version, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "flags"))
		{
			safe_strdup(deny->flags, cep->ce_vardata);
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
	if (!strcmp(ce->ce_vardata, "channel"))
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
			else if (!strcmp(cep->ce_varname, "mask"))
			{
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
			if (!cep->ce_entries)
			{
				if (config_is_blankorempty(cep, "deny link"))
				{
					errors++;
					continue;
				}
				else if (!strcmp(cep->ce_varname, "mask"))
				{
					has_mask = 1;
				} else if (!strcmp(cep->ce_varname, "rule"))
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
			else
			{
				// Sections
				if (!strcmp(cep->ce_varname, "mask"))
				{
					if (cep->ce_vardata || cep->ce_entries)
						has_mask = 1;
				}
				else
				{
					config_error_unknown(cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum, "deny link", cep->ce_varname);
					errors++;
					continue;
				}
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

int _test_security_group(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	ConfigEntry *cep;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: security-group block needs a name, eg: security-group web-users {",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	} else {
		if (!strcasecmp(ce->ce_vardata, "unknown-users"))
		{
			config_error("%s:%i: The 'unknown-users' group is a special group that is the "
			             "inverse of 'known-users', you cannot create or adjust it in the "
			             "config file, as it is created automatically by UnrealIRCd.",
			             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			errors++;
			return errors;
		}
		if (!security_group_valid_name(ce->ce_vardata))
		{
			config_error("%s:%i: security-group block name '%s' contains invalid characters or is too long. "
			             "Only letters, numbers, underscore and hyphen are allowed.",
			             ce->ce_fileptr->cf_filename, ce->ce_varlinenum, ce->ce_vardata);
			errors++;
		}
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "webirc"))
		{
			CheckNull(cep);
		} else
		if (!strcmp(cep->ce_varname, "identified"))
		{
			CheckNull(cep);
		} else
		if (!strcmp(cep->ce_varname, "tls"))
		{
			CheckNull(cep);
		} else
		if (!strcmp(cep->ce_varname, "reputation-score"))
		{
			int v;
			CheckNull(cep);
			v = atoi(cep->ce_vardata);
			if ((v < 1) || (v > 10000))
			{
				config_error("%s:%i: security-group::reputation-score needs to be a value of 1-10000",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		} else
		if (!strcmp(cep->ce_varname, "include-mask"))
		{
		} else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"security-group", cep->ce_varname);
			errors++;
			continue;
		}
	}

	return errors;
}

int _conf_security_group(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	SecurityGroup *s = add_security_group(ce->ce_vardata, 1);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "webirc"))
			s->webirc = config_checkval(cep->ce_vardata, CFG_YESNO);
		else if (!strcmp(cep->ce_varname, "identified"))
			s->identified = config_checkval(cep->ce_vardata, CFG_YESNO);
		else if (!strcmp(cep->ce_varname, "tls"))
			s->tls = config_checkval(cep->ce_vardata, CFG_YESNO);
		else if (!strcmp(cep->ce_varname, "reputation-score"))
			s->reputation_score = atoi(cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "priority"))
		{
			s->priority = atoi(cep->ce_vardata);
			DelListItem(s, securitygroups);
			AddListItemPrio(s, securitygroups, s->priority);
		}
		else if (!strcmp(cep->ce_varname, "include-mask"))
		{
			unreal_add_masks(&s->include_mask, cep);
		}
	}
	return 1;
}

Secret *find_secret(char *secret_name)
{
	Secret *s;
	for (s = secrets; s; s = s->next)
	{
		if (!strcasecmp(s->name, secret_name))
			return s;
	}
	return NULL;
}

void free_secret_cache(SecretCache *c)
{
	unrealdb_free_config(c->config);
	safe_free(c);
}

void free_secret(Secret *s)
{
	SecretCache *c, *c_next;
	for (c = s->cache; c; c = c_next)
	{
		c_next = c->next;
		DelListItem(c, s->cache);
		free_secret_cache(c);
	}
	safe_free(s->name);
	safe_free_sensitive(s->password);
	safe_free(s);
}

char *_conf_secret_read_password_file(char *fname)
{
	char *pwd, *err;
	int fd, n;

#ifndef _WIN32
	fd = open(fname, O_RDONLY);
#else
	fd = open(fname, _O_RDONLY|_O_BINARY);
#endif
	if (fd < 0)
	{
		/* This should not happen, as we tested for file exists earlier.. */
		config_error("Could not open file '%s': %s", fname, strerror(errno));
		return NULL;
	}

	pwd = safe_alloc_sensitive(512);
	n = read(fd, pwd, 511);
	if (n <= 0)
	{
		close(fd);
		config_error("Could not read from file '%s': %s", fname, strerror(errno));
		safe_free_sensitive(pwd);
		return NULL;
	}
	close(fd);
	stripcrlf(pwd);
	sodium_stackzero(1024);
	if (!valid_secret_password(pwd, &err))
	{
		config_error("Key from file '%s' does not meet password complexity requirements: %s", fname, err);
		safe_free_sensitive(pwd);
		return NULL;
	}
	return pwd;
}

char *_conf_secret_read_prompt(char *blockname)
{
	char *pwd, *pwd_prompt;
	char buf[256];

#ifdef _WIN32
	/* FIXME: add windows support? should be possible in GUI no? */
	return NULL;
#else
	snprintf(buf, sizeof(buf), "Enter password for secret '%s': ", blockname);
	pwd_prompt = getpass(buf);
	if (pwd_prompt)
	{
		pwd = safe_alloc_sensitive(512);
		strlcpy(pwd, pwd_prompt, 512);
		memset(pwd_prompt, 0, strlen(pwd_prompt)); // zero password out
		sodium_stackzero(1024);
		return pwd;
	}
	return NULL;
#endif
}

int _test_secret(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	int has_password = 0, has_password_file = 0, has_password_prompt = 0;
	ConfigEntry *cep;
	char *err;
	Secret *existing;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: secret block needs a name, eg: secret xyz {",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
		return errors; /* need to return here since we dereference ce->ce_vardata later.. */
	} else {
		if (!security_group_valid_name(ce->ce_vardata))
		{
			config_error("%s:%i: secret block name '%s' contains invalid characters or is too long. "
			             "Only letters, numbers, underscore and hyphen are allowed.",
			             ce->ce_fileptr->cf_filename, ce->ce_varlinenum, ce->ce_vardata);
			errors++;
		}
	}

	existing = find_secret(ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "password"))
		{
			int n;
			has_password = 1;
			CheckNull(cep);
			if (cep->ce_entries ||
			    (((n = Auth_AutoDetectHashType(cep->ce_vardata))) && ((n == AUTHTYPE_BCRYPT) || (n == AUTHTYPE_ARGON2))))
			{
				config_error("%s:%d: you cannot use hashed passwords here, see "
				             "https://www.unrealircd.org/docs/Secret_block#secret-plaintext",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				continue;
			}
			if (!valid_secret_password(cep->ce_vardata, &err))
			{
				config_error("%s:%d: secret::password does not meet password complexity requirements: %s",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum, err);
				errors++;
			}
		} else
		if (!strcmp(cep->ce_varname, "password-file"))
		{
			char *str;
			has_password_file = 1;
			CheckNull(cep);
			convert_to_absolute_path(&cep->ce_vardata, CONFDIR);
			if (!file_exists(cep->ce_vardata) && existing && existing->password)
			{
				/* Silently ignore the case where a secret block already
				 * has the password read and now the file is no longer available.
				 * This so secret::password-file can be used only to boot
				 * and then the media (eg: USB stick) can be pulled.
				 */
			} else
			{
				str = _conf_secret_read_password_file(cep->ce_vardata);
				if (!str)
				{
					config_error("%s:%d: secret::password-file: error reading password from file, see error from above.",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					errors++;
				}
				safe_free_sensitive(str);
			}
		} else
		if (!strcmp(cep->ce_varname, "password-prompt"))
		{
#ifdef _WIN32
			config_error("%s:%d: secret::password-prompt is not implemented in Windows at the moment, sorry!",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			config_error("Choose a different method to enter passwords or use *NIX");
			errors++;
			return errors;
#endif
			has_password_prompt = 1;
			if (loop.ircd_booted && !find_secret(ce->ce_vardata))
			{
				config_error("%s:%d: you cannot add a new secret { } block that uses password-prompt and then /REHASH. "
				             "With 'password-prompt' you can only add such a password on boot.",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				config_error("Either use a different method to enter passwords or restart the IRCd on the console.");
				errors++;
			}
			if (!loop.ircd_booted && !running_interactively())
			{
				config_error("ERROR: IRCd is not running interactively, but via a cron job or something similar.");
				config_error("%s:%d: unable to prompt for password since IRCd is not started in a terminal",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				config_error("Either use a different method to enter passwords or start the IRCd in a terminal/SSH/..");
			}
		} else
		if (!strcmp(cep->ce_varname, "password-url"))
		{
			config_error("%s:%d: secret::password-url is not supported yet in this UnrealIRCd version.",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
		} else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"secret", cep->ce_varname);
			errors++;
			continue;
		}
		if (cep->ce_entries)
		{
			config_error("%s:%d: secret::%s does not support sub-options (%s)",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				cep->ce_varname, cep->ce_entries->ce_varname);
			errors++;
		}
	}

	if (!has_password && !has_password_file && !has_password_prompt)
	{
		config_error("%s:%d: secret { } block must contain 1 of: password OR password-file OR password-prompt",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}

	return errors;
}

/* NOTE: contrary to all other _conf* stuff, this one actually runs during config_test,
 * so during the early CONFIG TEST stage rather than CONFIG RUN.
 * This so all secret { } block configuration is available already during TEST/POSTTEST
 * stage for modules, so they can check if the password is correct or not.
 */
int _conf_secret(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	Secret *s;
	Secret *existing = find_secret(ce->ce_vardata);

	s = safe_alloc(sizeof(Secret));
	safe_strdup(s->name, ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "password"))
		{
			safe_strdup_sensitive(s->password, cep->ce_vardata);
			destroy_string(cep->ce_vardata); /* destroy the original */
		} else
		if (!strcmp(cep->ce_varname, "password-file"))
		{
			if (!file_exists(cep->ce_vardata) && existing && existing->password)
			{
				/* Silently ignore the case where a secret block already
				 * has the password read and now the file is no longer available.
				 * This so secret::password-file can be used only to boot
				 * and then the media (eg: USB stick) can be pulled.
				 */
			} else
			{
				s->password = _conf_secret_read_password_file(cep->ce_vardata);
			}
		} else
		if (!strcmp(cep->ce_varname, "password-prompt"))
		{
			if (!loop.ircd_booted && running_interactively())
			{
				s->password = _conf_secret_read_prompt(ce->ce_vardata);
				if (!s->password || !valid_secret_password(s->password, NULL))
				{
					config_error("Invalid password entered on console (does not meet complexity requirements)");
					/* This cannot be the correct password, so exit */
					exit(-1);
				}
			}
		}
	}

	/* This may happen if we run twice, due to destroy_string() earlier: */
	if (BadPtr(s->password))
	{
		free_secret(s);
		return 1;
	}

	/* If there is an existing secret { } block with this name in memory
	 * and it has a different password, then free that secret block
	 */
	if (existing)
	{
		if (!strcmp(s->password, existing->password))
		{
			free_secret(s);
			return 1;
		}
		/* passwords differ, so free the old existing one,
		 * including purging the cache for it.
		 */
		DelListItem(existing, secrets);
		free_secret(existing);
	}
	AddListItem(s, secrets);
	return 1;
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
	for (inc = conf_include; inc; inc = inc->next)
	{
		if ( inc_key != (void *)inc )
			continue;
		if (!(inc->flag.type & INCLUDE_REMOTE))
			continue;
		if (inc->flag.type & INCLUDE_NOTLOADED)
			continue;
		if (strcasecmp(url, inc->url))
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
		safe_free(urlfile);

		if (cached)
		{
			unreal_copyfileex(inc->file, tmp, 1);
			unreal_copyfileex(inc->file, unreal_mkcache(url), 0);
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
			unreal_copyfileex(file, unreal_mkcache(url), 0);
		}
	}
	for (inc = conf_include; inc; inc = inc->next)
	{
		if (inc->flag.type & INCLUDE_DLQUEUED)
			return;
	}
	rehash_internal(loop.rehash_save_client, loop.rehash_save_sig);
}
#endif

int     rehash(Client *client, int sig)
{
#ifdef USE_LIBCURL
	ConfigItem_include *inc;
	char found_remote = 0;
	if (loop.ircd_rehashing)
	{
		if (!sig)
			sendnotice(client, "A rehash is already in progress");
		return 0;
	}

	/* Log who or what did the rehash: */
	if (sig)
	{
		ircd_log(LOG_ERROR, "Rehashing configuration file (SIGHUP signal received)");
	} else
	if (client && client->user)
	{
		ircd_log(LOG_ERROR, "Rehashing configuration file (requested by %s!%s@%s)",
			client->name, client->user->username, client->user->realhost);
	} else
	if (client)
	{
		ircd_log(LOG_ERROR, "Rehashing configuration file (requested by %s)",
			client->name);
	}

	loop.ircd_rehashing = 1;
	loop.rehash_save_client = client;
	loop.rehash_save_sig = sig;
	for (inc = conf_include; inc; inc = inc->next)
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
		return rehash_internal(client, sig);
	return 0;
#else
	loop.ircd_rehashing = 1;
	return rehash_internal(client, sig);
#endif
}

int	rehash_internal(Client *client, int sig)
{
	if (sig == 1)
		sendto_ops("Got signal SIGHUP, reloading %s file", configfile);
	loop.ircd_rehashing = 1; /* double checking.. */
	if (init_conf(configfile, 1) == 0)
		run_configuration();
	if (sig == 1)
		reread_motdsandrules();
	unload_all_unused_snomasks();
	unload_all_unused_umodes();
	unload_all_unused_extcmodes();
	unload_all_unused_caps();
	unload_all_unused_history_backends();
	// unload_all_unused_moddata(); -- this will crash
	extcmodes_check_for_changes();
	umodes_check_for_changes();
	charsys_check_for_changes();
	loop.ircd_rehashing = 0;
	remote_rehash_client = NULL;
	return 1;
}

void link_cleanup(ConfigItem_link *link_ptr)
{
	safe_free(link_ptr->servername);
	unreal_delete_masks(link_ptr->incoming.mask);
	Auth_FreeAuthConfig(link_ptr->auth);
	safe_free(link_ptr->outgoing.bind_ip);
	safe_free(link_ptr->outgoing.hostname);
	safe_free(link_ptr->hub);
	safe_free(link_ptr->leaf);
	if (link_ptr->ssl_ctx)
	{
		SSL_CTX_free(link_ptr->ssl_ctx);
		link_ptr->ssl_ctx = NULL;
	}
	if (link_ptr->tls_options)
	{
		free_tls_options(link_ptr->tls_options);
		link_ptr->tls_options = NULL;
    }
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
	safe_free(link_ptr);
}

void delete_classblock(ConfigItem_class *class_ptr)
{
	Debug((DEBUG_ERROR, "delete_classblock: deleting %s, clients=%d, xrefcount=%d",
		class_ptr->name, class_ptr->clients, class_ptr->xrefcount));
	safe_free(class_ptr->name);
	DelListItem(class_ptr, conf_class);
	safe_free(class_ptr);
}

void	listen_cleanup()
{
	int	i = 0;
	ConfigItem_listen *listen_ptr, *next;

	for (listen_ptr = conf_listen; listen_ptr; listen_ptr = next)
	{
		next = listen_ptr->next;
		if (listen_ptr->flag.temporary && !listen_ptr->clients)
		{
			safe_free(listen_ptr->ip);
			free_tls_options(listen_ptr->tls_options);
			DelListItem(listen_ptr, conf_listen);
			safe_free(listen_ptr);
			i++;
		}
	}

	if (i)
		close_unbound_listeners();
}

#ifdef USE_LIBCURL
char *find_remote_include(char *url, char **errorbuf)
{
	ConfigItem_include *inc;

	for (inc = conf_include; inc; inc = inc->next)
	{
		if (!(inc->flag.type & INCLUDE_NOTLOADED))
			continue;
		if (!(inc->flag.type & INCLUDE_REMOTE))
			continue;
		if (!strcasecmp(url, inc->url))
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

	for (inc = conf_include; inc; inc = inc->next)
	{
		if ((inc->flag.type & INCLUDE_NOTLOADED))
			continue;
		if (!(inc->flag.type & INCLUDE_REMOTE))
			continue;
		if (!strcasecmp(url, inc->url))
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
			config_status("Downloading %s", displayurl(url));
		file = download_file(url, &error);
		if (!file)
		{
			if (has_cached_version(url))
			{
				config_warn("%s:%i: include: error downloading '%s': %s -- using cached version instead.",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					displayurl(url), error);
				safe_strdup(file, unreal_mkcache(url));
				/* Let it pass to load_conf()... */
			} else {
				config_error("%s:%i: include: error downloading '%s': %s",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					 displayurl(url), error);
				return -1;
			}
		} else {
			unreal_copyfileex(file, unreal_mkcache(url), 0);
		}
		add_remote_include(file, url, 0, NULL, ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		ret = load_conf(file, url);
		safe_free(file);
		return ret;
	}
	else
	{
		if (errorbuf)
		{
			if (has_cached_version(url))
			{
				config_warn("%s:%i: include: error downloading '%s': %s -- using cached version instead.",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					displayurl(url), errorbuf);
				/* Let it pass to load_conf()... */
				safe_strdup(file, unreal_mkcache(url));
			} else {
				config_error("%s:%i: include: error downloading '%s': %s",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					displayurl(url), errorbuf);
				return -1;
			}
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

	inc = safe_alloc(sizeof(ConfigItem_include));
	safe_strdup(inc->file, file);
	inc->flag.type = INCLUDE_NOTLOADED;
	safe_strdup(inc->included_from, included_from);
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

	/* we rely on safe_alloc() zeroing the ConfigItem_include */
	inc = safe_alloc(sizeof(ConfigItem_include));
	if (included_from)
	{
		safe_strdup(inc->included_from, included_from);
		inc->included_from_line = included_from_line;
	}
	safe_strdup(inc->url, url);

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
		safe_strdup(inc->file, file);
	inc->flag.type |= flags;

	if (errorbuf)
		safe_strdup(inc->errorbuf, errorbuf);
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
		next = inc->next;
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
				safe_free(inc->url);
				safe_free(inc->errorbuf);
			}
#endif
			safe_free(inc->file);
			safe_free(inc->included_from);
			DelListItem(inc, conf_include);
			safe_free(inc);
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
		next = inc->next;
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
				safe_free(inc->url);
				safe_free(inc->errorbuf);
			}
#endif
			safe_free(inc->file);
			safe_free(inc->included_from);
			DelListItem(inc, conf_include);
			safe_free(inc);
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
	for (inc = conf_include; inc; inc = inc->next)
		inc->flag.type &= ~INCLUDE_NOTLOADED;
}

int tls_tests(void)
{
	if (have_tls_listeners == 0)
	{
		config_error("Your server is not listening on any SSL/TLS ports.");
		config_status("Add this to your unrealircd.conf: listen { ip %s; port 6697; options { tls; }; };",
		            port_6667_ip ? port_6667_ip : "*");
		config_status("See https://www.unrealircd.org/docs/FAQ#Your_server_is_not_listening_on_any_SSL_ports");
		return 0;
	}

	return 1;
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

char *link_generator_spkifp(TLSOptions *tlsoptions)
{
	SSL_CTX *ctx;
	SSL *ssl;
	X509 *cert;

	ctx = init_ctx(tlsoptions, 1);
	if (!ctx)
		exit(1);
	ssl = SSL_new(ctx);
	if (!ssl)
		exit(1);
	cert = SSL_get_certificate(ssl);
	return spki_fingerprint_ex(cert);
}

void link_generator(void)
{
	ConfigItem_listen *lstn;
	TLSOptions *tlsopt = iConf.tls_options; /* never null */
	int port = 0;
	char *ip = NULL;
	char *spkifp;

	for (lstn = conf_listen; lstn; lstn = lstn->next)
	{
		if ((lstn->options & LISTENER_SERVERSONLY) &&
		    (lstn->options & LISTENER_TLS))
		{
			if (lstn->tls_options)
				tlsopt = lstn->tls_options;
			port = lstn->port;
			if (strcmp(lstn->ip, "*"))
				ip = lstn->ip;
			/* else NULL */
			break;
		}
	}

	if (!port)
	{
		printf("You don't have any listen { } blocks that are serversonly (and have tls enabled).\n");
		printf("It is recommended to have at least one. Add this to your configuration file:\n");
		printf("listen { ip *; port 6900; options { tls; serversonly; }; };\n");
		exit(1);
	}

	spkifp = link_generator_spkifp(tlsopt);
	if (!spkifp)
	{
		printf("Could not calculate spkifp. Maybe you have uncommon SSL/TLS options set? Odd...\n");
		exit(1);
	}

	printf("\n");
	printf("Add the following link block to the unrealircd.conf on the OTHER side of the link\n");
	printf("(so NOT in the unrealircd.conf on THIS machine). Here it is, just copy-paste:\n");
	printf("################################################################################\n");
	printf("link %s {\n"
	       "    incoming {\n"
	       "        mask *;\n"
	       "    }\n"
	       "    outgoing {\n"
	       "        hostname %s;\n"
	       "        port %d;\n"
	       "        options { tls; autoconnect; }\n"
	       "    }\n"
	       "    password \"%s\" { spkifp; }\n"
	       "    class servers;\n"
	       "}\n",
	       conf_me->name,
	       ip ? ip : conf_me->name,
	       port,
	       spkifp);
	printf("################################################################################\n");
	exit(0);
}
