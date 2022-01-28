/*
 *   IRC - Internet Relay Chat, src/modules/dccdeny.c
 *   (C) 2004-2019 The UnrealIRCd Team
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

ModuleHeader MOD_HEADER
  = {
	"dccdeny",
	"6.0.2",
	"command /dccdeny", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Variables */
ConfigItem_deny_dcc     *conf_deny_dcc = NULL;
ConfigItem_allow_dcc    *conf_allow_dcc = NULL;

/* Forward declarions */
int dccdeny_configtest_deny_dcc(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int dccdeny_configtest_allow_dcc(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int dccdeny_configrun_deny_dcc(ConfigFile *cf, ConfigEntry *ce, int type);
int dccdeny_configrun_allow_dcc(ConfigFile *cf, ConfigEntry *ce, int type);
int dccdeny_stats(Client *client, const char *para);
int dccdeny_dcc_denied(Client *client, const char *target, const char *realfile, const char *displayfile, ConfigItem_deny_dcc *dccdeny);
CMD_FUNC(cmd_dccdeny);
CMD_FUNC(cmd_undccdeny);
CMD_FUNC(cmd_svsfline);
int dccdeny_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype);
int dccdeny_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);
int dccdeny_server_sync(Client *client);
static ConfigItem_deny_dcc *dcc_isforbidden(Client *client, const char *filename);
static ConfigItem_deny_dcc *dcc_isdiscouraged(Client *client, const char *filename);
static void DCCdeny_add(const char *filename, const char *reason, int type, int type2);
static void DCCdeny_del(ConfigItem_deny_dcc *deny);
static void dcc_wipe_services(void);
static const char *get_dcc_filename(const char *text);
static int can_dcc(Client *client, const char *target, Client *targetcli, const char *filename, const char **errmsg);
static int can_dcc_soft(Client *from, Client *to, const char *filename, const char **errmsg);
static void free_dcc_config_blocks(void);
void dccdeny_unload_free_all_conf_deny_dcc(ModData *m);
void dccdeny_unload_free_all_conf_allow_dcc(ModData *m);
ConfigItem_deny_dcc *find_deny_dcc(const char *name);

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, dccdeny_configtest_deny_dcc);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, dccdeny_configtest_allow_dcc);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	LoadPersistentPointer(modinfo, conf_deny_dcc, dccdeny_unload_free_all_conf_deny_dcc);
	LoadPersistentPointer(modinfo, conf_allow_dcc, dccdeny_unload_free_all_conf_allow_dcc);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, dccdeny_configrun_deny_dcc);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, dccdeny_configrun_allow_dcc);
	CommandAdd(modinfo->handle, "DCCDENY", cmd_dccdeny, 2, CMD_USER);
	CommandAdd(modinfo->handle, "UNDCCDENY", cmd_undccdeny, MAXPARA, CMD_USER);
	CommandAdd(modinfo->handle, "SVSFLINE", cmd_svsfline, MAXPARA, CMD_SERVER);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, dccdeny_stats);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_USER, 0, dccdeny_can_send_to_user);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, dccdeny_can_send_to_channel);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_SYNC, 0, dccdeny_server_sync);
	HookAdd(modinfo->handle, HOOKTYPE_DCC_DENIED, 0, dccdeny_dcc_denied);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	free_dcc_config_blocks();
	SavePersistentPointer(modinfo, conf_deny_dcc);
	SavePersistentPointer(modinfo, conf_allow_dcc);

	return MOD_SUCCESS;
}

int dccdeny_configtest_deny_dcc(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;
	char has_filename = 0, has_reason = 0, has_soft = 0;

	/* We are only interested in deny dcc { } */
	if ((type != CONFIG_DENY) || strcmp(ce->value, "dcc"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (config_is_blankorempty(cep, "deny dcc"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->name, "filename"))
		{
			if (has_filename)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "deny dcc::filename");
				continue;
			}
			has_filename = 1;
		}
		else if (!strcmp(cep->name, "reason"))
		{
			if (has_reason)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "deny dcc::reason");
				continue;
			}
			has_reason = 1;
		}
		else if (!strcmp(cep->name, "soft"))
		{
			if (has_soft)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "deny dcc::soft");
				continue;
			}
			has_soft = 1;
		}
		else
		{
			config_error_unknown(cep->file->filename,
				cep->line_number, "deny dcc", cep->name);
			errors++;
		}
	}
	if (!has_filename)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"deny dcc::filename");
		errors++;
	}
	if (!has_reason)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"deny dcc::reason");
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int dccdeny_configtest_allow_dcc(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0, has_filename = 0, has_soft = 0;

	/* We are only interested in allow dcc { } */
	if ((type != CONFIG_ALLOW) || strcmp(ce->value, "dcc"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (config_is_blankorempty(cep, "allow dcc"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->name, "filename"))
		{
			if (has_filename)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow dcc::filename");
				continue;
			}
			has_filename = 1;
		}
		else if (!strcmp(cep->name, "soft"))
		{
			if (has_soft)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "allow dcc::soft");
				continue;
			}
			has_soft = 1;
		}
		else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"allow dcc", cep->name);
			errors++;
		}
	}
	if (!has_filename)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"allow dcc::filename");
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int dccdeny_configrun_deny_dcc(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigItem_deny_dcc 	*deny = NULL;
	ConfigEntry 	    	*cep;

	/* We are only interested in deny dcc { } */
	if ((type != CONFIG_DENY) || strcmp(ce->value, "dcc"))
		return 0;

	deny = safe_alloc(sizeof(ConfigItem_deny_dcc));
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "filename"))
		{
			safe_strdup(deny->filename, cep->value);
		}
		else if (!strcmp(cep->name, "reason"))
		{
			safe_strdup(deny->reason, cep->value);
		}
		else if (!strcmp(cep->name, "soft"))
		{
			int x = config_checkval(cep->value,CFG_YESNO);
			if (x == 1)
				deny->flag.type = DCCDENY_SOFT;
		}
	}
	if (!deny->reason)
	{
		if (deny->flag.type == DCCDENY_HARD)
			safe_strdup(deny->reason, "Possible infected virus file");
		else
			safe_strdup(deny->reason, "Possible executable content");
	}
	AddListItem(deny, conf_deny_dcc);
	return 0;
}

int dccdeny_configrun_allow_dcc(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigItem_allow_dcc *allow = NULL;
	ConfigEntry *cep;

	/* We are only interested in allow dcc { } */
	if ((type != CONFIG_ALLOW) || strcmp(ce->value, "dcc"))
		return 0;

	allow = safe_alloc(sizeof(ConfigItem_allow_dcc));

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "filename"))
			safe_strdup(allow->filename, cep->value);
		else if (!strcmp(cep->name, "soft"))
		{
			int x = config_checkval(cep->value,CFG_YESNO);
			if (x)
				allow->flag.type = DCCDENY_SOFT;
		}
	}
	AddListItem(allow, conf_allow_dcc);
	return 1;
}

/** Free allow dcc { } and deny dcc { } blocks, called on REHASH */
void free_dcc_config_blocks(void)
{
	ConfigItem_deny_dcc *d, *d_next;
	ConfigItem_allow_dcc *a, *a_next;

	for (d = conf_deny_dcc; d; d = d_next)
	{
		d_next = d->next;
		if (d->flag.type2 == CONF_BAN_TYPE_CONF)
		{
			safe_free(d->filename);
			safe_free(d->reason);
			DelListItem(d, conf_deny_dcc);
			safe_free(d);
		}
	}
	for (a = conf_allow_dcc; a; a = a_next)
	{
		a_next = a->next;
		if (a->flag.type2 == CONF_BAN_TYPE_CONF)
		{
			safe_free(a->filename);
			DelListItem(a, conf_allow_dcc);
			safe_free(a);
		}
	}
}

/** Free all deny dcc { } blocks - only called on permanent module unload (rare) */
void dccdeny_unload_free_all_conf_deny_dcc(ModData *m)
{
	ConfigItem_deny_dcc *d, *d_next;

	for (d = conf_deny_dcc; d; d = d_next)
	{
		d_next = d->next;
		safe_free(d->filename);
		safe_free(d->reason);
		DelListItem(d, conf_deny_dcc);
		safe_free(d);
	}
	conf_deny_dcc = NULL;
}

/** Free all allow dcc { } blocks - only called on permanent module unload (rare) */
void dccdeny_unload_free_all_conf_allow_dcc(ModData *m)
{
	ConfigItem_allow_dcc *a, *a_next;

	for (a = conf_allow_dcc; a; a = a_next)
	{
		a_next = a->next;
		safe_free(a->filename);
		DelListItem(a, conf_allow_dcc);
		safe_free(a);
	}
	conf_allow_dcc = NULL;
}


/* Add a temporary dccdeny line
 *
 * parv[1] - file
 * parv[2] - reason
 */
CMD_FUNC(cmd_dccdeny)
{
	if (!MyUser(client))
		return;

	if (!ValidatePermissionsForPath("server-ban:dccdeny",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc < 2) || BadPtr(parv[2]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "DCCDENY");
		return;
	}

	if (!find_deny_dcc(parv[1]))
	{
		unreal_log(ULOG_INFO, "dccdeny", "DCCDENY_ADD", client,
		           "[dccdeny] $client added a temporary DCCDENY for $file ($reason)",
		           log_data_string("file", parv[1]),
		           log_data_string("reason", parv[2]));
		DCCdeny_add(parv[1], parv[2], DCCDENY_HARD, CONF_BAN_TYPE_TEMPORARY);
		return;
	} else
	{
		sendnotice(client, "*** %s already has a dccdeny", parv[1]);
	}
}

/* Remove a temporary dccdeny line
 * parv[1] - file/mask
 */
CMD_FUNC(cmd_undccdeny)
{
	ConfigItem_deny_dcc *d;

	if (!MyUser(client))
		return;

	if (!ValidatePermissionsForPath("server-ban:dccdeny",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "UNDCCDENY");
		return;
	}

	if ((d = find_deny_dcc(parv[1])) && d->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
	{
		unreal_log(ULOG_INFO, "dccdeny", "DCCDENY_DEL", client,
		           "[dccdeny] $client removed a temporary DCCDENY for $file ($reason)",
		           log_data_string("file", d->filename),
		           log_data_string("reason", d->reason));
		DCCdeny_del(d);
		return;
	} else
	{
		sendnotice(client, "*** Unable to find a temp dccdeny matching %s", parv[1]);
	}
}

CMD_FUNC(cmd_svsfline)
{
	if (parc < 2)
		return;

	switch (*parv[1])
	{
		/* Allow non-U-Lines to send ONLY SVSFLINE +, but don't send it out
		 * unless it is from a U-Line -- codemastr
		 */
		case '+':
		{
			if (parc < 4)
				return;

			if (!find_deny_dcc(parv[2]))
				DCCdeny_add(parv[2], parv[3], DCCDENY_HARD, CONF_BAN_TYPE_AKILL);

			if (IsULine(client))
			{
				sendto_server(client, 0, 0, NULL, ":%s SVSFLINE + %s :%s",
				    client->id, parv[2], parv[3]);
			}

			break;
		}

		case '-':
		{
			ConfigItem_deny_dcc *deny;

			if (!IsULine(client))
				return;

			if (parc < 3)
				return;

			if (!(deny = find_deny_dcc(parv[2])))
				break;

			DCCdeny_del(deny);

			sendto_server(client, 0, 0, NULL, ":%s SVSFLINE %s", client->id, parv[2]);

			break;
		}

		case '*':
		{
			if (!IsULine(client))
				return;

			dcc_wipe_services();

			sendto_server(client, 0, 0, NULL, ":%s SVSFLINE *", client->id);

			break;
		}
	}
}

/** Sync the DCC entries on server connect */
int dccdeny_server_sync(Client *client)
{
	ConfigItem_deny_dcc *p;
	for (p = conf_deny_dcc; p; p = p->next)
	{
		if (p->flag.type2 == CONF_BAN_TYPE_AKILL)
			sendto_one(client, NULL, ":%s SVSFLINE + %s :%s", me.id,
			    p->filename, p->reason);
	}
	return 0;
}

/** Check if a DCC should be blocked (user-to-user) */
int dccdeny_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype)
{
	if (**text == '\001')
	{
		const char *filename = get_dcc_filename(*text);
		if (filename)
		{
			if (MyUser(client) && !can_dcc(client, target->name, target, filename, errmsg))
				return HOOK_DENY;
			if (MyUser(target) && !can_dcc_soft(client, target, filename, errmsg))
				return HOOK_DENY;
		}
	}

	return HOOK_CONTINUE;
}

/** Check if a DCC should be blocked (user-to-channel, unusual) */
int dccdeny_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	static char errbuf[512];

	if (MyUser(client) && (**msg == '\001'))
	{
		const char *err = NULL;
		const char *filename = get_dcc_filename(*msg);
		if (filename && !can_dcc(client, channel->name, NULL, filename, &err))
		{
			strlcpy(errbuf, err, sizeof(errbuf));
			*errmsg = errbuf;
			return HOOK_DENY;
		}
	}
	return HOOK_CONTINUE;
}

/* e->flag.type2:
 * CONF_BAN_TYPE_AKILL		banned by SVSFLINE ('global')
 * CONF_BAN_TYPE_CONF		banned by conf
 * CONF_BAN_TYPE_TEMPORARY	banned by /DCCDENY ('temporary')
 * e->flag.type:
 * DCCDENY_HARD				Fully denied
 * DCCDENY_SOFT				Denied, but allowed to override via /DCCALLOW
 */

/** Make a viewable dcc filename.
 * This is to protect a bit against tricks like 'flood-it-off-the-buffer'
 * and color 1,1 etc...
 */
static const char *dcc_displayfile(const char *f)
{
	static char buf[512];
	const char *i;
	char *o = buf;
	size_t n = strlen(f);

	if (n < 300)
	{
		for (i = f; *i; i++)
			if (*i < 32)
				*o++ = '?';
			else
				*o++ = *i;
		*o = '\0';
		return buf;
	}

	/* Else, we show it as: [first 256 chars]+"[..TRUNCATED..]"+[last 20 chars] */
	for (i = f; i < f+256; i++)
		if (*i < 32)
			*o++ = '?';
		else
			*o++ = *i;
	strcpy(o, "[..TRUNCATED..]");
	o += sizeof("[..TRUNCATED..]");
	for (i = f+n-20; *i; i++)
		if (*i < 32)
			*o++ = '?';
		else
			*o++ = *i;
	*o = '\0';
	return buf;
}

static const char *get_dcc_filename(const char *text)
{
	static char filename[BUFSIZE+1];
	char *end;
	int size_string;

	if (*text != '\001')
		return 0;

	if (!strncasecmp(text+1, "DCC SEND ", 9))
		text = text + 10;
	else if (!strncasecmp(text+1, "DCC RESUME ", 11))
		text = text + 12;
	else
		return 0;

	for (; *text == ' '; text++); /* skip leading spaces */

	if (*text == '"' && *(text+1))
		end = strchr(text+1, '"');
	else
		end = strchr(text, ' ');

	if (!end || (end < text))
		return 0;

	size_string = (int)(end - text);

	if (!size_string || (size_string > (BUFSIZE - 1)))
		return 0;

	strlcpy(filename, text, size_string+1);
	return filename;
}

/** Checks if a DCC SEND is allowed.
 * @param client      Sending client
 * @param target      Target name (user or channel)
 * @param targetcli   Target client (NULL in case of channel!)
 * @param text        The entire message
 * @returns 1 if DCC SEND allowed, 0 if rejected
 */
static int can_dcc(Client *client, const char *target, Client *targetcli, const char *filename, const char **errmsg)
{
	ConfigItem_deny_dcc *fl;
	static char errbuf[256];
	int size_string, ret;

	/* User (IRCOp) may bypass send restrictions */
	if (ValidatePermissionsForPath("immune:dcc",client,targetcli,NULL,NULL))
		return 1;

	/* User (IRCOp) likes to receive bad dcc's */
	if (targetcli && ValidatePermissionsForPath("self:getbaddcc",targetcli,NULL,NULL,NULL))
		return 1;

	/* Check if user is already blocked (from the past) */
	if (IsDCCBlock(client))
	{
		*errmsg = "*** You are blocked from sending files as you have tried to "
		          "send a forbidden file - reconnect to regain ability to send";
		return 0;
	}

	if (match_spamfilter(client, filename, SPAMF_DCC, "PRIVMSG", target, 0, NULL))
	{
		/* Dirty hack, yeah spamfilter already sent the error message :( */
		*errmsg = "";
		return 0;
	}

	if ((fl = dcc_isforbidden(client, filename)))
	{
		const char *displayfile = dcc_displayfile(filename);

		RunHook(HOOKTYPE_DCC_DENIED, client, target, filename, displayfile, fl);

		ircsnprintf(errbuf, sizeof(errbuf), "Cannot DCC SEND file: %s", fl->reason);
		*errmsg = errbuf;
		SetDCCBlock(client);
		return 0;
	}

	/* Channel dcc (???) and discouraged? just block */
	if (!targetcli && ((fl = dcc_isdiscouraged(client, filename))))
	{
		ircsnprintf(errbuf, sizeof(errbuf), "Cannot DCC SEND file: %s", fl->reason);
		*errmsg = errbuf;
		return 0;
	}

	/* If we get here, the file is allowed */
	return 1;
}

/** Checks if a DCC is allowed by DCCALLOW rules (only SOFT bans are checked).
 * PARAMETERS:
 * from:		the sender client (possibly remote)
 * to:			the target client (always local)
 * text:		the whole msg
 * RETURNS:
 * 1:			allowed
 * 0:			block
 */
static int can_dcc_soft(Client *from, Client *to, const char *filename, const char **errmsg)
{
	ConfigItem_deny_dcc *fl;
	const char *displayfile;
	static char errbuf[256];

	/* User (IRCOp) may bypass send restrictions */
	if (ValidatePermissionsForPath("immune:dcc",from,to,NULL,NULL))
		return 1;

	/* User (IRCOp) likes to receive bad dcc's */
	if (ValidatePermissionsForPath("self:getbaddcc",to,NULL,NULL,NULL))
		return 1;

	/* On the 'soft' blocklist ? */
	if (!(fl = dcc_isdiscouraged(from, filename)))
		return 1; /* No, so is OK */

	/* If on DCCALLOW list then the user is OK with it */
	if (on_dccallow_list(to, from))
		return 1;

	/* Soft-blocked */
	displayfile = dcc_displayfile(filename);

	ircsnprintf(errbuf, sizeof(errbuf), "Cannot DCC SEND file: %s", fl->reason);
	*errmsg = errbuf;

	/* Inform target ('to') about the /DCCALLOW functionality */
	sendnotice(to, "%s (%s@%s) tried to DCC SEND you a file named '%s', the request has been blocked.",
		from->name, from->user->username, GetHost(from), displayfile);
	if (!IsDCCNotice(to))
	{
		SetDCCNotice(to);
		sendnotice(to, "Files like these might contain malicious content (viruses, trojans). "
			"Therefore, you must explicitly allow anyone that tries to send you such files.");
		sendnotice(to, "If you trust %s, and want him/her to send you this file, you may obtain "
			"more information on using the dccallow system by typing '/DCCALLOW HELP'", from->name);
	}
	return 0;
}

/** Checks if the dcc is blacklisted. */
static ConfigItem_deny_dcc *dcc_isforbidden(Client *client, const char *filename)
{
	ConfigItem_deny_dcc *d;
	ConfigItem_allow_dcc *a;

	if (!conf_deny_dcc || !filename)
		return NULL;

	for (d = conf_deny_dcc; d; d = d->next)
	{
		if ((d->flag.type == DCCDENY_HARD) && match_simple(d->filename, filename))
		{
			for (a = conf_allow_dcc; a; a = a->next)
			{
				if ((a->flag.type == DCCDENY_HARD) && match_simple(a->filename, filename))
					return NULL;
			}
			return d;
		}
	}

	return NULL;
}

/** checks if the dcc is discouraged ('soft bans'). */
static ConfigItem_deny_dcc *dcc_isdiscouraged(Client *client, const char *filename)
{
	ConfigItem_deny_dcc *d;
	ConfigItem_allow_dcc *a;

	if (!conf_deny_dcc || !filename)
		return NULL;

	for (d = conf_deny_dcc; d; d = d->next)
	{
		if ((d->flag.type == DCCDENY_SOFT) && match_simple(d->filename, filename))
		{
			for (a = conf_allow_dcc; a; a = a->next)
			{
				if ((a->flag.type == DCCDENY_SOFT) && match_simple(a->filename, filename))
					return NULL;
			}
			return d;
		}
	}

	return NULL;
}

static void DCCdeny_add(const char *filename, const char *reason, int type, int type2)
{
	ConfigItem_deny_dcc *deny = NULL;

	deny = safe_alloc(sizeof(ConfigItem_deny_dcc));
	safe_strdup(deny->filename, filename);
	safe_strdup(deny->reason, reason);
	deny->flag.type = type;
	deny->flag.type2 = type2;
	AddListItem(deny, conf_deny_dcc);
}

static void DCCdeny_del(ConfigItem_deny_dcc *deny)
{
	DelListItem(deny, conf_deny_dcc);
	safe_free(deny->filename);
	safe_free(deny->reason);
	safe_free(deny);
}

ConfigItem_deny_dcc *find_deny_dcc(const char *name)
{
	ConfigItem_deny_dcc	*e;

	if (!name)
		return NULL;

	for (e = conf_deny_dcc; e; e = e->next)
	{
		if (match_simple(name, e->filename))
			return e;
	}
	return NULL;
}

static void dcc_wipe_services(void)
{
	ConfigItem_deny_dcc *dconf, *next;

	for (dconf = conf_deny_dcc; dconf; dconf = next)
	{
		next = dconf->next;
		if (dconf->flag.type2 == CONF_BAN_TYPE_AKILL)
		{
			DelListItem(dconf, conf_deny_dcc);
			safe_free(dconf->filename);
			safe_free(dconf->reason);
			safe_free(dconf);
		}
	}

}

int dccdeny_stats(Client *client, const char *para)
{
	ConfigItem_deny_dcc *denytmp;
	ConfigItem_allow_dcc *allowtmp;
	char *filemask, *reason;
	char a = 0;

	/* '/STATS F' or '/STATS denydcc' is for us... */
	if (strcmp(para, "F") && strcasecmp(para, "denydcc"))
		return 0;

	for (denytmp = conf_deny_dcc; denytmp; denytmp = denytmp->next)
	{
		filemask = BadPtr(denytmp->filename) ? "<NULL>" : denytmp->filename;
		reason = BadPtr(denytmp->reason) ? "<NULL>" : denytmp->reason;
		if (denytmp->flag.type2 == CONF_BAN_TYPE_CONF)
			a = 'c';
		if (denytmp->flag.type2 == CONF_BAN_TYPE_AKILL)
			a = 's';
		if (denytmp->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
			a = 'o';
		/* <d> <s|h> <howadded> <filemask> <reason> */
		sendtxtnumeric(client, "d %c %c %s %s", (denytmp->flag.type == DCCDENY_SOFT) ? 's' : 'h',
			a, filemask, reason);
	}
	for (allowtmp = conf_allow_dcc; allowtmp; allowtmp = allowtmp->next)
	{
		filemask = BadPtr(allowtmp->filename) ? "<NULL>" : allowtmp->filename;
		if (allowtmp->flag.type2 == CONF_BAN_TYPE_CONF)
			a = 'c';
		if (allowtmp->flag.type2 == CONF_BAN_TYPE_AKILL)
			a = 's';
		if (allowtmp->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
			a = 'o';
		/* <a> <s|h> <howadded> <filemask> */
		sendtxtnumeric(client, "a %c %c %s", (allowtmp->flag.type == DCCDENY_SOFT) ? 's' : 'h',
			a, filemask);
	}
	return 1;
}

int dccdeny_dcc_denied(Client *client, const char *target, const char *realfile, const char *displayfile, ConfigItem_deny_dcc *dccdeny)
{
	unreal_log(ULOG_INFO, "dcc", "DCC_REJECTED", client,
	           "$client.details tried to send forbidden file $filename ($ban_reason) to $target (is blocked now)",
	           log_data_string("filename", displayfile),
	           log_data_string("ban_reason", dccdeny->reason),
	           log_data_string("target", target));
	return 0;
}
