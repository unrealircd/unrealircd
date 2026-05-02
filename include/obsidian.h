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
#define CMD_VERIFY      "VERIFY"

/* Email verification */
#define VERIFY_CODE_LENGTH       8
#define VERIFY_CODE_LIFETIME_DEFAULT (24L * 3600L)

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
#define SASL_TYPE_SCRAM_SHA_256     6
#define SASL_TYPE_TOTP_STEPUP       7   /* second-factor TOTP after PLAIN/SCRAM */
#define SASL_TYPE_WEBAUTHN_BIO      8   /* WEBAUTHN-BIO SASL mechanism */

/* ===================================================================
 * SCRAM-SHA-256 (RFC 7677)
 * =================================================================== */
#define SCRAM_SALT_BYTES            16
#define SCRAM_KEY_BYTES             32   /* SHA-256 output size */
#define SCRAM_DEFAULT_ITERATIONS    4096
#define SCRAM_SERVER_NONCE_BYTES    24

/* ===================================================================
 * 2FA (draft/account-2fa, RFC 6238 TOTP)
 * =================================================================== */
#define CMD_2FA                     "2FA"
#define TWOFA_TYPE_TOTP             "totp"
#define TWOFA_SECRET_BYTES          20   /* recommended TOTP secret length */
#define TWOFA_CODE_DIGITS           6
#define TWOFA_PERIOD_SECONDS        30
#define TWOFA_SKEW_WINDOWS          1    /* accept ±N 30s windows */
#define TWOFA_CHALLENGE_LIFETIME    120  /* enrolment challenge TTL (seconds) */
#define TWOFA_NAME_MAX              64
#define TWOFA_ID_MAX                32

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
    char *verify_code;      /**< Pending verification code (NULL once verified or if no verification was requested) */
    time_t verify_expires;  /**< Unix time after which verify_code is invalid */
    char *scram_salt;       /**< Base64 random salt for SCRAM-SHA-256 (NULL = no SCRAM credentials yet) */
    int   scram_iterations; /**< PBKDF2 iteration count */
    char *scram_stored_key; /**< Base64(SHA-256(ClientKey)) */
    char *scram_server_key; /**< Base64(HMAC(SaltedPassword, "Server Key")) */
    int   twofa_enabled;    /**< 1 = SASL must complete a second factor */
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
    int verify_email;            /**< Require email verification before login */
    long verify_code_lifetime;   /**< Seconds a verification code remains valid */

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
    int got_verify_email;
    int got_verify_code_lifetime;
} AccountRegistrationConfStruct;

/* ===================================================================
 * Globals defined in account-registration.c
 * =================================================================== */
extern sqlite3 *obsidian_db;

/* ===================================================================
 * Function declarations
 * =================================================================== */

/* 2FA credential record */
typedef struct TwoFACredential {
    long int id;
    long int account_id;
    char *type;
    char *name;
    char *secret;       /**< Base32 secret (TOTP) or other type-specific blob */
    time_t created_at;
    struct TwoFACredential *next;
} TwoFACredential;

/* Database */
int   obsidian_open_database(const char *filename);
void  obsidian_close_database(void);
int   write_account_to_db(const Account *acc);
int   update_account_verification(const Account *acc);
int   update_account_scram(const Account *acc);
int   update_account_twofa_enabled(const Account *acc);
Account **read_accounts_from_db(const char *name);
Account  *find_account(const char *name);
Account  *find_account_by_client(Client *client);
void  free_account(Account *acc);

/* 2FA credentials */
TwoFACredential *twofa_list_credentials(long int account_id);
int              twofa_insert_credential(long int account_id, const char *type,
                                         const char *name, const char *secret,
                                         long int *out_id);
int              twofa_delete_credential(long int account_id, long int cred_id);
int              twofa_count_credentials(long int account_id);
void             twofa_free_credential_list(TwoFACredential *head);

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
