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

ModuleHeader MOD_HEADER
  = {
	"message-tags",
	"5.0",
	"Message tags CAP", 
	"UnrealIRCd Team",
	"unrealircd-6",
	};

long CAP_MESSAGE_TAGS = 0L;
const char *_mtags_to_string(MessageTag *m, Client *client);
void _parse_message_tags(Client *client, char **str, MessageTag **mtag_list);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	EfunctionAddConstString(modinfo->handle, EFUNC_MTAGS_TO_STRING, _mtags_to_string);
	EfunctionAddVoid(modinfo->handle, EFUNC_PARSE_MESSAGE_TAGS, _parse_message_tags);

	return 0;
}

MOD_INIT()
{
	ClientCapabilityInfo cap;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&cap, 0, sizeof(cap));
	cap.name = "message-tags";
	cap.flags = CLICAP_FLAGS_AFFECTS_MTAGS; /* needed explicitly */
	ClientCapabilityAdd(modinfo->handle, &cap, &CAP_MESSAGE_TAGS);
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

/** Unescape a message tag (name or value).
 * @param in  The input string
 * @param out The output string for writing
 * @note  No size checking, so ensure that the output buffer
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
			else if (*in == '\0')
				break; /* unfinished escaping (\) */
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
 * @note  No size checking, so ensure that the output buffer
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
int message_tag_ok(Client *client, char *name, char *value)
{
	MessageTagHandler *m;

	m = MessageTagHandlerFind(name);
	if (!m)
	{
		/* Permit unknown message tags from trusted servers */
		if (IsServer(client) || !MyConnect(client))
			return 1;

		return 0;
	}

	if (m->is_ok(client, name, value))
		return 1;

	return 0;
}

void _parse_message_tags(Client *client, char **str, MessageTag **mtag_list)
{
	char *remainder;
	char *element, *p, *x;
	static char name[8192], value[8192];
	MessageTag *m;

	remainder = strchr(*str, ' ');
	if (remainder)
		*remainder = '\0';

	if (!IsServer(client) && (strlen(*str) > 4094))
	{
		sendnumeric(client, ERR_INPUTTOOLONG);
		remainder = NULL; /* stop parsing */
	}

	if (!remainder)
	{
		/* A message with only message tags (or starting with @ anyway).
		 * This is useless. So we make it point to the NUL byte,
		 * aka: empty message.
		 * This is also used by a line-length-check above to force the
		 * same error condition ("don't parse this").
		 */
		for (; **str; *str += 1);
		return;
	}

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

		/* Let the message tag handler check if this mtag is
		 * acceptable. If so, we add it to the list.
		 */
		if (message_tag_ok(client, name, value))
		{
			m = safe_alloc(sizeof(MessageTag));
			safe_strdup(m->name, name);
			/* Both NULL and empty become NULL: */
			if (!*value)
				m->value = NULL;
			else /* a real value... */
				safe_strdup(m->value, value);
			AddListItem(m, *mtag_list);
		}
	}

	*str = remainder + 1;
}

/** Outgoing filter for tags */
int client_accepts_tag(const char *token, Client *client)
{
	MessageTagHandler *m;

	/* Send all tags to remote links, without checking here.
	 * Note that mtags_to_string() already prevents sending messages
	 * with message tags to links without PROTOCTL MTAGS, so we can
	 * simply always return 1 here, regardless of checking (again).
	 */
	if (IsServer(client) || !MyConnect(client))
		return 1;

	m = MessageTagHandlerFind(token);
	if (!m)
		return 0;

	/* Maybe there is an outgoing filter in effect (usually not) */
	if (m->should_send_to_client && !m->should_send_to_client(client))
		return 0;

	/* If the client has indicated 'message-tags' support then we can
	 * send any message tag, regardless of other CAP's.
	 */
	if (HasCapability(client, "message-tags"))
		return 1;

	/* We continue here if the client did not indicate 'message-tags' support... */

	/* If 'message-tags' is not indicated, then these cannot be sent as they don't
	 * have a CAP to enable anyway (eg: msgid):
	 */
	if (m->flags & MTAG_HANDLER_FLAGS_NO_CAP_NEEDED)
		return 0;

	/* Otherwise, check if the capability is set:
	 * eg 'account-tag' for 'account', 'time' for 'server-time' and so on..
	 */
	if (m->clicap_handler && (client->local->caps & m->clicap_handler->cap))
		return 1;

	return 0;
}

/** Return the message tag string (without @) of the message tag linked list.
 * Taking into account the restrictions that 'client' may have.
 * @returns A string (static buffer) or NULL if no tags at all (!)
 */
const char *_mtags_to_string(MessageTag *m, Client *client)
{
	static char buf[4096], name[8192], value[8192];
	static char tbuf[4094];

	if (!m)
		return NULL;

	/* Remote servers need to indicate support via PROTOCTL MTAGS */
	if (client->direction && IsServer(client->direction) && !SupportMTAGS(client->direction))
		return NULL;

	*buf = '\0';
	for (; m; m = m->next)
	{
		if (!client_accepts_tag(m->name, client))
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
