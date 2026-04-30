/*
 * include/obsidian.h
 * Shared types and declarations for ObbyIRCd account system modules.
 *
 * Used by:  src/modules/account-registration.c
 *           any other module that listens to HOOKTYPE_ACCOUNT_REGISTER
 *
 * IMPORTANT: Always include "unrealircd.h" BEFORE this header in .c files.
 * This header does NOT re-include unrealircd.h to avoid redefinition errors
 * (dynconf.h and other UnrealIRCd headers lack proper include guards).
 */
#ifndef OBSIDIAN_H
#define OBSIDIAN_H

#include <sqlite3.h>

/* ===================================================================
 * Database
 * =================================================================== */
#define OBSIDIAN_DB             PERMDATADIR "/obsidian.db"

/* ===================================================================
 * Config block name
 * =================================================================== */
#define CONF_ACCOUNT_BLOCK      "account-registration"

/* ===================================================================
 * Allowed value ranges
 * =================================================================== */
#define MIN_ACCOUNT_NAME_LENGTH  1
#define MAX_ACCOUNT_NAME_LENGTH  200
#define MIN_PASSWORD_LENGTH      3
#define MAX_PASSWORD_LENGTH      200

/* ===================================================================
 * Command names
 * =================================================================== */
#define CMD_REGISTER    "REGISTER"
#define CMD_LISTACC     "LISTACC"
#define CMD_IDENTIFY    "IDENTIFY"
#define CMD_LOGOUT      "LOGOUT"

/* ===================================================================
 * Capability
 * =================================================================== */
#define REGCAP_NAME     "draft/account-registration"

/* ===================================================================
 * Custom hook: fired after a new account is registered.
 * Handler prototype: int my_handler(Account *acc, Client *client)
 * Note: 133 is the next available hook number after the core 132 hooks.
 * =================================================================== */
#define HOOKTYPE_ACCOUNT_REGISTER   133

/* ===================================================================
 * SASL type constants
 * =================================================================== */
#define SASL_TYPE_NONE              0
#define SASL_TYPE_PLAIN             1
#define SASL_TYPE_EXTERNAL          2
#define SASL_TYPE_ANONYMOUS         3
#define SASL_TYPE_SESSION_COOKIE    4
#define SASL_TYPE_OTP               5

/* SASL ModData helpers (sasl_md is defined in account-registration.c) */
extern ModDataInfo *sasl_md;
#define GetSaslType(x)      (moddata_client((x), sasl_md).i)
#define SetSaslType(x, y)   do { moddata_client((x), sasl_md).i = (y); } while (0)
#define DelSaslType(x)      do { moddata_client((x), sasl_md).i = SASL_TYPE_NONE; } while (0)

/* ===================================================================
 * Structs
 * =================================================================== */

/** IRCv3 metadata key/value node */
typedef struct Metadata {
    int ircv3;          /**< non-zero if this is an IRCv3 METADATA item */
    char *key;
    char *value;
    struct Metadata *prev, *next;
} Metadata;

/** Online client that is logged into an account */
typedef struct AccountMember {
    Client *client;
    struct AccountMember *next;
} AccountMember;

/** Registered account record */
typedef struct Account {
    long int id;            /**< Auto-incremented DB primary key */
    char *name;             /**< Account name */
    char *email;            /**< Email address */
    char *password;         /**< Argon2id hash */
    time_t time_registered;
    int verified;
    char **channels;        /**< NULL-terminated list of auto-join channels */
    Metadata *metadata_head;
    AccountMember *members; /**< Currently online members */
} Account;

/** Configuration for the account-registration module */
typedef struct AccountRegistrationConfStruct {
    int min_name_length;
    int max_name_length;
    int min_password_length;
    int max_password_length;
    int require_email;
    int require_terms_acceptance;
    int allow_username_changes;
    int allow_password_changes;
    int allow_email_changes;
    char *guest_nick_format;

    /* "got" flags to detect duplicates during config test */
    int got_min_name_length;
    int got_max_name_length;
    int got_min_password_length;
    int got_max_password_length;
    int got_require_email;
    int got_require_terms_acceptance;
    int got_allow_username_changes;
    int got_allow_password_changes;
    int got_allow_email_changes;
    int got_guest_nick_format;
} AccountRegistrationConfStruct;

/* ===================================================================
 * Globals defined in account-registration.c
 * =================================================================== */
extern sqlite3 *obsidian_db;

/* ===================================================================
 * Function declarations
 * =================================================================== */

/* Database */
int   obsidian_open_database(const char *filename);
void  obsidian_close_database(void);
int   write_account_to_db(const Account *acc);
Account **read_accounts_from_db(const char *name);
Account  *find_account(const char *name);
Account  *find_account_by_client(Client *client);
void  free_account(Account *acc);

/* Metadata */
Metadata *create_metadata(const char *key, const char *value);
void  add_metadata(Account *acc, const char *key, const char *value);
void  free_metadata(Metadata *head);

/* Miscellaneous */
TKL  *my_find_tkl_nameban(const char *name);

/* SASL ModData serialization */
void        sat_free(ModData *m);
const char *sat_serialize(ModData *m);
void        sat_unserialize(const char *str, ModData *m);

#endif /* OBSIDIAN_H */
