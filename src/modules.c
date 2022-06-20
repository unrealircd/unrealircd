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
/* dlsym for OpenBSD */
void *obsd_dlsym(void *handle, const char *symbol)
{
	size_t buflen = strlen(symbol) + 2;
	char *obsdsymbol = safe_alloc(buflen);
	void *symaddr = NULL;

	if (obsdsymbol)
	{
		ircsnprintf(obsdsymbol, buflen, "_%s", symbol);
		symaddr = dlsym(handle, obsdsymbol);
		safe_free(obsdsymbol);
	}

	return symaddr;
}
#endif

void deletetmp(const char *path)
{
#ifndef NOREMOVETMP
	if (!loop.config_test)
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
		if (!loop.booted)
			exit(7);
		return; 
	}

	while ((dir = readdir(fd)))
	{
		char *fname = dir->d_name;
		if (!strcmp(fname, ".") || !strcmp(fname, ".."))
			continue;
		if (!strstr(fname, ".so") && !strstr(fname, ".conf") &&
		    (strstr(fname, "core") || strstr(fname, "unrealircd_asan.")))
			continue; /* core dump or ASan log */
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

Module *Module_Find(const char *name)
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

int parse_modsys_version(const char *version)
{
	if (!strcmp(version, "unrealircd-6"))
		return 0x600000;
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
const char *Module_TransformPath(const char *path_)
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
const char *Module_GetRelPath(const char *fullpath)
{
	static char buf[512];
	char prefix[512];
	const char *without_prefix = fullpath;
	char *s;

	/* Strip the prefix */
	snprintf(prefix, sizeof(prefix), "%s/", MODULESDIR);
	if (!strncasecmp(fullpath, prefix, strlen(prefix)))
		without_prefix += strlen(prefix);
	strlcpy(buf, without_prefix, sizeof(buf));

	/* Strip the suffix */
	s = strstr(buf, MODULE_SUFFIX);
	if (s)
		*s = '\0';

	return buf;
}

/** Validate a modules' ModuleHeader.
 * @returns Error message is returned, or NULL if everything is OK.
 */
static const char *validate_mod_header(const char *relpath, ModuleHeader *mod_header)
{
	char *p;
	static char buf[256];

	if (!mod_header->name || !mod_header->version || !mod_header->author || !mod_header->description)
		return "NULL values encountered in Mod_Header struct members";

	/* Validate module name */
	if (strcmp(mod_header->name, relpath))
	{
		snprintf(buf, sizeof(buf), "Module has path '%s' but uses name '%s' in MOD_HEADER. These should be the same!",
			relpath, mod_header->name);
		return buf;
	}
	/* This too, just to be sure.. we never ever want other characters
	 * than these, since it may break the S2S SMOD command or /MODULE
	 * output etc.
	 */
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

int module_already_in_testing(const char *relpath)
{
	Module *m;
	for (m = Modules; m; m = m->next)
	{
		if (!(m->options & MOD_OPT_PERM) &&
		    (!(m->flags & MODFLAG_TESTING) || (m->flags & MODFLAG_DELAYED)))
			continue;
		if (!strcmp(m->relpath, relpath))
			return 1;
	}
	return 0;
}

/*
 * Returns an error if insucessful .. yes NULL is OK! 
*/
const char *Module_Create(const char *path_)
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
	const char	*path, *relpath, *tmppath;
	ModuleHeader    *mod_header = NULL;
	int		ret = 0;
	const char	*reterr;
	Module          *mod = NULL, **Mod_Handle = NULL;
	char *expectedmodversion = our_mod_version;
	unsigned int expectedcompilerversion = our_compiler_version;
	long modsys_ver = 0;

	path = Module_TransformPath(path_);

	relpath = Module_GetRelPath(path);
	if (module_already_in_testing(relpath))
		return 0;

	if (!file_exists(path))
	{
		snprintf(errorbuf, sizeof(errorbuf), "Cannot open module file: %s", strerror(errno));
		return errorbuf;
	}

	if (loop.config_test)
	{
		/* For './unrealircd configtest' we don't have to do any copying and shit */
		tmppath = path;
	} else {
		tmppath = unreal_mktemp(TMPDIR, unreal_getmodfilename(path));
		if (!tmppath)
			return "Unable to create temporary file!";

		/* We have to copy the module, because otherwise the dynamic loader
		 * will not load the new .so if we rehash while holding the original .so
		 * We used to hardlink here instead of copy, but then OpenBSD and Linux
		 * got smart and detected that, so now we always copy.
		 */
		ret = unreal_copyfileex(path, tmppath, 0);
		if (!ret)
		{
			snprintf(errorbuf, sizeof(errorbuf), "Failed to copy module file.");
			return errorbuf;
		}
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
		if ((reterr = validate_mod_header(relpath, mod_header)))
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
		safe_strdup(mod->tmp_file, tmppath);
		mod->mod_sys_version = modsys_ver;
		mod->compiler_version = compiler_version ? *compiler_version : 0;
		safe_strdup(mod->relpath, relpath);

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
		AddListItemPrio(mod, Modules, 0);
		return NULL;
	}
	else
	{
		/* Return the error .. */
		return irc_dlerror();
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
	
	modp = safe_alloc(sizeof(Module));
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
		ISupportDel(obj->object.isupport);
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
	else if (obj->type == MOBJ_RPC) {
		RPCHandlerDel(obj->object.rpc);
	}
	else
	{
		unreal_log(ULOG_FATAL, "module", "FREEMODOBJ_UNKNOWN_TYPE", NULL,
		           "[BUG] FreeModObj() called for unknown object (type $type)",
		           log_data_integer("type", obj->type));
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
			safe_free(child);
		}
		DelListItem(mi,Modules);
		irc_dlclose(mi->dll);
		deletetmp(mi->tmp_file);
		safe_free(mi->tmp_file);
		safe_free(mi->relpath);
		safe_free(mi);
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
			safe_free(child);
		}
		DelListItem(mi,Modules);
		irc_dlclose(mi->dll);
		deletetmp(mi->tmp_file);
		safe_free(mi->tmp_file);
		safe_free(mi->relpath);
		safe_free(mi);
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
				safe_free(cp);
				/* We can assume there can be only one. */
				break;
			}
		}
	}
	DelListItem(mod, Modules);
	irc_dlclose(mod->dll);
	safe_free(mod->tmp_file);
	safe_free(mod->relpath);
	safe_free(mod);
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
int Module_Unload(const char *name)
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
	ret = (*Mod_Unload)(&m->modinfo);
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
	
	childp = safe_alloc(sizeof(ModuleChild));
	childp->child = child;
	AddListItem(childp, parent->children);
}

/* cmd_module.
 * by Stskeeps, codemastr, Syzop.
 * Changed it so it's now public for users too, as quite some people
 * (and users) requested they should have the right to see what kind 
 * of weird modules are loaded on the server, especially since people
 * like to load spy modules these days.
 * I do not consider this sensitive information, but just in case
 * I stripped the version string for non-admins (eg: normal users). -- Syzop
 */
CMD_FUNC(cmd_module)
{
	Module *mi;
	int i;
	char tmp[1024], *p;
	RealCommand *mptr;
	int all = 0;

	if ((parc > 1) && !strcmp(parv[1], "-all"))
		all = 1;

	if (MyUser(client) && !IsOper(client) && all)
		add_fake_lag(client, 7000); /* Lag them up. Big list. */

	if ((parc > 2) && (hunt_server(client, recv_mtags, "MODULE", 2, parc, parv) != HUNTED_ISME))
		return;

	if ((parc == 2) && (parv[1][0] != '-') && (hunt_server(client, recv_mtags, "MODULE", 1, parc, parv) != HUNTED_ISME))
		return;

	if (all)
		sendtxtnumeric(client, "Showing ALL loaded modules:");
	else
		sendtxtnumeric(client, "Showing loaded 3rd party modules (use \"MODULE -all\" to show all modules):");

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
		if (!ValidatePermissionsForPath("server:module",client,NULL,NULL,NULL))
			sendtxtnumeric(client, "*** %s - %s - by %s %s",
				mi->header->name,
				mi->header->description,
				mi->header->author,
				mi->options & MOD_OPT_OFFICIAL ? "" : "[3RD]");
		else
			sendtxtnumeric(client, "*** %s %s - %s - by %s %s",
				mi->header->name,
				mi->header->version,
				mi->header->description,
				mi->header->author,
				tmp);
	}

	sendtxtnumeric(client, "End of module list");

	if (!ValidatePermissionsForPath("server:module",client,NULL,NULL,NULL))
		return;

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
			sendtxtnumeric(client, "Hooks: %s", tmp);
			tmp[0] = '\0';
			p = tmp;
		}
	}
	sendtxtnumeric(client, "Hooks: %s ", tmp);

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
					sendtxtnumeric(client, "Override: %s", tmp);
					tmp[0] = '\0';
					p = tmp;
				}
			}
	}
	sendtxtnumeric(client, "Override: %s", tmp);
}

Hooktype *HooktypeFind(const char *string) {
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
			parent = safe_alloc(sizeof(ModuleChild));
			parent->child = module;
			if (module) {
				ModuleObject *vflagobj;
				vflagobj = safe_alloc(sizeof(ModuleObject));
				vflagobj->type = MOBJ_VERSIONFLAG;
				vflagobj->object.versionflag = vflag;
				AddListItem(vflagobj, module->objects);
				module->errorcode = MODERR_NOERROR;
			}
			AddListItem(parent,vflag->parents);
		}
		return vflag;
	}
	vflag = safe_alloc(sizeof(Versionflag));
	vflag->flag = flag;
	parent = safe_alloc(sizeof(ModuleChild));
	parent->child = module;
	if (module)
	{
		ModuleObject *vflagobj;
		vflagobj = safe_alloc(sizeof(ModuleObject));
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
			safe_free(owner);
			break;
		}
	}
	if (module)
	{
		ModuleObject *objs;
		for (objs = module->objects; objs; objs = objs->next) {
			if (objs->type == MOBJ_VERSIONFLAG && objs->object.versionflag == vflag) {
				DelListItem(objs,module->objects);
				safe_free(objs);
				break;
			}
		}
	}
	if (!vflag->parents)
	{
		flag_del(vflag->flag);
		DelListItem(vflag, Versionflags);
		safe_free(vflag);
	}
}

Hook *HookAddMain(Module *module, int hooktype, int priority, int (*func)(), void (*vfunc)(), char *(*stringfunc)(), const char *(*conststringfunc)())
{
	Hook *p;
	
	p = (Hook *) safe_alloc(sizeof(Hook));
	if (func)
		p->func.intfunc = func;
	if (vfunc)
		p->func.voidfunc = vfunc;
	if (stringfunc)
		p->func.stringfunc = stringfunc;
	if (conststringfunc)
		p->func.conststringfunc = conststringfunc;
	p->type = hooktype;
	p->owner = module;
	p->priority = priority;

	if (module) {
		ModuleObject *hookobj = (ModuleObject *)safe_alloc(sizeof(ModuleObject));
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
						safe_free(hookobj);
						break;
					}
				}
			}
			safe_free(p);
			return q;
		}
	}
	return NULL;
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

Callback *CallbackAddMain(Module *module, int cbtype, int (*func)(), void (*vfunc)(), void *(*pvfunc)(), char *(*stringfunc)(), const char *(*conststringfunc)())
{
	Callback *p;
	
	if (num_callbacks(cbtype) > 0)
	{
		if (module)
			module->errorcode = MODERR_EXISTS;
		return NULL;
	}
	
	p = safe_alloc(sizeof(Callback));
	if (func)
		p->func.intfunc = func;
	if (vfunc)
		p->func.voidfunc = vfunc;
	if (pvfunc)
		p->func.pvoidfunc = pvfunc;
	if (stringfunc)
		p->func.stringfunc = stringfunc;
	if (conststringfunc)
		p->func.conststringfunc = conststringfunc;
	p->type = cbtype;
	p->owner = module;
	AddListItem(p, Callbacks[cbtype]);
	if (module) {
		ModuleObject *cbobj = safe_alloc(sizeof(ModuleObject));
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
						safe_free(cbobj);
						break;
					}
				}
			}
			safe_free(p);
			return q;
		}
	}
	return NULL;
}

CommandOverride *CommandOverrideAdd(Module *module, const char *name, int priority, OverrideCmdFunc function)
{
	RealCommand *p;
	CommandOverride *ovr;
	
	if (!(p = find_command_simple(name)))
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
	ovr = safe_alloc(sizeof(CommandOverride));
	ovr->func = function;
	ovr->owner = module;
	ovr->priority = priority;
	if (module)
	{
		ModuleObject *cmdoverobj = safe_alloc(sizeof(ModuleObject));
		cmdoverobj->type = MOBJ_COMMANDOVERRIDE;
		cmdoverobj->object.cmdoverride = ovr;
		AddListItem(cmdoverobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	ovr->command = p;
	AddListItemPrio(ovr, p->overriders, ovr->priority);
	if (p->friend)
	{
		AddListItem(ovr, p->friend->overriders);
	}
	return ovr;
}

void CommandOverrideDel(CommandOverride *cmd)
{
	DelListItem(cmd, cmd->command->overriders);
	if (cmd->command->friend)
	{
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
				safe_free(obj);
				break;
			}
		}
	}
	safe_free(cmd);
}

void CallCommandOverride(CommandOverride *ovr, Client *client, MessageTag *mtags, int parc, const char *parv[])
{
	if (ovr->next)
		ovr->next->func(ovr->next, client, mtags, parc, parv);
	else
		ovr->command->func(client, mtags, parc, parv);
}

EVENT(e_unload_module_delayed)
{
	char *name = (char *)data;
	int i; 
	i = Module_Unload(name);
	if (i == 1)
	{
		unreal_log(ULOG_INFO, "module", "MODULE_UNLOADING_DELAYED", NULL,
		           "Unloading module $module_name (was delayed earlier)",
		           log_data_string("module_name", name));
	}
	safe_free(name);
	extcmodes_check_for_changes();
	umodes_check_for_changes();
	return;
}

void unload_all_modules(void)
{
	Module *m;
	int	(*Mod_Unload)();
	for (m = Modules; m; m = m->next)
	{
#ifdef DEBUGMODE
		unreal_log(ULOG_DEBUG, "module", "MODULE_UNLOADING", NULL,
		           "Unloading module $module_name",
		           log_data_string("module_name", m->header->name));
#endif
		irc_dlsym(m->dll, "Mod_Unload", Mod_Unload);
		if (Mod_Unload)
			(*Mod_Unload)(&m->modinfo);
		deletetmp(m->tmp_file);
	}
}

void ModuleSetOptions(Module *module, unsigned int options, int action)
{
	unsigned int oldopts = module->options;

	if (options == MOD_OPT_UNLOAD_PRIORITY)
	{
		DelListItem(module, Modules);
		AddListItemPrio(module, Modules, action);
	} else {
		/* Simple bit flag(s) */
		if (action)
			module->options |= options;
		else
			module->options &= ~options;
	}
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
			             i);
			return -1;
		}
	}

	if (!Callbacks[CALLBACKTYPE_CLOAK_KEY_CHECKSUM])
	{
		unreal_log(ULOG_ERROR, "config", "NO_CLOAKING_MODULE", NULL,
		           "No cloaking module loaded, you must load 1 of these modulese:\n"
		           "1) cloak_sha256 - if you are a new network starting with UnrealIRCd 6\n"
		           "2) cloak_md5 - the old one if migrating an existing network from UnrealIRCd 3.2/4/5\n"
		           "3) cloak_none - if you don't want to use cloaking at all\n"
		           "See also https://www.unrealircd.org/docs/FAQ#choose-a-cloaking-module");
		return -1;
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
 * @note  The name is checked against the module name,
 *        this can be a problem if two modules have the same name.
 */
int is_module_loaded(const char *name)
{
	Module *mi;
	for (mi = Modules; mi; mi = mi->next)
	{
		if (mi->flags & MODFLAG_DELAYED)
			continue; /* unloading (delayed) */

		/* During testing/rehashing, ignore modules that are loaded,
		 * since we only care about the 'future' state.
		 */
		if ((loop.rehashing == 2) && (mi->flags == MODFLAG_LOADED))
			continue;

		if (!strcasecmp(mi->relpath, name))
			return 1;
	}
	return 0;
}

static const char *mod_var_name(ModuleInfo *modinfo, const char *varshortname)
{
	static char fullname[512];
	snprintf(fullname, sizeof(fullname), "%s:%s", modinfo->handle->header->name, varshortname);
	return fullname;
}

int LoadPersistentPointerX(ModuleInfo *modinfo, const char *varshortname, void **var, void (*free_variable)(ModData *m))
{
	ModDataInfo *m;
	const char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCAL_VARIABLE);
	if (m)
	{
		*var = moddata_local_variable(m).ptr;
		return 1;
	} else {
		ModDataInfo mreq;
		memset(&mreq, 0, sizeof(mreq));
		mreq.type = MODDATATYPE_LOCAL_VARIABLE;
		mreq.name = strdup(fullname);
		mreq.free = free_variable;
		m = ModDataAdd(modinfo->handle, mreq);
		moddata_local_variable(m).ptr = NULL;
		safe_free(mreq.name);
		return 0;
	}
}

void SavePersistentPointerX(ModuleInfo *modinfo, const char *varshortname, void *var)
{
	ModDataInfo *m;
	const char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCAL_VARIABLE);
	moddata_local_variable(m).ptr = var;
}

int LoadPersistentIntX(ModuleInfo *modinfo, const char *varshortname, int *var)
{
	ModDataInfo *m;
	const char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCAL_VARIABLE);
	if (m)
	{
		*var = moddata_local_variable(m).i;
		return 1;
	} else {
		ModDataInfo mreq;
		memset(&mreq, 0, sizeof(mreq));
		mreq.type = MODDATATYPE_LOCAL_VARIABLE;
		mreq.name = strdup(fullname);
		mreq.free = NULL;
		m = ModDataAdd(modinfo->handle, mreq);
		moddata_local_variable(m).i = 0;
		safe_free(mreq.name);
		return 0;
	}
}

void SavePersistentIntX(ModuleInfo *modinfo, const char *varshortname, int var)
{
	ModDataInfo *m;
	const char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCAL_VARIABLE);
	moddata_local_variable(m).i = var;
}

int LoadPersistentLongX(ModuleInfo *modinfo, const char *varshortname, long *var)
{
	ModDataInfo *m;
	const char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCAL_VARIABLE);
	if (m)
	{
		*var = moddata_local_variable(m).l;
		return 1;
	} else {
		ModDataInfo mreq;
		memset(&mreq, 0, sizeof(mreq));
		mreq.type = MODDATATYPE_LOCAL_VARIABLE;
		mreq.name = strdup(fullname);
		mreq.free = NULL;
		m = ModDataAdd(modinfo->handle, mreq);
		moddata_local_variable(m).l = 0;
		safe_free(mreq.name);
		return 0;
	}
}

void SavePersistentLongX(ModuleInfo *modinfo, const char *varshortname, long var)
{
	ModDataInfo *m;
	const char *fullname = mod_var_name(modinfo, varshortname);

	m = findmoddata_byname(fullname, MODDATATYPE_LOCAL_VARIABLE);
	moddata_local_variable(m).l = var;
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
				char *name = strdup(m->header->name);
				config_warn("Delaying module unloading of '%s' for a millisecond...", name);
				m->flags |= MODFLAG_DELAYED;
				EventAdd(NULL, "e_unload_module_delayed", e_unload_module_delayed, name, 0, 1);
			}
		}
	}
}
