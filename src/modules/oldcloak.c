/*
 *   IRC - Internet Relay Chat, src/modules/oldcloak.c
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

/* THIS IS THE OLD AND VULNERABLE CLOAKING ALGORITHM.
 * PLEASE ONLY USE THIS DURING THE UPGRADE PROGRESS.
 */

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef _WIN32
#include "version.h"
#endif

#define ircabs(x) ((x < 0) ? -x : x)

unsigned long cloak_key1=0, cloak_key2=0, cloak_key3=0;
static char cloak_checksum[64];
static int nokeys = 1;

#undef KEY
#undef KEY2
#undef KEY3
#define KEY cloak_key1
#define KEY2 cloak_key2
#define KEY3 cloak_key3

DLLFUNC char *hidehost(char *host);
DLLFUNC char *cloakcsum();
DLLFUNC int oldcloak_config_test(ConfigFile *, ConfigEntry *, int, int *);
DLLFUNC int oldcloak_config_run(ConfigFile *, ConfigEntry *, int);
DLLFUNC int oldcloak_config_posttest(int *);

Callback *cloak = NULL, *cloak_csum = NULL;

ModuleHeader MOD_HEADER(oldcloak)
  = {
  "oldcloak",
  "$Id$",
  "Old cloaking module (crc32, 3.2 and lower)",
  "3.2-b8-1",
  NULL
  };

DLLFUNC int MOD_TEST(oldcloak)(ModuleInfo *modinfo)
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
	HookAddEx(modinfo->handle, HOOKTYPE_CONFIGTEST, oldcloak_config_test);
	HookAddEx(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, oldcloak_config_posttest);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_INIT(oldcloak)(ModuleInfo *modinfo)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAddEx(modinfo->handle, HOOKTYPE_CONFIGRUN, oldcloak_config_run);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(oldcloak)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(oldcloak)(int module_unload)
{
	return MOD_SUCCESS;
}

DLLFUNC int oldcloak_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
ConfigEntry *cep;
int keycnt = 0, errors = 0;
unsigned long l1, l2, l3;

	if (type != CONFIG_CLOAKKEYS)
		return 0;

	nokeys = 0;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		keycnt++;
		
	if (keycnt != 3)
	{
		config_error("%s:%i: set::cloak-keys: we want 3 values, not %i!",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum, keycnt);
		errors++;
		goto fail;
	}
	/* i == 3 SHOULD make this true .. */
	l1 = ircabs(atol(ce->ce_entries->ce_varname));
	l2 = ircabs(atol(ce->ce_entries->ce_next->ce_varname));
	l3  = ircabs(atol(ce->ce_entries->ce_next->ce_next->ce_varname));
	if ((l1 < 10000) || (l2 < 10000) || (l3 < 10000))
	{
		config_error("%s:%i: set::cloak-keys: values must be over 10000",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	
	if ((l1 >= 2147483647) || (l2 >= 2147483647) || (l3 >= 2147483647))
	{
		config_error("%s:%i: set::cloak-keys: values must be below 2147483647 (2^31-1)",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
		
fail:
	*errs = errors;
	return errors ? -1 : 1;
}

DLLFUNC int oldcloak_config_posttest(int *errs)
{
int errors = 0;

	if (nokeys)
	{
		config_error("set::cloak-keys missing!");
		errors++;
	}
	config_status("oldcloak: WARNING: The 'oldcloak' module should *ONLY* be used temporarily "
	              "for upgrading purposes since it is considered INSECURE, "
	              "upgrade to 'cloak' AS SOON AS YOU CAN.");

	*errs = errors;
	return errors ? -1 : 1;
}

DLLFUNC int oldcloak_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
ConfigEntry *cep;
unsigned long r;
char temp[128];

	if (type != CONFIG_CLOAKKEYS)
		return 0;

	/* config test should ensure this goes fine... */
	cloak_key1 = ircabs(atol(ce->ce_entries->ce_varname));
	cloak_key2 = ircabs(atol(ce->ce_entries->ce_next->ce_varname));
	cloak_key3 = ircabs(atol(ce->ce_entries->ce_next->ce_next->ce_varname));

	/* Calculate checksum */
	ircsprintf(temp, "%li.%li.%li", cloak_key1, cloak_key2, cloak_key3);
	r = our_crc32(temp, strlen(temp));
	ircsprintf(cloak_checksum, "%lX", r);
	
	return 1;
}

DLLFUNC char *hidehost(char *host)
{
	static char	cloaked[512];
	static char	h1[512];
	static char	h2[4][4];
	static char	h3[300];
	unsigned long		l[8];
	int		i;
	char		*p, *q;

	/* Find out what kind of host we're dealing with here */
	/* IPv6 ? */	
	if (strchr(host, ':'))
	{
		/* Do IPv6 cloaking here */
		/* FIXME: what the hell to do with :FFFF:192.168.1.5? 
		*/
		/*
		 * a:b:c:d:e:f:g:h
		 * 
		 * crc(a.b.c.d)
		 * crc(a.b.c.d.e.f.g)
		 * crc(a.b.c.d.e.f.g.h)
		 */
		sscanf(host, "%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx",
		         &l[0], &l[1], &l[2], &l[3],
		         &l[4], &l[5], &l[6], &l[7]);
		ircsprintf(h3, "%lx:%lx:%lx:%lx",
			l[0], l[1], l[2], l[3]);
		l[0] = our_crc32(h3, strlen(h3));
		ircsprintf(h3, "%lx:%lx:%lx:%lx:%lx:%lx:%lx",
			l[0], l[1], l[2], l[3],
			l[4], l[5], l[6]);
		l[1] = our_crc32(h3, strlen(h3));
		l[2] = our_crc32(host, strlen(host));
		for (i = 0; i <= 2; i++)
		{
			l[i] = ((l[i] + KEY2) ^ KEY) + KEY3;
			l[i] &= 0x3FFFFFFF;
	        }
		ircsprintf(cloaked, "%lx:%lx:%lx:IP",
			l[2], l[1], l[0]);
		return cloaked;
	}
	/* Is this a IPv4 IP? */
	for (p = host; *p; p++)
	{
		if (!isdigit(*p) && !(*p == '.'))
		{
				break;
		}
	}
	if (!(*p))
	{	
		/* Do IPv4 cloaking here */
		strlcpy(h1, host, sizeof h1);
		i = 0;
		for (i = 0, p = strtok(h1, "."); p && (i <= 3); p = strtok(NULL, "."), i++)
		{
			strncpy(h2[i], p, 4);			
		}
		ircsprintf(h3, "%s.%s", h2[0], h2[1]);
		l[0] = ((our_crc32(h3, strlen(h3)) + KEY) ^ KEY2) + KEY3;
		ircsprintf(h3, "%s.%s.%s", h2[0], h2[1], h2[2]);		
		l[1] = ((KEY2 ^ our_crc32(h3, strlen(h3))) + KEY3) ^ KEY;
		l[4] = our_crc32(host, strlen(host));
		l[2] = ((l[4] + KEY3) ^ KEY) + KEY2;
		l[2] &= 0x3FFFFFFF;
		l[0] &= 0x7FFFFFFF;
		l[1] &= 0xFFFFFFFF;
		snprintf(cloaked, sizeof cloaked, "%lX.%lX.%lX.IP", l[2], l[1], l[0]);
		return cloaked;
	}
	else
	{
		/* Normal host cloaking here
		 *
		 * Find first .<alpha>
		*/
		for (p = host; *p; p++)
		{
			if (*p == '.')
			{
				if (isalpha(*(p + 1)))
					break;
			}
		}
		l[0] = ((our_crc32(host, strlen(host)) ^ KEY2) + KEY) ^ KEY3;
		l[0] &= 0x3FFFFFFF;
		if (*p) {
			int len;
			p++;
			snprintf(cloaked, sizeof cloaked, "%s-%lX.", hidden_host, l[0]);
			len = strlen(cloaked) + strlen(p);
			if (len <= HOSTLEN)
				strcat(cloaked, p);
			else
				strcat(cloaked, p + (len - HOSTLEN));
		}
		else
			snprintf(cloaked, sizeof cloaked, "%s-%lX", hidden_host, l[0]);
		return cloaked;
	}
	/* Couldn't cloak, -WTF? */
	return NULL;
}

DLLFUNC char *cloakcsum()
{
	return cloak_checksum;
}
