/*
 *   IRC - Internet Relay Chat, src/modules/isupport.c
 *   (C) 2025 Valware & The UnrealIRCd Team
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

/* One include for all */
#include "unrealircd.h"

#define CMD_ISUPPORT "ISUPPORT"

/* Forward declarations */
void _send_isupport(Client *client);

ModuleHeader MOD_HEADER
={
	"isupport", /* Name of module */
	"5.0", /* Version */
	"Implement ISUPPORT (numeric 005) sending", /* Short description of module */
	"UnrealIRCd Team", /* Author */
	"unrealircd-6", /* Version of UnrealIRCd */
};

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_ISUPPORT, _send_isupport);

	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

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

/* Command 'ISUPPORT' (no params)
 * This command is used to send ISUPPORT information to the client.
 * Clients who have the 'draft/extended-isupport' capability will be
 * able to use this command to view ISUPPORT tokens before sending
 * NICK/USER[/PASS].
 * I've left this open to other users as well as it doesn't make sense
 * to gatekeep it beyond what would be considered normal usage anyway.
 * -- Valware
 */
void _send_isupport(Client *client)
{
	char batch[BATCHLEN+1];
	int cb, ci, i; // Client supports batch, client support isupport, and an iterator

	ci = HasCapability(client, "draft/extended-isupport");
	cb = HasCapability(client, "batch");

	if (!MyUser(client) && !ci)
	{
		sendnumeric(client, ERR_NOTREGISTERED);
		return;
	}

	generate_batch_id(batch);

	if (cb && ci)
	{   
		sendto_one(client, NULL, ":%s BATCH +%s draft/extended-isupport", me.name, batch);
	}

	for (i = 0; ISupportStrings[i]; i++)
	{
		if (cb && ci)
		{
			MessageTag *mtags = NULL;
			new_message(client, NULL, &mtags);

			MessageTag *m = safe_alloc(sizeof(MessageTag));
			safe_strdup(m->name, "batch");
			safe_strdup(m->value, batch);

			AddListItem(m, mtags);

			sendtaggednumericfmt(client, mtags, RPL_ISUPPORT, "%s :are supported by this server", ISupportStrings[i]);
			free_message_tags(mtags);
		}
		else
		{
			sendnumeric(client, RPL_ISUPPORT, ISupportStrings[i]);
		}
	}
	if (cb && ci)
	{
		sendto_one(client, NULL, ":%s BATCH -%s", me.name, batch);
	}
}
