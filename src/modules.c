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

Hook	   	*Hooks[MAXHOOKTYPES];
Hook 	   	*global_i = NULL;
Module          *Modules = NULL;

Module *Module_make(ModuleHeader *header, 
#ifdef _WIN32
       HMODULE mod
#else
       void *mod
#endif
       );

void Module_Init(void)
{
	bzero(Hooks, sizeof(Hooks));
}

Module *Module_Find(char *name)
{
	Module *p;
	
	for (p = Modules; p; p = p->next)
	{
		if (!strcmp(p->header->name, name))
		{
			return (p);
		}
	}
	return NULL;
	
}

/*
 * Returns an error if insucessful .. yes NULL is OK! 
*/
char  *Module_Load (char *path, int load)
{
#ifndef STATIC_LINKING
#ifdef _WIN32
	HMODULE 	Mod;
#else /* _WIN32 */
	void   		*Mod;
#endif /* _WIN32 */
	int		(*Mod_Init)();
	int             (*Mod_Load)();
	int             (*Mod_Unload)();
	static char 	errorbuf[1024];
	ModuleHeader    *mod_header;
	int		ret = 0;
	Module          *mod = NULL;
	Debug((DEBUG_DEBUG, "Attempting to load module from %s",
	       path));
	if (Mod = irc_dlopen(path, RTLD_NOW))
	{
		/* We have engaged the borg cube. Scan for lifesigns. */
		if (!(mod_header = irc_dlsym(Mod, "Mod_Header")))
		{
			irc_dlclose(Mod);
			return ("Unable to locate Mod_Header");
		}
		if (!mod_header->modversion)
		{
			irc_dlclose(Mod);
			return ("Lacking mod_header->modversion");
		}
		if (match(MOD_WE_SUPPORT, mod_header->modversion))
		{
			ircsprintf(errorbuf, "Unsupported version, we support %s, %s is %s",
				   MOD_WE_SUPPORT, path, mod_header->modversion);
			irc_dlclose(Mod);
			return(errorbuf);
		}
		if (!mod_header->name || !mod_header->version ||
		    !mod_header->description)
		{
			irc_dlclose(Mod);
			return("Lacking sane header pointer");
		}
		if (Module_Find(mod_header->name))
		{
		        irc_dlclose(Mod);
			return ("Module already loaded");
		}
		mod = (Module *)Module_make(mod_header, Mod);
		if (!(Mod_Init = irc_dlsym(Mod, "Mod_Init")))
		{
			Module_free(mod);
			return ("Unable to locate Mod_Init");
		}
		if (!(Mod_Unload = irc_dlsym(Mod, "Mod_Unload")))
		{
			Module_free(mod);
			return ("Unable to locate Mod_Unload");
		}
		if ((ret = (*Mod_Init)(load)) < MOD_SUCCESS)
		{
			ircsprintf(errorbuf, "Mod_Init returned %i",
				   ret);
			/* We EXPECT the module to have cleaned up it's mess */
		        Module_free(mod);
			return (errorbuf);
		}
		
		if (load)
		{
			if (!(Mod_Load = irc_dlsym(Mod, "Mod_Load")))
			{
				/* We cannot do delayed unloading if this happens */
				(*Mod_Unload)();
				Module_free(mod);
				return ("Unable to locate Mod_Load"); 
			}
			if ((ret = (*Mod_Load)(load)) < MOD_SUCCESS)
			{
				ircsprintf(errorbuf, "Mod_Load returned %i",
					  ret);
				(*Mod_Unload)();
				Module_free(mod);
				return (errorbuf);
			}
			mod->flags |= MODFLAG_LOADED;
		}
		AddListItem(mod, Modules);
		return NULL;
	}
	else
	{
		/* Return the error .. */
		return (irc_dlerror());
	}
					     
	
#else /* !STATIC_LINKING */
	return "We don't support dynamic linking";
#endif
	
}

Module *Module_make(ModuleHeader *header, 
#ifdef _WIN32
       HMODULE mod
#else
       void *mod
#endif
       )
{
	Module *modp = NULL;
	
	modp = (Module *)MyMallocEx(sizeof(Module));
	modp->header = header;
	modp->dll = mod;
	modp->flags = MODFLAG_NONE;
	modp->children = NULL;
	return (modp);
}

/* 
 * Returns -1 if you cannot unload due to children still alive 
 * Returns 1 if successful 
 */
int    Module_free(Module *mod)
{
	Module *p;
	ModuleChild *cp;
	/* Do not kill parent if children still alive */

	if (mod->children)
        {
		for (cp = mod->children; cp; cp = cp->next)
		{
			sendto_realops("Unloading child module %s",
				      cp->child->header->name);
			Module_Unload(cp->child->header->name, 0);
		}
	}
	for (p = Modules; p; p = p->next)
	{
		for (cp = p->children; cp; cp = cp->next)
		{
			if (cp->child == mod)
			{
				DelListItem(mod, p->children);
				/* We can assume there can be only one. */
				break;
			}
		}
	}
	DelListItem(mod, Modules);
	irc_dlclose(mod->dll);
	MyFree(mod);
	return 1;
}

/*
 *  Module_Unload ()
 *     char *name        Internal module name
 *     int unload        If /module unload
 *  Returns:
 *     -1                Not able to locate module, severe failure, anything
 *      1                Module unloaded
 *      2                Module wishes delayed unloading, has placed event
 */
int     Module_Unload(char *name, int unload)
{
	Module *m;
	int    (*Mod_Unload)();
	int    ret;
	for (m = Modules; m; m = m->next)
	{
		if (!strcmp(m->header->name, name))
		{
		       break;
		}
	}      
	if (!m)
		return -1;
	if (!(Mod_Unload = irc_dlsym(m->dll, "Mod_Unload")))
	{
		return -1;
	}
	ret = (*Mod_Unload)(unload);
	if (ret == MOD_DELAY)
	{
		return 2;
	}
	if (ret == MOD_FAILED)
	{
		return -1;
	}
	/* No more pain detected, let's unload */
	DelListItem(m, Modules);
	Module_free(m);
	return 1;
}


vFP Module_Sym(char *name)
{
#ifndef STATIC_LINKING
	vFP	fp;
	char	buf[512];
	int	i;
	Module *mi;
	
	if (!name)
		return NULL;
	
	ircsprintf(buf, "_%s", name);

	/* Run through all modules and check for symbols */
	for (mi = Modules; mi; mi = mi->next)
	{
		if (fp = (vFP) irc_dlsym(mi->dll, name))
			return (fp);
		if (fp = (vFP) irc_dlsym(mi->dll, buf))
			return (fp);
	}
	return NULL;
#endif
}

vFP Module_SymX(char *name, Module **mptr)
{
#ifndef STATIC_LINKING
	vFP	fp;
	char	buf[512];
	int	i;
	Module *mi;
	
	if (!name)
		return NULL;
	
	ircsprintf(buf, "_%s", name);

	/* Run through all modules and check for symbols */
	for (mi = Modules; mi; mi = mi->next)
	{
		if (fp = (vFP) irc_dlsym(mi->dll, name))
		{
			*mptr = mi;
			return (fp);
		}
		if (fp = (vFP) irc_dlsym(mi->dll, buf))
		{
			*mptr = mi;
			return (fp);
		}
	}
	*mptr = NULL;
	return NULL;
#endif
}




void	module_loadall(int module_load)
{
#ifndef STATIC_LINKING
	iFP	fp;
	int	i;
	Module *mi;
	
	if (!loop.ircd_booted)
	{
		sendto_realops("Ehh, !loop.ircd_booted in module_loadall()");
		return ;
	}
	/* Run through all modules and check for module load */
	for (mi = Modules; mi; mi = mi->next)
	{
		if (mi->flags & MODFLAG_LOADED)
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
			config_error("cannot load module %s", mi->header->name);
		}
		else
		{
			mi->flags |= MODFLAG_LOADED;
		}
		
	}
#endif
}

inline int	Module_IsAlreadyChild(Module *parent, Module *child)
{
	ModuleChild *mcp;
	
	for (mcp = parent->children; mcp; mcp = mcp->next)
	{
		if (mcp->child == child) 
			return 1;
	}
	return 0;
}

inline void	Module_AddAsChild(Module *parent, Module *child)
{
	ModuleChild	*childp = NULL;
	
	childp = (ModuleChild *) MyMallocEx(sizeof(ModuleChild));
	childp->child = child;
	AddListItem(childp, parent->children);
}

int	Module_Depend_Resolve(Module *p)
{
	Mod_SymbolDepTable *d = p->header->symdep;
	Module		   *parental = NULL;
	
	if (d == NULL)
	{
		return 0;
	}
#ifndef STATIC_LINKING
	while (d->pointer)
	{
		*(d->pointer) = Module_SymX(d->symbol, &parental);
		if (!*(d->pointer))
		{
			config_progress("Unable to resolve symbol %s, attempting to load %s to find it", d->symbol, d->module);
			Module_Load(d->module,0);
			*(d->pointer) = Module_SymX(d->symbol, &parental);
			if (!*(d->pointer)) {
				config_progress("module dependancy error: cannot resolve symbol %s",
					d->symbol);
				return -1;
			}
			
		}
		if (!Module_IsAlreadyChild(parental, p))
			Module_AddAsChild(parental, p);
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
	int		i;
	char 		*ret;
	Module          *mi;
	
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
		if (!(ret = Module_Load(parv[2], 1)))
		{
			sendto_realops("Loaded module %s", parv[2]);
			return;
		}
		else
		{
			sendto_realops("Module load of %s failed: %s",
				parv[2], ret);
		}
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
		i = Module_Unload(parv[2], 0);
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
		if (!Modules)
		{
			sendto_one(sptr, ":%s NOTICE %s :*** No modules loaded", me.name, sptr->name);
			return 1;
		}
		for (mi = Modules; mi; mi = mi->next)
		{
			sendto_one(sptr, ":%s NOTICE %s :*** %s - %s (%s)", me.name, sptr->name,
				mi->header->name, mi->header->version, mi->header->description);	
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
	AddListItem(p, Hooks[hooktype]);
}

void	HookDelEx(int hooktype, int (*func)(), void (*vfunc)())
{
	Hook *p;
	
	for (p = Hooks[hooktype]; p; p = p->next)
		if ((func && (p->func.intfunc == func)) || 
			(vfunc && (p->func.voidfunc == vfunc)))
		{
			DelListItem(p, Hooks[hooktype]);
			return;
		}
}

EVENT(e_unload_module_delayed)
{
	char	*name = (char *) data;
	int	i; 
	sendto_realops("Delayed unload of module %s in progress",
		name);
	
	i = Module_Unload(name, 0);
	if (i == 2)
	{
		sendto_realops("Delayed unload of %s, again",
			name);
	}
        if (i == -2)
	{
		
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

void	unload_all_modules(void)
{
	Module *m;
	int	(*Mod_Unload)();
	for (m = Modules; m; m = m->next)
	{
		Mod_Unload = irc_dlsym(m->dll, "Mod_Unload");
		if (Mod_Unload)
			(*Mod_Unload)(0);
	}
}
