/************************************************************************
 *   Unreal Internet Relay Chat Daemo, src/aln.c
 *   (C) 2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
 *   Copyright (C) 2000 Lucas Madar [bahamut team]
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

#ifndef STANDALONE
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "version.h"
#endif

#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#ifndef STANDALONE
#include "h.h"
#include "proto.h"
ID_Copyright("(C) Carsten Munk 2000");
#endif


aClient *find_server_quick_search(char *name)
{
	aClient *lp;

	list_for_each_entry(lp, &global_server_list, client_node)
		if (!match(name, lp->name))
			return lp;
	return NULL;
}


aClient *find_server_quick_straight(char *name)
{
	aClient *lp;

	list_for_each_entry(lp, &global_server_list, client_node)
		if (!strcmp(name, lp->name))
			return lp;

	return NULL;
}



aClient *find_server_quickx(char *name, aClient *cptr)
{
	return find_server(name, cptr);
}
