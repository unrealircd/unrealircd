/*
 * Restrict specific commands until people have been connected for a certain amount of time
 * (C) Copyright 2019 Gottem and the UnrealIRCd team
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

#define GetReputation(acptr) (moddata_client_get(acptr, "reputation") ? atoi(moddata_client_get(acptr, "reputation")) : 0)

struct cfgstruct {
	int command_count;
};

typedef struct restrictedcmd RestrictedCmd;
struct restrictedcmd {
	RestrictedCmd *prev, *next;
	char *cmd;
	char *conftag;
	long connect_delay;
	int exempt_identified;
	int exempt_reputation_score;
	int disable;
};

typedef struct {
	char *conftag;
	char *cmd;
} CmdMap;

// Forward declarations
char *find_cmd_by_conftag(char *conftag);
RestrictedCmd *find_restrictions_bycmd(char *cmd);
RestrictedCmd *find_restrictions_byconftag(char *conftag);
int cmdrestrict_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int cmdrestrict_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
char *cmdrestrict_hook_prechanmsg(aClient *sptr, aChannel *chptr, MessageTag *mtags, char *text, int notice);
char *cmdrestrict_hook_preusermsg(aClient *sptr, aClient *to, char *text, int notice);
char *cmdrestrict_hook_wrapper(aClient *sptr, char *text, int notice, char *display, char *conftag);
CMD_OVERRIDE_FUNC(cmdrestrict_override);

// Globals
static ModuleInfo ModInf;
static struct cfgstruct cfg;
RestrictedCmd *RestrictedCmdList = NULL;
CmdMap conf_cmdmaps[] = {
	{ "invite", "INVITE" },
	{ "knock", "KNOCK" },
	{ "list", "LIST" },
	{ "channel-message", "PRIVMSG" },
	{ "channel-notice", "NOTICE" },
	{ "private-message", "PRIVMSG" },
	{ "private-notice", "NOTICE" },
	{ NULL, NULL, }, // REQUIRED for the loop to properly work
};

ModuleHeader MOD_HEADER(cmdrestrict) = {
	"cmdrestrict",
	"v1.0",
	"Restrict specific commands until certain conditions have been met",
	"3.2-b8-1",
	NULL
};

MOD_TEST(cmdrestrict)
{
	memcpy(&ModInf, modinfo, modinfo->size);
	memset(&cfg, 0, sizeof(cfg));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, cmdrestrict_configtest);
	return MOD_SUCCESS;
}

MOD_INIT(cmdrestrict)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, cmdrestrict_configrun);

	// Due to the nature of PRIVMSG/NOTICE we're gonna need to hook into PRE_* stuff instead of using command overrides
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_CHANMSG, -1000000, cmdrestrict_hook_prechanmsg);
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_USERMSG, -1000000, cmdrestrict_hook_preusermsg);
	return MOD_SUCCESS;
}

MOD_LOAD(cmdrestrict)
{
	if (ModuleGetError(modinfo->handle) != MODERR_NOERROR)
	{
		config_error("A critical error occurred when loading module %s: %s", MOD_HEADER(cmdrestrict).name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD(cmdrestrict)
{
	RestrictedCmd *rcmd, *next;
	for (rcmd = RestrictedCmdList; rcmd; rcmd = next)
	{
		next = rcmd->next;
		MyFree(rcmd->conftag);
		MyFree(rcmd->cmd);
		DelListItem(rcmd, RestrictedCmdList);
		MyFree(rcmd);
	}
	RestrictedCmdList = NULL;
	return MOD_SUCCESS;
}

char *find_cmd_by_conftag(char *conftag) {
	CmdMap *cmap;
	for (cmap = conf_cmdmaps; cmap->conftag; cmap++)
	{
		if (!strcmp(cmap->conftag, conftag))
			return cmap->cmd;
	}
	return NULL;
}

RestrictedCmd *find_restrictions_bycmd(char *cmd) {
	RestrictedCmd *rcmd;
	for (rcmd = RestrictedCmdList; rcmd; rcmd = rcmd->next)
	{
		if (!strcmp(rcmd->cmd, cmd))
			return rcmd;
	}
	return NULL;
}

RestrictedCmd *find_restrictions_byconftag(char *conftag) {
	RestrictedCmd *rcmd;
	for (rcmd = RestrictedCmdList; rcmd; rcmd = rcmd->next)
	{
		if (!strcmp(rcmd->conftag, conftag))
			return rcmd;
	}
	return NULL;
}

int cmdrestrict_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cep2;
	RestrictedCmd *rcmd;
	long connect_delay;
	int exempt_reputation_score;
	int has_restriction;

	// We are only interested in set::restrict-commands
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->ce_varname, "restrict-commands"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!find_cmd_by_conftag(cep->ce_varname))
		{
			config_error("%s:%i: unsupported command %s for set::restrict-commands", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		}

		has_restriction = 0;
		for (cep2 = cep->ce_entries; cep2; cep2 = cep2->ce_next)
		{
			if (!cep2->ce_vardata)
			{
				config_error("%s:%i: blank set::restrict-commands::%s:%s without value", cep2->ce_fileptr->cf_filename, cep2->ce_varlinenum, cep->ce_varname, cep2->ce_varname);
				errors++;
				continue;
			}

			if (!strcmp(cep2->ce_varname, "connect-delay"))
			{
				has_restriction = 1;
				connect_delay = config_checkval(cep2->ce_vardata, CFG_TIME);
				if (connect_delay < 10 || connect_delay > 3600)
				{
					config_error("%s:%i: set::restrict-commands::%s::connect-delay should be in range 10-3600", cep2->ce_fileptr->cf_filename, cep2->ce_varlinenum, cep->ce_varname);
					errors++;
				}
				continue;
			}

			if (!strcmp(cep2->ce_varname, "exempt-identified"))
				continue;

			if (!strcmp(cep2->ce_varname, "exempt-reputation-score"))
			{
				exempt_reputation_score = atoi(cep2->ce_vardata);
				if (exempt_reputation_score <= 0)
				{
					config_error("%s:%i: set::restrict-commands::%s::exempt-reputation-score must be greater than 0", cep2->ce_fileptr->cf_filename, cep2->ce_varlinenum, cep->ce_varname);
					errors++;
				}
				continue;
			}

			if (!strcmp(cep2->ce_varname, "disable"))
			{
				has_restriction = 1;
				continue;
			}

			config_error("%s:%i: unknown directive set::restrict-commands::%s::%s", cep2->ce_fileptr->cf_filename, cep2->ce_varlinenum, cep->ce_varname, cep2->ce_varname);
			errors++;
		}

		if (!has_restriction)
		{
			config_error("%s:%i: no restrictions were set for set::restrict-commands::%s (either 'connect-delay' or 'disable' is required)", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int cmdrestrict_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cep2;
	char *cmd;
	RestrictedCmd *rcmd;

	// We are only interested in set::restrict-commands
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->ce_varname, "restrict-commands"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		// Let's do it like this for good measure
		if (!(cmd = find_cmd_by_conftag(cep->ce_varname)))
			continue;

		// Try to add override before allocating the struct so we don't waste memory (should be safe in the configrun stage anyways?)
		// Also don't override PRIVMSG/NOTICE because those are handled through hooks instead
		if (strcmp(cmd, "PRIVMSG") && strcmp(cmd, "NOTICE"))
		{
			// Let's hope nobody tries to unload the module for PRIVMSG/NOTICE :^)
			if (!CommandExists(cmd))
			{
				config_warn("[cmdrestrict] The specified command isn't properly loaded (meaning we're unable to override it): %s", cmd);
				continue;
			}

			if (!CmdoverrideAdd(ModInf.handle, cmd, cmdrestrict_override))
			{
				config_warn("[cmdrestrict] Failed to add override for the specified command (NO RESTRICTIONS APPLY): %s", cmd);
				continue;
			}
		}

		rcmd = MyMallocEx(sizeof(RestrictedCmd));
		rcmd->cmd = strdup(cmd);
		rcmd->conftag = strdup(cep->ce_varname);
		for (cep2 = cep->ce_entries; cep2; cep2 = cep2->ce_next)
		{
			if (!cep2->ce_vardata)
				continue;

			if (!strcmp(cep2->ce_varname, "connect-delay"))
			{
				rcmd->connect_delay = config_checkval(cep2->ce_vardata, CFG_TIME);
				continue;
			}

			if (!strcmp(cep2->ce_varname, "exempt-identified"))
			{
				rcmd->exempt_identified = config_checkval(cep2->ce_vardata, CFG_YESNO);
				continue;
			}

			if (!strcmp(cep2->ce_varname, "exempt-reputation-score"))
			{
				rcmd->exempt_reputation_score = atoi(cep2->ce_vardata);
				continue;
			}

			if (!strcmp(cep2->ce_varname, "disable"))
			{
				rcmd->disable = config_checkval(cep2->ce_vardata, CFG_YESNO);
				continue; // Maybe break instead, since it takes precedence anyways?
			}
		}
		AddListItem(rcmd, RestrictedCmdList);
	}

	return 1;
}

int cmdrestrict_canbypass(aClient *sptr, RestrictedCmd *rcmd) {
	int exempt;

	if (!sptr || !rcmd)
		return 1;

	exempt = 0;
	if (rcmd->exempt_identified && IsLoggedIn(sptr))
		exempt = 1;
	if (rcmd->exempt_reputation_score > 0 && (GetReputation(sptr)) >= rcmd->exempt_reputation_score)
		exempt = 1;

	if (!exempt && sptr->local && (TStime() - sptr->local->firsttime) < rcmd->connect_delay)
		return 0;

	return 1;
}

char *cmdrestrict_hook_prechanmsg(aClient *sptr, aChannel *chptr, MessageTag *mtags, char *text, int notice)
{
	return cmdrestrict_hook_wrapper(sptr, text, notice, "channel", (notice ? "channel-notice" : "channel-message"));
}

char *cmdrestrict_hook_preusermsg(aClient *sptr, aClient *to, char *text, int notice)
{
	return cmdrestrict_hook_wrapper(sptr, text, notice, "user", (notice ? "private-notice" : "private-message"));
}

char *cmdrestrict_hook_wrapper(aClient *sptr, char *text, int notice, char *display, char *conftag)
{
	RestrictedCmd *rcmd;

	// Let's allow non-local users, opers and U:Lines early =]
	if (!MyClient(sptr) || !sptr->local || IsOper(sptr) || IsULine(sptr))
		return text;

	rcmd = find_restrictions_byconftag(conftag);
	if (rcmd)
	{
		if (rcmd->disable)
		{
			sendnotice(sptr, "Sending of %ss to %ss been disabled by the network administrators", (notice ? "notice" : "message"), display);
			return NULL;
		}
		if (!cmdrestrict_canbypass(sptr, rcmd))
		{
			sendnotice(sptr, "You cannot send %ss to %ss until you've been connected for %ld seconds or more", (notice ? "notice" : "message"), display, rcmd->connect_delay);
			return NULL;
		}
	}

	// No restrictions apply, process command as normal =]
	return text;
}

CMD_OVERRIDE_FUNC(cmdrestrict_override)
{
	RestrictedCmd *rcmd;

	if (!MyClient(sptr) || !sptr->local || IsOper(sptr) || IsULine(sptr))
		return CallCmdoverride(ovr, cptr, sptr, recv_mtags, parc, parv);

	rcmd = find_restrictions_bycmd(ovr->command->cmd);
	if (rcmd)
	{
		if (rcmd->disable)
		{
			sendnotice(sptr, "The command %s has been disabled by the network administrators", ovr->command->cmd);
			return 0;
		}
		if(!cmdrestrict_canbypass(sptr, rcmd))
		{
			sendnotice(sptr, "You cannot use the %s command until you've been connected for %ld seconds or more", ovr->command->cmd, rcmd->connect_delay);
			return 0;
		}
	}

	// No restrictions apply, process command as normal =]
	return CallCmdoverride(ovr, cptr, sptr, recv_mtags, parc, parv);
}
