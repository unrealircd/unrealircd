/*
 *   IRC - Internet Relay Chat, src/modules/message-tags.c
 *   (C) 2019 Syzop & The UnrealIRCd Team
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

ModuleHeader MOD_HEADER(message-tags)
  = {
	"message-tags",
	"4.2",
	"Message tags CAP", 
	"3.2-b8-1",
	NULL 
	};

long CAP_MESSAGE_TAGS = 0L;
char *_mtags_to_string(MessageTag *m, aClient *acptr);
void _parse_message_tags(aClient *cptr, char **str, MessageTag **mtag_list);

MOD_TEST(message-tags)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	EfunctionAddPChar(modinfo->handle, EFUNC_MTAGS_TO_STRING, _mtags_to_string);
	EfunctionAddVoid(modinfo->handle, EFUNC_PARSE_MESSAGE_TAGS, _parse_message_tags);

	return 0;
}

MOD_INIT(message-tags)
{
	ClientCapabilityInfo cap;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&cap, 0, sizeof(cap));
	cap.name = "message-tags";
	ClientCapabilityAdd(modinfo->handle, &cap, &CAP_MESSAGE_TAGS);
	return MOD_SUCCESS;
}

MOD_LOAD(message-tags)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(message-tags)
{
	return MOD_SUCCESS;
}

/** Unescape a message tag (name or value).
 * @param in  The input string
 * @param out The output string for writing
 * @notes No size checking, so ensure that the output buffer
 *        is at least as long as the input buffer.
 */
void message_tag_unescape(char *in, char *out)
{
	for (; *in; in++)
	{
		if (*in == '\\')
		{
			in++;
			if (*in == ':')
				*out++ = ';';  /* \: to ; */
			else if (*in == 's')
				*out++ = ' ';  /* \s to SPACE */
			else if (*in == 'r')
				*out++ = '\r'; /* \r to CR */
			else if (*in == 'n')
				*out++ = '\n'; /* \n to LF */
			else
				*out++ = *in; /* all rest is as-is */
			continue;
		}
		*out++ = *in;
	}
	*out = '\0';
}

/** Escape a message tag (name or value).
 * @param in  The input string
 * @param out The output string for writing
 * @notes No size checking, so ensure that the output buffer
 *        is at least twice as long as the input buffer + 1.
 */
void message_tag_escape(char *in, char *out)
{
	for (; *in; in++)
	{
		if (*in == ';')
		{
			*out++ = '\\';
			*out++ = ':';
		} else
		if (*in == ' ')
		{
			*out++ = '\\';
			*out++ = 's';
		} else
		if (*in == '\\')
		{
			*out++ = '\\';
			*out++ = '\\';
		} else
		if (*in == '\r')
		{
			*out++ = '\\';
			*out++ = 'r';
		} else
		if (*in == '\n')
		{
			*out++ = '\\';
			*out++ = 'n';
		} else
		{
			*out++ = *in;
		}
	}
	*out = '\0';
}

/** Incoming filter for message tags */
int message_tag_ok(aClient *cptr, char *name, char *value)
{
	MessageTagHandler *m;

	if (IsServer(cptr) || IsServer(cptr->from))
		return 1; /* assume upstream filtered already */

	m = MessageTagHandlerFind(name);
	if (!m)
		return 0;

	if (m->is_ok(cptr, name, value))
		return 1;

	return 0;
}

void _parse_message_tags(aClient *cptr, char **str, MessageTag **mtag_list)
{
	char *remainder;
	char *element, *p, *x;
	static char name[8192], value[8192];
	MessageTag *m;

	remainder = strchr(*str, ' ');
	if (!remainder)
	{
		/* A message with only message tags (or starting with @ anyway).
		 * This is useless. So we make it point to the NUL byte,
		 * aka: empty message.
		 */
		for (; **str; *str += 1);
		return;
	}

	*remainder = '\0';

	/* Now actually parse the tags: */
	for (element = strtoken(&p, *str+1, ";"); element; element = strtoken(&p, NULL, ";"))
	{
		*name = *value = '\0';

		/* Element has style: 'name=value', or it could be just 'name' */
		x = strchr(element, '=');
		if (x)
		{
			*x++ = '\0';
			message_tag_unescape(x, value);
		}
		message_tag_unescape(element, name);

		/* For now, we just add the message tag.
		 * In a real implementation we would actually apply some filtering.
		 */
		if (message_tag_ok(cptr, name, value))
		{
			m = MyMallocEx(sizeof(MessageTag));
			m->name = strdup(name);
			m->value = BadPtr(value) ? NULL : strdup(value);
			AddListItem(m, *mtag_list);
		}
	}

	*str = remainder + 1;
}

/** Outgoing filter for tags */
int client_accepts_tag(const char *token, aClient *acptr)
{
	// FIXME: we blindly send to servers right now
	// but we need to take into account older servers !!!
	if (!acptr || IsServer(acptr) || !MyConnect(acptr))
		return 1;

	if (!HasCapability(acptr, "message-tags"))
		return 0; /* easy :) */

	// TODO: move to message tag API ;)
	if (!strcmp(token, "msgid"))
		return 1;

	if (!strcmp(token, "account") && HasCapability(acptr, "account-tag"))
		return 1;

	if (!strcmp(token, "time") && HasCapability(acptr, "server-time"))
		return 1;

	return 0;
}

/** Return the message tag string (without @) of the message tag linked list.
 * Taking into account the restrictions that 'acptr' may have.
 * @returns A string (static buffer) or NULL if no tags at all (!)
 */
char *_mtags_to_string(MessageTag *m, aClient *acptr)
{
	static char buf[4096], name[8192], value[8192];
	char tbuf[512];

	if (!m)
		return NULL;

	*buf = '\0';
	for (; m; m = m->next)
	{
		if (!client_accepts_tag(m->name, acptr))
			continue;
		if (m->value)
		{
			message_tag_escape(m->name, name);
			message_tag_escape(m->value, value);
			snprintf(tbuf, sizeof(tbuf), "%s=%s;", name, value);
		} else {
			message_tag_escape(m->name, name);
			snprintf(tbuf, sizeof(tbuf), "%s;", name);
		}
		strlcat(buf, tbuf, sizeof(buf));
	}

	if (!*buf)
		return NULL;

	/* Strip off the final semicolon */
	buf[strlen(buf)-1] = '\0';

	return buf;
}

