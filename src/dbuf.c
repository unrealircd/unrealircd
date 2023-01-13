/*
 * UnrealIRCd, src/dbuf.c
 * Copyright (c) 2013 William Pitcock <nenolod@dereferenced.org>
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

#include "unrealircd.h"

static mp_pool_t *dbuf_bufpool = NULL;

void dbuf_init(void)
{
	dbuf_bufpool = mp_pool_new(sizeof(struct dbufbuf), 512 * 1024);
}

/*
** dbuf_alloc - allocates a dbufbuf structure either from freelist or
** creates a new one.
*/
static dbufbuf *dbuf_alloc(dbuf *dbuf_p)
{
	dbufbuf *ptr;

	assert(dbuf_p != NULL);

	ptr = mp_pool_get(dbuf_bufpool);
	memset(ptr, 0, sizeof(dbufbuf));

	INIT_LIST_HEAD(&ptr->dbuf_node);
	list_add_tail(&ptr->dbuf_node, &dbuf_p->dbuf_list);

	return ptr;
}

/*
** dbuf_free - return a dbufbuf structure to the freelist
*/
static void dbuf_free(dbufbuf *ptr)
{
	assert(ptr != NULL);

	list_del(&ptr->dbuf_node);
	mp_pool_release(ptr);
}

void dbuf_queue_init(dbuf *dyn)
{
	memset(dyn, 0, sizeof(dbuf));
	INIT_LIST_HEAD(&dyn->dbuf_list);
}

void dbuf_put(dbuf *dyn, const char *buf, size_t length)
{
	struct dbufbuf *block;
	size_t amount;

	assert(length > 0);
	if (list_empty(&dyn->dbuf_list))
		dbuf_alloc(dyn);

	while (length > 0)
	{
		block = container_of(dyn->dbuf_list.prev, struct dbufbuf, dbuf_node);

		amount = DBUF_BLOCK_SIZE - block->size;
		if (!amount)
		{
			block = dbuf_alloc(dyn);
			amount = DBUF_BLOCK_SIZE;
		}
		if (amount > length)
			amount = length;

		memcpy(&block->data[block->size], buf, amount);

		length -= amount;
		block->size += amount;
		dyn->length += amount;
		buf += amount;
	}
}

void dbuf_delete(dbuf *dyn, size_t length)
{
	struct dbufbuf *block;

	assert(dyn->length >= length);
	if (length == 0)
		return;

	for (;;)
	{
		if (length == 0)
			return;

		block = container_of(dyn->dbuf_list.next, struct dbufbuf, dbuf_node);
		if (length < block->size)
			break;

		dyn->length -= block->size;
		length -= block->size;
		dbuf_free(block);
	}

	block->size -= length;
	dyn->length -= length;
	memmove(block->data, &block->data[length], block->size);
}

/*
** dbuf_getmsg
**
** Check the buffers to see if there is a string which is terminted with
** either a \r or \n prsent.  If so, copy as much as possible (determined by
** length) into buf and return the amount copied - else return 0.
**
** Partially based on extract_one_line() from ircd-hybrid. --kaniini
*/
int  dbuf_getmsg(dbuf *dyn, char *buf)
{
	dbufbuf *block;
	int line_bytes = 0, empty_bytes = 0, phase = 0;
	unsigned int idx;
	char c;
	char *p = buf;

	/*
	 * Phase 0: "empty" characters before the line
	 * Phase 1: copying the line
	 * Phase 2: "empty" characters after the line
	 *          (delete them as well and free some space in the dbuf)
	 *
	 * Empty characters are CR, LF and space (but, of course, not
	 * in the middle of a line). We try to remove as much of them as we can,
	 * since they simply eat server memory.
	 *
	 * --adx
	 */
	list_for_each_entry2(block, dbufbuf, &dyn->dbuf_list, dbuf_node)
	{
		for (idx = 0; idx < block->size; idx++)
		{
			c = block->data[idx];
			if (c == '\r' || c == '\n' || (c == ' ' && phase != 1))
			{
				empty_bytes++;
				if (phase == 1)
					phase = 2;
			}
			else switch (phase)
			{
				case 0: phase = 1; /* FALLTHROUGH */
				case 1: if (line_bytes++ < READBUFSIZE - 2)
						*p++ = c;
					break;
				case 2: *p = '\0';
					dbuf_delete(dyn, line_bytes + empty_bytes);
					return MIN(line_bytes, READBUFSIZE - 2);
			}
		}
	}

	if (phase != 2)
	{
		/* If we have not reached phase 2 then this is not
		 * not a complete line and it is invalid (return 0).
		 */
		line_bytes = 0;
		*buf = '\0';
	} else {
		/* Zero terminate the string */
		*p = '\0';
	}

	/* Remove what is now unnecessary */
	dbuf_delete(dyn, line_bytes + empty_bytes);
	return MIN(line_bytes, READBUFSIZE - 2);
}

/*
** dbuf_get
**
** Get the entire dbuf buffer as a newly allocated string. There is NO CR/LF processing.
*/
int dbuf_get(dbuf *dyn, char **buf)
{
	dbufbuf *block;
	char *d;
	int bytes = 0;

	/* First calculate the room needed... */
	list_for_each_entry2(block, dbufbuf, &dyn->dbuf_list, dbuf_node)
		bytes += block->size;

	d = *buf = safe_alloc(bytes + 1);

	list_for_each_entry2(block, dbufbuf, &dyn->dbuf_list, dbuf_node)
	{
		memcpy(d, block->data, block->size);
		d += block->size;
	}
	*d = '\0'; /* zero terminate */

	/* Remove what is now unnecessary */
	dbuf_delete(dyn, bytes);
	return bytes;
}
