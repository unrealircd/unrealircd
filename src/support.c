/*
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
#include <string.h>


extern int errno;		/* ...seems that errno.h doesn't define this everywhere */
#endif
extern void outofmemory();

#define is_enabled match

long	TS2ts(char *s)
{
	if (*s == '!')
		return (xbase64dec(s + 1));
	else
		return (atoi(s));	
}

char	*my_itoa(int i)
{
	static char buf[128];
#ifndef _WIN32	
	ircsprintf(buf, "%d", i);
#else
	_itoa(i, buf, 10);
#endif
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

/*
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

/* Made by Potvin originally, i guess */
time_t	atime_exp(char *base, char *ptr)
{
	time_t	tmp;
	char	*p, c = *ptr;
	
	p = ptr;
	*ptr-- = '\0';
	while (ptr-- > base)
		if (isalpha(*ptr))
			break;
	tmp = atoi(ptr + 1);
	*p = c;

	return tmp;
}

#define Xtract(x, y) if (x) y = atime_exp(xtime, x)

time_t	atime(char *xtime)
{
	char *d, *h, *m, *s;
	time_t D, H, M, S;
	int i;
	
	d = h = m = s = NULL;
	D = H = M = S = 0;
	
	
	i = 0;
	for (d = xtime; *d; d++)
		if (isalpha(*d) && (i != 1))
			i = 1;
	if (i == 0)
		return (atol(xtime)); 
	d = strchr(xtime, 'd');
	h = strchr(xtime, 'h');
	m = strchr(xtime, 'm');
	s = strchr(xtime, 's');
	
	Xtract(d, D);
	Xtract(h, H);
	Xtract(m, M);
	Xtract(s, S);

	return ((D * 86400) + (H * 3600) + (M * 60) + S);		
}

void iCstrip(char *line)
{
	char *c;

	if ((c = strchr(line, '\n')))
		*c = '\0';
	if ((c = strchr(line, '\r')))
		*c = '\0';
}

/*
 * Copyright 1996, 1997, 1998, 1999, 2000 Michiel Boland.
 * Under the BSD license (without advertising clause)
 * From mathopd
*/
char *rfctime(time_t t, char *buf)
{
        struct tm *tp;

        tp = gmtime(&t);
        if (tp == 0) {
                return 0;
        }
        strftime(buf, 31, "%a, %d %b %Y %H:%M:%S GMT", tp);
        return buf;
}

time_t rfc2time(char *s)
{
	static const int daytab[2][12] = {
		{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
		{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
	};
	unsigned sec, min, hour, day, mon, year;
	char month[3];
	register char c;
	register unsigned n;
	register char flag;
	register char state;
	register char isctime;
	enum { D_START, D_END, D_MON, D_DAY, D_YEAR, D_HOUR, D_MIN, D_SEC };

	sec = 60;
	min = 60;
	hour = 24;
	day = 32;
	year = 1969;
	isctime = 0;
	month[0] = 0;
	state = D_START;
	n = 0;
	flag = 1;
	do {
		c = *s++;
		switch (state) {
		case D_START:
			if (c == ' ') {
				state = D_MON;
				isctime = 1;
			} else if (c == ',') state = D_DAY;
			break;
		case D_MON:
			if (isalpha(c)) {
				if (n < 3) month[n++] = c;
			} else {
				if (n < 3) return -1;
				n = 0;
				state = isctime ? D_DAY : D_YEAR;
			}
			break;
		case D_DAY:
			if (c == ' ' && flag)
				;
			else if (isdigit(c)) {
				flag = 0;
				n = 10 * n + (c - '0');
			} else {
				day = n;
				n = 0;
				state = isctime ? D_HOUR : D_MON;
			}
			break;
		case D_YEAR:
			if (isdigit(c))
				n = 10 * n + (c - '0');
			else {
				year = n;
				n = 0;
				state = isctime ? D_END : D_HOUR;
			}
			break;
		case D_HOUR:
			if (isdigit(c))
				n = 10 * n + (c - '0');
			else {
				hour = n;
				n = 0;
				state = D_MIN;
			}
			break;
		case D_MIN:
			if (isdigit(c))
				n = 10 * n + (c - '0');
			else {
				min = n;
				n = 0;
				state = D_SEC;
			}
			break;
		case D_SEC:
			if (isdigit(c))
				n = 10 * n + (c - '0');
			else {
				sec = n;
				n = 0;
				state = isctime ? D_YEAR : D_END;
			}
			break;
		}
	} while (state != D_END && c);
	switch (month[0]) {
	case 'A':
		mon = (month[1] == 'p') ? 4 : 8;
		break;
	case 'D':
		mon = 12;
		break;
	case 'F':
		mon = 2;
		break;
	case 'J':
		mon = (month[1] == 'a') ? 1 : ((month[2] == 'l') ? 7 : 6);
		break;
	case 'M':
		mon = (month[2] == 'r') ? 3 : 5;
		break;
	case 'N':
		mon = 11;
		break;
	case 'O':
		mon = 10;
		break;
	case 'S':
		mon = 9;
		break;
	default:
		return -1;
	}
	if (year <= 100)
		year += (year < 70) ? 2000 : 1900;
	--mon;
	--day;
	if (sec >= 60 || min >= 60 || hour >= 60 || day >= 31 || year < 1970)
		return -1;
	return sec + 60L * (min + 60L * (hour + 24L * (
		day + daytab[year % 4 == 0][mon] + 365L * (year - 1970L) + ((year - 1969L) >> 2))));
}
