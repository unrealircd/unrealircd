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

DLLFUNC int MOD_INIT(m_starttls)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_STARTTLS, m_starttls, MAXPARA, M_UNREGISTERED|M_ANNOUNCE);

	HookAddVoidEx(modinfo->handle, HOOKTYPE_CAPLIST, m_starttls_caplist);

	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_starttls)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_starttls)(int module_unload)
{
	return MOD_SUCCESS;
}

static void m_starttls_caplist(struct list_head *head)
{
#ifdef USE_SSL
ClientCapability *cap;

	cap = MyMallocEx(sizeof(ClientCapability));
	cap->name = strdup("tls");
	cap->cap = PROTO_STARTTLS,
	clicap_append(head, cap); /* this is wrong.. head? and unfreed */
	/* todo: free */
#endif
}

DLLFUNC CMD_FUNC(m_starttls)
{
	if (!MyConnect(sptr) || !IsUnknown(sptr))
		return 0;
#ifndef USE_SSL
	if (1) /* if not compiled with SSL support... */
#else
	if (!ctx_server) /* or SSL support is not enabled (failed to load cert/keys/..)... */
#endif
	{
		/* Pretend STARTTLS is an unknown command, this is the safest approach */
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.client.name, "STARTTLS");
		return 0;
	}

#ifdef USE_SSL
	if (iConf.ssl_options & SSLFLAG_NOSTARTTLS)
	{
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.client.name, "STARTTLS");
		return 0;
	}

	if (IsSecure(sptr))
	{
		sendto_one(sptr, err_str(ERR_STARTTLS), me.client.name, !BadPtr(sptr->name) ? sptr->name : "*", "STARTTLS failed. Already using TLS.");
		return 0;
	}

	dbuf_delete(&sptr->localClient->recvQ, 1000000); /* Clear up any remaining plaintext commands */
	sendto_one(sptr, rpl_str(RPL_STARTTLS), me.client.name, !BadPtr(sptr->name) ? sptr->name : "*");
	send_queued(sptr);

	SetSSLStartTLSHandshake(sptr);
	Debug((DEBUG_DEBUG, "Starting SSL handshake (due to STARTTLS) for %s", sptr->localClient->sockhost));
	if ((sptr->localClient->ssl = SSL_new(ctx_server)) == NULL)
		goto fail;
	sptr->flags |= FLAGS_SSL;
	SSL_set_fd(sptr->localClient->ssl, sptr->fd);
	SSL_set_nonblocking(sptr->localClient->ssl);
	if (!ircd_SSL_accept(sptr, sptr->fd)) {
		Debug((DEBUG_DEBUG, "Failed SSL accept handshake in instance 1: %s", sptr->localClient->sockhost));
		SSL_set_shutdown(sptr->localClient->ssl, SSL_RECEIVED_SHUTDOWN);
		SSL_smart_shutdown(sptr->localClient->ssl);
		SSL_free(sptr->localClient->ssl);
		goto fail;
	}

	/* HANDSHAKE IN PROGRESS */
	return 0;
fail:
	/* Failure */
	sendto_one(sptr, err_str(ERR_STARTTLS), me.client.name, !BadPtr(sptr->name) ? sptr->name : "*", "STARTTLS failed");
	sptr->localClient->ssl = NULL;
	sptr->flags &= ~FLAGS_SSL;
	SetUnknown(sptr);
	return 0;
#endif
}
