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
#define MAXEFUNCTIONS	90
#if defined(_WIN32)
 #define MOD_EXTENSION "dll"
 #define DLLFUNC	_declspec(dllexport)
 #define irc_dlopen(x,y) LoadLibrary(x)
 #define irc_dlclose FreeLibrary
 #define irc_dlsym(x,y,z) z = (void *)GetProcAddress(x,y)
 #define irc_dlerror our_dlerror
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

typedef struct Event Event;
typedef struct EventInfo EventInfo;
typedef struct Hook Hook;
typedef struct Hooktype Hooktype;
typedef struct Callback Callback;
typedef struct Efunction Efunction;
typedef enum EfunctionType EfunctionType;

/*
 * Module header that every module must include, with the name of
 * mod_header
*/

typedef struct ModuleHeader {
	char *name;
	char *version;
	char *description;
	char *author;
	char *modversion;
} ModuleHeader;

typedef struct Module Module;

typedef struct ModuleChild
{
	struct ModuleChild *prev, *next; 
	Module *child; /* Aww. aint it cute? */
} ModuleChild;

typedef struct {
	int size;
	int module_load;
	Module *handle;
} ModuleInfo;


typedef enum ModuleObjectType {
	MOBJ_EVENT = 1,
	MOBJ_HOOK = 2,
	MOBJ_COMMAND = 3,
	MOBJ_HOOKTYPE = 4,
	MOBJ_VERSIONFLAG = 5,
	MOBJ_SNOMASK = 6,
	MOBJ_UMODE = 7,
	MOBJ_COMMANDOVERRIDE = 8,
	MOBJ_EXTBAN = 9,
	MOBJ_CALLBACK = 10,
	MOBJ_ISUPPORT = 11,
	MOBJ_EFUNCTION = 12,
	MOBJ_CMODE = 13,
	MOBJ_MODDATA = 14,
	MOBJ_VALIDATOR = 15,
	MOBJ_CLICAP = 16,
	MOBJ_MTAG = 17,
	MOBJ_HISTORY_BACKEND = 18,
} ModuleObjectType;

typedef struct {
        long mode; /**< Mode mask */
        char flag; /**< Mode character */
        int unset_on_deoper; /**< When set to 1 then this user mode will be unset on de-oper */
        int (*allowed)(Client *sptr, int what); /**< The 'is this user allowed to set this mode?' routine */
        char unloaded; /**< Internal flag to indicate module is being unloaded */
        Module *owner; /**< Module that owns this user mode */
} Umode;

typedef struct {
        long mode; /**< Snomask mask */
        char flag; /**< Snomask character */
        int unset_on_deoper; /**< When set to 1 then this snomask will be unset on de-oper */
        int (*allowed)(Client *sptr, int what); /**< The 'is this user allowed to set this snomask?' routine */
        char unloaded; /**< Internal flag to indicate module is being unloaded */
        Module *owner; /**< Module that owns this snomask */
} Snomask;

typedef enum ModDataType {
	MODDATATYPE_LOCAL_VARIABLE	= 1,
	MODDATATYPE_GLOBAL_VARIABLE	= 2,
	MODDATATYPE_CLIENT		= 3,
	MODDATATYPE_LOCAL_CLIENT	= 4,
	MODDATATYPE_CHANNEL		= 5,
	MODDATATYPE_MEMBER		= 6,
	MODDATATYPE_MEMBERSHIP		= 7,
} ModDataType;

typedef struct ModDataInfo ModDataInfo;

struct ModDataInfo {
	ModDataInfo *prev, *next;
	char *name; /**< Name for this moddata */
	Module *owner; /**< Owner of this moddata */
	ModDataType type; /**< Type of module data (eg: for client, channel, etc..) */
	int slot; /**< Assigned slot */
	char unloaded; /**< Module being unloaded? */
	void (*free)(ModData *m); /**< Function will be called when the data needs to be freed (may be NULL if not using dynamic storage) */
	char *(*serialize)(ModData *m); /**< Function which converts the data to a string. May return NULL if 'm' contains no data (since for example m->ptr may be NULL). */
	void (*unserialize)(char *str, ModData *m); /**< Function which converts the string back to data */
	int sync; /**< Send in netsynch (when servers connect) */
};

#define moddata_client(acptr, md)    acptr->moddata[md->slot]
#define moddata_local_client(acptr, md)    acptr->local->moddata[md->slot]
#define moddata_channel(chptr, md)   chptr->moddata[md->slot]
#define moddata_member(m, md)        m->moddata[md->slot]
#define moddata_membership(m, md)    m->moddata[md->slot]
#define moddata_local_variable(md)         local_variable_moddata[md->slot]
#define moddata_global_variable(md)        global_variable_moddata[md->slot]

#define EXCHK_ACCESS		0 /* Check access */
#define EXCHK_ACCESS_ERR	1 /* Check access and send error if needed */
#define EXCHK_PARAM		2 /* Check parameter and send error if needed */

/* Can bypass message restriction - Types */
typedef enum BypassChannelMessageRestrictionType {
	BYPASS_CHANMSG_EXTERNAL = 1,
	BYPASS_CHANMSG_MODERATED = 2,
	BYPASS_CHANMSG_COLOR = 3,
	BYPASS_CHANMSG_CENSOR = 4,
	BYPASS_CHANMSG_NOTICE = 5,
} BypassChannelMessageRestrictionType;

#define EXSJ_SAME		0 /* Parameters are the same */
#define EXSJ_WEWON		1 /* We won! w00t */
#define EXSJ_THEYWON		2 /* They won :( */
#define EXSJ_MERGE		3 /* Merging of modes.. neither won nor lost (merged params are in 'our' on return) */

/* return values for EXCHK_ACCESS*: */
#define EX_DENY			0  /* Disallowed, except in case of operoverride */
#define EX_ALLOW		1  /* Allowed */
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
	 * Client *: the client
	 * Channel *: the channel
	 * para *: the parameter (NULL for paramless modes)
	 * int: check type (see EXCHK_*)
	 * int: what (MODE_ADD or MODE_DEL)
	 * return value: 1=ok, 0=bad
	 */
	int			(*is_ok)(Client *,Channel *, char mode, char *para, int, int);

	/** NOTE: The routines below are NULL for paramless modes */
	
	/** Store parameter in memory for channel.
	 * aExtCMtableParam *: the list (usually chptr->mode.extmodeparams).
	 * char *: the parameter.
	 * return value: the head of the list, RTFS if you wonder why.
	 * design notes: only alloc a new paramstruct if you need to, search for
	 * any current one first (like in case of mode +y 5 and then +y 6 later without -y).
	 */
	void *		(*put_param)(void *, char *);

	/** Get readable string version" of the stored parameter.
	 * aExtCMtableParam *: the list (usually chptr->mode.extmodeparams).
	 * return value: a pointer to the string (temp. storage)
	 */
	char *		(*get_param)(void *);

	/** Convert input parameter to output.
	 * Like +l "1aaa" becomes "1".
	 * char *: the input parameter.
	 * Client *: the client that the mode request came from:
	 *            1. Can be NULL! (eg: if called for set::modes-on-join)
	 *            2. Probably only used in rare cases, see also next remark
	 *            3. ERRORS SHOULD NOT BE SENT BY conv_param BUT BY is_ok!
	 * return value: pointer to output string (temp. storage)
	 */
	char *		(*conv_param)(char *, Client *);

	/** free and remove parameter from list.
	 * aExtCMtableParam *: the list (usually chptr->mode.extmodeparams).
	 */
	void		(*free_param)(void *);

	/** duplicate a struct and return a pointer to duplicate.
	 * This is usually just a malloc + memcpy.
	 * aExtCMtableParam *: source struct itself (no list).
	 * return value: pointer to newly allocated struct.
	 */
	void *	(*dup_struct)(void *);

	/** Compares 2 parameters and decides who wins the sjoin fight.
	 * When syncing channel modes (cmd_sjoin) a parameter conflict may occur, things like
	 * +l 5 vs +l 10. This function should determinate who wins the fight, this decision
	 * should of course not be random but the same at every server on the net.
	 * examples of such comparisons are "highest wins" (+l) and a strcmp() check (+k/+L).
	 * Channel *: channel the fight is about.
	 * aExtCMtableParam *: our parameter
	 * aExtCMtableParam *: their parameter
	 */
	int			(*sjoin_check)(Channel *, void *, void *);

	/** Local channel mode? Prevents remote servers from setting/unsetting this */
	char local;
	
	/** Unsetting also eats/requires a parameter. Unusual, but possible. */
	char unset_with_param;

	/** Is this mode being unloaded?
	 * This is set to 1 if the chanmode module providing this mode is unloaded
	 * and we are waiting to see if in our new round of loads a "new" chanmode
	 * module will popup to take this mode. This only happens during a rehash,
	 * should never be 0 outside an internal rehash.
	 */
	char unloaded;
	
	/* Slot#.. Can be used instead of GETPARAMSLOT() */
	int slot;
	
	/** Module owner */
        Module *owner;
} Cmode;

typedef struct {
	char		flag;
	int		paracount;
	int		(*is_ok)(Client *,Channel *, char mode, char *para, int, int);
	void *	(*put_param)(void *, char *);
	char *		(*get_param)(void *);
	char *		(*conv_param)(char *, Client *);
	void		(*free_param)(void *);
	void *	(*dup_struct)(void *);
	int		(*sjoin_check)(Channel *, void *, void *);
	char		local;
	char		unset_with_param;
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

/*** Extended bans ***/

#define EXBCHK_ACCESS		0 /* Check access */
#define EXBCHK_ACCESS_ERR	1 /* Check access and send error */
#define EXBCHK_PARAM		2 /* Check if the parameter is valid */

#define EXBTYPE_BAN			0 /* a ban */
#define EXBTYPE_EXCEPT		1 /* an except */
#define EXBTYPE_INVEX		2 /* an invite exception */

#define EXTBANTABLESZ		32

typedef enum ExtbanOptions { EXTBOPT_CHSVSMODE=0x1, EXTBOPT_ACTMODIFIER=0x2, EXTBOPT_NOSTACKCHILD=0x4, EXTBOPT_INVEX=0x8 } ExtbanOptions;

typedef struct {
	/** extbans module */
	Module *owner;
	/** extended ban character */
	char	flag;

	/** extban options */
	ExtbanOptions options;

	/** access checking [optional].
	 * Client *: the client
	 * Channel *: the channel
	 * para: the ban parameter
	 * int: check type (see EXBCHK_*)
	 * int: what (MODE_ADD or MODE_DEL)
	 * int: what2 (EXBTYPE_BAN or EXBTYPE_EXCEPT)
	 * return value: 1=ok, 0=bad
	 * NOTE: just set this of NULL if you want only +hoaq to place/remove bans as usual.
	 * NOTE2: This has not been tested yet!!
	 */
	int			(*is_ok)(Client *, Channel *, char *para, int, int, int);

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
	 * Client *: the client
	 * Channel *: the channel
	 * para: the ban entry
	 * int: a value of BANCHK_* (see struct.h)
	 * char **: optionally a message, can be NULL!! (for some BANCHK_ types)
	 * char **: optionally for setting an error message, can be NULL!!
	 */
	int			(*is_banned)(Client *sptr, Channel *chptr, char *para, int checktype, char **msg, char **errormsg);
} Extban;

typedef struct {
	char	flag;
	ExtbanOptions options;
	int			(*is_ok)(Client *, Channel *, char *para, int, int, int);
	char *			(*conv_param)(char *);
	int			(*is_banned)(Client *, Channel *, char *, int, char **, char **);
} ExtbanInfo;


typedef struct Command Command;
struct Command {
	Command *prev, *next;
	RealCommand *cmd;
};

typedef struct Versionflag Versionflag;
struct Versionflag {
	Versionflag *prev, *next;
	char flag;
	ModuleChild *parents;
};

/* This type needs a forward declaration: */
typedef struct MessageTagHandler MessageTagHandler;

#define CLICAP_FLAGS_NONE               0x0
#define CLICAP_FLAGS_ADVERTISE_ONLY     0x4

typedef struct ClientCapability ClientCapability;
struct ClientCapability {
	ClientCapability *prev, *next;
	char *name;                              /**< The name of the CAP */
	long cap;                                /**< The acptr->user->proto we should set (if any, can be 0, like for sts) */
	int flags;                               /**< A flag from CLICAP_FLAGS_* */
	int (*visible)(Client *);               /**< Should the capability be visible? Note: parameter may be NULL. [optional] */
	char *(*parameter)(Client *);           /**< CAP parameters. Note: parameter may be NULL. [optional] */
	MessageTagHandler *mtag_handler;         /**< For reverse dependency */
	Module *owner;                           /**< Module introducing this CAP. */
	char unloaded;                           /**< Internal flag to indicate module is being unloaded */
};

typedef struct {
	char *name;
	int flags;
	int (*visible)(Client *);
	char *(*parameter)(Client *);
} ClientCapabilityInfo;

#define MTAG_HANDLER_FLAGS_NONE			0x0
#define MTAG_HANDLER_FLAGS_NO_CAP_NEEDED	0x1

/** Message Tag Handler */
struct MessageTagHandler {
	MessageTagHandler *prev, *next;
	char *name;                                 /**< The name of the message-tag */
	int flags;                                  /**< A flag of MTAG_HANDLER_FLAGS_* */
	int (*is_ok)(Client *, char *, char *);    /**< Verify syntax and access rights */
	Module *owner;                              /**< Module introducing this CAP. */
	ClientCapability *clicap_handler;           /**< Client capability handler associated with this */
	char unloaded;                              /**< Internal flag to indicate module is being unloaded */
};

/** The struct used to register a message tag handler.
 * For documentation, see the MessageTagHandler struct.
 */
typedef struct {
	char *name;
	int flags;
	int (*is_ok)(Client *, char *, char *);
	ClientCapability *clicap_handler;
} MessageTagHandlerInfo;

/** Filter for history get requests */
typedef struct HistoryFilter HistoryFilter;
struct HistoryFilter {
    int last_lines;
    int last_seconds;
};

/** History Backend */
typedef struct HistoryBackend HistoryBackend;
struct HistoryBackend {
	HistoryBackend *prev, *next;
	char *name;                                   /**< The name of the history backend (eg: "mem") */
	int (*history_add)(char *object, MessageTag *mtags, char *line); /**< Add to history */
	int (*history_del)(char *object, int max_lines, long max_time); /**< Delete history, based on lines/time */
	int (*history_request)(Client *acptr, char *object, HistoryFilter *filter);  /**< Request history */
	int (*history_destroy)(char *object);  /**< Destroy history of this object completely */
	Module *owner;                                /**< Module introducing this */
	char unloaded;                                /**< Internal flag to indicate module is being unloaded */
};

/** The struct used to register a history backend.
 * For documentation, see the History Backend struct above.
 */
typedef struct {
	char *name;
	int (*history_add)(char *object, MessageTag *mtags, char *line);
	int (*history_del)(char *object, int max_lines, long max_time);
	int (*history_request)(Client *acptr, char *object, HistoryFilter *filter);
	int (*history_destroy)(char *object);
} HistoryBackendInfo;

struct Hook {
	Hook *prev, *next;
	int priority;
	int type;
	union {
		int (*intfunc)();
		void (*voidfunc)();
		char *(*pcharfunc)();
	} func;
	Module *owner;
};

struct Callback {
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
 * - efunctions are (usually) mandatory, while callbacks can be optional
 * - efunctions are meant for internal usage, so 3rd party modules are
 *   not allowed to add them.
 * - all efunctions are declared as function pointers in modules.c
 */
struct Efunction {
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

struct Hooktype {
	short id;
	char *string;
	ModuleChild *parents;
};

typedef struct ISupport ISupport;
struct ISupport {
	ISupport *prev, *next;
	char *token;
	char *value;
	Module *owner;
};

typedef struct ModuleObject {
	struct ModuleObject *prev, *next;
	ModuleObjectType type;
	union {
		Event *event;
		Hook *hook;
		Command *command;
		Hooktype *hooktype;
		Versionflag *versionflag;
		Snomask *snomask;
		Umode *umode;
		CommandOverride *cmdoverride;
		Extban *extban;
		Callback *callback;
		Efunction *efunction;
		ISupport *isupport;
		Cmode *cmode;
		ModDataInfo *moddata;
		OperClassValidator *validator;
		ClientCapability *clicap;
		MessageTagHandler *mtag;
		HistoryBackend *history_backend;
	} object;
} ModuleObject;

/*
 * What we use to keep track internally of the modules
*/

#define MODERR_NOERROR 0
#define MODERR_EXISTS  1
#define MODERR_NOSPACE 2
#define MODERR_INVALID 3
#define MODERR_NOTFOUND 4

extern unsigned int ModuleGetError(Module *module);
extern const char *ModuleGetErrorStr(Module *module);
extern unsigned int ModuleGetOptions(Module *module);
extern unsigned int ModuleSetOptions(Module *module, unsigned int options, int action);

struct Module
{
	struct Module *prev, *next;
	ModuleHeader    *header; /* The module's header */
#ifdef _WIN32
	HMODULE dll;		/* Return value of LoadLibrary */
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
	char *relpath;
	unsigned long mod_sys_version;
	unsigned int compiler_version;
};
/*
 * Symbol table
*/

#define MOD_OPT_PERM		0x0001 /* Permanent module (not unloadable) */
#define MOD_OPT_OFFICIAL	0x0002 /* Official module, do not set "tainted" */
#define MOD_OPT_PERM_RELOADABLE	0x0004 /* Module is semi-permanent: it can be re-loaded but not un-loaded */
#define MOD_OPT_GLOBAL		0x0008 /* Module is required to be loaded globally (i.e. across the entire network) */
#define MOD_Dep(name, container,module) {#name, (vFP *) &container, module}

/** Event structs */
struct Event {
	Event		*prev;		/**< Previous event (linked list) */
	Event		*next;		/**< Next event (linked list) */
	char		*name;		/**< Name of the event */
	long		every_msec;	/**< How often we should run this event */
	long		count;		/**< How many times this event should run (0 = infinite) */
	vFP		event;		/**< Actual function to call */
	void		*data;		/**< The data to pass in the function call */
	struct timeval	last_run;	/**< Last time this event ran */
	Module		*owner;		/**< To which module this event belongs */
};

#define EMOD_EVERY 0x0001
#define EMOD_HOWMANY 0x0002
#define EMOD_NAME 0x0004
#define EMOD_EVENT 0x0008
#define EMOD_DATA 0x0010

/** event struct information, for EventMod() only - see Event for documentation */
struct EventInfo {
	int flags;
	long count;
	time_t every_msec;
	char *name;
	vFP event;
	void *data;
};


extern MODVAR Hook		*Hooks[MAXHOOKTYPES];
extern MODVAR Hooktype		Hooktypes[MAXCUSTOMHOOKS];
extern MODVAR Callback *Callbacks[MAXCALLBACKS], *RCallbacks[MAXCALLBACKS];
extern MODVAR ClientCapability *clicaps;

extern Event *EventAdd(Module *module, char *name, vFP event, void *data, long every_msec, int count);
extern Event  *EventDel(Event *event);
extern Event *EventMarkDel(Event *event);
extern Event *EventFind(char *name);
extern int EventMod(Event *event, EventInfo *mods);
extern void DoEvents(void);
extern void EventStatus(Client *sptr);
extern void SetupEvents(void);


extern void    Module_Init(void);
extern char    *Module_Create(char *path);
extern char    *Module_TransformPath(char *path_);
extern void    Init_all_testing_modules(void);
extern void    Unload_all_loaded_modules(void);
extern void    Unload_all_testing_modules(void);
extern int     Module_Unload(char *name);
extern vFP     Module_Sym(char *name);
extern vFP     Module_SymX(char *name, Module **mptr);
extern int	Module_free(Module *mod);

#ifdef __OpenBSD__
extern void *obsd_dlsym(void *handle, char *symbol);
#endif

#ifdef _WIN32
extern const char *our_dlerror(void);
#endif

extern Versionflag *VersionflagAdd(Module *module, char flag);
extern void VersionflagDel(Versionflag *vflag, Module *module);

extern ISupport *ISupportAdd(Module *module, const char *token, const char *value);
extern void ISupportSetValue(ISupport *isupport, const char *value);
extern void ISupportDel(ISupport *isupport);
extern ISupport *ISupportFind(const char *token);
extern void ISupportSet(Module *module, const char *name, const char *value);
extern void ISupportSetFmt(Module *module, const char *name, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,3,4)));
extern void ISupportDelByName(const char *name);

extern ClientCapability *ClientCapabilityFind(const char *token, Client *sptr);
extern ClientCapability *ClientCapabilityFindReal(const char *token);
extern ClientCapability *ClientCapabilityAdd(Module *module, ClientCapabilityInfo *clicap_request, long *cap);
extern void ClientCapabilityDel(ClientCapability *clicap);

extern MessageTagHandler *MessageTagHandlerFind(const char *token);
extern MessageTagHandler *MessageTagHandlerAdd(Module *module, MessageTagHandlerInfo *mreq);
extern void MessageTagHandlerDel(MessageTagHandler *m);

extern HistoryBackend *HistoryBackendFind(const char *name);
extern HistoryBackend *HistoryBackendAdd(Module *module, HistoryBackendInfo *mreq);
extern void HistoryBackendDel(HistoryBackend *m);

#ifndef GCC_TYPECHECKING
#define HookAdd(module, hooktype, priority, func) HookAddMain(module, hooktype, priority, func, NULL, NULL)
#define HookAddVoid(module, hooktype, priority, func) HookAddMain(module, hooktype, priority, NULL, func, NULL)
#define HookAddPChar(module, hooktype, priority, func) HookAddMain(module, hooktype, priority, NULL, NULL, func)
#else
#define HookAdd(module, hooktype, priority, func) \
__extension__ ({ \
	ValidateHooks(hooktype, func); \
    HookAddMain(module, hooktype, priority, func, NULL, NULL); \
})

#define HookAddVoid(module, hooktype, priority, func) \
__extension__ ({ \
	ValidateHooks(hooktype, func); \
    HookAddMain(module, hooktype, priority, NULL, func, NULL); \
})

#define HookAddPChar(module, hooktype, priority, func) \
__extension__ ({ \
	ValidateHooks(hooktype, func); \
    HookAddMain(module, hooktype, priority, NULL, NULL, func); \
})
#endif /* GCC_TYPCHECKING */

extern Hook	*HookAddMain(Module *module, int hooktype, int priority, int (*intfunc)(), void (*voidfunc)(), char *(*pcharfunc)());
extern Hook	*HookDel(Hook *hook);

extern Hooktype *HooktypeAdd(Module *module, char *string, int *type);
extern void HooktypeDel(Hooktype *hooktype, Module *module);

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
#define RunHookReturnInt3(hooktype,x,y,z,retchk) \
{ \
 int retval; \
 Hook *h; \
 for (h = Hooks[hooktype]; h; h = h->next) \
 { \
  retval = (*(h->func.intfunc))(x,y,z); \
  if (retval retchk) return retval; \
 } \
}
#define RunHookReturnInt4(hooktype,a,b,c,d,retchk) \
{ \
 int retval; \
 Hook *h; \
 for (h = Hooks[hooktype]; h; h = h->next) \
 { \
  retval = (*(h->func.intfunc))(a,b,c,d); \
  if (retval retchk) return retval; \
 } \
}

#define RunHookReturnVoid(hooktype,x,ret) do { Hook *hook; for (hook = Hooks[hooktype]; hook; hook = hook->next) if((*(hook->func.intfunc))(x) ret) return; } while(0)
#define RunHook2(hooktype,x,y) do { Hook *hook; for (hook = Hooks[hooktype]; hook; hook = hook->next) (*(hook->func.intfunc))(x,y); } while(0)
#define RunHook3(hooktype,a,b,c) do { Hook *hook; for (hook = Hooks[hooktype]; hook; hook = hook->next) (*(hook->func.intfunc))(a,b,c); } while(0)
#define RunHook4(hooktype,a,b,c,d) do { Hook *hook; for (hook = Hooks[hooktype]; hook; hook = hook->next) (*(hook->func.intfunc))(a,b,c,d); } while(0)
#define RunHook5(hooktype,a,b,c,d,e) do { Hook *hook; for (hook = Hooks[hooktype]; hook; hook = hook->next) (*(hook->func.intfunc))(a,b,c,d,e); } while(0)
#define RunHook6(hooktype,a,b,c,d,e,f) do { Hook *hook; for (hook = Hooks[hooktype]; hook; hook = hook->next) (*(hook->func.intfunc))(a,b,c,d,e,f); } while(0)
#define RunHook7(hooktype,a,b,c,d,e,f,g) do { Hook *hook; for (hook = Hooks[hooktype]; hook; hook = hook->next) (*(hook->func.intfunc))(a,b,c,d,e,f,g); } while(0)
#define RunHook8(hooktype,a,b,c,d,e,f,g,h) do { Hook *hook; for (hook = Hooks[hooktype]; hook; hook = hook->next) (*(hook->func.intfunc))(a,b,c,d,e,f,g,h); } while(0)

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

extern Efunction *EfunctionAddMain(Module *module, EfunctionType eftype, int (*intfunc)(), void (*voidfunc)(), void *(*pvoidfunc)(), char *(*pcharfunc)());
extern Efunction *EfunctionDel(Efunction *cb);

extern Command *CommandAdd(Module *module, char *cmd, CmdFunc func, unsigned char params, int flags);
extern Command *AliasAdd(Module *module, char *cmd, AliasCmdFunc aliasfunc, unsigned char params, int flags);
extern void CommandDel(Command *command);
extern void CommandDelX(Command *command, RealCommand *cmd);
extern int CommandExists(char *name);
extern CommandOverride *CommandOverrideAdd(Module *module, char *cmd, OverrideCmdFunc func);
extern CommandOverride *CommandOverrideAddEx(Module *module, char *name, int priority, OverrideCmdFunc func);
extern void CommandOverrideDel(CommandOverride *ovr);
extern int CallCommandOverride(CommandOverride *ovr, Client *sptr, MessageTag *mtags, int parc, char *parv[]);

extern void moddata_free_client(Client *acptr);
extern void moddata_free_local_client(Client *acptr);
extern void moddata_free_channel(Channel *chptr);
extern void moddata_free_member(Member *m);
extern void moddata_free_membership(Membership *m);
extern ModDataInfo *findmoddata_byname(char *name, ModDataType type);
extern int moddata_client_set(Client *acptr, char *varname, char *value);
extern char *moddata_client_get(Client *acptr, char *varname);
extern int moddata_local_client_set(Client *acptr, char *varname, char *value);
extern char *moddata_local_client_get(Client *acptr, char *varname);

extern int LoadPersistentPointerX(ModuleInfo *modinfo, char *varshortname, void **var, void (*free_variable)(ModData *m));
#define LoadPersistentPointer(modinfo, var, free_variable) LoadPersistentPointerX(modinfo, #var, (void **)&var, free_variable)
extern void SavePersistentPointerX(ModuleInfo *modinfo, char *varshortname, void *var);
#define SavePersistentPointer(modinfo, var) SavePersistentPointerX(modinfo, #var, var)

extern int LoadPersistentIntX(ModuleInfo *modinfo, char *varshortname, int *var);
#define LoadPersistentInt(modinfo, var) LoadPersistentIntX(modinfo, #var, &var)
extern void SavePersistentIntX(ModuleInfo *modinfo, char *varshortname, int var);
#define SavePersistentInt(modinfo, var) SavePersistentIntX(modinfo, #var, var)

extern int LoadPersistentLongX(ModuleInfo *modinfo, char *varshortname, long *var);
#define LoadPersistentLong(modinfo, var) LoadPersistentIntX(modinfo, #var, &var)
extern void SavePersistentLongX(ModuleInfo *modinfo, char *varshortname, long var);
#define SavePersistentLong(modinfo, var) SavePersistentLongX(modinfo, #var, var)

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
#define HOOKTYPE_POST_SERVER_CONNECT 48
#define HOOKTYPE_RAWPACKET_IN 49
#define HOOKTYPE_LOCAL_NICKPASS 50
#define HOOKTYPE_PACKET 51
#define HOOKTYPE_HANDSHAKE 52
#define HOOKTYPE_AWAY 53
#define HOOKTYPE_INVITE 55
#define HOOKTYPE_CAN_JOIN 56
#define HOOKTYPE_CAN_SEND 57
#define HOOKTYPE_CAN_KICK 58
#define HOOKTYPE_FREE_CLIENT 59
#define HOOKTYPE_FREE_USER 60
#define HOOKTYPE_PRE_CHANMSG 61
#define HOOKTYPE_PRE_USERMSG 62
#define HOOKTYPE_KNOCK 63
#define HOOKTYPE_MODECHAR_ADD 64
#define HOOKTYPE_MODECHAR_DEL 65
#define HOOKTYPE_CAN_JOIN_LIMITEXCEEDED 67
#define HOOKTYPE_VISIBLE_IN_CHANNEL 68
#define HOOKTYPE_PRE_LOCAL_CHANMODE 69
#define HOOKTYPE_PRE_REMOTE_CHANMODE 70
#define HOOKTYPE_JOIN_DATA 71
#define HOOKTYPE_PRE_KNOCK 72
#define HOOKTYPE_PRE_INVITE 73
#define HOOKTYPE_OPER_INVITE_BAN 74
#define HOOKTYPE_VIEW_TOPIC_OUTSIDE_CHANNEL 75
#define HOOKTYPE_CHAN_PERMIT_NICK_CHANGE 76
#define HOOKTYPE_IS_CHANNEL_SECURE 77
#define HOOKTYPE_SEND_CHANNEL 78
#define HOOKTYPE_CHANNEL_SYNCED 79
#define HOOKTYPE_CAN_SAJOIN 80
#define HOOKTYPE_WHOIS 81
#define HOOKTYPE_CHECK_INIT 82
#define HOOKTYPE_WHO_STATUS 83
#define HOOKTYPE_MODE_DEOP 84
#define HOOKTYPE_PRE_KILL 85
#define HOOKTYPE_SEE_CHANNEL_IN_WHOIS 86
#define HOOKTYPE_DCC_DENIED 87
#define HOOKTYPE_SERVER_HANDSHAKE_OUT 88
#define HOOKTYPE_SERVER_SYNCHED	89
#define HOOKTYPE_SECURE_CONNECT 90
#define HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION 91
#define HOOKTYPE_REQUIRE_SASL 92
#define HOOKTYPE_SASL_CONTINUATION 93
#define HOOKTYPE_SASL_RESULT 94
#define HOOKTYPE_PLACE_HOST_BAN 95
#define HOOKTYPE_FIND_TKLINE_MATCH 96
#define HOOKTYPE_WELCOME 97
#define HOOKTYPE_PRE_COMMAND 98
#define HOOKTYPE_POST_COMMAND 99
#define HOOKTYPE_NEW_MESSAGE 100
#define HOOKTYPE_IS_HANDSHAKE_FINISHED 101
#define HOOKTYPE_PRE_LOCAL_QUIT_CHAN 102
#define HOOKTYPE_IDENT_LOOKUP 103
#define HOOKTYPE_CONFIGRUN_EX 104

/* Adding a new hook here?
 * 1) Add the #define HOOKTYPE_.... with a new number
 * 2) Add a hook prototype (see below)
 * 3) Add type checking (even more below)
 * 4) Document the hook at https://www.unrealircd.org/docs/Dev:Hook_API
 */

/* Hook prototypes */
int hooktype_local_connect(Client *sptr);
int hooktype_remote_connect(Client *sptr);
int hooktype_server_connect(Client *sptr);
int hooktype_post_server_connect(Client *sptr);
char *hooktype_pre_local_quit(Client *sptr, char *comment);
int hooktype_local_quit(Client *sptr, MessageTag *mtags, char *comment);
int hooktype_remote_quit(Client *sptr, MessageTag *mtags, char *comment);
int hooktype_unkuser_quit(Client *sptr, MessageTag *mtags, char *comment);
int hooktype_pre_local_connect(Client *sptr);
int hooktype_server_quit(Client *sptr, MessageTag *mtags);
int hooktype_local_nickchange(Client *sptr, char *newnick);
int hooktype_remote_nickchange(Client *sptr, char *newnick);
int hooktype_can_join(Client *sptr, Channel *chptr, char *key, char *parv[]);
int hooktype_pre_local_join(Client *sptr, Channel *chptr, char *parv[]);
int hooktype_local_join(Client *sptr, Channel *chptr, MessageTag *mtags, char *parv[]);
int hooktype_remote_join(Client *sptr, Channel *chptr, MessageTag *mtags, char *parv[]);
char *hooktype_pre_local_part(Client *sptr, Channel *chptr, char *comment);
int hooktype_local_part(Client *sptr, Channel *chptr, MessageTag *mtags, char *comment);
int hooktype_remote_part(Client *sptr, Channel *chptr, MessageTag *mtags, char *comment);
char *hooktype_pre_local_kick(Client *sptr, Client *victim, Channel *chptr, char *comment);
int hooktype_can_kick(Client *sptr, Client *victim, Channel *chptr, char *comment, long sptr_flags, long victim_flags, char **error);
int hooktype_local_kick(Client *sptr, Client *victim, Channel *chptr, MessageTag *mtags, char *comment);
int hooktype_remote_kick(Client *sptr, Client *victim, Channel *chptr, MessageTag *mtags, char *comment);
char *hooktype_pre_usermsg(Client *sptr, Client *to, char *text, int notice);
int hooktype_usermsg(Client *sptr, Client *to, MessageTag *mtags, char *text, int notice);
int hooktype_can_send(Client *sptr, Channel *chptr, Membership *member, char **text, char **errmsg, int notice);
char *hooktype_pre_chanmsg(Client *sptr, Channel *chptr, MessageTag *mtags, char *text, int notice);
int hooktype_chanmsg(Client *sptr, Channel *chptr, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, int notice);
char *hooktype_pre_local_topic(Client *sptr, Channel *chptr, char *topic);
int hooktype_local_topic(Client *sptr, Channel *chptr, char *topic);
int hooktype_topic(Client *sptr, Channel *chptr, char *topic);
int hooktype_pre_local_chanmode(Client *sptr, Channel *chptr, MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode);
int hooktype_pre_remote_chanmode(Client *sptr, Channel *chptr, MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode);
int hooktype_local_chanmode(Client *sptr, Channel *chptr, MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode);
int hooktype_remote_chanmode(Client *sptr, Channel *chptr, MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode);
int hooktype_modechar_del(Channel *chptr, int modechar);
int hooktype_modechar_add(Channel *chptr, int modechar);
int hooktype_away(Client *sptr, MessageTag *mtags, char *reason);
int hooktype_pre_invite(Client *sptr, Client *acptr, Channel *chptr, int *override);
int hooktype_invite(Client *from, Client *to, Channel *chptr, MessageTag *mtags);
int hooktype_pre_knock(Client *sptr, Channel *chptr);
int hooktype_knock(Client *sptr, Channel *chptr, MessageTag *mtags, char *comment);
int hooktype_whois(Client *sptr, Client *target);
int hooktype_who_status(Client *sptr, Client *target, Channel *chptr, Member *member, char *status, int cansee);
int hooktype_pre_kill(Client *sptr, Client *victim, char *killpath);
int hooktype_local_kill(Client *sptr, Client *victim, char *comment);
int hooktype_rehashflag(Client *sptr, char *str);
int hooktype_configposttest(int *errors);
int hooktype_rehash(void);
int hooktype_stats(Client *sptr, char *str);
int hooktype_configtest(ConfigFile *cfptr, ConfigEntry *ce, int section, int *errors);
int hooktype_configrun(ConfigFile *cfptr, ConfigEntry *ce, int section);
int hooktype_configrun_ex(ConfigFile *cfptr, ConfigEntry *ce, int section, void *ptr);
int hooktype_local_oper(Client *sptr, int add);
int hooktype_local_pass(Client *sptr, char *password);
int hooktype_channel_create(Client *sptr, Channel *chptr);
int hooktype_channel_destroy(Channel *chptr, int *should_destroy);
int hooktype_tkl_except(Client *cptr, int ban_type);
int hooktype_umode_change(Client *sptr, long setflags, long newflags);
int hooktype_rehash_complete(void);
int hooktype_tkl_add(Client *sptr, TKL *tkl);
int hooktype_tkl_del(Client *sptr, TKL *tkl);
int hooktype_log(int flags, char *timebuf, char *buf);
int hooktype_local_spamfilter(Client *acptr, char *str, char *str_in, int type, char *target, TKL *tkl);
int hooktype_silenced(Client *sptr, Client *to, int notice);
int hooktype_rawpacket_in(Client *sptr, char *readbuf, int *length);
int hooktype_local_nickpass(Client *sptr, Client *nickserv);
int hooktype_packet(Client *from, Client *to, Client *intended_to, char **msg, int *length);
int hooktype_handshake(Client *sptr);
int hooktype_free_client(Client *acptr);
int hooktype_free_user(ClientUser *user, Client *acptr);
int hooktype_can_join_limitexceeded(Client *sptr, Channel *chptr, char *key, char *parv[]);
int hooktype_visible_in_channel(Client *sptr, Channel *chptr);
int hooktype_join_data(Client *who, Channel *chptr);
int hooktype_oper_invite_ban(Client *sptr, Channel *chptr);
int hooktype_view_topic_outside_channel(Client *sptr, Channel *chptr);
int hooktype_chan_permit_nick_change(Client *sptr, Channel *chptr);
int hooktype_is_channel_secure(Channel *chptr);
int hooktype_can_send_secure(Client *sptr, Channel *chptr);
int hooktype_channel_synced(Channel *chptr, int merge, int removetheirs, int nomode);
int hooktype_can_sajoin(Client *target, Channel *chptr, Client *sptr);
int hooktype_check_init(Client *cptr, char *sockname, size_t size);
int hooktype_mode_deop(Client *sptr, Client *victim, Channel *chptr, u_int what, int modechar, long my_access, char **badmode);
int hooktype_see_channel_in_whois(Client *sptr, Client *target, Channel *chptr);
int hooktype_dcc_denied(Client *sptr, Client *target, char *realfile, char *displayfile, ConfigItem_deny_dcc *denydcc);
int hooktype_server_handshake_out(Client *sptr);
int hooktype_server_synched(Client *sptr);
int hooktype_secure_connect(Client *sptr);
int hooktype_can_bypass_channel_message_restriction(Client *sptr, Channel *chptr, BypassChannelMessageRestrictionType bypass_type);
int hooktype_require_sasl(Client *sptr, char *reason);
int hooktype_sasl_continuation(Client *sptr, char *buf);
int hooktype_sasl_result(Client *sptr, int success);
int hooktype_place_host_ban(Client *sptr, int action, char *reason, long duration);
int hooktype_find_tkline_match(Client *sptr, TKL *tk);
int hooktype_welcome(Client *sptr, int after_numeric);
int hooktype_pre_command(Client *from, MessageTag *mtags, char *buf);
int hooktype_post_command(Client *from, MessageTag *mtags, char *buf);
void hooktype_new_message(Client *sender, MessageTag *recv_mtags, MessageTag **mtag_list, char *signature);
int hooktype_is_handshake_finished(Client *acptr);
char *hooktype_pre_local_quit_chan(Client *sptr, Channel *chptr, char *comment);
int hooktype_ident_lookup(Client *acptr);

#ifdef GCC_TYPECHECKING
#define ValidateHook(validatefunc, func) __builtin_types_compatible_p(__typeof__(func), __typeof__(validatefunc))

_UNREAL_ERROR(_hook_error_incompatible, "Incompatible hook function. Check arguments and return type of function.")

#define ValidateHooks(hooktype, func) \
    if (((hooktype == HOOKTYPE_LOCAL_QUIT) && !ValidateHook(hooktype_local_quit, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_NICKCHANGE) && !ValidateHook(hooktype_local_nickchange, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_CONNECT) && !ValidateHook(hooktype_local_connect, func)) || \
        ((hooktype == HOOKTYPE_REHASHFLAG) && !ValidateHook(hooktype_rehashflag, func)) || \
        ((hooktype == HOOKTYPE_PRE_LOCAL_PART) && !ValidateHook(hooktype_pre_local_part, func)) || \
        ((hooktype == HOOKTYPE_CONFIGPOSTTEST) && !ValidateHook(hooktype_configposttest, func)) || \
        ((hooktype == HOOKTYPE_REHASH) && !ValidateHook(hooktype_rehash, func)) || \
        ((hooktype == HOOKTYPE_PRE_LOCAL_CONNECT) && !ValidateHook(hooktype_pre_local_connect, func)) || \
        ((hooktype == HOOKTYPE_PRE_LOCAL_QUIT) && !ValidateHook(hooktype_pre_local_quit, func)) || \
        ((hooktype == HOOKTYPE_SERVER_CONNECT) && !ValidateHook(hooktype_server_connect, func)) || \
        ((hooktype == HOOKTYPE_SERVER_QUIT) && !ValidateHook(hooktype_server_quit, func)) || \
        ((hooktype == HOOKTYPE_STATS) && !ValidateHook(hooktype_stats, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_JOIN) && !ValidateHook(hooktype_local_join, func)) || \
        ((hooktype == HOOKTYPE_CONFIGTEST) && !ValidateHook(hooktype_configtest, func)) || \
        ((hooktype == HOOKTYPE_CONFIGRUN) && !ValidateHook(hooktype_configrun, func)) || \
        ((hooktype == HOOKTYPE_USERMSG) && !ValidateHook(hooktype_usermsg, func)) || \
        ((hooktype == HOOKTYPE_CHANMSG) && !ValidateHook(hooktype_chanmsg, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_PART) && !ValidateHook(hooktype_local_part, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_KICK) && !ValidateHook(hooktype_local_kick, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_CHANMODE) && !ValidateHook(hooktype_local_chanmode, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_TOPIC) && !ValidateHook(hooktype_local_topic, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_OPER) && !ValidateHook(hooktype_local_oper, func)) || \
        ((hooktype == HOOKTYPE_UNKUSER_QUIT) && !ValidateHook(hooktype_unkuser_quit, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_PASS) && !ValidateHook(hooktype_local_pass, func)) || \
        ((hooktype == HOOKTYPE_REMOTE_CONNECT) && !ValidateHook(hooktype_remote_connect, func)) || \
        ((hooktype == HOOKTYPE_REMOTE_QUIT) && !ValidateHook(hooktype_remote_quit, func)) || \
        ((hooktype == HOOKTYPE_PRE_LOCAL_JOIN) && !ValidateHook(hooktype_pre_local_join, func)) || \
        ((hooktype == HOOKTYPE_PRE_LOCAL_KICK) && !ValidateHook(hooktype_pre_local_kick, func)) || \
        ((hooktype == HOOKTYPE_PRE_LOCAL_TOPIC) && !ValidateHook(hooktype_pre_local_topic, func)) || \
        ((hooktype == HOOKTYPE_REMOTE_NICKCHANGE) && !ValidateHook(hooktype_remote_nickchange, func)) || \
        ((hooktype == HOOKTYPE_CHANNEL_CREATE) && !ValidateHook(hooktype_channel_create, func)) || \
        ((hooktype == HOOKTYPE_CHANNEL_DESTROY) && !ValidateHook(hooktype_channel_destroy, func)) || \
        ((hooktype == HOOKTYPE_REMOTE_CHANMODE) && !ValidateHook(hooktype_remote_chanmode, func)) || \
        ((hooktype == HOOKTYPE_TKL_EXCEPT) && !ValidateHook(hooktype_tkl_except, func)) || \
        ((hooktype == HOOKTYPE_UMODE_CHANGE) && !ValidateHook(hooktype_umode_change, func)) || \
        ((hooktype == HOOKTYPE_TOPIC) && !ValidateHook(hooktype_topic, func)) || \
        ((hooktype == HOOKTYPE_REHASH_COMPLETE) && !ValidateHook(hooktype_rehash_complete, func)) || \
        ((hooktype == HOOKTYPE_TKL_ADD) && !ValidateHook(hooktype_tkl_add, func)) || \
        ((hooktype == HOOKTYPE_TKL_DEL) && !ValidateHook(hooktype_tkl_del, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_KILL) && !ValidateHook(hooktype_local_kill, func)) || \
        ((hooktype == HOOKTYPE_LOG) && !ValidateHook(hooktype_log, func)) || \
        ((hooktype == HOOKTYPE_REMOTE_JOIN) && !ValidateHook(hooktype_remote_join, func)) || \
        ((hooktype == HOOKTYPE_REMOTE_PART) && !ValidateHook(hooktype_remote_part, func)) || \
        ((hooktype == HOOKTYPE_REMOTE_KICK) && !ValidateHook(hooktype_remote_kick, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_SPAMFILTER) && !ValidateHook(hooktype_local_spamfilter, func)) || \
        ((hooktype == HOOKTYPE_SILENCED) && !ValidateHook(hooktype_silenced, func)) || \
        ((hooktype == HOOKTYPE_POST_SERVER_CONNECT) && !ValidateHook(hooktype_post_server_connect, func)) || \
        ((hooktype == HOOKTYPE_RAWPACKET_IN) && !ValidateHook(hooktype_rawpacket_in, func)) || \
        ((hooktype == HOOKTYPE_LOCAL_NICKPASS) && !ValidateHook(hooktype_local_nickpass, func)) || \
        ((hooktype == HOOKTYPE_PACKET) && !ValidateHook(hooktype_packet, func)) || \
        ((hooktype == HOOKTYPE_HANDSHAKE) && !ValidateHook(hooktype_handshake, func)) || \
        ((hooktype == HOOKTYPE_AWAY) && !ValidateHook(hooktype_away, func)) || \
        ((hooktype == HOOKTYPE_INVITE) && !ValidateHook(hooktype_invite, func)) || \
        ((hooktype == HOOKTYPE_CAN_JOIN) && !ValidateHook(hooktype_can_join, func)) || \
        ((hooktype == HOOKTYPE_CAN_SEND) && !ValidateHook(hooktype_can_send, func)) || \
        ((hooktype == HOOKTYPE_CAN_KICK) && !ValidateHook(hooktype_can_kick, func)) || \
        ((hooktype == HOOKTYPE_FREE_CLIENT) && !ValidateHook(hooktype_free_client, func)) || \
        ((hooktype == HOOKTYPE_FREE_USER) && !ValidateHook(hooktype_free_user, func)) || \
        ((hooktype == HOOKTYPE_PRE_CHANMSG) && !ValidateHook(hooktype_pre_chanmsg, func)) || \
        ((hooktype == HOOKTYPE_PRE_USERMSG) && !ValidateHook(hooktype_pre_usermsg, func)) || \
        ((hooktype == HOOKTYPE_KNOCK) && !ValidateHook(hooktype_knock, func)) || \
        ((hooktype == HOOKTYPE_MODECHAR_ADD) && !ValidateHook(hooktype_modechar_add, func)) || \
        ((hooktype == HOOKTYPE_MODECHAR_DEL) && !ValidateHook(hooktype_modechar_del, func)) || \
        ((hooktype == HOOKTYPE_CAN_JOIN_LIMITEXCEEDED) && !ValidateHook(hooktype_can_join_limitexceeded, func)) || \
        ((hooktype == HOOKTYPE_VISIBLE_IN_CHANNEL) && !ValidateHook(hooktype_visible_in_channel, func)) || \
        ((hooktype == HOOKTYPE_PRE_LOCAL_CHANMODE) && !ValidateHook(hooktype_pre_local_chanmode, func)) || \
        ((hooktype == HOOKTYPE_PRE_REMOTE_CHANMODE) && !ValidateHook(hooktype_pre_remote_chanmode, func)) || \
        ((hooktype == HOOKTYPE_JOIN_DATA) && !ValidateHook(hooktype_join_data, func)) || \
        ((hooktype == HOOKTYPE_PRE_KNOCK) && !ValidateHook(hooktype_pre_knock, func)) || \
        ((hooktype == HOOKTYPE_PRE_INVITE) && !ValidateHook(hooktype_pre_invite, func)) || \
        ((hooktype == HOOKTYPE_OPER_INVITE_BAN) && !ValidateHook(hooktype_oper_invite_ban, func)) || \
        ((hooktype == HOOKTYPE_VIEW_TOPIC_OUTSIDE_CHANNEL) && !ValidateHook(hooktype_view_topic_outside_channel, func)) || \
        ((hooktype == HOOKTYPE_CHAN_PERMIT_NICK_CHANGE) && !ValidateHook(hooktype_chan_permit_nick_change, func)) || \
        ((hooktype == HOOKTYPE_IS_CHANNEL_SECURE) && !ValidateHook(hooktype_is_channel_secure, func)) || \
        ((hooktype == HOOKTYPE_SEND_CHANNEL) && !ValidateHook(hooktype_can_send_secure, func)) || \
        ((hooktype == HOOKTYPE_CHANNEL_SYNCED) && !ValidateHook(hooktype_channel_synced, func)) || \
        ((hooktype == HOOKTYPE_CAN_SAJOIN) && !ValidateHook(hooktype_can_sajoin, func)) || \
        ((hooktype == HOOKTYPE_WHOIS) && !ValidateHook(hooktype_whois, func)) || \
        ((hooktype == HOOKTYPE_CHECK_INIT) && !ValidateHook(hooktype_check_init, func)) || \
        ((hooktype == HOOKTYPE_WHO_STATUS) && !ValidateHook(hooktype_who_status, func)) || \
        ((hooktype == HOOKTYPE_MODE_DEOP) && !ValidateHook(hooktype_mode_deop, func)) || \
        ((hooktype == HOOKTYPE_PRE_KILL) && !ValidateHook(hooktype_pre_kill, func)) || \
        ((hooktype == HOOKTYPE_SEE_CHANNEL_IN_WHOIS) && !ValidateHook(hooktype_see_channel_in_whois, func)) || \
        ((hooktype == HOOKTYPE_DCC_DENIED) && !ValidateHook(hooktype_dcc_denied, func)) || \
        ((hooktype == HOOKTYPE_SERVER_HANDSHAKE_OUT) && !ValidateHook(hooktype_server_handshake_out, func)) || \
        ((hooktype == HOOKTYPE_SERVER_SYNCHED) && !ValidateHook(hooktype_server_synched, func)) || \
        ((hooktype == HOOKTYPE_SECURE_CONNECT) && !ValidateHook(hooktype_secure_connect, func)) || \
        ((hooktype == HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION) && !ValidateHook(hooktype_can_bypass_channel_message_restriction, func)) || \
        ((hooktype == HOOKTYPE_REQUIRE_SASL) && !ValidateHook(hooktype_require_sasl, func)) || \
        ((hooktype == HOOKTYPE_SASL_CONTINUATION) && !ValidateHook(hooktype_sasl_continuation, func)) || \
        ((hooktype == HOOKTYPE_SASL_RESULT) && !ValidateHook(hooktype_sasl_result, func)) || \
        ((hooktype == HOOKTYPE_PLACE_HOST_BAN) && !ValidateHook(hooktype_place_host_ban, func)) || \
        ((hooktype == HOOKTYPE_FIND_TKLINE_MATCH) && !ValidateHook(hooktype_find_tkline_match, func)) || \
        ((hooktype == HOOKTYPE_WELCOME) && !ValidateHook(hooktype_welcome, func)) || \
        ((hooktype == HOOKTYPE_PRE_COMMAND) && !ValidateHook(hooktype_pre_command, func)) || \
        ((hooktype == HOOKTYPE_POST_COMMAND) && !ValidateHook(hooktype_post_command, func)) || \
        ((hooktype == HOOKTYPE_NEW_MESSAGE) && !ValidateHook(hooktype_new_message, func)) || \
        ((hooktype == HOOKTYPE_IS_HANDSHAKE_FINISHED) && !ValidateHook(hooktype_is_handshake_finished, func)) || \
        ((hooktype == HOOKTYPE_PRE_LOCAL_QUIT_CHAN) && !ValidateHook(hooktype_pre_local_quit_chan, func)) || \
        ((hooktype == HOOKTYPE_IDENT_LOOKUP) && !ValidateHook(hooktype_ident_lookup, func)) || \
        ((hooktype == HOOKTYPE_CONFIGRUN_EX) && !ValidateHook(hooktype_configrun_ex, func)) ) \
        _hook_error_incompatible();
#endif /* GCC_TYPECHECKING */

/* Hook return values */
#define HOOK_CONTINUE 0
#define HOOK_ALLOW -1
#define HOOK_DENY 1

/* Callback types */
#define CALLBACKTYPE_CLOAK 1
#define CALLBACKTYPE_CLOAKKEYCSUM 2
#define CALLBACKTYPE_CLOAK_EX 3
#define CALLBACKTYPE_BLACKLIST_CHECK 4
#define CALLBACKTYPE_REPUTATION_STARTTIME 5

/* To add a new efunction, only if you are an UnrealIRCd coder:
 * 1) Add a new entry here
 * 2) Add the function in src/api-efunctions.c
 * 3) Add the initalization in src/api-efunctions.c
 * 4) Add the extern entry in include/h.h in the
 *    section marked "Efuncs"
 */
/** Efunction types. */
enum EfunctionType {
	EFUNC_DO_JOIN=1,
	EFUNC_JOIN_CHANNEL,
	EFUNC_CAN_JOIN,
	EFUNC_DO_MODE,
	EFUNC_SET_MODE,
	EFUNC_CMD_UMODE,
	EFUNC_REGISTER_USER,
	EFUNC_TKL_HASH,
	EFUNC_TKL_TYPETOCHAR,
	EFUNC_TKL_ADD_SERVERBAN,
	EFUNC_TKL_DEL_LINE,
	EFUNC_TKL_CHECK_LOCAL_REMOVE_SHUN,
	EFUNC_TKL_EXPIRE,
	EFUNC_TKL_CHECK_EXPIRE,
	EFUNC_FIND_TKLINE_MATCH,
	EFUNC_FIND_SHUN,
	EFUNC_FIND_SPAMFILTER_USER,
	EFUNC_FIND_QLINE,
	EFUNC_FIND_TKLINE_MATCH_ZAP,
	EFUNC_TKL_STATS,
	EFUNC_TKL_SYNCH,
	EFUNC_CMD_TKL,
	EFUNC_PLACE_HOST_BAN,
	EFUNC_DOSPAMFILTER,
	EFUNC_DOSPAMFILTER_VIRUSCHAN,
	EFUNC_FIND_TKLINE_MATCH_ZAP_EX,
	EFUNC_SEND_LIST,
	EFUNC_STRIPCOLORS,
	EFUNC_STRIPCONTROLCODES,
	EFUNC_SPAMFILTER_BUILD_USER_STRING,
	EFUNC_SEND_PROTOCTL_SERVERS,
	EFUNC_VERIFY_LINK,
	EFUNC_SEND_SERVER_MESSAGE,
	EFUNC_BROADCAST_MD_CLIENT,
	EFUNC_BROADCAST_MD_CHANNEL,
	EFUNC_BROADCAST_MD_MEMBER,
	EFUNC_BROADCAST_MD_MEMBERSHIP,
	EFUNC_CHECK_BANNED,
	EFUNC_INTRODUCE_USER,
	EFUNC_CHECK_DENY_VERSION,
	EFUNC_BROADCAST_MD_CLIENT_CMD,
	EFUNC_BROADCAST_MD_CHANNEL_CMD,
	EFUNC_BROADCAST_MD_MEMBER_CMD,
	EFUNC_BROADCAST_MD_MEMBERSHIP_CMD,
	EFUNC_SEND_MODDATA_CLIENT,
	EFUNC_SEND_MODDATA_CHANNEL,
	EFUNC_SEND_MODDATA_MEMBERS,
	EFUNC_BROADCAST_MODDATA_CLIENT,
	EFUNC_MATCH_USER,
	EFUNC_USERHOST_SAVE_CURRENT,
	EFUNC_USERHOST_CHANGED,
	EFUNC_SEND_JOIN_TO_LOCAL_USERS,
	EFUNC_DO_NICK_NAME,
	EFUNC_DO_REMOTE_NICK_NAME,
	EFUNC_CHARSYS_GET_CURRENT_LANGUAGES,
	EFUNC_BROADCAST_SINFO,
	EFUNC_PARSE_MESSAGE_TAGS,
	EFUNC_MTAGS_TO_STRING,
	EFUNC_TKL_CHARTOTYPE,
	EFUNC_TKL_TYPE_STRING,
	EFUNC_CAN_SEND,
	EFUNC_BROADCAST_MD_GLOBALVAR,
	EFUNC_BROADCAST_MD_GLOBALVAR_CMD,
	EFUNC_TKL_IP_HASH,
	EFUNC_TKL_IP_HASH_TYPE,
	EFUNC_TKL_ADD_BANEXCEPTION,
	EFUNC_TKL_ADD_NAMEBAN,
	EFUNC_TKL_ADD_SPAMFILTER,
	EFUNC_SENDNOTICE_TKL_ADD,
	EFUNC_SENDNOTICE_TKL_DEL,
	EFUNC_FREE_TKL,
	EFUNC_FIND_TKL_SERVERBAN,
	EFUNC_FIND_TKL_BANEXCEPTION,
	EFUNC_FIND_TKL_NAMEBAN,
	EFUNC_FIND_TKL_SPAMFILTER,
	EFUNC_FIND_TKL_EXCEPTION,
	EFUNC_ADD_SILENCE,
	EFUNC_DEL_SILENCE,
	EFUNC_IS_SILENCED,
};

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
#define CONFIG_SET_ANTI_FLOOD 8
#define CONFIG_REQUIRE 9
#define CONFIG_LISTEN 10
#define CONFIG_LISTEN_OPTIONS 11

#define MOD_HEADER Mod_Header
#define MOD_TEST() DLLFUNC int Mod_Test(ModuleInfo *modinfo)
#define MOD_INIT() DLLFUNC int Mod_Init(ModuleInfo *modinfo)
#define MOD_LOAD() DLLFUNC int Mod_Load(ModuleInfo *modinfo)
#define MOD_UNLOAD() DLLFUNC int Mod_Unload(ModuleInfo *modinfo)

#define CLOAK_KEYCRC	RCallbacks[CALLBACKTYPE_CLOAKKEYCSUM] != NULL ? RCallbacks[CALLBACKTYPE_CLOAKKEYCSUM]->func.pcharfunc() : "nil"

#ifdef DYNAMIC_LINKING
 #include "modversion.h"
#endif

#endif

