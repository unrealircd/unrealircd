/************************************************************************
 *   IRC - Internet Relay Chat, events.c
 *   (C) 2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "version.h"
#include "proto.h"
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

ID_Copyright("(C) Carsten Munk 2001");

#ifndef HAVE_NO_THREADS
#include "threads.h"
MUTEX			sys_EventLock;
#endif


Event *events = NULL;


Event	*EventAdd(char *name, long every, long howmany,
		  vFP event, void *data)
{
	Event *newevent;

#ifndef HAVE_NO_THREADS
	IRCMutexLock(sys_EventLock);
#endif
	
	if (!name || (every < 0) || (howmany < 0) || !event)
	{
#ifndef HAVE_NO_THREADS
		IRCMutexUnlock(sys_EventLock);
#endif

		return NULL;
	}
	newevent = (Event *) MyMallocEx(sizeof(Event));
	newevent->name = strdup(name);
	newevent->howmany = howmany;
	newevent->every = every;
	newevent->event = event;
	newevent->data = data;
	/* We don't want a quick execution */
	newevent->last = TStime();
	AddListItem(newevent,events);
#ifndef HAVE_NO_THREADS
	IRCMutexUnlock(sys_EventLock);
#endif
	return newevent;
	
}

Event	*EventDel(Event *event)
{
	Event *p, *q;
	
	for (p = events; p; p = p->next)
	{
		if (p == event)
		{
			q = p->next;
			MyFree(p->name);
			DelListItem(p, events);
			MyFree(p);
			return q;		
		}
	}
	return NULL;
}

Event	*EventFind(char *name)
{
	Event *eventptr;

	for (eventptr = events; eventptr; eventptr = eventptr->next)
		if (!strcmp(eventptr->name, name))
			return (eventptr);
	return NULL;
}

void	EventModEvery(Event *event, long every)
{
	Event *eventptr;

#ifndef HAVE_NO_THREADS
	IRCMutexLock(sys_EventLock);
#endif
	if (event)
		event->every = every;

#ifndef HAVE_NO_THREADS
	IRCMutexUnlock(sys_EventLock);
#endif

}

inline void	DoEvents(void)
{
	Event *eventptr;
	Event temp;

#ifndef HAVE_NO_THREADS
	IRCMutexLock(sys_EventLock);
#endif
	for (eventptr = events; eventptr; eventptr = eventptr->next)
		if ((eventptr->every == 0) || ((TStime() - eventptr->last) >= eventptr->every))
		{
			eventptr->last = TStime();
			(*eventptr->event)(eventptr->data);
			if (eventptr->howmany > 0)
			{
				eventptr->howmany--;
				if (eventptr->howmany == 0)
				{
					temp.next = EventDel(eventptr->name);
					eventptr = &temp;
					continue;
				}
			}

		}
	
#ifndef HAVE_NO_THREADS
	IRCMutexUnlock(sys_EventLock);
#endif

}

void	EventStatus(aClient *sptr)
{
	Event *eventptr;

	if (!events)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** No events",
				me.name, sptr->name);
	}
	for (eventptr = events; eventptr; eventptr = eventptr->next)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** Event %s: e/%i h/%i n/%i l/%i", me.name,
			sptr->name, eventptr->name, eventptr->every, eventptr->howmany,
				TStime() - eventptr->last, (eventptr->last + eventptr->every) - TStime());
	}
}
extern char EVENT_CRC[];

void	SetupEvents(void)
{
	/* We're doomed! */
	if (match(EVENT_HASHES, &EVENT_CRC[EVENT_HASHVALUE]))
		exit (0);
#ifndef HAVE_NO_THREADS
	IRCCreateMutex(sys_EventLock);
#endif
	/* Start events */
	EventAdd("tklexpire", 5, 0, tkl_check_expire, NULL);
	EventAdd("tunefile", 300, 0, save_tunefile, NULL);
	EventAdd("garbage", GARBAGE_COLLECT_EVERY, 0, garbage_collect, NULL);
	EventAdd("loop", 0, 0, loop_event, NULL);
#ifndef NO_FDLIST
	EventAdd("fdlistcheck", 1, 0, e_check_fdlists, NULL);
#endif
}
