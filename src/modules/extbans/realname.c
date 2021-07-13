/*
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
	"unrealircd-5",
};

/* Forward declarations */
char *extban_realname_conv_param(char *para);
int extban_realname_is_banned(Client *client, Channel *channel, char *banin, int type, char **msg, char **errmsg);

/** Called upon module init */
MOD_INIT()
{
	ExtbanInfo req;
	
	req.flag = 'r';
	req.is_ok = NULL;
	req.conv_param = extban_realname_conv_param;
	req.is_banned = extban_realname_is_banned;
	req.options = EXTBOPT_CHSVSMODE|EXTBOPT_INVEX|EXTBOPT_TKL;
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

/** Realname bans - conv_param */
char *extban_realname_conv_param(char *para)
{
	static char retbuf[REALLEN + 8];
	char *mask;

	strlcpy(retbuf, para, sizeof(retbuf));

	mask = retbuf+3;

	if (!*mask)
		return NULL; /* don't allow "~r:" */

	if (strlen(mask) > REALLEN + 3)
		mask[REALLEN + 3] = '\0';

	if (*mask == '~')
		*mask = '?'; /* Is this good? No ;) */

	return retbuf;
}

int extban_realname_is_banned(Client *client, Channel *channel, char *banin, int type, char **msg, char **errmsg)
{
	char *ban = banin+3;

	if (match_esc(ban, client->info))
		return 1;

	return 0;
}
