/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/chanadmin.c
 * Channel Mode +a
 * (C) Copyright 2021 Syzop and the UnrealIRCd team
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
	"chanmodes/chanadmin",
	"6.0",
	"Channel Mode +a",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
int cmode_chanadmin_is_ok(Client *client, Channel *channel, char mode, const char *para, int type, int what);

MOD_INIT()
{
	CmodeInfo creq;
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&creq, 0, sizeof(creq));
	creq.paracount = 1;
	creq.is_ok = cmode_chanadmin_is_ok;
	creq.letter = 'a';
	creq.prefix = '&';
	creq.sjoin_prefix = '~'; /* yeah i know, totally not confusing! */
	creq.rank = RANK_CHANADMIN;
	creq.unset_with_param = 1;
	creq.type = CMODE_MEMBER;
	CmodeAdd(modinfo->handle, creq, NULL);

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

int cmode_chanadmin_is_ok(Client *client, Channel *channel, char mode, const char *param, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		Client *target = find_user(param, NULL);

		if ((what == MODE_DEL) && (client == target))
		{
			/* User may always remove their own modes */
			return EX_ALLOW;
		}
		if (check_channel_access(client, channel, "q"))
		{
			/* only +q people may do +a/-a */
			return EX_ALLOW;
		}
		if (type == EXCHK_ACCESS_ERR)
			sendnumeric(client, ERR_CHANOWNPRIVNEEDED, channel->name);
		return EX_DENY;
	}

	/* fallthrough -- should not be used */
	return EX_DENY;
}
