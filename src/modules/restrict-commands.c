/*
 * Restrict specific commands unless certain conditions have been met
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

ModuleHeader MOD_HEADER = {
	"restrict-commands",
	"1.0",
	"Restrict specific commands unless certain conditions have been met",
	"UnrealIRCd Team",
	"unrealircd-5",
};

#define GetReputation(acptr) (moddata_client_get(acptr, "reputation") ? atoi(moddata_client_get(acptr, "reputation")) : 0)

typedef struct RestrictedCommand RestrictedCommand;
struct RestrictedCommand {
	RestrictedCommand *prev, *next;
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
char *find_cmd_byconftag(char *conftag);
RestrictedCommand *find_restrictions_bycmd(char *cmd);
RestrictedCommand *find_restrictions_byconftag(char *conftag);
int rcmd_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int rcmd_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
char *rcmd_hook_prechanmsg(Client *sptr, Channel *chptr, MessageTag *mtags, char *text, int notice);
char *rcmd_hook_preusermsg(Client *sptr, Client *to, char *text, int notice);
char *rcmd_hook_wrapper(Client *sptr, char *text, int notice, char *display, char *conftag);
CMD_OVERRIDE_FUNC(rcmd_override);

// Globals
static ModuleInfo ModInf;
RestrictedCommand *RestrictedCommandList = NULL;
CmdMap conf_cmdmaps[] = {
	// These are special cases in which we can't override the command, so they are handled through hooks instead
	{ "channel-message", "PRIVMSG" },
	{ "channel-notice", "NOTICE" },
	{ "private-message", "PRIVMSG" },
	{ "private-notice", "NOTICE" },
	{ NULL, NULL, }, // REQUIRED for the loop to properly work
};

MOD_TEST()
{
	memcpy(&ModInf, modinfo, modinfo->size);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, rcmd_configtest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, rcmd_configrun);

	// Due to the nature of PRIVMSG/NOTICE we're gonna need to hook into PRE_* stuff instead of using command overrides
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_CHANMSG, -1000000, rcmd_hook_prechanmsg);
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_USERMSG, -1000000, rcmd_hook_preusermsg);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (ModuleGetError(modinfo->handle) != MODERR_NOERROR)
	{
		config_error("A critical error occurred when loading module %s: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	RestrictedCommand *rcmd, *next;
	for (rcmd = RestrictedCommandList; rcmd; rcmd = next)
	{
		next = rcmd->next;
		MyFree(rcmd->conftag);
		MyFree(rcmd->cmd);
		DelListItem(rcmd, RestrictedCommandList);
		MyFree(rcmd);
	}
	RestrictedCommandList = NULL;
	return MOD_SUCCESS;
}

char *find_cmd_byconftag(char *conftag) {
	CmdMap *cmap;
	for (cmap = conf_cmdmaps; cmap->conftag; cmap++)
	{
		if (!strcmp(cmap->conftag, conftag))
			return cmap->cmd;
	}
	return NULL;
}

RestrictedCommand *find_restrictions_bycmd(char *cmd) {
	RestrictedCommand *rcmd;
	for (rcmd = RestrictedCommandList; rcmd; rcmd = rcmd->next)
	{
		if (!strcasecmp(rcmd->cmd, cmd))
			return rcmd;
	}
	return NULL;
}

RestrictedCommand *find_restrictions_byconftag(char *conftag) {
	RestrictedCommand *rcmd;
	for (rcmd = RestrictedCommandList; rcmd; rcmd = rcmd->next)
	{
		if (rcmd->conftag && !strcmp(rcmd->conftag, conftag))
			return rcmd;
	}
	return NULL;
}

int rcmd_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cep2;
	RestrictedCommand *rcmd;
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
		has_restriction = 0;
		for (cep2 = cep->ce_entries; cep2; cep2 = cep2->ce_next)
		{
			if (!strcmp(cep2->ce_varname, "disable"))
			{
				has_restriction = 1;
				continue;
			}

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

int rcmd_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cep2;
	char *cmd, *conftag;
	RestrictedCommand *rcmd;

	// We are only interested in set::restrict-commands
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->ce_varname, "restrict-commands"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		// May need to switch some stuff around for special cases where the config directive doesn't match the actual command
		conftag = NULL;
		if ((cmd = find_cmd_byconftag(cep->ce_varname)))
			conftag = cep->ce_varname;
		else
			cmd = cep->ce_varname;

		// Try to add override before even allocating the struct so we can bail early
		// Also don't override anything from the conf_cmdmaps[] list because those are handled through hooks instead
		if (!conftag)
		{
			// Let's hope nobody tries to unload the module for PRIVMSG/NOTICE :^)
			if (!CommandExists(cmd))
			{
				config_warn("[restrict-commands] Command '%s' does not exist. Did you mistype? Or is the module providing it not loaded?", cmd);
				continue;
			}

			if (!CommandOverrideAdd(ModInf.handle, cmd, rcmd_override))
			{
				config_warn("[restrict-commands] Failed to add override for '%s' (NO RESTRICTIONS APPLY)", cmd);
				continue;
			}
		}

		rcmd = MyMallocEx(sizeof(RestrictedCommand));
		rcmd->cmd = strdup(cmd);
		rcmd->conftag = (conftag ? strdup(conftag) : NULL);
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
				rcmd->disable = cep2->ce_vardata ? config_checkval(cep2->ce_vardata, CFG_YESNO) : 1;
				break; // Using break instead of continue since 'disable' takes precedence anyways
			}
		}
		AddListItem(rcmd, RestrictedCommandList);
	}

	return 1;
}

int rcmd_canbypass(Client *sptr, RestrictedCommand *rcmd) {
	if (!sptr || !rcmd)
		return 1;
	if (rcmd->exempt_identified && IsLoggedIn(sptr))
		return 1;
	if (rcmd->exempt_reputation_score > 0 && (GetReputation(sptr) >= rcmd->exempt_reputation_score))
		return 1;
	if (sptr->local && (TStime() - sptr->local->firsttime < rcmd->connect_delay))
		return 0;
	return 1; // Default to yes so we don't drop too many commands
}

char *rcmd_hook_prechanmsg(Client *sptr, Channel *chptr, MessageTag *mtags, char *text, int notice)
{
	return rcmd_hook_wrapper(sptr, text, notice, "channel", (notice ? "channel-notice" : "channel-message"));
}

char *rcmd_hook_preusermsg(Client *sptr, Client *to, char *text, int notice)
{
	// Need a few extra exceptions for user messages only =]
	if ((sptr == to) || IsULine(to))
		return text;
	return rcmd_hook_wrapper(sptr, text, notice, "user", (notice ? "private-notice" : "private-message"));
}

char *rcmd_hook_wrapper(Client *sptr, char *text, int notice, char *display, char *conftag)
{
	RestrictedCommand *rcmd;

	// Let's allow non-local users, opers and U:Lines early =]
	if (!MyUser(sptr) || !sptr->local || IsOper(sptr) || IsULine(sptr))
		return text;

	rcmd = find_restrictions_byconftag(conftag);
	if (rcmd)
	{
		if (rcmd->disable)
		{
			sendnotice(sptr, "Sending of %ss to %ss been disabled by the network administrators", (notice ? "notice" : "message"), display);
			return NULL;
		}
		if (!rcmd_canbypass(sptr, rcmd))
		{
			sendnotice(sptr, "You cannot send %ss to %ss until you've been connected for %ld seconds or more", (notice ? "notice" : "message"), display, rcmd->connect_delay);
			return NULL;
		}
	}

	// No restrictions apply, process command as normal =]
	return text;
}

CMD_OVERRIDE_FUNC(rcmd_override)
{
	RestrictedCommand *rcmd;

	if (!MyUser(sptr) || !sptr->local || IsOper(sptr) || IsULine(sptr))
		return CallCommandOverride(ovr, cptr, sptr, recv_mtags, parc, parv);

	rcmd = find_restrictions_bycmd(ovr->command->cmd);
	if (rcmd)
	{
		if (rcmd->disable)
		{
			sendnotice(sptr, "The command %s has been disabled by the network administrators", ovr->command->cmd);
			return 0;
		}
		if (!rcmd_canbypass(sptr, rcmd))
		{
			sendnotice(sptr, "You cannot use the %s command until you've been connected for %ld seconds or more", ovr->command->cmd, rcmd->connect_delay);
			return 0;
		}
	}

	// No restrictions apply, process command as normal =]
	return CallCommandOverride(ovr, cptr, sptr, recv_mtags, parc, parv);
}
