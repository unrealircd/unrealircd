/*
 * Extended ban to ban/exempt by country/geoip info (+b ~country:UK)
 * (C) Copyright 2021 The UnrealIRCd Team
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
	"extbans/country",
	"6.0",
	"ExtBan ~country - Ban/exempt by country (geoip)",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
int extban_country_is_ok(BanContext *b);
const char *extban_country_conv_param(BanContext *b, Extban *extban);
int extban_country_is_banned(BanContext *b);

Extban *register_country_extban(ModuleInfo *modinfo)
{
	ExtbanInfo req;

	memset(&req, 0, sizeof(req));
	req.letter = 'C';
	req.name = "country";
	req.is_ok = extban_country_is_ok;
	req.conv_param = extban_country_conv_param;
	req.is_banned = extban_country_is_banned;
	req.is_banned_events = BANCHK_ALL|BANCHK_TKL;
	req.options = EXTBOPT_INVEX|EXTBOPT_TKL;
	return ExtbanAdd(modinfo->handle, req);
}

/* Called upon module test */
MOD_TEST()
{
	if (!register_country_extban(modinfo))
	{
		config_error("could not register extended ban type");
		return MOD_FAILED;
	}

	return MOD_SUCCESS;
}

/* Called upon module init */
MOD_INIT()
{
	if (!register_country_extban(modinfo))
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

int extban_country_usage(Client *client)
{
	sendnotice(client, "ERROR: ExtBan ~country expects a two letter country code, or * to ban unknown countries. "
					 "For example: +b ~country:UK");
	return EX_DENY;
}

int extban_country_is_ok(BanContext *b)
{
	if (b->is_ok_check == EXCHK_PARAM)
	{
		const char *p;

		if (!strcmp(b->banstr, "*"))
			return EX_ALLOW;

		if ((strlen(b->banstr) != 2))
			return extban_country_usage(b->client);

		for (p = b->banstr; *p; p++)
			if (!isalpha(*p))
				return extban_country_usage(b->client);

		return EX_ALLOW;
	}
	return EX_ALLOW;
}

/* Obtain targeted country from the ban string */
const char *extban_country_conv_param(BanContext *b, Extban *extban)
{
	static char retbuf[EVP_MAX_MD_SIZE * 2 + 1];
	char *p;

	strlcpy(retbuf, b->banstr, sizeof(retbuf));

	for (p = retbuf; *p; p++)
		*p = toupper(*p);

	return retbuf;
}

int extban_country_is_banned(BanContext *b)
{
	GeoIPResult *geo = geoip_client(b->client);
	char *country;

	country = geo ? geo->country_code : "*";

	if (!strcmp(b->banstr, country))
		return 1;

	return 0;
}
