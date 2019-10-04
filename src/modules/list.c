/*
 *   IRC - Internet Relay Chat, src/modules/list.c
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

CMD_FUNC(cmd_list);
void send_list(Client *cptr);

#define MSG_LIST 	"LIST"	

ModuleHeader MOD_HEADER
  = {
	"list",
	"5.0",
	"command /list", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

typedef struct ChannelListOptions ChannelListOptions;
struct ChannelListOptions {
	NameList *yeslist;
	NameList *nolist;
	unsigned int starthash;
	short int showall;
	unsigned short usermin;
	int  usermax;
	time_t currenttime;
	time_t chantimemin;
	time_t chantimemax;
	time_t topictimemin;
	time_t topictimemax;
};

/* Global variables */
ModDataInfo *list_md = NULL;

/* Macros */
#define CHANNELLISTOPTIONS(x)       ((ChannelListOptions *)moddata_local_client(x, list_md).ptr)
#define ALLOCATE_CHANNELLISTOPTIONS(cptr)	do { moddata_local_client(cptr, list_md).ptr = safe_alloc(sizeof(ChannelListOptions)); } while(0)
#define free_list_options(sptr)		list_md_free(&moddata_local_client(sptr, list_md))

#define DoList(x)               (MyUser((x)) && CHANNELLISTOPTIONS((x)))
#define IsSendable(x)		(DBufLength(&x->local->sendQ) < 2048)

/* Forward declarations */
EVENT(send_queued_list_data);
void list_md_free(ModData *md);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "list";
	mreq.type = MODDATATYPE_LOCAL_CLIENT;
	mreq.free = list_md_free;
	list_md = ModDataAdd(modinfo->handle, mreq);
	if (!list_md)
	{
		config_error("could not register list moddata");
		return MOD_FAILED;
	}

	CommandAdd(modinfo->handle, MSG_LIST, cmd_list, MAXPARA, CMD_USER);
	EventAdd(modinfo->handle, "send_queued_list_data", send_queued_list_data, NULL, 1500, 0);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/* Originally from bahamut, modified a bit for Unreal by codemastr
 * also Opers can now see +s channels -- codemastr */

/*
 * parv[1] = channel
 */
CMD_FUNC(cmd_list)
{
	Channel *chptr;
	time_t currenttime = TStime();
	char *name, *p = NULL;
	ChannelListOptions *lopt = NULL;
	int usermax, usermin, error = 0, doall = 0;
	time_t chantimemin, chantimemax;
	time_t topictimemin, topictimemax;
	NameList *yeslist = NULL;
	NameList *nolist = NULL;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("LIST");

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

	/* Remote /LIST is not supported */
	if (!MyUser(sptr))
		return;

	/* If a /LIST is in progress then a new one will cancel it */
	if (CHANNELLISTOPTIONS(sptr))
	{
		sendnumeric(sptr, RPL_LISTEND);
		free_list_options(sptr);
		return;
	}

	if (parc < 2 || BadPtr(parv[1]))
	{
		sendnumeric(sptr, RPL_LISTSTART);
		ALLOCATE_CHANNELLISTOPTIONS(sptr);
		CHANNELLISTOPTIONS(sptr)->showall = 1;

		if (DBufLength(&sptr->local->sendQ) < 2048)
			send_list(sptr);

		return;
	}

	if ((parc == 2) && (parv[1][0] == '?') && (parv[1][1] == '\0'))
	{
		char **ptr = usage;
		for (; *ptr; ptr++)
			sendnumeric(sptr, RPL_LISTSYNTAX, *ptr);
		return;
	}

	sendnumeric(sptr, RPL_LISTSTART);

	chantimemax = topictimemax = currenttime + 86400;
	chantimemin = topictimemin = 0;
	usermin = 0;		/* Minimum of 0 */
	usermax = -1;		/* No maximum */

	for (name = strtoken(&p, parv[1], ","); name && !error;
	    name = strtoken(&p, NULL, ","))
	{
		if (MyUser(sptr) && (++ntargets > maxtargets))
		{
			sendnumeric(sptr, ERR_TOOMANYTARGETS, name, maxtargets, "LIST");
			break;
		}
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
		  case 'c':	/* Channel time -- creation time? */
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
				    sendnumeric(sptr, ERR_LISTSYNTAX);
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
				    sendnumeric(sptr, ERR_LISTSYNTAX,
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
				  add_name_list(nolist, name + 1);
			  }
			  else if (strchr(name, '*') || strchr(name, '?'))
			  {
				  doall = 1;
				  add_name_list(yeslist, name);
			  }
			  else	/* Just a normal channel */
			  {
				  chptr = find_channel(name, NULL);
				  if (chptr && (ShowChannel(sptr, chptr) || ValidatePermissionsForPath("channel:see:list:secret",sptr,NULL,chptr,NULL))) {
#ifdef LIST_SHOW_MODES
					modebuf[0] = '[';
					channel_modes(sptr, modebuf+1, parabuf, sizeof(modebuf)-1, sizeof(parabuf), chptr);
					if (modebuf[2] == '\0')
						modebuf[0] = '\0';
					else
						strlcat(modebuf, "]", sizeof modebuf);
#endif
					  sendnumeric(sptr, RPL_LIST,
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
		ALLOCATE_CHANNELLISTOPTIONS(sptr);
		CHANNELLISTOPTIONS(sptr)->usermin = usermin;
		CHANNELLISTOPTIONS(sptr)->usermax = usermax;
		CHANNELLISTOPTIONS(sptr)->topictimemax = topictimemax;
		CHANNELLISTOPTIONS(sptr)->topictimemin = topictimemin;
		CHANNELLISTOPTIONS(sptr)->chantimemax = chantimemax;
		CHANNELLISTOPTIONS(sptr)->chantimemin = chantimemin;
		CHANNELLISTOPTIONS(sptr)->nolist = nolist;
		CHANNELLISTOPTIONS(sptr)->yeslist = yeslist;

		if (DBufLength(&sptr->local->sendQ) < 2048)
			send_list(sptr);
		return;
	}

	sendnumeric(sptr, RPL_LISTEND);
}
/*
 * The function which sends the actual channel list back to the user.
 * Operates by stepping through the hashtable, sending the entries back if
 * they match the criteria.
 * cptr = Local client to send the output back to.
 * Taken from bahamut, modified for Unreal by codemastr.
 */
void send_list(Client *cptr)
{
	Channel *chptr;
	ChannelListOptions *lopt = CHANNELLISTOPTIONS(cptr);
	unsigned int  hashnum;
	int numsend = (get_sendq(cptr) / 768) + 1; /* (was previously hard-coded) */
	/* ^
	 * numsend = Number (roughly) of lines to send back. Once this number has
	 * been exceeded, send_list will finish with the current hash bucket,
	 * and record that number as the number to start next time send_list
	 * is called for this user. So, this function will almost always send
	 * back more lines than specified by numsend (though not by much,
	 * assuming the hashing algorithm works well). Be conservative in your
	 * choice of numsend. -Rak
	 */	

	/* Begin of /list? then send official channels. */
	if ((lopt->starthash == 0) && conf_offchans)
	{
		ConfigItem_offchans *x;
		for (x = conf_offchans; x; x = x->next)
		{
			if (find_channel(x->chname, NULL))
				continue; /* exists, >0 users.. will be sent later */
			sendnumeric(cptr, RPL_LIST, x->chname,
			    0,
#ifdef LIST_SHOW_MODES
			    "",
#endif					    
			    x->topic ? x->topic : "");
		}
	}

	for (hashnum = lopt->starthash; hashnum < CHAN_HASH_TABLE_SIZE; hashnum++)
	{
		if (numsend > 0)
			for (chptr = hash_get_chan_bucket(hashnum);
			    chptr; chptr = chptr->hnextch)
			{
				if (SecretChannel(chptr)
				    && !IsMember(cptr, chptr)
				    && !ValidatePermissionsForPath("channel:see:list:secret",cptr,NULL,chptr,NULL))
					continue;

				/* set::hide-list { deny-channel } */
				if (!IsOper(cptr) && iConf.hide_list && Find_channel_allowed(cptr, chptr->chname))
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
					if (lopt->nolist && find_name_list_match(lopt->nolist, chptr->chname))
						continue;

					/* Must be on yeslist (if it exists) */
					if (lopt->yeslist && !find_name_list_match(lopt->yeslist, chptr->chname))
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
				if (!ValidatePermissionsForPath("channel:see:list:secret",cptr,NULL,chptr,NULL))
					sendnumeric(cptr, RPL_LIST,
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
					sendnumeric(cptr, RPL_LIST, chptr->chname,
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
	if (hashnum == CHAN_HASH_TABLE_SIZE)
	{
		sendnumeric(cptr, RPL_LISTEND);
		free_list_options(cptr);
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
	Client *acptr, *saved;
	list_for_each_entry_safe(acptr, saved, &lclient_list, lclient_node)
	{
		if (DoList(acptr) && IsSendable(acptr))
			send_list(acptr);
	}
}

/** Called on client exit: free the channel list options of this user */
void list_md_free(ModData *md)
{
	ChannelListOptions *lopt = (ChannelListOptions *)md->ptr;

	if (!lopt)
		return;

	free_entire_name_list(lopt->yeslist);
	free_entire_name_list(lopt->nolist);

	safe_free(md->ptr);
}
