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

/** List of all extbans, their handlers, etc */
MODVAR Extban *extbans = NULL;

void set_isupport_extban(void)
{
	char extbanstr[512];
	Extban *e;
	char *p = extbanstr;

	for (e = extbans; e; e = e->next)
		*p++ = e->letter;
	*p = '\0';

	ISupportSetFmt(NULL, "EXTBAN", "~,%s", extbanstr);
}

Extban *findmod_by_bantype_raw(const char *str, int ban_name_length)
{
	Extban *e;

	for (e=extbans; e; e = e->next)
	{
		if ((ban_name_length == 1) && (e->letter == str[0]))
			return e;
		if (e->name)
		{
			int namelen = strlen(e->name);
			if ((namelen == ban_name_length) && !strncmp(e->name, str, namelen))
				return e;
		}
	}

	 return NULL;
}

Extban *findmod_by_bantype(const char *str, const char **remainder)
{
	int ban_name_length;
	const char *p = strchr(str, ':');

	if (!p || !p[1])
	{
		if (remainder)
			*remainder = NULL;
		return NULL;
	}
	if (remainder)
		*remainder = p+1;

	ban_name_length = p - str - 1;
	return findmod_by_bantype_raw(str+1, ban_name_length);
}

/* Check if this is a valid extended ban name */
int is_valid_extban_name(const char *p)
{
	if (!*p)
		return 0; /* empty name */
	for (; *p; p++)
		if (!isalnum(*p) && !strchr("_-", *p))
			return 0;
	return 1;
}

static void extban_add_sorted(Extban *n)
{
	Extban *m;

	if (extbans == NULL)
	{
		extbans = n;
		return;
	}

	for (m = extbans; m; m = m->next)
	{
		if (m->letter == '\0')
			abort();
		if (sort_character_lowercase_before_uppercase(n->letter, m->letter))
		{
			/* Insert us before */
			if (m->prev)
				m->prev->next = n;
			else
				extbans = n; /* new head */
			n->prev = m->prev;

			n->next = m;
			m->prev = n;
			return;
		}
		if (!m->next)
		{
			/* Append us at end */
			m->next = n;
			n->prev = m;
			return;
		}
	}
}

Extban *ExtbanAdd(Module *module, ExtbanInfo req)
{
	Extban *e;
	ModuleObject *banobj;
	int existing = 0;

	if (!req.name)
	{
		module->errorcode = MODERR_INVALID;
		unreal_log(ULOG_ERROR, "module", "EXTBANADD_API_ERROR", NULL,
			   "ExtbanAdd(): name must be specified for ban (new in U6). Module: $module_name",
			   log_data_string("module_name", module->header->name));
		return NULL;
	}

	if (!req.is_banned_events && req.is_banned)
	{
		module->errorcode = MODERR_INVALID;
		unreal_log(ULOG_ERROR, "module", "EXTBANADD_API_ERROR", NULL,
			   "ExtbanAdd(): module must indicate via .is_banned_events on which BANCHK_* "
			   "events to listen on (new in U6). Module: $module_name",
			   log_data_string("module_name", module->header->name));
		return NULL;
	}

	if (!isalnum(req.letter))
	{
		module->errorcode = MODERR_INVALID;
		unreal_log(ULOG_ERROR, "module", "EXTBANADD_API_ERROR", NULL,
		           "ExtbanAdd(): module tried to add extban which is not alphanumeric. "
		           "Module: $module_name",
		           log_data_string("module_name", module->header->name));
		return NULL;
	}

	if (!is_valid_extban_name(req.name))
	{
		module->errorcode = MODERR_INVALID;
		unreal_log(ULOG_ERROR, "module", "EXTBANADD_API_ERROR", NULL,
		           "ExtbanAdd(): module tried to add extban with an invalid name ($extban_name). "
		           "Module: $module_name",
		           log_data_string("module_name", module->header->name),
		           log_data_string("extban_name", req.name));
		return NULL;
	}

	if (!req.conv_param)
	{
		module->errorcode = MODERR_INVALID;
		unreal_log(ULOG_ERROR, "module", "EXTBANADD_API_ERROR", NULL,
			   "ExtbanAdd(): conv_param event missing. Module: $module_name",
			   log_data_string("module_name", module->header->name));
		return NULL;
	}

	for (e=extbans; e; e = e->next)
	{
		if (e->letter == req.letter)
		{
			/* Extban already exists in our list, let's see... */
			if (e->unloaded)
			{
				e->unloaded = 0;
				existing = 1;
				break;
			} else
			if ((module->flags == MODFLAG_TESTING) && e->preregistered)
			{
				/* We are in MOD_INIT (yeah confusing, isn't it?)
				 * and the extban already exists and it was preregistered.
				 * Then go ahead with really registering it.
				 */
				e->preregistered = 0;
				existing = 1;
				break;
			} else
			if (module->flags == MODFLAG_NONE)
			{
				/* Better don't touch it, as we may still fail at this stage
				 * and if we would set .conv_param etc to this and the new module
				 * gets unloaded because of a config typo then we would be screwed
				 * (now we are not).
				 * NOTE: this does mean that if you hot-load an extban module
				 * then it may only be available for config stuff the 2nd rehash.
				 */
				return e;
			} else
			{
				module->errorcode = MODERR_EXISTS;
				return NULL;
			}
		}
	}

	if (!e)
	{
		/* Not found, create */
		e = safe_alloc(sizeof(Extban));
		e->letter = req.letter;
		extban_add_sorted(e);
	}
	e->letter = req.letter;
	safe_strdup(e->name, req.name);
	e->is_ok = req.is_ok;
	e->conv_param = req.conv_param;
	e->is_banned = req.is_banned;
	e->is_banned_events = req.is_banned_events;
	e->owner = module;
	e->options = req.options;

	if (module->flags == MODFLAG_NONE)
		e->preregistered = 1;

	banobj = safe_alloc(sizeof(ModuleObject));
	banobj->object.extban = e;
	banobj->type = MOBJ_EXTBAN;
	AddListItem(banobj, module->objects);
	module->errorcode = MODERR_NOERROR;

	set_isupport_extban();
	return e;
}

static void unload_extban_commit(Extban *e)
{
	/* Should we mass unban everywhere?
	 * Hmmm. Not needed per se, user can always unset
	 * themselves. Leaning towards no atm.
	 */
	// noop

	/* Then unload the extban */
	DelListItem(e, extbans);
	safe_free(e->name);
	safe_free(e);
	set_isupport_extban();
}

/** Unload all unused extended bans after a REHASH */
void unload_all_unused_extbans(void)
{
	Extban *e, *e_next;

	for (e=extbans; e; e = e_next)
	{
		e_next = e->next;
		if (e->letter && e->unloaded)
		{
			unload_extban_commit(e);
		}
	}

}

void ExtbanDel(Extban *e)
{
	/* Always free the module object */
	if (e->owner)
	{
		ModuleObject *banobj;
		for (banobj = e->owner->objects; banobj; banobj = banobj->next)
		{
			if (banobj->type == MOBJ_EXTBAN && banobj->object.extban == e)
			{
				DelListItem(banobj, e->owner->objects);
				safe_free(banobj);
				break;
			}
		}
	}

	/* Whether we can actually (already) free the Extban, it depends... */
	if (loop.rehashing)
		e->unloaded = 1;
	else
		unload_extban_commit(e);
}

/** General is_ok for n!u@h stuff that also deals with recursive extbans.
 */
int extban_is_ok_nuh_extban(BanContext *b)
{
	int isok;
	static int extban_is_ok_recursion = 0;

	/* Mostly copied from clean_ban_mask - but note MyUser checks aren't needed here: extban->is_ok() according to cmd_mode isn't called for nonlocal. */
	if (is_extended_ban(b->banstr))
	{
		const char *nextbanstr;
		Extban *extban = NULL;

		/* We're dealing with a stacked extended ban.
		 * Rules:
		 * 1) You can only stack once, so: ~x:~y:something and not ~x:~y:~z...
		 * 2) The second item may never be an action modifier, nor have the
		 *    EXTBOPT_NOSTACKCHILD letter set (for things like a textban).
		 */

		if (extban_is_ok_recursion)
			return 0; /* Rule #1 violation (more than one stacked extban) */

		if ((b->is_ok_check == EXBCHK_PARAM) && RESTRICT_EXTENDEDBANS && !ValidatePermissionsForPath("immune:restrict-extendedbans",b->client,NULL,b->channel,NULL))
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

		if ((extban->options & EXTBOPT_ACTMODIFIER) || (extban->options & EXTBOPT_NOSTACKCHILD))
		{
			/* Rule #2 violation */
			return 0;
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
const char *extban_conv_param_nuh(BanContext *b, Extban *extban)
{
	char tmpbuf[USERLEN + NICKLEN + HOSTLEN + 32];
	static char retbuf[USERLEN + NICKLEN + HOSTLEN + 32];

	/* Work on a copy */
	strlcpy(tmpbuf, b->banstr, sizeof(retbuf));
	return convert_regular_ban(tmpbuf, retbuf, sizeof(retbuf));
}

/** conv_param to deal with stacked extbans.
 */
const char *extban_conv_param_nuh_or_extban(BanContext *b, Extban *self_extban)
{
#if (USERLEN + NICKLEN + HOSTLEN + 32) > 256
 #error "wtf?"
#endif
	static char retbuf[256];
	static char printbuf[256];
	char *mask;
	char tmpbuf[USERLEN + NICKLEN + HOSTLEN + 32];
	const char *ret = NULL;
	const char *nextbanstr;
	Extban *extban = NULL;
	static int extban_recursion = 0;

	if (!is_extended_ban(b->banstr))
		return extban_conv_param_nuh(b, self_extban);

	/* We're dealing with a stacked extended ban.
	 * Rules:
	 * 1) You can only stack once, so: ~x:~y:something and not ~x:~y:~z...
	 * 2) The second item may never be an action modifier, nor have the
	 *    EXTBOPT_NOSTACKCHILD letter set (for things like a textban).
	 */
	 
	/* Rule #1. Yes the recursion check is also in extban_is_ok_nuh_extban,
	 * but it's possible to get here without the is_ok() function ever
	 * being called (think: non-local client). And no, don't delete it
	 * there either. It needs to be in BOTH places. -- Syzop
	 */
	if (extban_recursion)
		return NULL;

	strlcpy(tmpbuf, b->banstr, sizeof(tmpbuf));
	extban = findmod_by_bantype(tmpbuf, &nextbanstr);

	if (!extban)
	{
		/* Handling unknown bantypes in is_ok. Assume that it's ok here. */
		return b->banstr;
	}

	b->banstr = nextbanstr;

	if ((extban->options & EXTBOPT_ACTMODIFIER) || (extban->options & EXTBOPT_NOSTACKCHILD))
	{
		/* Rule #2 violation */
		return NULL;
	}

	extban_recursion++;
	ret = extban->conv_param(b, extban);
	extban_recursion--;
	ret = prefix_with_extban(ret, b, extban, retbuf, sizeof(retbuf));
	return ret;
}

char *prefix_with_extban(const char *remainder, BanContext *b, Extban *extban, char *buf, size_t buflen)
{
	/* Yes, we support this because it makes code at the caller cleaner */
	if (remainder == NULL)
		return NULL;

	if (iConf.named_extended_bans && !(b->conv_options & BCTX_CONV_OPTION_WRITE_LETTER_BANS))
		snprintf(buf, buflen, "~%s:%s", extban->name, remainder);
	else
		snprintf(buf, buflen, "~%c:%s", extban->letter, remainder);

	return buf;
}
