/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/webtv.c
 *   (C) Carsten V. Munk (Stskeeps <stskeeps@tspre.org>) 2000
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
#include "userload.h"
#include "version.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <utmp.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

ID_Copyright("(C) Carsten Munk 2000");

extern ircstats IRCstats;

typedef struct zMessage aMessage;
struct zMessage {
	char *command;
	int  (*func) ();
	int  maxpara;
};


int	w_whois(aClient *cptr, aClient *sptr, int parc, char *parv[]);

aMessage	webtv_cmds[] = 
{
	{"WHOIS", w_whois, 15},
	{NULL, 0, 15}
};


void	webtv_parse(aClient *sptr, char *string)
{
	char *command;
	char *cmd = NULL, *s = NULL;
	int i;
	aMessage *message = webtv_cmds;
	static char *para[16];
	
	if (!string || !*string)
	{
		sendto_one(sptr, ":IRC %s %s :No command given", MSG_PRIVATE, sptr->name);
		return;
	}
	
	cmd = strtok(string, " ");
	if (!cmd)
		return;	
		
	for (message = webtv_cmds; message->command; message++)
		if (strcasecmp(message->command, cmd) == 0)
			break;

	if (!message->command || !message->func)
	{
		sendto_one(sptr, ":IRC %s %s :Sorry, \"%s\" is an unknown command to me",
			MSG_PRIVATE, sptr->name, cmd);
		return;
	}

	i = 0;
	s = strtok(NULL, "");
	if (s)
	{
		if (message->maxpara > 15)
			message->maxpara = 15;
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

	(*message->func) (sptr->from, sptr, i, para);
	return;
}

int	w_whois(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	sendto_one(sptr, ":IRC %s %s :Mooooooooo!", MSG_PRIVATE, sptr->name);
}