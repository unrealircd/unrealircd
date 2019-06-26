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

#define NICK_HASH_TABLE_SIZE 32768
#define CHAN_HASH_TABLE_SIZE 32768
#define WATCH_HASH_TABLE_SIZE 32768
#define WHOWAS_HASH_TABLE_SIZE 32768
#define THROTTLING_HASH_TABLE_SIZE 8192

#define find_channel hash_find_channel

#endif /* __hash_include__ */
