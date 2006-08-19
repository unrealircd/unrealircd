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
#include "types.h"
#define MAXCUSTOMHOOKS  30
#define MAXHOOKTYPES	150
#define MAXCALLBACKS	30
#define MAXEFUNCTIONS	100
#if defined(_WIN32)
 #define MOD_EXTENSION "dll"
 #define DLLFUNC	_declspec(dllexport)
 #define irc_dlopen(x,y) LoadLibrary(x)
 #define irc_dlclose FreeLibrary
 #define irc_dlsym(x,y,z) z = (void *)GetProcAddress(x,y)
 #define irc_dlerror our_dlerror
#elif defined(HPUX)
 #define MOD_EXTENSION "so"
 #define irc_dlopen(x,y) shl_load(x,y,0L)
 #define irc_dlsym(x,y,z) shl_findsym(x,y,z)
 #define irc_dlclose shl_unload
 #define irc_dlerror() strerror(errno)
#else
 #define MOD_EXTENSION "so"
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

/* Casts to int, void, void *, and char * function pointers */
#define TO_INTFUNC(x) (int (*)())(x)
#define TO_VOIDFUNC(x) (void (*)())(x)
#define TO_PVOIDFUNC(x) (void *(*)())(x)
#define TO_PCHARFUNC(x) (char *(*)())(x)

typedef struct _mod_symboltable Mod_SymbolDepTable;
typedef struct _event Event;
typedef struct _eventinfo EventInfo;
typedef struct _irchook Hook;
typedef struct _hooktype Hooktype;
typedef struct _irccallback Callback;
typedef struct _ircefunction Efunction;

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


#define MOBJ_EVENT        0x0001
#define MOBJ_HOOK         0x0002
#define MOBJ_COMMAND      0x0004
#define MOBJ_HOOKTYPE     0x0008
#define MOBJ_VERSIONFLAG  0x0010
#define MOBJ_SNOMASK      0x0020
#define MOBJ_UMODE        0x0040
#define MOBJ_CMDOVERRIDE  0x0080
#define MOBJ_EXTBAN       0x0100
#define MOBJ_CALLBACK     0x0200
#define MOBJ_ISUPPORT	  0x0400
#define MOBJ_EFUNCTION    0x0800

typedef struct {
        long mode;
        char flag;
        int (*allowed)(aClient *sptr, int what);
        char unloaded;
        Module *owner;
} Umode;

typedef struct {
        long mode;
        char flag;
        int (*allowed)(aClient *sptr, int what);
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
#define EXSJ_MERGE			3 /* Merging of modes.. neither won nor lost (merged params are in 'our' on return) */

/* return values for EXCHK_ACCESS*: */
#define EX_DENY				0  /* Disallowed, except in case of operoverride */
#define EX_ALLOW			1  /* Allowed */
#define EX_ALWAYS_DENY		-1 /* Disallowed, even in case of operoverride
                                * (eg: for operlevel modes like +A)
                                */

/** Extended channel mode table.
 * This table contains all extended channelmode info like the flag, mode, their
 * functions, etc..
 */
typedef unsigned long Cmode_t;

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
	 * RETURNS: nothing! ;p
	 * design notes: only alloc a new paramstruct if you need to, search for
	 * any current one first (like in case of mode +y 5 and then +y 6 later without -y).
	 */
	void *(*put_param)(void *, char *);

	/** Get readable string version" of the stored parameter.
	 * void *: the param data
	 * return value: a pointer to the string (temp. storage)
	 */
	char *		(*get_param)(void *);

	/** Convert input parameter to output.
	 * Like +l "1aaa" becomes "1".
	 * char *: the input parameter.
	 * aClient *: the client that the mode request came from:
	 *            1. Can be NULL (eg: if called for set::modes-on-join
	 *            2. Probably only used in rare cases, see also next remark
	 *            3. ERRORS SHOULD NOT BE SENT BY conv_param BUT BY is_ok!
	 * return value: pointer to output string (temp. storage)
	 */
	char *		(*conv_param)(char *, aClient *);

	/** free and remove parameter from list.
	 * aExtCMtableParam *: the list (usually chptr->mode.extmodeparams).
	 */
	void		(*free_param)(void *);

	/** duplicate a struct and return a pointer to duplicate.
	 * This is usually just a malloc + memcpy.
	 * aExtCMtableParam *: source struct itself (no list).
	 * return value: pointer to newly allocated struct.
	 */
	void * (*dup_struct)(void *);

	/** Compares 2 parameters and decides who wins the sjoin fight.
	 * When syncing channel modes (m_sjoin) a parameter conflict may occur, things like
	 * +l 5 vs +l 10. This function should determinate who wins the fight, this decision
	 * should of course not be random but the same at every server on the net.
	 * examples of such comparisons are "highest wins" (+l) and a strcmp() check (+k/+L).
	 * aChannel *: channel the fight is about.
	 * aExtCMtableParam *: our parameter
	 * aExtCMtableParam *: their parameter
	 */
	int			(*sjoin_check)(aChannel *, void *, void *);

	/* Slot#.. Can be used instead of GETPARAMSLOT() */
	int slot;
} Cmode;

typedef struct {
	char		flag;
	int		paracount;
	int		(*is_ok)(aClient *,aChannel *, char *para, int, int);
	void *	(*put_param)(void *, char *);
	char *		(*get_param)(void *);
	char *		(*conv_param)(char *, aClient *);
	void		(*free_param)(void *);
	void *	(*dup_struct)(void *);
	int		(*sjoin_check)(aChannel *, void *, void *);
} CmodeInfo;

/* Get a slot# for a param.. eg... GETPARAMSLOT('k') ;p */
#define GETPARAMSLOT(x)	param_to_slot_mapping[x]

/* Get a cmode handler by slot.. for example for [dont use this]: GETPARAMHANDLERBYSLOT(5)->get_param(chptr) */
#define GETPARAMHANDLERBYSLOT(slotid)	ParamTable[slotid]

/* Same as GETPARAMHANDLERBYSLOT but then by letter.. like [dont use this]: GETPARAMHANDLERBYSLOT('k')->get_param(chptr) */
#define GETPARAMHANDLERBYLETTER(x)	ParamTable[GETPARAMSLOT(x)]

/* Get paramter data struct.. for like: ((aModejEntry *)GETPARASTRUCT(chptr, 'j'))->t */
#define GETPARASTRUCT(mychptr, mychar)	chptr->mode.extmodeparams[GETPARAMSLOT(mychar)]

#define GETPARASTRUCTEX(v, mychar)	v[GETPARAMSLOT(mychar)]

#define CMP_GETSLOT(x) GETPARAMSLOT(x)
#define CMP_GETHANDLERBYSLOT(x) GETPARAMHANDLERBYSLOT(x)
#define CMP_GETHANDLERBYLETTER(x) GETPARAMHANDLERBYLETTER(x)
#define CMP_GETSTRUCT(x,y) GETPARASTRUCT(x,y)

#endif

/*** Extended bans ***/

#define EXBCHK_ACCESS		0 /* Check access */
#define EXBCHK_ACCESS_ERR	1 /* Check access and send error */
#define EXBCHK_PARAM		2 /* Check if the parameter is valid */

#define EXBTYPE_BAN			0 /* a ban */
#define EXBTYPE_EXCEPT		1 /* an except */

#define EXTBANTABLESZ		32

typedef enum ExtbanOptions { EXTBOPT_CHSVSMODE=0x1 } ExtbanOptions;

typedef struct {
	/** extbans module */
	Module *owner;
	/** extended ban character */
	char	flag;

	/** extban options */
	ExtbanOptions options;

	/** access checking [optional].
	 * aClient *: the client
	 * aChannel *: the channel
	 * para: the ban parameter
	 * int: check type (see EXBCHK_*)
	 * int: what (MODE_ADD or MODE_DEL)
	 * int: what2 (EXBTYPE_BAN or EXBTYPE_EXCEPT)
	 * return value: 1=ok, 0=bad
	 * NOTE: just set this of NULL if you want only +hoaq to place/remove bans as usual.
	 * NOTE2: This has not been tested yet!!
	 */
	int			(*is_ok)(aClient *, aChannel *, char *para, int, int, int);

	/** Convert input parameter to output [optional].
	 * like with normal bans '+b blah' gets '+b blah!*@*', and it allows
	 * you to limit the length of the ban too. You can set this to NULL however
	 * to use the value as-is.
	 * char *: the input parameter.
	 * return value: pointer to output string (temp. storage)
	 */
	char *		(*conv_param)(char *);

	/** Checks if the user is affected by this ban [required].
	 * Called from is_banned.
	 * aClient *: the client
	 * aChannel *: the channel
	 * para: the ban entry
	 * int: a value of BANCHK_* (see struct.h)
	 */
	int			(*is_banned)(aClient *, aChannel *, char *, int);
} Extban;

typedef struct {
	char	flag;
	ExtbanOptions options;
	int			(*is_ok)(aClient *, aChannel *, char *para, int, int, int);
	char *			(*conv_param)(char *);
	int			(*is_banned)(aClient *, aChannel *, char *, int);
} ExtbanInfo;


typedef struct _command {
	struct _command *prev, *next;
	aCommand *cmd, *tok;
} Command;

typedef struct _versionflag {
	struct _versionflag *prev, *next;
	char flag;
	ModuleChild *parents;
} Versionflag;

typedef struct _isupport {
	struct _isupport *prev, *next;
	char *token;
	char *value;
	Module *owner;
} Isupport;

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
		Extban *extban;
		Callback *callback;
		Efunction *efunction;
		Isupport *isupport;
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

struct _irccallback {
	Callback *prev, *next;
	short type;
	union {
		int (*intfunc)();
		void (*voidfunc)();
		char *(*pcharfunc)();
	} func;
	Module *owner;
	char willberemoved; /* will be removed on next rehash? (eg the 'old'/'current' one) */
};

/* Definition of an efunction: a MANDATORY Extern Function (in a module),
 * for things like do_join, join_channel, etc.
 * The difference between callbacks and efunctions are:
 * - efunctions are mandatory, while callbacks can be optional (depends!)
 * - efunctions are ment for internal usage, so 3rd party modules are not allowed
 *   to add them.
 * - all efunctions are declared as function pointers in modules.c
 */
struct _ircefunction {
	Efunction *prev, *next;
	short type;
	union {
		int (*intfunc)();
		void (*voidfunc)();
		void *(*pvoidfunc)();
		char *(*pcharfunc)();
	} func;
	Module *owner;
	char willberemoved; /* will be removed on next rehash? (eg the 'old'/'current' one) */
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
	unsigned char flags;    /* 8-bits for flags .. [<- this is misleading, there's mod->flags = .. everywhere] */
	ModuleChild *children;
	ModuleObject *objects;
	ModuleInfo modinfo; /* Used to store handle info for module */
	unsigned char options;
	unsigned char errorcode;
	char *tmp_file;
	unsigned long mod_sys_version;
	unsigned int compiler_version;
};
/*
 * Symbol table
*/

#define MOD_OPT_PERM		0x0001 /* Permanent module (not unloadable) */
#define MOD_OPT_OFFICIAL	0x0002 /* Official module, do not set "tainted" */

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
extern MODVAR Hook		*Hooks[MAXHOOKTYPES];
extern MODVAR Hooktype		Hooktypes[MAXCUSTOMHOOKS];
extern MODVAR Callback *Callbacks[MAXCALLBACKS], *RCallbacks[MAXCALLBACKS];
extern MODVAR Efunction *Efunctions[MAXEFUNCTIONS];

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

Isupport *IsupportAdd(Module *module, const char *token, const char *value);
void IsupportSetValue(Isupport *isupport, const char *value);
void IsupportDel(Isupport *isupport);
Isupport *IsupportFind(const char *token);

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

#define RunHook0(hooktype) do { Hook *h; for (h = Hooks[hooktype]; h; h = h->next)(*(h->func.intfunc))(); } while(0)
#define RunHook(hooktype,x) do { Hook *h; for (h = Hooks[hooktype]; h; h = h->next) (*(h->func.intfunc))(x); } while(0)
#define RunHookReturn(hooktype,x,ret) do { Hook *h; for (h = Hooks[hooktype]; h; h = h->next) if((*(h->func.intfunc))(x) ret) return -1; } while(0)
#define RunHookReturnInt(hooktype,x,retchk) \
{ \
 int retval; \
 Hook *h; \
 for (h = Hooks[hooktype]; h; h = h->next) \
 { \
  retval = (*(h->func.intfunc))(x); \
  if (retval retchk) return retval; \
 } \
}
#define RunHookReturnInt2(hooktype,x,y,retchk) \
{ \
 int retval; \
 Hook *h; \
 for (h = Hooks[hooktype]; h; h = h->next) \
 { \
  retval = (*(h->func.intfunc))(x,y); \
  if (retval retchk) return retval; \
 } \
}

#define RunHookReturnVoid(hooktype,x,ret) do { Hook *h; for (h = Hooks[hooktype]; h; h = h->next) if((*(h->func.intfunc))(x) ret) return; } while(0)
#define RunHook2(hooktype,x,y) do { Hook *h; for (h = Hooks[hooktype]; h; h = h->next) (*(h->func.intfunc))(x,y); } while(0)
#define RunHook3(hooktype,a,b,c) do { Hook *h; for (h = Hooks[hooktype]; h; h = h->next) (*(h->func.intfunc))(a,b,c); } while(0)
#define RunHook4(hooktype,a,b,c,d) do { Hook *h; for (h = Hooks[hooktype]; h; h = h->next) (*(h->func.intfunc))(a,b,c,d); } while(0)
#define RunHook5(hooktype,a,b,c,d,e) do { Hook *h; for (h = Hooks[hooktype]; h; h = h->next) (*(h->func.intfunc))(a,b,c,d,e); } while(0)
#define RunHook6(hooktype,a,b,c,d,e,f) do { Hook *h; for (h = Hooks[hooktype]; h; h = h->next) (*(h->func.intfunc))(a,b,c,d,e,f); } while(0)
#define RunHook7(hooktype,a,b,c,d,e,f,g) do { Hook *h; for (h = Hooks[hooktype]; h; h = h->next) (*(h->func.intfunc))(a,b,c,d,e,f,g); } while(0)

#define CallbackAdd(cbtype, func) CallbackAddMain(NULL, cbtype, func, NULL, NULL)
#define CallbackAddEx(module, cbtype, func) CallbackAddMain(module, cbtype, func, NULL, NULL)
#define CallbackAddVoid(cbtype, func) CallbackAddMain(NULL, cbtype, NULL, func, NULL)
#define CallbackAddVoidEx(module, cbtype, func) CallbackAddMain(module, cbtype, NULL, func, NULL)
#define CallbackAddPChar(cbtype, func) CallbackAddMain(NULL, cbtype, NULL, NULL, func)
#define CallbackAddPCharEx(module, cbtype, func) CallbackAddMain(module, cbtype, NULL, NULL, func)

extern Callback	*CallbackAddMain(Module *module, int cbtype, int (*intfunc)(), void (*voidfunc)(), char *(*pcharfunc)());
extern Callback	*CallbackDel(Callback *cb);

#define EfunctionAdd(module, cbtype, func) EfunctionAddMain(module, cbtype, func, NULL, NULL, NULL)
#define EfunctionAddVoid(module, cbtype, func) EfunctionAddMain(module, cbtype, NULL, func, NULL, NULL)
#define EfunctionAddPVoid(module, cbtype, func) EfunctionAddMain(module, cbtype, NULL, NULL, func, NULL)
#define EfunctionAddPChar(module, cbtype, func) EfunctionAddMain(module, cbtype, NULL, NULL, NULL, func)

extern Efunction	*EfunctionAddMain(Module *module, int eftype, int (*intfunc)(), void (*voidfunc)(), void *(*pvoidfunc)(), char *(*pcharfunc)());
extern Efunction	*EfunctionDel(Efunction *cb);

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
#define HOOKTYPE_PRE_LOCAL_PART 5
#define HOOKTYPE_CONFIGPOSTTEST 6
#define HOOKTYPE_REHASH 7
#define HOOKTYPE_PRE_LOCAL_CONNECT 8
#define HOOKTYPE_PRE_LOCAL_QUIT 9
#define HOOKTYPE_GUEST 10
#define HOOKTYPE_SERVER_CONNECT 11
#define HOOKTYPE_SERVER_QUIT 12
#define HOOKTYPE_STATS 13
#define HOOKTYPE_LOCAL_JOIN 14
#define HOOKTYPE_CONFIGTEST 15
#define HOOKTYPE_CONFIGRUN 16
/* If you ever change the number of usermsg & chanmsg, notify Syzop first, kthx! ;p */
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
#define HOOKTYPE_REMOTE_NICKCHANGE 31
#define HOOKTYPE_CHANNEL_CREATE 32
#define HOOKTYPE_CHANNEL_DESTROY 33
#define HOOKTYPE_REMOTE_CHANMODE 34
#define HOOKTYPE_TKL_EXCEPT 35
#define HOOKTYPE_UMODE_CHANGE 36
#define HOOKTYPE_TOPIC 37
#define HOOKTYPE_REHASH_COMPLETE 38
#define HOOKTYPE_TKL_ADD 39
#define HOOKTYPE_TKL_DEL 40
#define HOOKTYPE_LOCAL_KILL 41
#define HOOKTYPE_LOG 42
#define HOOKTYPE_REMOTE_JOIN 43
#define HOOKTYPE_REMOTE_PART 44
#define HOOKTYPE_REMOTE_KICK 45
#define HOOKTYPE_LOCAL_SPAMFILTER 46
#define HOOKTYPE_SILENCED 47
#define HOOKTYPE_CAN_JOIN 48
#define HOOKTYPE_CAN_SEND 49
#define HOOKTYPE_CLEANUP_CLIENT 50
#define HOOKTYPE_CLEANUP_USER 51
#define HOOKTYPE_CLEANUP_USER2 52
#define HOOKTYPE_PRE_CHANMSG 53
#define HOOKTYPE_KNOCK 54
#define HOOKTYPE_MODECHAR_FIXME 55

/* Hook return values */
#define HOOK_CONTINUE 0
#define HOOK_ALLOW -1
#define HOOK_DENY 1

/* Callback types */
#define CALLBACKTYPE_CLOAK 1
#define CALLBACKTYPE_CLOAKKEYCSUM 2
#define CALLBACKTYPE_CLOAK_EX 3

/* Efunction types */
#define EFUNC_DO_JOIN       				1
#define EFUNC_JOIN_CHANNEL  				2
#define EFUNC_CAN_JOIN      				3
#define EFUNC_DO_MODE       				4
#define EFUNC_SET_MODE      				5
#define EFUNC_M_UMODE						6
#define EFUNC_REGISTER_USER					7
#define EFUNC_TKL_HASH						8
#define EFUNC_TKL_TYPETOCHAR				9
#define EFUNC_TKL_ADD_LINE					10
#define EFUNC_TKL_DEL_LINE					11
#define EFUNC_TKL_CHECK_LOCAL_REMOVE_SHUN	12
#define EFUNC_TKL_EXPIRE					13
#define EFUNC_TKL_CHECK_EXPIRE				14
#define EFUNC_FIND_TKLINE_MATCH				15
#define EFUNC_FIND_SHUN						16
#define EFUNC_FIND_SPAMFILTER_USER			17
#define EFUNC_FIND_QLINE					18
#define EFUNC_FIND_TKLINE_MATCH_ZAP			19
#define EFUNC_TKL_STATS						20
#define EFUNC_TKL_SYNCH						21
#define EFUNC_M_TKL							22
#define EFUNC_PLACE_HOST_BAN				23
#define EFUNC_DOSPAMFILTER					24
#define EFUNC_DOSPAMFILTER_VIRUSCHAN		25
#define EFUNC_FIND_TKLINE_MATCH_ZAP_EX		26
#define EFUNC_SEND_LIST						27
#define EFUNC_STRIPBADWORDS_CHANNEL			28
#define EFUNC_STRIPBADWORDS_MESSAGE			29
#define EFUNC_STRIPBADWORDS_QUIT			30
#define EFUNC_STRIPCOLORS					31
#define EFUNC_STRIPCONTROLCODES				32

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
#define CONFIG_CLOAKKEYS 7

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

#define CLOAK_KEYCRC	RCallbacks[CALLBACKTYPE_CLOAKKEYCSUM]->func.pcharfunc()

#ifdef DYNAMIC_LINKING
 #include "modversion.h"
#endif

#endif

