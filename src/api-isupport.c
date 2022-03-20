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

ISupport *ISupports; /* List of ISUPPORT (005) tokens */
#define MAXISUPPORTLINES 10

MODVAR char *ISupportStrings[MAXISUPPORTLINES+1];

void isupport_add_sorted(ISupport *is);
void make_isupportstrings(void);

/** Easier way to set a 005 name or name=value.
 * @param name   Name of the 005 token
 * @param value  Value of the 005 token (or NULL)
 * @note  The 'name' 005 token will be overwritten if it already exists.
 *        The 'value' may be NULL, in which case if there was a value
 *        it will be unset.
 */
void ISupportSet(Module *module, const char *name, const char *value)
{
	ISupport *is = ISupportFind(name);
	if (!is)
		is = ISupportAdd(module, name, value);
	ISupportSetValue(is, value);
}

/** Easy way to set a 005 name=value with printf style formatting.
 * @param name     Name of the 005 token
 * @param pattern  Value pattern for the 005 token (or NULL)
 * @param ...      Any variables needed for 'pattern'.
 * @note  The 'name' 005 token will be overwritten if it already exists.
 *        The 'pattern' may be NULL, in which case if there was a value
 *        it will be unset.
 */
void ISupportSetFmt(Module *module, const char *name, const char *pattern, ...)
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
	ISupportSet(module, name, value);
}

void ISupportDelByName(const char *name)
{
	ISupport *is = ISupportFind(name);
	if (is)
		ISupportDel(is);
}

extern void set_isupport_extban(void);
extern void set_isupport_targmax(void);

/**
 * Initializes the builtin isupport tokens.
 */
void isupport_init(void)
{
	ISupportSet(NULL, "INVEX", NULL);
	ISupportSet(NULL, "EXCEPTS", NULL);
#ifdef LIST_USE_T
	ISupportSet(NULL, "ELIST", "MNUCT");
#else
	ISupportSet(NULL, "ELIST", "MNUC");
#endif
	ISupportSet(NULL, "CASEMAPPING", "ascii");
	ISupportSet(NULL, "NETWORK", NETWORK_NAME_005);
	ISupportSetFmt(NULL, "CHANMODES",
	               CHPAR1 "%s,%s,%s,%s",
	               EXPAR1, EXPAR2, EXPAR3, EXPAR4);
	ISupportSet(NULL, "CHANTYPES", "#");
	ISupportSetFmt(NULL, "MODES", "%d", MAXMODEPARAMS);
	ISupportSetFmt(NULL, "SILENCE", "%d", SILENCE_LIMIT);
	if (WATCH_AWAY_NOTIFICATION)
		ISupportSet(NULL, "WATCHOPTS", "A");
	else
		ISupportDelByName("WATCHOPTS");
	ISupportSetFmt(NULL, "WATCH", "%d", MAXWATCH);
	ISupportSet(NULL, "WALLCHOPS", NULL);
	ISupportSetFmt(NULL, "AWAYLEN", "%d", iConf.away_length);
	ISupportSetFmt(NULL, "KICKLEN", "%d", iConf.kick_length);
	ISupportSetFmt(NULL, "TOPICLEN", "%d", iConf.topic_length);
	ISupportSetFmt(NULL, "QUITLEN", "%d", iConf.quit_length);
	ISupportSetFmt(NULL, "CHANNELLEN", "%d", CHANNELLEN);
	ISupportSetFmt(NULL, "MINNICKLEN", "%d", iConf.min_nick_length);
	ISupportSetFmt(NULL, "NICKLEN", "%d", iConf.nick_length);
	ISupportSetFmt(NULL, "MAXNICKLEN", "%d", NICKLEN);
	ISupportSetFmt(NULL, "MAXLIST", "b:%d,e:%d,I:%d", MAXBANS, MAXBANS, MAXBANS);
	ISupportSetFmt(NULL, "CHANLIMIT", "#:%d", MAXCHANNELSPERUSER);
	ISupportSetFmt(NULL, "MAXCHANNELS", "%d", MAXCHANNELSPERUSER);
	ISupportSet(NULL, "SAFELIST", NULL);
	ISupportSet(NULL, "NAMESX", NULL);
	if (UHNAMES_ENABLED)
		ISupportSet(NULL, "UHNAMES", NULL);
	else
		ISupportDelByName("UHNAMES");
	ISupportSet(NULL, "DEAF", "d");
	set_isupport_extban(); /* EXTBAN=xyz */
	set_isupport_targmax(); /* TARGMAX=... */
}

/**
 * Sets or changes the value of an existing isupport token.
 *
 * @param isupport The pointer to the isupport handle.
 * @param value    The new value of the token (NULL indicates no value).
 */
void ISupportSetValue(ISupport *isupport, const char *value)
{
	safe_strdup(isupport->value, value);
	make_isupportstrings();
}

/**
 * Returns an isupport handle based on the given token name.
 *
 * @param token The isupport token to search for.
 * @return Returns the handle to the isupport token if it was found,
 *         otherwise NULL is returned.
 */
ISupport *ISupportFind(const char *token)
{
	ISupport *isupport;

	for (isupport = ISupports; isupport; isupport = isupport->next)
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
ISupport *ISupportAdd(Module *module, const char *token, const char *value)
{
	ISupport *isupport;
	const char *c;

	if (ISupportFind(token))
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

	isupport = safe_alloc(sizeof(ISupport));
	isupport->owner = module;
	safe_strdup(isupport->token, token);
	if (value)
		safe_strdup(isupport->value, value);
	isupport_add_sorted(isupport);
	make_isupportstrings();
	if (module)
	{
		ModuleObject *isupportobj = safe_alloc(sizeof(ModuleObject));
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
void ISupportDel(ISupport *isupport)
{
	DelListItem(isupport, ISupports);
	if (isupport->owner)
	{
		ModuleObject *mo;
		for (mo = isupport->owner->objects; mo; mo = mo->next)
		{
			if (mo->type == MOBJ_ISUPPORT && mo->object.isupport == isupport)
			{
				DelListItem(mo, isupport->owner->objects);
				safe_free(mo);
				break;
			}
		}
	}
	safe_free(isupport->token);
	safe_free(isupport->value);
	safe_free(isupport);
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
	int tokcnt = 0;
	ISupport *isupport;
	char tmp[ISUPPORTLEN];

	/* Free any previous strings */
	for (i = 0; ISupportStrings[i]; i++)
		safe_free(ISupportStrings[i]);

	i = 0;
	ISupportStrings[i] = safe_alloc(bufsize+1);

	for (isupport = ISupports; isupport; isupport = isupport->next)
	{
		if (isupport->value)
			snprintf(tmp, sizeof(tmp), "%s=%s", isupport->token, isupport->value);
		else
			strlcpy(tmp, isupport->token, sizeof(tmp));

		tokcnt++;
		if ((strlen(ISupportStrings[i]) + strlen(tmp) + 1 >= ISUPPORTLEN) || (tokcnt == 13))
		{
			/* No room or max tokens reached: start a new buffer */
			ISupportStrings[++i] = safe_alloc(bufsize+1);
			tokcnt = 1;
			if (i == MAXISUPPORTLINES)
				abort(); /* should never happen anyway */
		}

		if (*ISupportStrings[i])
			strlcat(ISupportStrings[i], " ", ISUPPORTLEN);
		strlcat(ISupportStrings[i], tmp, ISUPPORTLEN);
	}
}

void isupport_add_sorted(ISupport *n)
{
	ISupport *e;

	if (!ISupports)
	{
		ISupports = n;
		return;
	}

	for (e = ISupports; e; e = e->next)
	{
		if (strcmp(n->token, e->token) < 0)
		{
			/* Insert us before */
			if (e->prev)
				e->prev->next = n;
			else
				ISupports = n; /* new head */
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
