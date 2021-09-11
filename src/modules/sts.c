/*
 *   IRC - Internet Relay Chat, src/modules/sts.c
 *   (C) 2017 Syzop & The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"sts",
	"5.0",
	"Strict Transport Security CAP", 
	"UnrealIRCd Team",
	"unrealircd-6",
	};

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	return MOD_SUCCESS;
}

void init_sts(ModuleInfo *modinfo);

MOD_LOAD()
{
	/* init_sts is delayed to MOD_LOAD due to configuration dependency */
	init_sts(modinfo);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/** Check if this capability should be visible.
 * Note that 'client' may be NULL.
 */
int sts_capability_visible(Client *client)
{
	TLSOptions *ssl;

	/* This is possible if queried from the CAP NEW/DEL code */
	if (client == NULL)
		return (iConf.tls_options && iConf.tls_options->sts_port) ? 1 : 0;

	if (!IsSecure(client))
	{
		if (iConf.tls_options && iConf.tls_options->sts_port)
			return 1; /* YES, non-TLS user and set::tls::sts-policy configured */
		return 0; /* NO, there is no sts-policy */
	}

	ssl = FindTLSOptionsForUser(client);

	if (ssl && ssl->sts_port)
		return 1;

	return 0;
}

const char *sts_capability_parameter(Client *client)
{
	TLSOptions *ssl;
	static char buf[256];

	if (IsSecure(client))
		ssl = FindTLSOptionsForUser(client);
	else
		ssl = iConf.tls_options;

	if (!ssl)
		return ""; /* This would be odd. */

	snprintf(buf, sizeof(buf), "port=%d,duration=%ld", ssl->sts_port, ssl->sts_duration);
	if (ssl->sts_preload)
		strlcat(buf, ",preload", sizeof(buf));

	return buf;
}

void init_sts(ModuleInfo *modinfo)
{
	ClientCapabilityInfo cap;

	memset(&cap, 0, sizeof(cap));
	cap.name = "sts";
	cap.flags = CLICAP_FLAGS_ADVERTISE_ONLY;
	cap.visible = sts_capability_visible;
	cap.parameter = sts_capability_parameter;
	ClientCapabilityAdd(modinfo->handle, &cap, NULL);
}
