/************************************************************************
 *   UnrealIRCd - Unreal Internet Relay Chat Daemon - src/api-isupport.c
 *   (c) 2004 Dominick Meglio and the UnrealIRCd Team
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
 *
 * $Id$
 */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "version.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include "h.h"
#include "proto.h"

Isupport *Isupports; /* List of ISUPPORT (005) tokens */
#define MAXISUPPORTLINES 10

MODVAR char *IsupportStrings[MAXISUPPORTLINES+1];
extern char *cmdstr;

/**
 * Builds isupport token strings.
 * Respects both the 13 token limit and the 512 buffer limit.
 */
/* TODO: is all this code really safe? */
void make_isupportstrings(void)
{
	int i;
	int bufsize = BUFSIZE-HOSTLEN-NICKLEN-39;
	int tokcnt = 0, len = 0;
	Isupport *isupport;

	/* Clear out the old junk */
	for (i = 0; IsupportStrings[i]; i++)
	{
		safefree(IsupportStrings[i]);
	}

	i = 0;
	IsupportStrings[i] = MyMallocEx(bufsize);
	for (isupport = Isupports; isupport; isupport = isupport->next)
	{
		int toklen;
		/* Just a token */
		if (!isupport->value)
		{
			toklen = strlen(isupport->token);
			if (tokcnt == 13 || bufsize < len+toklen+1)
			{
				tokcnt = 0;
				len = 0;
				IsupportStrings[++i] = MyMallocEx(bufsize); 
			}
			if (IsupportStrings[i][0]) toklen++;
			ircsnprintf(IsupportStrings[i]+len, bufsize-len, "%s%s", IsupportStrings[i][0]? " ": "", isupport->token);
			len += toklen;
			tokcnt++;
		}
		else
		{
			toklen = strlen(isupport->token)+strlen(isupport->value)+1;
			if (tokcnt == 13 || bufsize < len+toklen+1) {
				tokcnt = 0;
				len = 0;
				IsupportStrings[++i] = MyMallocEx(bufsize);
			}
			if (IsupportStrings[i][0]) toklen++;
			ircsnprintf(IsupportStrings[i]+len, bufsize-len, "%s%s=%s", IsupportStrings[i][0]? " ": "", isupport->token, isupport->value);
			len += toklen;
			tokcnt++;
		}
		if (i == MAXISUPPORTLINES)
			abort(); /* should never happen anyway */
	}
}

/**
 * Initializes the builtin isupport tokens.
 */
void isupport_init(void)
{
	char tmpbuf[512];
	int i;

	for (i=0; i <= MAXISUPPORTLINES; i++)
		IsupportStrings[i] = NULL;

	IsupportAdd(NULL, "INVEX", NULL);
	IsupportAdd(NULL, "EXCEPTS", NULL);
#ifdef PREFIX_AQ
	IsupportAdd(NULL, "STATUSMSG", "~&@%+");
#else
	IsupportAdd(NULL, "STATUSMSG", "@%+");
#endif
	IsupportAdd(NULL, "ELIST", "MNUCT");
	ircsnprintf(tmpbuf, sizeof(tmpbuf), "~,%s", extbanstr);
	IsupportAdd(NULL, "EXTBAN", tmpbuf);
	IsupportAdd(NULL, "CASEMAPPING", "ascii");
	IsupportAdd(NULL, "NETWORK", ircnet005);
	ircsnprintf(tmpbuf, sizeof(tmpbuf), CHPAR1 "%s," CHPAR2 "%s," CHPAR3 "%s," CHPAR4 "%s",
 			EXPAR1, EXPAR2, EXPAR3, EXPAR4);
	IsupportAdd(NULL, "CHANMODES", tmpbuf);
	IsupportAdd(NULL, "PREFIX", CHPFIX);
	IsupportAdd(NULL, "CHANTYPES", "#");
	IsupportAdd(NULL, "MODES", my_itoa(MAXMODEPARAMS));
	IsupportAdd(NULL, "SILENCE", my_itoa(SILENCE_LIMIT));
	if (WATCH_AWAY_NOTIFICATION)
		IsupportAdd(NULL, "WATCHOPTS", "A");
	IsupportAdd(NULL, "WATCH", my_itoa(MAXWATCH));
	IsupportAdd(NULL, "WALLCHOPS", NULL);
	IsupportAdd(NULL, "MAXTARGETS", my_itoa(MAXTARGETS));
	IsupportAdd(NULL, "AWAYLEN", my_itoa(TOPICLEN));
	IsupportAdd(NULL, "KICKLEN", my_itoa(TOPICLEN));
	IsupportAdd(NULL, "TOPICLEN", my_itoa(TOPICLEN));
	IsupportAdd(NULL, "CHANNELLEN", my_itoa(CHANNELLEN));
	IsupportAdd(NULL, "NICKLEN", my_itoa(iConf.nicklen));
	IsupportAdd(NULL, "MAXNICKLEN", my_itoa(NICKLEN));
	ircsnprintf(tmpbuf, sizeof(tmpbuf), "b:%d,e:%d,I:%d", MAXBANS, MAXBANS, MAXBANS);
	IsupportAdd(NULL, "MAXLIST", tmpbuf);
	ircsnprintf(tmpbuf, sizeof(tmpbuf), "#:%d", MAXCHANNELSPERUSER);
	IsupportAdd(NULL, "CHANLIMIT", tmpbuf);
	IsupportAdd(NULL, "MAXCHANNELS", my_itoa(MAXCHANNELSPERUSER));
	IsupportAdd(NULL, "HCN", NULL);
	IsupportAdd(NULL, "SAFELIST", NULL);
	IsupportAdd(NULL, "NAMESX", NULL);
	if (UHNAMES_ENABLED)
		IsupportAdd(NULL, "UHNAMES", NULL);
	if (cmdstr)
		IsupportAdd(NULL, "CMDS", cmdstr);
}

/**
 * Sets or changes the value of an existing isupport token.
 *
 * @param isupport The pointer to the isupport handle.
 * @param value    The new value of the token (NULL indicates no value).
 */
void IsupportSetValue(Isupport *isupport, const char *value)
{
	if (isupport->value)
		free(isupport->value);
	if (value)
		isupport->value = strdup(value);
	else
		isupport->value = NULL;

	make_isupportstrings();
}

/**
 * Returns an isupport handle based on the given token name.
 *
 * @param token The isupport token to search for.
 * @return Returns the handle to the isupport token if it was found,
 *         otherwise NULL is returned.
 */
Isupport *IsupportFind(const char *token)
{
	Isupport *isupport;

	for (isupport = Isupports; isupport; isupport = isupport->next)
	{
		if (!stricmp(token, isupport->token))
			return isupport;
	}
	return NULL;
}

/**
 * Adds a new isupport token.
 *
 * @param module The module which owns this token.
 * @param token  The name of the token to create.
 * @param value  The value of the token (NULL indicates no value).
 * @return Returns the handle to the new token if successful, otherwise NULL.
 *         The module's error code contains specific information about the
 *         error.
 */
Isupport *IsupportAdd(Module *module, const char *token, const char *value)
{
	Isupport *isupport;
	char *c;

	if (IsupportFind(token))
	{
		if (module)
			module->errorcode = MODERR_EXISTS;
		return NULL;
	}
	/* draft-brocklesby-irc-isupport:
	 * token = a-zA-Z0-9 and 20 or less characters
	 * value = ASCII 0x21 - 0x7E
         */
	for (c = (char *)token; c && *c; c++)
	{
		if (!isalnum(*c))
		{
			if (module)
				module->errorcode = MODERR_INVALID;
			return NULL;
		}
	}
	if (!token || !*token || c-token > 20)
	{
		if (module)
			module->errorcode = MODERR_INVALID;
		return NULL;
	}
	for (c = (char *)value; c && *c; c++)
	{
		if (*c < '!' || *c > '~')
		{
			if (module)
				module->errorcode = MODERR_INVALID;
			return NULL;
		}
	}

	isupport = MyMallocEx(sizeof(Isupport));
	isupport->owner = module;
	isupport->token = strdup(token);
	if (value)
		isupport->value = strdup(value);
	AddListItem(isupport, Isupports);
	make_isupportstrings(); 
	if (module)
	{
                ModuleObject *isupportobj = MyMallocEx(sizeof(ModuleObject));
                isupportobj->object.isupport = isupport;
                isupportobj->type = MOBJ_ISUPPORT;
                AddListItem(isupportobj, module->objects);
                module->errorcode = MODERR_NOERROR;
	}
	return isupport;
}

/**
 * Removes the specified isupport token.
 *
 * @param isupport The token to remove.
 */
void IsupportDel(Isupport *isupport)
{
	DelListItem(isupport, Isupports);
	free(isupport->token);
	if (isupport->value)
		free(isupport->value);
	free(isupport);
	make_isupportstrings();
}
