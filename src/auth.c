/*
 *   Unreal Internet Relay Chat Daemon, src/auth.c
 *   (C) 2001 Carsten V. Munk (stskeeps@tspre.org)
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
#include <argon2.h>

anAuthStruct MODVAR AuthTypes[] = {
	{"plain",           AUTHTYPE_PLAINTEXT},
	{"plaintext",       AUTHTYPE_PLAINTEXT},
	{"md5",             AUTHTYPE_MD5},
	{"sha1",            AUTHTYPE_SHA1},
	{"ripemd160",       AUTHTYPE_RIPEMD160},
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

/* Forward declarations */
static int parsepass(char *str, char **salt, char **hash);

/** Auto detect hash type for input hash 'hash'.
 * Will fallback to AUTHTYPE_PLAINTEXT when not found (or invalid).
 */
int Auth_AutoDetectHashType(char *hash)
{
	static char buf[512], hashbuf[256];
	char *saltstr, *hashstr;
	int bits;

	if (!strchr(hash, '$'))
	{
		/* SHA256 SSL fingerprint perhaps?
		 * These are exactly 64 bytes (00112233..etc..) or 95 bytes (00:11:22:33:etc) in size.
		 */
		if ((strlen(hash) == 64) || (strlen(hash) == 95))
		{
			char *p;
			char *hexchars = "0123456789abcdefABCDEF";
			for (p = hash; *p; p++)
				if ((*p != ':') && !strchr(hexchars, *p))
					return AUTHTYPE_PLAINTEXT; /* not hex and not colon */
			
			return AUTHTYPE_TLS_CLIENTCERTFP;
		}

		if (strlen(hash) == 44)
		{
			char *p;
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

	/* We (only) detect MD5 and SHA1 automatically.
	 * If, for some reason, you use RIPEMD160 then you'll have to be explicit about it ;)
	 */
	
	if (bits == 128)
		return AUTHTYPE_MD5;
	
	if (bits == 160)
		return AUTHTYPE_SHA1;
	
	/* else it's likely some other crypt() type */
	return AUTHTYPE_UNIXCRYPT;
}


/** Find authentication type for 'hash' and explicit type 'type'.
 * @param hash   The password hash (may be NULL if you are creating a password)
 * @param type   An explicit type. In that case we will search by this type, rather
 *               than trying to determine the type on the 'hash' parameter.
 *               Or leave NULL, then we use hash autodetection.
 */
int	Auth_FindType(char *hash, char *type)
{
	if (type)
	{
		anAuthStruct *p = AuthTypes;
		while (p->data)
		{
			if (!mycmp(p->data, type))
				return p->type;
			p++;
		}
		return -1; /* Not found */
	}

	if (hash)
		return Auth_AutoDetectHashType(hash);

	return -1; /* both 'hash' and 'type' are NULL */
}

/*
 * This is for converting something like:
 * {
 * 	password "data" { type; };
 * } 
*/

int		Auth_CheckError(ConfigEntry *ce)
{
	short		type = AUTHTYPE_PLAINTEXT;
	X509 *x509_filecert = NULL;
	FILE *x509_f = NULL;
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: authentication module failure: missing parameter",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	if (ce->ce_entries && ce->ce_entries->ce_next)
	{
		config_error("%s:%i: you may not have multiple authentication methods",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}

	type = Auth_FindType(ce->ce_vardata, ce->ce_entries ? ce->ce_entries->ce_varname : NULL);
	if (type == -1)
	{
		config_error("%s:%i: authentication module failure: %s is not an implemented/enabled authentication method",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_entries->ce_varname);
		return -1;
	}

	switch (type)
	{
		case AUTHTYPE_UNIXCRYPT:
			/* If our data is like 1 or none, we just let em through .. */
			if (strlen(ce->ce_vardata) < 2)
			{
				config_error("%s:%i: authentication module failure: AUTHTYPE_UNIXCRYPT: no salt (crypt strings will always be >2 in length)",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
				return -1;
			}
			break;
		case AUTHTYPE_TLS_CLIENTCERT:
			convert_to_absolute_path(&ce->ce_vardata, CONFDIR);
			if (!(x509_f = fopen(ce->ce_vardata, "r")))
			{
				config_error("%s:%i: authentication module failure: AUTHTYPE_TLS_CLIENTCERT: error opening file %s: %s",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum, ce->ce_vardata, strerror(errno));
				return -1;
			}
			x509_filecert = PEM_read_X509(x509_f, NULL, NULL, NULL);
			fclose(x509_f);
			if (!x509_filecert)
			{
				config_error("%s:%i: authentication module failure: AUTHTYPE_TLS_CLIENTCERT: PEM_read_X509 errored in file %s (format error?)",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum, ce->ce_vardata);
				return -1;
			}
			X509_free(x509_filecert);
			break;
		default: ;
	}

	if ((type == AUTHTYPE_MD5) || (type == AUTHTYPE_SHA1) || (type == AUTHTYPE_RIPEMD160))
	{
		config_warn("%s:%i: Deprecated authentication type. "
		            "Consider using the more secure auth-type 'argon2' instead. "
		            "See https://www.unrealircd.org/docs/Authentication_types for the complete list.",
		            ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		/* do not return, not an error. */
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
	if ((type == AUTHTYPE_UNIXCRYPT) && strncmp(ce->ce_vardata, "$5", 2) &&
	    strncmp(ce->ce_vardata, "$6", 2) && !strstr(ce->ce_vardata, "$rounds"))
	{
		config_warn("%s:%i: Using simple crypt for authentication is not recommended. "
		            "Consider using the more secure auth-type 'argon2' instead. "
		            "See https://www.unrealircd.org/docs/Authentication_types for the complete list.",
                            ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		/* do not return, not an error. */
	}
	if ((type == AUTHTYPE_PLAINTEXT) && (strlen(ce->ce_vardata) > PASSWDLEN))
	{
		config_error("%s:%i: passwords length may not exceed %d",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum, PASSWDLEN);
		return -1;
	}
	return 1;	
}

anAuthStruct	*Auth_ConvertConf2AuthStruct(ConfigEntry *ce)
{
	short		type = AUTHTYPE_PLAINTEXT;
	anAuthStruct 	*as = NULL;

	type = Auth_FindType(ce->ce_vardata, ce->ce_entries ? ce->ce_entries->ce_varname : NULL);
	if (type == -1)
		type = AUTHTYPE_PLAINTEXT;

	as = MyMallocEx(sizeof(anAuthStruct));
	as->data = strdup(ce->ce_vardata);
	as->type = type;
	return as;
}

void	Auth_DeleteAuthStruct(anAuthStruct *as)
{
	if (!as)
		return;
	if (as->data) 
		MyFree(as->data);
	MyFree(as);
}

/* Both values are pretty insane as of 2004, but... just in case. */
#define MAXSALTLEN		127
#define MAXHASHLEN		255

/* RAW salt length (before b64_encode) to use in /MKPASSWD
 * and REAL salt length (after b64_encode, including terminating nul),
 * used for reserving memory.
 */
#define RAWSALTLEN		6
#define REALSALTLEN		12

/** Parses a password.
 * This routine can parse a pass that has a salt (new as of unreal 3.2.1)
 * and will set the 'salt' pointer and 'hash' accordingly.
 * RETURN VALUES:
 * 1 If succeeded, salt and hash can be used.
 * 0 If it's a password without a salt ('old'), salt and hash are not touched.
 */
static int parsepass(char *str, char **salt, char **hash)
{
static char saltbuf[MAXSALTLEN+1], hashbuf[MAXHASHLEN+1];
char *p;
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

static int authcheck_argon2(Client *cptr, anAuthStruct *as, char *para)
{
	argon2_type hashtype;

	if (!para)
		return -1;

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
		return -1; /* unknown argon2 type */

	if (argon2_verify(as->data, para, strlen(para), hashtype) == ARGON2_OK)
		return 2; /* MATCH */

	return -1; /* NO MATCH or error */
}

static int authcheck_bcrypt(Client *cptr, anAuthStruct *as, char *para)
{
char data[512]; /* NOTE: only 64 required by BF_crypt() */
char *str;

	if (!para)
		return -1;

	memset(data, 0, sizeof(data));
	str = _crypt_blowfish_rn(para, as->data, data, sizeof(data));

	if (!str)
		return -1; /* ERROR / INVALID HASH */

	if (!strcmp(str, as->data))
		return 2; /* MATCH */
	
	return -1; /* NO MATCH */
}

static int authcheck_md5(Client *cptr, anAuthStruct *as, char *para)
{
static char buf[512];
int	i, r;
char *saltstr, *hashstr;

	if (!para)
		return -1;
	r = parsepass(as->data, &saltstr, &hashstr);
	if (r == 0) /* Old method without salt: b64(MD5(<pass>)) */
	{
		char result[16];
		
		DoMD5(result, para, strlen(para));
		if ((i = b64_encode(result, sizeof(result), buf, sizeof(buf))))
		{
			if (!strcmp(buf, as->data))
				return 2;
			else
				return -1;
		} else
			return -1;
	} else {
		/* New method with salt: b64(MD5(MD5(<pass>)+salt)) */
		char result1[MAXSALTLEN+16+1];
		char result2[16];
		char rsalt[MAXSALTLEN+1];
		int rsaltlen;
		
		/* First, decode the salt to something real... */
		rsaltlen = b64_decode(saltstr, rsalt, sizeof(rsalt));
		if (rsaltlen <= 0)
			return -1;
		
		/* Then hash the password (1st round)... */
		DoMD5(result1, para, strlen(para));

		/* Add salt to result */
		memcpy(result1+16, rsalt, rsaltlen); /* b64_decode already made sure bounds are ok */

		/* Then hash it all together again (2nd round)... */
		DoMD5(result2, result1, rsaltlen+16);
		
		/* Then base64 encode it all and we are done... */
		if ((i = b64_encode(result2, sizeof(result2), buf, sizeof(buf))))
		{
			if (!strcmp(buf, hashstr))
				return 2;
			else
				return -1;
		} else
			return -1;
	}
	return -1; /* NOTREACHED */
}

static int authcheck_sha1(Client *cptr, anAuthStruct *as, char *para)
{
char buf[512];
int i, r;
char *saltstr, *hashstr;

	if (!para)
		return -1;
	r = parsepass(as->data, &saltstr, &hashstr);
	if (r)
	{
		/* New method with salt: b64(SHA1(SHA1(<pass>)+salt)) */
		char result1[MAXSALTLEN+20+1];
		char result2[20];
		char rsalt[MAXSALTLEN+1];
		int rsaltlen;
		SHA_CTX hash;
		
		/* First, decode the salt to something real... */
		rsaltlen = b64_decode(saltstr, rsalt, sizeof(rsalt));
		if (rsaltlen <= 0)
			return -1;

		/* Then hash the password (1st round)... */
		SHA1_Init(&hash);
		SHA1_Update(&hash, para, strlen(para));
		SHA1_Final(result1, &hash);

		/* Add salt to result */
		memcpy(result1+20, rsalt, rsaltlen); /* b64_decode already made sure bounds are ok */

		/* Then hash it all together again (2nd round)... */
		SHA1_Init(&hash);
		SHA1_Update(&hash, result1, rsaltlen+20);
		SHA1_Final(result2, &hash);

		/* Then base64 encode it all and we are done... */
		if ((i = b64_encode(result2, sizeof(result2), buf, sizeof(buf))))
		{
			if (!strcmp(buf, hashstr))
				return 2;
			else
				return -1;
		} else
			return -1;
	} else {
		/* OLD auth */
		if ((i = b64_encode(SHA1(para, strlen(para), NULL), 20, buf, sizeof(buf))))
		{
			if (!strcmp(buf, as->data))
				return 2;
			else
				return -1;
		} else
			return -1;
	}
}

static int authcheck_ripemd160(Client *cptr, anAuthStruct *as, char *para)
{
char buf[512];
int i, r;
char *saltstr, *hashstr;

	if (!para)
		return -1;
	r = parsepass(as->data, &saltstr, &hashstr);
	if (r)
	{
		/* New method with salt: b64(RIPEMD160(RIPEMD160(<pass>)+salt)) */
		char result1[MAXSALTLEN+20+1];
		char result2[20];
		char rsalt[MAXSALTLEN+1];
		int rsaltlen;
		RIPEMD160_CTX hash;
		
		/* First, decode the salt to something real... */
		rsaltlen = b64_decode(saltstr, rsalt, sizeof(rsalt));
		if (rsaltlen <= 0)
			return -1;

		/* Then hash the password (1st round)... */
		RIPEMD160_Init(&hash);
		RIPEMD160_Update(&hash, para, strlen(para));
		RIPEMD160_Final(result1, &hash);
		/* Add salt to result */
		memcpy(result1+20, rsalt, rsaltlen); /* b64_decode already made sure bounds are ok */

		/* Then hash it all together again (2nd round)... */
		RIPEMD160_Init(&hash);
		RIPEMD160_Update(&hash, result1, rsaltlen+20);
		RIPEMD160_Final(result2, &hash);
		/* Then base64 encode it all and we are done... */
		if ((i = b64_encode(result2, sizeof(result2), buf, sizeof(buf))))
		{
			if (!strcmp(buf, hashstr))
				return 2;
			else
				return -1;
		} else
			return -1;
	} else {
		/* OLD auth */
		if ((i = b64_encode(RIPEMD160(para, strlen(para), NULL), 20, buf, sizeof(buf))))
		{
			if (!strcmp(buf, as->data))
				return 2;
			else
				return -1;
		} else
			return -1;
	}
}


/*
 * cptr MUST be a local client
 * as is what it will be compared with
 * para will used in coordination with the auth type	
*/

/*
 * -1 if authentication failed
 *  1 if authentication succeeded
 *  2 if authentication succeeded, using parameter
 * -2 if authentication is delayed, don't error
 * No AuthStruct = everyone allowed
*/
int	Auth_Check(Client *cptr, anAuthStruct *as, char *para)
{
	extern	char *crypt();
	char *res;

	if (!as)
		return 1;
		
	switch (as->type)
	{
		case AUTHTYPE_PLAINTEXT:
			if (!para)
				return -1;
			if (!strcmp(as->data, "changemeplease") && !strcmp(para, as->data))
			{
				sendto_realops("Rejecting default password 'changemeplease'. "
				               "Please change the password in the configuration file.");
				return -1;
			}
			/* plain text compare */
			if (!strcmp(para, as->data))
				return 2;
			return -1;

		case AUTHTYPE_ARGON2:
			return authcheck_argon2(cptr, as, para);

		case AUTHTYPE_BCRYPT:
			return authcheck_bcrypt(cptr, as, para);

		case AUTHTYPE_UNIXCRYPT:
			if (!para)
				return -1;
			/* If our data is like 1 or none, we just let em through .. */
			if (!(as->data[0] && as->data[1]))
				return 1;
			res = crypt(para, as->data);
			if (res && !strcmp(res, as->data))
				return 2;
			return -1;

		case AUTHTYPE_MD5:
			return authcheck_md5(cptr, as, para);

		case AUTHTYPE_SHA1:
			return authcheck_sha1(cptr, as, para);

		case AUTHTYPE_RIPEMD160:
			return authcheck_ripemd160(cptr, as, para);
		
		case AUTHTYPE_TLS_CLIENTCERT:
		{
			X509 *x509_clientcert = NULL;
			X509 *x509_filecert = NULL;
			FILE *x509_f = NULL;

			if (!cptr->local->ssl)
				return -1;
			x509_clientcert = SSL_get_peer_certificate(cptr->local->ssl);
			if (!x509_clientcert)
				return -1;
			if (!(x509_f = fopen(as->data, "r")))
			{
				X509_free(x509_clientcert);
				return -1;
			}
			x509_filecert = PEM_read_X509(x509_f, NULL, NULL, NULL);
			fclose(x509_f);
			if (!x509_filecert)
			{
				X509_free(x509_clientcert);
				return -1;
			}
			if (X509_cmp(x509_filecert, x509_clientcert) != 0)
			{
				X509_free(x509_clientcert);
				X509_free(x509_filecert);
				break;
			}
			X509_free(x509_clientcert);
			X509_free(x509_filecert);
			return 2;	
		}

		case AUTHTYPE_TLS_CLIENTCERTFP:
		{
			int i, k;
			char hexcolon[EVP_MAX_MD_SIZE * 3 + 1];
			char *fp;

			if (!cptr->local->ssl)
				return -1;
			
			fp = moddata_client_get(cptr, "certfp");
			if (!fp)
				return -1;
				
			/* Make a colon version so that we keep in line with
			 * previous versions, based on Nath's patch -dboyz
			 */
			k=0;
			for (i=0; i<strlen(fp); i++) {
				if (i != 0 && i % 2 == 0)
					hexcolon[k++] = ':';
				hexcolon[k++] = fp[i];
 			}
			hexcolon[k] = '\0';

			if (strcasecmp(as->data, hexcolon) && strcasecmp(as->data, fp))
				return -1;

			return 2;
		}

		case AUTHTYPE_SPKIFP:
		{
			char *fp = spki_fingerprint(cptr);

			if (!fp)
				return -1; /* auth failed: not SSL (or other failure) */

			if (strcasecmp(as->data, fp))
				return -1; /* auth failed: mismatch */

			return 2; /* SUCCESS */
		}
	}
	return -1;
}

#define UNREALIRCD_ARGON2_DEFAULT_TIME_COST             3
#define UNREALIRCD_ARGON2_DEFAULT_MEMORY_COST           8192
#define UNREALIRCD_ARGON2_DEFAULT_PARALLELISM_COST      2
#define UNREALIRCD_ARGON2_DEFAULT_HASH_LENGTH           32
#define UNREALIRCD_ARGON2_DEFAULT_SALT_LENGTH           (128/8)

static char *mkpass_argon2(char *para)
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

static char *mkpass_bcrypt(char *para)
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

char	*Auth_Make(short type, char *para)
{
	switch (type)
	{
		case AUTHTYPE_PLAINTEXT:
			return para;

		case AUTHTYPE_ARGON2:
			return mkpass_argon2(para);

		case AUTHTYPE_BCRYPT:
			return mkpass_bcrypt(para);

		default:
			return NULL;
	}
}
