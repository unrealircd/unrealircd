/************************************************************************
 *   UnrealIRCd - Unreal Internet Relay Chat Daemon - src/modules.c
 *   (C) 2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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
#include "version.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define RTLD_NOW 0
#else
#include <dlfcn.h>
#endif
#include <fcntl.h>
#include "h.h"

#ifndef RTLD_NOW
#define RTLD_NOW RTLD_LAZY
#endif

ModuleInfo	 *Modules[MAXMODULES];
unsigned char	ModuleFlags[MAXMODULES];
Hook	   	*Hooks[MAXHOOKTYPES];
Hook 	   	*global_i = NULL;

int  modules_loaded = 0;

void module_init(void)
{
	bzero(Modules, sizeof(Modules));
	bzero(Hooks, sizeof(Hooks));
	bzero(ModuleFlags, sizeof(ModuleFlags));
	modules_loaded = 0;
}

int  load_module(char *module, int module_load)
{
#ifndef STATIC_LINKING
#ifdef _WIN32
	HMODULE Mod;
#else
	void *Mod;
#endif
	int (*mod_init) ();
	int (*mod_load) ();
	void (*mod_unload) ();
	MSymbolTable *mod_dep;
	ModuleInfo *mod_header;
	int  i;

	Debug((DEBUG_DEBUG, "Attemping to load %s",
		module));

	if (Mod = irc_dlopen(module, RTLD_NOW))
	{
		/* Succeed loading module */
		/* We check header */
		mod_header = irc_dlsym(Mod, "mod_header");
		if (!mod_header) 
		{
			Debug((DEBUG_DEBUG, "Didn't find mod_header, trying _mod_header"));
			mod_header = irc_dlsym(Mod, "_mod_header");
		}
		if (!mod_header)
		{
			config_progress("%s: cannot load, no module header",
				module);
			irc_dlclose(Mod);
			return -1;
		}
		if (mod_header->mversion != MOD_VERSION)
		{
			config_progress
			    ("Failed to load module %s: mversion is %i, not %i as we require",
			    module, mod_header->mversion, MOD_VERSION);
			irc_dlclose(Mod);
			return -1;
		}
		if (!mod_header->name || !mod_header->version
		    || !mod_header->description)
		{
			config_progress
			    ("Failed to load module %s: name/version/description missing",
			    module);
			irc_dlclose(Mod);
			return -1;
		}
		for (i = 0; i < MAXMODULES; i++)
			if (Modules[i] && !strcmp(Modules[i]->name, mod_header->name))
			{
				Debug((DEBUG_DEBUG, "Module already loaded, duplicate"));
				/* We will unload it without notice, its a duplicate */
				irc_dlclose(Mod);
				return 1;
			}
		for (i = 0; i < MAXMODULES; i++)
		{
			if (!Modules[i])
			{
				Modules[i] = mod_header;
				modules_loaded++;
				break;
			}
		}
		if (i == MAXMODULES)
		{
			config_progress
			    ("Failed to load module %s: Too many modules loaded");
			Modules[i] = NULL;
			irc_dlclose(Mod);
			return -1;
		}
		
		/* Locate mod_depend */
		mod_dep =  irc_dlsym(Mod, "mod_depend");
		if (!mod_dep)
			mod_dep = irc_dlsym(Mod, "_mod_depend");
		if (mod_dep)
		{
			if (module_depend_resolve(mod_dep) == -1)
			{
				config_progress("%s: cannot load, missing dependancy",
					module);
				Modules[i] = NULL;
				irc_dlclose(Mod);
				return -1;
			}	
		}		
		
		/* Locate mod_init function */
		mod_init = irc_dlsym(Mod, "mod_init");
		if (!mod_init)
		{
			mod_init = irc_dlsym(Mod, "_mod_init");
			if (!mod_init)
			{
				config_progress
				    ("Failed to load module %s: Could not locate mod_init",
				    module);
				Modules[i] = NULL;
				irc_dlclose(Mod);
				return -1;
			}
		}
		mod_unload = irc_dlsym(Mod, "mod_unload");
		if (!mod_unload)
		{
			mod_unload = irc_dlsym(Mod, "_mod_unload");
			if (!mod_unload)
			{
				config_progress
				    ("Failed to load module %s: Could not locate mod_unload",
				    module);
				Modules[i] = NULL;
				irc_dlclose(Mod);
				return -1;
			}
		}
		
		mod_header->dll = Mod;
		mod_header->unload = mod_unload;

		if ((*mod_init)(module_load) < 0)
		{
			config_progress
			    ("Failed to load module %s: mod_init failed",
			    module);
			Modules[i] = NULL;
			irc_dlclose(Mod);
			return -1;
			
		}
		ModuleFlags[i] = 0;
		if (module_load)
		{
			mod_load = irc_dlsym(Mod, "mod_load");
			if (!mod_load)
			{
				mod_load = irc_dlsym(Mod, "_mod_load");
				if (mod_load)
				{
					(*mod_load)(module_load);
					ModuleFlags[i] |= MODFLAG_LOADED;
				}
			}
		}
		return 1;
	}
#ifndef _WIN32
	else
	{
		const char *err = irc_dlerror();

		if (err)
		{
			config_progress("Failed to load module %s: %s",
			    module, err);
		}
		return -1;
	}
#endif
#endif
}

void    unload_all_modules(void)
{
#ifndef STATIC_LINKING
	int	i;
	
	for (i = 0; i < MAXMODULES; i++)
		if (Modules[i])
		{
	
			/* Call unload */
			(*Modules[i]->unload)();
	
			irc_dlclose(Modules[i]->dll);
		
			Modules[i] = NULL;
			ModuleFlags[i] = MODFLAG_NONE;
			modules_loaded--;
	}
#endif
	return;
}

int	unload_module(char *name)
{
#ifndef STATIC_LINKING
	int	i;
	int	(*mod_delay)();
	
	for (i = 0; i < MAXMODULES; i++)
		if (Modules[i])
			if (!strcmp(Modules[i]->name, name))
				break;		
	if (i == MAXMODULES)
		return -1;
	
	
	mod_delay = irc_dlsym(Modules[i]->dll, "mod_delay");
	if (mod_delay)
	{
		/* 
		 * We are to question the modules mod_delay() if to unload or not
	 	 * 1 if we are to delay, 0 if not ..
	 	 * The module MUST queue a module unload event
	 	 */
		if ((*mod_delay)() == 1)
		{
			/* Return */
			return 2;
		}
	}
	 
	
	/* Call unload */
	(*Modules[i]->unload)();
	
	irc_dlclose(Modules[i]->dll);
	
	Modules[i] = NULL;
	ModuleFlags[i] = MODFLAG_NONE;
	modules_loaded--;
	return 1;
#endif
}

vFP module_sym(char *name)
{
#ifndef STATIC_LINKING
	vFP	fp;
	char	buf[512];
	int	i;
	ModuleInfo *mi;
	
	if (!name)
		return NULL;
	
	ircsprintf(buf, "_%s", name);

	/* Run through all modules and check for symbols */
	for (i = 0; i < MAXMODULES; i++)
	{
		mi = Modules[i];
		if (!mi)
			continue;
			
		if (fp = (vFP) irc_dlsym(mi->dll, name))
			return (fp);
		if (fp = (vFP) irc_dlsym(mi->dll, buf))
			return (fp);
	}
	return NULL;
#endif
}


void	module_loadall(int module_load)
{
#ifndef STATIC_LINKING
	iFP	fp;
	int	i;
	ModuleInfo *mi;
	
	if (!loop.ircd_booted)
	{
		sendto_realops("Ehh, !loop.ircd_booted in module_loadall()");
		return ;
	}
	/* Run through all modules and check for module load */
	for (i = 0; i < MAXMODULES; i++)
	{
		mi = Modules[i];
		if (!mi)
			continue;
		if (ModuleFlags[i] & MODFLAG_LOADED)
			continue;
		if (fp = (iFP) irc_dlsym(mi->dll, "mod_load"))
		{
		}
		else
		if (fp = (iFP) irc_dlsym(mi->dll, "_mod_load"))
		{
		}
		else
		{
			/* else, we didn't find it */
			continue;
		}
		/* Call the module_load */
		if ((*fp)(module_load) < 0)
		{
			config_error("cannot load module %s", mi->name);
		}
		else
		{
			ModuleFlags[i] |= MODFLAG_LOADED;
		}
		
	}
#endif
}

int	module_depend_resolve(MSymbolTable *dep)
{
	MSymbolTable *d = dep;
#ifndef STATIC_LINKING
	while (d->pointer)
	{
		*(d->pointer) = module_sym(d->symbol);
		if (!*(d->pointer))
		{

/*			config_progress("module dependancy error: cannot resolve symbol %s",
				d->symbol);*/
			config_progress("Unable to resolve symbol %s, attempting to load %s to find it",
d->symbol, d->module);
			load_module(d->module,0);
			*(d->pointer) = module_sym(d->symbol);
			if (!*(d->pointer)) {
				config_progress("module dependancy error: cannot resolve symbol %s",
					d->symbol);
				return -1;
			}
		}	
		d++;	
	}
	return 0;
#else
	while (d->pointer)
	{
		*((vFP *)d->pointer) = (vFP) d->realfunc;
		d++;
	}
#endif
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
			    me.name, parv[0], "MODULE LOAD");
			return 0;
		}
		sendto_realops("Trying to load module %s", parv[2]);
		if (load_module(parv[2],1) == 1)
			sendto_realops("Loaded module %s", parv[2]);
	}
	else
	if (!match(parv[1], "unload"))
	{
		if (parc < 3)
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			    me.name, parv[0], "MODULE UNLOAD");
			return 0;
		}
		sendto_realops("Trying to unload module %s", parv[2]);
		i = unload_module(parv[2]);
		{
			if (i == 1)
				sendto_realops("Unloaded module %s", parv[2]);
			else if (i == 2)
				sendto_realops("Delaying module unload of %s",
					parv[2]);
			else if (i == -1)
				sendto_realops("Couldn't unload module %s",	
					parv[2]);
		}
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

void	HookAddEx(int hooktype, int (*func)(), void (*vfunc)())
{
	Hook *p;
	
	p = (Hook *) MyMallocEx(sizeof(Hook));
	if (func)
		p->func.intfunc = func;
	if (vfunc)
		p->func.voidfunc = vfunc;
	add_ConfigItem((ConfigItem *) p, (ConfigItem **) &Hooks[hooktype]);
}

void	HookDelEx(int hooktype, int (*func)(), void (*vfunc)())
{
	Hook *p;
	
	for (p = Hooks[hooktype]; p; p = p->next)
		if ((func && (p->func.intfunc == func)) || 
			(vfunc && (p->func.voidfunc == vfunc)))
		{
			del_ConfigItem((ConfigItem *) p, (ConfigItem **) &Hooks[hooktype]);
			return;
		}
}

EVENT(e_unload_module_delayed)
{
	char	*name = (char *) data;
	int	i; 
	sendto_realops("Delayed unload of module %s in progress",
		name);
	
	i = unload_module(name);
	if (i == 2)
	{
		sendto_realops("Delayed unload of %s, again",
			name);
	}
	if (i == -1)
	{
		sendto_realops("Failed to unload '%s'", name);
	}
	if (i == 1)
	{
		sendto_realops("Unloaded module %s", name);
	}
	return;
}
