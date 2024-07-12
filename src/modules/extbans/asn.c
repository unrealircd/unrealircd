/*
 * Extended ban to ban/exempt by asn/geoip info (+b ~asn:64496)
 * (C) Copyright 2024 The UnrealIRCd Team
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
	"extbans/asn",
	"6.0",
	"ExtBan ~asn - Ban/exempt by ASN (geoip)",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
int extban_asn_is_ok(BanContext *b);
const char *extban_asn_conv_param(BanContext *b, Extban *extban);
int extban_asn_is_banned(BanContext *b);

Extban *register_asn_extban(ModuleInfo *modinfo)
{
	ExtbanInfo req;

	memset(&req, 0, sizeof(req));
	req.letter = 'A';
	req.name = "asn";
	req.is_ok = extban_asn_is_ok;
	req.conv_param = extban_asn_conv_param;
	req.is_banned = extban_asn_is_banned;
	req.is_banned_events = BANCHK_ALL|BANCHK_TKL;
	req.options = EXTBOPT_INVEX|EXTBOPT_TKL;
	return ExtbanAdd(modinfo->handle, req);
}

/* Called upon module test */
MOD_TEST()
{
	if (!register_asn_extban(modinfo))
	{
		config_error("could not register extended ban type");
		return MOD_FAILED;
	}

	return MOD_SUCCESS;
}

/* Called upon module init */
MOD_INIT()
{
	if (!register_asn_extban(modinfo))
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

int extban_asn_usage(Client *client)
{
	sendnotice(client, "ERROR: ExtBan ~asn expects the AS number (all digits). "
	                   "For example: +b ~asn:64496");
	return EX_DENY;
}

int extban_asn_is_ok(BanContext *b)
{
	if (b->is_ok_check == EXCHK_PARAM)
	{
		const char *p;

		if (!strcmp(b->banstr, "*"))
			return EX_ALLOW;

		if (!*b->banstr)
			return extban_asn_usage(b->client);

		for (p = b->banstr; *p; p++)
			if (!isdigit(*p))
				return extban_asn_usage(b->client);

		return EX_ALLOW;
	}
	return EX_ALLOW;
}

/* Obtain targeted asn from the ban string */
const char *extban_asn_conv_param(BanContext *b, Extban *extban)
{
	static char retbuf[32];
	unsigned int asn;
	char *p=NULL;

	if (!isdigit(b->banstr[0]))
		return NULL;

	asn = strtoul(b->banstr, &p, 10);
	if (!BadPtr(p))
		return NULL; /* contains invalid characters */

	snprintf(retbuf, sizeof(retbuf), "%u", asn);
	return retbuf;
}

int extban_asn_is_banned(BanContext *b)
{
	unsigned int banned_asn = strtoul(b->banstr, NULL, 10);
	GeoIPResult *geo = geoip_client(b->client);

	if (geo)
		return banned_asn == geo->asn;

	return banned_asn == 0; /* ASN 0 is for unknown */
}
