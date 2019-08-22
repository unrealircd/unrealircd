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

#include "unrealircd.h"

int CommandExists(char *name)
{
	aCommand *p;
	
	for (p = CommandHash[toupper(*name)]; p; p = p->next)
	{
		if (!stricmp(p->cmd, name))
			return 1;
	}

	return 0;
}

Command *CommandAddInternal(Module *module, char *cmd, CmdFunc func, AliasCmdFunc aliasfunc, unsigned char params, int flags)
{
	Command *command = NULL;
	aCommand *c;

	if (find_Command_simple(cmd))
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
		ModuleObject *cmdobj = MyMallocEx(sizeof(ModuleObject));
		command = MyMallocEx(sizeof(Command));
		command->cmd = c;
		command->cmd->owner = module;
		command->cmd->friend = NULL;
		cmdobj->object.command = command;
		cmdobj->type = MOBJ_COMMAND;
		AddListItem(cmdobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}

	if (flags & M_ANNOUNCE)
	{
		config_warn("Command '%s' has M_ANNOUNCE set, but this is no longer "
		            "supported. Old 3rd party module %s? Check for updates!",
		            c->cmd, module ? module->header->name : "");
	}

	return command;
}

Command *CommandAdd(Module *module, char *cmd, CmdFunc func, unsigned char params, int flags)
{
	if (flags & M_ALIAS)
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

Command *AliasAdd(Module *module, char *cmd, AliasCmdFunc aliasfunc, unsigned char params, int flags)
{
	if (!(flags & M_ALIAS))
		flags |= M_ALIAS;
	return CommandAddInternal(module, cmd, NULL, aliasfunc, params, flags);
}

void CommandDelX(Command *command, aCommand *cmd)
{
	Cmdoverride *ovr, *ovrnext;

	DelListItem(cmd, CommandHash[toupper(*cmd->cmd)]);
	if (command && cmd->owner)
	{
		ModuleObject *cmdobj;
		for (cmdobj = cmd->owner->objects; cmdobj; cmdobj = cmdobj->next)
		{
			if (cmdobj->type == MOBJ_COMMAND && cmdobj->object.command == command)
			{
				DelListItem(cmdobj,cmd->owner->objects);
				MyFree(cmdobj);
				break;
			}
		}
	}
	for (ovr = cmd->overriders; ovr; ovr = ovrnext)
	{
		ovrnext = ovr->next;
		CmdoverrideDel(ovr);
	}
	MyFree(cmd->cmd);
	MyFree(cmd);
	if (command)
		MyFree(command);
}

void CommandDel(Command *command)
{
	CommandDelX(command, command->cmd);
}
