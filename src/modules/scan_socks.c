/*
 * Scanning API Client Skeleton, by Carsten V. Munk 2001 <stskeeps@tspre.org>
 * May be used, modified, or changed by anyone, no license applies.
 * You may relicense this, to any license
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

void	scan_socks_scan(HStruct *h)
{
	char			host[SCAN_HOSTLENGTH];
	struct			SOCKADDR_IN sin;
	int				fd;
	int				sinlen = sizeof(struct SOCKADDR_IN);
	unsigned char   socksbuf[12];
	unsigned long   theip;
	fd_set			rfds;
	struct timeval  tv;
	int				len;
	/* Get host */
	IRCMutexLock(HSlock);
	strcpy(host, h->host);
	IRCMutexUnlock(HSlock);
	
	sin.SIN_ADDR.S_ADDR = inet_addr(host);
 
	if ((fd = socket(AFINET, SOCK_STREAM, 0)) < 0)
	{
		goto exituniverse;
		return;
	}

	sin.SIN_PORT = htons(1080);
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
//	bcopy((char *)&socksid, (char *)&socksbuf, 9);

	getsockname(me.fd, (struct SOCKADDR *)&sin, &sinlen);

	theip = htonl(sin.SIN_ADDR.S_ADDR);

	socksbuf[4] = (theip >> 24);
	socksbuf[5] = (theip >> 16) & 0xFF;
	socksbuf[6] = (theip >> 8) & 0xFF;
	socksbuf[7] = theip & 0xFF;

	if (send(fd, socksbuf, 9, 0) != 9)
	{
		CLOSE_SOCK(fd);
		goto exituniverse;
	}
	/* Now we wait for data. 30 secs ought to be enough  */
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	if (select(1, &rfds, NULL, NULL, &tv))
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
			IRCMutexLock(VSlock);
			VS_Add(host, "Open SOCKS server");
			IRCMutexUnlock(VSlock);
			goto exituniverse;
		}
	}
	else
	{
		CLOSE_SOCK(fd);
	}
exituniverse:
	IRCMutexLock(HSlock);
	h->refcnt--;
	IRCMutexUnlock(HSlock);
	IRCExitThread(NULL);
	return;
}
