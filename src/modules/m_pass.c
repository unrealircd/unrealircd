/*
 *   IRC - Internet Relay Chat, src/modules/m_pass.c
 *   (C) 2004 The UnrealIRCd Team
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
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
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
#endif
#include "inet.h"
#include <fcntl.h>
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_pass(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int m_webirc(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_PASS 	"PASS"	
#define TOK_PASS 	"<"	

#define MSG_WEBIRC	"WEBIRC"

ModuleHeader MOD_HEADER(m_pass)
  = {
	"m_pass",
	"$Id$",
	"command /pass", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_pass)(ModuleInfo *modinfo)
{
	add_CommandX(MSG_PASS, TOK_PASS, m_pass, 1, M_UNREGISTERED|M_USER|M_SERVER);
	CommandAdd(modinfo->handle, MSG_WEBIRC, NULL, m_webirc, MAXPARA, M_UNREGISTERED);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_pass)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_pass)(int module_unload)
{
	if (del_Command(MSG_PASS, TOK_PASS, m_pass) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_pass).name);
	}
	return MOD_SUCCESS;
}

#define CGIIRC_STRING     "CGIIRC_"
#define CGIIRC_STRINGLEN  (sizeof(CGIIRC_STRING)-1)

/* Does the CGI:IRC host spoofing work */
int docgiirc(aClient *cptr, char *ip, char *host)
{
	if (host && !strcmp(ip, host))
		host = NULL; /* host did not resolve, make it NULL */

	/* STEP 1: Update cptr->ip */
	if (inet_pton(AFINET, ip, &cptr->ip) <= 0)
		return exit_client(cptr, cptr, &me, "Invalid IP address");

	/* STEP 2: Update GetIP() */
	if (cptr->user)
	{
		/* Kinda unsure if this is actually used.. But maybe if USER, PASS, NICK ? */
		if (cptr->user->ip_str)
			MyFree(cptr->user->ip_str);
		cptr->user->ip_str = strdup(ip);
	}
		
	/* STEP 3: Update cptr->hostp */
	/* (free old) */
	if (cptr->hostp)
	{
		unreal_free_hostent(cptr->hostp);
		cptr->hostp = NULL;
	}
	/* (create new) */
	if (host)
		cptr->hostp = unreal_create_hostent(host, &cptr->ip);

	/* STEP 4: Update sockhost */		
	strlcpy(cptr->sockhost, ip, sizeof(cptr->sockhost));

	/* Do not save password, return / proceed as normal instead */
	SetCGIIRC(cptr);
	return 0;
}

/* WEBIRC <pass> "cgiirc" <hostname> <ip> */
DLLFUNC int m_webirc(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
char *ip, *host, *password;
size_t ourlen;
ConfigItem_cgiirc *e;

	if ((parc < 5) || BadPtr(parv[4]))
	{
		sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS), me.name, "*", "WEBIRC");
		return -1;
	}

	password = parv[1];
	host = parv[3];
	ip = parv[4];

	/* Check if allowed host */
	e = Find_cgiirc(sptr->username, sptr->sockhost, GetIP(sptr), CGIIRC_WEBIRC);
	if (!e)
		return exit_client(cptr, sptr, &me, "CGI:IRC -- No access");

	/* Check password */
	if (Auth_Check(sptr, e->auth, password) == -1)
		return exit_client(cptr, sptr, &me, "CGI:IRC -- Invalid password");

	/* And do our job.. */
	return docgiirc(cptr, ip, host);
}

/***************************************************************************
 * m_pass() - Added Sat, 4 March 1989
 ***************************************************************************/
/*
** m_pass
**	parv[0] = sender prefix
**	parv[1] = password
*/
DLLFUNC CMD_FUNC(m_pass)
{
	char *password = parc > 1 ? parv[1] : NULL;
	int  PassLen = 0;
	if (BadPtr(password))
	{
		sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "PASS");
		return 0;
	}
	if (!MyConnect(sptr) || (!IsUnknown(cptr) && !IsHandshake(cptr)))
	{
		sendto_one(cptr, err_str(ERR_ALREADYREGISTRED),
		    me.name, parv[0]);
		return 0;
	}

	if (!strncmp(password, CGIIRC_STRING, CGIIRC_STRINGLEN))
	{
		char *ip, *host;
		ConfigItem_cgiirc *e;

		e = Find_cgiirc(sptr->username, sptr->sockhost, GetIP(sptr), CGIIRC_PASS);
		if (e)
		{
			/* Ok now we got that sorted out, proceed:
			 * Syntax: CGIIRC_<ip>_<resolvedhostname>
			 * The <resolvedhostname> has been checked ip->host AND host->ip by CGI:IRC itself
			 * already so we trust it.
			 */
			ip = password + CGIIRC_STRINGLEN;
			host = strchr(ip, '_');
			if (!host)
				return exit_client(cptr, sptr, &me, "Invalid CGI:IRC IP received");
			*host++ = '\0';
		
			return docgiirc(cptr, ip, host);
		}
	}

	
	PassLen = strlen(password);
	if (cptr->passwd)
		MyFree(cptr->passwd);
	if (PassLen > (PASSWDLEN))
		PassLen = PASSWDLEN;
	cptr->passwd = MyMalloc(PassLen + 1);
	strncpyzt(cptr->passwd, password, PassLen + 1);

	/* note: the original non-truncated password is supplied as 2nd parameter. */
	RunHookReturnInt2(HOOKTYPE_LOCAL_PASS, sptr, password, !=0);
	return 0;
}
