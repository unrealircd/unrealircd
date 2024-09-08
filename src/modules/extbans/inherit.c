/*
 * Extended ban: inherit bans from other channel (+b ~inherit:#chan)
 * (C) Copyright 2024-.. Bram Matthys (Syzop) and the UnrealIRCd team
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
	"extbans/inherit",
	"1.0",
	"ExtBan ~inherit - inherit bans from another channel",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
static int inherit_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
static int inherit_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
int extban_inherit_is_ok(BanContext *b);
const char *extban_inherit_conv_param(BanContext *b, Extban *extban);
int extban_inherit_is_banned(BanContext *b);
int inherit_stats(Client *client, const char *flag);

/* Variables */
int maximum_inherit_ban_count = 1;
int maximum_inherit_exempt_count = 1;
int maximum_inherit_invex_count = 1;

Extban *register_channel_extban(ModuleInfo *modinfo)
{
	ExtbanInfo req;

	memset(&req, 0, sizeof(req));
	req.letter = 'i';
	req.name = "inherit";
	req.is_ok = extban_inherit_is_ok;
	req.conv_param = extban_inherit_conv_param;
	req.is_banned = extban_inherit_is_banned;
	/* We only check JOIN events for performance reasons: */
	req.is_banned_events = BANCHK_JOIN;
	/* We allow +I to also use ~inherit (option EXTBOPT_INVEX).
	 * We don't allow things like ~nick:~inherit, as we only work
	 * on JOINs (option EXTBOPT_NOSTACKCHILD).
	 */
	req.options = EXTBOPT_INVEX|EXTBOPT_NOSTACKCHILD;
	return ExtbanAdd(modinfo->handle, req);
}

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (!register_channel_extban(modinfo))
	{
		config_error("could not register extended ban type ~inherit");
		return MOD_FAILED;
	}
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, inherit_config_test);

	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (!register_channel_extban(modinfo))
	{
		config_error("could not register extended ban type ~inherit");
		return MOD_FAILED;
	}
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, inherit_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, inherit_stats);

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

static int inherit_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::max-inherit-extended-bans.. */
	if (!ce || strcmp(ce->name, "max-inherit-extended-bans"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		CheckNull(cep);
		if (!strcmp(cep->name, "ban") ||
		    !strcmp(cep->name, "ban-exception") ||
		    !strcmp(cep->name, "invite-exception"))
		{
			int v = atoi(cep->value);
			if (v < 0)
			{
				config_error("%s:%i: set::max-inherit-extended-bans::%s item has a value, which is unexpected. Check your syntax!",
					cep->file->filename, cep->line_number, cep->name);
					errors++;
				continue;
			}
		} else {
			config_error_unknown(cep->file->filename, cep->line_number,
			                     "set::max-inherit-extended-bans", cep->name);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

static int inherit_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::max-inherit-extended-bans.. */
	if (!ce || strcmp(ce->name, "max-inherit-extended-bans"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "ban"))
			maximum_inherit_ban_count = atoi(cep->value);
		else if (!strcmp(cep->name, "ban-exception"))
			maximum_inherit_exempt_count = atoi(cep->value);
		else if (!strcmp(cep->name, "invite-exception"))
			maximum_inherit_invex_count = atoi(cep->value);
	}
	return 1;
}

int inherit_stats(Client *client, const char *flag)
{
	if (*flag != 'S')
		return 0;

	sendtxtnumeric(client, "max-inherit-extended-bans::ban: %d", maximum_inherit_ban_count);
	sendtxtnumeric(client, "max-inherit-extended-bans::ban-exception: %d", maximum_inherit_exempt_count);
	sendtxtnumeric(client, "max-inherit-extended-bans::invite-exception: %d", maximum_inherit_invex_count);
	return 1;
}

/** Get the limit for ~inherit entries for the ban type (+b/+e/+I) */
static int maximum_ban_inherit_limit(ExtbanType ban_type)
{
	switch (ban_type)
	{
		case EXBTYPE_BAN:
			return maximum_inherit_ban_count;
		case EXBTYPE_EXCEPT:
			return maximum_inherit_exempt_count;
		case EXBTYPE_INVEX:
			return maximum_inherit_invex_count;
		default:
			return 0;
	}
}

/** Does this channel exceed the maximum number of ~inherit entries? */
static int exceeds_inherit_ban_count(BanContext *b)
{
	Ban *ban;
	int cnt = 0;
	int limit = 0;

	if (!b->channel)
		return 0;

	if (b->ban_type == EXBTYPE_BAN)
		ban = b->channel->banlist;
	else if (b->ban_type == EXBTYPE_EXCEPT)
		ban = b->channel->exlist;
	else if (b->ban_type == EXBTYPE_INVEX)
		ban = b->channel->invexlist;
	else
		return 1; /* Huh? Not +beI. Then we have no idea. Reject it. */

	limit = maximum_ban_inherit_limit(b->ban_type);

	for (; ban; ban = ban->next)
	{
		const char *banstr = ban->banstr;

		/* Pretend time does not exist... */
		if (!strncmp(banstr, "~t:", 3))
		{
			banstr = strchr(banstr+3, ':');
			if (!banstr)
				continue;
			banstr++;
		}
		else if (!strncmp(banstr, "~time:", 6))
		{
			banstr = strchr(banstr+6, ':');
			if (!banstr)
				continue;
			banstr++;
		}

		/* Now check against ~inherit */
		if ((!strncasecmp(banstr, "~inherit:", 9) ||
		     !strncmp(banstr, "~i:", 3)) &&
		     ++cnt >= limit)
		{
			return 1;
		}
	}

	return 0;
}

const char *extban_inherit_conv_param(BanContext *b, Extban *extban)
{
	static char retbuf[CHANNELLEN+1];

	strlcpy(retbuf, b->banstr, sizeof(retbuf));

	if (b->what == MODE_DEL)
		return retbuf;

	if (!valid_channelname(retbuf))
		return NULL;

	if (b->channel && exceeds_inherit_ban_count(b))
		return NULL;

	return retbuf;
}

int extban_inherit_is_ok(BanContext *b)
{
	char retbuf[CHANNELLEN+1];

	if (b->is_ok_check != EXBCHK_PARAM)
		return 1;

	strlcpy(retbuf, b->banstr, sizeof(retbuf));

	if (!valid_channelname(retbuf))
	{
		sendnotice(b->client, "ExtBan ~inherit expects a channel name");
		return 0;
	}

	if ((b->what == MODE_ADD) && b->channel && exceeds_inherit_ban_count(b))
	{
		sendnotice(b->client, "Your ExtBan ~inherit:%s was not accepted because "
		                      "this channel already contains the maximum "
		                      "amount of ~inherit entries (%d).",
		                      b->banstr,
		                      maximum_ban_inherit_limit(b->ban_type));
		return 0;
	}

	return 1;
}

static int extban_inherit_nested = 0;

int extban_inherit_is_banned(BanContext *b)
{
	Channel *channel;
	BanContext *newctx;
	Ban *ret;
	const char *errmsg = NULL;
	int retval;

	if (extban_inherit_nested)
		return 0;

	if (!b->client->user)
		return 0;

	channel = find_channel(b->banstr);
	if (!channel)
		return 0;

	extban_inherit_nested++;
	ret = is_banned(b->client, channel, BANCHK_JOIN, NULL, &errmsg);
	extban_inherit_nested--;

	return ret ? 1 : 0;
}
