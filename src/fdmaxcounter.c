/*
 *   Unreal Internet Relay Chat Daemon, src/fdmaxcounter.c
 *   Copyright (C) 2000 Carsten V. Munk <stskeeps@tspre.org>
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

#include <sys/types.h>
#include <sys/socket.h>
main()
{
	int  i;
	int  s;

	for (i = 1; i <= 10000; i++)
	{
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0)
		{
			printf("Max fds is %i\n", i + 2);
			exit(-1);
		}
	}
}
