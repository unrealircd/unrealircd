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
 *
 * Config (obbyircd.conf):
 *   persistence {
 *       timeout 7d;    // default 7 days
 *       default on;    // on/off for logged-in users with no preference set
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
	"draft/persistence - IRCv3 ghost client persistence",
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
	Client *ghost;
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
static long                CAP_PERSISTENCE = 0L;
static long                away_notify_cap = 0L;
static long                persist_timeout    = PERSIST_DEFAULT_TIMEOUT;
static int                 persist_default_on = 1;

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
static int           persistence_effective(Client *client, PersistEntry *e);
static void          send_status(Client *client, PersistEntry *e);
static const char   *pref_to_str(int pref);
static int           str_to_pref(const char *s);
static void          persist_save_db(void);
static void          persist_load_db(void);

static int persist_local_quit(Client *client, MessageTag *mtags, const char *comment);
static int persist_account_login(Client *client, MessageTag *mtags);
static int persist_welcome(Client *client, int after_numeric);
static int persist_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
static int persist_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
static int persist_configposttest(int *errs);
CMD_FUNC(cmd_persistence);
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

	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT,    0, persist_local_quit);
	HookAdd(modinfo->handle, HOOKTYPE_ACCOUNT_LOGIN, 0, persist_account_login);
	HookAdd(modinfo->handle, HOOKTYPE_WELCOME,       0, persist_welcome);

	EventAdd(modinfo->handle, "persist_cleanup", ghost_cleanup_event, NULL,
	         PERSIST_CLEANUP_INTERVAL_MS, 0);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	/* Look up the away-notify capability bit once so we can filter
	 * sendto_local_common_channels to clients that requested it. */
	away_notify_cap = ClientCapabilityBit("away-notify");

	persist_load_db();
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	PersistEntry *e, *enext;

	persist_save_db();

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

	/* make_client(NULL, &me) creates a client with LocalClient (local->fd=-1 initially).
	 * We set fd=-2 so all sends silently drop at the "fd < 0" guard.
	 * The client is NOT added to lclient_list, so check_deadsockets ignores it. */
	ghost = make_client(NULL, &me);
	ghost->local->fd = -2;

	strlcpy(ghost->name, e->nick, sizeof(ghost->name));

	if (*e->ip)
		safe_strdup(ghost->ip, e->ip);

	/* make_user initialises realhost from ghost->ip, then we override */
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

	/* Mark as away so WHO/WHOIS shows offline status */
	safe_strdup(ghost->user->away, "User is offline (persistent)");
	ghost->user->away_since = TStime();

	/* Tag as ghost; our LOCAL_QUIT hook skips ghost clients */
	moddata_client(ghost, ghost_md).i = 1;

	/* Register in nick hash + global client list.
	 * NOT added to lclient_list — prevents dead-socket processing. */
	add_to_client_hash_table(ghost->name, ghost);
	add_client_to_list(ghost);

	/* Adjust global counters to balance exit_client decrements */
	irccounts.clients++;
	irccounts.me_clients++;
	if (IsInvisible(ghost))
		irccounts.invisible++;
	if (ghost->uplink && ghost->uplink->server)
		ghost->uplink->server->users++;

	/* Place ghost into channels, creating them if they do not exist yet
	 * (e.g. after a server restart before any real users have joined). */
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

	ghost  = e->ghost;
	e->ghost = NULL;

	/* Silently strip channels — no QUIT broadcast to anyone */
	while ((mp = ghost->user->channel))
		remove_user_from_channel_withmb(ghost, mp->channel, mp, 1);

	/* Free module data, remove from hash tables and lists, free memory */
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
		moddata_client(client, restore_md).ptr = NULL;
		return;
	}

	/* Determine whether the reconnecting client reclaimed their persistent
	 * nick.  When same_nick=1 the ghost held that exact slot so we silently
	 * transfer ownership (no JOIN/PART visible to others).  When same_nick=0
	 * the client connected with a different nick, so we do a normal
	 * broadcast JOIN so other users' NAMES lists stay accurate. */
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
			/* Ghost occupied this slot with the same nick — silently transfer.
			 * Channel members see nothing; their NAMES list is already correct.
			 * Only the reconnecting client receives JOIN + topic + NAMES. */
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
			/* Different nick — broadcast a normal JOIN so channel members
			 * know this new nick is present. */
			MessageTag *jmtags = NULL;
			new_message(client, NULL, &jmtags);
			join_channel(channel, client, jmtags, e->channels[i].modes);
			free_message_tags(jmtags);
			if (IsDead(client))
				break;
		}
	}

	/* For the same-nick case: broadcast AWAY unset to away-notify clients so
	 * they see the user come back from away.  Also clear the client's own
	 * away message since they are now actively connected. */
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

	if (is_ghost_client(client))
		return 0;   /* ghosts exit normally without re-saving */

	if (!IsUser(client) || !MyUser(client))
		return 0;

	if (!IsLoggedIn(client))
		return 0;

	e = find_entry(client->user->account);
	if (!persistence_effective(client, e))
		return 0;

	e = get_or_create_entry(client->user->account);

	/* Destroy any stale ghost for this account */
	if (e->ghost)
		destroy_ghost(e, NULL);

	/* Capture current state (nick, channels, modes, away, etc.) */
	save_client_to_entry(client, e);

	/* Step 1: Remove the real client from the nick hash table so the ghost
	 * can immediately claim the same nick.  exit_one_client checks
	 * *client->name before calling del_from_client_hash_table, so we blank
	 * the name at step 5 to prevent a double-removal. */
	del_from_client_hash_table(client->name, client);

	/* Step 2: Create the ghost synchronously.  It takes the now-free nick
	 * and joins all saved channels (creating empty channels if needed, e.g.
	 * after a server restart).  This is atomic — there is no window where
	 * the nick or channel slot is vacant. */
	do_create_ghost(e);

	/* Step 3: Ghost is now present in all channels.  Broadcast AWAY to
	 * clients that have the away-notify capability so they see the user go
	 * away rather than quit. */
	if (e->ghost)
	{
		away_mtags = NULL;
		new_message(e->ghost, NULL, &away_mtags);
		sendto_local_common_channels(e->ghost, NULL, away_notify_cap, away_mtags,
		                             ":%s AWAY :%s", e->ghost->name,
		                             "User is offline (persistent)");
		free_message_tags(away_mtags);
	}

	/* Step 4: Silently remove the real client from all shared channels.
	 * The ghost already occupies those slots, so exit_one_client's
	 * sendto_local_common_channels will find no common channels and
	 * will not broadcast QUIT to anyone. */
	for (mb = client->user->channel; mb; mb = mb_next)
	{
		mb_next = mb->next;
		remove_user_from_channel_withmb(client, mb->channel, mb, 1);
	}

	/* Step 5: Blank the nick so exit_one_client skips the hash removal
	 * (it guards with `if (*client->name)`). */
	*client->name = '\0';

	persist_save_db();

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
		return 0;   /* logout event or pre-auth */

	e = find_entry(client->user->account);
	if (!e)
		return 0;

	/* Reclaim a live ghost */
	if (e->ghost)
	{
		char old_nick[NICKLEN + 1];

		unreal_log(ULOG_INFO, "persistence", "PERSIST_RECLAIM", client,
		           "Reclaiming persistent ghost for account $account",
		           log_data_string("account", e->account));

		/* Snapshot ghost's current channel list, then destroy it silently */
		i = 0;
		for (mb = e->ghost->user->channel; mb && i < PERSIST_MAX_CHANNELS; mb = mb->next, i++)
		{
			strlcpy(e->channels[i].name,  mb->channel->name, sizeof(e->channels[i].name));
			strlcpy(e->channels[i].modes, mb->member_modes,  sizeof(e->channels[i].modes));
		}
		e->num_channels = i;

		destroy_ghost(e, NULL);   /* silent: no QUIT broadcast */

		if (IsDead(client))
			return 0;

		/* Nick reclaim: if the client is still in the registration phase
		 * (before 001) and had to use a fallback nick because the ghost was
		 * holding their account nick, silently rename them now that the nick
		 * hash slot is free.  No NICK broadcast is needed because the client
		 * hasn't joined any channels yet. */
		if (!IsRegistered(client) &&
		    strcmp(client->name, e->nick) != 0 &&
		    !find_client(e->nick, NULL))
		{
			strlcpy(old_nick, client->name, sizeof(old_nick));
			del_from_client_hash_table(client->name, client);
			strlcpy(client->name, e->nick, sizeof(client->name));
			add_to_client_hash_table(client->name, client);
			/* Inform the client of their new nick */
			sendto_one(client, NULL, ":%s NICK :%s", old_nick, e->nick);
		}
	}

	/* Queue channel restoration for HOOKTYPE_WELCOME(376) */
	moddata_client(client, restore_md).ptr = (void *)e;

	/* If already registered (NickServ login post-burst), restore now */
	if (IsRegistered(client))
	{
		send_status(client, e);
		restore_channels(client, e);
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
		/* After last 005 ISUPPORT — send STATUS if CAP is active and logged in */
		if (!(client->local->caps & CAP_PERSISTENCE))
			return 0;
		if (!IsLoggedIn(client))
			return 0;
		e = find_entry(client->user->account);
		send_status(client, e);
	}
	else if (after_numeric == 376)
	{
		/* After ENDOFMOTD — restore channels */
		e = (PersistEntry *)moddata_client(client, restore_md).ptr;
		if (e)
			restore_channels(client, e);
	}

	return 0;
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
	/* Unknown subcommands silently ignored per spec */
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
		} else {
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
		} else {
			unrealdb_write_int64(db, (int64_t)e->num_channels);
			for (i = 0; i < e->num_channels; i++)
			{
				unrealdb_write_str(db, e->channels[i].name);
				unrealdb_write_str(db, e->channels[i].modes);
			}
		}
	}

	unrealdb_write_str(db, "");   /* end-of-records sentinel */
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
			/* Create the ghost immediately.  At server startup channels
			 * do not yet exist, but do_create_ghost calls make_channel
			 * to pre-create them so the ghost's presence is visible as
			 * soon as real users join. */
			do_create_ghost(e);
		}
	}

	safe_free(account);
	unrealdb_close(db);
}
