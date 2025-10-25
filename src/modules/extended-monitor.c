/*
 *   IRC - Internet Relay Chat, src/modules/extended-monitor.c
 *   (C) 2021 The UnrealIRCd Team
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

/* Global variables */
long CAP_EXTENDED_MONITOR = 0L;

/* externally looked up: */
long CAP_AWAY_NOTIFY = 0;
long CAP_ACCOUNT_NOTIFY = 0;
long CAP_CHGHOST = 0;
long CAP_SETNAME = 0;

/* Forward declarations */
int extended_monitor_away(Client *client, MessageTag *mtags, const char *reason, int already_as_away);
int extended_monitor_account_login(Client *client, MessageTag *mtags);
int extended_monitor_userhost_change(Client *client, const char *olduser, const char *oldhost);
int extended_monitor_realname_change(Client *client, const char *oldinfo);
int extended_monitor_notification(Client *client, Watch *watch, Link *lp, int event, void *data);

ModuleHeader MOD_HEADER
  = {
	"extended-monitor",
	"5.0",
	"extended functionality for /monitor", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	
	MARK_AS_OFFICIAL_MODULE(modinfo);

	ModDataInfo mreq;

	memset(&cap, 0, sizeof(cap));
	cap.name = "extended-monitor";
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_EXTENDED_MONITOR);
	if (!c)
	{
		config_error("[%s] Failed to request extended-monitor cap: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}

	HookAdd(modinfo->handle, HOOKTYPE_AWAY, 0, extended_monitor_away);
	HookAdd(modinfo->handle, HOOKTYPE_ACCOUNT_LOGIN, 0, extended_monitor_account_login);
	HookAdd(modinfo->handle, HOOKTYPE_USERHOST_CHANGE, 0, extended_monitor_userhost_change);
	HookAdd(modinfo->handle, HOOKTYPE_REALNAME_CHANGE, 0, extended_monitor_realname_change);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	CAP_AWAY_NOTIFY = ClientCapabilityBit("away-notify");
	CAP_ACCOUNT_NOTIFY = ClientCapabilityBit("account-notify");
	CAP_CHGHOST = ClientCapabilityBit("chghost");
	CAP_SETNAME = ClientCapabilityBit("setname");
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

int extended_monitor_away(Client *client, MessageTag *mtags, const char *reason, int already_as_away)
{
	if (reason)
		watch_check(client, WATCH_EVENT_AWAY, NULL, extended_monitor_notification);
	else
		watch_check(client, WATCH_EVENT_NOTAWAY, NULL, extended_monitor_notification);

	return 0;
}

int extended_monitor_account_login(Client *client, MessageTag *mtags)
{
	if (IsLoggedIn(client))
		watch_check(client, WATCH_EVENT_LOGGEDIN, NULL, extended_monitor_notification);
	else
		watch_check(client, WATCH_EVENT_LOGGEDOUT, NULL, extended_monitor_notification);

	return 0;
}

int extended_monitor_userhost_change(Client *client, const char *olduser, const char *oldhost)
{
	watch_check(client, WATCH_EVENT_USERHOST, NULL, extended_monitor_notification);
	return 0;
}

int extended_monitor_realname_change(Client *client, const char *oldinfo)
{
	watch_check(client, WATCH_EVENT_REALNAME, NULL, extended_monitor_notification);
	return 0;
}

int extended_monitor_notification(Client *client, Watch *watch, Link *lp, int event, void *data)
{
	if (!(lp->flags & WATCH_FLAG_TYPE_MONITOR))
		return 0;

	if (!HasCapabilityFast(lp->value.client, CAP_EXTENDED_MONITOR))
		return 0; /* this client does not support our notifications */

	if (has_common_channels(client, lp->value.client))
		return 0; /* will be notified anyway */

	switch (event)
	{
		case WATCH_EVENT_AWAY:
			if (HasCapabilityFast(lp->value.client, CAP_AWAY_NOTIFY))
				sendto_prefix_one(lp->value.client, client, NULL, ":%s AWAY :%s", client->name, client->user->away);
			break;
		case WATCH_EVENT_NOTAWAY:
			if (HasCapabilityFast(lp->value.client, CAP_AWAY_NOTIFY))
				sendto_prefix_one(lp->value.client, client, NULL, ":%s AWAY", client->name);
			break;
		case WATCH_EVENT_LOGGEDIN:
			if (HasCapabilityFast(lp->value.client, CAP_ACCOUNT_NOTIFY))
				sendto_prefix_one(lp->value.client, client, NULL, ":%s ACCOUNT :%s", client->name, client->user->account);
			break;
		case WATCH_EVENT_LOGGEDOUT:
			if (HasCapabilityFast(lp->value.client, CAP_ACCOUNT_NOTIFY))
				sendto_prefix_one(lp->value.client, client, NULL, ":%s ACCOUNT :*", client->name);
			break;
		case WATCH_EVENT_USERHOST:
			if (HasCapabilityFast(lp->value.client, CAP_CHGHOST))
				sendto_prefix_one(lp->value.client, client, NULL, ":%s CHGHOST %s %s", client->name, client->user->username, GetHost(client));
			break;
		case WATCH_EVENT_REALNAME:
			if (HasCapabilityFast(lp->value.client, CAP_SETNAME))
				sendto_prefix_one(lp->value.client, client, NULL, ":%s SETNAME :%s", client->name, client->info);
			break;
		default:
			break;
	}
	
	return 0;
}

