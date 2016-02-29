/*
 * Channel Mode +L.
 * (C) Copyright 1999-.. The UnrealIRCd team.
 */

#include "unrealircd.h"


ModuleHeader MOD_HEADER(link)
  = {
	"chanmodes/link",
	"4.0",
	"Channel Mode +L",
	"3.2-b8-1",
	NULL,
    };

Cmode_t EXTMODE_LINK = 0L;

typedef struct {
	char linked[CHANNELLEN+1];
} aModeLEntry;

int cmodeL_is_ok(aClient *sptr, aChannel *chptr, char mode, char *para, int type, int what);
void *cmodeL_put_param(void *r_in, char *param);
char *cmodeL_get_param(void *r_in);
char *cmodeL_conv_param(char *param_in, aClient *sptr);
void cmodeL_free_param(void *r);
void *cmodeL_dup_struct(void *r_in);
int cmodeL_sjoin_check(aChannel *chptr, void *ourx, void *theirx);

int link_can_join_limitexceeded(aClient *sptr, aChannel *chptr, char *key, char *parv[]);

MOD_INIT(link)
{
	CmodeInfo req;
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM_RELOADABLE, 1);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	memset(&req, 0, sizeof(req));
	req.paracount = 1;
	req.is_ok = cmodeL_is_ok;
	req.flag = 'L';
	req.unset_with_param = 1; /* Oh yeah, we are special! */
	req.put_param = cmodeL_put_param;
	req.get_param = cmodeL_get_param;
	req.conv_param = cmodeL_conv_param;
	req.free_param = cmodeL_free_param;
	req.dup_struct = cmodeL_dup_struct;
	req.sjoin_check = cmodeL_sjoin_check;
	CmodeAdd(modinfo->handle, req, &EXTMODE_LINK);

	HookAdd(modinfo->handle, HOOKTYPE_CAN_JOIN_LIMITEXCEEDED, 0, link_can_join_limitexceeded);
	return MOD_SUCCESS;
}

MOD_LOAD(link)
{
	return MOD_SUCCESS;
}


MOD_UNLOAD(link)
{
	return MOD_SUCCESS;
}

int cmodeL_is_ok(aClient *sptr, aChannel *chptr, char mode, char *para, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsPerson(sptr) && is_chanowner(sptr, chptr))
			return EX_ALLOW;
		if (type == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
			sendto_one(sptr, err_str(ERR_CHANOWNPRIVNEEDED), me.name, sptr->name, chptr->chname);
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		/* Check parameter.. syntax is +L #channel */
		char buf[CHANNELLEN+1], *p;
		
		if (strchr(para, ','))
			return EX_DENY; /* multiple channels not permitted */
		if (!IsChannelName(para))
		{
			if (MyClient(sptr))
				sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL), me.name, sptr->name, para);
			return EX_DENY;
		}
		
		/* This needs to be exactly the same as in conv_param... */
		strlcpy(buf, para, sizeof(buf));
		clean_channelname(buf);
		if ((p = strchr(buf, ':')))
			*p = '\0';
		if (find_channel(buf, NULL) == chptr)
		{
			if (MyClient(sptr))
				sendto_one(sptr, err_str(ERR_CANNOTCHANGECHANMODE),
				           me.name, sptr->name, 'L',
				           "a channel cannot be linked to itself");
			return EX_DENY;
		}
		return EX_ALLOW;
	}

	/* falltrough -- should not be used */
	return EX_DENY;
}

void *cmodeL_put_param(void *r_in, char *param)
{
aModeLEntry *r = (aModeLEntry *)r_in;

	if (!r)
	{
		/* Need to create one */
		r = (aModeLEntry *)MyMallocEx(sizeof(aModeLEntry));
	}
	strlcpy(r->linked, param, sizeof(r->linked));
	return (void *)r;
}

char *cmodeL_get_param(void *r_in)
{
aModeLEntry *r = (aModeLEntry *)r_in;
static char retbuf[CHANNELLEN+1];

	if (!r)
		return NULL;

	strlcpy(retbuf, r->linked, sizeof(retbuf));
	return retbuf;
}

/** Convert parameter to something proper.
 * NOTE: cptr may be NULL
 */
char *cmodeL_conv_param(char *param_in, aClient *sptr)
{
static char buf[CHANNELLEN+1];
char *p;
		
	strlcpy(buf, param_in, sizeof(buf));
	clean_channelname(buf);
	if ((p = strchr(buf, ':')))
		*p = '\0';

	if (*buf == '\0')
		strcpy(buf, "#<INVALID>"); /* better safe than sorry */

	return buf;
}

void cmodeL_free_param(void *r)
{
	MyFree(r);
}

void *cmodeL_dup_struct(void *r_in)
{
aModeLEntry *r = (aModeLEntry *)r_in;
aModeLEntry *w = (aModeLEntry *)MyMalloc(sizeof(aModeLEntry));

	memcpy(w, r, sizeof(aModeLEntry));
	return (void *)w;
}

int cmodeL_sjoin_check(aChannel *chptr, void *ourx, void *theirx)
{
aModeLEntry *our = (aModeLEntry *)ourx;
aModeLEntry *their = (aModeLEntry *)theirx;

	if (!strcmp(our->linked, their->linked))
		return EXSJ_SAME;
	if (strcmp(our->linked, their->linked) > 0)
		return EXSJ_WEWON;
	return EXSJ_THEYWON;
}

int link_can_join_limitexceeded(aClient *sptr, aChannel *chptr, char *key, char *parv[])
{
	if (chptr->mode.extmode & EXTMODE_LINK)
	{
		/* We are linked. */
		char *linked = cm_getparameter(chptr, 'L');
		if (!linked)
		        return 0; /* uh-huh? +L but not found. */
		sendto_one(sptr,
		    err_str(ERR_LINKCHANNEL), me.name, sptr->name, chptr->chname, linked);
		parv[0] = sptr->name;
		parv[1] = linked;
		do_join(sptr, sptr, 2, parv);
		return -1; /* Original channel join = ignored */
	}
	return 0;
}
