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

typedef struct _event Event;
typedef struct _eventinfo EventInfo;

struct _event {
	Event   *prev, *next;
	char	*name;
	time_t	every;
	long	howmany;
	vFP		event;
	void	*data;
	time_t	last;
};

#define EMOD_EVERY 0x0001
#define EMOD_HOWMANY 0x0002
#define EMOD_NAME 0x0004
#define EMOD_EVENT 0x0008
#define EMOD_DATA 0x0010

struct _eventinfo {
	int flags;
	long howmany;
	time_t every;
	char *name;
	vFP event;
	void *data;
};

Event	*EventAdd(char *name, long every, long howmany,
		  vFP event, void *data);
Event	*EventDel(Event *event);

Event	*EventFind(char *name);

int 	EventMod(Event *event, EventInfo *mods);

void	DoEvents(void);

void	EventStatus(aClient *sptr);

void	SetupEvents(void);

