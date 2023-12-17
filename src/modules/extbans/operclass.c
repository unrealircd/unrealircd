/*
 * Extended ban type: ban (or rather: exempt or invex) an operclass.
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
	"extbans/operclass",
	"4.2",
	"ExtBan ~O - Ban/exempt operclass",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
const char *extban_operclass_conv_param(BanContext *b, Extban *extban);
int extban_operclass_is_banned(BanContext *b);

/** Called upon module init */
MOD_INIT()
{
	ExtbanInfo req;
	
	memset(&req, 0, sizeof(req));
	req.letter = 'O';
	req.name = "operclass";
	req.is_ok = NULL;
	req.conv_param = extban_operclass_conv_param;
	req.is_banned = extban_operclass_is_banned;
	req.is_banned_events = BANCHK_ALL;
	req.options = EXTBOPT_INVEX;
	if (!ExtbanAdd(modinfo->handle, req))
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


const char *extban_operclass_conv_param(BanContext *b, Extban *extban)
{
	static char retbuf[OPERCLASSLEN + 4];
	char *p;

	strlcpy(retbuf, b->banstr, sizeof(retbuf));

	/* cut off at first invalid character (.. but allow wildcards) */
	for (p = retbuf; *p; p++)
	{
		if (!valid_operclass_character(*p) && !strchr("*?", *p))
		{
			*p = '\0';
			break;
		}
	}

	return retbuf;
}

int extban_operclass_is_banned(BanContext *b)
{
	if (MyUser(b->client) && IsOper(b->client))
	{
		const char *operclass = get_operclass(b->client);
		if (operclass && match_simple(b->banstr, operclass))
			return 1;
	}

	return 0;
}
