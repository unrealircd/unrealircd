/* 
 * IRC - Internet Relay Chat, src/modules/extbans/certfp.c
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

ModuleHeader MOD_HEADER
= {
	"extbans/certfp",
	"4.2",
	"ExtBan ~S - Ban/exempt by SHA256 TLS certificate fingerprint",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
int extban_certfp_is_ok(BanContext *b);
const char *extban_certfp_conv_param(BanContext *b, Extban *extban);
int extban_certfp_is_banned(BanContext *b);

Extban *register_certfp_extban(ModuleInfo *modinfo)
{
	ExtbanInfo req;

	memset(&req, 0, sizeof(req));
	req.letter = 'S';
	req.name = "certfp";
	req.is_ok = extban_certfp_is_ok;
	req.conv_param = extban_certfp_conv_param;
	req.is_banned = extban_certfp_is_banned;
	req.is_banned_events = BANCHK_ALL|BANCHK_TKL;
	req.options = EXTBOPT_INVEX|EXTBOPT_TKL;
	return ExtbanAdd(modinfo->handle, req);
}

/* Called upon module test */
MOD_TEST()
{
	if (!register_certfp_extban(modinfo))
	{
		config_error("could not register extended ban type");
		return MOD_FAILED;
	}

	return MOD_SUCCESS;
}

/* Called upon module init */
MOD_INIT()
{
	if (!register_certfp_extban(modinfo))
	{
		config_error("could not register extended ban type");
		return MOD_FAILED;
	}

	MARK_AS_OFFICIAL_MODULE(modinfo);

	return MOD_SUCCESS;
}

/* Called upon module load */
MOD_LOAD()
{
	return MOD_SUCCESS;
}

/* Called upon unload */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

#define CERT_FP_LEN 64

int extban_certfp_usage(Client *client)
{
	sendnotice(client, "ERROR: ExtBan ~S expects an SHA256 fingerprint in hexadecimal format (no colons). "
					 "For example: +e ~S:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef)");
	return EX_DENY;
}

int extban_certfp_is_ok(BanContext *b)
{
	if (b->is_ok_check == EXCHK_PARAM)
	{
		const char *p;

		if (strlen(b->banstr) != CERT_FP_LEN)
			return extban_certfp_usage(b->client);

		for (p = b->banstr; *p; p++)
			if (!isxdigit(*p))
				return extban_certfp_usage(b->client);

		return EX_ALLOW;
	}
	return EX_ALLOW;
}

/* Obtain targeted certfp from the ban string */
const char *extban_certfp_conv_param(BanContext *b, Extban *extban)
{
	static char retbuf[EVP_MAX_MD_SIZE * 2 + 1];
	char *p;

	strlcpy(retbuf, b->banstr, sizeof(retbuf));

	for (p = retbuf; *p; p++)
	{
		*p = tolower(*p);
	}

	return retbuf;
}

int extban_certfp_is_banned(BanContext *b)
{
	const char *fp = moddata_client_get(b->client, "certfp");

	if (!fp)
		return 0; /* not using TLS */

	if (!strcmp(b->banstr, fp))
		return 1;

	return 0;
}
