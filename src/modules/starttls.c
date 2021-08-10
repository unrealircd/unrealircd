/*
 *   IRC - Internet Relay Chat, src/modules/starttls.c
 *   (C) 2009 Syzop & The UnrealIRCd Team
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

CMD_FUNC(cmd_starttls);

#define MSG_STARTTLS 	"STARTTLS"	

ModuleHeader MOD_HEADER
  = {
	"starttls",
	"5.0",
	"command /starttls", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

long CLICAP_STARTTLS;

MOD_INIT()
{
	ClientCapabilityInfo cap;
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, MSG_STARTTLS, cmd_starttls, MAXPARA, CMD_UNREGISTERED);
	memset(&cap, 0, sizeof(cap));
	cap.name = "tls";
	ClientCapabilityAdd(modinfo->handle, &cap, &CLICAP_STARTTLS);

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

CMD_FUNC(cmd_starttls)
{
	SSL_CTX *ctx;
	int tls_options;

	if (!MyConnect(client) || !IsUnknown(client))
		return;

	ctx = client->local->listener->ssl_ctx ? client->local->listener->ssl_ctx : ctx_server;
	tls_options = client->local->listener->tls_options ? client->local->listener->tls_options->options : iConf.tls_options->options;

	/* This should never happen? */
	if (!ctx)
	{
		/* Pretend STARTTLS is an unknown command, this is the safest approach */
		sendnumeric(client, ERR_NOTREGISTERED);
		return;
	}

	/* Is STARTTLS disabled? (same response as above) */
	if (tls_options & TLSFLAG_NOSTARTTLS)
	{
		sendnumeric(client, ERR_NOTREGISTERED);
		return;
	}

	if (IsSecure(client))
	{
		sendnumeric(client, ERR_STARTTLS, "STARTTLS failed. Already using TLS.");
		return;
	}

	dbuf_delete(&client->local->recvQ, DBufLength(&client->local->recvQ)); /* Clear up any remaining plaintext commands */
	sendnumeric(client, RPL_STARTTLS);
	send_queued(client);

	SetStartTLSHandshake(client);
	if ((client->local->ssl = SSL_new(ctx)) == NULL)
		goto fail;
	SetTLS(client);
	SSL_set_fd(client->local->ssl, client->local->fd);
	SSL_set_nonblocking(client->local->ssl);
	if (!unreal_tls_accept(client, client->local->fd))
	{
		SSL_set_shutdown(client->local->ssl, SSL_RECEIVED_SHUTDOWN);
		SSL_smart_shutdown(client->local->ssl);
		SSL_free(client->local->ssl);
		goto fail;
	}

	/* HANDSHAKE IN PROGRESS */
	return;
fail:
	/* Failure */
	sendnumeric(client, ERR_STARTTLS, "STARTTLS failed");
	client->local->ssl = NULL;
	ClearTLS(client);
	SetUnknown(client);
}
