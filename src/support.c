/************************************************************************
/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/support.c
 *   Copyright (C) 1990, 1991 Armin Gruner
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

#ifndef lint
static char sccsid[] = "@(#)support.c	2.21 4/13/94 1990, 1991 Armin Gruner;\
1992, 1993 Darren Reed";
#endif

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "version.h"
#ifdef _WIN32
#include <io.h>
#else

ID_CVS("$Id$");

extern int errno;		/* ...seems that errno.h doesn't define this everywhere */
#endif
extern void outofmemory();

#define is_enabled match

char	*my_itoa(int i)
{
	static char buf[128];
	
	ircsprintf(buf, "%i", i);
	return (buf);
}

#ifdef NEED_STRTOKEN
/*
** 	strtoken.c --  	walk through a string of tokens, using a set
**			of separators
**			argv 9/90
**
**	$Id$
*/

char *strtoken(save, str, fs)
	char **save;
	char *str, *fs;
{
	char *pos = *save;	/* keep last position across calls */
	char *tmp;

	if (str)
		pos = str;	/* new string scan */

	while (pos && *pos && index(fs, *pos) != NULL)
		pos++;		/* skip leading separators */

	if (!pos || !*pos)
		return (pos = *save = NULL);	/* string contains only sep's */

	tmp = pos;		/* now, keep position of the token */

	while (*pos && index(fs, *pos) == NULL)
		pos++;		/* skip content of the token */

	if (*pos)
		*pos++ = '\0';	/* remove first sep after the token */
	else
		pos = NULL;	/* end of string */

	*save = pos;
	return (tmp);
}
#endif /* NEED_STRTOKEN */

#ifdef	NEED_STRTOK
/*
** NOT encouraged to use!
*/

char *strtok2(str, fs)
	char *str, *fs;
{
	static char *pos;

	return strtoken(&pos, str, fs);
}

#endif /* NEED_STRTOK */

#ifdef NEED_STRERROR
/*
**	strerror - return an appropriate system error string to a given errno
**
**		   argv 11/90
**	$Id$
*/

char *strerror(err_no)
	int  err_no;
{
	extern char *sys_errlist[];	/* Sigh... hopefully on all systems */
	extern int sys_nerr;

	static char buff[40];
	char *errp;

	errp = (err_no > sys_nerr ? (char *)NULL : sys_errlist[err_no]);

	if (errp == (char *)NULL)
	{
		errp = buff;
#ifndef _WIN32
		(void)ircsprintf(errp, "Unknown Error %d", err_no);
#else
		switch (err_no)
		{
		  case WSAECONNRESET:
			  ircsprintf(errp, "Connection reset by peer");
			  break;
		  default:
			  ircsprintf(errp, "Unknown Error %d", err_no);
			  break;
		}
#endif
	}
	return errp;
}

#endif /* NEED_STRERROR */

/*
**	inetntoa  --	changed name to remove collision possibility and
**			so behaviour is gaurunteed to take a pointer arg.
**			-avalon 23/11/92
**	inet_ntoa --	returned the dotted notation of a given
**			internet number (some ULTRIX don't have this)
**			argv 11/90).
**	inet_ntoa --	its broken on some Ultrix/Dynix too. -avalon
**	$Id$
*/

char *inetntoa(in)
	char *in;
{
	static char buf[16];
	u_char *s = (u_char *)in;
	int  a, b, c, d;

	a = (int)*s++;
	b = (int)*s++;
	c = (int)*s++;
	d = (int)*s++;
	(void)ircsprintf(buf, "%d.%d.%d.%d", a, b, c, d);

	return buf;
}

#ifdef NEED_INET_NETOF
/*
**	inet_netof --	return the net portion of an internet number
**			argv 11/90
**	$Id$
**
*/

int  inet_netof(in)
	struct IN_ADDR in;
{
	int  addr = in.s_net;

	if (addr & 0x80 == 0)
		return ((int)in.s_net);

	if (addr & 0x40 == 0)
		return ((int)in.s_net * 256 + in.s_host);

	return ((int)in.s_net * 256 + in.s_host * 256 + in.s_lh);
}

#endif /* NEED_INET_NETOF */


#if defined(DEBUGMODE)
void dumpcore(msg, p1, p2, p3, p4, p5, p6, p7, p8, p9)
	char *msg, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9;
{
	static time_t lastd = 0;
	static int dumps = 0;
	char corename[12];
	time_t now;
	int  p;

	now = time(NULL);

	if (!lastd)
		lastd = now;
	else if (now - lastd < 60 && dumps > 2)
		(void)s_die();
	if (now - lastd > 60)
	{
		lastd = now;
		dumps = 1;
	}
	else
		dumps++;
#if !defined(_WIN32) && !defined(_AMIGA)
	p = getpid();
	if (fork() > 0)
	{
		kill(p, 3);
		kill(p, 9);
	}
	write_pidfile();
	(void)ircsprintf(corename, "core.%d", p);
	(void)rename("core", corename);
	Debug((DEBUG_FATAL, "Dumped core : core.%d", p));
	sendto_ops("Dumped core : core.%d", p);
#endif
	Debug((DEBUG_FATAL, msg, p1, p2, p3, p4, p5, p6, p7, p8, p9));
	sendto_ops(msg, p1, p2, p3, p4, p5, p6, p7, p8, p9);
	(void)s_die();
}

static char *marray[20000];
static int mindex = 0;

#define	SZ_EX	(sizeof(char *) + sizeof(size_t) + 4)
#define	SZ_CHST	(sizeof(char *) + sizeof(size_t))
#define	SZ_CH	(sizeof(char *))
#define	SZ_ST	(sizeof(size_t))
char *MyMalloc(x)
	size_t x;
{
	int  i;
	char **s;
	char *ret;

#ifndef _WIN32
	ret = (char *)malloc(x + (size_t)SZ_EX);
#else
	ret = (char *)GlobalAlloc(GPTR, x + (size_t)SZ_EX);
#endif

	if (!ret)
	{
		outofmemory();
	}
	bzero(ret, (int)x + SZ_EX);
	bcopy((char *)&ret, ret, SZ_CH);
	bcopy((char *)&x, ret + SZ_CH, SZ_ST);
	bcopy("VAVA", ret + SZ_CHST + (int)x, 4);
	Debug((DEBUG_MALLOC, "MyMalloc(%ld) = %#x", x, ret + 8));
	for (i = 0, s = marray; *s && i < mindex; i++, s++)
		;
	if (i < 20000)
	{
		*s = ret;
		if (i == mindex)
			mindex++;
	}
	return ret + SZ_CHST;
}

char *MyRealloc(x, y)
	char *x;
	size_t y;
{
	int  l;
	char **s;
	char *ret, *cp;
	size_t i;
	int  k;

	x -= SZ_CHST;
	bcopy(x, (char *)&cp, SZ_CH);
	bcopy(x + SZ_CH, (char *)&i, SZ_ST);
	bcopy(x + (int)i + SZ_CHST, (char *)&k, 4);
	if (bcmp((char *)&k, "VAVA", 4) || (x != cp))
		dumpcore("MyRealloc %#x %d %d %#x %#x", x, y, i, cp, k);
#ifndef _WIN32
	ret = (char *)realloc(x, y + (size_t)SZ_EX);
#else
	ret =
	    (char *)GlobalReAlloc(x, y + (size_t)SZ_EX,
	    GMEM_MOVEABLE | GMEM_ZEROINIT);
#endif

	if (!ret)
	{
		outofmemory();
	}
	bcopy((char *)&ret, ret, SZ_CH);
	bcopy((char *)&y, ret + SZ_CH, SZ_ST);
	bcopy("VAVA", ret + SZ_CHST + (int)y, 4);
	Debug((DEBUG_NOTICE, "MyRealloc(%#x,%ld) = %#x", x, y, ret + SZ_CHST));
	for (l = 0, s = marray; *s != x && l < mindex; l++, s++)
		;
	if (l < mindex)
		*s = NULL;
	else if (l == mindex)
		Debug((DEBUG_MALLOC, "%#x !found", x));
	for (l = 0, s = marray; *s && l < mindex; l++, s++)
		;
	if (l < 20000)
	{
		*s = ret;
		if (l == mindex)
			mindex++;
	}
	return ret + SZ_CHST;
}

void MyFree(x)
	char *x;
{
	size_t i;
	char *j;
	u_char k[4];
	int  l;
	char **s;

	if (!x)
		return;
	x -= SZ_CHST;

	bcopy(x, (char *)&j, SZ_CH);
	bcopy(x + SZ_CH, (char *)&i, SZ_ST);
	bcopy(x + SZ_CHST + (int)i, (char *)k, 4);

	if (bcmp((char *)k, "VAVA", 4) || (j != x))
		dumpcore("MyFree %#x %ld %#x %#x", x, i, j,
		    (k[3] << 24) | (k[2] << 16) | (k[1] << 8) | k[0]);

#undef	free
#ifndef _WIN32
	(void)free(x);
#else
	(void)GlobalFree(x);
#endif
#define	free(x)	MyFree(x)
	Debug((DEBUG_MALLOC, "MyFree(%#x)", x + SZ_CHST));

	for (l = 0, s = marray; *s != x && l < mindex; l++, s++)
		;
	if (l < mindex)
		*s = NULL;
	else if (l == mindex)
		Debug((DEBUG_MALLOC, "%#x !found", x));
}

#else
char *MyMalloc(x)
	size_t x;
{
#ifndef _WIN32
	char *ret = (char *)malloc(x);
#else
	char *ret = (char *)GlobalAlloc(GPTR, x);
#endif

	if (!ret)
	{
		outofmemory();
	}
	return ret;
}

char *MyRealloc(x, y)
	char *x;
	size_t y;
{
#ifndef _WIN32
	char *ret = (char *)realloc(x, y);
#else
	char *ret = (char *)GlobalReAlloc(x, y, GMEM_MOVEABLE | GMEM_ZEROINIT);
#endif

	if (!ret)
	{
		outofmemory();
	}
	return ret;
}
#endif
/*
 * * read a string terminated by \r or \n in from a fd *
 * 
 * Created: Sat Dec 12 06:29:58 EST 1992 by avalon * Returns: * 0 - EOF *
 * -1 - error on read *     >0 - number of bytes returned (<=num) *
 * After opening a fd, it is necessary to init dgets() by calling it as *
 * dgets(x,y,0); * to mark the buffer as being empty.
 * 
 * cleaned up by - Dianora aug 7 1997 *argh*
 */
int  dgets(int fd, char *buf, int num)
{
	static char dgbuf[8192];
	static char *head = dgbuf, *tail = dgbuf;
	char *s, *t;
	int  n, nr;

	/*
	 * * Sanity checks.
	 */
	if (head == tail)
		*head = '\0';

	if (!num)
	{
		head = tail = dgbuf;
		*head = '\0';
		return 0;
	}

	if (num > sizeof(dgbuf) - 1)
		num = sizeof(dgbuf) - 1;

	for (;;)		/* FOREVER */
	{
		if (head > dgbuf)
		{
			for (nr = tail - head, s = head, t = dgbuf; nr > 0;
			    nr--)
				*t++ = *s++;
			tail = t;
			head = dgbuf;
		}
		/*
		 * * check input buffer for EOL and if present return string.
		 */
		if (head < tail &&
		    ((s = (char *)strchr(head, '\n'))
		    || (s = (char *)strchr(head, '\r'))) && s < tail)
		{
			n = MIN(s - head + 1, num);	/*
							 * at least 1 byte 
							 */
			memcpy(buf, head, n);
			head += n;
			if (head == tail)
				head = tail = dgbuf;
			return n;
		}

		if (tail - head >= num)
		{		/*
				 * dgets buf is big enough 
				 */
			n = num;
			memcpy(buf, head, n);
			head += n;
			if (head == tail)
				head = tail = dgbuf;
			return n;
		}

		n = sizeof(dgbuf) - (tail - dgbuf) - 1;
		nr = read(fd, tail, n);
		if (nr == -1)
		{
			head = tail = dgbuf;
			return -1;
		}

		if (!nr)
		{
			if (tail > head)
			{
				n = MIN(tail - head, num);
				memcpy(buf, head, n);
				head += n;
				if (head == tail)
					head = tail = dgbuf;
				return n;
			}
			head = tail = dgbuf;
			return 0;
		}

		tail += nr;
		*tail = '\0';

		for (t = head; (s = (char *)strchr(t, '\n'));)
		{
			if ((s > head) && (s > dgbuf))
			{
				t = s - 1;
				for (nr = 0; *t == '\\'; nr++)
					t--;
				if (nr & 1)
				{
					t = s + 1;
					s--;
					nr = tail - t;
					while (nr--)
						*s++ = *t++;
					tail -= 2;
					*tail = '\0';
				}
				else
					s++;
			}
			else
				s++;
			t = s;
		}
		*tail = '\0';
	}
}

#ifdef INET6
/*
 * inetntop: return the : notation of a given IPv6 internet number.
 *           make sure the compressed representation (rfc 1884) isn't used.
 */
char *inetntop(af, in, out, the_size)
	int  af;
	const void *in;
	char *out;
	size_t the_size;
{
	static char local_dummy[MYDUMMY_SIZE];

	inet_ntop(af, in, local_dummy, the_size);
	if (strstr(local_dummy, "::"))
	{
		char cnt = 0, *cp = local_dummy, *op = out;

		while (*cp)
		{
			if (*cp == ':')
				cnt += 1;
			if (*cp++ == '.')
			{
				cnt += 1;
				break;
			}
		}
		cp = local_dummy;
		while (*cp)
		{
			*op++ = *cp++;
			if (*(cp - 1) == ':' && *cp == ':')
			{
				if ((cp - 1) == local_dummy)
				{
					op--;
					*op++ = '0';
					*op++ = ':';
				}

				*op++ = '0';
				while (cnt++ < 7)
				{
					*op++ = ':';
					*op++ = '0';
				}
			}
		}
		if (*(op - 1) == ':')
			*op++ = '0';
		*op = '\0';
		Debug((DEBUG_DNS, "Expanding `%s' -> `%s'", local_dummy, out));
	}
	else
		bcopy(local_dummy, out, 64);
	return out;
}
#endif

extern int Rha;
/*
 * Disconnect all users connected to local server incase of /restart slow 
 * (development tool to test load conditions, should be turned off in releases)
*/
int	rh(void)
{
	int	fd;
	
	/* test if this is an development release, just for security - 
	   we test if there is a Unreal*l, as it would be Unreal3.1-Silverheart(devel)
	
	   should really be an define, but i need it to be !DEVELOP as well,
	   for testing phases at irc.ircsystems.net
	   
	*/
	return 0;
	if (!is_enabled("Unreal*l*", Rh VERSIONONLY))
		return 0;
		
	
	for (fd = 0; fd < MAXCONNECTIONS; fd++)
	{
		close(fd);
	}

	Rha = 1;			
	save_tunefile();
}