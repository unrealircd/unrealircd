/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/modules.h
 *   (C) Carsten V. Munk 2000 <stskeeps@tspre.org>
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
 *
 *   $Id$
 */

#define MOD_VERSION	1
#define MAXMODULES	50
#define MAXHOOKTYPES	20

#ifndef RTLD_NOW
#define RTLD_NOW RTLD_LAZY
#endif


#ifdef _WIN32
#define DLLFUNC	_declspec(dllexport)
#define irc_dlopen(x,y) LoadLibrary(x)
#define irc_dlclose FreeLibrary
#define irc_dlsym (void *)GetProcAddress
#undef irc_dlerror
#else
#define irc_dlopen dlopen
#define irc_dlclose dlclose
#define irc_dlsym dlsym
#define irc_dlerror dlerror
#define DLLFUNC 
#endif

typedef struct moduleInfo 	ModuleInfo;
typedef void			(*vFP)();	/* Void function pointer */
typedef int			(*iFP)();	/* Integer function pointer */
typedef char			(*cFP)();	/* char * function pointer */

struct moduleInfo
{
	short	mversion;	/* Written for module header version */
	char	*name;		/* Name of module */
	char	*version;	/* $Id$ */
	char	*description;   /* Small description */
#ifdef _WIN32
	HMODULE dll;		/* Return value of LoadLibrary */
#else
	void	*dll;		/* Return value of dlopen */
#endif
	void	(*unload)();	/* pointer to mod_unload */
};

extern ModuleInfo	*module_buffer;
extern Hook		*Hooks[MAXHOOKTYPES];
extern Hook		*global_i;

void 	module_init(void);
int  	load_module(char *module);
int	unload_module(char *name);
vFP	module_sym(char *name);

void	add_Hook(int hooktype, int (*func)());
void	del_Hook(int hooktype, int (*func)());

#define RunHook(hooktype,x) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) (*global_i->func)(x)
#define RunHook2(hooktype,x,y) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) (*global_i->func)(x,y)

#define HOOKTYPE_LOCAL_QUIT		1
#define HOOKTYPE_LOCAL_NICKCHANGE 	2

