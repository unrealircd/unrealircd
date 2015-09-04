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
	if (slot >= EXTBANTABLESZ - 1)
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
		ircsnprintf(tmpbuf, sizeof(tmpbuf), "~,%s", extbanstr);
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
	ircsnprintf(tmpbuf, sizeof(tmpbuf), "~,%s", extbanstr);
	IsupportSetValue(IsupportFind("EXTBAN"), tmpbuf);
	/* Hmm do we want to go trough all chans and remove the bans?
	 * I would say 'no' because perhaps we are just reloading,
	 * and else.. well... screw them?
	 */
}

/* NOTE: the routines below can safely assume the ban has at
 * least the '~t:' part (t=type). -- Syzop
 */

/** General is_ok for n!u@h stuff that also deals with recursive extbans.
 */
int extban_is_ok_nuh_extban(aClient* sptr, aChannel* chptr, char* para, int checkt, int what, int what2)
{
	char *mask = (para + 3);
	Extban *p = NULL;
	int isok;
	static int extban_is_ok_recursion = 0;

	/* Mostly copied from clean_ban_mask - but note MyClient checks aren't needed here: extban->is_ok() according to m_mode isn't called for nonlocal. */
	if ((*mask == '~') && mask[1] && (mask[2] == ':'))
	{
		if (extban_is_ok_recursion)
			return 0; /* Fail: more than one stacked extban */

		/* We can be sure RESTRICT_EXTENDEDBANS is not *. Else this extended ban wouldn't be happening at all. */
		if (what == EXBCHK_PARAM && RESTRICT_EXTENDEDBANS && !ValidatePermissionsForPath("channel:extbans",sptr,NULL,chptr,NULL))
		{
			if (strchr(RESTRICT_EXTENDEDBANS, mask[1]))
			{
				sendnotice(sptr, "Setting/removing of extended bantypes '%s' has been disabled.", RESTRICT_EXTENDEDBANS);
				return 0; /* Fail */
			}
		}
		p = findmod_by_bantype(mask[1]);
		if (!p)
		{
			if (what == MODE_DEL)
			{
				return 1; /* Always allow killing unknowns. */
			}
			return 0; /* Don't add unknown extbans. */
		}
		/* Now we have to ask the stacked extban if it's ok. */
		if (p->is_ok)
		{
			extban_is_ok_recursion++;
			isok = p->is_ok(sptr, chptr, mask, checkt, what, what2);
			extban_is_ok_recursion--;
			return isok;
		}
	}
	return 1; /* Either not an extban, or extban has NULL is_ok. Good to go. */
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

	strlcpy(tmpbuf, para, sizeof(retbuf));
	mask = tmpbuf + 3;
	strlcpy(pfix, tmpbuf, mask - tmpbuf + 1);

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

	ircsnprintf(retbuf, sizeof(retbuf), "%s%s", pfix, ret);
	return retbuf;
}

/** conv_param to deal with stacked extbans.
 */
char* extban_conv_param_nuh_or_extban(char* para)
{
#if (USERLEN + NICKLEN + HOSTLEN + 32) > 256
 #error "wtf?"
#endif
	static char retbuf[256];
	static char printbuf[256];
	char *mask;
	char tmpbuf[USERLEN + NICKLEN + HOSTLEN + 32];
	char bantype = para[1];
	char *ret = NULL;
	Extban *p = NULL;
	static int extban_recursion = 0;

	if (para[3] == '~' && para[4] && para[5] == ':')
	{
		/* We're dealing with a stacked extended ban.
		 * Rules:
		 * 1) You can only stack once, so: ~x:~y:something and not ~x:~y:~z...
		 * 2) The first item must be an action modifier, such as ~q/~n/~j
		 * 3) The second item may never be an action modifier, nor have the
		 *    EXTBOPT_NOSTACKCHILD flag set (for things like a textban).
		 */
		 
		/* Rule #1. Yes the recursion check is also in extban_is_ok_nuh_extban,
		 * but it's possible to get here without the is_ok() function ever
		 * being called (think: non-local client). And no, don't delete it
		 * there either. It needs to be in BOTH places. -- Syzop
		 */
		if (extban_recursion)
			return NULL;

		/* Rule #2 */
		p = findmod_by_bantype(para[1]);
		if (p && !(p->options & EXTBOPT_ACTMODIFIER))
		{
			/* Rule #2 violation */
			return NULL;
		}
		
		strlcpy(tmpbuf, para, sizeof(tmpbuf));
		mask = tmpbuf + 3;
		/* Already did restrict-extended bans check. */
		p = findmod_by_bantype(mask[1]);
		if (!p)
		{
			/* Handling unknown bantypes in is_ok. Assume that it's ok here. */
			return para;
		}
		if ((p->options & EXTBOPT_ACTMODIFIER) || (p->options & EXTBOPT_NOSTACKCHILD))
		{
			/* Rule #3 violation */
			return NULL;
		}
		
		if (p->conv_param)
		{
			extban_recursion++;
			ret = p->conv_param(mask);
			extban_recursion--;
			if (ret)
			{
				/*
				 * If bans are stacked, then we have to use two buffers
				 * to prevent ircsnprintf() from going into a loop.
				 */
				ircsnprintf(printbuf, sizeof(printbuf), "~%c:%s", bantype, ret); /* Make sure our extban prefix sticks. */
				memcpy(retbuf, printbuf, sizeof(retbuf));
				return retbuf;
			}
			else
			{
				return NULL; /* Fail. */
			}
		}
		/* I honestly don't know what the deal is with the 80 char cap in clean_ban_mask is about. So I'm leaving it out here. -- aquanight */
		/* I don't know why it's 80, but I like a limit anyway. A ban of 500 characters can never be good... -- Syzop */
		if (strlen(para) > 80)
		{
			strlcpy(retbuf, para, 128);
			return retbuf;
		}
		return para;
	}
	else
	{
		return extban_conv_param_nuh(para);
	}
}
