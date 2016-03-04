/*
 * Nopost UnrealIRCd module, (C) Copyright 2004-2010 Bram Matthys (Syzop)
 *
 * This module will kill any clients issuing POST's during pre-connect stage.
 * It protects against the 'Firefox XPS IRC Attack' (and variations thereof).
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

CMD_FUNC(m_nopost);

ModuleHeader MOD_HEADER(m_nopost)
  = {
	"m_nopost",
	"v1.1",
	"Ban GET/POST/PUT commands", 
	"3.2-b8-1",
	NULL 
    };

typedef struct _dynlist DynList;
struct _dynlist {
	DynList *prev, *next;
	char *entry;
};

struct {
	int ban_action;
	char *ban_reason;
	long ban_time;
	DynList *except_hosts;
} cfg;

static void free_config(void);
static void init_config(void);
DLLFUNC int m_nopost_config_test(ConfigFile *, ConfigEntry *, int, int *);
DLLFUNC int m_nopost_config_run(ConfigFile *, ConfigEntry *, int);
static int is_except_host(aClient *sptr);

MOD_TEST(m_nopost)
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, m_nopost_config_test);
	return MOD_SUCCESS;
}

MOD_INIT(m_nopost)
{
	CommandAdd(modinfo->handle, "GET", m_nopost, MAXPARA, M_UNREGISTERED);
	CommandAdd(modinfo->handle, "POST", m_nopost, MAXPARA, M_UNREGISTERED);
	CommandAdd(modinfo->handle, "PUT", m_nopost, MAXPARA, M_UNREGISTERED);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, m_nopost_config_run);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	init_config();
	return MOD_SUCCESS;
}

MOD_LOAD(m_nopost)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_nopost)
{
	free_config();
	return MOD_SUCCESS;
}

static void init_config(void)
{
	memset(&cfg, 0, sizeof(cfg));
	/* Default values */
	cfg.ban_reason = strdup("HTTP command from IRC connection (ATTACK?)");
	cfg.ban_action = BAN_ACT_KILL;
	cfg.ban_time = 60 * 60 * 4;
}
static void free_config(void)
{
DynList *d, *d_next;

	if (cfg.ban_reason)
		MyFree(cfg.ban_reason);
	for (d=cfg.except_hosts; d; d=d_next)
	{
		d_next = d->next;
		MyFree(d->entry);
		MyFree(d);
	}

	memset(&cfg, 0, sizeof(cfg)); /* needed! */
}

DLLFUNC int m_nopost_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
int errors = 0;
ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::nopost... */
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "nopost"))
		return 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: blank set::nopost item",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
		} else
		if (!strcmp(cep->ce_varname, "except-hosts"))
		{
		} else
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: set::nopost::%s with no value",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
		} else
		if (!strcmp(cep->ce_varname, "ban-action"))
		{
			if (!banact_stringtoval(cep->ce_vardata))
			{
				config_error("%s:%i: set::nopost::ban-action: unknown action '%s'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				errors++;
			}
		} else
		if (!strcmp(cep->ce_varname, "ban-reason"))
		{
		} else
		if (!strcmp(cep->ce_varname, "ban-time"))
		{
		} else
		{
			config_error("%s:%i: unknown directive set::nopost::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
		}
	}
	*errs = errors;
	return errors ? -1 : 1;
}

DLLFUNC int m_nopost_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
ConfigEntry *cep, *cep2;
DynList *d;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::nopost... */
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "nopost"))
		return 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "except-hosts"))
		{
			for (cep2 = cep->ce_entries; cep2; cep2 = cep2->ce_next)
			{
				d = MyMallocEx(sizeof(DynList));
				d->entry = strdup(cep2->ce_varname);
				AddListItem(d, cfg.except_hosts);
			}
		} else
		if (!strcmp(cep->ce_varname, "ban-action"))
		{
			cfg.ban_action = banact_stringtoval(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "ban-reason"))
		{
			if (cfg.ban_reason)
				MyFree(cfg.ban_reason);
			cfg.ban_reason = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "ban-time"))
		{
			cfg.ban_time = config_checkval(cep->ce_vardata, CFG_TIME);
		}
	}
	return 1;
}

/** Finds out if the host is on the except list. 1 if yes, 0 if no */
static int is_except_host(aClient *sptr)
{
char *host, *ip;
DynList *d;

	if (!cfg.except_hosts)
		return 0; /* quick return */

	host = sptr->user ? sptr->user->realhost : "???";
	ip = GetIP(sptr) ? GetIP(sptr) : "???";
	
	for (d=cfg.except_hosts; d; d=d->next)
		if (!match(d->entry, host) || !match(d->entry, ip))
			return 1;

	return 0;
}


CMD_FUNC(m_nopost)
{
	if (MyConnect(sptr) && !is_except_host(sptr))
	{
		/* We send a message to the ircops if the action is KILL, because otherwise
		 * you won't even notice it. This is not necessary for *LINE/SHUN/etc as
		 * ircops see them being added.
		 */
		if (cfg.ban_action == BAN_ACT_KILL)
		{
			sendto_realops("[m_nopost] Killed connection from %s", GetIP(sptr));
			ircd_log(LOG_CLIENT, "[m_nopost] Killed connection from %s", GetIP(sptr));
		}
		return place_host_ban(sptr, cfg.ban_action, cfg.ban_reason, cfg.ban_time);
	}
	return 0;
}
