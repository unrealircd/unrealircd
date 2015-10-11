/*
 * Extended ban to ban/exempt by certificate fingerprint (+b ~S:certfp)
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
	"4.0",
	"ExtBan ~S - Ban/exempt by SHA256 SSL certificate fingerprint",
	"3.2-b8-1",
	NULL 
};

/* Forward declarations */
int extban_certfp_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what, int what2);
char *extban_certfp_conv_param(char *para);
int extban_certfp_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type);

/* Called upon module init */
MOD_INIT(certfp)
{
	ExtbanInfo req;
	
	req.flag = 'S';
	req.is_ok = extban_certfp_is_ok;
	req.conv_param = extban_certfp_conv_param;
	req.is_banned = extban_certfp_is_banned;
	req.options = EXTBOPT_INVEX;
	if (!ExtbanAdd(modinfo->handle, req))
	{
		config_error("could not register extended ban type");
		return MOD_FAILED;
	}

	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	return MOD_SUCCESS;
}

/* Called upon module load */
MOD_LOAD(certfp)
{
	return MOD_SUCCESS;
}

/* Called upon unload */
MOD_UNLOAD(account)
{
	return MOD_SUCCESS;
}

#define CERT_FP_LEN 64

int extban_certfp_usage(aClient *sptr)
{
	sendnotice(sptr, "ERROR: ExtBan ~S expects an SHA256 fingerprint in hexadecimal format (no colons). "
					 "For example: +e ~S:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef)");
	return EX_DENY;
}

int extban_certfp_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what, int what2)
{
	if (checkt == EXCHK_PARAM)
	{
		char *p;
		
		if (strlen(para) != 3 + CERT_FP_LEN)
			return extban_certfp_usage(sptr);
		
		for (p = para + 3; *p; p++)
			if (!isxdigit(*p))
				return extban_certfp_usage(sptr);

		return EX_ALLOW;
	}
	return EX_ALLOW;
}

/* Obtain targeted certfp from the ban string */
char *extban_certfp_conv_param(char *para)
{
	static char retbuf[EVP_MAX_MD_SIZE * 2 + 1];
	char *p;
	
	strlcpy(retbuf, para, sizeof(retbuf));
	
	for (p = retbuf+3; *p; p++)
	{
		*p = tolower(*p);
	}

	return retbuf;
}

int extban_certfp_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type)
{
	char *ban = banin+3;
	char *fp;

	fp = moddata_client_get(sptr, "certfp");

	if (!fp)
		return 0; /* not using SSL */

	if (!strcmp(ban, fp))
		return 1;

	return 0;
}
