/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/dbuf.h
 *   Copyright (C) 1990 Markku Savela
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
	u_int offset;		/* Offset to the first byte */
	struct dbufbuf *head;	/* First data buffer, if length > 0 */
	/* added by mnystrom@mit.edu: */
	struct dbufbuf *tail;	/* last data buffer, if length > 0 */
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
	struct dbufbuf *next;	/* Next data buffer, NULL if this is last */
	char data[2032];	/* Actual data stored here */
} dbufbuf;

/*
** dbuf_put
**	Append the number of bytes to the buffer, allocating more
**	memory as needed. Bytes are copied into internal buffers
**	from users buffer.
**
**	returns	> 0, if operation successfull
**		< 0, if failed (due memory allocation problem)
*/
int dbuf_put(dbuf *, char *, int);
					/* Dynamic buffer header */
					/* Pointer to data to be stored */
					/* Number of bytes to store */

/*
** dbuf_get
**	Remove number of bytes from the buffer, releasing dynamic
**	memory, if applicaple. Bytes are copied from internal buffers
**	to users buffer.
**
**	returns	the number of bytes actually copied to users buffer,
**		if >= 0, any value less than the size of the users
**		buffer indicates the dbuf became empty by this operation.
**
**		Return 0 indicates that buffer was already empty.
**
**		Negative return values indicate some unspecified
**		error condition, rather fatal...
*/
int dbuf_get(dbuf *, char *, int);
				/* Dynamic buffer header */
				/* Pointer to buffer to receive the data */
				/* Max amount of bytes that can be received */

/*
** dbuf_map, dbuf_delete
**	These functions are meant to be used in pairs and offer
**	a more efficient way of emptying the buffer than the
**	normal 'dbuf_get' would allow--less copying needed.
**
**	map	returns a pointer to a largest contiguous section
**		of bytes in front of the buffer, the length of the
**		section is placed into the indicated "long int"
**		variable. Returns NULL *and* zero length, if the
**		buffer is empty.
**
**	delete	removes the specified number of bytes from the
**		front of the buffer releasing any memory used for them.
**
**	Example use (ignoring empty condition here ;)
**
**		buf = dbuf_map(&dyn, &count);
**		<process N bytes (N <= count) of data pointed by 'buf'>
**		dbuf_delete(&dyn, N);
**
**	Note: 	delete can be used alone, there is no real binding
**		between map and delete functions...
*/
char *dbuf_map(dbuf *, int *);
					/* Dynamic buffer header */
					/* Return number of bytes accessible */

int dbuf_delete(dbuf *, int);
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

extern int dbuf_getmsg(dbuf *, char *, int);

#endif /* __dbuf_include__ */
