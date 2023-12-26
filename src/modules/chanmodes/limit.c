/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/limit.c
 * Channel Mode +l
 * (C) Copyright 2021 Syzop and the UnrealIRCd team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"chanmodes/limit",
	"6.0",
	"Channel Mode +l",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

typedef struct ChannelLimit ChannelLimit;
struct ChannelLimit {
	int limit;
};

/* Global variables */
ModDataInfo *mdlimit = NULL;
Cmode_t EXTMODE_LIMIT = 0L;

#define IsLimit(x)	((x)->mode.mode & EXTMODE_LIMIT)

/* Just for buffers, nothing else */
#define LIMITLEN	32

/* Forward declarations */
int limit_can_join(Client *client, Channel *channel, const char *key, char **errmsg);
int cmode_limit_is_ok(Client *client, Channel *channel, char mode, const char *para, int type, int what);
void *cmode_limit_put_param(void *r_in, const char *param);
const char *cmode_limit_get_param(void *r_in);
const char *cmode_limit_conv_param(const char *param_in, Client *client, Channel *channel);
int cmode_limit_free_param(void *r, int soft);
void *cmode_limit_dup_struct(void *r_in);
int cmode_limit_sjoin_check(Channel *channel, void *ourx, void *theirx);
int transform_channel_limit(const char *param);

MOD_INIT()
{
	CmodeInfo creq;
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&creq, 0, sizeof(creq));
	creq.paracount = 1;
	creq.is_ok = cmode_limit_is_ok;
	creq.letter = 'l';
	creq.put_param = cmode_limit_put_param;
	creq.get_param = cmode_limit_get_param;
	creq.conv_param = cmode_limit_conv_param;
	creq.free_param = cmode_limit_free_param;
	creq.dup_struct = cmode_limit_dup_struct;
	creq.sjoin_check = cmode_limit_sjoin_check;
	CmodeAdd(modinfo->handle, creq, &EXTMODE_LIMIT);

	HookAdd(modinfo->handle, HOOKTYPE_CAN_JOIN, 0, limit_can_join);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/** Can the user join the channel? */
int limit_can_join(Client *client, Channel *channel, const char *key, char **errmsg)
{
	ChannelLimit *r = (ChannelLimit *)GETPARASTRUCT(channel, 'l');

	/* Is the channel +l? */
	if (r && r->limit && (channel->users >= r->limit))
	{
		Hook *h;
		for (h = Hooks[HOOKTYPE_CAN_JOIN_LIMITEXCEEDED]; h; h = h->next) 
		{
			int i = (*(h->func.intfunc))(client,channel,key,errmsg);
			if (i != 0)
				return i;
		}
		*errmsg = STR_ERR_CHANNELISFULL;
		return ERR_CHANNELISFULL;
	}

	return 0;
}

int cmode_limit_is_ok(Client *client, Channel *channel, char mode, const char *param, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		/* Permitted for +hoaq */
		if (IsUser(client) && check_channel_access(client, channel, "hoaq"))
			return EX_ALLOW;
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		/* Actually any value is valid, we just morph it */
		return EX_ALLOW;
	}

	/* fallthrough -- should not be used */
	return EX_DENY;
}

void *cmode_limit_put_param(void *k_in, const char *param)
{
	ChannelLimit *fld = (ChannelLimit *)k_in;

	if (!fld)
		fld = safe_alloc(sizeof(ChannelLimit));

	fld->limit = transform_channel_limit(param);

	return fld;
}

const char *cmode_limit_get_param(void *r_in)
{
	ChannelLimit *r = (ChannelLimit *)r_in;
	static char retbuf[32];

	if (!r)
		return NULL;

	snprintf(retbuf, sizeof(retbuf), "%d", r->limit);
	return retbuf;
}

const char *cmode_limit_conv_param(const char *param, Client *client, Channel *channel)
{
	static char retbuf[32];
	int v = transform_channel_limit(param);
	snprintf(retbuf, sizeof(retbuf), "%d", v);
	return retbuf;
}

int cmode_limit_free_param(void *r, int soft)
{
	safe_free(r);
	return 0;
}

void *cmode_limit_dup_struct(void *r_in)
{
	ChannelLimit *r = (ChannelLimit *)r_in;
	ChannelLimit *w = safe_alloc(sizeof(ChannelLimit));

	memcpy(w, r, sizeof(ChannelLimit));

	return (void *)w;
}

int cmode_limit_sjoin_check(Channel *channel, void *ourx, void *theirx)
{
	ChannelLimit *our = (ChannelLimit *)ourx;
	ChannelLimit *their = (ChannelLimit *)theirx;

	if (our->limit == their->limit)
		return EXSJ_SAME;
	else if (our->limit > their->limit)
		return EXSJ_WEWON;
	else
		return EXSJ_THEYWON;
}

int transform_channel_limit(const char *param)
{
	int v = atoi(param);
	if (v <= 0)
		v = 1; /* setting +l with a negative number makes no sense */
	if (v > 1000000)
		v = 1000000; /* some kind of limit, 1 million (mrah...) */
	return v;
}
