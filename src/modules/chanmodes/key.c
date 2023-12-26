/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/key.c
 * Channel Mode +k
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
	"chanmodes/key",
	"6.0",
	"Channel Mode +k",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

typedef struct ChannelKey ChannelKey;
struct ChannelKey {
	char key[KEYLEN+1];
};

/* Global variables */
ModDataInfo *mdkey = NULL;
Cmode_t EXTMODE_KEY = 0L;

#define IsKey(x)	((x)->mode.mode & EXTMODE_KEY)

/* Forward declarations */
int key_can_join(Client *client, Channel *channel, const char *key, char **errmsg);
int cmode_key_is_ok(Client *client, Channel *channel, char mode, const char *para, int type, int what);
void *cmode_key_put_param(void *r_in, const char *param);
const char *cmode_key_get_param(void *r_in);
const char *cmode_key_conv_param(const char *param_in, Client *client, Channel *channel);
int cmode_key_free_param(void *r, int soft);
void *cmode_key_dup_struct(void *r_in);
int cmode_key_sjoin_check(Channel *channel, void *ourx, void *theirx);
int is_valid_key(const char *key);
void transform_channel_key(const char *i, char *o, int n);

MOD_INIT()
{
	CmodeInfo creq;
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&creq, 0, sizeof(creq));
	creq.paracount = 1;
	creq.is_ok = cmode_key_is_ok;
	creq.letter = 'k';
	creq.unset_with_param = 1; /* yeah... +k is like this */
	creq.put_param = cmode_key_put_param;
	creq.get_param = cmode_key_get_param;
	creq.conv_param = cmode_key_conv_param;
	creq.free_param = cmode_key_free_param;
	creq.dup_struct = cmode_key_dup_struct;
	creq.sjoin_check = cmode_key_sjoin_check;
	CmodeAdd(modinfo->handle, creq, &EXTMODE_KEY);

	HookAdd(modinfo->handle, HOOKTYPE_CAN_JOIN, 0, key_can_join);
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
int key_can_join(Client *client, Channel *channel, const char *key, char **errmsg)
{
	ChannelKey *r = (ChannelKey *)GETPARASTRUCT(channel, 'k');

	/* Is the channel +k? */
	if (r && *r->key)
	{
		if (key && !strcmp(r->key, key))
			return 0;
		*errmsg = STR_ERR_BADCHANNELKEY;
		return ERR_BADCHANNELKEY;
	}

	return 0;
}

int cmode_key_is_ok(Client *client, Channel *channel, char mode, const char *param, int type, int what)
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
		if (!is_valid_key(param))
		{
			sendnumeric(client, ERR_INVALIDMODEPARAM,
				channel->name, 'k', "*", "Channel key contains forbidden characters or is too long");
			return EX_DENY;
		}
		return EX_ALLOW;
	}

	/* fallthrough -- should not be used */
	return EX_DENY;
}

void *cmode_key_put_param(void *k_in, const char *param)
{
	ChannelKey *fld = (ChannelKey *)k_in;

	if (!fld)
		fld = safe_alloc(sizeof(ChannelKey));

	transform_channel_key(param, fld->key, sizeof(fld->key));

	return fld;
}

const char *cmode_key_get_param(void *r_in)
{
	ChannelKey *r = (ChannelKey *)r_in;
	static char retbuf[KEYLEN+1];

	if (!r)
		return NULL;

	strlcpy(retbuf, r->key, sizeof(retbuf));
	return retbuf;
}

const char *cmode_key_conv_param(const char *param, Client *client, Channel *channel)
{
	static char retbuf[KEYLEN+1];

	transform_channel_key(param, retbuf, sizeof(retbuf));

	if (!*retbuf)
		return NULL; /* entire key was invalid */

	return retbuf;
}

int cmode_key_free_param(void *r, int soft)
{
	safe_free(r);
	return 0;
}

void *cmode_key_dup_struct(void *r_in)
{
	ChannelKey *r = (ChannelKey *)r_in;
	ChannelKey *w = safe_alloc(sizeof(ChannelKey));

	memcpy(w, r, sizeof(ChannelKey));

	return (void *)w;
}

int cmode_key_sjoin_check(Channel *channel, void *ourx, void *theirx)
{
	ChannelKey *our = (ChannelKey *)ourx;
	ChannelKey *their = (ChannelKey *)theirx;
	int i;
	int r;

	r = strcmp(our->key, their->key);
	if (r == 0)
		return EXSJ_SAME;
	else if (r > 0)
		return EXSJ_WEWON;
	else
		return EXSJ_THEYWON;
}

int valid_key_char(char c)
{
	if (strchr(" :,", c))
		return 0;
	if (c <= 32)
		return 0;
	return 1;
}

#define BADKEYCHARS " :,"
int is_valid_key(const char *key)
{
	const char *p;

	if (strlen(key) > KEYLEN)
		return 0;
	for (p = key; *p; p++)
		if (!valid_key_char(*p))
			return 0;
	return 1;
}

void transform_channel_key(const char *i, char *o, int n)
{
	n--; /* reserve one for final nul byte */

	for (; *i; i++)
	{
		if (!valid_key_char(*i))
			break;
		if (n <= 0)
			break;
		*o++ = *i;
		n--;
	}
	*o = '\0';
}
