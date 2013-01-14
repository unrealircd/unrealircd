/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/webtv.c
 *   (C) Carsten V. Munk (Stskeeps <stskeeps@tspre.org>) 2000
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

#include "config.h"
#undef DYNAMIC_LINKING
#include "struct.h"
#define DYNAMIC_LINKING
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
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

typedef struct zMessage aMessage;
struct zMessage {
	char *command;
	int  (*func) ();
	int  maxpara;
};


/* This really has nothing to do with WebTV yet, but eventually it will, so I figured
 * it's easiest to put it here so why not? -- codemastr
 */
int	ban_version(aClient *cptr, aClient *sptr, int parc, char *parv[]);

aMessage webtv_cmds[] = 
{
	{"\1VERSION", ban_version, 1},
	{"\1SCRIPT", ban_version, 1},
	{NULL, 0, 15}
};


int	webtv_parse(aClient *sptr, char *string)
{
	char *cmd = NULL, *s = NULL;
	int i, n;
	aMessage *message = webtv_cmds;
	static char *para[MAXPARA + 2];
	
	if (!string || !*string)
	{
		sendto_one(sptr, ":IRC %s %s :No command given", MSG_PRIVATE, sptr->name);
		return 0;
	}

	n = strlen(string);
	cmd = strtok(string, " ");
	if (!cmd)
		return -99;	
		
	for (message = webtv_cmds; message->command; message++)
		if (strcasecmp(message->command, cmd) == 0)
			break;

	if (!message->command || !message->func)
 	{
/*		sendto_one(sptr, ":IRC %s %s :Sorry, \"%s\" is an unknown command to me",
			MSG_PRIVATE, sptr->name, cmd); */
		/* restore the string*/
		if (strlen(cmd) < n)
			cmd[strlen(cmd)]= ' ';
		return -99;
	}

	i = 0;
	s = strtok(NULL, "");
	if (s)
	{
		if (message->maxpara > MAXPARA)
			message->maxpara = MAXPARA; /* paranoid ? ;p */
		for (;;)
		{
			/*
			   ** Never "FRANCE " again!! ;-) Clean
			   ** out *all* blanks.. --msa
			 */
			while (*s == ' ')
				*s++ = '\0';

			if (*s == '\0')
				break;
			if (*s == ':')
			{
				/*
				   ** The rest is single parameter--can
				   ** include blanks also.
				 */
				para[++i] = s + 1;
				break;
			}
			para[++i] = s;
			if (i >= message->maxpara)
				break;
			for (; *s != ' ' && *s; s++)
				;
		}
	}
	para[++i] = NULL;

	para[0] = sptr->name;

	return (*message->func) (sptr->from, sptr, i, para);
}

int	ban_version(aClient *cptr, aClient *sptr, int parc, char *parv[])
{	
	int len;
	ConfigItem_ban *ban;
	if (parc < 2)
		return 0;
	len = strlen(parv[1]);
	if (!len)
		return 0;
	if (parv[1][len-1] == '\1')
		parv[1][len-1] = '\0';
	if ((ban = Find_ban(NULL, parv[1], CONF_BAN_VERSION)))
		return place_host_ban(sptr, ban->action, ban->reason, BAN_VERSION_TKL_TIME);
	return 0;
}
