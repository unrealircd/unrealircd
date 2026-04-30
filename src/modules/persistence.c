/*
 * src/modules/persistence.c
 * IRCv3 draft/persistence extension for ObbyIRCd
 *
 * Spec: https://github.com/slingamn/ircv3-specifications/blob/b016f41d/extensions/persistence.md
 *
 * Behavior:
 *   - Authenticated users with persistence ON remain present in channels
 *     after disconnection via a ghost client (visible in WHO/NAMES as away).
 *   - On reconnect with the same services account, the new connection
 *     takes over the ghost's channels (server sends JOIN after 376/422).
 *   - State is saved to unrealdb so ghost memberships survive restarts.
 *   - An inactivity timeout (default 7 days) removes stale ghosts.
 *   - Multi-session: multiple connections may share the same IRC presence.
 *     Extra sessions receive channel traffic via relay; outgoing commands
 *     are proxied through the canonical client.  Ghost is created only
 *     when ALL sessions disconnect.
 *
 * Config (obbyircd.conf):
 *   persistence {
 *       timeout 7d;
 *       default on;
 *   };
 *
 * CAP:     draft/persistence
 * Command: PERSISTENCE GET | SET ON|OFF|DEFAULT
 * Server:  PERSISTENCE STATUS <client-setting> <effective-setting>
 */

#include "unrealircd.h"

/* ===================================================================
 * Module header
 * =================================================================== */
ModuleHeader MOD_HEADER = {
"persistence",
"1.0",
"draft/persistence - IRCv3 ghost client persistence + multi-session",
"ObbyIRCd Team",
"unrealircd-6",
};

/* ===================================================================
 * Constants
 * =================================================================== */
#define PERSIST_MAX_CHANNELS        100
#define PERSIST_DEFAULT_TIMEOUT     (7L * 86400L)
#define PERSIST_DB_VERSION          1
#define PERSIST_CLEANUP_INTERVAL_MS 60000

#define PREF_DEFAULT  (-1)
#define PREF_OFF        0
#define PREF_ON         1

/* ===================================================================
 * Data structures
 * =================================================================== */

typedef struct {
char name[CHANNELLEN + 1];
char modes[MEMBERMODESLEN];
} PersistChannel;

/* An extra session client attached to a canonical IRC presence. */
typedef struct PersistSession_ {
Client *client;
struct PersistSession_ *next;
} PersistSession;

typedef struct PersistEntry_ {
char account[ACCOUNTLEN + 1];
char nick[NICKLEN + 1];
char ident[USERLEN + 1];
char host[HOSTLEN + 1];
char realhost[HOSTLEN + 1];
char cloakedhost[HOSTLEN + 1];
char ip[64];
char realname[REALLEN + 1];
long umodes;
char *away;
time_t disconnect_time;
int num_channels;
PersistChannel channels[PERSIST_MAX_CHANNELS];
Client *ghost;            /* offline ghost placeholder (fd=-2)     */
Client *canonical;        /* live primary client (NULL if offline)  */
PersistSession *sessions; /* extra attached session clients         */
int num_sessions;
int preference;
struct PersistEntry_ *prev;
struct PersistEntry_ *next;
} PersistEntry;

/* ===================================================================
 * Module globals
 * =================================================================== */
static PersistEntry       *persist_list    = NULL;
static ModDataInfo        *ghost_md        = NULL;
static ModDataInfo        *restore_md      = NULL;
static ModDataInfo        *session_md      = NULL;
static long                CAP_PERSISTENCE = 0L;
static long                away_notify_cap = 0L;
static long                persist_timeout    = PERSIST_DEFAULT_TIMEOUT;
static int                 persist_default_on = 1;

/* Command overrides for session clients */
static CommandOverride *ovr_privmsg = NULL;
static CommandOverride *ovr_notice  = NULL;
static CommandOverride *ovr_join    = NULL;
static CommandOverride *ovr_part    = NULL;
static CommandOverride *ovr_away    = NULL;
static CommandOverride *ovr_nick    = NULL;

/* ===================================================================
 * Forward declarations
 * =================================================================== */
static PersistEntry *find_entry(const char *account);
static PersistEntry *get_or_create_entry(const char *account);
static void          free_entry(PersistEntry *e);
static void          save_client_to_entry(Client *client, PersistEntry *e);
static void          do_create_ghost(PersistEntry *e);
static void          destroy_ghost(PersistEntry *e, const char *reason);
static void          restore_channels(Client *client, PersistEntry *e);
static int           is_ghost_client(Client *client);
static int           is_session_client(Client *client);
static PersistEntry *find_entry_for_session(Client *client);
static void          add_session(PersistEntry *e, Client *client);
static void          remove_session(PersistEntry *e, Client *client);
static void          promote_session(PersistEntry *e);
static void          setup_session(Client *client, PersistEntry *e);
static void          relay_to_channel_sessions(Channel *channel, PersistEntry *skip_e, MessageTag *mtags, const char *buf);
static int           persistence_effective(Client *client, PersistEntry *e);
static void          send_status(Client *client, PersistEntry *e);
static const char   *pref_to_str(int pref);
static int           str_to_pref(const char *s);
static void          persist_save_db(void);
static void          persist_load_db(void);

static int persist_local_quit(Client *client, MessageTag *mtags, const char *comment);
static int persist_remote_quit(Client *client, MessageTag *mtags, const char *comment);
static int persist_account_login(Client *client, MessageTag *mtags);
static int persist_welcome(Client *client, int after_numeric);
static int persist_chanmsg(Client *client, Channel *channel, int sendflags, const char *prefix, const char *target, MessageTag *mtags, const char *text, SendType sendtype);
static int persist_usermsg(Client *client, Client *to, MessageTag *mtags, const char *text, SendType sendtype);
static int persist_local_join(Client *client, Channel *channel, MessageTag *mtags);
static int persist_remote_join(Client *client, Channel *channel, MessageTag *mtags);
static int persist_local_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment);
static int persist_remote_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment);
static int persist_local_nickchange(Client *client, MessageTag *mtags, const char *newnick);
static int persist_remote_nickchange(Client *client, MessageTag *mtags, const char *newnick);
static int persist_local_chanmode(Client *client, Channel *channel, MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode, int *destroy_channel);
static int persist_remote_chanmode(Client *client, Channel *channel, MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode, int *destroy_channel);
static int persist_topic(Client *client, Channel *channel, MessageTag *mtags, const char *topic);
static int persist_away(Client *client, MessageTag *mtags, const char *reason, int already_as_away);
static int persist_local_kick(Client *client, Client *victim, Channel *channel, MessageTag *mtags, const char *comment);
static int persist_remote_kick(Client *client, Client *victim, Channel *channel, MessageTag *mtags, const char *comment);
static int persist_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
static int persist_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
static int persist_configposttest(int *errs);
CMD_FUNC(cmd_persistence);
CMD_OVERRIDE_FUNC(session_msg_override);
CMD_OVERRIDE_FUNC(session_join_override);
CMD_OVERRIDE_FUNC(session_part_override);
CMD_OVERRIDE_FUNC(session_away_override);
CMD_OVERRIDE_FUNC(session_nick_override);
EVENT(ghost_cleanup_event);

/* ===================================================================
 * Module lifecycle
 * =================================================================== */
MOD_TEST()
{
MARK_AS_OFFICIAL_MODULE(modinfo);
HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN,      0, persist_configrun);
HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST,     0, persist_configtest);
HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, persist_configposttest);
return MOD_SUCCESS;
}

MOD_INIT()
{
ClientCapabilityInfo cap;
ModDataInfo mdi;

MARK_AS_OFFICIAL_MODULE(modinfo);

memset(&cap, 0, sizeof(cap));
cap.name = "draft/persistence";
ClientCapabilityAdd(modinfo->handle, &cap, &CAP_PERSISTENCE);

CommandAdd(modinfo->handle, "PERSISTENCE", cmd_persistence, MAXPARA, CMD_USER);

memset(&mdi, 0, sizeof(mdi));
mdi.name = "persistence_ghost";
mdi.type = MODDATATYPE_CLIENT;
ghost_md = ModDataAdd(modinfo->handle, mdi);
if (!ghost_md)
{
config_error("persistence: Failed to add ghost moddata");
return MOD_FAILED;
}

memset(&mdi, 0, sizeof(mdi));
mdi.name = "persistence_restore";
mdi.type = MODDATATYPE_CLIENT;
restore_md = ModDataAdd(modinfo->handle, mdi);
if (!restore_md)
{
config_error("persistence: Failed to add restore moddata");
return MOD_FAILED;
}

memset(&mdi, 0, sizeof(mdi));
mdi.name = "persistence_session";
mdi.type = MODDATATYPE_CLIENT;
session_md = ModDataAdd(modinfo->handle, mdi);
if (!session_md)
{
config_error("persistence: Failed to add session moddata");
return MOD_FAILED;
}

/* Core lifecycle hooks */
HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT,    0, persist_local_quit);
HookAdd(modinfo->handle, HOOKTYPE_REMOTE_QUIT,   0, persist_remote_quit);
HookAdd(modinfo->handle, HOOKTYPE_ACCOUNT_LOGIN, 0, persist_account_login);
HookAdd(modinfo->handle, HOOKTYPE_WELCOME,       0, persist_welcome);

/* Session relay hooks */
HookAdd(modinfo->handle, HOOKTYPE_CHANMSG,           0, persist_chanmsg);
HookAdd(modinfo->handle, HOOKTYPE_USERMSG,           0, persist_usermsg);
HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN,        0, persist_local_join);
HookAdd(modinfo->handle, HOOKTYPE_REMOTE_JOIN,       0, persist_remote_join);
HookAdd(modinfo->handle, HOOKTYPE_LOCAL_PART,        0, persist_local_part);
HookAdd(modinfo->handle, HOOKTYPE_REMOTE_PART,       0, persist_remote_part);
HookAdd(modinfo->handle, HOOKTYPE_LOCAL_NICKCHANGE,  0, persist_local_nickchange);
HookAdd(modinfo->handle, HOOKTYPE_REMOTE_NICKCHANGE, 0, persist_remote_nickchange);
HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CHANMODE,    0, persist_local_chanmode);
HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CHANMODE,   0, persist_remote_chanmode);
HookAdd(modinfo->handle, HOOKTYPE_TOPIC,             0, persist_topic);
HookAdd(modinfo->handle, HOOKTYPE_AWAY,              0, persist_away);
HookAdd(modinfo->handle, HOOKTYPE_LOCAL_KICK,        0, persist_local_kick);
HookAdd(modinfo->handle, HOOKTYPE_REMOTE_KICK,       0, persist_remote_kick);

/* Command overrides: proxy session client commands through canonical */
ovr_privmsg = CommandOverrideAdd(modinfo->handle, "PRIVMSG", 0, session_msg_override);
ovr_notice  = CommandOverrideAdd(modinfo->handle, "NOTICE",  0, session_msg_override);
ovr_join    = CommandOverrideAdd(modinfo->handle, "JOIN",    0, session_join_override);
ovr_part    = CommandOverrideAdd(modinfo->handle, "PART",    0, session_part_override);
ovr_away    = CommandOverrideAdd(modinfo->handle, "AWAY",    0, session_away_override);
ovr_nick    = CommandOverrideAdd(modinfo->handle, "NICK",    0, session_nick_override);

EventAdd(modinfo->handle, "persist_cleanup", ghost_cleanup_event, NULL,
         PERSIST_CLEANUP_INTERVAL_MS, 0);

return MOD_SUCCESS;
}

MOD_LOAD()
{
away_notify_cap = ClientCapabilityBit("away-notify");
persist_load_db();
return MOD_SUCCESS;
}

MOD_UNLOAD()
{
PersistEntry *e, *enext;

persist_save_db();

/* Clear session state: orphaned session clients will continue as
 * invisible users until they timeout; when they disconnect the
 * module hooks are gone so no ghost will be created. */
for (e = persist_list; e; e = e->next)
{
PersistSession *sess, *snext;
for (sess = e->sessions; sess; sess = snext)
{
snext = sess->next;
if (session_md && sess->client)
moddata_client(sess->client, session_md).i = 0;
safe_free(sess);
}
e->sessions     = NULL;
e->num_sessions = 0;
e->canonical    = NULL;
}

for (e = persist_list; e; e = enext)
{
enext = e->next;
if (e->ghost)
{
Client *ghost = e->ghost;
e->ghost = NULL;
exit_client(ghost, NULL, "Server restarting (persistence)");
}
free_entry(e);
}
persist_list = NULL;

return MOD_SUCCESS;
}

/* ===================================================================
 * Config
 * =================================================================== */
static int persist_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
ConfigEntry *cep;

if (type != CONFIG_MAIN)
return 0;
if (!ce || !ce->name || strcmp(ce->name, "persistence"))
return 0;

for (cep = ce->items; cep; cep = cep->next)
{
if (!strcmp(cep->name, "timeout"))
{
if (!cep->value)
{
config_error("%s:%d: persistence::timeout requires a value",
             cep->file->filename, cep->line_number);
(*errs)++;
}
} else if (!strcmp(cep->name, "default"))
{
if (!cep->value)
{
config_error("%s:%d: persistence::default requires on|off",
             cep->file->filename, cep->line_number);
(*errs)++;
}
} else {
config_error("%s:%d: Unknown directive persistence::%s",
             cep->file->filename, cep->line_number, cep->name);
(*errs)++;
}
}
return 1;
}

static int persist_configposttest(int *errs)
{
return 0;
}

static int persist_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
ConfigEntry *cep;

if (type != CONFIG_MAIN)
return 0;
if (!ce || !ce->name || strcmp(ce->name, "persistence"))
return 0;

for (cep = ce->items; cep; cep = cep->next)
{
if (!strcmp(cep->name, "timeout") && cep->value)
{
persist_timeout = config_checkval(cep->value, CFG_TIME);
if (persist_timeout <= 0)
persist_timeout = PERSIST_DEFAULT_TIMEOUT;
} else if (!strcmp(cep->name, "default") && cep->value)
{
if (!strcmp(cep->value, "on"))
persist_default_on = 1;
else if (!strcmp(cep->value, "off"))
persist_default_on = 0;
}
}
return 1;
}

/* ===================================================================
 * Entry management
 * =================================================================== */
static PersistEntry *find_entry(const char *account)
{
PersistEntry *e;

if (!account || !*account || !strcmp(account, "0"))
return NULL;
for (e = persist_list; e; e = e->next)
if (!strcasecmp(e->account, account))
return e;
return NULL;
}

static PersistEntry *get_or_create_entry(const char *account)
{
PersistEntry *e;

e = find_entry(account);
if (e)
return e;

e = safe_alloc(sizeof(PersistEntry));
strlcpy(e->account, account, sizeof(e->account));
e->preference      = PREF_DEFAULT;
e->disconnect_time = TStime();

e->next = persist_list;
if (persist_list)
persist_list->prev = e;
persist_list = e;
return e;
}

static void free_entry(PersistEntry *e)
{
if (!e)
return;
if (e->prev)
e->prev->next = e->next;
else
persist_list = e->next;
if (e->next)
e->next->prev = e->prev;
safe_free(e->away);
safe_free(e);
}

/* ===================================================================
 * State capture
 * =================================================================== */
static void save_client_to_entry(Client *client, PersistEntry *e)
{
Membership *mb;
int i;

strlcpy(e->nick,        client->name,              sizeof(e->nick));
strlcpy(e->ident,       client->ident,             sizeof(e->ident));
strlcpy(e->realname,    client->info,              sizeof(e->realname));
strlcpy(e->realhost,    client->user->realhost,    sizeof(e->realhost));
strlcpy(e->cloakedhost, client->user->cloakedhost, sizeof(e->cloakedhost));

if (client->user->virthost && *client->user->virthost)
strlcpy(e->host, client->user->virthost, sizeof(e->host));
else
strlcpy(e->host, e->cloakedhost, sizeof(e->host));

if (client->ip)
strlcpy(e->ip, client->ip, sizeof(e->ip));
else
*e->ip = '\0';

e->umodes          = client->umodes;
e->disconnect_time = TStime();

safe_free(e->away);
e->away = (client->user->away && *client->user->away)
          ? strdup(client->user->away) : NULL;

i = 0;
for (mb = client->user->channel; mb && i < PERSIST_MAX_CHANNELS; mb = mb->next, i++)
{
strlcpy(e->channels[i].name,  mb->channel->name, sizeof(e->channels[i].name));
strlcpy(e->channels[i].modes, mb->member_modes,  sizeof(e->channels[i].modes));
}
e->num_channels = i;
}

/* ===================================================================
 * Ghost lifecycle
 * =================================================================== */
static void do_create_ghost(PersistEntry *e)
{
Client *ghost;
Channel *channel;
int i;

if (!e || !*e->nick || !e->num_channels)
return;

ghost = make_client(NULL, &me);
ghost->local->fd = -2;

strlcpy(ghost->name, e->nick, sizeof(ghost->name));

if (*e->ip)
safe_strdup(ghost->ip, e->ip);

make_user(ghost);

strlcpy(ghost->ident,              e->ident,       sizeof(ghost->ident));
strlcpy(ghost->info,               e->realname,    sizeof(ghost->info));
strlcpy(ghost->user->username,     e->ident,       sizeof(ghost->user->username));
strlcpy(ghost->user->realhost,     e->realhost,    sizeof(ghost->user->realhost));
strlcpy(ghost->user->cloakedhost,  e->cloakedhost, sizeof(ghost->user->cloakedhost));
safe_free(ghost->user->virthost);
safe_strdup(ghost->user->virthost, e->host);
strlcpy(ghost->user->account, e->account, sizeof(ghost->user->account));
ghost->umodes = e->umodes;

SetUser(ghost);

safe_strdup(ghost->user->away, "User is offline (persistent)");
ghost->user->away_since = TStime();

moddata_client(ghost, ghost_md).i = 1;

add_to_client_hash_table(ghost->name, ghost);
add_client_to_list(ghost);

irccounts.clients++;
irccounts.me_clients++;
if (IsInvisible(ghost))
irccounts.invisible++;
if (ghost->uplink && ghost->uplink->server)
ghost->uplink->server->users++;

for (i = 0; i < e->num_channels; i++)
{
channel = find_channel(e->channels[i].name);
if (!channel)
channel = make_channel(e->channels[i].name);
if (channel)
add_user_to_channel(channel, ghost, e->channels[i].modes);
}

e->ghost = ghost;

unreal_log(ULOG_INFO, "persistence", "PERSIST_GHOST_CREATED", ghost,
           "Persistent ghost created for account $account (nick $client)",
           log_data_string("account", e->account));
}

static void destroy_ghost(PersistEntry *e, const char *reason)
{
Client *ghost;
Membership *mp;

if (!e || !e->ghost)
return;

ghost    = e->ghost;
e->ghost = NULL;

while ((mp = ghost->user->channel))
remove_user_from_channel_withmb(ghost, mp->channel, mp, 1);

moddata_free_client(ghost);
if (*ghost->id)
{
del_from_id_hash_table(ghost->id, ghost);
*ghost->id = '\0';
}
if (*ghost->name)
del_from_client_hash_table(ghost->name, ghost);
remove_client_from_list(ghost);
free_client(ghost);
}

/* ===================================================================
 * Channel restoration
 * =================================================================== */
static void restore_channels(Client *client, PersistEntry *e)
{
int i;
Channel *channel;
int same_nick;

if (!e || !e->num_channels)
{
if (restore_md)
moddata_client(client, restore_md).ptr = NULL;
return;
}

same_nick = !strcasecmp(client->name, e->nick);

for (i = 0; i < e->num_channels; i++)
{
channel = find_channel(e->channels[i].name);
if (!channel)
continue;
if (IsMember(client, channel))
continue;

if (same_nick)
{
add_user_to_channel(channel, client, e->channels[i].modes);

sendto_one(client, NULL, ":%s!%s@%s JOIN :%s",
           client->name,
           client->user->username,
           GetHost(client),
           channel->name);

if (channel->topic)
{
sendnumeric(client, RPL_TOPIC, channel->name, channel->topic);
sendnumeric(client, RPL_TOPICWHOTIME, channel->name,
            channel->topic_nick, (long long)channel->topic_time);
}

{
const char *parv[3];
parv[0] = NULL;
parv[1] = channel->name;
parv[2] = NULL;
do_cmd(client, NULL, "NAMES", 2, parv);
}
}
else
{
MessageTag *jmtags = NULL;
new_message(client, NULL, &jmtags);
join_channel(channel, client, jmtags, e->channels[i].modes);
free_message_tags(jmtags);
if (IsDead(client))
break;
}
}

if (same_nick)
{
MessageTag *away_mtags = NULL;

safe_free(client->user->away);
client->user->away = NULL;
client->user->away_since = 0;

new_message(client, NULL, &away_mtags);
sendto_local_common_channels(client, NULL, away_notify_cap, away_mtags,
                             ":%s AWAY", client->name);
free_message_tags(away_mtags);
}

if (restore_md)
moddata_client(client, restore_md).ptr = NULL;
}

/* ===================================================================
 * Helpers
 * =================================================================== */
static int is_ghost_client(Client *client)
{
if (!ghost_md || !client)
return 0;
return moddata_client(client, ghost_md).i == 1;
}

static int is_session_client(Client *client)
{
if (!session_md || !client)
return 0;
return moddata_client(client, session_md).i == 1;
}

static PersistEntry *find_entry_for_session(Client *client)
{
if (!restore_md || !client)
return NULL;
return (PersistEntry *)moddata_client(client, restore_md).ptr;
}

static void add_session(PersistEntry *e, Client *client)
{
PersistSession *sess;

sess = safe_alloc(sizeof(PersistSession));
sess->client = client;
sess->next   = e->sessions;
e->sessions  = sess;
e->num_sessions++;
}

static void remove_session(PersistEntry *e, Client *client)
{
PersistSession *sess, *prev = NULL;

for (sess = e->sessions; sess; sess = sess->next)
{
if (sess->client == client)
{
if (prev)
prev->next = sess->next;
else
e->sessions = sess->next;
e->num_sessions--;
safe_free(sess);
return;
}
prev = sess;
}
}

/*
 * Promote the first attached session to canonical.
 * Called when the canonical client quits but sessions remain.
 * Assumes save_client_to_entry() has already been called and the
 * canonical's nick has been removed from the hash table.
 */
static void promote_session(PersistEntry *e)
{
PersistSession *sess;
Client *new_canonical;

if (!e->sessions)
return;

sess            = e->sessions;
e->sessions     = sess->next;
e->num_sessions--;
new_canonical   = sess->client;
safe_free(sess);

/* Session was removed from nick hash in setup_session; re-add now
 * that it becomes canonical. client->name is already e->nick. */
add_to_client_hash_table(new_canonical->name, new_canonical);

/* No longer a session */
if (session_md)
moddata_client(new_canonical, session_md).i = 0;

/* Restore channels (same_nick path: client->name now equals e->nick) */
restore_channels(new_canonical, e);

/* Clear any away message (promoted client is now active) */
if (new_canonical->user && new_canonical->user->away)
{
safe_free(new_canonical->user->away);
new_canonical->user->away       = NULL;
new_canonical->user->away_since = 0;
}

e->canonical = new_canonical;

unreal_log(ULOG_INFO, "persistence", "PERSIST_SESSION_PROMOTED", new_canonical,
           "Session promoted to canonical for account $account",
           log_data_string("account", e->account));
}

/*
 * Complete the setup of a session client after registration (376).
 * Removes it from the nick hash (so find_client returns canonical),
 * marks it invisible, and sends synthetic channel JOINs.
 */
static void setup_session(Client *client, PersistEntry *e)
{
Membership *mb;

if (!e->canonical || IsDead(e->canonical) || !IsUser(e->canonical))
return;

/* The session was already removed from the nick hash in
 * persist_account_login (pre-001); canonical holds that slot. */

if (!IsInvisible(client))
{
client->umodes |= UMODE_INVISIBLE;
irccounts.invisible++;
}

add_session(e, client);

/* Send synthetic JOINs for each of the canonical's channels */
for (mb = e->canonical->user->channel; mb; mb = mb->next)
{
Channel *channel = mb->channel;

sendto_one(client, NULL, ":%s!%s@%s JOIN :%s",
           e->canonical->name,
           e->canonical->user->username,
           GetHost(e->canonical),
           channel->name);

if (channel->topic)
{
sendnumeric(client, RPL_TOPIC, channel->name, channel->topic);
sendnumeric(client, RPL_TOPICWHOTIME, channel->name,
            channel->topic_nick, (long long)channel->topic_time);
}

/* Send NAMES directly: do NOT use do_cmd() here — it runs the full
 * command pipeline including hooks and overrides, which can cause
 * JOIN broadcasts visible to session1. */
{
Member *nm;
char namebuf[512];
int  namelen = 0;

namebuf[0] = '\0';
for (nm = channel->members; nm; nm = nm->next)
{
const char *prefix = "";
if (strchr(nm->member_modes, 'q') || strchr(nm->member_modes, 'a'))
prefix = "&";
else if (strchr(nm->member_modes, 'o'))
prefix = "@";
else if (strchr(nm->member_modes, 'h'))
prefix = "%";
else if (strchr(nm->member_modes, 'v'))
prefix = "+";
int needed = strlen(prefix) + strlen(nm->client->name) + 2;
if (namelen > 0 && namelen + needed > 400)
{
sendnumeric(client, RPL_NAMREPLY, "=", channel->name, namebuf);
namebuf[0] = '\0';
namelen = 0;
}
if (namelen) { namebuf[namelen++] = ' '; namebuf[namelen] = '\0'; }
strlcat(namebuf, prefix, sizeof(namebuf));
strlcat(namebuf, nm->client->name, sizeof(namebuf));
namelen = strlen(namebuf);
}
if (namelen)
sendnumeric(client, RPL_NAMREPLY, "=", channel->name, namebuf);
sendnumeric(client, RPL_ENDOFNAMES, channel->name);
}
}

unreal_log(ULOG_INFO, "persistence", "PERSIST_SESSION_ATTACHED", client,
           "Session attached to canonical for account $account",
           log_data_string("account", e->account));
}

/*
 * Relay a pre-formatted IRC line to all session clients of canonical
 * members of `channel`.  Sessions belonging to `skip_e` are skipped
 * (used to avoid sending a canonical's own QUIT to its sessions when
 * a session promotion will handle continuity instead).
 */
static void relay_to_channel_sessions(Channel *channel, PersistEntry *skip_e,
                                      MessageTag *mtags, const char *buf)
{
Member *mb;
PersistEntry *e;
PersistSession *sess;

for (mb = channel->members; mb; mb = mb->next)
{
Client *member = mb->client;
if (!MyUser(member) || !IsLoggedIn(member))
continue;
e = find_entry(member->user->account);
if (!e || !e->sessions)
continue;
if (e == skip_e)
continue;
for (sess = e->sessions; sess; sess = sess->next)
{
/* Skip sessions already in the channel: they receive via normal IRC path */
if (!IsMember(sess->client, channel))
sendto_one(sess->client, mtags, "%s", buf);
}
}
}

static int persistence_effective(Client *client, PersistEntry *e)
{
int pref;

if (client && !IsLoggedIn(client))
return 0;

pref = e ? e->preference : PREF_DEFAULT;
if (pref == PREF_ON)  return 1;
if (pref == PREF_OFF) return 0;
return persist_default_on;
}

static const char *pref_to_str(int pref)
{
if (pref == PREF_ON)  return "ON";
if (pref == PREF_OFF) return "OFF";
return "DEFAULT";
}

static int str_to_pref(const char *s)
{
if (!s)                         return PREF_DEFAULT;
if (!strcasecmp(s, "ON"))       return PREF_ON;
if (!strcasecmp(s, "OFF"))      return PREF_OFF;
if (!strcasecmp(s, "DEFAULT"))  return PREF_DEFAULT;
return -99;
}

static void send_status(Client *client, PersistEntry *e)
{
int pref;

if (!MyUser(client))
return;

pref = e ? e->preference : PREF_DEFAULT;
sendto_one(client, NULL,
           ":%s PERSISTENCE STATUS %s %s",
           me.name,
           pref_to_str(pref),
           persistence_effective(client, e) ? "ON" : "OFF");
}

/* ===================================================================
 * Hook: HOOKTYPE_LOCAL_QUIT
 * =================================================================== */
static int persist_local_quit(Client *client, MessageTag *mtags, const char *comment)
{
PersistEntry *e;
Membership *mb, *mb_next;
MessageTag *away_mtags;

/* Ghosts exit normally without triggering new ghost creation */
if (is_ghost_client(client))
return 0;

/* Session clients: detach from sessions list only, no ghost */
if (is_session_client(client))
{
Membership *mb, *mb_next;
/* Strip from all channels silently so normal quit processing
 * does not broadcast QUIT to channel members. */
for (mb = client->user->channel; mb; mb = mb_next)
{
mb_next = mb->next;
remove_user_from_channel_withmb(client, mb->channel, mb, 1);
}
e = find_entry_for_session(client);
if (e)
remove_session(e, client);
/* Session was never added to nick hash — blank name so
 * exit_one_client does not attempt a stale hash removal. */
*client->name = '\0';
return 0;
}

if (!IsUser(client) || !MyUser(client))
return 0;

if (!IsLoggedIn(client))
return 0;

e = find_entry(client->user->account);
if (!persistence_effective(client, e))
return 0;

e = get_or_create_entry(client->user->account);

/* Safety: if another canonical is registered for this account, skip */
if (e->canonical && e->canonical != client)
return 0;

e->canonical = NULL;

/* Destroy any stale ghost */
if (e->ghost)
destroy_ghost(e, NULL);

/* Relay QUIT to sessions of OTHER channel members before stripping channels */
for (mb = client->user->channel; mb; mb = mb->next)
{
char buf[BUFSIZE];
snprintf(buf, sizeof(buf), ":%s!%s@%s QUIT :%s",
         client->name, client->user->username, GetHost(client),
         comment ? comment : "");
relay_to_channel_sessions(mb->channel, e, mtags, buf);
}

/* Capture current state */
save_client_to_entry(client, e);

/* Free the nick hash slot */
del_from_client_hash_table(client->name, client);

if (e->num_sessions > 0)
{
/* Promote first session to canonical — seamless hand-off */
promote_session(e);
}
else
{
/* No sessions — create ghost */
do_create_ghost(e);

if (e->ghost)
{
away_mtags = NULL;
new_message(e->ghost, NULL, &away_mtags);
sendto_local_common_channels(e->ghost, NULL, away_notify_cap, away_mtags,
                             ":%s AWAY :%s", e->ghost->name,
                             "User is offline (persistent)");
free_message_tags(away_mtags);
}
}

/* Strip real client from all channels so exit_one_client sends no QUIT */
for (mb = client->user->channel; mb; mb = mb_next)
{
mb_next = mb->next;
remove_user_from_channel_withmb(client, mb->channel, mb, 1);
}

/* Blank nick so exit_one_client skips the hash removal */
*client->name = '\0';

persist_save_db();

return 0;
}

/* ===================================================================
 * Hook: HOOKTYPE_REMOTE_QUIT
 * Relay remote user quits to sessions of local channel members.
 * =================================================================== */
static int persist_remote_quit(Client *client, MessageTag *mtags, const char *comment)
{
Membership *mb;

if (!IsUser(client) || is_ghost_client(client))
return 0;

for (mb = client->user->channel; mb; mb = mb->next)
{
char buf[BUFSIZE];
snprintf(buf, sizeof(buf), ":%s!%s@%s QUIT :%s",
         client->name, client->user->username, GetHost(client),
         comment ? comment : "");
relay_to_channel_sessions(mb->channel, NULL, mtags, buf);
}

return 0;
}

/* ===================================================================
 * Hook: HOOKTYPE_ACCOUNT_LOGIN
 * =================================================================== */
static int persist_account_login(Client *client, MessageTag *mtags)
{
PersistEntry *e;
Membership *mb;
int i;

if (!client->user || !MyConnect(client))
return 0;

if (!IsLoggedIn(client))
return 0;

e = find_entry(client->user->account);

/* --- Ghost reclaim --- */
if (e && e->ghost)
{
unreal_log(ULOG_INFO, "persistence", "PERSIST_RECLAIM", client,
           "Reclaiming persistent ghost for account $account",
           log_data_string("account", e->account));

i = 0;
for (mb = e->ghost->user->channel; mb && i < PERSIST_MAX_CHANNELS; mb = mb->next, i++)
{
strlcpy(e->channels[i].name,  mb->channel->name, sizeof(e->channels[i].name));
strlcpy(e->channels[i].modes, mb->member_modes,  sizeof(e->channels[i].modes));
}
e->num_channels = i;

destroy_ghost(e, NULL);

if (IsDead(client))
return 0;
}

/* --- Nick enforcement (pre-registration) ---
 * After ghost destruction the account nick is free.  Silently rename
 * the client to their account name if they don't have it already. */
if (!IsRegistered(client))
{
const char *want_nick = client->user->account;

if (strcmp(client->name, want_nick) != 0 && !find_client(want_nick, NULL))
{
del_from_client_hash_table(client->name, client);
strlcpy(client->name, want_nick, sizeof(client->name));
add_to_client_hash_table(client->name, client);
/* 001 will carry the correct nick; no NICK message needed */
}
}

if (!e)
return 0;   /* No entry, nothing to restore */

/* --- Session detection ---
 * If a live canonical is already online for this account, attach as
 * a session rather than restoring channels.
 * Rename to account nick here (pre-001) so 001 carries the right nick.
 * We remove from the old hash slot and don't re-add; canonical holds it. */
if (e->canonical && !IsDead(e->canonical) &&
    IsUser(e->canonical) && !is_ghost_client(e->canonical))
{
/* Remove old nick slot, rename to account nick */
del_from_client_hash_table(client->name, client);
strlcpy(client->name, e->account, sizeof(client->name));
/* Do NOT add back to hash: canonical already owns that nick slot */
moddata_client(client, session_md).i  = 1;
moddata_client(client, restore_md).ptr = (void *)e;

unreal_log(ULOG_INFO, "persistence", "PERSIST_SESSION_QUEUED", client,
           "Session queued for account $account (canonical: $existing_client)",
           log_data_string("account", e->account),
           log_data_client("existing_client", e->canonical));
return 0;
}

/* Clear stale canonical pointer */
e->canonical = NULL;

/* Queue channel restoration for after 376 */
moddata_client(client, restore_md).ptr = (void *)e;

/* If already registered (NickServ login post-burst), restore immediately */
if (IsRegistered(client))
{
send_status(client, e);
restore_channels(client, e);
e->canonical = client;
}

persist_save_db();

return 0;
}

/* ===================================================================
 * Hook: HOOKTYPE_WELCOME
 * =================================================================== */
static int persist_welcome(Client *client, int after_numeric)
{
PersistEntry *e;

if (!MyUser(client))
return 0;

if (after_numeric == 5)
{
if (!(client->local->caps & CAP_PERSISTENCE))
return 0;
if (!IsLoggedIn(client))
return 0;
e = find_entry(client->user->account);
send_status(client, e);
}
else if (after_numeric == 376)
{
e = (PersistEntry *)moddata_client(client, restore_md).ptr;
if (!e)
return 0;

if (is_session_client(client))
{
/* Session: rename to internal nick, relay channel state */
setup_session(client, e);
}
else
{
/* Normal restore: join channels, register as canonical */
restore_channels(client, e);
if (!IsDead(client))
e->canonical = client;
}
}

return 0;
}

/* ===================================================================
 * Session relay hooks
 * =================================================================== */

static int persist_chanmsg(Client *client, Channel *channel, int sendflags,
                           const char *prefix, const char *target,
                           MessageTag *mtags, const char *text, SendType sendtype)
{
Member *mb;
PersistEntry *e;
PersistSession *sess;

if (sendtype == SEND_TYPE_TAGMSG)
return 0;

for (mb = channel->members; mb; mb = mb->next)
{
Client *member = mb->client;
if (!MyUser(member) || !IsLoggedIn(member))
continue;
e = find_entry(member->user->account);
if (!e || !e->sessions)
continue;

for (sess = e->sessions; sess; sess = sess->next)
{
/* Skip sessions already in the channel */
if (!IsMember(sess->client, channel))
sendto_one(sess->client, mtags, ":%s!%s@%s %s %s :%s",
           client->name,
           IsUser(client) ? client->user->username : "*",
           IsUser(client) ? GetHost(client) : me.name,
           sendtype_to_cmd(sendtype),
           channel->name,
           text);
}
}
return 0;
}

static int persist_usermsg(Client *client, Client *to, MessageTag *mtags,
                           const char *text, SendType sendtype)
{
PersistEntry *e;
PersistSession *sess;

if (!IsUser(to) || !IsLoggedIn(to) || sendtype == SEND_TYPE_TAGMSG)
return 0;

e = find_entry(to->user->account);
if (!e || !e->sessions)
return 0;

for (sess = e->sessions; sess; sess = sess->next)
{
sendto_one(sess->client, mtags, ":%s!%s@%s %s %s :%s",
           client->name,
           IsUser(client) ? client->user->username : "*",
           IsUser(client) ? GetHost(client) : me.name,
           sendtype_to_cmd(sendtype),
           to->name,
           text);
}
return 0;
}

static int persist_local_join(Client *client, Channel *channel, MessageTag *mtags)
{
char buf[BUFSIZE];
PersistEntry *joiner_e;
PersistSession *sess;

if (is_ghost_client(client) || is_session_client(client))
return 0;

snprintf(buf, sizeof(buf), ":%s!%s@%s JOIN :%s",
         client->name,
         IsUser(client) ? client->user->username : "*",
         IsUser(client) ? GetHost(client) : me.name,
         channel->name);

joiner_e = (IsUser(client) && IsLoggedIn(client))
           ? find_entry(client->user->account) : NULL;

/* Relay to sessions of OTHER channel members */
relay_to_channel_sessions(channel, joiner_e, mtags, buf);

/* Also relay to the joiner's own sessions (they see themselves join) */
if (joiner_e)
for (sess = joiner_e->sessions; sess; sess = sess->next)
sendto_one(sess->client, mtags, "%s", buf);

return 0;
}

static int persist_remote_join(Client *client, Channel *channel, MessageTag *mtags)
{
char buf[BUFSIZE];

if (is_ghost_client(client))
return 0;

snprintf(buf, sizeof(buf), ":%s!%s@%s JOIN :%s",
         client->name,
         IsUser(client) ? client->user->username : "*",
         IsUser(client) ? GetHost(client) : me.name,
         channel->name);

relay_to_channel_sessions(channel, NULL, mtags, buf);
return 0;
}

static int persist_local_part(Client *client, Channel *channel, MessageTag *mtags,
                              const char *comment)
{
char buf[BUFSIZE];
PersistEntry *parter_e;
PersistSession *sess;

if (is_ghost_client(client) || is_session_client(client))
return 0;

if (comment && *comment)
snprintf(buf, sizeof(buf), ":%s!%s@%s PART %s :%s",
         client->name, client->user->username, GetHost(client),
         channel->name, comment);
else
snprintf(buf, sizeof(buf), ":%s!%s@%s PART :%s",
         client->name, client->user->username, GetHost(client),
         channel->name);

parter_e = (IsUser(client) && IsLoggedIn(client))
           ? find_entry(client->user->account) : NULL;

relay_to_channel_sessions(channel, parter_e, mtags, buf);

if (parter_e)
for (sess = parter_e->sessions; sess; sess = sess->next)
sendto_one(sess->client, mtags, "%s", buf);

return 0;
}

static int persist_remote_part(Client *client, Channel *channel, MessageTag *mtags,
                               const char *comment)
{
char buf[BUFSIZE];

if (is_ghost_client(client))
return 0;

if (comment && *comment)
snprintf(buf, sizeof(buf), ":%s!%s@%s PART %s :%s",
         client->name,
         IsUser(client) ? client->user->username : "*",
         IsUser(client) ? GetHost(client) : me.name,
         channel->name, comment);
else
snprintf(buf, sizeof(buf), ":%s!%s@%s PART :%s",
         client->name,
         IsUser(client) ? client->user->username : "*",
         IsUser(client) ? GetHost(client) : me.name,
         channel->name);

relay_to_channel_sessions(channel, NULL, mtags, buf);
return 0;
}

static int persist_local_nickchange(Client *client, MessageTag *mtags, const char *newnick)
{
char buf[BUFSIZE];
PersistEntry *e;
PersistSession *sess;
Membership *mb;

if (is_ghost_client(client) || is_session_client(client) || !IsUser(client))
return 0;

snprintf(buf, sizeof(buf), ":%s!%s@%s NICK :%s",
         client->name, client->user->username, GetHost(client), newnick);

e = IsLoggedIn(client) ? find_entry(client->user->account) : NULL;

if (e && e->canonical == client)
{
/* Update stored nick and inform sessions */
strlcpy(e->nick, newnick, sizeof(e->nick));
for (sess = e->sessions; sess; sess = sess->next)
sendto_one(sess->client, mtags, "%s", buf);
}

/* Relay to sessions of other channel members */
for (mb = client->user->channel; mb; mb = mb->next)
relay_to_channel_sessions(mb->channel, e, mtags, buf);

return 0;
}

static int persist_remote_nickchange(Client *client, MessageTag *mtags, const char *newnick)
{
char buf[BUFSIZE];
Membership *mb;

if (!IsUser(client) || is_ghost_client(client))
return 0;

snprintf(buf, sizeof(buf), ":%s!%s@%s NICK :%s",
         client->name, client->user->username, GetHost(client), newnick);

for (mb = client->user->channel; mb; mb = mb->next)
relay_to_channel_sessions(mb->channel, NULL, mtags, buf);

return 0;
}

static int persist_local_chanmode(Client *client, Channel *channel, MessageTag *mtags,
                                  const char *modebuf, const char *parabuf,
                                  time_t sendts, int samode, int *destroy_channel)
{
char buf[BUFSIZE];

if (parabuf && *parabuf)
snprintf(buf, sizeof(buf), ":%s!%s@%s MODE %s %s %s",
         client->name,
         IsUser(client) ? client->user->username : "*",
         IsUser(client) ? GetHost(client) : me.name,
         channel->name, modebuf, parabuf);
else
snprintf(buf, sizeof(buf), ":%s!%s@%s MODE %s %s",
         client->name,
         IsUser(client) ? client->user->username : "*",
         IsUser(client) ? GetHost(client) : me.name,
         channel->name, modebuf);

relay_to_channel_sessions(channel, NULL, mtags, buf);
return 0;
}

static int persist_remote_chanmode(Client *client, Channel *channel, MessageTag *mtags,
                                   const char *modebuf, const char *parabuf,
                                   time_t sendts, int samode, int *destroy_channel)
{
return persist_local_chanmode(client, channel, mtags, modebuf, parabuf,
                              sendts, samode, destroy_channel);
}

static int persist_topic(Client *client, Channel *channel, MessageTag *mtags,
                         const char *topic)
{
char buf[BUFSIZE];

snprintf(buf, sizeof(buf), ":%s!%s@%s TOPIC %s :%s",
         client->name,
         IsUser(client) ? client->user->username : "*",
         IsUser(client) ? GetHost(client) : me.name,
         channel->name, topic ? topic : "");

relay_to_channel_sessions(channel, NULL, mtags, buf);
return 0;
}

static int persist_away(Client *client, MessageTag *mtags, const char *reason,
                        int already_as_away)
{
char buf[BUFSIZE];
Membership *mb;
PersistEntry *e;

if (!IsUser(client) || is_ghost_client(client) || is_session_client(client))
return 0;

if (reason && *reason)
snprintf(buf, sizeof(buf), ":%s!%s@%s AWAY :%s",
         client->name, client->user->username, GetHost(client), reason);
else
snprintf(buf, sizeof(buf), ":%s!%s@%s AWAY",
         client->name, client->user->username, GetHost(client));

e = IsLoggedIn(client) ? find_entry(client->user->account) : NULL;

for (mb = client->user->channel; mb; mb = mb->next)
relay_to_channel_sessions(mb->channel, e, mtags, buf);

return 0;
}

static int persist_local_kick(Client *client, Client *victim, Channel *channel,
                              MessageTag *mtags, const char *comment)
{
char buf[BUFSIZE];
PersistEntry *victim_e;
PersistSession *sess;

snprintf(buf, sizeof(buf), ":%s!%s@%s KICK %s %s :%s",
         client->name,
         IsUser(client) ? client->user->username : "*",
         IsUser(client) ? GetHost(client) : me.name,
         channel->name, victim->name,
         comment ? comment : client->name);

relay_to_channel_sessions(channel, NULL, mtags, buf);

/* Also send to the victim's own sessions */
victim_e = (IsUser(victim) && IsLoggedIn(victim))
           ? find_entry(victim->user->account) : NULL;
if (victim_e)
for (sess = victim_e->sessions; sess; sess = sess->next)
sendto_one(sess->client, mtags, "%s", buf);

return 0;
}

static int persist_remote_kick(Client *client, Client *victim, Channel *channel,
                               MessageTag *mtags, const char *comment)
{
return persist_local_kick(client, victim, channel, mtags, comment);
}

/* ===================================================================
 * Command overrides: proxy session client commands through canonical
 * =================================================================== */

CMD_OVERRIDE_FUNC(session_msg_override)
{
if (is_session_client(client))
{
PersistEntry *e = find_entry_for_session(client);
if (e && e->canonical && IsUser(e->canonical) && !IsDead(e->canonical))
client = e->canonical;
else
return;
}
CALL_NEXT_COMMAND_OVERRIDE();
}

CMD_OVERRIDE_FUNC(session_join_override)
{
if (is_session_client(client))
{
PersistEntry *e = find_entry_for_session(client);
if (e && e->canonical && IsUser(e->canonical) && !IsDead(e->canonical))
client = e->canonical;
else
return;
}
CALL_NEXT_COMMAND_OVERRIDE();
}

CMD_OVERRIDE_FUNC(session_part_override)
{
if (is_session_client(client))
{
PersistEntry *e = find_entry_for_session(client);
if (e && e->canonical && IsUser(e->canonical) && !IsDead(e->canonical))
client = e->canonical;
else
return;
}
CALL_NEXT_COMMAND_OVERRIDE();
}

CMD_OVERRIDE_FUNC(session_away_override)
{
if (is_session_client(client))
{
PersistEntry *e = find_entry_for_session(client);
if (e && e->canonical && IsUser(e->canonical) && !IsDead(e->canonical))
client = e->canonical;
else
return;
}
CALL_NEXT_COMMAND_OVERRIDE();
}

CMD_OVERRIDE_FUNC(session_nick_override)
{
if (is_session_client(client))
return;   /* Silently block: session nicks are managed by the module */
CALL_NEXT_COMMAND_OVERRIDE();
}

/* ===================================================================
 * Command: PERSISTENCE GET | SET ON|OFF|DEFAULT
 * =================================================================== */
CMD_FUNC(cmd_persistence)
{
PersistEntry *e;
const char *subcmd;
int new_pref;

if (!MyUser(client) || !IsUser(client))
return;

if (parc < 2 || BadPtr(parv[1]))
{
sendto_one(client, NULL,
           "FAIL PERSISTENCE INVALID_PARAMETERS :Invalid parameters");
return;
}

if (!IsLoggedIn(client))
{
sendto_one(client, NULL,
           "FAIL PERSISTENCE ACCOUNT_REQUIRED :An account is required");
return;
}

subcmd = parv[1];
e = find_entry(client->user->account);

if (!strcasecmp(subcmd, "GET"))
{
send_status(client, e);
}
else if (!strcasecmp(subcmd, "SET"))
{
if (parc < 3 || BadPtr(parv[2]))
{
sendto_one(client, NULL,
           "FAIL PERSISTENCE INVALID_PARAMETERS :Invalid parameters");
return;
}

new_pref = str_to_pref(parv[2]);
if (new_pref == -99)
{
sendto_one(client, NULL,
           "FAIL PERSISTENCE INVALID_PARAMETERS :Invalid parameters");
return;
}

e = get_or_create_entry(client->user->account);
if (!e)
{
sendto_one(client, NULL,
           "FAIL PERSISTENCE INTERNAL_ERROR :An error occurred");
return;
}

e->preference = new_pref;

if (!persistence_effective(client, e) && e->ghost)
destroy_ghost(e, "Persistence disabled by user");

send_status(client, e);
persist_save_db();
}
}

/* ===================================================================
 * Cleanup event (every 60 s)
 * =================================================================== */
EVENT(ghost_cleanup_event)
{
PersistEntry *e, *enext;
time_t now = TStime();

for (e = persist_list; e; e = enext)
{
enext = e->next;

/* Skip entries with a live canonical or active sessions */
if (e->canonical && !IsDead(e->canonical))
continue;

if (!e->ghost)
{
if (now - e->disconnect_time > persist_timeout)
free_entry(e);
continue;
}

if (now - e->disconnect_time > persist_timeout)
{
unreal_log(ULOG_INFO, "persistence", "PERSIST_GHOST_EXPIRED", e->ghost,
           "Persistent ghost for account $account expired",
           log_data_string("account", e->account));
destroy_ghost(e, "Ghost expired (inactivity timeout)");
free_entry(e);
}
}
}

/* ===================================================================
 * Database: save / load (unrealdb)
 * =================================================================== */
static const char *persist_db_path(void)
{
static char path[512];
snprintf(path, sizeof(path), "%s/persistence.db", PERMDATADIR);
return path;
}

static void persist_save_db(void)
{
UnrealDB *db;
PersistEntry *e;
Membership *mb;
int i, chan_count;
const char *path = persist_db_path();

db = unrealdb_open(path, UNREALDB_MODE_WRITE, NULL);
if (!db)
{
unreal_log(ULOG_ERROR, "persistence", "PERSIST_DB_WRITE_ERROR", NULL,
           "persistence: failed to open $path for writing",
           log_data_string("path", path));
return;
}

unrealdb_write_int64(db, PERSIST_DB_VERSION);

for (e = persist_list; e; e = e->next)
{
if (e->ghost)
{
chan_count = 0;
for (mb = e->ghost->user->channel; mb; mb = mb->next)
chan_count++;
}
else if (e->canonical && !IsDead(e->canonical) && IsUser(e->canonical))
{
chan_count = 0;
for (mb = e->canonical->user->channel; mb; mb = mb->next)
chan_count++;
}
else
{
chan_count = e->num_channels;
}

if (!chan_count && e->preference == PREF_DEFAULT)
continue;

unrealdb_write_str(db, e->account);
unrealdb_write_str(db, e->nick);
unrealdb_write_str(db, e->ident);
unrealdb_write_str(db, e->host);
unrealdb_write_str(db, e->realhost);
unrealdb_write_str(db, e->cloakedhost);
unrealdb_write_str(db, e->ip);
unrealdb_write_str(db, e->realname);
unrealdb_write_int64(db, (int64_t)e->umodes);
unrealdb_write_str(db, e->away ? e->away : "");
unrealdb_write_int64(db, (int64_t)e->disconnect_time);
unrealdb_write_int64(db, (int64_t)e->preference);

if (e->ghost)
{
unrealdb_write_int64(db, (int64_t)chan_count);
for (mb = e->ghost->user->channel; mb; mb = mb->next)
{
unrealdb_write_str(db, mb->channel->name);
unrealdb_write_str(db, mb->member_modes);
}
}
else if (e->canonical && !IsDead(e->canonical) && IsUser(e->canonical))
{
unrealdb_write_int64(db, (int64_t)chan_count);
for (mb = e->canonical->user->channel; mb; mb = mb->next)
{
unrealdb_write_str(db, mb->channel->name);
unrealdb_write_str(db, mb->member_modes);
}
}
else
{
unrealdb_write_int64(db, (int64_t)e->num_channels);
for (i = 0; i < e->num_channels; i++)
{
unrealdb_write_str(db, e->channels[i].name);
unrealdb_write_str(db, e->channels[i].modes);
}
}
}

unrealdb_write_str(db, "");
unrealdb_close(db);
}

static void persist_load_db(void)
{
UnrealDB *db;
const char *path = persist_db_path();
int64_t version;
char *account;
PersistEntry *e;

db = unrealdb_open(path, UNREALDB_MODE_READ, NULL);
if (!db)
return;

if (!unrealdb_read_int64(db, &version) || version != PERSIST_DB_VERSION)
{
unrealdb_close(db);
return;
}

account = NULL;

while (1)
{
char *nick = NULL, *ident = NULL, *host = NULL;
char *realhost = NULL, *cloakedhost = NULL, *ip = NULL;
char *realname = NULL, *away = NULL;
int64_t umodes64 = 0, disconnect_time64 = 0;
int64_t preference64 = 0, num_channels64 = 0;
int i;

safe_free(account);
account = NULL;

if (!unrealdb_read_str(db, &account))
break;
if (!account || !*account)
break;

if (!unrealdb_read_str(db, &nick)          ||
    !unrealdb_read_str(db, &ident)         ||
    !unrealdb_read_str(db, &host)          ||
    !unrealdb_read_str(db, &realhost)      ||
    !unrealdb_read_str(db, &cloakedhost)   ||
    !unrealdb_read_str(db, &ip)            ||
    !unrealdb_read_str(db, &realname)      ||
    !unrealdb_read_int64(db, &umodes64)    ||
    !unrealdb_read_str(db, &away)          ||
    !unrealdb_read_int64(db, &disconnect_time64) ||
    !unrealdb_read_int64(db, &preference64)      ||
    !unrealdb_read_int64(db, &num_channels64))
{
safe_free(nick); safe_free(ident); safe_free(host);
safe_free(realhost); safe_free(cloakedhost); safe_free(ip);
safe_free(realname); safe_free(away);
break;
}

e = get_or_create_entry(account);
strlcpy(e->nick,        nick        ? nick        : "", sizeof(e->nick));
strlcpy(e->ident,       ident       ? ident       : "", sizeof(e->ident));
strlcpy(e->host,        host        ? host        : "", sizeof(e->host));
strlcpy(e->realhost,    realhost    ? realhost    : "", sizeof(e->realhost));
strlcpy(e->cloakedhost, cloakedhost ? cloakedhost : "", sizeof(e->cloakedhost));
strlcpy(e->ip,          ip          ? ip          : "", sizeof(e->ip));
strlcpy(e->realname,    realname    ? realname    : "", sizeof(e->realname));
e->umodes          = (long)umodes64;
e->disconnect_time = (time_t)disconnect_time64;
e->preference      = (int)preference64;

safe_free(e->away);
e->away = (away && *away) ? strdup(away) : NULL;

e->num_channels = (int)num_channels64;
if (e->num_channels > PERSIST_MAX_CHANNELS)
e->num_channels = PERSIST_MAX_CHANNELS;

for (i = 0; i < e->num_channels; i++)
{
char *cname = NULL, *cmodes = NULL;
if (!unrealdb_read_str(db, &cname) ||
    !unrealdb_read_str(db, &cmodes))
{
e->num_channels = i;
safe_free(cname);
safe_free(cmodes);
break;
}
strlcpy(e->channels[i].name,  cname  ? cname  : "", sizeof(e->channels[i].name));
strlcpy(e->channels[i].modes, cmodes ? cmodes : "", sizeof(e->channels[i].modes));
safe_free(cname);
safe_free(cmodes);
}

safe_free(nick); safe_free(ident); safe_free(host);
safe_free(realhost); safe_free(cloakedhost); safe_free(ip);
safe_free(realname); safe_free(away);

if (e->num_channels > 0 &&
    (TStime() - e->disconnect_time) < persist_timeout &&
    persistence_effective(NULL, e))
{
do_create_ghost(e);
}
}

safe_free(account);
unrealdb_close(db);
}
