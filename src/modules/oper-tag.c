/*
 *   IRC - Internet Relay Chat, src/modules/oper-tag.c
 *   (C) 2020 Syzop & The UnrealIRCd Team
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


/*
 * Due to limitations (cannot verify target in HOOKTYPE_NEW_MESSAGE where the mtag is generated) this implementation of
 * draft/oper is split up into categories so we can better decide which information to send the target
 * As the 'opername' and 'operclass' are not defined in any spec, I've prefixed them with unrealircd.org
*/
#define MTAG_OPER "draft/oper"
#define MTAG_OPER_NAME "unrealircd.org/opername"
#define MTAG_OPER_CLASS "unrealircd.org/operclass"

ModuleHeader MOD_HEADER
  = {
	"oper-tag",
	"5.0",
	"oper message tag",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Variables */
long CAP_ACCOUNT_TAG = 0L;

int oper_mtag_is_ok(Client *client, const char *name, const char *value);
int oper_name_mtag_should_send_to_client(Client *target);
int oper_class_mtag_should_send_to_client(Client *target);
void mtag_add_oper_name(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature);
void mtag_add_oper_class(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature);
void mtag_add_oper(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature);

MOD_INIT()
{
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = MTAG_OPER;
	mtag.is_ok = oper_mtag_is_ok;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	mtag.name = MTAG_OPER_NAME;
	mtag.is_ok = oper_mtag_is_ok;
	mtag.should_send_to_client = oper_name_mtag_should_send_to_client;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	mtag.name = MTAG_OPER_CLASS;
	mtag.is_ok = oper_mtag_is_ok;
	mtag.should_send_to_client = oper_class_mtag_should_send_to_client;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_oper);
	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_oper_name);
	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_oper_class);

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

/** This function verifies if the client sending
 * 'oper-tag' is permitted to do so and uses a permitted
 * syntax.
 * We simply allow oper-tag ONLY from servers and with any syntax.
 */
int oper_mtag_is_ok(Client *client, const char *name, const char *value)
{
	if (IsServer(client))
		return 1;

	return 0;
}

void mtag_add_oper(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature)
{
	MessageTag *m;

	if (IsUser(client) && IsOper(client))
	{
		MessageTag *m = find_mtag(recv_mtags, MTAG_OPER);
		if (m)
		{
			m = duplicate_mtag(m);
		} else {
			
			m = safe_alloc(sizeof(MessageTag));
			safe_strdup(m->name, MTAG_OPER);
			m->value = NULL;
		}
		AddListItem(m, *mtag_list);
	}
}

void mtag_add_oper_name(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature)
{
	MessageTag *m;

	if (IsUser(client) && IsOper(client))
	{
	  MessageTag *m = find_mtag(recv_mtags, MTAG_OPER_NAME);
		if (m)
		{
		  m = duplicate_mtag(m);
		} else {
			
			m = safe_alloc(sizeof(MessageTag));
			safe_strdup(m->name, MTAG_OPER_NAME);
			safe_strdup(m->value, client->user->operlogin);
		}
		AddListItem(m, *mtag_list);
	}
}

void mtag_add_oper_class(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature)
{
	MessageTag *m;

	if (IsUser(client) && IsOper(client))
	{
		MessageTag *m = find_mtag(recv_mtags, MTAG_OPER_CLASS);
		if (m)
		{
			m = duplicate_mtag(m);
		} else {
			
			m = safe_alloc(sizeof(MessageTag));
			safe_strdup(m->name, MTAG_OPER_CLASS);
			safe_strdup(m->value, moddata_client_get(client, "operclass"));
		}
		AddListItem(m, *mtag_list);
	}
}

/** Outgoing filter for draft/oper message tag */
int oper_name_mtag_should_send_to_client(Client *target)
{
	if (IsServer(target) || IsOper(target))
		return 1;
	return 0;
}

/** Outgoing filter for unrealircd.org/opername message tag */
int oper_name_mtag_should_send_to_client(Client *target)
{
	if (IsServer(target) || IsOper(target))
		return 1;
	return 0;
}

/** Outgoing filter for unrealircd.org/operclass message tag */
int oper_class_mtag_should_send_to_client(Client *target)
{
	if (IsServer(target) || IsOper(target))
		return 1;
	return 0;
}
