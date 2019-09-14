/*
 *   Unreal Internet Relay Chat Daemon, src/extra.c
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

#include "unrealircd.h"

ID_Copyright("(C) Carsten Munk 1999");

/* e->flag.type2:
 * CONF_BAN_TYPE_AKILL		banned by SVSFLINE ('global')
 * CONF_BAN_TYPE_CONF		banned by conf
 * CONF_BAN_TYPE_TEMPORARY	banned by /DCCDENY ('temporary')
 * e->flag.type:
 * DCCDENY_HARD				Fully denied
 * DCCDENY_SOFT				Denied, but allowed to override via /DCCALLOW
 */

/* checks if the dcc is blacklisted.
 */
ConfigItem_deny_dcc *dcc_isforbidden(Client *sptr, char *filename)
{
ConfigItem_deny_dcc *d;
ConfigItem_allow_dcc *a;

	if (!conf_deny_dcc || !filename)
		return NULL;

	for (d = conf_deny_dcc; d; d = d->next)
	{
		if ((d->flag.type == DCCDENY_HARD) && match_simple(d->filename, filename))
		{
			for (a = conf_allow_dcc; a; a = a->next)
			{
				if ((a->flag.type == DCCDENY_HARD) && match_simple(a->filename, filename))
					return NULL;
			}
			return d;
		}
	}

	return NULL;
}

/* checks if the dcc is discouraged ('soft bans').
 */
ConfigItem_deny_dcc *dcc_isdiscouraged(Client *sptr, char *filename)
{
ConfigItem_deny_dcc *d;
ConfigItem_allow_dcc *a;

	if (!conf_deny_dcc || !filename)
		return NULL;

	for (d = conf_deny_dcc; d; d = d->next)
	{
		if ((d->flag.type == DCCDENY_SOFT) && match_simple(d->filename, filename))
		{
			for (a = conf_allow_dcc; a; a = a->next)
			{
				if ((a->flag.type == DCCDENY_SOFT) && match_simple(a->filename, filename))
					return NULL;
			}
			return d;
		}
	}

	return NULL;
}

void dcc_sync(Client *sptr)
{
	ConfigItem_deny_dcc *p;
	for (p = conf_deny_dcc; p; p = p->next)
	{
		if (p->flag.type2 == CONF_BAN_TYPE_AKILL)
			sendto_one(sptr, NULL, ":%s SVSFLINE + %s :%s", me.name,
			    p->filename, p->reason);
	}
}

void	DCCdeny_add(char *filename, char *reason, int type, int type2)
{
	ConfigItem_deny_dcc *deny = NULL;
	
	deny = safe_alloc(sizeof(ConfigItem_deny_dcc));
	safe_strdup(deny->filename, filename);
	safe_strdup(deny->reason, reason);
	deny->flag.type = type;
	deny->flag.type2 = type2;
	AddListItem(deny, conf_deny_dcc);
}

void	DCCdeny_del(ConfigItem_deny_dcc *deny)
{
	DelListItem(deny, conf_deny_dcc);
	safe_free(deny->filename);
	safe_free(deny->reason);
	safe_free(deny);
}

void dcc_wipe_services(void)
{
	ConfigItem_deny_dcc *dconf, *next;
	
	for (dconf = conf_deny_dcc; dconf; dconf = next)
	{
		next = dconf->next;
		if (dconf->flag.type2 == CONF_BAN_TYPE_AKILL)
		{
			DelListItem(dconf, conf_deny_dcc);
			if (dconf->filename)
				safe_free(dconf->filename);
			if (dconf->reason)
				safe_free(dconf->reason);
			safe_free(dconf);
		}
	}

}

/* The dccallow functions below are all taken from bahamut (1.8.1).
 * Well, with some small modifications of course. -- Syzop
 */
int on_dccallow_list(Client *to, Client *from)
{
Link *lp;

	for(lp = to->user->dccallow; lp; lp = lp->next)
		if(lp->flags == DCC_LINK_ME && lp->value.cptr == from)
			return 1;
	return 0;
}
int add_dccallow(Client *sptr, Client *optr)
{
Link *lp;
int cnt = 0;

	for (lp = sptr->user->dccallow; lp; lp = lp->next)
	{
		if (lp->flags != DCC_LINK_ME)
			continue;
		cnt++;
		if (lp->value.cptr == optr)
			return 0;
	}

	if (cnt >= MAXDCCALLOW)
	{
		sendnumeric(sptr, ERR_TOOMANYDCC,
			optr->name, MAXDCCALLOW);
		return 0;
	}
	
	lp = make_link();
	lp->value.cptr = optr;
	lp->flags = DCC_LINK_ME;
	lp->next = sptr->user->dccallow;
	sptr->user->dccallow = lp;
	
	lp = make_link();
	lp->value.cptr = sptr;
	lp->flags = DCC_LINK_REMOTE;
	lp->next = optr->user->dccallow;
	optr->user->dccallow = lp;
	
	sendnumeric(sptr, RPL_DCCSTATUS, optr->name, "added to");
	return 0;
}

int del_dccallow(Client *sptr, Client *optr)
{
Link **lpp, *lp;
int found = 0;

	for (lpp = &(sptr->user->dccallow); *lpp; lpp=&((*lpp)->next))
	{
		if ((*lpp)->flags != DCC_LINK_ME)
			continue;
		if ((*lpp)->value.cptr == optr)
		{
			lp = *lpp;
			*lpp = lp->next;
			free_link(lp);
			found++;
			break;
		}
	}
	if (!found)
	{
		sendnumericfmt(sptr, RPL_DCCINFO, "%s is not in your DCC allow list", optr->name);
		return 0;
	}
	
	for (found = 0, lpp = &(optr->user->dccallow); *lpp; lpp=&((*lpp)->next))
	{
		if ((*lpp)->flags != DCC_LINK_REMOTE)
			continue;
		if ((*lpp)->value.cptr == sptr)
		{
			lp = *lpp;
			*lpp = lp->next;
			free_link(lp);
			found++;
			break;
		}
	}
	if (!found)
		sendto_realops("[BUG!] %s was in dccallowme list of %s but not in dccallowrem list!",
			optr->name, sptr->name);

	sendnumeric(sptr, RPL_DCCSTATUS, optr->name, "removed from");

	return 0;
}

/* irc logs.. */
void ircd_log(int flags, FORMAT_STRING(const char *format), ...)
{
	static int last_log_file_warning = 0;
	static char recursion_trap=0;

	va_list ap;
	ConfigItem_log *logs;
	char buf[2048], timebuf[128];
	struct stat fstats;
	int written = 0;
	int n;

	/* Trap infinite recursions to avoid crash if log file is unavailable,
	 * this will also avoid calling ircd_log from anything else called
	 */
	if (recursion_trap == 1)
		return;

	recursion_trap = 1;
	va_start(ap, format);
	ircvsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	snprintf(timebuf, sizeof(timebuf), "[%s] - ", myctime(TStime()));

	RunHook3(HOOKTYPE_LOG, flags, timebuf, buf);
	strlcat(buf, "\n", sizeof(buf));

	if (!loop.ircd_forked && (flags & LOG_ERROR))
		fprintf(stderr, "%s", buf);

	for (logs = conf_log; logs; logs = logs->next) {
#ifdef HAVE_SYSLOG
		if (!strcasecmp(logs->file, "syslog") && logs->flags & flags) {
			syslog(LOG_INFO, "%s", buf);
			written++;
			continue;
		}
#endif
		if (logs->flags & flags)
		{
			if (stat(logs->file, &fstats) != -1 && logs->maxsize && fstats.st_size >= logs->maxsize)
			{
				char oldlog[512];
				if (logs->logfd == -1)
				{
					/* Try to open, so we can write the 'Max file size reached' message. */
#ifndef _WIN32
					logs->logfd = fd_fileopen(logs->file, O_CREAT|O_APPEND|O_WRONLY);
#else
					logs->logfd = fd_fileopen(logs->file, O_CREAT|O_APPEND|O_WRONLY);
#endif
				}
				if (logs->logfd != -1)
				{
					if (write(logs->logfd, "Max file size reached, starting new log file\n", 45) < 0)
					{
						/* We already handle the unable to write to log file case for normal data.
						 * I think we can get away with not handling this one.
						 */
						;
					}
					fd_close(logs->logfd);
				}
				
				/* Rename log file to xxxxxx.old */
				snprintf(oldlog, sizeof(oldlog), "%s.old", logs->file);
				rename(logs->file, oldlog);
				
				logs->logfd = fd_fileopen(logs->file, O_CREAT|O_WRONLY|O_TRUNC);
				if (logs->logfd == -1)
					continue;
			}
			else if (logs->logfd == -1) {
#ifndef _WIN32
				logs->logfd = fd_fileopen(logs->file, O_CREAT|O_APPEND|O_WRONLY);
#else
				logs->logfd = fd_fileopen(logs->file, O_CREAT|O_APPEND|O_WRONLY);
#endif
				if (logs->logfd == -1)
				{
					if (!loop.ircd_booted)
					{
						config_status("WARNING: Unable to write to '%s': %s", logs->file, strerror(ERRNO));
					} else {
						if (last_log_file_warning + 300 < TStime())
						{
							config_status("WARNING: Unable to write to '%s': %s. This warning will not re-appear for at least 5 minutes.", logs->file, strerror(ERRNO));
							last_log_file_warning = TStime();
						}
					}
					continue;
				}
			}
			/* this shouldn't happen, but lets not waste unnecessary syscalls... */
			if (logs->logfd == -1)
				continue;
			if (write(logs->logfd, timebuf, strlen(timebuf)) < 0)
			{
				/* Let's ignore any write errors for this one. Next write() will catch it... */
				;
			}
			n = write(logs->logfd, buf, strlen(buf));
			if (n == strlen(buf))
			{
				written++;
			}
			else
			{
				if (!loop.ircd_booted)
				{
					config_status("WARNING: Unable to write to '%s': %s", logs->file, strerror(ERRNO));
				} else {
					if (last_log_file_warning + 300 < TStime())
					{
						config_status("WARNING: Unable to write to '%s': %s. This warning will not re-appear for at least 5 minutes.", logs->file, strerror(ERRNO));
						last_log_file_warning = TStime();
					}
				}
			}
		}
	}

	recursion_trap = 0;
}
