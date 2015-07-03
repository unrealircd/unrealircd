/*
 * Extended ban to ban/exempt by certfp (+b ~S:certfp)
 * (C) Copyright 2015 The UnrealIRCd Team
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

ModuleHeader MOD_HEADER(certfp)
= {
	"extbans/certfp",
	"$Id$",
	"ExtBan ~S - Ban/exempt by certfp",
	"3.2-b8-1",
	NULL 
};

/* Forward declarations */
int extban_certfp_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what, int what2);
char *extban_certfp_conv_param(char *para);
int extban_certfp_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type);

/* Called upon module init */
DLLFUNC int MOD_INIT(certfp)(ModuleInfo *modinfo)
{
	ExtbanInfo req;
	
	req.flag = 'S';
	req.is_ok = extban_certfp_is_ok;
	req.conv_param = extban_certfp_conv_param;
	req.is_banned = extban_certfp_is_banned;
	req.options = EXTBOPT_ACTMODIFIER;
	if (!ExtbanAdd(modinfo->handle, req))
	{
		config_error("could not register extended ban type");
		return MOD_FAILED;
	}

	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	return MOD_SUCCESS;
}

/* Called upon module load */
DLLFUNC int MOD_LOAD(certfp)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called upon unload */
DLLFUNC int MOD_UNLOAD(account)(int module_unload)
{
	return MOD_SUCCESS;
}

int extban_certfp_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what, int what2)
{
	char *retbuf0;
	retbuf0 = para+3;
	if(checkt == EXCHK_PARAM)
	{
		if(BadPtr(retbuf0) || (strlen(retbuf0) != EVP_MAX_MD_SIZE))
		{
			sendnotice(sptr, "Invalid certfp");
			return EX_DENY;
		}
		while(*retbuf0 != '\0') {
			if(!isxdigit(*retbuf0))
			{
				sendnotice(sptr, "Invalid certfp");
				return EX_DENY;
			}
			retbuf0++;
		}
	}
	return EX_ALLOW;
}

/* Obtain targeted certfp from the ban string */
char *extban_certfp_conv_param(char *para)
{
	int c0unt3 = 0;
	static char retbuf[EVP_MAX_MD_SIZE * 2 + 1];
	strlcpy(retbuf, para, sizeof(retbuf));
	while(c0unt3 < strlen(retbuf))
	{
		if(c0unt3 > 2)
		{
			retbuf[c0unt3] = tolower(retbuf[c0unt3]);
		}
		c0unt3++;
	}

	return retbuf;
}

int extban_certfp_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type)
{
	char *ban = banin+3;
	char *fp;

	fp = moddata_client_get(sptr, "certfp");

	if (!fp)
		return 0; /* lolwut? */

	if (!mycmp(ban, fp))
		return 1;

	return 0;
}