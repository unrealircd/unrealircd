/*
 * src/modules/guest-nicks.c
 * Guest nick enforcement for ObbyIRCd.
 *
 * Assigns and enforces a Guest-format nick for any user who is not
 * authenticated to a services account.
 *
 * Features:
 *   - On connect (after MOTD): unauthenticated users receive a Guest nick
 *   - On logout: user is renamed to a Guest nick
 *   - HOOKTYPE_CAN_USE_NICK: when enforcement is on, non-authenticated users
 *     may only use nicks matching the configured guest format pattern
 *   - Periodic EVENT re-enforcement (every 30 s)
 *
 * Config block (obbyircd.conf):
 *   guest-nicks {
 *       format "Guest$d$d$d$d";   // $d = random digit, $n = original nick
 *       enforce yes;              // yes = block non-guest nick changes too
 *   };
 *
 * This module has NO dependency on account-registration.c; it only uses
 * the core IsLoggedIn() check.
 */

#include "unrealircd.h"

/* ===================================================================
 * Module header
 * =================================================================== */
ModuleHeader MOD_HEADER = {
    "guest-nicks",
    "1.0",
    "Enforces Guest-format nicks for unauthenticated users",
    "ObbyIRCd Team",
    "unrealircd-6",
};

/* ===================================================================
 * Config
 * =================================================================== */
#define CONF_BLOCK "guest-nicks"

typedef struct {
    char *format;   /* nick format string, e.g. "Guest$d$d$d$d" */
    int   enforce;  /* block non-guest nick changes when 1 */
    int   enabled;  /* 1 if the block exists in config */

    /* duplicate-detection flags */
    int got_format;
    int got_enforce;
} GuestNicksConf;

static GuestNicksConf Conf;

/* ===================================================================
 * Forward declarations
 * =================================================================== */
static int  gn_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
static int  gn_configposttest(int *errs);
static int  gn_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
static int  gn_welcome(Client *client, int after_numeric);
static int  gn_account_login(Client *client, MessageTag *mtags);
static int  gn_can_use_nick(Client *client, const char *newnick,
                             const char **reject_reason);
static char *make_guest_nick(Client *client);
static int   nick_matches_guest_format(const char *nick);
static void  rename_to_guest(Client *client);

EVENT(gn_enforce_event);

/* ===================================================================
 * Module lifecycle
 * =================================================================== */
MOD_TEST()
{
    MARK_AS_OFFICIAL_MODULE(modinfo);
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST,     0, gn_configtest);
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, gn_configposttest);
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN,      0, gn_configrun);
    return MOD_SUCCESS;
}

MOD_INIT()
{
    MARK_AS_OFFICIAL_MODULE(modinfo);

    HookAdd(modinfo->handle, HOOKTYPE_WELCOME,       0, gn_welcome);
    HookAdd(modinfo->handle, HOOKTYPE_ACCOUNT_LOGIN, 0, gn_account_login);
    HookAdd(modinfo->handle, HOOKTYPE_CAN_USE_NICK,  0, gn_can_use_nick);

    /* Periodic re-enforcement every 30 seconds */
    EventAdd(modinfo->handle, "gn_enforce", gn_enforce_event, NULL, 30000, 0);

    return MOD_SUCCESS;
}

MOD_LOAD()
{
    return MOD_SUCCESS;
}

MOD_UNLOAD()
{
    safe_free(Conf.format);
    memset(&Conf, 0, sizeof(Conf));
    return MOD_SUCCESS;
}

/* ===================================================================
 * Config
 * =================================================================== */
static int gn_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
    ConfigEntry *cep;
    int errors = 0;

    if (type != CONFIG_MAIN)
        return 0;
    if (!ce || !ce->name || strcmp(ce->name, CONF_BLOCK))
        return 0;

    for (cep = ce->items; cep; cep = cep->next)
    {
        if (!cep->name)
        {
            config_error("%s:%i: blank item name in %s block",
                         cep->file->filename, cep->line_number, CONF_BLOCK);
            errors++;
            continue;
        }
        if (!strcmp(cep->name, "format"))
        {
            if (Conf.got_format)
            {
                config_error("%s:%i: duplicate %s::format",
                             cep->file->filename, cep->line_number, CONF_BLOCK);
                errors++;
            }
            if (BadPtr(cep->value))
            {
                config_error("%s:%i: %s::format cannot be empty",
                             cep->file->filename, cep->line_number, CONF_BLOCK);
                errors++;
            }
            Conf.got_format = 1;
        }
        else if (!strcmp(cep->name, "enforce"))
        {
            if (Conf.got_enforce)
            {
                config_error("%s:%i: duplicate %s::enforce",
                             cep->file->filename, cep->line_number, CONF_BLOCK);
                errors++;
            }
            Conf.got_enforce = 1;
        }
        else
        {
            config_warn("%s:%i: unknown directive %s::%s",
                        cep->file->filename, cep->line_number,
                        CONF_BLOCK, cep->name);
        }
    }

    *errs = errors;
    return errors ? -1 : 1;
}

static int gn_configposttest(int *errs)
{
    return 0;
}

static int gn_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
    ConfigEntry *cep;

    if (type != CONFIG_MAIN)
        return 0;
    if (!ce || !ce->name || strcmp(ce->name, CONF_BLOCK))
        return 0;

    Conf.enabled = 1;

    /* Apply defaults */
    if (!Conf.format)
        safe_strdup(Conf.format, "Guest$d$d$d$d");
    if (!Conf.got_enforce)
        Conf.enforce = 1;

    for (cep = ce->items; cep; cep = cep->next)
    {
        if (!cep->name)
            continue;
        if (!strcmp(cep->name, "format") && cep->value)
        {
            safe_free(Conf.format);
            safe_strdup(Conf.format, cep->value);
        }
        else if (!strcmp(cep->name, "enforce") && cep->value)
        {
            Conf.enforce = config_checkval(cep->value, CFG_YESNO);
        }
    }
    return 1;
}

/* ===================================================================
 * Nick helpers
 * =================================================================== */

/**
 * make_guest_nick - Expand the format string into a concrete nick.
 * $d  → random digit (0-9)
 * $n  → the client's current nick (safe truncation)
 */
static char *make_guest_nick(Client *client)
{
    const char *fmt;
    size_t fmtlen, outlen;
    char *result;
    size_t i, j;

    if (!Conf.format || !*Conf.format)
        fmt = "Guest$d$d$d$d";
    else
        fmt = Conf.format;

    fmtlen = strlen(fmt);
    /* Worst case: every $n expands to NICKLEN characters */
    outlen = fmtlen + (NICKLEN + 1) * 4 + 1;
    result = safe_alloc(outlen);
    j = 0;

    for (i = 0; i < fmtlen && j < outlen - 1; i++)
    {
        if (fmt[i] == '$' && i + 1 < fmtlen)
        {
            i++;
            if (fmt[i] == 'd')
            {
                result[j++] = (char)('0' + (rand() % 10));
            }
            else if (fmt[i] == 'n' && client && client->name)
            {
                size_t nicklen = strlen(client->name);
                if (j + nicklen < outlen - 1)
                {
                    strlcpy(result + j, client->name, outlen - j);
                    j += nicklen;
                }
            }
            else
            {
                /* Literal $ + unknown char */
                if (j < outlen - 1) result[j++] = '$';
                if (j < outlen - 1) result[j++] = fmt[i];
            }
        }
        else
        {
            result[j++] = fmt[i];
        }
    }
    result[j] = '\0';
    return result;
}

/**
 * nick_matches_guest_format - Returns 1 if `nick` could have been produced
 * by the configured format (simple prefix match on the literal prefix before
 * the first $d or $n substitution token).
 */
static int nick_matches_guest_format(const char *nick)
{
    const char *fmt;
    size_t i, prefix_len;
    char prefix[NICKLEN + 1];

    if (!nick || !*nick)
        return 0;

    fmt = (Conf.format && *Conf.format) ? Conf.format : "Guest$d$d$d$d";

    /* Extract the literal prefix before the first substitution */
    prefix_len = 0;
    for (i = 0; fmt[i] && fmt[i] != '$' && prefix_len < NICKLEN; i++)
        prefix[prefix_len++] = fmt[i];
    prefix[prefix_len] = '\0';

    if (prefix_len == 0)
        return 1; /* format starts with substitution — any nick passes */

    return strncasecmp(nick, prefix, prefix_len) == 0;
}

/**
 * rename_to_guest - Assign a fresh Guest nick to `client`.
 * Retries up to 10 times to find a nick not already in use.
 */
static void rename_to_guest(Client *client)
{
    char *newnick;
    int tries;

    if (!MyUser(client) || !IsUser(client))
        return;

    for (tries = 0; tries < 10; tries++)
    {
        newnick = make_guest_nick(client);
        if (!find_client(newnick, NULL))
        {
            /* Change the nick */
            MessageTag *mtags = NULL;
            new_message(client, NULL, &mtags);
            sendto_local_common_channels(client, client, 0, mtags,
                                         ":%s!%s@%s NICK :%s",
                                         client->name,
                                         client->ident,
                                         GetHost(client),
                                         newnick);
            sendto_one(client, mtags, ":%s!%s@%s NICK :%s",
                       client->name, client->ident, GetHost(client), newnick);
            /* Update hash and client->name */
            del_from_client_hash_table(client->name, client);
            strlcpy(client->name, newnick, sizeof(client->name));
            add_to_client_hash_table(client->name, client);
            free_message_tags(mtags);
            safe_free(newnick);
            return;
        }
        safe_free(newnick);
    }
    /* After 10 tries, give up silently — nick enforcement will retry next cycle */
}

/* ===================================================================
 * Hook: HOOKTYPE_WELCOME
 * After MOTD (after_numeric == 376), rename unauthenticated users.
 * =================================================================== */
static int gn_welcome(Client *client, int after_numeric)
{
    if (!Conf.enabled || after_numeric != 376)
        return 0;
    if (!MyUser(client) || !IsUser(client))
        return 0;
    if (IsLoggedIn(client))
        return 0;
    if (nick_matches_guest_format(client->name))
        return 0; /* already a guest nick */
    rename_to_guest(client);
    return 0;
}

/* ===================================================================
 * Hook: HOOKTYPE_ACCOUNT_LOGIN
 * When a user logs OUT (account set to "0"), rename them to a guest nick.
 * =================================================================== */
static int gn_account_login(Client *client, MessageTag *mtags)
{
    if (!Conf.enabled)
        return 0;
    if (!IsUser(client) || !MyUser(client))
        return 0;
    if (IsLoggedIn(client))
        return 0; /* this is a login, not a logout */
    /* Logged out: assign a guest nick */
    if (!nick_matches_guest_format(client->name))
        rename_to_guest(client);
    return 0;
}

/* ===================================================================
 * Hook: HOOKTYPE_CAN_USE_NICK
 * When enforcement is on, only allow guest-format nicks for unauthed users.
 * =================================================================== */
static int gn_can_use_nick(Client *client, const char *newnick,
                            const char **reject_reason)
{
    static char errbuf[256];

    if (!Conf.enabled || !Conf.enforce)
        return HOOK_CONTINUE;
    if (!IsUser(client) || !MyUser(client))
        return HOOK_CONTINUE;
    if (IsLoggedIn(client))
        return HOOK_CONTINUE;
    if (nick_matches_guest_format(newnick))
        return HOOK_CONTINUE;

    snprintf(errbuf, sizeof(errbuf),
             "You must be logged in to use that nickname.");
    *reject_reason = errbuf;
    return HOOK_DENY;
}

/* ===================================================================
 * Periodic enforcement event
 * =================================================================== */
EVENT(gn_enforce_event)
{
    Client *client;

    if (!Conf.enabled)
        return;

    list_for_each_entry(client, &lclient_list, lclient_node)
    {
        if (!IsUser(client) || !MyUser(client))
            continue;
        if (IsLoggedIn(client))
            continue;
        if (nick_matches_guest_format(client->name))
            continue;
        rename_to_guest(client);
        if (IsDead(client))
            break; /* safety: list may have changed */
    }
}
