/*
 * WebIRC / CGI:IRC Support
 * (C) Copyright 2006-.. Bram Matthys (Syzop) and the UnrealIRCd team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "unrealircd.h"

/* Types */
typedef struct _configitem_webirc ConfigItem_webirc;

typedef enum {
	WEBIRC_PASS=1, WEBIRC_WEBIRC=2
} WEBIRCType;

struct _configitem_webirc {
	ConfigItem_webirc *prev, *next;
	ConfigFlag flag;
	ConfigItem_mask *mask;
	WEBIRCType type;
	anAuthStruct *auth;
};

/* Module header */
ModuleHeader MOD_HEADER(webirc)
= {
	"webirc",
	"4.0",
	"WebIRC/CGI:IRC Support",
	"3.2-b8-1",
	NULL 
};

/* Global variables */
ModDataInfo *webirc_md = NULL;
ConfigItem_webirc *conf_webirc = NULL;

/* Forward declarations */
CMD_FUNC(m_webirc);
int webirc_check_init(aClient *cptr, char *sockn, size_t size);
int webirc_local_pass(aClient *sptr, char *password);
int webirc_config_test(ConfigFile *, ConfigEntry *, int, int *);
int webirc_config_run(ConfigFile *, ConfigEntry *, int);
void webirc_free_conf(void);
void delete_webircblock(ConfigItem_webirc *e);
void webirc_md_free(ModData *md);

#define IsWEBIRC(x)     (moddata_client(x, webirc_md).l)
#define SetWEBIRC(x)	do { moddata_client(x, webirc_md).l = 1; } while(0)
#define ClearWEBIRC(x)	do { moddata_client(x, webirc_md).l = 0; } while(0)

#define MSG_WEBIRC "WEBIRC"

MOD_TEST(webirc)
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, webirc_config_test);
	return MOD_SUCCESS;
}

/** Called upon module init */
MOD_INIT(webirc)
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "webirc";
	mreq.type = MODDATATYPE_CLIENT;
	mreq.free = webirc_md_free;
	webirc_md = ModDataAdd(modinfo->handle, mreq);
	if (!webirc_md)
	{
		config_error("could not register webirc moddata");
		return MOD_FAILED;
	}

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, webirc_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_CHECK_INIT, 0, webirc_check_init);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_PASS, 0, webirc_local_pass);

	CommandAdd(modinfo->handle, MSG_WEBIRC, m_webirc, MAXPARA, M_UNREGISTERED);
		
	return MOD_SUCCESS;
}

/** Called upon module load */
MOD_LOAD(webirc)
{
	return MOD_SUCCESS;
}

/** Called upon unload */
MOD_UNLOAD(webirc)
{
	webirc_free_conf();
	return MOD_SUCCESS;
}

void webirc_free_conf(void)
{
	ConfigItem_webirc *webirc_ptr, *next;

	for (webirc_ptr = conf_webirc; webirc_ptr; webirc_ptr = next)
	{
		next = webirc_ptr->next;
		delete_webircblock(webirc_ptr);
	}
	conf_webirc = NULL;
}

void delete_webircblock(ConfigItem_webirc *e)
{
	unreal_delete_masks(e->mask);
	if (e->auth)
		Auth_DeleteAuthStruct(e->auth);
	DelListItem(e, conf_webirc);
	MyFree(e);
}

int webirc_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry	*cep, *cepp;
	int errors = 0;
	char has_mask = 0; /* mandatory */
	char has_password = 0; /* mandatory */
	char has_type = 0; /* optional (used for dup checking) */
	WEBIRCType webirc_type = WEBIRC_WEBIRC; /* the default */

	if (type != CONFIG_MAIN)
		return 0;
	
	if (!ce || !ce->ce_varname)
		return 0;
	
	if (!strcmp(ce->ce_varname, "cgiirc"))
	{
		config_error("%s:%i: the cgiirc block has been renamed to webirc and "
		             "the syntax has changed in UnrealIRCd 4",
		             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		need_34_upgrade = 1;
		*errs = 1;
		return -1;
	}

	if (strcmp(ce->ce_varname, "webirc"))
		return 0; /* not interested in non-webirc stuff.. */

	/* Now actually go parse the webirc { } block */
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error_blank(cep->ce_fileptr->cf_filename, cep->ce_varlinenum, "webirc");
			errors++;
			continue;
		}
		if (!cep->ce_vardata)
		{
			config_error_empty(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"webirc", cep->ce_varname);
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "mask"))
		{
			if (cep->ce_vardata || cep->ce_entries)
				has_mask = 1;
		}
		else if (!strcmp(cep->ce_varname, "password"))
		{
			if (has_password)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "webirc::password");
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
					cep->ce_varlinenum, "webirc::type");
			}
			has_type = 1;
			if (!strcmp(cep->ce_vardata, "webirc"))
				webirc_type = WEBIRC_WEBIRC;
			else if (!strcmp(cep->ce_vardata, "old"))
				webirc_type = WEBIRC_PASS;
			else
			{
				config_error("%s:%i: unknown webirc::type '%s', should be either 'webirc' or 'old'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				errors++;
			}
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"webirc", cep->ce_varname);
			errors++;
		}
	}
	if (!has_mask)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"webirc::mask");
		errors++;
	}

	if (!has_password && (webirc_type == WEBIRC_WEBIRC))
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"webirc::password");
		errors++;
	}
	
	if (has_password && (webirc_type == WEBIRC_PASS))
	{
		config_error("%s:%i: webirc block has type set to 'old' but has a password set. "
		             "Passwords are not used with type 'old'. Either remove the password or "
		             "use the 'webirc' method instead.",
		             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int webirc_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	ConfigItem_webirc *webirc = NULL;
	
	if (type != CONFIG_MAIN)
		return 0;
	
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "webirc"))
		return 0; /* not interested */

	webirc = (ConfigItem_webirc *) MyMallocEx(sizeof(ConfigItem_webirc));
	webirc->type = WEBIRC_WEBIRC; /* default */

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "mask"))
			unreal_add_masks(&webirc->mask, cep);
		else if (!strcmp(cep->ce_varname, "password"))
			webirc->auth = Auth_ConvertConf2AuthStruct(cep);
		else if (!strcmp(cep->ce_varname, "type"))
		{
			if (!strcmp(cep->ce_vardata, "webirc"))
				webirc->type = WEBIRC_WEBIRC;
			else if (!strcmp(cep->ce_vardata, "old"))
				webirc->type = WEBIRC_PASS;
			else
				abort();
		}
	}
	
	AddListItem(webirc, conf_webirc);
	
	return 0;
}

void webirc_md_free(ModData *md)
{
	/* we have nothing to free actually, but we must set to zero */
	md->l = 0;
}

ConfigItem_webirc *Find_webirc(aClient *sptr, WEBIRCType type)
{
	ConfigItem_webirc *e;

	for (e = conf_webirc; e; e = e->next)
	{
		if ((e->type == type) && unreal_mask_match(sptr, e->mask))
			return e;
	}

	return NULL;
}

#define WEBIRC_STRING     "WEBIRC_"
#define WEBIRC_STRINGLEN  (sizeof(WEBIRC_STRING)-1)

/* Does the CGI:IRC host spoofing work */
int dowebirc(aClient *cptr, char *ip, char *host)
{
	char scratch[64];
	char *sockhost;

	if (IsWEBIRC(cptr))
		return exit_client(cptr, cptr, &me, "Double CGI:IRC request (already identified)");

	if (host && !strcmp(ip, host))
		host = NULL; /* host did not resolve, make it NULL */

	/* STEP 1: Update cptr->local->ip
	   inet_pton() returns 1 on success, 0 on bad input, -1 on bad AF */
	if ((inet_pton(AF_INET, ip, scratch) != 1) &&
	    (inet_pton(AF_INET6, ip, scratch) != 1))
	{
		/* then we have an invalid IP */
		return exit_client(cptr, cptr, &me, "Invalid IP address");
	}

	/* STEP 2: Update GetIP() */
	safefree(cptr->ip);
	cptr->ip = strdup(ip);
		
	/* STEP 3: Update cptr->local->hostp */
	/* (free old) */
	if (cptr->local->hostp)
	{
		unreal_free_hostent(cptr->local->hostp);
		cptr->local->hostp = NULL;
	}
	/* (create new) */
	if (host && verify_hostname(host))
		cptr->local->hostp = unreal_create_hostent(host, cptr->ip);

	/* STEP 4: Update sockhost
	   Make sure that if this any IPv4 address is _not_ prefixed with
	   "::ffff:" by using Inet_ia2p().
	 */
	// Hmm I ignored above warning. May be bad during transition period.
	strlcpy(cptr->local->sockhost, cptr->ip, sizeof(cptr->local->sockhost));

	SetWEBIRC(cptr);

	/* Check (g)zlines right now; these are normally checked upon accept(),
	 * but since we know the IP only now after PASS/WEBIRC, we have to check
	 * here again...
	 */
	return check_banned(cptr);
}

/* WEBIRC <pass> "cgiirc" <hostname> <ip> */
CMD_FUNC(m_webirc)
{
	char *ip, *host, *password;
	size_t ourlen;
	ConfigItem_webirc *e;

	if ((parc < 5) || BadPtr(parv[4]))
	{
		sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS), me.name, "*", "WEBIRC");
		return -1;
	}

	password = parv[1];
	host = !DONT_RESOLVE ? parv[3] : parv[4];
	ip = parv[4];

	/* Check if allowed host */
	e = Find_webirc(sptr, WEBIRC_WEBIRC);
	if (!e)
		return exit_client(cptr, sptr, &me, "CGI:IRC -- No access");

	/* Check password */
	if (Auth_Check(sptr, e->auth, password) == -1)
		return exit_client(cptr, sptr, &me, "CGI:IRC -- Invalid password");

	/* And do our job.. */
	return dowebirc(cptr, ip, host);
}


int webirc_check_init(aClient *cptr, char *sockn, size_t size)
{
	if (IsWEBIRC(cptr))
	{
		strlcpy(sockn, GetIP(cptr), size); /* use already set value */
		return 0;
	}
	
	return 1; /* nothing to do */
}

int webirc_local_pass(aClient *sptr, char *password)
{
	if (!strncmp(password, WEBIRC_STRING, WEBIRC_STRINGLEN))
	{
		char *ip, *host;
		ConfigItem_webirc *e;

		e = Find_webirc(sptr, WEBIRC_PASS);
		if (e)
		{
			/* Ok now we got that sorted out, proceed:
			 * Syntax: WEBIRC_<ip>_<resolvedhostname>
			 * The <resolvedhostname> has been checked ip->host AND host->ip by CGI:IRC itself
			 * already so we trust it.
			 */
			ip = password + WEBIRC_STRINGLEN;
			host = strchr(ip, '_');
			if (!host)
				return exit_client(sptr, sptr, &me, "Invalid CGI:IRC IP received");
			*host++ = '\0';
		
			return dowebirc(sptr, ip, host);
		}
		/* falltrough if not in webirc block.. */
	}

	return 0; /* not webirc */
}
