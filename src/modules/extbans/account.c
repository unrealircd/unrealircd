/*
 * Extended ban to ban/exempt by services account (~b ~a:accountname)
 * (C) Copyright 2011-.. Bram Matthys (Syzop) and the UnrealIRCd team
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

ModuleHeader MOD_HEADER(account)
= {
	"chanmodes/extbans/account",
	"4.0",
	"ExtBan ~a - Ban/exempt by services account name",
	"3.2-b8-1",
	NULL 
};

/* Forward declarations */
char *extban_account_conv_param(char *para);
int extban_account_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type);

/** Called upon module init */
MOD_INIT(account)
{
	ExtbanInfo req;
	
	req.flag = 'a';
	req.is_ok = NULL;
	req.conv_param = extban_account_conv_param;
	req.is_banned = extban_account_is_banned;
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
MOD_LOAD(account)
{
	return MOD_SUCCESS;
}

/** Called upon unload */
MOD_UNLOAD(account)
{
	return MOD_SUCCESS;
}

/** Account bans */
char *extban_account_conv_param(char *para)
{
	char *mask;
	static char retbuf[NICKLEN + 4];

	strlcpy(retbuf, para, sizeof(retbuf)); /* truncate */

	if (!strcmp(retbuf+3, "0"))
		return NULL; /* ~a:0 would mean ban all non-regged, but we already have +R for that. */

	return retbuf;
}

int extban_account_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type)
{
	char *ban = banin+3;

	if (!stricmp(ban, sptr->user->svid))
		return 1;

	return 0;
}
