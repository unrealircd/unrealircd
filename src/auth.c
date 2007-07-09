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


#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "version.h"
#include <time.h>
#ifdef _WIN32
#include <sys/timeb.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

anAuthStruct MODVAR AuthTypes[] = {
	{"plain",	AUTHTYPE_PLAINTEXT},
	{"plaintext",   AUTHTYPE_PLAINTEXT},
#ifdef AUTHENABLE_UNIXCRYPT
	{"crypt",	AUTHTYPE_UNIXCRYPT},
	{"unixcrypt",	AUTHTYPE_UNIXCRYPT},
#endif
	{"md5",	        AUTHTYPE_MD5},
#ifdef AUTHENABLE_SHA1
	{"sha1",	AUTHTYPE_SHA1},
#endif
#ifdef AUTHENABLE_SSL_CLIENTCERT
	{"sslclientcert",   AUTHTYPE_SSL_CLIENTCERT},
#endif
#ifdef AUTHENABLE_RIPEMD160
	{"ripemd160",	AUTHTYPE_RIPEMD160},
	/* sure, this is ugly, but it's our fault. -- Syzop */
	{"ripemd-160",	AUTHTYPE_RIPEMD160},
#endif
	{NULL,		0}
};

int		Auth_FindType(char *type)
{
	anAuthStruct 	*p = AuthTypes;
	
	while (p->data)
	{
		if (!strcmp(p->data, type))
			return p->type;
		p++;
	}
	return -1;
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
#ifdef AUTHENABLE_SSL_CLIENTCERT
	X509 *x509_filecert = NULL;
	FILE *x509_f = NULL;
#endif
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
	if (ce->ce_entries)
	{
		if (ce->ce_entries->ce_varname)
		{
			type = Auth_FindType(ce->ce_entries->ce_varname);
			if (type == -1)
			{
				config_error("%s:%i: authentication module failure: %s is not an implemented/enabled authentication method",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					ce->ce_entries->ce_varname);
				return -1;
			}
			switch (type)
			{
#ifdef AUTHENABLE_UNIXCRYPT
				case AUTHTYPE_UNIXCRYPT:
					/* If our data is like 1 or none, we just let em through .. */
					if (strlen(ce->ce_vardata) < 2)
					{
						config_error("%s:%i: authentication module failure: AUTHTYPE_UNIXCRYPT: no salt (crypt strings will always be >2 in length)",
							ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
						return -1;
					}
					break;
#endif
#ifdef AUTHENABLE_SSL_CLIENTCERT
				case AUTHTYPE_SSL_CLIENTCERT:
					if (!(x509_f = fopen(ce->ce_vardata, "r")))
					{
						config_error("%s:%i: authentication module failure: AUTHTYPE_SSL_CLIENTCERT: error opening file %s: %s",
							ce->ce_fileptr->cf_filename, ce->ce_varlinenum, ce->ce_vardata, strerror(errno));
						return -1;
					}
					x509_filecert = PEM_read_X509(x509_f, NULL, NULL, NULL);
					fclose(x509_f);
					if (!x509_filecert)
					{
						config_error("%s:%i: authentication module failure: AUTHTYPE_SSL_CLIENTCERT: PEM_read_X509 errored in file %s (format error?)",
							ce->ce_fileptr->cf_filename, ce->ce_varlinenum, ce->ce_vardata);
						return -1;
					}
					X509_free(x509_filecert);
					break;
#endif
				default: ;
			}
		}
	}
	return 1;	
}

anAuthStruct	*Auth_ConvertConf2AuthStruct(ConfigEntry *ce)
{
	short		type = AUTHTYPE_PLAINTEXT;
	anAuthStruct 	*as = NULL;
	/* If there is a {}, use it */
	if (ce->ce_entries)
	{
		if (ce->ce_entries->ce_varname)
		{
			type = Auth_FindType(ce->ce_entries->ce_varname);
		}
	}
	as = (anAuthStruct *) MyMalloc(sizeof(anAuthStruct));
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

static int authcheck_md5(aClient *cptr, anAuthStruct *as, char *para)
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

#ifdef AUTHENABLE_SHA1
static int authcheck_sha1(aClient *cptr, anAuthStruct *as, char *para)
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
#ifndef _WIN32
		SHA_CTX hash;
#else
		HCRYPTPROV hProv;
		HCRYPTHASH hHash;
		DWORD size = 20;
#endif
		
		/* First, decode the salt to something real... */
		rsaltlen = b64_decode(saltstr, rsalt, sizeof(rsalt));
		if (rsaltlen <= 0)
			return -1;

#ifdef _WIN32
		if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
			return -1;
#endif
		
		/* Then hash the password (1st round)... */
#ifndef _WIN32
		SHA1_Init(&hash);
		SHA1_Update(&hash, para, strlen(para));
		SHA1_Final(result1, &hash);
#else
		if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) return NULL;
		if (!CryptHashData(hHash, para, strlen(para), 0)) return NULL;
		if (!CryptGetHashParam(hHash, HP_HASHVAL, result1, &size, 0)) return NULL;
		CryptDestroyHash(hHash);
#endif
		/* Add salt to result */
		memcpy(result1+20, rsalt, rsaltlen); /* b64_decode already made sure bounds are ok */

		/* Then hash it all together again (2nd round)... */
#ifndef _WIN32
		SHA1_Init(&hash);
		SHA1_Update(&hash, result1, rsaltlen+20);
		SHA1_Final(result2, &hash);
#else
		if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) return NULL;
		if (!CryptHashData(hHash, result1, 20+rsaltlen, 0)) return NULL;
		if (!CryptGetHashParam(hHash, HP_HASHVAL, result2, &size, 0)) return NULL;
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
#endif		
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
#ifndef _WIN32
		if ((i = b64_encode(SHA1(para, strlen(para), NULL), 20, buf, sizeof(buf))))
		{
			if (!strcmp(buf, as->data))
				return 2;
			else
				return -1;
		} else
			return -1;
#else
		HCRYPTPROV hProv;
		HCRYPTHASH hHash;
		char buf2[512];
		DWORD size = 512;
		if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
		     CRYPT_VERIFYCONTEXT))
			return -1;
		if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash))
			return -1;
		if (!CryptHashData(hHash, para, strlen(para), 0))
			return -1;
		if (!CryptGetHashParam(hHash, HP_HASHVAL, buf, &size, 0))
			return -1;
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		b64_encode(buf, 20, buf2, sizeof(buf2));
		if (!strcmp(buf2, as->data))
			return 2;
		else 
			return -1;
#endif
	}
}
#endif /* AUTHENABLE_SHA1 */

#ifdef AUTHENABLE_RIPEMD160
static int authcheck_ripemd160(aClient *cptr, anAuthStruct *as, char *para)
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
#endif /* AUTHENABLE_RIPEMD160 */


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
int	Auth_Check(aClient *cptr, anAuthStruct *as, char *para)
{
#ifdef	AUTHENABLE_UNIXCRYPT
	extern	char *crypt();
#endif

#ifdef AUTHENABLE_SSL_CLIENTCERT
	X509 *x509_clientcert = NULL;
	X509 *x509_filecert = NULL;
	FILE *x509_f = NULL;
#endif

	if (!as)
		return 1;
		
	switch (as->type)
	{
		case AUTHTYPE_PLAINTEXT:
			if (!para)
				return -1;
			/* plain text compare */
			if (!strcmp(para, as->data))
				return 2;
			else
				return -1;
			break;
#ifdef AUTHENABLE_UNIXCRYPT
		case AUTHTYPE_UNIXCRYPT:
			if (!para)
				return -1;
			/* If our data is like 1 or none, we just let em through .. */
			if (!(as->data[0] && as->data[1]))
				return 1;
			if (!strcmp(crypt(para, as->data), as->data))
				return 2;
			else
				return -1;
			break;
#endif
		case AUTHTYPE_MD5:
			return authcheck_md5(cptr, as, para);
			break;
#ifdef AUTHENABLE_SHA1
		case AUTHTYPE_SHA1:
			return authcheck_sha1(cptr, as, para);
			break;
#endif
#ifdef AUTHENABLE_RIPEMD160
		case AUTHTYPE_RIPEMD160:
			return authcheck_ripemd160(cptr, as, para);
#endif
#ifdef AUTHENABLE_SSL_CLIENTCERT
		case AUTHTYPE_SSL_CLIENTCERT:
			if (!para)
				return -1;
			if (!cptr->ssl)
				return -1;
			x509_clientcert = SSL_get_peer_certificate((SSL *)cptr->ssl);
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
#endif
	}
	return -1;
}

static char *mkpass_md5(char *para)
{
static char buf[128];
char result1[16+REALSALTLEN];
char result2[16];
char saltstr[REALSALTLEN]; /* b64 encoded printable string*/
char saltraw[RAWSALTLEN];  /* raw binary */
char xresult[64];
int i;

	if (!para) return NULL;

	/* generate a random salt... */
	for (i=0; i < RAWSALTLEN; i++)
		saltraw[i] = getrandom8();

	i = b64_encode(saltraw, RAWSALTLEN, saltstr, REALSALTLEN);
	if (!i) return NULL;

	/* b64(MD5(MD5(<pass>)+salt))
	 *         ^^^^^^^^^^^
	 *           step 1
	 *     ^^^^^^^^^^^^^^^^^^^^^
	 *     step 2
	 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
	 * step 3
	 */

	/* STEP 1 */
	DoMD5(result1, para, strlen(para));

	/* STEP 2 */
	/* add salt to result */
	memcpy(result1+16, saltraw, RAWSALTLEN);
	/* Then hash it all together */
	DoMD5(result2, result1, RAWSALTLEN+16);
	
	/* STEP 3 */
	/* Then base64 encode it all together.. */
	i = b64_encode(result2, sizeof(result2), xresult, sizeof(xresult));
	if (!i) return NULL;

	/* Good.. now create the whole string:
	 * $<saltb64d>$<totalhashb64d>
	 */
	ircsprintf(buf, "$%s$%s", saltstr, xresult);
	return buf;
}

#ifdef AUTHENABLE_SHA1
static char *mkpass_sha1(char *para)
{
static char buf[128];
char result1[20+REALSALTLEN];
char result2[20];
char saltstr[REALSALTLEN]; /* b64 encoded printable string*/
char saltraw[RAWSALTLEN];  /* raw binary */
char xresult[64];
#ifndef _WIN32
SHA_CTX hash;
#else
HCRYPTPROV hProv;
HCRYPTHASH hHash;
DWORD size = 20;
#endif
int i;

	if (!para) return NULL;

	/* generate a random salt... */
	for (i=0; i < RAWSALTLEN; i++)
		saltraw[i] = getrandom8();

	i = b64_encode(saltraw, RAWSALTLEN, saltstr, REALSALTLEN);
	if (!i) return NULL;

#ifdef _WIN32
	if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
		return NULL;
#endif

	/* b64(SHA1(SHA1(<pass>)+salt))
	 *         ^^^^^^^^^^^
	 *           step 1
	 *     ^^^^^^^^^^^^^^^^^^^^^
	 *     step 2
	 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
	 * step 3
	 */

	/* STEP 1 */
#ifndef _WIN32
	SHA1_Init(&hash);
	SHA1_Update(&hash, para, strlen(para));
	SHA1_Final(result1, &hash);
#else
	if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) return NULL;
	if (!CryptHashData(hHash, para, strlen(para), 0)) return NULL;
	if (!CryptGetHashParam(hHash, HP_HASHVAL, result1, &size, 0)) return NULL;
	CryptDestroyHash(hHash);
#endif
	/* STEP 2 */
	/* add salt to result */
	memcpy(result1+20, saltraw, RAWSALTLEN);
	/* Then hash it all together */
#ifndef _WIN32
	SHA1_Init(&hash);
	SHA1_Update(&hash, result1, RAWSALTLEN+20);
	SHA1_Final(result2, &hash);
#else
	if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) return NULL;
	if (!CryptHashData(hHash, result1, RAWSALTLEN+20, 0)) return NULL;
	if (!CryptGetHashParam(hHash, HP_HASHVAL, result2, &size, 0)) return NULL;
	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);
#endif	
	/* STEP 3 */
	/* Then base64 encode it all together.. */
	i = b64_encode(result2, sizeof(result2), xresult, sizeof(xresult));
	if (!i) return NULL;

	/* Good.. now create the whole string:
	 * $<saltb64d>$<totalhashb64d>
	 */
	ircsprintf(buf, "$%s$%s", saltstr, xresult);
	return buf;
}
#endif /* AUTHENABLE_SHA1 */

#ifdef AUTHENABLE_RIPEMD160
static char *mkpass_ripemd160(char *para)
{
static char buf[128];
char result1[20+REALSALTLEN];
char result2[20];
char saltstr[REALSALTLEN]; /* b64 encoded printable string*/
char saltraw[RAWSALTLEN];  /* raw binary */
char xresult[64];
RIPEMD160_CTX hash;
int i;

	if (!para) return NULL;

	/* generate a random salt... */
	for (i=0; i < RAWSALTLEN; i++)
		saltraw[i] = getrandom8();

	i = b64_encode(saltraw, RAWSALTLEN, saltstr, REALSALTLEN);
	if (!i) return NULL;

	/* b64(RIPEMD160(RIPEMD160(<pass>)+salt))
	 *         ^^^^^^^^^^^
	 *           step 1
	 *     ^^^^^^^^^^^^^^^^^^^^^
	 *     step 2
	 * ^^^^^^^^^^^^^^^^^^^^^^^^^^
	 * step 3
	 */

	/* STEP 1 */
	RIPEMD160_Init(&hash);
	RIPEMD160_Update(&hash, para, strlen(para));
	RIPEMD160_Final(result1, &hash);

	/* STEP 2 */
	/* add salt to result */
	memcpy(result1+20, saltraw, RAWSALTLEN);
	/* Then hash it all together */
	RIPEMD160_Init(&hash);
	RIPEMD160_Update(&hash, result1, RAWSALTLEN+20);
	RIPEMD160_Final(result2, &hash);

	/* STEP 3 */
	/* Then base64 encode it all together.. */
	i = b64_encode(result2, sizeof(result2), xresult, sizeof(xresult));
	if (!i) return NULL;

	/* Good.. now create the whole string:
	 * $<saltb64d>$<totalhashb64d>
	 */
	ircsprintf(buf, "$%s$%s", saltstr, xresult);
	return buf;
}
#endif /* AUTHENABLE_RIPEMD160 */

char	*Auth_Make(short type, char *para)
{
#ifdef	AUTHENABLE_UNIXCRYPT
	char	salt[3];
	extern	char *crypt();
#endif

	switch (type)
	{
		case AUTHTYPE_PLAINTEXT:
			return (para);
			break;
#ifdef AUTHENABLE_UNIXCRYPT
		case AUTHTYPE_UNIXCRYPT:
			if (!para)
				return NULL;
			/* If our data is like 1 or none, we just let em through .. */
			if (!(para[0] && para[1]))
				return NULL;
			sprintf(salt, "%02X", (unsigned int)getrandom8());
			return(crypt(para, salt));
			break;
#endif

		case AUTHTYPE_MD5:
			return mkpass_md5(para);

#ifdef AUTHENABLE_SHA1
		case AUTHTYPE_SHA1:
			return mkpass_sha1(para);
#endif

#ifdef AUTHENABLE_RIPEMD160
		case AUTHTYPE_RIPEMD160:
			return mkpass_ripemd160(para);
#endif

		default:
			return (NULL);
	}

}

