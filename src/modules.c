/************************************************************************
 *   UnrealIRCd - Unreal Internet Relay Chat Daemon - src/modules.c
 *   (C) 1999-2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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
#include "msg.h"
#include "channel.h"
#include "userload.h"
#include "version.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#include <dlfcn.h>

ModuleInfo *Modules[MAXMODULES];
int  modules_loaded = 0;
ModuleInfo *module_buffer;
void module_init(void)
{
	bzero(Modules, sizeof(Modules));
	modules_loaded = 0;
}

int  load_module(char *module)
{
#ifdef _WIN32
	HMODULE Mod;
#else
	void *Mod;
#endif
	void (*mod_init) ();
	void (*mod_unload) ();
	int  i;
	module_buffer = NULL;
	if (Mod = irc_dlopen(module, RTLD_LAZY))
	{
		/* Succeed loading module */
		/* Locate mod_init function */
		mod_init = irc_dlsym(Mod, "mod_init");
		if (!mod_init)
		{
			mod_init = irc_dlsym(Mod, "_mod_init");
			if (!mod_init)
			{
				sendto_realops
				    ("Failed to load module %s: Could not locate mod_init",
				    module);
				irc_dlclose(Mod);
				return -1;
			}
		}
		/* Run mod_init */
		(*mod_init) ();
		if (!module_buffer)
		{
			sendto_realops
			    ("Failed to load module %s: mod_init did not set module_buffer",
			    module);
			irc_dlclose(Mod);
			return -1;
		}
		if (module_buffer->mversion != MOD_VERSION)
		{
			sendto_realops
			    ("Failed to load module %s: mversion is %i, not %i as we require",
			    module, module_buffer->mversion, MOD_VERSION);
			irc_dlclose(Mod);
			return -1;
		}
		if (!module_buffer->name || !module_buffer->version
		    || !module_buffer->description)
		{
			sendto_realops
			    ("Failed to load module %s: name/version/description missing",
			    module);
			irc_dlclose(Mod);
			return -1;
		}
		mod_unload = irc_dlsym(Mod, "mod_unload");
		if (!mod_unload)
		{
			mod_unload = irc_dlsym(Mod, "_mod_unload");
			if (!mod_unload)
			{
				sendto_realops
				    ("Failed to load module %s: Could not locate mod_unload",
				    module);
				irc_dlclose(Mod);
				return -1;
			}
		}
		module_buffer->dll = Mod;
		module_buffer->unload = mod_unload;
		for (i = 0; i <= MAXMODULES; i++)
		{
			if (!Modules[i])
			{
				Modules[i] = module_buffer;
				modules_loaded++;
				break;
			}
		}
		if (i == MAXMODULES)
		{
			sendto_realops
			    ("Failed to load module %s: Too many modules loaded");
			irc_dlclose(Mod);
			return -1;
		}
		return 1;
	}
	else
	{
		const char *err = irc_dlerror();

		if (err)
		{
			sendto_realops("Failed to load module %s: %s",
			    module, err);
		}
		return -1;
	}
}

int	unload_module(char *name)
{
	int	i;
	
	for (i = 0; i <= MAXMODULES; i++)
		if (Modules[i])
			if (!strcmp(Modules[i]->name, name))
				break;		
	if (i == MAXMODULES)
		return -1;
	
	/* Call unload */
	(*Modules[i]->unload)();
	
	irc_dlclose(Modules[i]->dll);
	
	Modules[i] = NULL;
	modules_loaded--;
	return 1;
}

int  m_module(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	int 		i;
	ModuleInfo *mi;
	
	if (!IsAdmin(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "MODULE");
		return 0;
	}
	if (!match(parv[1], "load"))
	{
		if (parc < 3)
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			    me.name, parv[0], "MODULES LOAD");
			return 0;
		}
		sendto_realops("Trying to load module %s", parv[2]);
		if (load_module(parv[2]) == 1)
			sendto_realops("Loaded module %s", parv[2]);
	}
	else
	if (!match(parv[1], "unload"))
	{
		if (parc < 3)
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			    me.name, parv[0], "MODULES UNLOAD");
			return 0;
		}
		sendto_realops("Trying to unload module %s", parv[2]);
		if (unload_module(parv[2]) == 1)
			sendto_realops("Unloaded module %s", parv[2]);
		
	}		
	else
	if (!match(parv[1], "status"))
	{
		if (modules_loaded == 0)
		{
			sendto_one(sptr, ":%s NOTICE %s :*** No modules loaded", me.name, sptr->name);
			return 1;
		}
		for (i = 0; i < MAXMODULES; i++)
			if (mi = Modules[i])
			{
				sendto_one(sptr, ":%s NOTICE %s :*** %s - %s (%s)", me.name, sptr->name,
					mi->name, mi->version, mi->description);	
			}
	}
	else
	{
		sendto_one(sptr, ":%s NOTICE %s :*** Syntax: /module load|unload|status",
			me.name, sptr->name);
	}
	return 1;
}