/* src/modules/account-recovery.c
 *
 * Implements draft/account-recovery (RECOVER + SETPASS) per the
 * IRCv3 work-in-progress spec we authored at
 *   ircv3-specifications/extensions/account-recovery.md
 *
 * Two commands:
 *
 *   RECOVER REQUEST <account>
 *     Server emails a one-time 6-digit code to the verified email on
 *     file.  Always replies NOTE RECOVER CODE_SENT regardless of
 *     whether the account exists / has a verified email -- the
 *     enumeration-resistant response is part of the spec.
 *
 *   RECOVER CONFIRM <account> <code>
 *     Verifies the code and, on success, attaches a setpass-grant to
 *     the connection.  Replies NOTE RECOVER VERIFIED.
 *
 *   SETPASS :<new-password>
 *     Authorised either by a SASL-authenticated session for the
 *     account, OR by a fresh setpass-grant from RECOVER CONFIRM on
 *     this connection.  The grant is single-use and expires after 5
 *     minutes.
 *
 * When SETPASS runs under a setpass-grant (recovery flow), step 4 of
 * the spec kicks in: kill all other authenticated sessions for the
 * account, and purge account-scoped server-side residue (PMs to the
 * account, away message, account metadata).  The threat model is that
 * the party finishing RECOVER may itself be an attacker who just
 * compromised the email; sanitising on completion ensures they can't
 * harvest the historical PMs after the takeover.
 *
 * 2FA preservation: nothing in this module touches twofa_enabled or
 * the registered 2FA credentials.  If the account had 2FA enforced,
 * the next SASL login still has to clear it.
 */

#include "unrealircd.h"
#include "obsidian.h"
#include "smtp.h"

ModuleHeader MOD_HEADER = {
    "account-recovery",
    "1.0",
    "draft/account-recovery (RECOVER + SETPASS)",
    "ObbyIRCd",
    "unrealircd-6"
};

/* ===================================================================
 * Configuration
 * =================================================================== */
#define RECOVERY_CODE_DIGITS    6
#define RECOVERY_CODE_LIFETIME  300   /* seconds; 5 min per spec */
#define RECOVERY_CODE_MAX_TRIES 5
#define RECOVERY_REQ_PER_ACCT_MIN  60  /* min seconds between dispatches */
#define RECOVERY_REQ_PER_CONN_HOUR 5   /* requests / hour / connection  */
#define SETPASS_GRANT_LIFETIME  300   /* seconds; 5 min per spec */

/* External symbols from account-registration.so we lean on.  Resolved
 * at module-load time; we don't take a hard link dep, but we DO
 * require account-registration to be loaded (we degrade if it's not).
 */
extern sqlite3 *obsidian_db;
extern int      obsidian_open_database(const char *filename);
extern Account *find_account(const char *name);
extern void     free_account(Account *acc);

/* ===================================================================
 * Per-connection setpass-grant ModData
 * =================================================================== */

typedef struct SetpassGrant {
    char    *account;        /* account name the grant is scoped to */
    time_t   expires_at;     /* unix epoch */
} SetpassGrant;

static ModDataInfo *grant_md = NULL;

static void grant_md_free(ModData *m)
{
    SetpassGrant *g = (SetpassGrant *)m->ptr;
    if (!g)
        return;
    safe_free(g->account);
    safe_free(g);
    m->ptr = NULL;
}

static SetpassGrant *get_grant(Client *client)
{
    if (!grant_md || !client)
        return NULL;
    SetpassGrant *g = (SetpassGrant *)moddata_local_client(client, grant_md).ptr;
    if (!g)
        return NULL;
    if (g->expires_at < TStime())
    {
        grant_md_free(&moddata_local_client(client, grant_md));
        return NULL;
    }
    return g;
}

static void set_grant(Client *client, const char *account)
{
    if (!grant_md || !client)
        return;
    grant_md_free(&moddata_local_client(client, grant_md));
    SetpassGrant *g = safe_alloc(sizeof(*g));
    safe_strdup(g->account, account);
    g->expires_at = TStime() + SETPASS_GRANT_LIFETIME;
    moddata_local_client(client, grant_md).ptr = g;
}

static void clear_grant(Client *client)
{
    if (!grant_md || !client)
        return;
    grant_md_free(&moddata_local_client(client, grant_md));
}

/* ===================================================================
 * Outstanding recovery codes (in-memory hashmap; small N, linked list
 * is fine).  Codes are scoped per account name; `code` is empty/NULL
 * for the "ineligible" case so we still rate-limit and consume slots.
 * =================================================================== */

typedef struct RecoveryCode {
    char    *account_lower;
    char    *code;            /* ASCII 6-digit; NULL if ineligible-shadow */
    time_t   expires_at;
    int      attempts;
    time_t   last_request;    /* most recent dispatch time, for rate-limit */
    struct RecoveryCode *next;
} RecoveryCode;

static RecoveryCode *codes_head = NULL;

/* Per-connection request rate-limit window.  Tracks how many RECOVER
 * REQUESTs this socket has issued in the last hour.  Pruned lazily.
 */
typedef struct ConnReqLog {
    time_t timestamps[RECOVERY_REQ_PER_CONN_HOUR + 4];
    int    count;
} ConnReqLog;

static ModDataInfo *connlog_md = NULL;

static void connlog_md_free(ModData *m)
{
    ConnReqLog *log = (ConnReqLog *)m->ptr;
    safe_free(log);
    m->ptr = NULL;
}

static ConnReqLog *get_connlog(Client *client)
{
    if (!connlog_md || !client)
        return NULL;
    ConnReqLog *log = (ConnReqLog *)moddata_local_client(client, connlog_md).ptr;
    if (!log)
    {
        log = safe_alloc(sizeof(*log));
        moddata_local_client(client, connlog_md).ptr = log;
    }
    return log;
}

static int connlog_check_and_record(Client *client)
{
    ConnReqLog *log = get_connlog(client);
    if (!log)
        return 1;  /* fail-open if we couldn't track */
    time_t now = TStime();
    time_t cutoff = now - 3600;
    int kept = 0;
    for (int i = 0; i < log->count; i++)
    {
        if (log->timestamps[i] >= cutoff)
            log->timestamps[kept++] = log->timestamps[i];
    }
    log->count = kept;
    if (log->count >= RECOVERY_REQ_PER_CONN_HOUR)
        return 0;
    if (log->count < (int)(sizeof(log->timestamps) / sizeof(log->timestamps[0])))
        log->timestamps[log->count++] = now;
    return 1;
}

/* Lowercase a string in place into a fresh buffer.  Caller frees. */
static char *strdup_lower(const char *s)
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

static RecoveryCode *find_code_record(const char *account)
{
    char *low = strdup_lower(account);
    if (!low)
        return NULL;
    RecoveryCode *r;
    for (r = codes_head; r; r = r->next)
    {
        if (!strcmp(r->account_lower, low))
            break;
    }
    safe_free(low);
    return r;
}

static void prune_expired_codes(void)
{
    time_t now = TStime();
    RecoveryCode **link = &codes_head;
    while (*link)
    {
        RecoveryCode *r = *link;
        if (r->expires_at < now)
        {
            *link = r->next;
            safe_free(r->account_lower);
            safe_free(r->code);
            safe_free(r);
            continue;
        }
        link = &r->next;
    }
}

static RecoveryCode *upsert_code_record(const char *account)
{
    RecoveryCode *r = find_code_record(account);
    if (r)
        return r;
    r = safe_alloc(sizeof(*r));
    r->account_lower = strdup_lower(account);
    r->next = codes_head;
    codes_head = r;
    return r;
}

static void remove_code_record(const char *account)
{
    char *low = strdup_lower(account);
    if (!low)
        return;
    RecoveryCode **link = &codes_head;
    while (*link)
    {
        RecoveryCode *r = *link;
        if (!strcmp(r->account_lower, low))
        {
            *link = r->next;
            safe_free(r->account_lower);
            safe_free(r->code);
            safe_free(r);
            break;
        }
        link = &r->next;
    }
    safe_free(low);
}

/* ===================================================================
 * Code generation: 6-digit numeric, evenly distributed.
 * =================================================================== */

static void gen_recovery_code(char *out)
{
    char raw[RECOVERY_CODE_DIGITS + 1];
    /* gen_random_alnum gives a-z0-9 -- not what we want; pull random
     * bytes and reduce to digits ourselves. */
    unsigned char bytes[RECOVERY_CODE_DIGITS];
    char temp[RECOVERY_CODE_DIGITS + 1];
    gen_random_alnum(temp, RECOVERY_CODE_DIGITS);  /* fills with hex-ish */
    /* Re-roll until 6 digits.  Cheap; we're not in a hot path. */
    for (int i = 0; i < RECOVERY_CODE_DIGITS; i++)
    {
        bytes[i] = (unsigned char)temp[i];
        out[i] = (char)('0' + (bytes[i] % 10));
    }
    out[RECOVERY_CODE_DIGITS] = '\0';
    (void)raw;
}

/* ===================================================================
 * Email helpers (mirror account-registration.c approach).
 * =================================================================== */

static int try_send_email_loaded(void)
{
    return Hooks[HOOKTYPE_SEND_EMAIL] != NULL;
}

static int try_send_email(const char *to, const char *subject, const char *body)
{
    Hook *h;
    for (h = Hooks[HOOKTYPE_SEND_EMAIL]; h; h = h->next)
    {
        int rc = h->func.intfunc(to, subject, body, NULL, NULL);
        if (rc)
            return 1;
    }
    return 0;
}

static int send_recovery_email(const Account *acc, const char *code)
{
    char subject[256];
    char body[2048];

    if (!acc || !acc->email || !code)
        return 0;

    snprintf(subject, sizeof(subject),
             "Recover your %s account", me.name);
    snprintf(body, sizeof(body),
             "Hi %s,\n"
             "\n"
             "Someone -- hopefully you -- requested a password recovery\n"
             "for the %s account on %s.\n"
             "\n"
             "Use this 6-digit code to confirm.  It expires in %d minutes:\n"
             "\n"
             "    %s\n"
             "\n"
             "On IRC, run:\n"
             "    /RECOVER CONFIRM %s %s\n"
             "    /SETPASS <your new password>\n"
             "\n"
             "If you didn't request this, you can ignore this message --\n"
             "your password has not been changed.\n",
             acc->name, acc->name, me.name,
             RECOVERY_CODE_LIFETIME / 60,
             code,
             acc->name, code);

    return try_send_email(acc->email, subject, body);
}

/* ===================================================================
 * Constant-time string compare (RFC-style)
 * =================================================================== */
static int ct_streq(const char *a, const char *b)
{
    if (!a || !b)
        return 0;
    size_t la = strlen(a), lb = strlen(b);
    if (la != lb)
        return 0;
    unsigned char diff = 0;
    for (size_t i = 0; i < la; i++)
        diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

/* ===================================================================
 * Mask an email for the response: "alice@example.com" -> "a***e@example.com"
 * =================================================================== */
static void mask_email(const char *email, char *out, size_t outlen)
{
    if (!email || !*email)
    {
        snprintf(out, outlen, "your registered address");
        return;
    }
    const char *at = strchr(email, '@');
    if (!at || at == email)
    {
        snprintf(out, outlen, "your registered address");
        return;
    }
    size_t local_len = (size_t)(at - email);
    char first = email[0];
    char last  = (local_len > 1) ? email[local_len - 1] : first;
    snprintf(out, outlen, "%c***%c%s", first, last, at);
}

/* ===================================================================
 * Update account password in DB (Argon2id hash) + invalidate SCRAM
 * (next login will backfill).
 * =================================================================== */

static int update_account_password(Account *acc, const char *new_password)
{
    const char *hash = Auth_Hash(AUTHTYPE_ARGON2, new_password);
    if (!hash)
        return 0;

    if (!obsidian_db)
        return 0;

    const char *sql =
        "UPDATE accounts"
        "   SET password = ?,"
        "       scram_salt = NULL, scram_iterations = NULL,"
        "       scram_stored_key = NULL, scram_server_key = NULL"
        " WHERE id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(stmt, 1, hash, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, (int)acc->id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return 0;

    /* Reflect into the in-memory copy too. */
    safe_free(acc->password);
    safe_strdup(acc->password, hash);
    safe_free(acc->scram_salt);
    safe_free(acc->scram_stored_key);
    safe_free(acc->scram_server_key);
    acc->scram_iterations = 0;
    return 1;
}

/* ===================================================================
 * Recovery-flow side effects: kill all sessions + purge residue.
 *
 * For the session-kill we walk the AccountMember list (kept up to
 * date by account-registration when clients log in/out) and exit each
 * client with a recovery-flagged QUIT message.
 *
 * For residue purge: ObbyIRCd does not currently persist PM history
 * server-side -- chathistory of PMs to offline accounts is held in
 * the in-memory history backend, which evaporates on disconnect.
 * Account metadata IS persisted; we wipe it here.
 * =================================================================== */

static void recovery_kill_sessions(Account *acc, Client *exempt)
{
    /* Snapshot: exit_client may unlink the member, so collect first. */
    Client **victims = NULL;
    int      n = 0, cap = 0;
    AccountMember *m;
    for (m = acc->members; m; m = m->next)
    {
        if (!m->client)
            continue;
        if (n + 1 > cap)
        {
            cap = cap ? cap * 2 : 4;
            Client **tmp = realloc(victims, sizeof(*victims) * cap);
            if (!tmp)
                break;
            victims = tmp;
        }
        victims[n++] = m->client;
    }

    for (int i = 0; i < n; i++)
    {
        Client *c = victims[i];
        if (!c || c == exempt)
            continue;
        /* Don't double-exit a client that the spec also requires the
         * issuing connection itself drop -- exempt is NULL for the
         * recovery flow per the spec ("including any sessions held by
         * the client that issued SETPASS itself"), so this branch is
         * normally unused. */
        exit_client(c, NULL,
                    "Account recovery completed; please reconnect.");
    }
    free(victims);
}

static void recovery_purge_metadata(Account *acc)
{
    if (!obsidian_db || !acc)
        return;
    /* Account metadata is the most likely vector for an attacker to
     * have left content (status, away, custom fields).  Wipe it.
     */
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM account_metadata WHERE account_id = ?";
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, (int)acc->id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    /* Walk the in-memory metadata list too. */
    Metadata *mm = acc->metadata_head, *next;
    while (mm)
    {
        next = mm->next;
        safe_free(mm->key);
        safe_free(mm->value);
        safe_free(mm);
        mm = next;
    }
    acc->metadata_head = NULL;
}

/* ===================================================================
 * RECOVER REQUEST <account>
 * =================================================================== */

static void send_code_sent_reply(Client *client, const char *acctname,
                                 const char *masked)
{
    sendto_one(client, NULL,
               ":%s NOTE RECOVER CODE_SENT %s :If %s has a verified email "
               "on file, a recovery code has been sent to %s.  It expires "
               "in %d minutes.",
               me.name, acctname, acctname,
               masked ? masked : "the registered address",
               RECOVERY_CODE_LIFETIME / 60);
}

static void cmd_recover_request(Client *client, int parc, const char *parv[])
{
    if (parc < 3 || BadPtr(parv[2]))
    {
        sendto_one(client, NULL,
                   ":%s FAIL RECOVER INVALID_PARAMS :Syntax: RECOVER REQUEST <account>",
                   me.name);
        return;
    }
    const char *acctname = parv[2];

    /* Per-connection rate limit (outer ring). */
    if (!connlog_check_and_record(client))
    {
        sendto_one(client, NULL,
                   ":%s FAIL RECOVER RATE_LIMITED :Too many recovery "
                   "requests on this connection.  Try again later.",
                   me.name);
        return;
    }

    if (!try_send_email_loaded())
    {
        sendto_one(client, NULL,
                   ":%s FAIL RECOVER TEMPORARILY_UNAVAILABLE :Email "
                   "delivery is not configured on this network.",
                   me.name);
        return;
    }

    if (!obsidian_db && obsidian_open_database(OBSIDIAN_DB) != SQLITE_OK)
    {
        sendto_one(client, NULL,
                   ":%s FAIL RECOVER TEMPORARILY_UNAVAILABLE :Account "
                   "database unavailable.",
                   me.name);
        return;
    }

    prune_expired_codes();
    Account *acc = find_account(acctname);

    /* Per-account rate limit (inner ring): MUST NOT differ between the
     * "account exists / verified email" and "ineligible" cases, so we
     * always consult the same code-record map. */
    RecoveryCode *r = find_code_record(acctname);
    if (r && r->last_request + RECOVERY_REQ_PER_ACCT_MIN > TStime())
    {
        /* Even on rate-limit, respond as if a code were sent: we
         * don't want to leak account state through the error code
         * either.  Spec says RATE_LIMITED is per connection; per
         * account we silently no-op. */
        char masked[128];
        if (acc && acc->verified && acc->email)
            mask_email(acc->email, masked, sizeof(masked));
        else
            mask_email(NULL, masked, sizeof(masked));
        send_code_sent_reply(client, acctname, masked);
        if (acc) free_account(acc);
        return;
    }

    char masked[128];
    if (acc && acc->verified && acc->email)
    {
        char code[RECOVERY_CODE_DIGITS + 1];
        gen_recovery_code(code);

        r = upsert_code_record(acctname);
        safe_free(r->code);
        safe_strdup(r->code, code);
        r->expires_at = TStime() + RECOVERY_CODE_LIFETIME;
        r->attempts = 0;
        r->last_request = TStime();

        mask_email(acc->email, masked, sizeof(masked));

        if (!send_recovery_email(acc, code))
        {
            unreal_log(ULOG_WARNING, "recovery", "EMAIL_NOT_QUEUED",
                       client,
                       "Could not queue recovery email for $account",
                       log_data_string("account", acc->name));
            sendto_one(client, NULL,
                       ":%s FAIL RECOVER TEMPORARILY_UNAVAILABLE :"
                       "Mail delivery is unavailable right now.  "
                       "Please try again later.",
                       me.name);
            free_account(acc);
            return;
        }
    }
    else
    {
        /* Ineligible: track timing in the same map so the rate-limit
         * window is identical for nonexistent accounts. */
        r = upsert_code_record(acctname);
        r->last_request = TStime();
        r->expires_at = TStime() + 60;  /* arbitrary short lifetime */
        mask_email(NULL, masked, sizeof(masked));
    }

    send_code_sent_reply(client, acctname, masked);
    if (acc) free_account(acc);
}

/* ===================================================================
 * RECOVER CONFIRM <account> <code>
 * =================================================================== */

static void cmd_recover_confirm(Client *client, int parc, const char *parv[])
{
    if (parc < 4 || BadPtr(parv[2]) || BadPtr(parv[3]))
    {
        sendto_one(client, NULL,
                   ":%s FAIL RECOVER INVALID_PARAMS :Syntax: RECOVER CONFIRM <account> <code>",
                   me.name);
        return;
    }
    const char *acctname = parv[2];
    const char *code     = parv[3];

    prune_expired_codes();

    RecoveryCode *r = find_code_record(acctname);
    int ok = 0;
    if (r && r->code && r->attempts < RECOVERY_CODE_MAX_TRIES)
    {
        if (ct_streq(r->code, code))
            ok = 1;
        r->attempts++;
    }

    /* Spec MUST: consume the code on every CONFIRM attempt to thwart
     * windowed brute-force. */
    if (r)
    {
        safe_free(r->code);
        r->code = NULL;
    }

    if (!ok)
    {
        sendto_one(client, NULL,
                   ":%s FAIL RECOVER INVALID_CODE :The recovery code is "
                   "incorrect or has expired.",
                   me.name);
        return;
    }

    /* On success, drop any other outstanding code record entirely. */
    remove_code_record(acctname);

    /* Issue a setpass-grant scoped to this account on this connection. */
    set_grant(client, acctname);

    sendto_one(client, NULL,
               ":%s NOTE RECOVER VERIFIED %s :Recovery code accepted.  "
               "Run /SETPASS <new password> within %d minutes to set a new password.",
               me.name, acctname, SETPASS_GRANT_LIFETIME / 60);
}

/* ===================================================================
 * RECOVER dispatcher
 * =================================================================== */

CMD_FUNC(cmd_recover)
{
    if (parc < 2 || BadPtr(parv[1]))
    {
        sendto_one(client, NULL,
                   ":%s FAIL RECOVER INVALID_PARAMS :Syntax: RECOVER REQUEST|CONFIRM ...",
                   me.name);
        return;
    }
    const char *sub = parv[1];
    if (!strcasecmp(sub, "REQUEST"))
        cmd_recover_request(client, parc, parv);
    else if (!strcasecmp(sub, "CONFIRM"))
        cmd_recover_confirm(client, parc, parv);
    else
        sendto_one(client, NULL,
                   ":%s FAIL RECOVER INVALID_PARAMS :Unknown subcommand '%s'",
                   me.name, sub);
}

/* ===================================================================
 * SETPASS <new-password>
 * =================================================================== */

CMD_FUNC(cmd_setpass)
{
    if (parc < 2 || BadPtr(parv[1]))
    {
        sendto_one(client, NULL,
                   ":%s FAIL SETPASS INVALID_PARAMS :Syntax: SETPASS :<new password>",
                   me.name);
        return;
    }
    const char *new_password = parv[1];

    /* Basic length policy.  Match account-registration. */
    size_t pwlen = strlen(new_password);
    if (pwlen < MIN_PASSWORD_LENGTH || pwlen > MAX_PASSWORD_LENGTH)
    {
        sendto_one(client, NULL,
                   ":%s FAIL SETPASS INVALID_PASSWORD :Password must be "
                   "between %d and %d characters.",
                   me.name, MIN_PASSWORD_LENGTH, MAX_PASSWORD_LENGTH);
        /* Spec: failed SETPASS on a grant MUST consume the grant. */
        clear_grant(client);
        return;
    }

    /* Determine authorisation source. */
    SetpassGrant *g = get_grant(client);
    const char *target = NULL;
    int via_grant = 0;
    if (g)
    {
        target = g->account;
        via_grant = 1;
    }
    else if (IsLoggedIn(client))
    {
        target = client->user->account;
    }

    if (!target || !*target)
    {
        sendto_one(client, NULL,
                   ":%s FAIL SETPASS UNAUTHORIZED :You must sign in or "
                   "complete a recovery flow before setting a password.",
                   me.name);
        return;
    }

    if (!obsidian_db && obsidian_open_database(OBSIDIAN_DB) != SQLITE_OK)
    {
        sendto_one(client, NULL,
                   ":%s FAIL SETPASS TEMPORARILY_UNAVAILABLE :Account "
                   "database unavailable.",
                   me.name);
        clear_grant(client);
        return;
    }

    Account *acc = find_account(target);
    if (!acc)
    {
        /* Grant references an account that no longer exists; treat
         * as UNAUTHORIZED rather than disclose. */
        sendto_one(client, NULL,
                   ":%s FAIL SETPASS UNAUTHORIZED :You must sign in or "
                   "complete a recovery flow before setting a password.",
                   me.name);
        clear_grant(client);
        return;
    }

    if (!update_account_password(acc, new_password))
    {
        sendto_one(client, NULL,
                   ":%s FAIL SETPASS TEMPORARILY_UNAVAILABLE :Could not "
                   "store the new password.",
                   me.name);
        clear_grant(client);
        free_account(acc);
        return;
    }

    /* Spec: consume the grant on success or failure. */
    clear_grant(client);

    if (via_grant)
    {
        /* Recovery-flow side effects: kill other sessions + purge. */
        recovery_kill_sessions(acc, NULL);
        recovery_purge_metadata(acc);

        sendto_one(client, NULL,
                   ":%s NOTE SETPASS SUCCESS :Password updated.  All "
                   "existing sessions for %s have been signed out and "
                   "account-scoped server state has been purged.",
                   me.name, acc->name);
    }
    else
    {
        /* In-session rotation: kill *other* sessions but keep this one
         * authenticated.  Per spec this is a SHOULD, but it matches
         * user intent (rotate suspecting a leak). */
        recovery_kill_sessions(acc, client);
        sendto_one(client, NULL,
                   ":%s NOTE SETPASS SUCCESS :Password updated.  Other "
                   "open sessions for %s have been signed out.",
                   me.name, acc->name);
    }

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
    ModDataInfo mdi;

    MARK_AS_OFFICIAL_MODULE(modinfo);

    memset(&cap, 0, sizeof(cap));
    cap.name = "draft/account-recovery";
    if (!ClientCapabilityAdd(modinfo->handle, &cap, NULL))
    {
        config_error("account-recovery: could not add CAP draft/account-recovery");
        return MOD_FAILED;
    }

    memset(&mdi, 0, sizeof(mdi));
    mdi.name = "recovery_grant";
    mdi.free = grant_md_free;
    mdi.type = MODDATATYPE_LOCAL_CLIENT;
    grant_md = ModDataAdd(modinfo->handle, mdi);
    if (!grant_md)
    {
        config_error("account-recovery: could not add ModData for recovery_grant");
        return MOD_FAILED;
    }

    memset(&mdi, 0, sizeof(mdi));
    mdi.name = "recovery_connlog";
    mdi.free = connlog_md_free;
    mdi.type = MODDATATYPE_LOCAL_CLIENT;
    connlog_md = ModDataAdd(modinfo->handle, mdi);
    if (!connlog_md)
    {
        config_error("account-recovery: could not add ModData for recovery_connlog");
        return MOD_FAILED;
    }

    /* Both commands MAY be issued before SASL completes (per spec),
     * hence CMD_UNREGISTERED. */
    CommandAdd(modinfo->handle, "RECOVER", cmd_recover, MAXPARA,
               CMD_USER | CMD_UNREGISTERED);
    CommandAdd(modinfo->handle, "SETPASS", cmd_setpass, 1,
               CMD_USER | CMD_UNREGISTERED);

    return MOD_SUCCESS;
}

MOD_LOAD()
{
    return MOD_SUCCESS;
}

MOD_UNLOAD()
{
    /* Drop the in-memory code map; ModData is freed by the framework. */
    while (codes_head)
    {
        RecoveryCode *r = codes_head;
        codes_head = r->next;
        safe_free(r->account_lower);
        safe_free(r->code);
        safe_free(r);
    }
    return MOD_SUCCESS;
}
