/*
   UnrealIRCd internal webserver - Virtual File System
   Copyright (c) 2001, The UnrealIRCd Team
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification, are permitted
   provided that the following conditions are met:
   
     * Redistributions of source code must retain the above copyright notice, this list of conditions
       and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions
       and the following disclaimer in the documentation and/or other materials provided with the
       distribution.
     * Neither the name of the The UnrealIRCd Team nor the names of its contributors may be used
       to endorse or promote products derived from this software without specific prior written permission.
     * The source code may not be redistributed for a fee or in closed source
       programs, without expressed oral consent by the UnrealIRCd Team, however
       for operating systems where binary distribution is required, if URL
       is passed with the package to get the full source
     * No warranty is given unless stated so by the The UnrealIRCd Team

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
   FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR  
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "threads.h"
#include "modules/web/httpd.h"

int h_u_vfs(HTTPd_Request *r);

typedef struct _vfs_table VFStable;

struct _vfs_table
{
	char 	*filename;
	char	*realfile;
	char	*ct;
};

VFStable	vfsTable[] =
{
	{"/sections/active.gif", "html/sections/active.gif", "image/gif"},
	{"/sections/inactive.gif", "html/sections/inactive.gif", "image/gif"},
	{"/html/main.html", "html/html/main.html", "text/html"},
	{"/icons/conffiles.jpg", "html/icons/conffiles.jpg", "image/jpeg"},
	{"/icons/modules.jpg", "html/icons/modules.jpg", "image/jpeg"},
	{"/icons/opers.jpg", "html/icons/opers.jpg", "image/jpeg"},
	{"/icons/settings.jpg", "html/icons/settings.jpg", "image/jpeg"},
	{"/icons/stats.jpg", "html/icons/stats.jpg", "image/jpeg"},
	{"/icons/users.jpg", "html/icons/users.jpg", "image/jpeg"},
	{"/back/background.jpg", "html/back/background.jpg", "image/jpeg"},
	{"/unrealircd.com/active.gif", "html/unrealircd.com/active.gif", "image/jpeg"},
	{"/unrealircd.com/inactive.gif", "html/unrealircd.com/inactive.gif", "image/jpeg"},
	{"/back/grad.jpg", "html/back/grad.jpg", "image/jpeg"},
	{"/back/line.gif", "html/back/line.gif", "image/gif"},
	{"/sections/bg.gif", "html/sections/bg.gif", "image/gif"},
	{"/", "html/index.html", "text/html"},
	{NULL, NULL, NULL}	
};
#define soprintf sockprintf

DLLFUNC int h_u_vfs(HTTPd_Request *r)
{
	VFStable *p = &vfsTable[0];
	struct stat statf;
	char	*ims;
	time_t	tmt;
	char	*datebuf = NULL;

	ims = GetHeader(r, "if-modified-since");
	while (p->filename)
	{
		if (!match(p->filename, r->url))
		{
			stat(p->realfile, &statf);
			if (ims)
			{
				if (tmt = rfc2time(ims) < 0)
				{
					httpd_500_header(r, "Bad date");
					return 1;	
				}
				if (statf.st_mtime < tmt)
				{
					httpd_304_header(r);
					return 1;
				} 
			}			
			httpd_standard_headerX(r, p->ct, 1);
			datebuf = MyMalloc(60);
			soprintf(r, "Last-Modified: %s",
				rfctime(statf.st_mtime, datebuf));
			soprintf(r, "Content-Length: %i",
				statf.st_size);
			MyFree(datebuf);
			soprintf(r, "");
			
			httpd_sendfile(r, p->realfile);
			return 1;
		}
		p++;
	}
	return 0;
}
