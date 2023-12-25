/*
 *   IRC - Internet Relay Chat, src/modules/client-tag-deny.c
 *   (C) 2020 k4be for The UnrealIRCd Team
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

char *ct_isupport_param(void);
int tags_rehash_complete(void);

Module *module;

ModuleHeader MOD_HEADER = {
	"clienttagdeny",
	"5.0",
	"Informs clients about supported client tags",
	"k4be",
	"unrealircd-6",
};

MOD_INIT(){
	MARK_AS_OFFICIAL_MODULE(modinfo);

	return MOD_SUCCESS;
}

MOD_LOAD(){
	module = modinfo->handle;
	ISupportAdd(module, "CLIENTTAGDENY", ct_isupport_param());
	HookAdd(module, HOOKTYPE_REHASH_COMPLETE, 0, tags_rehash_complete);

	return MOD_SUCCESS;
}

MOD_UNLOAD(){
	return MOD_SUCCESS;
}

#define BUFLEN 500

char *ct_isupport_param(void){
	static char buf[BUFLEN];
	MessageTagHandler *m;
	
	strlcpy(buf, "*", sizeof(buf));

	for (m = mtaghandlers; m; m = m->next) {
		if (!m->unloaded && m->name[0] == '+'){
			strlcat(buf, ",-", sizeof(buf));
			strlcat(buf, m->name+1, sizeof(buf));
		}
	}
	return buf;
}

int tags_rehash_complete(void){
	ISupportSet(module, "CLIENTTAGDENY", ct_isupport_param());
	return HOOK_CONTINUE;
}

