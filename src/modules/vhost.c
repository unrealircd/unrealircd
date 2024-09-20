/*
 *   Unreal Internet Relay Chat Daemon, src/modules/vhost.c
 *   (C) 2000-2024 Carsten V. Munk, Bram Matthys and the UnrealIRCd Team
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

/* Defines */
#define MSG_VHOST       "VHOST"

/* Structs */
ModuleHeader MOD_HEADER
  = {
	"vhost",
	"6.0",
	"command /VHOST and vhost { } blocks",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

typedef struct ConfigItem_vhost ConfigItem_vhost;
struct ConfigItem_vhost {
	ConfigItem_vhost *prev, *next;
	int auto_login;			/**< Auto-login users on connect? If they match 'auth' */
	SecurityGroup *match;           /**< Match criteria for user */
	char *login;                    /**< Login name for 'VHOST login pass' */
	AuthConfig *auth;		/**< Password for 'VHOST login pass */
	char *virtuser;			/**< Virtual ident to set */
	char *virthost;                 /**< Virtual host to set */
	SWhois *swhois;			/**< SWhois items to set */
};

/* Variables */
ConfigItem_vhost *conf_vhost = NULL;

/* Forward declarations */
void free_vhost_config(void);
int vhost_config_test(ConfigFile *conf, ConfigEntry *ce, int type, int *errs);
int vhost_config_run(ConfigFile *conf, ConfigEntry *ce, int type);
static int stats_vhost(Client *client, const char *flag);
ConfigItem_vhost *find_vhost(const char *name);
void do_vhost(Client *client, ConfigItem_vhost *vhost);
CMD_FUNC(cmd_vhost);
int vhost_auto_set(Client *client);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, vhost_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, vhost_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, stats_vhost);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, INT_MAX, vhost_auto_set);
	CommandAdd(modinfo->handle, MSG_VHOST, cmd_vhost, MAXPARA, CMD_USER);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}


MOD_UNLOAD()
{
	free_vhost_config();
	return MOD_SUCCESS;
}

void free_vhost_config(void)
{
	ConfigItem_vhost *e, *e_next;

	for (e = conf_vhost; e; e = e_next)
	{
		SWhois *s, *s_next;

		e_next = e->next;

		safe_free(e->login);
		Auth_FreeAuthConfig(e->auth);
		safe_free(e->virthost);
		safe_free(e->virtuser);
		free_security_group(e->match);
		for (s = e->swhois; s; s = s_next)
		{
			s_next = s->next;
			safe_free(s->line);
			safe_free(s->setby);
			safe_free(s);
		}
		DelListItem(e, conf_vhost);
		safe_free(e);
	}
	conf_vhost = NULL;
}
/** Test a vhost { } block in the configuration file */
int vhost_config_test(ConfigFile *conf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	char has_vhost = 0, has_login = 0, has_password = 0, has_mask = 0, has_match = 0;
	char has_auto_login = 0;
	int errors = 0;

	/* We are only interested in vhost { } blocks */
	if ((type != CONFIG_MAIN) || strcmp(ce->name, "vhost"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "auto-login"))
		{
			has_auto_login = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "vhost"))
		{
			char *at, *tmp, *host, *p;
			if (has_vhost)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "vhost::vhost");
				continue;
			}
			has_vhost = 1;
			if (!cep->value)
			{
				config_error_empty(cep->file->filename,
					cep->line_number, "vhost", "vhost");
				errors++;
				continue;
			}
			if (!potentially_valid_vhost(cep->value))
			{
				/* Note that the error message needs to be on the
				 * original cep->value and not on fakehost.
				 */
				config_error("%s:%i: vhost::vhost contains illegal characters or is too long: '%s'",
					     cep->file->filename, cep->line_number, cep->value);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "login"))
		{
			if (has_login)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "vhost::login");
			}
			has_login = 1;
			if (!cep->value)
			{
				config_error_empty(cep->file->filename,
					cep->line_number, "vhost", "login");
				errors++;
				continue;
			}
		}
		else if (!strcmp(cep->name, "password"))
		{
			if (has_password)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "vhost::password");
			}
			has_password = 1;
			if (!cep->value)
			{
				config_error_empty(cep->file->filename,
					cep->line_number, "vhost", "password");
				errors++;
				continue;
			}
			if (Auth_CheckError(cep, 0) < 0)
				errors++;
		}
		else if (!strcmp(cep->name, "mask"))
		{
			has_mask = 1;
			test_match_block(conf, cep, &errors);
		}
		else if (!strcmp(cep->name, "match"))
		{
			has_match = 1;
			test_match_block(conf, cep, &errors);
		}
		else if (!strcmp(cep->name, "swhois"))
		{
			/* multiple is ok */
		}
		else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"vhost", cep->name);
			errors++;
		}
	}

	if (!has_vhost)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"vhost::vhost");
		errors++;
	}

	if (!has_auto_login)
	{
		if (!has_login)
		{
			config_error_missing(ce->file->filename, ce->line_number,
				"vhost::login");
			errors++;

		}
		if (!has_password)
		{
			config_error_missing(ce->file->filename, ce->line_number,
				"vhost::password");
			errors++;
		}
	}

	if (!has_mask && !has_match)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"vhost::match");
		errors++;
	}
	if (has_mask && has_match)
	{
		config_error("%s:%d: You cannot have both ::mask and ::match. "
		             "You should only use %s::match.",
		             ce->file->filename, ce->line_number, ce->name);
		errors++;
	}

	if (has_auto_login && has_password)
	{
		config_error("%s:%d: If ::auto-login is set to 'yes' then you "
		             "cannot have a ::password configured. "
		             "Remove the password if you want to use auto-login.",
		             ce->file->filename, ce->line_number);
	}
	*errs = errors;
	return errors ? -1 : 1;
}

/** Process a vhost { } block in the configuration file */
int vhost_config_run(ConfigFile *conf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cepp;
	ConfigItem_vhost *vhost;

	/* We are only interested in vhost { } blocks */
	if ((type != CONFIG_MAIN) || strcmp(ce->name, "vhost"))
		return 0;

	vhost = safe_alloc(sizeof(ConfigItem_vhost));
	vhost->match = safe_alloc(sizeof(SecurityGroup));

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "auto-login"))
		{
			vhost->auto_login = config_checkval(cep->value, CFG_YESNO);
		}
		else if (!strcmp(cep->name, "vhost"))
		{
			char *user, *host;
			user = strtok(cep->value, "@");
			host = strtok(NULL, "");
			if (!host)
				safe_strdup(vhost->virthost, user);
			else
			{
				safe_strdup(vhost->virtuser, user);
				safe_strdup(vhost->virthost, host);
			}
		}
		else if (!strcmp(cep->name, "login"))
			safe_strdup(vhost->login, cep->value);
		else if (!strcmp(cep->name, "password"))
			vhost->auth = AuthBlockToAuthConfig(cep);
		else if (!strcmp(cep->name, "match") || !strcmp(cep->name, "mask"))
		{
			conf_match_block(conf, cep, &vhost->match);
		}
		else if (!strcmp(cep->name, "swhois"))
		{
			SWhois *s;
			if (cep->items)
			{
				for (cepp = cep->items; cepp; cepp = cepp->next)
				{
					s = safe_alloc(sizeof(SWhois));
					safe_strdup(s->line, cepp->name);
					safe_strdup(s->setby, "vhost");
					AddListItem(s, vhost->swhois);
				}
			} else
			if (cep->value)
			{
				s = safe_alloc(sizeof(SWhois));
				safe_strdup(s->line, cep->value);
				safe_strdup(s->setby, "vhost");
				AddListItem(s, vhost->swhois);
			}
		}
	}
	AppendListItem(vhost, conf_vhost);
	return 1;
}

static int stats_vhost(Client *client, const char *flag)
{
	ConfigItem_vhost *vhosts;
	NameValuePrioList *m;

	if (strcmp(flag, "S") && strcasecmp(flag, "vhost"))
		return 0; /* Not for us */

	for (vhosts = conf_vhost; vhosts; vhosts = vhosts->next)
	{
		for (m = vhosts->match->printable_list; m; m = m->next)
		{
			sendtxtnumeric(client, "vhost %s%s%s %s %s",
			               vhosts->virtuser ? vhosts->virtuser : "",
			               vhosts->virtuser ? "@" : "",
			               vhosts->virthost,
			               vhosts->login,
			               namevalue_nospaces(m));
		}
	}

	return 1;
}



ConfigItem_vhost *find_vhost(const char *name)
{
	ConfigItem_vhost *vhost;

	for (vhost = conf_vhost; vhost; vhost = vhost->next)
	{
		if (!strcmp(name, vhost->login))
			return vhost;
	}

	return NULL;
}

CMD_FUNC(cmd_vhost)
{
	ConfigItem_vhost *vhost;
	char login[HOSTLEN+1];
	const char *password;

	if (!MyUser(client))
		return;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "VHOST");
		return;

	}

	/* cut-off too long login names. HOSTLEN is arbitrary, we just don't want our
	 * error messages to be cut off because the user is sending huge login names.
	 */
	strlcpy(login, parv[1], sizeof(login));

	password = (parc > 2) ? parv[2] : "";

	if (!(vhost = find_vhost(login)))
	{
		unreal_log(ULOG_WARNING, "vhost", "VHOST_FAILED", client,
		           "Failed VHOST attempt by $client.details [reason: $reason] [vhost-block: $vhost_block]",
		           log_data_string("reason", "Vhost block not found"),
		           log_data_string("fail_type", "UNKNOWN_VHOST_NAME"),
		           log_data_string("vhost_block", login));
		sendnotice(client, "*** [\2vhost\2] Login for %s failed - password incorrect", login);
		return;
	}

	if (!user_allowed_by_security_group(client, vhost->match))
	{
		unreal_log(ULOG_WARNING, "vhost", "VHOST_FAILED", client,
		           "Failed VHOST attempt by $client.details [reason: $reason] [vhost-block: $vhost_block]",
		           log_data_string("reason", "Host does not match"),
		           log_data_string("fail_type", "NO_HOST_MATCH"),
		           log_data_string("vhost_block", login));
		sendnotice(client, "*** No vHost lines available for your host");
		return;
	}

	if (!Auth_Check(client, vhost->auth, password))
	{
		unreal_log(ULOG_WARNING, "vhost", "VHOST_FAILED", client,
		           "Failed VHOST attempt by $client.details [reason: $reason] [vhost-block: $vhost_block]",
		           log_data_string("reason", "Authentication failed"),
		           log_data_string("fail_type", "AUTHENTICATION_FAILED"),
		           log_data_string("vhost_block", login));
		sendnotice(client, "*** [\2vhost\2] Login for %s failed - password incorrect", login);
		return;
	}

	/* Authentication passed, but.. there could still be other restrictions: */
	switch (UHOST_ALLOWED)
	{
		case UHALLOW_NEVER:
			if (MyUser(client))
			{
				sendnotice(client, "*** /vhost is disabled");
				return;
			}
			break;
		case UHALLOW_ALWAYS:
			break;
		case UHALLOW_NOCHANS:
			if (MyUser(client) && client->user->joined)
			{
				sendnotice(client, "*** /vhost can not be used while you are on a channel");
				return;
			}
			break;
		case UHALLOW_REJOIN:
			/* join sent later when the host has been changed */
			break;
	}

	/* All checks passed, now let's go ahead and change the host */
	do_vhost(client, vhost);
}

void do_vhost(Client *client, ConfigItem_vhost *vhost)
{
	char olduser[USERLEN+1];
	char newhost[HOSTLEN+1];

	/* There are various IsUser() checks in the code below, that is because
	 * this code is also called for CLIENT_STATUS_UNKNOWN users in the handshake
	 * that have not yet been introduced to other servers. For such users we
	 * should not send SETIDENT and SETHOST messages out... such info will
	 * be sent in the UID message when the user is introduced.
	 */

	*newhost = '\0';
	unreal_expand_string(vhost->virthost, newhost, sizeof(newhost), NULL, 0, client);
	if (!valid_vhost(newhost))
	{
		sendnotice(client, "*** Unable to apply vhost automatically");
		if (vhost->auto_login)
		{
			unreal_log(ULOG_WARNING, "vhost", "AUTO_VHOST_FAILED", client,
			           "Unable to set auto-vhost on user $client.details. "
			           "Vhost '$vhost_format' expanded to '$newhost' but is invalid.",
			           log_data_string("vhost_format", vhost->virthost),
			           log_data_string("newhost", newhost));
		}
		return;
	}

	userhost_save_current(client);

	safe_strdup(client->user->virthost, newhost);
	if (vhost->virtuser)
	{
		strlcpy(olduser, client->user->username, sizeof(olduser));
		strlcpy(client->user->username, vhost->virtuser, sizeof(client->user->username));
		if (IsUser(client))
			sendto_server(client, 0, 0, NULL, ":%s SETIDENT %s", client->id, client->user->username);
	}
	client->umodes |= UMODE_HIDE;
	client->umodes |= UMODE_SETHOST;
	if (IsUser(client))
	{
		sendto_server(client, 0, 0, NULL, ":%s SETHOST %s", client->id, client->user->virthost);
		sendto_one(client, NULL, ":%s MODE %s :+tx", client->name, client->name);
	}
	if (vhost->swhois)
	{
		SWhois *s;
		for (s = vhost->swhois; s; s = s->next)
			swhois_add(client, "vhost", -100, s->line, &me, NULL);
	}
	if (IsUser(client))
	{
		sendnotice(client, "*** Your vhost is now %s%s%s",
			vhost->virtuser ? vhost->virtuser : "",
			vhost->virtuser ? "@" : "",
			newhost);
	}

	/* Only notify on logins, not on auto logins (should we make that configurable?)
	 * (if you do want it for auto logins, note that vhost->login will be NULL
	 *  in the unreal_log() call currently below).
	 */
	if (vhost->login)
	{
		if (vhost->virtuser)
		{
			/* virtuser@virthost */
			unreal_log(ULOG_INFO, "vhost", "VHOST_SUCCESS", client,
				   "$client.details is now using vhost $virtuser@$virthost [vhost-block: $vhost_block]",
				   log_data_string("virtuser", vhost->virtuser),
				   log_data_string("virthost", newhost),
				   log_data_string("vhost_block", vhost->login));
		} else {
			/* just virthost */
			unreal_log(ULOG_INFO, "vhost", "VHOST_SUCCESS", client,
				   "$client.details is now using vhost $virthost [vhost-block: $vhost_block]",
				   log_data_string("virthost", newhost),
				   log_data_string("vhost_block", vhost->login));
		}
	}

	userhost_changed(client);
}

int vhost_auto_set(Client *client)
{
	ConfigItem_vhost *vhost;

	if (IsSetHost(client))
		return 0; /* Don't override if e.g. anope already set a vhost */

	for (vhost = conf_vhost; vhost; vhost = vhost->next)
	{
		if (vhost->auto_login &&
		    !vhost->auth &&
		    vhost->match &&
		    user_allowed_by_security_group(client, vhost->match))
		{
			do_vhost(client, vhost);
			return 0;
		}
	}
	return 0;
}
