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

ModuleHeader MOD_HEADER(operclass)
= {
	"chanmodes/extbans/operclass",
	"4.0",
	"ExtBan ~O - Ban/exempt operclass",
	"3.2-b8-1",
	NULL 
};

/* Forward declarations */
char *extban_operclass_conv_param(char *para);
int extban_operclass_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type);

/** Called upon module init */
MOD_INIT(operclass)
{
	ExtbanInfo req;
	
	req.flag = 'O';
	req.is_ok = NULL;
	req.conv_param = extban_operclass_conv_param;
	req.is_banned = extban_operclass_is_banned;
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
MOD_LOAD(operclass)
{
	return MOD_SUCCESS;
}

/** Called upon unload */
MOD_UNLOAD(operclass)
{
	return MOD_SUCCESS;
}


#define OPERCLASSLEN 64

char *extban_operclass_conv_param(char *para)
{
	static char retbuf[OPERCLASSLEN + 4];
	char *p;

	strlcpy(retbuf, para, sizeof(retbuf));

	/* allow alpha, numeric, -, _, * and ? wildcards */
	for (p = retbuf+3; *p; p++)
		if (!strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_?*", *p))
			*p = '\0';

	if (retbuf[3] == '\0')
		return NULL; /* just "~O:" is invalid */

	return retbuf;
}

int extban_operclass_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type)
{
	char *ban = banin+3;

	if (MyClient(sptr) && IsOper(sptr))
	{
		char *operclass = NULL;
		ConfigItem_oper *oper = Find_oper(sptr->user->operlogin);
		if (oper && oper->operclass)
			operclass = oper->operclass;
		
		if (operclass && !match(ban, operclass))
			return 1;
	}

	return 0;
}
