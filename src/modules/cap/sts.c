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

ModuleHeader MOD_HEADER(sts)
  = {
	"sts",
	"4.0",
	"Strict Transport Security CAP", 
	"3.2-b8-1",
	NULL 
	};

MOD_INIT(sts)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	return MOD_SUCCESS;
}

void init_sts(ModuleInfo *modinfo);

MOD_LOAD(sts)
{
	/* init_sts is delayed to MOD_LOAD due to configuration dependency */
	init_sts(modinfo);
	return MOD_SUCCESS;
}

MOD_UNLOAD(sts)
{
	return MOD_SUCCESS;
}

int sts_capability_visible(aClient *acptr)
{
	return 1;
}

char *sts_capability_parameter(aClient *acptr)
{
	return "port=6697";
}

void init_sts(ModuleInfo *modinfo)
{
	ClientCapability cap;

	memset(&cap, 0, sizeof(cap));
	cap.name = "draft/sts";
	cap.cap = 0;
	cap.flags = CLICAP_FLAGS_ADVERTISE_ONLY;
	cap.visible = sts_capability_visible;
	cap.parameter = sts_capability_parameter;
	ClientCapabilityAdd(modinfo->handle, &cap);
}
