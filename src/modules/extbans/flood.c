/* 
 * IRC - Internet Relay Chat, src/modules/extbans/flood.c
 * Extended ban to exempt from +f/+F checking.
 * Eg: +e ~flood:*:~account:TrustedBot
 * (C) Copyright 2023-.. Bram Matthys (Syzop) and the UnrealIRCd team
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
	"extbans/flood",
	"1.0",
	"Extban ~flood - exempt from +f/+F checks",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/** Maximum length of the ~flood ban exemption */
#define MAX_FLOODBAN_LENGTH 128

/* Forward declarations */
int extban_flood_is_banned(BanContext *b);
int flood_extban_is_ok(BanContext *b);
const char *flood_extban_conv_param(BanContext *b, Extban *extban);

/** Called upon module init */
MOD_INIT()
{
	ExtbanInfo req;

	memset(&req, 0, sizeof(req));
	req.letter = 'F';
	req.name = "flood";
	req.is_ok = flood_extban_is_ok;
	req.conv_param = flood_extban_conv_param;
	req.options = EXTBOPT_ACTMODIFIER;
	if (!ExtbanAdd(modinfo->handle, req))
	{
		config_error("could not register extended ban type 'flood'");
		return MOD_FAILED;
	}

	MARK_AS_OFFICIAL_MODULE(modinfo);

	return MOD_SUCCESS;
}

/** Called upon module load */
MOD_LOAD()
{
	return MOD_SUCCESS;
}

/** Called upon unload */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/** Check if letters in 'str' are valid flood-types.
 * TODO: ideally this would call a function in chanmode +F module!!
 */
static int flood_type_ok(char *str)
{
	char *p;

	/* The * (asterisk) simply means ALL. */
	if (!strcmp(str, "*"))
		return 1;

	for (p = str; *p; p++)
		if (!strchr("cjkmntr", *p))
			return 0;

	return 1;
}

const char *flood_extban_conv_param(BanContext *b, Extban *extban)
{
	static char retbuf[MAX_FLOODBAN_LENGTH+1];
	char para[MAX_FLOODBAN_LENGTH+1];
	char tmpmask[MAX_FLOODBAN_LENGTH+1];
	char *type; /**< Type(s), such as 'j' */
	char *matchby; /**< Matching method, such as 'n!u@h' */
	const char *newmask; /**< Cleaned matching method, such as 'n!u@h' */

	strlcpy(para, b->banstr, sizeof(para)); /* work on a copy (and truncate it) */

	/* ~flood:type:n!u@h   for direct matching
	 * ~flood:type:~x:.... when calling another bantype
	 */

	type = para;
	matchby = strchr(para, ':');
	if (!matchby || !matchby[1])
		return NULL;
	*matchby++ = '\0';

	/* don't verify type(s), already done in is_ok for local clients */
	//if (!flood_type_ok(type))
	//	return NULL;
	/* ... but limit them to a reasonable value :D */
	if (strlen(type) > 16)
		return NULL;

	b->banstr = matchby;
	newmask = extban_conv_param_nuh_or_extban(b, extban);
	if (BadPtr(newmask))
		return NULL;

	snprintf(retbuf, sizeof(retbuf), "%s:%s", type, newmask);
	return retbuf;
}

int flood_extban_syntax(Client *client, int checkt, char *reason)
{
	if (MyUser(client) && (checkt == EXBCHK_PARAM))
	{
		sendnotice(client, "Error when setting ban exception: %s", reason);
		sendnotice(client, " Syntax: +e ~flood:floodtype(s):mask");
		sendnotice(client, "Example: +e ~flood:*:~account:TrustedUser");
		sendnotice(client, "Valid flood types are: c, j, k, m, n, t, r, and * for all");
		sendnotice(client, "Valid masks are: nick!user@host or another extban type such as ~account, ~certfp, etc.");
	}
	return 0; /* FAIL: ban rejected */
}

int flood_extban_is_ok(BanContext *b)
{
	static char para[MAX_FLOODBAN_LENGTH+1];
	char *type; /**< Type(s), such as 'j' */
	char *matchby; /**< Matching method, such as 'n!u@h' */
	char *newmask; /**< Cleaned matching method, such as 'n!u@h' */

	/* Always permit deletion */
	if (b->what == MODE_DEL)
		return 1;

	if (b->ban_type != EXBTYPE_EXCEPT)
	{
		if (b->is_ok_check == EXBCHK_PARAM)
			sendnotice(b->client, "Ban type ~flood only works with exceptions (+e) and not with bans or invex (+b/+I)");
		return 0; /* reject */
	}

	strlcpy(para, b->banstr, sizeof(para)); /* work on a copy (and truncate it) */

	/* ~flood:type:n!u@h   for direct matching
	 * ~flood:type:~x:.... when calling another bantype
	 */

	type = para;
	matchby = strchr(para, ':');
	if (!matchby || !matchby[1])
		return flood_extban_syntax(b->client, b->is_ok_check, "Invalid syntax");
	*matchby++ = '\0';

	if (!flood_type_ok(type))
		return flood_extban_syntax(b->client, b->is_ok_check, "Unknown flood type");
	if (strlen(type) > 16)
		return flood_extban_syntax(b->client, b->is_ok_check, "Too many flood types specified");

	b->banstr = matchby;
	if (extban_is_ok_nuh_extban(b) == 0)
	{
		/* This could be anything ranging from:
		 * invalid n!u@h syntax, unknown (sub)extbantype,
		 * disabled extban type in conf, too much recursion, etc.
		 */
		return flood_extban_syntax(b->client, b->is_ok_check, "Invalid matcher");
	}

	return 1; /* OK */
}
