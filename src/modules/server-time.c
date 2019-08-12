/*
 *   IRC - Internet Relay Chat, src/modules/server-time.c
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

ModuleHeader MOD_HEADER(server-time)
  = {
	"server-time",
	"5.0",
	"server-time CAP",
	"3.2-b8-1",
	NULL 
	};

/* Variables */
long CAP_SERVER_TIME = 0L;

int server_time_mtag_is_ok(aClient *acptr, char *name, char *value);
void mtag_add_or_inherit_time(aClient *sender, MessageTag *recv_mtags, MessageTag **mtag_list, char *signature);

MOD_INIT(server-time)
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&cap, 0, sizeof(cap));
	cap.name = "server-time";
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_SERVER_TIME);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "time";
	mtag.is_ok = server_time_mtag_is_ok;
	mtag.clicap_handler = c;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_or_inherit_time);

	return MOD_SUCCESS;
}

MOD_LOAD(server-time)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(server-time)
{
	return MOD_SUCCESS;
}

/** This function verifies if the client sending
 * 'server-time' is permitted to do so and uses a permitted
 * syntax.
 * We simply allow server-time ONLY from servers and with any syntax.
 */
int server_time_mtag_is_ok(aClient *acptr, char *name, char *value)
{
	if (IsServer(acptr))
		return 1;

	return 0;
}

void mtag_add_or_inherit_time(aClient *sender, MessageTag *recv_mtags, MessageTag **mtag_list, char *signature)
{
	MessageTag *m = find_mtag(recv_mtags, "time");
	if (m)
	{
		m = duplicate_mtag(m);
	} else
	{
		struct timeval t;
		struct tm *tm;
		char buf[64];

		gettimeofday(&t, NULL);
		tm = gmtime(&t.tv_sec);
		snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
			tm->tm_year + 1900,
			tm->tm_mon + 1,
			tm->tm_mday,
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec,
			(int)(t.tv_usec / 1000));

		m = MyMallocEx(sizeof(MessageTag));
		m->name = strdup("time");
		m->value = strdup(buf);
	}
	AddListItem(m, *mtag_list);
}
