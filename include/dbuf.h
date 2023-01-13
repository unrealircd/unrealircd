/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/dbuf.h
 *   Copyright (C) 1990 Markku Savela
 *   Copyright (C) 2013 William Pitcock <nenolod@dereferenced.org>
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

#ifndef __dbuf_include__
#define __dbuf_include__

#include "list.h"

/* 512 bytes -- 510 character bytes + \r\n, per rfc1459 */
#define DBUF_BLOCK_SIZE		(512)

/*
** dbuf is a collection of functions which can be used to
** maintain a dynamic buffering of a byte stream.
** Functions allocate and release memory dynamically as
** required [Actually, there is nothing that prevents
** this package maintaining the buffer on disk, either]
*/

/*
** These structure definitions are only here to be used
** as a whole, *DO NOT EVER REFER TO THESE FIELDS INSIDE
** THE STRUCTURES*! It must be possible to change the internal
** implementation of this package without changing the
** interface.
*/
typedef struct dbuf {
	u_int length;		/* Current number of bytes stored */
//	u_int offset;		/* Offset to the first byte */
	struct list_head dbuf_list;
} dbuf;

/*
** And this 'dbufbuf' should never be referenced outside the
** implementation of 'dbuf'--would be "hidden" if C had such
** keyword...
** If it was possible, this would compile to be exactly 1 memory
** page in size. 2048 bytes seems to be the most common size, so
** as long as a pointer is 4 bytes, we get 2032 bytes for buffer
** data after we take away a bit for malloc to play with. -avalon
*/
typedef struct dbufbuf {
	struct list_head dbuf_node;
	size_t size;
	char data[DBUF_BLOCK_SIZE];
} dbufbuf;

/*
** dbuf_put
**	Append the number of bytes to the buffer, allocating more
**	memory as needed. Bytes are copied into internal buffers
**	from users buffer.
*/
void dbuf_put(dbuf *, const char *, size_t);
					/* Dynamic buffer header */
					/* Pointer to data to be stored */
					/* Number of bytes to store */

void dbuf_delete(dbuf *, size_t);
					/* Dynamic buffer header */
					/* Number of bytes to delete */

/*
** DBufLength
**	Return the current number of bytes stored into the buffer.
**	(One should use this instead of referencing the internal
**	length field explicitly...)
*/
#define DBufLength(dyn) ((dyn)->length)

/*
** DBufClear
**	Scratch the current content of the buffer. Release all
**	allocated buffers and make it empty.
*/
#define DBufClear(dyn)	dbuf_delete((dyn),DBufLength(dyn))

extern int dbuf_getmsg(dbuf *, char *);
extern int dbuf_get(dbuf *dyn, char **buf);
extern void dbuf_queue_init(dbuf *dyn);
extern void dbuf_init(void);

#endif /* __dbuf_include__ */
