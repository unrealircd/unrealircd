/*
 *   IRC - Internet Relay Chat, src/modules/message-ids.c
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

ModuleHeader MOD_HEADER(message-ids)
  = {
	"message-ids",
	"5.0",
	"msgid CAP",
	"UnrealIRCd Team",
	"unrealircd-5",
	};

/** The length of a standard 'msgid' tag (note that special
 * msgid tags will be longer).
 * The 22 alphanumeric characters provide slightly more
 * than 128 bits of randomness (62^22 > 2^128).
 * See mtag_add_or_inherit_msgid() for more information.
 */
#define MSGIDLEN	22

/* Variables */
long CAP_ACCOUNT_TAG = 0L;

int msgid_mtag_is_ok(aClient *acptr, char *name, char *value);
void mtag_add_or_inherit_msgid(aClient *sender, MessageTag *recv_mtags, MessageTag **mtag_list, char *signature);

MOD_INIT(message-ids)
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

MOD_LOAD(message-ids)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(message-ids)
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
	if (IsServer(acptr) && !BadPtr(value))
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


void mtag_add_or_inherit_msgid(aClient *sender, MessageTag *recv_mtags, MessageTag **mtag_list, char *signature)
{
	MessageTag *m = find_mtag(recv_mtags, "msgid");
	if (m)
		m = duplicate_mtag(m);
	else
		m = mtag_generate_msgid();

	if (signature && !strchr(m->value, '-'))
	{
		/* Special case:
		 * Some commands will receive a single msgid from
		 * a remote server for multiple events.
		 * Take for example SJOIN which may contain 5 joins,
		 * 3 bans setting, 2 invites, and setting a few modes.
		 * This way we can still generate unique msgid's
		 * for such sub-events. It is a hash of the subevent
		 * concatenated to the existing msgid.
		 * The hash is the first half of a SHA256 hash, then
		 * base64'd, and with the == suffix removed.
		 *
		 * The !strchr(m->value, '-') in the if above
		 * ensures that we never do more stacking,
		 * although theoretically that could be necessary,
		 * in which case we should reconsider.
		 * For now this fixes some PART issue.
		 */
		SHA256_CTX hash;
		char binaryhash[SHA256_DIGEST_LENGTH];
		char b64hash[SHA256_DIGEST_LENGTH*2+1];
		char newbuf[256];
		memset(&binaryhash, 0, sizeof(binaryhash));
		memset(&b64hash, 0, sizeof(b64hash));
		SHA256_Init(&hash);
		SHA256_Update(&hash, signature, strlen(signature));
		SHA256_Final(binaryhash, &hash);
		b64_encode(binaryhash, sizeof(binaryhash)/2, b64hash, sizeof(b64hash));
		b64hash[22] = '\0'; /* cut off at '=' */
		snprintf(newbuf, sizeof(newbuf), "%s-%s", m->value, b64hash);
		safestrdup(m->value, newbuf);
	}
	AddListItem(m, *mtag_list);
}
