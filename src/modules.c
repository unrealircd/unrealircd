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
#elif defined(HPUX)
#include <dl.h>
#define RTLD_NOW BIND_IMMEDIATE
#else
#include <dlfcn.h>
#endif
#include <fcntl.h>
#ifndef _WIN32
#include <dirent.h>
#endif
#include "h.h"
#include "proto.h"
#ifndef RTLD_NOW
#define RTLD_NOW RTLD_LAZY
#endif

Hook	   	*Hooks[MAXHOOKTYPES];
Hooktype	Hooktypes[MAXCUSTOMHOOKS];
Module          *Modules = NULL;
Versionflag     *Versionflags = NULL;
int     Module_Depend_Resolve(Module *p);
Module *Module_make(ModuleHeader *header, 
#ifdef _WIN32
       HMODULE mod
#else
       void *mod
#endif
       );
#ifdef UNDERSCORE
void *obsd_dlsym(void *handle, char *symbol) {
    char *obsdsymbol = (char*)MyMalloc(strlen(symbol) + 2);
    void *symaddr = NULL;

    if (obsdsymbol) {
       sprintf(obsdsymbol, "_%s", symbol);
       symaddr = dlsym(handle, obsdsymbol);
       free(obsdsymbol);
    }

    return symaddr;
}
#endif


void DeleteTempModules(void)
{
#ifndef _WIN32
	DIR *fd = opendir("tmp");
	struct dirent *dir;
	char tempbuf[PATH_MAX+1];

	if (!fd) /* Ouch.. this is NOT good!! */
	{
		config_error("Unable to open 'tmp' directory: %s, please create one with the appropriate permissions",
			strerror(ERRNO));
		if (!loop.ircd_booted)
			exit(7);
		return; 
	}

	while ((dir = readdir(fd)))
	{
		if (!match("*.so", dir->d_name))
		{
			strcpy(tempbuf, "tmp/");
			strcat(tempbuf, dir->d_name);
			remove(tempbuf);
		}
	}
	closedir(fd);
#endif
}

void Module_Init(void)
{
	bzero(Hooks, sizeof(Hooks));
	bzero(Hooktypes, sizeof(Hooktypes));
}

Module *Module_Find(char *name)
{
	Module *p;
	
	for (p = Modules; p; p = p->next)
	{
		if (!(p->options & MOD_OPT_PERM) &&
		    (!(p->flags & MODFLAG_TESTING) || (p->flags & MODFLAG_DELAYED)))
			continue;
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
char  *Module_Create(char *path_)
{
#ifndef STATIC_LINKING
#ifdef _WIN32
	HMODULE 	Mod;
#else /* _WIN32 */
	void   		*Mod;
#endif /* _WIN32 */
	int		(*Mod_Test)();
	int		(*Mod_Init)();
	int             (*Mod_Load)();
	int             (*Mod_Unload)();
	static char 	errorbuf[1024];
	char 		*path, *tmppath;
	ModuleHeader    *mod_header;
	int		ret = 0;
	Module          *mod = NULL, **Mod_Handle = NULL;
	int *x;
	int betaversion,tag;
	Debug((DEBUG_DEBUG, "Attempting to load module from %s",
	       path_));
	path = path_;

	
	tmppath = unreal_mktemp("tmp", unreal_getfilename(path));
	if (!tmppath)
		return "Unable to create temporarely file!";
	if(!strchr(path, '/'))
	{
		path = MyMalloc(strlen(path) + 3);
		strcpy(path, "./");
		strcat(path, path_);
	}
	unreal_copyfile(path, tmppath);
	if ((Mod = irc_dlopen(tmppath, RTLD_NOW)))
	{
		/* We have engaged the borg cube. Scan for lifesigns. */
		irc_dlsym(Mod, "Mod_Header", mod_header);
		if (!mod_header)
		{
			irc_dlclose(Mod);
			remove(tmppath);
			return ("Unable to locate Mod_Header");
		}
		if (!mod_header->modversion)
		{
			irc_dlclose(Mod);
			remove(tmppath);
			return ("Lacking mod_header->modversion");
		}
		if (sscanf(mod_header->modversion, "3.2-b%d-%d", &betaversion, &tag)) {
			if (betaversion < 5 || betaversion >8) {
				snprintf(errorbuf, 1023, "Unsupported version, we support %s, %s is %s",
					   MOD_WE_SUPPORT, path, mod_header->modversion);
				irc_dlclose(Mod);
				remove(tmppath);
				return(errorbuf);
			}
		}
		if (!mod_header->name || !mod_header->version ||
		    !mod_header->description)
		{
			irc_dlclose(Mod);
			remove(tmppath);
			return("Lacking sane header pointer");
		}
		if (Module_Find(mod_header->name))
		{
		        irc_dlclose(Mod);
			remove(tmppath);
			return (NULL);
		}
		mod = (Module *)Module_make(mod_header, Mod);
		mod->tmp_file = strdup(tmppath);
		irc_dlsym(Mod, "Mod_Init", Mod_Init);
		if (!Mod_Init)
		{
			Module_free(mod);
			return ("Unable to locate Mod_Init");
		}
		irc_dlsym(Mod, "Mod_Unload", Mod_Unload);
		if (!Mod_Unload)
		{
			Module_free(mod);
			return ("Unable to locate Mod_Unload");
		}
		irc_dlsym(Mod, "Mod_Load", Mod_Load);
		if (!Mod_Load)
		{
			Module_free(mod);
			return ("Unable to locate Mod_Load"); 
		}
		if (Module_Depend_Resolve(mod) == -1)
		{
			Module_free(mod);
			return ("Dependancy problem");
		}
		irc_dlsym(Mod, "Mod_Handle", Mod_Handle);
		if (Mod_Handle)
			*Mod_Handle = mod;
		irc_dlsym(Mod, "Mod_Test", Mod_Test);
		if (Mod_Test)
		{
			if (betaversion >= 8) {
				if ((ret = (*Mod_Test)(&mod->modinfo)) < MOD_SUCCESS) {
					ircsprintf(errorbuf, "Mod_Test returned %i",
						   ret);
					/* We EXPECT the module to have cleaned up it's mess */
		        		Module_free(mod);
					return (errorbuf);
				}
			}
			else {
				if ((ret = (*Mod_Test)(0)) < MOD_SUCCESS)
				{
					snprintf(errorbuf, 1023, "Mod_Test returned %i",
						   ret);
					/* We EXPECT the module to have cleaned up it's mess */
				        Module_free(mod);
					return (errorbuf);
				}
			}
		}
		mod->flags = MODFLAG_TESTING;		
		AddListItem(mod, Modules);
		return NULL;
	}
	else
	{
		/* Return the error .. */
		return ((char *)irc_dlerror());
	}
	
	if (path != path_)
		free(path);				     
	
#else /* !STATIC_LINKING */
	return "We don't support dynamic linking";
#endif
	
}

void Module_DelayChildren(Module *m)
{
	ModuleChild *c;
	for (c = m->children; c; c = c->next)
	{
		c->child->flags |= MODFLAG_DELAYED;
		Module_DelayChildren(c->child);
	}
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
	modp->options = 0;
	modp->errorcode = MODERR_NOERROR;
	modp->children = NULL;
	modp->modinfo.size = sizeof(ModuleInfo);
	modp->modinfo.module_load = 0;
	modp->modinfo.handle = modp;
	return (modp);
}

void Init_all_testing_modules(void)
{
	
	Module *mi, *next;
	int betaversion, tag, ret;
	iFP Mod_Init;
	for (mi = Modules; mi; mi = next)
	{
		next = mi->next;
		if (!(mi->flags & MODFLAG_TESTING))
			continue;
		irc_dlsym(mi->dll, "Mod_Init", Mod_Init);
		sscanf(mi->header->modversion, "3.2-b%d-%d", &betaversion, &tag);
		if (betaversion >= 8) {
			if ((ret = (*Mod_Init)(&mi->modinfo)) < MOD_SUCCESS) {
				config_error("Error loading %s: Mod_Init returned %i",
					mi->header->name, ret);
		        	Module_free(mi);
				continue;
			}
		}
		else {
			if ((ret = (*Mod_Init)(0)) < MOD_SUCCESS)
			{
				config_error("Error loading %s: Mod_Init returned %i",
					mi->header->name, ret);
			        Module_free(mi);
				continue;
			}
		}		
		mi->flags = MODFLAG_INIT;
	}
}	

void Unload_all_loaded_modules(void)
{
	Module *mi, *next;
	ModuleChild *child, *childnext;
	ModuleObject *objs, *objnext;
	iFP Mod_Unload;
	int ret;

	for (mi = Modules; mi; mi = next)
	{
		next = mi->next;
		if (!(mi->flags & MODFLAG_LOADED) || (mi->flags & MODFLAG_DELAYED) || (mi->options & MOD_OPT_PERM))
			continue;
		irc_dlsym(mi->dll, "Mod_Unload", Mod_Unload);
		if (Mod_Unload)
		{
			ret = (*Mod_Unload)(0);
			if (ret == MOD_DELAY)
			{
				mi->flags |= MODFLAG_DELAYED;
				Module_DelayChildren(mi);
			}
		}
		for (objs = mi->objects; objs; objs = objnext) {
			objnext = objs->next;
			if (objs->type == MOBJ_EVENT) {
				LockEventSystem();
				EventDel(objs->object.event);
				UnlockEventSystem();
			}
			else if (objs->type == MOBJ_HOOK) {
				HookDel(objs->object.hook);
			}
			else if (objs->type == MOBJ_COMMAND) {
				CommandDel(objs->object.command);
			}
			else if (objs->type == MOBJ_HOOKTYPE) {
				HooktypeDel(objs->object.hooktype, mi);
			}
			else if (objs->type == MOBJ_VERSIONFLAG) {
				VersionflagDel(objs->object.versionflag, mi);
			}
			else if (objs->type == MOBJ_SNOMASK) {
				SnomaskDel(objs->object.snomask);
			}
			else if (objs->type == MOBJ_UMODE) {
				UmodeDel(objs->object.umode);
			}
			else if (objs->type == MOBJ_CMDOVERRIDE) {
				CmdoverrideDel(objs->object.cmdoverride);
			}
		}
		for (child = mi->children; child; child = childnext)
		{
			childnext = child->next;
			DelListItem(child,mi->children);
			MyFree(child);
		}
		DelListItem(mi,Modules);
		irc_dlclose(mi->dll);
		remove(mi->tmp_file);
		MyFree(mi->tmp_file);
		MyFree(mi);
	}
}

void Unload_all_testing_modules(void)
{
	Module *mi, *next;
	ModuleChild *child, *childnext;
	ModuleObject *objs, *objnext;

	for (mi = Modules; mi; mi = next)
	{
		next = mi->next;
		if (!(mi->flags & MODFLAG_TESTING))
			continue;
		for (objs = mi->objects; objs; objs = objnext) {
			objnext = objs->next;
			if (objs->type == MOBJ_EVENT) {
				LockEventSystem();
				EventDel(objs->object.event);
				UnlockEventSystem();
			}
			else if (objs->type == MOBJ_HOOK) {
				HookDel(objs->object.hook);
			}
			else if (objs->type == MOBJ_COMMAND) {
				CommandDel(objs->object.command);
			}
			else if (objs->type == MOBJ_HOOKTYPE) {
				HooktypeDel(objs->object.hooktype, mi);
			}
			else if (objs->type == MOBJ_VERSIONFLAG) {
				VersionflagDel(objs->object.versionflag, mi);
			}
			else if (objs->type == MOBJ_SNOMASK) {
				SnomaskDel(objs->object.snomask);
			}
			else if (objs->type == MOBJ_UMODE) {
				UmodeDel(objs->object.umode);
			}
			else if (objs->type == MOBJ_CMDOVERRIDE) {
				CmdoverrideDel(objs->object.cmdoverride);
			}
		}
		for (child = mi->children; child; child = childnext)
		{
			childnext = child->next;
			DelListItem(child,mi->children);
			MyFree(child);
		}
		DelListItem(mi,Modules);
		irc_dlclose(mi->dll);
		remove(mi->tmp_file);
		MyFree(mi->tmp_file);
		MyFree(mi);
	}
}

/* 
 * Returns -1 if you cannot unload due to children still alive 
 * Returns 1 if successful 
 */
int    Module_free(Module *mod)
{
	Module *p;
	ModuleChild *cp, *cpnext;
	ModuleObject *objs, *next;
	/* Do not kill parent if children still alive */

	for (cp = mod->children; cp; cp = cp->next)
	{
		sendto_realops("Unloading child module %s",
			      cp->child->header->name);
		Module_Unload(cp->child->header->name, 0);
	}
	for (objs = mod->objects; objs; objs = next) {
		next = objs->next;
		if (objs->type == MOBJ_EVENT) {
			LockEventSystem();
			EventDel(objs->object.event);
			UnlockEventSystem();
		}
		else if (objs->type == MOBJ_HOOK) {
			HookDel(objs->object.hook);
		}
		else if (objs->type == MOBJ_COMMAND) {
			CommandDel(objs->object.command);
		}
		else if (objs->type == MOBJ_HOOKTYPE) {
			HooktypeDel(objs->object.hooktype, mod);
		}
		else if (objs->type == MOBJ_VERSIONFLAG) {
			VersionflagDel(objs->object.versionflag, mod);
		}
		else if (objs->type == MOBJ_SNOMASK) {
			SnomaskDel(objs->object.snomask);
		}
		else if (objs->type == MOBJ_UMODE) {
			UmodeDel(objs->object.umode);
		}
		else if (objs->type == MOBJ_CMDOVERRIDE) {
			CmdoverrideDel(objs->object.cmdoverride);
		}
	}
	for (p = Modules; p; p = p->next)
	{
		for (cp = p->children; cp; cp = cpnext)
		{
			cpnext = cp->next;
			if (cp->child == mod)
			{
				DelListItem(mod, p->children);
				MyFree(cp);
				/* We can assume there can be only one. */
				break;
			}
		}
	}
	DelListItem(mod, Modules);
	irc_dlclose(mod->dll);
	MyFree(mod->tmp_file);
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
	irc_dlsym(m->dll, "Mod_Unload", Mod_Unload);
	if (!Mod_Unload)
	{
		return -1;
	}
	ret = (*Mod_Unload)(unload);
	if (ret == MOD_DELAY)
	{
		m->flags |= MODFLAG_DELAYED;
		Module_DelayChildren(m);
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

vFP Module_SymEx(
#ifdef _WIN32
	HMODULE mod
#else
	void *mod
#endif
	, char *name)
{
#ifndef STATIC_LINKING
	vFP	fp;

	if (!name)
		return NULL;
	
	irc_dlsym(mod, name, fp);
	if (fp)
		return (fp);
	return NULL;
#endif
	
}

vFP Module_Sym(char *name)
{
#ifndef STATIC_LINKING
	vFP	fp;
	Module *mi;
	
	if (!name)
		return NULL;

	/* Run through all modules and check for symbols */
	for (mi = Modules; mi; mi = mi->next)
	{
		if (!(mi->flags & MODFLAG_TESTING) || (mi->flags & MODFLAG_DELAYED))
			continue;
		irc_dlsym(mi->dll, name, fp);
		if (fp)
			return (fp);
	}
	return NULL;
#endif
}

vFP Module_SymX(char *name, Module **mptr)
{
#ifndef STATIC_LINKING
	vFP	fp;
	Module *mi;
	
	if (!name)
		return NULL;
	
	/* Run through all modules and check for symbols */
	for (mi = Modules; mi; mi = mi->next)
	{
		if (!(mi->flags & MODFLAG_TESTING) || (mi->flags & MODFLAG_DELAYED))
			continue;
		irc_dlsym(mi->dll, name, fp);
		if (fp)
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
	iFP	fp, fpp;
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
		irc_dlsym(mi->dll, "Mod_Load", fp);
		irc_dlsym(mi->dll, "_Mod_Load", fpp);
		if (fp);
		else if (fpp) { fp = fpp; }
		else
		{
			/* else, we didn't find it */
			continue;
		}
		/* Call the module_load */
		if ((*fp)(module_load) != MOD_SUCCESS)
		{
			config_status("cannot load module %s", mi->header->name);
		}
		else
		{
			mi->flags = MODFLAG_LOADED;
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
		if ((*(d->pointer) = Module_SymEx(p->dll, d->symbol)))
		{
			d++;
			continue;
		}
		*(d->pointer) = Module_SymX(d->symbol, &parental);
		if (!*(d->pointer))
		{
			config_progress("Unable to resolve symbol %s, attempting to load %s to find it", d->symbol, d->module);
			Module_Create(d->module);
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

/* m_module.
 * originally by ?? (codemastr?)
 * I (Syzop) changed it so it's now public for users too,
 * as quite some people (and users) requested they should have the
 * right to see what kind of weird modules are loaded on the server,
 * especially since people like to load spy modules these days.
 * I do not consider this sensitive information, but just in case
 * I stripped the version string for non-admins (eg: normal users).
 */
int  m_module(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	Module          *mi;
	int i;
	char tmp[1024], *p;
	aCommand *mptr;

	/* Opers can do /module <servername> */
	if ((parc > 1) && (IsServer(cptr) || IsOper(sptr)) &&
	    (hunt_server_token(cptr, sptr, MSG_MODULE, TOK_MODULE, ":%s", 1, parc, parv) != HUNTED_ISME))
		return 0;
	
	if (!Modules)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** No modules loaded", me.name, sptr->name);
		return 1;
	}
	for (mi = Modules; mi; mi = mi->next)
	{
		tmp[0] = '\0';
		if (mi->flags & MODFLAG_DELAYED)
			strcat(tmp, "[Unloading] ");
		if (mi->options & MOD_OPT_PERM)
			strcat(tmp, "[PERM] ");
		if (!(mi->options & MOD_OPT_OFFICIAL))
			strcat(tmp, "[3RD] ");
		if (!IsOper(sptr))
			sendto_one(sptr, ":%s NOTICE %s :*** %s (%s)%s", me.name, sptr->name,
				mi->header->name, mi->header->description,
				mi->options & MOD_OPT_OFFICIAL ? "" : " [3RD]");
		else
			sendto_one(sptr, ":%s NOTICE %s :*** %s - %s (%s) %s", me.name, sptr->name,
				mi->header->name, mi->header->version, mi->header->description, tmp);
	}

	if (!IsOper(sptr))
		return 0;

	tmp[0] = '\0';
	p = tmp;
	for (i=0; i < MAXHOOKTYPES; i++)
	{
		if (!Hooks[i])
			continue;
		sprintf(p, "%d ", i);
		p += strlen(p);
		if (p > tmp+480)
		{
			sendto_one(sptr, ":%s NOTICE %s :Hooks: %s", me.name, sptr->name, tmp);
			tmp[0] = '\0';
			p = tmp;
		}
	}
	sendto_one(sptr, ":%s NOTICE %s :Hooks: %s ", me.name, sptr->name, tmp);

	tmp[0] = '\0';
	p = tmp;
	for (i=0; i < 256; i++)
	{
		for (mptr = CommandHash[i]; mptr; mptr = mptr->next)
			if (mptr->overriders)
			{
				sprintf(p, "%s ", mptr->cmd);
				p += strlen(p);
				if (p > tmp+470)
				{
					sendto_one(sptr, ":%s NOTICE %s :Override: %s", me.name, sptr->name, tmp);
					tmp[0] = '\0';
					p = tmp;
				}
			}
	}
	sendto_one(sptr, ":%s NOTICE %s :Override: %s", me.name, sptr->name, tmp);
	
	return 1;
}

Hooktype *HooktypeFind(char *string) {
	Hooktype *hooktype;
	for (hooktype = Hooktypes; hooktype->string ;hooktype++) {
		if (!stricmp(hooktype->string, string))
			return hooktype;
	}
	return NULL;
}

Versionflag *VersionflagFind(char flag)
{
	Versionflag *vflag;
	for (vflag = Versionflags; vflag; vflag = vflag->next)
	{
		if (vflag->flag == flag)
			return vflag;
	}
	return NULL;
}

Versionflag *VersionflagAdd(Module *module, char flag)
{
	Versionflag *vflag;
	ModuleChild *parent;
	if ((vflag = VersionflagFind(flag)))
	{
		ModuleChild *child;
		for (child = vflag->parents; child; child = child->next) {
			if (child->child == module)
				break;
		}
		if (!child)
		{
			parent = MyMallocEx(sizeof(ModuleChild));
			parent->child = module;
			if (module) {
				ModuleObject *vflagobj;
				vflagobj = MyMallocEx(sizeof(ModuleObject));
				vflagobj->type = MOBJ_VERSIONFLAG;
				vflagobj->object.versionflag = vflag;
				AddListItem(vflagobj, module->objects);
				module->errorcode = MODERR_NOERROR;
			}
			AddListItem(parent,vflag->parents);
		}
		return vflag;
	}
	vflag = MyMallocEx(sizeof(Versionflag));
	vflag->flag = flag;
	parent = MyMallocEx(sizeof(ModuleChild));
	parent->child = module;
	if (module)
	{
		ModuleObject *vflagobj;
		vflagobj = MyMallocEx(sizeof(ModuleObject));
		vflagobj->type = MOBJ_VERSIONFLAG;
		vflagobj->object.versionflag = vflag;
		AddListItem(vflagobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	flag_add(flag);
	AddListItem(parent,vflag->parents);
	AddListItem(vflag, Versionflags);
	return vflag;
}
	
void VersionflagDel(Versionflag *vflag, Module *module)
{
	ModuleChild *owner;
	if (!vflag)
		return;

	for (owner = vflag->parents; owner; owner = owner->next)
	{
		if (owner->child == module)
		{
			DelListItem(owner,vflag->parents);
			MyFree(owner);
			break;
		}
	}
	if (module)
	{
		ModuleObject *objs;
		for (objs = module->objects; objs; objs = objs->next) {
			if (objs->type == MOBJ_VERSIONFLAG && objs->object.versionflag == vflag) {
				DelListItem(objs,module->objects);
				MyFree(objs);
				break;
			}
		}
	}
	if (!vflag->parents)
	{
		flag_del(vflag->flag);
		DelListItem(vflag, Versionflags);
		MyFree(vflag);
	}
}
		
Hooktype *HooktypeAdd(Module *module, char *string, int *type) {
	Hooktype *hooktype;
	int i;
	ModuleChild *parent;
	ModuleObject *hooktypeobj;
	if ((hooktype = HooktypeFind(string))) {
		ModuleChild *child;
		for (child = hooktype->parents; child; child = child->next) {
			if (child->child == module)
				break;
		}
		if (!child) {
			parent = MyMallocEx(sizeof(ModuleChild));
			parent->child = module;
			if (module) {
				hooktypeobj = MyMallocEx(sizeof(ModuleObject));
				hooktypeobj->type = MOBJ_HOOKTYPE;
				hooktypeobj->object.hooktype = hooktype;
				AddListItem(hooktypeobj, module->objects);
				module->errorcode = MODERR_NOERROR;
			}
			AddListItem(parent,hooktype->parents);
		}
		*type = hooktype->id;
		return hooktype;
	}
	for (hooktype = Hooktypes, i = 0; hooktype->string; hooktype++, i++) ;

	if (i >= 39)
	{
		if (module)
			module->errorcode = MODERR_NOSPACE;
		return NULL;
	}

	Hooktypes[i].id = i+41;
	Hooktypes[i].string = strdup(string);
	parent = MyMallocEx(sizeof(ModuleChild));
	parent->child = module;
	if (module) {
		hooktypeobj = MyMallocEx(sizeof(ModuleObject));
		hooktypeobj->type = MOBJ_HOOKTYPE;
		hooktypeobj->object.hooktype = &Hooktypes[i];
		AddListItem(hooktypeobj,module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	AddListItem(parent,Hooktypes[i].parents);
	*type = i+41;
	return &Hooktypes[i];
}

void HooktypeDel(Hooktype *hooktype, Module *module) {
	ModuleChild *child;
	ModuleObject *objs;
	for (child = hooktype->parents; child; child = child->next) {
		if (child->child == module) {
			DelListItem(child,hooktype->parents);
			MyFree(child);
			break;
		}
	}
	if (module) {
		for (objs = module->objects; objs; objs = objs->next) {
			if (objs->type == MOBJ_HOOKTYPE && objs->object.hooktype == hooktype) {
				DelListItem(objs,module->objects);
				MyFree(objs);
				break;
			}
		}
	}
	if (!hooktype->parents) {
		MyFree(hooktype->string);
		hooktype->string = NULL;
		hooktype->id = 0;
		hooktype->parents = NULL;
	}
}
		
	
Hook	*HookAddMain(Module *module, int hooktype, int (*func)(), void (*vfunc)(), char *(*cfunc)())
{
	Hook *p;
	
	p = (Hook *) MyMallocEx(sizeof(Hook));
	if (func)
		p->func.intfunc = func;
	if (vfunc)
		p->func.voidfunc = vfunc;
	if (cfunc)
		p->func.pcharfunc = cfunc;
	p->type = hooktype;
	p->owner = module;
	AddListItem(p, Hooks[hooktype]);
	if (module) {
		ModuleObject *hookobj = (ModuleObject *)MyMallocEx(sizeof(ModuleObject));
		hookobj->object.hook = p;
		hookobj->type = MOBJ_HOOK;
		AddListItem(hookobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	return p;
}

Hook *HookDel(Hook *hook)
{
	Hook *p, *q;
	for (p = Hooks[hook->type]; p; p = p->next) {
		if (p == hook) {
			q = p->next;
			DelListItem(p, Hooks[hook->type]);
			if (p->owner) {
				ModuleObject *hookobj;
				for (hookobj = p->owner->objects; hookobj; hookobj = hookobj->next) {
					if (hookobj->type == MOBJ_HOOK && hookobj->object.hook == p) {
						DelListItem(hookobj, hook->owner->objects);
						MyFree(hookobj);
						break;
					}
				}
			}
			MyFree(p);
			return q;
		}
	}
	return NULL;
}

Cmdoverride *CmdoverrideAdd(Module *module, char *name, iFP function)
{
	aCommand *p;
	Cmdoverride *ovr;
	
	if (!(p = find_Command_simple(name)))
	{
		if (module)
			module->errorcode = MODERR_NOTFOUND;
		return NULL;
	}
	ovr = MyMallocEx(sizeof(Cmdoverride));
	ovr->func = function;
	ovr->owner = module; /* TODO: module objects */
	if (module)
	{
		ModuleObject *cmdoverobj = MyMallocEx(sizeof(ModuleObject));
		cmdoverobj->type = MOBJ_CMDOVERRIDE;
		cmdoverobj->object.cmdoverride = ovr;
		AddListItem(cmdoverobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	ovr->command = p;
	if (!p->overriders)
		p->overridetail = ovr;
	AddListItem(ovr, p->overriders);
	if (p->friend)
	{
		if (!p->friend->overriders)
			p->friend->overridetail = ovr;
		AddListItem(ovr, p->friend->overriders);
	}
	return ovr;
}

void CmdoverrideDel(Cmdoverride *cmd)
{
	if (!cmd->next)
		cmd->command->overridetail = cmd->prev;
	DelListItem(cmd, cmd->command->overriders);
	if (!cmd->command->overriders)
		cmd->command->overridetail = NULL;
	if (cmd->command->friend)
	{
		if (!cmd->prev)
			cmd->command->friend->overridetail = NULL;
		DelListItem(cmd, cmd->command->friend->overriders);
	}
	if (cmd->owner)
	{
		ModuleObject *obj;
		for (obj = cmd->owner->objects; obj; obj = obj->next)
		{
			if (obj->type != MOBJ_CMDOVERRIDE)
				continue;
			if (obj->object.cmdoverride == cmd)
			{
				DelListItem(obj, cmd->owner->objects);
				MyFree(obj);
				break;
			}
		}
	}
	MyFree(cmd);
}

int CallCmdoverride(Cmdoverride *ovr, aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	if (ovr->prev)
		return ovr->prev->func(ovr->prev, cptr, sptr, parc, parv);
	return ovr->command->func(cptr, sptr, parc, parv);
}

EVENT(e_unload_module_delayed)
{
	char	*name = strdup(data);
	int	i; 
	i = Module_Unload(name, 0);
	if (i == -1)
	{
		sendto_realops("Failed to unload '%s'", name);
	}
	if (i == 1)
	{
		sendto_realops("Unloaded module %s", name);
	}
	free(name);
	return;
}

void	unload_all_modules(void)
{
	Module *m;
	int	(*Mod_Unload)();
	for (m = Modules; m; m = m->next)
	{
		irc_dlsym(m->dll, "Mod_Unload", Mod_Unload);
		if (Mod_Unload)
			(*Mod_Unload)(0);
		remove(m->tmp_file);
	}
}

unsigned int ModuleSetOptions(Module *module, unsigned int options)
{
	unsigned int oldopts = module->options;

	module->options = options;
	return oldopts;
}

unsigned int ModuleGetOptions(Module *module)
{
	return module->options;
}

unsigned int ModuleGetError(Module *module)
{
	return module->errorcode;
}

static const char *module_error_str[] = {
	"No error",
	"Object already exists",
	"No space available",
	"Invalid parameter(s)"
};

const char *ModuleGetErrorStr(Module *module)
{
	return module_error_str[module->errorcode];
}

