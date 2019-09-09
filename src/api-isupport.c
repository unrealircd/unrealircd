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
 */

#include "unrealircd.h"

Isupport *Isupports; /* List of ISUPPORT (005) tokens */
#define MAXISUPPORTLINES 10

MODVAR char *IsupportStrings[MAXISUPPORTLINES+1];

void isupport_add_sorted(Isupport *is);
void make_isupportstrings(void);

/** Easier way to set a 005 name or name=value.
 * @param name   Name of the 005 token
 * @param value  Value of the 005 token (or NULL)
 * @note  The 'name' 005 token will be overwritten if it already exists.
 *        The 'value' may be NULL, in which case if there was a value
 *        it will be unset.
 */
void IsupportSet(Module *module, const char *name, const char *value)
{
	Isupport *is = IsupportFind(name);
	if (!is)
		is = IsupportAdd(module, name, value);
	IsupportSetValue(is, value);
}

/** Easy way to set a 005 name=value with printf style formatting.
 * @param name     Name of the 005 token
 * @param pattern  Value pattern for the 005 token (or NULL)
 * @param ...      Any variables needed for 'pattern'.
 * @note  The 'name' 005 token will be overwritten if it already exists.
 *        The 'pattern' may be NULL, in which case if there was a value
 *        it will be unset.
 */
void IsupportSetFmt(Module *module, const char *name, const char *pattern, ...)
{
	const char *value = NULL;
	char buf[256];
	va_list vl;

	if (pattern)
	{
		va_start(vl, pattern);
		ircvsnprintf(buf, sizeof(buf), pattern, vl);
		va_end(vl);
		value = buf;
	}
	IsupportSet(module, name, value);
}

void IsupportDelByName(const char *name)
{
	Isupport *is = IsupportFind(name);
	if (is)
		IsupportDel(is);
}

extern void set_isupport_extban(void);
extern void set_isupport_targmax(void);

/**
 * Initializes the builtin isupport tokens.
 */
void isupport_init(void)
{
	IsupportSet(NULL, "INVEX", NULL);
	IsupportSet(NULL, "EXCEPTS", NULL);
#ifdef PREFIX_AQ
	IsupportSet(NULL, "STATUSMSG", "~&@%+");
#else
	IsupportSet(NULL, "STATUSMSG", "@%+");
#endif
	IsupportSet(NULL, "ELIST", "MNUCT");
	IsupportSet(NULL, "CASEMAPPING", "ascii");
	IsupportSet(NULL, "NETWORK", ircnet005);
	IsupportSetFmt(NULL, "CHANMODES",
	               CHPAR1 "%s," CHPAR2 "%s," CHPAR3 "%s," CHPAR4 "%s",
	               EXPAR1, EXPAR2, EXPAR3, EXPAR4);
	IsupportSet(NULL, "PREFIX", CHPFIX);
	IsupportSet(NULL, "CHANTYPES", "#");
	IsupportSetFmt(NULL, "MODES", "%d", MAXMODEPARAMS);
	IsupportSetFmt(NULL, "SILENCE", "%d", SILENCE_LIMIT);
	if (WATCH_AWAY_NOTIFICATION)
		IsupportSet(NULL, "WATCHOPTS", "A");
	else
		IsupportDelByName("WATCHOPTS");
	IsupportSetFmt(NULL, "WATCH", "%d", MAXWATCH);
	IsupportSet(NULL, "WALLCHOPS", NULL);
	IsupportSetFmt(NULL, "AWAYLEN", "%d", iConf.away_length);
	IsupportSetFmt(NULL, "KICKLEN", "%d", iConf.kick_length);
	IsupportSetFmt(NULL, "TOPICLEN", "%d", iConf.topic_length);
	IsupportSetFmt(NULL, "QUITLEN", "%d", iConf.quit_length);
	IsupportSetFmt(NULL, "CHANNELLEN", "%d", CHANNELLEN);
	IsupportSetFmt(NULL, "MINNICKLEN", "%d", iConf.min_nick_length);
	IsupportSetFmt(NULL, "NICKLEN", "%d", iConf.nick_length);
	IsupportSetFmt(NULL, "MAXNICKLEN", "%d", NICKLEN);
	IsupportSetFmt(NULL, "MAXLIST", "b:%d,e:%d,I:%d", MAXBANS, MAXBANS, MAXBANS);
	IsupportSetFmt(NULL, "CHANLIMIT", "#:%d", MAXCHANNELSPERUSER);
	IsupportSetFmt(NULL, "MAXCHANNELS", "%d", MAXCHANNELSPERUSER);
	IsupportSet(NULL, "HCN", NULL);
	IsupportSet(NULL, "SAFELIST", NULL);
	IsupportSet(NULL, "NAMESX", NULL);
	if (UHNAMES_ENABLED)
		IsupportSet(NULL, "UHNAMES", NULL);
	else
		IsupportDelByName("UHNAMES");
	IsupportSet(NULL, "DEAF", "d");
	set_isupport_extban(); /* EXTBAN=xyz */
	set_isupport_targmax(); /* TARGMAX=... */
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
		if (!strcasecmp(token, isupport->token))
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
	const char *c;

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
	for (c = token; c && *c; c++)
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
	for (c = value; c && *c; c++)
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
	isupport_add_sorted(isupport);
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

/**
 * Builds isupport token strings.
 * Respects both the 13 token limit and the 512 buffer limit.
 */
void make_isupportstrings(void)
{
	int i;
#define ISUPPORTLEN BUFSIZE-HOSTLEN-NICKLEN-39
	int bufsize = ISUPPORTLEN;
	int tokcnt = 0, len = 0;
	Isupport *isupport;
	char tmp[ISUPPORTLEN];

	/* Free any previous strings */
	for (i = 0; IsupportStrings[i]; i++)
		safefree(IsupportStrings[i]);

	i = 0;
	IsupportStrings[i] = MyMallocEx(bufsize+1);

	for (isupport = Isupports; isupport; isupport = isupport->next)
	{
		if (isupport->value)
			snprintf(tmp, sizeof(tmp), "%s=%s", isupport->token, isupport->value);
		else
			strlcpy(tmp, isupport->token, sizeof(tmp));

		tokcnt++;
		if ((strlen(IsupportStrings[i]) + strlen(tmp) + 1 >= ISUPPORTLEN) || (tokcnt == 13))
		{
			/* No room or max tokens reached: start a new buffer */
			IsupportStrings[++i] = MyMallocEx(bufsize+1);
			tokcnt = 1;
			if (i == MAXISUPPORTLINES)
				abort(); /* should never happen anyway */
		}

		if (*IsupportStrings[i])
			strlcat(IsupportStrings[i], " ", ISUPPORTLEN);
		strlcat(IsupportStrings[i], tmp, ISUPPORTLEN);
	}
}

void isupport_add_sorted(Isupport *n)
{
	Isupport *e;

	if (!Isupports)
	{
		Isupports = n;
		return;
	}

	for (e = Isupports; e; e = e->next)
	{
		if (strcmp(n->token, e->token) < 0)
		{
			/* Insert us before */
			if (e->prev)
				e->prev->next = n;
			else
				Isupports = n; /* new head */
			n->prev = e->prev;

			n->next = e;
			e->prev = n;
			return;
		}
		if (!e->next)
		{
			/* Append us at end */
			e->next = n;
			n->prev = e;
			return;
		}
	}
}
