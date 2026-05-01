/*
 * UnrealIRCd JavaScript Scripting Module
 * (C) 2026 Valerie
 * License: GPLv3 or later
 *
 * This module allows server-side JavaScript scripting using the Duktape engine.
 * Scripts can access IRC data through $ variables similar to n8n:
 * - $client, $client.name, $client.ip, etc
 * - $parv[0], $parv[1], etc
 * - $parc
 * - $channel (in channel contexts)
 * - $mtags - Message tags array (in hooks that receive mtags)
 * - Hooks can access their specific parameters
 *
 * Extended API (v2.0):
 * - registerChannelMode() - Register custom channel modes
 * - registerPrefixMode() - Register prefix modes like +o, +v
 * - registerUserMode() - Register custom user modes
 * - setModData/getModData - Store/retrieve module data on clients, channels, etc
 * - Utility functions: isUser, isServer, isOper, findClient, findChannel, etc
 * - doCmd, exitClient, sendToChannel, and more UnrealIRCd internal functions
 *
 * Message Tags API (v2.1):
 * - createMtag(name, value) - Create a single message tag object
 * - addMtag(mtags, name, value) - Add a tag to an mtags array
 * - findMtag(mtags, name) - Find a tag by name in an mtags array
 * - deleteMtag(mtags, name) - Remove a tag from an mtags array
 * - mtagsToString(mtags, client) - Convert mtags to IRC protocol string
 * - newMessageTags(sender) - Create new mtags with standard tags (msgid, time)
 * - $mtags variable available in hooks (JOIN, PART, KICK, QUIT, MSG, etc)
 * - sendToChannel/sendToServer accept optional mtags parameter
 *
 * Special Lists & RPC & Extbans API (v2.2):
 * - getAllClients(), getAllLocalClients() - Iterate all users
 * - getAllServers(), getAllLocalServers() - Iterate all servers
 * - getAllOpers() - Iterate all IRC operators
 * - getAllChannels() - Iterate all channels
 * - registerRPCMethod() - Register custom JSON-RPC methods
 * - rpcResponse(), rpcError() - Send RPC responses
 * - registerExtban() - Register custom extended ban types
 * - registerMessageTag() - Register custom message tag handlers
 * - registerConfigBlock() - Register custom configuration blocks
 * - config_error(), config_warn() - Report configuration errors/warnings
 */

/*** <<<MODULE MANAGER START>>>
module
{
	documentation "https://github.com/ObsidianIRC/ObbyScript/README.md";
	troubleshooting "In case of problems, documentation or e-mail me at v.a.pond@outlook.com";
	min-unrealircd-version "6.1.0";
	max-unrealircd-version "6.*";
	compile-flags "-lm";
	post-install-text {
		"The module is installed. Now all you need to do is add a loadmodule line:";
		"loadmodule \"obbyscript\";";
		"And /REHASH the IRCd.";
		"";
		"You can now create JavaScript scripts in unrealircd/scripts/ directory.";
		"";
		"Example script (scripts/hello.js):";
		"  // Register a command";
		"  registerCommand({";
		"    name: 'HELLO',";
		"    handler: function() {";
		"      sendNotice($client, 'Hello ' + $client.name + '!');";
		"      return 1;";
		"    }";
		"  });";
		"";
		"  // Register a hook";
		"  registerHook({";
		"    type: 'HOOKTYPE_LOCAL_CONNECT',";
		"    handler: function() {";
		"      sendNotice($client, 'Welcome to the server!');";
		"      return 0;";
		"    }";
		"  });";
	}
}
*** <<<MODULE MANAGER END>>>
*/

#define _GNU_SOURCE
#include "unrealircd.h"
#include <dirent.h>
#include <sys/types.h>

/* Embed Duktape directly */
#include "duktape.c"

#define SCRIPTS_DIR "scripts"
#define JS_MAX_OUTPUT 4096

ModuleHeader MOD_HEADER = {
	"obbyscript",
	"2.3",
	"JavaScript scripting support using Duktape (Extended API + RPC + Extbans + UnrealDB)",
	"Valware",
	"unrealircd-6",
};

/* Structure to hold JavaScript command handlers */
typedef struct JSCommand JSCommand;
struct JSCommand {
	JSCommand *prev, *next;
	char *name;
	Command *cmd;
	duk_context *ctx;
	char *handler_code;
};

/* Structure to hold JavaScript hook handlers */
typedef struct JSHook JSHook;
struct JSHook {
	JSHook *prev, *next;
	int hooktype;
	Hook *hook;
	duk_context *ctx;
	char *handler_code;
};

/* Structure to hold pending HTTP requests */
typedef struct JSHttpRequest JSHttpRequest;
struct JSHttpRequest {
	JSHttpRequest *prev, *next;
	char *callback_name;    /* Name of JS callback function */
	char *client_name;      /* Client who made the request (for sending response) */
	duk_context *ctx;
};

/* Structure to hold JavaScript timer/interval handlers */
typedef struct JSTimer JSTimer;
struct JSTimer {
	JSTimer *prev, *next;
	int id;                 /* Timer ID for clearInterval/clearTimeout */
	char *handler_code;     /* Stash key for handler function */
	duk_context *ctx;
	Event *event;           /* UnrealIRCd event handle */
	int is_interval;        /* 1 for setInterval, 0 for setTimeout */
	int count;              /* How many times to run (1 for timeout, -1 for infinite interval) */
	int cancelled;          /* 1 if timer was cancelled via clearInterval/clearTimeout */
};

/* Structure to hold JavaScript-registered channel modes */
typedef struct JSChannelMode JSChannelMode;
struct JSChannelMode {
	JSChannelMode *prev, *next;
	char letter;            /* Mode letter */
	char *name;             /* Mode name for reference */
	Cmode *cmode;           /* UnrealIRCd channel mode handle */
	Cmode_t mode_bit;       /* Mode bit */
	int paracount;          /* 0 = no param, 1 = has param */
	duk_context *ctx;
	char *is_ok_handler;    /* Stash key for is_ok callback */
};

/* Structure to hold JavaScript-registered prefix modes */
typedef struct JSPrefixMode JSPrefixMode;
struct JSPrefixMode {
	JSPrefixMode *prev, *next;
	char letter;            /* Mode letter */
	char prefix;            /* Prefix character (like @, +, %) */
	char sjoin_prefix;      /* SJOIN prefix (can be same as prefix) */
	int rank;               /* Rank (higher = more privileges) */
	char *name;             /* Mode name for reference */
	Cmode *cmode;           /* UnrealIRCd channel mode handle */
	duk_context *ctx;
	char *is_ok_handler;    /* Stash key for is_ok callback */
};

/* Structure to hold JavaScript-registered user modes */
typedef struct JSUserMode JSUserMode;
struct JSUserMode {
	JSUserMode *prev, *next;
	char letter;            /* Mode letter */
	char *name;             /* Mode name for reference */
	Umode *umode;           /* UnrealIRCd user mode handle */
	long mode_bit;          /* Mode bit */
	int is_global;          /* 1 = synced across servers, 0 = local only */
	int unset_on_deoper;    /* 1 = auto-unset when user de-opers */
	duk_context *ctx;
	char *allowed_handler;  /* Stash key for allowed callback */
};

/* Structure to hold JavaScript-registered ModData */
typedef struct JSModData JSModData;
struct JSModData {
	JSModData *prev, *next;
	char *name;             /* ModData name */
	ModDataInfo *md;        /* UnrealIRCd moddata handle */
	ModDataType type;       /* Type: client, channel, member, etc */
	int sync;               /* Sync across network */
};

/* Structure to hold JavaScript-registered config blocks */
typedef struct JSConfigBlock JSConfigBlock;
struct JSConfigBlock {
	JSConfigBlock *prev, *next;
	char *blockname;        /* Config block name (e.g., "mymodule") */
	duk_context *ctx;
	char *test_handler;     /* Stash key for test callback (validation) */
	char *run_handler;      /* Stash key for run callback (applying config) */
};

static JSHttpRequest *js_http_requests = NULL;
static JSTimer *js_timers = NULL;
static int js_timer_id_counter = 1;
static JSChannelMode *js_channelmodes = NULL;
static JSPrefixMode *js_prefixmodes = NULL;
static JSUserMode *js_usermodes = NULL;
static JSModData *js_moddatas = NULL;
static JSConfigBlock *js_config_blocks = NULL;

/* Track which hook types have been registered with UnrealIRCd */
static int js_registered_hooks[256] = {0}; /* Array to track registered hook types */

/* Global variables */
static duk_context *global_ctx = NULL;
static JSCommand *js_commands = NULL;
static JSHook *js_hooks = NULL;
static ModuleInfo *js_modinfo = NULL;

/* Forward declarations */
void js_load_scripts(void);
void js_cleanup(void);
int js_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int js_configrun(ConfigFile *cf, ConfigEntry *ce, int type);

/* Duktape JavaScript API functions */
duk_ret_t js_api_sendNotice(duk_context *ctx);
duk_ret_t js_api_sendNumeric(duk_context *ctx);
duk_ret_t js_api_sendRaw(duk_context *ctx);
duk_ret_t js_api_log(duk_context *ctx);
duk_ret_t js_api_registerCommand(duk_context *ctx);
duk_ret_t js_api_registerHook(duk_context *ctx);
duk_ret_t js_api_findClient(duk_context *ctx);
duk_ret_t js_api_isOper(duk_context *ctx);
duk_ret_t js_api_hasMode(duk_context *ctx);
duk_ret_t js_api_httpGet(duk_context *ctx);
duk_ret_t js_api_setInterval(duk_context *ctx);
duk_ret_t js_api_setTimeout(duk_context *ctx);
duk_ret_t js_api_clearInterval(duk_context *ctx);
duk_ret_t js_api_clearTimeout(duk_context *ctx);

/* NEW Extended API functions */
duk_ret_t js_api_registerChannelMode(duk_context *ctx);
duk_ret_t js_api_registerPrefixMode(duk_context *ctx);
duk_ret_t js_api_registerUserMode(duk_context *ctx);
duk_ret_t js_api_registerModData(duk_context *ctx);
duk_ret_t js_api_setModData(duk_context *ctx);
duk_ret_t js_api_getModData(duk_context *ctx);
duk_ret_t js_api_findChannel(duk_context *ctx);
duk_ret_t js_api_findServer(duk_context *ctx);
duk_ret_t js_api_isUser(duk_context *ctx);
duk_ret_t js_api_isServer(duk_context *ctx);
duk_ret_t js_api_isLoggedIn(duk_context *ctx);
duk_ret_t js_api_isSecure(duk_context *ctx);
duk_ret_t js_api_isULine(duk_context *ctx);
duk_ret_t js_api_doCmd(duk_context *ctx);
duk_ret_t js_api_exitClient(duk_context *ctx);
duk_ret_t js_api_sendToChannel(duk_context *ctx);
duk_ret_t js_api_sendToServer(duk_context *ctx);
duk_ret_t js_api_sendToAllServers(duk_context *ctx);
duk_ret_t js_api_setUserMode(duk_context *ctx);
duk_ret_t js_api_setChannelMode(duk_context *ctx);
duk_ret_t js_api_registerConfigBlock(duk_context *ctx);
duk_ret_t js_api_config_error(duk_context *ctx);
duk_ret_t js_api_config_warn(duk_context *ctx);
duk_ret_t js_api_checkChannelAccess(duk_context *ctx);
duk_ret_t js_api_getChannelMembers(duk_context *ctx);
duk_ret_t js_api_getUserChannels(duk_context *ctx);
duk_ret_t js_api_joinChannel(duk_context *ctx);
duk_ret_t js_api_partChannel(duk_context *ctx);
duk_ret_t js_api_kickUser(duk_context *ctx);
duk_ret_t js_api_setTopic(duk_context *ctx);
duk_ret_t js_api_changeNick(duk_context *ctx);
duk_ret_t js_api_setHost(duk_context *ctx);
duk_ret_t js_api_addTKL(duk_context *ctx);
duk_ret_t js_api_delTKL(duk_context *ctx);

/* Channel mode callbacks */
int js_channelmode_is_ok(Client *client, Channel *channel, char mode, const char *para, int checkt, int what);
int js_prefixmode_is_ok(Client *client, Channel *channel, char mode, const char *para, int checkt, int what);
int js_usermode_allowed(Client *client, int what);

/* ModData callbacks */
void js_moddata_free(ModData *md);
const char *js_moddata_serialize(ModData *md);
void js_moddata_unserialize(const char *str, ModData *md);

/* HTTP callback */
void js_http_callback(OutgoingWebRequest *request, OutgoingWebResponse *response);

/* Timer event callback */
EVENT(js_timer_event);

/* Hook handlers */
int js_hook_local_connect(Client *client);
int js_hook_remote_connect(Client *client);
int js_hook_local_quit(Client *client, MessageTag *mtags, const char *comment);
int js_hook_remote_quit(Client *client, MessageTag *mtags, const char *comment);
int js_hook_local_join(Client *client, Channel *channel, MessageTag *mtags);
int js_hook_remote_join(Client *client, Channel *channel, MessageTag *mtags);
int js_hook_local_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment);
int js_hook_remote_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment);
int js_hook_local_kick(Client *client, Client *victim, Channel *channel, MessageTag *mtags, const char *comment);
int js_hook_remote_kick(Client *client, Client *victim, Channel *channel, MessageTag *mtags, const char *comment);
int js_hook_chanmsg(Client *client, Channel *channel, int sendflags, const char *member_modes, const char *target, MessageTag *mtags, const char *text, SendType sendtype);
int js_hook_usermsg(Client *client, Client *to, MessageTag *mtags, const char *text, SendType sendtype);
int js_hook_local_nickchange(Client *client, MessageTag *mtags, const char *newnick);
int js_hook_remote_nickchange(Client *client, MessageTag *mtags, const char *newnick);
int js_hook_topic(Client *client, Channel *channel, MessageTag *mtags, const char *topic);
int js_hook_away(Client *client, MessageTag *mtags, const char *reason, int already_as_away);
int js_hook_local_oper(Client *client, int add, const char *oper_block, const char *operclass);
int js_hook_channel_create(Channel *channel);
int js_hook_channel_destroy(Channel *channel, int *should_destroy);
int js_hook_can_use_nick(Client *client, const char *newnick, const char **reject_reason);

/* Additional hook handlers - comprehensive implementation */
int js_hook_pre_local_connect(Client *client);
const char *js_hook_pre_local_quit(Client *client, const char *comment);
int js_hook_unkuser_quit(Client *client, MessageTag *mtags, const char *comment);
int js_hook_server_connect(Client *client);
int js_hook_server_quit(Client *client, MessageTag *mtags);
int js_hook_can_join(Client *client, Channel *channel, const char *key, char **errmsg);
int js_hook_pre_local_join(Client *client, Channel *channel, const char *key);
const char *js_hook_pre_local_part(Client *client, Channel *channel, const char *comment);
const char *js_hook_pre_local_kick(Client *client, Client *victim, Channel *channel, const char *comment);
int js_hook_can_kick(Client *client, Client *victim, Channel *channel, const char *comment, const char *client_modes, const char *victim_modes, const char **errmsg);
int js_hook_pre_chanmsg(Client *client, Channel *channel, MessageTag **mtags, const char *text, SendType sendtype);
int js_hook_can_send_to_channel(Client *client, Channel *channel, Membership *member, const char **text, const char **errmsg, SendType sendtype, ClientContext *clictx);
int js_hook_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype, ClientContext *clictx);
const char *js_hook_pre_local_topic(Client *client, Channel *channel, const char *topic);
int js_hook_can_set_topic(Client *client, Channel *channel, const char *topic, const char **errmsg);
int js_hook_pre_local_chanmode(Client *client, Channel *channel, MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode);
int js_hook_pre_remote_chanmode(Client *client, Channel *channel, MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode);
int js_hook_local_chanmode(Client *client, Channel *channel, MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode, int *destroy_channel);
int js_hook_remote_chanmode(Client *client, Channel *channel, MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode, int *destroy_channel);
int js_hook_pre_invite(Client *client, Client *acptr, Channel *channel, int *override);
int js_hook_invite(Client *client, Client *acptr, Channel *channel, MessageTag *mtags);
int js_hook_pre_knock(Client *client, Channel *channel, const char **reason);
int js_hook_knock(Client *client, Channel *channel, MessageTag *mtags, const char *comment);
int js_hook_whois(Client *client, Client *target, NameValuePrioList **list);
int js_hook_who_status(Client *client, Client *target, Channel *channel, Member *member, const char *status, int cansee);
int js_hook_pre_kill(Client *client, Client *victim, const char *reason);
int js_hook_local_kill(Client *client, Client *victim, const char *comment);
int js_hook_rehash(void);
int js_hook_rehash_complete(void);
int js_hook_stats(Client *client, const char *str);
int js_hook_local_pass(Client *client, const char *password);
int js_hook_umode_change(Client *client, long setflags, long newflags);
int js_hook_tkl_add(Client *client, TKL *tkl);
int js_hook_tkl_del(Client *client, TKL *tkl);
int js_hook_secure_connect(Client *client);
int js_hook_welcome(Client *client, int after_numeric);
int js_hook_pre_command(Client *from, MessageTag *mtags, const char *buf);
int js_hook_post_command(Client *from, MessageTag *mtags, const char *buf);
int js_hook_account_login(Client *client, MessageTag *mtags);
int js_hook_post_local_nickchange(Client *client, MessageTag *mtags, const char *oldnick);
int js_hook_post_remote_nickchange(Client *client, MessageTag *mtags, const char *oldnick);
int js_hook_userhost_change(Client *client, const char *olduser, const char *oldhost);
int js_hook_realname_change(Client *client, const char *oldinfo);
int js_hook_ip_change(Client *client, const char *oldip);
int js_hook_handshake(Client *client);
int js_hook_free_client(Client *client);
int js_hook_watch_add(char *nick, Client *client, int flags);
int js_hook_watch_del(char *nick, Client *client, int flags);

/* SASL hook handlers */
int js_hook_sasl_authenticate(Client *client, int first, const char *param);
int js_hook_sasl_continuation(Client *client, const char *buf);
int js_hook_sasl_result(Client *client, int success);
const char *js_hook_sasl_mechs(Client *client);

/* Helper functions */
void js_push_client_object(duk_context *ctx, Client *client);
void js_push_channel_object(duk_context *ctx, Channel *channel);
void js_push_mtags_object(duk_context *ctx, MessageTag *mtags);
MessageTag *js_pop_mtags(duk_context *ctx, duk_idx_t idx);

/* Client object methods */
duk_ret_t js_client_accountLogin(duk_context *ctx);
duk_ret_t js_client_accountLogout(duk_context *ctx);

/* MessageTag API functions */
duk_ret_t js_api_createMtag(duk_context *ctx);
duk_ret_t js_api_addMtag(duk_context *ctx);
duk_ret_t js_api_findMtag(duk_context *ctx);
duk_ret_t js_api_deleteMtag(duk_context *ctx);
duk_ret_t js_api_mtagsToString(duk_context *ctx);
duk_ret_t js_api_newMessageTags(duk_context *ctx);

/* Special list iteration API functions */
duk_ret_t js_api_getAllClients(duk_context *ctx);
duk_ret_t js_api_getAllLocalClients(duk_context *ctx);
duk_ret_t js_api_getAllServers(duk_context *ctx);
duk_ret_t js_api_getAllLocalServers(duk_context *ctx);
duk_ret_t js_api_getAllOpers(duk_context *ctx);
duk_ret_t js_api_getAllChannels(duk_context *ctx);

/* Structure to hold JavaScript-registered RPC handlers */
typedef struct JSRPCHandler JSRPCHandler;
struct JSRPCHandler {
	JSRPCHandler *prev, *next;
	char *method;             /* RPC method name */
	RPCHandler *rpc;          /* UnrealIRCd RPC handler */
	duk_context *ctx;
	char *handler_code;       /* Stash key for handler function */
};

static JSRPCHandler *js_rpc_handlers = NULL;

/* RPC API functions */
duk_ret_t js_api_registerRPCMethod(duk_context *ctx);
RPC_CALL_FUNC(js_rpc_handler);

/* Structure to hold JavaScript-registered extbans */
typedef struct JSExtban JSExtban;
struct JSExtban {
	JSExtban *prev, *next;
	char letter;              /* Extban letter */
	char *name;               /* Extban name */
	Extban *extban;           /* UnrealIRCd extban handle */
	int options;              /* Extban options flags */
	duk_context *ctx;
	char *is_ok_handler;      /* Stash key for is_ok callback */
	char *conv_param_handler; /* Stash key for conv_param callback */
	char *is_banned_handler;  /* Stash key for is_banned callback */
};

static JSExtban *js_extbans = NULL;
static JSExtban *js_current_extban = NULL; /* Used for callback context */

/* Extban API functions */
duk_ret_t js_api_registerExtban(duk_context *ctx);
int js_extban_is_ok(BanContext *b);
const char *js_extban_conv_param(BanContext *b, Extban *extban);
int js_extban_is_banned(BanContext *b);

/* Structure to hold JavaScript-registered message tag handlers */
typedef struct JSMessageTag JSMessageTag;
struct JSMessageTag {
	JSMessageTag *prev, *next;
	char *name;                 /* Tag name (e.g., "+draft/myapp") */
	MessageTagHandler *handler; /* UnrealIRCd mtag handler */
	int flags;                  /* MTAG_HANDLER_FLAGS_* */
	duk_context *ctx;
	char *is_ok_handler;        /* Stash key for is_ok callback */
	char *should_send_handler;  /* Stash key for should_send_to_client callback */
};

static JSMessageTag *js_mtag_handlers = NULL;

/* Structure to hold open JavaScript database handles */
typedef struct JSDatabase JSDatabase;
struct JSDatabase {
	JSDatabase *prev, *next;
	int id;                     /* Database handle ID for JavaScript */
	char *filename;             /* Database filename */
	UnrealDB *db;               /* UnrealIRCd database handle */
	int mode;                   /* UNREALDB_MODE_READ or UNREALDB_MODE_WRITE */
};

static JSDatabase *js_databases = NULL;
static int js_database_id_counter = 1;

/* Database API functions */
duk_ret_t js_api_dbOpen(duk_context *ctx);
duk_ret_t js_api_dbClose(duk_context *ctx);
duk_ret_t js_api_dbWriteInt64(duk_context *ctx);
duk_ret_t js_api_dbWriteInt32(duk_context *ctx);
duk_ret_t js_api_dbWriteInt16(duk_context *ctx);
duk_ret_t js_api_dbWriteStr(duk_context *ctx);
duk_ret_t js_api_dbWriteChar(duk_context *ctx);
duk_ret_t js_api_dbReadInt64(duk_context *ctx);
duk_ret_t js_api_dbReadInt32(duk_context *ctx);
duk_ret_t js_api_dbReadInt16(duk_context *ctx);
duk_ret_t js_api_dbReadStr(duk_context *ctx);
duk_ret_t js_api_dbReadChar(duk_context *ctx);
duk_ret_t js_api_dbGetError(duk_context *ctx);

/* Message tag API functions */
duk_ret_t js_api_registerMessageTag(duk_context *ctx);
int js_mtag_is_ok(Client *client, const char *name, const char *value);
int js_mtag_should_send_to_client(Client *target);
static JSMessageTag *js_current_mtag = NULL; /* Used for callback context */

/* RPC response API functions */
duk_ret_t js_api_rpcResponse(duk_context *ctx);
duk_ret_t js_api_rpcError(duk_context *ctx);

/*
 * Initialize the Duktape JavaScript engine and set up the API
 */
static void js_init_engine(void)
{
	if (global_ctx)
		return;

	global_ctx = duk_create_heap_default();
	if (!global_ctx)
	{
		unreal_log(ULOG_ERROR, "obbyscript", "JS_INIT_FAILED", NULL,
		           "Failed to create Duktape heap");
		return;
	}

	/* Register API functions */
	duk_push_global_object(global_ctx);

	/* IRC API functions */
	duk_push_c_function(global_ctx, js_api_sendNotice, 2);
	duk_put_prop_string(global_ctx, -2, "sendNotice");

	duk_push_c_function(global_ctx, js_api_sendNumeric, 3);
	duk_put_prop_string(global_ctx, -2, "sendNumeric");

	duk_push_c_function(global_ctx, js_api_sendRaw, 2);
	duk_put_prop_string(global_ctx, -2, "sendRaw");

	duk_push_c_function(global_ctx, js_api_log, 1);
	duk_put_prop_string(global_ctx, -2, "log");

	duk_push_c_function(global_ctx, js_api_registerCommand, 1);
	duk_put_prop_string(global_ctx, -2, "registerCommand");

	duk_push_c_function(global_ctx, js_api_registerHook, 1);
	duk_put_prop_string(global_ctx, -2, "registerHook");

	duk_push_c_function(global_ctx, js_api_findClient, 1);
	duk_put_prop_string(global_ctx, -2, "findClient");

	duk_push_c_function(global_ctx, js_api_isOper, 1);
	duk_put_prop_string(global_ctx, -2, "isOper");

	duk_push_c_function(global_ctx, js_api_hasMode, 2);
	duk_put_prop_string(global_ctx, -2, "hasMode");

	duk_push_c_function(global_ctx, js_api_httpGet, 2);
	duk_put_prop_string(global_ctx, -2, "httpGet");

	duk_push_c_function(global_ctx, js_api_setInterval, 2);
	duk_put_prop_string(global_ctx, -2, "setInterval");

	duk_push_c_function(global_ctx, js_api_setTimeout, 2);
	duk_put_prop_string(global_ctx, -2, "setTimeout");

	duk_push_c_function(global_ctx, js_api_clearInterval, 1);
	duk_put_prop_string(global_ctx, -2, "clearInterval");

	duk_push_c_function(global_ctx, js_api_clearTimeout, 1);
	duk_put_prop_string(global_ctx, -2, "clearTimeout");

	/* Extended API v2.0 functions */
	duk_push_c_function(global_ctx, js_api_registerChannelMode, 1);
	duk_put_prop_string(global_ctx, -2, "registerChannelMode");

	duk_push_c_function(global_ctx, js_api_registerPrefixMode, 1);
	duk_put_prop_string(global_ctx, -2, "registerPrefixMode");

	duk_push_c_function(global_ctx, js_api_registerUserMode, 1);
	duk_put_prop_string(global_ctx, -2, "registerUserMode");

	duk_push_c_function(global_ctx, js_api_registerModData, 1);
	duk_put_prop_string(global_ctx, -2, "registerModData");

	duk_push_c_function(global_ctx, js_api_setModData, 3);
	duk_put_prop_string(global_ctx, -2, "setModData");

	duk_push_c_function(global_ctx, js_api_getModData, 2);
	duk_put_prop_string(global_ctx, -2, "getModData");

	duk_push_c_function(global_ctx, js_api_findChannel, 1);
	duk_put_prop_string(global_ctx, -2, "findChannel");

	duk_push_c_function(global_ctx, js_api_findServer, 1);
	duk_put_prop_string(global_ctx, -2, "findServer");

	duk_push_c_function(global_ctx, js_api_isUser, 1);
	duk_put_prop_string(global_ctx, -2, "isUser");

	duk_push_c_function(global_ctx, js_api_isServer, 1);
	duk_put_prop_string(global_ctx, -2, "isServer");

	duk_push_c_function(global_ctx, js_api_isLoggedIn, 1);
	duk_put_prop_string(global_ctx, -2, "isLoggedIn");

	duk_push_c_function(global_ctx, js_api_isSecure, 1);
	duk_put_prop_string(global_ctx, -2, "isSecure");

	duk_push_c_function(global_ctx, js_api_isULine, 1);
	duk_put_prop_string(global_ctx, -2, "isULine");

	duk_push_c_function(global_ctx, js_api_doCmd, 3);
	duk_put_prop_string(global_ctx, -2, "doCmd");

	duk_push_c_function(global_ctx, js_api_exitClient, 2);
	duk_put_prop_string(global_ctx, -2, "exitClient");

	duk_push_c_function(global_ctx, js_api_sendToChannel, DUK_VARARGS);
	duk_put_prop_string(global_ctx, -2, "sendToChannel");

	duk_push_c_function(global_ctx, js_api_sendToServer, DUK_VARARGS);
	duk_put_prop_string(global_ctx, -2, "sendToServer");

	duk_push_c_function(global_ctx, js_api_sendToAllServers, DUK_VARARGS);
	duk_put_prop_string(global_ctx, -2, "sendToAllServers");

	duk_push_c_function(global_ctx, js_api_setUserMode, 2);
	duk_put_prop_string(global_ctx, -2, "setUserMode");

	duk_push_c_function(global_ctx, js_api_setChannelMode, 3);
	duk_put_prop_string(global_ctx, -2, "setChannelMode");

	duk_push_c_function(global_ctx, js_api_checkChannelAccess, 3);
	duk_put_prop_string(global_ctx, -2, "checkChannelAccess");

	duk_push_c_function(global_ctx, js_api_getChannelMembers, 1);
	duk_put_prop_string(global_ctx, -2, "getChannelMembers");

	duk_push_c_function(global_ctx, js_api_getUserChannels, 1);
	duk_put_prop_string(global_ctx, -2, "getUserChannels");

	duk_push_c_function(global_ctx, js_api_joinChannel, 2);
	duk_put_prop_string(global_ctx, -2, "joinChannel");

	duk_push_c_function(global_ctx, js_api_partChannel, 3);
	duk_put_prop_string(global_ctx, -2, "partChannel");

	duk_push_c_function(global_ctx, js_api_kickUser, 4);
	duk_put_prop_string(global_ctx, -2, "kickUser");

	duk_push_c_function(global_ctx, js_api_setTopic, 3);
	duk_put_prop_string(global_ctx, -2, "setTopic");

	duk_push_c_function(global_ctx, js_api_changeNick, 2);
	duk_put_prop_string(global_ctx, -2, "changeNick");

	duk_push_c_function(global_ctx, js_api_setHost, 2);
	duk_put_prop_string(global_ctx, -2, "setHost");

	duk_push_c_function(global_ctx, js_api_addTKL, 1);
	duk_put_prop_string(global_ctx, -2, "addTKL");

	duk_push_c_function(global_ctx, js_api_delTKL, 1);
	duk_put_prop_string(global_ctx, -2, "delTKL");

	/* MessageTag API functions */
	duk_push_c_function(global_ctx, js_api_createMtag, 2);
	duk_put_prop_string(global_ctx, -2, "createMtag");

	duk_push_c_function(global_ctx, js_api_addMtag, 3);
	duk_put_prop_string(global_ctx, -2, "addMtag");

	duk_push_c_function(global_ctx, js_api_findMtag, 2);
	duk_put_prop_string(global_ctx, -2, "findMtag");

	duk_push_c_function(global_ctx, js_api_deleteMtag, 2);
	duk_put_prop_string(global_ctx, -2, "deleteMtag");

	duk_push_c_function(global_ctx, js_api_mtagsToString, 2);
	duk_put_prop_string(global_ctx, -2, "mtagsToString");

	duk_push_c_function(global_ctx, js_api_newMessageTags, 1);
	duk_put_prop_string(global_ctx, -2, "newMessageTags");

	/* Special list iteration API functions */
	duk_push_c_function(global_ctx, js_api_getAllClients, 0);
	duk_put_prop_string(global_ctx, -2, "getAllClients");

	duk_push_c_function(global_ctx, js_api_getAllLocalClients, 0);
	duk_put_prop_string(global_ctx, -2, "getAllLocalClients");

	duk_push_c_function(global_ctx, js_api_getAllServers, 0);
	duk_put_prop_string(global_ctx, -2, "getAllServers");

	duk_push_c_function(global_ctx, js_api_getAllLocalServers, 0);
	duk_put_prop_string(global_ctx, -2, "getAllLocalServers");

	duk_push_c_function(global_ctx, js_api_getAllOpers, 0);
	duk_put_prop_string(global_ctx, -2, "getAllOpers");

	duk_push_c_function(global_ctx, js_api_getAllChannels, 0);
	duk_put_prop_string(global_ctx, -2, "getAllChannels");

	/* RPC API functions */
	duk_push_c_function(global_ctx, js_api_registerRPCMethod, 1);
	duk_put_prop_string(global_ctx, -2, "registerRPCMethod");

	/* Message tag registration API */
	duk_push_c_function(global_ctx, js_api_registerMessageTag, 1);
	duk_put_prop_string(global_ctx, -2, "registerMessageTag");

	/* Extban API functions */
	duk_push_c_function(global_ctx, js_api_registerExtban, 1);
	duk_put_prop_string(global_ctx, -2, "registerExtban");

	/* Config block API functions */
	duk_push_c_function(global_ctx, js_api_registerConfigBlock, 1);
	duk_put_prop_string(global_ctx, -2, "registerConfigBlock");

	duk_push_c_function(global_ctx, js_api_config_error, 1);
	duk_put_prop_string(global_ctx, -2, "config_error");

	duk_push_c_function(global_ctx, js_api_config_warn, 1);
	duk_put_prop_string(global_ctx, -2, "config_warn");

	/* RPC response API functions */
	duk_push_c_function(global_ctx, js_api_rpcResponse, 1);
	duk_put_prop_string(global_ctx, -2, "rpcResponse");

	duk_push_c_function(global_ctx, js_api_rpcError, 2);
	duk_put_prop_string(global_ctx, -2, "rpcError");

	/* UnrealDB Database API functions */
	duk_push_c_function(global_ctx, js_api_dbOpen, 3);
	duk_put_prop_string(global_ctx, -2, "dbOpen");

	duk_push_c_function(global_ctx, js_api_dbClose, 1);
	duk_put_prop_string(global_ctx, -2, "dbClose");

	duk_push_c_function(global_ctx, js_api_dbWriteInt64, 2);
	duk_put_prop_string(global_ctx, -2, "dbWriteInt64");

	duk_push_c_function(global_ctx, js_api_dbWriteInt32, 2);
	duk_put_prop_string(global_ctx, -2, "dbWriteInt32");

	duk_push_c_function(global_ctx, js_api_dbWriteInt16, 2);
	duk_put_prop_string(global_ctx, -2, "dbWriteInt16");

	duk_push_c_function(global_ctx, js_api_dbWriteStr, 2);
	duk_put_prop_string(global_ctx, -2, "dbWriteStr");

	duk_push_c_function(global_ctx, js_api_dbWriteChar, 2);
	duk_put_prop_string(global_ctx, -2, "dbWriteChar");

	duk_push_c_function(global_ctx, js_api_dbReadInt64, 1);
	duk_put_prop_string(global_ctx, -2, "dbReadInt64");

	duk_push_c_function(global_ctx, js_api_dbReadInt32, 1);
	duk_put_prop_string(global_ctx, -2, "dbReadInt32");

	duk_push_c_function(global_ctx, js_api_dbReadInt16, 1);
	duk_put_prop_string(global_ctx, -2, "dbReadInt16");

	duk_push_c_function(global_ctx, js_api_dbReadStr, 1);
	duk_put_prop_string(global_ctx, -2, "dbReadStr");

	duk_push_c_function(global_ctx, js_api_dbReadChar, 1);
	duk_put_prop_string(global_ctx, -2, "dbReadChar");

	duk_push_c_function(global_ctx, js_api_dbGetError, 0);
	duk_put_prop_string(global_ctx, -2, "dbGetError");

	duk_pop(global_ctx);

	/* Define hook type constants - ALL HOOKS */
	duk_push_global_object(global_ctx);
	
	/* Connection hooks */
	duk_push_int(global_ctx, HOOKTYPE_PRE_LOCAL_CONNECT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_LOCAL_CONNECT");
	duk_push_int(global_ctx, HOOKTYPE_LOCAL_CONNECT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOCAL_CONNECT");
	duk_push_int(global_ctx, HOOKTYPE_REMOTE_CONNECT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_REMOTE_CONNECT");
	duk_push_int(global_ctx, HOOKTYPE_PRE_LOCAL_QUIT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_LOCAL_QUIT");
	duk_push_int(global_ctx, HOOKTYPE_LOCAL_QUIT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOCAL_QUIT");
	duk_push_int(global_ctx, HOOKTYPE_REMOTE_QUIT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_REMOTE_QUIT");
	duk_push_int(global_ctx, HOOKTYPE_UNKUSER_QUIT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_UNKUSER_QUIT");
	duk_push_int(global_ctx, HOOKTYPE_SECURE_CONNECT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SECURE_CONNECT");
	duk_push_int(global_ctx, HOOKTYPE_WELCOME); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_WELCOME");
	duk_push_int(global_ctx, HOOKTYPE_ACCOUNT_LOGIN); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_ACCOUNT_LOGIN");
	duk_push_int(global_ctx, HOOKTYPE_CLOSE_CONNECTION); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CLOSE_CONNECTION");
	duk_push_int(global_ctx, HOOKTYPE_IDENT_LOOKUP); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_IDENT_LOOKUP");
	duk_push_int(global_ctx, HOOKTYPE_PRE_LOCAL_HANDSHAKE_TIMEOUT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_LOCAL_HANDSHAKE_TIMEOUT");
	
	/* Server hooks */
	duk_push_int(global_ctx, HOOKTYPE_SERVER_CONNECT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SERVER_CONNECT");
	duk_push_int(global_ctx, HOOKTYPE_SERVER_HANDSHAKE_OUT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SERVER_HANDSHAKE_OUT");
	duk_push_int(global_ctx, HOOKTYPE_SERVER_SYNC); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SERVER_SYNC");
	duk_push_int(global_ctx, HOOKTYPE_POST_SERVER_CONNECT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_POST_SERVER_CONNECT");
	duk_push_int(global_ctx, HOOKTYPE_SERVER_SYNCED); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SERVER_SYNCED");
	duk_push_int(global_ctx, HOOKTYPE_SERVER_QUIT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SERVER_QUIT");
	
	/* Nick change hooks */
	duk_push_int(global_ctx, HOOKTYPE_LOCAL_NICKCHANGE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOCAL_NICKCHANGE");
	duk_push_int(global_ctx, HOOKTYPE_REMOTE_NICKCHANGE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_REMOTE_NICKCHANGE");
	duk_push_int(global_ctx, HOOKTYPE_POST_LOCAL_NICKCHANGE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_POST_LOCAL_NICKCHANGE");
	duk_push_int(global_ctx, HOOKTYPE_POST_REMOTE_NICKCHANGE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_POST_REMOTE_NICKCHANGE");
	duk_push_int(global_ctx, HOOKTYPE_CAN_USE_NICK); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CAN_USE_NICK");
	
	/* Join hooks */
	duk_push_int(global_ctx, HOOKTYPE_CAN_JOIN); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CAN_JOIN");
	duk_push_int(global_ctx, HOOKTYPE_PRE_LOCAL_JOIN); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_LOCAL_JOIN");
	duk_push_int(global_ctx, HOOKTYPE_LOCAL_JOIN); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOCAL_JOIN");
	duk_push_int(global_ctx, HOOKTYPE_REMOTE_JOIN); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_REMOTE_JOIN");
	duk_push_int(global_ctx, HOOKTYPE_CAN_JOIN_LIMITEXCEEDED); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CAN_JOIN_LIMITEXCEEDED");
	duk_push_int(global_ctx, HOOKTYPE_JOIN_DATA); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_JOIN_DATA");
	duk_push_int(global_ctx, HOOKTYPE_CAN_SAJOIN); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CAN_SAJOIN");
	
	/* Part hooks */
	duk_push_int(global_ctx, HOOKTYPE_PRE_LOCAL_PART); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_LOCAL_PART");
	duk_push_int(global_ctx, HOOKTYPE_LOCAL_PART); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOCAL_PART");
	duk_push_int(global_ctx, HOOKTYPE_REMOTE_PART); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_REMOTE_PART");
	duk_push_int(global_ctx, HOOKTYPE_PRE_LOCAL_QUIT_CHAN); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_LOCAL_QUIT_CHAN");
	
	/* Kick hooks */
	duk_push_int(global_ctx, HOOKTYPE_PRE_LOCAL_KICK); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_LOCAL_KICK");
	duk_push_int(global_ctx, HOOKTYPE_CAN_KICK); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CAN_KICK");
	duk_push_int(global_ctx, HOOKTYPE_LOCAL_KICK); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOCAL_KICK");
	duk_push_int(global_ctx, HOOKTYPE_REMOTE_KICK); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_REMOTE_KICK");
	
	/* Message hooks */
	duk_push_int(global_ctx, HOOKTYPE_PRE_CHANMSG); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_CHANMSG");
	duk_push_int(global_ctx, HOOKTYPE_CAN_SEND_TO_USER); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CAN_SEND_TO_USER");
	duk_push_int(global_ctx, HOOKTYPE_CAN_SEND_TO_CHANNEL); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CAN_SEND_TO_CHANNEL");
	duk_push_int(global_ctx, HOOKTYPE_USERMSG); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_USERMSG");
	duk_push_int(global_ctx, HOOKTYPE_CHANMSG); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CHANMSG");
	duk_push_int(global_ctx, HOOKTYPE_NEW_MESSAGE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_NEW_MESSAGE");
	duk_push_int(global_ctx, HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION");
	
	/* Topic hooks */
	duk_push_int(global_ctx, HOOKTYPE_PRE_LOCAL_TOPIC); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_LOCAL_TOPIC");
	duk_push_int(global_ctx, HOOKTYPE_TOPIC); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_TOPIC");
	duk_push_int(global_ctx, HOOKTYPE_CAN_SET_TOPIC); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CAN_SET_TOPIC");
	duk_push_int(global_ctx, HOOKTYPE_VIEW_TOPIC_OUTSIDE_CHANNEL); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_VIEW_TOPIC_OUTSIDE_CHANNEL");
	
	/* Mode hooks */
	duk_push_int(global_ctx, HOOKTYPE_PRE_LOCAL_CHANMODE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_LOCAL_CHANMODE");
	duk_push_int(global_ctx, HOOKTYPE_PRE_REMOTE_CHANMODE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_REMOTE_CHANMODE");
	duk_push_int(global_ctx, HOOKTYPE_LOCAL_CHANMODE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOCAL_CHANMODE");
	duk_push_int(global_ctx, HOOKTYPE_REMOTE_CHANMODE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_REMOTE_CHANMODE");
	duk_push_int(global_ctx, HOOKTYPE_MODECHAR_DEL); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_MODECHAR_DEL");
	duk_push_int(global_ctx, HOOKTYPE_MODECHAR_ADD); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_MODECHAR_ADD");
	duk_push_int(global_ctx, HOOKTYPE_UMODE_CHANGE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_UMODE_CHANGE");
	duk_push_int(global_ctx, HOOKTYPE_MODE_DEOP); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_MODE_DEOP");
	duk_push_int(global_ctx, HOOKTYPE_CHAN_PERMIT_NICK_CHANGE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CHAN_PERMIT_NICK_CHANGE");
	
	/* Invite hooks */
	duk_push_int(global_ctx, HOOKTYPE_PRE_INVITE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_INVITE");
	duk_push_int(global_ctx, HOOKTYPE_INVITE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_INVITE");
	duk_push_int(global_ctx, HOOKTYPE_INVITE_BYPASS); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_INVITE_BYPASS");
	duk_push_int(global_ctx, HOOKTYPE_IS_INVITED); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_IS_INVITED");
	
	/* Knock hooks */
	duk_push_int(global_ctx, HOOKTYPE_PRE_KNOCK); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_KNOCK");
	duk_push_int(global_ctx, HOOKTYPE_KNOCK); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_KNOCK");
	
	/* Away hooks */
	duk_push_int(global_ctx, HOOKTYPE_AWAY); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_AWAY");
	
	/* WHOIS/WHO hooks */
	duk_push_int(global_ctx, HOOKTYPE_WHOIS); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_WHOIS");
	duk_push_int(global_ctx, HOOKTYPE_WHO_STATUS); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_WHO_STATUS");
	duk_push_int(global_ctx, HOOKTYPE_SEE_CHANNEL_IN_WHOIS); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SEE_CHANNEL_IN_WHOIS");
	
	/* Kill hooks */
	duk_push_int(global_ctx, HOOKTYPE_PRE_KILL); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_KILL");
	duk_push_int(global_ctx, HOOKTYPE_LOCAL_KILL); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOCAL_KILL");
	
	/* Rehash/config hooks */
	duk_push_int(global_ctx, HOOKTYPE_REHASHFLAG); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_REHASHFLAG");
	duk_push_int(global_ctx, HOOKTYPE_CONFIGPOSTTEST); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CONFIGPOSTTEST");
	duk_push_int(global_ctx, HOOKTYPE_REHASH); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_REHASH");
	duk_push_int(global_ctx, HOOKTYPE_REHASH_COMPLETE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_REHASH_COMPLETE");
	duk_push_int(global_ctx, HOOKTYPE_CONFIGTEST); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CONFIGTEST");
	duk_push_int(global_ctx, HOOKTYPE_CONFIGRUN); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CONFIGRUN");
	duk_push_int(global_ctx, HOOKTYPE_CONFIGRUN_EX); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CONFIGRUN_EX");
	
	/* Stats hooks */
	duk_push_int(global_ctx, HOOKTYPE_STATS); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_STATS");
	
	/* Oper hooks */
	duk_push_int(global_ctx, HOOKTYPE_LOCAL_OPER); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOCAL_OPER");
	duk_push_int(global_ctx, HOOKTYPE_LOCAL_PASS); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOCAL_PASS");
	
	/* Channel hooks */
	duk_push_int(global_ctx, HOOKTYPE_CHANNEL_CREATE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CHANNEL_CREATE");
	duk_push_int(global_ctx, HOOKTYPE_CHANNEL_DESTROY); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CHANNEL_DESTROY");
	duk_push_int(global_ctx, HOOKTYPE_CHANNEL_SYNCED); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CHANNEL_SYNCED");
	duk_push_int(global_ctx, HOOKTYPE_IS_CHANNEL_SECURE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_IS_CHANNEL_SECURE");
	
	/* TKL (server bans) hooks */
	duk_push_int(global_ctx, HOOKTYPE_TKL_EXCEPT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_TKL_EXCEPT");
	duk_push_int(global_ctx, HOOKTYPE_TKL_ADD); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_TKL_ADD");
	duk_push_int(global_ctx, HOOKTYPE_TKL_DEL); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_TKL_DEL");
	duk_push_int(global_ctx, HOOKTYPE_FIND_TKLINE_MATCH); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_FIND_TKLINE_MATCH");
	
	/* Log hooks */
	duk_push_int(global_ctx, HOOKTYPE_LOG); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOG");
	
	/* Spamfilter hooks */
	duk_push_int(global_ctx, HOOKTYPE_LOCAL_SPAMFILTER); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_LOCAL_SPAMFILTER");
	duk_push_int(global_ctx, HOOKTYPE_TAKE_ACTION); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_TAKE_ACTION");
	
	/* Silence hooks */
	duk_push_int(global_ctx, HOOKTYPE_SILENCED); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SILENCED");
	
	/* Packet hooks */
	duk_push_int(global_ctx, HOOKTYPE_RAWPACKET_IN); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_RAWPACKET_IN");
	duk_push_int(global_ctx, HOOKTYPE_PACKET); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PACKET");
	duk_push_int(global_ctx, HOOKTYPE_HANDSHAKE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_HANDSHAKE");
	duk_push_int(global_ctx, HOOKTYPE_IS_HANDSHAKE_FINISHED); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_IS_HANDSHAKE_FINISHED");
	
	/* Client/user hooks */
	duk_push_int(global_ctx, HOOKTYPE_FREE_CLIENT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_FREE_CLIENT");
	duk_push_int(global_ctx, HOOKTYPE_FREE_USER); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_FREE_USER");
	duk_push_int(global_ctx, HOOKTYPE_USERHOST_CHANGE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_USERHOST_CHANGE");
	duk_push_int(global_ctx, HOOKTYPE_REALNAME_CHANGE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_REALNAME_CHANGE");
	duk_push_int(global_ctx, HOOKTYPE_IP_CHANGE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_IP_CHANGE");
	duk_push_int(global_ctx, HOOKTYPE_CONNECT_EXTINFO); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_CONNECT_EXTINFO");
	
	/* DCC hooks */
	duk_push_int(global_ctx, HOOKTYPE_DCC_DENIED); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_DCC_DENIED");
	
	/* SASL hooks */
	duk_push_int(global_ctx, HOOKTYPE_SASL_CONTINUATION); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SASL_CONTINUATION");
	duk_push_int(global_ctx, HOOKTYPE_SASL_RESULT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SASL_RESULT");
	duk_push_int(global_ctx, HOOKTYPE_SASL_AUTHENTICATE); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SASL_AUTHENTICATE");
	duk_push_int(global_ctx, HOOKTYPE_SASL_MECHS); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_SASL_MECHS");
	
	/* Command hooks */
	duk_push_int(global_ctx, HOOKTYPE_PRE_COMMAND); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_PRE_COMMAND");
	duk_push_int(global_ctx, HOOKTYPE_POST_COMMAND); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_POST_COMMAND");
	
	/* JSON-RPC hooks */
	duk_push_int(global_ctx, HOOKTYPE_JSON_EXPAND_CLIENT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_JSON_EXPAND_CLIENT");
	duk_push_int(global_ctx, HOOKTYPE_JSON_EXPAND_CLIENT_USER); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_JSON_EXPAND_CLIENT_USER");
	duk_push_int(global_ctx, HOOKTYPE_JSON_EXPAND_CLIENT_SERVER); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_JSON_EXPAND_CLIENT_SERVER");
	duk_push_int(global_ctx, HOOKTYPE_JSON_EXPAND_CHANNEL); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_JSON_EXPAND_CHANNEL");
	
	/* Accept hook */
	duk_push_int(global_ctx, HOOKTYPE_ACCEPT); duk_put_prop_string(global_ctx, -2, "HOOKTYPE_ACCEPT");

	/* ModData type constants */
	duk_push_int(global_ctx, MODDATATYPE_CLIENT);
	duk_put_prop_string(global_ctx, -2, "MODDATATYPE_CLIENT");
	duk_push_int(global_ctx, MODDATATYPE_LOCAL_CLIENT);
	duk_put_prop_string(global_ctx, -2, "MODDATATYPE_LOCAL_CLIENT");
	duk_push_int(global_ctx, MODDATATYPE_CHANNEL);
	duk_put_prop_string(global_ctx, -2, "MODDATATYPE_CHANNEL");
	duk_push_int(global_ctx, MODDATATYPE_MEMBER);
	duk_put_prop_string(global_ctx, -2, "MODDATATYPE_MEMBER");
	duk_push_int(global_ctx, MODDATATYPE_MEMBERSHIP);
	duk_put_prop_string(global_ctx, -2, "MODDATATYPE_MEMBERSHIP");
	duk_push_int(global_ctx, MODDATATYPE_LOCAL_VARIABLE);
	duk_put_prop_string(global_ctx, -2, "MODDATATYPE_LOCAL_VARIABLE");
	duk_push_int(global_ctx, MODDATATYPE_GLOBAL_VARIABLE);
	duk_put_prop_string(global_ctx, -2, "MODDATATYPE_GLOBAL_VARIABLE");

	/* Channel mode access ranks */
	duk_push_int(global_ctx, RANK_CHANOWNER);
	duk_put_prop_string(global_ctx, -2, "RANK_CHANOWNER");
	duk_push_int(global_ctx, RANK_CHANADMIN);
	duk_put_prop_string(global_ctx, -2, "RANK_CHANADMIN");
	duk_push_int(global_ctx, RANK_CHANOP);
	duk_put_prop_string(global_ctx, -2, "RANK_CHANOP");
	duk_push_int(global_ctx, RANK_HALFOP);
	duk_put_prop_string(global_ctx, -2, "RANK_HALFOP");
	duk_push_int(global_ctx, RANK_VOICE);
	duk_put_prop_string(global_ctx, -2, "RANK_VOICE");

	/* Mode access check constants */
	duk_push_int(global_ctx, EX_DENY);
	duk_put_prop_string(global_ctx, -2, "EX_DENY");
	duk_push_int(global_ctx, EX_ALLOW);
	duk_put_prop_string(global_ctx, -2, "EX_ALLOW");
	duk_push_int(global_ctx, EX_ALWAYS_DENY);
	duk_put_prop_string(global_ctx, -2, "EX_ALWAYS_DENY");

	/* SendType constants */
	duk_push_int(global_ctx, SEND_TYPE_PRIVMSG);
	duk_put_prop_string(global_ctx, -2, "SEND_TYPE_PRIVMSG");
	duk_push_int(global_ctx, SEND_TYPE_NOTICE);
	duk_put_prop_string(global_ctx, -2, "SEND_TYPE_NOTICE");
	duk_push_int(global_ctx, SEND_TYPE_TAGMSG);
	duk_put_prop_string(global_ctx, -2, "SEND_TYPE_TAGMSG");

	/* Extban option flags */
	duk_push_int(global_ctx, EXTBOPT_ACTMODIFIER);
	duk_put_prop_string(global_ctx, -2, "EXTBOPT_ACTMODIFIER");
	duk_push_int(global_ctx, EXTBOPT_NOSTACKCHILD);
	duk_put_prop_string(global_ctx, -2, "EXTBOPT_NOSTACKCHILD");
	duk_push_int(global_ctx, EXTBOPT_INVEX);
	duk_put_prop_string(global_ctx, -2, "EXTBOPT_INVEX");
	duk_push_int(global_ctx, EXTBOPT_TKL);
	duk_put_prop_string(global_ctx, -2, "EXTBOPT_TKL");

	/* JSON-RPC error codes */
	duk_push_int(global_ctx, JSON_RPC_ERROR_PARSE_ERROR);
	duk_put_prop_string(global_ctx, -2, "JSON_RPC_ERROR_PARSE_ERROR");
	duk_push_int(global_ctx, JSON_RPC_ERROR_INVALID_REQUEST);
	duk_put_prop_string(global_ctx, -2, "JSON_RPC_ERROR_INVALID_REQUEST");
	duk_push_int(global_ctx, JSON_RPC_ERROR_METHOD_NOT_FOUND);
	duk_put_prop_string(global_ctx, -2, "JSON_RPC_ERROR_METHOD_NOT_FOUND");
	duk_push_int(global_ctx, JSON_RPC_ERROR_INVALID_PARAMS);
	duk_put_prop_string(global_ctx, -2, "JSON_RPC_ERROR_INVALID_PARAMS");
	duk_push_int(global_ctx, JSON_RPC_ERROR_INTERNAL_ERROR);
	duk_put_prop_string(global_ctx, -2, "JSON_RPC_ERROR_INTERNAL_ERROR");
	duk_push_int(global_ctx, JSON_RPC_ERROR_NOT_FOUND);
	duk_put_prop_string(global_ctx, -2, "JSON_RPC_ERROR_NOT_FOUND");
	duk_push_int(global_ctx, JSON_RPC_ERROR_ALREADY_EXISTS);
	duk_put_prop_string(global_ctx, -2, "JSON_RPC_ERROR_ALREADY_EXISTS");
	duk_push_int(global_ctx, JSON_RPC_ERROR_INVALID_NAME);
	duk_put_prop_string(global_ctx, -2, "JSON_RPC_ERROR_INVALID_NAME");
	duk_push_int(global_ctx, JSON_RPC_ERROR_DENIED);
	duk_put_prop_string(global_ctx, -2, "JSON_RPC_ERROR_DENIED");

	/* Message tag handler flags */
	duk_push_int(global_ctx, MTAG_HANDLER_FLAGS_NO_CAP_NEEDED);
	duk_put_prop_string(global_ctx, -2, "MTAG_HANDLER_FLAGS_NO_CAP_NEEDED");

	/* UnrealDB mode constants */
	duk_push_int(global_ctx, UNREALDB_MODE_READ);
	duk_put_prop_string(global_ctx, -2, "DB_READ");
	duk_push_int(global_ctx, UNREALDB_MODE_WRITE);
	duk_put_prop_string(global_ctx, -2, "DB_WRITE");

	/* UnrealDB error constants */
	duk_push_int(global_ctx, UNREALDB_ERROR_SUCCESS);
	duk_put_prop_string(global_ctx, -2, "DB_ERROR_SUCCESS");
	duk_push_int(global_ctx, UNREALDB_ERROR_FILENOTFOUND);
	duk_put_prop_string(global_ctx, -2, "DB_ERROR_FILENOTFOUND");
	duk_push_int(global_ctx, UNREALDB_ERROR_CRYPTED);
	duk_put_prop_string(global_ctx, -2, "DB_ERROR_CRYPTED");
	duk_push_int(global_ctx, UNREALDB_ERROR_NOTCRYPTED);
	duk_put_prop_string(global_ctx, -2, "DB_ERROR_NOTCRYPTED");
	duk_push_int(global_ctx, UNREALDB_ERROR_HEADER);
	duk_put_prop_string(global_ctx, -2, "DB_ERROR_HEADER");
	duk_push_int(global_ctx, UNREALDB_ERROR_SECRET);
	duk_put_prop_string(global_ctx, -2, "DB_ERROR_SECRET");
	duk_push_int(global_ctx, UNREALDB_ERROR_PASSWORD);
	duk_put_prop_string(global_ctx, -2, "DB_ERROR_PASSWORD");
	duk_push_int(global_ctx, UNREALDB_ERROR_IO);
	duk_put_prop_string(global_ctx, -2, "DB_ERROR_IO");
	duk_push_int(global_ctx, UNREALDB_ERROR_API);
	duk_put_prop_string(global_ctx, -2, "DB_ERROR_API");
	duk_push_int(global_ctx, UNREALDB_ERROR_INTERNAL);
	duk_put_prop_string(global_ctx, -2, "DB_ERROR_INTERNAL");

	duk_pop(global_ctx);

	unreal_log(ULOG_INFO, "obbyscript", "JS_INIT_SUCCESS", NULL,
	           "JavaScript engine initialized successfully (Extended API v2.3 + RPC + Extbans + UnrealDB)");
}

/*
 * Push a Client object to the JavaScript stack
 */
void js_push_client_object(duk_context *ctx, Client *client)
{
	if (!client)
	{
		duk_push_null(ctx);
		return;
	}

	duk_idx_t obj_idx = duk_push_object(ctx);

	/* Basic client properties */
	duk_push_string(ctx, client->name);
	duk_put_prop_string(ctx, obj_idx, "name");

	if (*client->info)
	{
		duk_push_string(ctx, client->info);
		duk_put_prop_string(ctx, obj_idx, "info");
	}

	if (*client->id)
	{
		duk_push_string(ctx, client->id);
		duk_put_prop_string(ctx, obj_idx, "id");
	}

	if (client->ip)
	{
		duk_push_string(ctx, client->ip);
		duk_put_prop_string(ctx, obj_idx, "ip");
	}

	/* User-specific properties */
	if (IsUser(client) && client->user)
	{
		duk_push_string(ctx, client->user->username);
		duk_put_prop_string(ctx, obj_idx, "username");

		if (*client->user->realhost)
		{
			duk_push_string(ctx, client->user->realhost);
			duk_put_prop_string(ctx, obj_idx, "realhost");
		}

		if (client->user->virthost)
		{
			duk_push_string(ctx, client->user->virthost);
			duk_put_prop_string(ctx, obj_idx, "vhost");
		}

		if (client->user->server)
		{
			duk_push_string(ctx, client->user->server);
			duk_put_prop_string(ctx, obj_idx, "server");
		}

		if (IsLoggedIn(client))
		{
			duk_push_string(ctx, client->user->account);
			duk_put_prop_string(ctx, obj_idx, "account");
		}

		duk_push_boolean(ctx, IsOper(client));
		duk_put_prop_string(ctx, obj_idx, "isOper");

		duk_push_int(ctx, client->umodes);
		duk_put_prop_string(ctx, obj_idx, "umodes");
	}

	/* Server-specific properties */
	if (IsServer(client) && client->server)
	{
		duk_push_int(ctx, client->server->users);
		duk_put_prop_string(ctx, obj_idx, "users");
	}

	duk_push_boolean(ctx, IsUser(client));
	duk_put_prop_string(ctx, obj_idx, "isUser");

	duk_push_boolean(ctx, IsServer(client));
	duk_put_prop_string(ctx, obj_idx, "isServer");

	/* Store the client pointer for methods to use */
	duk_push_pointer(ctx, client);
	duk_put_prop_string(ctx, obj_idx, "\xff" "_ptr");

	/* Add accountLogin method */
	duk_push_c_function(ctx, js_client_accountLogin, 1);
	duk_put_prop_string(ctx, obj_idx, "accountLogin");

	/* Add accountLogout method */
	duk_push_c_function(ctx, js_client_accountLogout, 0);
	duk_put_prop_string(ctx, obj_idx, "accountLogout");
}

/*
 * Client method: accountLogin(accountName)
 * Logs the client into the specified account.
 * Sets client->user->account and calls user_account_login().
 */
duk_ret_t js_client_accountLogin(duk_context *ctx)
{
	const char *account_name;
	Client *client;

	/* Get the account name argument */
	account_name = duk_require_string(ctx, 0);

	/* Get 'this' object and retrieve the client pointer */
	duk_push_this(ctx);
	if (!duk_get_prop_string(ctx, -1, "\xff" "_ptr"))
	{
		duk_pop_2(ctx);
		duk_push_boolean(ctx, 0);
		return 1;
	}

	client = (Client *)duk_get_pointer(ctx, -1);
	duk_pop_2(ctx);

	if (!client || !IsUser(client) || !client->user)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Set the account */
	strlcpy(client->user->account, account_name, sizeof(client->user->account));

	/* Call user_account_login to propagate the change */
	user_account_login(NULL, client);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * Client method: accountLogout()
 * Logs the client out of their account.
 * Sets client->user->account to "0" and calls user_account_login().
 */
duk_ret_t js_client_accountLogout(duk_context *ctx)
{
	Client *client;

	/* Get 'this' object and retrieve the client pointer */
	duk_push_this(ctx);
	if (!duk_get_prop_string(ctx, -1, "\xff" "_ptr"))
	{
		duk_pop_2(ctx);
		duk_push_boolean(ctx, 0);
		return 1;
	}

	client = (Client *)duk_get_pointer(ctx, -1);
	duk_pop_2(ctx);

	if (!client || !IsUser(client) || !client->user)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Set account to "0" (logged out) */
	strlcpy(client->user->account, "0", sizeof(client->user->account));

	/* Call user_account_login to propagate the change */
	user_account_login(NULL, client);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * Push a Channel object to the JavaScript stack
 */
void js_push_channel_object(duk_context *ctx, Channel *channel)
{
	if (!channel)
	{
		duk_push_null(ctx);
		return;
	}

	duk_idx_t obj_idx = duk_push_object(ctx);

	duk_push_string(ctx, channel->name);
	duk_put_prop_string(ctx, obj_idx, "name");

	if (channel->topic)
	{
		duk_push_string(ctx, channel->topic);
		duk_put_prop_string(ctx, obj_idx, "topic");
	}

	duk_push_int(ctx, channel->users);
	duk_put_prop_string(ctx, obj_idx, "users");

	duk_push_int(ctx, channel->mode.mode);
	duk_put_prop_string(ctx, obj_idx, "modes");
}

/*
 * Push a MessageTag linked list to the JavaScript stack as an array of objects
 * Each object has 'name' and optionally 'value' properties
 */
void js_push_mtags_object(duk_context *ctx, MessageTag *mtags)
{
	duk_idx_t arr_idx;
	int i = 0;
	MessageTag *m;

	if (!mtags)
	{
		duk_push_null(ctx);
		return;
	}

	arr_idx = duk_push_array(ctx);

	for (m = mtags; m; m = m->next)
	{
		duk_idx_t obj_idx = duk_push_object(ctx);

		duk_push_string(ctx, m->name);
		duk_put_prop_string(ctx, obj_idx, "name");

		if (m->value)
		{
			duk_push_string(ctx, m->value);
			duk_put_prop_string(ctx, obj_idx, "value");
		}
		else
		{
			duk_push_null(ctx);
			duk_put_prop_string(ctx, obj_idx, "value");
		}

		duk_put_prop_index(ctx, arr_idx, i++);
	}
}

/*
 * Pop a JavaScript array of mtag objects from the stack and convert to MessageTag linked list
 * Returns NULL if the value at idx is null/undefined or not a valid array
 * Caller is responsible for freeing the returned list with safe_free_message_tags()
 */
MessageTag *js_pop_mtags(duk_context *ctx, duk_idx_t idx)
{
	MessageTag *mtags = NULL;
	MessageTag *m;
	duk_size_t len, i;

	if (duk_is_null_or_undefined(ctx, idx))
		return NULL;

	if (!duk_is_array(ctx, idx))
		return NULL;

	len = duk_get_length(ctx, idx);

	for (i = 0; i < len; i++)
	{
		duk_get_prop_index(ctx, idx, i);

		if (duk_is_object(ctx, -1))
		{
			const char *name = NULL;
			const char *value = NULL;

			duk_get_prop_string(ctx, -1, "name");
			if (duk_is_string(ctx, -1))
				name = duk_get_string(ctx, -1);
			duk_pop(ctx);

			duk_get_prop_string(ctx, -1, "value");
			if (duk_is_string(ctx, -1))
				value = duk_get_string(ctx, -1);
			duk_pop(ctx);

			if (name)
			{
				m = safe_alloc(sizeof(MessageTag));
				safe_strdup(m->name, name);
				if (value)
					safe_strdup(m->value, value);
				AddListItem(m, mtags);
			}
		}

		duk_pop(ctx);
	}

	return mtags;
}

/*
 * API: sendNotice(client, message)
 * Send a NOTICE to a client
 */
duk_ret_t js_api_sendNotice(duk_context *ctx)
{
	Client *client;
	const char *message;

	/* Get client from first argument (should be $client object) */
	if (!duk_is_object(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "First argument must be a client object");
		return 0;
	}

	duk_get_prop_string(ctx, 0, "name");
	const char *client_name = duk_get_string(ctx, -1);
	client = find_client(client_name, NULL);
	duk_pop(ctx);

	if (!client || !IsUser(client))
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	message = duk_require_string(ctx, 1);

	sendnotice(client, "%s", message);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: sendNumeric(client, numeric, message)
 * Send a numeric reply to a client
 */
duk_ret_t js_api_sendNumeric(duk_context *ctx)
{
	Client *client;
	int numeric;
	const char *message;

	/* Get client from first argument */
	if (!duk_is_object(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "First argument must be a client object");
		return 0;
	}

	duk_get_prop_string(ctx, 0, "name");
	const char *client_name = duk_get_string(ctx, -1);
	client = find_client(client_name, NULL);
	duk_pop(ctx);

	if (!client || !IsUser(client))
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	numeric = duk_require_int(ctx, 1);
	message = duk_require_string(ctx, 2);

	sendnumericfmt(client, numeric, "%s", message);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: sendRaw(client, raw)
 * Send raw IRC protocol message to a client
 */
duk_ret_t js_api_sendRaw(duk_context *ctx)
{
	Client *client;
	const char *raw;

	/* Get client from first argument */
	if (!duk_is_object(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "First argument must be a client object");
		return 0;
	}

	duk_get_prop_string(ctx, 0, "name");
	const char *client_name = duk_get_string(ctx, -1);
	client = find_client(client_name, NULL);
	duk_pop(ctx);

	if (!client)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	raw = duk_require_string(ctx, 1);
	sendto_one(client, NULL, "%s", raw);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: log(message)
 * Log a message to UnrealIRCd logs
 */
duk_ret_t js_api_log(duk_context *ctx)
{
	const char *message = duk_require_string(ctx, 0);
	
	unreal_log(ULOG_INFO, "obbyscript", "JS_SCRIPT_LOG", NULL,
	           "[JS] $message", log_data_string("message", message));
	
	return 0;
}

/*
 * API: findClient(name)
 * Find a client by name or UID
 */
duk_ret_t js_api_findClient(duk_context *ctx)
{
	const char *name = duk_require_string(ctx, 0);
	Client *client = find_client(name, NULL);
	
	js_push_client_object(ctx, client);
	return 1;
}

/*
 * API: isOper(client)
 * Check if a client is an IRC operator
 */
duk_ret_t js_api_isOper(duk_context *ctx)
{
	if (!duk_is_object(ctx, 0))
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	duk_get_prop_string(ctx, 0, "name");
	const char *client_name = duk_get_string(ctx, -1);
	Client *client = find_client(client_name, NULL);
	duk_pop(ctx);

	duk_push_boolean(ctx, client && IsOper(client));
	return 1;
}

/*
 * API: hasMode(client, mode)
 * Check if a client has a specific user mode
 */
duk_ret_t js_api_hasMode(duk_context *ctx)
{
	if (!duk_is_object(ctx, 0))
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	duk_get_prop_string(ctx, 0, "name");
	const char *client_name = duk_get_string(ctx, -1);
	Client *client = find_client(client_name, NULL);
	duk_pop(ctx);

	if (!client || !IsUser(client))
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	const char *mode_str = duk_require_string(ctx, 1);
	if (strlen(mode_str) != 1)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	int mode = mode_str[0];
	long mode_flag = find_user_mode(mode);
	
	duk_push_boolean(ctx, (client->umodes & mode_flag) ? 1 : 0);
	return 1;
}

/*
 * API: httpGet(url, callback)
 * Make an async HTTP GET request. Callback receives (error, response).
 * Note: The callback is called globally, not in command context.
 */
duk_ret_t js_api_httpGet(duk_context *ctx)
{
	const char *url;
	const char *callback_name;
	OutgoingWebRequest *request;
	JSHttpRequest *jsreq;

	url = duk_require_string(ctx, 0);
	callback_name = duk_require_string(ctx, 1);

	/* Validate URL */
	if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "URL must start with http:// or https://");
		return 0;
	}

	/* Create tracking structure */
	jsreq = safe_alloc(sizeof(JSHttpRequest));
	safe_strdup(jsreq->callback_name, callback_name);
	jsreq->ctx = global_ctx;
	AddListItem(jsreq, js_http_requests);

	/* Create the web request */
	request = safe_alloc(sizeof(OutgoingWebRequest));
	safe_strdup(request->url, url);
	request->http_method = HTTP_METHOD_GET;
	safe_strdup(request->apicallback, "js_http_callback");
	request->callback_data = jsreq;
	request->max_redirects = 3;
	request->connect_timeout = 10;
	request->transfer_timeout = 30;

	/* Start async request */
	url_start_async(request);

	unreal_log(ULOG_DEBUG, "obbyscript", "JS_HTTP_REQUEST", NULL,
	           "JavaScript HTTP request started: $url -> callback $callback",
	           log_data_string("url", url),
	           log_data_string("callback", callback_name));

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * HTTP callback - called when async HTTP request completes
 */
void js_http_callback(OutgoingWebRequest *request, OutgoingWebResponse *response)
{
	JSHttpRequest *jsreq = (JSHttpRequest *)response->ptr;
	const char *error = NULL;
	const char *body = NULL;

	if (!jsreq || !jsreq->callback_name || !global_ctx)
	{
		if (jsreq)
		{
			safe_free(jsreq->callback_name);
			safe_free(jsreq->client_name);
			DelListItem(jsreq, js_http_requests);
			safe_free(jsreq);
		}
		return;
	}

	/* Get error or response body */
	if (response->errorbuf)
		error = response->errorbuf;
	else if (response->memory)
		body = response->memory;
	else
		error = "Empty response";

	/* Call the JavaScript callback function */
	duk_push_global_object(global_ctx);
	duk_get_prop_string(global_ctx, -1, jsreq->callback_name);

	if (!duk_is_function(global_ctx, -1))
	{
		unreal_log(ULOG_WARNING, "obbyscript", "JS_HTTP_CALLBACK_NOT_FOUND", NULL,
		           "HTTP callback function not found: $callback",
		           log_data_string("callback", jsreq->callback_name));
		duk_pop_2(global_ctx);
		goto cleanup;
	}

	/* Push arguments: (error, body) */
	if (error)
		duk_push_string(global_ctx, error);
	else
		duk_push_null(global_ctx);

	if (body)
		duk_push_string(global_ctx, body);
	else
		duk_push_null(global_ctx);

	/* Call callback(error, body) */
	if (duk_pcall(global_ctx, 2) != 0)
	{
		const char *err = duk_safe_to_string(global_ctx, -1);
		unreal_log(ULOG_ERROR, "obbyscript", "JS_HTTP_CALLBACK_ERROR", NULL,
		           "Error in HTTP callback: $error",
		           log_data_string("error", err));
	}

	duk_pop_2(global_ctx); /* pop result and global object */

cleanup:
	safe_free(jsreq->callback_name);
	safe_free(jsreq->client_name);
	DelListItem(jsreq, js_http_requests);
	safe_free(jsreq);
}

/*
 * Generic command handler wrapper for JavaScript commands
 */
CMD_FUNC(js_command_handler)
{
	JSCommand *jscmd = NULL;
	const char *cmdname;

	/* Get command name from client context */
	if (!clictx || !clictx->cmd || !clictx->cmd->cmd)
	{
		unreal_log(ULOG_ERROR, "obbyscript", "JS_COMMAND_NULL_CTX", client,
		           "JavaScript command handler called with NULL context");
		return;
	}
	cmdname = clictx->cmd->cmd;

	/* Find the JSCommand for this command */
	for (jscmd = js_commands; jscmd; jscmd = jscmd->next)
	{
		if (jscmd->name && !strcasecmp(jscmd->name, cmdname))
			break;
	}

	if (!jscmd || !jscmd->ctx || !jscmd->handler_code)
	{
		unreal_log(ULOG_ERROR, "obbyscript", "JS_COMMAND_NOT_FOUND", client,
		           "JavaScript command handler not found or invalid for: $command",
		           log_data_string("command", parv[0]));
		return;
	}

	/* Set up JavaScript context with command parameters */
	duk_push_global_object(jscmd->ctx);

	/* Push $client object */
	js_push_client_object(jscmd->ctx, client);
	duk_put_prop_string(jscmd->ctx, -2, "$client");

	/* Push $parc */
	duk_push_int(jscmd->ctx, parc);
	duk_put_prop_string(jscmd->ctx, -2, "$parc");

	/* Push $parv array - note: in UnrealIRCd, parv[0] is garbage/unset, params start at parv[1] */
	duk_idx_t parv_arr = duk_push_array(jscmd->ctx);
	for (int i = 1; i < parc && i < 15; i++)
	{
		if (parv[i])
			duk_push_string(jscmd->ctx, parv[i]);
		else
			duk_push_null(jscmd->ctx);
		duk_put_prop_index(jscmd->ctx, parv_arr, i - 1); /* Store at 0-based index */
	}
	duk_put_prop_string(jscmd->ctx, -2, "$parv");

	duk_pop(jscmd->ctx); /* pop global object */

	/* Get the handler function from stash and execute it */
	duk_push_heap_stash(jscmd->ctx);
	duk_get_prop_string(jscmd->ctx, -1, jscmd->handler_code); /* handler_code contains stash key */
	duk_remove(jscmd->ctx, -2); /* remove stash */

	if (!duk_is_function(jscmd->ctx, -1))
	{
		unreal_log(ULOG_ERROR, "obbyscript", "JS_COMMAND_ERROR", client,
		           "Handler function not found in stash for $command",
		           log_data_string("command", jscmd->name));
		duk_pop(jscmd->ctx);
		return;
	}

	/* Call the handler function */
	if (duk_pcall(jscmd->ctx, 0) != 0)
	{
		const char *error = duk_safe_to_string(jscmd->ctx, -1);
		unreal_log(ULOG_ERROR, "obbyscript", "JS_COMMAND_ERROR", client,
		           "Error executing JavaScript command handler for $command: $error",
		           log_data_string("command", jscmd->name),
		           log_data_string("error", error));
		sendnotice(client, "*** JavaScript error: %s", error);
	}
	duk_pop(jscmd->ctx); /* pop result/error */
}

/*
 * API: registerCommand(config)
 * Register a new IRC command from JavaScript
 * config = { name: "COMMANDNAME", handler: function() { ... } }
 */
duk_ret_t js_api_registerCommand(duk_context *ctx)
{
	JSCommand *jscmd;
	const char *cmd_name;
	int cmd_flags = CMD_USER; /* Default to CMD_USER */

	if (!duk_is_object(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Argument must be an object");
		return 0;
	}

	/* Get command name */
	duk_get_prop_string(ctx, 0, "name");
	if (!duk_is_string(ctx, -1))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Command name must be a string");
		return 0;
	}
	cmd_name = duk_get_string(ctx, -1);
	
	/* Create JSCommand structure - copy name immediately since Duktape string is temporary */
	jscmd = safe_alloc(sizeof(JSCommand));
	safe_strdup(jscmd->name, cmd_name);
	duk_pop(ctx); /* Now safe to pop after we've copied the name */

	/* Get optional flags */
	if (duk_get_prop_string(ctx, 0, "flags"))
	{
		if (duk_is_string(ctx, -1))
		{
			const char *flags_str = duk_get_string(ctx, -1);
			cmd_flags = 0; /* Reset to 0 when explicitly specified */
			
			/* Parse flags string (e.g., "USER|OPER|SERVER") */
			if (strstr(flags_str, "USER"))
				cmd_flags |= CMD_USER;
			if (strstr(flags_str, "OPER"))
				cmd_flags |= CMD_OPER;
			if (strstr(flags_str, "SERVER"))
				cmd_flags |= CMD_SERVER;
			if (strstr(flags_str, "UNREGISTERED"))
				cmd_flags |= CMD_UNREGISTERED;
			if (strstr(flags_str, "SHUN"))
				cmd_flags |= CMD_SHUN;
			
			/* If no flags matched, default to CMD_USER */
			if (cmd_flags == 0)
				cmd_flags = CMD_USER;
		}
		else if (duk_is_number(ctx, -1))
		{
			/* Allow direct numeric flags */
			cmd_flags = duk_get_int(ctx, -1);
		}
	}
	duk_pop(ctx); /* pop flags or undefined */

	/* Get handler function */
	duk_get_prop_string(ctx, 0, "handler");
	if (!duk_is_function(ctx, -1))
	{
		safe_free(jscmd->name);
		safe_free(jscmd);
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Handler must be a function");
		return 0;
	}

	jscmd->ctx = global_ctx; /* Use global context */

	/* Store the handler function in a stash with a unique key */
	char stash_key[128];
	snprintf(stash_key, sizeof(stash_key), "cmd_handler_%s", jscmd->name);
	
	/* Stack: [config_obj, handler_func] */
	duk_push_heap_stash(ctx);          /* Stack: [config_obj, handler_func, stash] */
	duk_dup(ctx, -2);                  /* Stack: [config_obj, handler_func, stash, handler_func_copy] */
	duk_put_prop_string(ctx, -2, stash_key); /* Stack: [config_obj, handler_func, stash] */
	duk_pop(ctx);                      /* Stack: [config_obj, handler_func] */

	safe_strdup(jscmd->handler_code, stash_key); /* Store stash key */

	/* Register the command with UnrealIRCd */
	CommandAdd(js_modinfo->handle, jscmd->name, js_command_handler, MAXPARA, cmd_flags);

	/* Add to linked list */
	AddListItem(jscmd, js_commands);

	duk_pop(ctx); /* pop handler function */

	unreal_log(ULOG_INFO, "obbyscript", "JS_COMMAND_REGISTERED", NULL,
	           "JavaScript command registered: $command",
	           log_data_string("command", jscmd->name));

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * Generic hook handler wrapper - example for LOCAL_CONNECT
 */
int js_hook_local_connect(Client *client)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_LOCAL_CONNECT)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		/* Set up context */
		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_pop(jshook->ctx);

		/* Get handler from stash and execute */
		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2); /* remove stash */

		if (!duk_is_function(jshook->ctx, -1))
		{
			unreal_log(ULOG_ERROR, "obbyscript", "JS_HOOK_ERROR", client,
			           "Hook handler function not found in stash");
			duk_pop(jshook->ctx);
			continue;
		}

		/* Call the handler */
		if (duk_pcall(jshook->ctx, 0) != 0)
		{
			const char *error = duk_safe_to_string(jshook->ctx, -1);
			unreal_log(ULOG_ERROR, "obbyscript", "JS_HOOK_ERROR", client,
			           "Error executing JavaScript hook handler: $error",
			           log_data_string("error", error));
		}
		else if (duk_is_number(jshook->ctx, -1))
		{
			retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}

	return retval;
}

/*
 * Hook handler for REMOTE_CONNECT
 */
int js_hook_remote_connect(Client *client)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_REMOTE_CONNECT)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for LOCAL_QUIT
 */
int js_hook_local_quit(Client *client, MessageTag *mtags, const char *comment)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_LOCAL_QUIT)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for REMOTE_QUIT
 */
int js_hook_remote_quit(Client *client, MessageTag *mtags, const char *comment)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_REMOTE_QUIT)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for LOCAL_JOIN
 */
int js_hook_local_join(Client *client, Channel *channel, MessageTag *mtags)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_LOCAL_JOIN)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for REMOTE_JOIN
 */
int js_hook_remote_join(Client *client, Channel *channel, MessageTag *mtags)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_REMOTE_JOIN)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for LOCAL_PART
 */
int js_hook_local_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_LOCAL_PART)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for REMOTE_PART
 */
int js_hook_remote_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_REMOTE_PART)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for LOCAL_KICK
 */
int js_hook_local_kick(Client *client, Client *victim, Channel *channel, MessageTag *mtags, const char *comment)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_LOCAL_KICK)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, victim);
		duk_put_prop_string(jshook->ctx, -2, "$victim");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for REMOTE_KICK
 */
int js_hook_remote_kick(Client *client, Client *victim, Channel *channel, MessageTag *mtags, const char *comment)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_REMOTE_KICK)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, victim);
		duk_put_prop_string(jshook->ctx, -2, "$victim");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for CHANMSG
 */
int js_hook_chanmsg(Client *client, Channel *channel, int sendflags, const char *member_modes, const char *target, MessageTag *mtags, const char *text, SendType sendtype)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_CHANMSG)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, text ? text : "");
		duk_put_prop_string(jshook->ctx, -2, "$text");
		duk_push_string(jshook->ctx, target ? target : "");
		duk_put_prop_string(jshook->ctx, -2, "$target");
		duk_push_int(jshook->ctx, sendtype);
		duk_put_prop_string(jshook->ctx, -2, "$sendtype");
		duk_push_boolean(jshook->ctx, sendtype == SEND_TYPE_PRIVMSG);
		duk_put_prop_string(jshook->ctx, -2, "$isPrivmsg");
		duk_push_boolean(jshook->ctx, sendtype == SEND_TYPE_NOTICE);
		duk_put_prop_string(jshook->ctx, -2, "$isNotice");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for USERMSG
 */
int js_hook_usermsg(Client *client, Client *to, MessageTag *mtags, const char *text, SendType sendtype)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_USERMSG)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, to);
		duk_put_prop_string(jshook->ctx, -2, "$target");
		duk_push_string(jshook->ctx, text ? text : "");
		duk_put_prop_string(jshook->ctx, -2, "$text");
		duk_push_int(jshook->ctx, sendtype);
		duk_put_prop_string(jshook->ctx, -2, "$sendtype");
		duk_push_boolean(jshook->ctx, sendtype == SEND_TYPE_PRIVMSG);
		duk_put_prop_string(jshook->ctx, -2, "$isPrivmsg");
		duk_push_boolean(jshook->ctx, sendtype == SEND_TYPE_NOTICE);
		duk_put_prop_string(jshook->ctx, -2, "$isNotice");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for LOCAL_NICKCHANGE
 */
int js_hook_local_nickchange(Client *client, MessageTag *mtags, const char *newnick)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_LOCAL_NICKCHANGE)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, newnick ? newnick : "");
		duk_put_prop_string(jshook->ctx, -2, "$newnick");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for REMOTE_NICKCHANGE
 */
int js_hook_remote_nickchange(Client *client, MessageTag *mtags, const char *newnick)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_REMOTE_NICKCHANGE)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, newnick ? newnick : "");
		duk_put_prop_string(jshook->ctx, -2, "$newnick");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for TOPIC
 */
int js_hook_topic(Client *client, Channel *channel, MessageTag *mtags, const char *topic)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_TOPIC)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, topic ? topic : "");
		duk_put_prop_string(jshook->ctx, -2, "$topic");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for AWAY
 */
int js_hook_away(Client *client, MessageTag *mtags, const char *reason, int already_as_away)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_AWAY)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		if (reason)
			duk_push_string(jshook->ctx, reason);
		else
			duk_push_null(jshook->ctx);
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		duk_push_boolean(jshook->ctx, reason != NULL);
		duk_put_prop_string(jshook->ctx, -2, "$isAway");
		duk_push_boolean(jshook->ctx, already_as_away);
		duk_put_prop_string(jshook->ctx, -2, "$wasAlreadyAway");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for LOCAL_OPER
 */
int js_hook_local_oper(Client *client, int add, const char *oper_block, const char *operclass)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_LOCAL_OPER)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_boolean(jshook->ctx, add);
		duk_put_prop_string(jshook->ctx, -2, "$isOper");
		duk_push_string(jshook->ctx, oper_block ? oper_block : "");
		duk_put_prop_string(jshook->ctx, -2, "$operBlock");
		duk_push_string(jshook->ctx, operclass ? operclass : "");
		duk_put_prop_string(jshook->ctx, -2, "$operClass");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for CHANNEL_CREATE
 */
int js_hook_channel_create(Channel *channel)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_CHANNEL_CREATE)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook handler for CHANNEL_DESTROY
 */
int js_hook_channel_destroy(Channel *channel, int *should_destroy)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_CHANNEL_DESTROY)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_CAN_USE_NICK
 * Check if a client can use a specific nickname
 */
int js_hook_can_use_nick(Client *client, const char *newnick, const char **reject_reason)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_CAN_USE_NICK)
			continue;

		if (!jshook->ctx || !jshook->handler_code)
			continue;

		/* Set up global variables */
		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, newnick);
		duk_put_prop_string(jshook->ctx, -2, "$newnick");
		duk_pop(jshook->ctx);

		/* Get handler function from stash */
		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		/* Execute handler */
		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0)
			{
				if (duk_is_number(jshook->ctx, -1))
				{
					int hook_ret = duk_get_int(jshook->ctx, -1);
					if (hook_ret != 0)
					{
						/* Non-zero means deny */
						retval = hook_ret;
						if (reject_reason)
							*reject_reason = "Nickname is reserved";
					}
				}
			}
		}
		duk_pop(jshook->ctx);

		/* If any hook denies, we return immediately */
		if (retval != 0)
			break;
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_LOCAL_CONNECT
 * Called before a local user connects, can deny connection
 */
int js_hook_pre_local_connect(Client *client)
{
	JSHook *jshook;
	int retval = HOOK_CONTINUE;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_LOCAL_CONNECT)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_LOCAL_QUIT
 * Called before a local quit, can modify quit reason
 */
const char *js_hook_pre_local_quit(Client *client, const char *comment)
{
	JSHook *jshook;
	static char new_comment[512];
	const char *result = comment;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_LOCAL_QUIT)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_string(jshook->ctx, -1))
			{
				strlcpy(new_comment, duk_get_string(jshook->ctx, -1), sizeof(new_comment));
				result = new_comment;
			}
		}
		duk_pop(jshook->ctx);
	}
	return result;
}

/*
 * Hook: HOOKTYPE_UNKUSER_QUIT
 * Called when an unregistered user quits
 */
int js_hook_unkuser_quit(Client *client, MessageTag *mtags, const char *comment)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_UNKUSER_QUIT)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_SERVER_CONNECT
 * Called when a server connects
 */
int js_hook_server_connect(Client *client)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_SERVER_CONNECT)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$server");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_SERVER_QUIT
 * Called when a server disconnects
 */
int js_hook_server_quit(Client *client, MessageTag *mtags)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_SERVER_QUIT)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$server");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_CAN_JOIN
 * Check if a user can join a channel
 */
int js_hook_can_join(Client *client, Channel *channel, const char *key, char **errmsg)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_CAN_JOIN)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, key ? key : "");
		duk_put_prop_string(jshook->ctx, -2, "$key");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
		if (retval != 0)
			break;
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_LOCAL_JOIN
 * Called before a local user joins a channel
 */
int js_hook_pre_local_join(Client *client, Channel *channel, const char *key)
{
	JSHook *jshook;
	int retval = HOOK_CONTINUE;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_LOCAL_JOIN)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, key ? key : "");
		duk_put_prop_string(jshook->ctx, -2, "$key");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_LOCAL_PART
 * Called before a local user parts a channel
 */
const char *js_hook_pre_local_part(Client *client, Channel *channel, const char *comment)
{
	JSHook *jshook;
	static char new_comment[512];
	const char *result = comment;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_LOCAL_PART)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_string(jshook->ctx, -1))
			{
				strlcpy(new_comment, duk_get_string(jshook->ctx, -1), sizeof(new_comment));
				result = new_comment;
			}
		}
		duk_pop(jshook->ctx);
	}
	return result;
}

/*
 * Hook: HOOKTYPE_PRE_LOCAL_KICK
 * Called before a local kick
 */
const char *js_hook_pre_local_kick(Client *client, Client *victim, Channel *channel, const char *comment)
{
	JSHook *jshook;
	static char new_comment[512];
	const char *result = comment;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_LOCAL_KICK)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, victim);
		duk_put_prop_string(jshook->ctx, -2, "$victim");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_string(jshook->ctx, -1))
			{
				strlcpy(new_comment, duk_get_string(jshook->ctx, -1), sizeof(new_comment));
				result = new_comment;
			}
		}
		duk_pop(jshook->ctx);
	}
	return result;
}

/*
 * Hook: HOOKTYPE_CAN_KICK
 * Check if a user can kick another user
 */
int js_hook_can_kick(Client *client, Client *victim, Channel *channel, const char *comment, const char *client_modes, const char *victim_modes, const char **errmsg)
{
	JSHook *jshook;
	int retval = EX_ALLOW;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_CAN_KICK)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, victim);
		duk_put_prop_string(jshook->ctx, -2, "$victim");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		duk_push_string(jshook->ctx, client_modes ? client_modes : "");
		duk_put_prop_string(jshook->ctx, -2, "$clientModes");
		duk_push_string(jshook->ctx, victim_modes ? victim_modes : "");
		duk_put_prop_string(jshook->ctx, -2, "$victimModes");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_CHANMSG
 * Called before a channel message
 */
int js_hook_pre_chanmsg(Client *client, Channel *channel, MessageTag **mtags, const char *text, SendType sendtype)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_CHANMSG)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, text ? text : "");
		duk_put_prop_string(jshook->ctx, -2, "$text");
		duk_push_int(jshook->ctx, (int)sendtype);
		duk_put_prop_string(jshook->ctx, -2, "$sendtype");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_CAN_SEND_TO_CHANNEL
 * Check if a user can send to a channel
 */
int js_hook_can_send_to_channel(Client *client, Channel *channel, Membership *member, const char **text, const char **errmsg, SendType sendtype, ClientContext *clictx)
{
	JSHook *jshook;
	int retval = HOOK_CONTINUE;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_CAN_SEND_TO_CHANNEL)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, text && *text ? *text : "");
		duk_put_prop_string(jshook->ctx, -2, "$text");
		duk_push_int(jshook->ctx, (int)sendtype);
		duk_put_prop_string(jshook->ctx, -2, "$sendtype");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_CAN_SEND_TO_USER
 * Check if a user can send to another user
 */
int js_hook_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype, ClientContext *clictx)
{
	JSHook *jshook;
	int retval = HOOK_CONTINUE;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_CAN_SEND_TO_USER)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, target);
		duk_put_prop_string(jshook->ctx, -2, "$target");
		duk_push_string(jshook->ctx, text && *text ? *text : "");
		duk_put_prop_string(jshook->ctx, -2, "$text");
		duk_push_int(jshook->ctx, (int)sendtype);
		duk_put_prop_string(jshook->ctx, -2, "$sendtype");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_LOCAL_TOPIC
 * Called before a topic change
 */
const char *js_hook_pre_local_topic(Client *client, Channel *channel, const char *topic)
{
	JSHook *jshook;
	static char new_topic[512];
	const char *result = topic;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_LOCAL_TOPIC)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, topic ? topic : "");
		duk_put_prop_string(jshook->ctx, -2, "$topic");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0)
			{
				if (duk_is_null(jshook->ctx, -1))
					result = NULL;
				else if (duk_is_string(jshook->ctx, -1))
				{
					strlcpy(new_topic, duk_get_string(jshook->ctx, -1), sizeof(new_topic));
					result = new_topic;
				}
			}
		}
		duk_pop(jshook->ctx);
	}
	return result;
}

/*
 * Hook: HOOKTYPE_CAN_SET_TOPIC
 * Check if a user can set the topic
 */
int js_hook_can_set_topic(Client *client, Channel *channel, const char *topic, const char **errmsg)
{
	JSHook *jshook;
	int retval = EX_ALLOW;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_CAN_SET_TOPIC)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, topic ? topic : "");
		duk_put_prop_string(jshook->ctx, -2, "$topic");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_LOCAL_CHANMODE
 * Called before a local user changes channel modes
 */
int js_hook_pre_local_chanmode(Client *client, Channel *channel, MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_LOCAL_CHANMODE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, modebuf ? modebuf : "");
		duk_put_prop_string(jshook->ctx, -2, "$modebuf");
		duk_push_string(jshook->ctx, parabuf ? parabuf : "");
		duk_put_prop_string(jshook->ctx, -2, "$parabuf");
		duk_push_int(jshook->ctx, samode);
		duk_put_prop_string(jshook->ctx, -2, "$samode");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_REMOTE_CHANMODE
 * Called before a remote user changes channel modes
 */
int js_hook_pre_remote_chanmode(Client *client, Channel *channel, MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_REMOTE_CHANMODE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, modebuf ? modebuf : "");
		duk_put_prop_string(jshook->ctx, -2, "$modebuf");
		duk_push_string(jshook->ctx, parabuf ? parabuf : "");
		duk_put_prop_string(jshook->ctx, -2, "$parabuf");
		duk_push_int(jshook->ctx, samode);
		duk_put_prop_string(jshook->ctx, -2, "$samode");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_LOCAL_CHANMODE
 * Called when a local user changes channel modes
 */
int js_hook_local_chanmode(Client *client, Channel *channel, MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode, int *destroy_channel)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_LOCAL_CHANMODE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, modebuf ? modebuf : "");
		duk_put_prop_string(jshook->ctx, -2, "$modes");
		duk_push_string(jshook->ctx, parabuf ? parabuf : "");
		duk_put_prop_string(jshook->ctx, -2, "$params");
		duk_push_boolean(jshook->ctx, samode);
		duk_put_prop_string(jshook->ctx, -2, "$samode");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_REMOTE_CHANMODE
 * Called when a remote user changes channel modes
 */
int js_hook_remote_chanmode(Client *client, Channel *channel, MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode, int *destroy_channel)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_REMOTE_CHANMODE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, modebuf ? modebuf : "");
		duk_put_prop_string(jshook->ctx, -2, "$modes");
		duk_push_string(jshook->ctx, parabuf ? parabuf : "");
		duk_put_prop_string(jshook->ctx, -2, "$params");
		duk_push_boolean(jshook->ctx, samode);
		duk_put_prop_string(jshook->ctx, -2, "$samode");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_INVITE
 * Called before an invite
 */
int js_hook_pre_invite(Client *client, Client *acptr, Channel *channel, int *override)
{
	JSHook *jshook;
	int retval = HOOK_CONTINUE;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_INVITE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, acptr);
		duk_put_prop_string(jshook->ctx, -2, "$target");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_INVITE
 * Called when someone invites another user
 */
int js_hook_invite(Client *client, Client *acptr, Channel *channel, MessageTag *mtags)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_INVITE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, acptr);
		duk_put_prop_string(jshook->ctx, -2, "$target");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_KNOCK
 * Called before a knock
 */
int js_hook_pre_knock(Client *client, Channel *channel, const char **reason)
{
	JSHook *jshook;
	int retval = HOOK_CONTINUE;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_KNOCK)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, reason && *reason ? *reason : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_KNOCK
 * Called when someone knocks on a channel
 */
int js_hook_knock(Client *client, Channel *channel, MessageTag *mtags, const char *comment)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_KNOCK)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_channel_object(jshook->ctx, channel);
		duk_put_prop_string(jshook->ctx, -2, "$channel");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_WHOIS
 * Called when someone does a WHOIS
 */
int js_hook_whois(Client *client, Client *target, NameValuePrioList **list)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_WHOIS)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, target);
		duk_put_prop_string(jshook->ctx, -2, "$target");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_WHO_STATUS
 * Called to add WHO status characters
 */
int js_hook_who_status(Client *client, Client *target, Channel *channel, Member *member, const char *status, int cansee)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_WHO_STATUS)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, target);
		duk_put_prop_string(jshook->ctx, -2, "$target");
		if (channel)
		{
			js_push_channel_object(jshook->ctx, channel);
			duk_put_prop_string(jshook->ctx, -2, "$channel");
		}
		duk_push_string(jshook->ctx, status ? status : "");
		duk_put_prop_string(jshook->ctx, -2, "$status");
		duk_push_boolean(jshook->ctx, cansee);
		duk_put_prop_string(jshook->ctx, -2, "$cansee");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_KILL
 * Called before a KILL
 */
int js_hook_pre_kill(Client *client, Client *victim, const char *reason)
{
	JSHook *jshook;
	int retval = EX_ALLOW;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_KILL)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, victim);
		duk_put_prop_string(jshook->ctx, -2, "$victim");
		duk_push_string(jshook->ctx, reason ? reason : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_LOCAL_KILL
 * Called when someone is killed
 */
int js_hook_local_kill(Client *client, Client *victim, const char *comment)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_LOCAL_KILL)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		js_push_client_object(jshook->ctx, victim);
		duk_put_prop_string(jshook->ctx, -2, "$victim");
		duk_push_string(jshook->ctx, comment ? comment : "");
		duk_put_prop_string(jshook->ctx, -2, "$reason");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_REHASH
 * Called when the server is rehashing
 */
int js_hook_rehash(void)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_REHASH)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_REHASH_COMPLETE
 * Called after rehash completes
 */
int js_hook_rehash_complete(void)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_REHASH_COMPLETE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_STATS
 * Called when someone does /STATS
 */
int js_hook_stats(Client *client, const char *str)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_STATS)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, str ? str : "");
		duk_put_prop_string(jshook->ctx, -2, "$flag");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_LOCAL_PASS
 * Called when a user sends PASS command
 */
int js_hook_local_pass(Client *client, const char *password)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_LOCAL_PASS)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, password ? password : "");
		duk_put_prop_string(jshook->ctx, -2, "$password");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_UMODE_CHANGE
 * Called when user modes change
 */
int js_hook_umode_change(Client *client, long setflags, long newflags)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_UMODE_CHANGE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_number(jshook->ctx, (double)setflags);
		duk_put_prop_string(jshook->ctx, -2, "$oldflags");
		duk_push_number(jshook->ctx, (double)newflags);
		duk_put_prop_string(jshook->ctx, -2, "$newflags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_TKL_ADD
 * Called when a TKL (ban) is added
 */
int js_hook_tkl_add(Client *client, TKL *tkl)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_TKL_ADD)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		/* Push basic TKL info */
		duk_push_object(jshook->ctx);
		duk_push_int(jshook->ctx, tkl->type);
		duk_put_prop_string(jshook->ctx, -2, "type");
		if (TKLIsServerBan(tkl))
		{
			duk_push_string(jshook->ctx, tkl->ptr.serverban->usermask ? tkl->ptr.serverban->usermask : "");
			duk_put_prop_string(jshook->ctx, -2, "usermask");
			duk_push_string(jshook->ctx, tkl->ptr.serverban->hostmask ? tkl->ptr.serverban->hostmask : "");
			duk_put_prop_string(jshook->ctx, -2, "hostmask");
			duk_push_string(jshook->ctx, tkl->ptr.serverban->reason ? tkl->ptr.serverban->reason : "");
			duk_put_prop_string(jshook->ctx, -2, "reason");
		}
		duk_put_prop_string(jshook->ctx, -2, "$tkl");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_TKL_DEL
 * Called when a TKL (ban) is removed
 */
int js_hook_tkl_del(Client *client, TKL *tkl)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_TKL_DEL)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		/* Push basic TKL info */
		duk_push_object(jshook->ctx);
		duk_push_int(jshook->ctx, tkl->type);
		duk_put_prop_string(jshook->ctx, -2, "type");
		if (TKLIsServerBan(tkl))
		{
			duk_push_string(jshook->ctx, tkl->ptr.serverban->usermask ? tkl->ptr.serverban->usermask : "");
			duk_put_prop_string(jshook->ctx, -2, "usermask");
			duk_push_string(jshook->ctx, tkl->ptr.serverban->hostmask ? tkl->ptr.serverban->hostmask : "");
			duk_put_prop_string(jshook->ctx, -2, "hostmask");
		}
		duk_put_prop_string(jshook->ctx, -2, "$tkl");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_SECURE_CONNECT
 * Called when user gets +z mode
 */
int js_hook_secure_connect(Client *client)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_SECURE_CONNECT)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_WELCOME
 * Called for welcome numerics
 */
int js_hook_welcome(Client *client, int after_numeric)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_WELCOME)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_int(jshook->ctx, after_numeric);
		duk_put_prop_string(jshook->ctx, -2, "$afterNumeric");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_PRE_COMMAND
 * Called before a command is processed
 */
int js_hook_pre_command(Client *from, MessageTag *mtags, const char *buf)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_PRE_COMMAND)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, from);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, buf ? buf : "");
		duk_put_prop_string(jshook->ctx, -2, "$command");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_POST_COMMAND
 * Called after a command is processed
 */
int js_hook_post_command(Client *from, MessageTag *mtags, const char *buf)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_POST_COMMAND)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, from);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, buf ? buf : "");
		duk_put_prop_string(jshook->ctx, -2, "$command");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_ACCOUNT_LOGIN
 * Called when a user logs in/out of a services account
 */
int js_hook_account_login(Client *client, MessageTag *mtags)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_ACCOUNT_LOGIN)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, client->user && *client->user->account ? client->user->account : "0");
		duk_put_prop_string(jshook->ctx, -2, "$account");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_POST_LOCAL_NICKCHANGE
 * Called after a local nick change
 */
int js_hook_post_local_nickchange(Client *client, MessageTag *mtags, const char *oldnick)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_POST_LOCAL_NICKCHANGE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, oldnick ? oldnick : "");
		duk_put_prop_string(jshook->ctx, -2, "$oldnick");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_POST_REMOTE_NICKCHANGE
 * Called after a remote nick change
 */
int js_hook_post_remote_nickchange(Client *client, MessageTag *mtags, const char *oldnick)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_POST_REMOTE_NICKCHANGE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, oldnick ? oldnick : "");
		duk_put_prop_string(jshook->ctx, -2, "$oldnick");
		js_push_mtags_object(jshook->ctx, mtags);
		duk_put_prop_string(jshook->ctx, -2, "$mtags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_USERHOST_CHANGE
 * Called when user@host changes
 */
int js_hook_userhost_change(Client *client, const char *olduser, const char *oldhost)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_USERHOST_CHANGE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, olduser ? olduser : "");
		duk_put_prop_string(jshook->ctx, -2, "$olduser");
		duk_push_string(jshook->ctx, oldhost ? oldhost : "");
		duk_put_prop_string(jshook->ctx, -2, "$oldhost");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_REALNAME_CHANGE
 * Called when realname changes
 */
int js_hook_realname_change(Client *client, const char *oldinfo)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_REALNAME_CHANGE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, oldinfo ? oldinfo : "");
		duk_put_prop_string(jshook->ctx, -2, "$oldinfo");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_IP_CHANGE
 * Called when IP changes
 */
int js_hook_ip_change(Client *client, const char *oldip)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_IP_CHANGE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, oldip ? oldip : "");
		duk_put_prop_string(jshook->ctx, -2, "$oldip");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_HANDSHAKE
 * Called early when a client connects
 */
int js_hook_handshake(Client *client)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_HANDSHAKE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_FREE_CLIENT
 * Called when a client structure is freed
 */
int js_hook_free_client(Client *client)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_FREE_CLIENT)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_WATCH_ADD
 * Called when a WATCH entry is added
 */
int js_hook_watch_add(char *nick, Client *client, int flags)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_WATCH_ADD)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, nick ? nick : "");
		duk_put_prop_string(jshook->ctx, -2, "$nick");
		duk_push_int(jshook->ctx, flags);
		duk_put_prop_string(jshook->ctx, -2, "$flags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * Hook: HOOKTYPE_WATCH_DEL
 * Called when a WATCH entry is removed
 */
int js_hook_watch_del(char *nick, Client *client, int flags)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_WATCH_DEL)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, nick ? nick : "");
		duk_put_prop_string(jshook->ctx, -2, "$nick");
		duk_push_int(jshook->ctx, flags);
		duk_put_prop_string(jshook->ctx, -2, "$flags");
		duk_pop(jshook->ctx);

		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * SASL Authentication hook handler
 * Called when a SASL AUTHENTICATE message is received
 * 
 * Parameters:
 *   $client - The client attempting authentication
 *   $sasl_first - Boolean: true if this is the first AUTHENTICATE, false if continuation
 *   $sasl_data - The AUTHENTICATE parameter (base64 encoded for PLAIN mechanism)
 */
int js_hook_sasl_authenticate(Client *client, int first, const char *param)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_SASL_AUTHENTICATE)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		/* Set up context with SASL-specific variables */
		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_boolean(jshook->ctx, first ? 1 : 0);
		duk_put_prop_string(jshook->ctx, -2, "$sasl_first");
		duk_push_string(jshook->ctx, param ? param : "");
		duk_put_prop_string(jshook->ctx, -2, "$sasl_data");
		duk_pop(jshook->ctx);

		/* Get handler from stash and execute */
		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * SASL Continuation hook handler
 * Called when a SASL continuation response is received
 * 
 * Parameters:
 *   $client - The client for which SASL authentication is taking place
 *   $sasl_data - The AUTHENTICATE buffer
 */
int js_hook_sasl_continuation(Client *client, const char *buf)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_SASL_CONTINUATION)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		/* Set up context */
		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_string(jshook->ctx, buf ? buf : "");
		duk_put_prop_string(jshook->ctx, -2, "$sasl_data");
		duk_pop(jshook->ctx);

		/* Get handler from stash and execute */
		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * SASL Result hook handler
 * Called when a SASL result response is received
 * 
 * Parameters:
 *   $client - The client for which SASL authentication is taking place
 *   $sasl_success - Boolean: true if authentication was successful
 */
int js_hook_sasl_result(Client *client, int success)
{
	JSHook *jshook;
	int retval = 0;

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_SASL_RESULT)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		/* Set up context */
		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_push_boolean(jshook->ctx, success ? 1 : 0);
		duk_put_prop_string(jshook->ctx, -2, "$sasl_success");
		duk_pop(jshook->ctx);

		/* Get handler from stash and execute */
		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_number(jshook->ctx, -1))
				retval = duk_get_int(jshook->ctx, -1);
		}
		duk_pop(jshook->ctx);
	}
	return retval;
}

/*
 * SASL Mechanisms hook handler
 * Called to query which SASL mechanisms are supported
 * 
 * Parameters:
 *   $client - The client requesting SASL mechanisms
 * 
 * Returns:
 *   A string containing space-separated SASL mechanism names (e.g., "PLAIN")
 */
const char *js_hook_sasl_mechs(Client *client)
{
	JSHook *jshook;
	static char mechs[512];
	const char *result = NULL;

	mechs[0] = '\0';

	unreal_log(ULOG_DEBUG, "obbyscript", "JS_SASL_MECHS", NULL,
	           "js_hook_sasl_mechs called for client $client",
	           log_data_string("client", client ? client->name : "NULL"));

	for (jshook = js_hooks; jshook; jshook = jshook->next)
	{
		if (jshook->hooktype != HOOKTYPE_SASL_MECHS)
			continue;
		if (!jshook->ctx || !jshook->handler_code)
			continue;

		unreal_log(ULOG_DEBUG, "obbyscript", "JS_SASL_MECHS_FOUND", NULL,
		           "Found SASL_MECHS hook, executing...");

		/* Set up context */
		duk_push_global_object(jshook->ctx);
		js_push_client_object(jshook->ctx, client);
		duk_put_prop_string(jshook->ctx, -2, "$client");
		duk_pop(jshook->ctx);

		/* Get handler from stash and execute */
		duk_push_heap_stash(jshook->ctx);
		duk_get_prop_string(jshook->ctx, -1, jshook->handler_code);
		duk_remove(jshook->ctx, -2);

		if (duk_is_function(jshook->ctx, -1))
		{
			if (duk_pcall(jshook->ctx, 0) == 0 && duk_is_string(jshook->ctx, -1))
			{
				const char *new_mechs = duk_get_string(jshook->ctx, -1);
				if (new_mechs && *new_mechs)
				{
					unreal_log(ULOG_DEBUG, "obbyscript", "JS_SASL_MECHS_RESULT", NULL,
					           "SASL_MECHS hook returned: $mechs",
					           log_data_string("mechs", new_mechs));
					if (mechs[0])
						strlcat(mechs, " ", sizeof(mechs));
					strlcat(mechs, new_mechs, sizeof(mechs));
				}
			}
			else
			{
				unreal_log(ULOG_DEBUG, "obbyscript", "JS_SASL_MECHS_ERROR", NULL,
				           "SASL_MECHS hook failed or returned non-string");
			}
		}
		duk_pop(jshook->ctx);
	}

	unreal_log(ULOG_DEBUG, "obbyscript", "JS_SASL_MECHS_FINAL", NULL,
	           "Final SASL mechs: $mechs",
	           log_data_string("mechs", mechs[0] ? mechs : "(null)"));

	return mechs[0] ? mechs : NULL;
}

/*
 * Timer event handler - called by UnrealIRCd's event system
 */
EVENT(js_timer_event)
{
	JSTimer *timer = (JSTimer *)data;
	
	if (!timer || !timer->ctx || !timer->handler_code)
		return;

	/* Check if timer was cancelled */
	if (timer->cancelled)
	{
		/* Clean up the cancelled timer */
		safe_free(timer->handler_code);
		DelListItem(timer, js_timers);
		safe_free(timer);
		return;
	}

	/* Get handler from stash and execute */
	duk_push_heap_stash(timer->ctx);
	duk_get_prop_string(timer->ctx, -1, timer->handler_code);
	duk_remove(timer->ctx, -2);

	if (!duk_is_function(timer->ctx, -1))
	{
		unreal_log(ULOG_ERROR, "obbyscript", "JS_TIMER_ERROR", NULL,
		           "Timer handler function not found in stash");
		duk_pop(timer->ctx);
		return;
	}

	/* Call the handler */
	if (duk_pcall(timer->ctx, 0) != 0)
	{
		const char *error = duk_safe_to_string(timer->ctx, -1);
		unreal_log(ULOG_ERROR, "obbyscript", "JS_TIMER_ERROR", NULL,
		           "Error executing JavaScript timer handler: $error",
		           log_data_string("error", error));
	}
	duk_pop(timer->ctx);

	/* For setTimeout (count=1), clean up after execution */
	if (!timer->is_interval && timer->count == 1)
	{
		/* Event system will auto-remove event with count=1 */
		/* Clean up our timer tracking structure */
		safe_free(timer->handler_code);
		DelListItem(timer, js_timers);
		safe_free(timer);
	}
}

/*
 * API: setInterval(callback, milliseconds)
 * Call a function repeatedly at specified intervals
 */
duk_ret_t js_api_setInterval(duk_context *ctx)
{
	JSTimer *timer;
	int interval_ms;
	char stash_key[128];
	char event_name[128];

	if (!duk_is_function(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "First argument must be a function");
		return 0;
	}

	if (!duk_is_number(ctx, 1))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Second argument must be a number (milliseconds)");
		return 0;
	}

	interval_ms = duk_get_int(ctx, 1);
	if (interval_ms < 10)
		interval_ms = 10; /* Minimum 10ms to prevent abuse */

	/* Create timer structure */
	timer = safe_alloc(sizeof(JSTimer));
	timer->id = js_timer_id_counter++;
	timer->ctx = global_ctx;
	timer->is_interval = 1;
	timer->count = 0; /* 0 = infinite */

	/* Store the handler function in stash */
	snprintf(stash_key, sizeof(stash_key), "timer_handler_%d", timer->id);
	duk_push_heap_stash(ctx);
	duk_dup(ctx, 0); /* Copy the function */
	duk_put_prop_string(ctx, -2, stash_key);
	duk_pop(ctx);

	safe_strdup(timer->handler_code, stash_key);

	/* Register with UnrealIRCd event system */
	snprintf(event_name, sizeof(event_name), "js_interval_%d", timer->id);
	timer->event = EventAdd(js_modinfo->handle, event_name, js_timer_event, timer, interval_ms, 0);

	/* Add to linked list */
	AddListItem(timer, js_timers);

	unreal_log(ULOG_DEBUG, "obbyscript", "JS_TIMER_CREATED", NULL,
	           "JavaScript interval created: id=$id, interval=$interval ms",
	           log_data_integer("id", timer->id),
	           log_data_integer("interval", interval_ms));

	duk_push_int(ctx, timer->id);
	return 1;
}

/*
 * API: setTimeout(callback, milliseconds)
 * Call a function once after specified delay
 */
duk_ret_t js_api_setTimeout(duk_context *ctx)
{
	JSTimer *timer;
	int delay_ms;
	char stash_key[128];
	char event_name[128];

	if (!duk_is_function(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "First argument must be a function");
		return 0;
	}

	if (!duk_is_number(ctx, 1))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Second argument must be a number (milliseconds)");
		return 0;
	}

	delay_ms = duk_get_int(ctx, 1);
	if (delay_ms < 10)
		delay_ms = 10; /* Minimum 10ms */

	/* Create timer structure */
	timer = safe_alloc(sizeof(JSTimer));
	timer->id = js_timer_id_counter++;
	timer->ctx = global_ctx;
	timer->is_interval = 0;
	timer->count = 1; /* Run once */

	/* Store the handler function in stash */
	snprintf(stash_key, sizeof(stash_key), "timer_handler_%d", timer->id);
	duk_push_heap_stash(ctx);
	duk_dup(ctx, 0); /* Copy the function */
	duk_put_prop_string(ctx, -2, stash_key);
	duk_pop(ctx);

	safe_strdup(timer->handler_code, stash_key);

	/* Register with UnrealIRCd event system - count=1 means run once */
	snprintf(event_name, sizeof(event_name), "js_timeout_%d", timer->id);
	timer->event = EventAdd(js_modinfo->handle, event_name, js_timer_event, timer, delay_ms, 1);

	/* Add to linked list */
	AddListItem(timer, js_timers);

	unreal_log(ULOG_DEBUG, "obbyscript", "JS_TIMER_CREATED", NULL,
	           "JavaScript timeout created: id=$id, delay=$delay ms",
	           log_data_integer("id", timer->id),
	           log_data_integer("delay", delay_ms));

	duk_push_int(ctx, timer->id);
	return 1;
}

/*
 * API: clearInterval(id) / clearTimeout(id)
 * Cancel a timer by ID
 */
duk_ret_t js_api_clearInterval(duk_context *ctx)
{
	JSTimer *timer;
	int timer_id;
	char stash_key[128];

	if (!duk_is_number(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Argument must be a timer ID (number)");
		return 0;
	}

	timer_id = duk_get_int(ctx, 0);

	/* Find and mark the timer as cancelled */
	for (timer = js_timers; timer; timer = timer->next)
	{
		if (timer->id == timer_id)
		{
			/* Remove from stash */
			snprintf(stash_key, sizeof(stash_key), "timer_handler_%d", timer->id);
			duk_push_heap_stash(ctx);
			duk_del_prop_string(ctx, -1, stash_key);
			duk_pop(ctx);

			/* Mark as cancelled - the event handler will clean up */
			timer->cancelled = 1;

			/* Remove the event to prevent further callbacks */
			if (timer->event)
			{
				EventDel(timer->event);
				timer->event = NULL;
			}

			unreal_log(ULOG_DEBUG, "obbyscript", "JS_TIMER_CLEARED", NULL,
			           "JavaScript timer cleared: id=$id",
			           log_data_integer("id", timer_id));

			/* Clean up now since event is deleted */
			safe_free(timer->handler_code);
			DelListItem(timer, js_timers);
			safe_free(timer);

			duk_push_boolean(ctx, 1);
			return 1;
		}
	}

	duk_push_boolean(ctx, 0);
	return 1;
}

/* clearTimeout is the same as clearInterval */
duk_ret_t js_api_clearTimeout(duk_context *ctx)
{
	return js_api_clearInterval(ctx);
}

/*
 * API: registerHook(config)
 * Register a hook handler from JavaScript
 * config = { type: HOOKTYPE_*, handler: function() { ... } }
 */
duk_ret_t js_api_registerHook(duk_context *ctx)
{
	JSHook *jshook;
	int hooktype;

	if (!duk_is_object(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Argument must be an object");
		return 0;
	}

	/* Get hook type */
	duk_get_prop_string(ctx, 0, "type");
	if (!duk_is_number(ctx, -1))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Hook type must be a number (use HOOKTYPE_* constants)");
		return 0;
	}
	hooktype = duk_get_int(ctx, -1);
	duk_pop(ctx);

	/* Get handler function */
	duk_get_prop_string(ctx, 0, "handler");
	if (!duk_is_function(ctx, -1))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Handler must be a function");
		return 0;
	}

	/* Create JSHook structure */
	jshook = safe_alloc(sizeof(JSHook));
	jshook->hooktype = hooktype;
	jshook->ctx = global_ctx; /* Use global context */

	/* Store the handler function in stash with unique key */
	char stash_key[128];
	snprintf(stash_key, sizeof(stash_key), "hook_handler_%d_%p", hooktype, (void*)jshook);
	
	/* Stack: [config_obj, handler_func] */
	duk_push_heap_stash(ctx);          /* Stack: [config_obj, handler_func, stash] */
	duk_dup(ctx, -2);                  /* Stack: [config_obj, handler_func, stash, handler_func_copy] */
	duk_put_prop_string(ctx, -2, stash_key); /* Stack: [config_obj, handler_func, stash] */
	duk_pop(ctx);                      /* Stack: [config_obj, handler_func] */

	safe_strdup(jshook->handler_code, stash_key); /* Store stash key */

	/* Register with UnrealIRCd based on hook type - only if not already registered */
	if (!js_registered_hooks[hooktype])
	{
		js_registered_hooks[hooktype] = 1;
		
		switch (hooktype)
		{
			case HOOKTYPE_LOCAL_CONNECT:
				HookAdd(js_modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, js_hook_local_connect);
				break;
			case HOOKTYPE_REMOTE_CONNECT:
				HookAdd(js_modinfo->handle, HOOKTYPE_REMOTE_CONNECT, 0, js_hook_remote_connect);
				break;
			case HOOKTYPE_LOCAL_QUIT:
				HookAdd(js_modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, js_hook_local_quit);
				break;
			case HOOKTYPE_REMOTE_QUIT:
				HookAdd(js_modinfo->handle, HOOKTYPE_REMOTE_QUIT, 0, js_hook_remote_quit);
				break;
			case HOOKTYPE_LOCAL_JOIN:
				HookAdd(js_modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, js_hook_local_join);
				break;
			case HOOKTYPE_REMOTE_JOIN:
				HookAdd(js_modinfo->handle, HOOKTYPE_REMOTE_JOIN, 0, js_hook_remote_join);
				break;
			case HOOKTYPE_LOCAL_PART:
				HookAdd(js_modinfo->handle, HOOKTYPE_LOCAL_PART, 0, js_hook_local_part);
				break;
			case HOOKTYPE_REMOTE_PART:
				HookAdd(js_modinfo->handle, HOOKTYPE_REMOTE_PART, 0, js_hook_remote_part);
				break;
			case HOOKTYPE_LOCAL_KICK:
				HookAdd(js_modinfo->handle, HOOKTYPE_LOCAL_KICK, 0, js_hook_local_kick);
				break;
			case HOOKTYPE_REMOTE_KICK:
				HookAdd(js_modinfo->handle, HOOKTYPE_REMOTE_KICK, 0, js_hook_remote_kick);
				break;
			case HOOKTYPE_CHANMSG:
				HookAdd(js_modinfo->handle, HOOKTYPE_CHANMSG, 0, js_hook_chanmsg);
				break;
			case HOOKTYPE_USERMSG:
				HookAdd(js_modinfo->handle, HOOKTYPE_USERMSG, 0, js_hook_usermsg);
				break;
			case HOOKTYPE_LOCAL_NICKCHANGE:
				HookAdd(js_modinfo->handle, HOOKTYPE_LOCAL_NICKCHANGE, 0, js_hook_local_nickchange);
				break;
			case HOOKTYPE_REMOTE_NICKCHANGE:
				HookAdd(js_modinfo->handle, HOOKTYPE_REMOTE_NICKCHANGE, 0, js_hook_remote_nickchange);
				break;
			case HOOKTYPE_TOPIC:
				HookAdd(js_modinfo->handle, HOOKTYPE_TOPIC, 0, js_hook_topic);
				break;
			case HOOKTYPE_AWAY:
				HookAdd(js_modinfo->handle, HOOKTYPE_AWAY, 0, js_hook_away);
				break;
			case HOOKTYPE_LOCAL_OPER:
				HookAdd(js_modinfo->handle, HOOKTYPE_LOCAL_OPER, 0, js_hook_local_oper);
				break;
			case HOOKTYPE_CHANNEL_CREATE:
				HookAdd(js_modinfo->handle, HOOKTYPE_CHANNEL_CREATE, 0, js_hook_channel_create);
				break;
			case HOOKTYPE_CHANNEL_DESTROY:
				HookAdd(js_modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, 0, js_hook_channel_destroy);
				break;
			case HOOKTYPE_CAN_USE_NICK:
				HookAdd(js_modinfo->handle, HOOKTYPE_CAN_USE_NICK, 0, js_hook_can_use_nick);
				break;
			/* Additional hooks - comprehensive implementation */
			case HOOKTYPE_PRE_LOCAL_CONNECT:
				HookAdd(js_modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0, js_hook_pre_local_connect);
				break;
			case HOOKTYPE_PRE_LOCAL_QUIT:
				HookAddConstString(js_modinfo->handle, HOOKTYPE_PRE_LOCAL_QUIT, 0, js_hook_pre_local_quit);
				break;
			case HOOKTYPE_UNKUSER_QUIT:
				HookAdd(js_modinfo->handle, HOOKTYPE_UNKUSER_QUIT, 0, js_hook_unkuser_quit);
				break;
			case HOOKTYPE_SERVER_CONNECT:
				HookAdd(js_modinfo->handle, HOOKTYPE_SERVER_CONNECT, 0, js_hook_server_connect);
				break;
			case HOOKTYPE_SERVER_QUIT:
				HookAdd(js_modinfo->handle, HOOKTYPE_SERVER_QUIT, 0, js_hook_server_quit);
				break;
			case HOOKTYPE_CAN_JOIN:
				HookAdd(js_modinfo->handle, HOOKTYPE_CAN_JOIN, 0, js_hook_can_join);
				break;
			case HOOKTYPE_PRE_LOCAL_JOIN:
				HookAdd(js_modinfo->handle, HOOKTYPE_PRE_LOCAL_JOIN, 0, js_hook_pre_local_join);
				break;
			case HOOKTYPE_PRE_LOCAL_PART:
				HookAddConstString(js_modinfo->handle, HOOKTYPE_PRE_LOCAL_PART, 0, js_hook_pre_local_part);
				break;
			case HOOKTYPE_PRE_LOCAL_KICK:
				HookAddConstString(js_modinfo->handle, HOOKTYPE_PRE_LOCAL_KICK, 0, js_hook_pre_local_kick);
				break;
			case HOOKTYPE_CAN_KICK:
				HookAdd(js_modinfo->handle, HOOKTYPE_CAN_KICK, 0, js_hook_can_kick);
				break;
			case HOOKTYPE_PRE_CHANMSG:
				HookAdd(js_modinfo->handle, HOOKTYPE_PRE_CHANMSG, 0, js_hook_pre_chanmsg);
				break;
			case HOOKTYPE_CAN_SEND_TO_CHANNEL:
				HookAdd(js_modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, js_hook_can_send_to_channel);
				break;
			case HOOKTYPE_CAN_SEND_TO_USER:
				HookAdd(js_modinfo->handle, HOOKTYPE_CAN_SEND_TO_USER, 0, js_hook_can_send_to_user);
				break;
			case HOOKTYPE_PRE_LOCAL_TOPIC:
				HookAddConstString(js_modinfo->handle, HOOKTYPE_PRE_LOCAL_TOPIC, 0, js_hook_pre_local_topic);
				break;
			case HOOKTYPE_CAN_SET_TOPIC:
				HookAdd(js_modinfo->handle, HOOKTYPE_CAN_SET_TOPIC, 0, js_hook_can_set_topic);
				break;
			case HOOKTYPE_PRE_LOCAL_CHANMODE:
				HookAdd(js_modinfo->handle, HOOKTYPE_PRE_LOCAL_CHANMODE, 0, js_hook_pre_local_chanmode);
				break;
			case HOOKTYPE_PRE_REMOTE_CHANMODE:
				HookAdd(js_modinfo->handle, HOOKTYPE_PRE_REMOTE_CHANMODE, 0, js_hook_pre_remote_chanmode);
				break;
			case HOOKTYPE_LOCAL_CHANMODE:
				HookAdd(js_modinfo->handle, HOOKTYPE_LOCAL_CHANMODE, 0, js_hook_local_chanmode);
				break;
			case HOOKTYPE_REMOTE_CHANMODE:
				HookAdd(js_modinfo->handle, HOOKTYPE_REMOTE_CHANMODE, 0, js_hook_remote_chanmode);
				break;
			case HOOKTYPE_PRE_INVITE:
				HookAdd(js_modinfo->handle, HOOKTYPE_PRE_INVITE, 0, js_hook_pre_invite);
				break;
			case HOOKTYPE_INVITE:
				HookAdd(js_modinfo->handle, HOOKTYPE_INVITE, 0, js_hook_invite);
				break;
			case HOOKTYPE_PRE_KNOCK:
				HookAdd(js_modinfo->handle, HOOKTYPE_PRE_KNOCK, 0, js_hook_pre_knock);
				break;
			case HOOKTYPE_KNOCK:
				HookAdd(js_modinfo->handle, HOOKTYPE_KNOCK, 0, js_hook_knock);
				break;
			case HOOKTYPE_WHOIS:
				HookAdd(js_modinfo->handle, HOOKTYPE_WHOIS, 0, js_hook_whois);
				break;
			case HOOKTYPE_WHO_STATUS:
				HookAdd(js_modinfo->handle, HOOKTYPE_WHO_STATUS, 0, js_hook_who_status);
				break;
			case HOOKTYPE_PRE_KILL:
				HookAdd(js_modinfo->handle, HOOKTYPE_PRE_KILL, 0, js_hook_pre_kill);
				break;
			case HOOKTYPE_LOCAL_KILL:
				HookAdd(js_modinfo->handle, HOOKTYPE_LOCAL_KILL, 0, js_hook_local_kill);
				break;
			case HOOKTYPE_REHASH:
				HookAdd(js_modinfo->handle, HOOKTYPE_REHASH, 0, js_hook_rehash);
				break;
			case HOOKTYPE_REHASH_COMPLETE:
				HookAdd(js_modinfo->handle, HOOKTYPE_REHASH_COMPLETE, 0, js_hook_rehash_complete);
				break;
			case HOOKTYPE_STATS:
				HookAdd(js_modinfo->handle, HOOKTYPE_STATS, 0, js_hook_stats);
				break;
			case HOOKTYPE_LOCAL_PASS:
				HookAdd(js_modinfo->handle, HOOKTYPE_LOCAL_PASS, 0, js_hook_local_pass);
				break;
			case HOOKTYPE_UMODE_CHANGE:
				HookAdd(js_modinfo->handle, HOOKTYPE_UMODE_CHANGE, 0, js_hook_umode_change);
				break;
			case HOOKTYPE_TKL_ADD:
				HookAdd(js_modinfo->handle, HOOKTYPE_TKL_ADD, 0, js_hook_tkl_add);
				break;
			case HOOKTYPE_TKL_DEL:
				HookAdd(js_modinfo->handle, HOOKTYPE_TKL_DEL, 0, js_hook_tkl_del);
				break;
			case HOOKTYPE_SECURE_CONNECT:
				HookAdd(js_modinfo->handle, HOOKTYPE_SECURE_CONNECT, 0, js_hook_secure_connect);
				break;
			case HOOKTYPE_WELCOME:
				HookAdd(js_modinfo->handle, HOOKTYPE_WELCOME, 0, js_hook_welcome);
				break;
			case HOOKTYPE_PRE_COMMAND:
				HookAdd(js_modinfo->handle, HOOKTYPE_PRE_COMMAND, 0, js_hook_pre_command);
				break;
			case HOOKTYPE_POST_COMMAND:
				HookAdd(js_modinfo->handle, HOOKTYPE_POST_COMMAND, 0, js_hook_post_command);
				break;
			case HOOKTYPE_ACCOUNT_LOGIN:
				HookAdd(js_modinfo->handle, HOOKTYPE_ACCOUNT_LOGIN, 0, js_hook_account_login);
				break;
			case HOOKTYPE_POST_LOCAL_NICKCHANGE:
				HookAdd(js_modinfo->handle, HOOKTYPE_POST_LOCAL_NICKCHANGE, 0, js_hook_post_local_nickchange);
				break;
			case HOOKTYPE_POST_REMOTE_NICKCHANGE:
				HookAdd(js_modinfo->handle, HOOKTYPE_POST_REMOTE_NICKCHANGE, 0, js_hook_post_remote_nickchange);
				break;
			case HOOKTYPE_USERHOST_CHANGE:
				HookAdd(js_modinfo->handle, HOOKTYPE_USERHOST_CHANGE, 0, js_hook_userhost_change);
				break;
			case HOOKTYPE_REALNAME_CHANGE:
				HookAdd(js_modinfo->handle, HOOKTYPE_REALNAME_CHANGE, 0, js_hook_realname_change);
				break;
			case HOOKTYPE_IP_CHANGE:
				HookAdd(js_modinfo->handle, HOOKTYPE_IP_CHANGE, 0, js_hook_ip_change);
				break;
			case HOOKTYPE_HANDSHAKE:
				HookAdd(js_modinfo->handle, HOOKTYPE_HANDSHAKE, 0, js_hook_handshake);
				break;
			case HOOKTYPE_FREE_CLIENT:
				HookAdd(js_modinfo->handle, HOOKTYPE_FREE_CLIENT, 0, js_hook_free_client);
				break;
			case HOOKTYPE_WATCH_ADD:
				HookAdd(js_modinfo->handle, HOOKTYPE_WATCH_ADD, 0, js_hook_watch_add);
				break;
			case HOOKTYPE_WATCH_DEL:
				HookAdd(js_modinfo->handle, HOOKTYPE_WATCH_DEL, 0, js_hook_watch_del);
				break;
			/* SASL hooks */
			case HOOKTYPE_SASL_AUTHENTICATE:
				HookAdd(js_modinfo->handle, HOOKTYPE_SASL_AUTHENTICATE, 0, js_hook_sasl_authenticate);
				break;
			case HOOKTYPE_SASL_CONTINUATION:
				HookAdd(js_modinfo->handle, HOOKTYPE_SASL_CONTINUATION, 0, js_hook_sasl_continuation);
				break;
			case HOOKTYPE_SASL_RESULT:
				HookAdd(js_modinfo->handle, HOOKTYPE_SASL_RESULT, 0, js_hook_sasl_result);
				break;
			case HOOKTYPE_SASL_MECHS:
				HookAddConstString(js_modinfo->handle, HOOKTYPE_SASL_MECHS, 0, js_hook_sasl_mechs);
				/* Set this server as the SASL server so SASL authentication is handled locally */
				if (!iConf.sasl_server || strcmp(iConf.sasl_server, me.name) != 0)
				{
					safe_strdup(iConf.sasl_server, me.name);
					unreal_log(ULOG_INFO, "obbyscript", "SASL_SERVER_SET", NULL,
					           "SASL server set to local server ($server) due to JavaScript SASL hook registration",
					           log_data_string("server", me.name));
				}
				break;
			default:
				unreal_log(ULOG_WARNING, "obbyscript", "JS_HOOK_UNSUPPORTED", NULL,
				           "Unsupported hook type: $hooktype",
				           log_data_integer("hooktype", hooktype));
				break;
		}
	}

	/* Add to linked list */
	AddListItem(jshook, js_hooks);

	duk_pop_2(ctx);

	unreal_log(ULOG_INFO, "obbyscript", "JS_HOOK_REGISTERED", NULL,
	           "JavaScript hook registered: hooktype=$hooktype",
	           log_data_integer("hooktype", hooktype));

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * Load all JavaScript files from the scripts directory
 */
void js_load_scripts(void)
{
	DIR *dir;
	struct dirent *entry;
	char filepath[PATH_MAX];
	char *script_content;
	FILE *fp;
	long file_size;

	snprintf(filepath, sizeof(filepath), "%s/%s", CONFDIR, SCRIPTS_DIR);
	
	dir = opendir(filepath);
	if (!dir)
	{
		unreal_log(ULOG_WARNING, "obbyscript", "JS_SCRIPTS_DIR_NOT_FOUND", NULL,
		           "Scripts directory not found: $path (this is normal if you haven't created it yet)",
		           log_data_string("path", filepath));
		return;
	}

	while ((entry = readdir(dir)) != NULL)
	{
		if (entry->d_name[0] == '.')
			continue;

		/* Only load .js files */
		int len = strlen(entry->d_name);
		if (len < 3 || strcmp(entry->d_name + len - 3, ".js") != 0)
			continue;

		snprintf(filepath, sizeof(filepath), "%s/%s/%s", CONFDIR, SCRIPTS_DIR, entry->d_name);

		fp = fopen(filepath, "rb");
		if (!fp)
		{
			unreal_log(ULOG_WARNING, "obbyscript", "JS_SCRIPT_OPEN_FAILED", NULL,
			           "Failed to open script: $file",
			           log_data_string("file", filepath));
			continue;
		}

		/* Get file size */
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		if (file_size <= 0 || file_size > 1024*1024) /* Max 1MB per script */
		{
			fclose(fp);
			unreal_log(ULOG_WARNING, "obbyscript", "JS_SCRIPT_INVALID_SIZE", NULL,
			           "Script file has invalid size: $file",
			           log_data_string("file", filepath));
			continue;
		}

		script_content = safe_alloc(file_size + 1);
		if (fread(script_content, 1, file_size, fp) != file_size)
		{
			fclose(fp);
			safe_free(script_content);
			unreal_log(ULOG_WARNING, "obbyscript", "JS_SCRIPT_READ_FAILED", NULL,
			           "Failed to read script: $file",
			           log_data_string("file", filepath));
			continue;
		}
		fclose(fp);
		script_content[file_size] = '\0';

		/* Execute the script */
		if (duk_peval_string(global_ctx, script_content) != 0)
		{
			const char *error = duk_safe_to_string(global_ctx, -1);
			unreal_log(ULOG_ERROR, "obbyscript", "JS_SCRIPT_LOAD_ERROR", NULL,
			           "Error loading script $file: $error",
			           log_data_string("file", entry->d_name),
			           log_data_string("error", error));
		}
		else
		{
			unreal_log(ULOG_INFO, "obbyscript", "JS_SCRIPT_LOADED", NULL,
			           "Loaded JavaScript script: $file",
			           log_data_string("file", entry->d_name));
		}
		duk_pop(global_ctx);

		safe_free(script_content);
	}

	closedir(dir);
}

/* ============================================================================
 * EXTENDED API v2.0 - Channel Modes, User Modes, ModData, and Utility Functions
 * ============================================================================ */

/*
 * ModData callbacks - used for all JavaScript-registered moddata
 */
void js_moddata_free(ModData *md)
{
	if (md && md->ptr)
	{
		safe_free(md->ptr);
		md->ptr = NULL;
	}
}

const char *js_moddata_serialize(ModData *md)
{
	if (md && md->ptr)
		return (const char *)md->ptr;
	return NULL;
}

void js_moddata_unserialize(const char *str, ModData *md)
{
	if (md)
	{
		safe_free(md->ptr);
		if (str)
			md->ptr = raw_strdup(str);
		else
			md->ptr = NULL;
	}
}

/*
 * Channel mode is_ok callback - used for JavaScript-registered channel modes
 */
int js_channelmode_is_ok(Client *client, Channel *channel, char mode, const char *para, int checkt, int what)
{
	/* Default: require chanop to set/unset */
	return extcmode_default_requirechop(client, channel, mode, para, checkt, what);
}

/*
 * Prefix mode is_ok callback - used for JavaScript-registered prefix modes  
 */
int js_prefixmode_is_ok(Client *client, Channel *channel, char mode, const char *para, int checkt, int what)
{
	/* Default: require chanop to set/unset */
	return extcmode_default_requirechop(client, channel, mode, para, checkt, what);
}

/*
 * User mode allowed callback - used for JavaScript-registered user modes
 */
int js_usermode_allowed(Client *client, int what)
{
	/* Default: allow all users to set/unset */
	return 1;
}

/*
 * API: registerChannelMode(config)
 * Register a new channel mode (paramless)
 * config = { letter: 'X', name: 'mymode' }
 */
duk_ret_t js_api_registerChannelMode(duk_context *ctx)
{
	JSChannelMode *jscm;
	CmodeInfo req;
	const char *letter_str;
	char letter;
	const char *name;

	if (!duk_is_object(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Argument must be an object");
		return 0;
	}

	/* Get mode letter */
	duk_get_prop_string(ctx, 0, "letter");
	letter_str = duk_get_string(ctx, -1);
	if (!letter_str || strlen(letter_str) != 1)
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "letter must be a single character");
		return 0;
	}
	letter = letter_str[0];
	duk_pop(ctx);

	/* Get mode name (optional) */
	duk_get_prop_string(ctx, 0, "name");
	name = duk_is_string(ctx, -1) ? duk_get_string(ctx, -1) : NULL;
	duk_pop(ctx);

	/* Create structure */
	jscm = safe_alloc(sizeof(JSChannelMode));
	jscm->letter = letter;
	if (name)
		safe_strdup(jscm->name, name);
	jscm->ctx = global_ctx;

	/* Register with UnrealIRCd */
	memset(&req, 0, sizeof(req));
	req.letter = letter;
	req.paracount = 0;
	req.is_ok = js_channelmode_is_ok;
	req.type = CMODE_NORMAL;

	jscm->cmode = CmodeAdd(js_modinfo->handle, req, &jscm->mode_bit);
	if (!jscm->cmode)
	{
		safe_free(jscm->name);
		safe_free(jscm);
		duk_error(ctx, DUK_ERR_ERROR, "Failed to register channel mode +%c", letter);
		return 0;
	}

	AddListItem(jscm, js_channelmodes);

	unreal_log(ULOG_INFO, "obbyscript", "JS_CHANNELMODE_REGISTERED", NULL,
	           "JavaScript channel mode registered: +$letter ($name)",
	           log_data_string("letter", letter_str),
	           log_data_string("name", name ? name : "(unnamed)"));

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: registerPrefixMode(config)
 * Register a new prefix mode (like +o, +v)
 * config = { letter: 'y', prefix: '!', rank: 150, name: 'founder' }
 */
duk_ret_t js_api_registerPrefixMode(duk_context *ctx)
{
	JSPrefixMode *jspm;
	CmodeInfo req;
	const char *letter_str, *prefix_str;
	char letter, prefix;
	int rank;
	const char *name;

	if (!duk_is_object(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Argument must be an object");
		return 0;
	}

	/* Get mode letter */
	duk_get_prop_string(ctx, 0, "letter");
	letter_str = duk_get_string(ctx, -1);
	if (!letter_str || strlen(letter_str) != 1)
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "letter must be a single character");
		return 0;
	}
	letter = letter_str[0];
	duk_pop(ctx);

	/* Get prefix character */
	duk_get_prop_string(ctx, 0, "prefix");
	prefix_str = duk_get_string(ctx, -1);
	if (!prefix_str || strlen(prefix_str) != 1)
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "prefix must be a single character");
		return 0;
	}
	prefix = prefix_str[0];
	duk_pop(ctx);

	/* Get rank */
	duk_get_prop_string(ctx, 0, "rank");
	if (!duk_is_number(ctx, -1))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "rank must be a number");
		return 0;
	}
	rank = duk_get_int(ctx, -1);
	duk_pop(ctx);

	/* Get mode name (optional) */
	duk_get_prop_string(ctx, 0, "name");
	name = duk_is_string(ctx, -1) ? duk_get_string(ctx, -1) : NULL;
	duk_pop(ctx);

	/* Create structure */
	jspm = safe_alloc(sizeof(JSPrefixMode));
	jspm->letter = letter;
	jspm->prefix = prefix;
	jspm->sjoin_prefix = prefix; /* Same as prefix by default */
	jspm->rank = rank;
	if (name)
		safe_strdup(jspm->name, name);
	jspm->ctx = global_ctx;

	/* Register with UnrealIRCd */
	memset(&req, 0, sizeof(req));
	req.letter = letter;
	req.prefix = prefix;
	req.sjoin_prefix = prefix;
	req.rank = rank;
	req.paracount = 1;
	req.unset_with_param = 1;
	req.is_ok = js_prefixmode_is_ok;
	req.type = CMODE_MEMBER;

	jspm->cmode = CmodeAdd(js_modinfo->handle, req, NULL);
	if (!jspm->cmode)
	{
		safe_free(jspm->name);
		safe_free(jspm);
		duk_error(ctx, DUK_ERR_ERROR, "Failed to register prefix mode +%c (%c)", letter, prefix);
		return 0;
	}

	AddListItem(jspm, js_prefixmodes);

	unreal_log(ULOG_INFO, "obbyscript", "JS_PREFIXMODE_REGISTERED", NULL,
	           "JavaScript prefix mode registered: +$letter (prefix: $prefix, rank: $rank)",
	           log_data_string("letter", letter_str),
	           log_data_string("prefix", prefix_str),
	           log_data_integer("rank", rank));

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: registerUserMode(config)
 * Register a new user mode
 * config = { letter: 'X', name: 'mymode', global: true, unsetOnDeoper: false }
 */
duk_ret_t js_api_registerUserMode(duk_context *ctx)
{
	JSUserMode *jsum;
	const char *letter_str;
	char letter;
	const char *name;
	int is_global = 1;
	int unset_on_deoper = 0;

	if (!duk_is_object(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Argument must be an object");
		return 0;
	}

	/* Get mode letter */
	duk_get_prop_string(ctx, 0, "letter");
	letter_str = duk_get_string(ctx, -1);
	if (!letter_str || strlen(letter_str) != 1)
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "letter must be a single character");
		return 0;
	}
	letter = letter_str[0];
	duk_pop(ctx);

	/* Get mode name (optional) */
	duk_get_prop_string(ctx, 0, "name");
	name = duk_is_string(ctx, -1) ? duk_get_string(ctx, -1) : NULL;
	duk_pop(ctx);

	/* Get global flag (optional, default true) */
	if (duk_get_prop_string(ctx, 0, "global"))
		is_global = duk_get_boolean(ctx, -1);
	duk_pop(ctx);

	/* Get unsetOnDeoper flag (optional, default false) */
	if (duk_get_prop_string(ctx, 0, "unsetOnDeoper"))
		unset_on_deoper = duk_get_boolean(ctx, -1);
	duk_pop(ctx);

	/* Create structure */
	jsum = safe_alloc(sizeof(JSUserMode));
	jsum->letter = letter;
	jsum->is_global = is_global;
	jsum->unset_on_deoper = unset_on_deoper;
	if (name)
		safe_strdup(jsum->name, name);
	jsum->ctx = global_ctx;

	/* Register with UnrealIRCd */
	jsum->umode = UmodeAdd(js_modinfo->handle, letter, 
	                       is_global ? UMODE_GLOBAL : UMODE_LOCAL,
	                       unset_on_deoper,
	                       js_usermode_allowed,
	                       &jsum->mode_bit);
	if (!jsum->umode)
	{
		safe_free(jsum->name);
		safe_free(jsum);
		duk_error(ctx, DUK_ERR_ERROR, "Failed to register user mode +%c", letter);
		return 0;
	}

	AddListItem(jsum, js_usermodes);

	unreal_log(ULOG_INFO, "obbyscript", "JS_USERMODE_REGISTERED", NULL,
	           "JavaScript user mode registered: +$letter ($name, global=$global)",
	           log_data_string("letter", letter_str),
	           log_data_string("name", name ? name : "(unnamed)"),
	           log_data_integer("global", is_global));

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: registerModData(config)
 * Register moddata storage
 * config = { name: 'mydata', type: MODDATATYPE_CLIENT, sync: true }
 */
duk_ret_t js_api_registerModData(duk_context *ctx)
{
	JSModData *jsmd;
	ModDataInfo mreq;
	const char *name;
	int type;
	int sync = 0;

	if (!duk_is_object(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "Argument must be an object");
		return 0;
	}

	/* Get name */
	duk_get_prop_string(ctx, 0, "name");
	name = duk_get_string(ctx, -1);
	if (!name || !*name)
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "name is required");
		return 0;
	}
	duk_pop(ctx);

	/* Get type */
	duk_get_prop_string(ctx, 0, "type");
	if (!duk_is_number(ctx, -1))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "type must be a MODDATATYPE_* constant");
		return 0;
	}
	type = duk_get_int(ctx, -1);
	duk_pop(ctx);

	/* Get sync flag (optional, default false) */
	if (duk_get_prop_string(ctx, 0, "sync"))
		sync = duk_get_boolean(ctx, -1);
	duk_pop(ctx);

	/* Create structure */
	jsmd = safe_alloc(sizeof(JSModData));
	safe_strdup(jsmd->name, name);
	jsmd->type = type;
	jsmd->sync = sync;

	/* Register with UnrealIRCd */
	memset(&mreq, 0, sizeof(mreq));
	mreq.name = jsmd->name;
	mreq.type = type;
	mreq.free = js_moddata_free;
	mreq.serialize = js_moddata_serialize;
	mreq.unserialize = js_moddata_unserialize;
	mreq.sync = sync ? MODDATA_SYNC_EARLY : 0;

	jsmd->md = ModDataAdd(js_modinfo->handle, mreq);
	if (!jsmd->md)
	{
		safe_free(jsmd->name);
		safe_free(jsmd);
		duk_error(ctx, DUK_ERR_ERROR, "Failed to register moddata '%s'", name);
		return 0;
	}

	AddListItem(jsmd, js_moddatas);

	unreal_log(ULOG_INFO, "obbyscript", "JS_MODDATA_REGISTERED", NULL,
	           "JavaScript moddata registered: $name (type=$type)",
	           log_data_string("name", name),
	           log_data_integer("type", type));

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: setModData(target, name, value)
 * Set moddata on a client or channel
 */
duk_ret_t js_api_setModData(duk_context *ctx)
{
	const char *name;
	const char *value;
	JSModData *jsmd;

	/* Get moddata name */
	name = duk_require_string(ctx, 1);

	/* Get value (can be null to unset) */
	if (duk_is_null(ctx, 2) || duk_is_undefined(ctx, 2))
		value = NULL;
	else
		value = duk_to_string(ctx, 2);

	/* Find registered moddata */
	for (jsmd = js_moddatas; jsmd; jsmd = jsmd->next)
	{
		if (!strcmp(jsmd->name, name))
			break;
	}

	if (!jsmd || !jsmd->md)
	{
		duk_error(ctx, DUK_ERR_ERROR, "ModData '%s' not found - did you registerModData() first?", name);
		return 0;
	}

	/* Handle based on target type */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *target_name = duk_get_string(ctx, -1);
		duk_pop(ctx);

		if (jsmd->type == MODDATATYPE_CLIENT || jsmd->type == MODDATATYPE_LOCAL_CLIENT)
		{
			Client *client = find_client(target_name, NULL);
			if (!client)
			{
				duk_push_boolean(ctx, 0);
				return 1;
			}
			moddata_client_set(client, name, value);
		}
		else if (jsmd->type == MODDATATYPE_CHANNEL)
		{
			Channel *channel = find_channel(target_name);
			if (!channel)
			{
				duk_push_boolean(ctx, 0);
				return 1;
			}
			/* For channel moddata, we need to set it directly */
			ModData *md = &moddata_channel(channel, jsmd->md);
			safe_free(md->ptr);
			if (value)
				md->ptr = raw_strdup(value);
			else
				md->ptr = NULL;
		}
	}
	else if (duk_is_string(ctx, 0))
	{
		const char *target_name = duk_get_string(ctx, 0);
		if (jsmd->type == MODDATATYPE_CLIENT || jsmd->type == MODDATATYPE_LOCAL_CLIENT)
		{
			Client *client = find_client(target_name, NULL);
			if (!client)
			{
				duk_push_boolean(ctx, 0);
				return 1;
			}
			moddata_client_set(client, name, value);
		}
		else if (jsmd->type == MODDATATYPE_CHANNEL)
		{
			Channel *channel = find_channel(target_name);
			if (!channel)
			{
				duk_push_boolean(ctx, 0);
				return 1;
			}
			ModData *md = &moddata_channel(channel, jsmd->md);
			safe_free(md->ptr);
			if (value)
				md->ptr = raw_strdup(value);
			else
				md->ptr = NULL;
		}
	}

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: getModData(target, name)
 * Get moddata from a client or channel
 */
duk_ret_t js_api_getModData(duk_context *ctx)
{
	const char *name;
	JSModData *jsmd;

	/* Get moddata name */
	name = duk_require_string(ctx, 1);

	/* Find registered moddata */
	for (jsmd = js_moddatas; jsmd; jsmd = jsmd->next)
	{
		if (!strcmp(jsmd->name, name))
			break;
	}

	if (!jsmd || !jsmd->md)
	{
		duk_push_null(ctx);
		return 1;
	}

	/* Handle based on target type */
	const char *target_name = NULL;
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		target_name = duk_get_string(ctx, -1);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		target_name = duk_get_string(ctx, 0);
	}

	if (!target_name)
	{
		duk_push_null(ctx);
		return 1;
	}

	if (jsmd->type == MODDATATYPE_CLIENT || jsmd->type == MODDATATYPE_LOCAL_CLIENT)
	{
		Client *client = find_client(target_name, NULL);
		if (!client)
		{
			duk_push_null(ctx);
			return 1;
		}
		const char *value = moddata_client_get(client, name);
		if (value)
			duk_push_string(ctx, value);
		else
			duk_push_null(ctx);
	}
	else if (jsmd->type == MODDATATYPE_CHANNEL)
	{
		Channel *channel = find_channel(target_name);
		if (!channel)
		{
			duk_push_null(ctx);
			return 1;
		}
		ModData *md = &moddata_channel(channel, jsmd->md);
		if (md->ptr)
			duk_push_string(ctx, (const char *)md->ptr);
		else
			duk_push_null(ctx);
	}
	else
	{
		duk_push_null(ctx);
	}

	return 1;
}

/*
 * API: findChannel(name)
 * Find a channel by name
 */
duk_ret_t js_api_findChannel(duk_context *ctx)
{
	const char *name = duk_require_string(ctx, 0);
	Channel *channel = find_channel(name);

	js_push_channel_object(ctx, channel);
	return 1;
}

/*
 * API: findServer(name)
 * Find a server by name
 */
duk_ret_t js_api_findServer(duk_context *ctx)
{
	const char *name = duk_require_string(ctx, 0);
	Client *server = find_server(name, NULL);

	js_push_client_object(ctx, server);
	return 1;
}

/*
 * API: isUser(client)
 * Check if client is a user (not server/unknown)
 */
duk_ret_t js_api_isUser(duk_context *ctx)
{
	Client *client = NULL;

	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	duk_push_boolean(ctx, client && IsUser(client));
	return 1;
}

/*
 * API: isServer(client)
 * Check if client is a server
 */
duk_ret_t js_api_isServer(duk_context *ctx)
{
	Client *client = NULL;

	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	duk_push_boolean(ctx, client && IsServer(client));
	return 1;
}

/*
 * API: isLoggedIn(client)
 * Check if client is logged into services
 */
duk_ret_t js_api_isLoggedIn(duk_context *ctx)
{
	Client *client = NULL;

	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	duk_push_boolean(ctx, client && IsUser(client) && IsLoggedIn(client));
	return 1;
}

/*
 * API: isSecure(client)
 * Check if client is using TLS
 */
duk_ret_t js_api_isSecure(duk_context *ctx)
{
	Client *client = NULL;

	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	duk_push_boolean(ctx, client && IsUser(client) && IsSecure(client));
	return 1;
}

/*
 * API: isULine(client)
 * Check if client is a U-Line (services)
 */
duk_ret_t js_api_isULine(duk_context *ctx)
{
	Client *client = NULL;

	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	duk_push_boolean(ctx, client && IsULine(client));
	return 1;
}

/*
 * API: doCmd(client, command, ...args)
 * Execute an IRC command as the specified client
 * Can be called as:
 *   doCmd($client, 'JOIN #welcome')  - parses the string
 *   doCmd($client, 'JOIN', '#welcome')  - separate args
 *   doCmd($client, 'PRIVMSG', '#test', 'Hello!')  - multiple args
 */
duk_ret_t js_api_doCmd(duk_context *ctx)
{
	Client *client = NULL;
	char *cmd = NULL;
	int parc = 0;
	char *parv_buf[MAXPARA];  // Temporary buffer for copied strings
	const char *parv[MAXPARA];
	int nargs = duk_get_top(ctx);
	int i;

	/* Initialize arrays */
	memset(parv_buf, 0, sizeof(parv_buf));
	memset(parv, 0, sizeof(parv));

	/* Get client */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	if (!client)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get command - if it's a full string like "JOIN #channel", parse it */
	const char *fullcmd = duk_require_string(ctx, 1);
	
	/* parv[0] is always NULL */
	parc = 1;
	
	/* Check if we have additional args or if we need to parse the command string */
	if (nargs == 2)
	{
		/* Parse the full command string "JOIN #channel" style */
		char *cmdcopy = strdup(fullcmd);
		char *token;
		char *saveptr = NULL;
		
		/* First token is the command */
		token = strtok_r(cmdcopy, " ", &saveptr);
		if (token)
		{
			cmd = strdup(token);
			
			/* Remaining tokens are arguments */
			while ((token = strtok_r(NULL, " ", &saveptr)) && parc < MAXPARA - 1)
			{
				parv_buf[parc] = strdup(token);
				parv[parc] = parv_buf[parc];
				parc++;
			}
		}
		else
		{
			cmd = strdup(fullcmd);
		}
		
		free(cmdcopy);
	}
	else
	{
		/* Command and args provided separately: doCmd($client, 'JOIN', '#channel') */
		cmd = strdup(fullcmd);
		
		/* Collect remaining arguments */
		for (i = 2; i < nargs && parc < MAXPARA - 1; i++)
		{
			const char *arg = duk_to_string(ctx, i);
			parv_buf[parc] = strdup(arg);
			parv[parc] = parv_buf[parc];
			parc++;
		}
	}

	/* NULL-terminate the parv array (required by do_cmd) */
	parv[parc] = NULL;

	/* Execute command */
	do_cmd(client, NULL, cmd, parc, parv);

	/* Free allocated strings */
	free(cmd);
	for (i = 0; i < parc; i++)
	{
		if (parv_buf[i])
			free(parv_buf[i]);
	}

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: exitClient(client, reason)
 * Disconnect a client with the specified reason
 */
duk_ret_t js_api_exitClient(duk_context *ctx)
{
	Client *client = NULL;
	const char *reason;

	/* Get client */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	if (!client)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get reason */
	reason = duk_require_string(ctx, 1);

	/* Exit the client */
	exit_client(client, NULL, reason);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: sendToChannel(channel, from, message, mtags)
 * Send a message to all users in a channel
 * Note: 'from' can be null to send as server, mtags is optional
 */
duk_ret_t js_api_sendToChannel(duk_context *ctx)
{
	Channel *channel = NULL;
	Client *from = NULL;
	const char *message;
	const char *channel_name;
	MessageTag *mtags = NULL;

	/* Get channel */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		channel_name = duk_get_string(ctx, -1);
		channel = find_channel(channel_name);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		channel = find_channel(duk_get_string(ctx, 0));
	}

	if (!channel)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get 'from' client (optional, arg 1) - can be null/undefined */
	if (!duk_is_null_or_undefined(ctx, 1))
	{
		if (duk_is_object(ctx, 1))
		{
			duk_get_prop_string(ctx, 1, "name");
			const char *name = duk_get_string(ctx, -1);
			from = find_client(name, NULL);
			duk_pop(ctx);
		}
		else if (duk_is_string(ctx, 1))
		{
			from = find_client(duk_get_string(ctx, 1), NULL);
		}
	}

	/* Default to server if no from specified */
	if (!from)
		from = &me;

	/* Get message (arg 2) */
	message = duk_require_string(ctx, 2);

	/* Get optional mtags (arg 3) */
	if (duk_get_top(ctx) > 3 && !duk_is_null_or_undefined(ctx, 3))
	{
		mtags = js_pop_mtags(ctx, 3);
	}

	/* Send PRIVMSG to channel */
	sendto_channel(channel, from, NULL, NULL, 0, 
	               SEND_LOCAL|SEND_REMOTE,
	               mtags, "%s", message);

	/* Free mtags if we created them */
	if (mtags)
		free_message_tags(mtags);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: sendToServer(server, raw, mtags)
 * Send raw data to a server with optional message tags
 */
duk_ret_t js_api_sendToServer(duk_context *ctx)
{
	Client *server = NULL;
	const char *raw;
	MessageTag *mtags = NULL;

	/* Get server */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		server = find_server(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		server = find_server(duk_get_string(ctx, 0), NULL);
	}

	if (!server)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get raw message */
	raw = duk_require_string(ctx, 1);

	/* Get optional mtags (arg 2) */
	if (duk_get_top(ctx) > 2 && !duk_is_null_or_undefined(ctx, 2))
	{
		mtags = js_pop_mtags(ctx, 2);
	}

	/* Send to server */
	sendto_one(server, mtags, "%s", raw);

	/* Free mtags if we created them */
	if (mtags)
		free_message_tags(mtags);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: sendToAllServers(raw, mtags)
 * Send raw data to all servers with optional message tags
 */
duk_ret_t js_api_sendToAllServers(duk_context *ctx)
{
	const char *raw;
	MessageTag *mtags = NULL;

	/* Get raw message */
	raw = duk_require_string(ctx, 0);

	/* Get optional mtags (arg 1) */
	if (duk_get_top(ctx) > 1 && !duk_is_null_or_undefined(ctx, 1))
	{
		mtags = js_pop_mtags(ctx, 1);
	}

	/* Send to all servers (NULL = broadcast to all) */
	sendto_server(NULL, 0, 0, mtags, "%s", raw);

	/* Free mtags if we created them */
	if (mtags)
		free_message_tags(mtags);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: setUserMode(client, mode)
 * Set or clear a user mode on a client (use +x or -x)
 */
duk_ret_t js_api_setUserMode(duk_context *ctx)
{
	Client *client = NULL;
	const char *mode_str;
	int adding = 1;

	/* Get client */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	if (!client || !IsUser(client))
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get mode */
	mode_str = duk_require_string(ctx, 1);

	/* Check for +/- prefix */
	if (mode_str[0] == '-')
	{
		adding = 0;
		mode_str++;
	}
	else if (mode_str[0] == '+')
	{
		mode_str++;
	}

	/* Set or clear the mode */
	long old = client->umodes;
	long mode = set_usermode(mode_str);
	if (adding)
		client->umodes |= mode;
	else
		client->umodes &= ~mode;
	if (MyUser(client))
		send_umode_out(client, 1, old);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: setChannelMode(channel, from, modestring)
 * Set channel mode(s)
 */
duk_ret_t js_api_setChannelMode(duk_context *ctx)
{
	Channel *channel = NULL;
	Client *from = NULL;
	const char *modestring;

	/* Get channel */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		channel = find_channel(name);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		channel = find_channel(duk_get_string(ctx, 0));
	}

	if (!channel)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get 'from' client (optional) */
	if (duk_is_object(ctx, 1))
	{
		duk_get_prop_string(ctx, 1, "name");
		const char *name = duk_get_string(ctx, -1);
		from = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 1))
	{
		from = find_client(duk_get_string(ctx, 1), NULL);
	}

	if (!from)
		from = &me;

	/* Get mode string */
	modestring = duk_require_string(ctx, 2);

	/* Execute mode change via command */
	const char *parv[4];
	parv[0] = channel->name;
	parv[1] = modestring;
	parv[2] = NULL;
	do_cmd(from, NULL, "MODE", 2, parv);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: checkChannelAccess(client, channel, modes)
 * Check if client has specified channel access
 */
duk_ret_t js_api_checkChannelAccess(duk_context *ctx)
{
	Client *client = NULL;
	Channel *channel = NULL;
	const char *modes;

	/* Get client */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	/* Get channel */
	if (duk_is_object(ctx, 1))
	{
		duk_get_prop_string(ctx, 1, "name");
		const char *name = duk_get_string(ctx, -1);
		channel = find_channel(name);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 1))
	{
		channel = find_channel(duk_get_string(ctx, 1));
	}

	if (!client || !channel)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get modes to check */
	modes = duk_require_string(ctx, 2);

	/* Check access */
	duk_push_boolean(ctx, check_channel_access(client, channel, modes));
	return 1;
}

/*
 * API: getChannelMembers(channel)
 * Get array of channel members
 */
duk_ret_t js_api_getChannelMembers(duk_context *ctx)
{
	Channel *channel = NULL;
	Member *m;
	int idx = 0;

	/* Get channel */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		channel = find_channel(name);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		channel = find_channel(duk_get_string(ctx, 0));
	}

	if (!channel)
	{
		duk_push_array(ctx);
		return 1;
	}

	/* Build array of members */
	duk_idx_t arr = duk_push_array(ctx);
	for (m = channel->members; m; m = m->next)
	{
		if (m->client)
		{
			duk_idx_t obj = duk_push_object(ctx);
			
			js_push_client_object(ctx, m->client);
			duk_put_prop_string(ctx, obj, "client");
			
			/* Get member modes/prefix */
			const char *access = get_channel_access(m->client, channel);
			if (access)
			{
				duk_push_string(ctx, access);
				duk_put_prop_string(ctx, obj, "modes");
			}

			duk_put_prop_index(ctx, arr, idx++);
		}
	}

	return 1;
}

/*
 * API: getUserChannels(client)
 * Get array of channels a user is in
 */
duk_ret_t js_api_getUserChannels(duk_context *ctx)
{
	Client *client = NULL;
	Membership *mb;
	int idx = 0;

	/* Get client */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	if (!client || !IsUser(client) || !client->user)
	{
		duk_push_array(ctx);
		return 1;
	}

	/* Build array of channels */
	duk_idx_t arr = duk_push_array(ctx);
	for (mb = client->user->channel; mb; mb = mb->next)
	{
		if (mb->channel)
		{
			duk_idx_t obj = duk_push_object(ctx);
			
			js_push_channel_object(ctx, mb->channel);
			duk_put_prop_string(ctx, obj, "channel");
			
			/* Get member modes */
			const char *access = get_channel_access(client, mb->channel);
			if (access)
			{
				duk_push_string(ctx, access);
				duk_put_prop_string(ctx, obj, "modes");
			}

			duk_put_prop_index(ctx, arr, idx++);
		}
	}

	return 1;
}

/*
 * API: joinChannel(client, channel)
 * Make a client join a channel
 */
duk_ret_t js_api_joinChannel(duk_context *ctx)
{
	Client *client = NULL;
	const char *channel_name;

	/* Get client */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	if (!client || !IsUser(client))
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get channel name */
	if (duk_is_object(ctx, 1))
	{
		duk_get_prop_string(ctx, 1, "name");
		channel_name = duk_get_string(ctx, -1);
		duk_pop(ctx);
	}
	else
	{
		channel_name = duk_require_string(ctx, 1);
	}

	/* Execute JOIN command */
	const char *parv[3];
	parv[0] = NULL;
	parv[1] = channel_name;
	parv[2] = NULL;
	do_cmd(client, NULL, "JOIN", 2, parv);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: partChannel(client, channel, reason)
 * Make a client part a channel
 */
duk_ret_t js_api_partChannel(duk_context *ctx)
{
	Client *client = NULL;
	const char *channel_name;
	const char *reason = NULL;

	/* Get client */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	if (!client || !IsUser(client))
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get channel name */
	if (duk_is_object(ctx, 1))
	{
		duk_get_prop_string(ctx, 1, "name");
		channel_name = duk_get_string(ctx, -1);
		duk_pop(ctx);
	}
	else
	{
		channel_name = duk_require_string(ctx, 1);
	}

	/* Get reason (optional) */
	if (duk_is_string(ctx, 2))
		reason = duk_get_string(ctx, 2);

	/* Execute PART command */
	const char *parv[4];
	parv[0] = NULL;
	parv[1] = channel_name;
	parv[2] = reason;
	parv[3] = NULL;
	do_cmd(client, NULL, "PART", reason ? 3 : 2, parv);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: kickUser(channel, client, victim, reason)
 * Kick a user from a channel
 */
duk_ret_t js_api_kickUser(duk_context *ctx)
{
	Client *client = NULL;
	Client *victim = NULL;
	const char *channel_name;
	const char *reason;

	/* Get channel name */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		channel_name = duk_get_string(ctx, -1);
		duk_pop(ctx);
	}
	else
	{
		channel_name = duk_require_string(ctx, 0);
	}

	/* Get kicker client */
	if (duk_is_object(ctx, 1))
	{
		duk_get_prop_string(ctx, 1, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 1))
	{
		client = find_client(duk_get_string(ctx, 1), NULL);
	}

	if (!client)
		client = &me;

	/* Get victim */
	if (duk_is_object(ctx, 2))
	{
		duk_get_prop_string(ctx, 2, "name");
		const char *name = duk_get_string(ctx, -1);
		victim = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 2))
	{
		victim = find_client(duk_get_string(ctx, 2), NULL);
	}

	if (!victim)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get reason */
	reason = duk_require_string(ctx, 3);

	/* Execute KICK command */
	const char *parv[5];
	parv[0] = NULL;
	parv[1] = channel_name;
	parv[2] = victim->name;
	parv[3] = reason;
	parv[4] = NULL;
	do_cmd(client, NULL, "KICK", 4, parv);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: setTopic(channel, client, topic)
 * Set the topic of a channel
 */
duk_ret_t js_api_setTopic(duk_context *ctx)
{
	Client *client = NULL;
	const char *channel_name;
	const char *topic;

	/* Get channel name */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		channel_name = duk_get_string(ctx, -1);
		duk_pop(ctx);
	}
	else
	{
		channel_name = duk_require_string(ctx, 0);
	}

	/* Get client */
	if (duk_is_object(ctx, 1))
	{
		duk_get_prop_string(ctx, 1, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 1))
	{
		client = find_client(duk_get_string(ctx, 1), NULL);
	}

	if (!client)
		client = &me;

	/* Get topic */
	topic = duk_require_string(ctx, 2);

	/* Execute TOPIC command */
	const char *parv[4];
	parv[0] = NULL;
	parv[1] = channel_name;
	parv[2] = topic;
	parv[3] = NULL;
	do_cmd(client, NULL, "TOPIC", 3, parv);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: changeNick(client, newnick)
 * Change a client's nickname
 */
duk_ret_t js_api_changeNick(duk_context *ctx)
{
	Client *client = NULL;
	const char *newnick;

	/* Get client */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	if (!client || !IsUser(client))
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get new nick */
	newnick = duk_require_string(ctx, 1);

	/* Execute NICK command */
	const char *parv[3];
	parv[0] = NULL;
	parv[1] = newnick;
	parv[2] = NULL;
	do_cmd(client, NULL, "NICK", 2, parv);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: setHost(client, newhost)
 * Set a client's virtual host (vhost)
 */
duk_ret_t js_api_setHost(duk_context *ctx)
{
	Client *client = NULL;
	const char *newhost;

	/* Get client */
	if (duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		client = find_client(name, NULL);
		duk_pop(ctx);
	}
	else if (duk_is_string(ctx, 0))
	{
		client = find_client(duk_get_string(ctx, 0), NULL);
	}

	if (!client || !IsUser(client) || !client->user)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	/* Get new host */
	newhost = duk_require_string(ctx, 1);

	/* Set the vhost */
	safe_strdup(client->user->virthost, newhost);
	if (MyUser(client))
		sendnumeric(client, RPL_HOSTHIDDEN, newhost);

	duk_push_boolean(ctx, 1);
	return 1;
}

/*
 * API: addTKL(config)
 * Add a TKL (server ban)
 * config = { type: 'gline', mask: '*@bad.host', reason: 'Banned', duration: 3600 }
 */
duk_ret_t js_api_addTKL(duk_context *ctx)
{
	/* TKL implementation is complex - simplified version */
	duk_push_boolean(ctx, 0);
	return 1;
}

/*
 * API: delTKL(config)
 * Delete a TKL (server ban)
 */
duk_ret_t js_api_delTKL(duk_context *ctx)
{
	/* TKL implementation is complex - simplified version */
	duk_push_boolean(ctx, 0);
	return 1;
}

/*
 * API: createMtag(name, value)
 * Create a single message tag object
 * Returns: { name: "tagname", value: "tagvalue" }
 */
duk_ret_t js_api_createMtag(duk_context *ctx)
{
	const char *name;
	const char *value = NULL;

	name = duk_require_string(ctx, 0);

	if (!duk_is_null_or_undefined(ctx, 1))
		value = duk_get_string(ctx, 1);

	duk_idx_t obj_idx = duk_push_object(ctx);

	duk_push_string(ctx, name);
	duk_put_prop_string(ctx, obj_idx, "name");

	if (value)
	{
		duk_push_string(ctx, value);
		duk_put_prop_string(ctx, obj_idx, "value");
	}
	else
	{
		duk_push_null(ctx);
		duk_put_prop_string(ctx, obj_idx, "value");
	}

	return 1;
}

/*
 * API: addMtag(mtags_array, name, value)
 * Add a message tag to an mtags array (modifies in place)
 * Returns the modified array
 */
duk_ret_t js_api_addMtag(duk_context *ctx)
{
	const char *name;
	const char *value = NULL;
	duk_size_t len;

	/* First arg must be an array (or null to create new) */
	if (duk_is_null_or_undefined(ctx, 0))
	{
		/* Create new array */
		duk_push_array(ctx);
		duk_replace(ctx, 0);
	}
	else if (!duk_is_array(ctx, 0))
	{
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "First argument must be an array or null");
		return 0;
	}

	name = duk_require_string(ctx, 1);

	if (!duk_is_null_or_undefined(ctx, 2))
		value = duk_get_string(ctx, 2);

	/* Get current length */
	len = duk_get_length(ctx, 0);

	/* Create new mtag object */
	duk_idx_t obj_idx = duk_push_object(ctx);

	duk_push_string(ctx, name);
	duk_put_prop_string(ctx, obj_idx, "name");

	if (value)
	{
		duk_push_string(ctx, value);
		duk_put_prop_string(ctx, obj_idx, "value");
	}
	else
	{
		duk_push_null(ctx);
		duk_put_prop_string(ctx, obj_idx, "value");
	}

	/* Add to array */
	duk_put_prop_index(ctx, 0, len);

	/* Return the array */
	duk_dup(ctx, 0);
	return 1;
}

/*
 * API: findMtag(mtags_array, name)
 * Find a message tag by name in an mtags array
 * Returns the mtag object or null if not found
 */
duk_ret_t js_api_findMtag(duk_context *ctx)
{
	const char *name;
	duk_size_t len, i;

	if (duk_is_null_or_undefined(ctx, 0) || !duk_is_array(ctx, 0))
	{
		duk_push_null(ctx);
		return 1;
	}

	name = duk_require_string(ctx, 1);
	len = duk_get_length(ctx, 0);

	for (i = 0; i < len; i++)
	{
		duk_get_prop_index(ctx, 0, i);

		if (duk_is_object(ctx, -1))
		{
			duk_get_prop_string(ctx, -1, "name");
			const char *tag_name = duk_get_string(ctx, -1);
			duk_pop(ctx);

			if (tag_name && strcmp(tag_name, name) == 0)
			{
				/* Found - return the object (it's still on stack) */
				return 1;
			}
		}

		duk_pop(ctx);
	}

	duk_push_null(ctx);
	return 1;
}

/*
 * API: deleteMtag(mtags_array, name)
 * Delete a message tag by name from an mtags array
 * Returns the modified array
 */
duk_ret_t js_api_deleteMtag(duk_context *ctx)
{
	const char *name;
	duk_size_t len, i;
	duk_idx_t new_arr_idx;
	duk_size_t new_idx = 0;

	if (duk_is_null_or_undefined(ctx, 0) || !duk_is_array(ctx, 0))
	{
		duk_push_null(ctx);
		return 1;
	}

	name = duk_require_string(ctx, 1);
	len = duk_get_length(ctx, 0);

	/* Create new array without the matching tag */
	new_arr_idx = duk_push_array(ctx);

	for (i = 0; i < len; i++)
	{
		duk_get_prop_index(ctx, 0, i);

		if (duk_is_object(ctx, -1))
		{
			duk_get_prop_string(ctx, -1, "name");
			const char *tag_name = duk_get_string(ctx, -1);
			duk_pop(ctx);

			if (!tag_name || strcmp(tag_name, name) != 0)
			{
				/* Keep this one */
				duk_put_prop_index(ctx, new_arr_idx, new_idx++);
				continue;
			}
		}

		duk_pop(ctx);
	}

	return 1;
}

/*
 * API: mtagsToString(mtags_array, client)
 * Convert an mtags array to an IRC protocol string (without the @ prefix)
 * client is optional - if provided, tags will be filtered based on client capabilities
 * Returns string or null
 */
duk_ret_t js_api_mtagsToString(duk_context *ctx)
{
	MessageTag *mtags;
	Client *client = NULL;
	const char *result;

	/* Get mtags array */
	mtags = js_pop_mtags(ctx, 0);

	if (!mtags)
	{
		duk_push_null(ctx);
		return 1;
	}

	/* Get optional client */
	if (!duk_is_null_or_undefined(ctx, 1) && duk_is_object(ctx, 1))
	{
		duk_get_prop_string(ctx, 1, "name");
		const char *name = duk_get_string(ctx, -1);
		if (name)
			client = find_client(name, NULL);
		duk_pop(ctx);
	}

	/* Convert to string */
	result = mtags_to_string(mtags, client);

	if (result)
		duk_push_string(ctx, result);
	else
		duk_push_null(ctx);

	/* Free the mtags we created */
	free_message_tags(mtags);

	return 1;
}

/*
 * API: newMessageTags(sender)
 * Create a new message tag list with standard tags (msgid, time) prepopulated
 * sender can be a client object or null for server-generated messages
 * Returns an mtags array
 */
duk_ret_t js_api_newMessageTags(duk_context *ctx)
{
	Client *sender = NULL;
	MessageTag *mtags = NULL;

	/* Get optional sender */
	if (!duk_is_null_or_undefined(ctx, 0) && duk_is_object(ctx, 0))
	{
		duk_get_prop_string(ctx, 0, "name");
		const char *name = duk_get_string(ctx, -1);
		if (name)
			sender = find_client(name, NULL);
		duk_pop(ctx);
	}

	if (!sender)
		sender = &me;

	/* Create new message tags using UnrealIRCd's standard function */
	new_message(sender, NULL, &mtags);

	/* Convert to JavaScript array */
	js_push_mtags_object(ctx, mtags);

	/* Free the C mtags */
	safe_free_message_tags(mtags);

	return 1;
}

/* =========================================================================
 * SPECIAL LIST ITERATION API FUNCTIONS
 * ========================================================================= */

/*
 * API: getAllClients()
 * Returns an array of all clients (users) on the network
 */
duk_ret_t js_api_getAllClients(duk_context *ctx)
{
	Client *acptr;
	duk_idx_t arr_idx = duk_push_array(ctx);
	int i = 0;

	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (IsUser(acptr))
		{
			js_push_client_object(ctx, acptr);
			duk_put_prop_index(ctx, arr_idx, i++);
		}
	}

	return 1;
}

/*
 * API: getAllLocalClients()
 * Returns an array of all locally connected clients (users)
 */
duk_ret_t js_api_getAllLocalClients(duk_context *ctx)
{
	Client *acptr;
	duk_idx_t arr_idx = duk_push_array(ctx);
	int i = 0;

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsUser(acptr))
		{
			js_push_client_object(ctx, acptr);
			duk_put_prop_index(ctx, arr_idx, i++);
		}
	}

	return 1;
}

/*
 * API: getAllServers()
 * Returns an array of all servers on the network
 */
duk_ret_t js_api_getAllServers(duk_context *ctx)
{
	Client *acptr;
	duk_idx_t arr_idx = duk_push_array(ctx);
	int i = 0;

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		js_push_client_object(ctx, acptr);
		duk_put_prop_index(ctx, arr_idx, i++);
	}

	return 1;
}

/*
 * API: getAllLocalServers()
 * Returns an array of locally connected servers
 */
duk_ret_t js_api_getAllLocalServers(duk_context *ctx)
{
	Client *acptr;
	duk_idx_t arr_idx = duk_push_array(ctx);
	int i = 0;

	list_for_each_entry(acptr, &server_list, special_node)
	{
		js_push_client_object(ctx, acptr);
		duk_put_prop_index(ctx, arr_idx, i++);
	}

	return 1;
}

/*
 * API: getAllOpers()
 * Returns an array of all locally connected IRC operators
 */
duk_ret_t js_api_getAllOpers(duk_context *ctx)
{
	Client *acptr;
	duk_idx_t arr_idx = duk_push_array(ctx);
	int i = 0;

	list_for_each_entry(acptr, &oper_list, special_node)
	{
		js_push_client_object(ctx, acptr);
		duk_put_prop_index(ctx, arr_idx, i++);
	}

	return 1;
}

/*
 * API: getAllChannels()
 * Returns an array of all channels on the network
 */
duk_ret_t js_api_getAllChannels(duk_context *ctx)
{
	Channel *channel;
	duk_idx_t arr_idx = duk_push_array(ctx);
	int i = 0;

	for (channel = channels; channel; channel = channel->nextch)
	{
		js_push_channel_object(ctx, channel);
		duk_put_prop_index(ctx, arr_idx, i++);
	}

	return 1;
}

/* =========================================================================
 * RPC HANDLER API FUNCTIONS
 * ========================================================================= */

/*
 * RPC call handler - called when a JavaScript-registered RPC method is invoked
 */
RPC_CALL_FUNC(js_rpc_handler)
{
	JSRPCHandler *handler;
	const char *method_name;
	char *json_str;

	/* Get the method name from the request */
	json_t *method = json_object_get(request, "method");
	if (!method || !json_is_string(method))
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_REQUEST, "Invalid method");
		return;
	}
	method_name = json_string_value(method);

	/* Find our handler */
	for (handler = js_rpc_handlers; handler; handler = handler->next)
	{
		if (!strcasecmp(handler->method, method_name))
			break;
	}

	if (!handler)
	{
		rpc_error(client, request, JSON_RPC_ERROR_METHOD_NOT_FOUND, "Method not found");
		return;
	}

	/* Store request/client info for rpcResponse/rpcError functions */
	duk_push_global_stash(handler->ctx);

	/* Store the RPC client in the stash */
	duk_push_pointer(handler->ctx, client);
	duk_put_prop_string(handler->ctx, -2, "__rpc_client");

	/* Store the request in the stash */
	duk_push_pointer(handler->ctx, request);
	duk_put_prop_string(handler->ctx, -2, "__rpc_request");

	duk_pop(handler->ctx);

	/* Set up $client for the RPC caller */
	duk_push_global_object(handler->ctx);
	js_push_client_object(handler->ctx, client);
	duk_put_prop_string(handler->ctx, -2, "$client");

	/* Set up $params with the RPC parameters */
	json_str = json_dumps(params, JSON_COMPACT);
	if (json_str)
	{
		duk_push_string(handler->ctx, json_str);
		duk_json_decode(handler->ctx, -1);
		duk_put_prop_string(handler->ctx, -2, "$params");
		free(json_str);
	}
	else
	{
		duk_push_object(handler->ctx);
		duk_put_prop_string(handler->ctx, -2, "$params");
	}

	/* Set up $request with the full request */
	json_str = json_dumps(request, JSON_COMPACT);
	if (json_str)
	{
		duk_push_string(handler->ctx, json_str);
		duk_json_decode(handler->ctx, -1);
		duk_put_prop_string(handler->ctx, -2, "$request");
		free(json_str);
	}

	duk_pop(handler->ctx);

	/* Get the handler function from the stash */
	duk_push_global_stash(handler->ctx);
	if (duk_get_prop_string(handler->ctx, -1, handler->handler_code))
	{
		if (duk_pcall(handler->ctx, 0) != 0)
		{
			const char *error = duk_safe_to_string(handler->ctx, -1);
			unreal_log(ULOG_ERROR, "obbyscript", "JS_RPC_ERROR", NULL,
			           "RPC handler error for '$method': $error",
			           log_data_string("method", handler->method),
			           log_data_string("error", error));
			rpc_error(client, request, JSON_RPC_ERROR_INTERNAL_ERROR, error);
		}
		duk_pop(handler->ctx);
	}
	else
	{
		rpc_error(client, request, JSON_RPC_ERROR_INTERNAL_ERROR, "Handler function not found");
	}
	duk_pop(handler->ctx); /* pop stash */

	/* Clean up stash entries */
	duk_push_global_stash(handler->ctx);
	duk_del_prop_string(handler->ctx, -1, "__rpc_client");
	duk_del_prop_string(handler->ctx, -1, "__rpc_request");
	duk_pop(handler->ctx);
}

/*
 * API: registerRPCMethod(config)
 * Register a new RPC method handler
 * config = { method: "namespace.method", handler: function() {...}, loglevel: "debug" }
 */
duk_ret_t js_api_registerRPCMethod(duk_context *ctx)
{
	JSRPCHandler *handler;
	RPCHandlerInfo req;
	const char *method;
	const char *loglevel_str;
	char stash_key[128];

	if (!duk_is_object(ctx, 0))
	{
		duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "registerRPCMethod requires an object parameter");
		return duk_throw(ctx);
	}

	/* Get method name */
	duk_get_prop_string(ctx, 0, "method");
	if (!duk_is_string(ctx, -1))
	{
		duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "method must be a string");
		return duk_throw(ctx);
	}
	method = duk_get_string(ctx, -1);
	duk_pop(ctx);

	/* Get handler function */
	duk_get_prop_string(ctx, 0, "handler");
	if (!duk_is_function(ctx, -1))
	{
		duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "handler must be a function");
		return duk_throw(ctx);
	}

	/* Store handler in stash */
	snprintf(stash_key, sizeof(stash_key), "rpc_%s", method);
	duk_push_global_stash(ctx);
	duk_dup(ctx, -2);
	duk_put_prop_string(ctx, -2, stash_key);
	duk_pop_2(ctx);

	/* Create our handler structure */
	handler = safe_alloc(sizeof(JSRPCHandler));
	safe_strdup(handler->method, method);
	safe_strdup(handler->handler_code, stash_key);
	handler->ctx = ctx;

	/* Get optional loglevel */
	memset(&req, 0, sizeof(req));
	req.method = handler->method;
	req.call = js_rpc_handler;

	duk_get_prop_string(ctx, 0, "loglevel");
	if (duk_is_string(ctx, -1))
	{
		loglevel_str = duk_get_string(ctx, -1);
		if (!strcasecmp(loglevel_str, "debug"))
			req.loglevel = ULOG_DEBUG;
		else if (!strcasecmp(loglevel_str, "info"))
			req.loglevel = ULOG_INFO;
		else if (!strcasecmp(loglevel_str, "warning"))
			req.loglevel = ULOG_WARNING;
		else if (!strcasecmp(loglevel_str, "error"))
			req.loglevel = ULOG_ERROR;
	}
	duk_pop(ctx);

	/* Register with UnrealIRCd */
	handler->rpc = RPCHandlerAdd(js_modinfo->handle, &req);
	if (!handler->rpc)
	{
		safe_free(handler->method);
		safe_free(handler->handler_code);
		safe_free(handler);
		duk_push_error_object(ctx, DUK_ERR_ERROR, "Failed to register RPC method");
		return duk_throw(ctx);
	}

	/* Add to our list */
	AddListItem(handler, js_rpc_handlers);

	unreal_log(ULOG_INFO, "obbyscript", "JS_RPC_REGISTERED", NULL,
	           "JavaScript registered RPC method: $method",
	           log_data_string("method", method));

	duk_push_true(ctx);
	return 1;
}

/*
 * API: rpcResponse(result)
 * Send a successful RPC response (only valid inside an RPC handler)
 * result is a JavaScript object that will be JSON-encoded
 */
duk_ret_t js_api_rpcResponse(duk_context *ctx)
{
	Client *client;
	json_t *request;
	json_t *result;
	const char *json_str;

	/* Get client and request from stash */
	duk_push_global_stash(ctx);

	if (!duk_get_prop_string(ctx, -1, "__rpc_client"))
	{
		duk_pop_2(ctx);
		duk_push_error_object(ctx, DUK_ERR_ERROR, "rpcResponse can only be called inside an RPC handler");
		return duk_throw(ctx);
	}
	client = (Client *)duk_get_pointer(ctx, -1);
	duk_pop(ctx);

	if (!duk_get_prop_string(ctx, -1, "__rpc_request"))
	{
		duk_pop_2(ctx);
		duk_push_error_object(ctx, DUK_ERR_ERROR, "rpcResponse can only be called inside an RPC handler");
		return duk_throw(ctx);
	}
	request = (json_t *)duk_get_pointer(ctx, -1);
	duk_pop_2(ctx);

	if (!client || !request)
	{
		duk_push_error_object(ctx, DUK_ERR_ERROR, "Invalid RPC context");
		return duk_throw(ctx);
	}

	/* Convert JS object to JSON string, then to jansson json_t */
	duk_dup(ctx, 0);
	json_str = duk_json_encode(ctx, -1);

	json_error_t error;
	result = json_loads(json_str, 0, &error);
	duk_pop(ctx);

	if (!result)
	{
		/* If parsing fails, create a simple object with the value */
		result = json_object();
		json_object_set_new(result, "result", json_true());
	}

	rpc_response(client, request, result);
	json_decref(result);

	return 0;
}

/*
 * API: rpcError(code, message)
 * Send an RPC error response (only valid inside an RPC handler)
 * code is the JSON-RPC error code
 * message is the error message
 */
duk_ret_t js_api_rpcError(duk_context *ctx)
{
	Client *client;
	json_t *request;
	int error_code;
	const char *error_message;

	/* Get client and request from stash */
	duk_push_global_stash(ctx);

	if (!duk_get_prop_string(ctx, -1, "__rpc_client"))
	{
		duk_pop_2(ctx);
		duk_push_error_object(ctx, DUK_ERR_ERROR, "rpcError can only be called inside an RPC handler");
		return duk_throw(ctx);
	}
	client = (Client *)duk_get_pointer(ctx, -1);
	duk_pop(ctx);

	if (!duk_get_prop_string(ctx, -1, "__rpc_request"))
	{
		duk_pop_2(ctx);
		duk_push_error_object(ctx, DUK_ERR_ERROR, "rpcError can only be called inside an RPC handler");
		return duk_throw(ctx);
	}
	request = (json_t *)duk_get_pointer(ctx, -1);
	duk_pop_2(ctx);

	if (!client || !request)
	{
		duk_push_error_object(ctx, DUK_ERR_ERROR, "Invalid RPC context");
		return duk_throw(ctx);
	}

	/* Get error code and message */
	error_code = duk_require_int(ctx, 0);
	error_message = duk_require_string(ctx, 1);

	rpc_error(client, request, (JsonRpcError)error_code, error_message);

	return 0;
}

/* =========================================================================
 * EXTBAN API FUNCTIONS
 * ========================================================================= */

/*
 * Extban is_ok handler - called to validate extban parameters
 * Uses js_current_extban set by conv_param
 */
int js_extban_is_ok(BanContext *b)
{
	JSExtban *eb = js_current_extban;

	if (!eb || !eb->is_ok_handler)
		return 1; /* Default: accept */

	/* Set up context */
	duk_push_global_object(eb->ctx);

	/* Create $ban object */
	duk_push_object(eb->ctx);
	if (b->client)
	{
		js_push_client_object(eb->ctx, b->client);
		duk_put_prop_string(eb->ctx, -2, "client");
	}
	if (b->channel)
	{
		js_push_channel_object(eb->ctx, b->channel);
		duk_put_prop_string(eb->ctx, -2, "channel");
	}
	duk_push_string(eb->ctx, b->banstr);
	duk_put_prop_string(eb->ctx, -2, "banstr");
	duk_push_int(eb->ctx, b->ban_check_types);
	duk_put_prop_string(eb->ctx, -2, "checkType");
	duk_push_boolean(eb->ctx, b->is_ok_check);
	duk_put_prop_string(eb->ctx, -2, "isCheck");

	duk_put_prop_string(eb->ctx, -2, "$ban");
	duk_pop(eb->ctx);

	/* Call handler */
	duk_push_global_stash(eb->ctx);
	if (duk_get_prop_string(eb->ctx, -1, eb->is_ok_handler))
	{
		if (duk_pcall(eb->ctx, 0) == 0)
		{
			int result = duk_to_boolean(eb->ctx, -1) ? 1 : 0;
			duk_pop_2(eb->ctx);
			return result;
		}
		const char *error = duk_safe_to_string(eb->ctx, -1);
		unreal_log(ULOG_ERROR, "obbyscript", "JS_EXTBAN_IS_OK_ERROR", NULL,
		           "Extban is_ok error: $error",
		           log_data_string("error", error));
		duk_pop_2(eb->ctx);
	}
	else
	{
		duk_pop(eb->ctx);
	}
	duk_pop(eb->ctx);

	return 1; /* Default accept on error */
}

/*
 * Extban conv_param handler - called to convert/validate ban parameters
 * Note: This is also used to set js_current_extban for is_banned callback
 */
const char *js_extban_conv_param(BanContext *b, Extban *extban)
{
	JSExtban *eb;
	static char retbuf[512];

	/* Find our extban handler */
	for (eb = js_extbans; eb; eb = eb->next)
	{
		if (eb->extban == extban)
			break;
	}

	/* Set current extban for use by is_banned and is_ok callbacks */
	js_current_extban = eb;

	if (!eb || !eb->conv_param_handler)
	{
		/* Default: return the banstr as-is (truncated to fit) */
		strlcpy(retbuf, b->banstr, sizeof(retbuf));
		return retbuf;
	}

	/* Set up context */
	duk_push_global_object(eb->ctx);

	/* Create $ban object */
	duk_push_object(eb->ctx);
	if (b->client)
	{
		js_push_client_object(eb->ctx, b->client);
		duk_put_prop_string(eb->ctx, -2, "client");
	}
	if (b->channel)
	{
		js_push_channel_object(eb->ctx, b->channel);
		duk_put_prop_string(eb->ctx, -2, "channel");
	}
	duk_push_string(eb->ctx, b->banstr);
	duk_put_prop_string(eb->ctx, -2, "banstr");

	duk_put_prop_string(eb->ctx, -2, "$ban");
	duk_pop(eb->ctx);

	/* Call handler */
	duk_push_global_stash(eb->ctx);
	if (duk_get_prop_string(eb->ctx, -1, eb->conv_param_handler))
	{
		if (duk_pcall(eb->ctx, 0) == 0)
		{
			if (duk_is_null_or_undefined(eb->ctx, -1))
			{
				duk_pop_2(eb->ctx);
				return NULL; /* Reject the ban */
			}
			const char *result = duk_to_string(eb->ctx, -1);
			strlcpy(retbuf, result, sizeof(retbuf));
			duk_pop_2(eb->ctx);
			return retbuf;
		}
		const char *error = duk_safe_to_string(eb->ctx, -1);
		unreal_log(ULOG_ERROR, "obbyscript", "JS_EXTBAN_CONV_PARAM_ERROR", NULL,
		           "Extban conv_param error: $error",
		           log_data_string("error", error));
		duk_pop_2(eb->ctx);
	}
	else
	{
		duk_pop(eb->ctx);
	}
	duk_pop(eb->ctx);

	/* Default: return banstr */
	strlcpy(retbuf, b->banstr, sizeof(retbuf));
	return retbuf;
}

/*
 * Extban is_banned handler - called to check if a client matches the ban
 */
int js_extban_is_banned(BanContext *b)
{
	JSExtban *eb = js_current_extban;

	if (!eb || !eb->is_banned_handler)
		return 0; /* Default: not banned */

	/* Set up context */
	duk_push_global_object(eb->ctx);

	/* Create $ban object */
	duk_push_object(eb->ctx);
	if (b->client)
	{
		js_push_client_object(eb->ctx, b->client);
		duk_put_prop_string(eb->ctx, -2, "client");
	}
	if (b->channel)
	{
		js_push_channel_object(eb->ctx, b->channel);
		duk_put_prop_string(eb->ctx, -2, "channel");
	}
	duk_push_string(eb->ctx, b->banstr);
	duk_put_prop_string(eb->ctx, -2, "banstr");
	duk_push_int(eb->ctx, b->ban_check_types);
	duk_put_prop_string(eb->ctx, -2, "checkType");

	duk_put_prop_string(eb->ctx, -2, "$ban");
	duk_pop(eb->ctx);

	/* Call handler */
	duk_push_global_stash(eb->ctx);
	if (duk_get_prop_string(eb->ctx, -1, eb->is_banned_handler))
	{
		if (duk_pcall(eb->ctx, 0) == 0)
		{
			int result = duk_to_boolean(eb->ctx, -1) ? 1 : 0;
			duk_pop_2(eb->ctx);
			return result;
		}
		const char *error = duk_safe_to_string(eb->ctx, -1);
		unreal_log(ULOG_ERROR, "obbyscript", "JS_EXTBAN_IS_BANNED_ERROR", NULL,
		           "Extban is_banned error: $error",
		           log_data_string("error", error));
		duk_pop_2(eb->ctx);
	}
	else
	{
		duk_pop(eb->ctx);
	}
	duk_pop(eb->ctx);

	return 0; /* Default: not banned on error */
}

/*
 * API: registerExtban(config)
 * Register a new extended ban type
 * config = {
 *   letter: 'x',               // Single character for the extban
 *   name: 'myextban',          // Name for the extban
 *   options: 0,                // Optional: EXTBOPT_* flags
 *   is_ok: function() {...},   // Optional: validation callback
 *   conv_param: function() {...}, // Optional: parameter conversion
 *   is_banned: function() {...}   // Required: check if banned
 * }
 */
duk_ret_t js_api_registerExtban(duk_context *ctx)
{
	JSExtban *eb;
	ExtbanInfo req;
	const char *name;
	const char *letter_str;
	char letter;
	char stash_key[128];

	if (!duk_is_object(ctx, 0))
	{
		duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "registerExtban requires an object parameter");
		return duk_throw(ctx);
	}

	/* Get letter */
	duk_get_prop_string(ctx, 0, "letter");
	if (!duk_is_string(ctx, -1))
	{
		duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "letter must be a string");
		return duk_throw(ctx);
	}
	letter_str = duk_get_string(ctx, -1);
	letter = letter_str[0];
	duk_pop(ctx);

	if (!letter)
	{
		duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "letter must not be empty");
		return duk_throw(ctx);
	}

	/* Get name */
	duk_get_prop_string(ctx, 0, "name");
	if (!duk_is_string(ctx, -1))
	{
		duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "name must be a string");
		return duk_throw(ctx);
	}
	name = duk_get_string(ctx, -1);
	duk_pop(ctx);

	/* Get is_banned handler (required) */
	duk_get_prop_string(ctx, 0, "is_banned");
	if (!duk_is_function(ctx, -1))
	{
		duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "is_banned handler is required and must be a function");
		return duk_throw(ctx);
	}

	/* Create our extban structure */
	eb = safe_alloc(sizeof(JSExtban));
	eb->letter = letter;
	safe_strdup(eb->name, name);
	eb->ctx = ctx;

	/* Store is_banned in stash */
	snprintf(stash_key, sizeof(stash_key), "extban_%c_is_banned", letter);
	duk_push_global_stash(ctx);
	duk_dup(ctx, -2);
	duk_put_prop_string(ctx, -2, stash_key);
	duk_pop_2(ctx);
	safe_strdup(eb->is_banned_handler, stash_key);

	/* Get optional is_ok handler */
	duk_get_prop_string(ctx, 0, "is_ok");
	if (duk_is_function(ctx, -1))
	{
		snprintf(stash_key, sizeof(stash_key), "extban_%c_is_ok", letter);
		duk_push_global_stash(ctx);
		duk_dup(ctx, -2);
		duk_put_prop_string(ctx, -2, stash_key);
		duk_pop(ctx);
		safe_strdup(eb->is_ok_handler, stash_key);
	}
	duk_pop(ctx);

	/* Get optional conv_param handler */
	duk_get_prop_string(ctx, 0, "conv_param");
	if (duk_is_function(ctx, -1))
	{
		snprintf(stash_key, sizeof(stash_key), "extban_%c_conv_param", letter);
		duk_push_global_stash(ctx);
		duk_dup(ctx, -2);
		duk_put_prop_string(ctx, -2, stash_key);
		duk_pop(ctx);
		safe_strdup(eb->conv_param_handler, stash_key);
	}
	duk_pop(ctx);

	/* Get optional options */
	duk_get_prop_string(ctx, 0, "options");
	if (duk_is_number(ctx, -1))
		eb->options = duk_get_int(ctx, -1);
	duk_pop(ctx);

	/* Register with UnrealIRCd */
	memset(&req, 0, sizeof(req));
	req.letter = letter;
	req.name = eb->name;
	req.options = eb->options;
	req.is_ok = eb->is_ok_handler ? js_extban_is_ok : NULL;
	/* Always register conv_param so we can set js_current_extban for is_banned/is_ok */
	req.conv_param = js_extban_conv_param;
	req.is_banned = js_extban_is_banned;
	req.is_banned_events = BANCHK_ALL;

	eb->extban = ExtbanAdd(js_modinfo->handle, req);
	if (!eb->extban)
	{
		safe_free(eb->name);
		safe_free(eb->is_ok_handler);
		safe_free(eb->conv_param_handler);
		safe_free(eb->is_banned_handler);
		safe_free(eb);
		duk_push_error_object(ctx, DUK_ERR_ERROR, "Failed to register extban");
		return duk_throw(ctx);
	}

	/* Add to our list */
	AddListItem(eb, js_extbans);

	unreal_log(ULOG_INFO, "obbyscript", "JS_EXTBAN_REGISTERED", NULL,
	           "JavaScript registered extban: ~$letter:$name",
	           log_data_char("letter", letter),
	           log_data_string("name", name));

	duk_push_true(ctx);
	return 1;
}

/* =========================================================================
 * MESSAGE TAG HANDLER API FUNCTIONS
 * ========================================================================= */

/*
 * Message tag is_ok handler - called to validate if a client can send this tag
 */
int js_mtag_is_ok(Client *client, const char *name, const char *value)
{
	JSMessageTag *mt;

	/* Find our handler by tag name */
	for (mt = js_mtag_handlers; mt; mt = mt->next)
	{
		if (!strcasecmp(mt->name, name))
			break;
	}

	if (!mt || !mt->is_ok_handler)
		return 0; /* Default: deny if no handler */

	/* Set up context */
	duk_push_global_object(mt->ctx);

	/* Set $client */
	js_push_client_object(mt->ctx, client);
	duk_put_prop_string(mt->ctx, -2, "$client");

	/* Set $tagName and $tagValue */
	duk_push_string(mt->ctx, name ? name : "");
	duk_put_prop_string(mt->ctx, -2, "$tagName");
	
	if (value)
		duk_push_string(mt->ctx, value);
	else
		duk_push_null(mt->ctx);
	duk_put_prop_string(mt->ctx, -2, "$tagValue");

	duk_pop(mt->ctx);

	/* Call handler */
	duk_push_global_stash(mt->ctx);
	if (duk_get_prop_string(mt->ctx, -1, mt->is_ok_handler))
	{
		if (duk_pcall(mt->ctx, 0) == 0)
		{
			int result = duk_to_boolean(mt->ctx, -1) ? 1 : 0;
			duk_pop_2(mt->ctx);
			return result;
		}
		const char *error = duk_safe_to_string(mt->ctx, -1);
		unreal_log(ULOG_ERROR, "obbyscript", "JS_MTAG_IS_OK_ERROR", NULL,
		           "Message tag is_ok error: $error",
		           log_data_string("error", error));
		duk_pop_2(mt->ctx);
	}
	else
	{
		duk_pop(mt->ctx);
	}
	duk_pop(mt->ctx);

	return 0; /* Default deny on error */
}

/*
 * Message tag should_send_to_client handler - called to check if tag should be sent to a client
 */
int js_mtag_should_send_to_client(Client *target)
{
	JSMessageTag *mt = js_current_mtag;

	if (!mt || !mt->should_send_handler)
		return 1; /* Default: send to everyone */

	/* Set up context */
	duk_push_global_object(mt->ctx);

	/* Set $target (the client who might receive the tag) */
	js_push_client_object(mt->ctx, target);
	duk_put_prop_string(mt->ctx, -2, "$target");

	duk_pop(mt->ctx);

	/* Call handler */
	duk_push_global_stash(mt->ctx);
	if (duk_get_prop_string(mt->ctx, -1, mt->should_send_handler))
	{
		if (duk_pcall(mt->ctx, 0) == 0)
		{
			int result = duk_to_boolean(mt->ctx, -1) ? 1 : 0;
			duk_pop_2(mt->ctx);
			return result;
		}
		const char *error = duk_safe_to_string(mt->ctx, -1);
		unreal_log(ULOG_ERROR, "obbyscript", "JS_MTAG_SHOULD_SEND_ERROR", NULL,
		           "Message tag should_send_to_client error: $error",
		           log_data_string("error", error));
		duk_pop_2(mt->ctx);
	}
	else
	{
		duk_pop(mt->ctx);
	}
	duk_pop(mt->ctx);

	return 1; /* Default send on error */
}

/*
 * API: registerMessageTag(config)
 * Register a new message tag handler
 * config = {
 *   name: '+draft/myapp',      // Tag name (usually +draft/xxx for custom tags)
 *   flags: 0,                  // Optional: MTAG_HANDLER_FLAGS_NO_CAP_NEEDED
 *   is_ok: function() {...},   // Optional: validate if client can send this tag
 *   should_send_to_client: function() {...}  // Optional: filter which clients receive the tag
 * }
 */
duk_ret_t js_api_registerMessageTag(duk_context *ctx)
{
	JSMessageTag *mt;
	MessageTagHandlerInfo req;
	const char *name;
	char stash_key[128];

	if (!duk_is_object(ctx, 0))
	{
		duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "registerMessageTag requires an object parameter");
		return duk_throw(ctx);
	}

	/* Get tag name */
	duk_get_prop_string(ctx, 0, "name");
	if (!duk_is_string(ctx, -1))
	{
		duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "name must be a string");
		return duk_throw(ctx);
	}
	name = duk_get_string(ctx, -1);
	duk_pop(ctx);

	/* Create our handler structure */
	mt = safe_alloc(sizeof(JSMessageTag));
	safe_strdup(mt->name, name);
	mt->ctx = ctx;

	/* Get optional flags */
	duk_get_prop_string(ctx, 0, "flags");
	if (duk_is_number(ctx, -1))
		mt->flags = duk_get_int(ctx, -1);
	else
		mt->flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED; /* Default: no CAP needed */
	duk_pop(ctx);

	/* Get optional is_ok handler */
	duk_get_prop_string(ctx, 0, "is_ok");
	if (duk_is_function(ctx, -1))
	{
		snprintf(stash_key, sizeof(stash_key), "mtag_%s_is_ok", name);
		duk_push_global_stash(ctx);
		duk_dup(ctx, -2);
		duk_put_prop_string(ctx, -2, stash_key);
		duk_pop(ctx);
		safe_strdup(mt->is_ok_handler, stash_key);
	}
	duk_pop(ctx);

	/* Note: should_send_to_client is not supported - see comment below */

	/* Register with UnrealIRCd */
	memset(&req, 0, sizeof(req));
	req.name = mt->name;
	req.flags = mt->flags;
	req.is_ok = mt->is_ok_handler ? js_mtag_is_ok : NULL;
	/* Note: should_send_to_client is not supported for JS handlers because
	 * the callback doesn't receive the tag name, making it impossible to
	 * identify which JS handler should process the call. Most tags don't
	 * need this filter anyway. */
	req.should_send_to_client = NULL;

	mt->handler = MessageTagHandlerAdd(js_modinfo->handle, &req);
	if (!mt->handler)
	{
		safe_free(mt->name);
		safe_free(mt->is_ok_handler);
		safe_free(mt->should_send_handler);
		safe_free(mt);
		duk_push_error_object(ctx, DUK_ERR_ERROR, "Failed to register message tag handler");
		return duk_throw(ctx);
	}

	/* Set current mtag for callbacks - this is needed for the shared callback functions */
	js_current_mtag = mt;

	/* Add to our list */
	AddListItem(mt, js_mtag_handlers);

	unreal_log(ULOG_INFO, "obbyscript", "JS_MTAG_REGISTERED", NULL,
	           "JavaScript registered message tag: $name",
	           log_data_string("name", name));

	duk_push_true(ctx);
	return 1;
}

/*
 * Config error reporting from JavaScript
 */
duk_ret_t js_api_config_error(duk_context *ctx)
{
	const char *format, *file;
	int line;

	if (!duk_is_string(ctx, 0))
	{
		duk_push_false(ctx);
		return 1;
	}

	format = duk_to_string(ctx, 0);
	
	/* Try to get $file and $line from global scope */
	duk_push_global_object(ctx);
	duk_get_prop_string(ctx, -1, "$configFile");
	file = duk_get_string(ctx, -1);
	duk_pop(ctx);
	
	duk_get_prop_string(ctx, -1, "$configLine");
	line = duk_get_int(ctx, -1);
	duk_pop_2(ctx);
	
	if (file && line > 0)
		config_error("%s:%d: %s", file, line, format);
	else
		config_error("%s", format);

	duk_push_true(ctx);
	return 1;
}

duk_ret_t js_api_config_warn(duk_context *ctx)
{
	const char *format, *file;
	int line;

	if (!duk_is_string(ctx, 0))
	{
		duk_push_false(ctx);
		return 1;
	}

	format = duk_to_string(ctx, 0);
	
	/* Try to get $file and $line from global scope */
	duk_push_global_object(ctx);
	duk_get_prop_string(ctx, -1, "$configFile");
	file = duk_get_string(ctx, -1);
	duk_pop(ctx);
	
	duk_get_prop_string(ctx, -1, "$configLine");
	line = duk_get_int(ctx, -1);
	duk_pop_2(ctx);
	
	if (file && line > 0)
		config_warn("%s:%d: %s", file, line, format);
	else
		config_warn("%s", format);

	duk_push_true(ctx);
	return 1;
}

/*
 * Helper: Push ConfigEntry as JavaScript object
 */
static void js_push_config_entry(duk_context *ctx, ConfigEntry *ce)
{
	ConfigEntry *child;
	int idx;

	if (!ce)
	{
		duk_push_null(ctx);
		return;
	}

	duk_push_object(ctx);
	
	/* Set name */
	if (ce->name)
	{
		duk_push_string(ctx, ce->name);
		duk_put_prop_string(ctx, -2, "name");
	}
	
	/* Set value */
	if (ce->value)
	{
		duk_push_string(ctx, ce->value);
		duk_put_prop_string(ctx, -2, "value");
	}
	
	/* Set file and line */
	if (ce->file && ce->file->filename)
	{
		duk_push_string(ctx, ce->file->filename);
		duk_put_prop_string(ctx, -2, "file");
	}
	
	duk_push_int(ctx, ce->line_number);
	duk_put_prop_string(ctx, -2, "line");
	
	/* Set items array (children) */
	if (ce->items)
	{
		duk_push_array(ctx);
		idx = 0;
		for (child = ce->items; child; child = child->next)
		{
			js_push_config_entry(ctx, child);
			duk_put_prop_index(ctx, -2, idx++);
		}
		duk_put_prop_string(ctx, -2, "items");
	}
}

/*
 * Register a config block handler
 */
duk_ret_t js_api_registerConfigBlock(duk_context *ctx)
{
	JSConfigBlock *cb;
	const char *blockname;
	char stash_key[256];

	if (!duk_is_object(ctx, 0))
	{
		duk_push_false(ctx);
		return 1;
	}

	/* Get block name */
	duk_get_prop_string(ctx, 0, "name");
	if (!duk_is_string(ctx, -1))
	{
		duk_pop(ctx);
		duk_push_false(ctx);
		return 1;
	}
	blockname = duk_to_string(ctx, -1);
	duk_pop(ctx);

	/* Check if already registered */
	for (cb = js_config_blocks; cb; cb = cb->next)
	{
		if (!strcmp(cb->blockname, blockname))
		{
			duk_push_false(ctx);
			return 1;
		}
	}

	/* Create config block handler */
	cb = safe_alloc(sizeof(JSConfigBlock));
	safe_strdup(cb->blockname, blockname);
	cb->ctx = ctx;

	/* Store test handler in global stash */
	duk_get_prop_string(ctx, 0, "test");
	if (duk_is_function(ctx, -1))
	{
		snprintf(stash_key, sizeof(stash_key), "config_%s_test", blockname);
		duk_push_global_stash(ctx);
		duk_dup(ctx, -2);
		duk_put_prop_string(ctx, -2, stash_key);
		duk_pop(ctx);
		safe_strdup(cb->test_handler, stash_key);
	}
	duk_pop(ctx);

	/* Store run handler in global stash */
	duk_get_prop_string(ctx, 0, "run");
	if (duk_is_function(ctx, -1))
	{
		snprintf(stash_key, sizeof(stash_key), "config_%s_run", blockname);
		duk_push_global_stash(ctx);
		duk_dup(ctx, -2);
		duk_put_prop_string(ctx, -2, stash_key);
		duk_pop(ctx);
		safe_strdup(cb->run_handler, stash_key);
	}
	duk_pop(ctx);

	/* Add to list */
	AddListItem(cb, js_config_blocks);

	unreal_log(ULOG_INFO, "obbyscript", "JS_CONFIG_REGISTERED", NULL,
	           "JavaScript registered config block: $name",
	           log_data_string("name", blockname));

	duk_push_true(ctx);
	return 1;
}

/*
 * Cleanup function
 */
void js_cleanup(void)
{
	JSCommand *jscmd, *jscmd_next;
	JSHook *jshook, *jshook_next;
	JSTimer *timer, *timer_next;
	JSChannelMode *jscm, *jscm_next;
	JSPrefixMode *jspm, *jspm_next;
	JSUserMode *jsum, *jsum_next;
	JSModData *jsmd, *jsmd_next;
	JSRPCHandler *jsrpc, *jsrpc_next;
	JSExtban *jseb, *jseb_next;
	JSMessageTag *jsmt, *jsmt_next;
	JSConfigBlock *jscb, *jscb_next;

	/* Clean up commands */
	for (jscmd = js_commands; jscmd; jscmd = jscmd_next)
	{
		jscmd_next = jscmd->next;
		safe_free(jscmd->name);
		safe_free(jscmd->handler_code);
		safe_free(jscmd);
	}
	js_commands = NULL;

	/* Clean up hooks */
	for (jshook = js_hooks; jshook; jshook = jshook_next)
	{
		jshook_next = jshook->next;
		safe_free(jshook->handler_code);
		safe_free(jshook);
	}
	js_hooks = NULL;

	/* Clean up timers */
	for (timer = js_timers; timer; timer = timer_next)
	{
		timer_next = timer->next;
		if (timer->event)
			EventDel(timer->event);
		safe_free(timer->handler_code);
		safe_free(timer);
	}
	js_timers = NULL;

	/* Clean up channel modes */
	for (jscm = js_channelmodes; jscm; jscm = jscm_next)
	{
		jscm_next = jscm->next;
		safe_free(jscm->name);
		safe_free(jscm->is_ok_handler);
		safe_free(jscm);
	}
	js_channelmodes = NULL;

	/* Clean up prefix modes */
	for (jspm = js_prefixmodes; jspm; jspm = jspm_next)
	{
		jspm_next = jspm->next;
		safe_free(jspm->name);
		safe_free(jspm->is_ok_handler);
		safe_free(jspm);
	}
	js_prefixmodes = NULL;

	/* Clean up user modes */
	for (jsum = js_usermodes; jsum; jsum = jsum_next)
	{
		jsum_next = jsum->next;
		safe_free(jsum->name);
		safe_free(jsum->allowed_handler);
		safe_free(jsum);
	}
	js_usermodes = NULL;

	/* Clean up moddata */
	for (jsmd = js_moddatas; jsmd; jsmd = jsmd_next)
	{
		jsmd_next = jsmd->next;
		safe_free(jsmd->name);
		safe_free(jsmd);
	}
	js_moddatas = NULL;

	/* Clean up RPC handlers */
	for (jsrpc = js_rpc_handlers; jsrpc; jsrpc = jsrpc_next)
	{
		jsrpc_next = jsrpc->next;
		safe_free(jsrpc->method);
		safe_free(jsrpc->handler_code);
		safe_free(jsrpc);
	}
	js_rpc_handlers = NULL;

	/* Clean up extbans */
	for (jseb = js_extbans; jseb; jseb = jseb_next)
	{
		jseb_next = jseb->next;
		safe_free(jseb->name);
		safe_free(jseb->is_ok_handler);
		safe_free(jseb->conv_param_handler);
		safe_free(jseb->is_banned_handler);
		safe_free(jseb);
	}
	js_extbans = NULL;

	/* Clean up message tag handlers */
	for (jsmt = js_mtag_handlers; jsmt; jsmt = jsmt_next)
	{
		jsmt_next = jsmt->next;
		safe_free(jsmt->name);
		safe_free(jsmt->is_ok_handler);
		safe_free(jsmt->should_send_handler);
		safe_free(jsmt);
	}
	js_mtag_handlers = NULL;

	/* Clean up config block handlers */
	for (jscb = js_config_blocks; jscb; jscb = jscb_next)
	{
		jscb_next = jscb->next;
		safe_free(jscb->blockname);
		safe_free(jscb->test_handler);
		safe_free(jscb->run_handler);
		safe_free(jscb);
	}
	js_config_blocks = NULL;

	/* Clean up open database handles */
	{
		JSDatabase *jsdb, *jsdb_next;
		for (jsdb = js_databases; jsdb; jsdb = jsdb_next)
		{
			jsdb_next = jsdb->next;
			if (jsdb->db)
				unrealdb_close(jsdb->db);
			safe_free(jsdb->filename);
			safe_free(jsdb);
		}
		js_databases = NULL;
	}

	/* Clear hook registration tracking */
	memset(js_registered_hooks, 0, sizeof(js_registered_hooks));

	/* Destroy Duktape context */
	if (global_ctx)
	{
		duk_destroy_heap(global_ctx);
		global_ctx = NULL;
	}
}

/* Configuration test */
int js_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	JSConfigBlock *cb;
	int errors = 0;
	int result;

	if (type != CONFIG_MAIN)
		return 0;

	/* Find matching config block handler */
	for (cb = js_config_blocks; cb; cb = cb->next)
	{
		if (!strcmp(cb->blockname, ce->name) && cb->test_handler)
		{
			/* Set up global variables */
			duk_push_global_object(cb->ctx);
			
			/* $config - the config entry */
			js_push_config_entry(cb->ctx, ce);
			duk_put_prop_string(cb->ctx, -2, "$config");
			
			/* $configFile and $configLine for error reporting */
			if (ce->file && ce->file->filename)
			{
				duk_push_string(cb->ctx, ce->file->filename);
				duk_put_prop_string(cb->ctx, -2, "$configFile");
			}
			duk_push_int(cb->ctx, ce->line_number);
			duk_put_prop_string(cb->ctx, -2, "$configLine");
			
			duk_pop(cb->ctx);

			/* Get and call test handler */
			duk_push_global_stash(cb->ctx);
			duk_get_prop_string(cb->ctx, -1, cb->test_handler);
			
			if (duk_pcall(cb->ctx, 0) != 0)
			{
				unreal_log(ULOG_ERROR, "obbyscript", "JS_CONFIG_TEST_ERROR", NULL,
				           "Error in config test handler for '$blockname': $error",
				           log_data_string("blockname", cb->blockname),
				           log_data_string("error", duk_safe_to_string(cb->ctx, -1)));
				errors++;
				duk_pop_2(cb->ctx);
				*errs = errors;
				return -1;
			}
			
			/* Check return value */
			result = duk_get_boolean(cb->ctx, -1);
			duk_pop_2(cb->ctx);
			
			if (!result)
				errors++;
			
			*errs = errors;
			return errors ? -1 : 1;
		}
	}

	return 0;  /* Not handled by us */
}

/* Configuration run */
int js_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	JSConfigBlock *cb;

	if (type != CONFIG_MAIN)
		return 0;

	/* Find matching config block handler */
	for (cb = js_config_blocks; cb; cb = cb->next)
	{
		if (!strcmp(cb->blockname, ce->name) && cb->run_handler)
		{
			/* Set up global variables */
			duk_push_global_object(cb->ctx);
			
			/* $config - the config entry */
			js_push_config_entry(cb->ctx, ce);
			duk_put_prop_string(cb->ctx, -2, "$config");
			
			/* $configFile and $configLine for error reporting */
			if (ce->file && ce->file->filename)
			{
				duk_push_string(cb->ctx, ce->file->filename);
				duk_put_prop_string(cb->ctx, -2, "$configFile");
			}
			duk_push_int(cb->ctx, ce->line_number);
			duk_put_prop_string(cb->ctx, -2, "$configLine");
			
			duk_pop(cb->ctx);

			/* Get and call run handler */
			duk_push_global_stash(cb->ctx);
			duk_get_prop_string(cb->ctx, -1, cb->run_handler);
			
			if (duk_pcall(cb->ctx, 0) != 0)
			{
				unreal_log(ULOG_ERROR, "obbyscript", "JS_CONFIG_RUN_ERROR", NULL,
				           "Error in config run handler for '$blockname': $error",
				           log_data_string("blockname", cb->blockname),
				           log_data_string("error", duk_safe_to_string(cb->ctx, -1)));
				duk_pop_2(cb->ctx);
				return 0;
			}
			
			duk_pop_2(cb->ctx);
			return 1;
		}
	}

	return 0;  /* Not handled by us */
}

/* =========================================================================
 * UnrealDB DATABASE API FUNCTIONS
 * ========================================================================= */

/*
 * Helper function to find a database by ID
 */
static JSDatabase *js_find_database(int id)
{
	JSDatabase *db;
	for (db = js_databases; db; db = db->next)
	{
		if (db->id == id)
			return db;
	}
	return NULL;
}

/*
 * API: dbOpen(filename, mode, secret_block)
 * Opens a database file for reading or writing.
 * 
 * filename: Path to database file (will be prefixed with data/ if not absolute)
 * mode: DB_READ or DB_WRITE
 * secret_block: Name of secret block for encryption, or null for unencrypted
 * 
 * Returns: Database handle ID on success, or null on failure
 * Use dbGetError() to get the error message on failure.
 */
duk_ret_t js_api_dbOpen(duk_context *ctx)
{
	const char *filename;
	int mode;
	const char *secret_block = NULL;
	UnrealDB *db;
	JSDatabase *jsdb;
	char path[512];

	filename = duk_require_string(ctx, 0);
	mode = duk_require_int(ctx, 1);

	if (duk_is_string(ctx, 2))
		secret_block = duk_get_string(ctx, 2);

	/* Validate mode */
	if (mode != UNREALDB_MODE_READ && mode != UNREALDB_MODE_WRITE)
	{
		duk_push_null(ctx);
		return 1;
	}

	/* Security: Only allow files in data/ directory */
	if (filename[0] == '/' || strstr(filename, "..") != NULL)
	{
		unreal_log(ULOG_ERROR, "obbyscript", "JS_DB_SECURITY", NULL,
		           "Database open denied: path must be relative without '..' ($filename)",
		           log_data_string("filename", filename));
		duk_push_null(ctx);
		return 1;
	}

	/* Construct path in data directory */
	snprintf(path, sizeof(path), "%s/%s", PERMDATADIR, filename);

	/* Open the database */
	db = unrealdb_open(path, (UnrealDBMode)mode, (char *)secret_block);
	if (!db)
	{
		duk_push_null(ctx);
		return 1;
	}

	/* Create tracking structure */
	jsdb = safe_alloc(sizeof(JSDatabase));
	jsdb->id = js_database_id_counter++;
	jsdb->db = db;
	jsdb->mode = mode;
	safe_strdup(jsdb->filename, filename);

	/* Add to list */
	AddListItem(jsdb, js_databases);

	unreal_log(ULOG_DEBUG, "obbyscript", "JS_DB_OPEN", NULL,
	           "Database opened: $filename (mode=$mode, id=$id)",
	           log_data_string("filename", filename),
	           log_data_integer("mode", mode),
	           log_data_integer("id", jsdb->id));

	duk_push_int(ctx, jsdb->id);
	return 1;
}

/*
 * API: dbClose(handle)
 * Closes an open database handle.
 * 
 * handle: Database handle ID returned by dbOpen()
 * 
 * Returns: true on success, false on failure
 */
duk_ret_t js_api_dbClose(duk_context *ctx)
{
	int id;
	JSDatabase *jsdb;
	int result;

	id = duk_require_int(ctx, 0);
	
	jsdb = js_find_database(id);
	if (!jsdb)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	result = unrealdb_close(jsdb->db);

	/* Remove from list and free */
	DelListItem(jsdb, js_databases);
	safe_free(jsdb->filename);
	safe_free(jsdb);

	duk_push_boolean(ctx, result);
	return 1;
}

/*
 * API: dbWriteInt64(handle, value)
 * Writes a 64-bit integer to the database.
 * 
 * Returns: true on success, false on failure
 */
duk_ret_t js_api_dbWriteInt64(duk_context *ctx)
{
	int id;
	JSDatabase *jsdb;
	double value;
	uint64_t int_value;
	int result;

	id = duk_require_int(ctx, 0);
	value = duk_require_number(ctx, 1);
	int_value = (uint64_t)value;

	jsdb = js_find_database(id);
	if (!jsdb || jsdb->mode != UNREALDB_MODE_WRITE)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	result = unrealdb_write_int64(jsdb->db, int_value);
	duk_push_boolean(ctx, result);
	return 1;
}

/*
 * API: dbWriteInt32(handle, value)
 * Writes a 32-bit integer to the database.
 * 
 * Returns: true on success, false on failure
 */
duk_ret_t js_api_dbWriteInt32(duk_context *ctx)
{
	int id;
	JSDatabase *jsdb;
	int value;
	uint32_t int_value;
	int result;

	id = duk_require_int(ctx, 0);
	value = duk_require_int(ctx, 1);
	int_value = (uint32_t)value;

	jsdb = js_find_database(id);
	if (!jsdb || jsdb->mode != UNREALDB_MODE_WRITE)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	result = unrealdb_write_int32(jsdb->db, int_value);
	duk_push_boolean(ctx, result);
	return 1;
}

/*
 * API: dbWriteInt16(handle, value)
 * Writes a 16-bit integer to the database.
 * 
 * Returns: true on success, false on failure
 */
duk_ret_t js_api_dbWriteInt16(duk_context *ctx)
{
	int id;
	JSDatabase *jsdb;
	int value;
	uint16_t int_value;
	int result;

	id = duk_require_int(ctx, 0);
	value = duk_require_int(ctx, 1);
	int_value = (uint16_t)value;

	jsdb = js_find_database(id);
	if (!jsdb || jsdb->mode != UNREALDB_MODE_WRITE)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	result = unrealdb_write_int16(jsdb->db, int_value);
	duk_push_boolean(ctx, result);
	return 1;
}

/*
 * API: dbWriteStr(handle, value)
 * Writes a string to the database.
 * 
 * value: String to write (can be null)
 * 
 * Returns: true on success, false on failure
 */
duk_ret_t js_api_dbWriteStr(duk_context *ctx)
{
	int id;
	JSDatabase *jsdb;
	const char *value = NULL;
	int result;

	id = duk_require_int(ctx, 0);
	
	if (!duk_is_null_or_undefined(ctx, 1))
		value = duk_require_string(ctx, 1);

	jsdb = js_find_database(id);
	if (!jsdb || jsdb->mode != UNREALDB_MODE_WRITE)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	result = unrealdb_write_str(jsdb->db, value);
	duk_push_boolean(ctx, result);
	return 1;
}

/*
 * API: dbWriteChar(handle, value)
 * Writes a single character (byte) to the database.
 * 
 * value: Single character string or integer
 * 
 * Returns: true on success, false on failure
 */
duk_ret_t js_api_dbWriteChar(duk_context *ctx)
{
	int id;
	JSDatabase *jsdb;
	char value;
	int result;

	id = duk_require_int(ctx, 0);
	
	if (duk_is_string(ctx, 1))
	{
		const char *str = duk_get_string(ctx, 1);
		value = str[0];
	}
	else
	{
		value = (char)duk_require_int(ctx, 1);
	}

	jsdb = js_find_database(id);
	if (!jsdb || jsdb->mode != UNREALDB_MODE_WRITE)
	{
		duk_push_boolean(ctx, 0);
		return 1;
	}

	result = unrealdb_write_char(jsdb->db, value);
	duk_push_boolean(ctx, result);
	return 1;
}

/*
 * API: dbReadInt64(handle)
 * Reads a 64-bit integer from the database.
 * 
 * Returns: The integer value on success, or null on failure
 */
duk_ret_t js_api_dbReadInt64(duk_context *ctx)
{
	int id;
	JSDatabase *jsdb;
	uint64_t value;
	int result;

	id = duk_require_int(ctx, 0);

	jsdb = js_find_database(id);
	if (!jsdb || jsdb->mode != UNREALDB_MODE_READ)
	{
		duk_push_null(ctx);
		return 1;
	}

	result = unrealdb_read_int64(jsdb->db, &value);
	if (!result)
	{
		duk_push_null(ctx);
		return 1;
	}

	duk_push_number(ctx, (double)value);
	return 1;
}

/*
 * API: dbReadInt32(handle)
 * Reads a 32-bit integer from the database.
 * 
 * Returns: The integer value on success, or null on failure
 */
duk_ret_t js_api_dbReadInt32(duk_context *ctx)
{
	int id;
	JSDatabase *jsdb;
	uint32_t value;
	int result;

	id = duk_require_int(ctx, 0);

	jsdb = js_find_database(id);
	if (!jsdb || jsdb->mode != UNREALDB_MODE_READ)
	{
		duk_push_null(ctx);
		return 1;
	}

	result = unrealdb_read_int32(jsdb->db, &value);
	if (!result)
	{
		duk_push_null(ctx);
		return 1;
	}

	duk_push_int(ctx, (int)value);
	return 1;
}

/*
 * API: dbReadInt16(handle)
 * Reads a 16-bit integer from the database.
 * 
 * Returns: The integer value on success, or null on failure
 */
duk_ret_t js_api_dbReadInt16(duk_context *ctx)
{
	int id;
	JSDatabase *jsdb;
	uint16_t value;
	int result;

	id = duk_require_int(ctx, 0);

	jsdb = js_find_database(id);
	if (!jsdb || jsdb->mode != UNREALDB_MODE_READ)
	{
		duk_push_null(ctx);
		return 1;
	}

	result = unrealdb_read_int16(jsdb->db, &value);
	if (!result)
	{
		duk_push_null(ctx);
		return 1;
	}

	duk_push_int(ctx, (int)value);
	return 1;
}

/*
 * API: dbReadStr(handle)
 * Reads a string from the database.
 * 
 * Returns: The string value on success, or null on failure/if stored as null
 */
duk_ret_t js_api_dbReadStr(duk_context *ctx)
{
	int id;
	JSDatabase *jsdb;
	char *value = NULL;
	int result;

	id = duk_require_int(ctx, 0);

	jsdb = js_find_database(id);
	if (!jsdb || jsdb->mode != UNREALDB_MODE_READ)
	{
		duk_push_null(ctx);
		return 1;
	}

	result = unrealdb_read_str(jsdb->db, &value);
	if (!result)
	{
		duk_push_null(ctx);
		return 1;
	}

	if (value)
	{
		duk_push_string(ctx, value);
		safe_free(value);
	}
	else
	{
		duk_push_null(ctx);
	}

	return 1;
}

/*
 * API: dbReadChar(handle)
 * Reads a single character (byte) from the database.
 * 
 * Returns: The character as a single-character string on success, or null on failure
 */
duk_ret_t js_api_dbReadChar(duk_context *ctx)
{
	int id;
	JSDatabase *jsdb;
	char value;
	char str[2];
	int result;

	id = duk_require_int(ctx, 0);

	jsdb = js_find_database(id);
	if (!jsdb || jsdb->mode != UNREALDB_MODE_READ)
	{
		duk_push_null(ctx);
		return 1;
	}

	result = unrealdb_read_char(jsdb->db, &value);
	if (!result)
	{
		duk_push_null(ctx);
		return 1;
	}

	str[0] = value;
	str[1] = '\0';
	duk_push_string(ctx, str);
	return 1;
}

/*
 * API: dbGetError()
 * Gets the last database error information.
 * 
 * Returns: Object with 'code' and 'message' properties
 */
duk_ret_t js_api_dbGetError(duk_context *ctx)
{
	UnrealDBError code;
	const char *message;
	duk_idx_t obj_idx;

	code = unrealdb_get_error_code();
	message = unrealdb_get_error_string();

	obj_idx = duk_push_object(ctx);

	duk_push_int(ctx, (int)code);
	duk_put_prop_string(ctx, obj_idx, "code");

	if (message)
		duk_push_string(ctx, message);
	else
		duk_push_null(ctx);
	duk_put_prop_string(ctx, obj_idx, "message");

	return 1;
}

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, js_configtest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	js_modinfo = modinfo;
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, js_configrun);
	
	/* Register HTTP callback for async requests */
	RegisterApiCallbackWebResponse(modinfo->handle, "js_http_callback", js_http_callback);
	
	js_init_engine();
	
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	js_load_scripts();
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	js_cleanup();
	return MOD_SUCCESS;
}
