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

ModuleHeader MOD_HEADER(plaintext_policy)
  = {
	"plaintext-policy",
	"4.2",
	"Plaintext Policy CAP",
	"3.2-b8-1",
	NULL 
	};

MOD_INIT(plaintext_policy)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	return MOD_SUCCESS;
}

void init_plaintext_policy(ModuleInfo *modinfo);

MOD_LOAD(plaintext_policy)
{
	/* init_plaintext_policy is delayed to MOD_LOAD due to configuration dependency */
	init_plaintext_policy(modinfo);
	return MOD_SUCCESS;
}

MOD_UNLOAD(plaintext_policy)
{
	return MOD_SUCCESS;
}

char *plaintext_policy_capability_parameter(aClient *acptr)
{
	static char buf[128];
	
	snprintf(buf, sizeof(buf), "user=%s,oper=%s,server=%s",
             plaintextpolicy_valtostr(iConf.plaintext_policy_user),
             plaintextpolicy_valtostr(iConf.plaintext_policy_oper),
             plaintextpolicy_valtostr(iConf.plaintext_policy_server));
	return buf;
}

void init_plaintext_policy(ModuleInfo *modinfo)
{
	ClientCapability cap;

	memset(&cap, 0, sizeof(cap));
	cap.name = "unrealircd.org/plaintext-policy";
	cap.cap = 0;
	cap.flags = CLICAP_FLAGS_ADVERTISE_ONLY;
	cap.parameter = plaintext_policy_capability_parameter;
	ClientCapabilityAdd(modinfo->handle, &cap);
}
