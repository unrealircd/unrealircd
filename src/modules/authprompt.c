/*
 * Auth prompt: SASL authentication for clients that don't support SASL
 * (C) Copyright 2018 Bram Matthys ("Syzop") and the UnrealIRCd team
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
	"authprompt",
	"1.0",
	"SASL authentication for clients that don't support SASL",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/** Configuration settings */
struct {
	int enabled;
	MultiLine *message;
	MultiLine *fail_message;
	MultiLine *unconfirmed_message;
} cfg;

/** User struct */
typedef struct APUser APUser;
struct APUser {
	char *authmsg;
};

/* Global variables */
ModDataInfo *authprompt_md = NULL;

/* Forward declarations */
static void free_config(void);
static void init_config(void);
static void config_postdefaults(void);
int authprompt_config_test(ConfigFile *, ConfigEntry *, int, int *);
int authprompt_config_run(ConfigFile *, ConfigEntry *, int);
int authprompt_sasl_continuation(Client *client, const char *buf);
int authprompt_sasl_result(Client *client, int success);
int authprompt_place_host_ban(Client *client, int action, const char *reason, long duration);
int authprompt_find_tkline_match(Client *client, TKL *tk);
int authprompt_pre_connect(Client *client);
CMD_FUNC(cmd_auth);
void authprompt_md_free(ModData *md);

/* Some macros */
#define SetAPUser(x, y) do { moddata_client(x, authprompt_md).ptr = y; } while(0)
#define SEUSER(x)       ((APUser *)moddata_client(x, authprompt_md).ptr)
#define AGENT_SID(agent_p)      (agent_p->user != NULL ? agent_p->user->server : agent_p->name)

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, authprompt_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "authprompt";
	mreq.type = MODDATATYPE_CLIENT;
	mreq.free = authprompt_md_free;
	authprompt_md = ModDataAdd(modinfo->handle, mreq);
	if (!authprompt_md)
	{
		config_error("could not register authprompt moddata");
		return MOD_FAILED;
	}

	init_config();
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, authprompt_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_SASL_CONTINUATION, 0, authprompt_sasl_continuation);
	HookAdd(modinfo->handle, HOOKTYPE_SASL_RESULT, 0, authprompt_sasl_result);
	HookAdd(modinfo->handle, HOOKTYPE_PLACE_HOST_BAN, 0, authprompt_place_host_ban);
	HookAdd(modinfo->handle, HOOKTYPE_FIND_TKLINE_MATCH, 0, authprompt_find_tkline_match);
	/* For HOOKTYPE_PRE_LOCAL_CONNECT we want a low priority, so we are called last.
	 * This gives hooks like the one from the blacklist module (pending softban)
	 * a chance to be handled first.
	 */
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, -1000000, authprompt_pre_connect);
	CommandAdd(modinfo->handle, "AUTH", cmd_auth, 1, CMD_UNREGISTERED);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	config_postdefaults();
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	free_config();
	return MOD_SUCCESS;
}

static void init_config(void)
{
	/* This sets some default values */
	memset(&cfg, 0, sizeof(cfg));
	cfg.enabled = 1;
}

static void config_postdefaults(void)
{
	if (!cfg.message)
	{
		addmultiline(&cfg.message, "The server requires clients from this IP address to authenticate with a registered nickname and password.");
		addmultiline(&cfg.message, "Please reconnect using SASL, or authenticate now by typing: /QUOTE AUTH nick:password");
	}
	if (!cfg.fail_message)
	{
		addmultiline(&cfg.fail_message, "Authentication failed.");
	}
	if (!cfg.unconfirmed_message)
	{
		addmultiline(&cfg.unconfirmed_message, "You are trying to use an unconfirmed services account.");
		addmultiline(&cfg.unconfirmed_message, "This services account can only be used after it has been activated/confirmed.");
	}
}

static void free_config(void)
{
	freemultiline(cfg.message);
	freemultiline(cfg.fail_message);
	freemultiline(cfg.unconfirmed_message);
	memset(&cfg, 0, sizeof(cfg)); /* needed! */
}

int authprompt_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::authentication-prompt... */
	if (!ce || !ce->name || strcmp(ce->name, "authentication-prompt"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->value)
		{
			config_error("%s:%i: set::authentication-prompt::%s with no value",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		} else
		if (!strcmp(cep->name, "enabled"))
		{
		} else
		if (!strcmp(cep->name, "message"))
		{
		} else
		if (!strcmp(cep->name, "fail-message"))
		{
		} else
		if (!strcmp(cep->name, "unconfirmed-message"))
		{
		} else
		{
			config_error("%s:%i: unknown directive set::authentication-prompt::%s",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		}
	}
	*errs = errors;
	return errors ? -1 : 1;
}

int authprompt_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::authentication-prompt... */
	if (!ce || !ce->name || strcmp(ce->name, "authentication-prompt"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "enabled"))
		{
			cfg.enabled = config_checkval(cep->value, CFG_YESNO);
		} else
		if (!strcmp(cep->name, "message"))
		{
			addmultiline(&cfg.message, cep->value);
		} else
		if (!strcmp(cep->name, "fail-message"))
		{
			addmultiline(&cfg.fail_message, cep->value);
		} else
		if (!strcmp(cep->name, "unconfirmed-message"))
		{
			addmultiline(&cfg.unconfirmed_message, cep->value);
		}
	}
	return 1;
}

void authprompt_md_free(ModData *md)
{
	APUser *se = md->ptr;

	if (se)
	{
		safe_free(se->authmsg);
		safe_free(se);
		md->ptr = se = NULL;
	}
}

/** Parse an authentication request from the user (form: <user>:<pass>).
 * @param str      The input string with the request.
 * @param username Pointer to the username string.
 * @param password Pointer to the password string.
 * @retval 1 if the format is correct, 0 if not.
 * @note The returned 'username' and 'password' are valid until next call to parse_nickpass().
 */
int parse_nickpass(const char *str, char **username, char **password)
{
	static char buf[250];
	char *p;

	strlcpy(buf, str, sizeof(buf));

	p = strchr(buf, ':');
	if (!p)
		return 0;

	*p++ = '\0';
	*username = buf;
	*password = p;

	if (!*username[0] || !*password[0])
		return 0;

	return 1;
}

char *make_authbuf(const char *username, const char *password)
{
	char inbuf[256];
	static char outbuf[512];
	int size;

	size = strlen(username) + 1 + strlen(username) + 1 + strlen(password);
	if (size >= sizeof(inbuf)-1)
		return NULL; /* too long */

	/* Because size limits are already checked above, we can cut some corners here: */
	memset(inbuf, 0, sizeof(inbuf));
	strcpy(inbuf, username);
	strcpy(inbuf+strlen(username)+1, username);
	strcpy(inbuf+strlen(username)+1+strlen(username)+1, password);
	/* ^ normal people use stpcpy here ;) */

	if (b64_encode(inbuf, size, outbuf, sizeof(outbuf)) < 0)
		return NULL; /* base64 encoding error */

	return outbuf;
}

/** Send first SASL authentication request (AUTHENTICATE PLAIN).
 * Among other things, this is used to discover the agent
 * which will later be used for this session.
 */
void send_first_auth(Client *client)
{
	Client *sasl_server;
	char *addr = BadPtr(client->ip) ? "0" : client->ip;
	const char *certfp = moddata_client_get(client, "certfp");
	sasl_server = find_client(SASL_SERVER, NULL);
	if (!sasl_server)
	{
		/* Services down. */
		return;
	}

	sendto_one(sasl_server, NULL, ":%s SASL %s %s H %s %s",
	    me.id, SASL_SERVER, client->id, addr, addr);

	if (certfp)
		sendto_one(sasl_server, NULL, ":%s SASL %s %s S %s %s",
		    me.id, SASL_SERVER, client->id, "PLAIN", certfp);
	else
		sendto_one(sasl_server, NULL, ":%s SASL %s %s S %s",
		    me.id, SASL_SERVER, client->id, "PLAIN");

	/* The rest is sent from authprompt_sasl_continuation() */

	client->local->sasl_out++;
}

CMD_FUNC(cmd_auth)
{
	char *username = NULL;
	char *password = NULL;
	char *authbuf;

	if (!SEUSER(client))
	{
		if (HasCapability(client, "sasl"))
			sendnotice(client, "ERROR: Cannot use /AUTH when your client is doing SASL.");
		else
			sendnotice(client, "ERROR: /AUTH authentication request received before authentication prompt (too early!)");
		return;
	}

	if ((parc < 2) || BadPtr(parv[1]) || !parse_nickpass(parv[1], &username, &password))
	{
		sendnotice(client, "ERROR: Syntax is: /AUTH <nickname>:<password>");
		sendnotice(client, "Example: /AUTH mynick:secretpass");
		return;
	}

	if (!SASL_SERVER)
	{
		sendnotice(client, "ERROR: SASL is not configured on this server, or services are down.");
		// numeric instead? SERVICESDOWN?
		return;
	}

	/* Presumably if the user is really fast, this could happen.. */
	if (*client->local->sasl_agent || SEUSER(client)->authmsg)
	{
		sendnotice(client, "ERROR: Previous authentication request is still in progress. Please wait.");
		return;
	}

	authbuf = make_authbuf(username, password);
	if (!authbuf)
	{
		sendnotice(client, "ERROR: Internal error. Oversized username/password?");
		return;
	}

	safe_strdup(SEUSER(client)->authmsg, authbuf);

	send_first_auth(client);
}

void authprompt_tag_as_auth_required(Client *client)
{
	/* Allocate, and therefore indicate, that we are going to handle SASL for this user */
	if (!SEUSER(client))
		SetAPUser(client, safe_alloc(sizeof(APUser)));
}

void authprompt_send_auth_required_message(Client *client)
{
	/* Display set::authentication-prompt::message */
	sendnotice_multiline(client, cfg.message);
}

/* Called upon "place a host ban on this user" (eg: spamfilter, blacklist, ..) */
int authprompt_place_host_ban(Client *client, int action, const char *reason, long duration)
{
	/* If it's a soft-xx action and the user is not logged in
	 * and the user is not yet online, then we will handle this user.
	 */
	if (IsSoftBanAction(action) && !IsLoggedIn(client) && !IsUser(client))
	{
		/* Send ban reason */
		if (reason)
			sendnotice(client, "%s", reason);

		/* And tag the user */
		authprompt_tag_as_auth_required(client);
		authprompt_send_auth_required_message(client);
		return 1; /* pretend user is killed */
	}
	return 99; /* no action taken, proceed normally */
}

/** Called upon "check for KLINE/GLINE" */
int authprompt_find_tkline_match(Client *client, TKL *tkl)
{
	/* If it's a soft-xx action and the user is not logged in
	 * and the user is not yet online, then we will handle this user.
	 */
	if (TKLIsServerBan(tkl) &&
	   (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) &&
	   !IsLoggedIn(client) &&
	   !IsUser(client))
	{
		/* Send ban reason */
		if (tkl->ptr.serverban->reason)
			sendnotice(client, "%s", tkl->ptr.serverban->reason);

		/* And tag the user */
		authprompt_tag_as_auth_required(client);
		authprompt_send_auth_required_message(client);
		return 1; /* pretend user is killed */
	}
	return 99; /* no action taken, proceed normally */
}

int authprompt_pre_connect(Client *client)
{
	/* If the user is tagged as auth required and not logged in, then.. */
	if (SEUSER(client) && !IsLoggedIn(client) && cgf.enabled)
	{
		authprompt_send_auth_required_message(client);
		return HOOK_DENY; /* do not process register_user() */
	}

	return HOOK_CONTINUE; /* no action taken, proceed normally */
}

int authprompt_sasl_continuation(Client *client, const char *buf)
{
	/* If it's not for us (eg: user is doing real SASL) then return 0. */
	if (!SEUSER(client) || !SEUSER(client)->authmsg)
		return 0;

	if (!strcmp(buf, "+"))
	{
		Client *agent = find_client(client->local->sasl_agent, NULL);
		if (agent)
		{
			sendto_one(agent, NULL, ":%s SASL %s %s C %s",
				me.id, AGENT_SID(agent), client->id, SEUSER(client)->authmsg);
		}
		safe_free(SEUSER(client)->authmsg);
	}
	return 1; /* inhibit displaying of message */
}

int authprompt_sasl_result(Client *client, int success)
{
	/* If it's not for us (eg: user is doing real SASL) then return 0. */
	if (!SEUSER(client))
		return 0;

	if (!success)
	{
		sendnotice_multiline(client, cfg.fail_message);
		return 1;
	}

	if (client->user && !IsLoggedIn(client))
	{
		sendnotice_multiline(client, cfg.unconfirmed_message);
		return 1;
	}

	/* Authentication was a success */
	if (*client->name && client->user && *client->user->username && IsNotSpoof(client))
	{
		register_user(client);
		/* User MAY be killed now. But since we 'return 1' below, it's safe */
	}

	return 1; /* inhibit success/failure message */
}
