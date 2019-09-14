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
	"unrealircd-5",
};

typedef struct MultiLine MultiLine;
struct MultiLine {
	MultiLine *prev, *next;
	char *line;
};

/** Configuration settings */
struct {
	int enabled;
	MultiLine *message;
	MultiLine *fail_message;
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
int authprompt_require_sasl(Client *acptr, char *reason);
int authprompt_sasl_continuation(Client *acptr, char *buf);
int authprompt_sasl_result(Client *acptr, int success);
int authprompt_place_host_ban(Client *sptr, int action, char *reason, long duration);
int authprompt_find_tkline_match(Client *sptr, TKL *tk);
int authprompt_pre_connect(Client *sptr);
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
	HookAdd(modinfo->handle, HOOKTYPE_REQUIRE_SASL, 0, authprompt_require_sasl);
	HookAdd(modinfo->handle, HOOKTYPE_SASL_CONTINUATION, 0, authprompt_sasl_continuation);
	HookAdd(modinfo->handle, HOOKTYPE_SASL_RESULT, 0, authprompt_sasl_result);
	HookAdd(modinfo->handle, HOOKTYPE_PLACE_HOST_BAN, 0, authprompt_place_host_ban);
	HookAdd(modinfo->handle, HOOKTYPE_FIND_TKLINE_MATCH, 0, authprompt_find_tkline_match);
	/* For HOOKTYPE_PRE_LOCAL_CONNECT we want a low priority, so we are called last.
	 * This gives hooks like the one from the blacklist module (pending softban)
	 * a chance to be handled first.
	 */
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, -1000000, authprompt_pre_connect);
	CommandAdd(modinfo->handle, "AUTH", cmd_auth, 1, M_UNREGISTERED);
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
	cfg.enabled = 0;
}

static void addmultiline(MultiLine **l, char *line)
{
	MultiLine *m = safe_alloc(sizeof(MultiLine));
	safe_strdup(m->line, line);
	append_ListItem((ListStruct *)m, (ListStruct **)l);
}

static void freemultiline(MultiLine *l)
{
	MultiLine *l_next;
	for (; l; l = l_next)
	{
		l_next = l->next;
		safe_free(l->line);
		safe_free(l);
	}
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
}

static void free_config(void)
{
	freemultiline(cfg.message);
	freemultiline(cfg.fail_message);
	memset(&cfg, 0, sizeof(cfg)); /* needed! */
}

int authprompt_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::authentication-prompt... */
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "authentication-prompt"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: set::authentication-prompt::%s with no value",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
		} else
		if (!strcmp(cep->ce_varname, "enabled"))
		{
		} else
		if (!strcmp(cep->ce_varname, "message"))
		{
		} else
		if (!strcmp(cep->ce_varname, "fail-message"))
		{
		} else
		{
			config_error("%s:%i: unknown directive set::authentication-prompt::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
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
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "authentication-prompt"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "enabled"))
		{
			cfg.enabled = config_checkval(cep->ce_vardata, CFG_YESNO);
		} else
		if (!strcmp(cep->ce_varname, "message"))
		{
			addmultiline(&cfg.message, cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "fail-message"))
		{
			addmultiline(&cfg.fail_message, cep->ce_vardata);
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
 * @notes The returned 'username' and 'password' are valid until next call to parse_nickpass().
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

/* NOTE: This function is stolen from cmd_sasl. Not good. */
static const char *encode_puid(Client *client)
{
	static char buf[HOSTLEN + 20];

	/* create a cookie if necessary (and in case getrandom16 returns 0, then run again) */
	while (!client->local->sasl_cookie)
		client->local->sasl_cookie = getrandom16();

	snprintf(buf, sizeof buf, "%s!0.%d", me.name, client->local->sasl_cookie);

	return buf;
}

char *make_authbuf(const char *username, const char *password)
{
	char inbuf[256];
	static char outbuf[512];
	int size;

	size = strlen(username) + 1 + strlen(username) + 1 + strlen(password);
	if (size >= sizeof(inbuf))
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
void send_first_auth(Client *sptr)
{
	Client *acptr;
	char *addr = BadPtr(sptr->ip) ? "0" : sptr->ip;
	char *certfp = moddata_client_get(sptr, "certfp");
	acptr = find_client(SASL_SERVER, NULL);
	if (!acptr)
	{
		/* Services down. */
		return;
	}

	sendto_one(acptr, NULL, ":%s SASL %s %s H %s %s",
	    me.name, SASL_SERVER, encode_puid(sptr), addr, addr);

	if (certfp)
		sendto_one(acptr, NULL, ":%s SASL %s %s S %s %s",
		    me.name, SASL_SERVER, encode_puid(sptr), "PLAIN", certfp);
	else
		sendto_one(acptr, NULL, ":%s SASL %s %s S %s",
		    me.name, SASL_SERVER, encode_puid(sptr), "PLAIN");

	/* The rest is sent from authprompt_sasl_continuation() */

	sptr->local->sasl_out++;
}

CMD_FUNC(cmd_auth)
{
	char *username = NULL;
	char *password = NULL;
	char *authbuf;

	if (!SEUSER(sptr))
	{
		if (HasCapability(sptr, "sasl"))
			sendnotice(sptr, "ERROR: Cannot use /AUTH when your client is doing SASL.");
		else
			sendnotice(sptr, "ERROR: /AUTH authentication request received before authentication prompt (too early!)");
		return 0;
	}

	if ((parc < 2) || BadPtr(parv[1]) || !parse_nickpass(parv[1], &username, &password))
	{
		sendnotice(sptr, "ERROR: Syntax is: /AUTH <nickname>:<password>");
		sendnotice(sptr, "Example: /AUTH mynick:secretpass");
		return 0;
	}

	if (!SASL_SERVER)
	{
		sendnotice(sptr, "ERROR: SASL is not configured on this server, or services are down.");
		// numeric instead? SERVICESDOWN?
		return 0;
	}

	/* Presumably if the user is really fast, this could happen.. */
	if (*sptr->local->sasl_agent || SEUSER(sptr)->authmsg)
	{
		sendnotice(sptr, "ERROR: Previous authentication request is still in progress. Please wait.");
		return 0;
	}

	authbuf = make_authbuf(username, password);
	if (!authbuf)
	{
		sendnotice(sptr, "ERROR: Internal error. Oversized username/password?");
		return 0;
	}

	safe_strdup(SEUSER(sptr)->authmsg, authbuf);

	send_first_auth(sptr);

	return 0;
}

void send_multinotice(Client *sptr, MultiLine *m)
{
	for (; m; m = m->next)
		sendnotice(sptr, "%s", m->line);
}

void authprompt_tag_as_auth_required(Client *sptr)
{
	/* Allocate, and therefore indicate, that we are going to handle SASL for this user */
	if (!SEUSER(sptr))
		SetAPUser(sptr, safe_alloc(sizeof(APUser)));
}

void authprompt_send_auth_required_message(Client *sptr)
{
	/* Display set::authentication-prompt::message */
	send_multinotice(sptr, cfg.message);
}

int authprompt_require_sasl(Client *sptr, char *reason)
{
	/* If the client did SASL then we (authprompt) will not kick in */
	if (HasCapability(sptr, "sasl"))
		return 0;

	authprompt_tag_as_auth_required(sptr);

	/* Display the require authentication::reason */
	if (reason && strcmp(reason, "-") && strcmp(reason, "*"))
		sendnotice(sptr, "%s", reason);

	authprompt_send_auth_required_message(sptr);

	return 1;
}

/* Called upon "place a host ban on this user" (eg: spamfilter, blacklist, ..) */
int authprompt_place_host_ban(Client *sptr, int action, char *reason, long duration)
{
	/* If it's a soft-xx action and the user is not logged in
	 * and the user is not yet online, then we will handle this user.
	 */
	if (IsSoftBanAction(action) && !IsLoggedIn(sptr) && !IsUser(sptr))
	{
		/* Send ban reason */
		if (reason)
			sendnotice(sptr, "%s", reason);

		/* And tag the user */
		authprompt_tag_as_auth_required(sptr);
		return 0; /* pretend user is exempt */
	}
	return 99; /* no action taken, proceed normally */
}

/** Called upon "check for KLINE/GLINE" */
int authprompt_find_tkline_match(Client *sptr, TKL *tkl)
{
	/* If it's a soft-xx action and the user is not logged in
	 * and the user is not yet online, then we will handle this user.
	 */
	if (TKLIsServerBan(tkl) &&
	   (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT) &&
	   !IsLoggedIn(sptr) &&
	   !IsUser(sptr))
	{
		/* Send ban reason */
		if (tkl->ptr.serverban->reason)
			sendnotice(sptr, "%s", tkl->ptr.serverban->reason);

		/* And tag the user */
		authprompt_tag_as_auth_required(sptr);
		return 0; /* pretend user is exempt */
	}
	return 99; /* no action taken, proceed normally */
}

int authprompt_pre_connect(Client *sptr)
{
	/* If the user is tagged as auth required and not logged in, then.. */
	if (SEUSER(sptr) && !IsLoggedIn(sptr))
	{
		authprompt_send_auth_required_message(sptr);
		return -1; /* do not process register_user() */
	}

	return 0; /* no action taken, proceed normally */
}

int authprompt_sasl_continuation(Client *sptr, char *buf)
{
	/* If it's not for us (eg: user is doing real SASL) then return 0. */
	if (!SEUSER(sptr) || !SEUSER(sptr)->authmsg)
		return 0;

	if (!strcmp(buf, "+"))
	{
		Client *agent = find_client(sptr->local->sasl_agent, NULL);
		if (agent)
		{
			sendto_one(agent, NULL, ":%s SASL %s %s C %s",
				me.name, AGENT_SID(agent), encode_puid(sptr), SEUSER(sptr)->authmsg);
		}
		SEUSER(sptr)->authmsg = NULL;
	}
	return 1; /* inhibit displaying of message */
}

int authprompt_sasl_result(Client *sptr, int success)
{
	/* If it's not for us (eg: user is doing real SASL) then return 0. */
	if (!SEUSER(sptr))
		return 0;

	if (!success)
	{
		send_multinotice(sptr, cfg.fail_message);
		return 1;
	}

	/* Authentication was a success */
	if (*sptr->name && sptr->user && *sptr->user->username && IsNotSpoof(sptr))
	{
		register_user(sptr, sptr, sptr->name, sptr->user->username, NULL, NULL, NULL);
		/* NOTE: register_user() may return FLUSH_BUFFER here, but since the caller
		 * won't continue processing (won't touch 'sptr') it's safe.
		 * That is, as long as we 'return 1'.
		 */
	}

	return 1; /* inhibit success/failure message */
}
