/*
 * Test for modulized channelmode system, adds 2 modes:
 * +w (paramless)
 * +y <0-100> (1 parameter)
 *
 * It's incomplete (like error signaling) but for testing purposes only.
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

#ifndef DYNAMIC_LINKING
ModuleHeader m_chmodetst_Header
#else
#define m_chmodetst_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"chmodetst",	/* Name of module */
	"channelmode test", /* Version */
	"channelmode +w", /* Short description of module */
	"3.2-b8-1",
	NULL,
    };

ExtCMode EXTCMODE_TEST = 0L; /* Just for testing */
ExtCMode EXTCMODE_TEST2 = 0L; /* Just for testing */

int modey_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what);
aExtCMtableParam * modey_put_param(aExtCMtableParam *lst, char *para);
char *modey_get_param(aExtCMtableParam *lst);
char *modey_conv_param(char *param);
aExtCMtableParam * modey_free_param(aExtCMtableParam *lst);
aExtCMtableParam *modey_dup_struct(aExtCMtableParam *src);
int modey_sjoin_check(aChannel *chptr, aExtCMtableParam *ourx, aExtCMtableParam *theirx);
aExtCMtableParam *modey_dup_struct(aExtCMtableParam *src);
int modey_sjoin_check(aChannel *chptr, aExtCMtableParam *ourx, aExtCMtableParam *theirx);

typedef struct {
	EXTCM_PAR_HEADER
	short val;
} aModewentry;

/* The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(ModuleInfo *modinfo)
#else
int    m_dummy_Init(ModuleInfo *modinfo)
#endif
{
aExtCMtable req;
	ircd_log(LOG_ERROR, "debug: mod_init called from chmodetst module");
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM);
	sendto_realops("chmodetst loading...");
	/* TODO: load mode here */
	/* +w doesn't do anything, it's just for testing */
	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.is_ok = extcmode_default_requirechop;
	req.flag = 'w';
	EXTCMODE_TEST = extcmode_get(&req);
	/* +y doesn't do anything except that you can set/unset it with a
	 * numeric parameter (1-100)
	 */
	memset(&req, 0, sizeof(req));
	req.paracount = 1;
	req.is_ok = modey_is_ok;
	req.put_param = modey_put_param;
	req.get_param = modey_get_param;
	req.conv_param	= modey_conv_param;
	req.free_param = modey_free_param;
	req.sjoin_check = modey_sjoin_check;
	req.dup_struct = modey_dup_struct;
	req.flag = 'y';
	EXTCMODE_TEST2 = extcmode_get(&req);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_dummy_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_dummy_Unload(int module_unload)
#endif
{
	/* Aaaaaaaargh... we are assumed to be a permanent module */
	sendto_realops("Mod_Unload was called??? Arghhhhhh..");
	return MOD_FAILED;
}

int modey_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what)
{
short t;

	if ((checkt == EXCHK_ACCESS) || (checkt == EXCHK_ACCESS_ERR))
	{
		if (!is_chan_op(sptr, chptr))
		{
			if (checkt == EXCHK_ACCESS_ERR)
				sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED), me.name, sptr->name, chptr->chname);
			return 0;
		} else {
			return 1;
		}
	} else
	if (checkt == EXCHK_PARAM)
	{
		t = (short)atoi(para);
		if ((t < 0) || (t > 100))
		{
			sendto_one(sptr, ":%s NOTICE %s :chanmode +y requires a parameter in the range 0..100",
				me.name, sptr->name);
			return 0;
		}
		return 1;
	}
	return 0;
}

aExtCMtableParam * modey_put_param(aExtCMtableParam *lst, char *para)
{
aModewentry *r;

	r = (aModewentry *)extcmode_get_struct(lst, 'y');
	Debug((DEBUG_DEBUG, "modey_put_param: get_struct returned 0x%x", r));
	if (!r)
	{
		/* Need to create one */
		r = (aModewentry *)malloc(sizeof(aModewentry));
		memset(r, 0, sizeof(aModewentry));
		r->flag = 'y';
		AddListItem(r, lst);
	}
	r->val = (short)atoi(para);
	return lst;
}

char *modey_get_param(aExtCMtableParam *lst)
{
aModewentry *r;
static char tmpret[16];

	r = (aModewentry *)extcmode_get_struct(lst, 'y');
	if (!r)
		return NULL;
	sprintf(tmpret, "%hu", r->val);
	return tmpret;
}

/* no this is not useless, imagine the parameter "1blah" :P */
char *modey_conv_param(char *param)
{
short i;
static char tmpret2[16];
	i = (short)atoi(param);
	sprintf(tmpret2, "%hu", i);
	return tmpret2;
}

aExtCMtableParam * modey_free_param(aExtCMtableParam *lst)
{
aModewentry *r;
	r = (aModewentry *)extcmode_get_struct(lst, 'y');
	Debug((DEBUG_DEBUG, "modey_free_param: lst=0x%x, r=0x%x", lst, r));
	if (r)
	{
		DelListItem(r, lst);
		free(r);
	}
	Debug((DEBUG_DEBUG, "modey_free_param: returning 0x%x", lst));
	return lst;
}

aExtCMtableParam *modey_dup_struct(aExtCMtableParam *src)
{
aModewentry *n = (aModewentry *)malloc(sizeof(aModewentry));
	memcpy(n, src, sizeof(aModewentry));
	return (aExtCMtableParam *)n;
}

int modey_sjoin_check(aChannel *chptr, aExtCMtableParam *ourx, aExtCMtableParam *theirx)
{
aModewentry *our = (aModewentry *)ourx;
aModewentry *their = (aModewentry *)theirx;

	if (our->val == their->val)
		return EXSJ_SAME;
	if (our->val > their->val)
		return EXSJ_WEWON;
	else
		return EXSJ_THEYWON;
}
