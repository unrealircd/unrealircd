/*
 *   IRC - Internet Relay Chat, src/modules/batch.c
 *   (C) 2019 Syzop & The UnrealIRCd Team
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

ModuleHeader MOD_HEADER
  = {
	"batch",
	"5.0",
	"Batch CAP", 
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Forward declarations */
CMD_FUNC(cmd_batch);

/* Variables */
long CAP_BATCH = 0L;

int batch_mtag_is_ok(Client *client, const char *name, const char *value);

MOD_INIT()
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&cap, 0, sizeof(cap));
	cap.name = "batch";
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_BATCH);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "batch";
	mtag.is_ok = batch_mtag_is_ok;
	mtag.clicap_handler = c;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	CommandAdd(modinfo->handle, "BATCH", cmd_batch, MAXPARA, CMD_USER|CMD_SERVER);
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

/* BATCH:
 * As an intra-server command:
 * :sender BATCH target +xxxxx [etc etc etc]
 */
CMD_FUNC(cmd_batch)
{
	Client *target;
	char buf[512];

	if (MyUser(client) || (parc < 3))
		return;

	target = find_client(parv[1], NULL);
	if (!target)
		return; /* race condition */

	/* If the recipient does not support message tags or
	 * does not support batch, then don't do anything.
	 */
	if (MyConnect(target) && !IsServer(target) && !HasCapability(target, "batch"))
		return;

	if (MyUser(target))
	{
		/* Send the batch message to the client */
		parv[1] = "BATCH";
		concat_params(buf, sizeof(buf), parc, parv);
		sendto_prefix_one(target, client, recv_mtags, ":%s %s", client->name, buf);
	} else {
		/* Relay the batch message to the server */
		concat_params(buf, sizeof(buf), parc, parv);
		sendto_prefix_one(target, client, recv_mtags, ":%s BATCH %s", client->name, buf);
	}
}

/** This function verifies if the client sending
 * 'batch' is permitted to do so and uses a permitted
 * syntax.
 * We simply allow batch ONLY from servers and with any syntax.
 */
int batch_mtag_is_ok(Client *client, const char *name, const char *value)
{
	if (IsServer(client))
		return 1;

	return 0;
}
