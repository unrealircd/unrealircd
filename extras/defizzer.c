/*
 * Defizzer, 3rd party module for Unreal3.2-beta15 and up
 * (C) Carsten V. Munk 2003 <stskeeps@tspre.org>
 * You can do everything you desire with this module, under the condition that if you 
 * meet the author, you must buy him a drink of his choice.
 * Copyright notice must ALWAYS stay in place.
 *
 * Removes unidented fizzer clients from the network pre-local-connect
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
DLLFUNC int h_defizzer_connect(aClient *sptr);

static Hook *LocConnect = NULL;
ModuleInfo DefizzerModInfo;

#ifndef DYNAMIC_LINKING
ModuleHeader defizzer_Header
#else
#define defizzer_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"defizzer",	/* Name of module */
	"$Id$", /* Version */
	"de-Fizzer", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };


/* The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(ModuleInfo *modinfo)
#else
int    defizzer_Init(ModuleInfo *modinfo)
#endif
{
	bcopy(modinfo,&DefizzerModInfo,modinfo->size);
	LocConnect = HookAddEx(DefizzerModInfo.handle, HOOKTYPE_PRE_LOCAL_CONNECT, h_defizzer_connect);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    defizzer_Load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	defizzer_Unload(int module_unload)
#endif
{
	HookDel(LocConnect);
	return MOD_SUCCESS;
}
static void ban_fizzer(aClient *cptr)
{
	int i;
	aClient *acptr;
	char hostip[128], mo[100], mo2[100];
	char *tkllayer[9] = {
		me.name,	/*0  server.name */
		"+",		/*1  +|- */
		"z",		/*2  G   */
		"*",		/*3  user */
		NULL,		/*4  host */
		NULL,
		NULL,		/*6  expire_at */
		NULL,		/*7  set_at */
		NULL		/*8  reason */
	};

	strlcpy(hostip, Inet_ia2p(&cptr->ip), sizeof(hostip));

	tkllayer[4] = hostip;
	tkllayer[5] = me.name;
	ircsprintf(mo, "%li", 86400 + TStime());
	ircsprintf(mo2, "%li", TStime());
	tkllayer[6] = mo;
	tkllayer[7] = mo2;
	tkllayer[8] = "Fizzer";
	m_tkl(&me, &me, 9, tkllayer);
	return;
}

DLLFUNC int h_defizzer_connect(aClient *sptr)
{
	char user[USERLEN + 1];
	char *infobackup;
	char *s1, *s2;

	/*
	 * Algorithm is basically like this, inspired by Zaphod:
	 * Exchange first word with second in realname, prepend with 
	 * ~, then add in second word and first word upto limit of username.
	 * sounds fun?
	*/
	infobackup = strdup(sptr->info);
	if (!(s1 = strtok(infobackup, " ")))
	{
		free(infobackup);
		return 0;
	}		
	if (!(s2 = strtok(NULL, " ")))
	{
		free(infobackup);
		return 0;
	}		
	snprintf(user, sizeof(user), "%s%s%s", (IDENT_CHECK ? "~" : ""), s2, s1);
	free(infobackup);	
	if (!strcmp(user, sptr->user->username))
	{
		ircstp->is_ref++;
		ban_fizzer(sptr);			
		return exit_client(sptr, sptr, &me, "Fizzer client");
	}	
	return 0;
}