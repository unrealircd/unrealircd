/*
 *   IRC - Internet Relay Chat, src/modules/scan_socks.c
 *   (C) 2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
 *
 *   SOCKS4 scanning module for scan.so
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
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "userload.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif
#include "modules/scan.h"
#include "modules/blackhole.h"

#ifndef SCAN_ON_PORT
#define SCAN_ON_PORT 1080
#endif

iFP			xVS_add = NULL;
ConfigItem_blackhole	*blackhole_conf = NULL;
void	scan_socks_scan(HStruct *h);

ModuleInfo scan_socks_info
  = {
  	1,
	"scan_socks",	/* Name of module */
	"$Id$", /* Version */
	"scanning API: socks", /* Short description of module */
	NULL, /* Pointer to our dlopen() return value */
	NULL 
    };

/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_init(void)
#else
void    scan_socks_init(void)
#endif
{
	/* extern variable to export scan_socks_info to temporary
           ModuleInfo *modulebuffer;
	   the module_load() will use this to add to the modules linked 
	   list
	*/
	xHSlock = (MUTEX *) module_sym("HSlock");
	xVSlock = (MUTEX *) module_sym("VSlock");
	xVS_add = (iFP) module_sym("VS_Add");
	
	if (!xHSlock || !xVSlock || !xVS_add)
	{
		module_buffer = NULL;
		config_error("scan_socks: i depend on scan.so being loaded");
		return;
	}	
	
	blackhole_conf = (ConfigItem_blackhole *) module_sym("blackhole_conf");
	if (!blackhole_conf)
	{
		module_buffer = NULL;
		config_error("scan_socks: i depend on blackhole.so being loaded");
		return;
	}
	module_buffer = &scan_socks_info;
	
	/*
	 * Add scanning hooks
	*/
	add_HookX(HOOKTYPE_SCAN_HOST, NULL, scan_socks_scan); 
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_load(void)
#else
void    scan_socks_load(void)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	scan_socks_unload(void)
#endif
{
	del_HookX(HOOKTYPE_SCAN_HOST, NULL, scan_socks_scan);
}

#define HICHAR(s)	(((unsigned short) s) >> 8)
#define LOCHAR(s)	(((unsigned short) s) & 0xFF)

void	scan_socks_scan(HStruct *h)
{
	int			retval;
	char			host[SCAN_HOSTLENGTH];
	struct			SOCKADDR_IN sin;
	SOCKET				fd;
	int				sinlen = sizeof(struct SOCKADDR_IN);
	unsigned short	sport = blackhole_conf->port;
	unsigned char   socksbuf[12];
	unsigned long   theip;
	fd_set			rfds;
	struct timeval  tv;
	int				len;
	/* Get host */
	IRCMutexLock((*xHSlock));
	strcpy(host, h->host);
	IRCMutexUnlock((*xHSlock));
	
#ifndef INET6
	sin.SIN_ADDR.S_ADDR = inet_addr(host);
#else
	inet_pton(AF_INET6, host, sin.SIN_ADDR.S_ADDR);
#endif 
	if ((fd = socket(AFINET, SOCK_STREAM, 0)) < 0)
	{
		goto exituniverse;
		return;
	}

	sin.SIN_PORT = htons(SCAN_ON_PORT);
	sin.SIN_FAMILY = AFINET;
	/* We do this blocking. */
	if (connect(fd, (struct sockaddr *)&sin,
		sizeof(sin)) == -1)
	{
		/* we have no socks server! */
		CLOSE_SOCK(fd);	
		goto exituniverse;
		return;
	}
	
	/* We wait for write-ready */
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if (!select(fd + 1, NULL, &rfds, NULL, &tv))
	{
		CLOSE_SOCK(fd);
		goto exituniverse;
	}
				
	sin.SIN_ADDR.S_ADDR = inet_addr(blackhole_conf->ip);
	theip = htonl(sin.SIN_ADDR.S_ADDR);
	bzero(socksbuf, sizeof(socksbuf));
	socksbuf[0] = 4;
	socksbuf[1] = 1;
	socksbuf[2] = HICHAR(sport);
	socksbuf[3] = LOCHAR(sport);
	socksbuf[4] = (theip >> 24);
	socksbuf[5] = (theip >> 16) & 0xFF;
	socksbuf[6] = (theip >> 8) & 0xFF;
	socksbuf[7] = theip & 0xFF;
	
	if ((retval = send(fd, socksbuf, 9, 0)) != 9)
	{
		CLOSE_SOCK(fd);
		goto exituniverse;
	}
	/* Now we wait for data. 30 secs ought to be enough  */
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if (retval = select(fd + 1, &rfds, NULL, NULL, &tv))
	{
		/* There's data in the jar. Let's read it */
		len = recv(fd, socksbuf, 9, 0);
		CLOSE_SOCK(fd);
		if (len < 4)
		{
			goto exituniverse;
		}
		if (socksbuf[1] == 90)
		{
			/* We found SOCKS. */
			IRCMutexLock((*xVSlock));
			(*xVS_add)(host, "Open SOCKS4/5 server");
			IRCMutexUnlock((*xVSlock));
			goto exituniverse;
		}
	}
	else
	{
		socksbuf[0] = retval;
		CLOSE_SOCK(fd);
	}
exituniverse:
	IRCMutexLock((*xHSlock));
	h->refcnt--;
	IRCMutexUnlock((*xHSlock));
	IRCExitThread(NULL);
	return;
}
