/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/cloak.c
 *   (C) VirtualWorld code made originally by RogerY (rogery@austnet.org)
 *   Some coding by Potvin (potvin@shadownet.org)
 *   Modified by Stskeeps with some TerraX codebits 
 *    TerraX (devcom@terrax.net) - great job guys!
 *    Stskeeps (stskeeps@tspre.org)
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

/*
#ifdef lint
static char sccxid[] = "@(#)cloak.c		9.00 7/12/99 UnrealIRCd";
#endif
*/

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "ircsprintf.h"
#include "channel.h"
#include "userload.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "h.h"


/* Hidden host code below */

#define MAXVIRTSIZE     (3 + 5 + 1)
#define HASHVAL_TOTAL   30011
#define HASHVAL_PARTIAL 211

extern aClient me;
extern int seed;
int  match(char *, char *), find_exception(char *);

extern unsigned char tolowertab[];

int  str2array(char **pparv, char *string, char *delim)
{
	char *tok;
	int  pparc = 0;

	tok = (char *)strtok((char *)string, delim);
	while (tok != NULL)
	{
		pparv[pparc++] = tok;
		tok = (char *)strtok((char *)NULL, (char *)delim);
	}

	return pparc;
}


void truncstring(char *stringvar, int firstlast, int amount)
{
	if (firstlast)
	{
		stringvar += amount;
		*stringvar = 0;
		stringvar -= amount;
	}
	else
	{
		stringvar += strlen(stringvar);
		stringvar -= amount;
	}
}

#define B_BASE                  1000

int  Maskchecksum(char *data, int len)
{
	int  i;
	int  j;

	j = 0;
	for (i = 0; i < len; i++)
	{
		j += *data++ * (i < 16 ? (i + 1) * (i + 1) : i * (i - 15));
	}

	return (j + B_BASE) % 0xffff;
}


/* hidehost
 * command takes the realhost of a user
 * and changes the content of it.
 * new hidehost by vmlinuz
 * added some extra fixes by stskeeps
 * originally based on TerraIRCd
 * Fixed serious memory leak
 */

char *hidehost(char *s, int useless)
{
	static char mask[128];
	static char ipmask[64];
	int  csum;
	char *dot;
	char *cp;
	int  i, isdns;
	int  dots = 0;

	memset(mask, 0, 128);

	csum = Maskchecksum(s, strlen(s));

	if (strlen(s) > 127)	/* this isn't likely to happen: s is limited to HOSTLEN+1 (64) */
	{
		s[128] = 0;
	}

	isdns = 0;
	cp = s;
	for (i = 0; i < strlen(s); i++)
	{
		if (*cp == '.')
		{
			dots++;
		}
		cp++;
	}

	for (i = 0; i < strlen(s); i++)
	{
		if (s[i] == '.')
		{
			continue;
		}

		if (isalpha(s[i]))
		{
			isdns = 1;
			break;
		}
	}

	if (isdns)
	{
		/* it is a resolved yes.. */
		if (dots == 1)
		{		/* mystro.org f.x */
			ircsprintf(mask, "%s%c%d.%s",
			    hidden_host,
			    (csum < 0 ? '=' : '-'),
			    (csum < 0 ? -csum : csum), s);
		}
		if (dots == 0)
		{		/* localhost */
			ircsprintf(mask, "%s%c%d",
			    s,
			    (csum < 0 ? '=' : '-'), (csum < 0 ? -csum : csum));
		}

		if (dots > 1)
		{
			dot = (char *)strchr((char *)s, '.');

			/* mask like *<first dot> */
			ircsprintf(mask, "%s%c%d.%s",
			    hidden_host,
			    (csum < 0 ? '=' : '-'),
			    (csum < 0 ? -csum : csum), dot + 1);
		}
	}
	else
	{
		strncpy(ipmask, s, sizeof(ipmask));
		ipmask[sizeof(ipmask) - 1] = '\0';	/* safety check */
		dot = (char *)strrchr((char *)ipmask, '.');
		*dot = '\0';

		if (dot == NULL)	/* dot should never be NULL: IP needs dots */
			ircsprintf(mask, "%s%c%i",
			    hidden_host,
			    (csum < 0 ? '=' : '-'), (csum < 0 ? -csum : csum));
		else
			ircsprintf(mask, "%s.%s%c%i",
			    ipmask,
			    hidden_host,
			    (csum < 0 ? '=' : '-'), (csum < 0 ? -csum : csum));
	}
	return mask;
}


/* Regular user host */
/* mode = 0, just use strncpyzt, 1 = Realloc new and return new pointer */
char *make_virthost(char *curr, char *new, int mode)
{
	char *mask;
	char *x;
	int  i;
	if (curr == NULL)
		return (char *)NULL;

	mask = hidehost(curr, 0);
	if (mode == 0)
	{
		strncpyzt(new, mask, HOSTLEN);	/* */
		return NULL;
	}
	i = strlen(mask) + 1;
	if (new)
		MyFree(new);
	x = MyMalloc(i);
	strcpy(x, mask);
	return x;
}
