/*
   UnrealIRCd internal webserver - httpd.h
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

typedef struct _httpd_header HTTPd_Header;

struct _httpd_header
{
	char	 *name;
	char	 *value;
	HTTPd_Header *next;
};

typedef struct _httpd_request HTTPd_Request;

struct _httpd_request
{
	SOCKET		fd;
	char		inbuf[1024];
	short		pos;
	/* 0 = gimme head request 1 = getting head */
	unsigned 	state : 2;
	/* 1 = GET    0 = POST */
	unsigned	method : 1;
	
	char		*url;
	HTTPd_Header	*headers;
	long		content_length;
	HTTPd_Header	*dataheaders;
};

char	*GetField(HTTPd_Header *header, char *name);
char	*GetHeader(HTTPd_Request *request, char *name);
void 	httpd_standard_header(HTTPd_Request *request, char *type);
void	httpd_badrequest(HTTPd_Request *request, char *reason);
void	sockprintf(HTTPd_Request *r, char *format, ...);
void	httpd_sendfile(HTTPd_Request *r, char *filename);



