/*
 *   IRC - Internet Relay Chat, src/modules/cloak.c
 *   (C) 2004 The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
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

static char *cloak_key1 = NULL, *cloak_key2 = NULL, *cloak_key3 = NULL;
static char cloak_checksum[64];
static int nokeys = 1;

#undef KEY1
#undef KEY2
#undef KEY3
#define KEY1 cloak_key1
#define KEY2 cloak_key2
#define KEY3 cloak_key3

DLLFUNC char *hidehost(char *host);
DLLFUNC char *cloakcsum();
DLLFUNC int cloak_config_test(ConfigFile *, ConfigEntry *, int, int *);
DLLFUNC int cloak_config_run(ConfigFile *, ConfigEntry *, int);
DLLFUNC int cloak_config_posttest(int *);

static char *hidehost_ipv4(char *host);
static char *hidehost_ipv6(char *host);
static char *hidehost_normalhost(char *host);
static inline unsigned int downsample(char *i);

Callback *cloak = NULL, *cloak_csum = NULL;

ModuleHeader MOD_HEADER(cloak)
  = {
  "cloak",
  "v1.0",
  "Official cloaking module (md5)",
  "3.2-b8-1",
  NULL
  };

MOD_TEST(cloak)
{
	cloak = CallbackAddPCharEx(modinfo->handle, CALLBACKTYPE_CLOAK, hidehost);
	if (!cloak)
	{
		config_error("cloak: Error while trying to install cloaking callback!");
		return MOD_FAILED;
	}
	cloak_csum = CallbackAddPCharEx(modinfo->handle, CALLBACKTYPE_CLOAKKEYCSUM, cloakcsum);
	if (!cloak_csum)
	{
		config_error("cloak: Error while trying to install cloaking checksum callback!");
		return MOD_FAILED;
	}
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, cloak_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, cloak_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT(cloak)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, cloak_config_run);
	return MOD_SUCCESS;
}

MOD_LOAD(cloak)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(cloak)
{
	if (cloak_key1)
	{
		MyFree(cloak_key1);
		MyFree(cloak_key2);
		MyFree(cloak_key3);
	}
	return MOD_SUCCESS;
}

static int check_badrandomness(char *key)
{
char gotlowcase=0, gotupcase=0, gotdigit=0;
char *p;
	for (p=key; *p; p++)
		if (islower(*p))
			gotlowcase = 1;
		else if (isupper(*p))
			gotupcase = 1;
		else if (isdigit(*p))
			gotdigit = 1;

	if (gotlowcase && gotupcase && gotdigit)
		return 0;
	return 1;
}


DLLFUNC int cloak_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
ConfigEntry *cep;
int keycnt = 0, errors = 0;
char *keys[3];

	if (type != CONFIG_CLOAKKEYS)
		return 0;

	nokeys = 0;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		keycnt++;
		/* TODO: check randomness */
		if (check_badrandomness(cep->ce_varname))
		{
			config_error("%s:%i: set::cloak-keys: (key %d) Keys should be mixed a-zA-Z0-9, "
			             "like \"a2JO6fh3Q6w4oN3s7\"", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, keycnt);
			errors++;
		}
		if (strlen(cep->ce_varname) < 5)
		{
			config_error("%s:%i: set::cloak-keys: (key %d) Each key should be at least 5 characters",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, keycnt);
			errors++;
		}
		if (strlen(cep->ce_varname) > 100)
		{
			config_error("%s:%i: set::cloak-keys: (key %d) Each key should be less than 100 characters",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, keycnt);
			errors++;
		}
		if (keycnt < 4)
			keys[keycnt-1] = cep->ce_varname;
	}
	if (keycnt != 3)
	{
		config_error("%s:%i: set::cloak-keys: we want 3 values, not %i!",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum, keycnt);
		errors++;
	}
	if ((keycnt == 3) && (!strcmp(keys[0], keys[1]) || !strcmp(keys[1], keys[2])))
	{
		config_error("%s:%i: set::cloak-keys: All your 3 keys should be RANDOM, they should not be equal",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	*errs = errors;
	return errors ? -1 : 1;
}

DLLFUNC int cloak_config_posttest(int *errs)
{
int errors = 0;

	if (nokeys)
	{
		config_error("set::cloak-keys missing!");
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

DLLFUNC int cloak_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
ConfigEntry *cep;
char buf[512], result[16];

	if (type != CONFIG_CLOAKKEYS)
		return 0;

	/* config test should ensure this goes fine... */
	cep = ce->ce_entries;
	cloak_key1 = strdup(cep->ce_varname);
	cep = cep->ce_next;
	cloak_key2 = strdup(cep->ce_varname);
	cep = cep->ce_next;
	cloak_key3 = strdup(cep->ce_varname);

	/* Calculate checksum */
	ircsnprintf(buf, sizeof(buf), "%s:%s:%s", KEY1, KEY2, KEY3);
	DoMD5(result, buf, strlen(buf));
	ircsnprintf(cloak_checksum, sizeof(cloak_checksum),
		"MD5:%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x",
		(u_int)(result[0] & 0xf), (u_int)(result[0] >> 4),
		(u_int)(result[1] & 0xf), (u_int)(result[1] >> 4),
		(u_int)(result[2] & 0xf), (u_int)(result[2] >> 4),
		(u_int)(result[3] & 0xf), (u_int)(result[3] >> 4),
		(u_int)(result[4] & 0xf), (u_int)(result[4] >> 4),
		(u_int)(result[5] & 0xf), (u_int)(result[5] >> 4),
		(u_int)(result[6] & 0xf), (u_int)(result[6] >> 4),
		(u_int)(result[7] & 0xf), (u_int)(result[7] >> 4),
		(u_int)(result[8] & 0xf), (u_int)(result[8] >> 4),
		(u_int)(result[9] & 0xf), (u_int)(result[9] >> 4),
		(u_int)(result[10] & 0xf), (u_int)(result[10] >> 4),
		(u_int)(result[11] & 0xf), (u_int)(result[11] >> 4),
		(u_int)(result[12] & 0xf), (u_int)(result[12] >> 4),
		(u_int)(result[13] & 0xf), (u_int)(result[13] >> 4),
		(u_int)(result[14] & 0xf), (u_int)(result[14] >> 4),
		(u_int)(result[15] & 0xf), (u_int)(result[15] >> 4));
	return 1;
}

DLLFUNC char *hidehost(char *host)
{
char *p;

	/* IPv6 ? */	
	if (strchr(host, ':'))
		return hidehost_ipv6(host);

	/* Is this a IPv4 IP? */
	for (p = host; *p; p++)
		if (!isdigit(*p) && !(*p == '.'))
			break;
	if (!(*p))
		return hidehost_ipv4(host);
	
	/* Normal host */
	return hidehost_normalhost(host);
}

DLLFUNC char *cloakcsum()
{
	return cloak_checksum;
}

/** Downsamples a 128bit result to 32bits (md5 -> unsigned int) */
static inline unsigned int downsample(char *i)
{
char r[4];

	r[0] = i[0] ^ i[1] ^ i[2] ^ i[3];
	r[1] = i[4] ^ i[5] ^ i[6] ^ i[7];
	r[2] = i[8] ^ i[9] ^ i[10] ^ i[11];
	r[3] = i[12] ^ i[13] ^ i[14] ^ i[15];
	
	return ( ((unsigned int)r[0] << 24) +
	         ((unsigned int)r[1] << 16) +
	         ((unsigned int)r[2] << 8) +
	         (unsigned int)r[3]);
}

static char *hidehost_ipv4(char *host)
{
unsigned int a, b, c, d;
static char buf[512], res[512], res2[512], result[128];
unsigned long n;
unsigned int alpha, beta, gamma;

	/* 
	 * Output: ALPHA.BETA.GAMMA.IP
	 * ALPHA is unique for a.b.c.d
	 * BETA  is unique for a.b.c.*
	 * GAMMA is unique for a.b.*
	 * We cloak like this:
	 * ALPHA = downsample(md5(md5("KEY2:A.B.C.D:KEY3")+"KEY1"));
	 * BETA  = downsample(md5(md5("KEY3:A.B.C:KEY1")+"KEY2"));
	 * GAMMA = downsample(md5(md5("KEY1:A.B:KEY2")+"KEY3"));
	 */
	sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d);

	/* ALPHA... */
	ircsnprintf(buf, sizeof(buf), "%s:%s:%s", KEY2, host, KEY3);
	DoMD5(res, buf, strlen(buf));
	strlcpy(res+16, KEY1, sizeof(res)-16); /* first 16 bytes are filled, append our key.. */
	n = strlen(res+16) + 16;
	DoMD5(res2, res, n);
	alpha = downsample(res2);

	/* BETA... */
	ircsnprintf(buf, sizeof(buf), "%s:%d.%d.%d:%s", KEY3, a, b, c, KEY1);
	DoMD5(res, buf, strlen(buf));
	strlcpy(res+16, KEY2, sizeof(res)-16); /* first 16 bytes are filled, append our key.. */
	n = strlen(res+16) + 16;
	DoMD5(res2, res, n);
	beta = downsample(res2);

	/* GAMMA... */
	ircsnprintf(buf, sizeof(buf), "%s:%d.%d:%s", KEY1, a, b, KEY2);
	DoMD5(res, buf, strlen(buf));
	strlcpy(res+16, KEY3, sizeof(res)-16); /* first 16 bytes are filled, append our key.. */
	n = strlen(res+16) + 16;
	DoMD5(res2, res, n);
	gamma = downsample(res2);

	ircsnprintf(result, sizeof(result), "%X.%X.%X.IP", alpha, beta, gamma);
	return result;
}

static char *hidehost_ipv6(char *host)
{
unsigned int a, b, c, d, e, f, g, h;
static char buf[512], res[512], res2[512], result[128];
unsigned long n;
unsigned int alpha, beta, gamma;

	/* 
	 * Output: ALPHA:BETA:GAMMA:IP
	 * ALPHA is unique for a:b:c:d:e:f:g:h
	 * BETA  is unique for a:b:c:d:e:f:g
	 * GAMMA is unique for a:b:c:d
	 * We cloak like this:
	 * ALPHA = downsample(md5(md5("KEY2:a:b:c:d:e:f:g:h:KEY3")+"KEY1"));
	 * BETA  = downsample(md5(md5("KEY3:a:b:c:d:e:f:g:KEY1")+"KEY2"));
	 * GAMMA = downsample(md5(md5("KEY1:a:b:c:d:KEY2")+"KEY3"));
	 */
	sscanf(host, "%x:%x:%x:%x:%x:%x:%x:%x",
		&a, &b, &c, &d, &e, &f, &g, &h);

	/* ALPHA... */
	ircsnprintf(buf, sizeof(buf), "%s:%s:%s", KEY2, host, KEY3);
	DoMD5(res, buf, strlen(buf));
	strlcpy(res+16, KEY1, sizeof(res)-16); /* first 16 bytes are filled, append our key.. */
	n = strlen(res+16) + 16;
	DoMD5(res2, res, n);
	alpha = downsample(res2);

	/* BETA... */
	ircsnprintf(buf, sizeof(buf), "%s:%x:%x:%x:%x:%x:%x:%x:%s", KEY3, a, b, c, d, e, f, g, KEY1);
	DoMD5(res, buf, strlen(buf));
	strlcpy(res+16, KEY2, sizeof(res)-16); /* first 16 bytes are filled, append our key.. */
	n = strlen(res+16) + 16;
	DoMD5(res2, res, n);
	beta = downsample(res2);

	/* GAMMA... */
	ircsnprintf(buf, sizeof(buf), "%s:%x:%x:%x:%x:%s", KEY1, a, b, c, d, KEY2);
	DoMD5(res, buf, strlen(buf));
	strlcpy(res+16, KEY3, sizeof(res)-16); /* first 16 bytes are filled, append our key.. */
	n = strlen(res+16) + 16;
	DoMD5(res2, res, n);
	gamma = downsample(res2);

	ircsnprintf(result, sizeof(result), "%X:%X:%X:IP", alpha, beta, gamma);
	return result;
}

static char *hidehost_normalhost(char *host)
{
char *p;
static char buf[512], res[512], res2[512], result[HOSTLEN+1];
unsigned int alpha, n;

	ircsnprintf(buf, sizeof(buf), "%s:%s:%s", KEY1, host, KEY2);
	DoMD5(res, buf, strlen(buf));
	strlcpy(res+16, KEY3, sizeof(res)-16); /* first 16 bytes are filled, append our key.. */
	n = strlen(res+16) + 16;
	DoMD5(res2, res, n);
	alpha = downsample(res2);

	for (p = host; *p; p++)
		if (*p == '.')
			if (isalpha(*(p + 1)))
				break;

	if (*p)
	{
		unsigned int len;
		p++;
		ircsnprintf(result, sizeof(result), "%s-%X.", hidden_host, alpha);
		len = strlen(result) + strlen(p);
		if (len <= HOSTLEN)
			strncat(result, p, sizeof(result)-strlen(result)-1);
		else
			strncat(result, p + (len - HOSTLEN), sizeof(result)-strlen(result)-1);
	} else
		ircsnprintf(result, sizeof(result),  "%s-%X", hidden_host, alpha);

	return result;
}
