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

ModuleHeader MOD_HEADER
= {
	"extbans/account",
	"4.2",
	"ExtBan ~a - Ban/exempt by services account name",
	"UnrealIRCd Team",
	"unrealircd-5",
};

/* Forward declarations */
char *extban_account_conv_param(char *para);
int extban_account_is_banned(Client *client, Channel *channel, char *banin, int type, char **msg, char **errmsg);

/** Called upon module init */
MOD_INIT()
{
	ExtbanInfo req;
	
	req.flag = 'a';
	req.is_ok = NULL;
	req.conv_param = extban_account_conv_param;
	req.is_banned = extban_account_is_banned;
	req.options = EXTBOPT_INVEX|EXTBOPT_TKL;
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

/** Account bans */
char *extban_account_conv_param(char *para)
{
	char *mask, *acc;
	static char retbuf[NICKLEN + 4];

	strlcpy(retbuf, para, sizeof(retbuf)); /* truncate */

	acc = retbuf+3;
	if (!*acc)
		return NULL; /* don't allow "~a:" */

	return retbuf;
}

int extban_account_is_banned(Client *client, Channel *channel, char *banin, int type, char **msg, char **errmsg)
{
	char *ban = banin+3;

	/* ~a:0 is special and matches all unauthenticated users */
	if (!strcmp(ban, "0") && !IsLoggedIn(client))
		return 1;

	/* ~a:* matches all authenticated users
	 * (Yes this special code is needed because svid
	 *  is 0 or * for unauthenticated users)
	 */
	if (!strcmp(ban, "*") && IsLoggedIn(client))
		return 1;

	if (match_simple(ban, client->user->svid))
		return 1;

	return 0;
}
