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

anAuthStruct AuthTypes[] = {
	{"plain",	AUTHTYPE_PLAINTEXT},
	{"plaintext",   AUTHTYPE_PLAINTEXT},
#ifdef AUTHENABLE_UNIXCRYPT
	{"crypt",	AUTHTYPE_UNIXCRYPT},
	{"unixcrypt",	AUTHTYPE_UNIXCRYPT},
#endif
#ifdef AUTHENABLE_MD5
	{"md5",	        AUTHTYPE_MD5},
#endif
#ifdef AUTHENABLE_SHA1
	{"sha1",	AUTHTYPE_SHA1},
#endif
#ifdef AUTHENABLE_SSL_CLIENTCERT
	{"sslclientcert",   AUTHTYPE_SSL_CLIENTCERT},
#endif
#ifdef AUTHENABLE_RIPEMD160
	{"ripemd160",	AUTHTYPE_RIPEMD160},
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
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_entries->ce_varname);
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
				default:
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
#if defined(AUTHENABLE_MD5) || defined(AUTHENABLE_SHA1) || defined(AUTHENABLE_RIPEMD160)
        static char    buf[512];
        int		i;
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
#ifdef AUTHENABLE_MD5
		case AUTHTYPE_MD5:
			if (!para)
				return -1;
#ifndef _WIN32
			if ((i = b64_encode(MD5(para, strlen(para), NULL),
                                MD5_DIGEST_LENGTH, buf, sizeof(buf))))
			{
				if (!strcmp(buf, as->data))
					return 2;
				else
					return -1;
			}
			else
				return -1;
		        break;
#else
			{
				HCRYPTPROV hProv;
				HCRYPTHASH hHash;
				char buf2[512];
				DWORD size = 512;
				if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
				    CRYPT_VERIFYCONTEXT))
					return -1;
				if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
					return -1;
				if (!CryptHashData(hHash, para, strlen(para), 0))
					return -1;
				if (!CryptGetHashParam(hHash, HP_HASHVAL, buf, &size, 0))
					return -1;
				CryptDestroyHash(hHash);
				CryptReleaseContext(hProv, 0);
				b64_encode(buf, 16, buf2, sizeof(buf2));
				if (!strcmp(buf2, as->data))
					return 2;
				else
					return -1;
			}
			break;

#endif
#endif
#ifdef AUTHENABLE_SHA1
		case AUTHTYPE_SHA1:
			if (!para)
				return -1;
#ifndef _WIN32
			
			if ((i = b64_encode(SHA1(para, strlen(para), NULL),
                                SHA_DIGEST_LENGTH, buf, sizeof(buf))))
			{
				if (!strcmp(buf, as->data))
					return 2;
				else
					return -1;
			}
			else
				return -1;
		        break;
#else
			{
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
			}
			break;
#endif
#endif
#ifdef AUTHENABLE_RIPEMD160
		case AUTHTYPE_RIPEMD160:
			if (!para)
				return -1;
			
			if ((i = b64_encode(RIPEMD160(para, strlen(para), NULL),
                                RIPEMD160_DIGEST_LENGTH, buf, sizeof(buf))))
			{
				if (!strcmp(buf, as->data))
					return 2;
				else
					return -1;
			}
			else
				return -1;
		        break;
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

char	*Auth_Make(short type, char *para)
{
#ifdef	AUTHENABLE_UNIXCRYPT
	char	salt[3];
	extern	char *crypt();
#endif
#if defined(AUTHENABLE_MD5) || defined(AUTHENABLE_SHA1) || defined(AUTHENABLE_RIPEMD160)
        static char    buf[512];
	int		i;
#endif
#ifdef _WIN32
	static char buf2[512];
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
			sprintf(salt, "%02X", (rand() >> 24) & 0xFF);
			return(crypt(para, salt));
			break;
#endif
#ifdef AUTHENABLE_MD5
		case AUTHTYPE_MD5:
			if (!para)
				return NULL;
#ifndef _WIN32
			
			if ((i = b64_encode(MD5(para, strlen(para), NULL),
                                MD5_DIGEST_LENGTH, buf, sizeof(buf))))
			{
				return (buf);
			}
			else
				return NULL;
		        break;
#else	
			{
				HCRYPTPROV hProv;
				HCRYPTHASH hHash;
				DWORD size = 512;
				if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, 
				     CRYPT_VERIFYCONTEXT))
					return NULL;
				if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
					return NULL;
				if (!CryptHashData(hHash, para, strlen(para), 0))
					return NULL;
				if (!CryptGetHashParam(hHash, HP_HASHVAL, buf, &size, 0))
					return NULL;
				CryptDestroyHash(hHash);
				CryptReleaseContext(hProv, 0);
				b64_encode(buf, 16, buf2, sizeof(buf2));
				return (buf2);
			}
			break;
#endif
#endif
#ifdef AUTHENABLE_SHA1
		case AUTHTYPE_SHA1:
			if (!para)
				return NULL;
#ifndef _WIN32			
			if ((i = b64_encode(SHA1(para, strlen(para), NULL),
                                SHA_DIGEST_LENGTH, buf, sizeof(buf))))
			{
				return (buf);
			}
			else
				return NULL;
		        break;
#else
			{
				HCRYPTPROV hProv;
				HCRYPTHASH hHash;
				DWORD size = 512;
				if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
				     CRYPT_VERIFYCONTEXT))
					return NULL;
				if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash))
					return NULL;
				if (!CryptHashData(hHash, para, strlen(para), 0))
					return NULL;
				if (!CryptGetHashParam(hHash, HP_HASHVAL, buf, &size, 0))
					return NULL;
				CryptDestroyHash(hHash);
				CryptReleaseContext(hProv, 0);
				b64_encode(buf, 20, buf2, sizeof(buf2));
				return (buf2);
			}
			break;
#endif

#endif
#ifdef AUTHENABLE_RIPEMD160
		case AUTHTYPE_RIPEMD160:
			if (!para)
				return NULL;
			
			if ((i = b64_encode(RIPEMD160(para, strlen(para), NULL),
                                RIPEMD160_DIGEST_LENGTH, buf, sizeof(buf))))
			{
				return (buf);
			}
			else
				return NULL;
		        break;
#endif
		default:
			return (NULL);
	}

}

