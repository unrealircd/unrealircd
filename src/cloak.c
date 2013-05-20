/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/cloak.c
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
#include "ircsprintf.h"
#include "channel.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "h.h"

/* mode = 0, just use strlcpy, 1 = Realloc new and return new pointer */
char *make_virthost(aClient *sptr, char *curr, char *new, int mode)
{
char host[256], *mask, *x, *p, *q;

	if (!curr)
		return NULL;

	/* Convert host to lowercase and cut off at 255 bytes just to be sure */
	for (p = curr, q = host; *p && (q < host+sizeof(host)-1); p++, q++)
		*q =  tolower(*p);
	*q = '\0';

	/* Call the cloaking layer */
	if (RCallbacks[CALLBACKTYPE_CLOAK_EX] != NULL)
		mask = RCallbacks[CALLBACKTYPE_CLOAK_EX]->func.pcharfunc(sptr, host);
	else if (RCallbacks[CALLBACKTYPE_CLOAK] != NULL)
		mask = RCallbacks[CALLBACKTYPE_CLOAK]->func.pcharfunc(host);
	else
		mask = curr;

	if (mode == 0)
	{
		strlcpy(new, mask, HOSTLEN + 1);
		return NULL;
	}
	if (new)
		MyFree(new);
	x = strdup(mask);
	return x;
}
