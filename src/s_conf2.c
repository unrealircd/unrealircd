/*
 *   Unreal Internet Relay Chat Daemon, src/s_conf2.c
 *   (C) 1998-2000 Chris Behrens & Fred Jacobs (comstud, moogle)
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
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
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "channel.h"
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

#include "h.h"

extern char *my_itoa(long i);
/*
 * TODO:
 *  - allow channel {} (chrestrict)
 *  - deny channel {} (chrestrict)
 *  - deny version {} (V:lines)
 *  - deny dcc {} (dccdeny)
 *  - set {} lines (unrealircd.conf, network files)
 *  - allow {} connfreq (Y:lines)
 *  - converter
*/
#define ircdupstr(x,y) if (x) MyFree(x); x = strdup(y)
#define ircstrdup ircdupstr
typedef struct _confcommand ConfigCommand;
struct	_confcommand
{
	char	*name;
	int	(*func)(ConfigFile *conf, ConfigEntry *ce);
};

typedef struct _conf_operflag OperFlag;
struct _conf_operflag 
{
	long	flag;
	char	*name;
};


/* 
 * Top-level configuration commands -Stskeeps
 */
int	_conf_admin		(ConfigFile *conf, ConfigEntry *ce);
int	_conf_me		(ConfigFile *conf, ConfigEntry *ce);
int	_conf_oper		(ConfigFile *conf, ConfigEntry *ce);
int	_conf_class		(ConfigFile *conf, ConfigEntry *ce);
int	_conf_drpass	(ConfigFile *conf, ConfigEntry *ce);
int	_conf_ulines	(ConfigFile *conf, ConfigEntry *ce);
int	_conf_include	(ConfigFile *conf, ConfigEntry *ce);
int	_conf_tld		(ConfigFile *conf, ConfigEntry *ce);
int	_conf_listen	(ConfigFile *conf, ConfigEntry *ce);
int	_conf_allow		(ConfigFile *conf, ConfigEntry *ce);
int	_conf_except	(ConfigFile *conf, ConfigEntry *ce);
int	_conf_vhost		(ConfigFile *conf, ConfigEntry *ce);
int	_conf_link		(ConfigFile *conf, ConfigEntry *ce);
int	_conf_ban		(ConfigFile *conf, ConfigEntry *ce);

extern int conf_debuglevel;

static ConfigCommand _ConfigCommands[] = {
	{ "admin", 		_conf_admin },
	{ "me", 		_conf_me },
	{ "oper", 		_conf_oper },
	{ "class", 		_conf_class },
	{ "drpass", 	_conf_drpass },
	{ "ulines", 	_conf_ulines },
	{ "include", 	_conf_include },
	{ "tld",		_conf_tld },
	{ "listen", 	_conf_listen },
	{ "allow",		_conf_allow },
	{ "except",		_conf_except },
	{ "vhost", 		_conf_vhost },
	{ "link", 		_conf_link },	
	{ "ban", 		_conf_ban },
	{ NULL, 		NULL  }
};

static OperFlag _OperFlags[] = {
	{ OFLAG_LOCAL,		"local" },
	{ OFLAG_GLOBAL,		"global" },
	{ OFLAG_REHASH,		"can_rehash" },
	{ OFLAG_EYES,		"eyes" },
	{ OFLAG_DIE,		"can_die" },
	{ OFLAG_RESTART,        "can_restart" },
	{ OFLAG_HELPOP,         "helpop" },
	{ OFLAG_GLOBOP,         "can_globops" },
	{ OFLAG_WALLOP,         "can_wallops" },
	{ OFLAG_RESTART,        "can_restart" },
	{ OFLAG_LOCOP,		"locop"},
	{ OFLAG_LROUTE,		"can_localroute" },
	{ OFLAG_GROUTE,		"can_globalroute" },
	{ OFLAG_LKILL,		"can_localkill" },
	{ OFLAG_GKILL,		"can_globalkill" },
	{ OFLAG_KLINE,		"can_kline" },
	{ OFLAG_UNKLINE,	"can_unkline" },
	{ OFLAG_LNOTICE,	"can_localnotice" },
	{ OFLAG_GNOTICE,	"can_globalnotice" },
	{ OFLAG_ADMIN,		"admin"},
	{ OFLAG_SADMIN,		"services-admin"},
	{ OFLAG_NETADMIN,	"netadmin"},
	{ OFLAG_TECHADMIN,	"techadmin"},
	{ OFLAG_COADMIN,	"coadmin"},
	{ OFLAG_UMODEC,		"get_umodec"},
	{ OFLAG_UMODEF,		"get_umodef"}, 
	{ OFLAG_ZLINE,		"can_zline"},
	{ OFLAG_WHOIS,		"get_umodew"},
	{ OFLAG_INVISIBLE,	"can_stealth"},
	{ 0L, 	NULL  }
};

static OperFlag _ListenerFlags[] = {
	{ LISTENER_NORMAL, 	"standard"},
	{ LISTENER_CLIENTSONLY, "clientsonly"},
	{ LISTENER_SERVERSONLY, "serversonly"},
	{ LISTENER_REMOTEADMIN, "remoteadmin"},
	{ LISTENER_JAVACLIENT, 	"java"},
	{ LISTENER_MASK, 	"mask"},
	{ LISTENER_SSL, 	"ssl"},
	{ 0L,			NULL },
};

static OperFlag _LinkFlags[] = {
	{ 0x0001,	"autoconnect" },
	{ 0x0002,	"ssl"		  },
	{ 0x0004,	"zip"		  }	
}; 

/*
 * Some prototypes
 */
void 			config_free(ConfigFile *cfptr);
ConfigFile 		*config_load(char *filename);
ConfigEntry 		*config_find(ConfigEntry *ceptr, char *name);
static void 		config_error(char *format, ...);
static ConfigFile 	*config_parse(char *filename, char *confdata);
static void 		config_entry_free(ConfigEntry *ceptr);
int			ConfigParse(ConfigFile *cfptr);

/*
 * Configuration linked lists
*/
ConfigItem_me		*conf_me = NULL;
ConfigItem_class 	*conf_class = NULL;
ConfigItem_admin 	*conf_admin = NULL;
ConfigItem_drpass	*conf_drpass = NULL;
ConfigItem_ulines	*conf_ulines = NULL;
ConfigItem_tld		*conf_tld = NULL;
ConfigItem_oper		*conf_oper = NULL;
ConfigItem_listen	*conf_listen = NULL;
ConfigItem_allow	*conf_allow = NULL;
ConfigItem_except	*conf_except = NULL;
ConfigItem_vhost	*conf_vhost = NULL;
ConfigItem_link		*conf_link = NULL;
ConfigItem_ban		*conf_ban = NULL;
/*
 * MyMalloc with the only difference that it clears the memory too
 * -Stskeeps
 * Should be moved to support.c
 */
void	*MyMallocEx(size_t size)
{
	void *p = MyMalloc(size);
	
	bzero(p, size);
	return (p);
}

/*
 * This will link in a ConfigItem into a list of it
 * Example:
 *    add_ConfigItem((ConfigItem *) class, (ConfigItem **) &conf_class);
 *
*/
void	add_ConfigItem(ConfigItem *item, ConfigItem **list)
{
	item->next = *list;
	item->prev = NULL;
	if (*list)
		(*list)->prev = item;
	*list = item;
}

/*
 * Removes a ConfigItem from a linked list
 * Example:
 *    del_ConfigItem((ConfigItem *) class, (ConfigItem **)&conf_class);
 * -Stskeeps
*/
ConfigItem *del_ConfigItem(ConfigItem *item, ConfigItem **list)
{
	ConfigItem *p, *q;
	
	for (p = *list; p; p = p->next)
	{
		if (p == item)
		{
			q = p->next;
			if (p->prev)
				p->prev->next = p->next;
			else
				*list = p->next;
				
			if (p->next)
				p->next->prev = p->prev;
			return q;		
		}
	}
	return NULL;
}

/* Small function to bitch about stuff */
static void config_error(char *format, ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	fprintf(stderr, "[error] %s\n", buffer);
}

/* Like above */
static void config_status(char *format, ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	fprintf(stderr, "* %s\n", buffer);
}
/* This is the internal parser, made by Chris Behrens & Fred Jacobs */
static ConfigFile *config_parse(char *filename, char *confdata)
{
	char		*ptr;
	char		*start;
	int			linenumber = 1;
	ConfigEntry	*curce;
	ConfigEntry	**lastce;
	ConfigEntry	*cursection;

	ConfigFile	*curcf;
	ConfigFile	*lastcf;

	lastcf = curcf = (ConfigFile *)malloc(sizeof(ConfigFile));
	memset(curcf, 0, sizeof(ConfigFile));
	curcf->cf_filename = strdup(filename);
	lastce = &(curcf->cf_entries);
	curce = NULL;
	cursection = NULL;
	for(ptr=confdata;*ptr;ptr++)
	{
		switch(*ptr)
		{
			case ';':
				if (!curce)
				{
					config_error("%s:%i Ignoring extra semicolon\n",
						filename, linenumber);
					break;
				}
/*
				if (!strcmp(curce->ce_varname, "include"))
				{
					ConfigFile	*cfptr;

					if (!curce->ce_vardata)
					{
						config_error("%s:%i Ignoring \"include\": No filename given\n",
							filename, linenumber);
						config_entry_free(curce);
						curce = NULL;
						continue;
					}
					if (strlen(curce->ce_vardata) > 255)
						curce->ce_vardata[255] = '\0';
					cfptr = config_load(curce->ce_vardata);
					if (cfptr)
					{
						lastcf->cf_next = cfptr;
						lastcf = cfptr;
					}
					config_entry_free(curce);
					curce = NULL;
					continue;
				}
*/
				*lastce = curce;
				lastce = &(curce->ce_next);
				curce->ce_fileposend = (ptr - confdata);
				curce = NULL;
				break;
			case '{':
				if (!curce)
				{
					config_error("%s:%i: No name for section start\n",
							filename, linenumber);
					continue;
				}
				else if (curce->ce_entries)
				{
					config_error("%s:%i: Ignoring extra section start\n",
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
					config_error("%s:%i: Ignoring extra close brace\n",
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
						if ((*ptr == '*') && (*(ptr+1) == '/'))
						{
							ptr++;
							break;
						}
						else if (*ptr == '\n')
							linenumber++;
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
				start = ++ptr;
				for(;*ptr;ptr++)
				{
					if ((*ptr == '\\') && (*(ptr+1) == '\"'))
					{
						char *tptr = ptr;
						while((*tptr = *(tptr+1)))
							tptr++;
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
						config_error("%s:%i: Ignoring extra data\n",
							filename, linenumber);
					}
					else
					{
						curce->ce_vardata = (char *)malloc(ptr-start+1);
						strncpy(curce->ce_vardata, start, ptr-start);
						curce->ce_vardata[ptr-start] = '\0';
						curce->ce_vardatanum = atoi(curce->ce_vardata);
					}
				}
				else
				{
					curce = (ConfigEntry *)malloc(sizeof(ConfigEntry));
					memset(curce, 0, sizeof(ConfigEntry));
					curce->ce_varname = (char *)malloc(ptr-start+1);
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
				if (*(ptr+1) == '#')
				{
					ptr += 2;
					while(*ptr && (*ptr != '\n'))
						ptr++;
					if (*ptr == '\n')
					{
						ptr--;
						continue;
					}
				}
				/* fall through */
			case '\t':
			case ' ':
			case '\r':
				break;
			default:
				if ((*ptr == '*') && (*(ptr+1) == '/'))
				{
					config_error("%s:%i Ignoring extra end comment\n",
						filename, linenumber);
					ptr++;
					break;
				}
				start = ptr;
				for(;*ptr;ptr++)
				{
					if ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n') || (*ptr == ';'))
						break;
				}
				if (!*ptr)
				{
					if (curce)
						config_error("%s: Unexpected EOF for variable starting at %i\n",
							filename, curce->ce_varlinenum);
					else if (cursection)
						config_error("%s: Unexpected EOF for section starting at %i\n",
							filename, curce->ce_sectlinenum);
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
						config_error("%s:%i: Ignoring extra data\n",
							filename, linenumber);
					}
					else
					{
						curce->ce_vardata = (char *)malloc(ptr-start+1);
						strncpy(curce->ce_vardata, start, ptr-start);
						curce->ce_vardata[ptr-start] = '\0';
						curce->ce_vardatanum = atoi(curce->ce_vardata);
					}
				}
				else
				{
					curce = (ConfigEntry *)malloc(sizeof(ConfigEntry));
					memset(curce, 0, sizeof(ConfigEntry));
					curce->ce_varname = (char *)malloc(ptr-start+1);
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

ConfigFile *config_load(char *filename)
{
	struct stat sb;
	int			fd;
	int			ret;
	char		*buf = NULL;
	ConfigFile	*cfptr;

	fd = open(filename, O_RDONLY);
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
		close(fd);
		return NULL;
	}
	buf = (char *)malloc(sb.st_size+1);
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
	buf[ret] = '\0';
	close(fd);
	cfptr = config_parse(filename, buf);
	free(buf);
	return cfptr;
}

ConfigEntry *config_find(ConfigEntry *ceptr, char *name)
{
	for(;ceptr;ceptr=ceptr->ce_next)
		if (!strcmp(ceptr->ce_varname, name))
			break;
	return ceptr;
}

/* This will load a config named <filename> -Stskeeps */
int	init_conf2(char *filename)
{
	ConfigFile	*cfptr;
	int		i = 0;
	if (!filename)
	{
		config_error("Could not load config file %s", filename);
		return 0;
	}
	config_status("Opening config file %s .. ", filename);
	if (cfptr = config_load(filename))
	{
		config_status("Config file %s loaded without problems",
			filename);
		i = ConfigParse(cfptr);
		config_free(cfptr);
		return i;
	}
	else
	{
		config_error("Could not load config file %s", filename);
		return 0;
	}
}

/* This is a function to make looking up config commands quick
   It goes in and checks the variable names and executes commands 
   that it is told to execute when encountering certain variable names
   -Stskeeps
*/
int	ConfigCmd(ConfigFile *cf, ConfigEntry *ce, ConfigCommand *cc)
{
	ConfigEntry *cep;
	ConfigCommand *ccp;
	if (!ce)
	{
		config_error("ConfigCmd: Got !ce");
		return -1;
	}
	if (!cc)
	{
		config_error("ConfigCmd: Got !cc");
		return -1;
	}
	for (cep = ce; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: (null) cep->ce_varname",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;	
		}
		for (ccp = cc; ccp->name; ccp++)
		{
			if (!strcasecmp(ccp->name, cep->ce_varname))
			{
				ccp->func(cf, cep);
			}
		}
	}
}

/* This simply starts the parsing of a config file from top level
   -Stskeeps
*/
int	ConfigParse(ConfigFile *cfptr)
{
	ConfigEntry	*ce = NULL;
		
	ConfigCmd(cfptr, cfptr->cf_entries, _ConfigCommands);
}

/* Here is the command parsing instructions */

/* include comment */
int	_conf_include(ConfigFile *conf, ConfigEntry *ce)
{
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: include: no filename given",
			ce->ce_fileptr->cf_filename, 
			ce->ce_varlinenum);
		return -1;
	}
	return (init_conf2(ce->ce_vardata));
}
/* 
 * The admin {} block parser
*/
int	_conf_admin(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_admin *ca;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: blank admin item",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum);
			continue;	
		}
		ca = MyMallocEx(sizeof(ConfigItem_admin));
		ircstrdup(ca->line, cep->ce_varname);
		add_ConfigItem((ConfigItem *)ca, (ConfigItem **) &conf_admin);
	} 
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
		if (!cep->ce_varname)
		{
			config_error("%s:%i: blank uline item",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum);
			continue;	
		}
		ca = MyMallocEx(sizeof(ConfigItem_ulines));
		ircstrdup(ca->servername, cep->ce_varname);
		add_ConfigItem((ConfigItem *)ca, (ConfigItem **) &conf_ulines);
	} 
}

/*
 * The class {} block parser
*/
int	_conf_class(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_class *class;
	unsigned char isnew = 0;
	
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: class without name",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	
	if (!(class = Find_class(ce->ce_vardata)))
	{
		class = (ConfigItem_class *) MyMallocEx(sizeof(ConfigItem_class));
		ircstrdup(class->name, ce->ce_vardata);
		isnew = 1;
	}
	else
	{
		isnew = 0;
		config_status("%s:%i: warning: redefining a record in class %s",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: class item without variable name",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;	
		}
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: class item without parameter",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;	
		}
		if (!strcmp(cep->ce_varname, "pingfreq"))
		{
			class->pingfreq = atol(cep->ce_vardata);
			if (!class->pingfreq)
			{
				config_error("%s:%i: class::pingfreq with illegal value",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			}
		} else
		if (!strcmp(cep->ce_varname, "maxclients"))
		{
			class->maxclients = atol(cep->ce_vardata);
			if (class->maxclients < 0)
			{
				config_error("%s:%i: class::maxclients with illegal value (<0))",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			} 
		} else
		if (!strcmp(cep->ce_varname, "sendq"))
		{
			class->sendq = atol(cep->ce_vardata);
			if (!class->sendq)
			{
				config_error("%s:%i: class::sendq with illegal value",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			}
		}
		else
		{
			config_error("%s:%i: unknown directive class::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_varname);
			continue;								
		}
	}
	if (isnew)
		add_ConfigItem((ConfigItem *) class, (ConfigItem **) &conf_class);
}

/*
 * The me {} block parser
*/
int	_conf_me(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;

	if (!conf_me)
	{
		conf_me = (ConfigItem_me *) MyMallocEx(sizeof(ConfigItem_me));
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: blank me line",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum);
			continue;	
		}
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: me::%s without parameter",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum,
				cep->ce_varname);
			continue;	
		}
		if (!strcmp(cep->ce_varname, "name"))
		{
			if (conf_me->name)
				MyFree(conf_me->name);
			ircstrdup(conf_me->name, cep->ce_vardata);
			if (!strchr(conf_me->name, '.'))
			{
				config_error("%s:%i: illegal me::name, missing .",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			}
		} else
		if (!strcmp(cep->ce_varname, "info"))
		{
			if (conf_me->info)
				MyFree(conf_me->info);
			conf_me->info = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "numeric"))
		{
			conf_me->numeric = atol(cep->ce_vardata);
			if ((conf_me->numeric < 0) && (conf_me->numeric > 254))
			{
				config_error("%s:%i: illegal me::numeric error (must be between 0 and 254). Setting to 0",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum);
				conf_me->numeric = 0;
			}
		}
		else
		{
			config_error("%s:%i: unknown directive me::%s",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, 
				cep->ce_varname);
		}
	}
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
	unsigned char	isnew = 0;
	
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: oper without name",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	
	if (!(oper = Find_oper(ce->ce_vardata)))
	{
		oper = (ConfigItem_oper *) MyMallocEx(sizeof(ConfigItem_oper));
		oper->name = strdup(ce->ce_vardata);
		isnew = 1;
	}
	else
	{
		isnew = 0;
		config_status("%s:%i: warning: redefining a record in oper %s",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);
	}

	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: oper item without variable name",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;
		}
		if (!cep->ce_entries)
		{
			/* standard variable */
			if (!cep->ce_vardata)
			{
				config_error("%s:%i: oper::%s without parameter",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum,
					cep->ce_varname);
				continue;	
			}
			if (!strcmp(cep->ce_varname, "class"))
			{
				oper->class = Find_class(cep->ce_vardata);
				if (!oper->class)
				{
					config_error("%s:%i: illegal oper::class, unknown class '%s'",
						cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum,
						cep->ce_vardata);
				}
			} else
			if (!strcmp(cep->ce_varname, "password"))
			{
				ircstrdup(oper->password, cep->ce_vardata);
				if (!(*oper->password))
				{
					config_error("%s:%i: illegal password, please write something",
						cep->ce_fileptr->cf_filename,
						cep->ce_varlinenum);
				}
			} else
			{
				config_error("%s:%i: unknown directive oper::%s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
						cep->ce_varname);
				continue;								
			}
		}
		else
		{
			/* Section */
			if (!strcmp(cep->ce_varname, "flags"))
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (!cepp->ce_varname)
					{
						config_error("%s:%i: oper::flags item without variable name",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						continue;
					}
					/* this should have been olp ;) -Stskeeps */
					for (ofp = _OperFlags; ofp->name; ofp++)
					{
						if (!strcmp(ofp->name, cepp->ce_varname))
						{
							if (!(oper->oflags & ofp->flag))
								oper->oflags |= ofp->flag;
							break;
						} 
					}
					if (!ofp->name)
					{
						config_error("%s:%i: unknown oper flag '%s'",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
							cepp->ce_varname);
						continue;								
					}			
				}	
				continue;
			}
			else
			if (!strcmp(cep->ce_varname, "from"))
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (!cepp->ce_varname)
					{
						config_error("%s:%i: oper::from item without variable name",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						continue;
					}
					if (!cepp->ce_vardata)
					{
						config_error("%s:%i: oper::from::%s without parameter",
							cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum,
							cepp->ce_varname);
						continue;
					}
					if (!strcmp(cepp->ce_varname, "userhost"))
					{
						from = (ConfigItem_oper_from *)MyMallocEx(sizeof(ConfigItem_oper_from));
						ircstrdup(from->name, cepp->ce_vardata);
						add_ConfigItem((ConfigItem *) from, (ConfigItem **)&oper->from);
					}
					else
					{
						config_error("%s:%i: unknown directive oper::from::%s",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
							cepp->ce_varname);
						continue;								
					}			
				}	
				continue;
			}
			else
			{
				config_error("%s:%i: unknown directive oper::%s (section)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
						cep->ce_varname);
				continue;								
			}
		}

	} 
	if (isnew)
		add_ConfigItem((ConfigItem *)oper, (ConfigItem **)&conf_oper);
}


int     _conf_drpass(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	
	if (!conf_drpass) {
		conf_drpass = (ConfigItem_drpass *) MyMallocEx(sizeof(ConfigItem_drpass));
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: drpass item without variable name", 
			 cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;
		}
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: missing parameter in drpass:%s", 
			 cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
			 	cep->ce_varname);
			continue;
		}
		if (!strcmp(cep->ce_varname, "restart"))
		{
			if (conf_drpass->restart)
				MyFree(conf_drpass->restart);
			conf_drpass->restart = strdup(cep->ce_vardata);
			ircstrdup(conf_drpass->restart, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "die"))
		{
			if (conf_drpass->die)
				MyFree(conf_drpass->die);
			conf_drpass->die = strdup(cep->ce_vardata);
		}
		else 
			config_error("%s:%i: warning: unknown drpass directive '%s'",
				 cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				 cep->ce_varname);
	}
}

int     _conf_tld(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_tld *ca;

	ca = MyMallocEx(sizeof(ConfigItem_tld));
        for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: blank tld item",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum);
			continue;
		}
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: missing parameter in tld::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				cep->ce_varname);
			continue;
		}
		
		if (!strcmp(cep->ce_varname, "mask")) {
			ca->mask = strdup(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "motd")) {
			ca->motd = strdup(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "rules")) {
			ca->rules = strdup(cep->ce_vardata);
		}
		else
		{
			config_error("%s:%i: unknown directive tld::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				cep->ce_varname); 
		}
	}
	add_ConfigItem((ConfigItem *)ca, (ConfigItem **) &conf_tld);
}

/*
 * listen {} block parser
*/
int	_conf_listen(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	ConfigItem_listen *listen = NULL;
	OperFlag    *ofp;
	char	    copy[256];
	char	    *ip;
	char	    *port;
	int	    iport;
	unsigned char	isnew = 0;
	
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: listen without ip:port",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	
	strcpy(copy, ce->ce_vardata);
	/* Seriously cheap hack to make listen <port> work -Stskeeps */
	if (!strcmp(copy, my_itoa(atoi(copy))))
	{
		ip = "*";
		port = copy;	
	}
	else
	{
		ip = strtok(copy, ":");
		if (!ip)
		{
			config_error("%s:%i: listen: illegal ip:port mask",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return -1;
		}
		if (strchr(ip, '*') && strcmp(ip, "*"))
		{
			config_error("%s:%i: listen: illegal ip, (mask, and not '*')",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return -1;
		
		}
		port = strtok(NULL, ":");
	}
	if (!port)
	{
		config_error("%s:%i: listen: missing port in mask",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	iport = atol(port);
	if ((iport < 0) || (iport > 65535))
	{
		config_error("%s:%i: listen: illegal port (must be 0..65536)",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return;
	} 
	if (!(listen = Find_listen(ip, iport)))
	{
		listen = (ConfigItem_listen *) MyMallocEx(sizeof(ConfigItem_listen));
		listen->ip = strdup(ip);
		listen->port = iport;
		isnew = 1;
	}
	else
	{
		isnew = 0;
		config_status("%s:%i: warning: redefining a record in listen %s",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);
	}

	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: listen item without variable name",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;
		}
		if (!cep->ce_vardata && !cep->ce_entries)
		{
			config_error("%s:%i: listen::%s without parameter",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum,
				cep->ce_varname);
			continue;	
		}
		if (!strcmp(cep->ce_varname, "options"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!cepp->ce_varname)
				{
					config_error("%s:%i: listen::options item without variable name",
						cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
					continue;
				}
				for (ofp = _ListenerFlags; ofp->name; ofp++)
				{
					if (!strcmp(ofp->name, cepp->ce_varname))
					{
						if (!(listen->options & ofp->flag))
							listen->options |= ofp->flag;
						break;
					} 
				}
				if (!ofp->name)
				{
					config_error("%s:%i: unknown listen option '%s'",
						cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
						cepp->ce_varname);
					continue;
				}
			}
		}
		else
		{
			config_error("%s:%i: unknown directive listen::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_varname);
			continue;								
		}

	} 
	if (isnew)
		add_ConfigItem((ConfigItem *)listen, (ConfigItem **)&conf_listen);
}

/*
 * allow {} block parser
*/
int	_conf_allow(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_allow *allow;
	unsigned char isnew = 0;
	
	allow = (ConfigItem_allow *) MyMallocEx(sizeof(ConfigItem_allow));
	isnew = 1;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: allow item without variable name",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;	
		}
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: allow item without parameter",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;	
		}
		if (!strcmp(cep->ce_varname, "ip"))
		{
			allow->ip = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "user"))
		{
			allow->user = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "hostname"))
		{
			allow->hostname = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "password"))
		{
			allow->password = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "class"))
		{
			allow->class = Find_class(cep->ce_vardata);
			if (!allow->class)
			{
				config_error("%s:%i: illegal allow::class, unknown class '%s'",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum,
					cep->ce_vardata);
			}
		}
		else
		{
			config_error("%s:%i: unknown directive allow::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_varname);
			continue;								
		}
	}
	if (isnew)
		add_ConfigItem((ConfigItem *) allow, (ConfigItem **) &conf_allow);
}

/*
 * vhost {} block parser
*/
int	_conf_vhost(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_vhost *vhost;
	unsigned char isnew = 0;
	
	vhost = (ConfigItem_vhost *) MyMallocEx(sizeof(ConfigItem_vhost));
	isnew = 1;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: vhost item without variable name",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;	
		}
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: vhost item without parameter",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;	
		}
		if (!strcmp(cep->ce_varname, "vhost"))
		{
			vhost->virthost = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "userhost"))
		{
			vhost->userhost = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "login"))
		{
			vhost->login = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "password"))
		{
			vhost->password = strdup(cep->ce_vardata);
		}
		else
		{
			config_error("%s:%i: unknown directive vhost::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_varname);
			continue;								
		}
	}
	if (isnew)
		add_ConfigItem((ConfigItem *) vhost, (ConfigItem **) &conf_vhost);
}

int     _conf_except(ConfigFile *conf, ConfigEntry *ce)
{

	ConfigEntry *cep;
	ConfigItem_except *ca;
	unsigned char isnew = 0;

	ca = (ConfigItem_except *) MyMallocEx(sizeof(ConfigItem_except));
	isnew = 1;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: except without type",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}

	if (!strcmp(ce->ce_vardata, "ban")) {
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "mask")) {
				ca->mask = strdup(cep->ce_vardata);
			}
			else {
				config_error("%s:%i: unknown directive except::ban::%s",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					cep->ce_varname);
			}
		}
		ca->flag.type = 1;
		add_ConfigItem((ConfigItem *)ca, (ConfigItem **) &conf_except);				
	}
	else if (!strcmp(ce->ce_vardata, "socks")) {
		config_status("Got except socks");
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "mask")) {
				ca->mask = strdup(cep->ce_vardata);
			}
			else {
			config_error("%s:%i: unknown directive except::socks::%s",		
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				cep->ce_varname);
			}
		}
		ca->flag.type = 0;
		add_ConfigItem((ConfigItem *)ca, (ConfigItem **) &conf_except);

	}
	else {
		config_error("%s:%i: unknown type except::%s",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);	
	}
}		

int     _conf_ban(ConfigFile *conf, ConfigEntry *ce)
{

	ConfigEntry *cep;
	ConfigItem_ban *ca;
	unsigned char isnew = 0;

	ca = (ConfigItem_ban *) MyMallocEx(sizeof(ConfigItem_ban));
	isnew = 1;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: ban without type",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	if (!strcmp(ce->ce_vardata, "nick"))
		ca->flag.type = CONF_BAN_NICK;
	else if (!strcmp(ce->ce_vardata, "ip"))
		ca->flag.type = CONF_BAN_IP;
	else if (!strcmp(ce->ce_vardata, "server"))
		ca->flag.type = CONF_BAN_SERVER;
	else if (!strcmp(ce->ce_vardata, "user"))
		ca->flag.type = CONF_BAN_USER;
	else if (!strcmp(ce->ce_vardata, "realname"))
		ca->flag.type = CONF_BAN_REALNAME;
	else
	{
		MyFree(ca);
		config_error("%s:%i: unknown ban type %s",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);
		return -1;
	}
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: ban %s::%s without parameter",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, ce->ce_vardata, cep->ce_varname);
			continue;
		}
		if (!strcmp(cep->ce_varname, "mask")) {
			ca->mask = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "reason")) {
			ca->reason = strdup(cep->ce_vardata);
		} 
		else {
				config_error("%s:%i: unknown directive ban %s::%s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					ce->ce_vardata, cep->ce_varname);
		}
	}
	add_ConfigItem((ConfigItem *)ca, (ConfigItem **) &conf_ban);				
}		

int	_conf_link(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	ConfigItem_link *link = NULL;
	OperFlag    *ofp;
	unsigned char	isnew = 0;
	
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: link without servername",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
		
	if (!strchr(ce->ce_vardata, '.'))
	{
		config_error("%s:%i: link: bogus server name",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
		
	link = (ConfigItem_link *) MyMallocEx(sizeof(ConfigItem_link));
	link->servername = strdup(ce->ce_vardata);
	isnew = 1;

	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: link item without variable name",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			continue;
		}
		if (!cep->ce_vardata && !cep->ce_entries)
		{
			config_error("%s:%i: link::%s without parameter",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum,
				cep->ce_varname);
			continue;	
		}
		if (!strcmp(cep->ce_varname, "options"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!cepp->ce_varname)
				{
					config_error("%s:%i: link::flag item without variable name",
						cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
					continue;
				}
				for (ofp = _LinkFlags; ofp->name; ofp++)
				{
					if (!strcmp(ofp->name, cepp->ce_varname))
					{
						if (!(link->options & ofp->flag))
							link->options |= ofp->flag;
						break;
					} 
				}
				if (!ofp->name)
				{
					config_error("%s:%i: unknown link option '%s'",
						cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
						cepp->ce_varname);
					continue;
				}
			}
		} else
		if (!strcmp(cep->ce_varname, "username"))
		{
			link->username = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "hostname"))
		{
			link->hostname = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "bind-ip"))
		{
			link->bindip = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "port"))
		{
			link->port = atol(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "hub"))
		{
			link->hubmask = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "leaf"))
		{
			link->leafmask = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "leafdepth"))
		{
			link->leafdepth = atol(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "password-connect"))
		{
			link->connpwd = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "password-recieve"))
		{
			link->recvpwd = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "class"))
		{
			link->class = Find_class(cep->ce_vardata);
			if (!link->class)
			{
				config_error("%s:%i: illegal link::class, unknown class '%s'",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum,
					cep->ce_vardata);
			}
		} else
		{
			config_error("%s:%i: unknown directive link::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_varname);
			continue;								
		}

	} 
	if (isnew)
		add_ConfigItem((ConfigItem *)link, (ConfigItem **)&conf_link);
}


/*
 * Report functions
*/

void	report_configuration(void)
{
	ConfigItem_admin	*admin_ptr;
	ConfigItem_oper 	*oper_ptr;
	ConfigItem_oper_from	*from_ptr;
	ConfigItem_class	*class_ptr;
	ConfigItem_ulines	*uline_ptr;
	ConfigItem_tld		*tld_ptr;
	ConfigItem_listen	*listen_ptr;
	ConfigItem_allow	*allow_ptr;
	ConfigItem_except	*except_ptr;
	OperFlag		*ofp;
	
	printf("Report:\n");
	printf("-------\n");
	if (conf_me)
	{
		printf("My name is %s and i describe myself as \"%s\", my numeric is %i\n",
			conf_me->name, conf_me->info, conf_me->numeric);
		
	}
	if (conf_admin)
	{
		printf("My pathetic admin is:\n");
		for (admin_ptr = conf_admin; admin_ptr; admin_ptr = (ConfigItem_admin *) admin_ptr->next)
			printf("        %s\n", admin_ptr->line);
	}
	if (conf_oper)
	{
		printf("My even more pathetic opers are:\n");
		for (oper_ptr = conf_oper; oper_ptr; oper_ptr = (ConfigItem_oper *)oper_ptr->next)
		{
			printf("      %s (%s) :\n", oper_ptr->name, oflagstr(oper_ptr->oflags));
			printf("        - Password: %s\n", oper_ptr->password);
			printf("        - Class: %s\n", oper_ptr->class->name);
			if (oper_ptr->from)
			{
				printf("        - He can come from (the grave):\n");
				for (from_ptr = (ConfigItem_oper_from *) oper_ptr->from; from_ptr; from_ptr = (ConfigItem_oper_from *) from_ptr->next)
				printf("          * %s\n", from_ptr->name);		
			}
		}
	}
	if (conf_class)
	{
		printf("I got some nice classes that i like to put servers or people in:\n");
		for (class_ptr = conf_class; class_ptr; class_ptr = (ConfigItem_class *)class_ptr->next)
		{	
			printf("       class %s:\n", class_ptr->name);
			printf("         * pingfreq: %i\n", class_ptr->pingfreq);
			printf("         * maxclients: %i\n", class_ptr->maxclients);
			printf("         * sendq: %i\n", class_ptr->sendq);
		}
	}
	if (conf_drpass)
	{
		printf("I also got a Die/Restart password pair\n");
		printf("         * restart: %s\n", conf_drpass->restart);
		printf("         *     die: %s\n", conf_drpass->die);
	}
	if (conf_ulines)
	{
		printf("Got some Ulines configured too:\n");
		for (uline_ptr = conf_ulines; uline_ptr; uline_ptr = (ConfigItem_ulines *) uline_ptr->next)
		{
			printf("       * %s\n", uline_ptr->servername);	
		}
	}
	if (conf_tld)
	{
		printf("Got some TLDs:\n");
		for (tld_ptr = conf_tld; tld_ptr; tld_ptr = (ConfigItem_tld *) tld_ptr->next)
			printf("       * %s (motd=%s) (rules=%s)\n",
					tld_ptr->mask,
					(tld_ptr->motd ? tld_ptr->motd : "no motd"),		
					(tld_ptr->rules ? tld_ptr->rules : "no rules"));
	}
	if (conf_listen)
	{
		for (listen_ptr = conf_listen; listen_ptr; listen_ptr = (ConfigItem_listen *) listen_ptr->next)
		{
			printf("I listen on %s:%i\n", listen_ptr->ip, listen_ptr->port);
			for (ofp = _ListenerFlags; ofp->name; ofp++)
				if (listen_ptr->options & ofp->flag)
					printf("  * option: %s\n", ofp->name);
		}		
	}
	if (conf_allow)
	{
		for (allow_ptr = conf_allow; allow_ptr; allow_ptr = (ConfigItem_allow *) allow_ptr->next)
		{
			printf("I allow for user %s IP %s and hostname %s to enter.\n",
				allow_ptr->user,
				allow_ptr->ip,
				allow_ptr->hostname);
				
			printf("      * class: %s\n      * password: %s\n",
				(allow_ptr->class ? allow_ptr->class->name : "NO CLASS (BAD)"),
				(allow_ptr->password ? allow_ptr->password : "(no password)"));
		}		
	}
	if (conf_except) {
		for (except_ptr = conf_except; except_ptr; except_ptr = (ConfigItem_except *) except_ptr->next)
		{
			printf("Got an except for %s (%s)\n", except_ptr->mask, except_ptr->flag.type ? "ban" : "socks");
		}
	}
}	


/*
 * Lookup functions
 * -Stskeeps
*/

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
