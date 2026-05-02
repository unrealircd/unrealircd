/*
 * src/modules/authtoken.c
 * IRCv3 draft/authtoken implementation for ObbyIRCd.
 *
 * Spec: https://github.com/skizzerz/ircv3-specifications/blob/40316fbf/extensions/authtoken.md
 *
 * Provides the TOKEN command (SERVICELIST / GENERATE / VALIDATE),
 * the draft/authtoken capability, the draft/AUTHTOKEN ISUPPORT token,
 * and the draft/authtoken batch type (server- and client-initiated).
 *
 * Tokens are opaque random strings stored in-memory with single-use
 * semantics and a configurable lifetime.  Claims (account, name,
 * member_of, operator_of, scope) are snapshotted at GENERATE time.
 *
 * Config (obbyircd.conf):
 *   authtoken {
 *       token-length 32;
 *       token-lifetime 15m;
 *
 *       service "FILEHOST" {
 *           url "https://upload.example.com";
 *           description "file upload service";
 *           require-account yes;
 *           allow-validate-mask "*@127.0.0.1";
 *       };
 *   };
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER = {
	"authtoken",
	"1.0",
	"draft/authtoken - Authentication tokens for external services",
	"ObbyIRCd Team",
	"unrealircd-6",
};

/* ===================================================================
 * Constants
 * =================================================================== */
#define AT_DEFAULT_TOKEN_LENGTH 32
#define AT_MIN_TOKEN_LENGTH     16
#define AT_MAX_TOKEN_LENGTH     128
#define AT_DEFAULT_LIFETIME     (15L * 60L)
#define AT_MAX_PENDING_TOKEN    16384
#define AT_CLEANUP_INTERVAL_MS  60000

/* ===================================================================
 * Types
 * =================================================================== */

typedef struct AuthMask_
{
	char *mask;
	struct AuthMask_ *next;
} AuthMask;

typedef struct AuthService_
{
	char *key;
	char *url;
	char *description;
	int require_account;
	AuthMask *masks;            /* if NULL => anyone may VALIDATE */
	struct AuthService_ *next;
} AuthService;

typedef struct AuthToken_
{
	char *token;
	char *service_key;
	char *service_url;
	char *account;
	char *nick;
	char *scope;                /* may be NULL */
	char *member_of;            /* space-sep, may be NULL */
	char *operator_of;          /* space-sep, may be NULL */
	time_t expires_at;
	struct AuthToken_ *next;
} AuthToken;

typedef struct TokenPiece_
{
	char *text;
	struct TokenPiece_ *next;
} TokenPiece;

/* In-flight client-initiated draft/authtoken batch (long VALIDATE). */
typedef struct PendingValidate_
{
	Client *client;
	char ref[MAXBATCHREFLEN + 1];
	char *service_key;
	char *service_url;
	TokenPiece *pieces;
	TokenPiece *pieces_tail;
	size_t total_len;
	int overflowed;
	struct PendingValidate_ *next;
} PendingValidate;

/* ===================================================================
 * Module state
 * =================================================================== */

static struct
{
	int token_length;
	long token_lifetime;
	AuthService *services;
} cfg;

static AuthToken *tokens = NULL;
static PendingValidate *pending = NULL;

static long CAP_AUTHTOKEN = 0L;
static long CAP_BATCH_LOCAL = 0L;

/* ===================================================================
 * Forward decls
 * =================================================================== */
static void setconf(void);
static void freeconf(void);
static void free_service(AuthService *s);
static void free_token(AuthToken *t);
static void free_pending(PendingValidate *p);

static AuthService *find_authservice(const char *key);
static AuthToken *find_and_unlink_token(const char *token);
static void store_token(AuthToken *t);
static void cull_expired(void);
static PendingValidate *find_pending(Client *c, const char *ref);
static void unlink_pending(PendingValidate *p);

static int authtoken_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
static int authtoken_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
static int authtoken_welcome(Client *client, int after_numeric);
static int authtoken_local_quit(Client *client, MessageTag *mtags, const char *comment);
static EVENT(authtoken_cleanup_event);

CMD_FUNC(cmd_token);
CMD_OVERRIDE_FUNC(authtoken_override_batch);

static void cmd_token_servicelist(Client *client);
static void cmd_token_generate(Client *client, int parc, const char *parv[]);
static void cmd_token_validate(Client *client, MessageTag *mtags,
                               int parc, const char *parv[]);

static void send_fail(Client *c, const char *code,
                      const char *p1, const char *p2, const char *msg);
static void send_note(Client *c, const char *code, const char *msg);
static int has_required_caps(Client *c);
static int client_matches_masks(Client *c, AuthMask *masks);
static char *collect_member_of(Client *c);
static char *collect_operator_of(Client *c);
static char *make_token_string(void);
static int batch_id(char *buf, size_t bufsize);
static void emit_validate_response(Client *to, AuthToken *tok);

/* ===================================================================
 * Helpers
 * =================================================================== */

static void send_fail(Client *c, const char *code,
                      const char *p1, const char *p2, const char *msg)
{
	if (p1 && p2)
		sendto_one(c, NULL, ":%s FAIL TOKEN %s %s %s :%s",
		           me.name, code, p1, p2, msg);
	else if (p1)
		sendto_one(c, NULL, ":%s FAIL TOKEN %s %s :%s",
		           me.name, code, p1, msg);
	else
		sendto_one(c, NULL, ":%s FAIL TOKEN %s :%s",
		           me.name, code, msg);
}

static void send_note(Client *c, const char *code, const char *msg)
{
	sendto_one(c, NULL, ":%s NOTE TOKEN %s :%s", me.name, code, msg);
}

static AuthService *find_authservice(const char *key)
{
	AuthService *s;
	for (s = cfg.services; s; s = s->next)
		if (!strcasecmp(s->key, key))
			return s;
	return NULL;
}

static int has_required_caps(Client *c)
{
	if (!HasCapabilityFast(c, CAP_BATCH_LOCAL))
	{
		send_fail(c, "NEED_CAPABILITY", "batch", NULL,
		          "You must negotiate the batch capability");
		return 0;
	}
	return 1;
}

static int client_matches_masks(Client *c, AuthMask *masks)
{
	AuthMask *m;
	if (!masks)
		return 1;
	for (m = masks; m; m = m->next)
	{
		if (match_user(m->mask, c, MATCH_CHECK_ALL))
			return 1;
	}
	return 0;
}

static char *make_token_string(void)
{
	int len = cfg.token_length;
	char *out;
	if (len < AT_MIN_TOKEN_LENGTH)
		len = AT_DEFAULT_TOKEN_LENGTH;
	out = safe_alloc(len + 1);
	gen_random_alnum(out, len);
	out[len] = '\0';
	return out;
}

static int batch_id(char *buf, size_t bufsize)
{
	if (bufsize < BATCHLEN + 1)
		return 0;
	gen_random_alnum(buf, BATCHLEN);
	buf[BATCHLEN] = '\0';
	return 1;
}

static int membership_is_elevated(Membership *mb)
{
	const char *m = mb->member_modes;
	while (m && *m)
	{
		if (*m == 'q' || *m == 'a' || *m == 'o' || *m == 'h')
			return 1;
		m++;
	}
	return 0;
}

static char *collect_member_of(Client *c)
{
	Membership *mb;
	size_t total = 1; /* '\0' */
	int count = 0;
	size_t pos = 0;
	char *buf;

	if (!c->user)
		return NULL;
	for (mb = c->user->channel; mb; mb = mb->next)
	{
		total += strlen(mb->channel->name);
		if (count++)
			total++; /* space */
	}
	if (!count)
		return NULL;

	buf = safe_alloc(total);
	for (mb = c->user->channel; mb; mb = mb->next)
	{
		size_t nlen = strlen(mb->channel->name);
		if (pos)
			buf[pos++] = ' ';
		memcpy(buf + pos, mb->channel->name, nlen);
		pos += nlen;
	}
	buf[pos] = '\0';
	return buf;
}

static char *collect_operator_of(Client *c)
{
	Membership *mb;
	size_t total = 1;
	int count = 0;
	size_t pos = 0;
	char *buf;

	if (!c->user)
		return NULL;
	for (mb = c->user->channel; mb; mb = mb->next)
	{
		if (!membership_is_elevated(mb))
			continue;
		total += strlen(mb->channel->name);
		if (count++)
			total++;
	}
	if (!count)
		return NULL;

	buf = safe_alloc(total);
	for (mb = c->user->channel; mb; mb = mb->next)
	{
		size_t nlen;
		if (!membership_is_elevated(mb))
			continue;
		nlen = strlen(mb->channel->name);
		if (pos)
			buf[pos++] = ' ';
		memcpy(buf + pos, mb->channel->name, nlen);
		pos += nlen;
	}
	buf[pos] = '\0';
	return buf;
}

/* ===================================================================
 * Token storage
 * =================================================================== */

static void store_token(AuthToken *t)
{
	t->next = tokens;
	tokens = t;
}

static AuthToken *find_and_unlink_token(const char *token)
{
	AuthToken *t, *prev = NULL;
	time_t now = TStime();
	for (t = tokens; t; prev = t, t = t->next)
	{
		if (t->expires_at >= now && !strcmp(t->token, token))
		{
			if (prev)
				prev->next = t->next;
			else
				tokens = t->next;
			t->next = NULL;
			return t;
		}
	}
	return NULL;
}

static void cull_expired(void)
{
	AuthToken *t = tokens, *prev = NULL, *next;
	time_t now = TStime();
	while (t)
	{
		next = t->next;
		if (t->expires_at < now)
		{
			if (prev)
				prev->next = next;
			else
				tokens = next;
			free_token(t);
		}
		else
		{
			prev = t;
		}
		t = next;
	}
}

/* ===================================================================
 * Pending client batches
 * =================================================================== */

static PendingValidate *find_pending(Client *c, const char *ref)
{
	PendingValidate *p;
	for (p = pending; p; p = p->next)
		if (p->client == c && !strcmp(p->ref, ref))
			return p;
	return NULL;
}

static void unlink_pending(PendingValidate *p)
{
	PendingValidate *cur = pending, *prev = NULL;
	while (cur)
	{
		if (cur == p)
		{
			if (prev)
				prev->next = cur->next;
			else
				pending = cur->next;
			free_pending(cur);
			return;
		}
		prev = cur;
		cur = cur->next;
	}
}

static void purge_pending_for_client(Client *c)
{
	PendingValidate *cur = pending, *prev = NULL, *next;
	while (cur)
	{
		next = cur->next;
		if (cur->client == c)
		{
			if (prev)
				prev->next = next;
			else
				pending = next;
			free_pending(cur);
		}
		else
		{
			prev = cur;
		}
		cur = next;
	}
}

/* ===================================================================
 * SERVICELIST
 * =================================================================== */

static void emit_servicelist_to(Client *to)
{
	char ref[BATCHLEN + 1];
	AuthService *s;

	if (!cfg.services)
	{
		send_note(to, "NO_SERVICES",
		          "No services are defined for this network.");
		return;
	}

	if (!batch_id(ref, sizeof(ref)))
	{
		send_fail(to, "INTERNAL_ERROR", NULL, NULL,
		          "Could not generate batch id");
		return;
	}

	sendto_one(to, NULL, ":%s BATCH +%s draft/authtoken * *", me.name, ref);
	for (s = cfg.services; s; s = s->next)
	{
		sendto_one(to, NULL, "@batch=%s :%s TOKEN SERVICE %s %s :%s",
		           ref, me.name, s->key, s->url,
		           s->description ? s->description : "");
	}
	sendto_one(to, NULL, ":%s BATCH -%s", me.name, ref);
}

static void cmd_token_servicelist(Client *client)
{
	if (!has_required_caps(client))
		return;
	emit_servicelist_to(client);
}

/* ===================================================================
 * GENERATE
 * =================================================================== */

static void cmd_token_generate(Client *client, int parc, const char *parv[])
{
	const char *service_name;
	const char *scope = NULL;
	AuthService *svc;
	AuthToken *t;
	char *tokstr;

	if (!has_required_caps(client))
		return;

	if (!IsUser(client))
	{
		send_fail(client, "NO_PERMISSIONS", NULL, NULL,
		          "You must complete connection registration first");
		return;
	}

	if (parc < 3 || BadPtr(parv[2]))
	{
		send_fail(client, "INVALID_SCOPE", NULL, NULL,
		          "Missing service parameter");
		return;
	}
	service_name = parv[2];

	if (parc >= 4 && !BadPtr(parv[3]))
		scope = parv[3];

	svc = find_authservice(service_name);
	if (!svc)
	{
		send_fail(client, "UNKNOWN_SERVICE", service_name, NULL,
		          "No external service by that name is defined");
		return;
	}

	if (svc->require_account && !IsLoggedIn(client))
	{
		send_fail(client, "ACCOUNT_REQUIRED", NULL, NULL,
		          "You must be logged into an account to generate a token "
		          "for this service");
		return;
	}

	tokstr = make_token_string();
	t = safe_alloc(sizeof(*t));
	t->token = tokstr;
	safe_strdup(t->service_key, svc->key);
	safe_strdup(t->service_url, svc->url);
	if (IsLoggedIn(client))
		safe_strdup(t->account, client->user->account);
	safe_strdup(t->nick, client->name);
	if (scope)
		safe_strdup(t->scope, scope);
	t->member_of = collect_member_of(client);
	t->operator_of = collect_operator_of(client);
	t->expires_at = TStime() + cfg.token_lifetime;
	store_token(t);

	sendto_one(client, NULL, ":%s TOKEN GENERATE %s %s :%s",
	           me.name, svc->key, svc->url, t->token);
}

/* ===================================================================
 * VALIDATE response
 * =================================================================== */

static void emit_validate_response(Client *to, AuthToken *tok)
{
	char ref[BATCHLEN + 1];

	if (!batch_id(ref, sizeof(ref)))
	{
		send_fail(to, "INTERNAL_ERROR", NULL, NULL,
		          "Could not generate batch id");
		return;
	}

	sendto_one(to, NULL, ":%s BATCH +%s draft/authtoken %s %s",
	           me.name, ref, tok->service_key, tok->service_url);

	if (tok->account && *tok->account)
		sendto_one(to, NULL, "@batch=%s :%s TOKEN CLAIM account :%s",
		           ref, me.name, tok->account);
	if (tok->nick && *tok->nick)
		sendto_one(to, NULL, "@batch=%s :%s TOKEN CLAIM name :%s",
		           ref, me.name, tok->nick);
	if (tok->member_of && *tok->member_of)
		sendto_one(to, NULL, "@batch=%s :%s TOKEN CLAIM member_of :%s",
		           ref, me.name, tok->member_of);
	if (tok->operator_of && *tok->operator_of)
		sendto_one(to, NULL, "@batch=%s :%s TOKEN CLAIM operator_of :%s",
		           ref, me.name, tok->operator_of);
	if (tok->scope && *tok->scope)
		sendto_one(to, NULL, "@batch=%s :%s TOKEN CLAIM scope :%s",
		           ref, me.name, tok->scope);

	sendto_one(to, NULL, ":%s BATCH -%s", me.name, ref);
}

static void do_validate(Client *client, const char *service_name,
                        const char *url, const char *token)
{
	AuthService *svc;
	AuthToken *tok;

	svc = find_authservice(service_name);
	if (!svc)
	{
		send_fail(client, "INVALID_TOKEN", NULL, NULL,
		          "The provided token could not be validated");
		return;
	}

	if (!client_matches_masks(client, svc->masks))
	{
		send_fail(client, "NO_PERMISSIONS", svc->key, NULL,
		          "You do not have permission to validate tokens for "
		          "this service");
		return;
	}

	if (strcmp(svc->url, url))
	{
		send_fail(client, "INVALID_TOKEN", NULL, NULL,
		          "The provided token could not be validated");
		return;
	}

	tok = find_and_unlink_token(token);
	if (!tok)
	{
		send_fail(client, "INVALID_TOKEN", NULL, NULL,
		          "The provided token could not be validated");
		return;
	}

	if (strcasecmp(tok->service_key, service_name) ||
	    strcmp(tok->service_url, url))
	{
		/* Token was for a different service.  Drop it (we already
		 * unlinked) and refuse. */
		free_token(tok);
		send_fail(client, "INVALID_TOKEN", NULL, NULL,
		          "The provided token could not be validated");
		return;
	}

	emit_validate_response(client, tok);
	free_token(tok);
}

static void cmd_token_validate(Client *client, MessageTag *mtags,
                               int parc, const char *parv[])
{
	MessageTag *batch_tag;
	const char *service_name;
	const char *url;
	const char *token;

	if (!has_required_caps(client))
		return;

	/* Inside-batch path: TOKEN VALIDATE :piece while a draft/authtoken
	 * client batch is open.  We ignore service/url args (per spec) and
	 * accumulate the token into the pending entry. */
	batch_tag = find_mtag(mtags, "batch");
	if (batch_tag && !BadPtr(batch_tag->value))
	{
		PendingValidate *p = find_pending(client, batch_tag->value);
		if (p)
		{
			const char *piece;
			size_t plen;
			TokenPiece *tp;
			if (parc < 3 || BadPtr(parv[2]))
				return;
			piece = parv[2];
			plen = strlen(piece);
			if (p->overflowed ||
			    p->total_len + plen >= AT_MAX_PENDING_TOKEN)
			{
				p->overflowed = 1;
				return;
			}
			tp = safe_alloc(sizeof(*tp));
			safe_strdup(tp->text, piece);
			if (p->pieces_tail)
				p->pieces_tail->next = tp;
			else
				p->pieces = tp;
			p->pieces_tail = tp;
			p->total_len += plen;
			return;
		}
		/* Fall through if the batch isn't ours. */
	}

	if (parc < 4 || BadPtr(parv[2]) || BadPtr(parv[3]))
	{
		send_fail(client, "INVALID_TOKEN", NULL, NULL,
		          "Missing service, url, or token parameter");
		return;
	}

	service_name = parv[2];
	url = parv[3];
	if (parc < 5 || BadPtr(parv[4]))
	{
		send_fail(client, "INVALID_TOKEN", NULL, NULL,
		          "Missing token parameter");
		return;
	}
	token = parv[4];

	do_validate(client, service_name, url, token);
}

/* ===================================================================
 * TOKEN command dispatcher
 * =================================================================== */

CMD_FUNC(cmd_token)
{
	const char *sub;

	if (parc < 2 || BadPtr(parv[1]))
	{
		send_fail(client, "UNKNOWN_COMMAND", "*", NULL,
		          "Missing TOKEN subcommand");
		return;
	}
	sub = parv[1];

	if (!strcasecmp(sub, "SERVICELIST"))
		cmd_token_servicelist(client);
	else if (!strcasecmp(sub, "GENERATE"))
		cmd_token_generate(client, parc, parv);
	else if (!strcasecmp(sub, "VALIDATE"))
		cmd_token_validate(client, recv_mtags, parc, parv);
	else
		send_fail(client, "UNKNOWN_COMMAND", sub, NULL,
		          "No such TOKEN subcommand");
}

/* ===================================================================
 * BATCH override (claim draft/authtoken client batches)
 * =================================================================== */

CMD_OVERRIDE_FUNC(authtoken_override_batch)
{
	const char *ref;

	if (!MyUser(client))
	{
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}

	if (parc < 2 || BadPtr(parv[1]))
	{
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}

	ref = parv[1];

	if (ref[0] == '+')
	{
		const char *type;

		if (parc < 3 || BadPtr(parv[2]))
		{
			CALL_NEXT_COMMAND_OVERRIDE();
			return;
		}
		type = parv[2];
		if (strcmp(type, "draft/authtoken"))
		{
			CALL_NEXT_COMMAND_OVERRIDE();
			return;
		}

		ref++; /* skip + */
		if (!valid_batch_reference_tag(ref))
		{
			sendto_one(client, NULL,
			           ":%s FAIL BATCH INVALID_REFTAG %s :Invalid "
			           "batch reference tag", me.name, ref);
			return;
		}

		if (!HasCapabilityFast(client, CAP_AUTHTOKEN))
		{
			send_fail(client, "NEED_CAPABILITY", "draft/authtoken",
			          NULL, "You must negotiate the draft/authtoken "
			          "capability before sending a draft/authtoken "
			          "batch");
			return;
		}

		if (!has_required_caps(client))
			return;

		if (parc < 5 || BadPtr(parv[3]) || BadPtr(parv[4]))
		{
			send_fail(client, "INVALID_TOKEN", NULL, NULL,
			          "draft/authtoken batch requires service and "
			          "url parameters");
			return;
		}

		if (find_pending(client, ref))
		{
			send_fail(client, "INTERNAL_ERROR", NULL, NULL,
			          "A draft/authtoken batch with that reference "
			          "is already open");
			return;
		}

		PendingValidate *p = safe_alloc(sizeof(*p));
		p->client = client;
		strlcpy(p->ref, ref, sizeof(p->ref));
		safe_strdup(p->service_key, parv[3]);
		safe_strdup(p->service_url, parv[4]);
		p->next = pending;
		pending = p;
		return;
	}

	if (ref[0] == '-')
	{
		PendingValidate *p;
		char *full;
		TokenPiece *tp;
		size_t pos = 0;

		ref++; /* skip - */
		p = find_pending(client, ref);
		if (!p)
		{
			CALL_NEXT_COMMAND_OVERRIDE();
			return;
		}

		if (p->overflowed || !p->total_len)
		{
			send_fail(client, "INVALID_TOKEN", NULL, NULL,
			          "Token in draft/authtoken batch was empty or "
			          "too long");
			unlink_pending(p);
			return;
		}

		full = safe_alloc(p->total_len + 1);
		for (tp = p->pieces; tp; tp = tp->next)
		{
			size_t l = strlen(tp->text);
			memcpy(full + pos, tp->text, l);
			pos += l;
		}
		full[pos] = '\0';

		do_validate(client, p->service_key, p->service_url, full);
		safe_free(full);
		unlink_pending(p);
		return;
	}

	CALL_NEXT_COMMAND_OVERRIDE();
}

/* ===================================================================
 * Welcome hook (registration burst SERVICELIST)
 * =================================================================== */

static int authtoken_welcome(Client *client, int after_numeric)
{
	if (after_numeric != 5)
		return 0;
	if (!HasCapabilityFast(client, CAP_AUTHTOKEN))
		return 0;
	if (!HasCapabilityFast(client, CAP_BATCH_LOCAL))
		return 0;
	emit_servicelist_to(client);
	return 0;
}

/* ===================================================================
 * Quit hooks (free any pending batches for the departing client)
 * =================================================================== */

static int authtoken_local_quit(Client *client, MessageTag *mtags,
                                const char *comment)
{
	purge_pending_for_client(client);
	return 0;
}

/* ===================================================================
 * Cleanup event
 * =================================================================== */

static EVENT(authtoken_cleanup_event)
{
	cull_expired();
}

/* ===================================================================
 * Config: setconf / freeconf
 * =================================================================== */

static void free_service(AuthService *s)
{
	AuthMask *m, *next;
	if (!s)
		return;
	safe_free(s->key);
	safe_free(s->url);
	safe_free(s->description);
	m = s->masks;
	while (m)
	{
		next = m->next;
		safe_free(m->mask);
		safe_free(m);
		m = next;
	}
	safe_free(s);
}

static void free_token(AuthToken *t)
{
	if (!t)
		return;
	safe_free(t->token);
	safe_free(t->service_key);
	safe_free(t->service_url);
	safe_free(t->account);
	safe_free(t->nick);
	safe_free(t->scope);
	safe_free(t->member_of);
	safe_free(t->operator_of);
	safe_free(t);
}

static void free_pending(PendingValidate *p)
{
	TokenPiece *tp, *next;
	if (!p)
		return;
	safe_free(p->service_key);
	safe_free(p->service_url);
	tp = p->pieces;
	while (tp)
	{
		next = tp->next;
		safe_free(tp->text);
		safe_free(tp);
		tp = next;
	}
	safe_free(p);
}

static void setconf(void)
{
	cfg.token_length = AT_DEFAULT_TOKEN_LENGTH;
	cfg.token_lifetime = AT_DEFAULT_LIFETIME;
	cfg.services = NULL;
}

static void freeconf(void)
{
	AuthService *s = cfg.services, *next;
	while (s)
	{
		next = s->next;
		free_service(s);
		s = next;
	}
	cfg.services = NULL;
}

static void free_all_tokens(void)
{
	AuthToken *t = tokens, *next;
	while (t)
	{
		next = t->next;
		free_token(t);
		t = next;
	}
	tokens = NULL;
}

static void free_all_pending(void)
{
	PendingValidate *p = pending, *next;
	while (p)
	{
		next = p->next;
		free_pending(p);
		p = next;
	}
	pending = NULL;
}

/* ===================================================================
 * Config: configtest
 * =================================================================== */

static int authtoken_configtest_service(ConfigEntry *ce, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;
	int has_url = 0;

	if (BadPtr(ce->value))
	{
		config_error("%s:%i: authtoken::service must have a name "
		             "(e.g. service \"FILEHOST\" { ... };)",
		             ce->file->filename, ce->line_number);
		(*errs)++;
		return -1;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
			continue;
		if (!strcasecmp(cep->name, "url"))
		{
			if (BadPtr(cep->value))
			{
				config_error("%s:%i: authtoken::service::url is empty",
				             cep->file->filename, cep->line_number);
				errors++;
				continue;
			}
			if (strlen(cep->value) > 250)
			{
				config_error("%s:%i: authtoken::service::url exceeds "
				             "250 bytes", cep->file->filename,
				             cep->line_number);
				errors++;
				continue;
			}
			has_url = 1;
		}
		else if (!strcasecmp(cep->name, "description"))
		{
			/* Free-form, optional. */
		}
		else if (!strcasecmp(cep->name, "require-account"))
		{
			/* yes/no validated by config_checkval at run time. */
		}
		else if (!strcasecmp(cep->name, "allow-validate-mask"))
		{
			if (BadPtr(cep->value))
			{
				config_error("%s:%i: authtoken::service::"
				             "allow-validate-mask is empty",
				             cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else
		{
			config_warn("%s:%i: unknown directive authtoken::service::"
			            "%s (ignored)",
			            cep->file->filename, cep->line_number,
			            cep->name);
		}
	}

	if (!has_url)
	{
		config_error("%s:%i: authtoken::service \"%s\" missing url",
		             ce->file->filename, ce->line_number, ce->value);
		errors++;
	}

	*errs += errors;
	return errors ? -1 : 1;
}

static int authtoken_configtest(ConfigFile *cf, ConfigEntry *ce,
                                int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;

	if (type != CONFIG_MAIN)
		return 0;
	if (!ce || !ce->name)
		return 0;
	if (strcmp(ce->name, "authtoken"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
			continue;
		if (!strcasecmp(cep->name, "token-length"))
		{
			int n;
			if (BadPtr(cep->value))
			{
				config_error("%s:%i: authtoken::token-length is empty",
				             cep->file->filename, cep->line_number);
				errors++;
				continue;
			}
			n = atoi(cep->value);
			if (n < AT_MIN_TOKEN_LENGTH || n > AT_MAX_TOKEN_LENGTH)
			{
				config_error("%s:%i: authtoken::token-length must be "
				             "between %d and %d", cep->file->filename,
				             cep->line_number, AT_MIN_TOKEN_LENGTH,
				             AT_MAX_TOKEN_LENGTH);
				errors++;
			}
		}
		else if (!strcasecmp(cep->name, "token-lifetime"))
		{
			if (BadPtr(cep->value))
			{
				config_error("%s:%i: authtoken::token-lifetime is empty",
				             cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcasecmp(cep->name, "service"))
		{
			authtoken_configtest_service(cep, &errors);
		}
		else
		{
			config_warn("%s:%i: unknown directive authtoken::%s",
			            cep->file->filename, cep->line_number,
			            cep->name);
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

/* ===================================================================
 * Config: configrun
 * =================================================================== */

static AuthService *authtoken_configrun_service(ConfigEntry *ce)
{
	ConfigEntry *cep;
	AuthService *s;

	s = safe_alloc(sizeof(*s));
	safe_strdup(s->key, ce->value);
	s->require_account = 1;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
			continue;
		if (!strcasecmp(cep->name, "url"))
			safe_strdup(s->url, cep->value);
		else if (!strcasecmp(cep->name, "description"))
			safe_strdup(s->description, cep->value);
		else if (!strcasecmp(cep->name, "require-account"))
			s->require_account = config_checkval(cep->value, CFG_YESNO);
		else if (!strcasecmp(cep->name, "allow-validate-mask"))
		{
			AuthMask *m = safe_alloc(sizeof(*m));
			safe_strdup(m->mask, cep->value);
			m->next = s->masks;
			s->masks = m;
		}
	}

	return s;
}

static int authtoken_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_MAIN)
		return 0;
	if (!ce || !ce->name)
		return 0;
	if (strcmp(ce->name, "authtoken"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
			continue;
		if (!strcasecmp(cep->name, "token-length"))
			cfg.token_length = atoi(cep->value);
		else if (!strcasecmp(cep->name, "token-lifetime"))
			cfg.token_lifetime = config_checkval(cep->value, CFG_TIME);
		else if (!strcasecmp(cep->name, "service"))
		{
			AuthService *s = authtoken_configrun_service(cep);
			s->next = cfg.services;
			cfg.services = s;
		}
	}

	return 1;
}

/* ===================================================================
 * Module entry points
 * =================================================================== */

MOD_TEST()
{
	setconf();
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, authtoken_configtest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ClientCapabilityInfo cap;

	memset(&cap, 0, sizeof(cap));
	cap.name = "draft/authtoken";
	ClientCapabilityAdd(modinfo->handle, &cap, &CAP_AUTHTOKEN);

	CommandAdd(modinfo->handle, "TOKEN", cmd_token, MAXPARA,
	           CMD_USER | CMD_UNREGISTERED);
	CommandOverrideAdd(modinfo->handle, "BATCH", 0,
	                   authtoken_override_batch);

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, authtoken_configrun);
	HookAdd(modinfo->handle, HOOKTYPE_WELCOME, 0, authtoken_welcome);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, authtoken_local_quit);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	/* draft/AUTHTOKEN ISUPPORT (no value per spec) */
	ISupportAdd(modinfo->handle, "draft/AUTHTOKEN", NULL);

	/* Resolve the batch capability bit so we can quickly check it. */
	{
		ClientCapability *bc = ClientCapabilityFind("batch", NULL);
		if (bc)
			CAP_BATCH_LOCAL = bc->cap;
	}

	EventAdd(modinfo->handle, "authtoken_cleanup",
	         authtoken_cleanup_event, NULL,
	         AT_CLEANUP_INTERVAL_MS, 0);

	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	free_all_tokens();
	free_all_pending();
	freeconf();
	return MOD_SUCCESS;
}
