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
#include "modules.h"

ID_Copyright("(C) Carsten Munk 2001");

#ifndef HAVE_NO_THREADS
#include "threads.h"
MUTEX			sys_EventLock;
#endif


Event *events = NULL;

void	LockEventSystem(void)
{
#ifndef HAVE_NO_THREADS
	IRCMutexLock(sys_EventLock);
#endif
}

void	UnlockEventSystem(void)
{
#ifndef HAVE_NO_THREADS
	IRCMutexUnLock(sys_EventLock);
#endif
}


Event	*EventAddEx(Module *module, char *name, long every, long howmany,
		  vFP event, void *data)
{
	Event *newevent;
	
	if (!name || (every < 0) || (howmany < 0) || !event)
	{
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
	newevent->owner = module;
	AddListItem(newevent,events);
	if (module) {
		ModuleObject *eventobj = (ModuleObject *)MyMallocEx(sizeof(ModuleObject));
		eventobj->object.event = newevent;
		eventobj->type = MOBJ_EVENT;
		AddListItem(eventobj, module->objects);
	}
	return newevent;
	
}

Event	*EventDel(Event *event)
{
	Event *p, *q;
	for (p = events; p; p = p->next) {
		if (p == event) {
			q = p->next;
			MyFree(p->name);
			DelListItem(p, events);
			if (p->owner) {
				ModuleObject *eventobjs;
				for (eventobjs = p->owner->objects; eventobjs; eventobjs = eventobjs->next) {
					if (eventobjs->type == MOBJ_EVENT && eventobjs->object.event == p) {
						DelListItem(eventobjs, p->owner->objects);
						MyFree(eventobjs);
						break;
					}
				}
			}
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

int EventMod(Event *event, EventInfo *mods) {
	if (!event || !mods)
		return -1;

	if (mods->flags & EMOD_EVERY)
		event->every = mods->every;
	if (mods->flags & EMOD_HOWMANY)
		event->howmany = mods->howmany;
	if (mods->flags & EMOD_NAME) {
		free(event->name);
		event->name = strdup(mods->name);
	}
	if (mods->flags & EMOD_EVENT)
		event->event = mods->event;
	if (mods->flags & EMOD_DATA)
		event->data = mods->data;
	return 0;
}

inline void	DoEvents(void)
{
	Event *eventptr;
	Event temp;

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
					temp.next = EventDel(eventptr);
					eventptr = &temp;
					continue;
				}
			}

		}
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
	LockEventSystem();

	/* Start events */
	EventAddEx(NULL, "tklexpire", 5, 0, tkl_check_expire, NULL);
	EventAddEx(NULL, "tunefile", 300, 0, save_tunefile, NULL);
	EventAddEx(NULL, "garbage", GARBAGE_COLLECT_EVERY, 0, garbage_collect, NULL);
	EventAddEx(NULL, "loop", 0, 0, loop_event, NULL);
#ifndef NO_FDLIST
	EventAddEx(NULL, "fdlistcheck", 1, 0, e_check_fdlists, NULL);
#endif
	UnlockEventSystem();
}
