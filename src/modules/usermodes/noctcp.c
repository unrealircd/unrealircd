/*
 * Block user-to-user CTCP UnrealIRCd Module (User Mode +T)
 * (C) Copyright 2000-.. Bram Matthys (Syzop) and the UnrealIRCd team
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

CMD_FUNC(noctcp);

ModuleHeader MOD_HEADER(noctcp)
  = {
	"usermodes/noctcp",
	"4.2",
	"User Mode +T",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

long UMODE_NOCTCP = 0L;

#define IsNoCTCP(cptr)    (cptr->umodes & UMODE_NOCTCP)

char *noctcp_preusermsg(Client *sptr, Client *acptr, char *text, int notice);

MOD_TEST(noctcp)
{
	return MOD_SUCCESS;
}

MOD_INIT(noctcp)
{
CmodeInfo req;

	UmodeAdd(modinfo->handle, 'T', UMODE_GLOBAL, 0, NULL, &UMODE_NOCTCP);
	
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_USERMSG, 0, noctcp_preusermsg);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(noctcp)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(noctcp)
{
	return MOD_SUCCESS;
}

static int IsACTCP(char *s)
{
	if (!s)
		return 0;

	if ((*s == '\001') && strncmp(&s[1], "ACTION ", 7) && strncmp(&s[1], "DCC ", 4))
		return 1;

	return 0;
}

char *noctcp_preusermsg(Client *sptr, Client *acptr, char *text, int notice)
{
	if (MyUser(sptr) && !notice && IsNoCTCP(acptr) && !IsOper(sptr) && IsACTCP(text))
	{
		sendnumeric(sptr, ERR_NOCTCP, acptr->name);
		return NULL;
	}
	return text;
}
