/*
 *   IRC - Internet Relay Chat, src/modules/scan_socks.c
 *   (C) 2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
 *
 *   SOCKS4/5 scanning module for scan.so
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


static iFP			xVS_add = NULL;
static ConfigItem_blackhole	*blackh_conf = NULL;
void	scan_socks_scan(HStruct *h);
void	scan_socks4_scan(HStruct *h);
void	scan_socks5_scan(HStruct *h);
#ifndef DYNAMIC_LINKING
ModuleInfo scan_socks_info
#else
ModuleInfo mod_header
#endif
  = {
  	2,
	"scan_socks",	/* Name of module */
	"$Id$", /* Version */
	"scanning API: socks", /* Short description of module */
	NULL, /* Pointer to our dlopen() return value */
	NULL 
    };

/*
 * Our symbol depencies
*/
#ifdef STATIC_LINKING
MSymbolTable scan_socks_depend[] = {
#else
MSymbolTable mod_depend[] = {
#endif
	SymD(VS_Add, xVS_add, "src/modules/scan.so"),
	SymD(HSlock, xHSlock, "src/modules/scan.so"),
	SymD(VSlock, xVSlock, "src/modules/scan.so"),
	SymD(blackhole_conf, blackh_conf, "src/modules/scan.so"),
	{NULL, NULL}
};


/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_init(int module_load)
#else
int    scan_socks_init(int module_load)
#endif
{
	/*
	 * Add scanning hooks
	*/
	add_HookX(HOOKTYPE_SCAN_HOST, NULL, scan_socks_scan); 
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    scan_socks_load(int module_load)
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

void scan_socks_scan(HStruct *h) {
	THREAD thread[2];
	THREAD_ATTR thread_attr;
	IRCMutexLock((*xHSlock));
	h->refcnt++;
	IRCCreateThread(thread[0], thread_attr, scan_socks4_scan, h);
	h->refcnt++;
	IRCCreateThread(thread[1], thread_attr, scan_socks5_scan, h);
	IRCMutexUnlock((*xHSlock));
	IRCJoinThread(thread[0], NULL);
	IRCJoinThread(thread[1], NULL);
	IRCMutexLock((*xHSlock));
	h->refcnt--;
	IRCMutexUnlock((*xHSlock));
	IRCExitThread(NULL);
	return;
}
	

void	scan_socks4_scan(HStruct *h)
{
	int			retval;
	char			host[SCAN_HOSTLENGTH];
	struct			sockaddr_in sin;
	SOCKET				fd;
	int				sinlen = sizeof(struct sockaddr_in);
	unsigned short	sport = blackh_conf->port;
	unsigned char   socksbuf[12];
	unsigned long   theip;
	fd_set			rfds;
	struct timeval  	tv;
	int				len;
	/* Get host */
	IRCMutexLock((*xHSlock));
	strcpy(host, h->host);
	IRCMutexUnlock((*xHSlock));
	/* IPv6 ?*/
	if (strchr(host, ':'))
		goto exituniverse;
		
	sin.sin_addr.s_addr = inet_addr(host);
	if ((fd = socket(AFINET, SOCK_STREAM, 0)) < 0)
	{
		goto exituniverse;
		return;
	}

	sin.sin_port = htons((unsigned short)SCAN_ON_PORT);
	sin.sin_family = AF_INET;
	/* We do this non-blocking to prevent a hang of the entire ircd with newer
	 * versions of glibc.  Don't you just love new "features?"
	 * Passing null to this is probably bad, a better method is needed. 
	 * Maybe a version of set_non_blocking that doesn't send error messages? 
	 * -Zogg
	 * 
	 * set_non_blocking(fd,cptr)
	 * when cptr == NULL, it doesnt error - changed some months ago
	 * also, don't we need a select loop to make this better?
         * -Stskeeps
         * I just gave a select loop a shot (select in a while(), waiting for
         * the thing to either set the writable flags or return a -# and set 
         * errno to EINTR.  Could be my ignorance, or my glibc, but select()
         * NEVER returned a negative number, and if I passed it a timeout (ie, tv)
         * then the loops never ended, either. 
         * -Zogg
	 */
	set_non_blocking(fd, NULL);
	if ((retval = connect(fd, (struct sockaddr *)&sin,
                sizeof(sin))) == -1 && ERRNO != P_EINPROGRESS)
	{
		/* we have no socks server! */
		CLOSE_SOCK(fd);	
		goto exituniverse;
		return;
	}
	
	/* We wait for write-ready */
	tv.tv_sec = 40;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if (!select(fd + 1, NULL, &rfds, NULL, &tv))
	{
		CLOSE_SOCK(fd);
		goto exituniverse;
	}
				
	sin.sin_addr.s_addr = inet_addr(blackh_conf->outip ? blackh_conf->outip : blackh_conf->ip);
	theip = htonl(sin.sin_addr.s_addr);
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
	/* Now we wait for data. 10 secs ought to be enough  */
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
			(*xVS_add)(host, "Open SOCKS4 server");
			IRCMutexUnlock((*xVSlock));
			goto exituniverse;
		}
	}
	else
	{
		CLOSE_SOCK(fd);
	}
exituniverse:
	IRCMutexLock((*xHSlock));
	h->refcnt--;
	IRCMutexUnlock((*xHSlock));
	IRCExitThread(NULL);
	return;
}

void	scan_socks5_scan(HStruct *h)
{
	int			retval;
	char			host[SCAN_HOSTLENGTH];
	struct			sockaddr_in sin;
	SOCKET				fd;
	int				sinlen = sizeof(struct sockaddr_in);
	unsigned short	sport = blackh_conf->port;
	unsigned char   socksbuf[12];
	unsigned long   theip;
	fd_set			rfds;
	struct timeval  	tv;
	int				len;
	/* Get host */
	IRCMutexLock((*xHSlock));
	strcpy(host, h->host);
	IRCMutexUnlock((*xHSlock));
	/* IPv6 ?*/
	if (strchr(host, ':'))
		goto exituniverse;
		
	sin.sin_addr.s_addr = inet_addr(host);
	if ((fd = socket(AFINET, SOCK_STREAM, 0)) < 0)
	{
		goto exituniverse;
		return;
	}

	sin.sin_port = htons(SCAN_ON_PORT);
	sin.sin_family = AF_INET;
	/* We do this non-blocking to prevent a hang of the entire ircd with newer
	 * versions of glibc.  Don't you just love new "features?"
	 * Passing null to this is probably bad, a better method is needed. 
	 * Maybe a version of set_non_blocking that doesn't send error messages? 
	 * -Zogg
	 * 
	 * set_non_blocking(fd,cptr)
	 * when cptr == NULL, it doesnt error - changed some months ago
	 * also, don't we need a select loop to make this better?
         * -Stskeeps
         * I just gave a select loop a shot (select in a while(), waiting for
         * the thing to either set the writable flags or return a -# and set 
         * errno to EINTR.  Could be my ignorance, or my glibc, but select()
         * NEVER returned a negative number, and if I passed it a timeout (ie, tv)
         * then the loops never ended, either. 
         * -Zogg
	 */
	set_non_blocking(fd, NULL);
	if ((retval = connect(fd, (struct sockaddr *)&sin,
                sizeof(sin))) == -1 && ERRNO != P_EINPROGRESS)
	{
		/* we have no socks server! */
		CLOSE_SOCK(fd);	
		goto exituniverse;
		return;
	}
	
	/* We wait for write-ready */
	tv.tv_sec = 40;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if (!select(fd + 1, NULL, &rfds, NULL, &tv))
	{
		CLOSE_SOCK(fd);
		goto exituniverse;
	}
				
	sin.sin_addr.s_addr = inet_addr(blackh_conf->outip ? blackh_conf->outip : blackh_conf->ip);
	theip = htonl(sin.sin_addr.s_addr);
	bzero(socksbuf, sizeof(socksbuf));

	if (send(fd, "\5\1\0", 3, 0) != 3) {
		CLOSE_SOCK(fd);
		goto exituniverse;
	}
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if (retval = select(fd + 1, &rfds, NULL, NULL, &tv))
	{
		/* There's data in the jar. Let's read it */
		len = recv(fd, socksbuf, 2, 0);
		if (len != 2)
		{
			CLOSE_SOCK(fd);
			goto exituniverse;
		}
		if (socksbuf[0] != 5 || socksbuf[1] != 0) {
			CLOSE_SOCK(fd);
			goto exituniverse;
		}
	}
	
	socksbuf[0] = 5;
	socksbuf[1] = 1;
	socksbuf[2] = 0;
	socksbuf[3] = 1;
	socksbuf[4] = (theip >> 24) & 0xFF;
	socksbuf[5] = (theip >> 16) & 0xFF;
	socksbuf[6] = (theip >> 8) & 0xFF;
	socksbuf[7] = theip & 0xFF;
	socksbuf[8] = HICHAR(sport);
	socksbuf[9] = LOCHAR(sport);
	
	if ((retval = send(fd, socksbuf, 10, 0)) != 10)
	{
		CLOSE_SOCK(fd);
		goto exituniverse;
	}
	bzero(socksbuf, sizeof(socksbuf));
	/* Now we wait for data. 10 secs ought to be enough  */
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if (retval = select(fd + 1, &rfds, NULL, NULL, &tv))
	{
		/* There's data in the jar. Let's read it */
		len = recv(fd, socksbuf, 2, 0);
		CLOSE_SOCK(fd);
		if (len != 2)
		{
			goto exituniverse;
		}
		if (socksbuf[1] == 5 && socksbuf[1] == 0)
		{
			/* We found SOCKS. */
			IRCMutexLock((*xVSlock));
			(*xVS_add)(host, "Open SOCKS5 server");
			IRCMutexUnlock((*xVSlock));
			goto exituniverse;
		}
	}
	else
	{
		CLOSE_SOCK(fd);
	}
exituniverse:
	IRCMutexLock((*xHSlock));
	h->refcnt--;
	IRCMutexUnlock((*xHSlock));
	IRCExitThread(NULL);
	return;
}

