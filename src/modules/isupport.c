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
void _isupport_check_for_changes(void);

ModuleHeader MOD_HEADER = {
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
	EfunctionAddVoid(modinfo->handle, EFUNC_ISUPPORT_CHECK_FOR_CHANGES, _isupport_check_for_changes);

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
	char batch[BATCHLEN + 1];
	int i;
	MessageTag *mtags = NULL, *m;

	*batch = '\0';

	if (HasCapability(client, "draft/extended-isupport") && HasCapability(client, "batch"))
	{
		generate_batch_id(batch);
		new_message(client, NULL, &mtags);
		m = safe_alloc(sizeof(MessageTag));
		safe_strdup(m->name, "batch");
		safe_strdup(m->value, batch);
		AddListItem(m, mtags);
	}

	if (*batch)
		sendto_one(client, NULL, ":%s BATCH +%s draft/isupport", me.name, batch);

	for (i = 0; ISupportStrings[i]; i++)
	{
		if (*batch)
			sendtaggednumericfmt(client, mtags, RPL_ISUPPORT, "%s :are supported by this server", ISupportStrings[i]);
		else
			sendnumeric(client, RPL_ISUPPORT, ISupportStrings[i]);
	}

	if (*batch)
	{
		sendto_one(client, NULL, ":%s BATCH -%s", me.name, batch);
		safe_free_message_tags(mtags);
	}
}

ISupport *isupport_find_ex(ISupport *list, const char *name)
{
	for (; list; list = list->next)
		if (!strcmp(list->token, name))
			return list;
	return NULL;
}

void isupport_check_for_changes_send(const char *addstr, char *buf, size_t buflen, char *batch, MessageTag *mtags, int *changes, int *buffered_changes)
{
	Client *acptr;

	if (!*buf)
		return;

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (HasCapability(acptr, "draft/extended-isupport") && HasCapability(acptr, "batch"))
		{
			sendtaggednumericfmt(acptr, mtags, RPL_ISUPPORT, "%s :are supported by this server", buf);
		} else
		{
			sendnumeric(acptr, RPL_ISUPPORT, buf);
		}
	}
	*buf = '\0';
	*buffered_changes = 0;
}

void isupport_check_for_changes_one(const char *addstr, char *buf, size_t buflen, char *batch, MessageTag *mtags, int *changes, int *buffered_changes)
{
	Client *acptr;

	if (*changes == 0)
	{
		/* First change, need to start the batch */
		list_for_each_entry(acptr, &lclient_list, lclient_node)
			if (HasCapability(acptr, "draft/extended-isupport") && HasCapability(acptr, "batch"))
				sendto_one(acptr, NULL, ":%s BATCH +%s draft/isupport", me.name, batch);
	}

	*changes = *changes + 1;
	*buffered_changes = *buffered_changes + 1;

	if ((strlen(buf) + strlen(addstr) >= ISUPPORTLEN) || (*buffered_changes == 13))
		isupport_check_for_changes_send(addstr, buf, buflen, batch, mtags, changes, buffered_changes);

	/* Append */
	if (*buf)
		strlcat(buf, " ", buflen);
	strlcat(buf, addstr, buflen);
}

void _isupport_check_for_changes(void)
{
	Client *acptr;
	MessageTag *mtags = NULL;
	char batch[BATCHLEN + 1];
	ISupport *n; // iterator for "new isupports"
	ISupport *o; // iterator for "old isupports"
	char buf[512], addstr[512];
	int changes = 0, bc = 0;

	if (!iConf.send_isupport_updates)
		return;

	buf[0] = '\0';

	generate_batch_id(batch);
	mtags = safe_alloc(sizeof(MessageTag));
	safe_strdup(mtags->name, "batch");
	safe_strdup(mtags->value, batch);

	/* New tokens and changed values */
	for (n = ISupports; n; n = n->next)
	{
		o = isupport_find_ex(ISupports_old, n->token);
		if (!o ||
		    (!o->value && n->value) ||
		    (n->value && !o->value) ||
		    (n->value && o->value && strcmp(n->value, o->value)))
		{
			/* New or changed */
			if (n->value)
			{
				snprintf(addstr, sizeof(addstr), "%s=%s",
				         n->token, n->value);
			} else
			{
				strlcpy(addstr, n->token, sizeof(addstr));
			}
			isupport_check_for_changes_one(addstr, buf, sizeof(buf), batch, mtags, &changes, &bc);
		}
	}

	/* Removed tokens */
	for (o = ISupports_old; o; o = o->next)
	{
		n = isupport_find_ex(ISupports, o->token);
		if (!n)
		{
			/* Removed */
			strlcpy(addstr, "-", sizeof(addstr));
			strlcpy(addstr, o->token, sizeof(addstr));
			isupport_check_for_changes_one(addstr, buf, sizeof(buf), batch, mtags, &changes, &bc);
		}
	}

	isupport_check_for_changes_send(NULL, buf, sizeof(buf), batch, mtags, &changes, &bc);

	if (changes)
	{
		/* End the batch (for those clients who received a batch, that is) */
		list_for_each_entry(acptr, &lclient_list, lclient_node)
			if (HasCapability(acptr, "draft/extended-isupport") && HasCapability(acptr, "batch"))
				sendto_one(acptr, NULL, ":%s BATCH -%s", me.name, batch);
	}

	safe_free_message_tags(mtags);
}
