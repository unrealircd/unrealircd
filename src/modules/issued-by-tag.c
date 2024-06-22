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
void _mtag_add_issued_by(MessageTag **mtags, Client *client, MessageTag *recv_mtags);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_MTAG_GENERATE_ISSUED_BY_IRC, _mtag_add_issued_by);
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

/** Add "unrealircd.org/issued-by" tag, if applicable.
 * @param mtags		Pointer to the message tags linked list head
 * @param client	The client issuing the command, or NULL for none.
 * @param recv_mtags	The mtags to inherit from, or NULL for none.
 * @notes If specifying both 'client' and 'recv_mtags' then
 * if inheritance through 'recv_mtags' takes precedence (if it exists).
 *
 * Typical usage is:
 * For locally generated:
 *   mtag_add_issued_by(&mtags, client, NULL);
 * For inheriting from remote requests:
 *   mtag_add_issued_by(&mtags, NULL, recv_mtags);
 * For both, such as if the command is used from RPC:
 *   mtag_add_issued_by(&mtags, client, recv_mtags);
 */
void _mtag_add_issued_by(MessageTag **mtags, Client *client, MessageTag *recv_mtags)
{
	MessageTag *m;
	char buf[512];

	m = find_mtag(recv_mtags, "unrealircd.org/issued-by");
	if (m)
	{
		m = duplicate_mtag(m);
		AddListItem(m, *mtags);
		return;
	}

	if (client == NULL)
		return;

	if (IsRPC(client) && client->rpc)
	{
		// TODO: test with all of: local rpc through unix socket, local rpc web, RRPC
		if (client->rpc->issuer)
		{
			// For several places in the source, like TOPIC
			// we cannot explicitly trust the uplink issuer's
			// name because it might contain spaces and that's
			// not good. It must conform to IRC nick standards.
			if (!do_nick_name(client->rpc->issuer)) 
				return;
			else
				snprintf(buf, sizeof(buf), "RPC:%s@%s:%s", client->rpc->rpc_user, client->uplink->name, client->rpc->issuer);
		} else {
			snprintf(buf, sizeof(buf), "RPC:%s@%s", client->rpc->rpc_user, client->uplink->name);
		}
	} else
	if (IsULine(client))
	{
		if (IsUser(client))
			snprintf(buf, sizeof(buf), "SERVICES:%s@%s", client->name, client->uplink->name);
		else
			snprintf(buf, sizeof(buf), "SERVICES:%s", client->name);
	} else
	if (IsOper(client))
	{
		const char *operlogin = moddata_client_get(client, "operlogin");
		if (operlogin)
			snprintf(buf, sizeof(buf), "OPER:%s@%s:%s", client->name, client->uplink->name, operlogin);
		else
			snprintf(buf, sizeof(buf), "OPER:%s@%s", client->name, client->uplink->name);
	} else
	{
		return;
	}

	m = safe_alloc(sizeof(MessageTag));
	safe_strdup(m->name, "unrealircd.org/issued-by");
	safe_strdup(m->value, buf);
	AddListItem(m, *mtags);
}
