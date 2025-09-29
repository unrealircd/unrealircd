/*
 * Store port info in ModData (server_port and local_port).
 * (C) Copyright 2025-.. Syzop and The UnrealIRCd Team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"portinfo",
	"1.0.0",
	"Store port info in ModData",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
void portinfo_free(ModData *m);
const char *portinfo_serialize(ModData *m);
void portinfo_unserialize(const char *str, ModData *m);

ModDataInfo *server_port_md = NULL; /* Module Data structure which we acquire */
ModDataInfo *local_port_md = NULL; /* Module Data structure which we acquire */

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "server_port";
	mreq.free = portinfo_free;
	mreq.serialize = portinfo_serialize;
	mreq.unserialize = portinfo_unserialize;
	mreq.sync = MODDATA_SYNC_EARLY;
	mreq.type = MODDATATYPE_CLIENT;
	server_port_md = ModDataAdd(modinfo->handle, mreq);
	if (!server_port_md)
		abort();

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "local_port";
	mreq.free = portinfo_free;
	mreq.serialize = portinfo_serialize;
	mreq.unserialize = portinfo_unserialize;
	mreq.sync = MODDATA_SYNC_EARLY;
	mreq.type = MODDATATYPE_CLIENT;
	mreq.priority = -999994;
	local_port_md = ModDataAdd(modinfo->handle, mreq);
	if (!local_port_md)
		abort();

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}


MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

void portinfo_free(ModData *m)
{
	m->i = 0;
}

const char *portinfo_serialize(ModData *m)
{
	static char buf[32];
	if (!m->i)
		return NULL;
	snprintf(buf, sizeof(buf), "%d", m->i);
	return buf;
}

void portinfo_unserialize(const char *str, ModData *m)
{
	m->i = atoi(str);
}
