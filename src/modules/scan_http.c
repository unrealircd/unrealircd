/*
 *   IRC - Internet Relay Chat, src/modules/scan_http.c
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

typedef struct _hsstruct HSStruct;

struct _hsstruct
{
	Scan_AddrStruct *hs;
	int	port;
};

static vFP			xEadd_scan = NULL;
static struct SOCKADDR_IN	*xScan_endpoint = NULL;

#ifdef STATIC_LINKING
extern void Eadd_scan();
extern struct SOCKADDR_IN	Scan_endpoint;
#endif
#ifdef DYNAMIC_LINKING
Module *Mod_Handle = NULL;
#else 
#define Mod_Handle NULL
#endif
static Mod_SymbolDepTable modsymdep[] = 
{
	MOD_Dep(Eadd_scan, xEadd_scan, "src/modules/scan.so"),
	MOD_Dep(Scan_endpoint, xScan_endpoint, "src/modules/scan.so"),
	{NULL, NULL}
};


#ifndef DYNAMIC_LINKING
ModuleHeader scan_http_Header
#else
ModuleHeader Mod_Header
#endif
  = {
	"scan_http",	/* Name of module */
	"$Id$", /* Version */
	"scanning API: http proxies", /* Short description of module */
	"3.2-b5",
	modsymdep
    };

/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/
void	scan_http_scan(Scan_AddrStruct *h);
void	scan_http_scan_port(HSStruct *z);

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(int module_load)
#else
int    scan_http_Init(int module_load)
#endif
{
	/*
	 * Add scanning hooks
	*/
	HookAddVoidEx(Mod_Handle, HOOKTYPE_SCAN_HOST, scan_http_scan); 
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    scan_http_Load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	scan_http_Unload(int module_unload)
#endif
{
	HookDelVoid(HOOKTYPE_SCAN_HOST, scan_http_scan);
}

#define HICHAR(s)	(((unsigned short) s) >> 8)
#define LOCHAR(s)	(((unsigned short) s) & 0xFF)


void 	scan_http_scan(Scan_AddrStruct *h)
{
	THREAD	thread[3];
	THREAD_ATTR thread_attr;
	HSStruct *p = NULL;
	
	IRCMutexLock((h->lock));
	/* First we take 3128 .. */
	h->refcnt++;
	p = MyMalloc(sizeof(HSStruct));
	p->hs = h;
	p->port = 3128;
	IRCCreateThread(thread[0], thread_attr, scan_http_scan_port, p);
	/* Then we take 8080 .. */
	h->refcnt++;
	p = MyMalloc(sizeof(HSStruct));
	p->hs = h;
	p->port = 8080;
	IRCCreateThread(thread[1], thread_attr, scan_http_scan_port, p);
	/* And then we try to infect them with Code Red .. */
	h->refcnt++;
	p = MyMalloc(sizeof(HSStruct));
	p->hs = h;
	p->port = 80;
	IRCCreateThread(thread[2], thread_attr, scan_http_scan_port, p);
	IRCMutexUnlock((h->lock));
	IRCJoinThread(thread[0], NULL);		
	IRCJoinThread(thread[1], NULL);		
	IRCJoinThread(thread[2], NULL);	
	IRCMutexLock((h->lock));
	h->refcnt--;
	IRCMutexUnlock((h->lock));
	IRCExitThread(NULL);
	return;
}

void	scan_http_scan_port(HSStruct *z)
{
	Scan_AddrStruct		*h = z->hs;
	int			retval;
	unsigned char			*cp;
	struct			SOCKADDR_IN sin;
	struct			in_addr ia4;
	SOCKET			fd;
	unsigned char		httpbuf[160];
	int			sinlen = sizeof(struct SOCKADDR_IN);
	unsigned long   	theip;
	fd_set			rfds;
	struct timeval  	tv;
	int			len;

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

	sin.SIN_PORT = htons((unsigned short)z->port);
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
				
	bzero(httpbuf, sizeof(httpbuf));
	sprintf(httpbuf, "CONNECT %s:%i HTTP/1.1\n\n",
		Inet_ia2p(&xScan_endpoint->SIN_ADDR), ntohs(xScan_endpoint->SIN_PORT));
	if ((retval = send(fd, httpbuf, strlen(httpbuf), 0)) != strlen(httpbuf))
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
		len = recv(fd, httpbuf, 13, 0);
		CLOSE_SOCK(fd);
		if (len < 4)
		{
			goto exituniverse;
		}
		if (!strncmp(httpbuf, "HTTP/1.0 200", 12))
		{
			/* Gotcha */
			IRCMutexLock((h->lock));
			(*xEadd_scan)(&h->in, "Open HTTP proxy");
			IRCMutexUnlock((h->lock));
			goto exituniverse;
		} 
	}
	else
	{
		CLOSE_SOCK(fd);
	}
exituniverse:
	MyFree(z);
	IRCMutexLock((h->lock));
	h->refcnt--;
	IRCMutexUnlock((h->lock));
	IRCExitThread(NULL);
	return;
}
