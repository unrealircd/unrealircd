/*
 *   IRC - Internet Relay Chat, src/modules/plaintext-policy.c
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
	"plaintext-policy",
	"5.0",
	"Plaintext Policy CAP",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	return MOD_SUCCESS;
}

void init_plaintext_policy(ModuleInfo *modinfo);

MOD_LOAD()
{
	/* init_plaintext_policy is delayed to MOD_LOAD due to configuration dependency */
	init_plaintext_policy(modinfo);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

const char *plaintext_policy_capability_parameter(Client *client)
{
	static char buf[128];
	
	snprintf(buf, sizeof(buf), "user=%s,oper=%s,server=%s",
             policy_valtostr(iConf.plaintext_policy_user),
             policy_valtostr(iConf.plaintext_policy_oper),
             policy_valtostr(iConf.plaintext_policy_server));
	return buf;
}

void init_plaintext_policy(ModuleInfo *modinfo)
{
	ClientCapabilityInfo cap;

	memset(&cap, 0, sizeof(cap));
	cap.name = "unrealircd.org/plaintext-policy";
	cap.flags = CLICAP_FLAGS_ADVERTISE_ONLY;
	cap.parameter = plaintext_policy_capability_parameter;
	ClientCapabilityAdd(modinfo->handle, &cap, NULL);
}
