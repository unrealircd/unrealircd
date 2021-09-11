/*
 *   Unreal Internet Relay Chat Daemon, src/debug.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

/** @file
 * @brief Some debugging functions that should probably be moved elsewhere.
 */

/* debug.c 2.30 1/3/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen */

#include "unrealircd.h"

/** String of server options in this compile (eg 'D' for debug mode).
 */
MODVAR char serveropts[] = {
#ifdef	DEBUGMODE
	'D',
#endif
	/* FDLIST (always) */
	'F',
	/* Hub (always) */
	'h',
	/* NOSPOOF (always) */
	'n',
#ifdef	VALLOC
	'V',
#endif
#ifdef	_WIN32
	'W',
#endif
#ifdef	USE_SYSLOG
	'Y',
#endif
	'6',
#ifndef NO_OPEROVERRIDE
	'O',
#endif
#ifndef OPEROVERRIDE_VERIFY
	'o',
#endif
	'E',
#ifdef USE_LIBCURL
	'r',
#endif
	'\0', /* Don't change those nuls. -- Syzop */
	'\0',
	'\0',
	'\0',
	'\0'
};

char *extraflags = NULL;

MODVAR int debugfd = 2;

void	flag_add(char ch)
{
	char *newextra;
	if (extraflags)
	{
		char tmp[2] = { ch, 0 };
		newextra = safe_alloc(strlen(extraflags) + 2);
		strcpy(newextra, extraflags);
		strcat(newextra, tmp);
		safe_free(extraflags);
		extraflags = newextra;
	}
	else
	{
		extraflags = safe_alloc(2);
		extraflags[0] = ch;
		extraflags[1] = '\0';
	}
}

void	flag_del(char ch)
{
	int newsz;
	char *p, *op;
	char *newflags;
	newsz = 0;	
	p = extraflags;
	for (newsz = 0, p = extraflags; *p; p++)
		if (*p != ch)
			newsz++;
	newflags = safe_alloc(newsz + 1);
	for (p = newflags, op = extraflags; (*op) && (newsz); newsz--, op++)
		if (*op != ch)
			*p++ = *op;
	*p = '\0';
	safe_free(extraflags);
	extraflags = newflags;
}



#ifdef DEBUGMODE

#ifndef _WIN32
#define SET_ERRNO(x) errno = x
#else
#define SET_ERRNO(x) WSASetLastError(x)
#endif /* _WIN32 */

static char debugbuf[4096];

void debug(int level, FORMAT_STRING(const char *form), ...)
{
	int err = ERRNO;

	va_list vl;
	va_start(vl, form);

	if ((debuglevel >= 0) && (level <= debuglevel))
	{
		(void)ircvsnprintf(debugbuf, sizeof(debugbuf), form, vl);

#ifndef _WIN32
		strlcat(debugbuf, "\n", sizeof(debugbuf));
		if (write(debugfd, debugbuf, strlen(debugbuf)) < 0)
		{
			/* Yeah.. what can we do if output isn't working? Outputting an error makes no sense */
			;
		}
#else
		strlcat(debugbuf, "\r\n", sizeof(debugbuf));
		OutputDebugString(debugbuf);
#endif
	}
	va_end(vl);
	SET_ERRNO(err);
}

int checkprotoflags(Client *client, int flags, const char *file, int line)
{
	if (!MyConnect(client))
	{
		unreal_log(ULOG_ERROR, "main", "BUG_ISTOKEN_REMOTE_CLIENT", client,
		           "IsToken($token_value) used on remote client in $file:$line",
		           log_data_integer("token_value", flags),
		           log_data_string("file", file),
		           log_data_integer("line", line));
	}
	return ((client->local->proto & flags) == flags) ? 1 : 0;
}
#endif
