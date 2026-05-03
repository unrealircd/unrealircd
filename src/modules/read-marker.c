/* src/modules/read-marker.c
 *
 * IRCv3 draft/read-marker extension for ObbyIRCd.
 *
 *   CAP:     draft/read-marker
 *   Command: MARKREAD <target> [<timestamp>]
 *   Server:  :server MARKREAD <target> {<timestamp>|*}
 *
 * The marker is a per-account, per-target ISO 8601 timestamp recording
 * the most recent message the user has read in that buffer.  Clients
 * push their guess, the server clamps to a monotonically-increasing
 * value, and we broadcast updates to every other session of the same
 * account.
 *
 * Storage: a small sqlite3 table (read_markers) hung off the existing
 * obsidian.db.  Account-keyed -- the marker survives reconnects.  PMs
 * use the lowercased target nickname; channels use the channel name.
 *
 * Spec wrinkle: the spec says the MARKREAD that follows a JOIN MUST
 * arrive before RPL_ENDOFNAMES.  HOOKTYPE_LOCAL_JOIN fires *after*
 * NAMES in the core IRCd, so we send MARKREAD post-NAMES instead.
 * Functionally identical to clients (it's still in the JOIN burst),
 * just out of strict-spec order; documenting here so a future cleanup
 * can move the call site if a proper "post-JOIN, pre-NAMES-end" hook
 * is added.
 */

#include "unrealircd.h"
#include "obsidian.h"

ModuleHeader MOD_HEADER = {
    "read-marker",
    "1.0",
    "draft/read-marker (MARKREAD per-buffer read state)",
    "ObbyIRCd",
    "unrealircd-6"
};

#define READMARKER_CAP_NAME      "draft/read-marker"
#define READMARKER_TS_PREFIX     "timestamp="
#define READMARKER_TS_PREFIX_LEN 10
/* "YYYY-MM-DDThh:mm:ss.sssZ" = 24 chars */
#define READMARKER_TS_LEN        24

extern sqlite3 *obsidian_db;
extern int      obsidian_open_database(const char *filename);
extern Account *find_account(const char *name);
extern Account *find_account_by_client(Client *client);
extern void     free_account(Account *acc);

static long CAP_READMARKER = 0L;

/* ===================================================================
 * Schema bootstrap (lazy: runs on first DB hit)
 * =================================================================== */
static int schema_ready = 0;

static int ensure_schema(void)
{
    if (schema_ready)
        return 1;
    if (!obsidian_db && obsidian_open_database(OBSIDIAN_DB) != SQLITE_OK)
        return 0;
    char *errmsg = NULL;
    const char *sql =
        "CREATE TABLE IF NOT EXISTS read_markers ("
        "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  account_id   INTEGER NOT NULL,"
        "  target_lower TEXT    NOT NULL,"
        "  timestamp    TEXT    NOT NULL,"
        "  updated_at   INTEGER NOT NULL,"
        "  UNIQUE(account_id, target_lower)"
        ");";
    if (sqlite3_exec(obsidian_db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        if (errmsg) sqlite3_free(errmsg);
        return 0;
    }
    schema_ready = 1;
    return 1;
}

/* ===================================================================
 * Helpers
 * =================================================================== */

static char *lower_dup(const char *s)
{
    if (!s)
        return NULL;
    size_t n = strlen(s);
    char *out = safe_alloc(n + 1);
    for (size_t i = 0; i < n; i++)
        out[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] - 'A' + 'a') : s[i];
    out[n] = '\0';
    return out;
}

/* Validate "YYYY-MM-DDThh:mm:ss.sssZ".  Cheap structural check; we're
 * not trying to be a strict ISO 8601 parser. */
static int valid_iso_ts(const char *s)
{
    if (!s || strlen(s) != READMARKER_TS_LEN)
        return 0;
    static const int digit_at[] = {
        0,1,2,3,    5,6,    8,9,    /* date */
        11,12, 14,15, 17,18,        /* time */
        20,21,22                    /* ms */
    };
    for (size_t i = 0; i < sizeof(digit_at) / sizeof(digit_at[0]); i++)
    {
        char c = s[digit_at[i]];
        if (c < '0' || c > '9')
            return 0;
    }
    return s[4]  == '-' && s[7]  == '-' &&
           s[10] == 'T' && s[13] == ':' &&
           s[16] == ':' && s[19] == '.' &&
           s[23] == 'Z';
}

/* Strip the "timestamp=" prefix; return pointer into the original
 * string or NULL if the prefix is missing. */
static const char *strip_ts_prefix(const char *p)
{
    if (!p || strncmp(p, READMARKER_TS_PREFIX, READMARKER_TS_PREFIX_LEN))
        return NULL;
    return p + READMARKER_TS_PREFIX_LEN;
}

/* ===================================================================
 * DB access: get and set the per-target marker.
 * =================================================================== */

static char *db_get_marker(long account_id, const char *target_lower)
{
    if (!ensure_schema())
        return NULL;
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT timestamp FROM read_markers"
        " WHERE account_id = ? AND target_lower = ?";
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_int(stmt, 1, (int)account_id);
    sqlite3_bind_text(stmt, 2, target_lower, -1, SQLITE_STATIC);
    char *out = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *ts = sqlite3_column_text(stmt, 0);
        if (ts)
            safe_strdup(out, (const char *)ts);
    }
    sqlite3_finalize(stmt);
    return out;
}

/* Set the marker, but only if `ts` is strictly newer than the stored
 * value.  Returns the *current* stored value (caller frees) which may
 * be the new one or, if rejected, the old one. */
static char *db_set_marker_if_newer(long account_id, const char *target_lower,
                                    const char *ts)
{
    if (!ensure_schema())
        return NULL;
    char *existing = db_get_marker(account_id, target_lower);
    if (existing && strcmp(existing, ts) >= 0)
        return existing;  /* keep old */

    /* UPSERT: account_id + target_lower is UNIQUE. */
    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT INTO read_markers (account_id, target_lower, timestamp, updated_at)"
        " VALUES (?, ?, ?, ?)"
        " ON CONFLICT(account_id, target_lower) DO UPDATE SET"
        "   timestamp = excluded.timestamp,"
        "   updated_at = excluded.updated_at";
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return existing;
    sqlite3_bind_int(stmt, 1, (int)account_id);
    sqlite3_bind_text(stmt, 2, target_lower, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, ts, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, (int)TStime());
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    safe_free(existing);
    if (rc != SQLITE_DONE)
        return NULL;
    char *out = NULL;
    safe_strdup(out, ts);
    return out;
}

/* ===================================================================
 * Wire emission
 * =================================================================== */

static void send_markread(Client *to, const char *target, const char *ts)
{
    if (!to || !target)
        return;
    if (ts && *ts)
        sendto_one(to, NULL, ":%s MARKREAD %s timestamp=%s",
                   me.name, target, ts);
    else
        sendto_one(to, NULL, ":%s MARKREAD %s *", me.name, target);
}

/* Broadcast to every session belonging to the same account.  The
 * AccountMember list is maintained by account-registration. */
static void broadcast_markread(Account *acc, const char *target, const char *ts)
{
    if (!acc)
        return;
    for (AccountMember *m = acc->members; m; m = m->next)
    {
        if (!m->client || !MyUser(m->client))
            continue;
        if (!HasCapability(m->client, READMARKER_CAP_NAME))
            continue;
        send_markread(m->client, target, ts);
    }
}

/* ===================================================================
 * Hook: HOOKTYPE_LOCAL_JOIN
 * Send the channel's marker to the joining client (and only that
 * client -- the *user's* other sessions already have it from when
 * they last set it themselves).
 * =================================================================== */

static int rm_local_join(Client *client, Channel *channel, MessageTag *mtags)
{
    if (!MyUser(client) || !HasCapability(client, READMARKER_CAP_NAME))
        return 0;
    if (!IsLoggedIn(client))
        return 0;
    Account *acc = find_account_by_client(client);
    if (!acc)
        return 0;
    char *low = lower_dup(channel->name);
    char *ts  = db_get_marker(acc->id, low);
    send_markread(client, channel->name, ts);
    safe_free(low);
    safe_free(ts);
    free_account(acc);
    return 0;
}

/* ===================================================================
 * MARKREAD command
 * =================================================================== */

static void send_fail(Client *client, const char *code,
                      const char *target, const char *msg)
{
    if (target)
        sendto_one(client, NULL, ":%s FAIL MARKREAD %s %s :%s",
                   me.name, code, target, msg);
    else
        sendto_one(client, NULL, ":%s FAIL MARKREAD %s :%s",
                   me.name, code, msg);
}

CMD_FUNC(cmd_markread)
{
    if (!MyUser(client))
        return;

    if (parc < 2 || BadPtr(parv[1]))
    {
        send_fail(client, "NEED_MORE_PARAMS", NULL, "Missing parameters");
        return;
    }
    const char *target = parv[1];

    if (!IsLoggedIn(client))
    {
        send_fail(client, "INTERNAL_ERROR", target,
                  "An account is required for MARKREAD.");
        return;
    }

    Account *acc = find_account_by_client(client);
    if (!acc)
    {
        send_fail(client, "INTERNAL_ERROR", target,
                  "Account lookup failed.");
        return;
    }

    char *low = lower_dup(target);

    /* GET form: no timestamp arg. */
    if (parc < 3 || BadPtr(parv[2]))
    {
        char *ts = db_get_marker(acc->id, low);
        send_markread(client, target, ts);
        safe_free(ts);
        safe_free(low);
        free_account(acc);
        return;
    }

    /* SET form: parv[2] is either "*" (which we don't accept from
     * clients per the spec) or "timestamp=...". */
    if (!strcmp(parv[2], "*"))
    {
        send_fail(client, "INVALID_PARAMS", target,
                  "Clients MUST NOT send a literal * as the timestamp.");
        safe_free(low);
        free_account(acc);
        return;
    }

    const char *ts = strip_ts_prefix(parv[2]);
    if (!ts || !valid_iso_ts(ts))
    {
        send_fail(client, "INVALID_PARAMS", target,
                  "Timestamp must be 'timestamp=YYYY-MM-DDThh:mm:ss.sssZ'.");
        safe_free(low);
        free_account(acc);
        return;
    }

    char *stored = db_set_marker_if_newer(acc->id, low, ts);
    if (!stored)
    {
        send_fail(client, "INTERNAL_ERROR", target,
                  "The read timestamp could not be set.");
        safe_free(low);
        free_account(acc);
        return;
    }

    /* Spec: if the client's value was older or equal, the server's
     * stored value goes back; otherwise broadcast the new value to
     * every session of this account. */
    int new_value_won = !strcmp(stored, ts);
    if (new_value_won)
    {
        broadcast_markread(acc, target, stored);
    }
    else
    {
        /* Reply only to the asking client with the older stored value. */
        send_markread(client, target, stored);
    }

    safe_free(stored);
    safe_free(low);
    free_account(acc);
}

/* ===================================================================
 * Module wiring
 * =================================================================== */

MOD_TEST()
{
    return MOD_SUCCESS;
}

MOD_INIT()
{
    ClientCapabilityInfo cap;

    MARK_AS_OFFICIAL_MODULE(modinfo);

    memset(&cap, 0, sizeof(cap));
    cap.name = READMARKER_CAP_NAME;
    if (!ClientCapabilityAdd(modinfo->handle, &cap, &CAP_READMARKER))
    {
        config_error("read-marker: could not add CAP %s", READMARKER_CAP_NAME);
        return MOD_FAILED;
    }

    CommandAdd(modinfo->handle, "MARKREAD", cmd_markread, MAXPARA, CMD_USER);
    HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, rm_local_join);
    return MOD_SUCCESS;
}

MOD_LOAD()
{
    return MOD_SUCCESS;
}

MOD_UNLOAD()
{
    return MOD_SUCCESS;
}
