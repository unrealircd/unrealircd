/************************************************************************
/************************************************************************
 *   IRC - Internet Relay Chat, s_unreal.c
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
#include "userload.h"
#include "version.h"
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

ID_Copyright("(C) Carsten Munk 1999");

time_t TSoffset = 0;
extern ircstats IRCstats;

#ifndef NO_FDLIST
extern float currentrate;
extern float currentrate2;
extern float highest_rate;
extern float highest_rate2;
extern int lifesux;
extern int noisy_htm;
extern int LCF;
extern int LRV;
#endif
/*
   m_sethost() added by Stskeeps (30/04/1999)
               (modified at 15/05/1999) by Stskeeps | Potvin
   :prefix SETHOST newhost
   parv[0] - sender
   parv[1] - newhost
   D: this performs a mode +x function to set hostname
      to whatever you want to (if you are IRCop) **efg**
      Very experimental currently
   A: Remember to see server_etabl ;)))))
      *evil fucking grin*
*/
int  m_sethost(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *vhost, *s;
#ifndef DISABLE_USERMOD
	int  permit = 0;	/* 0 = opers(glob/locop) 1 = global oper 2 = not MY clients.. */
#else
	int  permit = 2;
#endif
	int  legalhost = 1;	/* is legal characters? */


	if (!MyConnect(sptr))
		goto have_permit1;
	switch (permit)
	{
	  case 0:
		  if (!IsAnOper(sptr))
		  {
			  sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			      parv[0]);
			  return 0;
		  }
		  break;
	  case 1:
		  if (!IsOper(sptr))
		  {
			  sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			      parv[0]);
			  return 0;
		  }
		  break;
	  case 2:
		  if (MyConnect(sptr))
		  {
			  sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			      parv[0]);
			  return 0;
		  }
	  default:
		  sendto_ops_butone(IsServer(cptr) ? cptr : NULL, sptr,
		      ":%s WALLOPS :[SETHOST] Somebody fixing this corrupted server? !(0|1) !!!",
		      me.name);
		  break;
	}

      have_permit1:
	if (parc < 2)
		vhost = NULL;
	else
		vhost = parv[1];

	/* bad bad bad boys .. ;p */
	if (vhost == NULL)
	{
		if (MyConnect(sptr))
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** Syntax: /SetHost <new host>",
			    me.name, parv[0]);
		}
		return;
	}
	/* uh uh .. too small */
	if (strlen(parv[1]) < 1)
	{
		if (MyConnect(sptr))
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /SetHost Error: Atleast write SOMETHING that makes sense (':' string)",
			    me.name, sptr->name);
	}
	/* too large huh? */
	if (strlen(parv[1]) > (HOSTLEN))
	{
		/* ignore us as well if we're not a child of 3k */
		if (MyConnect(sptr))
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /SetHost Error: Hostnames are limited to %i characters.",
			    me.name, sptr->name, HOSTLEN);
		return;
	}

	/* illegal?! */
	for (s = vhost; *s; s++)
	{
		if (!isallowed(*s))
		{
			legalhost = 0;
		}
	}

	if (legalhost == 0)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /SetHost Error: A hostname may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them",
		    me.name, parv[0]);
		return 0;
	}

	/* hide it */
	sptr->umodes |= UMODE_HIDE;
	sptr->umodes |= UMODE_SETHOST;
	/* get it in */
	if (sptr->user->virthost)
		MyFree(sptr->user->virthost);
	sptr->user->virthost = MyMalloc(strlen(vhost) + 1);
	ircsprintf(sptr->user->virthost, "%s", vhost);
	/* spread it out */
	sendto_serv_butone_token(cptr, sptr->name, MSG_SETHOST, TOK_SETHOST,
	    "%s", parv[1]);

	if (MyConnect(sptr))
	{
		sendto_one(sptr, ":%s MODE %s :+xt", sptr->name, sptr->name);
		sendto_one(sptr,
		    ":%s NOTICE %s :Your nick!user@host-mask is now (%s!%s@%s) - To disable it type /mode %s -x",
		    me.name, parv[0], parv[0], sptr->user->username, vhost,
		    parv[0]);
	}
	return 0;
}

/* 
 * m_chghost - 12/07/1999 (two months after I made SETIDENT) - Stskeeps
 * :prefix CHGHOST <nick> <new hostname>
 * parv[0] - sender
 * parv[1] - nickname
 * parv[2] - hostname
 *
*/

int  m_chghost(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *acptr;
	char *s;
	int  legalhost = 1;

#ifdef DISABLE_USERMOD
	if (MyClient(sptr))
	{
		sendto_one(sptr, ":%s NOTICE %s :*** The /chghost command is disabled on this server", me.name, sptr->name);
		return 0;
	}
#endif

	if (MyClient(sptr))
		if (!IsAnOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;

		}

	if (parc < 3)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /ChgHost syntax is /ChgHost <nick> <newhost>",
		    me.name, sptr->name);
		return 0;
	}

	if (strlen(parv[2]) < 1)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Write atleast something to change the host to!",
		    me.name, sptr->name);
		return 0;
	}

	if (strlen(parv[2]) > (HOSTLEN))
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** ChgHost Error: Too long hostname!!",
		    me.name, sptr->name);
		return 0;
	}

	/* illegal?! */
	for (s = parv[2]; *s; s++)
	{
		if (!isallowed(*s))
		{
			legalhost = 0;
		}
	}

	if (legalhost == 0)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /ChgHost Error: A hostname may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them",
		    me.name, parv[0]);
		return 0;
	}

	if ((acptr = find_person(parv[1], NULL)))
	{
		if (!IsULine(sptr))
		{
			sendto_umode(UMODE_EYES,
			    "%s changed the virtual hostname of %s (%s@%s) to be %s",
			    sptr->name, acptr->name, acptr->user->username,
			    acptr->user->realhost, parv[2]);
		}
		acptr->umodes |= UMODE_HIDE;
		acptr->umodes |= UMODE_SETHOST;
		sendto_serv_butone_token(cptr, sptr->name,
		    MSG_CHGHOST, TOK_CHGHOST, "%s %s", acptr->name, parv[2]);
		if (acptr->user->virthost)
			MyFree(acptr->user->virthost);
		acptr->user->virthost = MyMalloc(strlen(parv[2]) + 1);
		ircsprintf(acptr->user->virthost, "%s", parv[2]);
		return 0;
	}
	else
	{
		sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name,
		    parv[1]);
		return 0;
	}
	return 0;
}

/* 
 * m_chgident - 12/07/1999 (two months after I made SETIDENT) - Stskeeps
 * :prefix CHGHOST <nick> <new identname>
 * parv[0] - sender
 * parv[1] - nickname
 * parv[2] - identname
 *
*/

int  m_chgident(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *acptr;
	char *s;
	int  legalident = 1;

#ifdef DISABLE_USERMOD
	if (MyClient(sptr))
	{
		sendto_one(sptr, ":%s NOTICE %s :*** The /chgident command is disabled on this server", me.name, sptr->name);
		return 0;
	}
#endif

	if (MyClient(sptr))
		if (!IsAnOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;

		}

	if (parc < 3)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /ChgIdent syntax is /ChgIdent <nick> <newident>",
		    me.name, sptr->name);
		return 0;
	}

	if (strlen(parv[2]) < 1)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Write atleast something to change the ident to!",
		    me.name, sptr->name);
		return 0;
	}

	if (strlen(parv[2]) > (USERLEN))
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** ChgIdent Error: Too long ident!!",
		    me.name, sptr->name);
		return 0;
	}

	/* illegal?! */
	for (s = parv[2]; *s; s++)
	{
		if (!isallowed(*s))
		{
			legalident = 0;
		}
	}

	if (legalident == 0)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /ChgIdent Error: A ident may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them",
		    me.name, parv[0]);
		return 0;
	}

	if ((acptr = find_person(parv[1], NULL)))
	{
		if (!IsULine(sptr))
		{
			sendto_umode(UMODE_EYES,
			    "%s changed the virtual ident of %s (%s@%s) to be %s",
			    sptr->name, acptr->name, acptr->user->username,
			    (acptr->umodes & UMODE_HIDE ? acptr->
			    user->realhost : acptr->user->realhost), parv[2]);
		}
		sendto_serv_butone_token(cptr, sptr->name,
		    MSG_CHGIDENT,
		    TOK_CHGIDENT, "%s %s", acptr->name, parv[2]);
		ircsprintf(acptr->user->username, "%s", parv[2]);
		return 0;
	}
	else
	{
		sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name,
		    parv[1]);
		return 0;
	}
	return 0;
}

/* m_setident - 12/05/1999 - Stskeeps
 *  :prefix SETIDENT newident
 *  parv[0] - sender
 *  parv[1] - newident
 *  D: This will set your username to be <x> (like (/setident Root))
 *     (if you are IRCop) **efg*
 *     Very experimental currently
 * 	   Cloning of m_sethost at some points - so same authors ;P
*/

int  m_setident(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{

	char *vident, *s;
#ifndef DISABLE_USERMOD
	int  permit = 0;	/* 0 = opers(glob/locop) 1 = global oper */
#else
	int  permit = 2;
#endif
	int  legalident = 1;	/* is legal characters? */
	if (!MyConnect(sptr))
		goto permit_2;
	switch (permit)
	{
	  case 0:
		  if (!IsAnOper(sptr))
		  {
			  sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			      parv[0]);
			  return 0;
		  }
		  break;
	  case 1:
		  if (!IsOper(sptr))
		  {
			  sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			      parv[0]);
			  return 0;
		  }
		  break;
	  case 2:
		  if (MyConnect(sptr))
		  {
			  sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			      parv[0]);
			  return 0;
		  }
		  break;
	  default:
		  sendto_ops_butone(IsServer(cptr) ? cptr : NULL, sptr,
		      ":%s WALLOPS :[SETIDENT] Somebody fixing this corrupted server? !(0|1) !!!",
		      me.name);
		  break;
	}
      permit_2:
	if (parc < 2)
		vident = NULL;
	else
		vident = parv[1];

	/* bad bad bad boys .. ;p */
	if (vident == NULL)
	{
		if (MyConnect(sptr))
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** Syntax: /SetIdent <new host>",
			    me.name, parv[0]);
		}
		return;
	}
	if (strlen(parv[1]) < 1)
	{
		if (MyConnect(sptr))
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /SetIdent Error: Atleast write SOMETHING that makes sense (':' string)",
			    me.name, sptr->name);
	}

	/* too large huh? */
	if (strlen(vident) > (USERLEN))
	{
		/* ignore us as well if we're not a child of 3k */
		if (MyConnect(sptr))
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /SetIdent Error: Usernames are limited to %i characters.",
			    me.name, sptr->name, USERLEN);
		return;
	}

	/* illegal?! */
	for (s = vident; *s; s++)
	{
		if (!isallowed(*s))
		{
			legalident = 0;
		}
		if (*s == '~')
			legalident = 1;

	}

	if (legalident == 0)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /SetIdent Error: A username may contain a-z, A-Z, 0-9, '-', '~' & '.' - Please only use them",
		    me.name, parv[0]);
		return 0;
	}

	/* get it in */
	ircsprintf(sptr->user->username, "%s", vident);
	/* spread it out */
	sendto_serv_butone_token(cptr, sptr->name,
	    MSG_SETIDENT, TOK_SETIDENT, "%s", parv[1]);

	if (MyConnect(sptr))
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :Your nick!user@host-mask is now (%s!%s@%s) - To disable ident set change it manually by /setident'ing again",
		    me.name, parv[0], parv[0], sptr->user->username,
		    IsHidden(sptr) ? sptr->user->virthost : sptr->
		    user->realhost);
	}
	return;
}
/* m_setname - 12/05/1999 - Stskeeps
 *  :prefix SETNAME :gecos
 *  parv[0] - sender
 *  parv[1] - gecos
 *  D: This will set your gecos to be <x> (like (/setname :The lonely wanderer))
   yes it is experimental but anyways ;P
    FREEDOM TO THE USERS! ;)
*/

int  m_setname(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	if (parc < 2)
		return;
	if (strlen(parv[1]) > (REALLEN))
	{
		if (MyConnect(sptr))
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /SetName Error: \"Real names\" may maximum be %i characters of length",
			    me.name, sptr->name, REALLEN);
		}
		return 0;
	}

	if (strlen(parv[1]) < 1)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :Couldn't change realname - Nothing in parameter",
		    me.name, sptr->name);
		return 0;
	}

	/* set the new name before we check, but don't send to servers unless it is ok */
	else
		ircsprintf(sptr->info, "%s", parv[1]);

	/* Check for n:lines here too */
	if (!IsAnOper(sptr) && Find_ban(sptr->info, CONF_BAN_REALNAME))
	{
		int  xx;
		xx =
		    exit_client(cptr, sptr, &me,
		    "Your GECOS (real name) is banned from this server");
		return xx;
	}
	sendto_serv_butone_token(cptr, sptr->name, MSG_SETNAME, TOK_SETNAME,
	    ":%s", parv[1]);
	if (MyConnect(sptr))
		sendto_one(sptr,
		    ":%s NOTICE %s :Your \"real name\" is now set to be %s - you have to set it manually to undo it",
		    me.name, parv[0], parv[1]);

	return 0;
}

/* m_sdesc - 15/05/1999 - Stskeeps
 *  :prefix SDESC
 *  parv[0] - sender
 *  parv[1] - description
 *  D: Sets server info if you are Server Admin (ONLINE)
*/

int  m_sdesc(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	if (IsCoAdmin(sptr))
		goto sdescok;
	/* ignoring */
	if (!IsAdmin(sptr))
		return;
      sdescok:

	if (parc < 2)
		return;

	if (strlen(parv[1]) < 1)
		if (MyConnect(sptr))
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** Nothing to change to (SDESC)",
			    me.name, sptr->name);
			return 0;
		}
	if (strlen(parv[1]) > (REALLEN))
	{
		if (MyConnect(sptr))
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /SDESC Error: \"Server info\" may maximum be %i characters of length",
			    me.name, sptr->name, REALLEN);
		}
		return 0;
	}

	ircsprintf(sptr->srvptr->info, "%s", parv[1]);

	sendto_serv_butone_token(cptr, sptr->name, MSG_SDESC, TOK_SDESC, ":%s",
	    parv[1]);

	if (MyConnect(sptr))
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :Your \"server description\" is now set to be %s - you have to set it manually to undo it",
		    me.name, parv[0], parv[1]);
		return 0;
	}
	sendto_ops("Server description for %s is now '%s' changed by %s",
	    sptr->srvptr->name, sptr->srvptr->info, parv[0]);
}


/*
** m_admins (Admin chat only) -Potvin
**      parv[0] = sender prefix
**      parv[1] = message text
*/
int  m_admins(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *message;


	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "ADCHAT");
		return 0;
	}
#ifdef ADMINCHAT
	if (MyClient(sptr) && !IsAdmin(sptr))
#else
	if (MyClient(sptr))
#endif
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL, parv[0],
   	    MSG_ADMINCHAT, TOK_ADMINCHAT, ":%s", message);	
#ifdef ADMINCHAT
	sendto_umode(UMODE_ADMIN, "*** AdminChat -- from %s: %s",
	    parv[0], message);
	sendto_umode(UMODE_COADMIN, "*** AdminChat -- from %s: %s",
	    parv[0], message);
#endif
	return 0;
}
/*
** m_techat (Techadmin chat only) -Potvin (cloned by --sts)
**      parv[0] = sender prefix
**      parv[1] = message text
*/
int  m_techat(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *message;


	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "TECHAT");
		return 0;
	}
#ifdef ADMINCHAT
	if (MyClient(sptr))
		if (!(IsTechAdmin(sptr) || IsNetAdmin(sptr)))
#else
	if (MyClient(sptr))
#endif
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL, parv[0],
	   MSG_TECHAT, TOK_TECHAT, ":%s", message);
#ifdef ADMINCHAT
	sendto_umode(UMODE_TECHADMIN, "*** Te-chat -- from %s: %s",
	    parv[0], message);
#endif
	return 0;
}
/*
** m_nachat (netAdmin chat only) -Potvin - another sts cloning
**      parv[0] = sender prefix
**      parv[1] = message text
*/
int  m_nachat(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *message;


	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "NACHAT");
		return 0;
	}
#ifdef ADMINCHAT
	if (MyClient(sptr))
		if (!(IsNetAdmin(sptr) || IsTechAdmin(sptr)))
#else
	if (MyClient(sptr))
#endif
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL, parv[0],
	   MSG_NACHAT, TOK_NACHAT, ":%s", message);
#ifdef ADMINCHAT
	sendto_umode(UMODE_NETADMIN, "*** NetAdmin.Chat -- from %s: %s",
	    parv[0], message);
	sendto_umode(UMODE_TECHADMIN, "*** NetAdmin.Chat -- from %s: %s",
	    parv[0], message);
#endif
	return 0;
}

/* m_lag (lag measure) - Stskeeps
 * parv[0] = prefix
 * parv[1] = server to query
*/

int  m_lag(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{

	if (MyClient(sptr))
		if (!IsAnOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;
		}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "LAG");
		return 0;
	}
	if (*parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "LAG");
		return 0;
	}
	if (hunt_server(cptr, sptr, ":%s LAG :%s", 1, parc,
	    parv) == HUNTED_NOSUCH)
	{
		return 0;
	}

	sendto_one(sptr, ":%s NOTICE %s :Lag reply -- %s %s %li",
	    me.name, sptr->name, me.name, parv[1], TStime());

	return 0;
}


/*
  Help.c interface for command line :)
*/
void unrealmanual(void)
{
	char *str;
	int  x = 0;

	str = MyMalloc(1024);
	printf("Starting UnrealIRCD Interactive help system\n");
	printf("Use 'QUIT' as topic name to get out of help system\n");
	x = parse_help(NULL, NULL, NULL);
	if (x == 0)
	{
		printf("*** Couldn't find main help topic!!\n");
		return;
	}
	while (myncmp(str, "QUIT", 8))
	{
		printf("Topic?> ");
		str = fgets(str, 1023, stdin);
		printf("\n");
		if (myncmp(str, "QUIT", 8))
			x = parse_help(NULL, NULL, str);
		if (x == 0)
		{
			printf("*** Couldn't find help topic '%s'\n", str);
		}
	}
	MyFree(str);
}

static char *militime(char *sec, char *usec)
{
	struct timeval tv;
	static char timebuf[18];
#ifndef _WIN32
	gettimeofday(&tv, NULL);
#else
	/* win32 unreal cannot fix gettimeofday - therefore only 90% precise */
	tv.tv_sec = TStime();
	tv.tv_usec = 0;
#endif
	if (sec && usec)
		ircsprintf(timebuf, "%ld",
		    (tv.tv_sec - atoi(sec)) * 1000 + (tv.tv_usec -
		    atoi(usec)) / 1000);
	else
		ircsprintf(timebuf, "%ld %ld", tv.tv_sec, tv.tv_usec);

	return timebuf;
}

aClient *find_match_server(char *mask)
{
	aClient *acptr;

	if (BadPtr(mask))
		return NULL;
	for (acptr = client, collapse(mask); acptr; acptr = acptr->next)
	{
		if (!IsServer(acptr) && !IsMe(acptr))
			continue;
		if (!match(mask, acptr->name))
			break;
		continue;
	}
	return acptr;
}

/*
 * m_rping  -- by Run
 *
 *    parv[0] = sender (sptr->name thus)
 * if sender is a person: (traveling towards start server)
 *    parv[1] = pinged server[mask]
 *    parv[2] = start server (current target)
 *    parv[3] = optional remark
 * if sender is a server: (traveling towards pinged server)
 *    parv[1] = pinged server (current target)
 *    parv[2] = original sender (person)
 *    parv[3] = start time in s
 *    parv[4] = start time in us
 *    parv[5] = the optional remark
 */
int  m_rping(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;

	if (!IsPrivileged(sptr))
		return 0;

	if (parc < (IsAnOper(sptr) ? (MyConnect(sptr) ? 2 : 3) : 6))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
		    "RPING");
		return 0;
	}
	if (MyClient(sptr))
	{
		if (parc == 2)
			parv[parc++] = me.name;
		else if (!(acptr = find_match_server(parv[2])))
		{
			parv[3] = parv[2];
			parv[2] = me.name;
			parc++;
		}
		else
			parv[2] = acptr->name;
		if (parc == 3)
			parv[parc++] = "<No client start time>";
	}

	if (IsAnOper(sptr))
	{
		if (hunt_server(cptr, sptr, ":%s RPING %s %s :%s", 2, parc,
		    parv) != HUNTED_ISME)
			return 0;
		if (!(acptr = find_match_server(parv[1])) || !IsServer(acptr))
		{
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name,
			    parv[0], parv[1]);
			return 0;
		}
		sendto_one(acptr, ":%s RPING %s %s %s :%s",
		    me.name, acptr->name, sptr->name, militime(NULL, NULL),
		    parv[3]);
	}
	else
	{
		if (hunt_server(cptr, sptr, ":%s RPING %s %s %s %s :%s", 1,
		    parc, parv) != HUNTED_ISME)
			return 0;
		sendto_one(cptr, ":%s RPONG %s %s %s %s :%s", me.name, parv[0],
		    parv[2], parv[3], parv[4], parv[5]);
	}
	return 0;
}

/*
 * m_rpong  -- by Run too :)
 *
 * parv[0] = sender prefix
 * parv[1] = from pinged server: start server; from start server: sender
 * parv[2] = from pinged server: sender; from start server: pinged server
 * parv[3] = pingtime in ms
 * parv[4] = client info (for instance start time)
 */
int  m_rpong(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;

	if (!IsServer(sptr))
		return 0;

	if (parc < 5)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "RPING");
		return 0;
	}

	/* rping blowbug */
	if (!(acptr = find_client(parv[1], (aClient *)NULL)))
		return 0;


	if (!IsMe(acptr))
	{
		if (IsServer(acptr) && parc > 5)
		{
			sendto_one(acptr, ":%s RPONG %s %s %s %s :%s",
			    parv[0], parv[1], parv[2], parv[3], parv[4],
			    parv[5]);
			return 0;
		}
	}
	else
	{
		parv[1] = parv[2];
		parv[2] = sptr->name;
		parv[0] = me.name;
		parv[3] = militime(parv[3], parv[4]);
		parv[4] = parv[5];
		if (!(acptr = find_person(parv[1], (aClient *)NULL)))
			return 0;	/* No bouncing between servers ! */
	}

	sendto_one(acptr, ":%s RPONG %s %s %s :%s",
	    parv[0], parv[1], parv[2], parv[3], parv[4]);
	return 0;
}
/*
 * m_swhois
 * parv[1] = nickname
 * parv[2] = new swhois
 *
*/

int  m_swhois(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *acptr;

	if (!IsServer(sptr) && !IsULine(sptr))
		return 0;
	if (parc < 3)
		return 0;

	acptr = find_person(parv[1], (aClient *)NULL);
	if (!acptr)
		return 0;

	if (acptr->user->swhois)
		MyFree(acptr->user->swhois);
	acptr->user->swhois = MyMalloc(strlen(parv[2]) + 1);
	ircsprintf(acptr->user->swhois, "%s", parv[2]);
	sendto_serv_butone_token(cptr, sptr->name,
	   MSG_SWHOIS, TOK_SWHOIS, "%s :%s", parv[1], parv[2]);
	return 0;
}
/*
** m_sendumode - Stskeeps
**      parv[0] = sender prefix
**      parv[1] = target
**      parv[2] = message text
** Pretty handy proc.. 
** Servers can use this to f.x:
**   :server.unreal.net SENDUMODE F :Client connecting at server server.unreal.net port 4141 usw..
** or for sending msgs to locops.. :P
*/
int  m_sendumode(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *message;
	char *p;

	message = parc > 2 ? parv[2] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "SENDUMODE");
		return 0;
	}

	if (!IsServer(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	sendto_serv_butone(IsServer(cptr) ? cptr : NULL,
	    ":%s SMO %s :%s", parv[0], parv[1], message);


	for (p = parv[1]; *p; p++)
	{
		switch (*p)
		{
		  case 'e':
			  sendto_umode(UMODE_EYES, "%s", parv[2]);
			  break;
		  case 'F':
		  {
			  if (*parv[2] != 'C' && *(parv[2] + 1) != 'l')
				  sendto_umode(UMODE_FCLIENT, "%s", parv[2]);
			  break;
		  }
		  case 'o':
			  sendto_umode(UMODE_OPER, "%s", parv[2]);
			  break;
		  case 'O':
			  sendto_umode(UMODE_LOCOP, "%s", parv[2]);
			  break;
		  case 'h':
			  sendto_umode(UMODE_HELPOP, "%s", parv[2]);
			  break;
		  case 'N':
			  sendto_umode(UMODE_NETADMIN | UMODE_TECHADMIN, "%s",
			      parv[2]);
			  break;
		  case 'A':
			  sendto_umode(UMODE_ADMIN, "%s", parv[2]);
			  break;
/*		  case '1':
			  sendto_umode(UMODE_CODER, "%s", parv[2]);
			  break;
*/
		  case 'I':
			  sendto_umode(UMODE_HIDING, "%s", parv[2]);
			  break;
		  case 'w':
			  sendto_umode(UMODE_WALLOP, "%s", parv[2]);
			  break;
		  case 's':
			  sendto_umode(UMODE_SERVNOTICE, "%s", parv[2]);
			  break;
		  case 'T':
			  sendto_umode(UMODE_TECHADMIN, "%s", parv[2]);
			  break;
		  case '*':
		  	  sendto_all_butone(NULL, &me, ":%s NOTICE :%s", 
			   	me.name, parv[2]);  	  	
					  	  	 	 
			  break;
		}
	}
	return 0;
}


/*
** m_tsctl - Stskeeps
**      parv[0] = sender prefix
**      parv[1] = command
**      parv[2] = options
*/

int  m_tsctl(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	time_t timediff;


	if (!MyClient(sptr))
		goto doit;
	if (!IsAdmin(sptr) && !IsCoAdmin(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
      doit:
	if (parv[1])
	{
		if (*parv[1] == '\0')
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			    me.name, parv[0], "TSCTL");
			return 0;
		}

		if (strcmp(parv[1], "offset") == 0)
		{
			if (!parv[3])
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;
			}
			if (*parv[2] == '\0' || *parv[3] == '\0')
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;
			}
			if (!(*parv[2] == '+' || *parv[2] == '-'))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;

			}

			switch (*parv[2])
			{
			  case '+':
				  timediff = atol(parv[3]);
				  TSoffset = timediff;
				  sendto_ops
				      ("TS Control - %s set TStime() to be diffed +%li",
				      sptr->name, timediff);
				  sendto_serv_butone(&me,
				      ":%s GLOBOPS :TS Control - %s set TStime to be diffed +%li",
				      me.name, sptr->name, timediff);
				  break;
			  case '-':
				  timediff = atol(parv[3]);
				  TSoffset = -timediff;
				  sendto_ops
				      ("TS Control - %s set TStime() to be diffed -%li",
				      sptr->name, timediff);
				  sendto_serv_butone(&me,
				      ":%s GLOBOPS :TS Control - %s set TStime to be diffed -%li",
				      me.name, sptr->name, timediff);
				  break;
			}
			return 0;
		}
		if (strcmp(parv[1], "time") == 0)
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** TStime=%li time()=%li TSoffset=%li",
			    me.name, sptr->name, TStime(), time(NULL),
			    TSoffset);
			return 0;
		}
		if (strcmp(parv[1], "alltime") == 0)
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** Server=%s TStime=%li time()=%li TSoffset=%li",
			    me.name, sptr->name, me.name, TStime(), time(NULL),
			    TSoffset);
			sendto_serv_butone(cptr, ":%s TSCTL alltime",
			    sptr->name);
			return 0;

		}
		if (strcmp(parv[1], "svstime") == 0)
		{
			if (parv[2] == '\0')
			{
				return 0;
			}
			if (!IsULine(sptr))
			{
				return 0;
			}

			timediff = atol(parv[2]);
			timediff = timediff - time(NULL);
			TSoffset = timediff;
			sendto_ops
			    ("TS Control - U:line set time to be %li (timediff: %li)",
			    atol(parv[2]), timediff);
			sendto_serv_butone(cptr, ":%s TSCTL svstime %li",
			    sptr->name, atol(parv[2]));
			return 0;
		}
	}
}



#ifdef GUEST
int m_guest (cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
int randnum;
char guestnick[NICKLEN];
char *param[2];

randnum = 1+(int) (99999.0*rand()/(RAND_MAX+10000.0));
snprintf(guestnick, NICKLEN, "Guest%li", randnum);

while(find_client(guestnick, (aClient *)NULL))
{ 
randnum = 1+(int) (99999.0*rand()/(RAND_MAX+10000.0));
snprintf(guestnick, NICKLEN, "Guest%li", randnum);
}
param[0] = sptr->name;
param[1] = guestnick;
m_nick(sptr,cptr,2,param);
}
#endif

int  m_htm(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	int  x;
	if (!IsOper(sptr))
		return 0;

#ifndef NO_FDLIST

	if (!parv[1] || !MyClient(sptr))
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Current incoming rate: %0.2f kb/s",
		    me.name, parv[0], currentrate);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Current outgoing rate: %0.2f kb/s",
		    me.name, parv[0], currentrate2);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Highest incoming rate: %0.2f kb/s",
		    me.name, parv[0], highest_rate);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Highest outgoing rate: %0.2f kb/s",
		    me.name, parv[0], highest_rate2);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** High traffic mode is currently \2%s\2",
		    me.name, parv[0], (lifesux ? "ON" : "OFF"));
		sendto_one(sptr,
		    ":%s NOTICE %s :*** High traffic mode is currently in \2%s\2 mode",
		    me.name, parv[0], (noisy_htm ? "NOISY" : "QUIET"));
		sendto_one(sptr,
		    ":%s NOTICE %s :*** HTM will be activated if incoming > %i kb/s",
		    me.name, parv[0], LRV);
	}

	else
	{
		char *command = parv[1];

		if (strchr(command, '.'))
		{
			if ((x =
			    hunt_server(cptr, sptr, ":%s HTM %s", 1, parc,
			    parv)) != HUNTED_ISME)
				return 0;
		}
		if (!stricmp(command, "ON"))
		{
			lifesux = 1;
			sendto_one(sptr,
			    ":%s NOTICE %s :High traffic mode is now ON.",
			    me.name, parv[0]);
			sendto_ops
			    ("%s (%s@%s) forced High traffic mode to activate",
			    parv[0], sptr->user->username,
			    sptr->user->realhost);
			LCF = 60;	/* 60 seconds */
		}
		else if (!stricmp(command, "OFF"))
		{
			lifesux = 0;
			LCF = LOADCFREQ;
			sendto_one(sptr,
			    ":%s NOTICE %s :High traffic mode is now OFF.",
			    me.name, parv[0]);
			sendto_ops
			    ("%s (%s@%s) forced High traffic mode to deactivate",
			    parv[0], sptr->user->username,
			    sptr->user->realhost);
		}
		else if (!stricmp(command, "TO"))
		{
			if (!parv[2])
				sendto_one(sptr,
				    ":%s NOTICE %s :You must specify an integer value",
				    me.name, parv[0]);
			else
			{
				int  new_val = atoi(parv[2]);
				if (new_val < 10)
					sendto_one(sptr,
					    ":%s NOTICE %s :New value must be > 10",
					    me.name, parv[0]);
				else
				{
					LRV = new_val;
					sendto_one(sptr,
					    ":%s NOTICE %s :New max rate is %dkb/s",
					    me.name, parv[0], LRV);
					sendto_ops
					    ("%s (%s@%s) changed the High traffic mode max rate to %dkb/s",
					    parv[0], sptr->user->username,
					    sptr->user->realhost, LRV);
				}
			}
		}
		else if (!stricmp(command, "QUIET"))
		{
			noisy_htm = 0;
			sendto_one(sptr,
			    ":%s NOTICE %s :High traffic mode is now QUIET",
			    me.name, parv[0]);
			sendto_ops("%s (%s@%s) set High traffic mode to QUIET",
			    parv[0], sptr->user->username,
			    sptr->user->realhost);
		}

		else if (!stricmp(command, "NOISY"))
		{
			noisy_htm = 1;
			sendto_one(sptr,
			    ":%s NOTICE %s :High traffic mode is now NOISY",
			    me.name, parv[0]);
			sendto_ops("%s (%s@%s) set High traffic mode to NOISY",
			    parv[0], sptr->user->username,
			    sptr->user->realhost);
		}
		else
			sendto_one(sptr, ":%s NOTICE %s :Unknown option: %s",
			    me.name, parv[0], command);
	}



#else
	sendto_one(sptr,
	    ":%s NOTICE %s :*** High traffic mode and fdlists are not enabled on this server",
	    me.name, sptr->name);
#endif
}


/* 
 * m_chgname - Tue May 23 13:06:35 BST 200 (almost a year after I made CHGIDENT) - Stskeeps
 * :prefix CHGNAME <nick> <new realname>
 * parv[0] - sender
 * parv[1] - nickname
 * parv[2] - realname
 *
*/

int  m_chgname(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *acptr;

#ifdef DISABLE_USERMOD
	if (MyClient(sptr))
	{
		sendto_one(sptr, ":%s NOTICE %s :*** The /chgname command is disabled on this server", me.name, sptr->name);
		return 0;
	}
#endif


	if (MyClient(sptr))
		if (!IsAnOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;

		}

	if (parc < 3)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /ChgName syntax is /ChgName <nick> <newident>",
		    me.name, sptr->name);
		return 0;
	}

	if (strlen(parv[2]) < 1)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Write atleast something to change the ident to!",
		    me.name, sptr->name);
		return 0;
	}

	if (strlen(parv[2]) > (REALLEN))
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** ChgName Error: Too long !!", me.name,
		    sptr->name);
		return 0;
	}

	if ((acptr = find_person(parv[1], NULL)))
	{
		/* set the realname first to make n:line checking work */
		ircsprintf(acptr->info, "%s", parv[2]);
		/* only check for n:lines if the person who's name is being changed is not an oper */
		if (!IsAnOper(acptr) && Find_ban(acptr->info, CONF_BAN_REALNAME)) {
			int xx;
			xx =
			   exit_client(cptr, sptr, &me,
			   "Your GECOS (real name) is banned from this server");
			return xx;
		}
		if (!IsULine(sptr))
		{
			sendto_umode(UMODE_EYES,
			    "%s changed the GECOS of %s (%s@%s) to be %s",
			    sptr->name, acptr->name, acptr->user->username,
			    (acptr->umodes & UMODE_HIDE ? acptr->
			    user->realhost : acptr->user->realhost), parv[2]);
		}
		sendto_serv_butone_token(cptr, sptr->name,
		    MSG_CHGNAME, TOK_CHGNAME, "%s :%s", acptr->name, parv[2]);
		return 0;
	}
	else
	{
		sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name,
		    parv[1]);
		return 0;
	}
	return 0;
}

