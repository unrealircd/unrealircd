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
        int (*allowed)(Client *client, int what); /**< The 'is this user allowed to set this mode?' routine */
        char unloaded; /**< Internal flag to indicate module is being unloaded */
        Module *owner; /**< Module that owns this user mode */
} Umode;

typedef struct {
        long mode; /**< Snomask mask */
        char flag; /**< Snomask character */
        int unset_on_deoper; /**< When set to 1 then this snomask will be unset on de-oper */
        int (*allowed)(Client *client, int what); /**< The 'is this user allowed to set this snomask?' routine */
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
#define moddata_channel(channel, md)   channel->moddata[md->slot]
#define moddata_member(m, md)        m->moddata[md->slot]
#define moddata_membership(m, md)    m->moddata[md->slot]
#define moddata_local_variable(md)         local_variable_moddata[md->slot]
#define moddata_global_variable(md)        global_variable_moddata[md->slot]

/* Can bypass message restriction - Types */
typedef enum BypassChannelMessageRestrictionType {
	BYPASS_CHANMSG_EXTERNAL = 1,
	BYPASS_CHANMSG_MODERATED = 2,
	BYPASS_CHANMSG_COLOR = 3,
	BYPASS_CHANMSG_CENSOR = 4,
	BYPASS_CHANMSG_NOTICE = 5,
} BypassChannelMessageRestrictionType;

/** @defgroup ChannelModeAPI Channel mode API
 * @{
 */

#define EXCHK_ACCESS		0 /**< Check user access */
#define EXCHK_ACCESS_ERR	1 /**< Check user access and send error to user */
#define EXCHK_PARAM		2 /**< Check parameter */

/* return values for EXCHK_ACCESS*: */
#define EX_DENY			0  /**< MODE change disallowed, except in case of operoverride */
#define EX_ALLOW		1  /**< MODE change allowed */
#define EX_ALWAYS_DENY		-1 /**< MODE change disallowed, even in case of operoverride */

#define EXSJ_SAME		0 /**< SJOIN: Parameters are the same */
#define EXSJ_WEWON		1 /**< SJOIN: We won! w00t */
#define EXSJ_THEYWON		2 /**< SJOIN: They won :( */
#define EXSJ_MERGE		3 /**< SJOIN: Merging of modes, neither won nor lost */

/** Channel mode bit/value */
typedef unsigned long Cmode_t;

/** Channel mode handler.
 * This struct contains all extended channel mode information,
 * like the flag, mode, their handler functions, etc.
 *
 * @note For a channel mode without parameters you only need to set 'flag'
 * and set the 'is_ok' function. All the rest is for parameter modes
 * or is optional.
 */
typedef struct {
	/** mode character (like 'Z') */
	char		flag;

	/** unique flag (like 0x10) */
	Cmode_t		mode;

	/** Number of parameters (1 or 0) */
	int			paracount;

	/** Check access or parameter of the channel mode.
	 * @param client	The client
	 * @param channel	The channel
	 * @param para		The parameter (NULL for paramless modes)
	 * @param checkt	Check type, see EXCHK_* macros
	 * @param what		MODE_ADD or MODE_DEL
	 * @returns EX_DENY, EX_ALLOW or EX_ALWAYS_DENY
	 */
	int (*is_ok)(Client *client, Channel *channel, char mode, char *para, int checkt, int what);

	/** Store parameter in memory for channel.
	 * This function pointer is NULL (unused) for modes without parameters.
	 * @param list		The list, this usually points to channel->mode.extmodeparams.
	 * @param para		The parameter to store.
	 * @returns the head of the list, RTFS if you wonder why.
	 * @note IMPORTANT: only allocate a new paramstruct if you need to.
	 *       Search for any current one first! Eg: in case of mode +y 5 and then +y 6 later without -y.
	 */
	void *(*put_param)(void *list, char *para);

	/** Get the stored parameter as a readable/printable string.
	 * This function pointer is NULL (unused) for modes without parameters.
	 * @param parastruct	The parameter struct
	 * @returns a pointer to the string (temporary storage)
	 */
	char *(*get_param)(void *parastruct);

	/** Convert input parameter to output.
	 * This converts stuff like where a MODE +l "1aaa" becomes "1".
	 *
	 * This function pointer is NULL (unused) for modes without parameters.
	 * @param para		The input parameter.
	 * @param client	The client that the mode request came from (can be NULL)
	 * @returns pointer to output string (temporary storage)
	 * @note The 'client' field will be NULL if for example called for set::modes-on-join.
	 * @note You should probably not use 'client' in most cases.
	 *       In particular you MUST NOT SEND ERRORS to the client.
	 *       This should be done in is_ok() and not in conv_param().
	 */
	char *(*conv_param)(char *para, Client *client);

	/** Free and remove parameter from list.
	 * This function pointer is NULL (unused) for modes without parameters.
	 * @param parastruct		The parameter struct
	 * @note In most cases you will just call safe_free() on 'list'
	 */
	void (*free_param)(void *parastruct);

	/** duplicate a struct and return a pointer to duplicate.
	 * This function pointer is NULL (unused) for modes without parameters.
	 * @param parastruct		The parameter struct
	 * @returns pointer to newly allocated struct.
	 * @note In most cases you will simply safe_alloc() and memcpy()
	 */
	void *(*dup_struct)(void *parastruct);

	/** Compares two parameters and decides who wins the SJOIN fight.
	 * When syncing channel modes (cmd_sjoin) a parameter conflict may occur, things like
	 * "+l 5" vs "+l 10". This function should determinate who wins the fight.
	 * This decision should, of course, not be random. It needs to decide according to
	 * the same principles on all servers on the IRC network. Examples of such
	 * comparisons are "highest wins" (+l) and a strcmp() check (+k/+L).
	 * 
	 * This function pointer is NULL (unused) for modes without parameters.
	 * @param channel	The channel that the fight is about
	 * @param our		Our parameter struct
	 * @param their		Their parameter struct
	 */
	int (*sjoin_check)(Channel *channel, void *our, void *their);

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
	
	/** Slot number - Can be used instead of GETPARAMSLOT() */
	int slot;
	
	/** Module owner */
        Module *owner;
} Cmode;

/** The struct used to register a channel mode handler.
 * For documentation, see Cmode struct.
 */
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

/** Get a slot number for a param - eg GETPARAMSLOT('k') */
#define GETPARAMSLOT(x)	param_to_slot_mapping[x]

/** Get a cmode handler by slot - for example for [dont use this]: GETPARAMHANDLERBYSLOT(5)->get_param(channel) */
#define GETPARAMHANDLERBYSLOT(slotid)	ParamTable[slotid]

/** Same as GETPARAMHANDLERBYSLOT but then by letter - like [dont use this]: GETPARAMHANDLERBYSLOT('k')->get_param(channel) */
#define GETPARAMHANDLERBYLETTER(x)	ParamTable[GETPARAMSLOT(x)]

/** Get paramter data struct - for like: ((aModejEntry *)GETPARASTRUCT(channel, 'j'))->t */
#define GETPARASTRUCT(mychannel, mychar)	channel->mode.extmodeparams[GETPARAMSLOT(mychar)]

#define GETPARASTRUCTEX(v, mychar)	v[GETPARAMSLOT(mychar)]

/** @} */

#define CMP_GETSLOT(x) GETPARAMSLOT(x)
#define CMP_GETHANDLERBYSLOT(x) GETPARAMHANDLERBYSLOT(x)
#define CMP_GETHANDLERBYLETTER(x) GETPARAMHANDLERBYLETTER(x)
#define CMP_GETSTRUCT(x,y) GETPARASTRUCT(x,y)

/*** Extended bans ***/

// TODO: These should be enums!

#define EXBCHK_ACCESS		0 /* Check access */
#define EXBCHK_ACCESS_ERR	1 /* Check access and send error */
#define EXBCHK_PARAM		2 /* Check if the parameter is valid */

#define EXBTYPE_BAN		0 /* a ban */
#define EXBTYPE_EXCEPT		1 /* an except */
#define EXBTYPE_INVEX		2 /* an invite exception */
#define EXBTYPE_TKL		3 /* TKL or other generic matcher outside banning routines */

#define EXTBANTABLESZ		32

typedef enum ExtbanOptions {
        EXTBOPT_CHSVSMODE=0x1,		/**< SVSMODE -b/-e/-I will clear this ban */
        EXTBOPT_ACTMODIFIER=0x2,	/**< Action modifier (not a matcher). These are extended bans like ~q/~n/~j. */
        EXTBOPT_NOSTACKCHILD=0x4,	/**< Disallow prefixing with another extban. Eg disallow ~n:~T:censor:xyz */
        EXTBOPT_INVEX=0x8,		/**< Available for use with +I too */
        EXTBOPT_TKL=0x10		/**< Available for use in TKL's too (eg: /GLINE ~a:account) */
} ExtbanOptions;

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
	int			(*is_banned)(Client *client, Channel *channel, char *para, int checktype, char **msg, char **errormsg);
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

/** @defgroup MessagetagAPI Message tag API
 * @{
 */

/** No special message-tag handler flags */
#define MTAG_HANDLER_FLAGS_NONE			0x0
/** This message-tag does not have a CAP REQ xx (eg: for "msgid") */
#define MTAG_HANDLER_FLAGS_NO_CAP_NEEDED	0x1

/** Message Tag Handler */
struct MessageTagHandler {
	MessageTagHandler *prev, *next;
	char *name;                                 /**< The name of the message-tag */
	int flags;                                  /**< A flag of MTAG_HANDLER_FLAGS_* */
	int (*is_ok)(Client *, char *, char *);     /**< Verify syntax and access rights */
	int (*can_send)(Client *);                  /**< Tag may be sent to this client (normally NULL!) */
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
	int (*can_send)(Client *);
	ClientCapability *clicap_handler;
} MessageTagHandlerInfo;

/** @} */

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
	int (*history_set_limit)(char *object, int max_lines, long max_time); /**< Impose a limit on a history object */
	int (*history_add)(char *object, MessageTag *mtags, char *line); /**< Add to history */
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
	int (*history_set_limit)(char *object, int max_lines, long max_time);
	int (*history_add)(char *object, MessageTag *mtags, char *line);
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
	char		deleted;	/**< Set to 1 if this event is marked for deletion */
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
extern void   EventDel(Event *event);
extern Event *EventMarkDel(Event *event);
extern Event *EventFind(char *name);
extern int EventMod(Event *event, EventInfo *mods);
extern void DoEvents(void);
extern void EventStatus(Client *client);
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

extern ClientCapability *ClientCapabilityFind(const char *token, Client *client);
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
#define RunHookReturn(hooktype,x,retchk) \
{ \
 int retval; \
 Hook *h; \
 for (h = Hooks[hooktype]; h; h = h->next) \
 { \
  retval = (*(h->func.intfunc))(x); \
  if (retval retchk) return; \
 } \
}
#define RunHookReturn2(hooktype,x,y,retchk) \
{ \
 int retval; \
 Hook *h; \
 for (h = Hooks[hooktype]; h; h = h->next) \
 { \
  retval = (*(h->func.intfunc))(x,y); \
  if (retval retchk) return; \
 } \
}
#define RunHookReturn3(hooktype,x,y,z,retchk) \
{ \
 int retval; \
 Hook *h; \
 for (h = Hooks[hooktype]; h; h = h->next) \
 { \
  retval = (*(h->func.intfunc))(x,y,z); \
  if (retval retchk) return; \
 } \
}
#define RunHookReturn4(hooktype,a,b,c,d,retchk) \
{ \
 int retval; \
 Hook *h; \
 for (h = Hooks[hooktype]; h; h = h->next) \
 { \
  retval = (*(h->func.intfunc))(a,b,c,d); \
  if (retval retchk) return; \
 } \
}
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
extern void CallCommandOverride(CommandOverride *ovr, Client *client, MessageTag *mtags, int parc, char *parv[]);

extern void moddata_free_client(Client *acptr);
extern void moddata_free_local_client(Client *acptr);
extern void moddata_free_channel(Channel *channel);
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
#define LoadPersistentLong(modinfo, var) LoadPersistentLongX(modinfo, #var, &var)
extern void SavePersistentLongX(ModuleInfo *modinfo, char *varshortname, long var);
#define SavePersistentLong(modinfo, var) SavePersistentLongX(modinfo, #var, var)

/** Hooks trigger on "events", such as a new user connecting, joining a channel, etc.
 * You are suggested to use CTRL+F on this page to search for any useful hook
 * in the 'Function Documentation' below.
 *
 * Once you have found your hook, you can use it in the code,
 * see https://www.unrealircd.org/docs/Dev:Hook_API#How_to_have_your_module_.22hook_in.22.
 *
 * @defgroup HookAPI Hook API
 * @{
 */

/* Hook types */
/** See hooktype_pre_local_connect() */
#define HOOKTYPE_PRE_LOCAL_CONNECT	1
/** See hooktype_local_connect() */
#define HOOKTYPE_LOCAL_CONNECT	2
/** See hooktype_remote_connect() */
#define HOOKTYPE_REMOTE_CONNECT	3
/** See hooktype_pre_local_quit() */
#define HOOKTYPE_PRE_LOCAL_QUIT	4
/** See hooktype_local_quit() */
#define HOOKTYPE_LOCAL_QUIT	5
/** See hooktype_remote_quit() */
#define HOOKTYPE_REMOTE_QUIT	6
/** See hooktype_unkuser_quit() */
#define HOOKTYPE_UNKUSER_QUIT	7
/** See hooktype_server_connect() */
#define HOOKTYPE_SERVER_CONNECT	8
/** See hooktype_server_handshake_out() */
#define HOOKTYPE_SERVER_HANDSHAKE_OUT	9
/** See hooktype_server_sync() */
#define HOOKTYPE_SERVER_SYNC	10
/** See hooktype_post_server_connect() */
#define HOOKTYPE_POST_SERVER_CONNECT	11
/** See hooktype_server_synced() */
#define HOOKTYPE_SERVER_SYNCED	12
/** See hooktype_server_quit() */
#define HOOKTYPE_SERVER_QUIT	13
/** See hooktype_local_nickchange() */
#define HOOKTYPE_LOCAL_NICKCHANGE	14
/** See hooktype_remote_nickchange() */
#define HOOKTYPE_REMOTE_NICKCHANGE	15
/** See hooktype_can_join() */
#define HOOKTYPE_CAN_JOIN	16
/** See hooktype_pre_local_join() */
#define HOOKTYPE_PRE_LOCAL_JOIN	17
/** See hooktype_local_join() */
#define HOOKTYPE_LOCAL_JOIN	18
/** See hooktype_remote_join() */
#define HOOKTYPE_REMOTE_JOIN	19
/** See hooktype_pre_local_part() */
#define HOOKTYPE_PRE_LOCAL_PART	20
/** See hooktype_local_part() */
#define HOOKTYPE_LOCAL_PART	21
/** See hooktype_remote_part() */
#define HOOKTYPE_REMOTE_PART	22
/** See hooktype_pre_local_kick() */
#define HOOKTYPE_PRE_LOCAL_KICK	23
/** See hooktype_can_kick() */
#define HOOKTYPE_CAN_KICK	24
/** See hooktype_local_kick() */
#define HOOKTYPE_LOCAL_KICK	25
/** See hooktype_remote_kick() */
#define HOOKTYPE_REMOTE_KICK	26
/** See hooktype_pre_chanmsg() */
#define HOOKTYPE_PRE_CHANMSG	28
/** See hooktype_can_send_to_user() */
#define HOOKTYPE_CAN_SEND_TO_USER	29
/** See hooktype_can_send_to_channel() */
#define HOOKTYPE_CAN_SEND_TO_CHANNEL	30
/** See hooktype_usermsg() */
#define HOOKTYPE_USERMSG	31
/** See hooktype_chanmsg() */
#define HOOKTYPE_CHANMSG	32
/** See hooktype_pre_local_topic() */
#define HOOKTYPE_PRE_LOCAL_TOPIC	33
/** See hooktype_local_topic() */
#define HOOKTYPE_LOCAL_TOPIC	34
/** See hooktype_topic() */
#define HOOKTYPE_TOPIC	35
/** See hooktype_pre_local_chanmode() */
#define HOOKTYPE_PRE_LOCAL_CHANMODE	36
/** See hooktype_pre_remote_chanmode() */
#define HOOKTYPE_PRE_REMOTE_CHANMODE	37
/** See hooktype_local_chanmode() */
#define HOOKTYPE_LOCAL_CHANMODE	38
/** See hooktype_remote_chanmode() */
#define HOOKTYPE_REMOTE_CHANMODE	39
/** See hooktype_modechar_del() */
#define HOOKTYPE_MODECHAR_DEL	40
/** See hooktype_modechar_add() */
#define HOOKTYPE_MODECHAR_ADD	41
/** See hooktype_away() */
#define HOOKTYPE_AWAY	42
/** See hooktype_pre_invite() */
#define HOOKTYPE_PRE_INVITE	43
/** See hooktype_invite() */
#define HOOKTYPE_INVITE	44
/** See hooktype_pre_knock() */
#define HOOKTYPE_PRE_KNOCK	45
/** See hooktype_knock() */
#define HOOKTYPE_KNOCK	46
/** See hooktype_whois() */
#define HOOKTYPE_WHOIS	47
/** See hooktype_who_status() */
#define HOOKTYPE_WHO_STATUS	48
/** See hooktype_pre_kill() */
#define HOOKTYPE_PRE_KILL	49
/** See hooktype_local_kill() */
#define HOOKTYPE_LOCAL_KILL	50
/** See hooktype_rehashflag() */
#define HOOKTYPE_REHASHFLAG	51
/** See hooktype_configposttest() */
#define HOOKTYPE_CONFIGPOSTTEST	52
/** See hooktype_rehash() */
#define HOOKTYPE_REHASH	53
/** See hooktype_rehash_complete() */
#define HOOKTYPE_REHASH_COMPLETE	54
/** See hooktype_configtest() */
#define HOOKTYPE_CONFIGTEST	55
/** See hooktype_configrun() */
#define HOOKTYPE_CONFIGRUN	56
/** See hooktype_configrun_ex() */
#define HOOKTYPE_CONFIGRUN_EX	57
/** See hooktype_stats() */
#define HOOKTYPE_STATS	58
/** See hooktype_local_oper() */
#define HOOKTYPE_LOCAL_OPER	59
/** See hooktype_local_pass() */
#define HOOKTYPE_LOCAL_PASS	60
/** See hooktype_channel_create() */
#define HOOKTYPE_CHANNEL_CREATE	61
/** See hooktype_channel_destroy() */
#define HOOKTYPE_CHANNEL_DESTROY	62
/** See hooktype_tkl_except() */
#define HOOKTYPE_TKL_EXCEPT	63
/** See hooktype_umode_change() */
#define HOOKTYPE_UMODE_CHANGE	64
/** See hooktype_tkl_add() */
#define HOOKTYPE_TKL_ADD	65
/** See hooktype_tkl_del() */
#define HOOKTYPE_TKL_DEL	66
/** See hooktype_log() */
#define HOOKTYPE_LOG	67
/** See hooktype_local_spamfilter() */
#define HOOKTYPE_LOCAL_SPAMFILTER	68
/** See hooktype_silenced() */
#define HOOKTYPE_SILENCED	69
/** See hooktype_rawpacket_in() */
#define HOOKTYPE_RAWPACKET_IN	70
/** See hooktype_packet() */
#define HOOKTYPE_PACKET	71
/** See hooktype_handshake() */
#define HOOKTYPE_HANDSHAKE	72
/** See hooktype_free_client() */
#define HOOKTYPE_FREE_CLIENT	73
/** See hooktype_free_user() */
#define HOOKTYPE_FREE_USER	74
/** See hooktype_can_join_limitexceeded() */
#define HOOKTYPE_CAN_JOIN_LIMITEXCEEDED	75
/** See hooktype_visible_in_channel() */
#define HOOKTYPE_VISIBLE_IN_CHANNEL	76
/** See hooktype_see_channel_in_whois() */
#define HOOKTYPE_SEE_CHANNEL_IN_WHOIS	77
/** See hooktype_join_data() */
#define HOOKTYPE_JOIN_DATA	78
/** See hooktype_oper_invite_ban() */
#define HOOKTYPE_OPER_INVITE_BAN	79
/** See hooktype_view_topic_outside_channel() */
#define HOOKTYPE_VIEW_TOPIC_OUTSIDE_CHANNEL	80
/** See hooktype_chan_permit_nick_change() */
#define HOOKTYPE_CHAN_PERMIT_NICK_CHANGE	81
/** See hooktype_is_channel_secure() */
#define HOOKTYPE_IS_CHANNEL_SECURE	82
/** See hooktype_channel_synced() */
#define HOOKTYPE_CHANNEL_SYNCED	83
/** See hooktype_can_sajoin() */
#define HOOKTYPE_CAN_SAJOIN	84
/** See hooktype_check_init() */
#define HOOKTYPE_CHECK_INIT	85
/** See hooktype_mode_deop() */
#define HOOKTYPE_MODE_DEOP	86
/** See hooktype_dcc_denied() */
#define HOOKTYPE_DCC_DENIED	87
/** See hooktype_secure_connect() */
#define HOOKTYPE_SECURE_CONNECT	88
/** See hooktype_can_bypass_channel_message_restriction() */
#define HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION	89
/** See hooktype_require_sasl() */
#define HOOKTYPE_REQUIRE_SASL	90
/** See hooktype_sasl_continuation() */
#define HOOKTYPE_SASL_CONTINUATION	91
/** See hooktype_sasl_result() */
#define HOOKTYPE_SASL_RESULT	92
/** See hooktype_place_host_ban() */
#define HOOKTYPE_PLACE_HOST_BAN	93
/** See hooktype_find_tkline_match() */
#define HOOKTYPE_FIND_TKLINE_MATCH	94
/** See hooktype_welcome() */
#define HOOKTYPE_WELCOME	95
/** See hooktype_pre_command() */
#define HOOKTYPE_PRE_COMMAND	96
/** See hooktype_post_command() */
#define HOOKTYPE_POST_COMMAND	97
/** See hooktype_new_message() */
#define HOOKTYPE_NEW_MESSAGE	98
/** See hooktype_is_handshake_finished() */
#define HOOKTYPE_IS_HANDSHAKE_FINISHED	99
/** See hooktype_pre_local_quit_chan() */
#define HOOKTYPE_PRE_LOCAL_QUIT_CHAN	100
/** See hooktype_ident_lookup() */
#define HOOKTYPE_IDENT_LOOKUP	101
/** See hooktype_account_login() */
#define HOOKTYPE_ACCOUNT_LOGIN	102
/** See hooktype_close_connection() */
#define HOOKTYPE_CLOSE_CONNECTION	103

/* Adding a new hook here?
 * 1) Add the #define HOOKTYPE_.... with a new number
 * 2) Add a hook prototype (see below)
 * 3) Add type checking (even more below)
 * 4) Document the hook at https://www.unrealircd.org/docs/Dev:Hook_API
 */

/* Hook prototypes */
/** Called when a local user connects, allows pausing or rejecting the user (function prototype for HOOKTYPE_PRE_LOCAL_CONNECT).
 * @param client		The client
 * @retval HOOK_DENY		Stop the connection (hold/pause it).
 * @retval HOOK_ALLOW		Allow the connection (stop processing other modules)
 * @retval HOOK_CONTINUE	Allow the connection, unless another module blocks it.
 */
int hooktype_pre_local_connect(Client *client);
/** Called when a local user connects (function prototype for HOOKTYPE_LOCAL_CONNECT).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_connect(Client *client);
/** Called when a remote user connects (function prototype for HOOKTYPE_REMOTE_CONNECT).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_remote_connect(Client *client);
/** Called when a local user disconnects, allows changing the quit/disconnect reason (function prototype for HOOKTYPE_PRE_LOCAL_QUIT).
 * @param client		The client
 * @param client		The quit/disconnect reason
 * @return The quit reason (you may also return 'comment' if it should be unchanged) or NULL for an empty reason.
 */
char *hooktype_pre_local_quit(Client *client, char *comment);
/** Called when a local user quits or otherwise disconnects (function prototype for HOOKTYPE_PRE_LOCAL_QUIT).
 * @param client		The client
 * @param mtags         	Message tags associated with the quit
 * @param comment       	The quit/exit reason
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_quit(Client *client, MessageTag *mtags, char *comment);
/** Called when a remote user qutis or otherwise disconnects (function prototype for HOOKTYPE_REMOTE_QUIT).
 * @param client		The client
 * @param mtags         	Message tags associated with the quit
 * @param comment       	The quit/exit reason
 * @return The return value is ignored (use return 0)
 */
int hooktype_remote_quit(Client *client, MessageTag *mtags, char *comment);
/** Called when an unregistered user disconnects, so before the user was fully online (function prototype for HOOKTYPE_UNKUSER_QUIT).
 * @param client		The client
 * @param mtags         	Message tags associated with the quit
 * @param comment       	The quit/exit reason
 * @return The return value is ignored (use return 0)
 */
int hooktype_unkuser_quit(Client *client, MessageTag *mtags, char *comment);
/** Called when a local or remote server connects / links in (function prototype for HOOKTYPE_SERVER_CONNECT).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_server_connect(Client *client);
/** Called very early when doing an outgoing server connect (function prototype for HOOKTYPE_SERVER_HANDSHAKE_OUT).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_server_handshake_out(Client *client);
/** Called on new locally connected server, in or out, after all users/channels/TKLs/etc have been synced, but before EOS (function prototype for HOOKTYPE_SERVER_SYNC).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_server_sync(Client *client);
/** Called when a local or remote server connects / links in, but only after EOS (End Of Sync) has been received or sent (function prototype for HOOKTYPE_POST_SERVER_CONNECT).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_post_server_connect(Client *client);
/** Called when a local or remote server is linked in and fully synced, after EOS / End Of Sync (function prototype for HOOKTYPE_SERVER_SYNCED).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_server_synced(Client *client);
/** Called when a local or remote server disconnects (function prototype for HOOKTYPE_SERVER_QUIT).
 * @param client		The client
 * @param mtags         	Message tags associated with the disconnect
 * @return The return value is ignored (use return 0)
 */
int hooktype_server_quit(Client *client, MessageTag *mtags);
/** Called when a local user changes the nick name (function prototype for HOOKTYPE_LOCAL_NICKCHANGE).
 * @param client		The client
 * @param newnick		The new nick name
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_nickchange(Client *client, char *newnick);
/** Called when a remote user changes the nick name (function prototype for HOOKTYPE_REMOTE_NICKCHANGE).
 * @param client		The client
 * @param newnick		The new nick name
 * @return The return value is ignored (use return 0)
 */
int hooktype_remote_nickchange(Client *client, char *newnick);
/** Called when a user wants to join a channel, may the user join? (function prototype for HOOKTYPE_CAN_JOIN).
 * @param client		The client
 * @param channel		The channel the user wants to join
 * @param key			The key supplied by the client
 * @param parv			The parameters from the JOIN. Normally you should not use this.
 * @return Return 0 to allow the user, any other value should be an IRC numeric (eg: ERR_BANNEDFROMCHAN).
 */
int hooktype_can_join(Client *client, Channel *channel, char *key, char *parv[]);
/** Called when a user wants to join a channel, may the user join? (function prototype for HOOKTYPE_PRE_LOCAL_JOIN).
 * FIXME: It's not entirely clear why we have both hooktype_can_join() and hooktype_pre_local_join().
 * @param client		The client
 * @param channel		The channel the user wants to join
 * @param parv			The parameters from the JOIN. May contain channel key in parv[2].
 * @retval HOOK_DENY		Deny the join.
 * @retval HOOK_ALLOW		Allow the join (stop processing other modules)
 * @retval HOOK_CONTINUE	Allow the join, unless another module blocks it.
 */
int hooktype_pre_local_join(Client *client, Channel *channel, char *parv[]);
/** Called when a local user joins a channel (function prototype for HOOKTYPE_LOCAL_JOIN).
 * @param client		The client
 * @param channel		The channel the user wants to join
 * @param mtags         	Message tags associated with the event
 * @param parv			The parameters from the JOIN. May contain channel key in parv[2].
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_join(Client *client, Channel *channel, MessageTag *mtags, char *parv[]);
/** Called when a remote user joins a channel (function prototype for HOOKTYPE_REMOTE_JOIN).
 * @param client		The client
 * @param channel		The channel the user wants to join
 * @param mtags         	Message tags associated with the event
 * @param parv			The parameters from the JOIN. May contain channel key in parv[2].
 * @return The return value is ignored (use return 0)
 */
int hooktype_remote_join(Client *client, Channel *channel, MessageTag *mtags, char *parv[]);
/** Called when a local user wants to part a channel (function prototype for HOOKTYPE_PRE_LOCAL_PART).
 * @param client		The client
 * @param channel		The channel the user wants to part
 * @param comment		The PART reason, this may be NULL.
 * @return The part reason (you may also return 'comment' if it should be unchanged) or NULL for an empty reason.
 */
char *hooktype_pre_local_part(Client *client, Channel *channel, char *comment);
/** Called when a local user parts a channel (function prototype for HOOKTYPE_LOCAL_PART).
 * @param client		The client
 * @param channel		The channel the user is leaving
 * @param mtags         	Message tags associated with the event
 * @param comment		The PART reason, this may be NULL.
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_part(Client *client, Channel *channel, MessageTag *mtags, char *comment);
/** Called when a remote user parts a channel (function prototype for HOOKTYPE_REMOTE_PART).
 * @param client		The client
 * @param channel		The channel the user is leaving
 * @param mtags         	Message tags associated with the event
 * @param comment		The PART reason, this may be NULL.
 * @return The return value is ignored (use return 0)
 */
int hooktype_remote_part(Client *client, Channel *channel, MessageTag *mtags, char *comment);
/** Do not use this function, use hooktype_can_kick() instead!
 */
char *hooktype_pre_local_kick(Client *client, Client *victim, Channel *channel, char *comment);
/** Called when a local user wants to kick another user from a channel (function prototype for HOOKTYPE_CAN_KICK).
 * @param client		The client issuing the command
 * @param victim		The victim that should be kicked
 * @param channel		The channel the user should be kicked from
 * @param comment		The KICK reason, this may be NULL.
 * @param client_flags		The access flags of 'client', one of CHFL_*, eg CHFL_CHANOP.
 * @param victim_flags		The access flags of 'victim', one of CHFL_*, eg CHFL_VOICE.
 * @param error			The error message that should be shown to the user (full IRC protocol line).
 * @retval EX_DENY		Deny the KICK (unless IRCOp with sufficient override rights).
 * @retval EX_ALWAYS_DENY	Deny the KICK always (even if IRCOp).
 * @retval EX_ALLOW		Allow the kick, unless another module blocks it.
 */
int hooktype_can_kick(Client *client, Client *victim, Channel *channel, char *comment, long client_flags, long victim_flags, char **error);
/** Called when a local user is kicked (function prototype for HOOKTYPE_LOCAL_KICK).
 * @param client		The client issuing the command
 * @param victim		The victim that should be kicked
 * @param channel		The channel the user should be kicked from
 * @param mtags         	Message tags associated with the event
 * @param comment		The KICK reason, this may be NULL.
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_kick(Client *client, Client *victim, Channel *channel, MessageTag *mtags, char *comment);
/** Called when a remote user is kicked (function prototype for HOOKTYPE_REMOTE_KICK).
 * @param client		The client issuing the command
 * @param victim		The victim that should be kicked
 * @param channel		The channel the user should be kicked from
 * @param mtags         	Message tags associated with the event
 * @param comment		The KICK reason, this may be NULL.
 * @return The return value is ignored (use return 0)
 */
int hooktype_remote_kick(Client *client, Client *victim, Channel *channel, MessageTag *mtags, char *comment);
/** Called right before a message is sent to the channel (function prototype for HOOKTYPE_PRE_CHANMSG).
 * This function is only used by delayjoin. It cannot block a message. See hooktype_can_send_to_user() instead!
 * @param client		The client
 * @param channel		The channel
 * @param mtags         	Message tags associated with the event
 * @param text			The text that will be sent
 * @return The return value is ignored (use return 0)
 */
int hooktype_pre_chanmsg(Client *client, Channel *channel, MessageTag *mtags, char *text, SendType sendtype);
/** Called when a user wants to message another user (function prototype for HOOKTYPE_CAN_SEND_TO_USER).
 * @param client		The sender
 * @param target		The recipient
 * @param text			The text to be sent (double pointer!)
 * @param errmsg		The error message. If you block the message (HOOK_DENY) then you MUST set this!
 * @param sendtype		The message type, for example SEND_TYPE_PRIVMSG.
 * @retval HOOK_DENY		Deny the message. The 'errmsg' will be sent to the user.
 * @retval HOOK_CONTINUE	Allow the message, unless other modules block it.
 */
int hooktype_can_send_to_user(Client *client, Client *target, char **text, char **errmsg, SendType sendtype);
/** Called when xxxx (function prototype for HOOKTYPE_CAN_SEND_TO_CHANNEL).
 * @param client		The sender
 * @param channel		The channel to send to
 * @param membership		The membership struct, so you can see for example op status.
 * @param text			The text to be sent (double pointer!)
 * @param errmsg		The error message. If you block the message (HOOK_DENY) then you MUST set this!
 * @param sendtype		The message type, for example SEND_TYPE_PRIVMSG.
 * @retval HOOK_DENY		Deny the message. The 'errmsg' will be sent to the user.
 * @retval HOOK_CONTINUE	Allow the message, unless other modules block it.
 */
int hooktype_can_send_to_channel(Client *client, Channel *channel, Membership *member, char **text, char **errmsg, SendType sendtype);
/** Called when a message is sent from one user to another user (function prototype for HOOKTYPE_USERMSG).
 * @param client		The sender
 * @param to			The recipient
 * @param mtags         	Message tags associated with the event
 * @param text			The text
 * @param sendtype		The message type, for example SEND_TYPE_PRIVMSG.
 * @return The return value is ignored (use return 0)
 */
int hooktype_usermsg(Client *client, Client *to, MessageTag *mtags, char *text, SendType sendtype);
/** Called when a message is sent to a channel (function prototype for HOOKTYPE_CHANMSG).
 * @param client		The sender
 * @param channel		The channel
 * @param sendflags		One of SEND_* (eg SEND_ALL, SKIP_DEAF).
 * @param prefix		Either zero, one or a combination of PREFIX_*.
 * @param target		Target string, usually this is "#channel", but it can also contain prefixes like "@#channel"
 * @param mtags         	Message tags associated with the event
 * @param text			The text
 * @param sendtype		The message type, for example SEND_TYPE_PRIVMSG.
 * @return The return value is ignored (use return 0)
 */
int hooktype_chanmsg(Client *client, Channel *channel, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, SendType sendtype);
/** Called when xxxx (function prototype for HOOKTYPE_PRE_LOCAL_TOPIC).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
char *hooktype_pre_local_topic(Client *client, Channel *channel, char *topic);
/** Called when xxxx (function prototype for HOOKTYPE_LOCAL_TOPIC).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_topic(Client *client, Channel *channel, char *topic);
/** Called when xxxx (function prototype for HOOKTYPE_TOPIC).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_topic(Client *client, Channel *channel, MessageTag *mtags, char *topic);
/** Called when xxxx (function prototype for HOOKTYPE_PRE_LOCAL_CHANMODE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_pre_local_chanmode(Client *client, Channel *channel, MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode);
/** Called when xxxx (function prototype for HOOKTYPE_PRE_REMOTE_CHANMODE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_pre_remote_chanmode(Client *client, Channel *channel, MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode);
/** Called when xxxx (function prototype for HOOKTYPE_LOCAL_CHANMODE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_chanmode(Client *client, Channel *channel, MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode);
/** Called when xxxx (function prototype for HOOKTYPE_REMOTE_CHANMODE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_remote_chanmode(Client *client, Channel *channel, MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode);
/** Called when xxxx (function prototype for HOOKTYPE_MODECHAR_DEL).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_modechar_del(Channel *channel, int modechar);
/** Called when xxxx (function prototype for HOOKTYPE_MODECHAR_ADD).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_modechar_add(Channel *channel, int modechar);
/** Called when xxxx (function prototype for HOOKTYPE_AWAY).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_away(Client *client, MessageTag *mtags, char *reason);
/** Called when xxxx (function prototype for HOOKTYPE_PRE_INVITE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_pre_invite(Client *client, Client *acptr, Channel *channel, int *override);
/** Called when xxxx (function prototype for HOOKTYPE_INVITE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_invite(Client *from, Client *to, Channel *channel, MessageTag *mtags);
/** Called when xxxx (function prototype for HOOKTYPE_PRE_KNOCK).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_pre_knock(Client *client, Channel *channel);
/** Called when xxxx (function prototype for HOOKTYPE_KNOCK).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_knock(Client *client, Channel *channel, MessageTag *mtags, char *comment);
/** Called when xxxx (function prototype for HOOKTYPE_WHOIS).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_whois(Client *client, Client *target);
/** Called when xxxx (function prototype for HOOKTYPE_WHO_STATUS).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_who_status(Client *client, Client *target, Channel *channel, Member *member, char *status, int cansee);
/** Called when xxxx (function prototype for HOOKTYPE_PRE_KILL).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_pre_kill(Client *client, Client *victim, char *killpath);
/** Called when xxxx (function prototype for HOOKTYPE_LOCAL_KILL).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_kill(Client *client, Client *victim, char *comment);
/** Called when xxxx (function prototype for HOOKTYPE_REHASHFLAG).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_rehashflag(Client *client, char *str);
/** Called when xxxx (function prototype for HOOKTYPE_CONFIGPOSTTEST).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_configposttest(int *errors);
/** Called when xxxx (function prototype for HOOKTYPE_REHASH).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_rehash(void);
/** Called when xxxx (function prototype for HOOKTYPE_REHASH_COMPLETE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_rehash_complete(void);
/** Called when xxxx (function prototype for HOOKTYPE_CONFIGTEST).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_configtest(ConfigFile *cfptr, ConfigEntry *ce, int section, int *errors);
/** Called when xxxx (function prototype for HOOKTYPE_CONFIGRUN).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_configrun(ConfigFile *cfptr, ConfigEntry *ce, int section);
/** Called when xxxx (function prototype for HOOKTYPE_CONFIGRUN_EX).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_configrun_ex(ConfigFile *cfptr, ConfigEntry *ce, int section, void *ptr);
/** Called when xxxx (function prototype for HOOKTYPE_STATS).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_stats(Client *client, char *str);
/** Called when xxxx (function prototype for HOOKTYPE_LOCAL_OPER).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_oper(Client *client, int add);
/** Called when xxxx (function prototype for HOOKTYPE_LOCAL_PASS).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_pass(Client *client, char *password);
/** Called when xxxx (function prototype for HOOKTYPE_CHANNEL_CREATE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_channel_create(Client *client, Channel *channel);
/** Called when xxxx (function prototype for HOOKTYPE_CHANNEL_DESTROY).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_channel_destroy(Channel *channel, int *should_destroy);
/** Called when xxxx (function prototype for HOOKTYPE_TKL_EXCEPT).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_tkl_except(Client *cptr, int ban_type);
/** Called when xxxx (function prototype for HOOKTYPE_UMODE_CHANGE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_umode_change(Client *client, long setflags, long newflags);
/** Called when xxxx (function prototype for HOOKTYPE_TKL_ADD).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_tkl_add(Client *client, TKL *tkl);
/** Called when xxxx (function prototype for HOOKTYPE_TKL_DEL).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_tkl_del(Client *client, TKL *tkl);
/** Called when xxxx (function prototype for HOOKTYPE_LOG).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_log(int flags, char *timebuf, char *buf);
/** Called when xxxx (function prototype for HOOKTYPE_LOCAL_SPAMFILTER).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_local_spamfilter(Client *acptr, char *str, char *str_in, int type, char *target, TKL *tkl);
/** Called when xxxx (function prototype for HOOKTYPE_SILENCED).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_silenced(Client *client, Client *to, SendType sendtype);
/** Called when xxxx (function prototype for HOOKTYPE_RAWPACKET_IN).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_rawpacket_in(Client *client, char *readbuf, int *length);
/** Called when xxxx (function prototype for HOOKTYPE_PACKET).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_packet(Client *from, Client *to, Client *intended_to, char **msg, int *length);
/** Called when xxxx (function prototype for HOOKTYPE_HANDSHAKE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_handshake(Client *client);
/** Called when xxxx (function prototype for HOOKTYPE_FREE_CLIENT).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_free_client(Client *acptr);
/** Called when xxxx (function prototype for HOOKTYPE_FREE_USER).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_free_user(Client *acptr);
/** Called when xxxx (function prototype for HOOKTYPE_CAN_JOIN_LIMITEXCEEDED).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_can_join_limitexceeded(Client *client, Channel *channel, char *key, char *parv[]);
/** Called when xxxx (function prototype for HOOKTYPE_VISIBLE_IN_CHANNEL).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_visible_in_channel(Client *client, Channel *channel);
/** Called when xxxx (function prototype for HOOKTYPE_SEE_CHANNEL_IN_WHOIS).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_see_channel_in_whois(Client *client, Client *target, Channel *channel);
/** Called when xxxx (function prototype for HOOKTYPE_JOIN_DATA).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_join_data(Client *who, Channel *channel);
/** Called when xxxx (function prototype for HOOKTYPE_OPER_INVITE_BAN).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_oper_invite_ban(Client *client, Channel *channel);
/** Called when xxxx (function prototype for HOOKTYPE_VIEW_TOPIC_OUTSIDE_CHANNEL).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_view_topic_outside_channel(Client *client, Channel *channel);
/** Called when xxxx (function prototype for HOOKTYPE_CHAN_PERMIT_NICK_CHANGE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_chan_permit_nick_change(Client *client, Channel *channel);
/** Called when xxxx (function prototype for HOOKTYPE_IS_CHANNEL_SECURE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_is_channel_secure(Channel *channel);
/** Called when xxxx (function prototype for HOOKTYPE_CHANNEL_SYNCED).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_channel_synced(Channel *channel, int merge, int removetheirs, int nomode);
/** Called when xxxx (function prototype for HOOKTYPE_CAN_SAJOIN).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_can_sajoin(Client *target, Channel *channel, Client *client);
/** Called when xxxx (function prototype for HOOKTYPE_CHECK_INIT).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_check_init(Client *cptr, char *sockname, size_t size);
/** Called when xxxx (function prototype for HOOKTYPE_MODE_DEOP).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_mode_deop(Client *client, Client *victim, Channel *channel, u_int what, int modechar, long my_access, char **badmode);
/** Called when xxxx (function prototype for HOOKTYPE_DCC_DENIED).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_dcc_denied(Client *client, char *target, char *realfile, char *displayfile, ConfigItem_deny_dcc *denydcc);
/** Called when xxxx (function prototype for HOOKTYPE_SECURE_CONNECT).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_secure_connect(Client *client);
/** Called when xxxx (function prototype for HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_can_bypass_channel_message_restriction(Client *client, Channel *channel, BypassChannelMessageRestrictionType bypass_type);
/** Called when xxxx (function prototype for HOOKTYPE_REQUIRE_SASL).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_require_sasl(Client *client, char *reason);
/** Called when xxxx (function prototype for HOOKTYPE_SASL_CONTINUATION).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_sasl_continuation(Client *client, char *buf);
/** Called when xxxx (function prototype for HOOKTYPE_SASL_RESULT).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_sasl_result(Client *client, int success);
/** Called when xxxx (function prototype for HOOKTYPE_PLACE_HOST_BAN).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_place_host_ban(Client *client, int action, char *reason, long duration);
/** Called when xxxx (function prototype for HOOKTYPE_FIND_TKLINE_MATCH).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_find_tkline_match(Client *client, TKL *tk);
/** Called when xxxx (function prototype for HOOKTYPE_WELCOME).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_welcome(Client *client, int after_numeric);
/** Called when xxxx (function prototype for HOOKTYPE_PRE_COMMAND).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_pre_command(Client *from, MessageTag *mtags, char *buf);
/** Called when xxxx (function prototype for HOOKTYPE_POST_COMMAND).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_post_command(Client *from, MessageTag *mtags, char *buf);
/** Called when xxxx (function prototype for HOOKTYPE_NEW_MESSAGE).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
void hooktype_new_message(Client *sender, MessageTag *recv_mtags, MessageTag **mtag_list, char *signature);
/** Called when xxxx (function prototype for HOOKTYPE_IS_HANDSHAKE_FINISHED).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_is_handshake_finished(Client *acptr);
/** Called when xxxx (function prototype for HOOKTYPE_PRE_LOCAL_QUIT_CHAN).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
char *hooktype_pre_local_quit_chan(Client *client, Channel *channel, char *comment);
/** Called when xxxx (function prototype for HOOKTYPE_IDENT_LOOKUP).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_ident_lookup(Client *acptr);
/** Called when xxxx (function prototype for HOOKTYPE_ACCOUNT_LOGIN).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_account_login(Client *client, MessageTag *mtags);
/** Called when xxxx (function prototype for HOOKTYPE_CLOSE_CONNECTION).
 * @param client		The client
 * @return The return value is ignored (use return 0)
 */
int hooktype_close_connection(Client *client);

/** @} */

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
        ((hooktype == HOOKTYPE_SERVER_SYNC) && !ValidateHook(hooktype_server_sync, func)) || \
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
        ((hooktype == HOOKTYPE_PACKET) && !ValidateHook(hooktype_packet, func)) || \
        ((hooktype == HOOKTYPE_HANDSHAKE) && !ValidateHook(hooktype_handshake, func)) || \
        ((hooktype == HOOKTYPE_AWAY) && !ValidateHook(hooktype_away, func)) || \
        ((hooktype == HOOKTYPE_INVITE) && !ValidateHook(hooktype_invite, func)) || \
        ((hooktype == HOOKTYPE_CAN_JOIN) && !ValidateHook(hooktype_can_join, func)) || \
        ((hooktype == HOOKTYPE_CAN_SEND_TO_CHANNEL) && !ValidateHook(hooktype_can_send_to_channel, func)) || \
        ((hooktype == HOOKTYPE_CAN_SEND_TO_USER) && !ValidateHook(hooktype_can_send_to_user, func)) || \
        ((hooktype == HOOKTYPE_CAN_KICK) && !ValidateHook(hooktype_can_kick, func)) || \
        ((hooktype == HOOKTYPE_FREE_CLIENT) && !ValidateHook(hooktype_free_client, func)) || \
        ((hooktype == HOOKTYPE_FREE_USER) && !ValidateHook(hooktype_free_user, func)) || \
        ((hooktype == HOOKTYPE_PRE_CHANMSG) && !ValidateHook(hooktype_pre_chanmsg, func)) || \
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
        ((hooktype == HOOKTYPE_SEND_CHANNEL) && !ValidateHook(hooktype_can_send_to_channel_secure, func)) || \
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
        ((hooktype == HOOKTYPE_SERVER_SYNCED) && !ValidateHook(hooktype_server_synced, func)) || \
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
        ((hooktype == HOOKTYPE_CONFIGRUN_EX) && !ValidateHook(hooktype_configrun_ex, func)) || \
        ((hooktype == HOOKTYPE_ACCOUNT_LOGIN) && !ValidateHook(hooktype_account_login, func)) || \
        ((hooktype == HOOKTYPE_CLOSE_CONNECTION) && !ValidateHook(hooktype_close_connection, func)) ) \
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
	EFUNC_CAN_SEND_TO_CHANNEL,
	EFUNC_CAN_SEND_TO_USER,
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
	EFUNC_LABELED_RESPONSE_SAVE_CONTEXT,
	EFUNC_LABELED_RESPONSE_SET_CONTEXT,
	EFUNC_LABELED_RESPONSE_FORCE_END,
	EFUNC_KICK_USER,
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

