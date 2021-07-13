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
	"unrealircd-5",
};

/* Forward declarations */
int extban_msgbypass_is_banned(Client *client, Channel *channel, char *banin, int type, char **msg, char **errmsg);
int msgbypass_can_bypass(Client *client, Channel *channel, BypassChannelMessageRestrictionType bypass_type);
int msgbypass_extban_is_ok(Client *client, Channel* channel, char *para, int checkt, int what, int what2);
char *msgbypass_extban_conv_param(char *para);

/** Called upon module init */
MOD_INIT()
{
	ExtbanInfo req;
	
	req.flag = 'm';
	req.is_ok = msgbypass_extban_is_ok;
	req.conv_param = msgbypass_extban_conv_param;
	req.is_banned = extban_msgbypass_is_banned;
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

/** Is the user banned? No, never by us anyway. */
int extban_msgbypass_is_banned(Client *client, Channel *channel, char *banin, int type, char **msg, char **errmsg)
{
	return 0; /* not banned by us */
}

/** Can the user bypass restrictions? */
int msgbypass_can_bypass(Client *client, Channel *channel, BypassChannelMessageRestrictionType bypass_type)
{
	Ban *ban;
	char *p;
	
	for (ban = channel->exlist; ban; ban=ban->next)
	{
		if (!strncmp(ban->banstr, "~m:", 3))
		{
			char *type = ban->banstr + 3;
			char *matchby;
			
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
				
				if (ban_check_mask(client, channel, matchby, BANCHK_MSG, NULL, NULL, 0))
					return HOOK_ALLOW; /* Yes, user may bypass */
			}
		}
	}

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
char *msgbypass_extban_conv_param(char *para_in)
{
	static char retbuf[MAX_LENGTH+1];
	char para[MAX_LENGTH+1];
	char tmpmask[MAX_LENGTH+1];
	char *type; /**< Type, such as 'external' */
	char *matchby; /**< Matching method, such as 'n!u@h' */
	char *newmask; /**< Cleaned matching method, such as 'n!u@h' */

	strlcpy(para, para_in+3, sizeof(para)); /* work on a copy (and truncate it) */
	
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

	/* This is quite silly, we have to create a fake extban here due to
	 * the current API of extban_conv_param_nuh and extban_conv_param_nuh_or_extban
	 * expecting the full banmask rather than the portion that actually matters.
	 */
	snprintf(tmpmask, sizeof(tmpmask), "~?:%s", matchby);
	newmask = extban_conv_param_nuh_or_extban(tmpmask);
	if (!newmask || (strlen(newmask) <= 3))
		return NULL;

	snprintf(retbuf, sizeof(retbuf), "~m:%s:%s", type, newmask+3);
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

int msgbypass_extban_is_ok(Client *client, Channel* channel, char *para_in, int checkt, int what, int what2)
{
	char para[MAX_LENGTH+1];
	char tmpmask[MAX_LENGTH+1];
	char *type; /**< Type, such as 'external' */
	char *matchby; /**< Matching method, such as 'n!u@h' */
	char *newmask; /**< Cleaned matching method, such as 'n!u@h' */

	/* Always permit deletion */
	if (what == MODE_DEL)
		return 1;
	
	if (what2 != EXBTYPE_EXCEPT)
	{
		if (checkt == EXBCHK_PARAM)
			sendnotice(client, "Ban type ~m only works with exceptions (+e) and not with bans or invex (+b/+I)");
		return 0; /* reject */
	}

	strlcpy(para, para_in+3, sizeof(para)); /* work on a copy (and truncate it) */
	
	/* ~m:type:n!u@h   for direct matching
	 * ~m:type:~x:.... when calling another bantype
	 */

	type = para;
	matchby = strchr(para, ':');
	if (!matchby || !matchby[1])
		return msgbypass_extban_syntax(client, checkt, "Invalid syntax");
	*matchby++ = '\0';

	if (!msgbypass_extban_type_ok(type))
		return msgbypass_extban_syntax(client, checkt, "Unknown type");

	/* This is quite silly, we have to create a fake extban here due to
	 * the current API of extban_conv_param_nuh and extban_conv_param_nuh_or_extban
	 * expecting the full banmask rather than the portion that actually matters.
	 */
	snprintf(tmpmask, sizeof(tmpmask), "~?:%s", matchby);
	if (extban_is_ok_nuh_extban(client, channel, tmpmask, checkt, what, what2) == 0)
	{
		/* This could be anything ranging from:
		 * invalid n!u@h syntax, unknown (sub)extbantype,
		 * disabled extban type in conf, too much recursion, etc.
		 */
		return msgbypass_extban_syntax(client, checkt, "Invalid matcher");
	}

	return 1; /* OK */
}

