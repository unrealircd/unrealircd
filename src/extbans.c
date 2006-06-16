/************************************************************************
 *   IRC - Internet Relay Chat, extbans.c
 *   (C) 2003 Bram Matthys (Syzop) and the UnrealIRCd Team
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

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include "version.h"
#include <time.h>
#ifdef _WIN32
#include <sys/timeb.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

Extban MODVAR ExtBan_Table[EXTBANTABLESZ]; /* this should be fastest */
unsigned MODVAR short ExtBan_highest = 0;

char MODVAR extbanstr[EXTBANTABLESZ+1];

void make_extbanstr(void)
{
	int i;
	char *m;

	m = extbanstr;
	for (i = 0; i <= ExtBan_highest; i++)
	{
		if (ExtBan_Table[i].flag)
			*m++ = ExtBan_Table[i].flag;
	}
	*m = 0;
}

Extban *findmod_by_bantype(char c)
{
int i;

	for (i=0; i <= ExtBan_highest; i++)
		if (ExtBan_Table[i].flag == c)
			return &ExtBan_Table[i];

	 return NULL;
}

Extban *ExtbanAdd(Module *module, ExtbanInfo req)
{
int slot;
char tmpbuf[512];

	if (findmod_by_bantype(req.flag))
	{
		if (module)
			module->errorcode = MODERR_EXISTS;
		return NULL; 
	}

	/* TODO: perhaps some sanity checking on a-zA-Z0-9? */
	for (slot = 0; slot < EXTBANTABLESZ; slot++)
		if (ExtBan_Table[slot].flag == '\0')
			break;
	if (slot == EXTBANTABLESZ - 1)
	{
		if (module)
			module->errorcode = MODERR_NOSPACE;
		return NULL;
	}
	ExtBan_Table[slot].flag = req.flag;
	ExtBan_Table[slot].is_ok = req.is_ok;
	ExtBan_Table[slot].conv_param = req.conv_param;
	ExtBan_Table[slot].is_banned = req.is_banned;
	ExtBan_Table[slot].owner = module;
	ExtBan_Table[slot].options = req.options;
	if (module)
	{
		ModuleObject *banobj = MyMallocEx(sizeof(ModuleObject));
		banobj->object.extban = &ExtBan_Table[slot];
		banobj->type = MOBJ_EXTBAN;
		AddListItem(banobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	ExtBan_highest = slot;
	if (loop.ircd_booted)
	{
		make_extbanstr();
		ircsprintf(tmpbuf, "~,%s", extbanstr);
		IsupportSetValue(IsupportFind("EXTBAN"), tmpbuf);
	}
	return &ExtBan_Table[slot];
}

void ExtbanDel(Extban *eb)
{
char tmpbuf[512];
	/* Just zero it all away.. */

	if (eb->owner)
	{
		ModuleObject *banobj;
		for (banobj = eb->owner->objects; banobj; banobj = banobj->next)
		{
			if (banobj->type == MOBJ_EXTBAN && banobj->object.extban == eb)
			{
				DelListItem(banobj, eb->owner->objects);
				MyFree(banobj);
				break;
			}
		}
	}
	memset(eb, 0, sizeof(Extban));
	make_extbanstr();
	ircsprintf(tmpbuf, "~,%s", extbanstr);
	IsupportSetValue(IsupportFind("EXTBAN"), tmpbuf);
	/* Hmm do we want to go trough all chans and remove the bans?
	 * I would say 'no' because perhaps we are just reloading,
	 * and else.. well... screw them?
	 */
}

/* NOTE: the routines below can safely assume the ban has at
 * least the '~t:' part (t=type). -- Syzop
 */

/* TODO: just get rid of strchr */

char *extban_modec_conv_param(char *para)
{
static char retbuf[CHANNELLEN+6];
char *chan, *p, symbol='\0';

	strncpyzt(retbuf, para, sizeof(retbuf));
	chan = retbuf+3;

	if ((*chan == '+') || (*chan == '%') || (*chan == '%') ||
	    (*chan == '@') || (*chan == '&') || (*chan == '~'))
	    chan++;

	if ((*chan != '#') && (*chan != '*') && (*chan != '?'))
		return NULL;

	if (strlen(chan) > CHANNELLEN)
		chan[CHANNELLEN] = '\0';
	clean_channelname(chan);
	p = strchr(chan, ':'); /* ~r:#chan:*.blah.net is not allowed (for now) */
	if (p)
		*p = '\0';
	/* on a sidenote '#' is allowed because it's a valid channel (atm) */
	return retbuf;
}

/* The only purpose of this function is a temporary workaround to prevent a desynch.. pfff */
int extban_modec_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what, int what2)
{
char *p;

	if ((checkt == EXBCHK_PARAM) && MyClient(sptr) && (what == MODE_ADD) && (strlen(para) > 3))
	{
		p = para + 3;
		if ((*p == '+') || (*p == '%') || (*p == '%') ||
		    (*p == '@') || (*p == '&') || (*p == '~'))
		    p++;

		if (*p != '#')
		{
			sendnotice(sptr, "Please use a # in the channelname (eg: ~c:#*blah*)");
			return 0;
		}
	}
	return 1;
}


static int extban_modec_compareflags(char symbol, int flags)
{
int require=0;

	if (symbol == '+')
		require = CHFL_VOICE|CHFL_HALFOP|CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER;
	else if (symbol == '%')
		require = CHFL_HALFOP|CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER;
	else if (symbol == '@')
		require = CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER;
	else if (symbol == '&')
		require = CHFL_CHANPROT|CHFL_CHANOWNER;
	else if (symbol == '~')
		require = CHFL_CHANOWNER;

	if (flags & require)
		return 1;
	return 0;
}
int extban_modec_is_banned(aClient *sptr, aChannel *chptr, char *ban, int type)
{
Membership *lp;
char *p = ban+3, symbol = '\0';

	if (*p != '#')
	{
		symbol = *p;
		p++;
	}
	for (lp = sptr->user->channel; lp; lp = lp->next)
	{
		if (!match_esc(p, lp->chptr->chname))
		{
			/* Channel matched, check symbol if needed (+/%/@/etc) */
			if (symbol)
			{
				if (extban_modec_compareflags(symbol, lp->flags))
					return 1;
			} else
				return 1;
		}
	}
	return 0;
}

int extban_modeq_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type)
{
char *ban = banin + 3;

	if (type != BANCHK_MSG)
		return 0;

	return extban_is_banned_helper(ban);
}

int extban_moden_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type)
{
char *ban = banin + 3;

	if (type != BANCHK_NICK)
		return 0;

	if (has_voice(sptr, chptr))
		return 0;
	
	return extban_is_banned_helper(ban);
}

/** Some kind of general conv_param routine,
 * to ensure the parameter is nick!user@host.
 * most of the code is just copied from clean_ban_mask.
 */
char *extban_conv_param_nuh(char *para)
{
char *cp, *user, *host, *mask, *ret = NULL;
static char retbuf[USERLEN + NICKLEN + HOSTLEN + 32];
char tmpbuf[USERLEN + NICKLEN + HOSTLEN + 32];
char pfix[8];

	strncpyzt(tmpbuf, para, sizeof(retbuf));
	mask = tmpbuf + 3;
	strncpyzt(pfix, tmpbuf, mask - tmpbuf + 1);

	if ((*mask == '~') && !strchr(mask, '@'))
		return NULL; /* not a user@host ban, too confusing. */
	if ((user = index((cp = mask), '!')))
		*user++ = '\0';
	if ((host = rindex(user ? user : cp, '@')))
	{
		*host++ = '\0';
		if (!user)
			ret = make_nick_user_host(NULL, trim_str(cp,USERLEN), trim_str(host,HOSTLEN));
	}
	else if (!user && index(cp, '.'))
		ret = make_nick_user_host(NULL, NULL, trim_str(cp,HOSTLEN));
	if (!ret)
		ret = make_nick_user_host(trim_str(cp,NICKLEN), trim_str(user,USERLEN), trim_str(host,HOSTLEN));

	ircsprintf(retbuf, "%s%s", pfix, ret);
	return retbuf;
}

/** Realname bans - conv_param */
char *extban_moder_conv_param(char *para)
{
char *mask;
static char retbuf[REALLEN + 8];

	strncpyzt(retbuf, para, sizeof(retbuf));
	mask = retbuf+3;
	if (strlen(mask) > REALLEN + 3)
		mask[REALLEN + 3] = '\0';
	return retbuf;
}

int extban_moder_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type)
{
char *ban = banin+3;
	if (!match_esc(ban, sptr->info))
		return 1;
	return 0;
}

void extban_init(void)
{
	ExtbanInfo req;

	memset(&req, 0, sizeof(ExtbanInfo));
	req.flag = 'c';
	req.conv_param = extban_modec_conv_param;
	req.is_banned = extban_modec_is_banned;
	req.is_ok = extban_modec_is_ok;
	ExtbanAdd(NULL, req);

	memset(&req, 0, sizeof(ExtbanInfo));
	req.flag = 'q';
	req.conv_param = extban_conv_param_nuh;
	req.is_banned = extban_modeq_is_banned;
	ExtbanAdd(NULL, req);

	memset(&req, 0, sizeof(ExtbanInfo));
	req.flag = 'n';
	req.conv_param = extban_conv_param_nuh;
	req.is_banned = extban_moden_is_banned;
	ExtbanAdd(NULL, req);

	memset(&req, 0, sizeof(ExtbanInfo));
	req.flag = 'r';
	req.conv_param = extban_moder_conv_param;
	req.is_banned = extban_moder_is_banned;
	req.options = EXTBOPT_CHSVSMODE;
	ExtbanAdd(NULL, req);

	/* When adding new extbans, be sure to always add a prior memset like above
	 * so you don't "inherit" old options (we are 2005 and the 20 nanoseconds
	 * per-boot/rehash is NOT EXACTLY a problem....) -- Syzop.
	 */
}
