/*
 *   IRC - Internet Relay Chat, src/modules/staff.c
 *   cmd_staff: Displays a file(/URL) when the /STAFF command is used.
 *   (C) Copyright 2004-2016 Syzop <syzop@vulnscan.org>
 *   (C) Copyright 2003-2004 AngryWolf <angrywolf@flashmail.com>
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
	"staff",
	"3.8",
	"/STAFF command",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

#define MSG_STAFF	"STAFF"

#define DEF_STAFF_FILE   CONFDIR "/network.staff"
#define STAFF_FILE       (staff_file ? staff_file : DEF_STAFF_FILE)

#define RPL_STAFF        ":%s 700 %s :- %s"
#define RPL_STAFFSTART   ":%s 701 %s :- %s IRC Network Staff Information -"
#define RPL_ENDOFSTAFF   ":%s 702 %s :End of /STAFF command."
#define RPL_NOSTAFF      ":%s 703 %s :Network Staff File is missing"

/* Forward declarations */
static void unload_motd_file(MOTDFile *list);
CMD_FUNC(cmd_staff);
static int cb_test(ConfigFile *, ConfigEntry *, int, int *);
static int cb_conf(ConfigFile *, ConfigEntry *, int);
static int cb_stats(Client *client, const char *flag);
static void FreeConf();

static MOTDFile staff;
static char *staff_file = NULL;

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, cb_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	memset(&staff, 0, sizeof(staff));

	CommandAdd(modinfo->handle, MSG_STAFF, cmd_staff, MAXPARA, CMD_USER);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, cb_conf);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, cb_stats);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	FreeConf();
	unload_motd_file(&staff);

	return MOD_SUCCESS;
}

static void FreeConf()
{
	safe_free(staff_file);
}

static void unload_motd_file(MOTDFile *list)
{
	MOTDLine *old, *new;

	if (!list)
		return;

	new = list->lines;

	if (!new)
		return;

	while (new)
	{
		old = new->next;
		safe_free(new->line);
		safe_free(new);
		new = old;
	}
}

static int cb_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;

	if (type == CONFIG_SET)
	{
		if (!strcmp(ce->name, "staff-file"))
		{
			*errs = errors;
			return errors ? -1 : 1;
		}
	}

	return 0;
}

static int cb_conf(ConfigFile *cf, ConfigEntry *ce, int type)
{
	if (type == CONFIG_SET)
	{
		if (!strcmp(ce->name, "staff-file"))
		{
			convert_to_absolute_path(&ce->value, CONFDIR);
			read_motd(ce->value, &staff);
			return 1;
		}
	}

	return 0;
}

static int cb_stats(Client *client, const char *flag)
{
	if (*flag == 'S')
	{
		sendtxtnumeric(client, "staff-file: %s", STAFF_FILE);
		return 1;
	}

	return 0;
}

/** The routine that actual does the /STAFF command */
CMD_FUNC(cmd_staff)
{
	MOTDFile *temp;
	MOTDLine *aLine;

	if (!IsUser(client))
		return;

	if (hunt_server(client, recv_mtags, "STAFF", 1, parc, parv) != HUNTED_ISME)
		return;

	if (!staff.lines)
	{
		sendto_one(client, NULL, RPL_NOSTAFF, me.name, client->name);
		return;
	}

	sendto_one(client, NULL, RPL_STAFFSTART, me.name, client->name, NETWORK_NAME);

	temp = &staff;

	for (aLine = temp->lines; aLine; aLine = aLine->next)
		sendto_one(client, NULL, RPL_STAFF, me.name, client->name, aLine->line);

	sendto_one(client, NULL, RPL_ENDOFSTAFF, me.name, client->name);
}
