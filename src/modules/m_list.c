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

#include "unrealircd.h"

CMD_FUNC(m_list);
void _send_list(aClient *cptr);

#define MSG_LIST 	"LIST"	

ModuleHeader MOD_HEADER(m_list)
  = {
	"m_list",
	"4.0",
	"command /list", 
	"3.2-b8-1",
	NULL 
    };

EVENT(send_queued_list_data);

MOD_TEST(m_list)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_LIST, _send_list);
	return MOD_SUCCESS;
}

MOD_INIT(m_list)
{
	CommandAdd(modinfo->handle, MSG_LIST, m_list, MAXPARA, M_USER);
	EventAddEx(modinfo->handle, "send_queued_list_data", 1, 0, send_queued_list_data, NULL);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_list)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_list)
{
	return MOD_SUCCESS;
}

/* Originally from bahamut, modified a bit for Unreal by codemastr
 * also Opers can now see +s channels and can still use /list while
 * HTM is active -- codemastr */

/*
 * parv[1] = channel
 */
CMD_FUNC(m_list)
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
		sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, sptr->name);
		free_str_list(sptr->user->lopt->yeslist);
		free_str_list(sptr->user->lopt->nolist);
		MyFree(sptr->user->lopt);
		sptr->user->lopt = NULL;
		return 0;
	}

	if (parc < 2 || BadPtr(parv[1]))
	{

		sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, sptr->name);
		lopt = sptr->user->lopt = (LOpts *) MyMalloc(sizeof(LOpts));
		memset(lopt, '\0', sizeof(LOpts));

		lopt->showall = 1;

		if (DBufLength(&cptr->local->sendQ) < 2048)
			send_list(cptr);

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

	sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, sptr->name);

	chantimemax = topictimemax = currenttime + 86400;
	chantimemin = topictimemin = 0;
	usermin = 0;		/* Minimum of 0 */
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
				  if (chptr && (ShowChannel(sptr, chptr) || ValidatePermissionsForPath("override:see:list:secret",sptr,NULL,chptr,NULL))) {
#ifdef LIST_SHOW_MODES
					modebuf[0] = '[';
					channel_modes(sptr, modebuf+1, parabuf, sizeof(modebuf)-1, sizeof(parabuf), chptr);
					if (modebuf[2] == '\0')
						modebuf[0] = '\0';
					else
						strlcat(modebuf, "]", sizeof modebuf);
#endif
					  sendto_one(sptr,
					      rpl_str(RPL_LIST),
					      me.name, sptr->name,
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

		if (DBufLength(&cptr->local->sendQ) < 2048)
			send_list(cptr);
		return 0;
	}

	sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, sptr->name);

	return 0;
}
/*
 * The function which sends the actual channel list back to the user.
 * Operates by stepping through the hashtable, sending the entries back if
 * they match the criteria.
 * cptr = Local client to send the output back to.
 * Taken from bahamut, modified for Unreal by codemastr.
 */
void _send_list(aClient *cptr)
{
	aChannel *chptr;
	LOpts *lopt = cptr->user->lopt;
	unsigned int  hashnum;
	int numsend = (get_sendq(cptr) / 768) + 1; /* (was previously hard-coded) */
	/* ^
	 * numsend = Number (roughly) of lines to send back. Once this number has
	 * been exceeded, send_list will finish with the current hash bucket,
	 * and record that number as the number to start next time send_list
	 * is called for this user. So, this function will almost always send
	 * back more lines than specified by numsend (though not by much,
	 * assuming CH_MAX is was well picked). So be conservative in your choice
	 * of numsend. -Rak
	 */	

	/* Begin of /list? then send official channels. */
	if ((lopt->starthash == 0) && conf_offchans)
	{
		ConfigItem_offchans *x;
		for (x = conf_offchans; x; x = (ConfigItem_offchans *)x->next)
		{
			if (find_channel(x->chname, (aChannel *)NULL))
				continue; /* exists, >0 users.. will be sent later */
			sendto_one(cptr,
			    rpl_str(RPL_LIST), me.name,
			    cptr->name, x->chname,
			    0,
#ifdef LIST_SHOW_MODES
			    "",
#endif					    
			    x->topic ? x->topic : "");
		}
	}

	for (hashnum = lopt->starthash; hashnum < CH_MAX; hashnum++)
	{
		if (numsend > 0)
			for (chptr =
			    (aChannel *)hash_get_chan_bucket(hashnum);
			    chptr; chptr = chptr->hnextch)
			{
				if (SecretChannel(chptr)
				    && !IsMember(cptr, chptr)
				    && !ValidatePermissionsForPath("override:see:list:secret",cptr,NULL,chptr,NULL))
					continue;

				/* Much more readable like this -- codemastr */
				if ((!lopt->showall))
				{
					/* User count must be in range */
					if ((chptr->users < lopt->usermin) || 
					    ((lopt->usermax >= 0) && (chptr->users > 
					    lopt->usermax)))
						continue;

					/* Creation time must be in range */
					if ((chptr->creationtime && (chptr->creationtime <
					    lopt->chantimemin)) || (chptr->creationtime >
					    lopt->chantimemax))
						continue;

					/* Topic time must be in range */
					if ((chptr->topic_time < lopt->topictimemin) ||
					    (chptr->topic_time > lopt->topictimemax))
						continue;

					/* Must not be on nolist (if it exists) */
					if (lopt->nolist && find_str_match_link(lopt->nolist,
					    chptr->chname))
						continue;

					/* Must be on yeslist (if it exists) */
					if (lopt->yeslist && !find_str_match_link(lopt->yeslist,
					    chptr->chname))
						continue;
				}
#ifdef LIST_SHOW_MODES
				modebuf[0] = '[';
				channel_modes(cptr, modebuf+1, parabuf, sizeof(modebuf)-1, sizeof(parabuf), chptr);
				if (modebuf[2] == '\0')
					modebuf[0] = '\0';
				else
					strlcat(modebuf, "]", sizeof modebuf);
#endif
				if (!ValidatePermissionsForPath("override:see:list:secret",cptr,NULL,chptr,NULL))
					sendto_one(cptr,
					    rpl_str(RPL_LIST), me.name,
					    cptr->name,
					    ShowChannel(cptr,
					    chptr) ? chptr->chname :
					    "*", chptr->users,
#ifdef LIST_SHOW_MODES
					    ShowChannel(cptr, chptr) ?
					    modebuf : "",
#endif
					    ShowChannel(cptr,
					    chptr) ? (chptr->topic ?
					    chptr->topic : "") : "");
				else
					sendto_one(cptr,
					    rpl_str(RPL_LIST), me.name,
					    cptr->name, chptr->chname,
					    chptr->users,
#ifdef LIST_SHOW_MODES
					    modebuf,
#endif					    
					    (chptr->topic ? chptr->topic : ""));
				numsend--;
			}
		else
			break;
	}

	/* All done */
	if (hashnum == CH_MAX)
	{
		sendto_one(cptr, rpl_str(RPL_LISTEND), me.name, cptr->name);
		free_str_list(cptr->user->lopt->yeslist);
		free_str_list(cptr->user->lopt->nolist);
		MyFree(cptr->user->lopt);
		cptr->user->lopt = NULL;
		return;
	}

	/* 
	 * We've exceeded the limit on the number of channels to send back
	 * at once.
	 */
	lopt->starthash = hashnum;
	return;
}

EVENT(send_queued_list_data)
{
	aClient *acptr, *saved;
	list_for_each_entry_safe(acptr, saved, &lclient_list, lclient_node)
	{
		if (DoList(acptr) && IsSendable(acptr))
			send_list(acptr);
	}
}
