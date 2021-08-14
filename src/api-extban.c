/************************************************************************
 *   IRC - Internet Relay Chat, api-extban.c
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

#include "unrealircd.h"

MODVAR Extban ExtBan_Table[EXTBANTABLESZ]; /* this should be fastest */
MODVAR int ExtBan_highest = 0;

void set_isupport_extban(void)
{
	int i;
	char extbanstr[EXTBANTABLESZ+1], *m;

	m = extbanstr;
	for (i = 0; i <= ExtBan_highest; i++)
	{
		if (ExtBan_Table[i].letter)
			*m++ = ExtBan_Table[i].letter;
	}
	*m = 0;
	ISupportSetFmt(NULL, "EXTBAN", "~,%s", extbanstr);
}

Extban *findmod_by_bantype(char *str, char **remainder)
{
	int i;
	char *p = strchr(str, ':');

	if (!p || !p[1])
	{
		if (remainder)
			*remainder = NULL;
		return NULL;
	}
	if (remainder)
		*remainder = p+1;

	for (i=0; i <= ExtBan_highest; i++)
	{
		if (ExtBan_Table[i].letter == str[1])
			return &ExtBan_Table[i];
		if (ExtBan_Table[i].name && !strncmp(ExtBan_Table[i].name, str+1, strlen(ExtBan_Table[i].name)))
			return &ExtBan_Table[i];
	}

	 return NULL;
}

Extban *ExtbanAdd(Module *module, ExtbanInfo req)
{
	int slot;

	if (!req.name)
	{
		if (module)
			module->errorcode = MODERR_INVALID;
		unreal_log(ULOG_ERROR, "module", "EXTBANADD_API_ERROR", NULL,
			   "ExtbanAdd(): name must be specified for ban (new in U6). Module: $module_name",
			   log_data_string("module_name", module->header->name));
		return NULL;
	}

	for (slot=0; slot <= ExtBan_highest; slot++)
	{
		if ((ExtBan_Table[slot].letter == req.letter) ||
		    (ExtBan_Table[slot].name && !strcasecmp(ExtBan_Table[slot].name, req.name)))
		{
			if (module)
				module->errorcode = MODERR_EXISTS;
			return NULL;
		}
	}

	/* TODO: perhaps some sanity checking on a-zA-Z0-9? */

	/* Find next available slot... */
	for (slot = 0; slot < EXTBANTABLESZ; slot++)
		if (ExtBan_Table[slot].letter == '\0')
			break;

	if (slot >= EXTBANTABLESZ - 1)
	{
		unreal_log(ULOG_ERROR, "module", "EXTBAN_OUT_OF_SPACE", NULL,
		           "ExtbanAdd: out of space!!!");
		if (module)
			module->errorcode = MODERR_NOSPACE;
		return NULL;
	}

	ExtBan_Table[slot].letter = req.letter;
	safe_strdup(ExtBan_Table[slot].name, req.name);
	ExtBan_Table[slot].is_ok = req.is_ok;
	ExtBan_Table[slot].conv_param = req.conv_param;
	ExtBan_Table[slot].is_banned = req.is_banned;
	ExtBan_Table[slot].owner = module;
	ExtBan_Table[slot].options = req.options;
	if (module)
	{
		ModuleObject *banobj = safe_alloc(sizeof(ModuleObject));
		banobj->object.extban = &ExtBan_Table[slot];
		banobj->type = MOBJ_EXTBAN;
		AddListItem(banobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	ExtBan_highest = slot;
	set_isupport_extban();
	return &ExtBan_Table[slot];
}

void ExtbanDel(Extban *eb)
{
	/* Just zero it all away.. */

	if (eb->owner)
	{
		ModuleObject *banobj;
		for (banobj = eb->owner->objects; banobj; banobj = banobj->next)
		{
			if (banobj->type == MOBJ_EXTBAN && banobj->object.extban == eb)
			{
				DelListItem(banobj, eb->owner->objects);
				safe_free(banobj);
				break;
			}
		}
	}
	safe_free(eb->name);
	memset(eb, 0, sizeof(Extban));
	set_isupport_extban();
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
int extban_is_ok_nuh_extban(BanContext *b)
{
	int isok;
	static int extban_is_ok_recursion = 0;

	/* Mostly copied from clean_ban_mask - but note MyUser checks aren't needed here: extban->is_ok() according to cmd_mode isn't called for nonlocal. */
	if (is_extended_ban(b->banstr))
	{
		char *nextbanstr;
		Extban *extban = NULL;

		if (extban_is_ok_recursion)
			return 0; /* Fail: more than one stacked extban */

		if ((b->is_ok_checktype == EXBCHK_PARAM) && RESTRICT_EXTENDEDBANS && !ValidatePermissionsForPath("immune:restrict-extendedbans",b->client,NULL,b->channel,NULL))
		{
			/* Test if this specific extban has been disabled.
			 * (We can be sure RESTRICT_EXTENDEDBANS is not *. Else this extended ban wouldn't be happening at all.)
			 */
			if (strchr(RESTRICT_EXTENDEDBANS, b->banstr[1]))
			{
				sendnotice(b->client, "Setting/removing of extended bantypes '%s' has been disabled.", RESTRICT_EXTENDEDBANS);
				return 0; /* Fail */
			}
		}
		extban = findmod_by_bantype(b->banstr, &nextbanstr);
		if (!extban)
		{
			if (b->what == MODE_DEL)
			{
				return 1; /* Always allow killing unknowns. */
			}
			return 0; /* Don't add unknown extbans. */
		}
		/* Now we have to ask the stacked extban if it's ok. */
		if (extban->is_ok)
		{
			b->banstr = nextbanstr;
			extban_is_ok_recursion++;
			isok = extban->is_ok(b);
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
char *extban_conv_param_nuh(BanContext *b, Extban *extban)
{
	char *cp, *user, *host, *mask, *ret = NULL;
	static char retbuf[USERLEN + NICKLEN + HOSTLEN + 32];
	char tmpbuf[USERLEN + NICKLEN + HOSTLEN + 32];

	/* Work on a copy */
	strlcpy(tmpbuf, b->banstr, sizeof(retbuf));
	mask = tmpbuf;

	if (!*mask)
		return NULL; /* empty extban */
	if ((*mask == '~') && !strchr(mask, '@'))
		return NULL; /* not a user@host ban, too confusing. */
	if ((user = strchr((cp = mask), '!')))
		*user++ = '\0';
	if ((host = strrchr(user ? user : cp, '@')))
	{
		*host++ = '\0';
		if (!user)
			ret = make_nick_user_host(NULL, trim_str(cp,USERLEN), trim_str(host,HOSTLEN));
	}
	else if (!user && strchr(cp, '.'))
		ret = make_nick_user_host(NULL, NULL, trim_str(cp,HOSTLEN));
	if (!ret)
		ret = make_nick_user_host(trim_str(cp,NICKLEN), trim_str(user,USERLEN), trim_str(host,HOSTLEN));

	strlcpy(retbuf, ret, sizeof(retbuf));
	return retbuf;
}

/** conv_param to deal with stacked extbans.
 */
char *extban_conv_param_nuh_or_extban(BanContext *b, Extban *self_extban)
{
#if (USERLEN + NICKLEN + HOSTLEN + 32) > 256
 #error "wtf?"
#endif
	static char retbuf[256];
	static char printbuf[256];
	char *mask;
	char tmpbuf[USERLEN + NICKLEN + HOSTLEN + 32];
	char *ret = NULL;
	char *nextbanstr;
	Extban *extban = NULL;
	static int extban_recursion = 0;

	if (!is_extended_ban(b->banstr))
		return extban_conv_param_nuh(b, self_extban);

	/* We're dealing with a stacked extended ban.
	 * Rules:
	 * 1) You can only stack once, so: ~x:~y:something and not ~x:~y:~z...
	 * 2) The first item must be an action modifier, such as ~q/~n/~j
	 * 3) The second item may never be an action modifier, nor have the
	 *    EXTBOPT_NOSTACKCHILD letter set (for things like a textban).
	 */
	 
	/* Rule #1. Yes the recursion check is also in extban_is_ok_nuh_extban,
	 * but it's possible to get here without the is_ok() function ever
	 * being called (think: non-local client). And no, don't delete it
	 * there either. It needs to be in BOTH places. -- Syzop
	 */
	if (extban_recursion)
		return NULL;

#if 0
	// FIXME: FIX THIS AGAIN PLZZZZZZZZZZZZZZZZZZZZZZZ
	// CURRENTLY CANNOT LOOKUP SELF!
	// ACTUALLY WE CAN NOW WITH extban->letter.. but it is a char not a string ;)

	/* Rule #2 */
	extban = findmod_by_bantype(b->banstr, &nextbanstr);
	if (p && !(p->options & EXTBOPT_ACTMODIFIER))
	{
		/* Rule #2 violation */
		return NULL;
	}
	
	strlcpy(tmpbuf, b->banstr, sizeof(tmpbuf));
	mask = tmpbuf + 3;
	/* Already did restrict-extended bans check. */
	extban = findmod_by_bantype(mask[1]);
#else
	strlcpy(tmpbuf, b->banstr, sizeof(tmpbuf));
	extban = findmod_by_bantype(tmpbuf, &nextbanstr);
#endif
	if (!extban)
	{
		/* Handling unknown bantypes in is_ok. Assume that it's ok here. */
		return b->banstr;
	}

	b->banstr = nextbanstr;

	if ((extban->options & EXTBOPT_ACTMODIFIER) || (extban->options & EXTBOPT_NOSTACKCHILD))
	{
		/* Rule #3 violation */
		return NULL;
	}

	if (extban->conv_param)
	{
		//BanContext *b = safe_alloc(sizeof(BanContext));
		//b->banstr = mask; <-- this is redundant right? we can use existing 'b' context??
		extban_recursion++;
		ret = extban->conv_param(b, extban);
		extban_recursion--;
		ret = prefix_with_extban(ret, b, extban, retbuf, sizeof(retbuf));
		//safe_free(b);
		return ret;
	}
	/* I honestly don't know what the deal is with the 80 char cap in clean_ban_mask is about. So I'm leaving it out here. -- aquanight */
	/* I don't know why it's 80, but I like a limit anyway. A ban of 500 characters can never be good... -- Syzop */
	if (strlen(b->banstr) > 80)
	{
		strlcpy(retbuf, b->banstr, 128);
		return retbuf;
	}
	return b->banstr;
}

char *prefix_with_extban(char *remainder, BanContext *b, Extban *extban, char *buf, size_t buflen)
{
	/* Yes, we support this because it makes code at the caller cleaner */
	if (remainder == NULL)
		return NULL;

	if (iConf.named_extended_bans && !b->write_letter_bans)
		snprintf(buf, buflen, "~%s:%s", extban->name, remainder);
	else
		snprintf(buf, buflen, "~%c:%s", extban->letter, remainder);

	return buf;
}
