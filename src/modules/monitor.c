/*
 *   IRC - Internet Relay Chat, src/modules/monitor.c
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

#define MSG_MONITOR 	"MONITOR"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

CMD_FUNC(cmd_monitor);
char *monitor_isupport_param(void);
int monitor_nickchange(Client *client, MessageTag *mtags, const char *newnick);
int monitor_post_nickchange(Client *client, MessageTag *mtags, const char *oldnick);
int monitor_quit(Client *client, MessageTag *mtags, const char *comment);
int monitor_connect(Client *client);
int monitor_notification(Client *client, Watch *watch, Link *lp, int event, void *data);

ModuleHeader MOD_HEADER
  = {
	"monitor",
	"5.0",
	"command /monitor", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	CommandAdd(modinfo->handle, MSG_MONITOR, cmd_monitor, 2, CMD_USER);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_NICKCHANGE, 0, monitor_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_NICKCHANGE, 0, monitor_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_POST_LOCAL_NICKCHANGE, 0, monitor_post_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_POST_REMOTE_NICKCHANGE, 0, monitor_post_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_QUIT, 0, monitor_quit);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, monitor_quit);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, monitor_connect);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CONNECT, 0, monitor_connect);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	ISupportAdd(modinfo->handle, "MONITOR", monitor_isupport_param());
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

char *monitor_isupport_param(void)
{
	/* i find it unlikely for a client to use WATCH and MONITOR at the same time, so keep a single limit for both */
	return STR(MAXWATCH);
}

int monitor_nickchange(Client *client, MessageTag *mtags, const char *newnick)
{
	if (!smycmp(client->name, newnick)) // new nick is same as old one, maybe the case changed
		return 0;

	watch_check(client, WATCH_EVENT_OFFLINE, NULL, monitor_notification);
	return 0;
}

int monitor_post_nickchange(Client *client, MessageTag *mtags, const char *oldnick)
{
	if (!smycmp(client->name, oldnick)) // new nick is same as old one, maybe the case changed
		return 0;

	watch_check(client, WATCH_EVENT_ONLINE, NULL, monitor_notification);
	return 0;
}

int monitor_quit(Client *client, MessageTag *mtags, const char *comment)
{
	watch_check(client, WATCH_EVENT_OFFLINE, NULL, monitor_notification);
	return 0;
}

int monitor_connect(Client *client)
{
	watch_check(client, WATCH_EVENT_ONLINE, NULL, monitor_notification);
	return 0;
}

int monitor_notification(Client *client, Watch *watch, Link *lp, int event, void *data)
{
	if (!(lp->flags & WATCH_FLAG_TYPE_MONITOR))
		return 0;

	switch (event)
	{
		case WATCH_EVENT_ONLINE:
			sendnumeric(lp->value.client, RPL_MONONLINE, client->name, client->user->username, GetHost(client));
			RunHook(HOOKTYPE_MONITOR_NOTIFICATION, lp->value.client, client, 1);
			break;
		case WATCH_EVENT_OFFLINE:
			sendnumeric(lp->value.client, RPL_MONOFFLINE, client->name);
			RunHook(HOOKTYPE_MONITOR_NOTIFICATION, lp->value.client, client, 0);
			break;
		default:
			break; /* may be handled by other modules */
	}
	
	return 0;
}

void send_status(Client *client, MessageTag *recv_mtags, const char *nick)
{
	MessageTag *mtags = NULL;
	Client *user;
	user = find_user(nick, NULL);
	new_message(client, recv_mtags, &mtags);
	if (!user){
		sendnumeric(client, RPL_MONOFFLINE, nick);
	} else {
		sendnumeric(client, RPL_MONONLINE, user->name, user->user->username, GetHost(user));
	}
	free_message_tags(mtags);
}

#define WATCHES(client) (moddata_local_client(client, watchCounterMD).i)
#define WATCH(client) (moddata_local_client(client, watchListMD).ptr)

CMD_FUNC(cmd_monitor)
{
	char request[BUFSIZE];
	char cmd;
	char *s, *p = NULL;
	int i;
	int toomany = 0;
	Link *lp;

	if (!MyUser(client))
		return;

	if (parc < 2 || BadPtr(parv[1]))
		cmd = 'l';
	else
		cmd = tolower(*parv[1]);

	ModDataInfo *watchCounterMD = findmoddata_byname("watchCount", MODDATATYPE_LOCAL_CLIENT);
	ModDataInfo *watchListMD = findmoddata_byname("watchList", MODDATATYPE_LOCAL_CLIENT);
	
	if (!watchCounterMD || !watchListMD)
	{
		unreal_log(ULOG_ERROR, "monitor", "WATCH_BACKEND_MISSING", NULL,
		           "[monitor] moddata unavailable. Is the 'watch-backend' module loaded?");
		sendnotice(client, "MONITOR command is not available at this moment. Please try again later.");
		return;
	}
	
	switch(cmd)
	{
		case 'c':
			watch_del_list(client, WATCH_FLAG_TYPE_MONITOR);
			break;
		case 'l':
			lp = WATCH(client);
			while (lp)
			{
				if (!(lp->flags & WATCH_FLAG_TYPE_MONITOR))
				{
					lp = lp->next;
					continue; /* this one is not ours */
				}
				sendnumeric(client, RPL_MONLIST, lp->value.wptr->nick);
				lp = lp->next;
			}

			sendnumeric(client, RPL_ENDOFMONLIST);
			break;
		case 's':
			lp = WATCH(client);
			while (lp)
			{
				if (!(lp->flags & WATCH_FLAG_TYPE_MONITOR))
				{
					lp = lp->next;
					continue; /* this one is not ours */
				}
				send_status(client, recv_mtags, lp->value.wptr->nick);
				lp = lp->next;
			}
			break;
		case '-':
		case '+':
			if (parc < 3 || BadPtr(parv[2]))
				return;
			strlcpy(request, parv[2], sizeof(request));
			for (s = strtoken(&p, request, ","); s; s = strtoken(&p, NULL, ","))
			{
				if (cmd == '-') {
					watch_del(s, client, WATCH_FLAG_TYPE_MONITOR);
				} else {
					if (WATCHES(client) >= MAXWATCH)
					{
						sendnumeric(client, ERR_MONLISTFULL, MAXWATCH, s);
						continue;
					}
					if (do_nick_name(s))
						watch_add(s, client, WATCH_FLAG_TYPE_MONITOR);
					send_status(client, recv_mtags, s);
				}
			}
			break;
	}
}

