/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/events.h
 *   (C) Carsten V. Munk 2001 <stskeeps@tspre.org>
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
 *
 *   $Id$
 */

#define EVENT(x) void (x) (void *data)

void	EventAdd(char *name, long every, long howmany,
		  vFP event, void *data);

typedef struct _event Event;

struct _event {
	char	*name;
	time_t	every;
	long	howmany;
	vFP	event;
	void	*data;
	time_t	last;
};
