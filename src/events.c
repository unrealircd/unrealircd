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


MODVAR Event *events = NULL;

extern EVENT(unrealdns_removeoldrecords);

void	LockEventSystem(void)
{
}

void	UnlockEventSystem(void)
{
}


Event	*EventAddEx(Module *module, char *name, long every, long howmany,
		  vFP event, void *data)
{
	Event *newevent;
	if (!name || (every < 0) || (howmany < 0) || !event)
	{
		if (module)
			module->errorcode = MODERR_INVALID;
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
		module->errorcode = MODERR_NOERROR;
	}
	return newevent;
	
}

Event	*EventMarkDel(Event *event)
{
	event->howmany = -1;
	return event;
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
	{
		if (event && event->owner)
			event->owner->errorcode = MODERR_INVALID;
		return -1;
	}

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
	if (event->owner)
		event->owner->errorcode = MODERR_NOERROR;
	return 0;
}

#ifndef _WIN32
inline void	DoEvents(void)
#else
void DoEvents(void)
#endif
{
	Event *eventptr;
	Event temp;

	for (eventptr = events; eventptr; eventptr = eventptr->next)
	{
		if (eventptr->howmany == -1)
			goto freeit;
		if ((eventptr->every == 0) || ((TStime() - eventptr->last) >= eventptr->every))
		{
			eventptr->last = TStime();
			(*eventptr->event)(eventptr->data);
			if (eventptr->howmany > 0)
			{
				eventptr->howmany--;
				if (eventptr->howmany == 0)
				{
freeit:
					temp.next = EventDel(eventptr);
					eventptr = &temp;
					continue;
				}
			}
		}
	}
}

void	EventStatus(aClient *sptr)
{
	Event *eventptr;
	time_t now = TStime();
	
	if (!events)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** No events",
				me.name, sptr->name);
		return;
	}
	for (eventptr = events; eventptr; eventptr = eventptr->next)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** Event %s: e/%ld h/%ld n/%ld l/%ld", me.name,
			sptr->name, eventptr->name, eventptr->every, eventptr->howmany,
				now - eventptr->last, (eventptr->last + eventptr->every) - now);
	}
}

void	SetupEvents(void)
{
	LockEventSystem();

	/* Start events */
	EventAddEx(NULL, "tunefile", 300, 0, save_tunefile, NULL);
	EventAddEx(NULL, "garbage", GARBAGE_COLLECT_EVERY, 0, garbage_collect, NULL);
	EventAddEx(NULL, "loop", 0, 0, loop_event, NULL);
	EventAddEx(NULL, "unrealdns_removeoldrecords", 15, 0, unrealdns_removeoldrecords, NULL);
	EventAddEx(NULL, "check_pings", 1, 0, check_pings, NULL);
	EventAddEx(NULL, "check_deadsockets", 1, 0, check_deadsockets, NULL);
	EventAddEx(NULL, "check_unknowns", 1, 0, check_unknowns, NULL);
	EventAddEx(NULL, "try_connections", 2, 0, try_connections, NULL);

	UnlockEventSystem();
}
