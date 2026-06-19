/* stats.* RPC calls - Comprehensive Server Statistics
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 *
 * This module provides comprehensive server statistics with historical
 * snapshot support for graphing and monitoring purposes.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/stats",
	"2.0.0",
	"stats.* RPC calls - Comprehensive Statistics",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Database version for persistence */
#define STATSDB_VERSION 1
#define STATSDB_FILENAME "stats.db"

/* Configuration: how many hourly snapshots to keep */
#define STATS_HISTORY_SIZE 168  /* 7 days of hourly snapshots */
#define STATS_SNAPSHOT_INTERVAL (10 * 60 * 1000)  /* 10 minutes in ms */

/* Macros for database operations */
#define WARN_WRITE_ERROR(fname) \
	do { \
		unreal_log(ULOG_ERROR, "rpc/stats", "STATSDB_FILE_WRITE_ERROR", NULL, \
			   "[rpc/stats] Error writing to temporary database file $filename: $system_error", \
			   log_data_string("filename", fname), \
			   log_data_string("system_error", unrealdb_get_error_string())); \
	} while(0)

#define W_SAFE(x) \
	do { \
		if (!(x)) { \
			WARN_WRITE_ERROR(tmpfname); \
			unrealdb_close(db); \
			return 0; \
		} \
	} while(0)

#define R_SAFE(x) \
	do { \
		if (!(x)) { \
			config_warn("[rpc/stats] Error reading database '%s'", cfg.database); \
			unrealdb_close(db); \
			return 0; \
		} \
	} while(0)

/* Forward declarations */
void rpc_stats_get(Client *client, json_t *request, json_t *params);
void rpc_stats_history(Client *client, json_t *request, json_t *params);
EVENT(stats_snapshot_event);
EVENT(stats_delayed_init);

/* Statistics snapshot structure - captures server state at a point in time */
typedef struct StatsSnapshot {
	time_t timestamp;
	/* User statistics */
	int users_total;
	int users_invisible;
	int users_opers;
	int users_unknown;
	int users_local;
	int users_local_max;
	int users_global_max;
	/* Server statistics */
	int servers_total;
	int servers_ulined;
	/* Channel statistics */
	int channels_total;
	/* Traffic statistics */
	long long traffic_bytes_sent;
	long long traffic_bytes_received;
	long long traffic_messages_sent;
	long long traffic_messages_received;
	/* Connection statistics */
	unsigned int conn_total_accepted;
	unsigned int conn_total_refused;
	unsigned int conn_clients;
	unsigned int conn_servers;
	unsigned int conn_unknown;
	/* Auth statistics */
	unsigned int auth_success;
	unsigned int auth_fail;
	/* TKL/Ban statistics */
	int tkl_total;
	int tkl_gline;
	int tkl_gzline;
	int tkl_kline;
	int tkl_zline;
	int tkl_shun;
	int tkl_spamfilter;
	int tkl_qline;
	int tkl_except;
	/* Additional statistics */
	unsigned int protocol_errors;
	unsigned int nick_collisions;
} StatsSnapshot;

/* Configuration structure */
struct cfgstruct {
	char *database;
};

/* Circular buffer for historical snapshots */
static StatsSnapshot *stats_history = NULL;
static int stats_history_count = 0;
static int stats_history_index = 0;
static int stats_initialized = 0;

/* Configuration */
static struct cfgstruct cfg;

/* Helper function prototypes */
static void json_expand_countries(json_t *main, const char *name, NameValuePrioList *geo);
static void json_expand_asns(json_t *main, const char *name, NameValuePrioList *asn_list);
static void collect_user_stats(json_t *parent, int detail);
static void collect_channel_stats(json_t *parent, int detail);
static void collect_server_stats(json_t *parent, int detail);
static void collect_traffic_stats(json_t *parent);
static void collect_connection_stats(json_t *parent);
static void collect_tkl_stats(json_t *parent, int detail);
static void collect_command_stats(json_t *parent);
static void collect_system_stats(json_t *parent);
static void take_snapshot(StatsSnapshot *snap);
static json_t *snapshot_to_json(StatsSnapshot *snap);
static int json_array_contains_string(json_t *array, const char *str);
static int write_statsdb(void);
static int read_statsdb(void);
static void setcfg(void);
static void freecfg(void);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	/* Initialize configuration */
	setcfg();

	/* Initialize history buffer */
	stats_history = safe_alloc(sizeof(StatsSnapshot) * STATS_HISTORY_SIZE);
	stats_history_count = 0;
	stats_history_index = 0;
	stats_initialized = 0;

	/* Register RPC handlers */
	memset(&r, 0, sizeof(r));
	r.method = "stats.get";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_stats_get;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/stats] Could not register RPC handler for stats.get");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "stats.history";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_stats_history;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/stats] Could not register RPC handler for stats.history");
		return MOD_FAILED;
	}

	/* Schedule hourly snapshot event */
	EventAdd(modinfo->handle, "stats_snapshot", stats_snapshot_event, NULL, STATS_SNAPSHOT_INTERVAL, 0);

	/* Schedule delayed initialization - runs once after 5 seconds to let server fully load */
	EventAdd(modinfo->handle, "stats_delayed_init", stats_delayed_init, NULL, 5000, 1);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	/* Try to read existing database */
	read_statsdb();
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	/* Save database on unload if we're shutting down */
	if (loop.terminating && stats_initialized)
		write_statsdb();

	safe_free(stats_history);
	freecfg();
	return MOD_SUCCESS;
}

/** Set default configuration */
static void setcfg(void)
{
	safe_strdup(cfg.database, STATSDB_FILENAME);
	convert_to_absolute_path(&cfg.database, PERMDATADIR);
}

/** Free configuration */
static void freecfg(void)
{
	safe_free(cfg.database);
}

/** Delayed initialization event - runs once after server is fully loaded */
EVENT(stats_delayed_init)
{
	if (!stats_initialized)
	{
		/* Take initial snapshot now that server is fully loaded */
		take_snapshot(&stats_history[stats_history_index]);
		if (stats_history_count == 0)
			stats_history_count = 1;
		stats_initialized = 1;
		unreal_log(ULOG_DEBUG, "rpcstats", "STATS_INIT", NULL,
			   "[rpc/stats] Initial snapshot taken (users=$users, channels=$channels, servers=$servers)",
			   log_data_integer("users", stats_history[stats_history_index].users_total),
			   log_data_integer("channels", stats_history[stats_history_index].channels_total),
			   log_data_integer("servers", stats_history[stats_history_index].servers_total));
	}
}

/** Event handler for taking periodic snapshots */
EVENT(stats_snapshot_event)
{
	if (!stats_initialized)
		return;

	stats_history_index = (stats_history_index + 1) % STATS_HISTORY_SIZE;
	take_snapshot(&stats_history[stats_history_index]);
	if (stats_history_count < STATS_HISTORY_SIZE)
		stats_history_count++;

	/* Save to database periodically */
	write_statsdb();
}

/** Write stats database to disk */
static int write_statsdb(void)
{
	UnrealDB *db;
	char tmpfname[512];
	int i, idx;

	if (stats_history_count == 0)
		return 1; /* Nothing to save */

	/* Write to temporary file first */
	snprintf(tmpfname, sizeof(tmpfname), "%s.%x.tmp", cfg.database, getrandom32());

	db = unrealdb_open(tmpfname, UNREALDB_MODE_WRITE, NULL);
	if (!db)
	{
		WARN_WRITE_ERROR(tmpfname);
		return 0;
	}

	/* Write header */
	W_SAFE(unrealdb_write_int32(db, STATSDB_VERSION));
	W_SAFE(unrealdb_write_int32(db, stats_history_count));
	W_SAFE(unrealdb_write_int32(db, stats_history_index));

	/* Write snapshots from oldest to newest */
	for (i = 0; i < stats_history_count; i++)
	{
		/* Calculate actual index - go from oldest to newest */
		if (stats_history_count < STATS_HISTORY_SIZE)
			idx = i;
		else
			idx = (stats_history_index + 1 + i) % STATS_HISTORY_SIZE;

		StatsSnapshot *snap = &stats_history[idx];

		W_SAFE(unrealdb_write_int64(db, snap->timestamp));
		W_SAFE(unrealdb_write_int32(db, snap->users_total));
		W_SAFE(unrealdb_write_int32(db, snap->users_invisible));
		W_SAFE(unrealdb_write_int32(db, snap->users_opers));
		W_SAFE(unrealdb_write_int32(db, snap->users_unknown));
		W_SAFE(unrealdb_write_int32(db, snap->users_local));
		W_SAFE(unrealdb_write_int32(db, snap->users_local_max));
		W_SAFE(unrealdb_write_int32(db, snap->users_global_max));
		W_SAFE(unrealdb_write_int32(db, snap->servers_total));
		W_SAFE(unrealdb_write_int32(db, snap->servers_ulined));
		W_SAFE(unrealdb_write_int32(db, snap->channels_total));
		W_SAFE(unrealdb_write_int64(db, snap->traffic_bytes_sent));
		W_SAFE(unrealdb_write_int64(db, snap->traffic_bytes_received));
		W_SAFE(unrealdb_write_int64(db, snap->traffic_messages_sent));
		W_SAFE(unrealdb_write_int64(db, snap->traffic_messages_received));
		W_SAFE(unrealdb_write_int32(db, snap->conn_total_accepted));
		W_SAFE(unrealdb_write_int32(db, snap->conn_total_refused));
		W_SAFE(unrealdb_write_int32(db, snap->conn_clients));
		W_SAFE(unrealdb_write_int32(db, snap->conn_servers));
		W_SAFE(unrealdb_write_int32(db, snap->conn_unknown));
		W_SAFE(unrealdb_write_int32(db, snap->auth_success));
		W_SAFE(unrealdb_write_int32(db, snap->auth_fail));
		W_SAFE(unrealdb_write_int32(db, snap->tkl_total));
		W_SAFE(unrealdb_write_int32(db, snap->tkl_gline));
		W_SAFE(unrealdb_write_int32(db, snap->tkl_gzline));
		W_SAFE(unrealdb_write_int32(db, snap->tkl_kline));
		W_SAFE(unrealdb_write_int32(db, snap->tkl_zline));
		W_SAFE(unrealdb_write_int32(db, snap->tkl_shun));
		W_SAFE(unrealdb_write_int32(db, snap->tkl_spamfilter));
		W_SAFE(unrealdb_write_int32(db, snap->tkl_qline));
		W_SAFE(unrealdb_write_int32(db, snap->tkl_except));
		W_SAFE(unrealdb_write_int32(db, snap->protocol_errors));
		W_SAFE(unrealdb_write_int32(db, snap->nick_collisions));
	}

	if (!unrealdb_close(db))
	{
		WARN_WRITE_ERROR(tmpfname);
		return 0;
	}

	/* Atomic rename */
#ifdef _WIN32
	unlink(cfg.database);
#endif
	if (rename(tmpfname, cfg.database) < 0)
	{
		config_error("[rpc/stats] ERROR renaming '%s' to '%s': %s",
			tmpfname, cfg.database, strerror(ERRNO));
		return 0;
	}

	return 1;
}

/** Read stats database from disk */
static int read_statsdb(void)
{
	UnrealDB *db;
	uint32_t version;
	uint32_t count;
	uint32_t index;
	int i;

	db = unrealdb_open(cfg.database, UNREALDB_MODE_READ, NULL);
	if (!db)
	{
		/* File not found is OK - first run */
		if (unrealdb_get_error_code() == UNREALDB_ERROR_FILENOTFOUND)
			return 1;
		config_warn("[rpc/stats] Could not open database '%s': %s",
			cfg.database, unrealdb_get_error_string());
		return 0;
	}

	/* Read header */
	R_SAFE(unrealdb_read_int32(db, &version));
	if (version != STATSDB_VERSION)
	{
		config_warn("[rpc/stats] Database version mismatch (got %u, expected %u), starting fresh",
			version, STATSDB_VERSION);
		unrealdb_close(db);
		return 1;
	}

	R_SAFE(unrealdb_read_int32(db, &count));
	R_SAFE(unrealdb_read_int32(db, &index));

	if (count > STATS_HISTORY_SIZE)
		count = STATS_HISTORY_SIZE;

	/* Read snapshots */
	for (i = 0; i < (int)count; i++)
	{
		StatsSnapshot *snap = &stats_history[i];
		uint64_t tmp64;
		uint32_t tmp32;

		R_SAFE(unrealdb_read_int64(db, &tmp64)); snap->timestamp = tmp64;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->users_total = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->users_invisible = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->users_opers = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->users_unknown = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->users_local = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->users_local_max = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->users_global_max = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->servers_total = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->servers_ulined = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->channels_total = tmp32;
		R_SAFE(unrealdb_read_int64(db, &tmp64)); snap->traffic_bytes_sent = tmp64;
		R_SAFE(unrealdb_read_int64(db, &tmp64)); snap->traffic_bytes_received = tmp64;
		R_SAFE(unrealdb_read_int64(db, &tmp64)); snap->traffic_messages_sent = tmp64;
		R_SAFE(unrealdb_read_int64(db, &tmp64)); snap->traffic_messages_received = tmp64;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->conn_total_accepted = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->conn_total_refused = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->conn_clients = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->conn_servers = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->conn_unknown = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->auth_success = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->auth_fail = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->tkl_total = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->tkl_gline = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->tkl_gzline = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->tkl_kline = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->tkl_zline = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->tkl_shun = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->tkl_spamfilter = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->tkl_qline = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->tkl_except = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->protocol_errors = tmp32;
		R_SAFE(unrealdb_read_int32(db, &tmp32)); snap->nick_collisions = tmp32;
	}

	stats_history_count = count;
	stats_history_index = (count > 0) ? count - 1 : 0;

	unrealdb_close(db);

	unreal_log(ULOG_DEBUG, "rpcstats", "STATSDB_LOADED", NULL,
		   "[rpc/stats] Loaded $count historical snapshots from database",
		   log_data_integer("count", count));

	return 1;
}

/** Take a snapshot of current server statistics */
static void take_snapshot(StatsSnapshot *snap)
{
	Client *client;
	Channel *channel;
	TKL *tkl;
	int index, index2;

	memset(snap, 0, sizeof(StatsSnapshot));
	snap->timestamp = TStime();

	/* User counts - count manually for accuracy */
	list_for_each_entry(client, &client_list, client_node)
	{
		if (IsUser(client))
		{
			snap->users_total++;
			if (IsInvisible(client))
				snap->users_invisible++;
			if (IsOper(client))
				snap->users_opers++;
		}
	}

	/* Unknown connections */
	list_for_each_entry(client, &unknown_list, lclient_node)
	{
		snap->users_unknown++;
	}

	/* Local user count */
	snap->users_local = irccounts.me_clients;
	snap->users_local_max = irccounts.me_max;
	snap->users_global_max = irccounts.global_max;

	/* Server counts - count manually */
	snap->servers_total = 1; /* ourselves */
	list_for_each_entry(client, &global_server_list, client_node)
	{
		if (IsServer(client))
		{
			snap->servers_total++;
			if (IsULine(client))
				snap->servers_ulined++;
		}
	}

	/* Channel counts - iterate through channel list */
	for (channel = channels; channel; channel = channel->nextch)
	{
		snap->channels_total++;
	}

	/* Traffic stats */
	snap->traffic_bytes_sent = me.local->traffic.bytes_sent;
	snap->traffic_bytes_received = me.local->traffic.bytes_received;
	snap->traffic_messages_sent = me.local->traffic.messages_sent;
	snap->traffic_messages_received = me.local->traffic.messages_received;

	/* Connection stats from ircstats */
	snap->conn_total_accepted = ircstats.is_ac;
	snap->conn_total_refused = ircstats.is_ref;
	snap->conn_clients = ircstats.is_cl;
	snap->conn_servers = ircstats.is_sv;
	snap->conn_unknown = ircstats.is_ni;

	/* Auth stats */
	snap->auth_success = ircstats.is_asuc;
	snap->auth_fail = ircstats.is_abad;

	/* Protocol stats */
	snap->protocol_errors = ircstats.is_unco + ircstats.is_wrdi + ircstats.is_unpf + ircstats.is_empt;
	snap->nick_collisions = ircstats.is_kill;

	/* TKL statistics - iterate through both hash tables */
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				snap->tkl_total++;
				if (TKLIsSpamfilter(tkl))
					snap->tkl_spamfilter++;
				else if (TKLIsBanException(tkl))
					snap->tkl_except++;
				else if (TKLIsNameBan(tkl))
					snap->tkl_qline++;
				else if (tkl->type == (TKL_KILL | TKL_GLOBAL))
					snap->tkl_gline++;
				else if (tkl->type == (TKL_ZAP | TKL_GLOBAL))
					snap->tkl_gzline++;
				else if (tkl->type == TKL_KILL)
					snap->tkl_kline++;
				else if (tkl->type == TKL_ZAP)
					snap->tkl_zline++;
				else if (tkl->type == (TKL_SHUN | TKL_GLOBAL))
					snap->tkl_shun++;
			}
		}
	}

	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			snap->tkl_total++;
			if (TKLIsSpamfilter(tkl))
				snap->tkl_spamfilter++;
			else if (TKLIsBanException(tkl))
				snap->tkl_except++;
			else if (TKLIsNameBan(tkl))
				snap->tkl_qline++;
			else if (tkl->type == (TKL_KILL | TKL_GLOBAL))
				snap->tkl_gline++;
			else if (tkl->type == (TKL_ZAP | TKL_GLOBAL))
				snap->tkl_gzline++;
			else if (tkl->type == TKL_KILL)
				snap->tkl_kline++;
			else if (tkl->type == TKL_ZAP)
				snap->tkl_zline++;
			else if (tkl->type == (TKL_SHUN | TKL_GLOBAL))
				snap->tkl_shun++;
		}
	}
}

/** Convert a snapshot to JSON */
static json_t *snapshot_to_json(StatsSnapshot *snap)
{
	json_t *obj = json_object();
	json_t *users, *servers, *channels, *traffic, *connections, *tkl;

	json_object_set_new(obj, "timestamp", json_timestamp(snap->timestamp));

	/* Users */
	users = json_object();
	json_object_set_new(users, "total", json_integer(snap->users_total));
	json_object_set_new(users, "invisible", json_integer(snap->users_invisible));
	json_object_set_new(users, "operators", json_integer(snap->users_opers));
	json_object_set_new(users, "unknown", json_integer(snap->users_unknown));
	json_object_set_new(users, "local", json_integer(snap->users_local));
	json_object_set_new(users, "local_max", json_integer(snap->users_local_max));
	json_object_set_new(users, "global_max", json_integer(snap->users_global_max));
	json_object_set_new(obj, "users", users);

	/* Servers */
	servers = json_object();
	json_object_set_new(servers, "total", json_integer(snap->servers_total));
	json_object_set_new(servers, "ulined", json_integer(snap->servers_ulined));
	json_object_set_new(obj, "servers", servers);

	/* Channels */
	channels = json_object();
	json_object_set_new(channels, "total", json_integer(snap->channels_total));
	json_object_set_new(obj, "channels", channels);

	/* Traffic */
	traffic = json_object();
	json_object_set_new(traffic, "bytes_sent", json_integer(snap->traffic_bytes_sent));
	json_object_set_new(traffic, "bytes_received", json_integer(snap->traffic_bytes_received));
	json_object_set_new(traffic, "messages_sent", json_integer(snap->traffic_messages_sent));
	json_object_set_new(traffic, "messages_received", json_integer(snap->traffic_messages_received));
	json_object_set_new(obj, "traffic", traffic);

	/* Connections */
	connections = json_object();
	json_object_set_new(connections, "total_accepted", json_integer(snap->conn_total_accepted));
	json_object_set_new(connections, "total_refused", json_integer(snap->conn_total_refused));
	json_object_set_new(connections, "clients", json_integer(snap->conn_clients));
	json_object_set_new(connections, "servers", json_integer(snap->conn_servers));
	json_object_set_new(connections, "unknown", json_integer(snap->conn_unknown));
	json_object_set_new(connections, "auth_success", json_integer(snap->auth_success));
	json_object_set_new(connections, "auth_fail", json_integer(snap->auth_fail));
	json_object_set_new(connections, "protocol_errors", json_integer(snap->protocol_errors));
	json_object_set_new(connections, "nick_collisions", json_integer(snap->nick_collisions));
	json_object_set_new(obj, "connections", connections);

	/* TKL */
	tkl = json_object();
	json_object_set_new(tkl, "total", json_integer(snap->tkl_total));
	json_object_set_new(tkl, "gline", json_integer(snap->tkl_gline));
	json_object_set_new(tkl, "gzline", json_integer(snap->tkl_gzline));
	json_object_set_new(tkl, "kline", json_integer(snap->tkl_kline));
	json_object_set_new(tkl, "zline", json_integer(snap->tkl_zline));
	json_object_set_new(tkl, "shun", json_integer(snap->tkl_shun));
	json_object_set_new(tkl, "spamfilter", json_integer(snap->tkl_spamfilter));
	json_object_set_new(tkl, "qline", json_integer(snap->tkl_qline));
	json_object_set_new(tkl, "except", json_integer(snap->tkl_except));
	json_object_set_new(obj, "server_ban", tkl);

	return obj;
}

/** Expand countries list to JSON */
static void json_expand_countries(json_t *main, const char *name, NameValuePrioList *geo)
{
	json_t *list = json_array();
	json_t *item;

	json_object_set_new(main, name, list);

	for (; geo; geo = geo->next)
	{
		item = json_object();
		json_object_set_new(item, "country", json_string_unreal(geo->name));
		json_object_set_new(item, "count", json_integer(0 - geo->priority));
		json_array_append_new(list, item);
	}
}

/** Expand ASN list to JSON */
static void json_expand_asns(json_t *main, const char *name, NameValuePrioList *asn_list)
{
	json_t *list = json_array();
	json_t *item;

	json_object_set_new(main, name, list);

	for (; asn_list; asn_list = asn_list->next)
	{
		item = json_object();
		json_object_set_new(item, "asn", json_string_unreal(asn_list->name));
		if (asn_list->value)
			json_object_set_new(item, "name", json_string_unreal(asn_list->value));
		json_object_set_new(item, "count", json_integer(0 - asn_list->priority));
		json_array_append_new(list, item);
	}
}

/** Collect detailed user statistics */
static void collect_user_stats(json_t *parent, int detail)
{
	Client *client;
	int total = 0, ulined = 0, oper = 0, tls_users = 0, identified = 0;
	int websocket = 0, invisible = 0;
	json_t *child;
	GeoIPResult *geo;
	NameValuePrioList *countries = NULL;
	NameValuePrioList *asns = NULL;

	child = json_object();
	json_object_set_new(parent, "user", child);

	list_for_each_entry(client, &client_list, client_node)
	{
		if (IsUser(client))
		{
			total++;
			if (IsULine(client))
				ulined++;
			if (IsOper(client))
				oper++;
			if (IsInvisible(client))
				invisible++;
			if (IsSecure(client))
				tls_users++;
			if (IsLoggedIn(client))
				identified++;
			if (client->local && moddata_client_get(client, "websocket"))
				websocket++;

			if (detail >= 1)
			{
				geo = geoip_client(client);
				if (geo)
				{
					/* Country tracking */
					if (geo->country_code)
					{
						NameValuePrioList *e = find_nvplist(countries, geo->country_code);
						if (e)
						{
							DelListItem(e, countries);
							e->priority--;
							AddListItemPrio(e, countries, e->priority);
						} else {
							add_nvplist(&countries, -1, geo->country_code, NULL);
						}
					}
					/* ASN tracking */
					if (detail >= 2 && geo->asn > 0)
					{
						char asn_str[32];
						snprintf(asn_str, sizeof(asn_str), "%u", geo->asn);
						NameValuePrioList *e = find_nvplist(asns, asn_str);
						if (e)
						{
							DelListItem(e, asns);
							e->priority--;
							AddListItemPrio(e, asns, e->priority);
						} else {
							add_nvplist(&asns, -1, asn_str, geo->asname);
						}
					}
				}
			}
		}
	}

	json_object_set_new(child, "total", json_integer(total));
	json_object_set_new(child, "ulined", json_integer(ulined));
	json_object_set_new(child, "oper", json_integer(oper));
	json_object_set_new(child, "invisible", json_integer(invisible));
	json_object_set_new(child, "tls", json_integer(tls_users));
	json_object_set_new(child, "identified", json_integer(identified));
	json_object_set_new(child, "websocket", json_integer(websocket));
	json_object_set_new(child, "local", json_integer(irccounts.me_clients));
	json_object_set_new(child, "unknown_connections", json_integer(irccounts.unknown));
	json_object_set_new(child, "record_local", json_integer(irccounts.me_max));
	json_object_set_new(child, "record_global", json_integer(irccounts.global_max));

	if (detail >= 1)
		json_expand_countries(child, "countries", countries);
	if (detail >= 2)
		json_expand_asns(child, "asns", asns);

	safe_free_nvplist(countries);
	safe_free_nvplist(asns);
}

/** Collect detailed channel statistics */
static void collect_channel_stats(json_t *parent, int detail)
{
	Channel *channel;
	json_t *child;
	int total = 0, secret = 0, private_chans = 0, moderated = 0;
	int total_members = 0, total_bans = 0, total_excepts = 0, total_invex = 0;
	int largest_channel = 0;
	char largest_name[CHANNELLEN + 1] = "";

	child = json_object();
	json_object_set_new(parent, "channel", child);

	for (channel = channels; channel; channel = channel->nextch)
	{
		Ban *b;
		total++;
		total_members += channel->users;

		if (channel->users > largest_channel)
		{
			largest_channel = channel->users;
			strlcpy(largest_name, channel->name, sizeof(largest_name));
		}

		if (has_channel_mode(channel, 's'))
			secret++;
		if (has_channel_mode(channel, 'p'))
			private_chans++;
		if (has_channel_mode(channel, 'm'))
			moderated++;

		if (detail >= 2)
		{
			for (b = channel->banlist; b; b = b->next)
				total_bans++;
			for (b = channel->exlist; b; b = b->next)
				total_excepts++;
			for (b = channel->invexlist; b; b = b->next)
				total_invex++;
		}
	}

	json_object_set_new(child, "total", json_integer(total));
	json_object_set_new(child, "secret", json_integer(secret));
	json_object_set_new(child, "private", json_integer(private_chans));
	json_object_set_new(child, "moderated", json_integer(moderated));
	json_object_set_new(child, "total_memberships", json_integer(total_members));
	if (detail >= 1 && total > 0)
	{
		json_object_set_new(child, "average_users", json_real((double)total_members / total));
		json_object_set_new(child, "largest_size", json_integer(largest_channel));
		if (*largest_name)
			json_object_set_new(child, "largest_channel", json_string_unreal(largest_name));
	}
	if (detail >= 2)
	{
		json_object_set_new(child, "total_bans", json_integer(total_bans));
		json_object_set_new(child, "total_ban_exceptions", json_integer(total_excepts));
		json_object_set_new(child, "total_invite_exceptions", json_integer(total_invex));
	}
}

/** Collect detailed server statistics */
static void collect_server_stats(json_t *parent, int detail)
{
	Client *client;
	json_t *child, *servers_list;
	int total = 1, ulined = 0;  /* Start with 1 for ourselves */
	long total_users = 0;

	child = json_object();
	json_object_set_new(parent, "server", child);

	if (detail >= 2)
		servers_list = json_array();
	else
		servers_list = NULL;

	list_for_each_entry(client, &global_server_list, client_node)
	{
		if (IsServer(client))
		{
			total++;
			if (IsULine(client))
				ulined++;
			if (client->server)
				total_users += client->server->users;

			if (detail >= 2)
			{
				json_t *srv = json_object();
				json_object_set_new(srv, "name", json_string_unreal(client->name));
				json_object_set_new(srv, "ulined", json_boolean(IsULine(client)));
				if (client->server)
				{
					json_object_set_new(srv, "users", json_integer(client->server->users));
					if (client->server->boottime)
						json_object_set_new(srv, "boot_time", json_timestamp(client->server->boottime));
					if (client->server->features.software)
						json_object_set_new(srv, "software", json_string_unreal(client->server->features.software));
				}
				json_array_append_new(servers_list, srv);
			}
		}
	}

	json_object_set_new(child, "total", json_integer(total));
	json_object_set_new(child, "ulined", json_integer(ulined));
	json_object_set_new(child, "local_servers", json_integer(irccounts.me_servers));

	if (detail >= 1)
	{
		json_object_set_new(child, "total_remote_users", json_integer(total_users));
		json_object_set_new(child, "boot_time", json_timestamp(me.server->boottime));
		json_object_set_new(child, "uptime_seconds", json_integer(TStime() - me.server->boottime));
		if (me.server->features.software)
			json_object_set_new(child, "software", json_string_unreal(me.server->features.software));
	}

	if (detail >= 2 && servers_list)
		json_object_set_new(child, "list", servers_list);
	else if (servers_list)
		json_decref(servers_list);
}

/** Collect traffic statistics */
static void collect_traffic_stats(json_t *parent)
{
	json_t *child = json_object();
	json_object_set_new(parent, "traffic", child);

	json_object_set_new(child, "bytes_sent", json_integer(me.local->traffic.bytes_sent));
	json_object_set_new(child, "bytes_received", json_integer(me.local->traffic.bytes_received));
	json_object_set_new(child, "messages_sent", json_integer(me.local->traffic.messages_sent));
	json_object_set_new(child, "messages_received", json_integer(me.local->traffic.messages_received));

	/* Calculate rates if we have uptime */
	if (me.server->boottime > 0)
	{
		time_t uptime = TStime() - me.server->boottime;
		if (uptime > 0)
		{
			json_object_set_new(child, "bytes_sent_per_second", json_real((double)me.local->traffic.bytes_sent / uptime));
			json_object_set_new(child, "bytes_received_per_second", json_real((double)me.local->traffic.bytes_received / uptime));
			json_object_set_new(child, "messages_sent_per_second", json_real((double)me.local->traffic.messages_sent / uptime));
			json_object_set_new(child, "messages_received_per_second", json_real((double)me.local->traffic.messages_received / uptime));
		}
	}
}

/** Collect connection statistics */
static void collect_connection_stats(json_t *parent)
{
	json_t *child = json_object();
	json_object_set_new(parent, "connections", child);

	json_object_set_new(child, "total_accepted", json_integer(ircstats.is_ac));
	json_object_set_new(child, "total_refused", json_integer(ircstats.is_ref));
	json_object_set_new(child, "client_connections", json_integer(ircstats.is_cl));
	json_object_set_new(child, "server_connections", json_integer(ircstats.is_sv));
	json_object_set_new(child, "unknown_connections", json_integer(ircstats.is_ni));
	json_object_set_new(child, "local_connections_made", json_integer(ircstats.is_loc));

	/* Time stats */
	json_object_set_new(child, "client_time_connected", json_integer(ircstats.is_cti));
	json_object_set_new(child, "server_time_connected", json_integer(ircstats.is_sti));

	/* Auth stats */
	json_object_set_new(child, "auth_success", json_integer(ircstats.is_asuc));
	json_object_set_new(child, "auth_fail", json_integer(ircstats.is_abad));

	/* Protocol errors */
	json_object_set_new(child, "unknown_commands", json_integer(ircstats.is_unco));
	json_object_set_new(child, "wrong_direction", json_integer(ircstats.is_wrdi));
	json_object_set_new(child, "unknown_prefix", json_integer(ircstats.is_unpf));
	json_object_set_new(child, "empty_messages", json_integer(ircstats.is_empt));
	json_object_set_new(child, "numeric_messages", json_integer(ircstats.is_num));
	json_object_set_new(child, "nick_collisions", json_integer(ircstats.is_kill));
	json_object_set_new(child, "mode_fakes", json_integer(ircstats.is_fake));
}

/** Collect detailed TKL/ban statistics */
static void collect_tkl_stats(json_t *parent, int detail)
{
	TKL *tkl;
	int index, index2;
	json_t *child;
	int total = 0;
	int gline = 0, gzline = 0, kline = 0, zline = 0, shun = 0;
	int spamfilter = 0, qline = 0, except = 0;
	long long total_spamfilter_hits = 0;

	child = json_object();
	json_object_set_new(parent, "server_ban", child);

	/* Hashed entries */
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				total++;
				if (TKLIsSpamfilter(tkl))
				{
					spamfilter++;
					if (tkl->ptr.spamfilter)
						total_spamfilter_hits += tkl->ptr.spamfilter->hits;
				}
				else if (TKLIsBanException(tkl))
					except++;
				else if (TKLIsNameBan(tkl))
					qline++;
				else if (tkl->type == (TKL_KILL | TKL_GLOBAL))
					gline++;
				else if (tkl->type == (TKL_ZAP | TKL_GLOBAL))
					gzline++;
				else if (tkl->type == TKL_KILL)
					kline++;
				else if (tkl->type == TKL_ZAP)
					zline++;
				else if (tkl->type == (TKL_SHUN | TKL_GLOBAL))
					shun++;
			}
		}
	}

	/* Non-hashed entries */
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			total++;
			if (TKLIsSpamfilter(tkl))
			{
				spamfilter++;
				if (tkl->ptr.spamfilter)
					total_spamfilter_hits += tkl->ptr.spamfilter->hits;
			}
			else if (TKLIsBanException(tkl))
				except++;
			else if (TKLIsNameBan(tkl))
				qline++;
			else if (tkl->type == (TKL_KILL | TKL_GLOBAL))
				gline++;
			else if (tkl->type == (TKL_ZAP | TKL_GLOBAL))
				gzline++;
			else if (tkl->type == TKL_KILL)
				kline++;
			else if (tkl->type == TKL_ZAP)
				zline++;
			else if (tkl->type == (TKL_SHUN | TKL_GLOBAL))
				shun++;
		}
	}

	json_object_set_new(child, "total", json_integer(total));
	json_object_set_new(child, "gline", json_integer(gline));
	json_object_set_new(child, "gzline", json_integer(gzline));
	json_object_set_new(child, "kline", json_integer(kline));
	json_object_set_new(child, "zline", json_integer(zline));
	json_object_set_new(child, "shun", json_integer(shun));
	json_object_set_new(child, "spamfilter", json_integer(spamfilter));
	json_object_set_new(child, "spamfilter_hits", json_integer(total_spamfilter_hits));
	json_object_set_new(child, "qline", json_integer(qline));
	json_object_set_new(child, "except", json_integer(except));
}

/** Collect command usage statistics */
static void collect_command_stats(json_t *parent)
{
	int i;
	RealCommand *cmd;
	json_t *child = json_object();
	json_t *commands = json_array();
	unsigned int total_count = 0;
	unsigned long total_bytes = 0;

	json_object_set_new(parent, "commands", child);

	for (i = 0; i < 256; i++)
	{
		for (cmd = CommandHash[i]; cmd; cmd = cmd->next)
		{
			if (cmd->count > 0)
			{
				json_t *item = json_object();
				json_object_set_new(item, "name", json_string_unreal(cmd->cmd));
				json_object_set_new(item, "count", json_integer(cmd->count));
				json_object_set_new(item, "bytes", json_integer(cmd->bytes));
				json_array_append_new(commands, item);

				total_count += cmd->count;
				total_bytes += cmd->bytes;
			}
		}
	}

	json_object_set_new(child, "total_count", json_integer(total_count));
	json_object_set_new(child, "total_bytes", json_integer(total_bytes));
	json_object_set_new(child, "list", commands);
}

/** Collect system-level statistics */
static void collect_system_stats(json_t *parent)
{
	json_t *child = json_object();
	json_object_set_new(parent, "system", child);

	json_object_set_new(child, "server_name", json_string_unreal(me.name));
	json_object_set_new(child, "current_time", json_timestamp(TStime()));
	json_object_set_new(child, "boot_time", json_timestamp(me.server->boottime));
	json_object_set_new(child, "uptime_seconds", json_integer(TStime() - me.server->boottime));
	json_object_set_new(child, "max_connections", json_integer(MAXCONNECTIONS));

	if (me.server->features.software)
		json_object_set_new(child, "software", json_string_unreal(me.server->features.software));

	/* History snapshot info */
	json_object_set_new(child, "stats_history_size", json_integer(STATS_HISTORY_SIZE));
	json_object_set_new(child, "stats_history_count", json_integer(stats_history_count));
	json_object_set_new(child, "stats_snapshot_interval_seconds", json_integer(STATS_SNAPSHOT_INTERVAL / 1000));
	json_object_set_new(child, "stats_initialized", json_boolean(stats_initialized));
}

/** Helper: check if json array contains a string */
static int json_array_contains_string(json_t *array, const char *str)
{
	size_t index;
	json_t *value;
	json_array_foreach(array, index, value)
	{
		const char *s = json_string_value(value);
		if (s && !strcmp(s, str))
			return 1;
	}
	return 0;
}

/** RPC handler: stats.get - Get comprehensive current statistics
 *
 * Parameters:
 *   object_detail_level (optional): 0=minimal, 1=include countries, 2=include everything
 *   sections (optional): array of section names to include, or omit for all
 *
 * Returns comprehensive server statistics including:
 *   - user: User counts (total, oper, tls, etc.) with optional country/ASN breakdown
 *   - channel: Channel counts with mode breakdown
 *   - server: Server list with details
 *   - traffic: Bytes/messages sent/received
 *   - connections: Connection statistics
 *   - server_ban: TKL/ban counts by type
 *   - commands: Command usage statistics
 *   - system: Server info and uptime
 */
void rpc_stats_get(Client *client, json_t *request, json_t *params)
{
	json_t *result;
	int detail;
	json_t *sections = NULL;

	OPTIONAL_PARAM_INTEGER("object_detail_level", detail, 1);
	sections = json_object_get(params, "sections");

	result = json_object();

	/* Collect all requested statistics */
	if (!sections || !json_is_array(sections) || json_array_size(sections) == 0)
	{
		/* Include everything */
		collect_system_stats(result);
		collect_user_stats(result, detail);
		collect_channel_stats(result, detail);
		collect_server_stats(result, detail);
		collect_traffic_stats(result);
		collect_connection_stats(result);
		collect_tkl_stats(result, detail);
		if (detail >= 2)
			collect_command_stats(result);
	}
	else
	{
		/* Selective sections */
		size_t index;
		json_t *value;
		json_array_foreach(sections, index, value)
		{
			const char *section = json_string_value(value);
			if (!section)
				continue;
			if (!strcmp(section, "system"))
				collect_system_stats(result);
			else if (!strcmp(section, "user"))
				collect_user_stats(result, detail);
			else if (!strcmp(section, "channel"))
				collect_channel_stats(result, detail);
			else if (!strcmp(section, "server"))
				collect_server_stats(result, detail);
			else if (!strcmp(section, "traffic"))
				collect_traffic_stats(result);
			else if (!strcmp(section, "connections"))
				collect_connection_stats(result);
			else if (!strcmp(section, "server_ban"))
				collect_tkl_stats(result, detail);
			else if (!strcmp(section, "commands"))
				collect_command_stats(result);
		}
	}

	rpc_response(client, request, result);
	json_decref(result);
}

/** RPC handler: stats.history - Get historical statistics snapshots
 *
 * Parameters:
 *   count (optional): Number of snapshots to return (default: all available, max: STATS_HISTORY_SIZE)
 *   since (optional): Only return snapshots newer than this timestamp
 *
 * Returns an array of historical snapshots, newest first, suitable for timeline graphing.
 */
void rpc_stats_history(Client *client, json_t *request, json_t *params)
{
	json_t *result, *snapshots;
	int count, i, idx;
	time_t since = 0;

	OPTIONAL_PARAM_INTEGER("count", count, stats_history_count);
	if (count <= 0 || count > stats_history_count)
		count = stats_history_count;

	/* Check for 'since' parameter */
	json_t *since_param = json_object_get(params, "since");
	if (since_param && json_is_string(since_param))
	{
		/* Parse ISO timestamp */
		const char *since_str = json_string_value(since_param);
		if (since_str)
		{
			/* Simple parsing - just treat as unix timestamp for now */
			since = (time_t)atoll(since_str);
		}
	}
	else if (since_param && json_is_integer(since_param))
	{
		since = json_integer_value(since_param);
	}

	result = json_object();
	snapshots = json_array();

	/* Iterate through history buffer, newest first */
	for (i = 0; i < count; i++)
	{
		/* Calculate index going backwards from current position */
		idx = (stats_history_index - i + STATS_HISTORY_SIZE) % STATS_HISTORY_SIZE;

		/* Skip if we don't have this slot yet */
		if (i >= stats_history_count)
			break;

		/* Skip if before 'since' */
		if (since > 0 && stats_history[idx].timestamp < since)
			continue;

		json_array_append_new(snapshots, snapshot_to_json(&stats_history[idx]));
	}

	json_object_set_new(result, "count", json_integer(json_array_size(snapshots)));
	json_object_set_new(result, "max_available", json_integer(stats_history_count));
	json_object_set_new(result, "history_size", json_integer(STATS_HISTORY_SIZE));
	json_object_set_new(result, "snapshot_interval_seconds", json_integer(STATS_SNAPSHOT_INTERVAL / 1000));
	json_object_set_new(result, "snapshots", snapshots);

	rpc_response(client, request, result);
	json_decref(result);
}
