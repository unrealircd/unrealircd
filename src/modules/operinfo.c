/*
 * Store oper login in ModData, used by WHOIS and for auditting purposes.
 * (C) Copyright 2021-.. Syzop and The UnrealIRCd Team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"operinfo",
	"5.0",
	"Store oper login in ModData",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
int operinfo_local_oper(Client *client, int up, const char *oper_block, const char *operclass);
void operinfo_free(ModData *m);
const char *operinfo_serialize(ModData *m);
void operinfo_unserialize(const char *str, ModData *m);

ModDataInfo *operlogin_md = NULL; /* Module Data structure which we acquire */
ModDataInfo *operclass_md = NULL; /* Module Data structure which we acquire */

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "operlogin";
	mreq.free = operinfo_free;
	mreq.serialize = operinfo_serialize;
	mreq.unserialize = operinfo_unserialize;
	mreq.sync = MODDATA_SYNC_EARLY;
	mreq.type = MODDATATYPE_CLIENT;
	operlogin_md = ModDataAdd(modinfo->handle, mreq);
	if (!operlogin_md)
		abort();

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "operclass";
	mreq.free = operinfo_free;
	mreq.serialize = operinfo_serialize;
	mreq.unserialize = operinfo_unserialize;
	mreq.sync = MODDATA_SYNC_EARLY;
	mreq.type = MODDATATYPE_CLIENT;
	operclass_md = ModDataAdd(modinfo->handle, mreq);
	if (!operclass_md)
		abort();

	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_OPER, 0, operinfo_local_oper);

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

int operinfo_local_oper(Client *client, int up, const char *oper_block, const char *operclass)
{
	if (up)
	{
		moddata_client_set(client, "operlogin", oper_block);
		moddata_client_set(client, "operclass", operclass);
	} else {
		moddata_client_set(client, "operlogin", NULL);
		moddata_client_set(client, "operclass", NULL);
	}
	return 0;
}

void operinfo_free(ModData *m)
{
	safe_free(m->str);
}

const char *operinfo_serialize(ModData *m)
{
	if (!m->str)
		return NULL;
	return m->str;
}

void operinfo_unserialize(const char *str, ModData *m)
{
	safe_strdup(m->str, str);
}
