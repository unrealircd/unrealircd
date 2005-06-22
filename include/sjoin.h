/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/sjoin.h
 *   (C) Carsten Munk 2000
 *   Contains code from StarChat IRCd, (C) their respective authors
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


typedef struct SynchList aSynchList;

/* SJOIN synch structure */
struct SynchList {
	aClient *cptr;
	long setflags;
	aSynchList *next, *prev;
};

MODVAR aSynchList *SJSynchList = NULL;

aSynchList *make_synchlist()
{
	register aSynchList *synchptr;

	synchptr = (aSynchList *) MyMalloc(sizeof(aSynchList));
	synchptr->cptr = NULL;
	synchptr->setflags = 0;
	synchptr->prev = synchptr->next = NULL;

	return synchptr;
}

void free_synchlist(synchptr)
	aSynchList *synchptr;
{
	MyFree((char *)synchptr);
}
