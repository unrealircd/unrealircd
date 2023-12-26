/* IRC - Internet Relay Chat, src/modules/real-quit-reason.c
 * unrealircd.org/real-quit-reason message tag (server only)
 * This is really server-only, it does not traverse to any clients.
 * (C) Copyright 2023-.. Syzop and The UnrealIRCd Team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"real-quit-reason",
	"6.0",
	"unrealircd.org/real-quit-reason message tag",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Forward declarations */
int real_quit_reason_mtag_is_ok(Client *client, const char *name, const char *value);
int real_quit_reason_mtag_should_send_to_client(Client *target);
void mtag_inherit_real_quit_reason(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature);

MOD_INIT()
{
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "unrealircd.org/real-quit-reason";
	mtag.is_ok = real_quit_reason_mtag_is_ok;
	mtag.should_send_to_client = real_quit_reason_mtag_should_send_to_client;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_inherit_real_quit_reason);

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

/** This function verifies if the client sending 'unrealircd.org/real-quit-reason'
 * is permitted to do so and uses a permitted syntax.
 * We simply allow unrealircd.org/real-quit-reason ONLY from servers and with any syntax.
 */
int real_quit_reason_mtag_is_ok(Client *client, const char *name, const char *value)
{
	if (IsServer(client))
		return 1;

	return 0;
}

void mtag_inherit_real_quit_reason(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature)
{
	MessageTag *m = find_mtag(recv_mtags, "unrealircd.org/real-quit-reason");
	if (m)
	{
		m = duplicate_mtag(m);
		AddListItem(m, *mtag_list);
	}
}

/** Outgoing filter for this message tag */
int real_quit_reason_mtag_should_send_to_client(Client *target)
{
	if (IsServer(target))
		return 1;

	return 0;
}
