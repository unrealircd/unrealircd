/*
 * src/modules/account-registration.c
 * Native account registration system for ObbyIRCd.
 *
 * Features:
 *   - SQLite3-backed account storage (PERMDATADIR/obsidian.db)
 *   - IRCv3 draft/account-registration capability
 *   - Commands: REGISTER, IDENTIFY, LOGOUT, LISTACC (oper)
 *   - Built-in SASL server: PLAIN and ANONYMOUS mechanisms
 *   - RPC: obsidianirc.accounts.list, obsidianirc.accounts.find
 *   - Config block: account-registration { ... }
 *   - Custom hook HOOKTYPE_ACCOUNT_REGISTER (133) fired on new registrations
 *
 * Config example (obbyircd.conf):
 *   account-registration {
 *       min-name-length 3;
 *       max-name-length 50;
 *       min-password-length 8;
 *       max-password-length 200;
 *       require-email yes;
 *   };
 *
 * Requires: sqlite3 (-lsqlite3)
 */

#include "unrealircd.h"
#include "obsidian.h"
#include "smtp.h"

/* ===================================================================
 * Module header
 * =================================================================== */
ModuleHeader MOD_HEADER = {
    "account-registration",
    "1.0",
    "Native account registration with built-in SASL (PLAIN, ANONYMOUS)",
    "ObbyIRCd Team",
    "unrealircd-6",
};

/* ===================================================================
 * Module globals (definitions; obsidian.h has the externs)
 * =================================================================== */
sqlite3    *obsidian_db  = NULL;
ModDataInfo *sasl_md     = NULL;
static long CAP_ACCOUNTREGISTRATION = 0L;
static struct AccountRegistrationConfStruct MyConf;

/* ===================================================================
 * Forward declarations
 * =================================================================== */
static int   accreg_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
static int   accreg_configposttest(int *errs);
static int   accreg_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
static int   authenticate_attempt(Client *client, int first, const char *param);
static const char *saslmechs(Client *client);
static const char *accreg_capability_parameter(Client *client);
static int   accreg_capability_visible(Client *client);
static json_t *account2json(const Account *acc);
static void  set_accreg_conf(void);
static void  free_accreg_conf(void);

CMD_FUNC(register_account);
CMD_FUNC(list_accounts);
CMD_FUNC(cmd_identify);
CMD_FUNC(cmd_logout);
CMD_FUNC(cmd_verify);
CMD_FUNC(cmd_2fa);

static int  try_send_email_loaded(void);
static int  send_verify_email(const Account *acc);

/* SCRAM forward decls (impl in SCRAM section below) */
typedef struct ScramState_ ScramState;
static ModDataInfo *scram_md;
static void scram_md_free(ModData *m);
static int  scram_make_credentials(Account *acc, const char *password);

/* 2FA forward decls (impl in 2FA section below) */
typedef struct TwoFAEnroll_
{
    char *type;
    char *secret_b32;
    time_t expires_at;
} TwoFAEnroll;
typedef struct TwoFAStepup_
{
    int active;
    char account[ACCOUNTLEN + 1];
} TwoFAStepup;
static ModDataInfo *twofa_enroll_md;
static ModDataInfo *twofa_stepup_md;
static void twofa_enroll_md_free(ModData *m);
static void twofa_stepup_md_free(ModData *m);
static int  twofa_maybe_start_stepup(Client *client, Account *acc);
static int  twofa_handle_stepup_authenticate(Client *client, const char *param);
static const char *twofa_capability_parameter(Client *client);
static int  twofa_capability_visible(Client *client);
static void twofa_clear_stepup(Client *c);

/* WebAuthn forward decls (impl in webauthn section below) */
typedef struct WebAuthnSaslState_
{
    int step;
    char username_hint[ACCOUNTLEN+1];
    unsigned char challenge[32];
} WebAuthnSaslState;
static ModDataInfo *webauthn_sasl_md;
static void webauthn_sasl_md_free(ModData *m);
static void webauthn_sasl_handle_hello(Client *client, const char *param);
static void webauthn_sasl_handle_assertion(Client *client, const char *param);
static void webauthn_sasl_clear(Client *c);
static int  webauthn_2fa_handle_add(Client *client, Account *acc,
                                    const char *name, const char *data_b64);
static void webauthn_2fa_handle_challenge(Client *client, Account *acc);
static const char *webauthn_rp_id_capability_parameter(Client *client);
#define WebAuthnSaslGet(c)   ((WebAuthnSaslState *)moddata_local_client((c), webauthn_sasl_md).ptr)
#define WebAuthnSaslSet(c,p) do { moddata_local_client((c), webauthn_sasl_md).ptr = (p); } while(0)
#define TwoFAStepupGet(c)   ((TwoFAStepup *)moddata_local_client((c), twofa_stepup_md).ptr)
#define TwoFAStepupSet(c,p) do { moddata_local_client((c), twofa_stepup_md).ptr = (p); } while(0)
#define TwoFAEnrollGet(c)   ((TwoFAEnroll *)moddata_local_client((c), twofa_enroll_md).ptr)
#define TwoFAEnrollSet(c,p) do { moddata_local_client((c), twofa_enroll_md).ptr = (p); } while(0)
RPC_CALL_FUNC(rpc_list_accounts);
RPC_CALL_FUNC(rpc_accounts_find);

/* ===================================================================
 * Module lifecycle
 * =================================================================== */
MOD_TEST()
{
    MARK_AS_OFFICIAL_MODULE(modinfo);
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST,     0, accreg_configtest);
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, accreg_configposttest);
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN,      0, accreg_configrun);
    return MOD_SUCCESS;
}

// COPILOT DO NOT REMOVE >:/
CMD_FUNC(cmd_debuglmao)
{
    Client *c;
    list_for_each_entry(c, &client_list, client_node)
    {
        sendnotice(client, "%s (%s)", c->name, c->id);
    }
}

MOD_INIT()
{
    ClientCapabilityInfo cap;
    ModDataInfo mdi;
    RPCHandlerInfo rpc;

    MARK_AS_OFFICIAL_MODULE(modinfo);
    set_accreg_conf();

    /* SASL ModData — tracks which SASL mechanism is in progress per client */
    memset(&mdi, 0, sizeof(mdi));
    mdi.name        = "sasl_auth_type";
    mdi.free        = sat_free;
    mdi.serialize   = sat_serialize;
    mdi.unserialize = sat_unserialize;
    mdi.type        = MODDATATYPE_CLIENT;
    sasl_md = ModDataAdd(modinfo->handle, mdi);
    if (!sasl_md)
    {
        config_error("account-registration: Could not add ModData for sasl_auth_type");
        return MOD_FAILED;
    }

    /* SCRAM-SHA-256 in-flight state, per-local-client */
    memset(&mdi, 0, sizeof(mdi));
    mdi.name        = "scram_state";
    mdi.free        = scram_md_free;
    mdi.type        = MODDATATYPE_LOCAL_CLIENT;
    scram_md = ModDataAdd(modinfo->handle, mdi);
    if (!scram_md)
    {
        config_error("account-registration: Could not add ModData for scram_state");
        return MOD_FAILED;
    }

    /* 2FA enrolment challenge state, per-local-client */
    memset(&mdi, 0, sizeof(mdi));
    mdi.name        = "twofa_enroll";
    mdi.free        = twofa_enroll_md_free;
    mdi.type        = MODDATATYPE_LOCAL_CLIENT;
    twofa_enroll_md = ModDataAdd(modinfo->handle, mdi);
    if (!twofa_enroll_md)
    {
        config_error("account-registration: Could not add ModData for twofa_enroll");
        return MOD_FAILED;
    }

    /* 2FA SASL step-up state, per-local-client */
    memset(&mdi, 0, sizeof(mdi));
    mdi.name        = "twofa_stepup";
    mdi.free        = twofa_stepup_md_free;
    mdi.type        = MODDATATYPE_LOCAL_CLIENT;
    twofa_stepup_md = ModDataAdd(modinfo->handle, mdi);
    if (!twofa_stepup_md)
    {
        config_error("account-registration: Could not add ModData for twofa_stepup");
        return MOD_FAILED;
    }

    /* WEBAUTHN-BIO SASL state, per-local-client */
    memset(&mdi, 0, sizeof(mdi));
    mdi.name        = "webauthn_sasl";
    mdi.free        = webauthn_sasl_md_free;
    mdi.type        = MODDATATYPE_LOCAL_CLIENT;
    webauthn_sasl_md = ModDataAdd(modinfo->handle, mdi);
    if (!webauthn_sasl_md)
    {
        config_error("account-registration: Could not add ModData for webauthn_sasl");
        return MOD_FAILED;
    }

    /* Open DB during init to verify it is accessible */
    if (obsidian_open_database(OBSIDIAN_DB) != SQLITE_OK)
    {
        config_error("account-registration: Could not open database at %s", OBSIDIAN_DB);
        return MOD_FAILED;
    }

    /* CAP: draft/account-registration */
    memset(&cap, 0, sizeof(cap));
    cap.name      = REGCAP_NAME;
    cap.visible   = accreg_capability_visible;
    cap.parameter = accreg_capability_parameter;
    if (!ClientCapabilityAdd(modinfo->handle, &cap, &CAP_ACCOUNTREGISTRATION))
    {
        config_error("account-registration: Could not add CAP " REGCAP_NAME);
        return MOD_FAILED;
    }

    /* CAP: draft/account-2fa */
    {
        static long CAP_TWOFA = 0L;
        ClientCapabilityInfo cap2;
        memset(&cap2, 0, sizeof(cap2));
        cap2.name      = "draft/account-2fa";
        cap2.visible   = twofa_capability_visible;
        cap2.parameter = twofa_capability_parameter;
        if (!ClientCapabilityAdd(modinfo->handle, &cap2, &CAP_TWOFA))
        {
            config_error("account-registration: Could not add CAP draft/account-2fa");
            return MOD_FAILED;
        }
    }

    /* CAP: draft/webauthn-rp-id (advertise the WebAuthn RP ID) */
    {
        static long CAP_WAUTH_RPID = 0L;
        ClientCapabilityInfo cap3;
        memset(&cap3, 0, sizeof(cap3));
        cap3.name      = "draft/webauthn-rp-id";
        cap3.visible   = twofa_capability_visible;
        cap3.parameter = webauthn_rp_id_capability_parameter;
        if (!ClientCapabilityAdd(modinfo->handle, &cap3, &CAP_WAUTH_RPID))
        {
            config_error("account-registration: Could not add CAP draft/webauthn-rp-id");
            return MOD_FAILED;
        }
    }

    /* Commands */
    CommandAdd(modinfo->handle, CMD_REGISTER, register_account, MAXPARA,
               CMD_USER | CMD_UNREGISTERED);
    CommandAdd(modinfo->handle, CMD_LISTACC,  list_accounts,    MAXPARA, CMD_OPER);
    CommandAdd(modinfo->handle, CMD_IDENTIFY, cmd_identify,     MAXPARA, CMD_USER);
    CommandAdd(modinfo->handle, CMD_LOGOUT,   cmd_logout,       MAXPARA, CMD_USER);
    CommandAdd(modinfo->handle, CMD_VERIFY,   cmd_verify,       MAXPARA,
               CMD_USER | CMD_UNREGISTERED);
    CommandAdd(modinfo->handle, CMD_2FA,      cmd_2fa,          MAXPARA, CMD_USER);
    CommandAdd(modinfo->handle, "DEBUGLMAO", cmd_debuglmao, 0, CMD_USER); // Stop removing my command, copilot! >:/

    /* Built-in SASL hooks */
    HookAddConstString(modinfo->handle, HOOKTYPE_SASL_MECHS, 0, saslmechs);
    HookAdd(modinfo->handle, HOOKTYPE_SASL_AUTHENTICATE, 0, authenticate_attempt);

    /* RPC handlers */
    memset(&rpc, 0, sizeof(rpc));
    rpc.method   = "obsidianirc.accounts.list";
    rpc.loglevel = ULOG_DEBUG;
    rpc.call     = rpc_list_accounts;
    RPCHandlerAdd(modinfo->handle, &rpc);

    memset(&rpc, 0, sizeof(rpc));
    rpc.method   = "obsidianirc.accounts.find";
    rpc.loglevel = ULOG_DEBUG;
    rpc.call     = rpc_accounts_find;
    RPCHandlerAdd(modinfo->handle, &rpc);
    
    return MOD_SUCCESS;
}

MOD_LOAD()
{
    ModuleSetOptions(modinfo->handle, MOD_OPT_PERM_RELOADABLE, 1);

    if (obsidian_open_database(OBSIDIAN_DB) != SQLITE_OK)
    {
        config_error("account-registration: Could not open database on load");
        return MOD_FAILED;
    }

    /* Advertise ourselves as the SASL server so sasl.c routes AUTHENTICATE here */
    safe_strdup(iConf.sasl_server, me.name);
    moddata_client_set(&me, "saslmechlist",
                       "PLAIN,SCRAM-SHA-256,TOTP,DRAFT-WEBAUTHN-BIO,ANONYMOUS");

    return MOD_SUCCESS;
}

MOD_UNLOAD()
{
    obsidian_close_database();
    safe_free(iConf.sasl_server);
    iConf.sasl_server = NULL;
    free_accreg_conf();
    return MOD_SUCCESS;
}

/* ===================================================================
 * Config
 * =================================================================== */
static int accreg_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
    ConfigEntry *cep;
    int errors = 0;

    if (type != CONFIG_MAIN)
        return 0;
    if (!ce || !ce->name || strcmp(ce->name, CONF_ACCOUNT_BLOCK))
        return 0;

    for (cep = ce->items; cep; cep = cep->next)
    {
        if (!cep->value)
        {
            config_error("%s:%i: blank value for %s::%s",
                         cep->file->filename, cep->line_number,
                         CONF_ACCOUNT_BLOCK, cep->name ? cep->name : "(null)");
            errors++;
            continue;
        }
        if (!cep->name)
        {
            config_error("%s:%i: blank item name in %s block",
                         cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
            errors++;
            continue;
        }
        if (!strcmp(cep->name, "min-name-length"))
        {
            if (MyConf.got_min_name_length)
            {
                config_error("%s:%i: duplicate %s::min-name-length",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            int v = atoi(cep->value);
            if (v < MIN_ACCOUNT_NAME_LENGTH || v > MAX_ACCOUNT_NAME_LENGTH)
            {
                config_error("%s:%i: %s::min-name-length must be %d-%d",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK,
                             MIN_ACCOUNT_NAME_LENGTH, MAX_ACCOUNT_NAME_LENGTH);
                errors++;
            }
            MyConf.got_min_name_length = 1;
        }
        else if (!strcmp(cep->name, "max-name-length"))
        {
            if (MyConf.got_max_name_length)
            {
                config_error("%s:%i: duplicate %s::max-name-length",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            int v = atoi(cep->value);
            if (v < MIN_ACCOUNT_NAME_LENGTH || v > MAX_ACCOUNT_NAME_LENGTH)
            {
                config_error("%s:%i: %s::max-name-length must be %d-%d",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK,
                             MIN_ACCOUNT_NAME_LENGTH, MAX_ACCOUNT_NAME_LENGTH);
                errors++;
            }
            MyConf.got_max_name_length = 1;
        }
        else if (!strcmp(cep->name, "min-password-length"))
        {
            if (MyConf.got_min_password_length)
            {
                config_error("%s:%i: duplicate %s::min-password-length",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            int v = atoi(cep->value);
            if (v < MIN_PASSWORD_LENGTH || v > MAX_PASSWORD_LENGTH)
            {
                config_error("%s:%i: %s::min-password-length must be %d-%d",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK,
                             MIN_PASSWORD_LENGTH, MAX_PASSWORD_LENGTH);
                errors++;
            }
            MyConf.got_min_password_length = 1;
        }
        else if (!strcmp(cep->name, "max-password-length"))
        {
            if (MyConf.got_max_password_length)
            {
                config_error("%s:%i: duplicate %s::max-password-length",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            int v = atoi(cep->value);
            if (v < MIN_PASSWORD_LENGTH || v > MAX_PASSWORD_LENGTH)
            {
                config_error("%s:%i: %s::max-password-length must be %d-%d",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK,
                             MIN_PASSWORD_LENGTH, MAX_PASSWORD_LENGTH);
                errors++;
            }
            MyConf.got_max_password_length = 1;
        }
        else if (!strcmp(cep->name, "require-email"))
        {
            if (MyConf.got_require_email)
            {
                config_error("%s:%i: duplicate %s::require-email",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            MyConf.got_require_email = 1;
        }
        else if (!strcmp(cep->name, "require-terms-acceptance"))
        {
            if (MyConf.got_require_terms_acceptance)
            {
                config_error("%s:%i: duplicate %s::require-terms-acceptance",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            MyConf.got_require_terms_acceptance = 1;
        }
        else if (!strcmp(cep->name, "allow-username-changes"))
        {
            if (MyConf.got_allow_username_changes)
            {
                config_error("%s:%i: duplicate %s::allow-username-changes",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            MyConf.got_allow_username_changes = 1;
        }
        else if (!strcmp(cep->name, "allow-password-changes"))
        {
            if (MyConf.got_allow_password_changes)
            {
                config_error("%s:%i: duplicate %s::allow-password-changes",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            MyConf.got_allow_password_changes = 1;
        }
        else if (!strcmp(cep->name, "allow-email-changes"))
        {
            if (MyConf.got_allow_email_changes)
            {
                config_error("%s:%i: duplicate %s::allow-email-changes",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            MyConf.got_allow_email_changes = 1;
        }
        else if (!strcmp(cep->name, "guest-nick-format"))
        {
            if (MyConf.got_guest_nick_format)
            {
                config_error("%s:%i: duplicate %s::guest-nick-format",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            if (BadPtr(cep->value))
            {
                config_error("%s:%i: %s::guest-nick-format cannot be empty",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            MyConf.got_guest_nick_format = 1;
        }
        else if (!strcmp(cep->name, "verify-email"))
        {
            if (MyConf.got_verify_email)
            {
                config_error("%s:%i: duplicate %s::verify-email",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            MyConf.got_verify_email = 1;
        }
        else if (!strcmp(cep->name, "verify-code-lifetime"))
        {
            if (MyConf.got_verify_code_lifetime)
            {
                config_error("%s:%i: duplicate %s::verify-code-lifetime",
                             cep->file->filename, cep->line_number, CONF_ACCOUNT_BLOCK);
                errors++;
            }
            MyConf.got_verify_code_lifetime = 1;
        }
        else
        {
            config_warn("%s:%i: unknown directive %s::%s",
                        cep->file->filename, cep->line_number,
                        CONF_ACCOUNT_BLOCK, cep->name);
        }
    }

    *errs = errors;
    return errors ? -1 : 1;
}

static int accreg_configposttest(int *errs)
{
    return 0;
}


static int accreg_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
    ConfigEntry *cep;

    if (type != CONFIG_MAIN)
        return 0;
    if (!ce || !ce->name || strcmp(ce->name, CONF_ACCOUNT_BLOCK))
        return 0;

    for (cep = ce->items; cep; cep = cep->next)
    {
        if (!cep->name)
            continue;
        if (!strcmp(cep->name, "min-name-length"))
            MyConf.min_name_length = atoi(cep->value);
        else if (!strcmp(cep->name, "max-name-length"))
            MyConf.max_name_length = atoi(cep->value);
        else if (!strcmp(cep->name, "min-password-length"))
            MyConf.min_password_length = atoi(cep->value);
        else if (!strcmp(cep->name, "max-password-length"))
            MyConf.max_password_length = atoi(cep->value);
        else if (!strcmp(cep->name, "require-email"))
            MyConf.require_email = config_checkval(cep->value, CFG_YESNO);
        else if (!strcmp(cep->name, "require-terms-acceptance"))
            MyConf.require_terms_acceptance = config_checkval(cep->value, CFG_YESNO);
        else if (!strcmp(cep->name, "allow-username-changes"))
            MyConf.allow_username_changes = config_checkval(cep->value, CFG_YESNO);
        else if (!strcmp(cep->name, "allow-password-changes"))
            MyConf.allow_password_changes = config_checkval(cep->value, CFG_YESNO);
        else if (!strcmp(cep->name, "allow-email-changes"))
            MyConf.allow_email_changes = config_checkval(cep->value, CFG_YESNO);
        else if (!strcmp(cep->name, "guest-nick-format"))
        {
            safe_free(MyConf.guest_nick_format);
            safe_strdup(MyConf.guest_nick_format, cep->value);
        }
        else if (!strcmp(cep->name, "verify-email"))
            MyConf.verify_email = config_checkval(cep->value, CFG_YESNO);
        else if (!strcmp(cep->name, "verify-code-lifetime"))
            MyConf.verify_code_lifetime = config_checkval(cep->value, CFG_TIME);
    }
    return 1;
}

/* ===================================================================
 * Config defaults / cleanup
 * =================================================================== */
static void set_accreg_conf(void)
{
    memset(&MyConf, 0, sizeof(MyConf));
    MyConf.min_name_length          = 3;
    MyConf.max_name_length          = 50;
    MyConf.min_password_length      = 8;
    MyConf.max_password_length      = 200;
    MyConf.require_email            = 1;
    MyConf.require_terms_acceptance = 1;
    MyConf.allow_username_changes   = 1;
    MyConf.allow_password_changes   = 1;
    MyConf.allow_email_changes      = 1;
    MyConf.verify_email             = 0;
    MyConf.verify_code_lifetime     = VERIFY_CODE_LIFETIME_DEFAULT;
    safe_strdup(MyConf.guest_nick_format, "Guest$d$d$d$d");
}

static void free_accreg_conf(void)
{
    safe_free(MyConf.guest_nick_format);
}

/* ===================================================================
 * SQLite3 database layer
 * =================================================================== */
int obsidian_open_database(const char *filename)
{
    const char *sql;
    char *errmsg;

    if (obsidian_db)
        return SQLITE_OK; /* already open */

    if (sqlite3_open(filename, &obsidian_db) != SQLITE_OK)
    {
        obsidian_db = NULL;
        return SQLITE_ERROR;
    }

    sql =
        "CREATE TABLE IF NOT EXISTS accounts ("
        "  id                INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name              TEXT NOT NULL COLLATE NOCASE,"
        "  email             TEXT,"
        "  password          TEXT,"
        "  time_registered   INTEGER,"
        "  verified          INTEGER DEFAULT 0,"
        "  verify_code       TEXT,"
        "  verify_expires    INTEGER,"
        "  scram_salt        TEXT,"
        "  scram_iterations  INTEGER,"
        "  scram_stored_key  TEXT,"
        "  scram_server_key  TEXT,"
        "  twofa_enabled     INTEGER DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS account_2fa_credentials ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  account_id  INTEGER NOT NULL,"
        "  type        TEXT NOT NULL,"
        "  name        TEXT NOT NULL,"
        "  secret      TEXT NOT NULL,"
        "  created_at  INTEGER NOT NULL,"
        "  FOREIGN KEY (account_id) REFERENCES accounts(id)"
        ");";

    errmsg = NULL;
    if (sqlite3_exec(obsidian_db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        sqlite3_free(errmsg);
        sqlite3_close(obsidian_db);
        obsidian_db = NULL;
        return SQLITE_ERROR;
    }

    /* Schema migrations for installations created before these columns existed.
     * SQLite has no portable IF NOT EXISTS for ADD COLUMN, so we just try and
     * swallow the "duplicate column" error. */
    errmsg = NULL;
    sqlite3_exec(obsidian_db,
                 "ALTER TABLE accounts ADD COLUMN verify_code TEXT;",
                 NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    errmsg = NULL;
    sqlite3_exec(obsidian_db,
                 "ALTER TABLE accounts ADD COLUMN verify_expires INTEGER;",
                 NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    errmsg = NULL;
    sqlite3_exec(obsidian_db,
                 "ALTER TABLE accounts ADD COLUMN scram_salt TEXT;",
                 NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    errmsg = NULL;
    sqlite3_exec(obsidian_db,
                 "ALTER TABLE accounts ADD COLUMN scram_iterations INTEGER;",
                 NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    errmsg = NULL;
    sqlite3_exec(obsidian_db,
                 "ALTER TABLE accounts ADD COLUMN scram_stored_key TEXT;",
                 NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    errmsg = NULL;
    sqlite3_exec(obsidian_db,
                 "ALTER TABLE accounts ADD COLUMN scram_server_key TEXT;",
                 NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    errmsg = NULL;
    sqlite3_exec(obsidian_db,
                 "ALTER TABLE accounts ADD COLUMN twofa_enabled INTEGER DEFAULT 0;",
                 NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);

    return SQLITE_OK;
}

void obsidian_close_database(void)
{
    if (obsidian_db)
    {
        sqlite3_close(obsidian_db);
        obsidian_db = NULL;
    }
}

void free_account(Account *acc)
{
    if (!acc)
        return;
    free(acc->name);
    free(acc->email);
    free(acc->password);
    free(acc->verify_code);
    free(acc->scram_salt);
    free(acc->scram_stored_key);
    free(acc->scram_server_key);
    if (acc->channels)
    {
        for (char **c = acc->channels; *c; c++)
            free(*c);
        free(acc->channels);
    }
    /* Free online member list (Client* pointers are NOT freed) */
    AccountMember *m = acc->members, *mnext;
    while (m)
    {
        mnext = m->next;
        free(m);
        m = mnext;
    }
    free_metadata(acc->metadata_head);
    free(acc);
}

int write_account_to_db(const Account *acc)
{
    const char *sql =
        "INSERT INTO accounts"
        " (name, email, password, time_registered, verified,"
        "  verify_code, verify_expires,"
        "  scram_salt, scram_iterations, scram_stored_key, scram_server_key,"
        "  twofa_enabled)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;
    int result;

    if (!obsidian_db)
        return 0;
    if (find_account(acc->name))
        return 0; /* already exists */

    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, acc->name,  -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, acc->email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, acc->password, -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 4, (int)acc->time_registered);
    sqlite3_bind_int (stmt, 5, acc->verified);
    if (acc->verify_code)
        sqlite3_bind_text(stmt, 6, acc->verify_code, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 6);
    if (acc->verify_expires)
        sqlite3_bind_int(stmt, 7, (int)acc->verify_expires);
    else
        sqlite3_bind_null(stmt, 7);
    if (acc->scram_salt)
        sqlite3_bind_text(stmt, 8, acc->scram_salt, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 8);
    if (acc->scram_iterations)
        sqlite3_bind_int(stmt, 9, acc->scram_iterations);
    else
        sqlite3_bind_null(stmt, 9);
    if (acc->scram_stored_key)
        sqlite3_bind_text(stmt, 10, acc->scram_stored_key, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 10);
    if (acc->scram_server_key)
        sqlite3_bind_text(stmt, 11, acc->scram_server_key, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 11);
    sqlite3_bind_int(stmt, 12, acc->twofa_enabled);

    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE ? 1 : 0;
}

int update_account_twofa_enabled(const Account *acc)
{
    const char *sql = "UPDATE accounts SET twofa_enabled = ? WHERE id = ?";
    sqlite3_stmt *stmt;
    int result;

    if (!obsidian_db)
        return 0;
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_int(stmt, 1, acc->twofa_enabled);
    sqlite3_bind_int(stmt, 2, (int)acc->id);
    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE ? 1 : 0;
}

int update_account_scram(const Account *acc)
{
    const char *sql =
        "UPDATE accounts SET scram_salt = ?, scram_iterations = ?,"
        "  scram_stored_key = ?, scram_server_key = ?"
        " WHERE id = ?";
    sqlite3_stmt *stmt;
    int result;

    if (!obsidian_db)
        return 0;
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, acc->scram_salt, -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 2, acc->scram_iterations);
    sqlite3_bind_text(stmt, 3, acc->scram_stored_key, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, acc->scram_server_key, -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 5, (int)acc->id);

    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE ? 1 : 0;
}

int update_account_verification(const Account *acc)
{
    const char *sql =
        "UPDATE accounts SET verified = ?, verify_code = ?, verify_expires = ?"
        " WHERE id = ?";
    sqlite3_stmt *stmt;
    int result;

    if (!obsidian_db)
        return 0;
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, acc->verified);
    if (acc->verify_code)
        sqlite3_bind_text(stmt, 2, acc->verify_code, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 2);
    if (acc->verify_expires)
        sqlite3_bind_int(stmt, 3, (int)acc->verify_expires);
    else
        sqlite3_bind_null(stmt, 3);
    sqlite3_bind_int(stmt, 4, (int)acc->id);

    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE ? 1 : 0;
}

Account **read_accounts_from_db(const char *name)
{
    const char *sql_all = "SELECT id,name,email,password,time_registered,verified,"
                          "verify_code,verify_expires,"
                          "scram_salt,scram_iterations,scram_stored_key,scram_server_key,"
                          "twofa_enabled"
                          " FROM accounts";
    const char *sql_one = "SELECT id,name,email,password,time_registered,verified,"
                          "verify_code,verify_expires,"
                          "scram_salt,scram_iterations,scram_stored_key,scram_server_key,"
                          "twofa_enabled"
                          " FROM accounts WHERE lower(name) = lower(?) LIMIT 1";
    sqlite3_stmt *stmt;
    Account **accounts = NULL;
    size_t count = 0;
    Client *cptr;

    if (!obsidian_db)
        return NULL;

    if (sqlite3_prepare_v2(obsidian_db, name ? sql_one : sql_all, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;

    if (name)
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        Account *acc = safe_alloc(sizeof(Account));
        const unsigned char *col;
        acc->id             = sqlite3_column_int(stmt, 0);
        acc->name           = strdup((const char *)sqlite3_column_text(stmt, 1));
        acc->email          = strdup((const char *)sqlite3_column_text(stmt, 2));
        acc->password       = strdup((const char *)sqlite3_column_text(stmt, 3));
        acc->time_registered= (time_t)sqlite3_column_int(stmt, 4);
        acc->verified       = sqlite3_column_int(stmt, 5);
        col = sqlite3_column_text(stmt, 6);
        acc->verify_code    = col ? strdup((const char *)col) : NULL;
        acc->verify_expires = (time_t)sqlite3_column_int(stmt, 7);
        col = sqlite3_column_text(stmt, 8);
        acc->scram_salt     = col ? strdup((const char *)col) : NULL;
        acc->scram_iterations = sqlite3_column_int(stmt, 9);
        col = sqlite3_column_text(stmt, 10);
        acc->scram_stored_key = col ? strdup((const char *)col) : NULL;
        col = sqlite3_column_text(stmt, 11);
        acc->scram_server_key = col ? strdup((const char *)col) : NULL;
        acc->twofa_enabled = sqlite3_column_int(stmt, 12);
        acc->channels       = NULL;
        acc->metadata_head  = NULL;
        acc->members        = NULL;

        /* Populate online clients for this account */
        list_for_each_entry(cptr, &client_list, client_node)
        {
            if (IsUser(cptr) && IsLoggedIn(cptr) &&
                !strcasecmp(cptr->user->account, acc->name))
            {
                AccountMember *member = safe_alloc(sizeof(AccountMember));
                member->client = cptr;
                member->next   = acc->members;
                acc->members   = member;
            }
        }

        Account **tmp = realloc(accounts, sizeof(Account *) * (count + 2));
        if (!tmp)
        {
            for (size_t i = 0; accounts && accounts[i]; i++)
                free_account(accounts[i]);
            free(accounts);
            sqlite3_finalize(stmt);
            free_account(acc);
            return NULL;
        }
        accounts = tmp;
        accounts[count++] = acc;
    }
    sqlite3_finalize(stmt);

    if (!accounts)
    {
        accounts = safe_alloc(sizeof(Account *));
        accounts[0] = NULL;
    }
    else
    {
        accounts[count] = NULL;
    }
    return accounts;
}

Account *find_account(const char *name)
{
    Account **accounts;
    Account *result = NULL;

    if (!obsidian_db || !name)
        return NULL;

    accounts = read_accounts_from_db(name);
    if (!accounts)
        return NULL;

    if (accounts[0])
    {
        result      = accounts[0];
        accounts[0] = NULL; /* don't free the result */
    }
    for (size_t i = 0; accounts[i]; i++)
        free_account(accounts[i]);
    free(accounts);
    return result;
}

Account *find_account_by_client(Client *client)
{
    if (!obsidian_db || !client || !IsLoggedIn(client))
        return NULL;
    return find_account(client->user->account);
}

/* ===================================================================
 * Metadata helpers
 * =================================================================== */
Metadata *create_metadata(const char *key, const char *value)
{
    Metadata *m = safe_alloc(sizeof(Metadata));
    m->key   = strdup(key);
    m->value = strdup(value);
    m->prev = m->next = NULL;
    return m;
}

void add_metadata(Account *acc, const char *key, const char *value)
{
    Metadata *m = create_metadata(key, value);
    m->next = acc->metadata_head;
    if (acc->metadata_head)
        acc->metadata_head->prev = m;
    acc->metadata_head = m;
}

void free_metadata(Metadata *head)
{
    Metadata *cur = head, *tmp;
    while (cur)
    {
        tmp = cur->next;
        free(cur->key);
        free(cur->value);
        free(cur);
        cur = tmp;
    }
}

/* ===================================================================
 * TKL nameban check
 * =================================================================== */
TKL *my_find_tkl_nameban(const char *name)
{
    TKL *tkl;
    for (tkl = tklines[tkl_hash('Q')]; tkl; tkl = tkl->next)
    {
        if (!TKLIsNameBan(tkl))
            continue;
        if (!strcasecmp(name, tkl->ptr.nameban->name))
            return tkl;
    }
    return NULL;
}

/* ===================================================================
 * SASL ModData serialization
 * =================================================================== */
void sat_free(ModData *m)
{
    m->i = 0;
}

const char *sat_serialize(ModData *m)
{
    static char buf[32];
    if (m->i == 0)
        return NULL;
    snprintf(buf, sizeof(buf), "%d", m->i);
    return buf;
}

void sat_unserialize(const char *str, ModData *m)
{
    m->i = atoi(str);
}

/* ===================================================================
 * SCRAM-SHA-256 (RFC 7677) implementation
 * =================================================================== */
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

struct ScramState_
{
    int step;                /* 0 = expect client-first, 1 = expect client-final */
    Account *account;        /* loaded after client-first; NULL until then */
    char *client_first_bare; /* "n=user,r=clientNonce" */
    char *server_first;      /* "r=combinedNonce,s=salt,i=iters" */
    char *combined_nonce;    /* what we put in server-first's r= */
};

#define ScramGet(c)  ((ScramState *)moddata_local_client((c), scram_md).ptr)
#define ScramSet(c, p) do { moddata_local_client((c), scram_md).ptr = (p); } while (0)

static void scram_state_free(ScramState *s)
{
    if (!s) return;
    free_account(s->account);
    safe_free(s->client_first_bare);
    safe_free(s->server_first);
    safe_free(s->combined_nonce);
    safe_free(s);
}

static void scram_md_free(ModData *m)
{
    if (m->ptr)
    {
        scram_state_free((ScramState *)m->ptr);
        m->ptr = NULL;
    }
}

static void scram_clear(Client *client)
{
    ScramState *s = ScramGet(client);
    if (s)
    {
        scram_state_free(s);
        ScramSet(client, NULL);
    }
}

/* ----- Crypto primitives ----- */

static void scram_xor(const unsigned char *a, const unsigned char *b,
                      size_t n, unsigned char *out)
{
    size_t i;
    for (i = 0; i < n; i++)
        out[i] = a[i] ^ b[i];
}

/** Compute (StoredKey, ServerKey) from (password, salt, iterations). */
static int scram_compute_credentials(const char *password, size_t passlen,
                                     const unsigned char *salt, size_t saltlen,
                                     int iterations,
                                     unsigned char stored_key_out[SCRAM_KEY_BYTES],
                                     unsigned char server_key_out[SCRAM_KEY_BYTES])
{
    unsigned char salted[SCRAM_KEY_BYTES];
    unsigned char client_key[SCRAM_KEY_BYTES];
    unsigned int  outlen = 0;
    int ok = 0;

    if (!PKCS5_PBKDF2_HMAC(password, (int)passlen,
                           salt, (int)saltlen,
                           iterations,
                           EVP_sha256(),
                           SCRAM_KEY_BYTES, salted))
        goto out;

    if (!HMAC(EVP_sha256(), salted, SCRAM_KEY_BYTES,
              (const unsigned char *)"Client Key", 10,
              client_key, &outlen) ||
        outlen != SCRAM_KEY_BYTES)
        goto out;

    if (!SHA256(client_key, SCRAM_KEY_BYTES, stored_key_out))
        goto out;

    if (!HMAC(EVP_sha256(), salted, SCRAM_KEY_BYTES,
              (const unsigned char *)"Server Key", 10,
              server_key_out, &outlen) ||
        outlen != SCRAM_KEY_BYTES)
        goto out;

    ok = 1;
out:
    /* Wipe key material from the stack so it can't linger and leak via
     * uninitialised-stack reads in unrelated callers. */
    OPENSSL_cleanse(salted, sizeof(salted));
    OPENSSL_cleanse(client_key, sizeof(client_key));
    return ok;
}

/** Generate fresh SCRAM credentials from a plaintext password and write them
 *  into acc->scram_*.  Returns 1 on success, 0 on crypto failure. */
static int scram_make_credentials(Account *acc, const char *password)
{
    unsigned char salt[SCRAM_SALT_BYTES];
    unsigned char stored_key[SCRAM_KEY_BYTES];
    unsigned char server_key[SCRAM_KEY_BYTES];
    char salt_b64[64];
    char stored_b64[64];
    char server_b64[64];
    int ok = 0;

    if (RAND_bytes(salt, SCRAM_SALT_BYTES) != 1)
        goto out;

    if (!scram_compute_credentials(password, strlen(password),
                                   salt, SCRAM_SALT_BYTES,
                                   SCRAM_DEFAULT_ITERATIONS,
                                   stored_key, server_key))
        goto out;

    if (b64_encode(salt, SCRAM_SALT_BYTES, salt_b64, sizeof(salt_b64)) <= 0 ||
        b64_encode(stored_key, SCRAM_KEY_BYTES, stored_b64, sizeof(stored_b64)) <= 0 ||
        b64_encode(server_key, SCRAM_KEY_BYTES, server_b64, sizeof(server_b64)) <= 0)
        goto out;

    free(acc->scram_salt);
    free(acc->scram_stored_key);
    free(acc->scram_server_key);
    acc->scram_salt       = strdup(salt_b64);
    acc->scram_iterations = SCRAM_DEFAULT_ITERATIONS;
    acc->scram_stored_key = strdup(stored_b64);
    acc->scram_server_key = strdup(server_b64);
    ok = 1;
out:
    OPENSSL_cleanse(salt, sizeof(salt));
    OPENSSL_cleanse(stored_key, sizeof(stored_key));
    OPENSSL_cleanse(server_key, sizeof(server_key));
    /* The base64 encodings of stored_key / server_key end up persisted in the
     * DB anyway, so cleansing them here is mostly belt-and-suspenders. */
    OPENSSL_cleanse(stored_b64, sizeof(stored_b64));
    OPENSSL_cleanse(server_b64, sizeof(server_b64));
    return ok;
}

/* ----- Wire-format parsers ----- */

/* SCRAM usernames have ',' and '=' escaped as =2C and =3D.  Returns a
 * heap-allocated unescaped string, or NULL on malformed input. */
static char *scram_unescape_username(const char *s)
{
    size_t n = strlen(s);
    char *out = safe_alloc(n + 1);
    char *q = out;
    while (*s)
    {
        if (*s == '=')
        {
            if (s[1] == '2' && s[2] == 'C') { *q++ = ','; s += 3; continue; }
            if (s[1] == '3' && s[2] == 'D') { *q++ = '='; s += 3; continue; }
            safe_free(out);
            return NULL;
        }
        if (*s == ',')
        {
            safe_free(out);
            return NULL;
        }
        *q++ = *s++;
    }
    *q = 0;
    return out;
}

/* Extract attribute from "n=foo,r=bar,..." sequence.  Returns a heap-allocated
 * string, or NULL if the attribute isn't present. */
static char *scram_get_attr(const char *msg, char attr)
{
    const char *p = msg;
    while (*p)
    {
        char a = *p++;
        if (*p++ != '=') return NULL;
        const char *vstart = p;
        while (*p && *p != ',') p++;
        if (a == attr)
        {
            size_t vlen = (size_t)(p - vstart);
            char *out = safe_alloc(vlen + 1);
            memcpy(out, vstart, vlen);
            out[vlen] = 0;
            return out;
        }
        if (*p == ',') p++;
    }
    return NULL;
}

/* Parse client-first-message.  Layout: gs2-header "," client-first-message-bare
 *   gs2-header   = "n,," | "y,," (we don't accept channel binding, no authzid)
 *   bare         = "n=username,r=clientNonce" (additional attrs allowed/ignored)
 * On success populates *bare, *username, *client_nonce (all heap-allocated). */
static int scram_parse_client_first(const char *msg,
                                    char **bare_out,
                                    char **username_out,
                                    char **client_nonce_out)
{
    const char *p = msg;
    const char *bare;
    char *username = NULL, *escaped_user = NULL, *cnonce = NULL;

    *bare_out = *username_out = *client_nonce_out = NULL;

    /* gs2 cb-flag: 'n' or 'y' (we reject 'p' since we don't advertise -PLUS) */
    if (*p != 'n' && *p != 'y') return 0;
    p++;
    if (*p++ != ',') return 0;
    /* authzid optional ("a=..."), we don't accept one for v1 */
    if (*p && *p != ',') return 0;
    if (*p++ != ',') return 0;

    bare = p;

    escaped_user = scram_get_attr(bare, 'n');
    cnonce       = scram_get_attr(bare, 'r');
    if (!escaped_user || !cnonce)
    {
        safe_free(escaped_user);
        safe_free(cnonce);
        return 0;
    }

    username = scram_unescape_username(escaped_user);
    safe_free(escaped_user);
    if (!username)
    {
        safe_free(cnonce);
        return 0;
    }

    safe_strdup(*bare_out, bare);
    *username_out = username;
    *client_nonce_out = cnonce;
    return 1;
}

/* ----- Protocol handlers ----- */

static void scram_fail(Client *client)
{
    client->local->sasl_sent_time = 0;
    add_fake_lag(client, 5000);
    sendnumeric(client, ERR_SASLFAIL);
    DelSaslType(client);
    scram_clear(client);
}

static int scram_send_b64(Client *client, const char *plain, size_t len)
{
    /* Result line: "AUTHENTICATE <base64>" -- max 400 bytes per IRC SASL.
     * If empty, send "+" placeholder. */
    if (len == 0)
    {
        sendto_one(client, NULL, ":%s AUTHENTICATE +", me.name);
        return 1;
    }
    /* Worst-case b64 size = ((len+2)/3)*4 + 1 */
    size_t outsize = ((len + 2) / 3) * 4 + 1;
    char *b64 = safe_alloc(outsize);
    int n = b64_encode((const unsigned char *)plain, len, b64, outsize);
    if (n <= 0 || n > 400)
    {
        safe_free(b64);
        return 0;
    }
    sendto_one(client, NULL, ":%s AUTHENTICATE %s", me.name, b64);
    safe_free(b64);
    return 1;
}

static void scram_handle_client_first(Client *client, const char *msg, size_t msglen)
{
    char *bare = NULL, *username = NULL, *cnonce = NULL;
    Account *account = NULL;
    char server_nonce[SCRAM_SERVER_NONCE_BYTES + 1];
    char *combined = NULL;
    char *server_first = NULL;
    int sf_len;
    ScramState *st;
    (void)msglen;

    if (!scram_parse_client_first(msg, &bare, &username, &cnonce))
        goto fail;

    account = find_account(username);
    if (!account || !account->scram_salt || !account->scram_stored_key ||
        account->scram_iterations <= 0)
    {
        /* Don't leak whether the account exists.  Generate a dummy nonce
         * and reply normally; final step will fail with bad proof. */
        free_account(account);
        account = NULL;
        goto fail; /* simpler: just fail outright (legitimate clients only hit this rarely) */
    }

    gen_random_alnum(server_nonce, SCRAM_SERVER_NONCE_BYTES);
    server_nonce[SCRAM_SERVER_NONCE_BYTES] = 0;

    /* combined nonce = client nonce || server nonce */
    {
        size_t need = strlen(cnonce) + SCRAM_SERVER_NONCE_BYTES + 1;
        combined = safe_alloc(need);
        snprintf(combined, need, "%s%s", cnonce, server_nonce);
    }

    /* server-first-message = "r=" combined "," "s=" salt "," "i=" iters */
    {
        size_t need = 16 + strlen(combined) + strlen(account->scram_salt) + 16;
        server_first = safe_alloc(need);
        sf_len = snprintf(server_first, need, "r=%s,s=%s,i=%d",
                          combined, account->scram_salt,
                          account->scram_iterations);
    }

    /* Persist state for step 2. */
    st = safe_alloc(sizeof(*st));
    st->step              = 1;
    st->account           = account;            account = NULL; /* moved */
    safe_strdup(st->client_first_bare, bare);
    safe_strdup(st->server_first, server_first);
    safe_strdup(st->combined_nonce, combined);

    scram_clear(client); /* in case of stale state */
    ScramSet(client, st);

    if (!scram_send_b64(client, server_first, sf_len))
        goto fail;

    safe_free(bare);
    safe_free(username);
    safe_free(cnonce);
    safe_free(combined);
    safe_free(server_first);
    return;

fail:
    safe_free(bare);
    safe_free(username);
    safe_free(cnonce);
    safe_free(combined);
    safe_free(server_first);
    free_account(account);
    scram_fail(client);
}

static void scram_handle_client_final(Client *client, const char *msg, size_t msglen)
{
    ScramState *st = ScramGet(client);
    char *cb_b64 = NULL;          /* "c=" attribute (channel binding header) */
    char *full_nonce = NULL;
    char *proof_b64 = NULL;
    char *final_no_proof = NULL;
    Account *acc;
    unsigned char client_proof[SCRAM_KEY_BYTES];
    unsigned char stored_key[SCRAM_KEY_BYTES];
    unsigned char server_key[SCRAM_KEY_BYTES];
    unsigned char client_signature[SCRAM_KEY_BYTES];
    unsigned char client_key[SCRAM_KEY_BYTES];
    unsigned char computed_stored[SCRAM_KEY_BYTES];
    unsigned char server_sig[SCRAM_KEY_BYTES];
    unsigned int  outlen = 0;
    char *auth_message = NULL;
    char server_final[80];
    int sf_len;
    (void)msglen;

    if (!st || st->step != 1 || !st->account)
    {
        scram_fail(client);
        return;
    }
    acc = st->account;
    if (!acc->scram_stored_key || !acc->scram_server_key)
    {
        scram_fail(client);
        return;
    }

    /* Extract attributes.  We need at minimum c, r, p. */
    cb_b64     = scram_get_attr(msg, 'c');
    full_nonce = scram_get_attr(msg, 'r');
    proof_b64  = scram_get_attr(msg, 'p');
    if (!cb_b64 || !full_nonce || !proof_b64)
        goto fail;

    /* "biws" is base64 of "n,," -- the only GS2 header we accept. */
    if (strcmp(cb_b64, "biws") && strcmp(cb_b64, "eSws") /* "y,," */)
        goto fail;

    /* Nonce must match exactly. */
    if (strcmp(full_nonce, st->combined_nonce))
        goto fail;

    /* Decode client proof. */
    {
        unsigned char tmp[64];
        int n = b64_decode(proof_b64, tmp, sizeof(tmp));
        if (n != SCRAM_KEY_BYTES)
            goto fail;
        memcpy(client_proof, tmp, SCRAM_KEY_BYTES);
    }

    /* Decode stored_key and server_key from account. */
    {
        unsigned char tmp[64];
        int n = b64_decode(acc->scram_stored_key, tmp, sizeof(tmp));
        if (n != SCRAM_KEY_BYTES)
            goto fail;
        memcpy(stored_key, tmp, SCRAM_KEY_BYTES);

        n = b64_decode(acc->scram_server_key, tmp, sizeof(tmp));
        if (n != SCRAM_KEY_BYTES)
            goto fail;
        memcpy(server_key, tmp, SCRAM_KEY_BYTES);
    }

    /* Build client-final-message-without-proof: drop ",p=..." tail */
    {
        const char *p_attr = strstr(msg, ",p=");
        if (!p_attr)
            goto fail;
        size_t len = (size_t)(p_attr - msg);
        final_no_proof = safe_alloc(len + 1);
        memcpy(final_no_proof, msg, len);
        final_no_proof[len] = 0;
    }

    /* AuthMessage = client_first_bare + "," + server_first + "," + final_no_proof */
    {
        size_t need = strlen(st->client_first_bare) + 1 +
                      strlen(st->server_first) + 1 +
                      strlen(final_no_proof) + 1;
        auth_message = safe_alloc(need);
        snprintf(auth_message, need, "%s,%s,%s",
                 st->client_first_bare, st->server_first, final_no_proof);
    }

    /* ClientSignature = HMAC(StoredKey, AuthMessage) */
    if (!HMAC(EVP_sha256(), stored_key, SCRAM_KEY_BYTES,
              (const unsigned char *)auth_message, strlen(auth_message),
              client_signature, &outlen) ||
        outlen != SCRAM_KEY_BYTES)
        goto fail;

    /* ClientKey = ClientProof XOR ClientSignature */
    scram_xor(client_proof, client_signature, SCRAM_KEY_BYTES, client_key);

    /* Verify SHA-256(ClientKey) == StoredKey using a constant-time compare so
     * the failure path doesn't leak how many leading bytes matched. */
    if (!SHA256(client_key, SCRAM_KEY_BYTES, computed_stored))
        goto fail;
    if (CRYPTO_memcmp(computed_stored, stored_key, SCRAM_KEY_BYTES) != 0)
        goto fail;

    /* ServerSignature = HMAC(ServerKey, AuthMessage) */
    if (!HMAC(EVP_sha256(), server_key, SCRAM_KEY_BYTES,
              (const unsigned char *)auth_message, strlen(auth_message),
              server_sig, &outlen) ||
        outlen != SCRAM_KEY_BYTES)
        goto fail;

    /* Reply: server-final-message = "v=" base64(server_sig) */
    {
        char sig_b64[64];
        if (b64_encode(server_sig, SCRAM_KEY_BYTES, sig_b64, sizeof(sig_b64)) <= 0)
            goto fail;
        sf_len = snprintf(server_final, sizeof(server_final), "v=%s", sig_b64);
    }

    if (!scram_send_b64(client, server_final, sf_len))
        goto fail;

    /* If 2FA is enforced, swap into step-up mode instead of completing
     * login.  Save the account name (twofa_maybe_start_stepup makes a copy)
     * BEFORE scram_clear() destroys st->account. */
    if (twofa_maybe_start_stepup(client, acc))
    {
        DelSaslType(client);
        scram_clear(client);
        safe_free(cb_b64);
        safe_free(full_nonce);
        safe_free(proof_b64);
        safe_free(final_no_proof);
        safe_free(auth_message);
        OPENSSL_cleanse(client_proof, sizeof(client_proof));
        OPENSSL_cleanse(stored_key, sizeof(stored_key));
        OPENSSL_cleanse(server_key, sizeof(server_key));
        OPENSSL_cleanse(client_signature, sizeof(client_signature));
        OPENSSL_cleanse(client_key, sizeof(client_key));
        OPENSSL_cleanse(computed_stored, sizeof(computed_stored));
        OPENSSL_cleanse(server_sig, sizeof(server_sig));
        return;
    }

    /* Log the user in. */
    strlcpy(client->user->account, acc->name, sizeof(client->user->account));
    unreal_log(ULOG_INFO, "account", "SASL_LOGIN", client,
               "SASL/SCRAM-SHA-256 login for $client.details "
               "[account: $account] [email: $email]",
               log_data_string("account", acc->name),
               log_data_string("email",   acc->email));
    user_account_login(NULL, client);
    if (!IsDead(client))
    {
        client->local->sasl_complete = 1;
        sendnumeric(client, RPL_SASLSUCCESS);
    }
    DelSaslType(client);
    scram_clear(client);

    safe_free(cb_b64);
    safe_free(full_nonce);
    safe_free(proof_b64);
    safe_free(final_no_proof);
    safe_free(auth_message);
    OPENSSL_cleanse(client_proof, sizeof(client_proof));
    OPENSSL_cleanse(stored_key, sizeof(stored_key));
    OPENSSL_cleanse(server_key, sizeof(server_key));
    OPENSSL_cleanse(client_signature, sizeof(client_signature));
    OPENSSL_cleanse(client_key, sizeof(client_key));
    OPENSSL_cleanse(computed_stored, sizeof(computed_stored));
    OPENSSL_cleanse(server_sig, sizeof(server_sig));
    return;

fail:
    safe_free(cb_b64);
    safe_free(full_nonce);
    safe_free(proof_b64);
    safe_free(final_no_proof);
    safe_free(auth_message);
    OPENSSL_cleanse(client_proof, sizeof(client_proof));
    OPENSSL_cleanse(stored_key, sizeof(stored_key));
    OPENSSL_cleanse(server_key, sizeof(server_key));
    OPENSSL_cleanse(client_signature, sizeof(client_signature));
    OPENSSL_cleanse(client_key, sizeof(client_key));
    OPENSSL_cleanse(computed_stored, sizeof(computed_stored));
    OPENSSL_cleanse(server_sig, sizeof(server_sig));
    scram_fail(client);
}

/* ===================================================================
 * CAP helpers
 * =================================================================== */
static const char *accreg_capability_parameter(Client *client)
{
    return "before-connect,custom-account-name,email-required";
}

static int accreg_capability_visible(Client *client)
{
    return 1;
}

/* ===================================================================
 * SASL hook: authenticate_attempt
 * Called by sasl.c cmd_authenticate when SASL_SERVER == &me.
 * =================================================================== */
static int authenticate_attempt(Client *client, int first, const char *param)
{
    if (!SASL_SERVER || !MyConnect(client) || !param || !*param)
        return 0;

    /* If we are in the middle of a 2FA step-up, route AUTHENTICATE
     * messages to the step-up handler before normal mechanism dispatch. */
    {
        TwoFAStepup *s = TwoFAStepupGet(client);
        if (s && s->active)
        {
            if (twofa_handle_stepup_authenticate(client, param))
                return 0;
        }
    }

    if (!strcmp(param, "*"))
    {
        if (GetSaslType(client))
            DelSaslType(client);
        twofa_clear_stepup(client);
        return 0;
    }
    else if (!strcmp(param, "PLAIN"))
    {
        SetSaslType(client, SASL_TYPE_PLAIN);
        sendto_one(client, NULL, ":%s AUTHENTICATE +", me.name);
        return 0;
    }
    else if (!strcmp(param, "ANONYMOUS"))
    {
        strlcpy(client->user->account, "0", sizeof(client->user->account));
        user_account_login(NULL, client);
        if (IsDead(client))
            return 0;
        client->local->sasl_complete = 1;
        sendnumeric(client, RPL_SASLSUCCESS);
        DelSaslType(client);
        return 0;
    }
    else if (!strcmp(param, "EXTERNAL"))
    {
        SetSaslType(client, SASL_TYPE_EXTERNAL);
    }
    else if (!strcasecmp(param, "DRAFT-WEBAUTHN-BIO") ||
             !strcasecmp(param, "WEBAUTHN-BIO"))
    {
        SetSaslType(client, SASL_TYPE_WEBAUTHN_BIO);
        webauthn_sasl_clear(client);
        WebAuthnSaslState *st = safe_alloc(sizeof(*st));
        st->step = 0;
        if (RAND_bytes(st->challenge, sizeof(st->challenge)) != 1)
        {
            safe_free(st);
            sendnumeric(client, ERR_SASLFAIL);
            DelSaslType(client);
            return 0;
        }
        WebAuthnSaslSet(client, st);
        sendto_one(client, NULL, ":%s AUTHENTICATE +", me.name);
        return 0;
    }
    else if (!strcasecmp(param, "SCRAM-SHA-256"))
    {
        SetSaslType(client, SASL_TYPE_SCRAM_SHA_256);
        scram_clear(client);
        sendto_one(client, NULL, ":%s AUTHENTICATE +", me.name);
        return 0;
    }

    if (!GetSaslType(client) || GetSaslType(client) == SASL_TYPE_NONE)
        return 0;

    if (GetSaslType(client) == SASL_TYPE_WEBAUTHN_BIO)
    {
        WebAuthnSaslState *st = WebAuthnSaslGet(client);
        if (!st)
        {
            sendnumeric(client, ERR_SASLFAIL);
            DelSaslType(client);
            return 0;
        }
        if (st->step == 0)
            webauthn_sasl_handle_hello(client, param);
        else
            webauthn_sasl_handle_assertion(client, param);
        return 0;
    }

    if (GetSaslType(client) == SASL_TYPE_SCRAM_SHA_256)
    {
        unsigned char buf[1024];
        int n = b64_decode(param, buf, sizeof(buf) - 1);
        if (n <= 0)
        {
            scram_fail(client);
            return 0;
        }
        buf[n] = 0;

        ScramState *st = ScramGet(client);
        if (!st || st->step == 0)
            scram_handle_client_first(client, (const char *)buf, (size_t)n);
        else
            scram_handle_client_final(client, (const char *)buf, (size_t)n);
        return 0;
    }

    if (GetSaslType(client) == SASL_TYPE_PLAIN)
    {
        char *auth_id, *username, *password;

        if (!decode_authenticate_plain(param, &auth_id, &username, &password))
        {
            client->local->sasl_sent_time = 0;
            add_fake_lag(client, 7000);
            sendnumeric(client, ERR_SASLFAIL);
            DelSaslType(client);
            return 0;
        }

        if (BadPtr(username) || BadPtr(password))
        {
            sendnumeric(client, ERR_SASLFAIL);
            DelSaslType(client);
            return 0;
        }

        Account *account = find_account(username);
        if (account &&
            argon2_verify(account->password, password, strlen(password),
                          Argon2_id) == ARGON2_OK)
        {
            /* Opportunistic SCRAM credentials backfill: pre-existing accounts
             * have no scram_* columns; we have plaintext now, so populate. */
            if (!account->scram_salt || !account->scram_stored_key)
            {
                if (scram_make_credentials(account, password))
                    update_account_scram(account);
            }

            /* If 2FA is enforced, withhold the success reply and ask the
             * client to do a second-factor SASL exchange. */
            if (twofa_maybe_start_stepup(client, account))
            {
                /* Keep SaslType set so subsequent AUTHENTICATE messages
                 * keep flowing through this hook; the step-up handler
                 * dispatches them. */
                free_account(account);
                return 0;
            }

            strlcpy(client->user->account, account->name,
                    sizeof(client->user->account));
            unreal_log(ULOG_INFO, "account", "SASL_LOGIN", client,
                       "SASL login for $client.details [account: $account] [email: $email]",
                       log_data_string("account", account->name),
                       log_data_string("email",   account->email));
            user_account_login(NULL, client);
            client->local->sasl_complete = 1;
            sendnumeric(client, RPL_SASLSUCCESS);
            DelSaslType(client);
        }
        else
        {
            client->local->sasl_sent_time = 0;
            add_fake_lag(client, 7000);
            sendnumeric(client, ERR_SASLFAIL);
            DelSaslType(client);
        }
        free_account(account);
    }
    return 0;
}

/* ===================================================================
 * 2FA - draft/account-2fa (TOTP / RFC 6238)
 * =================================================================== */

/* ----- Base32 (RFC 4648) ----- */

static int base32_encode(const unsigned char *in, size_t in_len,
                         char *out, size_t out_size)
{
    static const char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    size_t in_pos = 0, out_pos = 0;
    int bits = 0;
    unsigned int value = 0;

    while (in_pos < in_len)
    {
        value = (value << 8) | in[in_pos++];
        bits += 8;
        while (bits >= 5)
        {
            if (out_pos + 1 >= out_size) return -1;
            out[out_pos++] = alpha[(value >> (bits - 5)) & 0x1F];
            bits -= 5;
        }
    }
    if (bits > 0)
    {
        if (out_pos + 1 >= out_size) return -1;
        out[out_pos++] = alpha[(value << (5 - bits)) & 0x1F];
    }
    while (out_pos % 8 != 0)
    {
        if (out_pos + 1 >= out_size) return -1;
        out[out_pos++] = '=';
    }
    if (out_pos >= out_size) return -1;
    out[out_pos] = '\0';
    return (int)out_pos;
}

static int base32_decode(const char *in, unsigned char *out, size_t out_size)
{
    int bits = 0;
    unsigned int value = 0;
    size_t out_pos = 0;
    while (*in && *in != '=')
    {
        char c = *in++;
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a';
        else if (c >= '2' && c <= '7') v = c - '2' + 26;
        else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        else return -1;
        value = (value << 5) | v;
        bits += 5;
        if (bits >= 8)
        {
            if (out_pos >= out_size) return -1;
            out[out_pos++] = (value >> (bits - 8)) & 0xFF;
            bits -= 8;
        }
    }
    return (int)out_pos;
}

/* ----- HMAC-SHA1 TOTP (RFC 6238) ----- */

static int totp_compute_at(const unsigned char *key, size_t key_len,
                           uint64_t counter)
{
    unsigned char counter_buf[8];
    unsigned char hmac_buf[20];
    unsigned int hmac_len = 0;
    int offset, code;

    for (int i = 7; i >= 0; i--)
    {
        counter_buf[i] = counter & 0xFF;
        counter >>= 8;
    }

    if (!HMAC(EVP_sha1(), key, (int)key_len, counter_buf, sizeof(counter_buf),
              hmac_buf, &hmac_len) || hmac_len != 20)
    {
        OPENSSL_cleanse(hmac_buf, sizeof(hmac_buf));
        return -1;
    }

    offset = hmac_buf[19] & 0x0F;
    code = ((hmac_buf[offset]   & 0x7F) << 24) |
           ((hmac_buf[offset+1] & 0xFF) << 16) |
           ((hmac_buf[offset+2] & 0xFF) << 8)  |
           ( hmac_buf[offset+3] & 0xFF);
    OPENSSL_cleanse(hmac_buf, sizeof(hmac_buf));
    return code % 1000000;
}

/** Returns 1 if 'code_str' (6 ASCII digits) matches the TOTP for 'secret_b32'
 *  within the configured ±N skew window.  Constant-time compare.  */
static int totp_verify(const char *secret_b32, const char *code_str)
{
    unsigned char key[64];
    int  key_len;
    int  matched = 0;
    uint64_t now;

    if (!secret_b32 || !code_str || strlen(code_str) != TWOFA_CODE_DIGITS)
        return 0;
    /* All chars must be ASCII digits */
    for (int i = 0; i < TWOFA_CODE_DIGITS; i++)
        if (code_str[i] < '0' || code_str[i] > '9')
            return 0;

    key_len = base32_decode(secret_b32, key, sizeof(key));
    if (key_len <= 0)
    {
        OPENSSL_cleanse(key, sizeof(key));
        return 0;
    }

    now = (uint64_t)time(NULL) / TWOFA_PERIOD_SECONDS;
    for (int delta = -TWOFA_SKEW_WINDOWS; delta <= TWOFA_SKEW_WINDOWS; delta++)
    {
        int expected = totp_compute_at(key, key_len, now + delta);
        if (expected < 0)
            continue;
        char expected_str[TWOFA_CODE_DIGITS + 1];
        snprintf(expected_str, sizeof(expected_str), "%0*d",
                 TWOFA_CODE_DIGITS, expected);
        if (CRYPTO_memcmp(expected_str, code_str, TWOFA_CODE_DIGITS) == 0)
            matched = 1;
        OPENSSL_cleanse(expected_str, sizeof(expected_str));
        /* Don't break early -- keep timing constant. */
    }

    OPENSSL_cleanse(key, sizeof(key));
    return matched;
}

static char *totp_make_secret_b32(void)
{
    unsigned char raw[TWOFA_SECRET_BYTES];
    char *out;
    int outsize = ((TWOFA_SECRET_BYTES + 4) / 5) * 8 + 1;

    if (RAND_bytes(raw, sizeof(raw)) != 1)
        return NULL;
    out = safe_alloc(outsize);
    if (base32_encode(raw, sizeof(raw), out, outsize) <= 0)
    {
        safe_free(out);
        OPENSSL_cleanse(raw, sizeof(raw));
        return NULL;
    }
    OPENSSL_cleanse(raw, sizeof(raw));
    return out;
}

/* ----- otpauth:// URI ----- */

static void otpauth_url_encode(const char *in, char *out, size_t out_size)
{
    size_t pos = 0;
    while (*in && pos + 4 < out_size)
    {
        unsigned char c = (unsigned char)*in++;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~')
        {
            out[pos++] = (char)c;
        }
        else
        {
            if (pos + 3 >= out_size) break;
            out[pos++] = '%';
            static const char hex[] = "0123456789ABCDEF";
            out[pos++] = hex[(c >> 4) & 0xF];
            out[pos++] = hex[c & 0xF];
        }
    }
    out[pos] = '\0';
}

static char *totp_build_otpauth_uri(const char *issuer, const char *account,
                                    const char *secret_b32)
{
    char enc_issuer[256];
    char enc_account[256];
    char *uri;
    size_t need;

    otpauth_url_encode(issuer, enc_issuer, sizeof(enc_issuer));
    otpauth_url_encode(account, enc_account, sizeof(enc_account));

    need = strlen(enc_issuer) * 2 + strlen(enc_account) +
           strlen(secret_b32) + 128;
    uri = safe_alloc(need);
    snprintf(uri, need,
             "otpauth://totp/%s:%s?secret=%s&issuer=%s"
             "&algorithm=SHA1&digits=%d&period=%d",
             enc_issuer, enc_account, secret_b32, enc_issuer,
             TWOFA_CODE_DIGITS, TWOFA_PERIOD_SECONDS);
    return uri;
}

/* ----- 2FA credential CRUD ----- */

void twofa_free_credential_list(TwoFACredential *head)
{
    TwoFACredential *cur = head, *n;
    while (cur)
    {
        n = cur->next;
        free(cur->type);
        free(cur->name);
        free(cur->secret);
        free(cur);
        cur = n;
    }
}

TwoFACredential *twofa_list_credentials(long int account_id)
{
    const char *sql =
        "SELECT id,account_id,type,name,secret,created_at"
        " FROM account_2fa_credentials WHERE account_id = ? ORDER BY id ASC";
    sqlite3_stmt *stmt;
    TwoFACredential *head = NULL, *tail = NULL;

    if (!obsidian_db) return NULL;
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_int(stmt, 1, (int)account_id);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        TwoFACredential *c = safe_alloc(sizeof(*c));
        c->id          = sqlite3_column_int(stmt, 0);
        c->account_id  = sqlite3_column_int(stmt, 1);
        c->type        = strdup((const char *)sqlite3_column_text(stmt, 2));
        c->name        = strdup((const char *)sqlite3_column_text(stmt, 3));
        c->secret      = strdup((const char *)sqlite3_column_text(stmt, 4));
        c->created_at  = (time_t)sqlite3_column_int(stmt, 5);
        c->next        = NULL;
        if (tail) tail->next = c;
        else      head = c;
        tail = c;
    }
    sqlite3_finalize(stmt);
    return head;
}

int twofa_count_credentials(long int account_id)
{
    const char *sql =
        "SELECT COUNT(*) FROM account_2fa_credentials WHERE account_id = ?";
    sqlite3_stmt *stmt;
    int n = -1;
    if (!obsidian_db) return -1;
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(stmt, 1, (int)account_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
        n = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return n;
}

int twofa_insert_credential(long int account_id, const char *type,
                            const char *name, const char *secret,
                            long int *out_id)
{
    const char *sql =
        "INSERT INTO account_2fa_credentials"
        " (account_id, type, name, secret, created_at) VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;
    int result;

    if (!obsidian_db) return 0;
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_int (stmt, 1, (int)account_id);
    sqlite3_bind_text(stmt, 2, type,   -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, name,   -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, secret, -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 5, (int)time(NULL));
    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (result != SQLITE_DONE) return 0;
    if (out_id)
        *out_id = (long int)sqlite3_last_insert_rowid(obsidian_db);
    return 1;
}

int twofa_delete_credential(long int account_id, long int cred_id)
{
    const char *sql =
        "DELETE FROM account_2fa_credentials"
        " WHERE id = ? AND account_id = ?";
    sqlite3_stmt *stmt;
    int result, rows;

    if (!obsidian_db) return 0;
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_int(stmt, 1, (int)cred_id);
    sqlite3_bind_int(stmt, 2, (int)account_id);
    result = sqlite3_step(stmt);
    rows = sqlite3_changes(obsidian_db);
    sqlite3_finalize(stmt);
    return (result == SQLITE_DONE && rows > 0) ? 1 : 0;
}

/* ----- Per-client enrolment + step-up state -----
 * (struct types and ModData macros are forward-declared near the top of
 * the file so the SASL hook can reference them.) */

static void twofa_enroll_free(TwoFAEnroll *e)
{
    if (!e) return;
    if (e->secret_b32)
        OPENSSL_cleanse(e->secret_b32, strlen(e->secret_b32));
    safe_free(e->secret_b32);
    safe_free(e->type);
    safe_free(e);
}

static void twofa_enroll_md_free(ModData *m)
{
    if (m->ptr)
    {
        twofa_enroll_free((TwoFAEnroll *)m->ptr);
        m->ptr = NULL;
    }
}

static void twofa_clear_enroll(Client *c)
{
    TwoFAEnroll *e = TwoFAEnrollGet(c);
    if (e)
    {
        twofa_enroll_free(e);
        TwoFAEnrollSet(c, NULL);
    }
}

static void twofa_stepup_md_free(ModData *m)
{
    if (m->ptr)
    {
        safe_free(m->ptr);
        m->ptr = NULL;
    }
}

static void twofa_clear_stepup(Client *c)
{
    TwoFAStepup *s = TwoFAStepupGet(c);
    if (s)
    {
        safe_free(s);
        TwoFAStepupSet(c, NULL);
    }
}

/* ----- Helpers ----- */

static int twofa_valid_name(const char *s)
{
    if (!s || !*s) return 0;
    size_t n = strlen(s);
    if (n > TWOFA_NAME_MAX) return 0;
    for (size_t i = 0; i < n; i++)
    {
        unsigned char c = (unsigned char)s[i];
        if (c <= 0x20 || c == 0x7F) return 0;
    }
    return 1;
}

static int parse_cred_id(const char *id_str, long int *out_id)
{
    /* Format: "cred-<int>". */
    if (!id_str || strncmp(id_str, "cred-", 5)) return 0;
    const char *p = id_str + 5;
    if (!*p) return 0;
    char *end;
    long n = strtol(p, &end, 10);
    if (*end || n <= 0) return 0;
    *out_id = (long int)n;
    return 1;
}

static void format_cred_id(char *out, size_t out_size, long int id)
{
    snprintf(out, out_size, "cred-%ld", id);
}

static char *iso8601_utc(time_t t)
{
    static char buf[32];
    struct tm *tm = gmtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
    return buf;
}

/* ----- Standard-replies senders ----- */

static void twofa_fail(Client *c, const char *code,
                       const char *p1, const char *msg)
{
    if (p1)
        sendto_one(c, NULL, ":%s FAIL 2FA %s %s :%s", me.name, code, p1, msg);
    else
        sendto_one(c, NULL, ":%s FAIL 2FA %s :%s", me.name, code, msg);
}

static void twofa_note(Client *c, const char *code,
                       const char *p1, const char *p2, const char *msg)
{
    if (p1 && p2)
        sendto_one(c, NULL, ":%s NOTE 2FA %s %s %s :%s",
                   me.name, code, p1, p2, msg);
    else if (p1)
        sendto_one(c, NULL, ":%s NOTE 2FA %s %s :%s",
                   me.name, code, p1, msg);
    else
        sendto_one(c, NULL, ":%s NOTE 2FA %s :%s", me.name, code, msg);
}

/* ----- Subcommand handlers ----- */

static void twofa_cmd_status(Client *client, Account *acc)
{
    if (acc->twofa_enabled)
        twofa_note(client, "ENABLED", NULL, NULL,
                   "Two-factor authentication is enabled on your account.");
    else
        twofa_note(client, "DISABLED", NULL, NULL,
                   "Two-factor authentication is not enabled on your account.");
}

static void twofa_cmd_list(Client *client, Account *acc)
{
    TwoFACredential *creds = twofa_list_credentials(acc->id);
    if (!creds)
    {
        twofa_note(client, "NO_CREDENTIALS", NULL, NULL,
                   "You have no 2FA credentials registered.");
        return;
    }
    char id_buf[TWOFA_ID_MAX + 1];
    for (TwoFACredential *c = creds; c; c = c->next)
    {
        format_cred_id(id_buf, sizeof(id_buf), c->id);
        sendto_one(client, NULL,
                   ":%s NOTE 2FA CREDENTIAL %s %s %s %s :Registered credential",
                   me.name, id_buf, c->type, c->name, iso8601_utc(c->created_at));
    }
    twofa_free_credential_list(creds);
}

static void twofa_cmd_challenge(Client *client, Account *acc, int parc, const char *parv[])
{
    const char *type;
    if (parc < 3 || BadPtr(parv[2]))
    {
        twofa_fail(client, "INVALID_TYPE", NULL,
                   "Syntax: /2FA CHALLENGE <type>");
        return;
    }
    type = parv[2];
    if (!strcasecmp(type, "webauthn"))
    {
        webauthn_2fa_handle_challenge(client, acc);
        return;
    }
    if (strcasecmp(type, TWOFA_TYPE_TOTP))
    {
        twofa_fail(client, "INVALID_TYPE", type,
                   "Unsupported credential type.");
        return;
    }

    char *secret = totp_make_secret_b32();
    if (!secret)
    {
        twofa_fail(client, "TEMPORARILY_UNAVAILABLE", NULL,
                   "Could not generate a TOTP secret.");
        return;
    }

    char *uri = totp_build_otpauth_uri(me.name, acc->name, secret);

    /* Build the JSON challenge payload, then base64 it. */
    json_t *j = json_object();
    json_object_set_new(j, "type",      json_string("totp"));
    json_object_set_new(j, "secret",    json_string(secret));
    json_object_set_new(j, "issuer",    json_string(me.name));
    json_object_set_new(j, "account",   json_string(acc->name));
    json_object_set_new(j, "algorithm", json_string("SHA1"));
    json_object_set_new(j, "digits",    json_integer(TWOFA_CODE_DIGITS));
    json_object_set_new(j, "period",    json_integer(TWOFA_PERIOD_SECONDS));
    json_object_set_new(j, "uri",       json_string(uri));
    char *json_str = json_dumps(j, JSON_COMPACT);
    json_decref(j);
    safe_free(uri);

    if (!json_str)
    {
        OPENSSL_cleanse(secret, strlen(secret));
        safe_free(secret);
        twofa_fail(client, "TEMPORARILY_UNAVAILABLE", NULL,
                   "Could not encode challenge.");
        return;
    }

    /* base64 the JSON */
    size_t jlen = strlen(json_str);
    size_t b64size = ((jlen + 2) / 3) * 4 + 1;
    char *b64 = safe_alloc(b64size);
    int n = b64_encode((const unsigned char *)json_str, jlen, b64, b64size);
    OPENSSL_cleanse(json_str, jlen);
    free(json_str);
    if (n <= 0)
    {
        OPENSSL_cleanse(secret, strlen(secret));
        safe_free(secret);
        safe_free(b64);
        twofa_fail(client, "TEMPORARILY_UNAVAILABLE", NULL,
                   "Could not encode challenge.");
        return;
    }

    /* Save the pending secret on the client. */
    twofa_clear_enroll(client);
    TwoFAEnroll *e = safe_alloc(sizeof(*e));
    safe_strdup(e->type, "totp");
    e->secret_b32 = secret; /* moved */
    e->expires_at = time(NULL) + TWOFA_CHALLENGE_LIFETIME;
    TwoFAEnrollSet(client, e);

    sendto_one(client, NULL,
               ":%s NOTE 2FA REGISTRATION_CHALLENGE totp %s :"
               "Add the secret to your authenticator app then run "
               "/2FA ADD totp <name> <code>",
               me.name, b64);
    safe_free(b64);
}

static void twofa_cmd_add(Client *client, Account *acc, int parc, const char *parv[])
{
    const char *type, *name, *data;

    if (parc < 5 || BadPtr(parv[2]) || BadPtr(parv[3]) || BadPtr(parv[4]))
    {
        twofa_fail(client, "INVALID_CREDENTIAL_DATA", NULL,
                   "Syntax: /2FA ADD <type> <name> <data>");
        return;
    }
    type = parv[2];
    name = parv[3];
    data = parv[4];

    if (!twofa_valid_name(name))
    {
        twofa_fail(client, "INVALID_NAME", NULL,
                   "Name must be printable, no whitespace, max 64 bytes.");
        return;
    }
    if (!strcasecmp(type, "webauthn"))
    {
        if (!webauthn_2fa_handle_add(client, acc, name, data))
        {
            add_fake_lag(client, 3000);
            twofa_fail(client, "INVALID_CREDENTIAL_DATA", NULL,
                       "WebAuthn registration data did not verify.");
        }
        return;
    }
    if (strcasecmp(type, TWOFA_TYPE_TOTP))
    {
        twofa_fail(client, "INVALID_TYPE", type,
                   "Unsupported credential type.");
        return;
    }

    TwoFAEnroll *e = TwoFAEnrollGet(client);
    if (!e || strcmp(e->type, "totp") || time(NULL) > e->expires_at ||
        !e->secret_b32)
    {
        twofa_fail(client, "NO_CHALLENGE", NULL,
                   "No active TOTP enrolment challenge; run /2FA CHALLENGE totp first.");
        return;
    }

    if (!totp_verify(e->secret_b32, data))
    {
        add_fake_lag(client, 3000);
        twofa_fail(client, "INVALID_CREDENTIAL_DATA", NULL,
                   "TOTP code did not verify.");
        return;
    }

    long int new_id = 0;
    if (!twofa_insert_credential(acc->id, "totp", name, e->secret_b32, &new_id))
    {
        twofa_fail(client, "TEMPORARILY_UNAVAILABLE", NULL,
                   "Could not persist credential.");
        return;
    }

    char id_buf[TWOFA_ID_MAX + 1];
    format_cred_id(id_buf, sizeof(id_buf), new_id);
    sendto_one(client, NULL,
               ":%s 2FA ADD SUCCESS totp %s :Credential '%s' registered.",
               me.name, id_buf, name);

    twofa_clear_enroll(client);
}

static void twofa_cmd_remove(Client *client, Account *acc, int parc, const char *parv[])
{
    const char *id_str;
    long int cred_id;
    int count;

    if (parc < 3 || BadPtr(parv[2]))
    {
        twofa_fail(client, "UNKNOWN_CREDENTIAL", NULL,
                   "Syntax: /2FA REMOVE <id>");
        return;
    }
    id_str = parv[2];
    if (!parse_cred_id(id_str, &cred_id))
    {
        twofa_fail(client, "UNKNOWN_CREDENTIAL", id_str,
                   "Bad credential id.");
        return;
    }

    if (acc->twofa_enabled)
    {
        count = twofa_count_credentials(acc->id);
        if (count <= 1)
        {
            twofa_fail(client, "REMOVE_LAST_CREDENTIAL", NULL,
                       "Cannot remove the last credential while 2FA is enabled.  "
                       "Run /2FA DISABLE first.");
            return;
        }
    }

    if (!twofa_delete_credential(acc->id, cred_id))
    {
        twofa_fail(client, "UNKNOWN_CREDENTIAL", id_str,
                   "No such credential.");
        return;
    }

    sendto_one(client, NULL,
               ":%s 2FA REMOVE SUCCESS %s :Credential removed.",
               me.name, id_str);
}

static void twofa_cmd_enable(Client *client, Account *acc)
{
    int count;
    if (acc->twofa_enabled)
    {
        twofa_fail(client, "ALREADY_ENABLED", NULL,
                   "Two-factor authentication is already enabled.");
        return;
    }
    count = twofa_count_credentials(acc->id);
    if (count <= 0)
    {
        twofa_fail(client, "NO_CREDENTIALS", NULL,
                   "Add at least one credential before enabling 2FA.");
        return;
    }
    acc->twofa_enabled = 1;
    if (!update_account_twofa_enabled(acc))
    {
        acc->twofa_enabled = 0;
        twofa_fail(client, "TEMPORARILY_UNAVAILABLE", NULL,
                   "Could not persist 2FA state.");
        return;
    }
    sendto_one(client, NULL,
               ":%s 2FA ENABLE SUCCESS :Two-factor authentication is now "
               "required for login.", me.name);
    unreal_log(ULOG_INFO, "account", "2FA_ENABLE", client,
               "$client.details enabled 2FA on account $account",
               log_data_string("account", acc->name));
}

static int twofa_verify_proof(Account *acc, const char *type,
                              const char *data)
{
    /* For TOTP, walk every TOTP credential and try to match.  Constant
     * time per credential; not constant overall (depends on count) but
     * acceptable. */
    if (!strcasecmp(type, TWOFA_TYPE_TOTP))
    {
        TwoFACredential *creds = twofa_list_credentials(acc->id);
        int matched = 0;
        for (TwoFACredential *c = creds; c; c = c->next)
        {
            if (strcasecmp(c->type, "totp"))
                continue;
            if (totp_verify(c->secret, data))
                matched = 1;
        }
        twofa_free_credential_list(creds);
        return matched;
    }
    return 0;
}

static void twofa_cmd_disable(Client *client, Account *acc, int parc, const char *parv[])
{
    const char *type, *data;

    if (!acc->twofa_enabled)
    {
        twofa_fail(client, "ALREADY_DISABLED", NULL,
                   "Two-factor authentication is not enabled.");
        return;
    }
    if (parc < 4 || BadPtr(parv[2]) || BadPtr(parv[3]))
    {
        twofa_fail(client, "INVALID_CHALLENGE_RESPONSE", NULL,
                   "Syntax: /2FA DISABLE <type> <data>");
        return;
    }
    type = parv[2];
    data = parv[3];

    if (!twofa_verify_proof(acc, type, data))
    {
        add_fake_lag(client, 3000);
        twofa_fail(client, "INVALID_CHALLENGE_RESPONSE", NULL,
                   "Second-factor proof did not verify.");
        return;
    }

    acc->twofa_enabled = 0;
    if (!update_account_twofa_enabled(acc))
    {
        acc->twofa_enabled = 1;
        twofa_fail(client, "TEMPORARILY_UNAVAILABLE", NULL,
                   "Could not persist 2FA state.");
        return;
    }
    sendto_one(client, NULL,
               ":%s 2FA DISABLE SUCCESS :Two-factor authentication has been "
               "disabled.", me.name);
    unreal_log(ULOG_INFO, "account", "2FA_DISABLE", client,
               "$client.details disabled 2FA on account $account",
               log_data_string("account", acc->name));
}

/* ----- Top-level dispatcher ----- */

CMD_FUNC(cmd_2fa)
{
    Account *acc;
    const char *sub;

    if (!IsLoggedIn(client))
    {
        twofa_fail(client, "NOT_AUTHENTICATED", NULL,
                   "You must be logged in to use 2FA commands.");
        return;
    }
    if (parc < 2 || BadPtr(parv[1]))
    {
        twofa_fail(client, "INVALID_TYPE", NULL,
                   "Syntax: /2FA <STATUS|LIST|CHALLENGE|ADD|REMOVE|ENABLE|DISABLE> ...");
        return;
    }
    acc = find_account_by_client(client);
    if (!acc)
    {
        twofa_fail(client, "NOT_AUTHENTICATED", NULL,
                   "Could not load your account.");
        return;
    }
    sub = parv[1];

    if (!strcasecmp(sub, "STATUS"))      twofa_cmd_status   (client, acc);
    else if (!strcasecmp(sub, "LIST"))   twofa_cmd_list     (client, acc);
    else if (!strcasecmp(sub, "CHALLENGE")) twofa_cmd_challenge(client, acc, parc, parv);
    else if (!strcasecmp(sub, "ADD"))    twofa_cmd_add      (client, acc, parc, parv);
    else if (!strcasecmp(sub, "REMOVE")) twofa_cmd_remove   (client, acc, parc, parv);
    else if (!strcasecmp(sub, "ENABLE")) twofa_cmd_enable   (client, acc);
    else if (!strcasecmp(sub, "DISABLE")) twofa_cmd_disable (client, acc, parc, parv);
    else
        twofa_fail(client, "INVALID_TYPE", sub, "Unknown 2FA subcommand.");

    free_account(acc);
}

/* ----- SASL step-up integration ----- */

/** Called after a first-factor SASL exchange has verified a password.
 *  If 2FA is enforced on the account, withholds 900/903 and emits
 *  AUTHENTICATE 2FA-REQUIRED instead.  Returns 1 if step-up was started
 *  (caller MUST NOT log the user in or send 900/903), 0 otherwise. */
static int twofa_maybe_start_stepup(Client *client, Account *acc)
{
    if (!acc || !acc->twofa_enabled)
        return 0;
    if (twofa_count_credentials(acc->id) <= 0)
    {
        /* 2FA flagged on but no creds left: degrade to first-factor only.
         * Avoids permanently locking the user out after manual DB edits. */
        return 0;
    }

    twofa_clear_stepup(client);
    TwoFAStepup *s = safe_alloc(sizeof(*s));
    s->active = 1;
    strlcpy(s->account, acc->name, sizeof(s->account));
    TwoFAStepupSet(client, s);

    sendto_one(client, NULL, ":%s AUTHENTICATE 2FA-REQUIRED", me.name);
    return 1;
}

/** Handle an AUTHENTICATE message in the TOTP step-up phase.
 *  Returns 1 if it consumed the message, 0 to fall through. */
static int twofa_handle_stepup_authenticate(Client *client, const char *param)
{
    TwoFAStepup *s = TwoFAStepupGet(client);
    if (!s || !s->active)
        return 0;

    /* If client says "*", abort. */
    if (!strcmp(param, "*"))
    {
        twofa_clear_stepup(client);
        DelSaslType(client);
        sendnumeric(client, ERR_SASLABORTED);
        return 1;
    }

    /* Mechanism selection: client sends "AUTHENTICATE TOTP" */
    if (!strcasecmp(param, "TOTP"))
    {
        SetSaslType(client, SASL_TYPE_TOTP_STEPUP);
        sendto_one(client, NULL, ":%s AUTHENTICATE +", me.name);
        return 1;
    }

    /* Code message: arrives when SaslType == SASL_TYPE_TOTP_STEPUP */
    if (GetSaslType(client) != SASL_TYPE_TOTP_STEPUP)
    {
        /* Unexpected -- abort. */
        twofa_clear_stepup(client);
        DelSaslType(client);
        sendnumeric(client, ERR_SASLFAIL);
        return 1;
    }

    /* Decode base64 to get the 6-digit code. */
    unsigned char buf[64];
    int n = b64_decode(param, buf, sizeof(buf) - 1);
    if (n <= 0)
    {
        twofa_clear_stepup(client);
        DelSaslType(client);
        add_fake_lag(client, 5000);
        sendnumeric(client, ERR_SASLFAIL);
        return 1;
    }
    buf[n] = 0;

    Account *acc = find_account(s->account);
    if (!acc)
    {
        twofa_clear_stepup(client);
        DelSaslType(client);
        sendnumeric(client, ERR_SASLFAIL);
        return 1;
    }

    if (!twofa_verify_proof(acc, "totp", (const char *)buf))
    {
        free_account(acc);
        twofa_clear_stepup(client);
        DelSaslType(client);
        add_fake_lag(client, 5000);
        sendnumeric(client, ERR_SASLFAIL);
        return 1;
    }

    /* Both factors satisfied: complete login. */
    strlcpy(client->user->account, acc->name, sizeof(client->user->account));
    unreal_log(ULOG_INFO, "account", "SASL_LOGIN", client,
               "SASL+2FA login for $client.details [account: $account]",
               log_data_string("account", acc->name));
    user_account_login(NULL, client);
    if (!IsDead(client))
    {
        client->local->sasl_complete = 1;
        sendnumeric(client, RPL_SASLSUCCESS);
    }
    DelSaslType(client);
    twofa_clear_stepup(client);
    free_account(acc);
    OPENSSL_cleanse(buf, sizeof(buf));
    return 1;
}

/* CAP value provider for draft/account-2fa */
static const char *twofa_capability_parameter(Client *client)
{
    return "totp,webauthn";
}

/* CAP value provider for draft/webauthn-rp-id */
static const char *webauthn_rp_id_capability_parameter(Client *client)
{
    return me.name;
}

static int twofa_capability_visible(Client *client)
{
    return 1;
}

/* ===================================================================
 * WEBAUTHN-BIO SASL mechanism + WebAuthn registration
 * (RFC: see ircv3-specifications/extensions/sasl-webauthn-bio.md and
 *  account-2fa.md)
 *
 * Limitations of this implementation:
 *  - ES256 (alg=-7, ECDSA P-256/SHA-256) only.  RS256/EdDSA not supported.
 *  - Attestation is NOT verified -- registration accepts any well-formed
 *    attestation object.  This is acceptable for many deployments but
 *    operators that require attestation must add a verifier.
 * =================================================================== */

#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/* ----- base64url (no padding) ----- */

static int b64url_decode(const char *in, unsigned char *out, size_t out_size)
{
    char buf[2048];
    size_t n = strlen(in);
    if (n + 4 >= sizeof(buf)) return -1;
    for (size_t i = 0; i < n; i++)
    {
        char c = in[i];
        if (c == '-')      buf[i] = '+';
        else if (c == '_') buf[i] = '/';
        else               buf[i] = c;
    }
    size_t pad = (4 - (n % 4)) % 4;
    for (size_t i = 0; i < pad; i++) buf[n + i] = '=';
    buf[n + pad] = 0;
    return b64_decode(buf, out, out_size);
}

static int b64url_encode(const unsigned char *in, size_t in_len,
                         char *out, size_t out_size)
{
    int n = b64_encode(in, in_len, out, out_size);
    if (n <= 0) return -1;
    while (n > 0 && out[n - 1] == '=') { out[n - 1] = 0; n--; }
    for (int i = 0; i < n; i++)
    {
        if (out[i] == '+')      out[i] = '-';
        else if (out[i] == '/') out[i] = '_';
    }
    return n;
}

/* ----- minimal CBOR decoder (RFC 8949 subset) ----- */

typedef struct {
    int major;                          /* 0..7 */
    uint64_t arg;                       /* uint value, length, or count */
    const unsigned char *bytes;         /* for major 2/3: data start */
    size_t bytes_len;                   /* for major 2/3: data length */
    const unsigned char *items;         /* for major 4/5: inner data start */
    size_t items_len;                   /* for major 4/5: inner data length */
} CborItem;

static int cbor_read_one(const unsigned char **p,
                         const unsigned char *end, CborItem *out);

static int cbor_read_head(const unsigned char **p,
                          const unsigned char *end,
                          int *major, uint64_t *arg)
{
    if (*p >= end) return 0;
    unsigned char ib = *(*p)++;
    *major = (ib >> 5) & 0x07;
    int info = ib & 0x1F;
    if (info < 24) { *arg = info; return 1; }
    int extra;
    switch (info)
    {
        case 24: extra = 1; break;
        case 25: extra = 2; break;
        case 26: extra = 4; break;
        case 27: extra = 8; break;
        default: return 0;
    }
    if (*p + extra > end) return 0;
    uint64_t v = 0;
    for (int i = 0; i < extra; i++) v = (v << 8) | *(*p)++;
    *arg = v;
    return 1;
}

static int cbor_skip(const unsigned char **p, const unsigned char *end)
{
    CborItem it;
    return cbor_read_one(p, end, &it);
}

static int cbor_read_one(const unsigned char **p, const unsigned char *end,
                         CborItem *out)
{
    int major;
    uint64_t arg;
    const unsigned char *start;

    if (!cbor_read_head(p, end, &major, &arg))
        return 0;
    out->major = major;
    out->arg = arg;
    out->bytes = NULL;
    out->bytes_len = 0;
    out->items = NULL;
    out->items_len = 0;

    switch (major)
    {
        case 0: case 1: case 7:
            return 1;
        case 2: case 3:
            if (*p + arg > end) return 0;
            out->bytes = *p;
            out->bytes_len = (size_t)arg;
            *p += arg;
            return 1;
        case 4:
        {
            start = *p;
            for (uint64_t i = 0; i < arg; i++)
                if (!cbor_skip(p, end)) return 0;
            out->items = start;
            out->items_len = (size_t)(*p - start);
            return 1;
        }
        case 5:
        {
            start = *p;
            for (uint64_t i = 0; i < arg; i++)
            {
                if (!cbor_skip(p, end)) return 0;  /* key */
                if (!cbor_skip(p, end)) return 0;  /* value */
            }
            out->items = start;
            out->items_len = (size_t)(*p - start);
            return 1;
        }
        default:
            return 0;
    }
}

/** Iterate a CBOR map and call cb(key_item, value_item) for each entry.
 *  cb returns 1 to keep iterating, 0 to stop with success. */
static int cbor_map_for_each(CborItem *map_item,
                             int (*cb)(CborItem *k, CborItem *v, void *ud),
                             void *ud)
{
    const unsigned char *p = map_item->items;
    const unsigned char *end = map_item->items + map_item->items_len;
    for (uint64_t i = 0; i < map_item->arg; i++)
    {
        CborItem k, v;
        if (!cbor_read_one(&p, end, &k)) return 0;
        if (!cbor_read_one(&p, end, &v)) return 0;
        int r = cb(&k, &v, ud);
        if (!r) return 1;  /* caller-requested early stop, still success */
    }
    return 1;
}

/* ----- COSE -> EVP_PKEY (ES256) ----- */

typedef struct {
    int kty;
    int alg;
    int crv;
    const unsigned char *x;
    size_t x_len;
    const unsigned char *y;
    size_t y_len;
} CoseEs256;

static int cose_es256_callback(CborItem *k, CborItem *v, void *ud)
{
    CoseEs256 *c = ud;
    int64_t key;
    if (k->major == 0)      key = (int64_t)k->arg;
    else if (k->major == 1) key = -(int64_t)(k->arg + 1);
    else return 1;

    if (key == 1 && v->major == 0) c->kty = (int)v->arg;
    else if (key == 3 && v->major == 1) c->alg = -(int)(v->arg + 1);
    else if (key == -1 && v->major == 0) c->crv = (int)v->arg;
    else if (key == -2 && v->major == 2) { c->x = v->bytes; c->x_len = v->bytes_len; }
    else if (key == -3 && v->major == 2) { c->y = v->bytes; c->y_len = v->bytes_len; }
    return 1;
}

static EVP_PKEY *cose_to_evp_pkey_es256(const unsigned char *cose,
                                        size_t cose_len)
{
    const unsigned char *p = cose;
    CborItem map;
    if (!cbor_read_one(&p, cose + cose_len, &map) || map.major != 5)
        return NULL;

    CoseEs256 c = {0};
    cbor_map_for_each(&map, cose_es256_callback, &c);

    if (c.kty != 2 || c.alg != -7 || c.crv != 1 ||
        c.x_len != 32 || c.y_len != 32)
        return NULL;

    EC_KEY *ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!ec) return NULL;
    BIGNUM *bx = BN_bin2bn(c.x, 32, NULL);
    BIGNUM *by = BN_bin2bn(c.y, 32, NULL);
    EVP_PKEY *pkey = NULL;
    if (bx && by &&
        EC_KEY_set_public_key_affine_coordinates(ec, bx, by) == 1)
    {
        pkey = EVP_PKEY_new();
        if (pkey && !EVP_PKEY_assign_EC_KEY(pkey, ec))
        {
            EVP_PKEY_free(pkey);
            pkey = NULL;
            EC_KEY_free(ec);
        }
        else if (pkey)
        {
            ec = NULL; /* ownership transferred */
        }
    }
    if (ec) EC_KEY_free(ec);
    BN_free(bx);
    BN_free(by);
    return pkey;
}

/* ----- authenticatorData parser ----- */

typedef struct {
    unsigned char rp_id_hash[32];
    unsigned char flags;
    uint32_t sign_count;
    /* Attested credential data (only when AT bit set in flags) */
    int has_attested;
    unsigned char aaguid[16];
    const unsigned char *cred_id;
    size_t cred_id_len;
    const unsigned char *cose_pubkey;
    size_t cose_pubkey_len;
} WebAuthnAuthData;

#define AUTHFLAG_UP   0x01
#define AUTHFLAG_UV   0x04
#define AUTHFLAG_AT   0x40
#define AUTHFLAG_ED   0x80

static int parse_authenticator_data(const unsigned char *data, size_t len,
                                    WebAuthnAuthData *out)
{
    if (len < 37) return 0;
    memset(out, 0, sizeof(*out));
    memcpy(out->rp_id_hash, data, 32);
    out->flags = data[32];
    out->sign_count = ((uint32_t)data[33] << 24) |
                      ((uint32_t)data[34] << 16) |
                      ((uint32_t)data[35] << 8)  |
                      ((uint32_t)data[36]);
    if (!(out->flags & AUTHFLAG_AT))
        return 1;

    if (len < 37 + 16 + 2) return 0;
    memcpy(out->aaguid, data + 37, 16);
    size_t cred_id_len = ((size_t)data[53] << 8) | data[54];
    if (cred_id_len == 0 || cred_id_len > 1023) return 0;
    if (len < 37 + 16 + 2 + cred_id_len) return 0;
    out->cred_id = data + 55;
    out->cred_id_len = cred_id_len;

    /* COSE key: parse one CBOR value to determine its length. */
    const unsigned char *p = data + 55 + cred_id_len;
    const unsigned char *end = data + len;
    CborItem item;
    const unsigned char *ks = p;
    if (!cbor_read_one(&p, end, &item))
        return 0;
    out->cose_pubkey = ks;
    out->cose_pubkey_len = (size_t)(p - ks);
    out->has_attested = 1;
    return 1;
}

/* ----- ECDSA verify ----- */

static int webauthn_ecdsa_verify(EVP_PKEY *pkey,
                                 const unsigned char *msg, size_t msg_len,
                                 const unsigned char *sig, size_t sig_len)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    int ok = 0;
    if (ctx &&
        EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pkey) == 1 &&
        EVP_DigestVerifyUpdate(ctx, msg, msg_len) == 1 &&
        EVP_DigestVerifyFinal(ctx, sig, sig_len) == 1)
    {
        ok = 1;
    }
    if (ctx) EVP_MD_CTX_free(ctx);
    return ok;
}

/* ----- WebAuthn credential record ----- */

typedef struct {
    char *credential_id_b64u;
    char *x_b64u;
    char *y_b64u;
    char *user_handle_b64u;
    long counter;
    long row_id;            /* DB rowid in account_2fa_credentials */
    long account_id;
    char *account_name;     /* loaded on demand */
} WebAuthnCred;

static void webauthn_cred_free(WebAuthnCred *c)
{
    if (!c) return;
    safe_free(c->credential_id_b64u);
    safe_free(c->x_b64u);
    safe_free(c->y_b64u);
    safe_free(c->user_handle_b64u);
    safe_free(c->account_name);
    safe_free(c);
}

/* The "secret" column of account_2fa_credentials holds a JSON blob:
 *   {"cid":"<b64u>","x":"<b64u>","y":"<b64u>","cnt":N,"uh":"<b64u>"}
 */
static char *webauthn_pack(const char *credential_id_b64u,
                           const char *x_b64u, const char *y_b64u,
                           const char *user_handle_b64u,
                           long counter)
{
    json_t *j = json_object();
    json_object_set_new(j, "cid", json_string(credential_id_b64u));
    json_object_set_new(j, "x",   json_string(x_b64u));
    json_object_set_new(j, "y",   json_string(y_b64u));
    json_object_set_new(j, "uh",  json_string(user_handle_b64u ? user_handle_b64u : ""));
    json_object_set_new(j, "cnt", json_integer(counter));
    char *s = json_dumps(j, JSON_COMPACT);
    json_decref(j);
    return s;
}

static int webauthn_unpack(const char *blob, WebAuthnCred *out)
{
    json_error_t err;
    json_t *j = json_loads(blob, 0, &err);
    if (!j) return 0;
    const char *cid = json_string_value(json_object_get(j, "cid"));
    const char *x   = json_string_value(json_object_get(j, "x"));
    const char *y   = json_string_value(json_object_get(j, "y"));
    const char *uh  = json_string_value(json_object_get(j, "uh"));
    json_t *jcnt    = json_object_get(j, "cnt");

    if (!cid || !x || !y) { json_decref(j); return 0; }

    safe_strdup(out->credential_id_b64u, cid);
    safe_strdup(out->x_b64u, x);
    safe_strdup(out->y_b64u, y);
    if (uh && *uh)
        safe_strdup(out->user_handle_b64u, uh);
    out->counter = (jcnt && json_is_integer(jcnt))
                   ? (long)json_integer_value(jcnt) : 0;

    json_decref(j);
    return 1;
}

/** Walk all WebAuthn credentials, optionally filtered by account_id (<=0
 *  for "all").  Caller frees with webauthn_free_list. */
typedef struct WebAuthnRow_ {
    long row_id;
    long account_id;
    char *secret_blob;
    struct WebAuthnRow_ *next;
} WebAuthnRow;

static void webauthn_free_rows(WebAuthnRow *head)
{
    while (head)
    {
        WebAuthnRow *n = head->next;
        safe_free(head->secret_blob);
        safe_free(head);
        head = n;
    }
}

static WebAuthnRow *webauthn_load_rows(long account_id_filter)
{
    sqlite3_stmt *stmt;
    const char *sql_a =
        "SELECT id, account_id, secret FROM account_2fa_credentials"
        " WHERE type = 'webauthn' AND account_id = ? ORDER BY id ASC";
    const char *sql_b =
        "SELECT id, account_id, secret FROM account_2fa_credentials"
        " WHERE type = 'webauthn' ORDER BY id ASC";

    if (!obsidian_db) return NULL;
    if (sqlite3_prepare_v2(obsidian_db,
                           account_id_filter > 0 ? sql_a : sql_b,
                           -1, &stmt, NULL) != SQLITE_OK)
        return NULL;
    if (account_id_filter > 0)
        sqlite3_bind_int(stmt, 1, (int)account_id_filter);

    WebAuthnRow *head = NULL, *tail = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        WebAuthnRow *r = safe_alloc(sizeof(*r));
        r->row_id     = sqlite3_column_int(stmt, 0);
        r->account_id = sqlite3_column_int(stmt, 1);
        r->secret_blob = strdup((const char *)sqlite3_column_text(stmt, 2));
        r->next = NULL;
        if (tail) tail->next = r; else head = r;
        tail = r;
    }
    sqlite3_finalize(stmt);
    return head;
}

static int webauthn_update_secret(long row_id, const char *new_secret)
{
    const char *sql =
        "UPDATE account_2fa_credentials SET secret = ? WHERE id = ?";
    sqlite3_stmt *stmt;
    int rc;
    if (!obsidian_db) return 0;
    if (sqlite3_prepare_v2(obsidian_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(stmt, 1, new_secret, -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 2, (int)row_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

static char *account_name_by_id(long account_id)
{
    sqlite3_stmt *stmt;
    char *out = NULL;
    if (!obsidian_db) return NULL;
    if (sqlite3_prepare_v2(obsidian_db,
            "SELECT name FROM accounts WHERE id = ? LIMIT 1",
            -1, &stmt, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_int(stmt, 1, (int)account_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *t = sqlite3_column_text(stmt, 0);
        if (t) out = strdup((const char *)t);
    }
    sqlite3_finalize(stmt);
    return out;
}

/* ----- WebAuthn registration parse (attestationObject -> cred fields) ----- */

typedef struct AttestationFindCtx_ {
    const unsigned char *auth_data;
    size_t auth_data_len;
} AttestationFindCtx;

static int attestation_cb(CborItem *k, CborItem *v, void *ud)
{
    AttestationFindCtx *ctx = ud;
    if (k->major == 3 && k->bytes_len == 8 &&
        !memcmp(k->bytes, "authData", 8) && v->major == 2)
    {
        ctx->auth_data = v->bytes;
        ctx->auth_data_len = v->bytes_len;
    }
    return 1;
}

typedef struct {
    char *credential_id_b64u;
    char *x_b64u;
    char *y_b64u;
    long counter;
} WebAuthnRegistration;

static void webauthn_registration_free(WebAuthnRegistration *r)
{
    if (!r) return;
    safe_free(r->credential_id_b64u);
    safe_free(r->x_b64u);
    safe_free(r->y_b64u);
}

/** Parse the inner attestationObject CBOR + verify rpIdHash + UV + UP.
 *  On success populates *out with heap-owned base64url strings. */
static int parse_webauthn_registration(const unsigned char *attestation,
                                       size_t attestation_len,
                                       const char *expected_rp_id,
                                       WebAuthnRegistration *out)
{
    const unsigned char *p = attestation;
    CborItem map;
    if (!cbor_read_one(&p, attestation + attestation_len, &map) ||
        map.major != 5)
        return 0;

    AttestationFindCtx ctx = {0};
    cbor_map_for_each(&map, attestation_cb, &ctx);
    if (!ctx.auth_data)
        return 0;

    WebAuthnAuthData ad;
    if (!parse_authenticator_data(ctx.auth_data, ctx.auth_data_len, &ad))
        return 0;
    if (!(ad.flags & AUTHFLAG_AT))
        return 0;
    if (!(ad.flags & AUTHFLAG_UP))
        return 0;
    if (!(ad.flags & AUTHFLAG_UV))
        return 0;

    unsigned char rp_id_hash[32];
    SHA256((const unsigned char *)expected_rp_id, strlen(expected_rp_id),
           rp_id_hash);
    if (CRYPTO_memcmp(rp_id_hash, ad.rp_id_hash, 32) != 0)
        return 0;

    /* COSE key: must be ES256.  Validate by attempting to build EVP_PKEY. */
    EVP_PKEY *pkey = cose_to_evp_pkey_es256(ad.cose_pubkey, ad.cose_pubkey_len);
    if (!pkey)
        return 0;
    EVP_PKEY_free(pkey);

    /* Re-parse to extract x/y. */
    CoseEs256 c = {0};
    {
        const unsigned char *cp = ad.cose_pubkey;
        CborItem cosem;
        if (!cbor_read_one(&cp, ad.cose_pubkey + ad.cose_pubkey_len, &cosem) ||
            cosem.major != 5)
            return 0;
        cbor_map_for_each(&cosem, cose_es256_callback, &c);
    }
    if (!c.x || !c.y || c.x_len != 32 || c.y_len != 32)
        return 0;

    char buf[256];
    if (b64url_encode(ad.cred_id, ad.cred_id_len, buf, sizeof(buf)) <= 0)
        return 0;
    out->credential_id_b64u = strdup(buf);
    if (b64url_encode(c.x, 32, buf, sizeof(buf)) <= 0 ||
        !(out->x_b64u = strdup(buf)))
        return 0;
    if (b64url_encode(c.y, 32, buf, sizeof(buf)) <= 0 ||
        !(out->y_b64u = strdup(buf)))
        return 0;
    out->counter = (long)ad.sign_count;
    return 1;
}

/* ----- Origin acceptance ----- */

static int webauthn_origin_acceptable(const char *origin, const char *rp_id)
{
    char buf[256];
    if (!origin || !rp_id) return 0;

    snprintf(buf, sizeof(buf), "ircs://%s", rp_id);
    if (!strcmp(origin, buf)) return 1;
    snprintf(buf, sizeof(buf), "irc://%s", rp_id);
    if (!strcmp(origin, buf)) return 1;
    snprintf(buf, sizeof(buf), "https://%s", rp_id);
    if (!strcmp(origin, buf)) return 1;
    return 0;
}

/* ----- Top-level WebAuthn assertion verify -----
 *  Inputs are JSON-decoded base64url byte buffers + the expected challenge
 *  bytes the server issued earlier in this SASL exchange.
 *  On success, returns the matched account_id.  On failure returns 0 and
 *  writes a WebAuthn error code into *err_code (caller-owned static string,
 *  do not free). */
static long webauthn_verify_assertion(
    const unsigned char *credential_id, size_t credential_id_len,
    const unsigned char *authenticator_data, size_t authenticator_data_len,
    const unsigned char *client_data_json, size_t client_data_json_len,
    const unsigned char *signature, size_t signature_len,
    const unsigned char *user_handle, size_t user_handle_len,
    const unsigned char *expected_challenge, size_t expected_challenge_len,
    const char *username_hint,         /* NULL for resident-key flow */
    const char **err_code)
{
    *err_code = "WEBAUTHN_MALFORMED";

    /* 1. Parse clientDataJSON */
    char *cd = safe_alloc(client_data_json_len + 1);
    memcpy(cd, client_data_json, client_data_json_len);
    cd[client_data_json_len] = 0;
    json_error_t je;
    json_t *cj = json_loads(cd, 0, &je);
    safe_free(cd);
    if (!cj) return 0;

    const char *type   = json_string_value(json_object_get(cj, "type"));
    const char *chal   = json_string_value(json_object_get(cj, "challenge"));
    const char *origin = json_string_value(json_object_get(cj, "origin"));
    if (!type || strcmp(type, "webauthn.get") || !chal || !origin)
    {
        json_decref(cj);
        *err_code = "WEBAUTHN_MALFORMED";
        return 0;
    }

    /* 2. Verify challenge in clientDataJSON matches the server-issued one */
    {
        unsigned char buf[128];
        int n = b64url_decode(chal, buf, sizeof(buf));
        if (n != (int)expected_challenge_len ||
            CRYPTO_memcmp(buf, expected_challenge, expected_challenge_len) != 0)
        {
            json_decref(cj);
            *err_code = "WEBAUTHN_INVALID_CHALLENGE";
            return 0;
        }
    }

    /* 3. Verify origin */
    if (!webauthn_origin_acceptable(origin, me.name))
    {
        json_decref(cj);
        *err_code = "WEBAUTHN_INVALID_ORIGIN";
        return 0;
    }
    json_decref(cj);

    /* 4. Parse authenticatorData */
    WebAuthnAuthData ad;
    if (!parse_authenticator_data(authenticator_data,
                                  authenticator_data_len, &ad))
    {
        *err_code = "WEBAUTHN_MALFORMED";
        return 0;
    }
    if (!(ad.flags & AUTHFLAG_UP))
    {
        *err_code = "WEBAUTHN_INVALID_SIGNATURE";
        return 0;
    }
    if (!(ad.flags & AUTHFLAG_UV))
    {
        *err_code = "WEBAUTHN_UV_REQUIRED";
        return 0;
    }
    {
        unsigned char rp_hash[32];
        SHA256((const unsigned char *)me.name, strlen(me.name), rp_hash);
        if (CRYPTO_memcmp(rp_hash, ad.rp_id_hash, 32) != 0)
        {
            *err_code = "WEBAUTHN_INVALID_ORIGIN";
            return 0;
        }
    }

    /* 5. Identify credential.  Walk all WebAuthn rows for the account
     * (or globally for resident-key flow), match credentialId. */
    long matched_account_id = 0;
    long matched_row_id = 0;
    long stored_counter = 0;
    char *stored_uh = NULL;
    char *stored_x = NULL, *stored_y = NULL;

    long account_filter = 0;
    if (username_hint && *username_hint)
    {
        Account *acct = find_account(username_hint);
        if (acct)
        {
            account_filter = acct->id;
            free_account(acct);
        }
        if (account_filter <= 0)
        {
            *err_code = "WEBAUTHN_UNKNOWN_CREDENTIAL";
            return 0;
        }
    }

    char cid_b64u[512];
    if (b64url_encode(credential_id, credential_id_len,
                      cid_b64u, sizeof(cid_b64u)) <= 0)
    {
        *err_code = "WEBAUTHN_MALFORMED";
        return 0;
    }

    WebAuthnRow *rows = webauthn_load_rows(account_filter);
    for (WebAuthnRow *r = rows; r; r = r->next)
    {
        WebAuthnCred c = {0};
        if (!webauthn_unpack(r->secret_blob, &c))
            continue;
        if (!strcmp(c.credential_id_b64u, cid_b64u))
        {
            /* Resident-key flow: also match userHandle if supplied. */
            if (!username_hint && user_handle_len > 0 && c.user_handle_b64u)
            {
                char uh_b64u[256];
                if (b64url_encode(user_handle, user_handle_len,
                                  uh_b64u, sizeof(uh_b64u)) <= 0 ||
                    strcmp(uh_b64u, c.user_handle_b64u))
                {
                    safe_free(c.credential_id_b64u);
                    safe_free(c.x_b64u);
                    safe_free(c.y_b64u);
                    safe_free(c.user_handle_b64u);
                    continue;
                }
            }
            matched_account_id = r->account_id;
            matched_row_id     = r->row_id;
            stored_counter     = c.counter;
            stored_x = c.x_b64u; c.x_b64u = NULL;
            stored_y = c.y_b64u; c.y_b64u = NULL;
            stored_uh = c.user_handle_b64u; c.user_handle_b64u = NULL;
            safe_free(c.credential_id_b64u);
            break;
        }
        safe_free(c.credential_id_b64u);
        safe_free(c.x_b64u);
        safe_free(c.y_b64u);
        safe_free(c.user_handle_b64u);
    }
    webauthn_free_rows(rows);

    if (!matched_account_id)
    {
        *err_code = "WEBAUTHN_UNKNOWN_CREDENTIAL";
        safe_free(stored_x); safe_free(stored_y); safe_free(stored_uh);
        return 0;
    }

    /* 6. Build pubkey from x,y and verify signature. */
    unsigned char x_raw[64], y_raw[64];
    int xlen = b64url_decode(stored_x, x_raw, sizeof(x_raw));
    int ylen = b64url_decode(stored_y, y_raw, sizeof(y_raw));
    safe_free(stored_x); safe_free(stored_y); safe_free(stored_uh);
    if (xlen != 32 || ylen != 32)
    {
        *err_code = "WEBAUTHN_INVALID_SIGNATURE";
        return 0;
    }

    EC_KEY *ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    BIGNUM *bx = BN_bin2bn(x_raw, 32, NULL);
    BIGNUM *by = BN_bin2bn(y_raw, 32, NULL);
    EVP_PKEY *pkey = NULL;
    if (ec && bx && by &&
        EC_KEY_set_public_key_affine_coordinates(ec, bx, by) == 1)
    {
        pkey = EVP_PKEY_new();
        if (pkey) EVP_PKEY_assign_EC_KEY(pkey, ec);
        else      EC_KEY_free(ec);
    }
    else if (ec) EC_KEY_free(ec);
    BN_free(bx); BN_free(by);
    if (!pkey)
    {
        *err_code = "WEBAUTHN_INVALID_SIGNATURE";
        return 0;
    }

    /* 7. signed = authData || sha256(clientDataJSON) */
    unsigned char client_data_hash[32];
    SHA256(client_data_json, client_data_json_len, client_data_hash);
    size_t signed_len = authenticator_data_len + 32;
    unsigned char *signed_buf = safe_alloc(signed_len);
    memcpy(signed_buf, authenticator_data, authenticator_data_len);
    memcpy(signed_buf + authenticator_data_len, client_data_hash, 32);

    int sig_ok = webauthn_ecdsa_verify(pkey, signed_buf, signed_len,
                                       signature, signature_len);
    safe_free(signed_buf);
    EVP_PKEY_free(pkey);
    if (!sig_ok)
    {
        *err_code = "WEBAUTHN_INVALID_SIGNATURE";
        return 0;
    }

    /* 8. Counter check. */
    if (stored_counter > 0 && ad.sign_count != 0 &&
        ad.sign_count <= (uint32_t)stored_counter)
    {
        *err_code = "WEBAUTHN_COUNTER_REPLAY";
        return 0;
    }

    /* 9. Persist updated counter (only if authenticator supplies one). */
    if (ad.sign_count > 0)
    {
        WebAuthnRow *rows2 = webauthn_load_rows(matched_account_id);
        for (WebAuthnRow *r = rows2; r; r = r->next)
        {
            if (r->row_id != matched_row_id) continue;
            WebAuthnCred c = {0};
            if (webauthn_unpack(r->secret_blob, &c))
            {
                c.counter = (long)ad.sign_count;
                char *new_blob = webauthn_pack(c.credential_id_b64u,
                                               c.x_b64u, c.y_b64u,
                                               c.user_handle_b64u,
                                               c.counter);
                if (new_blob)
                {
                    webauthn_update_secret(r->row_id, new_blob);
                    free(new_blob);
                }
                safe_free(c.credential_id_b64u);
                safe_free(c.x_b64u);
                safe_free(c.y_b64u);
                safe_free(c.user_handle_b64u);
            }
            break;
        }
        webauthn_free_rows(rows2);
    }

    *err_code = NULL;
    return matched_account_id;
}

/* ----- WEBAUTHN-BIO SASL state -----
 * (struct + ModData macros forward-declared near top of file) */

static void webauthn_sasl_md_free(ModData *m)
{
    if (m->ptr) { safe_free(m->ptr); m->ptr = NULL; }
}
static void webauthn_sasl_clear(Client *c)
{
    WebAuthnSaslState *s = WebAuthnSaslGet(c);
    if (s) { safe_free(s); WebAuthnSaslSet(c, NULL); }
}

/* Send a FAIL AUTHENTICATE WEBAUTHN_* before 904 */
static void webauthn_sasl_fail_with_code(Client *client, const char *code)
{
    if (code)
        sendto_one(client, NULL,
                   ":%s FAIL AUTHENTICATE %s :WebAuthn authentication failed",
                   me.name, code);
    client->local->sasl_sent_time = 0;
    add_fake_lag(client, 5000);
    sendnumeric(client, ERR_SASLFAIL);
    DelSaslType(client);
    webauthn_sasl_clear(client);
}

/* ----- WEBAUTHN-BIO SASL handlers ----- */

static void webauthn_sasl_send_challenge(Client *client, WebAuthnSaslState *st)
{
    json_t *j = json_object();
    json_object_set_new(j, "version", json_integer(1));

    char chal_b64u[64];
    b64url_encode(st->challenge, sizeof(st->challenge),
                  chal_b64u, sizeof(chal_b64u));
    json_object_set_new(j, "challenge", json_string(chal_b64u));
    json_object_set_new(j, "rpId", json_string(me.name));
    json_object_set_new(j, "timeout", json_integer(60000));
    json_object_set_new(j, "userVerification", json_string("required"));

    if (*st->username_hint)
    {
        Account *acct = find_account(st->username_hint);
        if (acct)
        {
            WebAuthnRow *rows = webauthn_load_rows(acct->id);
            if (rows)
            {
                json_t *arr = json_array();
                for (WebAuthnRow *r = rows; r; r = r->next)
                {
                    WebAuthnCred c = {0};
                    if (webauthn_unpack(r->secret_blob, &c))
                    {
                        json_t *o = json_object();
                        json_object_set_new(o, "type",
                                            json_string("public-key"));
                        json_object_set_new(o, "id",
                                            json_string(c.credential_id_b64u));
                        json_array_append_new(arr, o);
                        safe_free(c.credential_id_b64u);
                        safe_free(c.x_b64u);
                        safe_free(c.y_b64u);
                        safe_free(c.user_handle_b64u);
                    }
                }
                json_object_set_new(j, "allowCredentials", arr);
            }
            webauthn_free_rows(rows);
            free_account(acct);
        }
    }

    char *jstr = json_dumps(j, JSON_COMPACT);
    json_decref(j);
    if (!jstr)
    {
        webauthn_sasl_fail_with_code(client, "WEBAUTHN_MALFORMED");
        return;
    }
    size_t jlen = strlen(jstr);
    size_t b64size = ((jlen + 2) / 3) * 4 + 1;
    char *b64 = safe_alloc(b64size);
    int n = b64_encode((const unsigned char *)jstr, jlen, b64, b64size);
    free(jstr);
    if (n <= 0 || n > 400)
    {
        safe_free(b64);
        webauthn_sasl_fail_with_code(client, "WEBAUTHN_MALFORMED");
        return;
    }
    sendto_one(client, NULL, ":%s AUTHENTICATE %s", me.name, b64);
    safe_free(b64);
    st->step = 1;
}

static void webauthn_sasl_handle_hello(Client *client, const char *param)
{
    WebAuthnSaslState *st = WebAuthnSaslGet(client);
    if (!st || st->step != 0)
    {
        webauthn_sasl_fail_with_code(client, "WEBAUTHN_MALFORMED");
        return;
    }

    if (!strcmp(param, "+"))
    {
        /* Discoverable-credential flow: no allowCredentials. */
        st->username_hint[0] = 0;
        webauthn_sasl_send_challenge(client, st);
        return;
    }

    /* Decode hello JSON */
    unsigned char buf[1024];
    int n = b64_decode(param, buf, sizeof(buf) - 1);
    if (n <= 0)
    {
        webauthn_sasl_fail_with_code(client, "WEBAUTHN_MALFORMED");
        return;
    }
    buf[n] = 0;
    json_error_t err;
    json_t *j = json_loads((const char *)buf, 0, &err);
    if (!j)
    {
        webauthn_sasl_fail_with_code(client, "WEBAUTHN_MALFORMED");
        return;
    }
    const char *uname = json_string_value(json_object_get(j, "username"));
    if (uname && *uname)
        strlcpy(st->username_hint, uname, sizeof(st->username_hint));
    else
        st->username_hint[0] = 0;
    json_decref(j);

    webauthn_sasl_send_challenge(client, st);
}

static void webauthn_sasl_handle_assertion(Client *client, const char *param)
{
    WebAuthnSaslState *st = WebAuthnSaslGet(client);
    if (!st || st->step != 1)
    {
        webauthn_sasl_fail_with_code(client, "WEBAUTHN_MALFORMED");
        return;
    }

    unsigned char buf[2048];
    int n = b64_decode(param, buf, sizeof(buf) - 1);
    if (n <= 0)
    {
        webauthn_sasl_fail_with_code(client, "WEBAUTHN_MALFORMED");
        return;
    }
    buf[n] = 0;

    json_error_t err;
    json_t *j = json_loads((const char *)buf, 0, &err);
    if (!j)
    {
        webauthn_sasl_fail_with_code(client, "WEBAUTHN_MALFORMED");
        return;
    }

    const char *cid_b = json_string_value(json_object_get(j, "credentialId"));
    const char *ad_b  = json_string_value(json_object_get(j, "authenticatorData"));
    const char *cd_b  = json_string_value(json_object_get(j, "clientDataJSON"));
    const char *sig_b = json_string_value(json_object_get(j, "signature"));
    const char *uh_b  = json_string_value(json_object_get(j, "userHandle"));
    if (!cid_b || !ad_b || !cd_b || !sig_b)
    {
        json_decref(j);
        webauthn_sasl_fail_with_code(client, "WEBAUTHN_MALFORMED");
        return;
    }

    unsigned char cid_buf[512], ad_buf[1024], cd_buf[1024];
    unsigned char sig_buf[256], uh_buf[64];
    int cid_len = b64url_decode(cid_b, cid_buf, sizeof(cid_buf));
    int ad_len  = b64url_decode(ad_b,  ad_buf,  sizeof(ad_buf));
    int cd_len  = b64url_decode(cd_b,  cd_buf,  sizeof(cd_buf));
    int sig_len = b64url_decode(sig_b, sig_buf, sizeof(sig_buf));
    int uh_len  = uh_b ? b64url_decode(uh_b, uh_buf, sizeof(uh_buf)) : 0;
    json_decref(j);
    if (cid_len <= 0 || ad_len <= 0 || cd_len <= 0 || sig_len <= 0)
    {
        webauthn_sasl_fail_with_code(client, "WEBAUTHN_MALFORMED");
        return;
    }

    const char *err_code = NULL;
    long account_id = webauthn_verify_assertion(
        cid_buf, cid_len, ad_buf, ad_len, cd_buf, cd_len,
        sig_buf, sig_len, uh_b ? uh_buf : NULL, uh_len > 0 ? (size_t)uh_len : 0,
        st->challenge, sizeof(st->challenge),
        *st->username_hint ? st->username_hint : NULL,
        &err_code);

    if (!account_id)
    {
        webauthn_sasl_fail_with_code(client, err_code);
        return;
    }

    char *aname = account_name_by_id(account_id);
    if (!aname)
    {
        webauthn_sasl_fail_with_code(client, "WEBAUTHN_UNKNOWN_CREDENTIAL");
        return;
    }

    strlcpy(client->user->account, aname, sizeof(client->user->account));
    unreal_log(ULOG_INFO, "account", "SASL_LOGIN", client,
               "SASL/WEBAUTHN-BIO login for $client.details [account: $account]",
               log_data_string("account", aname));
    free(aname);
    user_account_login(NULL, client);
    if (!IsDead(client))
    {
        client->local->sasl_complete = 1;
        sendnumeric(client, RPL_SASLSUCCESS);
    }
    DelSaslType(client);
    webauthn_sasl_clear(client);
}

#pragma GCC diagnostic pop

/* ----- 2FA ADD webauthn integration -----
 * (called from twofa_cmd_add when type == "webauthn") */
static int webauthn_2fa_handle_add(Client *client, Account *acc,
                                   const char *name, const char *data_b64)
{
    /* data is base64-encoded JSON: clientDataJSON + attestationObject (both
     * base64url within the JSON). */
    unsigned char buf[2048];
    int n = b64_decode(data_b64, buf, sizeof(buf) - 1);
    if (n <= 0)
        return 0;
    buf[n] = 0;
    json_error_t je;
    json_t *j = json_loads((const char *)buf, 0, &je);
    if (!j) return 0;

    const char *cd_b = json_string_value(json_object_get(j, "clientDataJSON"));
    const char *ao_b = json_string_value(json_object_get(j, "attestationObject"));
    if (!cd_b || !ao_b) { json_decref(j); return 0; }

    unsigned char cd[2048], ao[2048];
    int cd_len = b64url_decode(cd_b, cd, sizeof(cd) - 1);
    int ao_len = b64url_decode(ao_b, ao, sizeof(ao));
    json_decref(j);
    if (cd_len <= 0 || ao_len <= 0) return 0;
    cd[cd_len] = 0;

    /* Verify clientDataJSON.type / challenge / origin against the pending
     * enrolment challenge (stored in TwoFAEnroll for this client). */
    TwoFAEnroll *e = TwoFAEnrollGet(client);
    if (!e || strcmp(e->type, "webauthn") || time(NULL) > e->expires_at ||
        !e->secret_b32)
        return 0;

    json_t *cdj = json_loads((const char *)cd, 0, &je);
    if (!cdj) return 0;
    const char *type   = json_string_value(json_object_get(cdj, "type"));
    const char *chal   = json_string_value(json_object_get(cdj, "challenge"));
    const char *origin = json_string_value(json_object_get(cdj, "origin"));
    if (!type || strcmp(type, "webauthn.create") || !chal || !origin)
    {
        json_decref(cdj);
        return 0;
    }
    /* The enrolment "secret" carries the issued challenge as base64url. */
    if (strcmp(chal, e->secret_b32) ||
        !webauthn_origin_acceptable(origin, me.name))
    {
        json_decref(cdj);
        return 0;
    }
    json_decref(cdj);

    WebAuthnRegistration reg = {0};
    if (!parse_webauthn_registration(ao, ao_len, me.name, &reg))
        return 0;

    /* User handle: stable per-account.  Use base64url(account_id-as-bigint
     * little-endian 8 bytes). */
    unsigned char uh_raw[8];
    long aid = acc->id;
    for (int i = 0; i < 8; i++) { uh_raw[i] = aid & 0xFF; aid >>= 8; }
    char uh_b64u[24];
    b64url_encode(uh_raw, sizeof(uh_raw), uh_b64u, sizeof(uh_b64u));

    char *blob = webauthn_pack(reg.credential_id_b64u, reg.x_b64u, reg.y_b64u,
                               uh_b64u, reg.counter);
    webauthn_registration_free(&reg);
    if (!blob) return 0;

    long new_id = 0;
    int ok = twofa_insert_credential(acc->id, "webauthn", name, blob, &new_id);
    free(blob);
    if (!ok) return 0;

    char id_buf[TWOFA_ID_MAX + 1];
    format_cred_id(id_buf, sizeof(id_buf), new_id);
    sendto_one(client, NULL,
               ":%s 2FA ADD SUCCESS webauthn %s :WebAuthn credential '%s' registered.",
               me.name, id_buf, name);
    twofa_clear_enroll(client);
    return 1;
}

/* Called from twofa_cmd_challenge when type == "webauthn". */
static void webauthn_2fa_handle_challenge(Client *client, Account *acc)
{
    unsigned char chal[32];
    if (RAND_bytes(chal, sizeof(chal)) != 1)
    {
        twofa_fail(client, "TEMPORARILY_UNAVAILABLE", NULL,
                   "Could not generate challenge.");
        return;
    }
    char chal_b64u[64];
    b64url_encode(chal, sizeof(chal), chal_b64u, sizeof(chal_b64u));

    /* User handle (base64url of 8-byte account_id LE). */
    unsigned char uh_raw[8];
    long aid = acc->id;
    for (int i = 0; i < 8; i++) { uh_raw[i] = aid & 0xFF; aid >>= 8; }
    char uh_b64u[24];
    b64url_encode(uh_raw, sizeof(uh_raw), uh_b64u, sizeof(uh_b64u));

    json_t *j = json_object();
    json_object_set_new(j, "challenge",         json_string(chal_b64u));
    json_object_set_new(j, "rpId",              json_string(me.name));
    json_object_set_new(j, "rpName",            json_string(me.name));
    json_object_set_new(j, "userId",            json_string(uh_b64u));
    json_object_set_new(j, "userName",          json_string(acc->name));
    json_object_set_new(j, "userVerification",  json_string("required"));
    json_t *params = json_array();
    json_t *p1 = json_object();
    json_object_set_new(p1, "type", json_string("public-key"));
    json_object_set_new(p1, "alg",  json_integer(-7));
    json_array_append_new(params, p1);
    json_object_set_new(j, "pubKeyCredParams", params);

    char *jstr = json_dumps(j, JSON_COMPACT);
    json_decref(j);
    if (!jstr)
    {
        twofa_fail(client, "TEMPORARILY_UNAVAILABLE", NULL,
                   "Could not encode challenge.");
        return;
    }

    size_t jlen = strlen(jstr);
    size_t b64size = ((jlen + 2) / 3) * 4 + 1;
    char *b64 = safe_alloc(b64size);
    int b64n = b64_encode((const unsigned char *)jstr, jlen, b64, b64size);
    free(jstr);
    if (b64n <= 0)
    {
        safe_free(b64);
        twofa_fail(client, "TEMPORARILY_UNAVAILABLE", NULL,
                   "Could not encode challenge.");
        return;
    }

    twofa_clear_enroll(client);
    TwoFAEnroll *e = safe_alloc(sizeof(*e));
    safe_strdup(e->type, "webauthn");
    safe_strdup(e->secret_b32, chal_b64u); /* stash issued challenge */
    e->expires_at = time(NULL) + TWOFA_CHALLENGE_LIFETIME;
    TwoFAEnrollSet(client, e);

    sendto_one(client, NULL,
               ":%s NOTE 2FA REGISTRATION_CHALLENGE webauthn %s :"
               "Perform a WebAuthn credential creation gesture, then run "
               "/2FA ADD webauthn <name> <data>", me.name, b64);
    safe_free(b64);
}

static const char *saslmechs(Client *client)
{
    return "PLAIN,SCRAM-SHA-256,TOTP,DRAFT-WEBAUTHN-BIO,ANONYMOUS";
}

/* ===================================================================
 * CMD: REGISTER <name> <email> <password>
 * =================================================================== */
CMD_FUNC(register_account)
{
    const char *name, *email, *password;
    const char *password_hash;
    Account *acc;

    if (!obsidian_db && obsidian_open_database(OBSIDIAN_DB) != SQLITE_OK)
    {
        sendto_one(client, NULL,
                   ":%s FAIL REGISTER SERVER_BUG :Database unavailable.", me.name);
        return;
    }

    if (parc < 4)
    {
        sendto_one(client, NULL,
                   ":%s NOTE REGISTER INVALID_PARAMS "
                   ":Syntax: /REGISTER <name> <email> <password>", me.name);
        return;
    }

    name     = parv[1];
    email    = parv[2];
    password = parv[3];

    if ((int)strlen(name) < MyConf.min_name_length ||
        (int)strlen(name) > MyConf.max_name_length)
    {
        sendto_one(client, NULL,
                   ":%s FAIL REGISTER BAD_ACCOUNT_NAME %s "
                   ":Account name must be between %d and %d characters.",
                   me.name, name, MyConf.min_name_length, MyConf.max_name_length);
        return;
    }

    if ((int)strlen(password) < MyConf.min_password_length ||
        (int)strlen(password) > MyConf.max_password_length)
    {
        sendto_one(client, NULL,
                   ":%s FAIL REGISTER BAD_PASSWORD %s "
                   ":Password must be between %d and %d characters.",
                   me.name, name, MyConf.min_password_length, MyConf.max_password_length);
        return;
    }

    if (MyConf.require_email &&
        (strlen(email) < 5 || !strcmp(email, "*") ||
         !strchr(email, '@') || !strchr(email, '.')))
    {
        sendto_one(client, NULL,
                   ":%s FAIL REGISTER BAD_EMAIL %s "
                   ":A valid email address is required.", me.name, name);
        return;
    }

    /* Nick in use by another client */
    {
        Client *found = find_client(name, NULL);
        if (found && found != client)
        {
            if (client->name)
                sendto_one(client, NULL,
                           ":%s FAIL REGISTER BAD_ACCOUNT_NAME %s "
                           ":That account name is currently in use.", me.name, name);
            else
                sendto_one(client, NULL,
                           ":%s FAIL REGISTER BAD_ACCOUNT_NAME %s "
                           ":That account name is banned.", me.name, name);
            return;
        }
    }

    /* Nameban check */
    if (my_find_tkl_nameban(name))
    {
        sendto_one(client, NULL,
                   ":%s FAIL REGISTER BAD_ACCOUNT_NAME %s "
                   ":That account name is banned.", me.name, name);
        return;
    }

    /* Already registered? */
    {
        Account *existing = find_account(name);
        if (existing)
        {
            sendto_one(client, NULL,
                       ":%s FAIL REGISTER ACCOUNT_EXISTS %s "
                       ":That account name is already registered.", me.name, name);
            free_account(existing);
            return;
        }
    }

    /* Hash the password with Argon2id */
    password_hash = Auth_Hash(AUTHTYPE_ARGON2, password);
    if (!password_hash)
    {
        sendto_one(client, NULL,
                   ":%s FAIL REGISTER SERVER_BUG %s "
                   ":Password hashing failed. Contact an administrator.", me.name, name);
        return;
    }

    /* Build and save the account */
    acc = safe_alloc(sizeof(Account));
    acc->name            = strdup(name);
    acc->email           = strdup(email);
    acc->password        = strdup(password_hash);
    acc->time_registered = time(NULL);
    acc->verified        = 0;
    acc->channels        = NULL;
    acc->metadata_head   = NULL;

    /* SCRAM-SHA-256 credentials.  Failure here is non-fatal: argon2 still
     * works for PLAIN, and we'll backfill on the next successful login. */
    scram_make_credentials(acc, password);

    /* Email verification path: if enabled, generate a code and try to send
     * it via the smtp hook BEFORE we log the user in. */
    int wants_verify = MyConf.verify_email && try_send_email_loaded();
    if (wants_verify)
    {
        char code[VERIFY_CODE_LENGTH + 1];
        gen_random_alnum(code, VERIFY_CODE_LENGTH);
        code[VERIFY_CODE_LENGTH] = '\0';
        acc->verify_code    = strdup(code);
        acc->verify_expires = time(NULL) + MyConf.verify_code_lifetime;
    }

    if (write_account_to_db(acc))
    {
        unreal_log(ULOG_INFO, "account", "REGISTER", client,
                   "New account registered by $client.details "
                   "[account: $account] [email: $email]",
                   log_data_string("account", acc->name),
                   log_data_string("email",   acc->email));

        if (wants_verify)
        {
            int sent = send_verify_email(acc);
            if (sent)
            {
                sendto_one(client, NULL,
                           ":%s REGISTER VERIFICATION_REQUIRED %s :"
                           "Account created.  Check %s for a verification code, "
                           "then run /VERIFY %s <code>",
                           me.name, name, acc->email, name);
                /* Do NOT log in yet; verification happens via /VERIFY. */
                RunHook(HOOKTYPE_ACCOUNT_REGISTER, acc, client);
                free_account(acc);
                return;
            }
            /* Email send couldn't be queued -- fall through to old behavior
             * (account exists, verify_code stays valid in DB).  Log warning.
             */
            unreal_log(ULOG_WARNING, "account", "VERIFY_EMAIL_NOT_QUEUED",
                       client,
                       "Could not queue verification email for $account; "
                       "smtp module unavailable or misconfigured.  "
                       "Account created without verification.",
                       log_data_string("account", acc->name));
        }

        sendto_one(client, NULL,
                   ":%s REGISTER SUCCESS %s :Account registered successfully.",
                   me.name, name);
        strlcpy(client->user->account, name, sizeof(client->user->account));
        user_account_login(NULL, client);
        RunHook(HOOKTYPE_ACCOUNT_REGISTER, acc, client);
    }
    else
    {
        sendto_one(client, NULL,
                   ":%s FAIL REGISTER INTERNAL_ERROR "
                   ":Failed to register account.", me.name);
    }
    free_account(acc);
}

/* ===================================================================
 * CMD: LISTACC [<name>]  (oper-only)
 * =================================================================== */
CMD_FUNC(list_accounts)
{
    Account **accounts;
    size_t i;

    if (!obsidian_db && obsidian_open_database(OBSIDIAN_DB) != SQLITE_OK)
    {
        sendto_one(client, NULL,
                   ":%s LISTACC SERVER_BUG :Database unavailable.", me.name);
        return;
    }

    accounts = read_accounts_from_db(!BadPtr(parv[1]) ? parv[1] : NULL);
    if (!accounts || !accounts[0])
    {
        sendto_one(client, NULL,
                   ":%s LISTACC NO_ACCOUNTS :No accounts registered.", me.name);
        free(accounts);
        return;
    }

    for (i = 0; accounts[i]; i++)
    {
        int member_count = 0;
        AccountMember *m = accounts[i]->members;
        while (m) { member_count++; m = m->next; }

        sendto_one(client, NULL,
                   ":%s LISTACC ACCOUNT %ld %s %s %ld %d %d",
                   me.name,
                   accounts[i]->id,
                   accounts[i]->name,
                   accounts[i]->email,
                   (long)accounts[i]->time_registered,
                   accounts[i]->verified,
                   member_count);
        free_account(accounts[i]);
    }
    free(accounts);
}

/* ===================================================================
 * CMD: IDENTIFY <account> <password>
 * Fallback for clients without SASL support.
 * =================================================================== */
CMD_FUNC(cmd_identify)
{
    const char *account_name, *password;
    Account *acc;

    if (!obsidian_db && obsidian_open_database(OBSIDIAN_DB) != SQLITE_OK)
    {
        sendto_one(client, NULL,
                   ":%s FAIL IDENTIFY SERVER_BUG :Database unavailable.", me.name);
        return;
    }

    if (parc < 3)
    {
        sendto_one(client, NULL,
                   ":%s NOTE IDENTIFY INVALID_PARAMS "
                   ":Syntax: /IDENTIFY <account> <password>", me.name);
        return;
    }

    account_name = parv[1];
    password     = parv[2];

    if (!account_name || !*account_name)
    {
        sendto_one(client, NULL,
                   ":%s FAIL IDENTIFY INVALID_ACCOUNT :Account name cannot be empty.",
                   me.name);
        return;
    }

    if (!strcasecmp(account_name, client->user->account))
    {
        sendto_one(client, NULL,
                   ":%s FAIL IDENTIFY ALREADY_IDENTIFIED "
                   ":You are already identified to account %s.", me.name, account_name);
        return;
    }

    /* Nick collision check */
    {
        Client *found = find_client(account_name, NULL);
        if (found && found != client)
        {
            sendto_one(client, NULL,
                       ":%s FAIL IDENTIFY INVALID_ACCOUNT "
                       ":That account name is currently in use.", me.name);
            return;
        }
    }

    if (my_find_tkl_nameban(account_name))
    {
        sendto_one(client, NULL,
                   ":%s FAIL IDENTIFY INVALID_ACCOUNT :That account name is banned.",
                   me.name);
        return;
    }

    if (!password || !*password)
    {
        sendto_one(client, NULL,
                   ":%s FAIL IDENTIFY INVALID_PASSWORD :Password cannot be empty.",
                   me.name);
        return;
    }

    acc = find_account(account_name);
    if (!acc)
    {
        sendto_one(client, NULL,
                   ":%s FAIL IDENTIFY ACCOUNT_NOT_FOUND :Account %s not found.",
                   me.name, account_name);
        return;
    }

    if (argon2_verify(acc->password, password, strlen(password), Argon2_id) == ARGON2_OK)
    {
        if (!acc->scram_salt || !acc->scram_stored_key)
        {
            if (scram_make_credentials(acc, password))
                update_account_scram(acc);
        }

        sendto_one(client, NULL,
                   ":%s IDENTIFY SUCCESS %s :You have been successfully identified.",
                   me.name, acc->name);
        strlcpy(client->user->account, acc->name, sizeof(client->user->account));
        user_account_login(NULL, client);
        DelSaslType(client);
        unreal_log(ULOG_INFO, "account", "IDENTIFY", client,
                   "User $client.details identified [account: $account] [email: $email]",
                   log_data_string("account", acc->name),
                   log_data_string("email",   acc->email));
    }
    else
    {
        sendto_one(client, NULL,
                   ":%s FAIL IDENTIFY INVALID_PASSWORD :Invalid password for account %s.",
                   me.name, acc->name);
        client->local->sasl_sent_time = 0;
        add_fake_lag(client, 7000);
    }
    free_account(acc);
}

/* ===================================================================
 * CMD: LOGOUT
 * =================================================================== */
CMD_FUNC(cmd_logout)
{
    if (!IsLoggedIn(client))
    {
        sendto_one(client, NULL,
                   ":%s FAIL LOGOUT NOT_LOGGED_IN :You are not logged in.", me.name);
        return;
    }
    strlcpy(client->user->account, "0", sizeof(client->user->account));
    user_account_login(NULL, client);
    sendto_one(client, NULL,
               ":%s LOGOUT SUCCESS :You have been logged out successfully.", me.name);
}

/* ===================================================================
 * Email verification helpers
 * =================================================================== */

/** Returns 1 if any module is registered for HOOKTYPE_SEND_EMAIL. */
static int try_send_email_loaded(void)
{
    return Hooks[HOOKTYPE_SEND_EMAIL] != NULL;
}

/** Walk HOOKTYPE_SEND_EMAIL handlers; return 1 if any returns nonzero
 *  (meaning the email was queued by some backend). */
static int try_send_email(const char *to, const char *subject,
                          const char *body)
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

static int send_verify_email(const Account *acc)
{
    char subject[256];
    char body[2048];

    if (!acc || !acc->email || !acc->verify_code)
        return 0;

    snprintf(subject, sizeof(subject),
             "Verify your %s account", me.name);
    snprintf(body, sizeof(body),
             "Hi %s,\n"
             "\n"
             "Someone (hopefully you) registered the account '%s' on %s\n"
             "with this email address.\n"
             "\n"
             "To complete registration, run this command on IRC:\n"
             "\n"
             "    /VERIFY %s %s\n"
             "\n"
             "This code expires in %ld minutes.\n"
             "\n"
             "If you didn't request this, you can ignore this message.\n",
             acc->name, acc->name, me.name,
             acc->name, acc->verify_code,
             (long)(MyConf.verify_code_lifetime / 60));

    return try_send_email(acc->email, subject, body);
}

/* ===================================================================
 * CMD: VERIFY <account> <code>
 * =================================================================== */
CMD_FUNC(cmd_verify)
{
    const char *name;
    const char *code;
    Account *acc;

    if (!obsidian_db && obsidian_open_database(OBSIDIAN_DB) != SQLITE_OK)
    {
        sendto_one(client, NULL,
                   ":%s FAIL VERIFY SERVER_BUG :Database unavailable.", me.name);
        return;
    }

    if (parc < 3 || BadPtr(parv[1]) || BadPtr(parv[2]))
    {
        sendto_one(client, NULL,
                   ":%s NOTE VERIFY INVALID_PARAMS "
                   ":Syntax: /VERIFY <account> <code>", me.name);
        return;
    }

    name = parv[1];
    code = parv[2];

    acc = find_account(name);
    if (!acc)
    {
        sendto_one(client, NULL,
                   ":%s FAIL VERIFY UNKNOWN_ACCOUNT %s "
                   ":No such account.", me.name, name);
        return;
    }

    if (acc->verified)
    {
        sendto_one(client, NULL,
                   ":%s FAIL VERIFY ALREADY_VERIFIED %s "
                   ":Account is already verified.", me.name, name);
        free_account(acc);
        return;
    }

    if (!acc->verify_code)
    {
        sendto_one(client, NULL,
                   ":%s FAIL VERIFY NO_PENDING %s "
                   ":No verification is pending for this account.",
                   me.name, name);
        free_account(acc);
        return;
    }

    if (acc->verify_expires && time(NULL) > acc->verify_expires)
    {
        sendto_one(client, NULL,
                   ":%s FAIL VERIFY CODE_EXPIRED %s "
                   ":The verification code has expired.  Re-register the "
                   "account.", me.name, name);
        free_account(acc);
        return;
    }

    if (strcmp(acc->verify_code, code))
    {
        add_fake_lag(client, 5000);
        sendto_one(client, NULL,
                   ":%s FAIL VERIFY BAD_CODE %s "
                   ":Incorrect verification code.", me.name, name);
        free_account(acc);
        return;
    }

    /* Success: clear code, mark verified, persist. */
    free(acc->verify_code);
    acc->verify_code = NULL;
    acc->verify_expires = 0;
    acc->verified = 1;

    if (!update_account_verification(acc))
    {
        sendto_one(client, NULL,
                   ":%s FAIL VERIFY INTERNAL_ERROR %s "
                   ":Could not persist verification.", me.name, name);
        free_account(acc);
        return;
    }

    sendto_one(client, NULL,
               ":%s VERIFY SUCCESS %s :Account verified.  You are now logged in.",
               me.name, name);
    unreal_log(ULOG_INFO, "account", "VERIFY", client,
               "Account $account verified by $client.details",
               log_data_string("account", acc->name));

    /* Auto-login the verifying client. */
    strlcpy(client->user->account, acc->name, sizeof(client->user->account));
    user_account_login(NULL, client);

    free_account(acc);
}

/* ===================================================================
 * RPC helpers
 * =================================================================== */
static json_t *account2json(const Account *acc)
{
    json_t *j        = json_object();
    json_t *jchannels= json_array();
    json_t *jmeta    = json_array();
    json_t *jmembers = json_object();

    json_object_set_new(j, "id",              acc->id ? json_integer(acc->id) : json_null());
    json_object_set_new(j, "name",            json_string(acc->name));
    json_object_set_new(j, "email",           json_string(acc->email));
    json_object_set_new(j, "time_registered", json_integer(acc->time_registered));
    json_object_set_new(j, "verified",        json_integer(acc->verified));

    if (acc->channels)
        for (char **c = acc->channels; *c; c++)
            json_array_append_new(jchannels, json_string(*c));
    json_object_set_new(j, "channels", jchannels);

    for (Metadata *m = acc->metadata_head; m; m = m->next)
    {
        json_t *mj = json_object();
        json_object_set_new(mj, "key",   json_string(m->key));
        json_object_set_new(mj, "value", json_string(m->value));
        json_array_append_new(jmeta, mj);
    }
    json_object_set_new(j, "metadata", jmeta);

    for (AccountMember *m = acc->members; m; m = m->next)
        json_expand_client(jmembers, m->client->id, m->client, 2);
    json_object_set_new(j, "online_clients", jmembers);

    return j;
}

RPC_CALL_FUNC(rpc_list_accounts)
{
    Account **accounts;
    json_t *jaccounts, *result;

    if (!obsidian_db)
    {
        rpc_error(client, request, JSON_RPC_ERROR_INTERNAL_ERROR,
                  "Database is not available.");
        return;
    }

    accounts = read_accounts_from_db(NULL);
    if (!accounts || !accounts[0])
    {
        free(accounts);
        rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND,
                  "No accounts registered.");
        return;
    }

    jaccounts = json_array();
    result    = json_object();
    for (size_t i = 0; accounts[i]; i++)
    {
        json_array_append_new(jaccounts, account2json(accounts[i]));
        free_account(accounts[i]);
    }
    free(accounts);
    json_object_set_new(result, "accounts", jaccounts);
    rpc_response(client, request, result);
    json_decref(result);
}

RPC_CALL_FUNC(rpc_accounts_find)
{
    const char *name;
    Account *acc;
    json_t *jacc;

    if (!obsidian_db)
    {
        rpc_error(client, request, JSON_RPC_ERROR_INTERNAL_ERROR,
                  "Database is not available.");
        return;
    }

    REQUIRE_PARAM_STRING("name", name);

    acc = find_account(name);
    if (!acc)
    {
        rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Account not found.");
        return;
    }

    jacc = account2json(acc);
    rpc_response(client, request, jacc);
    json_decref(jacc);
    free_account(acc);
}
