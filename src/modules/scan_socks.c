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

#ifndef SCAN_ON_PORT
#define SCAN_ON_PORT 1080
#endif
static Hook *SocksScanHost = NULL;
static vFP			xEadd_scan = NULL;
static struct SOCKADDR_IN	*xScan_endpoint = NULL;
static int xScan_TimeOut = 0;
#ifdef STATIC_LINKING
extern void Eadd_scan();
extern struct SOCKADDR_IN	Scan_endpoint;
extern int Scan_TimeOut;
#endif
void	scan_socks_scan(Scan_AddrStruct *sr);
void	scan_socks4_scan(Scan_AddrStruct *sr);
void	scan_socks5_scan(Scan_AddrStruct *sr);
static int HOOKTYPE_SCAN_HOST;
static Hooktype *ScanHost;
static Mod_SymbolDepTable modsymdep[] = 
{
	MOD_Dep(Eadd_scan, xEadd_scan, "src/modules/scan.so"),
	MOD_Dep(Scan_endpoint, xScan_endpoint, "src/modules/scan.so"),
	MOD_Dep(Scan_TimeOut, xScan_TimeOut, "src/modules/scan.so"),
	{NULL, NULL}
};
ModuleInfo ScanSocksModInfo;

#ifndef DYNAMIC_LINKING
ModuleHeader scan_socks_Header
#else
ModuleHeader Mod_Header
#endif
  = {
	"scan_socks",	/* Name of module */
	"$Id$", /* Version */
	"scanning API: socks", /* Short description of module */
	"3.2-b8-1",
    	modsymdep
    };


/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(ModuleInfo *modinfo)
#else
int    scan_socks_Init(ModuleInfo *modinfo)
#endif
{
	/*
	 * Add scanning hooks
	*/
	bcopy(modinfo,&ScanSocksModInfo,modinfo->size);
	ScanHost = HooktypeAdd(ScanSocksModInfo.handle, "HOOKTYPE_SCAN_HOST", &HOOKTYPE_SCAN_HOST);
	SocksScanHost = HookAddVoidEx(ScanSocksModInfo.handle, HOOKTYPE_SCAN_HOST, scan_socks_scan); 
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    scan_socks_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	scan_socks_Unload(int module_unload)
#endif
{
	HookDel(SocksScanHost);
	HooktypeDel(ScanHost,ScanSocksModInfo.handle);
	return MOD_SUCCESS;
}

#define HICHAR(s)	(((unsigned short) s) >> 8)
#define LOCHAR(s)	(((unsigned short) s) & 0xFF)

void scan_socks_scan(Scan_AddrStruct *h)
{
	THREAD thread[2];
	IRCMutexLock((h->lock));
	h->refcnt++;
	IRCCreateThread(thread[0], scan_socks4_scan, h);
	h->refcnt++;
	IRCCreateThread(thread[1], scan_socks5_scan, h);
	IRCMutexUnlock((h->lock));
	IRCJoinThread(thread[0], NULL);
	IRCJoinThread(thread[1], NULL);
	IRCMutexLock((h->lock));
	h->refcnt--;
	IRCMutexUnlock((h->lock));
	IRCDetachThread(IRCThreadSelf());
	IRCExitThread(NULL);
	return;
}
	

void	scan_socks4_scan(Scan_AddrStruct *h)
{
	int			retval;
#ifdef INET6
	unsigned char		*cp;
#endif
	struct			SOCKADDR_IN sin;
	struct			in_addr ia4;
	SOCKET			fd;
	unsigned char		socksbuf[10];
	unsigned long   	theip;
	fd_set			rfds;
	struct timeval  	tv;
	int			len;
	/* Get host */

	IRCMutexLock((h->lock));

#ifndef INET6
	sin.SIN_ADDR.S_ADDR = h->in.S_ADDR;
#else
	bcopy(sin.SIN_ADDR.S_ADDR, h->in.S_ADDR, sizeof(h->in.S_ADDR));
#endif
	IRCMutexUnlock((h->lock));
	/* IPv6 ?*/
#ifdef INET6
	IRCMutexLock((h->lock));
#ifndef INET6
	sin.SIN_ADDR.S_ADDR = h->in.S_ADDR;
#else
	bcopy(sin.SIN_ADDR.S_ADDR, h->in.S_ADDR, sizeof(h->in.S_ADDR));
#endif
	IRCMutexUnlock((h->lock));
	/* ::ffff:ip hack */
	cp = (u_char *)&h->in.s6_addr;
	if (!(cp[0] == 0 && cp[1] == 0 && cp[2] == 0 && cp[3] == 0 && cp[4] == 0
	    && cp[5] == 0 && cp[6] == 0 && cp[7] == 0 && cp[8] == 0
	    && cp[9] == 0 && ((cp[10] == 0 && cp[11] == 0) || (cp[10] == 0xff
	    && cp[11] == 0xff))))

		goto exituniverse;
#endif
	if ((fd = socket(AFINET, SOCK_STREAM, 0)) < 0)
	{
		goto exituniverse;
		return;
	}
	sin.SIN_PORT = htons((unsigned short)SCAN_ON_PORT);
	sin.SIN_FAMILY = AFINET;
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
                sizeof(sin))) == -1 && !(ERRNO == P_EINPROGRESS))
	{
		/* we have no socks server! */
		CLOSE_SOCK(fd);	
		goto exituniverse;
		return;
	}
ircd_log(LOG_ERROR, "%s:%d - (Connection Established)", Inet_ia2p(&h->in), SCAN_ON_PORT);
	
	/* We wait for write-ready */
	tv.tv_sec = xScan_TimeOut;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if (!select(fd + 1, NULL, &rfds, NULL, &tv))
	{
		CLOSE_SOCK(fd);
		goto exituniverse;
	}
#ifdef INET6
	ia4.s_addr = inet_aton((char *)Inet_ia2p(&xScan_endpoint->SIN_ADDR));
#else
	bcopy(&xScan_endpoint->sin_addr, &ia4, sizeof(struct IN_ADDR));
#endif
	theip = htonl(ia4.s_addr);
	bzero(socksbuf, sizeof(socksbuf));
	socksbuf[0] = 4;
	socksbuf[1] = 1;
	socksbuf[2] = LOCHAR(xScan_endpoint->SIN_PORT);
	socksbuf[3] = HICHAR(xScan_endpoint->SIN_PORT);
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
	tv.tv_sec = xScan_TimeOut;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if ((retval = select(fd + 1, &rfds, NULL, NULL, &tv)))
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
			IRCMutexLock((h->lock));
			(*xEadd_scan)(&h->in, "Open SOCKS4 server");
			IRCMutexUnlock((h->lock));
			goto exituniverse;
		}
	}
	else
	{
		CLOSE_SOCK(fd);
	}
exituniverse:
	IRCMutexLock((h->lock));
	h->refcnt--;
	IRCMutexUnlock((h->lock));
	/* We get joined, we need no steekin Detach */
	IRCExitThread(NULL);
	return;
}

void	scan_socks5_scan(Scan_AddrStruct *h)
{
	int			retval;
#ifdef INET6
	unsigned char			*cp;
#endif
	struct			SOCKADDR_IN sin;
	struct			in_addr ia4;
	SOCKET			fd;
	unsigned long   	theip;
	fd_set			rfds;
	struct timeval  	tv;
	int			len;
	unsigned char		socksbuf[10];
	/* Get host */
	IRCMutexLock((h->lock));

#ifndef INET6
	sin.SIN_ADDR.S_ADDR = h->in.S_ADDR;
#else
	bcopy(sin.SIN_ADDR.S_ADDR, h->in.S_ADDR, sizeof(h->in.S_ADDR));
#endif
	IRCMutexUnlock((h->lock));
	/* IPv6 ?*/
#ifdef INET6
	/* ::ffff:ip hack */
	cp = (u_char *)&h->in.s6_addr;
	if (!(cp[0] == 0 && cp[1] == 0 && cp[2] == 0 && cp[3] == 0 && cp[4] == 0
	    && cp[5] == 0 && cp[6] == 0 && cp[7] == 0 && cp[8] == 0
	    && cp[9] == 0 && ((cp[10] == 0 && cp[11] == 0) || (cp[10] == 0xff
	    && cp[11] == 0xff))))

		goto exituniverse;
#endif
	if ((fd = socket(AFINET, SOCK_STREAM, 0)) < 0)
	{
		goto exituniverse;
		return;
	}

	sin.SIN_PORT = htons((unsigned short)SCAN_ON_PORT);
	sin.SIN_FAMILY = AFINET;
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
                sizeof(sin))) == -1 && 
		 !(ERRNO == P_EINPROGRESS))
	{
		/* we have no socks server! */
		CLOSE_SOCK(fd);	
		goto exituniverse;
		return;
	}
	
	/* We wait for write-ready */
	tv.tv_sec = xScan_TimeOut;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if (!select(fd + 1, NULL, &rfds, NULL, &tv))
	{
		CLOSE_SOCK(fd);
		goto exituniverse;
	}
#ifdef INET6
	ia4.s_addr = inet_aton((char *)Inet_ia2p(&xScan_endpoint->SIN_ADDR));
#else
	bcopy(&xScan_endpoint->sin_addr, &ia4, sizeof(struct IN_ADDR));
#endif
	theip = htonl(ia4.s_addr);

	bzero(socksbuf, sizeof(socksbuf));

	if (send(fd, "\5\1\0", 3, 0) != 3) {
		CLOSE_SOCK(fd);
		goto exituniverse;
	}
	tv.tv_sec = xScan_TimeOut;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if ((retval = select(fd + 1, &rfds, NULL, NULL, &tv)))
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
	socksbuf[8] = HICHAR(xScan_endpoint->SIN_PORT);
	socksbuf[9] = LOCHAR(xScan_endpoint->SIN_PORT);
	
	if ((retval = send(fd, socksbuf, 10, 0)) != 10)
	{
		CLOSE_SOCK(fd);
		goto exituniverse;
	}
	bzero(socksbuf, sizeof(socksbuf));
	/* Now we wait for data. 10 secs ought to be enough  */
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if ((retval = select(fd + 1, &rfds, NULL, NULL, &tv)))
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
			IRCMutexLock((h->lock));
			(*xEadd_scan)(&h->in, "Open SOCKS5 server");
			IRCMutexUnlock((h->lock));
			goto exituniverse;
		}
	}
	else
	{
		CLOSE_SOCK(fd);
	}
exituniverse:
	IRCMutexLock(h->lock);
	h->refcnt--;
	IRCMutexUnlock(h->lock);
	/* We need no steekin detach */
	IRCExitThread(NULL);
	return;
}

