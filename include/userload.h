/****************************************************************************
 *  Userload module by Michael L. VanLoon (mlv) <michaelv@iastate.edu>
 *  Written 2/93.  Originally grafted into irc2.7.2g 4/93.
 *
 *   Unreal Internet Relay Chat Daemon, ircd/userload.h
 *   Copyright (C) 1990 University of Oulu, Computing Center
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
 ****************************************************************************/

/* $Id$ */

/* This needs to be defined for the counts to be correct--it should be the
 * default anyway, as opers shouldn't be superior to lusers except where
 * absolutely necessary, and here it isn't necessary.                       */
#ifndef SHOW_INVISIBLE_LUSERS
#define SHOW_INVISIBLE_LUSERS
#endif

struct current_load_struct {
  u_short client_count, local_count, conn_count;
  u_long  entries;
};  

extern struct current_load_struct current_load_data;

struct load_entry {
  struct  load_entry *prev;
  u_short client_count, local_count, conn_count;
#ifdef DEBUGMODE
  u_short cpu_usage;
#endif
  long    time_incr;
};

extern struct load_entry *load_list_head, *load_list_tail,
                         *load_free_head, *load_free_tail;


extern void initload PROTO ((void));
extern void update_load PROTO ((void));
extern void calc_load PROTO ((aClient *, char *));
