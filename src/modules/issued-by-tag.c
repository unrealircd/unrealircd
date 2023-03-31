/*
 * unrealircd.org/issued-by message tag (server only)
 * Shows who or what actually issued the command.
 * (C) Copyright 2023-.. Syzop and The UnrealIRCd Team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"issued-by-tag",
	"6.0",
	"unrealircd.org/issued-by message tag",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Forward declarations */
int issued_by_mtag_is_ok(Client *client, const char *name, const char *value);
int issued_by_mtag_should_send_to_client(Client *target);
void mtag_inherit_issued_by(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature);
void _mtag_generate_issued_by_irc(MessageTag **mtags, Client *client);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_MTAG_GENERATE_ISSUED_BY_IRC, _mtag_generate_issued_by_irc);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "unrealircd.org/issued-by";
	mtag.is_ok = issued_by_mtag_is_ok;
	mtag.should_send_to_client = issued_by_mtag_should_send_to_client;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_inherit_issued_by);

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

/** This function verifies if the client sending 'unrealircd.org/issued-by'
 * is permitted to do so and uses a permitted syntax.
 * We simply allow unrealircd.org/issued-by ONLY from servers and with any syntax.
 */
int issued_by_mtag_is_ok(Client *client, const char *name, const char *value)
{
	if (IsServer(client))
		return 1;

	return 0;
}

void mtag_inherit_issued_by(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature)
{
	MessageTag *m = find_mtag(recv_mtags, "unrealircd.org/issued-by");
	if (m)
	{
		m = duplicate_mtag(m);
		AddListItem(m, *mtag_list);
	}
}

/** Outgoing filter for this message tag */
int issued_by_mtag_should_send_to_client(Client *target)
{
	if (IsServer(target) || IsOper(target))
		return 1;

	return 0;
}

void _mtag_generate_issued_by_irc(MessageTag **mtags, Client *client)
{
	MessageTag *m;
	char buf[512];

	if (IsULine(client))
	{
		if (IsUser(client))
			snprintf(buf, sizeof(buf), "SERVICES:%s@%s", client->name, client->uplink->name);
		else
			snprintf(buf, sizeof(buf), "SERVICES:%s", client->name);
	} else if (IsOper(client))
	{
		const char *operlogin = moddata_client_get(client, "operlogin");
		if (operlogin)
			snprintf(buf, sizeof(buf), "OPER:%s@%s:%s", client->name, client->uplink->name, operlogin);
		else
			snprintf(buf, sizeof(buf), "OPER:%s@%s", client->name, client->uplink->name);
	} else {
		return;
	}

	m = safe_alloc(sizeof(MessageTag));
	safe_strdup(m->name, "unrealircd.org/issued-by");
	safe_strdup(m->value, buf);
	AddListItem(m, *mtags);
}

void _mtag_generate_issued_by_rpc(MessageTag **mtags, Client *client)
{
	MessageTag *m;
	char buf[512];

	if (!IsRPC(client))
		return;

	// TODO: grab all RPC info directly instead of from 'client'?
	// TODO: take into account RRPC
	// TODO: also add :optional_text
	snprintf(buf, sizeof(buf), "RPC:%s@%s", client->name, client->uplink->name);

	m = safe_alloc(sizeof(MessageTag));
	safe_strdup(m->name, "unrealircd.org/issued-by");
	safe_strdup(m->value, buf);
	AddListItem(m, *mtags);
}
