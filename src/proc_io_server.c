/************************************************************************
 *   UnrealIRCd - Unreal Internet Relay Chat Daemon - src/proc_io_server.c
 *   (c) 2022- Bram Matthys and The UnrealIRCd team
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

/** @file
 * @brief Inter-process I/O
 */
#include "unrealircd.h"
#include <ares.h>

/* Forward declarations */
CMD_FUNC(procio_status);
CMD_FUNC(procio_modules);
CMD_FUNC(procio_rehash);
CMD_FUNC(procio_exit);
CMD_FUNC(procio_help);
void start_of_control_client_handshake(Client *client);
int procio_accept(Client *client);

/** Create the unrealircd.ctl socket (server-side) */
void add_proc_io_server(void)
{
	ConfigItem_listen *listener;

#ifdef _WIN32
	/* Ignore silently on Windows versions older than W10 build 17061 */
	if (!unix_sockets_capable())
		return;
#endif
	listener = safe_alloc(sizeof(ConfigItem_listen));
	safe_strdup(listener->file, CONTROLFILE);
	listener->socket_type = SOCKET_TYPE_UNIX;
	listener->options = LISTENER_CONTROL|LISTENER_NO_CHECK_CONNECT_FLOOD|LISTENER_NO_CHECK_ZLINED;
	listener->start_handshake = start_of_control_client_handshake;
	listener->fd = -1;
	AddListItem(listener, conf_listen);
	if (add_listener(listener) == -1)
		exit(-1);
	CommandAdd(NULL, "STATUS", procio_status, MAXPARA, CMD_CONTROL);
	CommandAdd(NULL, "MODULES", procio_modules, MAXPARA, CMD_CONTROL);
	CommandAdd(NULL, "REHASH", procio_rehash, MAXPARA, CMD_CONTROL);
	CommandAdd(NULL, "EXIT", procio_exit, MAXPARA, CMD_CONTROL);
	CommandAdd(NULL, "HELP", procio_help, MAXPARA, CMD_CONTROL);
	HookAdd(NULL, HOOKTYPE_ACCEPT, -1000000, procio_accept);
}

int procio_accept(Client *client)
{
	if (client->local->listener->options & LISTENER_CONTROL)
	{
		irccounts.unknown--;
		client->status = CLIENT_STATUS_CONTROL;
		list_del(&client->lclient_node);
		list_add(&client->lclient_node, &control_list);
	}
	return 0;
}

/** Start of "control channel" client handshake - this is minimal
 * @param client	The client
 */
void start_of_control_client_handshake(Client *client)
{
	sendto_one(client, NULL, "READY %s %s", me.name, version);
	fd_setselect(client->local->fd, FD_SELECT_READ, read_packet, client);
}

CMD_FUNC(procio_status)
{
	sendto_one(client, NULL, "REPLY servername %s", me.name);
	sendto_one(client, NULL, "REPLY unrealircd_version %s", version);
	sendto_one(client, NULL, "REPLY libssl_version %s", SSLeay_version(SSLEAY_VERSION));
	sendto_one(client, NULL, "REPLY libsodium_version %s", sodium_version_string());
#ifdef USE_LIBCURL
	sendto_one(client, NULL, "REPLY libcurl_version %s", curl_version());
#endif
	sendto_one(client, NULL, "REPLY libcares_version %s", ares_version(NULL));
	sendto_one(client, NULL, "REPLY libpcre2_version %s", pcre2_version());
	sendto_one(client, NULL, "REPLY global_clients %ld", (long)irccounts.clients);
	sendto_one(client, NULL, "REPLY local_clients %ld", (long)irccounts.me_clients);
	sendto_one(client, NULL, "REPLY operators %ld", (long)irccounts.operators);
	sendto_one(client, NULL, "REPLY servers %ld", (long)irccounts.servers);
	sendto_one(client, NULL, "REPLY channels %ld", (long)irccounts.channels);
	sendto_one(client, NULL, "END 0");
}

extern MODVAR Module *Modules;
CMD_FUNC(procio_modules)
{
	char tmp[1024];
	Module *m;

	for (m = Modules; m; m = m->next)
	{
		tmp[0] = '\0';
		if (m->flags & MODFLAG_DELAYED)
			strlcat(tmp, "[Unloading] ", sizeof(tmp));
		if (m->options & MOD_OPT_PERM_RELOADABLE)
			strlcat(tmp, "[PERM-BUT-RELOADABLE] ", sizeof(tmp));
		if (m->options & MOD_OPT_PERM)
			strlcat(tmp, "[PERM] ", sizeof(tmp));
		if (!(m->options & MOD_OPT_OFFICIAL))
			strlcat(tmp, "[3RD] ", sizeof(tmp));
		sendto_one(client, NULL, "REPLY %s %s - %s - by %s %s",
		           m->header->name,
		           m->header->version,
		           m->header->description,
		           m->header->author,
		           tmp);
	}
	sendto_one(client, NULL, "END 0");
}

CMD_FUNC(procio_rehash)
{
	if (loop.rehashing)
	{
		sendto_one(client, NULL, "REPLY ERROR: A rehash is already in progress");
		sendto_one(client, NULL, "END 1");
		return;
	}
	

	if (parv[1] && !strcmp(parv[1], "-tls"))
	{
		int ret;
		SetMonitorRehash(client);
		unreal_log(ULOG_INFO, "config", "CONFIG_RELOAD_TLS", NULL, "Reloading all TLS related data (./unrealircd reloadtls)");
		ret = reinit_tls();
		sendto_one(client, NULL, "END %d", ret == 0 ? -1 : 0);
		ClearMonitorRehash(client);
	} else {
		SetMonitorRehash(client);
		request_rehash(client);
		/* completion will go via procio_post_rehash() */
	}
}

CMD_FUNC(procio_exit)
{
	sendto_one(client, NULL, "END 0");
	exit_client(client, NULL, "");
}

CMD_FUNC(procio_help)
{
	sendto_one(client, NULL, "REPLY Commands available:");
	sendto_one(client, NULL, "REPLY EXIT");
	sendto_one(client, NULL, "REPLY HELP");
	sendto_one(client, NULL, "REPLY REHASH");
	sendto_one(client, NULL, "REPLY STATUS");
	sendto_one(client, NULL, "REPLY MODULES");
	sendto_one(client, NULL, "END 0");
}

/** Called upon REHASH completion (with or without failure) */
void procio_post_rehash(int failure)
{
	Client *client;

	list_for_each_entry(client, &control_list, lclient_node)
	{
		if (IsMonitorRehash(client))
		{
			sendto_one(client, NULL, "END %d", failure);
			ClearMonitorRehash(client);
		}
	}
}
