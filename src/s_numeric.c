/*
 *   Unreal Internet Relay Chat Daemon, src/s_numeric.c
 *   Copyright (C) 1990 Jarkko Oikarinen
 *
 *   Numerous fixes by Markku Savela
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

/* s_numeric.c	2.14 1/30/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "h.h"
#include "proto.h"
#include <string.h>

extern char backupbuf[];
static char buffer[1024];

/*
** DoNumeric (replacement for the old do_numeric)
**
**	parc	number of arguments ('sender' counted as one!)
**	parv[1]..parv[parc-1]
**		pointers to additional parameters, this is a NULL
**		terminated list (parv[parc] == NULL).
**
** *WARNING*
**	Numerics are mostly error reports. If there is something
**	wrong with the message, just *DROP* it! Don't even think of
**	sending back a neat error message -- big danger of creating
**	a ping pong error message...
*/
int  do_numeric(int numeric, aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	aChannel *chptr;
	char *nick, *p;
	int  i;

	if (!IsServer(sptr) && !IsPerson(sptr) && IsHandshake(cptr) && sptr->serv && !IsServerSent(sptr))
	{
		/* This is an outgoing server connect that is currently not yet IsServer() but in 'unknown' state.
		 * We need to handle a few responses here.
		 */
		
		/* If we get a numeric 451 (not registered) back for the magic command __PANGPANG__
		 * Then this means we are dealing with an Unreal server <3.2.9 and we should send the
		 * SERVER command right now.
		 */
		if ((numeric == 451) && (parc > 2) && strstr(parv[1], "__PANGPANG__"))
		{
			send_server_message(sptr);
			return 0;
		}

		/* STARTTLS: unknown command */
		if ((numeric == 451) && (parc > 2) && strstr(parv[1], "STARTTLS"))
		{
			if (cptr->serv->conf && (cptr->serv->conf->outgoing.options & CONNECT_INSECURE))
				start_server_handshake(cptr);
			else
				reject_insecure_server(cptr);
			return 0;
		}

		/* STARTTLS failed */
		if (numeric == 691)
		{
			sendto_umode(UMODE_OPER, "STARTTLS failed for link %s. Please check the other side of the link.", cptr->name);
			reject_insecure_server(cptr);
			return 0;
		}

		/* STARTTLS OK */
		if (numeric == 670)
		{
			int ret = client_starttls(cptr);
			if (ret < 0)
			{
				sendto_umode(UMODE_OPER, "STARTTLS handshake failed for link %s. Strange.", cptr->name);
				reject_insecure_server(cptr);
				return ret;
			}
			/* We don't call start_server_handshake() here. First the TLS handshake will
			 * be completed, then completed_connection() will be called for a second time,
			 * which will call completed_connection() from there.
			 */
			return 0;
		}
	}

	if (parc < 1 || !IsServer(sptr))
		return 0;
	/* Remap low number numerics. */
	if (numeric < 100)
		numeric += 100;

	/*
	   ** Prepare the parameter portion of the message into 'buffer'.
	   ** (Because the buffer is twice as large as the message buffer
	   ** for the socket, no overflow can occur here... ...on current
	   ** assumptions--bets are off, if these are changed --msa)
	   ** Note: if buffer is non-empty, it will begin with SPACE.
	 */
	buffer[0] = '\0';
	if (parc > 2)
	{
		/*
		 * For strlcat nazis, please read above
		*/
		for (i = 2; i < (parc - 1); i++)
		{
			(void)strcat(buffer, " ");
			(void)strcat(buffer, parv[i]);
		}
		(void)strcat(buffer, " :");
		(void)strcat(buffer, parv[parc - 1]);
	}
	else
		sendto_realops("do_numeric( %i, %s, %s, %i, { %s, %s } )!",
		    numeric, cptr->name, sptr->name, parc,
		    sptr->name, parv[1] ? parv[1] : "<null>");

	if (BadPtr(parv[1]))
		return 0;

	for (; (nick = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	{
		if ((acptr = find_client(nick, (aClient *)NULL)))
		{
			/*
			   ** Drop to bit bucket if for me...
			   ** ...one might consider sendto_ops
			   ** here... --msa
			   ** And so it was done. -avalon
			   ** And regretted. Dont do it that way. Make sure
			   ** it goes only to non-servers. -avalon
			   ** Check added to make sure servers don't try to loop
			   ** with numerics which can happen with nick collisions.
			   ** - Avalon
			 */
			if (!IsMe(acptr) && IsPerson(acptr))
			{
				/* Added for .U3.2. drop remote 'You are not on
				   ** that channel', we should be synced anyway,
				   ** and this is an annoying message with TSpre7
				   ** still on the net; would result in numeric 442 for
				   ** every KICK... Can be removed when TSpre7 is gone.
				   ** --Run
				   if (numeric==ERR_NOTONCHANNEL) return 0;
				 */

				sendto_prefix_one(acptr, sptr, ":%s %d %s%s",
				    sptr->name, numeric, nick, buffer);
			}
			else if (IsServer(acptr) && acptr->from != cptr)
				sendto_prefix_one(acptr, sptr, ":%s %d %s%s",
				    sptr->name, numeric, nick, buffer);
		}
		else if ((acptr = find_server_quick(nick)))
		{
			if (!IsMe(acptr) && acptr->from != cptr)
				sendto_prefix_one(acptr, sptr, ":%s %d %s%s",
				    sptr->name, numeric, nick, buffer);
		}
		else if ((chptr = find_channel(nick, (aChannel *)NULL)))
			sendto_channel_butone(cptr, sptr, chptr, ":%s %d %s%s",
			    sptr->name, numeric, chptr->chname, buffer);
	}
	return 0;
}
