/*
 *   Unreal Internet Relay Chat Daemon, src/auth.c
 *   (C) 2001 Carsten V. Munk (stskeeps@tspre.org)
 *   (C) 2003-2019 Bram Matthys (syzop@vulnscan.org) and the UnrealIRCd team
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"
#include "crypt_blowfish.h"

typedef struct AuthTypeList AuthTypeList;
struct AuthTypeList {
	char			*name;
	AuthenticationType	type;
};

/** The list of authentication types that we support. */
AuthTypeList MODVAR AuthTypeLists[] = {
	{"plain",           AUTHTYPE_PLAINTEXT},
	{"plaintext",       AUTHTYPE_PLAINTEXT},
	{"crypt",           AUTHTYPE_UNIXCRYPT},
	{"unixcrypt",       AUTHTYPE_UNIXCRYPT},
	{"bcrypt",          AUTHTYPE_BCRYPT},
	{"cert",            AUTHTYPE_TLS_CLIENTCERT},
	{"sslclientcert",   AUTHTYPE_TLS_CLIENTCERT},
	{"tlsclientcert",   AUTHTYPE_TLS_CLIENTCERT},
	{"certfp",          AUTHTYPE_TLS_CLIENTCERTFP},
	{"sslclientcertfp", AUTHTYPE_TLS_CLIENTCERTFP},
	{"tlsclientcertfp", AUTHTYPE_TLS_CLIENTCERTFP},
	{"spkifp",          AUTHTYPE_SPKIFP},
	{"argon2",          AUTHTYPE_ARGON2},
	{NULL,              0}
};

/* Helper function for Auth_AutoDetectHashType() */
static int parsepass(const char *str, char **salt, char **hash)
{
	static char saltbuf[512], hashbuf[512];
	const char *p;
	int max;

	/* Syntax: $<salt>$<hash> */
	if (*str != '$')
		return 0;
	p = strchr(str+1, '$');
	if (!p || (p == str+1) || !p[1])
		return 0;

	max = p - str;
	if (max > sizeof(saltbuf))
		max = sizeof(saltbuf);
	strlcpy(saltbuf, str+1, max);
	strlcpy(hashbuf, p+1, sizeof(hashbuf));
	*salt = saltbuf;
	*hash = hashbuf;
	return 1;
}

/** Auto detect hash type for input hash 'hash'.
 * Will fallback to AUTHTYPE_PLAINTEXT when not found (or invalid).
 */
int Auth_AutoDetectHashType(const char *hash)
{
	static char hashbuf[256];
	char *saltstr, *hashstr;
	int bits;

	if (!strchr(hash, '$'))
	{
		/* SHA256 certificate fingerprint perhaps?
		 * These are exactly 64 bytes (00112233..etc..) or 95 bytes (00:11:22:33:etc) in size.
		 */
		if ((strlen(hash) == 64) || (strlen(hash) == 95))
		{
			const char *p;
			char *hexchars = "0123456789abcdefABCDEF";
			for (p = hash; *p; p++)
				if ((*p != ':') && !strchr(hexchars, *p))
					return AUTHTYPE_PLAINTEXT; /* not hex and not colon */

			return AUTHTYPE_TLS_CLIENTCERTFP;
		}

		if (strlen(hash) == 44)
		{
			const char *p;
			char *b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
			for (p = hash; *p; p++)
				if (!strchr(b64chars, *p))
					return AUTHTYPE_PLAINTEXT; /* not base64 */

			return AUTHTYPE_SPKIFP;
		}
	}

	if ((*hash != '$') || !strchr(hash+1, '$'))
		return AUTHTYPE_PLAINTEXT;

	if (!strncmp(hash, "$2a$", 4) || !strncmp(hash, "$2b$", 4) || !strncmp(hash, "$2y$", 4))
		return AUTHTYPE_BCRYPT;

	if (!strncmp(hash, "$argon2", 7))
		return AUTHTYPE_ARGON2;

	/* Now handle UnrealIRCd-style password hashes.. */
	if (parsepass(hash, &saltstr, &hashstr) == 0)
		return AUTHTYPE_PLAINTEXT; /* old method (pre-3.2.1) or could not detect, fallback. */

	bits = b64_decode(hashstr, hashbuf, sizeof(hashbuf)) * 8;
	if (bits <= 0)
		return AUTHTYPE_UNIXCRYPT; /* decode failed. likely some other crypt() type. */

	/* else it's likely some other crypt() type */
	return AUTHTYPE_UNIXCRYPT;
}


/** Find authentication type for 'hash' and explicit type 'type'.
 * @param hash   The password hash (may be NULL if you are creating a password)
 * @param type   An explicit type. In that case we will search by this type, rather
 *               than trying to determine the type on the 'hash' parameter.
 *               Or leave NULL, then we use hash autodetection.
 */
AuthenticationType Auth_FindType(const char *hash, const char *type)
{
	if (type)
	{
		AuthTypeList *e = AuthTypeLists;
		while (e->name)
		{
			if (!mycmp(e->name, type))
				return e->type;
			e++;
		}
		return AUTHTYPE_INVALID; /* Not found */
	}

	if (hash)
		return Auth_AutoDetectHashType(hash);

	return AUTHTYPE_INVALID; /* both 'hash' and 'type' are NULL */
}

/** Check the syntax of an authentication block.
 * This is a block like: password "data" { type; };
 * in the configuration file.
*/
int Auth_CheckError(ConfigEntry *ce)
{
	AuthenticationType type = AUTHTYPE_PLAINTEXT;
	X509 *x509_filecert = NULL;
	FILE *x509_f = NULL;
	if (!ce->value)
	{
		config_error("%s:%i: authentication module failure: missing parameter",
			ce->file->filename, ce->line_number);
		return -1;
	}
	if (ce->items && ce->items->next)
	{
		config_error("%s:%i: you may not have multiple authentication methods",
			ce->file->filename, ce->line_number);
		return -1;
	}

	type = Auth_FindType(ce->value, ce->items ? ce->items->name : NULL);
	if (type == -1)
	{
		config_error("%s:%i: authentication module failure: %s is not an implemented/enabled authentication method",
			ce->file->filename, ce->line_number,
			ce->items->name);
		return -1;
	}

	switch (type)
	{
		case AUTHTYPE_UNIXCRYPT:
			/* If our data is like 1 or none, we just let em through .. */
			if (strlen(ce->value) < 2)
			{
				config_error("%s:%i: authentication module failure: AUTHTYPE_UNIXCRYPT: no salt (crypt strings will always be >2 in length)",
					ce->file->filename, ce->line_number);
				return -1;
			}
			break;
		case AUTHTYPE_TLS_CLIENTCERT:
			convert_to_absolute_path(&ce->value, CONFDIR);
			if (!(x509_f = fopen(ce->value, "r")))
			{
				config_error("%s:%i: authentication module failure: AUTHTYPE_TLS_CLIENTCERT: error opening file %s: %s",
					ce->file->filename, ce->line_number, ce->value, strerror(errno));
				return -1;
			}
			x509_filecert = PEM_read_X509(x509_f, NULL, NULL, NULL);
			fclose(x509_f);
			if (!x509_filecert)
			{
				config_error("%s:%i: authentication module failure: AUTHTYPE_TLS_CLIENTCERT: PEM_read_X509 errored in file %s (format error?)",
					ce->file->filename, ce->line_number, ce->value);
				return -1;
			}
			X509_free(x509_filecert);
			break;
		default: ;
	}

	/* Unix crypt is a bit more complicated: most types are outright 'bad',
	 * while other types have reasonable security similar to 'bcrypt'.
	 * To be honest these people should probably use 'argon2' since it's
	 * a lot better. Then again, warning about this when it's still such
	 * a common hashing method (now, in 2018) may be a bit overzealous.
	 * So: not warning about crypt types $5/$6 which use SHA256/SHA512
	 * with normally at least 5000 rounds (unless deliberately weakened
	 * by the user).
	 */
	if ((type == AUTHTYPE_UNIXCRYPT) && strncmp(ce->value, "$5", 2) &&
	    strncmp(ce->value, "$6", 2) && !strstr(ce->value, "$rounds"))
	{
		config_warn("%s:%i: Using simple crypt for authentication is not recommended. "
		            "Consider using the more secure auth-type 'argon2' instead. "
		            "See https://www.unrealircd.org/docs/Authentication_types for the complete list.",
                            ce->file->filename, ce->line_number);
		/* do not return, not an error. */
	}
	if ((type == AUTHTYPE_PLAINTEXT) && (strlen(ce->value) > PASSWDLEN))
	{
		config_error("%s:%i: passwords length may not exceed %d",
			ce->file->filename, ce->line_number, PASSWDLEN);
		return -1;
	}
	return 1;
}

/** Convert an authentication block from the configuration file
 * into an AuthConfig structure so it can be used at runtime.
 */
AuthConfig *AuthBlockToAuthConfig(ConfigEntry *ce)
{
	AuthenticationType type = AUTHTYPE_PLAINTEXT;
	AuthConfig *as = NULL;

	type = Auth_FindType(ce->value, ce->items ? ce->items->name : NULL);
	if (type == AUTHTYPE_INVALID)
		type = AUTHTYPE_PLAINTEXT;

	as = safe_alloc(sizeof(AuthConfig));
	safe_strdup(as->data, ce->value);
	as->type = type;
	return as;
}

/** Free an AuthConfig struct */
void Auth_FreeAuthConfig(AuthConfig *as)
{
	if (as)
	{
		safe_free(as->data);
		safe_free(as);
	}
}

/* RAW salt length (before b64_encode) to use in /MKPASSWD
 * and REAL salt length (after b64_encode, including terminating nul),
 * used for reserving memory.
 */
#define RAWSALTLEN		6
#define REALSALTLEN		12

static int authcheck_argon2(Client *client, AuthConfig *as, const char *para)
{
	argon2_type hashtype;

	if (!para)
		return 0;

	/* Find out the hashtype. Why do we need to do this, why is this
	 * not in the library or irrelevant by using some generic function?
	 */
	if (!strncmp(as->data, "$argon2id", 9))
		hashtype = Argon2_id;
	else if (!strncmp(as->data, "$argon2i", 8))
		hashtype = Argon2_i;
	else if (!strncmp(as->data, "$argon2d", 8))
		hashtype = Argon2_d;
	else
		return 0; /* unknown argon2 type */

	if (argon2_verify(as->data, para, strlen(para), hashtype) == ARGON2_OK)
		return 1; /* MATCH */

	return 0; /* NO MATCH or error */
}

static int authcheck_bcrypt(Client *client, AuthConfig *as, const char *para)
{
	char data[512]; /* NOTE: only 64 required by BF_crypt() */
	char *str;

	if (!para)
		return 0;

	memset(data, 0, sizeof(data));
	str = _crypt_blowfish_rn(para, as->data, data, sizeof(data));

	if (!str)
		return 0; /* ERROR / INVALID HASH */

	if (!strcmp(str, as->data))
		return 1; /* MATCH */

	return 0; /* NO MATCH */
}

static int authcheck_tls_clientcert(Client *client, AuthConfig *as, const char *para)
{
	X509 *x509_clientcert = NULL;
	X509 *x509_filecert = NULL;
	FILE *x509_f = NULL;

	if (!client->local->ssl)
		return 0;
	x509_clientcert = SSL_get_peer_certificate(client->local->ssl);
	if (!x509_clientcert)
		return 0;
	if (!(x509_f = fopen(as->data, "r")))
	{
		X509_free(x509_clientcert);
		return 0;
	}
	x509_filecert = PEM_read_X509(x509_f, NULL, NULL, NULL);
	fclose(x509_f);
	if (!x509_filecert)
	{
		X509_free(x509_clientcert);
		return 0;
	}
	if (X509_cmp(x509_filecert, x509_clientcert) != 0)
	{
		X509_free(x509_clientcert);
		X509_free(x509_filecert);
		return 0;
	}
	X509_free(x509_clientcert);
	X509_free(x509_filecert);
	return 1;
}

static int authcheck_tls_clientcert_fingerprint(Client *client, AuthConfig *as, const char *para)
{
	int i, k;
	char hexcolon[EVP_MAX_MD_SIZE * 3 + 1];
	const char *fp;

	if (!client->local->ssl)
		return 0;

	fp = moddata_client_get(client, "certfp");
	if (!fp)
		return 0;

	/* Make a colon version so that we keep in line with
	 * previous versions, based on Nath's patch -dboyz
	 */
	k=0;
	for (i=0; i<strlen(fp); i++)
	{
		if (i != 0 && i % 2 == 0)
			hexcolon[k++] = ':';
		hexcolon[k++] = fp[i];
	}
	hexcolon[k] = '\0';

	if (strcasecmp(as->data, hexcolon) && strcasecmp(as->data, fp))
		return 0;

	return 1;
}

static int authcheck_spkifp(Client *client, AuthConfig *as, const char *para)
{
	const char *fp = spki_fingerprint(client);

	if (!fp)
		return 0; /* auth failed: not TLS or some other failure */

	if (strcasecmp(as->data, fp))
		return 0; /* auth failed: mismatch */

	return 1; /* SUCCESS */
}


/*
 * client MUST be a local client
 * as is what it will be compared with
 * para will used in coordination with the auth type	
*/

/** Check authentication, such as a password against the
 * provided AuthConfig (which was parsed from the configuration
 * file earlier).
 * @param client    The client.
 * @param as      The authentication config.
 * @param para    The provided parameter (NULL allowed)
 * @returns 1 if passed, 0 if incorrect (eg: invalid password)
 * @note
 * - The return value was different in versions before UnrealIRCd 5.0.0!
 * - In older versions a NULL 'as' was treated as an allow, now it's deny.
 */
int Auth_Check(Client *client, AuthConfig *as, const char *para)
{
	extern char *crypt();
	char *res;

	if (!as || !as->data)
		return 0; /* Should not happen, but better be safe.. */

	switch (as->type)
	{
		case AUTHTYPE_PLAINTEXT:
			if (!para)
				return 0;
			if (!strcmp(as->data, "changemeplease") && !strcmp(para, as->data))
			{
				unreal_log(ULOG_INFO, "auth", "AUTH_REJECT_DEFAULT_PASSWORD", client,
				           "Rejecting default password 'changemeplease'. "
				           "Please change the password in the configuration file.");
				return 0;
			}
			/* plain text compare */
			if (!strcmp(para, as->data))
				return 1;
			return 0;

		case AUTHTYPE_ARGON2:
			return authcheck_argon2(client, as, para);

		case AUTHTYPE_BCRYPT:
			return authcheck_bcrypt(client, as, para);

		case AUTHTYPE_UNIXCRYPT:
			if (!para)
				return 0;
			res = crypt(para, as->data);
			if (res && !strcmp(res, as->data))
				return 1;
			return 0;

		case AUTHTYPE_TLS_CLIENTCERT:
			return authcheck_tls_clientcert(client, as, para);

		case AUTHTYPE_TLS_CLIENTCERTFP:
			return authcheck_tls_clientcert_fingerprint(client, as, para);

		case AUTHTYPE_SPKIFP:
			return authcheck_spkifp(client, as, para);

		case AUTHTYPE_INVALID:
			return 0; /* Should never happen */
	}
	return 0;
}

#define UNREALIRCD_ARGON2_DEFAULT_TIME_COST             2
#define UNREALIRCD_ARGON2_DEFAULT_MEMORY_COST           6144
#define UNREALIRCD_ARGON2_DEFAULT_PARALLELISM_COST      2
#define UNREALIRCD_ARGON2_DEFAULT_HASH_LENGTH           32
#define UNREALIRCD_ARGON2_DEFAULT_SALT_LENGTH           (128/8)

static char *mkpass_argon2(const char *para)
{
	static char buf[512];
	char salt[UNREALIRCD_ARGON2_DEFAULT_SALT_LENGTH];
	int ret, i;

	if (!para)
		return NULL;

	/* Initialize salt */
	for (i=0; i < sizeof(salt); i++)
		salt[i] = getrandom8();

	*buf = '\0';

	ret = argon2id_hash_encoded(UNREALIRCD_ARGON2_DEFAULT_TIME_COST,
	                            UNREALIRCD_ARGON2_DEFAULT_MEMORY_COST,
	                            UNREALIRCD_ARGON2_DEFAULT_PARALLELISM_COST,
	                            para,
	                            strlen(para),
	                            salt,
	                            sizeof(salt),
	                            UNREALIRCD_ARGON2_DEFAULT_HASH_LENGTH,
	                            buf,
	                            sizeof(buf));

	if (ret != ARGON2_OK)
		return NULL; /* internal error */

	return buf;
}

static char *mkpass_bcrypt(const char *para)
{
	static char buf[128];
	char data[512]; /* NOTE: only 64 required by BF_crypt() */
	char salt[64];
	char random_data[32];
	char *str;
	char *saltstr;
	int i;

	if (!para)
		return NULL;

	memset(data, 0, sizeof(data));

	for (i=0; i<sizeof(random_data); i++)
		random_data[i] = getrandom8();

	saltstr = _crypt_gensalt_blowfish_rn("$2y", 9, random_data, sizeof(random_data), salt, sizeof(salt));
	if (!saltstr)
		return NULL;

	str = _crypt_blowfish_rn(para, saltstr, data, sizeof(data));

	if (!str)
		return NULL; /* INTERNAL ERROR */

	strlcpy(buf, str, sizeof(buf));
	return buf;
}

/** Create a hashed password for the specified string.
 * @param type  One of AUTHTYPE_*, eg AUTHTYPE_ARGON2.
 * @param text  The password in plaintext.
 * @returns The hashed password.
 */
const char *Auth_Hash(AuthenticationType type, const char *text)
{
	switch (type)
	{
		case AUTHTYPE_PLAINTEXT:
			return text;

		case AUTHTYPE_ARGON2:
			return mkpass_argon2(text);

		case AUTHTYPE_BCRYPT:
			return mkpass_bcrypt(text);

		default:
			return NULL;
	}
}
