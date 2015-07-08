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
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef _WIN32
#include "version.h"
#endif
#include "m_cap.h"

DLLFUNC CMD_FUNC(m_starttls);

#define MSG_STARTTLS 	"STARTTLS"	

ModuleHeader MOD_HEADER(m_starttls)
  = {
	"m_starttls",
	"$Id$",
	"command /starttls", 
	"3.2-b8-1",
	NULL 
    };

static void m_starttls_caplist(struct list_head *head);

MOD_INIT(m_starttls)
{
	CommandAdd(modinfo->handle, MSG_STARTTLS, m_starttls, MAXPARA, M_UNREGISTERED|M_ANNOUNCE);

	HookAddVoid(modinfo->handle, HOOKTYPE_CAPLIST, 0, m_starttls_caplist);

	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_starttls)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_starttls)
{
	return MOD_SUCCESS;
}

static void m_starttls_caplist(struct list_head *head)
{
ClientCapability *cap;

	cap = MyMallocEx(sizeof(ClientCapability));
	cap->name = strdup("tls");
	cap->cap = PROTO_STARTTLS,
	clicap_append(head, cap); /* this is wrong.. head? and unfreed */
	/* todo: free */
}

DLLFUNC CMD_FUNC(m_starttls)
{
	if (!MyConnect(sptr) || !IsUnknown(sptr))
		return 0;
	if (!ctx_server) /* or SSL support is not enabled (failed to load cert/keys/..)... */
	{
		/* Pretend STARTTLS is an unknown command, this is the safest approach */
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "STARTTLS");
		return 0;
	}

	if (iConf.ssl_options & SSLFLAG_NOSTARTTLS)
	{
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "STARTTLS");
		return 0;
	}

	if (IsSecure(sptr))
	{
		sendto_one(sptr, err_str(ERR_STARTTLS), me.name, !BadPtr(sptr->name) ? sptr->name : "*", "STARTTLS failed. Already using TLS.");
		return 0;
	}

	dbuf_delete(&sptr->recvQ, DBufLength(&sptr->recvQ)); /* Clear up any remaining plaintext commands */
	sendto_one(sptr, rpl_str(RPL_STARTTLS), me.name, !BadPtr(sptr->name) ? sptr->name : "*");
	send_queued(sptr);

	SetSSLStartTLSHandshake(sptr);
	Debug((DEBUG_DEBUG, "Starting SSL handshake (due to STARTTLS) for %s", sptr->sockhost));
	if ((sptr->ssl = SSL_new(ctx_server)) == NULL)
		goto fail;
	sptr->flags |= FLAGS_SSL;
	SSL_set_fd(sptr->ssl, sptr->fd);
	SSL_set_nonblocking(sptr->ssl);
	if (!ircd_SSL_accept(sptr, sptr->fd)) {
		Debug((DEBUG_DEBUG, "Failed SSL accept handshake in instance 1: %s", sptr->sockhost));
		SSL_set_shutdown(sptr->ssl, SSL_RECEIVED_SHUTDOWN);
		SSL_smart_shutdown(sptr->ssl);
		SSL_free(sptr->ssl);
		goto fail;
	}

	/* HANDSHAKE IN PROGRESS */
	return 0;
fail:
	/* Failure */
	sendto_one(sptr, err_str(ERR_STARTTLS), me.name, !BadPtr(sptr->name) ? sptr->name : "*", "STARTTLS failed");
	sptr->ssl = NULL;
	sptr->flags &= ~FLAGS_SSL;
	SetUnknown(sptr);
	return 0;
}
