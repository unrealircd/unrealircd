/************************************************************************
 *   IRC - Internet Relay Chat, include/sjoin.h
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


typedef struct  SynchList aSynchList;

/* SJOIN synch structure */
struct SynchList {
        char            nick[NICKLEN];
        int             deop;
        int             devoice;
	int		dehalf;
 	int		deown;
 	int		deprot;
        int             op;
        int             voice;
	int		half;
	int		own;
	int		prot;
        aSynchList      *next, *prev;
};

aSynchList *SJSynchList = NULL;

aSynchList *make_synchlist()
{
    Reg1 aSynchList *synchptr;

    synchptr = (aSynchList *) MyMalloc(sizeof(aSynchList));

    synchptr->nick[0] = 0;
    synchptr->deop = synchptr->dehalf = synchptr->deown = synchptr->deprot = 0;
    synchptr->devoice = 0;
    synchptr->op = 0;
    synchptr->voice = synchptr->half = synchptr->own = synchptr->prot = 0;
    synchptr->prev = synchptr->next = NULL;

    return synchptr;
}

void free_synchlist(synchptr)
    aSynchList *synchptr;
{
    MyFree((char *) synchptr);
}

