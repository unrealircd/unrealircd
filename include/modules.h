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
#ifndef MODULES_H
#define MODULES_H
#define MOD_VERSION	"3.2-b5-1"
#define MOD_WE_SUPPORT  "3.2-b5*"
#define MAXCUSTOMHOOKS  30
#define MAXHOOKTYPES	70
typedef void			(*vFP)();	/* Void function pointer */
typedef int			(*iFP)();	/* Integer function pointer */
typedef char			(*cFP)();	/* char * function pointer */
#if defined(_WIN32)
#define DLLFUNC	_declspec(dllexport)
#define irc_dlopen(x,y) LoadLibrary(x)
#define irc_dlclose FreeLibrary
#define irc_dlsym(x,y,z) z = (void *)GetProcAddress(x,y)
#undef irc_dlerror
#elif defined(HPUX)
#define irc_dlopen(x,y) shl_load(x,y,0L)
#define irc_dlsym(x,y,z) shl_findsym(x,y,z)
#define irc_dlclose shl_unload
#define irc_dlerror() strerror(errno)
#else
#define irc_dlopen dlopen
#define irc_dlclose dlclose
#if defined(UNDERSCORE)
#define irc_dlsym(x,y,z) z = obsd_dlsym(x,y)
#else
#define irc_dlsym(x,y,z) z = dlsym(x,y)
#endif
#define irc_dlerror dlerror
#define DLLFUNC 
#endif

#define EVENT(x) void (x) (void *data)

typedef struct _mod_symboltable Mod_SymbolDepTable;
typedef struct _event Event;
typedef struct _eventinfo EventInfo;
typedef struct _irchook Hook;
typedef struct _hooktype Hooktype;

/*
 * Module header that every module must include, with the name of
 * mod_header
*/

typedef struct _ModuleHeader {
	char	*name;
	char	*version;
	char	*description;
	char	*modversion;
	Mod_SymbolDepTable *symdep;
} ModuleHeader;

/*
 * One piece of Borg ass..
*/
typedef struct _Module Module;

typedef struct _ModuleChild
{
	struct _ModuleChild *prev, *next; 
	Module *child; /* Aww. aint it cute? */
} ModuleChild;

typedef struct {
	int size;
	int module_load;
	Module *handle;
} ModuleInfo;


#define MOBJ_EVENT   0x0001
#define MOBJ_HOOK    0x0002
#define MOBJ_COMMAND 0x0004
#define MOBJ_HOOKTYPE 0x0008
#define MOBJ_VERSIONFLAG 0x0010
#define MOBJ_SNOMASK 0x0020
#define MOBJ_UMODE 0x0040
#define MOBJ_CMDOVERRIDE 0x0080

typedef struct {
        long mode;
        char flag;
        int (*allowed)(aClient *sptr);
        char unloaded;
        Module *owner;
} Umode;

typedef struct {
        long mode;
        char flag;
        int (*allowed)(aClient *sptr);
        char unloaded;
        Module *owner;
} Snomask;

#ifdef EXTCMODE
#define EXCHK_ACCESS		0 /* Check access */
#define EXCHK_ACCESS_ERR	1 /* Check access and send error if needed */
#define EXCHK_PARAM			2 /* Check parameter and send error if needed */

#define EXSJ_SAME			0 /* Parameters are the same */
#define EXSJ_WEWON			1 /* We won! w00t */
#define EXSJ_THEYWON		2 /* They won :( */

/** Extended channel mode table.
 * This table contains all extended channelmode info like the flag, mode, their
 * functions, etc..
 */
typedef unsigned long Cmode_t;

#define EXTCM_PAR_HEADER struct _CmodeParam *prev, *next; char flag;

typedef struct _CmodeParam {
	EXTCM_PAR_HEADER
	/** other fields are placed after this header in your own paramstruct */
} CmodeParam;

typedef struct {
	/** mode character (like 'Z') */
	char		flag;

	/** unique flag (like 0x10) */
	Cmode_t		mode;

	/** # of paramters (1 or 0) */
	int			paracount;

	/** access and parameter checking.
	 * aClient *: the client
	 * aChannel *: the channel
	 * para *: the parameter (NULL for paramless modes)
	 * int: check type (see EXCHK_*)
	 * int: what (MODE_ADD or MODE_DEL)
	 * return value: 1=ok, 0=bad
	 */
	int			(*is_ok)(aClient *,aChannel *, char *para, int, int);

	/** NOTE: The routines below are NULL for paramless modes */
	
	/** Store parameter in memory for channel.
	 * aExtCMtableParam *: the list (usually chptr->mode.extmodeparams).
	 * char *: the parameter.
	 * return value: the head of the list, RTFS if you wonder why.
	 * design notes: only alloc a new paramstruct if you need to, search for
	 * any current one first (like in case of mode +y 5 and then +y 6 later without -y).
	 */
	CmodeParam *		(*put_param)(CmodeParam *, char *);

	/** Get readable string version" of the stored parameter.
	 * aExtCMtableParam *: the list (usually chptr->mode.extmodeparams).
	 * return value: a pointer to the string (temp. storage)
	 */
	char *		(*get_param)(CmodeParam *);

	/** Convert input parameter to output.
	 * Like +l "1aaa" becomes "1".
	 * char *: the input parameter.
	 * return value: pointer to output string (temp. storage)
	 */
	char *		(*conv_param)(char *);

	/** free and remove parameter from list.
	 * aExtCMtableParam *: the list (usually chptr->mode.extmodeparams).
	 */
	void		(*free_param)(CmodeParam *);

	/** duplicate a struct and return a pointer to duplicate.
	 * This is usually just a malloc + memcpy.
	 * aExtCMtableParam *: source struct itself (no list).
	 * return value: pointer to newly allocated struct.
	 */
	CmodeParam *	(*dup_struct)(CmodeParam *);

	/** Compares 2 parameters and decides who wins the sjoin fight.
	 * When syncing channel modes (m_sjoin) a parameter conflict may occur, things like
	 * +l 5 vs +l 10. This function should determinate who wins the fight, this decision
	 * should of course not be random but the same at every server on the net.
	 * examples of such comparisons are "highest wins" (+l) and a strcmp() check (+k/+L).
	 * aChannel *: channel the fight is about.
	 * aExtCMtableParam *: our parameter
	 * aExtCMtableParam *: their parameter
	 */
	int			(*sjoin_check)(aChannel *, CmodeParam *, CmodeParam *);
} Cmode;

typedef struct {
	char		flag;
	int		paracount;
	int		(*is_ok)(aClient *,aChannel *, char *para, int, int);
	CmodeParam *	(*put_param)(CmodeParam *, char *);
	char *		(*get_param)(CmodeParam *);
	char *		(*conv_param)(char *);
	void		(*free_param)(CmodeParam *);
	CmodeParam *	(*dup_struct)(CmodeParam *);
	int		(*sjoin_check)(aChannel *, CmodeParam *, CmodeParam *);
} CmodeInfo;
#endif

typedef struct _command {
	struct _command *prev, *next;
	aCommand *cmd, *tok;
} Command;

typedef struct _versionflag {
	struct _versionflag *prev, *next;
	char flag;
	ModuleChild *parents;
} Versionflag;

typedef struct _ModuleObject {
	struct _ModuleObject *prev, *next;
	short type;
	union {
		Event *event;
		Hook *hook;
		Command *command;
		Hooktype *hooktype;
		Versionflag *versionflag;
		Snomask *snomask;
		Umode *umode;
		Cmdoverride *cmdoverride;
	} object;
} ModuleObject;

struct _irchook {
	Hook *prev, *next;
	short type;
	union {
		int (*intfunc)();
		void (*voidfunc)();
		char *(*pcharfunc)();
	} func;
	Module *owner;
};

struct _hooktype {
	short id;
	char *string;
	ModuleChild *parents;
};
/*
 * What we use to keep track internally of the modules
*/

#define MODERR_NOERROR 0
#define MODERR_EXISTS  1
#define MODERR_NOSPACE 2
#define MODERR_INVALID 3
#define MODERR_NOTFOUND 4

unsigned int ModuleGetError(Module *module);
const char *ModuleGetErrorStr(Module *module);
unsigned int ModuleGetOptions(Module *module);
unsigned int ModuleSetOptions(Module *module, unsigned int options);

struct _Module
{
	struct _Module *prev, *next;
	ModuleHeader    *header; /* The module's header */
#ifdef _WIN32
	HMODULE dll;		/* Return value of LoadLibrary */
#elif defined(HPUX)
	shl_t	dll;
#else
	void	*dll;		/* Return value of dlopen */
#endif	
	unsigned char flags;    /* 8-bits for flags .. */
	ModuleChild *children;
	ModuleObject *objects;
	ModuleInfo modinfo; /* Used to store handle info for module */
	unsigned char options;
	unsigned char errorcode;
};
/*
 * Symbol table
*/

#define MOD_OPT_PERM 0x0001

struct _mod_symboltable
{
#ifndef STATIC_LINKING
	char	*symbol;
#else
	void	*realfunc;
#endif
	vFP 	*pointer;
#ifndef STATIC_LINKING
	char	*module;
#endif
};

#ifndef STATIC_LINKING
#define MOD_Dep(name, container,module) {#name, (vFP *) &container, module}
#else
#define MOD_Dep(name, container,module) {(void *)&name, (vFP *) &container}
#endif
/* Event structs */
struct _event {
	Event   *prev, *next;
	char    *name;
	time_t  every;
	long    howmany;
	vFP	event;
	void    *data;
	time_t  last;
	Module *owner;
};

#define EMOD_EVERY 0x0001
#define EMOD_HOWMANY 0x0002
#define EMOD_NAME 0x0004
#define EMOD_EVENT 0x0008
#define EMOD_DATA 0x0010

struct _eventinfo {
	int flags;
	long howmany;
	time_t every;
	char *name;
	vFP event;
	void *data;
};


/* Huh? Why are those not marked as extern?? -- Syzop */

#define EventAdd(name, every, howmany, event, data) EventAddEx(NULL, name, every, howmany, event, data)
Event   *EventAddEx(Module *, char *name, long every, long howmany,
                  vFP event, void *data);
Event   *EventDel(Event *event);
Event   *EventMarkDel(Event *event);
Event   *EventFind(char *name);
int     EventMod(Event *event, EventInfo *mods);
void    DoEvents(void);
void    EventStatus(aClient *sptr);
void    SetupEvents(void);
void	LockEventSystem(void);
void	UnlockEventSystem(void);
extern Hook		*Hooks[MAXHOOKTYPES];
extern Hooktype		Hooktypes[MAXCUSTOMHOOKS];
extern Hook		*global_i;

void    Module_Init(void);
char    *Module_Create(char *path);
void    Init_all_testing_modules(void);
void    Unload_all_loaded_modules(void);
void    Unload_all_testing_modules(void);
int     Module_Unload(char *name, int unload);
vFP     Module_Sym(char *name);
vFP     Module_SymX(char *name, Module **mptr);
int	Module_free(Module *mod);

#ifdef __OpenBSD__
void *obsd_dlsym(void *handle, char *symbol);
#endif

Versionflag *VersionflagAdd(Module *module, char flag);
void VersionflagDel(Versionflag *vflag, Module *module);

#define add_Hook(hooktype, func) HookAddMain(NULL, hooktype, func, NULL, NULL)
#define HookAdd(hooktype, func) HookAddMain(NULL, hooktype, func, NULL, NULL)
#define HookAddEx(module, hooktype, func) HookAddMain(module, hooktype, func, NULL, NULL)
#define HookAddVoid(hooktype, func) HookAddMain(NULL, hooktype, NULL, func, NULL)
#define HookAddVoidEx(module, hooktype, func) HookAddMain(module, hooktype, NULL, func, NULL)
#define HookAddPChar(hooktype, func) HookAddMain(NULL, hooktype, NULL, NULL, func)
#define HookAddPCharEx(module, hooktype, func) HookAddMain(module, hooktype, NULL, NULL, func)
#define add_HookX(hooktype, func1, func2, func3) HookAddMain(NULL, hooktype, func1, func2, func3)

Hook	*HookAddMain(Module *module, int hooktype, int (*intfunc)(), void (*voidfunc)(), char *(*pcharfunc)());
Hook	*HookDel(Hook *hook);

Hooktype *HooktypeAdd(Module *module, char *string, int *type);
void HooktypeDel(Hooktype *hooktype, Module *module);

#define RunHook0(hooktype) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next)(*(global_i->func.intfunc))()
#define RunHook(hooktype,x) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) (*(global_i->func.intfunc))(x)
#define RunHookReturn(hooktype,x,ret) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) if((*(global_i->func.intfunc))(x) ret) return -1
#define RunHookReturnInt(hooktype,x,retchk) \
{ \
 int retval; \
 for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) \
 { \
  retval = (*(global_i->func.intfunc))(x); \
  if (retval retchk) return retval; \
 } \
}
#define RunHookReturnInt2(hooktype,x,y,retchk) \
{ \
 int retval; \
 for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) \
 { \
  retval = (*(global_i->func.intfunc))(x,y); \
  if (retval retchk) return retval; \
 } \
}

#define RunHookReturnVoid(hooktype,x,ret) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) if((*(global_i->func.intfunc))(x) ret) return
#define RunHook2(hooktype,x,y) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) (*(global_i->func.intfunc))(x,y)
#define RunHook3(hooktype,a,b,c) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) (*(global_i->func.intfunc))(a,b,c)
#define RunHook4(hooktype,a,b,c,d) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) (*(global_i->func.intfunc))(a,b,c,d)
#define RunHook5(hooktype,a,b,c,d,e) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) (*(global_i->func.intfunc))(a,b,c,d,e)
#define RunHook6(hooktype,a,b,c,d,e,f) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) (*(global_i->func.intfunc))(a,b,c,d,e,f)
#define RunHook7(hooktype,a,b,c,d,e,f,g) for (global_i = Hooks[hooktype]; global_i; global_i = global_i->next) (*(global_i->func.intfunc))(a,b,c,d,e,f,g)

Command *CommandAdd(Module *module, char *cmd, char *tok, int (*func)(), unsigned char params, int flags);
void CommandDel(Command *command);
int CommandExists(char *name);
Cmdoverride *CmdoverrideAdd(Module *module, char *cmd, iFP function);
void CmdoverrideDel(Cmdoverride *ovr);
int CallCmdoverride(Cmdoverride *ovr, aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Hook types */
#define HOOKTYPE_LOCAL_QUIT	1
#define HOOKTYPE_LOCAL_NICKCHANGE 2
#define HOOKTYPE_LOCAL_CONNECT 3
#define HOOKTYPE_REHASHFLAG 4
#undef HOOKTYPE_SCAN_INFO
#define HOOKTYPE_CONFIGPOSTTEST 6
#define HOOKTYPE_REHASH 7
#define HOOKTYPE_PRE_LOCAL_CONNECT 8
#define HOOKTYPE_HTTPD_URL 9
#define HOOKTYPE_GUEST 10
#define HOOKTYPE_SERVER_CONNECT 11
#define HOOKTYPE_SERVER_QUIT 12
#define HOOKTYPE_STATS 13
#define HOOKTYPE_LOCAL_JOIN 14
#define HOOKTYPE_CONFIGTEST 15
#define HOOKTYPE_CONFIGRUN 16
#define HOOKTYPE_USERMSG 17
#define HOOKTYPE_CHANMSG 18
#define HOOKTYPE_LOCAL_PART 19
#define HOOKTYPE_LOCAL_KICK 20
#define HOOKTYPE_LOCAL_CHANMODE 21
#define HOOKTYPE_LOCAL_TOPIC 22
#define HOOKTYPE_LOCAL_OPER 23
#define HOOKTYPE_UNKUSER_QUIT 24
#define HOOKTYPE_LOCAL_PASS 25
#define HOOKTYPE_REMOTE_CONNECT 26
#define HOOKTYPE_REMOTE_QUIT 27
#define HOOKTYPE_PRE_LOCAL_JOIN 28
#define HOOKTYPE_PRE_LOCAL_KICK 29
#define HOOKTYPE_PRE_LOCAL_TOPIC 30

/* Module flags */
#define MODFLAG_NONE	0x0000
#define MODFLAG_LOADED	0x0001 /* Fully loaded */
#define MODFLAG_TESTING 0x0002 /* Not yet initialized */
#define MODFLAG_INIT	0x0004 /* Initialized */
#define MODFLAG_DELAYED 0x0008 /* Delayed unload */

/* Module function return values */
#define MOD_SUCCESS 0
#define MOD_FAILED -1
#define MOD_DELAY 2

#define CONFIG_MAIN 1
#define CONFIG_SET 2
#define CONFIG_BAN 3
#define CONFIG_EXCEPT 4
#define CONFIG_DENY 5
#define CONFIG_ALLOW 6

#ifdef DYNAMIC_LINKING
#define MOD_HEADER(name) Mod_Header
#define MOD_TEST(name) Mod_Test
#define MOD_INIT(name) Mod_Init
#define MOD_LOAD(name) Mod_Load
#define MOD_UNLOAD(name) Mod_Unload
#else
#define MOD_HEADER(name) name##_Header
#define MOD_TEST(name) name##_Test
#define MOD_INIT(name) name##_Init
#define MOD_LOAD(name) name##_Load
#define MOD_UNLOAD(name) name##_Unload
#endif

#endif
