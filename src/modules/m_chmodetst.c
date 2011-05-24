/*
 * Test for modulized channelmode system, adds 2 modes:
 * +w (paramless)
 * +y <0-100> (1 parameter)
 *
 * It's incomplete (like error signaling) but for testing purposes only.
 *
 * Again, this is not an "official module", rather a test module.
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


ModuleHeader MOD_HEADER(m_chmodetst)
  = {
	"chmodetst",	/* Name of module */
	"channelmode test", /* Version */
	"channelmode +w", /* Short description of module */
	"3.2-b8-1",
	NULL,
    };

Cmode_t EXTCMODE_TEST = 0L; /* Just for testing */
Cmode_t EXTCMODE_TEST2 = 0L; /* Just for testing */

int modey_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what);
CmodeParam * modey_put_param(CmodeParam *lst, char *para);
char *modey_get_param(CmodeParam *lst);
char *modey_conv_param(char *param);
void modey_free_param(CmodeParam *lst);
CmodeParam *modey_dup_struct(CmodeParam *src);
int modey_sjoin_check(aChannel *chptr, CmodeParam *ourx, CmodeParam *theirx);
CmodeParam *modey_dup_struct(CmodeParam *src);

typedef struct {
	EXTCM_PAR_HEADER
	short val;
} aModewentry;

/* The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

Cmode *ModeTest = NULL, *ModeTest2 = NULL;
/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_dummy)(ModuleInfo *modinfo)
{
	CmodeInfo req;
	ircd_log(LOG_ERROR, "debug: mod_init called from chmodetst module");
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM);
	sendto_realops("chmodetst loading...");
	/* TODO: load mode here */
	/* +w doesn't do anything, it's just for testing */
	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.is_ok = extcmode_default_requirechop;
	req.flag = 'w';
	ModeTest = CmodeAdd(modinfo->handle, req, &EXTCMODE_TEST);
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
	ModeTest2 = CmodeAdd(modinfo->handle, req, &EXTCMODE_TEST2);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_dummy)(int module_load)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_dummy)(int module_unload)
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

CmodeParam * modey_put_param(CmodeParam *ypara, char *para)
{
	aModewentry *r = (aModewentry *)ypara;

	if (!r)
	{
		/* Need to create one */
		r = (aModewentry *)malloc(sizeof(aModewentry));
		memset(r, 0, sizeof(aModewentry));
		r->flag = 'y';
	}
	r->val = (short)atoi(para);
	return (CmodeParam *)r;
}

char *modey_get_param(CmodeParam *ypara)
{
	aModewentry *r = (aModewentry *)ypara;
	static char tmpret[16];

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

void modey_free_param(CmodeParam *ypara)
{
	aModewentry *r = (aModewentry *)ypara;
	free(r);
}

CmodeParam *modey_dup_struct(CmodeParam *src)
{
	aModewentry *n = (aModewentry *)malloc(sizeof(aModewentry));
	memcpy(n, src, sizeof(aModewentry));
	return (CmodeParam *)n;
}

int modey_sjoin_check(aChannel *chptr, CmodeParam *ourx, CmodeParam *theirx)
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
