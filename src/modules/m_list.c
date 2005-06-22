/*
 *   IRC - Internet Relay Chat, src/modules/m_list.c
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
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_list(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_LIST 	"LIST"	
#define TOK_LIST 	"("	

ModuleHeader MOD_HEADER(m_list)
  = {
	"m_list",
	"$Id$",
	"command /list", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_list)(ModuleInfo *modinfo)
{
	add_Command(MSG_LIST, TOK_LIST, m_list, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_list)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_list)(int module_unload)
{
	if (del_Command(MSG_LIST, TOK_LIST, m_list) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_list).name);
	}
	return MOD_SUCCESS;
}

/* Originally from bahamut, modified a bit for Unreal by codemastr
 * also Opers can now see +s channels and can still use /list while
 * HTM is active -- codemastr */

/*
 * * m_list *      parv[0] = sender prefix *      parv[1] = channel
 */
DLLFUNC CMD_FUNC(m_list)
{
	aChannel *chptr;
	TS   currenttime = TStime();
	char *name, *p = NULL;
	LOpts *lopt = NULL;
	Link *lp;
	int  usermax, usermin, error = 0, doall = 0;
	TS   chantimemin, chantimemax;
	TS   topictimemin, topictimemax;
	Link *yeslist = NULL, *nolist = NULL;

	static char *usage[] = {
		"   Usage: /LIST <options>",
		"",
		"If you don't include any options, the default is to send you the",
		"entire unfiltered list of channels. Below are the options you can",
		"use, and what channels LIST will return when you use them.",
		">number  List channels with more than <number> people.",
		"<number  List channels with less than <number> people.",
		"C>number List channels created between now and <number> minutes ago.",
		"C<number List channels created earlier than <number> minutes ago.",
		"T>number List channels whose topics are older than <number> minutes",
		"         (Ie, they have not changed in the last <number> minutes.",
		"T<number List channels whose topics are not older than <number> minutes.",
		"*mask*   List channels that match *mask*",
		"!*mask*  List channels that do not match *mask*",
		NULL
	};

	/* Some starting san checks -- No interserver lists allowed. */
	if (cptr != sptr || !sptr->user)
		return 0;

	/* If a /list is in progress, then another one will cancel it */
	if ((lopt = sptr->user->lopt) != NULL)
	{
		sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);
		free_str_list(sptr->user->lopt->yeslist);
		free_str_list(sptr->user->lopt->nolist);
		MyFree(sptr->user->lopt);
		sptr->user->lopt = NULL;
		return 0;
	}

	/* if HTM, drop this too */
#ifndef NO_FDLIST
	if (lifesux && !IsOper(cptr))
	{
		sendto_one(sptr, err_str(ERR_HTMDISABLED), me.name,
		    sptr->name, "/List");
		/* We need this for mIRC compatibility -- codemastr */
		sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);
		return 0;
	}
#endif
	if (parc < 2 || BadPtr(parv[1]))
	{

		sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, parv[0]);
		lopt = sptr->user->lopt = (LOpts *) MyMalloc(sizeof(LOpts));
		memset(lopt, '\0', sizeof(LOpts));

		lopt->showall = 1;

		if (DBufLength(&cptr->sendQ) < 2048)
			send_list(cptr, 64);

		return 0;
	}

	if ((parc == 2) && (parv[1][0] == '?') && (parv[1][1] == '\0'))
	{
		char **ptr = usage;
		for (; *ptr; ptr++)
			sendto_one(sptr, rpl_str(RPL_LISTSYNTAX),
			    me.name, cptr->name, *ptr);
		return 0;
	}

	sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, parv[0]);

	chantimemax = topictimemax = currenttime + 86400;
	chantimemin = topictimemin = 0;
	usermin = 1;		/* Minimum of 1 */
	usermax = -1;		/* No maximum */

	for (name = strtoken(&p, parv[1], ","); name && !error;
	    name = strtoken(&p, (char *)NULL, ","))
	{

		switch (*name)
		{
		  case '<':
			  usermax = atoi(name + 1) - 1;
			  doall = 1;
			  break;
		  case '>':
			  usermin = atoi(name + 1) + 1;
			  doall = 1;
			  break;
		  case 'C':
		  case 'c':	/* Channel TS time -- creation time? */
			  ++name;
			  switch (*name++)
			  {
			    case '<':
				    chantimemax = currenttime - 60 * atoi(name);
				    doall = 1;
				    break;
			    case '>':
				    chantimemin = currenttime - 60 * atoi(name);
				    doall = 1;
				    break;
			    default:
				    sendto_one(sptr, err_str(ERR_LISTSYNTAX), me.name, cptr->name);
				    error = 1;
			  }
			  break;
#ifdef LIST_USE_T
		  case 'T':
		  case 't':
			  ++name;
			  switch (*name++)
			  {
			    case '<':
				    topictimemax =
					currenttime - 60 * atoi(name);
				    doall = 1;
				    break;
			    case '>':
				    topictimemin =
					currenttime - 60 * atoi(name);
				    doall = 1;
				    break;
			    default:
				    sendto_one(sptr,
					err_str(ERR_LISTSYNTAX),
					me.name, cptr->name,
					"Bad list syntax, type /list ?");
				    error = 1;
			  }
			  break;
#endif
		  default:	/* A channel, possibly with wildcards.
				 * Thought for the future: Consider turning wildcard
				 * processing on the fly.
				 * new syntax: !channelmask will tell ircd to ignore
				 * any channels matching that mask, and then
				 * channelmask will tell ircd to send us a list of
				 * channels only masking channelmask. Note: Specifying
				 * a channel without wildcards will return that
				 * channel even if any of the !channelmask masks
				 * matches it.
				 */
			  if (*name == '!')
			  {
				  doall = 1;
				  lp = make_link();
				  lp->next = nolist;
				  nolist = lp;
				  DupString(lp->value.cp, name + 1);
			  }
			  else if (strchr(name, '*') || strchr(name, '?'))
			  {
				  doall = 1;
				  lp = make_link();
				  lp->next = yeslist;
				  yeslist = lp;
				  DupString(lp->value.cp, name);
			  }
			  else	/* Just a normal channel */
			  {
				  chptr = find_channel(name, NullChn);
				  if (chptr && (ShowChannel(sptr, chptr) || IsAnOper(sptr))) {
#ifdef LIST_SHOW_MODES
					modebuf[0] = '[';
					channel_modes(sptr, &modebuf[1], parabuf, chptr);
					if (modebuf[2] == '\0')
						modebuf[0] = '\0';
					else
						strlcat(modebuf, "]", sizeof modebuf);
#endif
					  sendto_one(sptr,
					      rpl_str(RPL_LIST),
					      me.name, parv[0],
					      name, chptr->users,
#ifdef LIST_SHOW_MODES
					      modebuf,
#endif
					      (chptr->topic ? chptr->topic :
					      ""));
}
			  }
		}		/* switch */
	}			/* while */

	if (doall)
	{
		lopt = sptr->user->lopt = (LOpts *) MyMalloc(sizeof(LOpts));
		memset(lopt, '\0', sizeof(LOpts));
		lopt->usermin = usermin;
		lopt->usermax = usermax;
		lopt->topictimemax = topictimemax;
		lopt->topictimemin = topictimemin;
		lopt->chantimemax = chantimemax;
		lopt->chantimemin = chantimemin;
		lopt->nolist = nolist;
		lopt->yeslist = yeslist;

		if (DBufLength(&cptr->sendQ) < 2048)
			send_list(cptr, 64);
		return 0;
	}

	sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);

	return 0;
}
