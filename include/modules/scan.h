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

 /*
 * Settings:
 * 
 * SCAN_AT_ONCE
 *  - How many the scanning module can support scanning at once
 * Memory will be SCAN_AT_ONCE * (HOSTLENGTH + 1)
*/

#define SCAN_AT_ONCE 100
#define SCAN_HOSTLENGTH 30


typedef struct _HStruct HStruct;
typedef struct _VHStruct VHStruct;

struct _HStruct
{
	char			host[SCAN_HOSTLENGTH];
	unsigned char	refcnt;
};

struct _VHStruct
{
	char			host[SCAN_HOSTLENGTH];
	char			reason[50];
};

#ifndef IS_SCAN_C
extern HStruct			Hosts[SCAN_AT_ONCE];
extern VHStruct		VHosts[SCAN_AT_ONCE];

/* 
 * If it is legal to edit Hosts table
*/
MUTEX		*xHSlock;
MUTEX		*xVSlock;
#endif
/* Some prototypes .. aint they sweet? */
DLLFUNC int			h_scan_connect(aClient *sptr);
DLLFUNC EVENT		(HS_Cleanup);
DLLFUNC EVENT		(VS_Ban);
DLLFUNC int			h_scan_info(aClient *sptr);
DLLFUNC int			m_scan(aClient *cptr, aClient *sptr, int parc, char *parv[]);
VHStruct			*VS_Add(char *host, char *reason);