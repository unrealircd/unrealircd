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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

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

DLLFUNC int MOD_INIT(m_starttls)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_STARTTLS, NULL, m_starttls, MAXPARA, M_UNREGISTERED);
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

DLLFUNC CMD_FUNC(m_starttls)
{
	if (!MyConnect(sptr) || !IsUnknown(sptr))
		return 0;
#ifndef USE_SSL
	/* sendnotice(sptr, "This server does not support SSL"); */
	/* or numeric 691? */
	/* actually... it's probably best to just act like we don't know this command...? */
	sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "STARTTLS");
	return 0;
#else
	if (iConf.ssl_options & SSLFLAG_NOSTARTTLS)
	{
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "STARTTLS");
		return 0;
	}
	if (IsSecure(sptr))
	{
		sendto_one(sptr, ":%s 691 %s :STARTTLS failed. Already using TLS.", me.name, sptr->name);
		return 0;
	}
	dbuf_delete(&sptr->recvQ, 1000000); /* Clear up any remaining plaintext commands */
	sendto_one(sptr, ":%s 670 %s :STARTTLS successful, go ahead with TLS handshake", me.name, sptr->name);
	// ^^ FIXME, use: RPL_STARTTLS
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
	sendto_one(sptr, ":%s 691 %s :STARTTLS failed", me.name, sptr->name); // FIXME, use: ERR_STARTTLS
	sptr->ssl = NULL;
	sptr->flags &= ~FLAGS_SSL;
	SetUnknown(sptr);
	return 0;
#endif
}
