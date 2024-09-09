/*
 * Extended ban that allows user to bypass message restrictions
 * (C) Copyright 2017-.. Bram Matthys (Syzop) and the UnrealIRCd team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"extbans/msgbypass",
	"4.2",
	"ExtBan ~m - bypass +m/+n/+c/+S/+T (msgbypass)",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
int msgbypass_can_bypass(Client *client, Channel *channel, BypassChannelMessageRestrictionType bypass_type);
int msgbypass_extban_is_ok(BanContext *b);
const char *msgbypass_extban_conv_param(BanContext *b, Extban *extban);

/** Called upon module init */
MOD_INIT()
{
	ExtbanInfo req;
	
	memset(&req, 0, sizeof(req));
	req.letter = 'm';
	req.name = "msgbypass";
	req.is_ok = msgbypass_extban_is_ok;
	req.conv_param = msgbypass_extban_conv_param;
	req.options = EXTBOPT_ACTMODIFIER;
	if (!ExtbanAdd(modinfo->handle, req))
	{
		config_error("could not register extended ban type ~m");
		return MOD_FAILED;
	}

	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	return MOD_SUCCESS;
}

/** Called upon module load */
MOD_LOAD()
{
	HookAdd(modinfo->handle, HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION, 0, msgbypass_can_bypass);
	return MOD_SUCCESS;
}

/** Called upon unload */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/** Can the user bypass restrictions? */
int msgbypass_can_bypass(Client *client, Channel *channel, BypassChannelMessageRestrictionType bypass_type)
{
	Ban *ban;
	char *p;
	BanContext *b = safe_alloc(sizeof(BanContext));

	b->client = client;
	b->channel = channel;
	b->ban_check_types = BANCHK_MSG;
	b->ban_type = EXBTYPE_EXCEPT;
	for (ban = channel->exlist; ban; ban=ban->next)
	{
		char *type;
		char *matchby;

		if (!strncmp(ban->banstr, "~m:", 3))
			type = ban->banstr + 3;
		else if (!strncmp(ban->banstr, "~msgbypass:", 11))
			type = ban->banstr + 11;
		else
			continue;

		if (((bypass_type == BYPASS_CHANMSG_EXTERNAL) && !strncmp(type, "external:", 9)) ||
		    ((bypass_type == BYPASS_CHANMSG_MODERATED) && !strncmp(type, "moderated:", 10)) ||
		    ((bypass_type == BYPASS_CHANMSG_COLOR) && !strncmp(type, "color:", 6)) ||
		    ((bypass_type == BYPASS_CHANMSG_CENSOR) && !strncmp(type, "censor:", 7)) ||
		    ((bypass_type == BYPASS_CHANMSG_NOTICE) && !strncmp(type, "notice:", 7)))
		{
			matchby = strchr(type, ':');
			if (!matchby)
				continue;
			matchby++;
			
			b->banstr = matchby;
			if (ban_check_mask(b))
			{
				safe_free(b);
				return HOOK_ALLOW; /* Yes, user may bypass */
			}
		}
	}

	safe_free(b);
	return HOOK_CONTINUE; /* No, may NOT bypass. */
}

/** Does this bypass type exist? (eg: 'external') */
int msgbypass_extban_type_ok(char *type)
{
	if (!strcmp(type, "external") ||
	    !strcmp(type, "moderated") ||
	    !strcmp(type, "censor") ||
	    !strcmp(type, "color") ||
	    !strcmp(type, "notice"))
	{
		return 1; /* Yes, OK type */
	}
	return 0; /* NOMATCH */
}

#define MAX_LENGTH 128
const char *msgbypass_extban_conv_param(BanContext *b, Extban *extban)
{
	static char retbuf[MAX_LENGTH+1];
	char para[MAX_LENGTH+1];
	char tmpmask[MAX_LENGTH+1];
	char *type; /**< Type, such as 'external' */
	char *matchby; /**< Matching method, such as 'n!u@h' */
	const char *newmask; /**< Cleaned matching method, such as 'n!u@h' */

	strlcpy(para, b->banstr, sizeof(para)); /* work on a copy (and truncate it) */
	
	/* ~m:type:n!u@h   for direct matching
	 * ~m:type:~x:.... when calling another bantype
	 */

	type = para;
	matchby = strchr(para, ':');
	if (!matchby || !matchby[1])
		return NULL;
	*matchby++ = '\0';

	if (!msgbypass_extban_type_ok(type))
		return NULL;

	b->banstr = matchby;
	newmask = extban_conv_param_nuh_or_extban(b, extban);
	if (BadPtr(newmask))
		return NULL;

	//snprintf(retbuf, sizeof(retbuf), "~m:%s:%s", type, newmask);
	snprintf(retbuf, sizeof(retbuf), "%s:%s", type, newmask);
	return retbuf;
}

int msgbypass_extban_syntax(Client *client, int checkt, char *reason)
{
	if (MyUser(client) && (checkt == EXBCHK_PARAM))
	{
		sendnotice(client, "Error when setting ban exception: %s", reason);
		sendnotice(client, " Syntax: +e ~m:type:mask");
		sendnotice(client, "Example: +e ~m:moderated:~a:TrustedUser");
		sendnotice(client, "Valid types are: external, moderated, color, notice");
		sendnotice(client, "Valid masks are: nick!user@host or another extban type such as ~a, ~c, ~S, ..");
	}
	return 0; /* FAIL: ban rejected */
}

int msgbypass_extban_is_ok(BanContext *b)
{
	static char para[MAX_LENGTH+1];
	char *type; /**< Type, such as 'external' */
	char *matchby; /**< Matching method, such as 'n!u@h' */
	char *newmask; /**< Cleaned matching method, such as 'n!u@h' */

	/* Always permit deletion */
	if (b->what == MODE_DEL)
		return 1;
	
	if (b->ban_type != EXBTYPE_EXCEPT)
	{
		if (b->is_ok_check == EXBCHK_PARAM)
			sendnotice(b->client, "Ban type ~m only works with exceptions (+e) and not with bans or invex (+b/+I)");
		return 0; /* reject */
	}

	strlcpy(para, b->banstr, sizeof(para)); /* work on a copy (and truncate it) */
	
	/* ~m:type:n!u@h   for direct matching
	 * ~m:type:~x:.... when calling another bantype
	 */

	type = para;
	matchby = strchr(para, ':');
	if (!matchby || !matchby[1])
		return msgbypass_extban_syntax(b->client, b->is_ok_check, "Invalid syntax");
	*matchby++ = '\0';

	if (!msgbypass_extban_type_ok(type))
		return msgbypass_extban_syntax(b->client, b->is_ok_check, "Unknown type");

	b->banstr = matchby;
	if (extban_is_ok_nuh_extban(b) == 0)
	{
		/* This could be anything ranging from:
		 * invalid n!u@h syntax, unknown (sub)extbantype,
		 * disabled extban type in conf, too much recursion, etc.
		 */
		return msgbypass_extban_syntax(b->client, b->is_ok_check, "Invalid matcher");
	}

	return 1; /* OK */
}

