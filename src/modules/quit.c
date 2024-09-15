/*
 *   Unreal Internet Relay Chat Daemon, src/modules/quit.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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

/* Defines */
#define MSG_QUIT        "QUIT"  /* QUIT */

/* Structs */
ModuleHeader MOD_HEADER
  = {
	"quit",	/* Name of module */
	"5.0", /* Version */
	"command /quit", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
CMD_FUNC(cmd_quit);
void _exit_client(Client *client, MessageTag *recv_mtags, const char *comment);
void _exit_client_fmt(Client *client, MessageTag *recv_mtags, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf, 3, 4)));
void _exit_client_ex(Client *client, Client *origin, MessageTag *recv_mtags, const char *comment);
void _banned_client(Client *client, const char *bantype, const char *reason, int global, int noexit);
static void remove_dependents(Client *client, Client *from, MessageTag *mtags, const char *comment, const char *splitstr);
static void exit_one_client(Client *, MessageTag *mtags_i, const char *);
static int should_hide_ban_reason(Client *client, const char *reason);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_EXIT_CLIENT, _exit_client);
	EfunctionAddVoid(modinfo->handle, EFUNC_EXIT_CLIENT_FMT, TO_VOIDFUNC(_exit_client_fmt));
	EfunctionAddVoid(modinfo->handle, EFUNC_EXIT_CLIENT_EX, _exit_client_ex);
	EfunctionAddVoid(modinfo->handle, EFUNC_BANNED_CLIENT, _banned_client);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, MSG_QUIT, cmd_quit, 1, CMD_UNREGISTERED|CMD_USER|CMD_VIRUS);
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

/*
** cmd_quit
**	parv[1] = comment
*/
CMD_FUNC(cmd_quit)
{
	const char *comment = (parc > 1 && parv[1]) ? parv[1] : client->name;
	char commentbuf[MAXQUITLEN + 1];
	char commentbuf2[MAXQUITLEN + 1];

	if (parc > 1 && parv[1])
	{
		strlncpy(commentbuf, parv[1], sizeof(commentbuf), iConf.quit_length);
		comment = commentbuf;
	} else {
		comment = client->name;
	}

	if (MyUser(client))
	{
		int n;
		Hook *tmphook;
		const char *str;

		if ((str = get_setting_for_user_string(client, SET_STATIC_QUIT)))
		{
			exit_client(client, recv_mtags, str);
			return;
		}

		if (IsVirus(client))
		{
			exit_client(client, recv_mtags, "Client exited");
			return;
		}

		if (match_spamfilter(client, comment, SPAMF_QUIT, "QUIT", NULL, 0, NULL))
		{
			comment = client->name;
			if (IsDead(client))
				return;
		}
		
		if (!ValidatePermissionsForPath("immune:anti-spam-quit-message-time",client,NULL,NULL,NULL) && ANTI_SPAM_QUIT_MSG_TIME)
		{
			if (client->local->creationtime+ANTI_SPAM_QUIT_MSG_TIME > TStime())
				comment = client->name;
		}

		if (iConf.part_instead_of_quit_on_comment_change && MyUser(client))
		{
			Membership *lp, *lp_next;
			const char *newcomment;
			Channel *channel;

			for (lp = client->user->channel; lp; lp = lp_next)
			{
				channel = lp->channel;
				newcomment = comment;
				lp_next = lp->next;

				for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_QUIT_CHAN]; tmphook; tmphook = tmphook->next)
				{
					newcomment = (*(tmphook->func.stringfunc))(client, channel, comment);
					if (!newcomment)
						break;
				}

				if (newcomment && is_banned(client, channel, BANCHK_LEAVE_MSG, &newcomment, NULL))
					newcomment = NULL;

				/* Comment changed? Then PART the user before we do the QUIT. */
				if (comment != newcomment)
				{
					const char *parx[4];
					char tmp[512];
					int ret;


					parx[0] = NULL;
					parx[1] = channel->name;
					if (newcomment)
					{
						strlcpy(tmp, newcomment, sizeof(tmp));
						parx[2] = tmp;
						parx[3] = NULL;
					} else {
						parx[2] = NULL;
					}

					do_cmd(client, recv_mtags, "PART", newcomment ? 3 : 2, parx);
					/* This would be unusual, but possible (somewhere in the future perhaps): */
					if (IsDead(client))
						return;
				}
			}
		}

		for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_QUIT]; tmphook; tmphook = tmphook->next)
		{
			comment = (*(tmphook->func.stringfunc))(client, comment);
			if (!comment)
			{			
				comment = client->name;
				break;
			}
		}

		if (PREFIX_QUIT)
			snprintf(commentbuf2, sizeof(commentbuf2), "%s: %s", PREFIX_QUIT, comment);
		else
			strlcpy(commentbuf2, comment, sizeof(commentbuf2));

		exit_client(client, recv_mtags, commentbuf2);
	}
	else
	{
		/* Remote quits and non-person quits always use their original comment.
		 * Also pass recv_mtags so to keep the msgid and such.
		 */
		exit_client(client, recv_mtags, comment);
	}
}

/** Exit this IRC client, and all the dependents (users, servers) if this is a server.
 * @param client        The client to exit.
 * @param recv_mtags  Message tags to use as a base (if any).
 * @param comment     The (s)quit message
 */
void _exit_client(Client *client, MessageTag *recv_mtags, const char *comment)
{
	exit_client_ex(client, client->direction, recv_mtags, comment);
}

/** Exit this IRC client, and all the dependents (users, servers) if this is a server.
 * @param client        The client to exit.
 * @param recv_mtags  Message tags to use as a base (if any).
 * @param comment     The (s)quit message
 */
void _exit_client_fmt(Client *client, MessageTag *recv_mtags, FORMAT_STRING(const char *pattern), ...)
{
	char comment[512];

	va_list vl;
	va_start(vl, pattern);
	vsnprintf(comment, sizeof(comment), pattern, vl);
	va_end(vl);

	exit_client_ex(client, client->direction, recv_mtags, comment);
}

/** Exit this IRC client, and all the dependents (users, servers) if this is a server.
 * @param client        The client to exit.
 * @param recv_mtags  Message tags to use as a base (if any).
 * @param comment     The (s)quit message
 */
void _exit_client_ex(Client *client, Client *origin, MessageTag *recv_mtags, const char *comment)
{
	long long on_for;
	ConfigItem_listen *listen_conf;
	MessageTag *mtags_generated = NULL;

	if (IsDead(client))
		return; /* Already marked as exited */

	/* We replace 'recv_mtags' here with a newly
	 * generated id if 'recv_mtags' is NULL or is
	 * non-NULL and contains no msgid etc.
	 * This saves us from doing a new_message()
	 * prior to the exit_client() call at around
	 * 100+ places elsewhere in the code.
	 */
	new_message(client, recv_mtags, &mtags_generated);
	recv_mtags = mtags_generated;

	if (MyConnect(client))
	{
		if (client->local->class)
		{
			client->local->class->clients--;
			if ((client->local->class->flag.temporary) && !client->local->class->clients && !client->local->class->xrefcount)
			{
				delete_classblock(client->local->class);
				client->local->class = NULL;
			}
		}
		if (IsUser(client))
			irccounts.me_clients--;
		if (client->server && client->server->conf)
		{
			client->server->conf->refcount--;
			if (!client->server->conf->refcount
			  && client->server->conf->flag.temporary)
			{
				delete_linkblock(client->server->conf);
				client->server->conf = NULL;
			}
		}
		if (IsServer(client))
		{
			irccounts.me_servers--;
			if (!IsServerDisconnectLogged(client))
			{
				unreal_log(ULOG_ERROR, "link", "LINK_DISCONNECTED", client,
					   "Lost server link to $client [$client.ip]: $reason",
					   log_data_string("reason", comment));
			}
		}
		free_pending_net(client);
		SetClosing(client);
		if (IsUser(client))
		{
			long connected_time = TStime() - client->local->creationtime;
			RunHook(HOOKTYPE_LOCAL_QUIT, client, recv_mtags, comment);
			unreal_log(ULOG_INFO, "connect", "LOCAL_CLIENT_DISCONNECT", client,
				   "Client exiting: $client ($client.user.username@$client.hostname) [$client.ip] ($reason)",
				   log_data_string("extended_client_info", get_connect_extinfo(client)),
				   log_data_string("reason", comment),
				   log_data_integer("connected_time", connected_time));
		} else
		if (IsUnknown(client))
		{
			RunHook(HOOKTYPE_UNKUSER_QUIT, client, recv_mtags, comment);
		}

		if (client->local->fd >= 0 && !IsConnecting(client))
		{
			if (!IsControl(client) && !IsRPC(client))
				sendto_one(client, NULL, "ERROR :Closing Link: %s (%s)", get_client_name(client, FALSE), comment);
		}
		close_connection(client);
	}
	else if (IsUser(client) && !IsULine(client))
	{
		if (client->uplink != &me)
		{
			unreal_log(ULOG_INFO, "connect", "REMOTE_CLIENT_DISCONNECT", client,
				   "Client exiting: $client ($client.user.username@$client.hostname) [$client.ip] ($reason)",
				   log_data_string("extended_client_info", get_connect_extinfo(client)),
				   log_data_string("reason", comment),
				   log_data_string("from_server_name", client->user->server));
		}
	}

	/*
	 * Recurse down the client list and get rid of clients who are no
	 * longer connected to the network (from my point of view)
	 * Only do this expensive stuff if exited==server -Donwulff
	 */
	if (IsServer(client))
	{
		char splitstr[HOSTLEN + HOSTLEN + 2];
		Client *acptr, *next;

		assert(client->server != NULL && client->uplink != NULL);

		if (FLAT_MAP)
			strlcpy(splitstr, "*.net *.split", sizeof splitstr);
		else
			ircsnprintf(splitstr, sizeof splitstr, "%s %s", client->uplink->name, client->name);

		remove_dependents(client, origin, recv_mtags, comment, splitstr);

		/* Special case for remote async RPC, server.rehash in particular.. */
		list_for_each_entry_safe(acptr, next, &rpc_remote_list, client_node)
			if (!strncmp(client->id, acptr->id, SIDLEN))
				free_client(acptr);

		RunHook(HOOKTYPE_SERVER_QUIT, client, recv_mtags);
	}
	else if (IsUser(client) && !IsKilled(client))
	{
		sendto_server(client, 0, 0, recv_mtags, ":%s QUIT :%s", client->id, comment);
	}

	/* Finally, the client/server itself exits.. */
	exit_one_client(client, recv_mtags, comment);

	free_message_tags(mtags_generated);
}

static int should_hide_ban_reason(Client *client, const char *reason)
{
	if (HIDE_BAN_REASON == HIDE_BAN_REASON_AUTO)
	{
		/* If we detect the IP address in the ban reason or
		 * it contains an unrealircd.org/ URL then the
		 * ban reason is hidden since it may expose client
		 * details.
		 */
		// First the simple check:
		if (strstr(reason, "unrealircd.org/") ||
		    strstr(reason, client->ip))
		{
			return 1;
		}
		// For IPv6, check compressed IP too:
		if (IsIPV6(client))
		{
			const char *ip = compressed_ip(client->ip);
			if (strstr(reason, ip))
				return 1;
		}
		return 0;
	} else {
		return HIDE_BAN_REASON == HIDE_BAN_REASON_YES ? 1 : 0;
	}
}

/** Delete all DCCALLOW references.
 * Ultimately, this should be moved to modules/dccallow.c
 */
void remove_dcc_references(Client *client)
{
	Client *acptr;
	Link *lp, *nextlp;
	Link **lpp, *tmp;
	int found;

	lp = client->user->dccallow;
	while(lp)
	{
		nextlp = lp->next;
		acptr = lp->value.client;
		for(found = 0, lpp = &(acptr->user->dccallow); *lpp; lpp=&((*lpp)->next))
		{
			if (lp->flags == (*lpp)->flags)
				continue; /* match only opposite types for sanity */
			if ((*lpp)->value.client == client)
			{
				if ((*lpp)->flags == DCC_LINK_ME)
				{
					sendto_one(acptr, NULL, ":%s %d %s :%s has been removed from "
						"your DCC allow list for signing off",
						me.name, RPL_DCCINFO, acptr->name, client->name);
				}
				tmp = *lpp;
				*lpp = tmp->next;
				free_link(tmp);
				found++;
				break;
			}
		}

		if (!found)
		{
			unreal_log(ULOG_WARNING, "main", "BUG_REMOVE_DCC_REFERENCES", acptr,
			           "[BUG] remove_dcc_references: $client was in dccallowme "
			           "list of $existing_client but not in dccallowrem list!",
			           log_data_client("existing_client", client));
		}

		free_link(lp);
		lp = nextlp;
	}
}

/*
 * Remove all clients that depend on source_p; assumes all (S)QUITs have
 * already been sent.  we make sure to exit a server's dependent clients
 * and servers before the server itself; exit_one_client takes care of
 * actually removing things off llists.   tweaked from +CSr31  -orabidoo
 */
static void recurse_remove_clients(Client *client, MessageTag *mtags, const char *comment)
{
	Client *acptr, *next;

	list_for_each_entry_safe(acptr, next, &client_list, client_node)
	{
		if (acptr->uplink != client)
			continue;

		exit_one_client(acptr, mtags, comment);
	}

	list_for_each_entry_safe(acptr, next, &global_server_list, client_node)
	{
		if (acptr->uplink != client)
			continue;

		recurse_remove_clients(acptr, mtags, comment);
		exit_one_client(acptr, mtags, comment);
	}
}

/*
** Remove *everything* that depends on source_p, from all lists, and sending
** all necessary QUITs and SQUITs.  source_p itself is still on the lists,
** and its SQUITs have been sent except for the upstream one  -orabidoo
*/
static void remove_dependents(Client *client, Client *from, MessageTag *mtags, const char *comment, const char *splitstr)
{
	Client *acptr;

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if (acptr != from && !(acptr->direction && (acptr->direction == from)))
			sendto_one(acptr, mtags, "SQUIT %s :%s", client->name, comment);
	}

	recurse_remove_clients(client, mtags, splitstr);
}

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
static void exit_one_client(Client *client, MessageTag *mtags_i, const char *comment)
{
	Link *lp;
	Membership *mp;

	assert(!IsMe(client));

	if (IsUser(client))
	{
		MessageTag *mtags_o = NULL;

		if (!MyUser(client))
			RunHook(HOOKTYPE_REMOTE_QUIT, client, mtags_i, comment);

		new_message_special(client, mtags_i, &mtags_o, ":%s QUIT", client->name);
		if (find_mtag(mtags_o, "unrealircd.org/real-quit-reason"))
			quit_sendto_local_common_channels(client, mtags_o, comment);
		else
			sendto_local_common_channels(client, NULL, 0, mtags_o, ":%s QUIT :%s", client->name, comment);
		free_message_tags(mtags_o);

		while ((mp = client->user->channel))
			remove_user_from_channel(client, mp->channel, 1);
		/* again, this is all that is needed */

		/* Clean up dccallow list and (if needed) notify other clients
		 * that have this person on DCCALLOW that the user just left/got removed.
		 */
		remove_dcc_references(client);

		/* For remote clients, we need to check for any outstanding async
		 * connects attached to this 'client', and set those records to NULL.
		 * Why not for local? Well, we already do that in close_connection ;)
		 */
		if (!MyConnect(client))
			unrealdns_delreq_bycptr(client);
	}

	/* Free module related data for this client */
	moddata_free_client(client);
	if (MyConnect(client))
		moddata_free_local_client(client);

	/* Remove client from the client list */
	if (*client->id)
	{
		del_from_id_hash_table(client->id, client);
		*client->id = '\0';
	}
	if (*client->name)
		del_from_client_hash_table(client->name, client);
	if (remote_rehash_client == client)
		remote_rehash_client = NULL; /* client did a /REHASH and QUIT before rehash was complete */
	remove_client_from_list(client);
}

/** Generic function to inform the user he/she has been banned.
 * @param client   The affected client.
 * @param bantype  The ban type, such as: "K-Lined", "G-Lined" or "realname".
 * @param reason   The specified reason.
 * @param global   Whether the ban is global (1) or for this server only (0)
 * @param noexit   Set this to NO_EXIT_CLIENT to make us not call exit_client().
 *                 This is really only needed from the accept code, do not
 *                 use it anywhere else. No really, never.
 *
 * @note This function will call exit_client() appropriately.
 */
void _banned_client(Client *client, const char *bantype, const char *reason, int global, int noexit)
{
	char buf[512];
	char *fmt = global ? iConf.reject_message_gline : iConf.reject_message_kline;
	const char *vars[6], *values[6];
	MessageTag *mtags = NULL;

	if (!MyConnect(client))
		abort();

	/* This was: "You are not welcome on this %s. %s: %s. %s" but is now dynamic: */
	vars[0] = "bantype";
	values[0] = bantype;
	vars[1] = "banreason";
	values[1] = reason;
	vars[2] = "klineaddr";
	values[2] = KLINE_ADDRESS;
	vars[3] = "glineaddr";
	values[3] = GLINE_ADDRESS ? GLINE_ADDRESS : KLINE_ADDRESS; /* fallback to klineaddr */
	vars[4] = "ip";
	values[4] = GetIP(client);
	vars[5] = NULL;
	values[5] = NULL;
	buildvarstring(fmt, buf, sizeof(buf), vars, values);

	/* This is a bit extensive but we will send both a YOUAREBANNEDCREEP
	 * and a notice to the user.
	 * The YOUAREBANNEDCREEP will be helpful for the client since it makes
	 * clear the user should not quickly reconnect, as (s)he is banned.
	 * The notice still needs to be there because it stands out well
	 * at most IRC clients.
	 */
	if (noexit != NO_EXIT_CLIENT)
	{
		sendnumeric(client, ERR_YOUREBANNEDCREEP, buf);
		sendnotice(client, "%s", buf);
	}

	/* The final message in the ERROR is shorter. */
	if (IsRegistered(client) && should_hide_ban_reason(client, reason))
	{
		/* Hide the ban reason, but put the real reason in unrealircd.org/real-quit-reason */
		MessageTag *m = safe_alloc(sizeof(MessageTag));
		safe_strdup(m->name, "unrealircd.org/real-quit-reason");
		snprintf(buf, sizeof(buf), "Banned (%s): %s", bantype, reason);
		safe_strdup(m->value, buf);
		AddListItem(m, mtags);
		/* And the quit reason for anyone else, goes here.. */
		snprintf(buf, sizeof(buf), "Banned (%s)", bantype);
	} else {
		snprintf(buf, sizeof(buf), "Banned (%s): %s", bantype, reason);
	}

	if (noexit != NO_EXIT_CLIENT)
	{
		exit_client(client, mtags, buf);
	} else {
		/* Special handling for direct Z-line code */
		client->flags |= CLIENT_FLAG_DEADSOCKET_IS_BANNED;
		dead_socket(client, buf);
	}
	safe_free_message_tags(mtags);
}
