/************************************************************************
 *   IRC - Internet Relay Chat, s_unreal.c
 *   (C) 1999-2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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

ID_Copyright("(C) Carsten Munk 1999");

time_t TSoffset = 0;
extern ircstats IRCstats;





/*
  Help.c interface for command line :)
*/
void unrealmanual(void)
{
	char *str;
	int  x = 0;

	str = MyMalloc(1024);
	printf("Starting UnrealIRCD Interactive help system\n");
	printf("Use 'QUIT' as topic name to get out of help system\n");
	x = parse_help(NULL, NULL, NULL);
	if (x == 0)
	{
		printf("*** Couldn't find main help topic!!\n");
		return;
	}
	while (myncmp(str, "QUIT", 8))
	{
		printf("Topic?> ");
		str = fgets(str, 1023, stdin);
		printf("\n");
		if (myncmp(str, "QUIT", 8))
			x = parse_help(NULL, NULL, str);
		if (x == 0)
		{
			printf("*** Couldn't find help topic '%s'\n", str);
		}
	}
	MyFree(str);
}

aClient *find_match_server(char *mask)
{
	aClient *acptr;

	if (BadPtr(mask))
		return NULL;
	for (acptr = client, collapse(mask); acptr; acptr = acptr->next)
	{
		if (!IsServer(acptr) && !IsMe(acptr))
			continue;
		if (!match(mask, acptr->name))
			break;
		continue;
	}
	return acptr;
}



