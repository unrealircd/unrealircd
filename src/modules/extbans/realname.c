/* 
 * IRC - Internet Relay Chat, src/modules/extbans/realname.c
 * Extended ban to ban based on real name / gecos field (+b ~r)
 * (C) Copyright 2003-.. Bram Matthys (Syzop) and the UnrealIRCd team
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
	"extbans/realname",
	"4.2",
	"ExtBan ~r - Ban based on realname/gecos field",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
const char *extban_realname_conv_param(BanContext *b, Extban *extban);
int extban_realname_is_banned(BanContext *b);

Extban *register_realname_extban(ModuleInfo *modinfo)
{
	ExtbanInfo req;

	memset(&req, 0, sizeof(req));
	req.letter = 'r';
	req.name = "realname";
	req.is_ok = NULL;
	req.conv_param = extban_realname_conv_param;
	req.is_banned = extban_realname_is_banned;
	req.is_banned_events = BANCHK_ALL|BANCHK_TKL;
	req.options = EXTBOPT_INVEX|EXTBOPT_TKL;
	return ExtbanAdd(modinfo->handle, req);
}

/** Called upon module test */
MOD_TEST()
{
	if (!register_realname_extban(modinfo))
	{
		config_error("could not register extended ban type");
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

/** Called upon module init */
MOD_INIT()
{
	if (!register_realname_extban(modinfo))
	{
		config_error("could not register extended ban type");
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

/** Realname bans - conv_param */
const char *extban_realname_conv_param(BanContext *b, Extban *extban)
{
	static char retbuf[REALLEN + 8];
	char *mask;

	strlcpy(retbuf, b->banstr, sizeof(retbuf));

	mask = retbuf;

	if (!*mask)
		return NULL; /* don't allow "~r:" */

	if (strlen(mask) > REALLEN)
		mask[REALLEN] = '\0';

	/* Prevent otherwise confusing extban relationship */
	if (*mask == '~')
		*mask = '?';

	return retbuf;
}

int extban_realname_is_banned(BanContext *b)
{
	if (match_esc(b->banstr, b->client->info))
		return 1;

	return 0;
}
