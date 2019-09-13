/*
 *   IRC - Internet Relay Chat, src/modules/m_starttls.c
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

CMD_FUNC(m_starttls);

#define MSG_STARTTLS 	"STARTTLS"	

ModuleHeader MOD_HEADER
  = {
	"starttls",
	"5.0",
	"command /starttls", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

long CLICAP_STARTTLS;

MOD_INIT()
{
	ClientCapabilityInfo cap;
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, MSG_STARTTLS, m_starttls, MAXPARA, M_UNREGISTERED);
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

CMD_FUNC(m_starttls)
{
	SSL_CTX *ctx;
	int tls_options;

	if (!MyConnect(sptr) || !IsUnknown(sptr))
		return 0;

	ctx = sptr->local->listener->ssl_ctx ? sptr->local->listener->ssl_ctx : ctx_server;
	tls_options = sptr->local->listener->tls_options ? sptr->local->listener->tls_options->options : iConf.tls_options->options;

	/* Is SSL support enabled? (may not, if failed to load cert/keys/..) */
	if (!ctx)
	{
		/* Pretend STARTTLS is an unknown command, this is the safest approach */
		sendnumeric(sptr, ERR_NOTREGISTERED);
		return 0;
	}

	/* Is STARTTLS disabled? (same response as above) */
	if (tls_options & TLSFLAG_NOSTARTTLS)
	{
		sendnumeric(sptr, ERR_NOTREGISTERED);
		return 0;
	}

	if (IsSecure(sptr))
	{
		sendnumeric(sptr, ERR_STARTTLS, "STARTTLS failed. Already using TLS.");
		return 0;
	}

	dbuf_delete(&sptr->local->recvQ, DBufLength(&sptr->local->recvQ)); /* Clear up any remaining plaintext commands */
	sendnumeric(sptr, RPL_STARTTLS);
	send_queued(sptr);

	SetStartTLSHandshake(sptr);
	Debug((DEBUG_DEBUG, "Starting SSL handshake (due to STARTTLS) for %s", sptr->local->sockhost));
	if ((sptr->local->ssl = SSL_new(ctx)) == NULL)
		goto fail;
	SetTLS(sptr);
	SSL_set_fd(sptr->local->ssl, sptr->local->fd);
	SSL_set_nonblocking(sptr->local->ssl);
	if (!ircd_SSL_accept(sptr, sptr->local->fd)) {
		Debug((DEBUG_DEBUG, "Failed SSL accept handshake in instance 1: %s", sptr->local->sockhost));
		SSL_set_shutdown(sptr->local->ssl, SSL_RECEIVED_SHUTDOWN);
		SSL_smart_shutdown(sptr->local->ssl);
		SSL_free(sptr->local->ssl);
		goto fail;
	}

	/* HANDSHAKE IN PROGRESS */
	return 0;
fail:
	/* Failure */
	sendnumeric(sptr, ERR_STARTTLS, "STARTTLS failed");
	sptr->local->ssl = NULL;
	ClearTLS(sptr);
	SetUnknown(sptr);
	return 0;
}
