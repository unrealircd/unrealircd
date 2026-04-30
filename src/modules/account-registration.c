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

    /* Commands */
    CommandAdd(modinfo->handle, CMD_REGISTER, register_account, MAXPARA,
               CMD_USER | CMD_UNREGISTERED);
    CommandAdd(modinfo->handle, CMD_LISTACC,  list_accounts,    MAXPARA, CMD_OPER);
    CommandAdd(modinfo->handle, CMD_IDENTIFY, cmd_identify,     MAXPARA, CMD_USER);
    CommandAdd(modinfo->handle, CMD_LOGOUT,   cmd_logout,       MAXPARA, CMD_USER);
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
    moddata_client_set(&me, "saslmechlist", "PLAIN,ANONYMOUS");

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
        "  id               INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name             TEXT NOT NULL COLLATE NOCASE,"
        "  email            TEXT,"
        "  password         TEXT,"
        "  time_registered  INTEGER,"
        "  verified         INTEGER DEFAULT 0"
        ");";

    errmsg = NULL;
    if (sqlite3_exec(obsidian_db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        sqlite3_free(errmsg);
        sqlite3_close(obsidian_db);
        obsidian_db = NULL;
        return SQLITE_ERROR;
    }
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
        "INSERT INTO accounts (name, email, password, time_registered, verified)"
        " VALUES (?, ?, ?, ?, ?)";
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

    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE ? 1 : 0;
}

Account **read_accounts_from_db(const char *name)
{
    const char *sql_all = "SELECT id,name,email,password,time_registered,verified FROM accounts";
    const char *sql_one = "SELECT id,name,email,password,time_registered,verified"
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
        acc->id             = sqlite3_column_int(stmt, 0);
        acc->name           = strdup((const char *)sqlite3_column_text(stmt, 1));
        acc->email          = strdup((const char *)sqlite3_column_text(stmt, 2));
        acc->password       = strdup((const char *)sqlite3_column_text(stmt, 3));
        acc->time_registered= (time_t)sqlite3_column_int(stmt, 4);
        acc->verified       = sqlite3_column_int(stmt, 5);
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

    if (!strcmp(param, "*"))
    {
        if (GetSaslType(client))
            DelSaslType(client);
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

    if (!GetSaslType(client) || GetSaslType(client) == SASL_TYPE_NONE)
        return 0;

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

static const char *saslmechs(Client *client)
{
    return "PLAIN,ANONYMOUS";
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

    if (write_account_to_db(acc))
    {
        sendto_one(client, NULL,
                   ":%s REGISTER SUCCESS %s :Account registered successfully.",
                   me.name, name);
        strlcpy(client->user->account, name, sizeof(client->user->account));
        user_account_login(NULL, client);
        unreal_log(ULOG_INFO, "account", "REGISTER", client,
                   "New account registered by $client.details "
                   "[account: $account] [email: $email]",
                   log_data_string("account", acc->name),
                   log_data_string("email",   acc->email));
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
