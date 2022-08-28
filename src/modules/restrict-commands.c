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
	"1.0.2",
	"Restrict specific commands unless certain conditions have been met",
	"UnrealIRCd Team",
	"unrealircd-6",
};

typedef struct RestrictedCommand RestrictedCommand;
struct RestrictedCommand {
	RestrictedCommand *prev, *next;
	char *cmd;
	char *conftag;
	SecurityGroup *except;
};

typedef struct {
	char *conftag;
	char *cmd;
} CmdMap;

// Forward declarations
const char *find_cmd_byconftag(const char *conftag);
RestrictedCommand *find_restrictions_bycmd(const char *cmd);
RestrictedCommand *find_restrictions_byconftag(const char *conftag);
int rcmd_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int rcmd_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
int rcmd_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);
int rcmd_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype);
int rcmd_block_message(Client *client, const char *text, SendType sendtype, const char **errmsg, const char *display, const char *conftag);
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
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, -1000000, rcmd_can_send_to_channel);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_USER, -1000000, rcmd_can_send_to_user);
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
		safe_free(rcmd->conftag);
		safe_free(rcmd->cmd);
		free_security_group(rcmd->except);
		DelListItem(rcmd, RestrictedCommandList);
		safe_free(rcmd);
	}
	RestrictedCommandList = NULL;
	return MOD_SUCCESS;
}

const char *find_cmd_byconftag(const char *conftag) {
	CmdMap *cmap;
	for (cmap = conf_cmdmaps; cmap->conftag; cmap++)
	{
		if (!strcmp(cmap->conftag, conftag))
			return cmap->cmd;
	}
	return NULL;
}

RestrictedCommand *find_restrictions_bycmd(const char *cmd) {
	RestrictedCommand *rcmd;
	for (rcmd = RestrictedCommandList; rcmd; rcmd = rcmd->next)
	{
		if (!strcasecmp(rcmd->cmd, cmd))
			return rcmd;
	}
	return NULL;
}

RestrictedCommand *find_restrictions_byconftag(const char *conftag) {
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
	int warn_disable = 0;
	ConfigEntry *cep, *cep2;

	// We are only interested in set::restrict-commands
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->name, "restrict-commands"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		for (cep2 = cep->items; cep2; cep2 = cep2->next)
		{
			if (!strcmp(cep2->name, "disable"))
			{
				config_warn("%s:%i: set::restrict-commands::%s: the 'disable' option has been removed.",
				            cep2->file->filename, cep2->line_number, cep->name);
				if (!warn_disable)
				{
					config_warn("Simply remove 'disable yes;' from the configuration file and "
				                   "it will have the same effect without it (will disable the command).");
					warn_disable = 1;
				}
				continue;
			}

			if (!strcmp(cep2->name, "except"))
			{
				test_match_block(cf, cep2, &errors);
				continue;
			}

			if (!cep2->value)
			{
				config_error("%s:%i: blank set::restrict-commands::%s:%s without value", cep2->file->filename, cep2->line_number, cep->name, cep2->name);
				errors++;
				continue;
			}

			if (!strcmp(cep2->name, "connect-delay"))
			{
				long v = config_checkval(cep2->value, CFG_TIME);
				if ((v < 1) || (v > 3600))
				{
					config_error("%s:%i: set::restrict-commands::%s::connect-delay should be in range 1-3600", cep2->file->filename, cep2->line_number, cep->name);
					errors++;
				}
				continue;
			}

			if (!strcmp(cep2->name, "exempt-identified"))
				continue;

			if (!strcmp(cep2->name, "exempt-webirc"))
				continue;

			if (!strcmp(cep2->name, "exempt-tls"))
				continue;

			if (!strcmp(cep2->name, "exempt-reputation-score"))
			{
				int v = atoi(cep2->value);
				if (v <= 0)
				{
					config_error("%s:%i: set::restrict-commands::%s::exempt-reputation-score must be greater than 0", cep2->file->filename, cep2->line_number, cep->name);
					errors++;
				}
				continue;
			}

			config_error("%s:%i: unknown directive set::restrict-commands::%s::%s", cep2->file->filename, cep2->line_number, cep->name, cep2->name);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int rcmd_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cep2;
	const char *cmd, *conftag;
	RestrictedCommand *rcmd;

	// We are only interested in set::restrict-commands
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->name, "restrict-commands"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		// May need to switch some stuff around for special cases where the config directive doesn't match the actual command
		conftag = NULL;
		if ((cmd = find_cmd_byconftag(cep->name)))
			conftag = cep->name;
		else
			cmd = cep->name;

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
			if (find_restrictions_bycmd(cmd))
			{
				config_warn("[restrict-commands] Multiple set::restrict-commands items for command '%s'. "
				            "Only one config block will be effective.",
				            cmd);
				continue;
			}
			if (!CommandOverrideAdd(ModInf.handle, cmd, 0, rcmd_override))
			{
				config_warn("[restrict-commands] Failed to add override for '%s' (NO RESTRICTIONS APPLY)", cmd);
				continue;
			}
		}

		rcmd = safe_alloc(sizeof(RestrictedCommand));
		safe_strdup(rcmd->cmd, cmd);
		safe_strdup(rcmd->conftag, conftag);
		rcmd->except = safe_alloc(sizeof(SecurityGroup));

		for (cep2 = cep->items; cep2; cep2 = cep2->next)
		{
			if (!strcmp(cep2->name, "except"))
			{
				conf_match_block(cf, cep2, &rcmd->except);
				continue;
			}

			if (!cep2->value)
				continue;

			if (!strcmp(cep2->name, "connect-delay"))
			{
				rcmd->except->connect_time = config_checkval(cep2->value, CFG_TIME);
				continue;
			}

			if (!strcmp(cep2->name, "exempt-identified"))
			{
				rcmd->except->identified = config_checkval(cep2->value, CFG_YESNO);
				continue;
			}
			
			if (!strcmp(cep2->name, "exempt-webirc"))
			{
				rcmd->except->webirc = config_checkval(cep2->value, CFG_YESNO);
				continue;
			}

			if (!strcmp(cep2->name, "exempt-tls"))
			{
				rcmd->except->tls = config_checkval(cep2->value, CFG_YESNO);
				continue;
			}

			if (!strcmp(cep2->name, "exempt-reputation-score"))
			{
				rcmd->except->reputation_score = atoi(cep2->value);
				continue;
			}
		}
		AddListItem(rcmd, RestrictedCommandList);
	}

	return 1;
}

int rcmd_canbypass(Client *client, RestrictedCommand *rcmd)
{
	if (!client || !rcmd)
		return 1;
	if (user_allowed_by_security_group(client, rcmd->except))
		return 1;
	return 0;
}

int rcmd_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	if (rcmd_block_message(client, *msg, sendtype, errmsg, "channel", (sendtype == SEND_TYPE_NOTICE ? "channel-notice" : "channel-message")))
		return HOOK_DENY;

	return HOOK_CONTINUE;
}

int rcmd_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype)
{
	// Need a few extra exceptions for user messages only =]
	if ((client == target) || IsULine(target))
		return HOOK_CONTINUE; /* bypass/exempt */

	if (rcmd_block_message(client, *text, sendtype, errmsg, "user", (sendtype == SEND_TYPE_NOTICE ? "private-notice" : "private-message")))
		return HOOK_DENY;

	return HOOK_CONTINUE;
}

int rcmd_block_message(Client *client, const char *text, SendType sendtype, const char **errmsg, const char *display, const char *conftag)
{
	RestrictedCommand *rcmd;
	static char errbuf[256];

	// Let's allow non-local users, opers and U:Lines early =]
	if (!MyUser(client) || !client->local || IsOper(client) || IsULine(client))
		return 0;

	rcmd = find_restrictions_byconftag(conftag);
	if (rcmd && !rcmd_canbypass(client, rcmd))
	{
		int notice = (sendtype == SEND_TYPE_NOTICE ? 1 : 0); // temporary hack FIXME !!!
		if (rcmd->except->connect_time)
		{
			ircsnprintf(errbuf, sizeof(errbuf),
				    "You cannot send %ss to %ss until you've been connected for %ld seconds or more",
				    (notice ? "notice" : "message"), display, rcmd->except->connect_time);
		} else {
			ircsnprintf(errbuf, sizeof(errbuf),
				    "Sending of %ss to %ss been disabled by the network administrators",
				    (notice ? "notice" : "message"), display);
		}
		*errmsg = errbuf;
		return 1;
	}

	// No restrictions apply, process command as normal =]
	return 0;
}

CMD_OVERRIDE_FUNC(rcmd_override)
{
	RestrictedCommand *rcmd;

	if (!MyUser(client) || !client->local || IsOper(client) || IsULine(client))
	{
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}

	rcmd = find_restrictions_bycmd(ovr->command->cmd);
	if (rcmd && !rcmd_canbypass(client, rcmd))
	{
		if (rcmd->except->connect_time)
		{
			sendnumericfmt(client, ERR_UNKNOWNCOMMAND,
			               "%s :You must be connected for at least %ld seconds before you can use this command",
			               ovr->command->cmd, rcmd->except->connect_time);
		} else {
			sendnumericfmt(client, ERR_UNKNOWNCOMMAND,
			               "%s :This command is disabled by the network administrator",
			               ovr->command->cmd);
		}
		return;
	}

	// No restrictions apply, process command as normal =]
	CALL_NEXT_COMMAND_OVERRIDE();
}
