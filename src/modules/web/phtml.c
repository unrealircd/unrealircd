/*
   UnrealIRCd internal webserver - Prototype HTML
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

DLLFUNC int h_u_phtml(HTTPd_Request *r);

#define soprintf sockprintf

#define SECTION_OPERS 1
#define SECTION_STATS 2

char *Titles[] = 
{
	"<NULL>",
	"Operators",
	"IRC Server Stats"
};

char *Icons[] =
{
	"<NULL>",
	"/icons/opers.jpg",
	"/icons/stats.jpg"
};

char *Sections[] =
{
	"<NULL>",
	"IRC Operators",
	"IRC Server Stats"
};

char *ButtonTexts[] =
{
	"<NULL>",
	"Operators",
	"Stats"
};

char *ButtonURLs[] = 
{
	"<NULL>",
	"/section/opers",
	"/section/stats"
};

DLLFUNC int h_u_phtml(HTTPd_Request *r)
{
	int	section = 0;
	char	buf[1024];
	char	bbuf[1024];
	char	*s;
	FILE	*f = NULL;

	f = fopen("html/html/index.phtml", "r");
	if (!f)
		return 0;	
	if (!match("/section/opers", r->url))
		section = SECTION_OPERS;
	if (!match("/section/stats", r->url))
		section = SECTION_STATS;
	if (section < 1)
		return 0;
		
	httpd_standard_header(r, "text/html");
	while (fgets(buf, 1023, f))
	{
		iCstrip(buf);
		strcpy(bbuf, buf);
		if (*buf == '$' && (buf[1] == ' '))
		{
			s = strtok(buf, " ");
			if (s)
			{
				s = strtok(NULL, " ");
				if (s)
				{
					if (!strcmp(s, "TITLE"))
					{
						sockprintf(r, "%s", Titles[section]);
					}
					else
					if (!strcmp(s, "ICON"))
					{
						sockprintf(r, " src=\"%s\" ",
							Icons[section]);
					}
					else
					if (!strcmp(s, "SECTION"))
					{
						sockprintf(r, "%s",
							Sections[section]);
					}
					else
					if (!strcmp(s, "DATA"))
					{
						time_t	tmpnow;
						
						if (section == SECTION_STATS)
						{
							tmpnow = TStime() - me.since;
							sockprintf(r, "My name is %s<br>", me.name);
							sockprintf(r, 
								"Been up for %lu days, %lu hours, %lu minutes, %lu seconds",
									tmpnow / 86400, (tmpnow / 3600) % 24, (tmpnow /60) %60, 
									tmpnow % 60);
							
						}
					}
					
				}	
				else
					sockprintf(r, "%s", bbuf);
			}
			else
			    sockprintf(r, "%s", bbuf);
		}
		else
			sockprintf(r, "%s", bbuf);
	}
	return 1;
}
