/*
 *   Unreal Internet Relay Chat Daemon, src/s_extra.c
 *   (C) 1999-2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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
#include <stdarg.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#include "proto.h"

ID_Copyright("(C) Carsten Munk 1999");

/*
    fl->type = 
       0     = set by dccconf.conf
       1     = set by services
       2     = set by ircops by /dccdeny
*/

#define AllocCpy(x,y) x = (char *) MyMalloc(strlen(y) + 1); strcpy(x,y)

/* ircd.dcc configuration */

ConfigItem_deny_dcc *dcc_isforbidden(aClient *cptr, aClient *sptr, aClient *target, char *filename)
{
	ConfigItem_deny_dcc *p;

	if (!conf_deny_dcc || !target || !filename)
		return NULL;

	if (IsOper(sptr) || IsULine(sptr))
		return NULL;

	if (IsOper(target))
		return NULL;
	if (IsVictim(target))
	{
		return NULL;
	}
	for (p = conf_deny_dcc; p; p = (ConfigItem_deny_dcc *) p->next)
	{
		if (!match(p->filename, filename))
		{
			return p;
		}
	}

	/* no target found */
	return NULL;
}

void dcc_sync(aClient *sptr)
{
	ConfigItem_deny_dcc *p;
	for (p = conf_deny_dcc; p; p = (ConfigItem_deny_dcc *) p->next)
	{
		if (p->flag.type2 == CONF_BAN_TYPE_AKILL)
			sendto_one(sptr, ":%s %s + %s :%s", me.name,
			    (IsToken(sptr) ? TOK_SVSFLINE : MSG_SVSFLINE),
			    p->filename, p->reason);
	}
}

void	DCCdeny_add(char *filename, char *reason, int type)
{
	ConfigItem_deny_dcc *deny = NULL;
	
	deny = (ConfigItem_deny_dcc *) MyMallocEx(sizeof(ConfigItem_deny_dcc));
	deny->filename = strdup(filename);
	deny->reason = strdup(reason);
	deny->flag.type2 = type;
	AddListItem(deny, conf_deny_dcc);
}

void	DCCdeny_del(ConfigItem_deny_dcc *deny)
{
	DelListItem(deny, conf_deny_dcc);
	if (deny->filename)
		MyFree(deny->filename);
	if (deny->reason)
		MyFree(deny->reason);
	MyFree(deny);
}

void dcc_wipe_services(void)
{
	ConfigItem_deny_dcc *dconf, *next;
	
	for (dconf = conf_deny_dcc; dconf; dconf = (ConfigItem_deny_dcc *) next)
	{
		next = (ConfigItem_deny_dcc *)dconf->next;
		if ((dconf->flag.type2 == CONF_BAN_TYPE_AKILL))
		{
			DelListItem(dconf, conf_deny_dcc);
			if (dconf->filename)
				MyFree(dconf->filename);
			if (dconf->reason)
				MyFree(dconf->reason);
			MyFree(dconf);
		}
	}

}

/* restrict channel stuff */

int  channel_canjoin(aClient *sptr, char *name)
{
	ConfigItem_deny_channel *p;

	if (IsOper(sptr))
		return 1;
	if (IsULine(sptr))
		return 1;
	if (!conf_deny_channel)
		return 1;
	p = Find_channel_allowed(name);
	if (p)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** %s",
			me.name, sptr->name, p->reason);
		return 0;
	}
	return 1;
}

/* irc logs.. */
void ircd_log(int flags, char *format, ...)
{
	va_list ap;
	ConfigItem_log *logs;
	char buf[2048], timebuf[128];
	int fd;
	struct stat fstats;

	va_start(ap, format);
	ircvsprintf(buf, format, ap);	
	strlcat(buf, "\n", sizeof buf);
	snprintf(timebuf, sizeof timebuf, "[%s] - ", myctime(TStime()));
	for (logs = conf_log; logs; logs = (ConfigItem_log *) logs->next) {
#ifdef HAVE_SYSLOG
		if (!stricmp(logs->file, "syslog") && logs->flags & flags) {
#ifdef HAVE_VSYSLOG
			vsyslog(LOG_INFO, format, ap);
#else
			/* %s just to be safe */
			syslog(LOG_INFO, "%s", buf);
#endif
			continue;
		}
#endif
		if (logs->flags & flags) {
			if (stat(logs->file, &fstats) != -1 && logs->maxsize && fstats.st_size >= logs->maxsize) {
#ifndef _WIN32
				fd = open(logs->file, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
#else
				fd = open(logs->file, O_CREAT|O_WRONLY|O_TRUNC, S_IREAD|S_IWRITE);
#endif
				if (fd == -1)
					continue;
				write(fd, "Max file size reached, starting new log file\n", 45);
			}
			else {
#ifndef _WIN32
			fd = open(logs->file, O_CREAT|O_APPEND|O_WRONLY, S_IRUSR|S_IWUSR);
#else
			fd = open(logs->file, O_CREAT|O_APPEND|O_WRONLY, S_IREAD|S_IWRITE);
#endif
			if (fd == -1)
				continue;
			}	
			write(fd, timebuf, strlen(timebuf));
			write(fd, buf, strlen(buf));
			close(fd);
		}
	}
	va_end(ap);
}
