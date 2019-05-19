/*
 *   IRC - Internet Relay Chat, src/modules/cap/msgid.c
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

ModuleHeader MOD_HEADER(msgid)
  = {
	"msgid",
	"4.2",
	"msgid CAP",
	"3.2-b8-1",
	NULL 
	};

/* Variables */
long CAP_ACCOUNT_TAG = 0L;

int msgid_mtag_is_ok(aClient *acptr, char *name, char *value);
void mtag_add_or_inherit_msgid(aClient *sender, MessageTag *recv_mtags, MessageTag **mtag_list);

MOD_INIT(msgid)
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "msgid";
	mtag.is_ok = msgid_mtag_is_ok;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_or_inherit_msgid);

	return MOD_SUCCESS;
}

MOD_LOAD(msgid)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(msgid)
{
	return MOD_SUCCESS;
}

/** This function verifies if the client sending
 * 'msgid' is permitted to do so and uses a permitted
 * syntax.
 * We simply allow msgid ONLY from servers and with any syntax.
 */
int msgid_mtag_is_ok(aClient *acptr, char *name, char *value)
{
	if (IsServer(acptr))
		return 1;

	return 0;
}

/** Generate a msgid.
 * @returns a MessageTag struct that you can use directly.
 * @notes
 * Apparently there has been some discussion on what method to use to
 * generate msgid's. I am not going to list them here. Just saying that
 * they have been considered and we chose to go for a string that contains
 * 128+ bits of randomness, which has an extremely low chance of colissions.
 * Or, to quote wikipedia on the birthday attack problem:
 * "For comparison, 10^-18 to 10^-15 is the uncorrectable bit error rate
 *  of a typical hard disk. In theory, hashes or UUIDs being 128 bits,
 *  should stay within that range until about 820 billion outputs"
 * For reference, 10^-15 is 0.000000000000001%
 * The main reasons for this choice are: that it is extremely simple,
 * the chance of making a mistake in an otherwise complex implementation
 * is nullified and we don't risk "leaking" any details.
 */
MessageTag *mtag_generate_msgid(void)
{
	MessageTag *m = MyMallocEx(sizeof(MessageTag));
	m->name = strdup("msgid");
	m->value = MyMallocEx(MSGIDLEN+1);
	gen_random_alnum(m->value, MSGIDLEN);
	return m;
}


void mtag_add_or_inherit_msgid(aClient *sender, MessageTag *recv_mtags, MessageTag **mtag_list)
{
	MessageTag *m = find_mtag(recv_mtags, "msgid");
	if (m)
		m = duplicate_mtag(m);
	else
		m = mtag_generate_msgid();
	AddListItem(m, *mtag_list);
}
