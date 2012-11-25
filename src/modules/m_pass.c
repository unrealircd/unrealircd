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

extern MODVAR char zlinebuf[BUFSIZE];

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

/** Handles zlines/gzlines/throttling/unknown connections */
static int my_check_banned(aClient *cptr)
{
int i, j;
aTKline *tk;
aClient *acptr, *acptr2;
ConfigItem_ban *bconf;

	j = 1;

	list_for_each_entry(acptr, &unknown_list, lclient_node)
	{
		if (IsUnknown(acptr) &&
#ifndef INET6
			acptr->ip.S_ADDR == cptr->ip.S_ADDR)
#else
			!bcmp(acptr->ip.S_ADDR, cptr->ip.S_ADDR, sizeof(cptr->ip.S_ADDR)))
#endif
		{
			j++;
			if (j > MAXUNKNOWNCONNECTIONSPERIP)
				return exit_client(cptr, cptr, &me, "Too many unknown connections from your IP");
		}
	}

	if ((bconf = Find_ban(cptr, Inet_ia2p(&cptr->ip), CONF_BAN_IP)))
	{
		ircsprintf(zlinebuf,
			"You are not welcome on this server: %s. Email %s for more information.",
			bconf->reason ? bconf->reason : "no reason", KLINE_ADDRESS);
		return exit_client(cptr, cptr, &me, zlinebuf);
	}
	else if (find_tkline_match_zap_ex(cptr, &tk) != -1)
	{
		ircsprintf(zlinebuf, "Z:Lined (%s)", tk->reason);
		return exit_client(cptr, cptr, &me, zlinebuf);
	}
	else
	{
		int val;
		if (!(val = throttle_can_connect(cptr, &cptr->ip)))
		{
			ircsprintf(zlinebuf, "Throttled: Reconnecting too fast - Email %s for more information.",
					KLINE_ADDRESS);
			return exit_client(cptr, cptr, &me, zlinebuf);
		}
		else if (val == 1)
			add_throttling_bucket(&cptr->ip);
	}

	return 0;
}

#define CGIIRC_STRING     "CGIIRC_"
#define CGIIRC_STRINGLEN  (sizeof(CGIIRC_STRING)-1)

/* Does the CGI:IRC host spoofing work */
int docgiirc(aClient *cptr, char *ip, char *host)
{
#ifdef INET6
	char ipbuf[64];
#endif
	char *sockhost;

	if (IsCGIIRC(cptr))
		return exit_client(cptr, cptr, &me, "Double CGI:IRC request (already identified)");

	if (host && !strcmp(ip, host))
		host = NULL; /* host did not resolve, make it NULL */

	/* STEP 1: Update cptr->ip
	   inet_pton() returns 1 on success, 0 on bad input, -1 on bad AF */
	if(inet_pton(AFINET, ip, &cptr->ip) != 1)
	{
#ifndef INET6
		/* then we have an invalid IP */
		return exit_client(cptr, cptr, &me, "Invalid IP address");
#else
		/* The address may be IPv4. We have to try ::ffff:ipv4 */
		snprintf(ipbuf, sizeof(ipbuf), "::ffff:%s", ip);
		if(inet_pton(AFINET, ipbuf, &cptr->ip) != 1)
			return exit_client(cptr, cptr, &me, "Invalid IP address");
#endif
	}

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
	if (host && verify_hostname(host))
		cptr->hostp = unreal_create_hostent(host, &cptr->ip);

	/* STEP 4: Update sockhost
	   Make sure that if this any IPv4 address is _not_ prefixed with
	   "::ffff:" by using Inet_ia2p().
	 */
	sockhost = Inet_ia2p(&cptr->ip);
	if(!sockhost)
	{
		return exit_client(cptr, cptr, &me, "Error processing CGI:IRC IP address.");
	}
	strlcpy(cptr->sockhost, sockhost, sizeof(cptr->sockhost));

	SetCGIIRC(cptr);

	/* Check (g)zlines right now; these are normally checked upon accept(),
	 * but since we know the IP only now after PASS/WEBIRC, we have to check
	 * here again...
	 */
	return my_check_banned(cptr);
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
	host = !DONT_RESOLVE ? parv[3] : parv[4];
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
	strlcpy(cptr->passwd, password, PassLen + 1);

	/* note: the original non-truncated password is supplied as 2nd parameter. */
	RunHookReturnInt2(HOOKTYPE_LOCAL_PASS, sptr, password, !=0);
	return 0;
}
