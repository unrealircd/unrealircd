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
#include "version.h"
#include <time.h>
#ifdef _WIN32
#include <sys/timeb.h>
#endif
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
extern time_t LCF;
extern int LRV;
#endif

/*
** m_admins (Admin chat only) -Potvin
**      parv[0] = sender prefix
**      parv[1] = message text
*/
int m_admins(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
int m_techat(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
int m_nachat(aClient *cptr, aClient *sptr, int parc, char *parv[])
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

int m_lag(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
/* Now just as accurate on win as on linux -- codemastr */
#ifndef _WIN32
	struct timeval tv;
#else
	struct _timeb tv;
#endif
	static char timebuf[18];
#ifndef _WIN32
	gettimeofday(&tv, NULL);
#else
	_ftime(&tv);
#endif
	if (sec && usec)
		ircsprintf(timebuf, "%ld",
#ifndef _WIN32
		    (tv.tv_sec - atoi(sec)) * 1000 + (tv.tv_usec - atoi(usec)) / 1000);
#else
		    (tv.time - atoi(sec)) * 1000 + (tv.millitm - atoi(usec)) / 1000);
#endif
	else
#ifndef _WIN32
		ircsprintf(timebuf, "%ld %ld", tv.tv_sec, tv.tv_usec);
#else
		ircsprintf(timebuf, "%ld %ld", tv.time, tv.millitm);
#endif
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
** m_sendumode - Stskeeps
**      parv[0] = sender prefix
**      parv[1] = target
**      parv[2] = message text
** Pretty handy proc.. 
** Servers can use this to f.x:
**   :server.unreal.net SENDUMODE F :Client connecting at server server.unreal.net port 4141 usw..
** or for sending msgs to locops.. :P
*/
int m_sendumode(aClient *cptr, aClient *sptr, int parc, char *parv[])
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

int m_tsctl(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
int m_guest(aClient *cptr, aClient *sptr, int parc, char *parv[])
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

int m_htm(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	int  x = HUNTED_NOSUCH;
	char *command, *param;
	if (!IsOper(sptr))
		return 0;

#ifndef NO_FDLIST

	switch(parc) {
		case 1:
			break;
		case 2:
			x = hunt_server(cptr, sptr, ":%s HTM %s", 1, parc, parv);
			break;
		case 3:
			x = hunt_server(cptr, sptr, ":%s HTM %s %s", 1, parc, parv);
			break;
		default:
			x = hunt_server(cptr, sptr, ":%s HTM %s %s %s", 1, parc, parv);
	}

	switch (x) {
		case HUNTED_NOSUCH:
			command = (parv[1]);
			param = (parv[2]);
			break;
		case HUNTED_ISME:
			command = (parv[2]);
			param = (parv[3]);
			break;
		default:
			return 0;
	}


	if (!command)
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
#if 0
		char *command = parv[1];

		if (strchr(command, '.'))
		{
			if ((x =
			    hunt_server(cptr, sptr, ":%s HTM %s", 1, parc,
			    parv)) != HUNTED_ISME)
				return 0;
		}
#endif
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
			EventModEvery("lcf", LCF);
		}
		else if (!stricmp(command, "OFF"))
		{
			lifesux = 0;
			LCF = LOADCFREQ;
			EventModEvery("lcf", LCF);
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
			if (!param)
				sendto_one(sptr,
				    ":%s NOTICE %s :You must specify an integer value",
				    me.name, parv[0]);
			else
			{
				int  new_val = atoi(param);
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

int m_chgname(aClient *cptr, aClient *sptr, int parc, char *parv[])
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

