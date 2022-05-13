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
	{ "log",		config_run_log,		config_test_log	},
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

void	port_range(const char *string, int *start, int *end);
long	config_checkval(const char *value, unsigned short flags);

/*
 * Parser
*/

ConfigFile		*config_load(const char *filename, const char *displayname);
void			config_free(ConfigFile *cfptr);
ConfigFile		*config_parse_with_offset(const char *filename, char *confdata, unsigned int line_offset);
ConfigFile	 	*config_parse(const char *filename, char *confdata);
ConfigEntry		*config_find_entry(ConfigEntry *ce, const char *name);

extern void add_entropy_configfile(struct stat *st, const char *buf);
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
int			config_read_file(const char *filename, const char *display_name);
void			config_rehash(void);
int			config_run_blocks(void);
int	config_test_blocks();

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
ConfigItem_vhost	*conf_vhost = NULL;
ConfigItem_link		*conf_link = NULL;
ConfigItem_ban		*conf_ban = NULL;
ConfigItem_deny_channel *conf_deny_channel = NULL;
ConfigItem_allow_channel *conf_allow_channel = NULL;
ConfigItem_deny_link	*conf_deny_link = NULL;
ConfigItem_deny_version *conf_deny_version = NULL;
ConfigItem_alias	*conf_alias = NULL;
ConfigResource	*config_resources = NULL;
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

int need_operclass_permissions_upgrade = 0;
int invalid_snomasks_encountered = 0;
int have_tls_listeners = 0;
char *port_6667_ip = NULL;

int add_config_resource(const char *resource, int type, ConfigEntry *ce);
void resource_download_complete(const char *url, const char *file, const char *errorbuf, int cached, void *rs_key);
void free_all_config_resources(void);
int rehash_internal(Client *client);
int is_blacklisted_module(const char *name);

/** Return the printable string of a 'cep' location, such as set::something::xyz */
const char *config_var(ConfigEntry *cep)
{
	static char buf[256];
	ConfigEntry *e;
	char *elem[16];
	int numel = 0, i;

	if (!cep)
		return "";

	buf[0] = '\0';

	/* First, walk back to the top */
	for (e = cep; e; e = e->parent)
	{
		elem[numel++] = e->name;
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

void port_range(const char *string, int *start, int *end)
{
	char buf[256];
	char *c;
	strlcpy(buf, string, sizeof(buf));
	c = strchr(buf, '-');
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
int config_parse_flood(const char *orig, int *times, int *period)
{
	char buf[256];
	char *x;

	strlcpy(buf, orig, sizeof(buf));

	*times = *period = 0;
	x = strchr(buf, ':');
	/* 'blah', ':blah', '1:' */
	if (!x || (x == buf) || (*(x+1) == '\0'))
		return 0;

	*x = '\0';
	*times = atoi(buf);
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

long config_checkval(const char *orig, unsigned short flags)
{
	char *value;
	char *text;
	long ret = 0;

	/* Handle empty strings early, since we use +1 later in the code etc. */
	if (BadPtr(orig))
		return 0;

	value = raw_strdup(orig);
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
	memset(store, 0, sizeof(struct ChMode));
}

/* Set configuration, used for set::modes-on-join */
void conf_channelmodes(const char *modes, struct ChMode *store)
{
	Cmode *cm;
	const char *m;
	char *params = strchr(modes, ' ');
	char *parambuf = NULL;
	const char *param = NULL;
	const char *param_in;
	char *save = NULL;
	int found;

	/* Free existing parameters first (no inheritance) */
	free_conf_channelmodes(store);

	if (params)
	{
		params++;
		safe_strdup(parambuf, params);
		param = strtoken(&save, parambuf, " ");
	}

	for (m = modes; *m && *m != ' '; m++)
	{
		if (*m == '+')
			continue;

		if (*m == '-')
		{
			/* When a channel is created it has no modes, so just ignore if the
			 * user asks us to unset anything -- codemastr
			 */
			while (*m && *m != '+')
				m++;
			continue;
		}

		found = 0;
		for (cm=channelmodes; cm; cm = cm->next)
		{
			if (!(cm->letter))
				continue;
			if (*m == cm->letter)
			{
				found = 1;
				if (cm->paracount)
				{
					if (!param)
					{
						config_warn("set::modes-on-join '%s'. Parameter missing for mode %c.", modes, *m);
						break;
					}
					param_in = param; /* save it */
					param = cm->conv_param(param, NULL, NULL);
					if (!param)
					{
						config_warn("set::modes-on-join '%s'. Parameter for mode %c is invalid (%s).", modes, *m, param_in);
						break; /* invalid parameter fmt, do not set mode. */
					}
					safe_strdup(store->extparams[cm->letter], param);
					/* Get next parameter */
					param = strtoken(&save, NULL, " ");
				}
				store->extmodes |= cm->mode;
				break;
			}
		}
		if (!found)
			config_warn("set::modes-on-join '%s'. Channel mode %c not found.", modes, *m);
	}
	safe_free(parambuf);
}

void chmode_str(struct ChMode *modes, char *mbuf, char *pbuf, size_t mbuf_size, size_t pbuf_size)
{
	Cmode *cm;

	if (!(mbuf_size && pbuf_size))
		return;

	*pbuf = 0;
	*mbuf++ = '+';

	for (cm=channelmodes; cm; cm = cm->next)
	{
		if (!(cm->letter))
			continue;

		if (modes->extmodes & cm->mode)
		{
			if (mbuf_size)
			{
				*mbuf++ = cm->letter;
				if (!--mbuf_size)
				{
					*--mbuf=0;
					break;
				}
			}
			if (cm->paracount)
			{
				strlcat(pbuf, modes->extparams[cm->letter], pbuf_size);
				strlcat(pbuf, " ", pbuf_size);
			}
		}
	}
	*mbuf=0;
}

const char *channellevel_to_string(const char *s)
{
	/* Requested at http://bugs.unrealircd.org/view.php?id=3852 */
	if (!strcmp(s, "none"))
		return "";
	if (!strcmp(s, "voice"))
		return "v";
	if (!strcmp(s, "halfop"))
		return "h";
	if (!strcmp(s, "op") || !strcmp(s, "chanop"))
		return "o";
	if (!strcmp(s, "protect") || !strcmp(s, "chanprot") || !strcmp(s, "chanadmin") || !strcmp(s, "admin"))
		return "a";
	if (!strcmp(s, "owner") || !strcmp(s, "chanowner"))
		return "q";

	return NULL; /* unknown or unsupported */
}

Policy policy_strtoval(const char *s)
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

const char *policy_valtostr(Policy policy)
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

AllowedChannelChars allowed_channelchars_strtoval(const char *str)
{
	if (!strcmp(str, "ascii"))
		return ALLOWED_CHANNELCHARS_ASCII;
	else if (!strcmp(str, "utf8"))
		return ALLOWED_CHANNELCHARS_UTF8;
	else if (!strcmp(str, "any"))
		return ALLOWED_CHANNELCHARS_ANY;
	return 0;
}

const char *allowed_channelchars_valtostr(AllowedChannelChars v)
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
BanTarget ban_target_strtoval(const char *str)
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
const char *ban_target_valtostr(BanTarget v)
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

HideIdleTimePolicy hideidletime_strtoval(const char *str)
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

const char *hideidletime_valtostr(HideIdleTimePolicy v)
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

ConfigFile *config_load(const char *filename, const char *displayname)
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
		nptr = cfptr->next;
		if (cfptr->items)
			config_entry_free_all(cfptr->items);
		safe_free(cfptr->filename);
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
int unreal_add_quotes_r(const char *i, char *o, size_t len)
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
const char *unreal_add_quotes(const char *str)
{
	static char qbuf[2048];
	
	*qbuf = '\0';
	unreal_add_quotes_r(str, qbuf, sizeof(qbuf));
	return qbuf;
}

ConfigFile *config_parse(const char *filename, char *confdata)
{
	return config_parse_with_offset(filename, confdata, 0);
}

/* This is the internal parser, made by Chris Behrens & Fred Jacobs <2005.
 * Enhanced (or mutilated) by Bram Matthys over the years (2015-2019).
 */
ConfigFile *config_parse_with_offset(const char *filename, char *confdata, unsigned int line_offset)
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
	safe_strdup(curcf->filename, filename);
	lastce = &(curcf->items);
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
				lastce = &(curce->next);
				curce->file_position_end = (ptr - confdata);
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
				else if (curce->items)
				{
					config_error("%s:%i: New section start but previous section did not end properly. "
					             "Check line %d and the line(s) before, you are likely missing a '};' there.\n",
							filename, linenumber, linenumber);
					errors++;
					continue;
				}
				curce->section_linenumber = linenumber;
				lastce = &(curce->items);
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
				cursection->file_position_end = (ptr - confdata);
				cursection = cursection->parent;
				if (!cursection)
					lastce = &(curcf->items);
				else
					lastce = &(cursection->items);
				for(;*lastce;lastce = &((*lastce)->next))
					continue;
				if (*(ptr+1) != ';')
				{
					/* Simulate closing ; so you can get away with } instead of ugly }; */
					*lastce = curce;
					lastce = &(curce->next);
					curce->file_position_end = (ptr - confdata);
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
			case '\'':
				if (curce)
					curce->escaped = 1;
				/* fallthrough */
			case '\"':
				if (curce && curce->line_number != linenumber && cursection)
				{
					config_error("%s:%i: Missing semicolon (';') at end of line. "
					             "Line %d must end with a ; character\n",
						filename, curce->line_number, curce->line_number);
					errors++;

					*lastce = curce;
					lastce = &(curce->next);
					curce->file_position_end = (ptr - confdata);
					curce = NULL;
				}

				start = ++ptr;
				for(;*ptr;ptr++)
				{
					if (*ptr == '\\')
					{
						if (strchr("\\\"'", ptr[1]))
						{
							/* \\ or \" in config file (escaped) */
							ptr++; /* skip */
							continue;
						}
					}
					else if (*ptr == '\n')
						break;
					else if (curce && curce->escaped && (*ptr == '\''))
						break;
					else if ((!curce || !curce->escaped) && (*ptr == '"'))
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
					if (curce->value)
					{
						config_error("%s:%i: Extra data detected. Perhaps missing a ';' or one too many?\n",
							filename, linenumber);
						errors++;
					}
					else
					{
						safe_strldup(curce->value, start, ptr-start+1);
						preprocessor_replace_defines(&curce->value, curce);
						unreal_del_quotes(curce->value);
					}
				}
				else
				{
					curce = safe_alloc(sizeof(ConfigEntry));
					curce->line_number = linenumber;
					curce->file = curcf;
					curce->parent = cursection;
					curce->file_position_start = (start - confdata);
					safe_strldup(curce->name, start, ptr-start+1);
					preprocessor_replace_defines(&curce->name, curce);
					unreal_del_quotes(curce->name);
					preprocessor_cc_duplicate_list(cc_list, &curce->conditional_config);
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
							filename, curce->line_number);
					else if (cursection)
						config_error("%s: End of file reached but the section which starts at line %i did never end properly. "
									 "Perhaps a missing }; ?\n",
								filename, cursection->section_linenumber);
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
					if (curce->value)
					{
						config_error("%s:%i: Extra data detected. Check for a missing ; character at or around line %d\n",
							filename, linenumber, linenumber-1);
						errors++;
					}
					else
					{
						safe_strldup(curce->value, start, ptr-start+1);
						preprocessor_replace_defines(&curce->value, curce);
					}
				}
				else
				{
					curce = safe_alloc(sizeof(ConfigEntry));
					memset(curce, 0, sizeof(ConfigEntry));
					curce->line_number = linenumber;
					curce->file = curcf;
					curce->parent = cursection;
					curce->file_position_start = (start - confdata);
					safe_strldup(curce->name, start, ptr-start+1);
					preprocessor_replace_defines(&curce->name, curce);
					if (curce->conditional_config)
						abort();
					preprocessor_cc_duplicate_list(cc_list, &curce->conditional_config);
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
			filename, curce->line_number);
		errors++;
		config_entry_free_all(curce);
	}
	else if (cursection)
	{
		config_error("%s: End of file reached but the section which starts at line %i did never end properly. "
		             "Perhaps a missing }; ?\n",
				filename, cursection->section_linenumber);
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
		nptr = ce->next;
		if (ce->items)
			config_entry_free_all(ce->items);
		safe_free(ce->name);
		safe_free(ce->value);
		if (ce->conditional_config)
			preprocessor_cc_free_list(ce->conditional_config);
		safe_free(ce);
	}
}

/** Free a specific ConfigEntry struct (and it's children).
 * Caller must ensure that the entry is not in the linked list anymore.
 */
void config_entry_free(ConfigEntry *ce)
{
	if (ce->items)
		config_entry_free_all(ce->items);
	safe_free(ce->name);
	safe_free(ce->value);
	if (ce->conditional_config)
		preprocessor_cc_free_list(ce->conditional_config);
	safe_free(ce);
}

ConfigEntry *config_find_entry(ConfigEntry *ce, const char *name)
{
	ConfigEntry *cep;

	for (cep = ce; cep; cep = cep->next)
		if (cep->name && !strcmp(cep->name, name))
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
	unreal_log_raw(ULOG_ERROR, "config", "CONFIG_ERROR_GENERIC", NULL, buffer);
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
	unreal_log_raw(ULOG_INFO, "config", "CONFIG_INFO_GENERIC", NULL, buffer);
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
	unreal_log_raw(ULOG_WARNING, "config", "CONFIG_WARNING_GENERIC", NULL, buffer);
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

	if (!cep->value)
	{
		if (fatal)
			config_error("%s:%i: %s: <no file specified>: no file specified",
				     cep->file->filename,
				     cep->line_number,
				     entry);
		else

			config_warn("%s:%i: %s: <no file specified>: no file specified",
				    cep->file->filename,
				    cep->line_number,
				    entry);
		return 1;
	}

	/* There's not much checking that can be done for asynchronously downloaded files */
	if (url_is_valid(cep->value))
	{
		if (allow_url)
			return 0;

		/* but we can check if a URL is used wrongly :-) */
		config_warn("%s:%i: %s: %s: URL used where not allowed",
			    cep->file->filename,
			    cep->line_number,
			    entry, cep->value);
		if (fatal)
			return 1;
		else
			return 0;
	}

	/*
	 * Make sure that files are created with the correct mode. This is
	 * because we don't feel like unlink()ing them...which would require
	 * stat()ing them to make sure that we don't delete existing ones
	 * and that we deal with all of the bugs that come with complexity.
	 * The only files we may be creating are the tunefile and pidfile so far.
	 */
	if (flags & O_CREAT)
		fd = open(cep->value, flags, mode);
	else
		fd = open(cep->value, flags);
	if (fd == -1)
	{
		if (fatal)
			config_error("%s:%i: %s: %s: %s",
				     cep->file->filename,
				     cep->line_number,
				     entry,
				     cep->value,
				     strerror(errno));
		else
			config_warn("%s:%i: %s: %s: %s",
				     cep->file->filename,
				     cep->line_number,
				     entry,
				     cep->value,
				     strerror(errno));
		return 1;
	}
	close(fd);
	return 0;
}

int config_is_blankorempty(ConfigEntry *cep, const char *block)
{
	if (!cep->value)
	{
		config_error_empty(cep->file->filename, cep->line_number, block,
			cep->name);
		return 1;
	}
	return 0;
}

ConfigCommand *config_binary_search(const char *cmd) {
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

void free_iConf(Configuration *i)
{
	FloodSettings *f, *f_next;

	safe_free(i->link_bindip);
	safe_free(i->kline_address);
	safe_free(i->gline_address);
	safe_free(i->oper_snomask);
	safe_free(i->auto_join_chans);
	safe_free(i->oper_auto_join_chans);
	safe_free(i->allow_user_stats);
	// allow_user_stats_ext is freed elsewhere
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
	safe_free(i->level_on_join);
	safe_free(i->spamfilter_ban_reason);
	safe_free(i->spamfilter_virus_help_channel);
	// spamexcept is freed elsewhere
	safe_free(i->spamexcept_line);
	safe_free(i->reject_message_too_many_connections);
	safe_free(i->reject_message_server_full);
	safe_free(i->reject_message_unauthorized);
	safe_free(i->reject_message_kline);
	safe_free(i->reject_message_gline);
	safe_free(i->network_name);
	safe_free(i->network_name_005);
	safe_free(i->default_server);
	safe_free(i->services_name);
	safe_free(i->cloak_prefix);
	safe_free(i->prefix_quit);
	safe_free(i->helpchan);
	safe_free(i->stats_server);
	safe_free(i->sasl_server);
	// anti-flood:
	for (f = i->floodsettings; f; f = f_next)
	{
		f_next = f->next;
		free_floodsettings(f);
	}
	i->floodsettings = NULL;
}

void config_setdefaultsettings(Configuration *i)
{
	char tmp[512];

	safe_strdup(i->oper_snomask, OPER_SNOMASKS);
	i->server_notice_colors = 1;
	i->server_notice_show_event = 1;
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
	i->conn_modes = set_usermode("+ixw");
	i->check_target_nick_bans = 1;
	i->maxbans = 60;
	i->maxbanlength = 2048;
	safe_strdup(i->level_on_join, "o");
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
	safe_strdup(i->cloak_prefix, "Clk");
	if (!ipv6_capable())
		DISABLE_IPV6 = 1;
	safe_strdup(i->prefix_quit, "Quit");
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
	config_parse_flood_generic("3:90", i, "known-users", FLD_VHOST); /* MODE -x flood protection: max 3 per 90s */
	config_parse_flood_generic("4:120", i, "known-users", FLD_AWAY); /* AWAY flood protection: max 4 per 120s */
	config_parse_flood_generic("4:60", i, "known-users", FLD_INVITE); /* INVITE flood protection: max 4 per 60s */
	config_parse_flood_generic("4:120", i, "known-users", FLD_KNOCK); /* KNOCK protection: max 4 per 120s */
	config_parse_flood_generic("10:15", i, "known-users", FLD_CONVERSATIONS); /* 10 users, new user every 15s */
	config_parse_flood_generic("180:750", i, "known-users", FLD_LAG_PENALTY); /* 180 bytes / 750 msec */
	/* - unknown-users */
	config_parse_flood_generic("2:60", i, "unknown-users", FLD_NICK); /* NICK flood protection: max 2 per 60s */
	config_parse_flood_generic("2:90", i, "unknown-users", FLD_JOIN); /* JOIN flood protection: max 2 per 90s */
	config_parse_flood_generic("2:90", i, "unknown-users", FLD_VHOST); /* MODE -x flood protection: max 2 per 90s */
	config_parse_flood_generic("4:120", i, "unknown-users", FLD_AWAY); /* AWAY flood protection: max 4 per 120s */
	config_parse_flood_generic("2:60", i, "unknown-users", FLD_INVITE); /* INVITE flood protection: max 2 per 60s */
	config_parse_flood_generic("2:120", i, "unknown-users", FLD_KNOCK); /* KNOCK protection: max 2 per 120s */
	config_parse_flood_generic("4:15", i, "unknown-users", FLD_CONVERSATIONS); /* 4 users, new user every 15s */
	config_parse_flood_generic("90:1000", i, "unknown-users", FLD_LAG_PENALTY); /* 90 bytes / 1000 msec */

	/* TLS options */
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

	i->named_extended_bans = 1;
}

/** Similar to config_setdefaultsettings but this one is applied *AFTER*
 * the entire configuration has been ran (sometimes this is the only way it can be done..).
 * NOTE: iConf is thus already populated with (non-default) values. Only overwrite if necessary!
 */
void postconf_defaults(void)
{
	TKL *tk;
	char *encoded;

	if (!iConf.modes_on_join_set)
	{
		/* We could not do this in config_setdefaultsettings()
		 * because the channel mode modules were not initialized yet.
		 */
		conf_channelmodes("+nt", &iConf.modes_on_join);
	}
	if (!iConf.plaintext_policy_user_message)
	{
		/* The message depends on whether it's reject or warn.. */
		if (iConf.plaintext_policy_user == POLICY_DENY)
			addmultiline(&iConf.plaintext_policy_user_message, "Insecure connection. Please reconnect using TLS.");
		else if (iConf.plaintext_policy_user == POLICY_WARN)
			addmultiline(&iConf.plaintext_policy_user_message, "WARNING: Insecure connection. Please consider using TLS.");
	}

	if (!iConf.plaintext_policy_oper_message)
	{
		/* The message depends on whether it's reject or warn.. */
		if (iConf.plaintext_policy_oper == POLICY_DENY)
		{
			addmultiline(&iConf.plaintext_policy_oper_message, "You need to use a secure connection (TLS) in order to /OPER.");
			addmultiline(&iConf.plaintext_policy_oper_message, "See https://www.unrealircd.org/docs/FAQ#oper-requires-tls");
		}
		else if (iConf.plaintext_policy_oper == POLICY_WARN)
			addmultiline(&iConf.plaintext_policy_oper_message, "WARNING: You /OPER'ed up from an insecure connection. Please consider using TLS.");
	}

	if (!iConf.outdated_tls_policy_user_message)
	{
		/* The message depends on whether it's reject or warn.. */
		if (iConf.outdated_tls_policy_user == POLICY_DENY)
			safe_strdup(iConf.outdated_tls_policy_user_message, "Your IRC client is using an outdated TLS protocol or ciphersuite ($protocol-$cipher). Please upgrade your IRC client.");
		else if (iConf.outdated_tls_policy_user == POLICY_WARN)
			safe_strdup(iConf.outdated_tls_policy_user_message, "WARNING: Your IRC client is using an outdated TLS protocol or ciphersuite ($protocol-$cipher). Please upgrade your IRC client.");
	}

	if (!iConf.outdated_tls_policy_oper_message)
	{
		/* The message depends on whether it's reject or warn.. */
		if (iConf.outdated_tls_policy_oper == POLICY_DENY)
			safe_strdup(iConf.outdated_tls_policy_oper_message, "Your IRC client is using an outdated TLS protocol or ciphersuite ($protocol-$cipher). Please upgrade your IRC client.");
		else if (iConf.outdated_tls_policy_oper == POLICY_WARN)
			safe_strdup(iConf.outdated_tls_policy_oper_message, "WARNING: Your IRC client is using an outdated TLS protocol or ciphersuite ($protocol-$cipher). Please upgrade your IRC client.");
	}

	postconf_defaults_log_block();
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
 * This function is called by config_test(), both on boot and on rehash.
 */
void postconf(void)
{
	postconf_defaults();
	postconf_fixes();
	do_weird_shun_stuff();
	isupport_init(); /* for all the 005 values that changed.. */
	tls_check_expiry(NULL);

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
	if (loop.rehashing)
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

/** Run config test and all post config tests. */
int config_test_all(void)
{
	if ((config_test_blocks() < 0) || (callbacks_check() < 0) || (efunctions_check() < 0) ||
	    reloadable_perm_module_unloaded() || !tls_tests() || !log_tests())
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

	for (cfptr = conf; cfptr; cfptr = cfptr->next)
	{
		if (config_verbose > 1)
			config_status("Testing %s", cfptr->filename);
		for (ce = cfptr->items; ce; ce = ce->next)
		{
			if (!strcmp(ce->name, "loadmodule"))
			{
				if (ce->conditional_config)
				{
					config_error("%s:%d: Currently you cannot have a 'loadmodule' statement "
						     "within an @if block, sorry.",
						     ce->file->filename, ce->line_number);
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

/** Reject the configuration load.
 * This is called both from boot and from rehash.
 */
void config_load_failed(void)
{
	if (conf)
		unreal_log(ULOG_ERROR, "config", "CONFIG_NOT_LOADED", NULL, "IRCd configuration failed to load");
	Unload_all_testing_modules();
	free_all_config_resources();
	config_free(conf);
	conf = NULL;
	free_iConf(&tempiConf);
#ifdef _WIN32
	if (!loop.rehashing)
		win_error(); /* GUI popup */
#endif
}

int config_read_start(void)
{
	int ret;

	config_status("Loading IRCd configuration..");
	loop.config_load_failed = 0;

	if (conf)
	{
		config_error("%s:%i - Someone forgot to clean up", __FILE__, __LINE__);
		return -1;
	}

	/* We set this to 1 because otherwise we may call rehash_internal()
	 * already from config_read_file() which is too soon (race).
	 */
	loop.rehash_download_busy = 1;
	add_config_resource(configfile, RESOURCE_INCLUDE, NULL);
	ret = config_read_file(configfile, configfile);
	loop.rehash_download_busy = 0;
	if (ret < 0)
	{
		config_load_failed();
		return -1;
	}
	return 1;
}

int is_config_read_finished(void)
{
	ConfigResource *rs;

	if (loop.rehash_download_busy)
		return 0;

	for (rs = config_resources; rs; rs = rs->next)
	{
		if (rs->type & RESOURCE_DLQUEUED)
		{
			//config_status("Waiting for %s...", rs->url);
			return 0;
		}
	}

	return 1;
}

int config_test(void)
{
	char *old_pid_file = NULL;

	if (loop.config_load_failed)
	{
		/* An error was already printed to the user.
		 * This happens in case of a failed loaded remote URL
		 */
		config_load_failed();
		return -1;
	}

	config_status("Testing IRCd configuration..");

	memset(&tempiConf, 0, sizeof(iConf));
	memset(&settings, 0, sizeof(settings));
	memset(&requiredstuff, 0, sizeof(requiredstuff));
	memset(&nicklengths, 0, sizeof(nicklengths));
	config_setdefaultsettings(&tempiConf);
	clicap_pre_rehash();
	log_pre_rehash();
	free_config_defines();

	if (!config_loadmodules())
	{
		config_load_failed();
		return -1;
	}

	preprocessor_resolve_conditionals_all(PREPROCESSOR_PHASE_MODULE);

	if (!config_test_all())
	{
		config_error("IRCd configuration failed to pass testing");
		config_load_failed();
		return -1;
	}
	callbacks_switchover();
	efunctions_switchover();
	set_targmax_defaults();
	set_security_group_defaults();
	if (loop.rehashing)
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
	}
	config_pre_run_log();

	Init_all_testing_modules();

	if (config_run_blocks() < 0)
	{
		config_error("Bad case of config errors. Server will now die. This really shouldn't happen");
#ifdef _WIN32
		if (!loop.rehashing)
			win_error();
#endif
		abort();
	}

	applymeblock();

	if (old_pid_file && strcmp(old_pid_file, conf_files->pid_file))
	{
		write_pidfile();
		unlink(old_pid_file);
	}
	safe_free(old_pid_file);

	config_free(conf);
	conf = NULL;
	if (loop.rehashing)
	{
		module_loadall();
		RunHook(HOOKTYPE_REHASH_COMPLETE);
	}
	postconf();
	unreal_log(ULOG_INFO, "config", "CONFIG_LOADED", NULL, "Configuration loaded");
	clicap_post_rehash();
	unload_all_unused_mtag_handlers();
	return 0;
}

void config_parse_and_queue_urls(ConfigEntry *ce)
{
	for (; ce; ce = ce->next)
	{
		if (loop.config_load_failed)
			break;
		if (ce->name && !strcmp(ce->name, "include"))
			continue; /* handled elsewhere */
		if (ce->value && !ce->escaped && url_is_valid(ce->value))
			add_config_resource(ce->value, 0, ce);
		if (ce->items)
			config_parse_and_queue_urls(ce->items);
	}
}

/**
 * Read configuration file into ConfigEntry items and add it to the 'conf'
 * list. This checks the file for parse errors, but doesn't do much
 * otherwise. Only: module blacklist checking and checking for "include"
 * items to see if we need to read and parse more configuration files
 * that are included from this one.
 *
 * One _must_ call add_config_resource() before calling config_read_file().
 * This way, include recursion may be detected and reported to the user
 * as an error instead of causing the IRCd to hang in an infinite
 * recursion, eat up memory, and eventually overflow its stack ;-).
 *
 * @param filename the file where the conf may be read from
 * @param display_name The path or URL used to refer to this file.
 *        (mostly to support remote includes' URIs for recursive include detection).
 * @return 1 on success, a negative number on error
 */
int config_read_file(const char *filename, const char *display_name)
{
	ConfigFile 	*cfptr, *cfptr2, **cfptr3;
	ConfigEntry 	*ce;
	ConfigResource *rs;
	int ret;
	int counter;

	if (config_verbose > 0)
		config_status("Loading config file %s ..", filename);

	need_operclass_permissions_upgrade = 0;

	/* Check if we're accidentally including a file a second
	 * time. We should expect to find one entry in this list: the
	 * entry for our current file.
	 * Note that no user should be able to trigger this, this
	 * can only happen if we have buggy code somewhere.
	 */
	counter = 0;
	for (rs = config_resources; rs; rs = rs->next)
	{
#ifndef _WIN32
		if (rs->file && !strcmp(filename, rs->file))
#else
		if (rs->file && !strcasecmp(filename, rs->file))
#endif
		{
			counter ++;
			continue;
		}
		if (rs->url && !strcmp(display_name, rs->url))
		{
			counter ++;
			continue;
		}
	}
	if (counter > 1)
	{
		unreal_log(ULOG_ERROR, "config", "CONFIG_BUG_DUPLICATE_RESOURCE", NULL,
		           "[BUG] Config file $file has been loaded $counter times. "
		           "This should not happen. Someone forgot to call "
		           "add_config_resource() or check its return value!",
		           log_data_string("file", filename),
		           log_data_integer("counter", counter));
		return -1;
	}
	/* end include recursion checking code */

	if ((cfptr = config_load(filename, display_name)))
	{
		for (cfptr3 = &conf, cfptr2 = conf; cfptr2; cfptr2 = cfptr2->next)
			cfptr3 = &cfptr2->next;
		*cfptr3 = cfptr;

		if (config_verbose > 1)
			config_status("Loading module blacklist in %s", filename);

		preprocessor_resolve_conditionals_ce(&cfptr->items, PREPROCESSOR_PHASE_INITIAL);

		for (ce = cfptr->items; ce; ce = ce->next)
			if (!strcmp(ce->name, "blacklist-module"))
				 _test_blacklist_module(cfptr, ce);

		/* Load urls */
		config_parse_and_queue_urls(cfptr->items);

		if(loop.config_load_failed) /* something bad happened while processing urls */
			return -1;

		/* Load includes */
		if (config_verbose > 1)
			config_status("Searching through %s for include files..", filename);

		for (ce = cfptr->items; ce; ce = ce->next)
		{
			if (!strcmp(ce->name, "include"))
			{
				if (ce->conditional_config)
				{
					config_error("%s:%d: Currently you cannot have an 'include' statement "
					             "within an @if block, sorry. However, you CAN do it the other "
					             "way around, that is: put the @if within the included file itself.",
					             ce->file->filename, ce->line_number);
					return -1;
				}
				ret = _conf_include(cfptr, ce);
				if (ret < 0)
					return ret;
			}
		}
		return 1;
	}
	else
	{
		unreal_log(ULOG_ERROR, "config", "CONFIG_LOAD_FILE_FAILED", NULL,
		           "Could not load configuration file: $resource",
		           log_data_string("resource", display_name),
		           log_data_string("filename", filename));
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

void config_rehash()
{
	ConfigItem_oper			*oper_ptr;
	ConfigItem_class 		*class_ptr;
	ConfigItem_ulines 		*uline_ptr;
	ConfigItem_allow 		*allow_ptr;
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
			delete_linkblock(link_ptr);
		}
		else
		{
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
		if (!(listen_ptr->options & LISTENER_CONTROL))
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

/** Make the "read" config the "live" config */
void config_switchover(void)
{
	free_iConf(&iConf);
	memcpy(&iConf, &tempiConf, sizeof(iConf));
	memset(&tempiConf, 0, sizeof(tempiConf));
	log_blocks_switchover();
}

/** Priority of config blocks during CONFIG_TEST stage */
static const char *config_test_priority_blocks[] =
{
	"me",
	"secret",
	"log", /* "log" needs to be before "set" in CONFIG_TEST */
	"set",
	"class",
};

/** Priority of config blocks during CONFIG_RUN stage */
static const char *config_run_priority_blocks[] =
{
	"me",
	"secret",
	"set",
	"log", /* "log" needs to be after "set" in CONFIG_RUN */
	"class",
};

int config_test_blocks()
{
	ConfigEntry 	*ce;
	ConfigFile	*cfptr;
	ConfigCommand	*cc;
	int		errors = 0;
	int i;
	Hook *h;

	invalid_snomasks_encountered = 0;

	/* Stage 1: first the priority blocks, in the order as specified
	 *          in config_test_priority_blocks[]
	 */
	for (i=0; i < ARRAY_SIZEOF(config_test_priority_blocks); i++)
	{
		const char *config_block = config_test_priority_blocks[i];
		cc = config_binary_search(config_block);
		if (!cc)
			abort(); /* internal fuckup */
		for (cfptr = conf; cfptr; cfptr = cfptr->next)
		{
			if (config_verbose > 1)
				config_status("Running %s", cfptr->filename);
			for (ce = cfptr->items; ce; ce = ce->next)
			{
				if (!strcmp(ce->name, config_block))
				{
					int n = cc->testfunc(cfptr, ce);
					errors += n;
					if (!strcmp(config_block, "secret") && (n == 0))
					{
						/* Yeah special case: secret { } blocks we run
						 * immediately here.
						 */
						_conf_secret(cfptr, ce);
					}
				}
			}
		}
	}

	/* Stage 2: now all the other config blocks */
	for (cfptr = conf; cfptr; cfptr = cfptr->next)
	{
		if (config_verbose > 1)
			config_status("Running %s", cfptr->filename);
		for (ce = cfptr->items; ce; ce = ce->next)
		{
			char skip = 0;
			for (i=0; i < ARRAY_SIZEOF(config_test_priority_blocks); i++)
			{
				if (!strcmp(ce->name, config_test_priority_blocks[i]))
				{
					skip = 1;
					break;
				}
			}
			if (skip)
				continue;

			if ((cc = config_binary_search(ce->name))) {
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
						ce->file->filename, ce->line_number,
						ce->name);
					errors++;
					if (strchr(ce->name, ':'))
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

	if (invalid_snomasks_encountered)
	{
		config_error("It seems your set::snomask-on-oper and/or oper::snomask needs to be updated. Are you perhaps upgrading from an older version to UnrealIRCd 6?");
		config_error("See https://www.unrealircd.org/docs/Upgrading_from_5.x#Update_your_snomasks");
	}

	return (errors > 0 ? -1 : 1);
}

int config_run_blocks(void)
{
	ConfigEntry 	*ce;
	ConfigFile	*cfptr;
	ConfigCommand	*cc;
	int		errors = 0;
	int i;
	Hook *h;
	ConfigItem_allow *allow;

	/* Stage 1: first the priority blocks, in the order as specified
	 *          in config_run_priority_blocks[]
	 */
	for (i=0; i < ARRAY_SIZEOF(config_run_priority_blocks); i++)
	{
		const char *config_block = config_run_priority_blocks[i];
		cc = config_binary_search(config_block);
		if (!cc)
			abort(); /* internal fuckup */
		if (!strcmp(config_block, "secret"))
			continue; /* yeah special case, we already processed the run part in test for these */
		for (cfptr = conf; cfptr; cfptr = cfptr->next)
		{
			if (config_verbose > 1)
				config_status("Running %s", cfptr->filename);
			for (ce = cfptr->items; ce; ce = ce->next)
			{
				if (!strcmp(ce->name, config_block))
				{
					if (cc->conffunc(cfptr, ce) < 0)
						errors++;
				}
			}
		}
	}

	/* Stage 2: now all the other config blocks */
	for (cfptr = conf; cfptr; cfptr = cfptr->next)
	{
		if (config_verbose > 1)
			config_status("Running %s", cfptr->filename);
		for (ce = cfptr->items; ce; ce = ce->next)
		{
			char skip = 0;
			for (i=0; i < ARRAY_SIZEOF(config_run_priority_blocks); i++)
			{
				if (!strcmp(ce->name, config_run_priority_blocks[i]))
				{
					skip = 1;
					break;
				}
			}
			if (skip)
				continue;

			if ((cc = config_binary_search(ce->name))) {
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

	close_unbound_listeners();
	listen_cleanup();
	close_unbound_listeners();
	loop.do_bancheck = 1;
	config_switchover();
	update_throttling_timer_settings();

	/* initialize conf_files with defaults if the block isn't set: */
	if (!conf_files)
	  _conf_files(NULL, NULL);

	if (errors > 0)
	{
		config_error("%i fatal errors encountered", errors);
	}
	return (errors > 0 ? -1 : 1);
}

/*
 * Service functions
*/

ConfigItem_alias *find_alias(const char *name)
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

ConfigItem_class *find_class(const char *name)
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


ConfigItem_oper	*find_oper(const char *name)
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

ConfigItem_operclass *find_operclass(const char *name)
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

int count_oper_sessions(const char *name)
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

ConfigItem_listen *find_listen(const char *ipmask, int port, SocketType socket_type)
{
	ConfigItem_listen *e;

	if (!ipmask)
		return NULL;

	for (e = conf_listen; e; e = e->next)
	{
		if (e->socket_type != socket_type)
			continue;
		if (e->socket_type == SOCKET_TYPE_UNIX)
		{
			if (!strcmp(e->file, ipmask))
				return e;
		} else
		{
			if ((e->socket_type == socket_type) && (e->port == port) && !strcmp(e->ip, ipmask))
				return e;
		}
	}

	return NULL;
}

/** Find an SNI match.
 * @param name The hostname to look for (eg: irc.xyz.com).
 */
ConfigItem_sni *find_sni(const char *name)
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

ConfigItem_ulines *find_uline(const char *host)
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


ConfigItem_tld *find_tld(Client *client)
{
	ConfigItem_tld *tld;

	for (tld = conf_tld; tld; tld = tld->next)
	{
		if (unreal_mask_match(client, tld->mask))
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


ConfigItem_link *find_link(const char *servername, Client *client)
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
ConfigItem_ban *find_ban(Client *client, const char *host, short type)
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
ConfigItem_ban 	*find_banEx(Client *client, const char *host, short type, short type2)
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

ConfigItem_vhost *find_vhost(const char *name)
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
ConfigItem_deny_channel *find_channel_allowed(Client *client, const char *name)
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

const char *pretty_time_val_r(char *buf, size_t buflen, long timeval)
{
	if (timeval == 0)
		return "0";

	buf[0] = 0;

	if (timeval/86400)
		snprintf(buf, buflen, "%ldd", timeval/86400);
	if ((timeval/3600) % 24)
		snprintf(buf+strlen(buf), buflen-strlen(buf), "%ldh", (timeval/3600)%24);
	if ((timeval/60)%60)
		snprintf(buf+strlen(buf), buflen-strlen(buf), "%ldm", (timeval/60)%60);
	if ((timeval%60))
		snprintf(buf+strlen(buf), buflen-strlen(buf), "%lds", timeval%60);

	return buf;
}

const char *pretty_time_val(long timeval)
{
	static char buf[512];
	return pretty_time_val_r(buf, sizeof(buf), timeval);
}

/* This converts a relative path to an absolute path, but only if necessary. */
void convert_to_absolute_path(char **path, const char *reldir)
{
	char *s;

	if (!*path || !**path)
		return; /* NULL or empty */

	if (strstr(*path, "://"))
		return; /* URL: don't touch */

#ifdef _WIN32
	if (!strncmp(*path, "cache/", 6))
		return; /* downloaded from URL: don't touch (is only relative path on Windows) */
#endif

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

int _conf_include(ConfigFile *conf, ConfigEntry *ce)
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
	if (!ce->value)
	{
		config_status("%s:%i: include: no filename given",
			ce->file->filename,
			ce->line_number);
		return -1;
	}

	convert_to_absolute_path(&ce->value, CONFDIR);

	if (url_is_valid(ce->value))
	{
		add_config_resource(ce->value, RESOURCE_INCLUDE, ce);
		return 0;
	}
#if !defined(_WIN32) && !defined(_AMIGA) && !defined(OSXTIGER) && DEFAULT_PERMISSIONS != 0
	(void)chmod(ce->value, DEFAULT_PERMISSIONS);
#endif
#ifdef GLOBH
#if defined(__OpenBSD__) && defined(GLOB_LIMIT)
	glob(ce->value, GLOB_NOSORT|GLOB_NOCHECK|GLOB_LIMIT, NULL, &files);
#else
	glob(ce->value, GLOB_NOSORT|GLOB_NOCHECK, NULL, &files);
#endif
	if (!files.gl_pathc) {
		globfree(&files);
		config_status("%s:%i: include %s: invalid file given",
			ce->file->filename, ce->line_number,
			ce->value);
		return -1;
	}
	for (i = 0; i < files.gl_pathc; i++)
	{
		if (add_config_resource(files.gl_pathv[i], RESOURCE_INCLUDE, ce))
		{
			ret = config_read_file(files.gl_pathv[i], files.gl_pathv[i]);
			if (ret < 0)
			{
				globfree(&files);
				return ret;
			}
		}
	}
	globfree(&files);
#elif defined(_WIN32)
	memset(cPath, 0, MAX_PATH);
	if (strchr(ce->value, '/') || strchr(ce->value, '\\')) {
		strlcpy(cPath,ce->value,MAX_PATH);
		cSlash=cPath+strlen(cPath);
		while(*cSlash != '\\' && *cSlash != '/' && cSlash > cPath)
			cSlash--;
		*(cSlash+1)=0;
	}
	if ( (hFind = FindFirstFile(ce->value, &FindData)) == INVALID_HANDLE_VALUE )
	{
		config_status("%s:%i: include %s: invalid file given",
			ce->file->filename, ce->line_number,
			ce->value);
		return -1;
	}
	if (cPath) {
		path = safe_alloc(strlen(cPath) + strlen(FindData.cFileName)+1);
		strcpy(path, cPath);
		strcat(path, FindData.cFileName);

		if (add_config_resource(path, RESOURCE_INCLUDE, ce))
		{
			ret = config_read_file(path, path);
			safe_free(path);
		}
	}
	else
	{
		if (add_config_resource(FindData.cFileName, RESOURCE_INCLUDE, ce))
			ret = config_read_file(FindData.cFileName, FindData.cFileName);
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

			if (add_config_resource(path, RESOURCE_INCLUDE, ce))
			{
				ret = config_read_file(path, path);
				safe_free(path);
				if (ret < 0)
					break;
			}
		}
		else
		{
			if (add_config_resource(FindData.cFileName, RESOURCE_INCLUDE, ce))
				ret = config_read_file(FindData.cFileName, FindData.cFileName);
		}
	}
	FindClose(hFind);
	if (ret < 0)
		return ret;
#else
	if (add_config_resource(ce->value, RESOURCE_INCLUDE, ce))
		ret = config_read_file(ce->value, ce->value);
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		ca = safe_alloc(sizeof(ConfigItem_admin));
		if (!conf_admin)
			conf_admin_tail = ca;
		safe_strdup(ca->line, cep->name);
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
		config_warn_duplicate(ce->file->filename, ce->line_number, "admin");
		return 0;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (strlen(cep->name) > 500)
		{
			config_error("%s:%i: oversized data in admin block",
				cep->file->filename,
				cep->line_number);
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "name"))
		{
			safe_strdup(conf_me->name, cep->value);
		}
		else if (!strcmp(cep->name, "info"))
		{
			safe_strdup(conf_me->info, cep->value);
		}
		else if (!strcmp(cep->name, "sid"))
		{
			safe_strdup(conf_me->sid, cep->value);
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
		config_warn_duplicate(ce->file->filename, ce->line_number, "me");
		return 0;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (config_is_blankorempty(cep, "me"))
			continue;

		/* me::name */
		if (!strcmp(cep->name, "name"))
		{
			if (has_name)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "me::name");
				continue;
			}
			has_name = 1;
			if (!strchr(cep->value, '.'))
			{
				config_error("%s:%i: illegal me::name, must be fully qualified hostname",
					cep->file->filename,
					cep->line_number);
				errors++;
			}
			if (strlen(cep->value) > HOSTLEN)
			{
				config_error("%s:%i: illegal me::name, must be less or equal to %i characters",
					cep->file->filename,
					cep->line_number, HOSTLEN);
				errors++;
			}
			if (!valid_server_name(cep->value))
			{
				config_error("%s:%i: illegal me::name contains invalid character(s) [only a-z, 0-9, _, -, . are allowed]",
					cep->file->filename,
					cep->line_number);
				errors++;
			}
		}
		/* me::info */
		else if (!strcmp(cep->name, "info"))
		{
			char *p;
			char valid = 0;
			if (has_info)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "me::info");
				continue;
			}
			has_info = 1;
			if (strlen(cep->value) > (REALLEN-1))
			{
				config_error("%s:%i: too long me::info, must be max. %i characters",
					cep->file->filename, cep->line_number,
					REALLEN-1);
				errors++;
			}

			/* Valid me::info? Any data except spaces is ok */
			for (p=cep->value; *p; p++)
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
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "numeric"))
		{
			config_error("%s:%i: me::numeric has been removed, you must now specify a Server ID (SID) instead. "
			             "Edit your configuration file and change 'numeric' to 'sid' and make up "
			             "a server id of exactly 3 characters, starting with a digit, eg: \"001\" or \"0AB\".",
			             cep->file->filename, cep->line_number);
			errors++;
		}
		else if (!strcmp(cep->name, "sid"))
		{
			if (has_sid)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "me::sid");
				continue;
			}
			has_sid = 1;

			if (!valid_sid(cep->value))
			{
				config_error("%s:%i: me::sid must be 3 characters long, begin with a number, "
				             "and the 2nd and 3rd character must be a number or uppercase letter. "
				             "Example: \"001\" and \"0AB\" is good. \"AAA\" and \"0ab\" are bad. ",
				             cep->file->filename, cep->line_number);
				errors++;
			}

			if (!isdigit(*cep->value))
			{
				config_error("%s:%i: me::sid must be 3 characters long and begin with a number",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		/* Unknown entry */
		else
		{
			config_error_unknown(ce->file->filename, ce->line_number,
				"me", cep->name);
			errors++;
		}
	}
	if (!has_name)
	{
		config_error_missing(ce->file->filename, ce->line_number, "me::name");
		errors++;
	}
	if (!has_info)
	{
		config_error_missing(ce->file->filename, ce->line_number, "me::info");
		errors++;
	}
	if (!has_sid)
	{
		config_error_missing(ce->file->filename, ce->line_number, "me::sid");
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
	 * if config_run_blocks() calls us with a NULL ce, it's got a bug...but we can't detect that.
	 */
	if (!ce)
	  return 1;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "motd"))
			safe_strdup(conf_files->motd_file, cep->value);
		else if (!strcmp(cep->name, "shortmotd"))
			safe_strdup(conf_files->smotd_file, cep->value);
		else if (!strcmp(cep->name, "opermotd"))
			safe_strdup(conf_files->opermotd_file, cep->value);
		else if (!strcmp(cep->name, "svsmotd"))
			safe_strdup(conf_files->svsmotd_file, cep->value);
		else if (!strcmp(cep->name, "botmotd"))
			safe_strdup(conf_files->botmotd_file, cep->value);
		else if (!strcmp(cep->name, "rules"))
			safe_strdup(conf_files->rules_file, cep->value);
		else if (!strcmp(cep->name, "tunefile"))
			safe_strdup(conf_files->tune_file, cep->value);
		else if (!strcmp(cep->name, "pidfile"))
			safe_strdup(conf_files->pid_file, cep->value);
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		/* files::motd */
		if (!strcmp(cep->name, "motd"))
		{
			if (has_motd)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "files::motd");
				continue;
			}
			convert_to_absolute_path(&cep->value, CONFDIR);
			config_test_openfile(cep, O_RDONLY, 0, "files::motd", 0, 1);
			has_motd = 1;
		}
		/* files::smotd */
		else if (!strcmp(cep->name, "shortmotd"))
		{
			if (has_smotd)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "files::shortmotd");
				continue;
			}
			convert_to_absolute_path(&cep->value, CONFDIR);
			config_test_openfile(cep, O_RDONLY, 0, "files::shortmotd", 0, 1);
			has_smotd = 1;
		}
		/* files::rules */
		else if (!strcmp(cep->name, "rules"))
		{
			if (has_rules)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "files::rules");
				continue;
			}
			convert_to_absolute_path(&cep->value, CONFDIR);
			config_test_openfile(cep, O_RDONLY, 0, "files::rules", 0, 1);
			has_rules = 1;
		}
		/* files::botmotd */
		else if (!strcmp(cep->name, "botmotd"))
		{
			if (has_botmotd)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "files::botmotd");
				continue;
			}
			convert_to_absolute_path(&cep->value, CONFDIR);
			config_test_openfile(cep, O_RDONLY, 0, "files::botmotd", 0, 1);
			has_botmotd = 1;
		}
		/* files::opermotd */
		else if (!strcmp(cep->name, "opermotd"))
		{
			if (has_opermotd)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "files::opermotd");
				continue;
			}
			convert_to_absolute_path(&cep->value, CONFDIR);
			config_test_openfile(cep, O_RDONLY, 0, "files::opermotd", 0, 1);
			has_opermotd = 1;
		}
		/* files::svsmotd
		 * This config stuff should somehow be inside of modules/svsmotd.c!!!... right?
		 */
		else if (!strcmp(cep->name, "svsmotd"))
		{
			if (has_svsmotd)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "files::svsmotd");
				continue;
			}
			convert_to_absolute_path(&cep->value, CONFDIR);
			/* svsmotd can't be a URL because we have to be able to write to it */
			config_test_openfile(cep, O_RDONLY, 0, "files::svsmotd", 0, 0);
			has_svsmotd = 1;
		}
		/* files::pidfile */
		else if (!strcmp(cep->name, "pidfile"))
		{
			if (has_pidfile)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "files::pidfile");
				continue;
			}
			convert_to_absolute_path(&cep->value, PERMDATADIR);
			errors += config_test_openfile(cep, O_WRONLY | O_CREAT, 0600, "files::pidfile", 1, 0);
			has_pidfile = 1;
		}
		/* files::tunefile */
		else if (!strcmp(cep->name, "tunefile"))
		{
			if (has_tunefile)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "files::tunefile");
				continue;
			}
			convert_to_absolute_path(&cep->value, PERMDATADIR);
			errors += config_test_openfile(cep, O_RDWR | O_CREAT, 0600, "files::tunefile", 1, 0);
			has_tunefile = 1;
		}
		/* <random directive here> */
		else
		{
			config_error("%s:%d: Unknown directive: \"%s\" in files {}", cep->file->filename,
				     cep->line_number, cep->name);
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

	if (!strcmp(ce->name,"allow"))
		entry->type = OPERCLASSENTRY_ALLOW;
	else
		entry->type = OPERCLASSENTRY_DENY;

	for (cep = ce->items; cep; cep = cep->next)
	{
		OperClassACLEntryVar *var = safe_alloc(sizeof(OperClassACLEntryVar));
		safe_strdup(var->name, cep->name);
		if (cep->value)
		{
			safe_strdup(var->value, cep->value);
		}
		AddListItem(var,entry->variables);
	}

	return entry;
}

OperClassACL* _conf_parseACL(const char *name, ConfigEntry *ce)
{
	ConfigEntry *cep;
	OperClassACL *acl = NULL;

	acl = safe_alloc(sizeof(OperClassACL));
	safe_strdup(acl->name, name);

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "deny") || !strcmp(cep->name, "allow"))
		{
			OperClassACLEntry *entry = _conf_parseACLEntry(cep);
			AddListItem(entry,acl->entries);
		}
		else {
			OperClassACL *subAcl = _conf_parseACL(cep->name,cep);
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
	safe_strdup(operClass->classStruct->name, ce->value);

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "parent"))
		{
			safe_strdup(operClass->classStruct->ISA, cep->value);
		}
		else if (!strcmp(cep->name, "permissions"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				OperClassACL *acl = _conf_parseACL(cepp->name,cepp);
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
	             ce->file->filename, ce->line_number);
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

	if (!ce->value)
	{
		config_error_noname(ce->file->filename, ce->line_number, "operclass");
		errors++;
	}
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "parent"))
		{
			if (has_parent)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "operclass::parent");
				continue;
			}
			has_parent = 1;
			continue;
		} else
		if (!strcmp(cep->name, "permissions"))
		{
			if (has_permissions)
			{
				config_warn_duplicate(cep->file->filename,
				cep->line_number, "oper::permissions");
				continue;
			}
			has_permissions = 1;
			continue;
		} else
		if (!strcmp(cep->name, "privileges"))
		{
			new_permissions_system(conf, cep);
			errors++;
			return errors;
		} else
		{
			config_error_unknown(cep->file->filename,
				cep->line_number, "operclass", cep->name);
			errors++;
			continue;
		}
	}

	if (!has_permissions)
	{
		config_error_missing(ce->file->filename, ce->line_number,
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
	safe_strdup(oper->name, ce->value);

	/* Inherit some defaults: */
	oper->server_notice_colors = tempiConf.server_notice_colors;
	oper->server_notice_show_event = tempiConf.server_notice_show_event;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "operclass"))
			safe_strdup(oper->operclass, cep->value);
		if (!strcmp(cep->name, "password"))
			oper->auth = AuthBlockToAuthConfig(cep);
		else if (!strcmp(cep->name, "class"))
		{
			oper->class = find_class(cep->value);
			if (!oper->class || (oper->class->flag.temporary == 1))
			{
				config_status("%s:%i: illegal oper::class, unknown class '%s' using default of class 'default'",
					cep->file->filename, cep->line_number,
					cep->value);
				oper->class = default_class;
			}
		}
		else if (!strcmp(cep->name, "swhois"))
		{
			SWhois *s;
			if (cep->items)
			{
				for (cepp = cep->items; cepp; cepp = cepp->next)
				{
					s = safe_alloc(sizeof(SWhois));
					safe_strdup(s->line, cepp->name);
					safe_strdup(s->setby, "oper");
					AddListItem(s, oper->swhois);
				}
			} else
			if (cep->value)
			{
				s = safe_alloc(sizeof(SWhois));
				safe_strdup(s->line, cep->value);
				safe_strdup(s->setby, "oper");
				AddListItem(s, oper->swhois);
			}
		}
		else if (!strcmp(cep->name, "snomask"))
		{
			safe_strdup(oper->snomask, cep->value);
		}
		else if (!strcmp(cep->name, "server-notice-colors"))
		{
			oper->server_notice_colors = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "server-notice-show-event"))
		{
			oper->server_notice_show_event = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "modes"))
		{
			oper->modes = set_usermode(cep->value);
		}
		else if (!strcmp(cep->name, "require-modes"))
		{
			oper->require_modes = set_usermode(cep->value);
		}
		else if (!strcmp(cep->name, "maxlogins"))
		{
			oper->maxlogins = atoi(cep->value);
		}
		else if (!strcmp(cep->name, "mask"))
		{
			unreal_add_masks(&oper->mask, cep);
		}
		else if (!strcmp(cep->name, "vhost"))
		{
			safe_strdup(oper->vhost, cep->value);
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

	if (!ce->value)
	{
		config_error_noname(ce->file->filename, ce->line_number, "oper");
		errors++;
	}
	for (cep = ce->items; cep; cep = cep->next)
	{
		/* Regular variables */
		if (!cep->items)
		{
			if (config_is_blankorempty(cep, "oper"))
			{
				errors++;
				continue;
			}
			/* oper::password */
			if (!strcmp(cep->name, "password"))
			{
				if (has_password)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "oper::password");
					continue;
				}
				has_password = 1;
				if (Auth_CheckError(cep) < 0)
					errors++;

				if (ce->value && cep->value &&
					!strcmp(ce->value, "bobsmith") &&
					!strcmp(cep->value, "test"))
				{
					config_error("%s:%i: please change the the name and password of the "
								 "default 'bobsmith' oper block",
								 ce->file->filename, ce->line_number);
					errors++;
				}
				continue;
			}
			/* oper::operclass */
			else if (!strcmp(cep->name, "operclass"))
			{
				if (has_operclass)
				{
					config_warn_duplicate(cep->file->filename,
					cep->line_number, "oper::operclass");
					continue;
				}
				has_operclass = 1;
				continue;
			}
			/* oper::class */
			else if (!strcmp(cep->name, "class"))
			{
				if (has_class)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "oper::class");
					continue;
				}
				has_class = 1;
			}
			/* oper::swhois */
			else if (!strcmp(cep->name, "swhois"))
			{
			}
			/* oper::vhost */
			else if (!strcmp(cep->name, "vhost"))
			{
				if (has_vhost)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "oper::vhost");
					continue;
				}
				has_vhost = 1;
			}
			/* oper::snomask */
			else if (!strcmp(cep->name, "snomask"))
			{
				char *wrong_snomask;
				if (has_snomask)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "oper::snomask");
					continue;
				}
				if (!is_valid_snomask_string_testing(cep->value, &wrong_snomask))
				{
					config_error("%s:%i: oper::snomask contains unknown snomask letter(s) '%s'",
					             cep->file->filename, cep->line_number, wrong_snomask);
					errors++;
					invalid_snomasks_encountered++;
				}
				has_snomask = 1;
			}
			else if (!strcmp(cep->name, "server-notice-colors"))
			{
			}
			else if (!strcmp(cep->name, "server-notice-show-event"))
			{
			}
			/* oper::modes */
			else if (!strcmp(cep->name, "modes"))
			{
				char *p;
				for (p = cep->value; *p; p++)
					if (strchr("orzS", *p))
					{
						config_error("%s:%i: oper::modes may not include mode '%c'",
							cep->file->filename, cep->line_number, *p);
						errors++;
					}
				if (has_modes)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "oper::modes");
					continue;
				}
				has_modes = 1;
			}
			/* oper::require-modes */
			else if (!strcmp(cep->name, "require-modes"))
			{
				char *p;
				for (p = cep->value; *p; p++)
					if (strchr("o", *p))
					{
						config_warn("%s:%i: oper::require-modes probably shouldn't include mode '%c'",
							cep->file->filename, cep->line_number, *p);
					}
				if (has_require_modes)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "oper::require-modes");
					continue;
				}
				has_require_modes = 1;
			}
			/* oper::maxlogins */
			else if (!strcmp(cep->name, "maxlogins"))
			{
				int l;

				if (has_maxlogins)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "oper::maxlogins");
					continue;
				}
				has_maxlogins = 1;

				l = atoi(cep->value);
				if ((l < 0) || (l > 5000))
				{
					config_error("%s:%i: oper::maxlogins: value out of range (%d) should be 0-5000",
						cep->file->filename, cep->line_number, l);
					errors++;
					continue;
				}
			}
			else if (!strcmp(cep->name, "mask"))
			{
				if (cep->value || cep->items)
					has_mask = 1;
			}
			else
			{
				config_error_unknown(cep->file->filename,
					cep->line_number, "oper", cep->name);
				errors++;
				continue;
			}
		}
		/* Sections */
		else
		{
			if (!strcmp(cep->name, "swhois"))
			{
				/* ok */
			}
			else if (!strcmp(cep->name, "mask"))
			{
				if (cep->value || cep->items)
					has_mask = 1;
			}
			else if (!strcmp(cep->name, "password"))
			{
				if (has_password)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "oper::password");
					continue;
				}
				has_password = 1;
				if (Auth_CheckError(cep) < 0)
					errors++;
			}
			else
			{
				config_error_unknown(cep->file->filename,
					cep->line_number, "oper", cep->name);
				errors++;
				continue;
			}
		}
	}
	if (!has_password)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"oper::password");
		errors++;
	}
	if (!has_mask)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"oper::mask");
		errors++;
	}
	if (!has_class)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"oper::class");
		errors++;
	}
	if (!has_operclass)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"oper::operclass");
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

	if (!(class = find_class(ce->value)))
	{
		class = safe_alloc(sizeof(ConfigItem_class));
		safe_strdup(class->name, ce->value);
		isnew = 1;
	}
	else
	{
		isnew = 0;
		class->flag.temporary = 0;
		class->options = 0; /* RESET OPTIONS */
	}
	safe_strdup(class->name, ce->value);

	class->connfreq = 15; /* default */

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "pingfreq"))
			class->pingfreq = config_checkval(cep->value,CFG_TIME);
		else if (!strcmp(cep->name, "connfreq"))
			class->connfreq = config_checkval(cep->value,CFG_TIME);
		else if (!strcmp(cep->name, "maxclients"))
			class->maxclients = atol(cep->value);
		else if (!strcmp(cep->name, "sendq"))
			class->sendq = config_checkval(cep->value,CFG_SIZE);
		else if (!strcmp(cep->name, "recvq"))
			class->recvq = config_checkval(cep->value,CFG_SIZE);
		else if (!strcmp(cep->name, "options"))
		{
			for (cep2 = cep->items; cep2; cep2 = cep2->next)
				if (!strcmp(cep2->name, "nofakelag"))
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

	if (!ce->value)
	{
		config_error_noname(ce->file->filename, ce->line_number, "class");
		return 1;
	}
	if (!strcasecmp(ce->value, "default"))
	{
		config_error("%s:%d: Class cannot be named 'default', this class name is reserved for internal use.",
			ce->file->filename, ce->line_number);
		errors++;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "options"))
		{
			for (cep2 = cep->items; cep2; cep2 = cep2->next)
			{
#ifdef FAKELAG_CONFIGURABLE
				if (!strcmp(cep2->name, "nofakelag"))
					;
				else
#endif
				{
					config_error("%s:%d: Unknown option '%s' in class::options",
						cep2->file->filename, cep2->line_number, cep2->name);
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
		else if (!strcmp(cep->name, "pingfreq"))
		{
			int v = config_checkval(cep->value,CFG_TIME);
			if (has_pingfreq)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "class::pingfreq");
				continue;
			}
			has_pingfreq = 1;
			if ((v < 30) || (v > 600))
			{
				config_error("%s:%i: class::pingfreq should be a reasonable value (30-600)",
					cep->file->filename, cep->line_number);
				errors++;
				continue;
			}
		}
		/* class::maxclients */
		else if (!strcmp(cep->name, "maxclients"))
		{
			long l;
			if (has_maxclients)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "class::maxclients");
				continue;
			}
			has_maxclients = 1;
			l = atol(cep->value);
			if ((l < 1) || (l > 1000000))
			{
				config_error("%s:%i: class::maxclients with illegal value",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		/* class::connfreq */
		else if (!strcmp(cep->name, "connfreq"))
		{
			long l;
			if (has_connfreq)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "class::connfreq");
				continue;
			}
			has_connfreq = 1;
			l = config_checkval(cep->value,CFG_TIME);
			if ((l < 5) || (l > 604800))
			{
				config_error("%s:%i: class::connfreq with illegal value (must be >5 and <7d)",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		/* class::sendq */
		else if (!strcmp(cep->name, "sendq"))
		{
			long l;
			if (has_sendq)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "class::sendq");
				continue;
			}
			has_sendq = 1;
			l = config_checkval(cep->value,CFG_SIZE);
			if ((l <= 0) || (l > 2000000000))
			{
				config_error("%s:%i: class::sendq with illegal value",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		/* class::recvq */
		else if (!strcmp(cep->name, "recvq"))
		{
			long l;
			if (has_recvq)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "class::recvq");
				continue;
			}
			has_recvq = 1;
			l = config_checkval(cep->value,CFG_SIZE);
			if ((l < 512) || (l > 32768))
			{
				config_error("%s:%i: class::recvq with illegal value (must be >512 and <32k)",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		/* Unknown */
		else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"class", cep->name);
			errors++;
			continue;
		}
	}
	if (!has_pingfreq)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"class::pingfreq");
		errors++;
	}
	if (!has_maxclients)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"class::maxclients");
		errors++;
	}
	if (!has_sendq)
	{
		config_error_missing(ce->file->filename, ce->line_number,
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "restart"))
		{
			if (conf_drpass->restartauth)
				Auth_FreeAuthConfig(conf_drpass->restartauth);

			conf_drpass->restartauth = AuthBlockToAuthConfig(cep);
		}
		else if (!strcmp(cep->name, "die"))
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (config_is_blankorempty(cep, "drpass"))
		{
			errors++;
			continue;
		}
		/* drpass::restart */
		if (!strcmp(cep->name, "restart"))
		{
			if (has_restart)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "drpass::restart");
				continue;
			}
			has_restart = 1;
			if (Auth_CheckError(cep) < 0)
				errors++;
			continue;
		}
		/* drpass::die */
		else if (!strcmp(cep->name, "die"))
		{
			if (has_die)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "drpass::die");
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
			config_error_unknown(cep->file->filename, cep->line_number,
				"drpass", cep->name);
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		ca = safe_alloc(sizeof(ConfigItem_ulines));
		safe_strdup(ca->servername, cep->name);
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "mask"))
			unreal_add_masks(&ca->mask, cep);
		else if (!strcmp(cep->name, "motd"))
		{
			safe_strdup(ca->motd_file, cep->value);
			read_motd(cep->value, &ca->motd);
		}
		else if (!strcmp(cep->name, "shortmotd"))
		{
			safe_strdup(ca->smotd_file, cep->value);
			read_motd(cep->value, &ca->smotd);
		}
		else if (!strcmp(cep->name, "opermotd"))
		{
			safe_strdup(ca->opermotd_file, cep->value);
			read_motd(cep->value, &ca->opermotd);
		}
		else if (!strcmp(cep->name, "botmotd"))
		{
			safe_strdup(ca->botmotd_file, cep->value);
			read_motd(cep->value, &ca->botmotd);
		}
		else if (!strcmp(cep->name, "rules"))
		{
			safe_strdup(ca->rules_file, cep->value);
			read_motd(cep->value, &ca->rules);
		}
		else if (!strcmp(cep->name, "options"))
		{
			ConfigEntry *cepp;
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "ssl") || !strcmp(cepp->name, "tls"))
					ca->options |= TLD_TLS;
				else if (!strcmp(cepp->name, "remote"))
					ca->options |= TLD_REMOTE;
			}
		}
		else if (!strcmp(cep->name, "channel"))
			safe_strdup(ca->channel, cep->value);
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->value && strcmp(cep->name, "options"))
		{
			config_error_empty(cep->file->filename, cep->line_number,
				"tld", cep->name);
			errors++;
			continue;
		}
		/* tld::mask */
		if (!strcmp(cep->name, "mask"))
		{
			if (cep->value || cep->items)
				has_mask = 1;
		}
		/* tld::motd */
		else if (!strcmp(cep->name, "motd"))
		{
			if (has_motd)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "tld::motd");
				continue;
			}
			has_motd = 1;
			convert_to_absolute_path(&cep->value, CONFDIR);
			if (((fd = open(cep->value, O_RDONLY)) == -1))
			{
				config_error("%s:%i: tld::motd: %s: %s",
					cep->file->filename, cep->line_number,
					cep->value, strerror(errno));
				errors++;
			}
			else
				close(fd);
		}
		/* tld::rules */
		else if (!strcmp(cep->name, "rules"))
		{
			if (has_rules)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "tld::rules");
				continue;
			}
			has_rules = 1;
			convert_to_absolute_path(&cep->value, CONFDIR);
			if (((fd = open(cep->value, O_RDONLY)) == -1))
			{
				config_error("%s:%i: tld::rules: %s: %s",
					cep->file->filename, cep->line_number,
					cep->value, strerror(errno));
				errors++;
			}
			else
				close(fd);
		}
		/* tld::channel */
		else if (!strcmp(cep->name, "channel"))
		{
			if (has_channel)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "tld::channel");
				continue;
			}
			has_channel = 1;
		}
		/* tld::shortmotd */
		else if (!strcmp(cep->name, "shortmotd"))
		{
			if (has_shortmotd)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "tld::shortmotd");
				continue;
			}
			has_shortmotd = 1;
			convert_to_absolute_path(&cep->value, CONFDIR);
			if (((fd = open(cep->value, O_RDONLY)) == -1))
			{
				config_error("%s:%i: tld::shortmotd: %s: %s",
					cep->file->filename, cep->line_number,
					cep->value, strerror(errno));
				errors++;
			}
			else
				close(fd);
		}
		/* tld::opermotd */
		else if (!strcmp(cep->name, "opermotd"))
		{
			if (has_opermotd)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "tld::opermotd");
				continue;
			}
			has_opermotd = 1;
			convert_to_absolute_path(&cep->value, CONFDIR);
			if (((fd = open(cep->value, O_RDONLY)) == -1))
			{
				config_error("%s:%i: tld::opermotd: %s: %s",
					cep->file->filename, cep->line_number,
					cep->value, strerror(errno));
				errors++;
			}
			else
				close(fd);
		}
		/* tld::botmotd */
		else if (!strcmp(cep->name, "botmotd"))
		{
			if (has_botmotd)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "tld::botmotd");
				continue;
			}
			has_botmotd = 1;
			convert_to_absolute_path(&cep->value, CONFDIR);
			if (((fd = open(cep->value, O_RDONLY)) == -1))
			{
				config_error("%s:%i: tld::botmotd: %s: %s",
					cep->file->filename, cep->line_number,
					cep->value, strerror(errno));
				errors++;
			}
			else
				close(fd);
		}
		/* tld::options */
		else if (!strcmp(cep->name, "options")) {
			ConfigEntry *cep2;

			if (has_options)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "tld::options");
				continue;
			}
			has_options = 1;

			for (cep2 = cep->items; cep2; cep2 = cep2->next)
			{
				if (strcmp(cep2->name, "ssl") &&
				    strcmp(cep2->name, "tls") &&
				    strcmp(cep2->name, "remote"))
				{
					config_error_unknownopt(cep2->file->filename,
						cep2->line_number, "tld", cep2->name);
					errors++;
				}
			}
		}
		else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"tld", cep->name);
			errors++;
			continue;
		}
	}
	if (!has_mask)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"tld::mask");
		errors++;
	}
	if (!has_motd)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"tld::motd");
		errors++;
	}
	if (!has_rules)
	{
		config_error_missing(ce->file->filename, ce->line_number,
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
	char *file = NULL;
	char *ip = NULL;
	int start=0, end=0, port, isnew;
	int tmpflags =0;
	Hook *h;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "file"))
		{
			file = cep->value;
		} else
		if (!strcmp(cep->name, "ip"))
		{
			ip = cep->value;
		} else
		if (!strcmp(cep->name, "port"))
		{
			port_range(cep->value, &start, &end);
			if ((start < 0) || (start > 65535) || (end < 0) || (end > 65535))
				return -1; /* this is already validated in _test_listen, but okay.. */
		} else
		if (!strcmp(cep->name, "options"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				long v;
				if ((v = nv_find_by_name(_ListenerFlags, cepp->name)))
				{
					tmpflags |= v;
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
		if (!strcmp(cep->name, "ssl-options") || !strcmp(cep->name, "tls-options"))
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

	/* UNIX domain socket code */
	if (file)
	{
		if (!(listen = find_listen(file, 0, SOCKET_TYPE_UNIX)))
		{
			listen = safe_alloc(sizeof(ConfigItem_listen));
			safe_strdup(listen->file, file);
			listen->socket_type = SOCKET_TYPE_UNIX;
			listen->fd = -1;
			isnew = 1;
		} else {
			isnew = 0;
		}

		if (listen->options & LISTENER_BOUND)
			tmpflags |= LISTENER_BOUND;

		listen->options = tmpflags;
		if (isnew)
			AddListItem(listen, conf_listen);
		listen->flag.temporary = 0;

		return 1;
	}

	for (port = start; port <= end; port++)
	{
		/* First deal with IPv4 */
		if (!strchr(ip, ':'))
		{
			if (!(listen = find_listen(ip, port, SOCKET_TYPE_IPV4)))
			{
				listen = safe_alloc(sizeof(ConfigItem_listen));
				safe_strdup(listen->ip, ip);
				listen->port = port;
				listen->fd = -1;
				listen->socket_type = SOCKET_TYPE_IPV4;
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
			
			safe_free(listen->websocket_forward);

			/* For modules that hook CONFIG_LISTEN and CONFIG_LISTEN_OPTIONS.
			 * Yeah, ugly we have this here..
			 * and again about 100 lines down too.
			 */
			for (cep = ce->items; cep; cep = cep->next)
			{
				if (!strcmp(cep->name, "ip"))
					;
				else if (!strcmp(cep->name, "port"))
					;
				else if (!strcmp(cep->name, "options"))
				{
					for (cepp = cep->items; cepp; cepp = cepp->next)
					{
						NameValue *ofp;
						if (!nv_find_by_name(_ListenerFlags, cepp->name))
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
				if (!strcmp(cep->name, "ssl-options") || !strcmp(cep->name, "tls-options"))
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
				if (!(listen = find_listen(ip, port, SOCKET_TYPE_IPV6)))
				{
					listen = safe_alloc(sizeof(ConfigItem_listen));
					safe_strdup(listen->ip, ip);
					listen->port = port;
					listen->fd = -1;
					listen->socket_type = SOCKET_TYPE_IPV6;
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
				
				safe_free(listen->websocket_forward);
				
				/* For modules that hook CONFIG_LISTEN and CONFIG_LISTEN_OPTIONS.
				 * Yeah, ugly we have this here..
				 */
				for (cep = ce->items; cep; cep = cep->next)
				{
					if (!strcmp(cep->name, "ip"))
						;
					else if (!strcmp(cep->name, "port"))
						;
					else if (!strcmp(cep->name, "options"))
					{
						for (cepp = cep->items; cepp; cepp = cepp->next)
						{
							if (!nv_find_by_name(_ListenerFlags, cepp->name))
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
					if (!strcmp(cep->name, "ssl-options") || !strcmp(cep->name, "tls-options"))
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
	char has_file = 0, has_ip = 0, has_port = 0, has_options = 0, port_6667 = 0;
	char *file = NULL;
	char *ip = NULL;
	Hook *h;

	if (ce->value)
	{
		config_error("%s:%i: listen block has a new syntax, see https://www.unrealircd.org/docs/Listen_block",
			ce->file->filename, ce->line_number);
		return 1;
	}

	for (cep = ce->items; cep; cep = cep->next)
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
		if (!strcmp(cep->name, "options"))
		{
			if (has_options)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "listen::options");
				continue;
			}
			has_options = 1;
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!nv_find_by_name(_ListenerFlags, cepp->name))
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
						config_error_unknownopt(cepp->file->filename,
							cepp->line_number, "listen::options", cepp->name);
						errors++;
						continue;
					}
				}
				if (!strcmp(cepp->name, "ssl") || !strcmp(cepp->name, "tls"))
					have_tls_listeners = 1; /* for ssl config test */
			}
		}
		else
		if (!strcmp(cep->name, "ssl-options") || !strcmp(cep->name, "tls-options"))
		{
			test_tlsblock(conf, cep, &errors);
		}
		else
		if (!cep->value)
		{
			if (!used_by_module)
			{
				config_error_empty(cep->file->filename,
					cep->line_number, "listen", cep->name);
				errors++;
			}
			continue; /* always */
		} else
		if (!strcmp(cep->name, "file"))
		{
			has_file = 1;
			file = cep->value;
		} else
		if (!strcmp(cep->name, "ip"))
		{
			has_ip = 1;

			if (strcmp(cep->value, "*") && !is_valid_ip(cep->value))
			{
				config_error("%s:%i: listen: illegal listen::ip (%s). Must be either '*' or contain a valid IP.",
					cep->file->filename, cep->line_number, cep->value);
				return 1;
			}
			ip = cep->value;
		} else
		if (!strcmp(cep->name, "host"))
		{
			config_error("%s:%i: listen: unknown option listen::host, did you mean listen::ip?",
				cep->file->filename, cep->line_number);
			errors++;
		} else
		if (!strcmp(cep->name, "port"))
		{
			int start = 0, end = 0;

			has_port = 1;

			port_range(cep->value, &start, &end);
			if (start == end)
			{
				if ((start < 1) || (start > 65535))
				{
					config_error("%s:%i: listen: illegal port (must be 1..65535)",
						cep->file->filename, cep->line_number);
					return 1;
				}
			}
			else
			{
				if (end < start)
				{
					config_error("%s:%i: listen: illegal port range end value is less than starting value",
						cep->file->filename, cep->line_number);
					return 1;
				}
				if (end - start >= 100)
				{
					config_error("%s:%i: listen: you requested port %d-%d, that's %d ports "
						"(and thus consumes %d sockets) this is probably not what you want.",
						cep->file->filename, cep->line_number, start, end,
						end - start + 1, end - start + 1);
					return 1;
				}
				if ((start < 1) || (start > 65535) || (end < 1) || (end > 65535))
				{
					config_error("%s:%i: listen: illegal port range values must be between 1 and 65535",
						cep->file->filename, cep->line_number);
					return 1;
				}
			}

			if ((6667 >= start) && (6667 <= end))
				port_6667 = 1;
		} else
		{
			if (!used_by_module)
			{
				config_error_unknown(cep->file->filename, cep->line_number,
					"listen", cep->name);
				errors++;
			}
			continue; /* always */
		}
	}

	if (has_file)
	{
		if (has_ip || has_port)
		{
			config_error("%s:%d: listen block should either have a 'file' (for *NIX domain socket), "
			             "OR have an 'ip' and 'port' (for IPv4/IPv6). You cannot combine both in one listen block.",
			             ce->file->filename, ce->line_number);
			errors++;
		} else {
			// TODO: check if file can be created fresh etc.
		}
	} else
	{
		if (!has_ip)
		{
			config_error("%s:%d: listen block requires an listen::ip",
				ce->file->filename, ce->line_number);
			errors++;
		}

		if (!has_port)
		{
			config_error("%s:%d: listen block requires an listen::port",
				ce->file->filename, ce->line_number);
			errors++;
		}
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

	if (ce->value)
	{
		if (!strcmp(ce->value, "channel"))
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
	allow->ipv6_clone_mask = tempiConf.default_ipv6_clone_mask;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "mask") || !strcmp(cep->name, "ip") || !strcmp(cep->name, "hostname"))
		{
			unreal_add_masks(&allow->mask, cep);
		}
		else if (!strcmp(cep->name, "password"))
			allow->auth = AuthBlockToAuthConfig(cep);
		else if (!strcmp(cep->name, "class"))
		{
			allow->class = find_class(cep->value);
			if (!allow->class || (allow->class->flag.temporary == 1))
			{
				config_status("%s:%i: illegal allow::class, unknown class '%s' using default of class 'default'",
					cep->file->filename,
					cep->line_number,
					cep->value);
					allow->class = default_class;
			}
		}
		else if (!strcmp(cep->name, "maxperip"))
			allow->maxperip = atoi(cep->value);
		else if (!strcmp(cep->name, "global-maxperip"))
			allow->global_maxperip = atoi(cep->value);
		else if (!strcmp(cep->name, "redirect-server"))
			safe_strdup(allow->server, cep->value);
		else if (!strcmp(cep->name, "redirect-port"))
			allow->port = atoi(cep->value);
		else if (!strcmp(cep->name, "ipv6-clone-mask"))
		{
			/*
			 * If this item isn't set explicitly by the
			 * user, the value will temporarily be
			 * zero. Defaults are applied in config_run_blocks().
			 */
			allow->ipv6_clone_mask = atoi(cep->value);
		}
		else if (!strcmp(cep->name, "options"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "noident"))
					allow->flags.noident = 1;
				else if (!strcmp(cepp->name, "useip"))
					allow->flags.useip = 1;
				else if (!strcmp(cepp->name, "ssl") || !strcmp(cepp->name, "tls"))
					allow->flags.tls = 1;
				else if (!strcmp(cepp->name, "reject-on-auth-failure"))
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

	if (ce->value)
	{
		if (!strcmp(ce->value, "channel"))
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
					ce->file->filename, ce->line_number);
				return 1;
			}
			return errors;
		}
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (strcmp(cep->name, "options") &&
		    strcmp(cep->name, "mask") &&
		    config_is_blankorempty(cep, "allow"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->name, "ip"))
		{
			if (has_ip)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow::ip");
				continue;
			}
			has_ip = 1;
		}
		else if (!strcmp(cep->name, "hostname"))
		{
			if (has_hostname)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow::hostname");
				continue;
			}
			has_hostname = 1;
			if (!strcmp(cep->value, "*@*") || !strcmp(cep->value, "*"))
				hostname_possible_silliness = 1;
		}
		else if (!strcmp(cep->name, "mask"))
		{
			has_mask = 1;
		}
		else if (!strcmp(cep->name, "maxperip"))
		{
			int v = atoi(cep->value);
			if (has_maxperip)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow::maxperip");
				continue;
			}
			has_maxperip = 1;
			if ((v <= 0) || (v > 1000000))
			{
				config_error("%s:%i: allow::maxperip with illegal value (must be 1-1000000)",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "global-maxperip"))
		{
			int v = atoi(cep->value);
			if (has_global_maxperip)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow::global-maxperip");
				continue;
			}
			has_global_maxperip = 1;
			if ((v <= 0) || (v > 1000000))
			{
				config_error("%s:%i: allow::global-maxperip with illegal value (must be 1-1000000)",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "ipv6-clone-mask"))
		{
			/* keep this in sync with _test_set() */
			int ipv6mask;
			ipv6mask = atoi(cep->value);
			if (ipv6mask == 0)
			{
				config_error("%s:%d: allow::ipv6-clone-mask given a value of zero. This cannnot be correct, as it would treat all IPv6 hosts as one host.",
					     cep->file->filename, cep->line_number);
				errors++;
			}
			if (ipv6mask > 128)
			{
				config_error("%s:%d: set::default-ipv6-clone-mask was set to %d. The maximum value is 128.",
					     cep->file->filename, cep->line_number,
					     ipv6mask);
				errors++;
			}
			if (ipv6mask <= 32)
			{
				config_warn("%s:%d: allow::ipv6-clone-mask was given a very small value.",
					    cep->file->filename, cep->line_number);
			}
		}
		else if (!strcmp(cep->name, "password"))
		{
			if (has_password)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow::password");
				continue;
			}
			has_password = 1;
			/* some auth check stuff? */
			if (Auth_CheckError(cep) < 0)
				errors++;
		}
		else if (!strcmp(cep->name, "class"))
		{
			if (has_class)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow::class");
				continue;
			}
			has_class = 1;
		}
		else if (!strcmp(cep->name, "redirect-server"))
		{
			if (has_redirectserver)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow::redirect-server");
				continue;
			}
			has_redirectserver = 1;
		}
		else if (!strcmp(cep->name, "redirect-port"))
		{
			if (has_redirectport)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow::redirect-port");
				continue;
			}
			has_redirectport = 1;
		}
		else if (!strcmp(cep->name, "options"))
		{
			if (has_options)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow::options");
				continue;
			}
			has_options = 1;
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "noident"))
				{}
				else if (!strcmp(cepp->name, "useip"))
				{}
				else if (!strcmp(cepp->name, "ssl") || !strcmp(cepp->name, "tls"))
				{}
				else if (!strcmp(cepp->name, "reject-on-auth-failure"))
				{}
				else if (!strcmp(cepp->name, "sasl"))
				{
					config_error("%s:%d: The option allow::options::sasl no longer exists. "
					             "Please use a require authentication { } block instead, which "
					             "is more flexible and provides the same functionality. See "
					             "https://www.unrealircd.org/docs/Require_authentication_block",
					             cepp->file->filename, cepp->line_number);
					errors++;
				}
				else
				{
					config_error_unknownopt(cepp->file->filename,
						cepp->line_number, "allow", cepp->name);
					errors++;
				}
			}
		}
		else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"allow", cep->name);
			errors++;
			continue;
		}
	}

	if (has_mask && (has_ip || has_hostname))
	{
		config_error("%s:%d: The allow block uses allow::mask, but you also have an allow::ip and allow::hostname.",
			ce->file->filename, ce->line_number);
		config_error("Please delete your allow::ip and allow::hostname entries and/or integrate them into allow::mask");
	} else
	if (has_ip)
	{
		config_warn("%s:%d: The allow block uses allow::mask nowadays. Rename your allow::ip item to allow::mask.",
			ce->file->filename, ce->line_number);
		config_warn("See https://www.unrealircd.org/docs/FAQ#allow-mask for more information");
	} else
	if (has_hostname)
	{
		config_warn("%s:%d: The allow block uses allow::mask nowadays. Rename your allow::hostname item to allow::mask.",
			ce->file->filename, ce->line_number);
		config_warn("See https://www.unrealircd.org/docs/FAQ#allow-mask for more information");
	} else
	if (!has_mask)
	{
		config_error("%s:%d: allow block needs an allow::mask",
				 ce->file->filename, ce->line_number);
		errors++;
	}

	if (has_ip && has_hostname)
	{
		config_error("%s:%d: allow block has both allow::ip and allow::hostname, this is no longer permitted.",
		             ce->file->filename, ce->line_number);
		config_error("Please integrate your allow::ip and allow::hostname items into a single allow::mask block");
		errors++;
	} else
	if (hostname_possible_silliness)
	{
		config_error("%s:%d: allow block contains 'hostname *;'. This means means that users "
		             "without a valid hostname (unresolved IP's) will be unable to connect. "
		             "You most likely want to use 'mask *;' instead.",
		             ce->file->filename, ce->line_number);
	}

	if (!has_class)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"allow::class");
		errors++;
	}

	if (!has_maxperip)
	{
		config_error_missing(ce->file->filename, ce->line_number,
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
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "class"))
			class = cep->value;
		else if (!strcmp(cep->name, "mask"))
			mask = cep;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "channel"))
		{
			/* This way, we permit multiple ::channel items in one allow block */
			allow = safe_alloc(sizeof(ConfigItem_allow_channel));
			safe_strdup(allow->channel, cep->value);
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
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (config_is_blankorempty(cep, "allow channel"))
		{
			errors++;
			continue;
		}

		if (!strcmp(cep->name, "channel"))
		{
			has_channel = 1;
		}
		else if (!strcmp(cep->name, "class"))
		{

			if (has_class)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow channel::class");
				continue;
			}
			has_class = 1;
		}
		else if (!strcmp(cep->name, "mask"))
		{
		}
		else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"allow channel", cep->name);
			errors++;
		}
	}
	if (!has_channel)
	{
		config_error_missing(ce->file->filename, ce->line_number,
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

	if (!ce->value)
	{
		config_error("%s:%i: except without type",
			ce->file->filename, ce->line_number);
		return 1;
	}

	if (!strcmp(ce->value, "tkl"))
	{
		config_warn("%s:%i: except tkl { } is now called except ban { }. "
		            "Simply rename the block from 'except tkl' to 'except ban' "
		            "to get rid of this warning.",
		            ce->file->filename, ce->line_number);
		safe_strdup(ce->value, "ban"); /* awww */
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
			ce->file->filename, ce->line_number,
			ce->value);
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "vhost"))
		{
			char *user, *host;
			user = strtok(cep->value, "@");
			host = strtok(NULL, "");
			if (!host)
				safe_strdup(vhost->virthost, user);
			else
			{
				safe_strdup(vhost->virtuser, user);
				safe_strdup(vhost->virthost, host);
			}
		}
		else if (!strcmp(cep->name, "login"))
			safe_strdup(vhost->login, cep->value);
		else if (!strcmp(cep->name, "password"))
			vhost->auth = AuthBlockToAuthConfig(cep);
		else if (!strcmp(cep->name, "mask"))
		{
			unreal_add_masks(&vhost->mask, cep);
		}
		else if (!strcmp(cep->name, "swhois"))
		{
			SWhois *s;
			if (cep->items)
			{
				for (cepp = cep->items; cepp; cepp = cepp->next)
				{
					s = safe_alloc(sizeof(SWhois));
					safe_strdup(s->line, cepp->name);
					safe_strdup(s->setby, "vhost");
					AddListItem(s, vhost->swhois);
				}
			} else
			if (cep->value)
			{
				s = safe_alloc(sizeof(SWhois));
				safe_strdup(s->line, cep->value);
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "vhost"))
		{
			char *at, *tmp, *host;
			if (has_vhost)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "vhost::vhost");
				continue;
			}
			has_vhost = 1;
			if (!cep->value)
			{
				config_error_empty(cep->file->filename,
					cep->line_number, "vhost", "vhost");
				errors++;
				continue;
			}
			if ((at = strchr(cep->value, '@')))
			{
				for (tmp = cep->value; tmp != at; tmp++)
				{
					if (*tmp == '~' && tmp == cep->value)
						continue;
					if (!isallowed(*tmp))
						break;
				}
				if (tmp != at)
				{
					config_error("%s:%i: vhost::vhost contains an invalid ident",
						cep->file->filename, cep->line_number);
					errors++;
				}
				host = at+1;
			}
			else
				host = cep->value;
			if (!*host)
			{
				config_error("%s:%i: vhost::vhost does not have a host set",
					cep->file->filename, cep->line_number);
				errors++;
			}
			else
			{
				if (!valid_host(host, 0))
				{
					config_error("%s:%i: vhost::vhost contains an invalid host",
						cep->file->filename, cep->line_number);
					errors++;
				}
			}
		}
		else if (!strcmp(cep->name, "login"))
		{
			if (has_login)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "vhost::login");
			}
			has_login = 1;
			if (!cep->value)
			{
				config_error_empty(cep->file->filename,
					cep->line_number, "vhost", "login");
				errors++;
				continue;
			}
		}
		else if (!strcmp(cep->name, "password"))
		{
			if (has_password)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "vhost::password");
			}
			has_password = 1;
			if (!cep->value)
			{
				config_error_empty(cep->file->filename,
					cep->line_number, "vhost", "password");
				errors++;
				continue;
			}
			if (Auth_CheckError(cep) < 0)
				errors++;
		}
		else if (!strcmp(cep->name, "mask"))
		{
			has_mask = 1;
		}
		else if (!strcmp(cep->name, "swhois"))
		{
			/* multiple is ok */
		}
		else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"vhost", cep->name);
			errors++;
		}
	}
	if (!has_vhost)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"vhost::vhost");
		errors++;
	}
	if (!has_login)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"vhost::login");
		errors++;

	}
	if (!has_password)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"vhost::password");
		errors++;
	}
	if (!has_mask)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"vhost::mask");
		errors++;
	}
	return errors;
}

int	_test_sni(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	ConfigEntry *cep;

	if (!ce->value)
	{
		config_error("%s:%i: sni block needs a name, eg: sni irc.xyz.com {",
			ce->file->filename, ce->line_number);
		errors++;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "ssl-options") || !strcmp(cep->name, "tls-options"))
		{
			test_tlsblock(conf, cep, &errors);
		} else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"sni", cep->name);
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

	name = ce->value;
	if (!name)
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "ssl-options") || !strcmp(cep->name, "tls-options"))
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

	if (!ce->value)
		ca->command = NULL;
	else
		safe_strdup(ca->command, ce->value);

	for (cep = ce->items; cep; cep = cep->next)
	{
		temp = safe_alloc(sizeof(MOTDLine));
		safe_strdup(temp->line, cep->name);
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
	if (!ce->items)
	{
		config_error("%s:%i: empty help block",
			ce->file->filename, ce->line_number);
		return 1;
	}
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (strlen(cep->name) > 500)
		{
			config_error("%s:%i: oversized help item",
				ce->file->filename, ce->line_number);
			errors++;
			continue;
		}
	}
	return errors;
}

int	_conf_link(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp, *ceppp;
	ConfigItem_link *link = NULL;

	link = safe_alloc(sizeof(ConfigItem_link));
	safe_strdup(link->servername, ce->value);

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "incoming"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "mask"))
				{
					unreal_add_masks(&link->incoming.mask, cepp);
				}
			}
		}
		else if (!strcmp(cep->name, "outgoing"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "bind-ip"))
					safe_strdup(link->outgoing.bind_ip, cepp->value);
				else if (!strcmp(cepp->name, "file"))
					safe_strdup(link->outgoing.file, cepp->value);
				else if (!strcmp(cepp->name, "hostname"))
					safe_strdup(link->outgoing.hostname, cepp->value);
				else if (!strcmp(cepp->name, "port"))
					link->outgoing.port = atoi(cepp->value);
				else if (!strcmp(cepp->name, "options"))
				{
					link->outgoing.options = 0;
					for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
					{
						long v;
						if ((v = nv_find_by_name(_LinkFlags, ceppp->name)))
							link->outgoing.options |= v;
					}
				}
				else if (!strcmp(cepp->name, "ssl-options") || !strcmp(cepp->name, "tls-options"))
				{
					link->tls_options = safe_alloc(sizeof(TLSOptions));
					conf_tlsblock(conf, cepp, link->tls_options);
					link->ssl_ctx = init_ctx(link->tls_options, 0);
				}
			}
		}
		else if (!strcmp(cep->name, "password"))
			link->auth = AuthBlockToAuthConfig(cep);
		else if (!strcmp(cep->name, "hub"))
			safe_strdup(link->hub, cep->value);
		else if (!strcmp(cep->name, "leaf"))
			safe_strdup(link->leaf, cep->value);
		else if (!strcmp(cep->name, "leaf-depth") || !strcmp(cep->name, "leafdepth"))
			link->leaf_depth = atoi(cep->value);
		else if (!strcmp(cep->name, "class"))
		{
			link->class = find_class(cep->value);
			if (!link->class || (link->class->flag.temporary == 1))
			{
				config_status("%s:%i: illegal link::class, unknown class '%s' using default of class 'default'",
					cep->file->filename,
					cep->line_number,
					cep->value);
				link->class = default_class;
			}
			link->class->xrefcount++;
		}
		else if (!strcmp(cep->name, "verify-certificate"))
		{
			link->verify_certificate = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "options"))
		{
			link->options = 0;
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				long v;
				if ((v = nv_find_by_name(_LinkFlags, cepp->name)))
					link->options |= v;
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
 */
int config_detect_duplicate(int *var, ConfigEntry *ce, int *errors)
{
	if (*var)
	{
		config_error("%s:%d: Duplicate %s directive",
			ce->file->filename, ce->line_number,
			ce->name);
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

	int has_incoming = 0, has_incoming_mask = 0, has_outgoing = 0, has_outgoing_file = 0;
	int has_outgoing_bind_ip = 0, has_outgoing_hostname = 0, has_outgoing_port = 0;
	int has_outgoing_options = 0, has_hub = 0, has_leaf = 0, has_leaf_depth = 0;
	int has_password = 0, has_class = 0, has_options = 0;

	if (!ce->value)
	{
		config_error("%s:%i: link without servername. Expected: link servername { ... }",
			ce->file->filename, ce->line_number);
		return 1;

	}

	if (!strchr(ce->value, '.'))
	{
		config_error("%s:%i: link: bogus server name. Expected: link servername { ... }",
			ce->file->filename, ce->line_number);
		return 1;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "incoming"))
		{
			config_detect_duplicate(&has_incoming, cep, &errors);
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "mask"))
				{
					if (cepp->value || cepp->items)
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
		else if (!strcmp(cep->name, "outgoing"))
		{
			config_detect_duplicate(&has_outgoing, cep, &errors);
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "bind-ip"))
				{
					if (config_is_blankorempty(cepp, "link::outgoing"))
					{
						errors++;
						continue;
					}
					config_detect_duplicate(&has_outgoing_bind_ip, cepp, &errors);
					// todo: ipv4 vs ipv6
				}
				else if (!strcmp(cepp->name, "file"))
				{
					if (config_is_blankorempty(cepp, "link::outgoing"))
					{
						errors++;
						continue;
					}
					config_detect_duplicate(&has_outgoing_file, cepp, &errors);
				}
				else if (!strcmp(cepp->name, "hostname"))
				{
					if (config_is_blankorempty(cepp, "link::outgoing"))
					{
						errors++;
						continue;
					}
					config_detect_duplicate(&has_outgoing_hostname, cepp, &errors);
					if (strchr(cepp->value, '*') || strchr(cepp->value, '?'))
					{
						config_error("%s:%i: hostname in link::outgoing(!) cannot contain wildcards",
							cepp->file->filename, cepp->line_number);
						errors++;
					}
				}
				else if (!strcmp(cepp->name, "port"))
				{
					if (config_is_blankorempty(cepp, "link::outgoing"))
					{
						errors++;
						continue;
					}
					config_detect_duplicate(&has_outgoing_port, cepp, &errors);
				}
				else if (!strcmp(cepp->name, "options"))
				{
					config_detect_duplicate(&has_outgoing_options, cepp, &errors);
					for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
					{
						if (!strcmp(ceppp->name, "autoconnect"))
							;
						else if (!strcmp(ceppp->name, "ssl") || !strcmp(ceppp->name, "tls"))
							;
						else if (!strcmp(ceppp->name, "insecure"))
							;
						else
						{
							config_error_unknownopt(ceppp->file->filename,
								ceppp->line_number, "link::outgoing", ceppp->name);
							errors++;
						}
					}
				}
				else if (!strcmp(cepp->name, "ssl-options") || !strcmp(cepp->name, "tls-options"))
				{
					test_tlsblock(conf, cepp, &errors);
				}
				else
				{
					config_error("%s:%d: Unknown directive '%s'",
					             cepp->file->filename, cepp->line_number,
					             config_var(cepp));
					errors++;
				}
			}
		}
		else if (!strcmp(cep->name, "password"))
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
					             "certificate or SPKI fingerprint of the remote link (=better)",
					             cep->file->filename, cep->line_number);
					errors++;
				}
				Auth_FreeAuthConfig(auth);
			}
		}
		else if (!strcmp(cep->name, "hub"))
		{
			if (config_is_blankorempty(cep, "link"))
			{
				errors++;
				continue;
			}
			config_detect_duplicate(&has_hub, cep, &errors);
		}
		else if (!strcmp(cep->name, "leaf"))
		{
			if (config_is_blankorempty(cep, "link"))
			{
				errors++;
				continue;
			}
			config_detect_duplicate(&has_leaf, cep, &errors);
		}
		else if (!strcmp(cep->name, "leaf-depth") || !strcmp(cep->name, "leafdepth"))
		{
			if (config_is_blankorempty(cep, "link"))
			{
				errors++;
				continue;
			}
			config_detect_duplicate(&has_leaf_depth, cep, &errors);
		}
		else if (!strcmp(cep->name, "class"))
		{
			if (config_is_blankorempty(cep, "link"))
			{
				errors++;
				continue;
			}
			config_detect_duplicate(&has_class, cep, &errors);
		}
		else if (!strcmp(cep->name, "ciphers"))
		{
			config_error("%s:%d: link::ciphers has been moved to link::outgoing::ssl-options::ciphers, "
			             "see https://www.unrealircd.org/docs/FAQ#link::ciphers_no_longer_works",
			             cep->file->filename, cep->line_number);
			errors++;
		}
		else if (!strcmp(cep->name, "verify-certificate"))
		{
			if (config_is_blankorempty(cep, "link"))
			{
				errors++;
				continue;
			}
		}
		else if (!strcmp(cep->name, "options"))
		{
			config_detect_duplicate(&has_options, cep, &errors);
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "quarantine"))
					;
				else
				{
					config_error("%s:%d: link::options only has one possible option ('quarantine', rarely used). "
					             "Option '%s' is unrecognized. "
					             "Perhaps you meant to set an outgoing option in link::outgoing::options instead?",
					             cepp->file->filename, cepp->line_number, cepp->name);
					errors++;
				}
			}
		}
		else
		{
			config_error_unknown(cep->file->filename,
			    cep->line_number, "link", cep->name);
			errors++;
			continue;
		}
	}

	if (!has_incoming && !has_outgoing)
	{
		config_error("%s:%d: link block needs at least an incoming or outgoing section.",
			ce->file->filename, ce->line_number);
		errors++;
	}

	if (has_incoming)
	{
		/* If we have an incoming sub-block then we need at least 'mask' and 'password' */
		if (!has_incoming_mask)
		{
			config_error_missing(ce->file->filename, ce->line_number, "link::incoming::mask");
			errors++;
		}
	}

	if (has_outgoing)
	{
		/* If we have an outgoing sub-block then we need at least a hostname and port or a file */
		if (!has_outgoing_file)
		{
			if (!has_outgoing_hostname)
			{
				config_error_missing(ce->file->filename, ce->line_number, "link::outgoing::hostname");
				errors++;
			}
			if (!has_outgoing_port)
			{
				config_error_missing(ce->file->filename, ce->line_number, "link::outgoing::port");
				errors++;
			}
		}
		else if (has_outgoing_file && (has_outgoing_hostname || has_outgoing_port))
		{
			config_error("%s:%d: link block should either have a 'file' (for *NIX domain socket), "
			             "OR have a 'hostname' and 'port' (for IPv4/IPv6). You cannot combine both in one link block.",
			             ce->file->filename, ce->line_number);
			errors++;
		}
	}

	/* The only other generic options that are required are 'class' and 'password' */
	if (!has_password)
	{
		config_error_missing(ce->file->filename, ce->line_number, "link::password");
		errors++;
	}
	if (!has_class)
	{
		config_error_missing(ce->file->filename, ce->line_number,
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
	if (!strcmp(ce->value, "realname"))
		ca->flag.type = CONF_BAN_REALNAME;
	else if (!strcmp(ce->value, "server"))
		ca->flag.type = CONF_BAN_SERVER;
	else if (!strcmp(ce->value, "version"))
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
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "mask"))
		{
			safe_strdup(ca->mask, cep->value);
		}
		else if (!strcmp(cep->name, "reason"))
			safe_strdup(ca->reason, cep->value);
		else if (!strcmp(cep->name, "action"))
			ca->action = banact_stringtoval(cep->value);
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

	if (!ce->value)
	{
		config_error("%s:%i: ban without type",
			ce->file->filename, ce->line_number);
		return 1;
	}
	else if (!strcmp(ce->value, "server"))
	{}
	else if (!strcmp(ce->value, "realname"))
	{}
	else if (!strcmp(ce->value, "version"))
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
				ce->file->filename, ce->line_number,
				ce->value);
			return 1;
		}
		return errors;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (config_is_blankorempty(cep, "ban"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->name, "mask"))
		{
			if (has_mask)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "ban::mask");
				continue;
			}
			has_mask = 1;
		}
		else if (!strcmp(cep->name, "reason"))
		{
			if (has_reason)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "ban::reason");
				continue;
			}
			has_reason = 1;
		}
		else if (!strcmp(cep->name, "action"))
		{
			if (has_action)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "ban::action");
			}
			has_action = 1;
			if (!banact_stringtoval(cep->value))
			{
				config_error("%s:%i: ban::action has unknown action type '%s'",
					cep->file->filename, cep->line_number,
					cep->value);
				errors++;
			}
		}
	}

	if (!has_mask)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"ban::mask");
		errors++;
	}
	if (!has_reason)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"ban::reason");
		errors++;
	}
	if (has_action && type != 'v')
	{
		config_error("%s:%d: ban::action specified even though type is not 'version'",
			ce->file->filename, ce->line_number);
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

	if (strcmp(ce->value, "authentication") && strcmp(ce->value, "sasl"))
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "mask"))
		{
			char buf[512], *p;
			strlcpy(buf, cep->value, sizeof(buf));
			p = strchr(buf, '@');
			if (p)
			{
				*p++ = '\0';
				safe_strdup(usermask, buf);
				safe_strdup(hostmask, p);
			} else {
				safe_strdup(hostmask, cep->value);
			}
		}
		else if (!strcmp(cep->name, "reason"))
			safe_strdup(reason, cep->value);
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

	if (!ce->value)
	{
		config_error("%s:%i: require without type, did you mean 'require authentication'?",
			ce->file->filename, ce->line_number);
		return 1;
	}
	if (!strcmp(ce->value, "authentication"))
	{}
	else if (!strcmp(ce->value, "sasl"))
	{
		config_warn("%s:%i: the 'require sasl' block is now called 'require authentication'",
		            ce->file->filename, ce->line_number);
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
				ce->file->filename, ce->line_number,
				ce->value);
			return 1;
		}
		return errors;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (config_is_blankorempty(cep, "require"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->name, "mask"))
		{
			if (has_mask)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "require::mask");
				continue;
			}
			has_mask = 1;
		}
		else if (!strcmp(cep->name, "reason"))
		{
			if (has_reason)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "require::reason");
				continue;
			}
			has_reason = 1;
		}
	}

	if (!has_mask)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"require::mask");
		errors++;
	}
	if (!has_reason)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"require::reason");
		errors++;
	}
	return errors;
}

#define CheckDuplicate(cep, name, display) if (settings.has_##name) { config_warn_duplicate((cep)->file->filename, cep->line_number, "set::" display); continue; } else settings.has_##name = 1

void test_tlsblock(ConfigFile *conf, ConfigEntry *cep, int *totalerrors)
{
	ConfigEntry *cepp, *ceppp;
	int errors = 0;

	for (cepp = cep->items; cepp; cepp = cepp->next)
	{
		if (!strcmp(cepp->name, "renegotiate-timeout"))
		{
		}
		else if (!strcmp(cepp->name, "renegotiate-bytes"))
		{
		}
		else if (!strcmp(cepp->name, "ciphers") || !strcmp(cepp->name, "server-cipher-list"))
		{
			CheckNull(cepp);
		}
		else if (!strcmp(cepp->name, "ciphersuites"))
		{
			CheckNull(cepp);
		}
		else if (!strcmp(cepp->name, "ecdh-curves"))
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
		else if (!strcmp(cepp->name, "protocols"))
		{
			char copy[512], *p, *name;
			int v = 0;
			int option;
			char modifier;

			CheckNull(cepp);
			strlcpy(copy, cepp->value, sizeof(copy));
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
								 cepp->file->filename, cepp->line_number, config_var(cepp), name);
#else
					config_warn("%s:%i: %s: unknown protocol '%s'. "
								 "Valid protocols are: TLSv1,TLSv1.1,TLSv1.2",
								 cepp->file->filename, cepp->line_number, config_var(cepp), name);
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
					cepp->file->filename, cepp->line_number, config_var(cepp));
				errors++;
			}
		}
		else if (!strcmp(cepp->name, "certificate") ||
		         !strcmp(cepp->name, "key") ||
		         !strcmp(cepp->name, "trusted-ca-file"))
		{
			char *path;
			CheckNull(cepp);
			path = convert_to_absolute_path_duplicate(cepp->value, CONFDIR);
			if (!file_exists(path))
			{
				config_error("%s:%i: %s: could not open '%s': %s",
					cepp->file->filename, cepp->line_number, config_var(cepp),
					path, strerror(errno));
				safe_free(path);
				errors++;
			}
			safe_free(path);
		}
		else if (!strcmp(cepp->name, "dh"))
		{
			/* Support for this undocumented option was silently dropped in 5.0.0.
			 * Since 5.0.7 we print a warning about it, since you never know
			 * someone may still have it configured. -- Syzop
			 */
			config_warn("%s:%d: Not reading DH file '%s'. UnrealIRCd does not support old DH(E), we use modern ECDHE/EECDH. "
			            "Just remove the 'dh' directive from your config file to get rid of this warning.",
				cepp->file->filename, cepp->line_number,
				cepp->value ? cepp->value : "");
		}
		else if (!strcmp(cepp->name, "outdated-protocols"))
		{
			char copy[512], *p, *name;
			int v = 0;
			int option;
			char modifier;

			CheckNull(cepp);
			strlcpy(copy, cepp->value, sizeof(copy));
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
								 cepp->file->filename, cepp->line_number, config_var(cepp), name);
#else
					config_warn("%s:%i: %s: unknown protocol '%s'. "
								 "Valid protocols are: TLSv1,TLSv1.1,TLSv1.2",
								 cepp->file->filename, cepp->line_number, config_var(cepp), name);
#endif
		                }
			}
		}
		else if (!strcmp(cepp->name, "outdated-ciphers"))
		{
			CheckNull(cepp);
		}
		else if (!strcmp(cepp->name, "options"))
		{
			for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
			{
				if (!nv_find_by_name(_TLSFlags, ceppp->name))
				{
					config_error("%s:%i: unknown TLS option '%s'",
							 ceppp->file->filename,
							 ceppp->line_number, ceppp->name);
					errors ++;
				}
			}
		}
		else if (!strcmp(cepp->name, "sts-policy"))
		{
			int has_port = 0;
			int has_duration = 0;

			for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
			{
				if (!strcmp(ceppp->name, "port"))
				{
					int port;
					CheckNull(ceppp);
					port = atoi(ceppp->value);
					if ((port < 1) || (port > 65535))
					{
						config_error("%s:%i: invalid port number specified in sts-policy::port (%d)",
						             ceppp->file->filename, ceppp->line_number, port);
						errors++;
					}
					has_port = 1;
				}
				else if (!strcmp(ceppp->name, "duration"))
				{
					long duration;
					CheckNull(ceppp);
					duration = config_checkval(ceppp->value, CFG_TIME);
					if (duration < 1)
					{
						config_error("%s:%i: invalid duration specified in sts-policy::duration (%ld seconds)",
						             ceppp->file->filename, ceppp->line_number, duration);
						errors++;
					}
					has_duration = 1;
				}
				else if (!strcmp(ceppp->name, "preload"))
				{
					CheckNull(ceppp);
				}
			}
			if (!has_port)
			{
				config_error("%s:%i: sts-policy block without port",
				             cepp->file->filename, cepp->line_number);
				errors++;
			}
			if (!has_duration)
			{
				config_error("%s:%i: sts-policy block without duration",
				             cepp->file->filename, cepp->line_number);
				errors++;
			}
		}
		else
		{
			config_error("%s:%i: unknown directive %s",
				cepp->file->filename, cepp->line_number,
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
	for (cepp = cep->items; cepp; cepp = cepp->next)
	{
		if (!strcmp(cepp->name, "ciphers") || !strcmp(cepp->name, "server-cipher-list"))
		{
			safe_strdup(tlsoptions->ciphers, cepp->value);
		}
		else if (!strcmp(cepp->name, "ciphersuites"))
		{
			safe_strdup(tlsoptions->ciphersuites, cepp->value);
		}
		else if (!strcmp(cepp->name, "ecdh-curves"))
		{
			safe_strdup(tlsoptions->ecdh_curves, cepp->value);
		}
		else if (!strcmp(cepp->name, "protocols"))
		{
			char copy[512], *p, *name;
			int option;
			char modifier;

			strlcpy(copy, cepp->value, sizeof(copy));
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
		else if (!strcmp(cepp->name, "certificate"))
		{
			convert_to_absolute_path(&cepp->value, CONFDIR);
			safe_strdup(tlsoptions->certificate_file, cepp->value);
		}
		else if (!strcmp(cepp->name, "key"))
		{
			convert_to_absolute_path(&cepp->value, CONFDIR);
			safe_strdup(tlsoptions->key_file, cepp->value);
		}
		else if (!strcmp(cepp->name, "trusted-ca-file"))
		{
			convert_to_absolute_path(&cepp->value, CONFDIR);
			safe_strdup(tlsoptions->trusted_ca_file, cepp->value);
		}
		else if (!strcmp(cepp->name, "outdated-protocols"))
		{
			safe_strdup(tlsoptions->outdated_protocols, cepp->value);
		}
		else if (!strcmp(cepp->name, "outdated-ciphers"))
		{
			safe_strdup(tlsoptions->outdated_ciphers, cepp->value);
		}
		else if (!strcmp(cepp->name, "renegotiate-bytes"))
		{
			tlsoptions->renegotiate_bytes = config_checkval(cepp->value, CFG_SIZE);
		}
		else if (!strcmp(cepp->name, "renegotiate-timeout"))
		{
			tlsoptions->renegotiate_timeout = config_checkval(cepp->value, CFG_TIME);
		}
		else if (!strcmp(cepp->name, "options"))
		{
			tlsoptions->options = 0;
			for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
			{
				long v = nv_find_by_name(_TLSFlags, ceppp->name);
				tlsoptions->options |= v;
			}
		}
		else if (!strcmp(cepp->name, "sts-policy"))
		{
			/* We do not inherit ::sts-policy if there is a specific block for this one... */
			tlsoptions->sts_port = 0;
			tlsoptions->sts_duration = 0;
			tlsoptions->sts_preload = 0;
			for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
			{
				if (!strcmp(ceppp->name, "port"))
					tlsoptions->sts_port = atoi(ceppp->value);
				else if (!strcmp(ceppp->name, "duration"))
					tlsoptions->sts_duration = config_checkval(ceppp->value, CFG_TIME);
				else if (!strcmp(ceppp->name, "preload"))
					tlsoptions->sts_preload = config_checkval(ceppp->value, CFG_YESNO);
			}
		}
	}
}

int	_conf_set(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp, *ceppp, *cep4;
	Hook *h;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "kline-address")) {
			safe_strdup(tempiConf.kline_address, cep->value);
		}
		if (!strcmp(cep->name, "gline-address")) {
			safe_strdup(tempiConf.gline_address, cep->value);
		}
		else if (!strcmp(cep->name, "modes-on-connect")) {
			tempiConf.conn_modes = (long) set_usermode(cep->value);
		}
		else if (!strcmp(cep->name, "modes-on-oper")) {
			tempiConf.oper_modes = (long) set_usermode(cep->value);
		}
		else if (!strcmp(cep->name, "modes-on-join")) {
			conf_channelmodes(cep->value, &tempiConf.modes_on_join);
			tempiConf.modes_on_join_set = 1;
		}
		else if (!strcmp(cep->name, "snomask-on-oper")) {
			safe_strdup(tempiConf.oper_snomask, cep->value);
		}
		else if (!strcmp(cep->name, "server-notice-colors")) {
			tempiConf.server_notice_colors = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "server-notice-show-event")) {
			tempiConf.server_notice_show_event = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "level-on-join")) {
			const char *res = channellevel_to_string(cep->value); /* 'halfop', etc */
			if (!res)
			{
				/* This check needs to be here, in config run, because
				 * now the channel modules are initialized and we know
				 * which ones are available. This same information is
				 * not available during config test, so we can't test
				 * for it there like we normally do.
				 */
				if (!valid_channel_access_mode_letter(*cep->value))
				{
					config_warn("%s:%d: set::level-on-join: Unknown mode (access level) '%c'. "
					            "That mode does not exist or is not a valid access mode "
					            "like vhoaq.",
					            cep->file->filename, cep->line_number,
					            *cep->value);
					config_warn("Falling back to to set::level-on-join none; now. "
					            "This is probably not what you want!!!");
				}
				res = cep->value; /* if we reach this.. then it is a single letter */
			}
			safe_strdup(tempiConf.level_on_join, res);
		}
		else if (!strcmp(cep->name, "static-quit")) {
			safe_strdup(tempiConf.static_quit, cep->value);
		}
		else if (!strcmp(cep->name, "static-part")) {
			safe_strdup(tempiConf.static_part, cep->value);
		}
		else if (!strcmp(cep->name, "who-limit")) {
			tempiConf.who_limit = atol(cep->value);
		}
		else if (!strcmp(cep->name, "maxbans")) {
			tempiConf.maxbans = atol(cep->value);
		}
		else if (!strcmp(cep->name, "maxbanlength")) {
			tempiConf.maxbanlength = atol(cep->value);
		}
		else if (!strcmp(cep->name, "silence-limit")) {
			tempiConf.silence_limit = atol(cep->value);
		}
		else if (!strcmp(cep->name, "auto-join")) {
			safe_strdup(tempiConf.auto_join_chans, cep->value);
		}
		else if (!strcmp(cep->name, "oper-auto-join")) {
			safe_strdup(tempiConf.oper_auto_join_chans, cep->value);
		}
		else if (!strcmp(cep->name, "check-target-nick-bans")) {
			tempiConf.check_target_nick_bans = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "ping-cookie")) {
			tempiConf.ping_cookie = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "watch-away-notification")) {
			tempiConf.watch_away_notification = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "uhnames")) {
			tempiConf.uhnames = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "allow-userhost-change")) {
			if (!strcasecmp(cep->value, "always"))
				tempiConf.userhost_allowed = UHALLOW_ALWAYS;
			else if (!strcasecmp(cep->value, "never"))
				tempiConf.userhost_allowed = UHALLOW_NEVER;
			else if (!strcasecmp(cep->value, "not-on-channels"))
				tempiConf.userhost_allowed = UHALLOW_NOCHANS;
			else
				tempiConf.userhost_allowed = UHALLOW_REJOIN;
		}
		else if (!strcmp(cep->name, "channel-command-prefix")) {
			safe_strdup(tempiConf.channel_command_prefix, cep->value);
		}
		else if (!strcmp(cep->name, "restrict-usermodes")) {
			int i;
			char *p = safe_alloc(strlen(cep->value) + 1), *x = p;
			/* The data should be something like 'Gw' or something,
			 * but just in case users use '+Gw' then ignore the + (and -).
			 */
			for (i=0; i < strlen(cep->value); i++)
				if ((cep->value[i] != '+') && (cep->value[i] != '-'))
					*x++ = cep->value[i];
			*x = '\0';
			tempiConf.restrict_usermodes = p;
		}
		else if (!strcmp(cep->name, "restrict-channelmodes")) {
			int i;
			char *p = safe_alloc(strlen(cep->value) + 1), *x = p;
			/* The data should be something like 'GL' or something,
			 * but just in case users use '+GL' then ignore the + (and -).
			 */
			for (i=0; i < strlen(cep->value); i++)
				if ((cep->value[i] != '+') && (cep->value[i] != '-'))
					*x++ = cep->value[i];
			*x = '\0';
			tempiConf.restrict_channelmodes = p;
		}
		else if (!strcmp(cep->name, "restrict-extendedbans")) {
			safe_strdup(tempiConf.restrict_extendedbans, cep->value);
		}
		else if (!strcmp(cep->name, "named-extended-bans")) {
			tempiConf.named_extended_bans = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "anti-spam-quit-message-time")) {
			tempiConf.anti_spam_quit_message_time = config_checkval(cep->value,CFG_TIME);
		}
		else if (!strcmp(cep->name, "allow-user-stats")) {
			if (!cep->items)
			{
				safe_strdup(tempiConf.allow_user_stats, cep->value);
			}
			else
			{
				for (cepp = cep->items; cepp; cepp = cepp->next)
				{
					OperStat *os = safe_alloc(sizeof(OperStat));
					safe_strdup(os->flag, cepp->name);
					AddListItem(os, tempiConf.allow_user_stats_ext);
				}
			}
		}
		else if (!strcmp(cep->name, "maxchannelsperuser")) {
			tempiConf.maxchannelsperuser = atoi(cep->value);
		}
		else if (!strcmp(cep->name, "ping-warning")) {
			tempiConf.ping_warning = atoi(cep->value);
		}
		else if (!strcmp(cep->name, "maxdccallow")) {
			tempiConf.maxdccallow = atoi(cep->value);
		}
		else if (!strcmp(cep->name, "max-targets-per-command"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				int v;
				if (!strcmp(cepp->value, "max"))
					v = MAXTARGETS_MAX;
				else
					v = atoi(cepp->value);
				setmaxtargets(cepp->name, v);
			}
		}
		else if (!strcmp(cep->name, "network-name")) {
			char *tmp;
			safe_strdup(tempiConf.network_name, cep->value);
			for (tmp = cep->value; *cep->value; cep->value++) {
				if (*cep->value == ' ')
					*cep->value='-';
			}
			safe_strdup(tempiConf.network_name_005, tmp);
			cep->value = tmp;
		}
		else if (!strcmp(cep->name, "default-server")) {
			safe_strdup(tempiConf.default_server, cep->value);
		}
		else if (!strcmp(cep->name, "services-server")) {
			safe_strdup(tempiConf.services_name, cep->value);
		}
		else if (!strcmp(cep->name, "sasl-server")) {
			safe_strdup(tempiConf.sasl_server, cep->value);
		}
		else if (!strcmp(cep->name, "stats-server")) {
			safe_strdup(tempiConf.stats_server, cep->value);
		}
		else if (!strcmp(cep->name, "help-channel")) {
			safe_strdup(tempiConf.helpchan, cep->value);
		}
		else if (!strcmp(cep->name, "cloak-prefix") || !strcmp(cep->name, "hiddenhost-prefix")) {
			safe_strdup(tempiConf.cloak_prefix, cep->value);
		}
		else if (!strcmp(cep->name, "hide-ban-reason")) {
			tempiConf.hide_ban_reason = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "prefix-quit")) {
			if (!strcmp(cep->value, "0") || !strcmp(cep->value, "no"))
				safe_free(tempiConf.prefix_quit);
			else
				safe_strdup(tempiConf.prefix_quit, cep->value);
		}
		else if (!strcmp(cep->name, "link")) {
			for (cepp = cep->items; cepp; cepp = cepp->next) {
				if (!strcmp(cepp->name, "bind-ip")) {
					safe_strdup(tempiConf.link_bindip, cepp->value);
				}
			}
		}
		else if (!strcmp(cep->name, "anti-flood")) {
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				int lag_penalty = -1;
				int lag_penalty_bytes = -1;
				for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
				{
					if (!strcmp(ceppp->name, "handshake-data-flood"))
					{
						for (cep4 = ceppp->items; cep4; cep4 = cep4->next)
						{
							if (!strcmp(cep4->name, "amount"))
								tempiConf.handshake_data_flood_amount = config_checkval(cep4->value, CFG_SIZE);
							else if (!strcmp(cep4->name, "ban-time"))
								tempiConf.handshake_data_flood_ban_time = config_checkval(cep4->value, CFG_TIME);
							else if (!strcmp(cep4->name, "ban-action"))
								tempiConf.handshake_data_flood_ban_action = banact_stringtoval(cep4->value);
						}
					}
					else if (!strcmp(ceppp->name, "away-flood"))
					{
						config_parse_flood_generic(ceppp->value, &tempiConf, cepp->name, FLD_AWAY);
					}
					else if (!strcmp(ceppp->name, "nick-flood"))
					{
						config_parse_flood_generic(ceppp->value, &tempiConf, cepp->name, FLD_NICK);
					}
					else if (!strcmp(ceppp->name, "vhost-flood"))
					{
						config_parse_flood_generic(ceppp->value, &tempiConf, cepp->name, FLD_VHOST);
					}
					else if (!strcmp(ceppp->name, "join-flood"))
					{
						config_parse_flood_generic(ceppp->value, &tempiConf, cepp->name, FLD_JOIN);
					}
					else if (!strcmp(ceppp->name, "invite-flood"))
					{
						config_parse_flood_generic(ceppp->value, &tempiConf, cepp->name, FLD_INVITE);
					}
					else if (!strcmp(ceppp->name, "knock-flood"))
					{
						config_parse_flood_generic(ceppp->value, &tempiConf, cepp->name, FLD_KNOCK);
					}
					else if (!strcmp(ceppp->name, "lag-penalty"))
					{
						lag_penalty = atoi(ceppp->value);
					}
					else if (!strcmp(ceppp->name, "lag-penalty-bytes"))
					{
						lag_penalty_bytes = config_checkval(ceppp->value, CFG_SIZE);
						if (lag_penalty_bytes <= 0)
							lag_penalty_bytes = INT_MAX;
					}
					else if (!strcmp(ceppp->name, "connect-flood"))
					{
						int cnt, period;
						config_parse_flood(ceppp->value, &cnt, &period);
						tempiConf.throttle_count = cnt;
						tempiConf.throttle_period = period;
					}
					if (!strcmp(ceppp->name, "max-concurrent-conversations"))
					{
						/* We use a hack here to make it fit our storage format */
						char buf[64];
						int users=0;
						long every=0;
						for (cep4 = ceppp->items; cep4; cep4 = cep4->next)
						{
							if (!strcmp(cep4->name, "users"))
							{
								users = atoi(cep4->value);
							} else
							if (!strcmp(cep4->name, "new-user-every"))
							{
								every = config_checkval(cep4->value, CFG_TIME);
							}
						}
						snprintf(buf, sizeof(buf), "%d:%ld", users, every);
						config_parse_flood_generic(buf, &tempiConf, cepp->name, FLD_CONVERSATIONS);
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
					config_parse_flood_generic(buf, &tempiConf, cepp->name, FLD_LAG_PENALTY);
				}
			}
		}
		else if (!strcmp(cep->name, "options")) {
			for (cepp = cep->items; cepp; cepp = cepp->next) {
				if (!strcmp(cepp->name, "hide-ulines")) {
					tempiConf.hide_ulines = 1;
				}
				else if (!strcmp(cepp->name, "flat-map")) {
					tempiConf.flat_map = 1;
				}
				else if (!strcmp(cepp->name, "show-opermotd")) {
					tempiConf.show_opermotd = 1;
				}
				else if (!strcmp(cepp->name, "identd-check")) {
					tempiConf.ident_check = 1;
				}
				else if (!strcmp(cepp->name, "fail-oper-warn")) {
					tempiConf.fail_oper_warn = 1;
				}
				else if (!strcmp(cepp->name, "show-connect-info")) {
					tempiConf.show_connect_info = 1;
				}
				else if (!strcmp(cepp->name, "no-connect-tls-info")) {
					tempiConf.no_connect_tls_info = 1;
				}
				else if (!strcmp(cepp->name, "dont-resolve")) {
					tempiConf.dont_resolve = 1;
				}
				else if (!strcmp(cepp->name, "mkpasswd-for-everyone")) {
					tempiConf.mkpasswd_for_everyone = 1;
				}
				else if (!strcmp(cepp->name, "allow-insane-bans")) {
					tempiConf.allow_insane_bans = 1;
				}
				else if (!strcmp(cepp->name, "allow-part-if-shunned")) {
					tempiConf.allow_part_if_shunned = 1;
				}
				else if (!strcmp(cepp->name, "disable-cap")) {
					tempiConf.disable_cap = 1;
				}
				else if (!strcmp(cepp->name, "disable-ipv6")) {
					/* other code handles this */
				}
			}
		}
		else if (!strcmp(cep->name, "cloak-keys"))
		{
			for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
			{
				int value;
				value = (*(h->func.intfunc))(conf, cep, CONFIG_CLOAKKEYS);
				if (value == 1)
					break;
			}
		}
		else if (!strcmp(cep->name, "ident"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "connect-timeout"))
					tempiConf.ident_connect_timeout = config_checkval(cepp->value,CFG_TIME);
				if (!strcmp(cepp->name, "read-timeout"))
					tempiConf.ident_read_timeout = config_checkval(cepp->value,CFG_TIME);
			}
		}
		else if (!strcmp(cep->name, "spamfilter"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "ban-time"))
					tempiConf.spamfilter_ban_time = config_checkval(cepp->value,CFG_TIME);
				else if (!strcmp(cepp->name, "ban-reason"))
					safe_strdup(tempiConf.spamfilter_ban_reason, cepp->value);
				else if (!strcmp(cepp->name, "virus-help-channel"))
					safe_strdup(tempiConf.spamfilter_virus_help_channel, cepp->value);
				else if (!strcmp(cepp->name, "virus-help-channel-deny"))
					tempiConf.spamfilter_vchan_deny = config_checkval(cepp->value,CFG_YESNO);
				else if (!strcmp(cepp->name, "except"))
				{
					char *name, *p;
					SpamExcept *e;
					safe_strdup(tempiConf.spamexcept_line, cepp->value);
					for (name = strtoken(&p, cepp->value, ","); name; name = strtoken(&p, NULL, ","))
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
				else if (!strcmp(cepp->name, "detect-slow-warn"))
				{
					tempiConf.spamfilter_detectslow_warn = atol(cepp->value);
				}
				else if (!strcmp(cepp->name, "detect-slow-fatal"))
				{
					tempiConf.spamfilter_detectslow_fatal = atol(cepp->value);
				}
				else if (!strcmp(cepp->name, "stop-on-first-match"))
				{
					tempiConf.spamfilter_stop_on_first_match = config_checkval(cepp->value, CFG_YESNO);
				}
			}
		}
		else if (!strcmp(cep->name, "default-bantime"))
		{
			tempiConf.default_bantime = config_checkval(cep->value,CFG_TIME);
		}
		else if (!strcmp(cep->name, "ban-version-tkl-time"))
		{
			tempiConf.ban_version_tkl_time = config_checkval(cep->value,CFG_TIME);
		}
		else if (!strcmp(cep->name, "min-nick-length")) {
			int v = atoi(cep->value);
			tempiConf.min_nick_length = v;
		}
		else if (!strcmp(cep->name, "nick-length")) {
			int v = atoi(cep->value);
			tempiConf.nick_length = v;
		}
		else if (!strcmp(cep->name, "topic-length")) {
			int v = atoi(cep->value);
			tempiConf.topic_length = v;
		}
		else if (!strcmp(cep->name, "away-length")) {
			int v = atoi(cep->value);
			tempiConf.away_length = v;
		}
		else if (!strcmp(cep->name, "kick-length")) {
			int v = atoi(cep->value);
			tempiConf.kick_length = v;
		}
		else if (!strcmp(cep->name, "quit-length")) {
			int v = atoi(cep->value);
			tempiConf.quit_length = v;
		}
		else if (!strcmp(cep->name, "ssl") || !strcmp(cep->name, "tls")) {
			/* no need to alloc tempiConf.tls_options since config_defaults() already ensures it exists */
			conf_tlsblock(conf, cep, tempiConf.tls_options);
		}
		else if (!strcmp(cep->name, "plaintext-policy"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "user"))
					tempiConf.plaintext_policy_user = policy_strtoval(cepp->value);
				else if (!strcmp(cepp->name, "oper"))
					tempiConf.plaintext_policy_oper = policy_strtoval(cepp->value);
				else if (!strcmp(cepp->name, "server"))
					tempiConf.plaintext_policy_server = policy_strtoval(cepp->value);
				else if (!strcmp(cepp->name, "user-message"))
					addmultiline(&tempiConf.plaintext_policy_user_message, cepp->value);
				else if (!strcmp(cepp->name, "oper-message"))
					addmultiline(&tempiConf.plaintext_policy_oper_message, cepp->value);
			}
		}
		else if (!strcmp(cep->name, "outdated-tls-policy"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "user"))
					tempiConf.outdated_tls_policy_user = policy_strtoval(cepp->value);
				else if (!strcmp(cepp->name, "oper"))
					tempiConf.outdated_tls_policy_oper = policy_strtoval(cepp->value);
				else if (!strcmp(cepp->name, "server"))
					tempiConf.outdated_tls_policy_server = policy_strtoval(cepp->value);
				else if (!strcmp(cepp->name, "user-message"))
					safe_strdup(tempiConf.outdated_tls_policy_user_message, cepp->value);
				else if (!strcmp(cepp->name, "oper-message"))
					safe_strdup(tempiConf.outdated_tls_policy_oper_message, cepp->value);
			}
		}
		else if (!strcmp(cep->name, "default-ipv6-clone-mask"))
		{
			tempiConf.default_ipv6_clone_mask = atoi(cep->value);
		}
		else if (!strcmp(cep->name, "hide-list")) {
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "deny-channel"))
				{
					tempiConf.hide_list = 1;
					/* if we would expand this later then change this to a bitmask or struct or whatever */
				}
			}
		}
		else if (!strcmp(cep->name, "max-unknown-connections-per-ip"))
		{
			tempiConf.max_unknown_connections_per_ip = atoi(cep->value);
		}
		else if (!strcmp(cep->name, "handshake-timeout"))
		{
			tempiConf.handshake_timeout = config_checkval(cep->value, CFG_TIME);
		}
		else if (!strcmp(cep->name, "sasl-timeout"))
		{
			tempiConf.sasl_timeout = config_checkval(cep->value, CFG_TIME);
		}
		else if (!strcmp(cep->name, "handshake-delay"))
		{
			tempiConf.handshake_delay = config_checkval(cep->value, CFG_TIME);
		}
		else if (!strcmp(cep->name, "automatic-ban-target"))
		{
			tempiConf.automatic_ban_target = ban_target_strtoval(cep->value);
		}
		else if (!strcmp(cep->name, "manual-ban-target"))
		{
			tempiConf.manual_ban_target = ban_target_strtoval(cep->value);
		}
		else if (!strcmp(cep->name, "reject-message"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "too-many-connections"))
					safe_strdup(tempiConf.reject_message_too_many_connections, cepp->value);
				else if (!strcmp(cepp->name, "server-full"))
					safe_strdup(tempiConf.reject_message_server_full, cepp->value);
				else if (!strcmp(cepp->name, "unauthorized"))
					safe_strdup(tempiConf.reject_message_unauthorized, cepp->value);
				else if (!strcmp(cepp->name, "kline"))
					safe_strdup(tempiConf.reject_message_kline, cepp->value);
				else if (!strcmp(cepp->name, "gline"))
					safe_strdup(tempiConf.reject_message_gline, cepp->value);
			}
		}
		else if (!strcmp(cep->name, "topic-setter"))
		{
			if (!strcmp(cep->value, "nick"))
				tempiConf.topic_setter = SETTER_NICK;
			else if (!strcmp(cep->value, "nick-user-host"))
				tempiConf.topic_setter = SETTER_NICK_USER_HOST;
		}
		else if (!strcmp(cep->name, "ban-setter"))
		{
			if (!strcmp(cep->value, "nick"))
				tempiConf.ban_setter = SETTER_NICK;
			else if (!strcmp(cep->value, "nick-user-host"))
				tempiConf.ban_setter = SETTER_NICK_USER_HOST;
		}
		else if (!strcmp(cep->name, "ban-setter-sync") || !strcmp(cep->name, "ban-setter-synch"))
		{
			tempiConf.ban_setter_sync = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "part-instead-of-quit-on-comment-change"))
		{
			tempiConf.part_instead_of_quit_on_comment_change = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "broadcast-channel-messages"))
		{
			if (!strcmp(cep->value, "auto"))
				tempiConf.broadcast_channel_messages = BROADCAST_CHANNEL_MESSAGES_AUTO;
			else if (!strcmp(cep->value, "always"))
				tempiConf.broadcast_channel_messages = BROADCAST_CHANNEL_MESSAGES_ALWAYS;
			else if (!strcmp(cep->value, "never"))
				tempiConf.broadcast_channel_messages = BROADCAST_CHANNEL_MESSAGES_NEVER;
		}
		else if (!strcmp(cep->name, "allowed-channelchars"))
		{
			tempiConf.allowed_channelchars = allowed_channelchars_strtoval(cep->value);
		}
		else if (!strcmp(cep->name, "hide-idle-time"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "policy"))
					tempiConf.hide_idle_time = hideidletime_strtoval(cepp->value);
			}
		} else
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "kline-address")) {
			CheckNull(cep);
			CheckDuplicate(cep, kline_address, "kline-address");
			if (!strchr(cep->value, '@') && !strchr(cep->value, ':'))
			{
				config_error("%s:%i: set::kline-address must be an e-mail or an URL",
					cep->file->filename, cep->line_number);
				errors++;
				continue;
			}
			else if (match_simple("*@unrealircd.com", cep->value) || match_simple("*@unrealircd.org",cep->value) || match_simple("unreal-*@lists.sourceforge.net",cep->value))
			{
				config_error("%s:%i: set::kline-address may not be an UnrealIRCd Team address",
					cep->file->filename, cep->line_number);
				errors++; continue;
			}
		}
		else if (!strcmp(cep->name, "gline-address")) {
			CheckNull(cep);
			CheckDuplicate(cep, gline_address, "gline-address");
			if (!strchr(cep->value, '@') && !strchr(cep->value, ':'))
			{
				config_error("%s:%i: set::gline-address must be an e-mail or an URL",
					cep->file->filename, cep->line_number);
				errors++;
				continue;
			}
			else if (match_simple("*@unrealircd.com", cep->value) || match_simple("*@unrealircd.org",cep->value) || match_simple("unreal-*@lists.sourceforge.net",cep->value))
			{
				config_error("%s:%i: set::gline-address may not be an UnrealIRCd Team address",
					cep->file->filename, cep->line_number);
				errors++; continue;
			}
		}
		else if (!strcmp(cep->name, "modes-on-connect")) {
			char *p;
			CheckNull(cep);
			CheckDuplicate(cep, modes_on_connect, "modes-on-connect");
			for (p = cep->value; *p; p++)
				if (strchr("orzSHqtW", *p))
				{
					config_error("%s:%i: set::modes-on-connect may not include mode '%c'",
						cep->file->filename, cep->line_number, *p);
					errors++;
				}
		}
		else if (!strcmp(cep->name, "modes-on-join")) {
			char *c;
			struct ChMode temp;
			memset(&temp, 0, sizeof(temp));
			CheckNull(cep);
			CheckDuplicate(cep, modes_on_join, "modes-on-join");
			for (c = cep->value; *c; c++)
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
						config_error("%s:%i: set::modes-on-join may not contain +%c",
							cep->file->filename, cep->line_number, *c);
						errors++;
						break;
				}
			}
			/* We can't really verify much here.
			 * The channel mode modules have not been initialized
			 * yet at this point, so we can't really verify much
			 * here.
			 */
		}
		else if (!strcmp(cep->name, "modes-on-oper")) {
			char *p;
			CheckNull(cep);
			CheckDuplicate(cep, modes_on_oper, "modes-on-oper");
			for (p = cep->value; *p; p++)
				if (strchr("orzS", *p))
				{
					config_error("%s:%i: set::modes-on-oper may not include mode '%c'",
						cep->file->filename, cep->line_number, *p);
					errors++;
				}
			set_usermode(cep->value);
		}
		else if (!strcmp(cep->name, "snomask-on-oper")) {
			char *wrong_snomask;
			CheckNull(cep);
			CheckDuplicate(cep, snomask_on_oper, "snomask-on-oper");
			if (!is_valid_snomask_string_testing(cep->value, &wrong_snomask))
			{
				config_error("%s:%i: set::snomask-on-oper contains unknown snomask letter(s) '%s'",
					     cep->file->filename, cep->line_number, wrong_snomask);
				errors++;
				invalid_snomasks_encountered++;
			}
		}
		else if (!strcmp(cep->name, "server-notice-colors")) {
			CheckNull(cep);
		}
		else if (!strcmp(cep->name, "server-notice-show-event")) {
			CheckNull(cep);
		}
		else if (!strcmp(cep->name, "level-on-join")) {
			CheckNull(cep);
			CheckDuplicate(cep, level_on_join, "level-on-join");
			if (!channellevel_to_string(cep->value) && (strlen(cep->value) != 1))
			{
				config_error("%s:%i: set::level-on-join: unknown value '%s', should be one of: "
				             "'none', 'voice', 'halfop', 'op', 'admin', 'owner', or a single letter (eg 'o')",
				             cep->file->filename, cep->line_number, cep->value);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "static-quit")) {
			CheckNull(cep);
			CheckDuplicate(cep, static_quit, "static-quit");
		}
		else if (!strcmp(cep->name, "static-part")) {
			CheckNull(cep);
			CheckDuplicate(cep, static_part, "static-part");
		}
		else if (!strcmp(cep->name, "who-limit")) {
			CheckNull(cep);
			CheckDuplicate(cep, who_limit, "who-limit");
			if (!config_checkval(cep->value,CFG_SIZE))
			{
				config_error("%s:%i: set::who-limit: value must be at least 1",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "maxbans")) {
			CheckNull(cep);
			CheckDuplicate(cep, maxbans, "maxbans");
		}
		else if (!strcmp(cep->name, "maxbanlength")) {
			CheckNull(cep);
			CheckDuplicate(cep, maxbanlength, "maxbanlength");
		}
		else if (!strcmp(cep->name, "silence-limit")) {
			CheckNull(cep);
			CheckDuplicate(cep, silence_limit, "silence-limit");
		}
		else if (!strcmp(cep->name, "auto-join")) {
			CheckNull(cep);
			CheckDuplicate(cep, auto_join, "auto-join");
		}
		else if (!strcmp(cep->name, "oper-auto-join")) {
			CheckNull(cep);
			CheckDuplicate(cep, oper_auto_join, "oper-auto-join");
		}
		else if (!strcmp(cep->name, "check-target-nick-bans")) {
			CheckNull(cep);
			CheckDuplicate(cep, check_target_nick_bans, "check-target-nick-bans");
		}
		else if (!strcmp(cep->name, "pingpong-warning")) {
			config_error("%s:%i: set::pingpong-warning no longer exists (the warning is always off)",
			             cep->file->filename, cep->line_number);
			errors++;
		}
		else if (!strcmp(cep->name, "ping-cookie")) {
			CheckNull(cep);
			CheckDuplicate(cep, ping_cookie, "ping-cookie");
		}
		else if (!strcmp(cep->name, "watch-away-notification")) {
			CheckNull(cep);
			CheckDuplicate(cep, watch_away_notification, "watch-away-notification");
		}
		else if (!strcmp(cep->name, "uhnames")) {
			CheckNull(cep);
			CheckDuplicate(cep, uhnames, "uhnames");
		}
		else if (!strcmp(cep->name, "channel-command-prefix")) {
			CheckNullAllowEmpty(cep);
			CheckDuplicate(cep, channel_command_prefix, "channel-command-prefix");
		}
		else if (!strcmp(cep->name, "allow-userhost-change")) {
			CheckNull(cep);
			CheckDuplicate(cep, allow_userhost_change, "allow-userhost-change");
			if (strcasecmp(cep->value, "always") &&
			    strcasecmp(cep->value, "never") &&
			    strcasecmp(cep->value, "not-on-channels") &&
			    strcasecmp(cep->value, "force-rejoin"))
			{
				config_error("%s:%i: set::allow-userhost-change is invalid",
					cep->file->filename,
					cep->line_number);
				errors++;
				continue;
			}
		}
		else if (!strcmp(cep->name, "anti-spam-quit-message-time")) {
			CheckNull(cep);
			CheckDuplicate(cep, anti_spam_quit_message_time, "anti-spam-quit-message-time");
		}
		else if (!strcmp(cep->name, "oper-only-stats"))
		{
			config_warn("%s:%d: We no longer use a blacklist for stats (set::oper-only-stats) but "
			             "have a whitelist now instead (set::allow-user-stats). ",
			             cep->file->filename, cep->line_number);
			config_warn("Simply delete the oper-only-stats line from your configuration file %s around line %d to get rid of this warning",
			             cep->file->filename, cep->line_number);
			continue;
		}
		else if (!strcmp(cep->name, "allow-user-stats"))
		{
			CheckDuplicate(cep, allow_user_stats, "allow-user-stats");
			CheckNull(cep);
		}
		else if (!strcmp(cep->name, "maxchannelsperuser")) {
			CheckNull(cep);
			CheckDuplicate(cep, maxchannelsperuser, "maxchannelsperuser");
			tempi = atoi(cep->value);
			if (tempi < 1)
			{
				config_error("%s:%i: set::maxchannelsperuser must be > 0",
					cep->file->filename,
					cep->line_number);
				errors++;
				continue;
			}
		}
		else if (!strcmp(cep->name, "ping-warning")) {
			CheckNull(cep);
			CheckDuplicate(cep, ping_warning, "ping-warning");
			tempi = atoi(cep->value);
			/* it is pointless to allow setting higher than 170 */
			if (tempi > 170)
			{
				config_error("%s:%i: set::ping-warning must be < 170",
					cep->file->filename,
					cep->line_number);
				errors++;
				continue;
			}
		}
		else if (!strcmp(cep->name, "maxdccallow")) {
			CheckNull(cep);
			CheckDuplicate(cep, maxdccallow, "maxdccallow");
		}
		else if (!strcmp(cep->name, "max-targets-per-command"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				CheckNull(cepp);
				if (!strcasecmp(cepp->name, "NAMES") || !strcasecmp(cepp->name, "WHOWAS"))
				{
					if (atoi(cepp->value) != 1)
					{
						config_error("%s:%i: set::max-targets-per-command::%s: "
						             "this command is hardcoded at a maximum of 1 "
						             "and cannot be configured to accept more.",
						             cepp->file->filename,
						             cepp->line_number,
						             cepp->name);
						errors++;
					}
				} else
				if (!strcasecmp(cepp->name, "USERHOST") ||
				    !strcasecmp(cepp->name, "USERIP") ||
				    !strcasecmp(cepp->name, "ISON") ||
				    !strcasecmp(cepp->name, "WATCH"))
				{
					if (strcmp(cepp->value, "max"))
					{
						config_error("%s:%i: set::max-targets-per-command::%s: "
						             "this command is hardcoded at a maximum of 'max' "
						             "and cannot be changed. This because it is "
						             "highly discouraged to change it.",
						             cepp->file->filename,
						             cepp->line_number,
						             cepp->name);
						errors++;
					}
				}
				/* Now check the value syntax in general: */
				if (strcmp(cepp->value, "max")) /* anything other than 'max'.. */
				{
					int v = atoi(cepp->value);
					if ((v < 1) || (v > 20))
					{
						config_error("%s:%i: set::max-targets-per-command::%s: "
						             "value should be 1-20 or 'max'",
						             cepp->file->filename,
						             cepp->line_number,
						             cepp->name);
						errors++;
					}
				}
			}
		}
		else if (!strcmp(cep->name, "network-name")) {
			char *p;
			CheckNull(cep);
			CheckDuplicate(cep, network_name, "network-name");
			for (p = cep->value; *p; p++)
				if ((*p < ' ') || (*p > '~'))
				{
					config_error("%s:%i: set::network-name can only contain ASCII characters 33-126. Invalid character = '%c'",
						cep->file->filename, cep->line_number, *p);
					errors++;
					break;
				}
		}
		else if (!strcmp(cep->name, "default-server")) {
			CheckNull(cep);
			CheckDuplicate(cep, default_server, "default-server");
		}
		else if (!strcmp(cep->name, "services-server")) {
			CheckNull(cep);
			CheckDuplicate(cep, services_server, "services-server");
		}
		else if (!strcmp(cep->name, "sasl-server")) {
			CheckNull(cep);
			CheckDuplicate(cep, sasl_server, "sasl-server");
		}
		else if (!strcmp(cep->name, "stats-server")) {
			CheckNull(cep);
			CheckDuplicate(cep, stats_server, "stats-server");
		}
		else if (!strcmp(cep->name, "help-channel")) {
			CheckNull(cep);
			CheckDuplicate(cep, help_channel, "help-channel");
		}
		else if (!strcmp(cep->name, "cloak-prefix") || !strcmp(cep->name, "hiddenhost-prefix")) {
			CheckNull(cep);
			CheckDuplicate(cep, hiddenhost_prefix, "cloak-prefix");
			if (strchr(cep->value, ' ') || (*cep->value == ':'))
			{
				config_error("%s:%i: set::cloak-prefix must not contain spaces or be prefixed with ':'",
					cep->file->filename, cep->line_number);
				errors++;
				continue;
			}
		}
		else if (!strcmp(cep->name, "prefix-quit")) {
			CheckNull(cep);
			CheckDuplicate(cep, prefix_quit, "prefix-quit");
		}
		else if (!strcmp(cep->name, "hide-ban-reason")) {
			CheckNull(cep);
			CheckDuplicate(cep, hide_ban_reason, "hide-ban-reason");
		}
		else if (!strcmp(cep->name, "restrict-usermodes"))
		{
			CheckNull(cep);
			CheckDuplicate(cep, restrict_usermodes, "restrict-usermodes");
			if (cep->name) {
				int warn = 0;
				char *p;
				for (p = cep->value; *p; p++)
					if ((*p == '+') || (*p == '-'))
						warn = 1;
				if (warn) {
					config_status("%s:%i: warning: set::restrict-usermodes: should only contain modechars, no + or -.\n",
						cep->file->filename, cep->line_number);
				}
			}
		}
		else if (!strcmp(cep->name, "restrict-channelmodes"))
		{
			CheckNull(cep);
			CheckDuplicate(cep, restrict_channelmodes, "restrict-channelmodes");
			if (cep->name) {
				int warn = 0;
				char *p;
				for (p = cep->value; *p; p++)
					if ((*p == '+') || (*p == '-'))
						warn = 1;
				if (warn) {
					config_status("%s:%i: warning: set::restrict-channelmodes: should only contain modechars, no + or -.\n",
						cep->file->filename, cep->line_number);
				}
			}
		}
		else if (!strcmp(cep->name, "restrict-extendedbans"))
		{
			CheckDuplicate(cep, restrict_extendedbans, "restrict-extendedbans");
			CheckNull(cep);
		}
		else if (!strcmp(cep->name, "named-extended-bans"))
		{
			CheckNull(cep);
		}
		else if (!strcmp(cep->name, "link")) {
					for (cepp = cep->items; cepp; cepp = cepp->next) {
						CheckNull(cepp);
						if (!strcmp(cepp->name, "bind-ip")) {
							CheckDuplicate(cepp, link_bind_ip, "link::bind-ip");
							if (strcmp(cepp->value, "*"))
							{
								if (!is_valid_ip(cepp->value))
								{
									config_error("%s:%i: set::link::bind-ip (%s) is not a valid IP",
										cepp->file->filename, cepp->line_number,
										cepp->value);
									errors++;
									continue;
								}
							}
						}
					}
		}
		else if (!strcmp(cep->name, "throttle")) {
			config_error("%s:%i: set::throttle has been renamed. you now use "
			             "set::anti-flood::connect-flood <connections>:<period>. "
			             "Or just remove the throttle block and you get the default "
			             "of 3 per 60 seconds.",
			             cep->file->filename, cep->line_number);
			errors++;
			continue;
		}
		else if (!strcmp(cep->name, "anti-flood"))
		{
			int anti_flood_old = 0;
			int anti_flood_old_and_default = 0;

			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				int has_lag_penalty = 0;
				int has_lag_penalty_bytes = 0;

				/* Test for old options: */
				if (flood_option_is_old(cepp->name))
				{
					/* Special code if the user is using 100% of the defaults */
					if (cepp->value &&
					    ((!strcmp(cepp->name, "nick-flood") && !strcmp(cepp->value, "3:60")) ||
					     (!strcmp(cepp->name, "connect-flood") && cepp->value && !strcmp(cepp->value, "3:60")) ||
					     (!strcmp(cepp->name, "away-flood") && cepp->value && !strcmp(cepp->value, "4:120"))))
					{
						anti_flood_old_and_default = 1;
					} else
					{
						anti_flood_old = 1;
					}
					continue;
				}

				for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
				{
					int everyone = !strcmp(cepp->name, "everyone") ? 1 : 0;
					int for_everyone = flood_option_is_for_everyone(ceppp->name);

					if (everyone && !for_everyone)
					{
						config_error("%s:%i: %s cannot be in the set::anti-flood::everyone block. "
						             "You can put it in 'known-users' or 'unknown-users' instead.",
							ceppp->file->filename, ceppp->line_number,
							ceppp->name);
						errors++;
						continue;
					} else
					if (!everyone && for_everyone)
					{
						config_error("%s:%i: %s must be in the set::anti-flood::everyone block, not anywhere else.",
							ceppp->file->filename, ceppp->line_number,
							ceppp->name);
						errors++;
						continue;
					}

					/* Now comes the actual config check for each element... */
					if (!strcmp(ceppp->name, "max-concurrent-conversations"))
					{
						for (cep4 = ceppp->items; cep4; cep4 = cep4->next)
						{
							CheckNull(cep4);
							if (!strcmp(cep4->name, "users"))
							{
								int v = atoi(cep4->value);
								if ((v < 1) || (v > MAXCCUSERS))
								{
									config_error("%s:%i: set::anti-flood::max-concurrent-conversations::users: "
										     "value should be between 1 and %d",
										     cep4->file->filename, cep4->line_number, MAXCCUSERS);
									errors++;
								}
							} else
							if (!strcmp(cep4->name, "new-user-every"))
							{
								long v = config_checkval(cep4->value, CFG_TIME);
								if ((v < 1) || (v > 120))
								{
									config_error("%s:%i: set::anti-flood::max-concurrent-conversations::new-user-every: "
										     "value should be between 1 and 120 seconds",
										     cep4->file->filename, cep4->line_number);
									errors++;
								}
							} else
							{
								config_error_unknownopt(cep4->file->filename,
									cep4->line_number, "set::anti-flood",
									cep4->name);
								errors++;
							}
						}
						continue; /* required here, due to checknull directly below */
					}
					else if (!strcmp(ceppp->name, "unknown-flood-amount") ||
						 !strcmp(ceppp->name, "unknown-flood-bantime"))
					{
						config_error("%s:%i: set::anti-flood::%s: this setting has been moved. "
							     "See https://www.unrealircd.org/docs/Anti-flood_settings#handshake-data-flood",
							     ceppp->file->filename, ceppp->line_number, ceppp->name);
						errors++;
						continue;
					}
					else if (!strcmp(ceppp->name, "handshake-data-flood"))
					{
						for (cep4 = ceppp->items; cep4; cep4 = cep4->next)
						{
							if (!strcmp(cep4->name, "amount"))
							{
								long v;
								CheckNull(cep4);
								v = config_checkval(cep4->value, CFG_SIZE);
								if (v < 1024)
								{
									config_error("%s:%i: set::anti-flood::handshake-data-flood::amount must be at least 1024 bytes",
										cep4->file->filename, cep4->line_number);
									errors++;
								}
							} else
							if (!strcmp(cep4->name, "ban-action"))
							{
								CheckNull(cep4);
								if (!banact_stringtoval(cep4->value))
								{
									config_error("%s:%i: set::anti-flood::handshake-data-flood::ban-action has unknown action type '%s'",
										cep4->file->filename, cep4->line_number,
										cep4->value);
									errors++;
								}
							} else
							if (!strcmp(cep4->name, "ban-time"))
							{
								CheckNull(cep4);
							} else
							{
								config_error_unknownopt(cep4->file->filename,
									cep4->line_number, "set::anti-flood::handshake-data-flood",
									cep4->name);
								errors++;
							}
						}
					}
					else if (!strcmp(ceppp->name, "away-count"))
					{
						int temp = atol(ceppp->value);
						CheckNull(ceppp);
						if (temp < 1 || temp > 255)
						{
							config_error("%s:%i: set::anti-flood::away-count must be between 1 and 255",
								ceppp->file->filename, ceppp->line_number);
							errors++;
						}
					}
					else if (!strcmp(ceppp->name, "away-period"))
					{
						CheckNull(ceppp);
						int temp = config_checkval(ceppp->value, CFG_TIME);
						if (temp < 10)
						{
							config_error("%s:%i: set::anti-flood::away-period must be greater than 9",
								ceppp->file->filename, ceppp->line_number);
							errors++;
						}
					}
					else if (!strcmp(ceppp->name, "away-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);
						if (!config_parse_flood(ceppp->value, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 10))
						{
							config_error("%s:%i: set::anti-flood::away-flood error. Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be greater than 9",
								ceppp->file->filename, ceppp->line_number);
							errors++;
						}
					}
					else if (!strcmp(ceppp->name, "nick-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);
						if (!config_parse_flood(ceppp->value, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 5))
						{
							config_error("%s:%i: set::anti-flood::nick-flood error. Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be greater than 4",
								ceppp->file->filename, ceppp->line_number);
							errors++;
						}
					}
					else if (!strcmp(ceppp->name, "vhost-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);
						if (!config_parse_flood(ceppp->value, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 5))
						{
							config_error("%s:%i: set::anti-flood::vhost-flood error. Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be greater than 4",
								ceppp->file->filename, ceppp->line_number);
							errors++;
						}
					}
					else if (!strcmp(ceppp->name, "join-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);

						if (!config_parse_flood(ceppp->value, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 5))
						{
							config_error("%s:%i: join-flood error. Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be greater than 4",
								ceppp->file->filename, ceppp->line_number);
							errors++;
						}
					}
					else if (!strcmp(ceppp->name, "invite-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);
						if (!config_parse_flood(ceppp->value, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 5))
						{
							config_error("%s:%i: set::anti-flood::invite-flood error. Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be greater than 4",
								ceppp->file->filename, ceppp->line_number);
							errors++;
						}
					}
					else if (!strcmp(ceppp->name, "knock-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);
						if (!config_parse_flood(ceppp->value, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 5))
						{
							config_error("%s:%i: set::anti-flood::knock-flood error. Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be greater than 4",
								ceppp->file->filename, ceppp->line_number);
							errors++;
						}
					}
					else if (!strcmp(ceppp->name, "lag-penalty"))
					{
						int v;
						CheckNull(ceppp);
						v = atoi(ceppp->value);
						has_lag_penalty = 1;
						if ((v < 0) || (v > 10000))
						{
							config_error("%s:%i: set::anti-flood::%s::lag-penalty: value is in milliseconds and should be between 0 and 10000",
								ceppp->file->filename, ceppp->line_number, cepp->name);
							errors++;
						}
					}
					else if (!strcmp(ceppp->name, "lag-penalty-bytes"))
					{
						has_lag_penalty_bytes = 1;
						CheckNull(ceppp);
					}
					else if (!strcmp(ceppp->name, "connect-flood"))
					{
						int cnt, period;
						CheckNull(ceppp);
						if (strcmp(cepp->name, "everyone"))
						{
							config_error("%s:%i: connect-flood must be in the set::anti-flood::everyone block, not anywhere else.",
								ceppp->file->filename, ceppp->line_number);
							errors++;
							continue;
						}
						if (!config_parse_flood(ceppp->value, &cnt, &period) ||
						    (cnt < 1) || (cnt > 255) || (period < 1) || (period > 3600))
						{
							config_error("%s:%i: set::anti-flood::connect-flood: Syntax is '<count>:<period>' (eg 5:60), "
								     "count should be 1-255, period should be 1-3600",
								ceppp->file->filename, ceppp->line_number);
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
							config_error_unknownopt(ceppp->file->filename,
								ceppp->line_number, "set::anti-flood",
								ceppp->name);
							errors++;
						}
						continue;
					}
				}
				if (has_lag_penalty+has_lag_penalty_bytes == 1)
				{
					config_error("%s:%i: set::anti-flood::%s: if you use lag-penalty then you must also add an lag-penalty-bytes item (and vice-versa)",
						cepp->file->filename, cepp->line_number, cepp->name);
					errors++;
				}
			}
			/* Now the warnings: */
			if (anti_flood_old == 1)
			{
				config_warn("%s:%d: the set::anti-flood block has been reorganized to be more flexible. "
				            "Your custom anti-flood settings have NOT been read.",
				            cep->file->filename, cep->line_number);
				config_warn("See https://www.unrealircd.org/docs/Anti-flood_settings for the new block style,");
				config_warn("OR: simply remove all the anti-flood options from the conf to get rid of this "
				            "warning and use the built-in defaults.");
			} else
			if (anti_flood_old_and_default == 1)
			{
				config_warn("%s:%d: the set::anti-flood block has been reorganized to be more flexible.",
					    cep->file->filename, cep->line_number);
				config_warn("To fix this warning, delete the anti-flood block from your configuration file "
				            "(file %s around line %d), this will make UnrealIRCd use the built-in defaults.",
				            cep->file->filename, cep->line_number);
				config_warn("If you want to learn more about the new functionality you can visit "
				            "https://www.unrealircd.org/docs/Anti-flood_settings");
			}
		}
		else if (!strcmp(cep->name, "options")) {
			for (cepp = cep->items; cepp; cepp = cepp->next) {
				if (!strcmp(cepp->name, "hide-ulines"))
				{
					CheckDuplicate(cepp, options_hide_ulines, "options::hide-ulines");
				}
				else if (!strcmp(cepp->name, "flat-map")) {
					CheckDuplicate(cepp, options_flat_map, "options::flat-map");
				}
				else if (!strcmp(cepp->name, "show-opermotd")) {
					CheckDuplicate(cepp, options_show_opermotd, "options::show-opermotd");
				}
				else if (!strcmp(cepp->name, "identd-check")) {
					CheckDuplicate(cepp, options_identd_check, "options::identd-check");
				}
				else if (!strcmp(cepp->name, "fail-oper-warn")) {
					CheckDuplicate(cepp, options_fail_oper_warn, "options::fail-oper-warn");
				}
				else if (!strcmp(cepp->name, "show-connect-info")) {
					CheckDuplicate(cepp, options_show_connect_info, "options::show-connect-info");
				}
				else if (!strcmp(cepp->name, "no-connect-tls-info")) {
					CheckDuplicate(cepp, options_no_connect_tls_info, "options::no-connect-tls-info");
				}
				else if (!strcmp(cepp->name, "dont-resolve")) {
					CheckDuplicate(cepp, options_dont_resolve, "options::dont-resolve");
				}
				else if (!strcmp(cepp->name, "mkpasswd-for-everyone")) {
					CheckDuplicate(cepp, options_mkpasswd_for_everyone, "options::mkpasswd-for-everyone");
				}
				else if (!strcmp(cepp->name, "allow-insane-bans")) {
					CheckDuplicate(cepp, options_allow_insane_bans, "options::allow-insane-bans");
				}
				else if (!strcmp(cepp->name, "allow-part-if-shunned")) {
					CheckDuplicate(cepp, options_allow_part_if_shunned, "options::allow-part-if-shunned");
				}
				else if (!strcmp(cepp->name, "disable-cap")) {
					CheckDuplicate(cepp, options_disable_cap, "options::disable-cap");
				}
				else if (!strcmp(cepp->name, "disable-ipv6")) {
					CheckDuplicate(cepp, options_disable_ipv6, "options::disable-ipv6");
					DISABLE_IPV6 = 1; /* ugly ugly. needs to be done here because at conf runtime is too late. */
				}
				else
				{
					config_error_unknownopt(cepp->file->filename,
						cepp->line_number, "set::options",
						cepp->name);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->name, "hosts")) {
			config_error("%s:%i: set::hosts has been removed. You can use oper::vhost now.",
				cep->file->filename, cep->line_number);
			errors++;
		}
		else if (!strcmp(cep->name, "cloak-keys"))
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
		else if (!strcmp(cep->name, "ident")) {
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				int is_ok = 0;
				CheckNull(cepp);
				if (!strcmp(cepp->name, "connect-timeout"))
				{
					is_ok = 1;
					CheckDuplicate(cepp, ident_connect_timeout, "ident::connect-timeout");
				}
				else if (!strcmp(cepp->name, "read-timeout"))
				{
					is_ok = 1;
					CheckDuplicate(cepp, ident_read_timeout, "ident::read-timeout");
				}
				if (is_ok)
				{
					int v = config_checkval(cepp->value,CFG_TIME);
					if ((v > 60) || (v < 1))
					{
						config_error("%s:%i: set::ident::%s value out of range (%d), should be between 1 and 60.",
							cepp->file->filename, cepp->line_number, cepp->name, v);
						errors++;
						continue;
					}
				} else {
					config_error_unknown(cepp->file->filename,
						cepp->line_number, "set::ident",
						cepp->name);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->name, "timesync") || !strcmp(cep->name, "timesynch"))
		{
			config_warn("%s:%i: Timesync support has been removed from UnrealIRCd. "
			            "Please remove any set::timesync blocks you may have.",
			            cep->file->filename, cep->line_number);
			config_warn("Use the time synchronization feature of your OS/distro instead!");
		}
		else if (!strcmp(cep->name, "spamfilter")) {
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->name, "ban-time"))
				{
					long x;
					CheckDuplicate(cepp, spamfilter_ban_time, "spamfilter::ban-time");
					x = config_checkval(cepp->value,CFG_TIME);
					if ((x < 0) > (x > 2000000000))
					{
						config_error("%s:%i: set::spamfilter:ban-time: value '%ld' out of range",
							cep->file->filename, cep->line_number, x);
						errors++;
						continue;
					}
				} else
				if (!strcmp(cepp->name, "ban-reason"))
				{
					CheckDuplicate(cepp, spamfilter_ban_reason, "spamfilter::ban-reason");

				}
				else if (!strcmp(cepp->name, "virus-help-channel"))
				{
					CheckDuplicate(cepp, spamfilter_virus_help_channel, "spamfilter::virus-help-channel");
					if ((cepp->value[0] != '#') || (strlen(cepp->value) > CHANNELLEN))
					{
						config_error("%s:%i: set::spamfilter:virus-help-channel: "
						             "specified channelname is too long or contains invalid characters (%s)",
						             cep->file->filename, cep->line_number,
						             cepp->value);
						errors++;
						continue;
					}
				} else
				if (!strcmp(cepp->name, "virus-help-channel-deny"))
				{
					CheckDuplicate(cepp, spamfilter_virus_help_channel_deny, "spamfilter::virus-help-channel-deny");
				} else
				if (!strcmp(cepp->name, "except"))
				{
					CheckDuplicate(cepp, spamfilter_except, "spamfilter::except");
				} else
#ifdef SPAMFILTER_DETECTSLOW
				if (!strcmp(cepp->name, "detect-slow-warn"))
				{
				} else
				if (!strcmp(cepp->name, "detect-slow-fatal"))
				{
				} else
#endif
				if (!strcmp(cepp->name, "stop-on-first-match"))
				{
				} else
				{
					config_error_unknown(cepp->file->filename,
						cepp->line_number, "set::spamfilter",
						cepp->name);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->name, "default-bantime"))
		{
			long x;
			CheckDuplicate(cep, default_bantime, "default-bantime");
			CheckNull(cep);
			x = config_checkval(cep->value,CFG_TIME);
			if ((x < 0) > (x > 2000000000))
			{
				config_error("%s:%i: set::default-bantime: value '%ld' out of range",
					cep->file->filename, cep->line_number, x);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "ban-version-tkl-time")) {
			long x;
			CheckDuplicate(cep, ban_version_tkl_time, "ban-version-tkl-time");
			CheckNull(cep);
			x = config_checkval(cep->value,CFG_TIME);
			if ((x < 0) > (x > 2000000000))
			{
				config_error("%s:%i: set::ban-version-tkl-time: value '%ld' out of range",
					cep->file->filename, cep->line_number, x);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "min-nick-length")) {
			int v;
			CheckDuplicate(cep, min_nick_length, "min-nick-length");
			CheckNull(cep);
			v = atoi(cep->value);
			if ((v <= 0) || (v > NICKLEN))
			{
				config_error("%s:%i: set::min-nick-length: value '%d' out of range (should be 1-%d)",
					cep->file->filename, cep->line_number, v, NICKLEN);
				errors++;
			}
			else
				nicklengths.min = v;
		}
		else if (!strcmp(cep->name, "nick-length")) {
			int v;
			CheckDuplicate(cep, nick_length, "nick-length");
			CheckNull(cep);
			v = atoi(cep->value);
			if ((v <= 0) || (v > NICKLEN))
			{
				config_error("%s:%i: set::nick-length: value '%d' out of range (should be 1-%d)",
					cep->file->filename, cep->line_number, v, NICKLEN);
				errors++;
			}
			else
				nicklengths.max = v;
		}
		else if (!strcmp(cep->name, "topic-length")) {
			int v;
			CheckNull(cep);
			v = atoi(cep->value);
			if ((v <= 0) || (v > MAXTOPICLEN))
			{
				config_error("%s:%i: set::topic-length: value '%d' out of range (should be 1-%d)",
					cep->file->filename, cep->line_number, v, MAXTOPICLEN);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "away-length")) {
			int v;
			CheckNull(cep);
			v = atoi(cep->value);
			if ((v <= 0) || (v > MAXAWAYLEN))
			{
				config_error("%s:%i: set::away-length: value '%d' out of range (should be 1-%d)",
					cep->file->filename, cep->line_number, v, MAXAWAYLEN);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "kick-length")) {
			int v;
			CheckNull(cep);
			v = atoi(cep->value);
			if ((v <= 0) || (v > MAXKICKLEN))
			{
				config_error("%s:%i: set::kick-length: value '%d' out of range (should be 1-%d)",
					cep->file->filename, cep->line_number, v, MAXKICKLEN);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "quit-length")) {
			int v;
			CheckNull(cep);
			v = atoi(cep->value);
			if ((v <= 0) || (v > MAXQUITLEN))
			{
				config_error("%s:%i: set::quit-length: value '%d' out of range (should be 1-%d)",
					cep->file->filename, cep->line_number, v, MAXQUITLEN);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "ssl") || !strcmp(cep->name, "tls")) {
			test_tlsblock(conf, cep, &errors);
		}
		else if (!strcmp(cep->name, "plaintext-policy"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "user") ||
					!strcmp(cepp->name, "oper") ||
					!strcmp(cepp->name, "server"))
				{
					Policy policy;
					CheckNull(cepp);
					policy = policy_strtoval(cepp->value);
					if (!policy)
					{
						config_error("%s:%i: set::plaintext-policy::%s: needs to be one of: 'allow', 'warn' or 'reject'",
							cepp->file->filename, cepp->line_number, cepp->name);
						errors++;
					}
				} else if (!strcmp(cepp->name, "user-message") ||
				           !strcmp(cepp->name, "oper-message"))
				{
					CheckNull(cepp);
				} else {
					config_error_unknown(cepp->file->filename,
						cepp->line_number, "set::plaintext-policy",
						cepp->name);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->name, "outdated-tls-policy"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "user") ||
					!strcmp(cepp->name, "oper") ||
					!strcmp(cepp->name, "server"))
				{
					Policy policy;
					CheckNull(cepp);
					policy = policy_strtoval(cepp->value);
					if (!policy)
					{
						config_error("%s:%i: set::outdated-tls-policy::%s: needs to be one of: 'allow', 'warn' or 'reject'",
							cepp->file->filename, cepp->line_number, cepp->name);
						errors++;
					}
				} else if (!strcmp(cepp->name, "user-message") ||
				           !strcmp(cepp->name, "oper-message"))
				{
					CheckNull(cepp);
				} else {
					config_error_unknown(cepp->file->filename,
						cepp->line_number, "set::outdated-tls-policy",
						cepp->name);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->name, "default-ipv6-clone-mask"))
		{
			/* keep this in sync with _test_allow() */
			int ipv6mask;
			ipv6mask = atoi(cep->value);
			if (ipv6mask == 0)
			{
				config_error("%s:%d: set::default-ipv6-clone-mask given a value of zero. This cannnot be correct, as it would treat all IPv6 hosts as one host.",
					     cep->file->filename, cep->line_number);
				errors++;
			}
			if (ipv6mask > 128)
			{
				config_error("%s:%d: set::default-ipv6-clone-mask was set to %d. The maximum value is 128.",
					     cep->file->filename, cep->line_number,
					     ipv6mask);
				errors++;
			}
			if (ipv6mask <= 32)
			{
				config_warn("%s:%d: set::default-ipv6-clone-mask was given a very small value.",
					    cep->file->filename, cep->line_number);
			}
		}
		else if (!strcmp(cep->name, "hide-list")) {
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "deny-channel"))
				{
				} else
				{
					config_error_unknown(cepp->file->filename,
						cepp->line_number, "set::hide-list",
						cepp->name);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->name, "max-unknown-connections-per-ip")) {
			int v;
			CheckNull(cep);
			v = atoi(cep->value);
			if (v < 1)
			{
				config_error("%s:%i: set::max-unknown-connections-per-ip: value should be at least 1.",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "handshake-timeout")) {
			int v;
			CheckNull(cep);
			v = config_checkval(cep->value, CFG_TIME);
			if (v < 5)
			{
				config_error("%s:%i: set::handshake-timeout: value should be at least 5 seconds.",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "sasl-timeout")) {
			int v;
			CheckNull(cep);
			v = config_checkval(cep->value, CFG_TIME);
			if (v < 5)
			{
				config_error("%s:%i: set::sasl-timeout: value should be at least 5 seconds.",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "handshake-delay"))
		{
			int v;
			CheckNull(cep);
			v = config_checkval(cep->value, CFG_TIME);
			if (v >= 10)
			{
				config_error("%s:%i: set::handshake-delay: value should be less than 10 seconds.",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "ban-include-username"))
		{
			config_error("%s:%i: set::ban-include-username is no longer supported. "
			             "Use set { automatic-ban-target userip; }; instead.",
			             cep->file->filename, cep->line_number);
			config_error("See https://www.unrealircd.org/docs/Set_block#set::automatic-ban-target "
			             "for more information and options.");
			errors++;
		}
		else if (!strcmp(cep->name, "automatic-ban-target"))
		{
			CheckNull(cep);
			if (!ban_target_strtoval(cep->value))
			{
				config_error("%s:%i: set::automatic-ban-target: value '%s' is not recognized. "
				             "See https://www.unrealircd.org/docs/Set_block#set::automatic-ban-target",
				             cep->file->filename, cep->line_number, cep->value);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "manual-ban-target"))
		{
			CheckNull(cep);
			if (!ban_target_strtoval(cep->value))
			{
				config_error("%s:%i: set::manual-ban-target: value '%s' is not recognized. "
				             "See https://www.unrealircd.org/docs/Set_block#set::manual-ban-target",
				             cep->file->filename, cep->line_number, cep->value);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "reject-message"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->name, "password-mismatch"))
					;
				else if (!strcmp(cepp->name, "too-many-connections"))
					;
				else if (!strcmp(cepp->name, "server-full"))
					;
				else if (!strcmp(cepp->name, "unauthorized"))
					;
				else if (!strcmp(cepp->name, "kline"))
					;
				else if (!strcmp(cepp->name, "gline"))
					;
				else
				{
					config_error_unknown(cepp->file->filename,
						cepp->line_number, "set::reject-message",
						cepp->name);
					errors++;
					continue;
				}
			}
		}
		else if (!strcmp(cep->name, "topic-setter"))
		{
			CheckNull(cep);
			if (strcmp(cep->value, "nick") && strcmp(cep->value, "nick-user-host"))
			{
				config_error("%s:%i: set::topic-setter: value should be 'nick' or 'nick-user-host'",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "ban-setter"))
		{
			CheckNull(cep);
			if (strcmp(cep->value, "nick") && strcmp(cep->value, "nick-user-host"))
			{
				config_error("%s:%i: set::ban-setter: value should be 'nick' or 'nick-user-host'",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "ban-setter-sync") || !strcmp(cep->name, "ban-setter-synch"))
		{
			CheckNull(cep);
		}
		else if (!strcmp(cep->name, "part-instead-of-quit-on-comment-change"))
		{
			CheckNull(cep);
		}
		else if (!strcmp(cep->name, "broadcast-channel-messages"))
		{
			CheckNull(cep);
			if (strcmp(cep->value, "auto") &&
			    strcmp(cep->value, "always") &&
			    strcmp(cep->value, "never"))
			{
				config_error("%s:%i: set::broadcast-channel-messages: value should be 'auto', 'always' or 'never'",
				             cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "allowed-channelchars"))
		{
			CheckNull(cep);
			if (!allowed_channelchars_strtoval(cep->value))
			{
				config_error("%s:%i: set::allowed-channelchars: value should be one of: 'ascii', 'utf8' or 'any'",
				             cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "hide-idle-time"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->name, "policy"))
				{
					if (!hideidletime_strtoval(cepp->value))
					{
						config_error("%s:%i: set::hide-idle-time::policy: value should be one of: 'never', 'always', 'usermode' or 'oper-usermode'",
							     cepp->file->filename, cepp->line_number);
						errors++;
					}
				}
				else
				{
					config_error_unknown(cepp->file->filename,
						cepp->line_number, "set::hide-idle-time",
						cepp->name);
					errors++;
					continue;
				}
			}
		} else
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
					cep->file->filename, cep->line_number,
					cep->name);
				errors++;
			}
		}
	}
	return errors;
}

int	_conf_loadmodule(ConfigFile *conf, ConfigEntry *ce)
{
	const char *ret;
	if (!ce->value)
	{
		config_status("%s:%i: loadmodule without filename",
			ce->file->filename, ce->line_number);
		return -1;
	}

	if (is_blacklisted_module(ce->value))
	{
		/* config_warn("%s:%i: Module '%s' is blacklisted, not loading",
			ce->file->filename, ce->line_number, ce->value); */
		return 1;
	}

	if ((ret = Module_Create(ce->value))) {
		config_error("%s:%i: loadmodule %s: failed to load: %s",
			ce->file->filename, ce->line_number,
			ce->value, ret);
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
	const char *path;
	ConfigItem_blacklist_module *m;

	if (!ce->value)
	{
		config_status("%s:%i: blacklist-module: no module name given to blacklist",
			ce->file->filename, ce->line_number);
		return -1;
	}

	path = Module_TransformPath(ce->value);

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
			ce->file->filename, ce->line_number, ce->value);
		/* fallthrough */
	}

	m = safe_alloc(sizeof(ConfigItem_blacklist_module));
	safe_strdup(m->name, ce->value);
	AddListItem(m, conf_blacklist_module);

	return 0;
}

int is_blacklisted_module(const char *name)
{
	const char *path = Module_TransformPath(name);
	ConfigItem_blacklist_module *m;

	for (m = conf_blacklist_module; m; m = m->next)
		if (!strcasecmp(m->name, name) || !strcasecmp(m->name, path))
			return 1;

	return 0;
}

void start_listeners(void)
{
	ConfigItem_listen *listener;
	int failed = 0, ports_bound = 0;
	char boundmsg_ipv4[512], boundmsg_ipv6[512];
	int last_errno = 0;

	*boundmsg_ipv4 = *boundmsg_ipv6 = '\0';

	for (listener = conf_listen; listener; listener = listener->next)
	{
		/* Try to bind to any ports that are not yet bound and not marked as temporary */
		if (!(listener->options & LISTENER_BOUND) && !listener->flag.temporary)
		{
			if (add_listener(listener) == -1)
			{
				/* Error already printed upstream */
				failed = 1;
				last_errno = ERRNO;
			} else {
				if (loop.booted)
				{
					unreal_log(ULOG_INFO, "listen", "LISTEN_ADDED", NULL,
					           "UnrealIRCd is now also listening on $listen_ip:$listen_port",
					           log_data_string("listen_ip", listener->ip),
					           log_data_integer("listen_port", listener->port));
				} else {
					switch (listener->socket_type)
					{
						case SOCKET_TYPE_IPV4:
							snprintf(boundmsg_ipv4+strlen(boundmsg_ipv4), sizeof(boundmsg_ipv4)-strlen(boundmsg_ipv4),
								"%s:%d%s, ", listener->ip, listener->port,
								listener->options & LISTENER_TLS ? "(TLS)" : "");
							break;
						case SOCKET_TYPE_IPV6:
							snprintf(boundmsg_ipv6+strlen(boundmsg_ipv6), sizeof(boundmsg_ipv6)-strlen(boundmsg_ipv6),
								"%s:%d%s, ", listener->ip, listener->port,
								listener->options & LISTENER_TLS ? "(TLS)" : "");
							break;
						// TODO: show unix domain sockets ;)
						default:
							break;
					}
				}
			}
		}

		/* NOTE: do not merge this with code above (nor in an else block),
		 * as add_listener() affects this flag.
		 */
		if (listener->options & LISTENER_BOUND)
			ports_bound++;
	}

	if (ports_bound == 0)
	{
#ifdef _WIN32
		if (last_errno == WSAEADDRINUSE)
#else
		if (last_errno == EADDRINUSE)
#endif
		{
			/* We can be specific */
			unreal_log(ULOG_FATAL, "listen", "ALL_LISTEN_PORTS_FAILED", NULL,
				   "Unable to listen on any ports. "
				   "Most likely UnrealIRCd is already running.");
		} else {
			unreal_log(ULOG_FATAL, "listen", "ALL_LISTEN_PORTS_FAILED", NULL,
				   "Unable to listen on any ports. "
				   "Please verify that no other process is using the ports. "
				   "Also, on some IRCd shells you may have to use listen::bind-ip "
				   "with a specific IP assigned to you (rather than \"*\").");
		}
		exit(-1);
	}

	if (failed && !loop.booted)
	{
		unreal_log(ULOG_FATAL, "listen", "SOME_LISTEN_PORTS_FAILED", NULL,
			   "Unable to listen on all ports (some of them succeeded, some of them failed). "
			   "Please verify that no other process is using the port(s). "
			   "Also, on some IRCd shells you may have to use listen::bind-ip "
			   "with a specific IP assigned to you (rather than \"*\").");
		exit(-1);
	}

	if (!loop.booted)
	{
		if (strlen(boundmsg_ipv4) > 2)
			boundmsg_ipv4[strlen(boundmsg_ipv4)-2] = '\0';
		if (strlen(boundmsg_ipv6) > 2)
			boundmsg_ipv6[strlen(boundmsg_ipv6)-2] = '\0';

		if (!*boundmsg_ipv4)
			strlcpy(boundmsg_ipv4, "<none>", sizeof(boundmsg_ipv4));
		if (!*boundmsg_ipv6)
			strlcpy(boundmsg_ipv6, "<none>", sizeof(boundmsg_ipv6));

		unreal_log(ULOG_INFO, "listen", "LISTENING", NULL,
		           "UnrealIRCd is now listening on the following addresses/ports:\n"
		           "IPv4: $ipv4_port_list\n"
		           "IPv6: $ipv6_port_list\n",
		           log_data_string("ipv4_port_list", boundmsg_ipv4),
		           log_data_string("ipv6_port_list", boundmsg_ipv6));
	}
}

/* Actually use configuration */
void config_run(void)
{
	extcmodes_check_for_changes();
	start_listeners();
	add_proc_io_server();
	free_all_config_resources();
}

int	_conf_offchans(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;

	for (cep = ce->items; cep; cep = cep->next)
	{
		ConfigItem_offchans *of = safe_alloc(sizeof(ConfigItem_offchans));
		strlcpy(of->name, cep->name, CHANNELLEN+1);
		for (cepp = cep->items; cepp; cepp = cepp->next)
		{
			if (!strcmp(cepp->name, "topic"))
				safe_strdup(of->topic, cepp->value);
		}
		AddListItem(of, conf_offchans);
	}
	return 0;
}

int	_test_offchans(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	ConfigEntry *cep, *cep2;

	if (!ce->items)
	{
		config_error("%s:%i: empty official-channels block",
			ce->file->filename, ce->line_number);
		return 1;
	}

	config_warn("set::official-channels is deprecated. It often does not do what you want. "
	            "You're better of creating a channel, setting all modes, topic, etc. to your liking "
	            "and then making the channel permanent (MODE #channel +P). "
	            "The channel will then be stored in a database to preserve it between restarts.");

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (strlen(cep->name) > CHANNELLEN)
		{
			config_error("%s:%i: official-channels: '%s' name too long (max %d characters).",
				cep->file->filename, cep->line_number, cep->name, CHANNELLEN);
			errors++;
			continue;
		}
		if (!valid_channelname(cep->name))
		{
			config_error("%s:%i: official-channels: '%s' is not a valid channel name.",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
			continue;
		}
		for (cep2 = cep->items; cep2; cep2 = cep2->next)
		{
			if (!cep2->value)
			{
				config_error_empty(cep2->file->filename,
					cep2->line_number, "official-channels",
					cep2->name);
				errors++;
				continue;
			}
			if (!strcmp(cep2->name, "topic"))
			{
				if (strlen(cep2->value) > MAXTOPICLEN)
				{
					config_error("%s:%i: official-channels::%s: topic too long (max %d characters).",
						cep2->file->filename, cep2->line_number, cep->name, MAXTOPICLEN);
					errors++;
					continue;
				}
			} else {
				config_error_unknown(cep2->file->filename,
					cep2->line_number, "official-channels",
					cep2->name);
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

	if ((cmptr = find_command(ce->value, CMD_ALIAS)))
		CommandDelX(NULL, cmptr);
	if (find_command_simple(ce->value))
	{
		config_warn("%s:%i: Alias '%s' would conflict with command (or server token) '%s', alias not added.",
			ce->file->filename, ce->line_number,
			ce->value, ce->value);
		return 0;
	}
	if ((alias = find_alias(ce->value)))
		DelListItem(alias, conf_alias);
	alias = safe_alloc(sizeof(ConfigItem_alias));
	safe_strdup(alias->alias, ce->value);
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "format")) {
			format = safe_alloc(sizeof(ConfigItem_alias_format));
			safe_strdup(format->format, cep->value);
			format->expr = unreal_create_match(MATCH_PCRE_REGEX, cep->value, NULL);
			if (!format->expr)
				abort(); /* Impossible due to _test_alias earlier */
			for (cepp = cep->items; cepp; cepp = cepp->next) {
				if (!strcmp(cepp->name, "nick") ||
				    !strcmp(cepp->name, "target") ||
				    !strcmp(cepp->name, "command")) {
					safe_strdup(format->nick, cepp->value);
				}
				else if (!strcmp(cepp->name, "parameters")) {
					safe_strdup(format->parameters, cepp->value);
				}
				else if (!strcmp(cepp->name, "type")) {
					if (!strcmp(cepp->value, "services"))
						format->type = ALIAS_SERVICES;
					else if (!strcmp(cepp->value, "stats"))
						format->type = ALIAS_STATS;
					else if (!strcmp(cepp->value, "normal"))
						format->type = ALIAS_NORMAL;
					else if (!strcmp(cepp->value, "channel"))
						format->type = ALIAS_CHANNEL;
					else if (!strcmp(cepp->value, "real"))
						format->type = ALIAS_REAL;
				}
			}
			AddListItem(format, alias->format);
		}

		else if (!strcmp(cep->name, "nick") || !strcmp(cep->name, "target"))
		{
			safe_strdup(alias->nick, cep->value);
		}
		else if (!strcmp(cep->name, "type")) {
			if (!strcmp(cep->value, "services"))
				alias->type = ALIAS_SERVICES;
			else if (!strcmp(cep->value, "stats"))
				alias->type = ALIAS_STATS;
			else if (!strcmp(cep->value, "normal"))
				alias->type = ALIAS_NORMAL;
			else if (!strcmp(cep->value, "channel"))
				alias->type = ALIAS_CHANNEL;
			else if (!strcmp(cep->value, "command"))
				alias->type = ALIAS_COMMAND;
		}
		else if (!strcmp(cep->name, "spamfilter"))
			alias->spamfilter = config_checkval(cep->value, CFG_YESNO);
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

	if (!ce->items)
	{
		config_error("%s:%i: empty alias block",
			ce->file->filename, ce->line_number);
		return 1;
	}
	if (!ce->value)
	{
		config_error("%s:%i: alias without name",
			ce->file->filename, ce->line_number);
		errors++;
	}
	else if (!find_command(ce->value, CMD_ALIAS) && find_command(ce->value, 0)) {
		config_status("%s:%i: %s is an existing command, can not add alias",
			ce->file->filename, ce->line_number, ce->value);
		errors++;
	}
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (config_is_blankorempty(cep, "alias"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->name, "format")) {
			char *err = NULL;
			Match *expr;
			char has_type = 0, has_target = 0, has_parameters = 0;

			has_format = 1;
			expr = unreal_create_match(MATCH_PCRE_REGEX, cep->value, &err);
			if (!expr)
			{
				config_error("%s:%i: alias::format contains an invalid regex: %s",
					cep->file->filename, cep->line_number, err);
			} else {
				unreal_delete_match(expr);
			}

			for (cepp = cep->items; cepp; cepp = cepp->next) {
				if (config_is_blankorempty(cepp, "alias::format"))
				{
					errors++;
					continue;
				}
				if (!strcmp(cepp->name, "nick") ||
				    !strcmp(cepp->name, "command") ||
				    !strcmp(cepp->name, "target"))
				{
					if (has_target)
					{
						config_warn_duplicate(cepp->file->filename,
							cepp->line_number,
							"alias::format::target");
						continue;
					}
					has_target = 1;
				}
				else if (!strcmp(cepp->name, "type"))
				{
					if (has_type)
					{
						config_warn_duplicate(cepp->file->filename,
							cepp->line_number,
							"alias::format::type");
						continue;
					}
					has_type = 1;
					if (!strcmp(cepp->value, "services"))
						;
					else if (!strcmp(cepp->value, "stats"))
						;
					else if (!strcmp(cepp->value, "normal"))
						;
					else if (!strcmp(cepp->value, "channel"))
						;
					else if (!strcmp(cepp->value, "real"))
						;
					else
					{
						config_error("%s:%i: unknown alias type",
						cepp->file->filename, cepp->line_number);
						errors++;
					}
				}
				else if (!strcmp(cepp->name, "parameters"))
				{
					if (has_parameters)
					{
						config_warn_duplicate(cepp->file->filename,
							cepp->line_number,
							"alias::format::parameters");
						continue;
					}
					has_parameters = 1;
				}
				else
				{
					config_error_unknown(cepp->file->filename,
						cepp->line_number, "alias::format",
						cepp->name);
					errors++;
				}
			}
			if (!has_target)
			{
				config_error_missing(cep->file->filename,
					cep->line_number, "alias::format::target");
				errors++;
			}
			if (!has_type)
			{
				config_error_missing(cep->file->filename,
					cep->line_number, "alias::format::type");
				errors++;
			}
			if (!has_parameters)
			{
				config_error_missing(cep->file->filename,
					cep->line_number, "alias::format::parameters");
				errors++;
			}
		}
		else if (!strcmp(cep->name, "nick") || !strcmp(cep->name, "target"))
		{
			if (has_target)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "alias::target");
				continue;
			}
			has_target = 1;
		}
		else if (!strcmp(cep->name, "type")) {
			if (has_type)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "alias::type");
				continue;
			}
			has_type = 1;
			if (!strcmp(cep->value, "services"))
				;
			else if (!strcmp(cep->value, "stats"))
				;
			else if (!strcmp(cep->value, "normal"))
				;
			else if (!strcmp(cep->value, "channel"))
				;
			else if (!strcmp(cep->value, "command"))
				type = 'c';
			else {
				config_error("%s:%i: unknown alias type",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "spamfilter"))
			;
		else {
			config_error_unknown(cep->file->filename, cep->line_number,
				"alias", cep->name);
			errors++;
		}
	}
	if (!has_type)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"alias::type");
		errors++;
	}
	if (!has_format && type == 'c')
	{
		config_error("%s:%d: alias::type is 'command' but no alias::format was specified",
			ce->file->filename, ce->line_number);
		errors++;
	}
	else if (has_format && type != 'c')
	{
		config_error("%s:%d: alias::format specified when type is not 'command'",
			ce->file->filename, ce->line_number);
		errors++;
	}
	return errors;
}

int	_conf_deny(ConfigFile *conf, ConfigEntry *ce)
{
Hook *h;

	if (!strcmp(ce->value, "channel"))
		_conf_deny_channel(conf, ce);
	else if (!strcmp(ce->value, "link"))
		_conf_deny_link(conf, ce);
	else if (!strcmp(ce->value, "version"))
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
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "channel"))
		{
			safe_strdup(deny->channel, cep->value);
		}
		else if (!strcmp(cep->name, "redirect"))
		{
			safe_strdup(deny->redirect, cep->value);
		}
		else if (!strcmp(cep->name, "reason"))
		{
			safe_strdup(deny->reason, cep->value);
		}
		else if (!strcmp(cep->name, "warn"))
		{
			deny->warn = config_checkval(cep->value,CFG_YESNO);
		}
		else if (!strcmp(cep->name, "class"))
		{
			safe_strdup(deny->class, cep->value);
		}
		else if (!strcmp(cep->name, "mask"))
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
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "mask"))
		{
			unreal_add_masks(&deny->mask, cep);
		}
		else if (!strcmp(cep->name, "rule"))
		{
			deny->rule = (char *)crule_parse(cep->value);
			safe_strdup(deny->prettyrule, cep->value);
		}
		else if (!strcmp(cep->name, "type")) {
			if (!strcmp(cep->value, "all"))
				deny->flag.type = CRULE_ALL;
			else if (!strcmp(cep->value, "auto"))
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
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "mask"))
		{
			safe_strdup(deny->mask, cep->value);
		}
		else if (!strcmp(cep->name, "version"))
		{
			safe_strdup(deny->version, cep->value);
		}
		else if (!strcmp(cep->name, "flags"))
		{
			safe_strdup(deny->flags, cep->value);
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

	if (!ce->value)
	{
		config_error("%s:%i: deny without type",
			ce->file->filename, ce->line_number);
		return 1;
	}
	if (!strcmp(ce->value, "channel"))
	{
		char has_channel = 0, has_warn = 0, has_reason = 0, has_redirect = 0, has_class = 0;
		for (cep = ce->items; cep; cep = cep->next)
		{
			if (config_is_blankorempty(cep, "deny channel"))
			{
				errors++;
				continue;
			}
			if (!strcmp(cep->name, "channel"))
			{
				if (has_channel)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "deny channel::channel");
					continue;
				}
				has_channel = 1;
			}
			else if (!strcmp(cep->name, "redirect"))
			{
				if (has_redirect)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "deny channel::redirect");
					continue;
				}
				has_redirect = 1;
			}
			else if (!strcmp(cep->name, "reason"))
			{
				if (has_reason)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "deny channel::reason");
					continue;
				}
				has_reason = 1;
			}
			else if (!strcmp(cep->name, "warn"))
			{
				if (has_warn)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "deny channel::warn");
					continue;
				}
				has_warn = 1;
			}
			else if (!strcmp(cep->name, "class"))
			{
				if (has_class)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "deny channel::class");
					continue;
				}
				has_class = 1;
			}
			else if (!strcmp(cep->name, "mask"))
			{
			}
			else
			{
				config_error_unknown(cep->file->filename,
					cep->line_number, "deny channel", cep->name);
				errors++;
			}
		}
		if (!has_channel)
		{
			config_error_missing(ce->file->filename, ce->line_number,
				"deny channel::channel");
			errors++;
		}
		if (!has_reason)
		{
			config_error_missing(ce->file->filename, ce->line_number,
				"deny channel::reason");
			errors++;
		}
	}
	else if (!strcmp(ce->value, "link"))
	{
		char has_mask = 0, has_rule = 0, has_type = 0;
		for (cep = ce->items; cep; cep = cep->next)
		{
			if (!cep->items)
			{
				if (config_is_blankorempty(cep, "deny link"))
				{
					errors++;
					continue;
				}
				else if (!strcmp(cep->name, "mask"))
				{
					has_mask = 1;
				} else if (!strcmp(cep->name, "rule"))
				{
					int val = 0;
					if (has_rule)
					{
						config_warn_duplicate(cep->file->filename,
							cep->line_number, "deny link::rule");
						continue;
					}
					has_rule = 1;
					if ((val = crule_test(cep->value)))
					{
						config_error("%s:%i: deny link::rule contains an invalid expression: %s",
							cep->file->filename,
							cep->line_number,
							crule_errstring(val));
						errors++;
					}
				}
				else if (!strcmp(cep->name, "type"))
				{
					if (has_type)
					{
						config_warn_duplicate(cep->file->filename,
							cep->line_number, "deny link::type");
						continue;
					}
					has_type = 1;
					if (!strcmp(cep->value, "auto"))
					;
					else if (!strcmp(cep->value, "all"))
					;
					else {
						config_status("%s:%i: unknown deny link type",
						cep->file->filename, cep->line_number);
						errors++;
					}
				}
				else
				{
					config_error_unknown(cep->file->filename,
						cep->line_number, "deny link", cep->name);
					errors++;
				}
			}
			else
			{
				// Sections
				if (!strcmp(cep->name, "mask"))
				{
					if (cep->value || cep->items)
						has_mask = 1;
				}
				else
				{
					config_error_unknown(cep->file->filename,
						cep->line_number, "deny link", cep->name);
					errors++;
					continue;
				}
			}
		}
		if (!has_mask)
		{
			config_error_missing(ce->file->filename, ce->line_number,
				"deny link::mask");
			errors++;
		}
		if (!has_rule)
		{
			config_error_missing(ce->file->filename, ce->line_number,
				"deny link::rule");
			errors++;
		}
		if (!has_type)
		{
			config_error_missing(ce->file->filename, ce->line_number,
				"deny link::type");
			errors++;
		}
	}
	else if (!strcmp(ce->value, "version"))
	{
		char has_mask = 0, has_version = 0, has_flags = 0;
		for (cep = ce->items; cep; cep = cep->next)
		{
			if (config_is_blankorempty(cep, "deny version"))
			{
				errors++;
				continue;
			}
			if (!strcmp(cep->name, "mask"))
			{
				if (has_mask)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "deny version::mask");
					continue;
				}
				has_mask = 1;
			}
			else if (!strcmp(cep->name, "version"))
			{
				if (has_version)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "deny version::version");
					continue;
				}
				has_version = 1;
			}
			else if (!strcmp(cep->name, "flags"))
			{
				if (has_flags)
				{
					config_warn_duplicate(cep->file->filename,
						cep->line_number, "deny version::flags");
					continue;
				}
				has_flags = 1;
			}
			else
			{
				config_error_unknown(cep->file->filename,
					cep->line_number, "deny version", cep->name);
				errors++;
			}
		}
		if (!has_mask)
		{
			config_error_missing(ce->file->filename, ce->line_number,
				"deny version::mask");
			errors++;
		}
		if (!has_version)
		{
			config_error_missing(ce->file->filename, ce->line_number,
				"deny version::version");
			errors++;
		}
		if (!has_flags)
		{
			config_error_missing(ce->file->filename, ce->line_number,
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
				ce->file->filename, ce->line_number,
				ce->value);
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

	if (!ce->value)
	{
		config_error("%s:%i: security-group block needs a name, eg: security-group web-users {",
			ce->file->filename, ce->line_number);
		errors++;
	} else {
		if (!strcasecmp(ce->value, "unknown-users"))
		{
			config_error("%s:%i: The 'unknown-users' group is a special group that is the "
			             "inverse of 'known-users', you cannot create or adjust it in the "
			             "config file, as it is created automatically by UnrealIRCd.",
			             ce->file->filename, ce->line_number);
			errors++;
			return errors;
		}
		if (!security_group_valid_name(ce->value))
		{
			config_error("%s:%i: security-group block name '%s' contains invalid characters or is too long. "
			             "Only letters, numbers, underscore and hyphen are allowed.",
			             ce->file->filename, ce->line_number, ce->value);
			errors++;
		}
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "webirc") || !strcmp(cep->name, "exclude-webirc"))
		{
			CheckNull(cep);
		} else
		if (!strcmp(cep->name, "identified") || !strcmp(cep->name, "exclude-identified"))
		{
			CheckNull(cep);
		} else
		if (!strcmp(cep->name, "tls") || !strcmp(cep->name, "exclude-tls"))
		{
			CheckNull(cep);
		} else
		if (!strcmp(cep->name, "reputation-score") || !strcmp(cep->name, "exclude-reputation-score"))
		{
			const char *str = cep->value;
			int v;
			CheckNull(cep);
			if (*str == '<')
				str++;
			v = atoi(str);
			if ((v < 1) || (v > 10000))
			{
				config_error("%s:%i: security-group::%s needs to be a value of 1-10000",
					cep->file->filename, cep->line_number, cep->name);
				errors++;
			}
		} else
		if (!strcmp(cep->name, "connect-time") || !strcmp(cep->name, "exclude-connect-time"))
		{
			const char *str = cep->value;
			long v;
			CheckNull(cep);
			if (*str == '<')
				str++;
			v = config_checkval(str, CFG_TIME);
			if (v < 1)
			{
				config_error("%s:%i: security-group::%s needs to be a time value (and more than 0 seconds)",
					cep->file->filename, cep->line_number, cep->name);
				errors++;
			}
		} else
		if (!strcmp(cep->name, "mask") || !strcmp(cep->name, "include-mask") || !strcmp(cep->name, "exclude-mask"))
		{
		} else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"security-group", cep->name);
			errors++;
			continue;
		}
	}

	return errors;
}

int _conf_security_group(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	SecurityGroup *s = add_security_group(ce->value, 1);

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "priority"))
		{
			s->priority = atoi(cep->value);
			DelListItem(s, securitygroups);
			AddListItemPrio(s, securitygroups, s->priority);
		}
		else if (!strcmp(cep->name, "webirc"))
			s->webirc = config_checkval(cep->value, CFG_YESNO);
		else if (!strcmp(cep->name, "identified"))
			s->identified = config_checkval(cep->value, CFG_YESNO);
		else if (!strcmp(cep->name, "tls"))
			s->tls = config_checkval(cep->value, CFG_YESNO);
		else if (!strcmp(cep->name, "reputation-score"))
		{
			if (*cep->value == '<')
				s->reputation_score = 0 - atoi(cep->value+1);
			else
				s->reputation_score = atoi(cep->value);
		}
		else if (!strcmp(cep->name, "connect-time"))
		{
			if (*cep->value == '<')
				s->connect_time = 0 - config_checkval(cep->value+1, CFG_TIME);
			else
				s->connect_time = config_checkval(cep->value, CFG_TIME);
		}
		else if (!strcmp(cep->name, "include-mask"))
		{
			unreal_add_masks(&s->include_mask, cep);
		}
		else if (!strcmp(cep->name, "exclude-webirc"))
			s->exclude_webirc = config_checkval(cep->value, CFG_YESNO);
		else if (!strcmp(cep->name, "exclude-identified"))
			s->exclude_identified = config_checkval(cep->value, CFG_YESNO);
		else if (!strcmp(cep->name, "exclude-tls"))
			s->exclude_tls = config_checkval(cep->value, CFG_YESNO);
		else if (!strcmp(cep->name, "exclude-reputation-score"))
		{
			if (*cep->value == '<')
				s->exclude_reputation_score = 0 - atoi(cep->value+1);
			else
				s->exclude_reputation_score = atoi(cep->value);
		}
		else if (!strcmp(cep->name, "exclude-mask"))
		{
			unreal_add_masks(&s->exclude_mask, cep);
		}
	}
	return 1;
}

Secret *find_secret(const char *secret_name)
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

char *_conf_secret_read_password_file(const char *fname)
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

char *_conf_secret_read_prompt(const char *blockname)
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

	if (!ce->value)
	{
		config_error("%s:%i: secret block needs a name, eg: secret xyz {",
			ce->file->filename, ce->line_number);
		errors++;
		return errors; /* need to return here since we dereference ce->value later.. */
	} else {
		if (!security_group_valid_name(ce->value))
		{
			config_error("%s:%i: secret block name '%s' contains invalid characters or is too long. "
			             "Only letters, numbers, underscore and hyphen are allowed.",
			             ce->file->filename, ce->line_number, ce->value);
			errors++;
		}
	}

	existing = find_secret(ce->value);

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "password"))
		{
			int n;
			has_password = 1;
			CheckNull(cep);
			if (cep->items ||
			    (((n = Auth_AutoDetectHashType(cep->value))) && ((n == AUTHTYPE_BCRYPT) || (n == AUTHTYPE_ARGON2))))
			{
				config_error("%s:%d: you cannot use hashed passwords here, see "
				             "https://www.unrealircd.org/docs/Secret_block#secret-plaintext",
				             cep->file->filename, cep->line_number);
				errors++;
				continue;
			}
			if (!valid_secret_password(cep->value, &err))
			{
				config_error("%s:%d: secret::password does not meet password complexity requirements: %s",
				             cep->file->filename, cep->line_number, err);
				errors++;
			}
		} else
		if (!strcmp(cep->name, "password-file"))
		{
			char *str;
			has_password_file = 1;
			CheckNull(cep);
			convert_to_absolute_path(&cep->value, CONFDIR);
			if (!file_exists(cep->value) && existing && existing->password)
			{
				/* Silently ignore the case where a secret block already
				 * has the password read and now the file is no longer available.
				 * This so secret::password-file can be used only to boot
				 * and then the media (eg: USB stick) can be pulled.
				 */
			} else
			{
				str = _conf_secret_read_password_file(cep->value);
				if (!str)
				{
					config_error("%s:%d: secret::password-file: error reading password from file, see error from above.",
						cep->file->filename, cep->line_number);
					errors++;
				}
				safe_free_sensitive(str);
			}
		} else
		if (!strcmp(cep->name, "password-prompt"))
		{
#ifdef _WIN32
			config_error("%s:%d: secret::password-prompt is not implemented in Windows at the moment, sorry!",
				cep->file->filename, cep->line_number);
			config_error("Choose a different method to enter passwords or use *NIX");
			errors++;
			return errors;
#endif
			has_password_prompt = 1;
			if (loop.booted && !find_secret(ce->value))
			{
				config_error("%s:%d: you cannot add a new secret { } block that uses password-prompt and then /REHASH. "
				             "With 'password-prompt' you can only add such a password on boot.",
				             cep->file->filename, cep->line_number);
				config_error("Either use a different method to enter passwords or restart the IRCd on the console.");
				errors++;
			}
			if (!loop.booted && !running_interactively())
			{
				config_error("ERROR: IRCd is not running interactively, but via a cron job or something similar.");
				config_error("%s:%d: unable to prompt for password since IRCd is not started in a terminal",
					cep->file->filename, cep->line_number);
				config_error("Either use a different method to enter passwords or start the IRCd in a terminal/SSH/..");
			}
		} else
		if (!strcmp(cep->name, "password-url"))
		{
			config_error("%s:%d: secret::password-url is not supported yet in this UnrealIRCd version.",
				cep->file->filename, cep->line_number);
			errors++;
		} else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"secret", cep->name);
			errors++;
			continue;
		}
		if (cep->items)
		{
			config_error("%s:%d: secret::%s does not support sub-options (%s)",
				cep->file->filename, cep->line_number,
				cep->name, cep->items->name);
			errors++;
		}
	}

	if (!has_password && !has_password_file && !has_password_prompt)
	{
		config_error("%s:%d: secret { } block must contain 1 of: password OR password-file OR password-prompt",
			ce->file->filename, ce->line_number);
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
	Secret *existing = find_secret(ce->value);

	s = safe_alloc(sizeof(Secret));
	safe_strdup(s->name, ce->value);

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "password"))
		{
			safe_strdup_sensitive(s->password, cep->value);
			destroy_string(cep->value); /* destroy the original */
		} else
		if (!strcmp(cep->name, "password-file"))
		{
			if (!file_exists(cep->value) && existing && existing->password)
			{
				/* Silently ignore the case where a secret block already
				 * has the password read and now the file is no longer available.
				 * This so secret::password-file can be used only to boot
				 * and then the media (eg: USB stick) can be pulled.
				 */
			} else
			{
				s->password = _conf_secret_read_password_file(cep->value);
			}
		} else
		if (!strcmp(cep->name, "password-prompt"))
		{
			if (!loop.booted && running_interactively())
			{
				s->password = _conf_secret_read_prompt(ce->value);
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

void resource_download_complete(const char *url, const char *file, const char *errorbuf, int cached, void *rs_key)
{
	ConfigResource *rs = (ConfigResource *)rs_key;

	rs->type &= ~RESOURCE_DLQUEUED;

	if (config_verbose)
		config_status("resource_download_complete() for %s [%s]", url, errorbuf?errorbuf:"success");

	if (!file && !cached)
	{
		/* DOWNLOAD FAILED */
		if (rs->cache_file)
		{
			unreal_log(ULOG_ERROR, "config", "DOWNLOAD_FAILED_SOFT", NULL,
				   "$file:$line_number: Failed to download '$url': $error_message\n"
				   "Using a cached copy instead.",
				   log_data_string("file", rs->wce->ce->file->filename),
				   log_data_integer("line_number", rs->wce->ce->line_number),
				   log_data_string("url", displayurl(url)),
				   log_data_string("error_message", errorbuf));
			safe_strdup(rs->file, rs->cache_file);
		} else {
			unreal_log(ULOG_ERROR, "config", "DOWNLOAD_FAILED_HARD", NULL,
				   "$file:$line_number: Failed to download '$url': $error_message",
				   log_data_string("file", rs->wce->ce->file->filename),
				   log_data_integer("line_number", rs->wce->ce->line_number),
				   log_data_string("url", displayurl(url)),
				   log_data_string("error_message", errorbuf));
			/* Set error condition, this so config_read_file() later will stop. */
			loop.config_load_failed = 1;
			/* We keep the other transfers running since they may raise (more) errors.
			 * Which can be helpful so you can differentiate between an error of an
			 * include on one server, or complete lack of internet connectvitity.
			 */
		}
	}
	else
	{
		if (cached)
		{
			/* Copy from cache */
			safe_strdup(rs->file, rs->cache_file);
		} else {
			/* Copy to cache */
			const char *cache_file = unreal_mkcache(url);
			unreal_copyfileex(file, cache_file, 1);
			safe_strdup(rs->file, cache_file);
		}
	}

	if (rs->file)
	{
		if (rs->type & RESOURCE_INCLUDE)
		{
			if (config_read_file(rs->file, (char *)displayurl(rs->url)) < 0)
				loop.config_load_failed = 1;
		} else {
			ConfigEntryWrapper *wce;
			for (wce = rs->wce; wce; wce = wce->next)
				safe_strdup(wce->ce->value, rs->file); // now information of url is lost, hm!!
		}
	}
}

/** Request to REHASH the configuration file.
 * There is no guarantee that the request will be done immediately
 * (eg: it won't in case of remote includes).
 * @param client	The client requesting the /REHASH.
 *                      If this is NULL then the rehash was requested
 *                      via a signal to the process or GUI.
 */
void request_rehash(Client *client)
{
	if (loop.rehashing)
	{
		if (client)
			sendnotice(client, "A rehash is already in progress");
		return;
	}

	loop.rehashing = 1;
	loop.rehash_save_client = client;
	config_read_start();
	/* If we already have everything, then can we proceed with the rehash */
	if (is_config_read_finished())
	{
		rehash_internal(client);
		return;
	}
	/* Otherwise, I/O events will take care of it later
	 * after all remote includes have been downloaded.
	 */
}

int rehash_internal(Client *client)
{
	int failure;

	/* Log it here if it is by a signal */
	if (client == NULL)
		unreal_log(ULOG_INFO, "config", "CONFIG_RELOAD", client, "Rehashing server configuration file [./unrealircd rehash]");

	loop.rehashing = 2; /* now doing the actual rehash */

	failure = config_test();
	if (failure == 0)
		config_run();
	/* TODO: uh.. are we supposed to do all this for a failed rehash too? maybe some but not all? */
	reread_motdsandrules();
	unload_all_unused_umodes();
	unload_all_unused_extcmodes();
	unload_all_unused_caps();
	unload_all_unused_history_backends();
	// unload_all_unused_moddata(); -- this will crash
	umodes_check_for_changes();
	charsys_check_for_changes();

	/* Clear everything now that we are done */
	loop.rehashing = 0;
	remote_rehash_client = NULL;
	procio_post_rehash(failure);
	return 1;
}

void link_cleanup(ConfigItem_link *link_ptr)
{
	safe_free(link_ptr->servername);
	unreal_delete_masks(link_ptr->incoming.mask);
	Auth_FreeAuthConfig(link_ptr->auth);
	safe_free(link_ptr->outgoing.file);
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
			safe_free(listen_ptr->websocket_forward);
			safe_free(listen_ptr);
			i++;
		}
	}

	if (i)
		close_unbound_listeners();
}

ConfigResource *find_config_resource(const char *resource)
{
	ConfigResource *rs;

	for (rs = config_resources; rs; rs = rs->next)
	{
#ifdef _WIN32
		if (rs->file && !strcasecmp(resource, rs->file))
			return rs;
#else
		if (rs->file && !strcmp(resource, rs->file))
			return rs;
#endif
		if (rs->url && !strcasecmp(resource, rs->url))
			return rs;
	}
	return NULL;
}

/* Add configuration resource to list.
 * For files this doesn't do terribly much, except that you can use
 * the return value to judge on whether you should call config_read_file() or not.
 * For urls this adds the resource to the list of links to be downloaded.
 * @param resource	File or URL of the resource
 * @param type		A RESOURCE_ type such as RESOURCE_INCLUDE
 * @param ce		The ConfigEntry where the add_config_resource() happened
 *			for, such as the include block, etc.
 * @returns 0 if the file is already on our list (so no need to load it!)
 */
int add_config_resource(const char *resource, int type, ConfigEntry *ce)
{
	ConfigResource *rs;
	ConfigEntryWrapper *wce;

	if (config_verbose)
		config_status("add_config_resource() for '%s", resource);

	wce = safe_alloc(sizeof(ConfigEntryWrapper));
	wce->ce = ce;

	rs = find_config_resource(resource);
	if (rs)
	{
		/* Existing entry, add us to the list of
		 * items who are interested in this resource ;)
		 */
		AddListItem(wce, rs->wce);
		return 0;
	}

	/* New entry */
	rs = safe_alloc(sizeof(ConfigResource));
	rs->wce = wce;
	AddListItem(rs, config_resources);

	if (!url_is_valid(resource))
	{
		safe_strdup(rs->file, resource);
	} else {
		const char *cache_file;
		time_t modtime;

		safe_strdup(rs->url, resource);
		rs->type = type|RESOURCE_REMOTE|RESOURCE_DLQUEUED;

		cache_file = unreal_mkcache(rs->url);
		modtime = unreal_getfilemodtime(cache_file);
		if (modtime > 0)
		{
			safe_strdup(rs->cache_file, cache_file); /* Cached copy is available */
			/* Check if there is an "url-refresh" argument */
			ConfigEntry *cep, *prev = NULL;
			for (cep = ce->items; cep; cep = cep->next)
			{
				if (!strcmp(cep->name, "url-refresh"))
				{
					/* First find out the time value of url-refresh... (eg '7d' -> 86400*7) */
					long refresh_time = 0;
					if (cep->value)
						refresh_time = config_checkval(cep->value, CFG_TIME);
					/* Then remove the config item so it is not seen by the rest of unrealircd.
					 * Can't use DelListItem() here as ConfigEntry has no ->prev, only ->next.
					 */
					if (prev)
						prev->next = cep->next; /* (skip over us) */
					else
						ce->items = cep->next; /* (new head) */
					/* ..and free it */
					config_entry_free(cep);
					/* And now check if the current cached copy is recent enough */
					if (TStime() - modtime < refresh_time)
					{
						/* Don't download, use cached copy */
						//config_status("DEBUG: using cached copy due to url-refresh %ld", refresh_time);
						resource_download_complete(rs->url, NULL, NULL, 1, rs);
						return 1;
					} else {
						//config_status("DEBUG: requires download attempt, out of date url-refresh %ld < %ld", refresh_time, TStime() - modtime);
					}
					break; // MUST break now as we touched the linked list.
				}
				prev = cep;
			}
		}
		download_file_async(rs->url, modtime, resource_download_complete, (void *)rs, NULL, DOWNLOAD_MAX_REDIRECTS);
	}
	return 1;
}

void free_all_config_resources(void)
{
	ConfigResource *rs, *next;
	ConfigEntryWrapper *wce, *wce_next;

	for (rs = config_resources; rs; rs = next)
	{
		next = rs->next;
		for (wce = rs->wce; wce; wce = wce_next)
		{
			wce_next = wce->next;
			safe_free(wce);
		}
		rs->wce = NULL;
		if (rs->type & RESOURCE_REMOTE)
		{
			/* Delete the file, but only if it's not a cached version */
			if (rs->file && strncmp(rs->file, CACHEDIR, strlen(CACHEDIR)))
			{
				remove(rs->file);
			}
			safe_free(rs->url);
		}
		safe_free(rs->file);
		safe_free(rs->cache_file);
		DelListItem(rs, config_resources);
		safe_free(rs);
	}
}

int tls_tests(void)
{
	if (have_tls_listeners == 0)
	{
		config_error("Your server is not listening on any TLS ports.");
		config_status("Add this to your unrealircd.conf: listen { ip %s; port 6697; options { tls; }; };",
		            port_6667_ip ? port_6667_ip : "*");
		config_status("See https://www.unrealircd.org/docs/FAQ#no-tls-ports");
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

const char *link_generator_spkifp(TLSOptions *tlsoptions)
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
	const char *spkifp;

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
		printf("Could not calculate spkifp. Maybe you have uncommon TLS options set? Odd...\n");
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
