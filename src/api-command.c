/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/api-command.c
 *   Copyright (C) 2004 Dominick Meglio and The UnrealIRCd Team
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

/** @file
 * @brief Command API - both for modules and the core
 */
#include "unrealircd.h"

/* Forward declarations */
static Command *CommandAddInternal(Module *module, const char *cmd, CmdFunc func, AliasCmdFunc aliasfunc, unsigned char params, int flags);
static RealCommand *add_Command_backend(const char *cmd);

/** @defgroup CommandAPI Command API
 * @{
 */

/** Returns 1 if the specified command exists
 */
int CommandExists(const char *name)
{
	RealCommand *p;
	
	for (p = CommandHash[toupper(*name)]; p; p = p->next)
	{
		if (!strcasecmp(p->cmd, name))
			return 1;
	}

	return 0;
}

/** Register a new command.
 * @param module	The module (usually modinfo->handle)
 * @param cmd		The command name (eg: "SOMECMD")
 * @param func		The command handler function
 * @param params	Number of parameters or MAXPARA
 * @param flags		Who may execute this command - one or more CMD_* flags
 * @returns The newly registered command, or NULL in case of error (eg: already exist)
 */
Command *CommandAdd(Module *module, const char *cmd, CmdFunc func, unsigned char params, int flags)
{
	if (flags & CMD_ALIAS)
	{
		config_error("Command '%s' used CommandAdd() to add a command alias, "
		             "but should have used AliasAdd() instead. "
		             "Old 3rd party module %s? Check for updates!",
		             cmd,
		             module ? module->header->name : "");
		return NULL;
	}
	return CommandAddInternal(module, cmd, func, NULL, params, flags);
}

/** Register a new alias.
 * @param module	The module (usually modinfo->handle)
 * @param cmd		The alias name (eg: "SOMECMD")
 * @param func		The alias handler function
 * @param params	Number of parameters or MAXPARA
 * @param flags		Who may execute this command - one or more CMD_* flags
 * @returns The newly registered command (alias), or NULL in case of error (eg: already exist)
 */
Command *AliasAdd(Module *module, const char *cmd, AliasCmdFunc aliasfunc, unsigned char params, int flags)
{
	if (!(flags & CMD_ALIAS))
		flags |= CMD_ALIAS;
	return CommandAddInternal(module, cmd, NULL, aliasfunc, params, flags);
}

/** @} */

static Command *CommandAddInternal(Module *module, const char *cmd, CmdFunc func, AliasCmdFunc aliasfunc, unsigned char params, int flags)
{
	Command *command = NULL;
	RealCommand *c;

	if ((c = find_command(cmd, flags)) && (c->flags == flags))
	{
		if (module)
			module->errorcode = MODERR_EXISTS;
		return NULL;
	}
	
	if (!flags)
	{
		config_error("CommandAdd(): Could not add command '%s': flags are 0", cmd);
		if (module)
			module->errorcode = MODERR_INVALID;
		return NULL;
	}
	
	c = add_Command_backend(cmd);
	c->parameters = (params > MAXPARA) ? MAXPARA : params;
	c->flags = flags;
	c->func = func;
	c->aliasfunc = aliasfunc;

	if (module)
	{
		ModuleObject *cmdobj = safe_alloc(sizeof(ModuleObject));
		command = safe_alloc(sizeof(Command));
		command->cmd = c;
		command->cmd->owner = module;
		command->cmd->friend = NULL;
		cmdobj->object.command = command;
		cmdobj->type = MOBJ_COMMAND;
		AddListItem(cmdobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}

	return command;
}

/** Delete a command - only used internally.
 * @param command	The command (can be NULL)
 * @param cmd		The "real" command
 */
void CommandDelX(Command *command, RealCommand *cmd)
{
	CommandOverride *ovr, *ovrnext;

	DelListItem(cmd, CommandHash[toupper(*cmd->cmd)]);
	if (command && cmd->owner)
	{
		ModuleObject *cmdobj;
		for (cmdobj = cmd->owner->objects; cmdobj; cmdobj = cmdobj->next)
		{
			if (cmdobj->type == MOBJ_COMMAND && cmdobj->object.command == command)
			{
				DelListItem(cmdobj,cmd->owner->objects);
				safe_free(cmdobj);
				break;
			}
		}
	}
	for (ovr = cmd->overriders; ovr; ovr = ovrnext)
	{
		ovrnext = ovr->next;
		CommandOverrideDel(ovr);
	}
	safe_free(cmd->cmd);
	safe_free(cmd);
	if (command)
		safe_free(command);
}

/** De-register a command - not called by modules, only internally.
 * For modules this is done automatically.
 */
void CommandDel(Command *command)
{
	CommandDelX(command, command->cmd);
}

/** @defgroup CommandAPI Command API
 * @{
 */

/** Calls the specified command for the user, as if it was received
 * that way on IRC.
 * @param client	Client that is the source.
 * @param mtags		Message tags for this command (or NULL).
 * @param cmd		Command to run, eg "JOIN".
 * @param parc		Parameter count plus 1.
 * @param parv		Parameter array.
 * @note Make sure you terminate the last parv[] parameter with NULL,
 *       this can easily be forgotten, but certain functions depend on it,
 *       you risk crashes otherwise.
 * @note Once do_cmd() has returned, be sure to check IsDead(client) to
 *       see if the client has been killed. This may happen due to various
 *       reasons, including spamfilter kicking in or some other security
 *       measure.
 * @note Do not pass insane parameters. The combined size of all parameters
 *       should not exceed 510 bytes, since that is what all code expects.
 *       Similarly, you should not exceed MAXPARA for parc.
 * @note If mtags is NULL then new message tags are created for the command
 *       (and destroyed before return).
 */
void do_cmd(Client *client, MessageTag *mtags, const char *cmd, int parc, const char *parv[])
{
	RealCommand *cmptr;

	cmptr = find_command_simple(cmd);
	if (cmptr)
	{
		int gen_mtags = (mtags == NULL) ? 1 : 0;
		if (gen_mtags)
			new_message(client, NULL, &mtags);
		(*cmptr->func) (client, mtags, parc, parv);
		if (gen_mtags)
			free_message_tags(mtags);
	}
}

/** @} */

/**** This is the "real command" API *****
 * Perhaps one day we will merge the two, if possible.
 */

RealCommand *CommandHash[256]; /* one per letter */

/** Initialize the command API - executed on startup.
 * This also registers some core functions.
 */
void init_CommandHash(void)
{
	memset(CommandHash, 0, sizeof(CommandHash));
	CommandAdd(NULL, MSG_ERROR, cmd_error, MAXPARA, CMD_UNREGISTERED|CMD_SERVER);
	CommandAdd(NULL, MSG_VERSION, cmd_version, MAXPARA, CMD_UNREGISTERED|CMD_USER|CMD_SERVER);
	CommandAdd(NULL, MSG_INFO, cmd_info, MAXPARA, CMD_USER);
	CommandAdd(NULL, MSG_DNS, cmd_dns, MAXPARA, CMD_USER);
	CommandAdd(NULL, MSG_REHASH, cmd_rehash, MAXPARA, CMD_USER|CMD_SERVER);
	CommandAdd(NULL, MSG_RESTART, cmd_restart, 2, CMD_USER);
	CommandAdd(NULL, MSG_DIE, cmd_die, MAXPARA, CMD_USER);
	CommandAdd(NULL, MSG_CREDITS, cmd_credits, MAXPARA, CMD_USER);
	CommandAdd(NULL, MSG_LICENSE, cmd_license, MAXPARA, CMD_USER);
	CommandAdd(NULL, MSG_MODULE, cmd_module, MAXPARA, CMD_USER);
}

static RealCommand *add_Command_backend(const char *cmd)
{
	RealCommand *c = safe_alloc(sizeof(RealCommand));

	safe_strdup(c->cmd, cmd);

	/* Add in hash with hash value = first byte */
	AddListItem(c, CommandHash[toupper(*cmd)]);

	return c;
}

/** @defgroup CommandAPI Command API
 * @{
 */

/** Find a command by name and flags */
RealCommand *find_command(const char *cmd, int flags)
{
	RealCommand *p;
	for (p = CommandHash[toupper(*cmd)]; p; p = p->next)
	{
		if (flags & CMD_CONTROL)
		{
			if (!(p->flags & CMD_CONTROL))
				continue;
		} else
		{
			if ((flags & CMD_UNREGISTERED) && !(p->flags & CMD_UNREGISTERED))
				continue;
			if ((flags & CMD_SHUN) && !(p->flags & CMD_SHUN))
				continue;
			if ((flags & CMD_VIRUS) && !(p->flags & CMD_VIRUS))
				continue;
			if ((flags & CMD_ALIAS) && !(p->flags & CMD_ALIAS))
				continue;
			if (p->flags & CMD_CONTROL)
				continue; /* important to also filter it this way ;) */
		}

		if (!strcasecmp(p->cmd, cmd))
			return p;
	}
	return NULL;
}

/** Find a command by name (no access rights check) */
RealCommand *find_command_simple(const char *cmd)
{
	RealCommand *c;

	for (c = CommandHash[toupper(*cmd)]; c; c = c->next)
	{
		if (!strcasecmp(c->cmd, cmd))
				return c;
	}

	return NULL;
}

/** @} */
