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

#define UNREALCORE
#include "unrealircd.h"
#ifdef _WIN32
#define RTLD_NOW 0
#elif defined(HPUX)
#include <dl.h>
#define RTLD_NOW BIND_IMMEDIATE
#else
#include <dlfcn.h>
#endif
#ifndef _WIN32
#include <dirent.h>
#endif
#ifndef RTLD_NOW
#define RTLD_NOW RTLD_LAZY
#endif
#include "modversion.h"

Hook	   	*Hooks[MAXHOOKTYPES];
Hooktype	Hooktypes[MAXCUSTOMHOOKS];
Callback	*Callbacks[MAXCALLBACKS];	/* Callback objects for modules, used for rehashing etc (can be multiple) */
Callback	*RCallbacks[MAXCALLBACKS];	/* 'Real' callback function, used for callback function calls */
MODVAR Module          *Modules = NULL;
MODVAR Versionflag     *Versionflags = NULL;

Module *Module_make(ModuleHeader *header, 
#ifdef _WIN32
       HMODULE mod
#else
       void *mod
#endif
       );

#ifdef UNDERSCORE
void *obsd_dlsym(void *handle, char *symbol) {
    size_t buflen = strlen(symbol) + 2;
    char *obsdsymbol = MyMallocEx(buflen);
    void *symaddr = NULL;

    if (obsdsymbol) {
       ircsnprintf(obsdsymbol, buflen, "_%s", symbol);
       symaddr = dlsym(handle, obsdsymbol);
       free(obsdsymbol);
    }

    return symaddr;
}
#endif

void deletetmp(char *path)
{
#ifndef NOREMOVETMP
	remove(path);
#endif
}

void DeleteTempModules(void)
{
	char tempbuf[PATH_MAX+1];
#ifndef _WIN32
	DIR *fd = opendir(TMPDIR);
	struct dirent *dir;

	if (!fd) /* Ouch.. this is NOT good!! */
	{
		config_error("Unable to open temp directory %s: %s, please create one with the appropriate permissions",
			TMPDIR, strerror(errno));
		if (!loop.ircd_booted)
			exit(7);
		return; 
	}

	while ((dir = readdir(fd)))
	{
		char *fname = dir->d_name;
		if (!strcmp(fname, ".") || !strcmp(fname, ".."))
			continue;
		if (!strstr(fname, ".so") && !strstr(fname, ".conf") && strstr(fname, "core"))
			continue; /* core dump */
		ircsnprintf(tempbuf, sizeof(tempbuf), "%s/%s", TMPDIR, fname);
		deletetmp(tempbuf);
	}
	closedir(fd);
#else
	WIN32_FIND_DATA hData;
	HANDLE hFile;
	
	snprintf(tempbuf, sizeof(tempbuf), "%s/*", TMPDIR);
	
	hFile = FindFirstFile(tempbuf, &hData);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		if (strcmp(hData.cFileName, ".") || strcmp(hData.cFileName, ".."))
		{
			ircsnprintf(tempbuf, sizeof(tempbuf), "%s/%s", TMPDIR, hData.cFileName);
			deletetmp(tempbuf);
		}
	}
	while (FindNextFile(hFile, &hData))
	{
		if (!strcmp(hData.cFileName, ".") || !strcmp(hData.cFileName, ".."))
			continue;
		ircsnprintf(tempbuf, sizeof(tempbuf), "%s/%s", TMPDIR, hData.cFileName);
		deletetmp(tempbuf);
	}
	FindClose(hFile);
#endif	
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

int parse_modsys_version(char *version)
{
	if (!strcmp(version, "unrealircd-5"))
		return 0x500000;
	return 0;
}

void make_compiler_string(char *buf, size_t buflen, unsigned int ver)
{
unsigned int maj, min, plevel;

	if (ver == 0)
	{
		strlcpy(buf, "0", buflen);
		return;
	}
	
	maj = ver >> 16;
	min = (ver >> 8) & 0xff;
	plevel = ver & 0xff;
	
	if (plevel == 0)
		snprintf(buf, buflen, "%d.%d", maj, min);
	else
		snprintf(buf, buflen, "%d.%d.%d", maj, min, plevel);
}

/** Transform a loadmodule path like "third/la" to
 * something like "/home/xyz/unrealircd/modules/third/la.so
 * (and other tricks)
 */
char *Module_TransformPath(char *path_)
{
	static char path[1024];

	/* Prefix the module path with MODULESDIR, unless it's an absolute path
	 * (we check for "/", "\" and things like "C:" to detect absolute paths).
	 */
	if ((*path_ != '/') && (*path_ != '\\') && !(*path_ && (path_[1] == ':')))
	{
		snprintf(path, sizeof(path), "%s/%s", MODULESDIR, path_);
	} else {
		strlcpy(path, path_, sizeof(path));
	}

	/* Auto-suffix .dll / .so */
	if (!strstr(path, MODULE_SUFFIX))
		strlcat(path, MODULE_SUFFIX, sizeof(path));

	return path;
}

/** This function is the inverse of Module_TransformPath() */
char *Module_GetRelPath(char *fullpath)
{
	static char buf[512];
	char prefix[512];
	char *s = fullpath;

	/* Strip the prefix */
	snprintf(prefix, sizeof(prefix), "%s/", MODULESDIR);
	if (!strncasecmp(fullpath, prefix, strlen(prefix)))
		s += strlen(prefix);
	strlcpy(buf, s, sizeof(buf));

	/* Strip the suffix */
	s = strstr(buf, MODULE_SUFFIX);
	if (s)
		*s = '\0';

	return buf;
}

/** Validate a modules' ModuleHeader.
 * @returns Error message is returned, or NULL if everything is OK.
 */
static char *validate_mod_header(ModuleHeader *mod_header)
{
	char *p;

	if (!mod_header->name || !mod_header->version || !mod_header->author || !mod_header->description)
		return "NULL values encountered in Mod_Header struct members";

	/* Validate module name */
	for (p = mod_header->name; *p; p++)
		if (!isalnum(*p) && !strchr("._-/", *p))
			return "ModuleHeader.name contains illegal characters (must be: a-zA-Z0-9._-/)";

	/* Validate version, even more strict */
	if (!isdigit(mod_header->version[0]))
		return "ModuleHeader.version must start with a digit";
	for (p = mod_header->version; *p; p++)
		if (!isalnum(*p) && !strchr("._-", *p))
			return "ModuleHeader.version contains illegal characters (must be: a-zA-Z0-9._-)";

	/* Author and description are not checked, has no constraints */

	return NULL; /* SUCCESS */
}

/*
 * Returns an error if insucessful .. yes NULL is OK! 
*/
char  *Module_Create(char *path_)
{
#ifdef _WIN32
	HMODULE 	Mod;
#else /* _WIN32 */
	void   		*Mod;
#endif /* _WIN32 */
	int		(*Mod_Test)();
	int		(*Mod_Init)();
	int             (*Mod_Load)();
	int             (*Mod_Unload)();
	char    *Mod_Version;
	unsigned int *compiler_version;
	static char 	errorbuf[1024];
	char 		*path, *tmppath;
	ModuleHeader    *mod_header = NULL;
	int		ret = 0;
	char		*reterr;
	Module          *mod = NULL, **Mod_Handle = NULL;
	char *expectedmodversion = our_mod_version;
	unsigned int expectedcompilerversion = our_compiler_version;
	long modsys_ver = 0;
	Debug((DEBUG_DEBUG, "Attempting to load module from %s", path_));

	path = Module_TransformPath(path_);

	tmppath = unreal_mktemp(TMPDIR, unreal_getmodfilename(path));
	if (!tmppath)
		return "Unable to create temporary file!";

	if (!file_exists(path))
	{
		snprintf(errorbuf, sizeof(errorbuf), "Cannot open module file: %s", strerror(errno));
		return errorbuf;
	}
	/* For OpenBSD, do not do a hardlinkink attempt first because it checks inode
	 * numbers to see if a certain module is already loaded. -- Syzop
	 * EDIT (2009): Looks like Linux got smart too, from now on we always copy....
	 */
	ret = unreal_copyfileex(path, tmppath, 0);
	if (!ret)
	{
		snprintf(errorbuf, sizeof(errorbuf), "Failed to copy module file.");
		return errorbuf;
	}
	if ((Mod = irc_dlopen(tmppath, RTLD_NOW)))
	{
		/* We have engaged the borg cube. Scan for lifesigns. */
		irc_dlsym(Mod, "Mod_Version", Mod_Version);
		if (Mod_Version && strcmp(Mod_Version, expectedmodversion))
		{
			snprintf(errorbuf, sizeof(errorbuf),
			         "Module was compiled for '%s', we were configured for '%s'. SOLUTION: Recompile the module(s).",
			         Mod_Version, expectedmodversion);
			irc_dlclose(Mod);
			deletetmp(tmppath);
			return errorbuf;
		}
		if (!Mod_Version)
		{
			snprintf(errorbuf, sizeof(errorbuf),
				"Module is lacking Mod_Version. Perhaps a very old one you forgot to recompile?");
			irc_dlclose(Mod);
			deletetmp(tmppath);
			return errorbuf;
		}
		irc_dlsym(Mod, "compiler_version", compiler_version);
		if (compiler_version && ( ((*compiler_version) & 0xffff00) != (expectedcompilerversion & 0xffff00) ) )
		{
			char theyhad[64], wehave[64];
			make_compiler_string(theyhad, sizeof(theyhad), *compiler_version);
			make_compiler_string(wehave, sizeof(wehave), expectedcompilerversion);
			snprintf(errorbuf, sizeof(errorbuf),
			         "Module was compiled with GCC %s, core was compiled with GCC %s. SOLUTION: Recompile your UnrealIRCd and all its modules by doing a 'make clean; ./Config -quick && make'.",
			         theyhad, wehave);
			irc_dlclose(Mod);
			deletetmp(tmppath);
			return errorbuf;
		}
		irc_dlsym(Mod, "Mod_Header", mod_header);
		if (!mod_header)
		{
			irc_dlclose(Mod);
			deletetmp(tmppath);
			return ("Unable to locate Mod_Header");
		}
		if (!mod_header->modversion)
		{
			irc_dlclose(Mod);
			deletetmp(tmppath);
			return ("Lacking mod_header->modversion");
		}
		if (!(modsys_ver = parse_modsys_version(mod_header->modversion)))
		{
			snprintf(errorbuf, 1023, "Unsupported module system version '%s'",
				   mod_header->modversion);
			irc_dlclose(Mod);
			deletetmp(tmppath);
			return(errorbuf);
		}
		if ((reterr = validate_mod_header(mod_header)))
		{
			irc_dlclose(Mod);
			deletetmp(tmppath);
			return(reterr);
		}
		if (Module_Find(mod_header->name))
		{
		        irc_dlclose(Mod);
			deletetmp(tmppath);
			return (NULL);
		}
		mod = (Module *)Module_make(mod_header, Mod);
		mod->tmp_file = strdup(tmppath);
		mod->mod_sys_version = modsys_ver;
		mod->compiler_version = compiler_version ? *compiler_version : 0;
		mod->relpath = strdup(Module_GetRelPath(path));

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
		irc_dlsym(Mod, "Mod_Handle", Mod_Handle);
		if (Mod_Handle)
			*Mod_Handle = mod;
		irc_dlsym(Mod, "Mod_Test", Mod_Test);
		if (Mod_Test)
		{
			if ((ret = (*Mod_Test)(&mod->modinfo)) < MOD_SUCCESS) {
				ircsnprintf(errorbuf, sizeof(errorbuf), "Mod_Test returned %i",
					   ret);
				/* We EXPECT the module to have cleaned up its mess */
				Module_free(mod);
				return (errorbuf);
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
	
	modp = MyMallocEx(sizeof(Module));
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
	int ret;
	iFP Mod_Init;
	for (mi = Modules; mi; mi = next)
	{
		next = mi->next;
		if (!(mi->flags & MODFLAG_TESTING))
			continue;
		irc_dlsym(mi->dll, "Mod_Init", Mod_Init);
		if ((ret = (*Mod_Init)(&mi->modinfo)) < MOD_SUCCESS) {
			config_error("Error loading %s: Mod_Init returned %i",
			    mi->header->name, ret);
			Module_free(mi);
			continue;
		}
		mi->flags = MODFLAG_INIT;
	}
}	

void FreeModObj(ModuleObject *obj, Module *m)
{
	if (obj->type == MOBJ_EVENT) {
		EventDel(obj->object.event);
	}
	else if (obj->type == MOBJ_HOOK) {
		HookDel(obj->object.hook);
	}
	else if (obj->type == MOBJ_COMMAND) {
		CommandDel(obj->object.command);
	}
	else if (obj->type == MOBJ_HOOKTYPE) {
		//HooktypeDel(obj->object.hooktype, m); -- reinstate if we audited this code
	}
	else if (obj->type == MOBJ_VERSIONFLAG) {
		VersionflagDel(obj->object.versionflag, m);
	}
	else if (obj->type == MOBJ_SNOMASK) {
		SnomaskDel(obj->object.snomask);
	}
	else if (obj->type == MOBJ_UMODE) {
		UmodeDel(obj->object.umode);
	}
	else if (obj->type == MOBJ_CMODE) {
		CmodeDel(obj->object.cmode);
	}
	else if (obj->type == MOBJ_COMMANDOVERRIDE) {
		CommandOverrideDel(obj->object.cmdoverride);
	}
	else if (obj->type == MOBJ_EXTBAN) {
		ExtbanDel(obj->object.extban);
	}
	else if (obj->type == MOBJ_CALLBACK) {
		CallbackDel(obj->object.callback);
	}
	else if (obj->type == MOBJ_EFUNCTION) {
		EfunctionDel(obj->object.efunction);
	}
	else if (obj->type == MOBJ_ISUPPORT) {
		IsupportDel(obj->object.isupport);
	}
	else if (obj->type == MOBJ_MODDATA) {
		ModDataDel(obj->object.moddata);
	}
	else if (obj->type == MOBJ_VALIDATOR) {
		OperClassValidatorDel(obj->object.validator);
	}
	else if (obj->type == MOBJ_CLICAP) {
		ClientCapabilityDel(obj->object.clicap);
	}
	else if (obj->type == MOBJ_MTAG) {
		MessageTagHandlerDel(obj->object.mtag);
	}
	else if (obj->type == MOBJ_HISTORY_BACKEND) {
		HistoryBackendDel(obj->object.history_backend);
	}
	else
	{
		ircd_log(LOG_ERROR, "FreeModObj() called for unknown object");
		abort();
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
			ret = (*Mod_Unload)(&mi->modinfo);
			if (ret == MOD_DELAY)
			{
				mi->flags |= MODFLAG_DELAYED;
				Module_DelayChildren(mi);
			}
		}
		for (objs = mi->objects; objs; objs = objnext) {
			objnext = objs->next;
			FreeModObj(objs, mi);
		}
		for (child = mi->children; child; child = childnext)
		{
			childnext = child->next;
			DelListItem(child,mi->children);
			MyFree(child);
		}
		DelListItem(mi,Modules);
		irc_dlclose(mi->dll);
		deletetmp(mi->tmp_file);
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
			FreeModObj(objs, mi);
		}
		for (child = mi->children; child; child = childnext)
		{
			childnext = child->next;
			DelListItem(child,mi->children);
			MyFree(child);
		}
		DelListItem(mi,Modules);
		irc_dlclose(mi->dll);
		deletetmp(mi->tmp_file);
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
		Module_Unload(cp->child->header->name);
	}
	for (objs = mod->objects; objs; objs = next) {
		next = objs->next;
		FreeModObj(objs, mod);
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
 *  Returns:
 *     -1                Not able to locate module, severe failure, anything
 *      1                Module unloaded
 *      2                Module wishes delayed unloading, has placed event
 */
int Module_Unload(char *name)
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
	ret = (*Mod_Unload)(m->modinfo);
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

void module_loadall(void)
{
	iFP	fp;
	Module *mi, *next;
	
	if (!loop.ircd_booted)
	{
		sendto_realops("Ehh, !loop.ircd_booted in module_loadall()");
		return ;
	}
	/* Run through all modules and check for module load */
	for (mi = Modules; mi; mi = next)
	{
		next = mi->next;
		if (mi->flags & MODFLAG_LOADED)
			continue;
		irc_dlsym(mi->dll, "Mod_Load", fp);
		/* Call the module_load */
		if ((*fp)(&mi->modinfo) != MOD_SUCCESS)
		{
			config_status("cannot load module %s", mi->header->name);
			Module_free(mi);
		}
		else
			mi->flags = MODFLAG_LOADED;
	}
}

int	Module_IsAlreadyChild(Module *parent, Module *child)
{
	ModuleChild *mcp;
	
	for (mcp = parent->children; mcp; mcp = mcp->next)
	{
		if (mcp->child == child) 
			return 1;
	}
	return 0;
}

void	Module_AddAsChild(Module *parent, Module *child)
{
	ModuleChild	*childp = NULL;
	
	childp = MyMallocEx(sizeof(ModuleChild));
	childp->child = child;
	AddListItem(childp, parent->children);
}

/* m_module.
 * by Stskeeps, codemastr, Syzop.
 * Changed it so it's now public for users too, as quite some people
 * (and users) requested they should have the right to see what kind 
 * of weird modules are loaded on the server, especially since people
 * like to load spy modules these days.
 * I do not consider this sensitive information, but just in case
 * I stripped the version string for non-admins (eg: normal users). -- Syzop
 */
CMD_FUNC(m_module)
{
	Module *mi;
	int i;
	char tmp[1024], *p;
	RealCommand *mptr;
	int all = 0;

	if ((parc > 1) && !strcmp(parv[1], "-all"))
		all = 1;

	if (MyClient(sptr) && !IsOper(sptr) && all)
		sptr->local->since += 7; /* Lag them up. Big list. */

	if ((parc > 2) && (hunt_server(cptr, sptr, recv_mtags, ":%s MODULE %s :%s", 2, parc, parv) != HUNTED_ISME))
		return 0;

	if ((parc == 2) && (parv[1][0] != '-') && (hunt_server(cptr, sptr, recv_mtags, ":%s MODULE :%s", 1, parc, parv) != HUNTED_ISME))
		return 0;

	if (all)
		sendtxtnumeric(sptr, "Showing ALL loaded modules:");
	else
		sendtxtnumeric(sptr, "Showing loaded 3rd party modules (use \"MODULE -all\" to show all modules):");

	for (mi = Modules; mi; mi = mi->next)
	{
		/* Skip official modules unless "MODULE -all" */
		if (!all && (mi->options & MOD_OPT_OFFICIAL))
			continue;

		tmp[0] = '\0';
		if (mi->flags & MODFLAG_DELAYED)
			strlcat(tmp, "[Unloading] ", sizeof(tmp));
		if (mi->options & MOD_OPT_PERM_RELOADABLE)
			strlcat(tmp, "[PERM-BUT-RELOADABLE] ", sizeof(tmp));
		if (mi->options & MOD_OPT_PERM)
			strlcat(tmp, "[PERM] ", sizeof(tmp));
		if (!(mi->options & MOD_OPT_OFFICIAL))
			strlcat(tmp, "[3RD] ", sizeof(tmp));
		if (!ValidatePermissionsForPath("server:module",sptr,NULL,NULL,NULL))
			sendtxtnumeric(sptr, "*** %s - %s - by %s %s",
				mi->header->name,
				mi->header->description,
				mi->header->author,
				mi->options & MOD_OPT_OFFICIAL ? "" : "[3RD]");
		else
			sendtxtnumeric(sptr, "*** %s %s - %s - by %s %s",
				mi->header->name,
				mi->header->version,
				mi->header->description,
				mi->header->author,
				tmp);
	}

	sendtxtnumeric(sptr, "End of module list");

	if (!ValidatePermissionsForPath("server:module",sptr,NULL,NULL,NULL))
		return 0;

	tmp[0] = '\0';
	p = tmp;
	for (i=0; i < MAXHOOKTYPES; i++)
	{
		if (!Hooks[i])
			continue;
		ircsnprintf(p, sizeof(tmp) - strlen(tmp), "%d ", i);
		p += strlen(p);
		if (p > tmp + 380)
		{
			sendtxtnumeric(sptr, "Hooks: %s", tmp);
			tmp[0] = '\0';
			p = tmp;
		}
	}
	sendtxtnumeric(sptr, "Hooks: %s ", tmp);

	tmp[0] = '\0';
	p = tmp;
	for (i=0; i < 256; i++)
	{
		for (mptr = CommandHash[i]; mptr; mptr = mptr->next)
			if (mptr->overriders)
			{
				ircsnprintf(p, sizeof(tmp)-strlen(tmp), "%s ", mptr->cmd);
				p += strlen(p);
				if (p > tmp+380)
				{
					sendtxtnumeric(sptr, "Override: %s", tmp);
					tmp[0] = '\0';
					p = tmp;
				}
			}
	}
	sendtxtnumeric(sptr, "Override: %s", tmp);

	return 1;
}

Hooktype *HooktypeFind(char *string) {
	Hooktype *hooktype;
	for (hooktype = Hooktypes; hooktype->string ;hooktype++) {
		if (!strcasecmp(hooktype->string, string))
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

#if 0
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
#endif		
	
Hook *HookAddMain(Module *module, int hooktype, int priority, int (*func)(), void (*vfunc)(), char *(*cfunc)())
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
	p->priority = priority;

	if (module) {
		ModuleObject *hookobj = (ModuleObject *)MyMallocEx(sizeof(ModuleObject));
		hookobj->object.hook = p;
		hookobj->type = MOBJ_HOOK;
		AddListItem(hookobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	
	AddListItemPrio(p, Hooks[hooktype], p->priority);

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

Callback	*CallbackAddMain(Module *module, int cbtype, int (*func)(), void (*vfunc)(), char *(*cfunc)())
{
	Callback *p;
	
	p = MyMallocEx(sizeof(Callback));
	if (func)
		p->func.intfunc = func;
	if (vfunc)
		p->func.voidfunc = vfunc;
	if (cfunc)
		p->func.pcharfunc = cfunc;
	p->type = cbtype;
	p->owner = module;
	AddListItem(p, Callbacks[cbtype]);
	if (module) {
		ModuleObject *cbobj = MyMallocEx(sizeof(ModuleObject));
		cbobj->object.callback = p;
		cbobj->type = MOBJ_CALLBACK;
		AddListItem(cbobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	return p;
}

Callback *CallbackDel(Callback *cb)
{
	Callback *p, *q;
	for (p = Callbacks[cb->type]; p; p = p->next) {
		if (p == cb) {
			q = p->next;
			DelListItem(p, Callbacks[cb->type]);
			if (RCallbacks[cb->type] == p)
				RCallbacks[cb->type] = NULL;
			if (p->owner) {
				ModuleObject *cbobj;
				for (cbobj = p->owner->objects; cbobj; cbobj = cbobj->next) {
					if ((cbobj->type == MOBJ_CALLBACK) && (cbobj->object.callback == p)) {
						DelListItem(cbobj, cb->owner->objects);
						MyFree(cbobj);
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

CommandOverride *CommandOverrideAddEx(Module *module, char *name, int priority, OverrideCmdFunc function)
{
	RealCommand *p;
	CommandOverride *ovr;
	
	if (!(p = find_Command_simple(name)))
	{
		if (module)
			module->errorcode = MODERR_NOTFOUND;
		return NULL;
	}
	for (ovr=p->overriders; ovr; ovr=ovr->next)
	{
		if ((ovr->owner == module) && (ovr->func == function))
		{
			if (module)
				module->errorcode = MODERR_EXISTS;
			return NULL;
		}
	}
	ovr = MyMallocEx(sizeof(CommandOverride));
	ovr->func = function;
	ovr->owner = module; /* TODO: module objects */
	ovr->priority = priority;
	if (module)
	{
		ModuleObject *cmdoverobj = MyMallocEx(sizeof(ModuleObject));
		cmdoverobj->type = MOBJ_COMMANDOVERRIDE;
		cmdoverobj->object.cmdoverride = ovr;
		AddListItem(cmdoverobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	ovr->command = p;
	if (!p->overriders)
		p->overridetail = ovr;
	AddListItemPrio(ovr, p->overriders, ovr->priority);
	if (p->friend)
	{
		if (!p->friend->overriders)
			p->friend->overridetail = ovr;
		AddListItem(ovr, p->friend->overriders);
	}
	return ovr;
}

CommandOverride *CommandOverrideAdd(Module *module, char *name, OverrideCmdFunc function)
{
	return CommandOverrideAddEx(module, name, 0, function);
}

void CommandOverrideDel(CommandOverride *cmd)
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
			if (obj->type != MOBJ_COMMANDOVERRIDE)
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

int CallCommandOverride(CommandOverride *ovr, Client *cptr, Client *sptr, MessageTag *mtags, int parc, char *parv[])
{
	if (ovr->next)
		return ovr->next->func(ovr->next, cptr, sptr, mtags, parc, parv);
	return ovr->command->func(cptr, sptr, mtags, parc, parv);
}

EVENT(e_unload_module_delayed)
{
	char	*name = strdup(data);
	int	i; 
	i = Module_Unload(name);
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
			(*Mod_Unload)(&m->modinfo);
		deletetmp(m->tmp_file);
	}
}

unsigned int ModuleSetOptions(Module *module, unsigned int options, int action)
{
	unsigned int oldopts = module->options;

	if (action)
        	module->options |= options;
        else
        	module->options &= ~options;
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
	"Invalid parameter(s)",
	"Object was not found"
};

const char *ModuleGetErrorStr(Module *module)
{
	if (module->errorcode >= sizeof(module_error_str)/sizeof(module_error_str[0]))
		return NULL;

	return module_error_str[module->errorcode];
}

static int num_callbacks(int cbtype)
{
Callback *e;
int cnt = 0;

	for (e = Callbacks[cbtype]; e; e = e->next)
		if (!e->willberemoved)
			cnt++;
			
	return cnt;
}

/** Ensure that all required callbacks are in place and meet
 * all specified requirements
 */
int callbacks_check(void)
{
int i;

	for (i=0; i < MAXCALLBACKS; i++)
	{
		if (num_callbacks(i) > 1)
		{
			config_error("ERROR: Multiple callbacks loaded for type %d. "
			             "Make sure you only load 1 module of 1 type (eg: only 1 cloaking module)",
			             i); /* TODO: make more clear? */
			return -1;
		}
	}
		
	return 0;
}

void callbacks_switchover(void)
{
Callback *e;
int i;

	/* Now set the real callback, and tag the new one
	 * as 'willberemoved' if needed.
	 */

	for (i=0; i < MAXCALLBACKS; i++)
		for (e = Callbacks[i]; e; e = e->next)
			if (!e->willberemoved)
			{
				RCallbacks[i] = e; /* This is the new one. */
				if (!(e->owner->options & MOD_OPT_PERM))
					e->willberemoved = 1;
				break;
			}
}

#ifdef _WIN32
const char *our_dlerror(void)
{
	static char errbuf[513];
	DWORD err = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
		0, errbuf, 512, NULL);
	if (err == ERROR_MOD_NOT_FOUND)
		strlcat(errbuf, " This could be because the DLL depends on another DLL, for example if you "
		               "are trying to load a 3rd party module which was compiled with a different compiler version.",
		               sizeof(errbuf));
	return errbuf;
}
#endif

/** Check if a module is loaded.
 * @param name Name of the module by relative path (eg: 'extbans/timedban')
 * @returns 1 if module is loaded, 0 if not.
 * @notes The name is checked against the module name,
 *        this can be a problem if two modules have the same name.
 */
int is_module_loaded(char *name)
{
	Module *mi;
	for (mi = Modules; mi; mi = mi->next)
	{
		if (mi->flags & MODFLAG_DELAYED)
			continue; /* unloading (delayed) */

		if (!strcasecmp(mi->relpath, name))
			return 1;
	}
	return 0;
}

static char *mod_var_name(ModuleInfo *modinfo, char *varshortname)
{
	static char fullname[512];
	snprintf(fullname, sizeof(fullname), "%s:%s", modinfo->handle->header->name, varshortname);
	return fullname;
}

int LoadPersistentPointerX(ModuleInfo *modinfo, char *varshortname, void **var, void (*free_variable)(ModData *m))
{
	ModDataInfo *m;
	char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCALVAR);
	if (m)
	{
		*var = moddata_localvar(m).ptr;
		return 1;
	} else {
		ModDataInfo mreq;
		memset(&mreq, 0, sizeof(mreq));
		mreq.type = MODDATATYPE_LOCALVAR;
		mreq.name = fullname;
		mreq.free = free_variable;
		m = ModDataAdd(modinfo->handle, mreq);
		moddata_localvar(m).ptr = NULL;
		return 0;
	}
}

void SavePersistentPointerX(ModuleInfo *modinfo, char *varshortname, void *var)
{
	ModDataInfo *m;
	char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCALVAR);
	moddata_localvar(m).ptr = var;
}

int LoadPersistentIntX(ModuleInfo *modinfo, char *varshortname, int *var)
{
	ModDataInfo *m;
	char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCALVAR);
	if (m)
	{
		*var = moddata_localvar(m).i;
		return 1;
	} else {
		ModDataInfo mreq;
		memset(&mreq, 0, sizeof(mreq));
		mreq.type = MODDATATYPE_LOCALVAR;
		mreq.name = fullname;
		mreq.free = NULL;
		m = ModDataAdd(modinfo->handle, mreq);
		moddata_localvar(m).i = 0;
		return 0;
	}
}

void SavePersistentIntX(ModuleInfo *modinfo, char *varshortname, int var)
{
	ModDataInfo *m;
	char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCALVAR);
	moddata_localvar(m).i = var;
}

int LoadPersistentLongX(ModuleInfo *modinfo, char *varshortname, long *var)
{
	ModDataInfo *m;
	char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCALVAR);
	if (m)
	{
		*var = moddata_localvar(m).l;
		return 1;
	} else {
		ModDataInfo mreq;
		memset(&mreq, 0, sizeof(mreq));
		mreq.type = MODDATATYPE_LOCALVAR;
		mreq.name = fullname;
		mreq.free = NULL;
		m = ModDataAdd(modinfo->handle, mreq);
		moddata_localvar(m).l = 0;
		return 0;
	}
}

void SavePersistentLongX(ModuleInfo *modinfo, char *varshortname, long var)
{
	ModDataInfo *m;
	char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCALVAR);
	moddata_localvar(m).l = var;
}

extern int module_has_moddata(Module *mod);
extern int module_has_extcmode_param_mode(Module *mod);

/** Special hack for unloading modules with moddata and parameter extcmodes */
void special_delayed_unloading(void)
{
	Module *m, *m2;
	extern Module *Modules;

	for (m = Modules; m; m = m->next)
	{
		if (!(m->flags & MODFLAG_LOADED))
			continue;
		if ((m->options & MOD_OPT_PERM) || (m->options & MOD_OPT_PERM_RELOADABLE))
			continue;
		if (module_has_moddata(m) || module_has_extcmode_param_mode(m))
		{
			int found = 0;
			for (m2 = Modules; m2; m2 = m2->next)
			{
				if ((m != m2) && !strcmp(m->header->name, m2->header->name))
					found = 1;
			}
			if (!found)
			{
			    config_warn("Delaying module unloading of '%s' a few seconds...", m->header->name);
			    m->flags |= MODFLAG_DELAYED;
			    EventAdd(NULL, "e_unload_module_delayed", 5, 1, e_unload_module_delayed, m->header->name);
			}
		}
	}
}
