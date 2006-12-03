/*
 * Channel Mode +G
 * (C) Copyright 2005-2006 Bram Matthys and The UnrealIRCd team.
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
#endif
#include <fcntl.h>
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif


ModuleHeader MOD_HEADER(chmode_upG)
  = {
	"chmode_upG",
	"$Id$",
	"Channel Mode +G",
	"3.2-b8-1",
	NULL,
    };


ModuleInfo *ModInfo = NULL;

Cmode_t EXTMODE_STRIPBADWORDS = 0L;

#define IsModeG(x) ((x)->mode.extmode & EXTMODE_STRIPBADWORDS)

int cmodeG_is_ok(aClient *cptr, aChannel *chptr, char *para, int checkt, int what);
char *chmodeG_pre_chanmsg(aClient *sptr, aChannel *chptr, char *text, int notice);
char *chmodeG_pre_local_part(aClient *sptr, aChannel *chptr, char *text);
char *chmodeG_pre_local_quit(aClient *sptr, char *text);

DLLFUNC int MOD_INIT(chmode_upG)(ModuleInfo *modinfo)
{
	CmodeInfo req;
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	memset(&req, 0, sizeof(req));
	ModInfo = modinfo;

	req.paracount = 0;
	req.is_ok = cmodeG_is_ok;
	req.flag = 'G';
	CmodeAdd(modinfo->handle, req, &EXTMODE_STRIPBADWORDS);

	HookAddPCharEx(modinfo->handle, HOOKTYPE_PRE_CHANMSG, chmodeG_pre_chanmsg);
	HookAddPCharEx(modinfo->handle, HOOKTYPE_PRE_LOCAL_PART, chmodeG_pre_local_part);
	HookAddPCharEx(modinfo->handle, HOOKTYPE_PRE_LOCAL_QUIT, chmodeG_pre_local_quit);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(chmode_upG)(int module_load)
{
	return MOD_SUCCESS;
}


DLLFUNC int MOD_UNLOAD(chmode_upG)(int module_unload)
{
	sendto_realops("Mod_Unload was called??? Arghhhhhh..");
	return MOD_FAILED;
}

int cmodeG_is_ok(aClient *cptr, aChannel *chptr, char *para, int checkt, int what)
{
	if (IsPerson(cptr) && is_chan_op(cptr, chptr))
		return EX_ALLOW;
	if (checkt == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
		sendto_one(cptr, err_str(ERR_NOTFORHALFOPS), me.name, cptr->name, 'G');
	return EX_DENY;
}

char *stripbadwords_channel(char *str, int *blocked)
{
	return stripbadwords(str, conf_badword_channel, blocked);
}

char *stripbadwords_quit(char *str, int *blocked)
{
	return stripbadwords(str, conf_badword_quit, blocked);
}


char *chmodeG_pre_chanmsg(aClient *sptr, aChannel *chptr, char *text, int notice)
{
int blocked;

	if (!(chptr->mode.extmode & EXTMODE_STRIPBADWORDS))
		return text;
	
	text = stripbadwords_channel(text, &blocked);
	if (blocked)
	{
		if (!notice)
			sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
				me.name, sptr->name, chptr->chname,
				"Swearing is not permitted in this channel", chptr->chname);
		return NULL;
	}

	return text;
}

char *chmodeG_pre_local_part(aClient *sptr, aChannel *chptr, char *text)
{
int blocked;

	if (!(chptr->mode.extmode & EXTMODE_STRIPBADWORDS))
		return text;
	
	text = stripbadwords_channel(text, &blocked);
	return blocked ? NULL : text;
}

char *chmodeG_pre_local_quit(aClient *sptr, char *text)
{
int blocked;

	text = stripbadwords_quit(text, &blocked);
	return blocked ? NULL : text;
}
