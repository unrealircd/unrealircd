/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/hash.h
 *   Copyright (C) 1991 Darren Reed
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

#ifndef	__hash_include__
#define __hash_include__

typedef struct hashentry {
	int  hits;
	int  links;
	void *list;
} aHashEntry;

/* Taner had BITS_PER_COL 4 BITS_PER_COL_MASK 0xF - Dianora */

#define BITS_PER_COL 3
#define BITS_PER_COL_MASK 0x7
#define MAX_SUB     (1<<BITS_PER_COL)

/* Client hash table 
 * used in hash.c 
 */

#define U_MAX_INITIAL 2048
#define U_MAX_INITIAL_MASK (U_MAX_INITIAL-1)
#define U_MAX (U_MAX_INITIAL*MAX_SUB)

/* Channel hash table 
 * used in hash.c 
 */

#define CH_MAX_INITIAL  2048
#define CH_MAX_INITIAL_MASK (CH_MAX_INITIAL-1)
#define CH_MAX (CH_MAX_INITIAL*MAX_SUB)

/* Who was hash table 
 * used in whowas.c 
 */

#define WW_MAX_INITIAL  16
#define WW_MAX_INITIAL_MASK (WW_MAX_INITIAL-1)
#define WW_MAX (WW_MAX_INITIAL*MAX_SUB)

#define WATCHHASHSIZE  10007	/* prime number  */

/*
 * Throttling
*/
#define THROTTLING_HASH_SIZE	1019 /* prime number */


#define NullChn ((aChannel *)0)

#define find_channel hash_find_channel

#endif /* __hash_include__ */
