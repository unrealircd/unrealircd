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
 * $Id$
 *
 * $Log$
 * Revision 1.1.1.1  2000/01/30 12:16:34  stskeeps
 * Begin of CVS at cvs.unreal.sourceforge.net
 *
 *
 * Revision 1.1.1.1  1999/09/01 23:20:37  stskeeps
 *
 * Revision 1.1.1.1  1999/07/22 13:56:41  stskeeps
 * 16:56 22-07-99 techie
 * - Started on using CVS to develop Unreal
 *
 *
 * Revision 1.1.1.1  1999/07/21 10:48:18  stskeeps
 * 12:47 GMT+2 21 July 1999 - Techie
 * Starting Unreal with CVS.. 
 *
 *
 * Revision 1.2  1997/12/29 07:17:35  wd
 * df4.6.2
 * ee CHANGES for updates
 * -wd
 *
 * Revision 1.1.1.1  1997/08/22 17:23:01  donwulff
 * Original import from the "deadlined" version.
 *
 * Revision 1.1.1.1  1996/11/18 07:53:42  explorer
 * ircd 4.3.3 -- about time
 *
 * Revision 1.1.1.1.4.1  1996/09/16 02:45:41  donwulff
 * *** empty log message ***
 *
 * Revision 6.1  1991/07/04  21:04:39  gruner
 * Revision 2.6.1 [released]
 *
 * Revision 6.0  1991/07/04  18:05:08  gruner
 * frozen beta revision 2.6.1
 *
 */

#ifndef	__whowas_include__
#define __whowas_include__

#ifndef PROTO
#if __STDC__
#	define PROTO(x)	x
#else
#	define PROTO(x) ()
#endif /* __STDC__ */
#endif /* ! PROTO */

/*
** WHOWAS structure moved here from whowas.c
*/
typedef struct aname {
	anUser	*ww_user;
	aClient	*ww_online;
	time_t	ww_logout;
        long    ww_umodes;
	char	ww_nick[NICKLEN+1];
	char	ww_info[REALLEN+1];
} aName;

/*
** add_history
**	Add the currently defined name of the client to history.
**	usually called before changing to a new name (nick).
**	Client must be a fully registered user (specifically,
**	the user structure must have been allocated).
*/
void	add_history PROTO((aClient *));

/*
** off_history
**	This must be called when the client structure is about to
**	be released. History mechanism keeps pointers to client
**	structures and it must know when they cease to exist. This
**	also implicitly calls AddHistory.
*/
void	off_history PROTO((aClient *));

/*
** get_history
**	Return the current client that was using the given
**	nickname within the timelimit. Returns NULL, if no
**	one found...
*/
aClient	*get_history PROTO((char *, time_t));
					/* Nick name */
					/* Time limit in seconds */

int	m_whowas PROTO((aClient *, aClient *, int, char *[]));

/*
** for debugging...counts related structures stored in whowas array.
*/
void	count_whowas_memory PROTO((int *, int *, u_long *));

#endif /* __whowas_include__ */
