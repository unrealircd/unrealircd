/************************************************************************
 *   IRC - Internet Relay Chat, include/whowas.h
 *   Copyright (C) 1990  Markku Savela
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

/*
 * from original rcs 
 * $ Id: whowas.h,v 6.1 1991/07/04 21:04:39 gruner stable gruner $
 *
 * $ Log: whowas.h,v $
 * Revision 6.1  1991/07/04  21:04:39  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:05:08  gruner
 * frozen beta revision 2.6.1
 *
 * th+hybrid rcs version
 * $Id$
 */

#ifndef	__whowas_include__
#define __whowas_include__

/* NOTE: Don't reorder values of these, as they are used in whowasdb */
typedef enum WhoWasEvent {
    WHOWAS_EVENT_QUIT=0,
    WHOWAS_EVENT_NICK_CHANGE=1,
    WHOWAS_EVENT_SERVER_TERMINATING=2
} WhoWasEvent;
#define WHOWAS_LOWEST_EVENT 0
#define WHOWAS_HIGHEST_EVENT 2

/*
** add_history
**	Add the currently defined name of the client to history.
**	usually called before changing to a new name (nick).
**	Client must be a fully registered user (specifically,
**	the user structure must have been allocated).
*/
void add_history(Client *, int, WhoWasEvent);

/*
** off_history
**	This must be called when the client structure is about to
**	be released. History mechanism keeps pointers to client
**	structures and it must know when they cease to exist. This
**	also implicitly calls AddHistory.
*/
void off_history(Client *);

/*
** get_history
**	Return the current client that was using the given
**	nickname within the timelimit. Returns NULL, if no
**	one found...
*/
Client *get_history(const char *, time_t);
					/* Nick name */
					/* Time limit in seconds */

/*
** for debugging...counts related structures stored in whowas array.
*/
void count_whowas_memory(int *, u_long *);

#endif /* __whowas_include__ */
