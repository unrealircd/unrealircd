/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_kill.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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
#endif
#include <fcntl.h>
#include "h.h"
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_kill(aClient *cptr, aClient *sptr, int parc, char *parv[]);
static char buf[BUFSIZE], buf2[BUFSIZE];

/* Place includes here */
#define MSG_KILL        "KILL"  /* KILL */
#define TOK_KILL        "."     /* 46 */

ModuleHeader MOD_HEADER(m_kill)
  = {
	"kill",	/* Name of module */
	"$Id$", /* Version */
	"command /kill", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_kill)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_KILL, TOK_KILL, m_kill, 2);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_kill)(int module_load)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_kill)(int module_unload)
{
	if (del_Command(MSG_KILL, TOK_KILL, m_kill) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_kill).name);
	}
	return MOD_SUCCESS;
	
}


/*
** m_kill
**	parv[0] = sender prefix
**	parv[1] = kill victim(s) - comma separated list
**	parv[2] = kill path
*/
DLLFUNC int  m_kill(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	anUser *auser;
	char inpath[HOSTLEN * 2 + USERLEN + 5];
	char *oinpath = get_client_name(cptr, FALSE);
	char *user, *path, *killer, *nick, *p, *s;
	int  chasing = 0, kcount = 0;



	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "KILL");
		return 0;
	}

	user = parv[1];
	path = parv[2];		/* Either defined or NULL (parc >= 2!!) */

	strlcpy(inpath, oinpath, sizeof inpath);

#ifndef ROXnet
	if (IsServer(cptr) && (s = (char *)index(inpath, '.')) != NULL)
		*s = '\0';	/* Truncate at first "." */
#endif

	if (!IsPrivileged(cptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (IsAnOper(cptr))
	{
		if (BadPtr(path))
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			    me.name, parv[0], "KILL");
			return 0;
		}
		if (strlen(path) > (size_t)TOPICLEN)
			path[TOPICLEN] = '\0';
	}

	if (MyClient(sptr))
		user = (char *)canonize(user);

	for (p = NULL, nick = strtoken(&p, user, ","); nick;
	    nick = strtoken(&p, NULL, ","))
	{

		chasing = 0;

		if (!(acptr = find_client(nick, NULL)))
		{
			/*
			   ** If the user has recently changed nick, we automaticly
			   ** rewrite the KILL for this new nickname--this keeps
			   ** servers in synch when nick change and kill collide
			 */
			if (!(acptr =
			    get_history(nick, (long)KILLCHASETIMELIMIT)))
			{
				sendto_one(sptr, err_str(ERR_NOSUCHNICK),
				    me.name, parv[0], nick);
				continue;
			}
			sendto_one(sptr,
			    ":%s %s %s :*** KILL changed from %s to %s",
			    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], nick, acptr->name);
			chasing = 1;
		}
		if ((!MyConnect(acptr) && MyClient(cptr) && !OPCanGKill(cptr))
		    || (MyConnect(acptr) && MyClient(cptr)
		    && !OPCanLKill(cptr)))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			continue;
		}
		if (IsServer(acptr) || IsMe(acptr))
		{
			sendto_one(sptr, err_str(ERR_CANTKILLSERVER),
			    me.name, parv[0]);
			continue;
		}
		if (!IsPerson(acptr))
		{
			/* Nick exists but user is not registered yet: IOTW "doesn't exist". -- Syzop */
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
			    me.name, parv[0], nick);
			continue;
		}

		if (IsServices(acptr) && !(IsNetAdmin(sptr) || IsULine(sptr)))
		{
			sendto_one(sptr, err_str(ERR_KILLDENY), me.name,
			    parv[0], parv[1]);
			return 0;
		}
		/* From here on, the kill is probably going to be successful. */

		kcount++;

		if (!IsServer(sptr) && (kcount > MAXKILLS))
		{
			sendto_one(sptr,
			    ":%s %s %s :*** Too many targets, kill list was truncated. Maximum is %d.",
			    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], MAXKILLS);
			break;
		}
		if (!IsServer(cptr))
		{
			/*
			   ** The kill originates from this server, initialize path.
			   ** (In which case the 'path' may contain user suplied
			   ** explanation ...or some nasty comment, sigh... >;-)
			   **
			   **   ...!operhost!oper
			   **   ...!operhost!oper (comment)
			 */
			strlcpy(inpath, GetHost(cptr), sizeof inpath);
			if (kcount < 2) {	/* Only check the path the first time
					   around, or it gets appended to itself. */
				if (!BadPtr(path))
				{
					(void)ircsprintf(buf, "%s%s (%s)",
					    cptr->name,
					    IsOper(sptr) ? "" : "(L)", path);
					path = buf;
				}
				else
					path = cptr->name;
			}
		}
		else if (BadPtr(path))
			path = "*no-path*";	/* Bogus server sending??? */
		/*
		   ** Notify all *local* opers about the KILL (this includes the one
		   ** originating the kill, if from this server--the special numeric
		   ** reply message is not generated anymore).
		   **
		   ** Note: "acptr->name" is used instead of "user" because we may
		   **    have changed the target because of the nickname change.
		 */

		auser = acptr->user;

		if (index(parv[0], '.'))
			sendto_snomask(SNO_KILLS,
			    "*** Notice -- Received KILL message for %s!%s@%s from %s Path: %s!%s",
			    acptr->name, auser->username,
			    IsHidden(acptr) ? auser->virthost : auser->realhost,
			    parv[0], inpath, path);
		else
			sendto_snomask(SNO_KILLS,
			    "*** Notice -- Received KILL message for %s!%s@%s from %s Path: %s!%s",
			    acptr->name, auser->username,
			    IsHidden(acptr) ? auser->virthost : auser->realhost,
			    parv[0], inpath, path);
#if defined(USE_SYSLOG) && defined(SYSLOG_KILL)
		if (IsOper(sptr))
			syslog(LOG_DEBUG, "KILL From %s For %s Path %s!%s",
			    parv[0], acptr->name, inpath, path);
#endif
		/*
		 * By otherguy
		*/
                ircd_log
                    (LOG_KILL, "KILL (%s) by  %s(%s!%s)",
                           make_nick_user_host
                     (acptr->name, acptr->user->username, GetHost(acptr)),
                            parv[0],
                            inpath,
                            path);
		/*
		   ** And pass on the message to other servers. Note, that if KILL
		   ** was changed, the message has to be sent to all links, also
		   ** back.
		   ** Suicide kills are NOT passed on --SRB
		 */
		if (!MyConnect(acptr) || !MyConnect(sptr) || !IsAnOper(sptr))
		{
			sendto_serv_butone(cptr, ":%s KILL %s :%s!%s",
			    parv[0], acptr->name, inpath, path);
			if (chasing && IsServer(cptr))
				sendto_one(cptr, ":%s KILL %s :%s!%s",
				    me.name, acptr->name, inpath, path);
			acptr->flags |= FLAGS_KILLED;
		}

		/*
		   ** Tell the victim she/he has been zapped, but *only* if
		   ** the victim is on current server--no sense in sending the
		   ** notification chasing the above kill, it won't get far
		   ** anyway (as this user don't exist there any more either)
		 */
		if (MyConnect(acptr))
			sendto_prefix_one(acptr, sptr, ":%s KILL %s :%s!%s",
			    parv[0], acptr->name, inpath, path);
		/*
		   ** Set FLAGS_KILLED. This prevents exit_one_client from sending
		   ** the unnecessary QUIT for this. (This flag should never be
		   ** set in any other place)
		 */
		if (MyConnect(acptr) && MyConnect(sptr) && IsAnOper(sptr))

			(void)ircsprintf(buf2, "[%s] Local kill by %s (%s)",
			    me.name, sptr->name,
			    BadPtr(parv[2]) ? sptr->name : parv[2]);
		else
		{
			if ((killer = index(path, ' ')))
			{
				while ((killer >= path) && *killer && *killer != '!')
					killer--;
				if (!*killer)
					killer = path;
				else
					killer++;
			}
			else
				killer = path;
			(void)ircsprintf(buf2, "Killed (%s)", killer);
		}

		if (exit_client(cptr, acptr, sptr, buf2) == FLUSH_BUFFER)
			return FLUSH_BUFFER;
	}
	return 0;
}
