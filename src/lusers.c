/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/lusers.c
 *      (C) Carsten Munk <stskeeps@tspre.org> 2000
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

ID_Copyright("(C) Carsten Munk 2000");

ircstats IRCstats;

#ifdef IRC_COMMENT
int  clients;			/* total */
int  invisible;			/* invisible */
int  servers;			/* servers */
int  operators;			/* operators */
int  unknown;			/* unknown local connections */
int  channels;			/* channels */
int  me_clients;		/* my clients */
int  me_servers;		/* my servers */
int  me_max;			/* local max */
int  global_max;		/* global max */
#endif

void init_ircstats(void)
{
	bzero(&IRCstats, sizeof(IRCstats));
	IRCstats.servers = 1;
}
