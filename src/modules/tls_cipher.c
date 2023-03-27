/*
 * Store TLS cipher in ModData
 * (C) Copyright 2021-.. Syzop and The UnrealIRCd Team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"tls_cipher",
	"5.0",
	"Store and retrieve TLS cipher string",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
void tls_cipher_free(ModData *m);
const char *tls_cipher_serialize(ModData *m);
void tls_cipher_unserialize(const char *str, ModData *m);
int tls_cipher_handshake(Client *client);
int tls_cipher_connect(Client *client);
int tls_cipher_whois(Client *client, Client *target);
int tls_json_expand_client(Client *client, int detail, json_t *j);

ModDataInfo *tls_cipher_md; /* Module Data structure which we acquire */

MOD_INIT()
{
ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "tls_cipher";
	mreq.free = tls_cipher_free;
	mreq.serialize = tls_cipher_serialize;
	mreq.unserialize = tls_cipher_unserialize;
	mreq.sync = MODDATA_SYNC_EARLY;
	mreq.type = MODDATATYPE_CLIENT;
	tls_cipher_md = ModDataAdd(modinfo->handle, mreq);
	if (!tls_cipher_md)
		abort();

	HookAdd(modinfo->handle, HOOKTYPE_HANDSHAKE, 0, tls_cipher_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_HANDSHAKE_OUT, 0, tls_cipher_handshake);

	HookAdd(modinfo->handle, HOOKTYPE_JSON_EXPAND_CLIENT, 0, tls_json_expand_client);

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

int tls_cipher_handshake(Client *client)
{
	if (client->local->ssl)
	{
		const char *cipher = tls_get_cipher(client);

		if (!cipher)
			return 0;

		moddata_client_set(client, "tls_cipher", cipher);
	}
	return 0;
}

void tls_cipher_free(ModData *m)
{
	safe_free(m->str);
}

const char *tls_cipher_serialize(ModData *m)
{
	if (!m->str)
		return NULL;
	return m->str;
}

void tls_cipher_unserialize(const char *str, ModData *m)
{
	safe_strdup(m->str, str);
}

int tls_json_expand_client(Client *client, int detail, json_t *j)
{
	json_t *tls;
	const char *str;

	if (detail < 2)
		return 0;

	str = moddata_client_get(client, "tls_cipher");
	if (!str)
		return 0;

	tls = json_object_get(j, "tls");
	if (!tls)
	{
		tls = json_object();
		json_object_set_new(j, "tls", tls);
	}

	json_object_set_new(tls, "cipher", json_string_unreal(str));

	return 0;
}
