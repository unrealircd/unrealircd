/*
 * draft/whoami
 * Allows users to better track their own n!u@h for knowing how long their expected sent buffer will be
 *
 * (C) Copyright 2026 - Valware and the UnrealIRCd team.
 * 
 * License: GPLv2 or later
 */

#include "unrealircd.h"

long CAP_WHOAMI = 0L;

ModuleHeader MOD_HEADER
  = {
	"whoami",
	"6.0",
	"draft/whoami IRCv3 capability",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
int whoami_welcome_user(Client *client, int after_numeric);

MOD_INIT()
{
	ClientCapabilityInfo c;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&c, 0, sizeof(c));
	c.name = "draft/whoami";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_WHOAMI);

	HookAdd(modinfo->handle, HOOKTYPE_WELCOME, 0, whoami_welcome_user);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (!ClientCapabilityBit("chghost"))
	{
		config_error("[whoami] The chghost module must be loaded for whoami to work");
		return MOD_FAILED;
	}

	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/** Send SETNAME burst line to a connecting client that has negotiated
 * draft/whoami + chghost, immediately after the final 005 RPL_ISUPPORT
 * and before LUSERS numerics, as required by the draft/whoami spec.
 */
int whoami_welcome_user(Client *client, int after_numeric)
{
	long CAP_CHGHOST = 0;

	if (after_numeric != 5)
		return 0;

	CAP_CHGHOST = ClientCapabilityBit("chghost");

	if (HasCapabilityFast(client, CAP_WHOAMI) && HasCapabilityFast(client, CAP_CHGHOST))
	{
		sendto_one(client, NULL, ":%s!%s@%s SETNAME :%s",
		           client->name,
		           client->user->username,
		           GetHost(client),
		           client->info);
	}

	return 0;
}
