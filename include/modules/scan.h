/*
 *   IRC - Internet Relay Chat, include/modules/scan.h
 *   (C) 2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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

/* We need it */
#include "threads.h"

typedef struct _Scan_Result {
	struct IN_ADDR	in;
	char	reason[60];
} Scan_Result;

typedef struct _Scan_AddrStruct
{
	struct _Scan_AddrStruct *prev, *next;
	struct IN_ADDR 		in;
	unsigned char		refcnt;
	MUTEX		 	lock;
} Scan_AddrStruct;


extern EVENT(e_scannings_clean);
 