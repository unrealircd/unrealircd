/*
 * Module skeleton, by Carsten V. Munk 2001 <stskeeps@tspre.org>
 * May be used, modified, or changed by anyone, no license applies.
 * You may relicense this, to any license
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_chgname(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_CHGNAME     "CHGNAME"
#define TOK_CHGNAME     "BK"

#ifndef DYNAMIC_LINKING
ModuleInfo m_chgname_info
#else
#define m_chgname_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"chgname",	/* Name of module */
	"$Id$", /* Version */
	"command /chgname", /* Short description of module */
	NULL, /* Pointer to our dlopen() return value */
	NULL 
    };


/* The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_init(int module_load)
#else
int    m_chgname_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_CHGNAME, TOK_CHGNAME, m_chgname, MAXPARA);
        add_Command(MSG_SVSNAME, TOK_CHGNAME, m_chgname, MAXPARA);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_chgname_load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_chgname_unload(void)
#endif
{
	if (del_Command(MSG_CHGNAME, TOK_CHGNAME, m_chgname) < 0)
	{
		sendto_realops("Failed to delete command chgname when unloading %s",
				m_chgname_info.name);
	}
	if (del_Command(MSG_SVSNAME, TOK_CHGNAME, m_chgname) < 0)
	{
		sendto_realops("Failed to delete command svsname when unloading %s",
				m_chgname_info.name);
	}
}


/* 
 * m_chgname - Tue May 23 13:06:35 BST 200 (almost a year after I made CHGIDENT) - Stskeeps
 * :prefix CHGNAME <nick> <new realname>
 * parv[0] - sender
 * parv[1] - nickname
 * parv[2] - realname
 *
*/

DLLFUNC int m_chgname(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
			sendto_snomask(SNO_EYES,
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

