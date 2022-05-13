/*
 * Store creationtime in ModData
 * (C) Copyright 2022-.. Syzop and The UnrealIRCd Team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"creationtime",
	"6.0",
	"Store and retrieve creation time of clients",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
void creationtime_free(ModData *m);
const char *creationtime_serialize(ModData *m);
void creationtime_unserialize(const char *str, ModData *m);
int creationtime_handshake(Client *client);
int creationtime_welcome_user(Client *client, int numeric);
int creationtime_whois(Client *client, Client *target);

ModDataInfo *creationtime_md; /* Module Data structure which we acquire */

#define SetCreationTime(x,y)	do { moddata_client(x, creationtime_md).ll = y; } while(0)

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "creationtime";
	mreq.free = creationtime_free;
	mreq.serialize = creationtime_serialize;
	mreq.unserialize = creationtime_unserialize;
	mreq.sync = MODDATA_SYNC_EARLY;
	mreq.type = MODDATATYPE_CLIENT;
	creationtime_md = ModDataAdd(modinfo->handle, mreq);
	if (!creationtime_md)
		abort();

	/* This event sets creationtime very early: on handshake in and out */
	HookAdd(modinfo->handle, HOOKTYPE_HANDSHAKE, 0, creationtime_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_HANDSHAKE_OUT, 0, creationtime_handshake);

	/* And this event (re)sets it because that also happens in
	 * welcome_user() in nick.c regarding #2174
	 */
	HookAdd(modinfo->handle, HOOKTYPE_WELCOME, 0, creationtime_welcome_user);

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

int creationtime_handshake(Client *client)
{
	SetCreationTime(client, client->local->creationtime);
	return 0;
}

int creationtime_welcome_user(Client *client, int numeric)
{
	if (numeric == 0)
		SetCreationTime(client, client->local->creationtime);
	return 0;
}

void creationtime_free(ModData *m)
{
	m->ll = 0;
}

const char *creationtime_serialize(ModData *m)
{
	static char buf[64];

	snprintf(buf, sizeof(buf), "%lld", (long long)m->ll);
	return buf;
}

void creationtime_unserialize(const char *str, ModData *m)
{
	m->ll = atoll(str);
}
