/*
 *   IRC - Internet Relay Chat, src/modules/cloak_sha256.c
 *   (C) 2004-2021 Bram Matthys and The UnrealIRCd Team
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

int CLOAK_IP_ONLY = 0;

#undef KEY1
#undef KEY2
#undef KEY3
#define KEY1 cloak_key1
#define KEY2 cloak_key2
#define KEY3 cloak_key3

#define SHA256_HASH_SIZE	(256/8)

char *hidehost(Client *client, char *host);
char *cloakcsum();
int cloak_config_test(ConfigFile *, ConfigEntry *, int, int *);
int cloak_config_run(ConfigFile *, ConfigEntry *, int);
int cloak_config_posttest(int *);

static char *hidehost_ipv4(char *host);
static char *hidehost_ipv6(char *host);
static char *hidehost_normalhost(char *host);
static inline unsigned int downsample(char *i);

ModuleHeader MOD_HEADER = {
	"cloak_sha256",
	"1.0",
	"Cloaking module (SHA256)",
	"UnrealIRCd Team",
	"unrealircd-6",
};

MOD_TEST()
{
	if (!CallbackAddString(modinfo->handle, CALLBACKTYPE_CLOAK_KEY_CHECKSUM, cloakcsum))
	{
		unreal_log(ULOG_ERROR, "config", "CLOAK_MODULE_DUPLICATE", NULL,
		           "cloak_sha256: Error while trying to install callback.\n"
		           "Maybe you have multiple cloaking modules loaded? You can only load one!");
		return MOD_FAILED;
	}
	if (!CallbackAddString(modinfo->handle, CALLBACKTYPE_CLOAK_EX, hidehost))
	{
		config_error("cloak_sha256: Error while trying to install cloaking callback!");
		return MOD_FAILED;
	}
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, cloak_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, cloak_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, cloak_config_run);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	if (cloak_key1)
	{
		safe_free(cloak_key1);
		safe_free(cloak_key2);
		safe_free(cloak_key3);
	}
	return MOD_SUCCESS;
}

static int check_badrandomness(char *key)
{
	char gotlowcase=0, gotupcase=0, gotdigit=0;
	char *p;

	for (p=key; *p; p++)
	{
		if (islower(*p))
			gotlowcase = 1;
		else if (isupper(*p))
			gotupcase = 1;
		else if (isdigit(*p))
			gotdigit = 1;
	}

	if (gotlowcase && gotupcase && gotdigit)
		return 0;

	return 1;
}


int cloak_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int keycnt = 0, errors = 0;
	char *keys[3];

	if (type == CONFIG_SET)
	{
		/* set::cloak-method */
		if (!ce || !ce->name || strcmp(ce->name, "cloak-method"))
			return 0;

		if (!ce->value)
		{
			config_error("%s:%i: set::cloak-method: no method specified. The only supported methods are: 'ip' and 'host'",
				ce->file->filename, ce->line_number);
			errors++;
		} else
		if (strcmp(ce->value, "ip") && strcmp(ce->value, "host"))
		{
			config_error("%s:%i: set::cloak-method: unknown method '%s'. The only supported methods are: 'ip' and 'host'",
				ce->file->filename, ce->line_number, ce->value);
			errors++;
		}

		*errs = errors;
		return errors ? -1 : 1;
	}

	if (type != CONFIG_CLOAKKEYS)
		return 0;

	nokeys = 0;
	for (cep = ce->items; cep; cep = cep->next)
	{
		keycnt++;
		if (check_badrandomness(cep->name))
		{
			config_error("%s:%i: set::cloak-keys: (key %d) Keys should be mixed a-zA-Z0-9, "
			             "like \"a2JO6fh3Q6w4oN3s7\"", cep->file->filename, cep->line_number, keycnt);
			errors++;
		}
		if (strlen(cep->name) < 80)
		{
			config_error("%s:%i: set::cloak-keys: (key %d) Each key should be at least 80 characters",
				cep->file->filename, cep->line_number, keycnt);
			errors++;
		}
		if (strlen(cep->name) > 1000)
		{
			config_error("%s:%i: set::cloak-keys: (key %d) Each key should be less than 1000 characters",
				cep->file->filename, cep->line_number, keycnt);
			errors++;
		}
		if (keycnt < 4)
			keys[keycnt-1] = cep->name;
	}
	if (keycnt != 3)
	{
		config_error("%s:%i: set::cloak-keys: we want 3 values, not %i!",
			ce->file->filename, ce->line_number, keycnt);
		errors++;
	}
	if ((keycnt == 3) && (!strcmp(keys[0], keys[1]) || !strcmp(keys[1], keys[2])))
	{
		config_error("%s:%i: set::cloak-keys: All your 3 keys should be RANDOM, they should not be equal",
			ce->file->filename, ce->line_number);
		errors++;
	}
	*errs = errors;
	return errors ? -1 : 1;
}

int cloak_config_posttest(int *errs)
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

int cloak_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;
	char buf[4096];
	char result[128];

	if (type == CONFIG_SET)
	{
		/* set::cloak-method */
		if (!ce || !ce->name || strcmp(ce->name, "cloak-method"))
			return 0;

		if (!strcmp(ce->value, "ip"))
			CLOAK_IP_ONLY = 1;

		return 0;
	}

	if (type != CONFIG_CLOAKKEYS)
		return 0;

	/* config test should ensure this goes fine... */
	cep = ce->items;
	safe_strdup(cloak_key1, cep->name);
	cep = cep->next;
	safe_strdup(cloak_key2, cep->name);
	cep = cep->next;
	safe_strdup(cloak_key3, cep->name);

	/* Calculate checksum */
	ircsnprintf(buf, sizeof(buf), "%s:%s:%s", KEY1, KEY2, KEY3);
	ircsnprintf(cloak_checksum, sizeof(cloak_checksum), "SHA256:%s", sha256hash(result, buf, strlen(buf)));
	return 1;
}

char *hidehost(Client *client, char *host)
{
	char *p;
	int host_type;

	if (CLOAK_IP_ONLY)
		host = GetIP(client);

	host_type = is_valid_ip(host);

	if (host_type == 4)
		return hidehost_ipv4(host);
	else if (host_type == 6)
		return hidehost_ipv6(host);
	else
		return hidehost_normalhost(host);
}

char *cloakcsum()
{
	return cloak_checksum;
}

/** Downsamples a 256 bit result to 32 bits (SHA256 -> unsigned int) */
static inline unsigned int downsample(char *i)
{
	char r[4];

	r[0] = i[0] ^ i[1] ^ i[2] ^ i[3] ^ i[4] ^ i[5] ^ i[6] ^ i[7];
	r[1] = i[8] ^ i[9] ^ i[10] ^ i[11] ^ i[12] ^ i[13] ^ i[14] ^ i[15];
	r[2] = i[16] ^ i[17] ^ i[18] ^ i[19] ^ i[20] ^ i[21] ^ i[22] ^ i[23];
	r[3] = i[24] ^ i[25] ^ i[26] ^ i[27] ^ i[28] ^ i[29] ^ i[30] ^ i[31];
	
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
	 * ALPHA = downsample(sha256(sha256("KEY2:A.B.C.D:KEY3")+"KEY1"));
	 * BETA  = downsample(sha256(sha256("KEY3:A.B.C:KEY1")+"KEY2"));
	 * GAMMA = downsample(sha256(sha256("KEY1:A.B:KEY2")+"KEY3"));
	 */
	sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d);

	/* ALPHA... */
	ircsnprintf(buf, sizeof(buf), "%s:%s:%s", KEY2, host, KEY3);
	sha256hash_binary(res, buf, strlen(buf));
	strlcpy(res+SHA256_HASH_SIZE, KEY1, sizeof(res)-SHA256_HASH_SIZE); /* first bytes are filled, append our key.. */
	n = strlen(res+SHA256_HASH_SIZE) + SHA256_HASH_SIZE;
	sha256hash_binary(res2, res, n);
	alpha = downsample(res2);

	/* BETA... */
	ircsnprintf(buf, sizeof(buf), "%s:%d.%d.%d:%s", KEY3, a, b, c, KEY1);
	sha256hash_binary(res, buf, strlen(buf));
	strlcpy(res+SHA256_HASH_SIZE, KEY2, sizeof(res)-SHA256_HASH_SIZE); /* first bytes are filled, append our key.. */
	n = strlen(res+SHA256_HASH_SIZE) + SHA256_HASH_SIZE;
	sha256hash_binary(res2, res, n);
	beta = downsample(res2);

	/* GAMMA... */
	ircsnprintf(buf, sizeof(buf), "%s:%d.%d:%s", KEY1, a, b, KEY2);
	sha256hash_binary(res, buf, strlen(buf));
	strlcpy(res+SHA256_HASH_SIZE, KEY3, sizeof(res)-SHA256_HASH_SIZE); /* first bytes are filled, append our key.. */
	n = strlen(res+SHA256_HASH_SIZE) + SHA256_HASH_SIZE;
	sha256hash_binary(res2, res, n);
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
	 * ALPHA = downsample(sha256(sha256("KEY2:a:b:c:d:e:f:g:h:KEY3")+"KEY1"));
	 * BETA  = downsample(sha256(sha256("KEY3:a:b:c:d:e:f:g:KEY1")+"KEY2"));
	 * GAMMA = downsample(sha256(sha256("KEY1:a:b:c:d:KEY2")+"KEY3"));
	 */
	sscanf(host, "%x:%x:%x:%x:%x:%x:%x:%x",
		&a, &b, &c, &d, &e, &f, &g, &h);

	/* ALPHA... */
	ircsnprintf(buf, sizeof(buf), "%s:%s:%s", KEY2, host, KEY3);
	sha256hash_binary(res, buf, strlen(buf));
	strlcpy(res+SHA256_HASH_SIZE, KEY1, sizeof(res)-SHA256_HASH_SIZE); /* first bytes are filled, append our key.. */
	n = strlen(res+SHA256_HASH_SIZE) + SHA256_HASH_SIZE;
	sha256hash_binary(res2, res, n);
	alpha = downsample(res2);

	/* BETA... */
	ircsnprintf(buf, sizeof(buf), "%s:%x:%x:%x:%x:%x:%x:%x:%s", KEY3, a, b, c, d, e, f, g, KEY1);
	sha256hash_binary(res, buf, strlen(buf));
	strlcpy(res+SHA256_HASH_SIZE, KEY2, sizeof(res)-SHA256_HASH_SIZE); /* first bytes are filled, append our key.. */
	n = strlen(res+SHA256_HASH_SIZE) + SHA256_HASH_SIZE;
	sha256hash_binary(res2, res, n);
	beta = downsample(res2);

	/* GAMMA... */
	ircsnprintf(buf, sizeof(buf), "%s:%x:%x:%x:%x:%s", KEY1, a, b, c, d, KEY2);
	sha256hash_binary(res, buf, strlen(buf));
	strlcpy(res+SHA256_HASH_SIZE, KEY3, sizeof(res)-SHA256_HASH_SIZE); /* first bytes are filled, append our key.. */
	n = strlen(res+SHA256_HASH_SIZE) + SHA256_HASH_SIZE;
	sha256hash_binary(res2, res, n);
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
	sha256hash_binary(res, buf, strlen(buf));
	strlcpy(res+SHA256_HASH_SIZE, KEY3, sizeof(res)-SHA256_HASH_SIZE); /* first bytes are filled, append our key.. */
	n = strlen(res+SHA256_HASH_SIZE) + SHA256_HASH_SIZE;
	sha256hash_binary(res2, res, n);
	alpha = downsample(res2);

	for (p = host; *p; p++)
		if (*p == '.')
			if (isalpha(*(p + 1)))
				break;

	if (*p)
	{
		unsigned int len;
		p++;
		ircsnprintf(result, sizeof(result), "%s-%X.", CLOAK_PREFIX, alpha);
		len = strlen(result) + strlen(p);
		if (len <= HOSTLEN)
			strlcat(result, p, sizeof(result));
		else
			strlcat(result, p + (len - HOSTLEN), sizeof(result));
	} else
		ircsnprintf(result, sizeof(result),  "%s-%X", CLOAK_PREFIX, alpha);

	return result;
}
