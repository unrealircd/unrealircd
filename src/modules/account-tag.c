/*
 *   IRC - Internet Relay Chat, src/modules/account-tag.c
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

ModuleHeader MOD_HEADER(account-tag)
  = {
	"account-tag",
	"4.2",
	"account-tag CAP",
	"3.2-b8-1",
	NULL 
	};

/* Variables */
long CAP_ACCOUNT_TAG = 0L;

int account_tag_mtag_is_ok(aClient *acptr, char *name, char *value);
void mtag_add_or_inherit_account(aClient *acptr, MessageTag *recv_mtags, MessageTag **mtag_list);

MOD_INIT(account-tag)
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&cap, 0, sizeof(cap));
	cap.name = "account-tag";
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_ACCOUNT_TAG);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "account";
	mtag.is_ok = account_tag_mtag_is_ok;
	mtag.clicap_handler = c;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_or_inherit_account);

	return MOD_SUCCESS;
}

MOD_LOAD(account-tag)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(account-tag)
{
	return MOD_SUCCESS;
}

/** This function verifies if the client sending
 * 'account-tag' is permitted to do so and uses a permitted
 * syntax.
 * We simply allow account-tag ONLY from servers and with any syntax.
 */
int account_tag_mtag_is_ok(aClient *acptr, char *name, char *value)
{
	if (IsServer(acptr))
		return 1;

	return 0;
}

void mtag_add_or_inherit_account(aClient *acptr, MessageTag *recv_mtags, MessageTag **mtag_list)
{
	MessageTag *m = find_mtag(recv_mtags, "account");
	if (m)
	{
		m = duplicate_mtag(m);
	} else
	{
		if (acptr && acptr->user &&
		    (*acptr->user->svid != '*') && !isdigit(*acptr->user->svid))
		{
			m = MyMallocEx(sizeof(MessageTag));
			m->name = strdup("account");
			m->value = strdup(acptr->user->svid);
		}
	}
	if (m)
		AddListItem(m, *mtag_list);
}
