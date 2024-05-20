/*
 * Unreal Internet Relay Chat Daemon, src/modules/crule.c
 * crule parser and checker.
 * (C) Tony Vencill (Tonto on IRC) <vencill@bga.com>
 * (C) 2023- Bram Matthys and the UnrealIRCd Team
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
	"crule",
	"1.0.1",
	"Crule support for and deny link::rule and spamfilter::rule",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Originally by Tony Vencill (Tonto on IRC) <vencill@bga.com>
 *
 * The majority of this file is a recursive descent parser used to convert
 * connection rules into expression trees when the conf file is read.
 * All parsing structures and types are hidden in the interest of good
 * programming style and to make possible future data structure changes
 * without affecting the interface between this patch and the rest of the
 * server.  The only functions accessible externally are crule_parse,
 * crule_free, and crule_eval.  Prototypes for these functions can be
 * found in h.h.
 *
 * The production rules for the grammar are as follows ("rule" is the
 * starting production):
 *
 *   rule:
 *     orexpr END          END is end of input or :
 *   orexpr:
 *     andexpr
 *     andexpr || orexpr
 *   andexpr:
 *     primary
 *     primary && andexpr
 *  primary:
 *    function
 *    ! primary
 *    ( orexpr )
 *  function:
 *    word ( )             word is alphanumeric string, first character
 *    word ( arglist )       must be a letter
 *  arglist:
 *    word
 *    word , arglist
 */

/* Last update of parser functions taken from ircu on 2023-03-19
 * matching ircu's ircd/crule.c from 2021-09-04.
 * Then ported / UnrealIRCd-ized by Syzop and re-adding crule_test()
 * and such. All the actual "functions" like crule_connected() are
 * our own and not re-feteched (but were based on older versions).
 */

/** Input scanner tokens. */
typedef enum crule_token crule_token;
enum crule_token {
	CR_UNKNOWN,    /**< Unknown token type. */
	CR_END,        /**< End of input ('\\0' or ':'). */
	CR_AND,        /**< Logical and operator (&&). */
	CR_OR,         /**< Logical or operator (||). */
	CR_NOT,        /**< Logical not operator (!). */
	CR_OPENPAREN,  /**< Open parenthesis. */
	CR_CLOSEPAREN, /**< Close parenthesis. */
	CR_COMMA,      /**< Comma. */
	CR_EQUAL,       /**< Operator == */
	CR_LESS_THAN,  /**< Operator < */
	CR_MORE_THAN,  /**< Operator > */
	CR_WORD        /**< Something that looks like a hostmask (alphanumerics, "*?.-"). */
};

#define IsComparisson(x)	((x == CR_EQUAL) || (x == CR_LESS_THAN) || (x == CR_MORE_THAN))

/** Parser error codes. */
typedef enum crule_errcode crule_errcode;
enum crule_errcode {
	CR_NOERR,      /**< No error. */
	CR_UNEXPCTTOK, /**< Invalid token given context. */
	CR_UNKNWTOK,   /**< Input did not form a valid token. */
	CR_EXPCTAND,   /**< Did not see expected && operator. */
	CR_EXPCTOR,    /**< Did not see expected || operator. */
	CR_EXPCTPRIM,  /**< Expected a primitive (parentheses, ! or word). */
	CR_EXPCTOPEN,  /**< Expected an open parenthesis after function name. */
	CR_EXPCTCLOSE, /**< Expected a close parenthesis to match open parenthesis. */
	CR_UNKNWFUNC,  /**< Attempt to use an unknown function. */
	CR_ARGMISMAT,  /**< Wrong number of arguments to function. */
	CR_EXPCTVALUE, /**< Missing value in a comparisson */
};

/* NOTE: Expression tree structure, function pointer, and tree pointer local! */

int _crule_test(const char *rule);
CRuleNode *_crule_parse(const char *rule);
void _crule_free(CRuleNode**);
int _crule_eval(crule_context *context, CRuleNode *rule);
const char *_crule_errstring(int errcode);

/* rule function prototypes - local! */
static int crule_connected(crule_context *, int, void **);
static int crule_directcon(crule_context *, int, void **);
static int crule_via(crule_context *, int, void **);
static int crule_directop(crule_context *, int, void **);
static int crule__andor(crule_context *, int, void **);
static int crule__not(crule_context *, int, void **);
// newstyle
static int crule_online_time(crule_context *, int, void **);
static int crule_reputation(crule_context *, int, void **);
static int crule_tag(crule_context *, int, void **);
static int crule_in_channel(crule_context *, int, void **);
static int crule_destination(crule_context *, int, void **);
static int crule_cap_version(crule_context *, int, void **);
static int crule_cap_set(crule_context *, int, void **);
static int crule_has_user_mode(crule_context *, int, void **);
static int crule_has_channel_mode(crule_context *, int, void **);
static int crule_away(crule_context *, int, void **);
static int crule_is_identified(crule_context *, int, void **);
static int crule_is_webirc(crule_context *, int, void **);
static int crule_is_websocket(crule_context *, int, void **);
static int crule_tls(crule_context *, int, void **);
static int crule_in_security_group(crule_context *, int, void **);
static int crule_match_mask(crule_context *, int, void **);
static int crule_match_ip(crule_context *, int, void **);
static int crule_match_account(crule_context *, int, void **);
static int crule_match_country(crule_context *, int, void **);
static int crule_match_certfp(crule_context *, int, void **);

/* parsing function prototypes - local! */
static int crule_gettoken(crule_token *next_tokp, const char **str);
static void crule_getword(char *, int *, size_t, const char **);
static int crule_parseandexpr(CRuleNode **, crule_token *, const char **);
static int crule_parseorexpr(CRuleNode **, crule_token *, const char **);
static int crule_parseprimary(CRuleNode **, crule_token *, const char **);
static int crule_parsefunction(CRuleNode **, crule_token *, const char **);
static int crule_parsearglist(CRuleNode *, crule_token *, const char **);

/* error messages */
char *crule_errstr[] = {
	"Unknown error",	/* NOERR? - for completeness */
	"Unexpected token",	/* UNEXPCTTOK */
	"Unknown token",	/* UNKNWTOK */
	"And expr expected",	/* EXPCTAND */
	"Or expr expected",	/* EXPCTOR */
	"Primary expected",	/* EXPCTPRIM */
	"( expected",		/* EXPCTOPEN */
	") expected",		/* EXPCTCLOSE */
	"Unknown function",	/* UNKNWFUNC */
	"Argument mismatch",	/* ARGMISMAT */
	"Missing value in comparisson",	/* CR_EXPCTVALUE */
};

/* function table - null terminated */
struct crule_funclistent {
	char name[32];
	int reqnumargs;
	crule_funcptr funcptr;
};

struct crule_funclistent crule_funclist[] = {
	{"connected", 1, crule_connected},
	{"online_time", 0, crule_online_time},
	{"reputation", 0, crule_reputation},
	{"tag", 1, crule_tag},
	{"inchannel", 1, crule_in_channel}, // old name, keep it around for now..
	{"in_channel", 1, crule_in_channel}, // new name (6.1.6+)
	{"destination", 1, crule_destination},
	{"cap_version", 0, crule_cap_version},
	{"cap_set", 1, crule_cap_set},
	{"directcon", 1, crule_directcon},
	{"via", 2, crule_via},
	{"directop", 0, crule_directop},
	{"has_user_mode", 1, crule_has_user_mode},
	{"has_channel_mode", 1, crule_has_channel_mode},
	{"is_away", 0, crule_away},
	{"is_identified", 0, crule_is_identified},
	{"is_tls", 0, crule_tls},
	{"is_webirc", 0, crule_is_webirc},
	{"is_websocket", 0, crule_is_websocket},
	{"in_security_group", 1, crule_in_security_group},
	{"match_mask", 1, crule_match_mask},
	{"match_ip", 1, crule_match_ip},
	{"match_account", 1, crule_match_account},
	{"match_country", 1, crule_match_country},
	{"match_certfp", 1, crule_match_certfp},
	{"", 0, NULL} /* this must be here to mark end of list */
};

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_CRULE_TEST, _crule_test);
	EfunctionAddPVoid(modinfo->handle, EFUNC_CRULE_PARSE, TO_PVOIDFUNC(_crule_parse));
	EfunctionAddVoid(modinfo->handle, EFUNC_CRULE_FREE, _crule_free);
	EfunctionAdd(modinfo->handle, EFUNC_CRULE_EVAL, _crule_eval);
	EfunctionAddConstString(modinfo->handle, EFUNC_CRULE_ERRSTRING, _crule_errstring);
	return MOD_SUCCESS;
}

MOD_INIT()
{
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

static int crule_away(crule_context *context, int numargs, void *crulearg[])
{
	if (!context || !context->client || !IsUser(context->client))
		return 0;

	return (!BadPtr(context->client->user->away)) ? 1 : 0;
}

static int crule_is_identified(crule_context *context, int numargs, void *crulearg[])
{
	if (!context || !context->client)
		return 0;

	return (IsLoggedIn(context->client)) ? 1 : 0;
}

static int crule_is_websocket(crule_context *context, int numargs, void *crulearg[])
{
	if (!context || !context->client)
		return 0;

	return (moddata_client_get(context->client, "websocket")) ? 1 : 0;
}

static int crule_is_webirc(crule_context *context, int numargs, void *crulearg[])
{
	if (!context || !context->client)
		return 0;

	return (moddata_client_get(context->client, "webirc")) ? 1 : 0;
}

static int crule_tls(crule_context *context, int numargs, void *crulearg[])
{
	if (!context || !context->client)
		return 0;

	return (IsSecure(context->client) || IsSecureConnect(context->client)) ? 1 : 0;
}

static int crule_has_user_mode(crule_context *context, int numargs, void *crulearg[])
{
	const char *modes = (char *)crulearg[0];

	if (!context || !context->client || !strlen(modes))
		return 0;

	for (; *modes; modes++)
		if (!has_user_mode(context->client, *modes))
			return 0;

	return 1;
}

static int crule_has_channel_mode(crule_context *context, int numargs, void *crulearg[])
{
	const char *modes = (char *)crulearg[0];
	Channel *channel;

	if (!context || !context->destination || (context->destination[0] != '#'))
		return 0;

	channel = find_channel(context->destination);
	if (!channel)
		return 0;

	for (; *modes; modes++)
		if (!has_channel_mode(channel, *modes))
			return 0;

	return 1;
}

static int crule_connected(crule_context *context, int numargs, void *crulearg[])
{
	Client *client;

	/* taken from cmd_links */
	/* Faster this way -- codemastr*/
	list_for_each_entry(client, &global_server_list, client_node)
	{
		if (!match_simple((char *)crulearg[0], client->name))
			continue;
		return 1;
	}
	return 0;
}

static int crule_directcon(crule_context *context, int numargs, void *crulearg[])
{
	Client *client;

	/* adapted from cmd_trace and exit_one_client */
	/* XXX: iterate server_list when added */
	list_for_each_entry(client, &lclient_list, lclient_node)
	{
		if (!IsServer(client))
			continue;
		if (!match_simple((char *)crulearg[0], client->name))
			continue;
		return 1;
	}
	return 0;
}

static int crule_via(crule_context *context, int numargs, void *crulearg[])
{
	Client *client;

	/* adapted from cmd_links */
	/* Faster this way -- codemastr */
	list_for_each_entry(client, &global_server_list, client_node)
	{
		if (!match_simple((char *)crulearg[1], client->name))
			continue;
		if (!match_simple((char *)crulearg[0], client->uplink->name))
			continue;
		return 1;
	}
	return 0;
}

static int crule_directop(crule_context *context, int numargs, void *crulearg[])
{
	Client *client;

	/* adapted from cmd_trace */
	list_for_each_entry(client, &lclient_list, lclient_node)
	{
		if (!IsOper(client))
			continue;

		return 1;
	}

	return 0;
}

static int crule_online_time(crule_context *context, int numargs, void *crulearg[])
{
	if (context && context->client)
		return get_connected_time(context->client);
	return 0;
}

static int crule_reputation(crule_context *context, int numargs, void *crulearg[])
{
	if (context && context->client)
		return GetReputation(context->client);
	return 0;
}

static int crule_tag(crule_context *context, int numargs, void *crulearg[])
{
	Tag *tag;
	if (!context || !context->client)
		return 0;
	tag = find_tag(context->client, (char *)crulearg[0]);
	if (tag)
		return tag->value;
	return 0;
}

static int crule_in_channel(crule_context *context, int numargs, void *crulearg[])
{
	Membership *lp;
	const char *channelname = (char *)crulearg[0];
	const char *p = channelname;
	char symbol = '\0';

	if (!context || !context->client || !context->client->user)
		return 0;

	/* The following code is taken from src/modules/extbans/inchannel.c
	 * function extban_inchannel_is_banned()
	 */

	if (*p != '#')
	{
		symbol = *p;
		p++;
	}

	for (lp = context->client->user->channel; lp; lp = lp->next)
	{
		if (match_esc(p, lp->channel->name))
		{
			/* Channel matched, check symbol if needed (+/%/@/etc) */
			if (symbol)
			{
				if (inchannel_compareflags(symbol, lp->member_modes))
					return 1;
			} else
				return 1;
		}
	}
	return 0;
}

static int crule_destination(crule_context *context, int numargs, void *crulearg[])
{
	const char *arg = (char *)crulearg[0];

	if (!context || !context->destination)
		return 0;

	return match_simple(arg, context->destination);
}

static int crule_cap_version(crule_context *context, int numargs, void *crulearg[])
{
	if (context && context->client && context->client->local)
		return context->client->local->cap_protocol;
	return 0;
}

static int crule_cap_set(crule_context *context, int numargs, void *crulearg[])
{
	const char *capname = (char *)crulearg[0];

	if (!context || !context->client || !context->client->local )
		return 0;

	if (HasCapability(context->client, capname))
		return 1;
	return 0;
}

static int crule_in_security_group(crule_context *context, int numargs, void *crulearg[])
{
	const char *arg = (char *)crulearg[0];

	if (!context || !context->client)
		return 0;

	if (user_allowed_by_security_group_name(context->client, arg))
		return 1;

	return 0;
}

static int crule_match_mask(crule_context *context, int numargs, void *crulearg[])
{
	const char *arg = (char *)crulearg[0];

	if (!context || !context->client)
		return 0;

	if (match_user(arg, context->client, MATCH_CHECK_REAL_HOST|MATCH_CHECK_IP|MATCH_CHECK_EXTENDED))
		return 1;

	return 0;
}

static int crule_match_ip(crule_context *context, int numargs, void *crulearg[])
{
	const char *arg = (char *)crulearg[0];

	if (!context || !context->client)
		return 0;

	if (match_user(arg, context->client, MATCH_CHECK_IP|MATCH_MASK_IS_HOST))
		return 1;

	return 0;
}

static int crule_match_account(crule_context *context, int numargs, void *crulearg[])
{
	const char *arg = (char *)crulearg[0];
	return user_matches_extended_server_ban(context->client, "account", arg);
}

static int crule_match_country(crule_context *context, int numargs, void *crulearg[])
{
	const char *arg = (char *)crulearg[0];
	return user_matches_extended_server_ban(context->client, "country", arg);
}

static int crule_match_certfp(crule_context *context, int numargs, void *crulearg[])
{
	const char *arg = (char *)crulearg[0];
	return user_matches_extended_server_ban(context->client, "certfp", arg);
}

/** Evaluate a connection rule.
 * @param[in] rule Rule to evalute.
 * @return Non-zero if the rule allows the connection, zero otherwise.
 */
int _crule_eval(crule_context *context, CRuleNode* rule)
{
	int ret = rule->funcptr(context, rule->numargs, rule->arg);
	switch (rule->func_test_type)
	{
		case CR_EQUAL:
			/* something()==xyz */
			if (ret == rule->func_test_value)
				return 1;
			return 0;
		case CR_LESS_THAN:
			/* something()<xyz */
			if (ret < rule->func_test_value)
				return 1;
			return 0;
		case CR_MORE_THAN:
			/* something()>xyz */
			if (ret > rule->func_test_value)
				return 1;
			return 0;
		default:
			/* Basic true/false handling */
			return ret;
	}
}

/** Perform an and-or-or test on crulearg[0] and crulearg[1].
 * If crulearg[2] is non-NULL, it means do OR; if it is NULL, do AND.
 * @param[in] numargs Number of valid args in \a crulearg.
 * @param[in] crulearg Argument array.
 * @return Non-zero if the condition is true, zero if not.
 */
static int crule__andor(crule_context *context, int numargs, void *crulearg[])
{
	int result1;

	result1 = crule_eval(context, crulearg[0]);
	if (crulearg[2]) /* or */
		return (result1 || crule_eval(context, crulearg[1]));
	else
		return (result1 && crule_eval(context, crulearg[1]));
}

/** Logically invert the result of crulearg[0].
 * @param[in] numargs Number of valid args in \a crulearg.
 * @param[in] crulearg Argument array.
 * @return Non-zero if the condition is true, zero if not.
 */
static int crule__not(crule_context *context, int numargs, void *crulearg[])
{
	return !crule_eval(context, crulearg[0]);
}

/** Scan an input token from \a ruleptr.
 * @param[out] next_tokp Receives type of next token.
 * @param[in,out] ruleptr Next readable character from input.
 * @return Either CR_UNKNWTOK if the input was unrecognizable, else CR_NOERR.
 */
static int crule_gettoken(crule_token *next_tokp, const char **ruleptr)
{
	char pending = '\0';

	*next_tokp = CR_UNKNOWN;
	while (*next_tokp == CR_UNKNOWN)
		switch (*(*ruleptr)++)
		{
			case ' ':
			case '\t':
				break;
			case '&':
				if (pending == '\0')
					pending = '&';
				else if (pending == '&')
					*next_tokp = CR_AND;
				else
					return CR_UNKNWTOK;
				break;
			case '|':
				if (pending == '\0')
					pending = '|';
				else if (pending == '|')
					*next_tokp = CR_OR;
				else
					return CR_UNKNWTOK;
				break;
			case '!':
				*next_tokp = CR_NOT;
				break;
			case '(':
				*next_tokp = CR_OPENPAREN;
				break;
			case ')':
				*next_tokp = CR_CLOSEPAREN;
				break;
			case '<':
				*next_tokp = CR_LESS_THAN;
				break;
			case '>':
				*next_tokp = CR_MORE_THAN;
				break;
			case '=':
				if (pending == '\0')
					pending = '=';
				else if (pending == '=')
					*next_tokp = CR_EQUAL;
				else
					return CR_UNKNWTOK;
				break;
			case ',':
				*next_tokp = CR_COMMA;
				break;
			case '\0':
				(*ruleptr)--;
				*next_tokp = CR_END;
				break;
			case ':':
				*next_tokp = CR_END;
				break;
			default:
				if ((isalnum(*(--(*ruleptr)))) || (**ruleptr == '*') ||
						(**ruleptr == '?') || (**ruleptr == '.') || (**ruleptr == '-') || (**ruleptr == '_') || (**ruleptr == '\''))
					*next_tokp = CR_WORD;
				else
					return CR_UNKNWTOK;
				break;
		}
	return CR_NOERR;
}

/** Scan a word from \a ruleptr.
 * @param[out] word Output buffer.
 * @param[out] wordlenp Length of word written to \a word (not including terminating NUL).
 * @param[in] maxlen Maximum number of bytes writable to \a word.
 * @param[in,out] ruleptr Next readable character from input.
 */
static void crule_getword(char *word, int *wordlenp, size_t maxlen, const char **ruleptr)
{
	char *word_ptr;
	char quoted = 0;

	word_ptr = word;

	if (**ruleptr == '\'')
	{
		(*ruleptr)++;
		quoted = 1;
	}

	while ((size_t)(word_ptr - word) < maxlen
	       && (isalnum(**ruleptr)
	           || **ruleptr == '*' || **ruleptr == '?'
	           || **ruleptr == '.' || **ruleptr == '-'
	           || **ruleptr == '_' || (quoted && (**ruleptr != '\''))))
	{
		*word_ptr++ = *(*ruleptr)++;
	}
	*word_ptr = '\0';
	*wordlenp = word_ptr - word;

	/* Eat the remaining ' quote, if needed... */
	if (quoted && (**ruleptr == '\''))
		(*ruleptr)++;
}

/** Parse an entire rule.
 * @param[in] rule Text form of rule.
 * @return CRuleNode for rule, or NULL if there was a parse error.
 */
CRuleNode *_crule_parse(const char *rule)
{
	const char *ruleptr = rule;
	crule_token next_tok;
	CRuleNode* ruleroot = 0;
	int errcode = CR_NOERR;

	if ((errcode = crule_gettoken(&next_tok, &ruleptr)) == CR_NOERR) {
		if ((errcode = crule_parseorexpr(&ruleroot, &next_tok, &ruleptr)) == CR_NOERR) {
			if (ruleroot != NULL) {
				if (next_tok == CR_END)
					return ruleroot;
				else
					errcode = CR_UNEXPCTTOK;
			}
			else
				errcode = CR_EXPCTOR;
		}
	}
	if (ruleroot != NULL)
		crule_free(&ruleroot);
	return 0;
}

/** Test-parse an entire rule.
 * @param[in] rule Text form of rule.
 * @return error code, or 0 for no failure
 */
int _crule_test(const char *rule)
{
	const char *ruleptr = rule;
	crule_token next_tok;
	CRuleNode* ruleroot = 0;
	int errcode = CR_NOERR;

	if ((errcode = crule_gettoken(&next_tok, &ruleptr)) == CR_NOERR) {
		if ((errcode = crule_parseorexpr(&ruleroot, &next_tok, &ruleptr)) == CR_NOERR) {
			if (ruleroot != NULL) {
				if (next_tok == CR_END)
				{
					/* PASS */
					crule_free(&ruleroot);
					return 0;
				} else {
					errcode = CR_UNEXPCTTOK;
				}
			}
			else
				errcode = CR_EXPCTOR;
		}
	}
	if (ruleroot != NULL)
		crule_free(&ruleroot);
	return errcode + 1;
}

const char *_crule_errstring(int errcode)
{
	if (errcode == 0)
		return "No error";
	else
		return crule_errstr[errcode-1];
}

/** Parse an or expression.
 * @param[out] orrootp Receives parsed node.
 * @param[in,out] next_tokp Next input token type.
 * @param[in,out] ruleptr Next input character.
 * @return A crule_errcode value.
 */
static int crule_parseorexpr(CRuleNode **orrootp, crule_token *next_tokp, const char **ruleptr)
{
	int errcode = CR_NOERR;
	CRuleNode *andexpr;
	CRuleNode *orptr;

	*orrootp = NULL;
	while (errcode == CR_NOERR)
	{
		errcode = crule_parseandexpr(&andexpr, next_tokp, ruleptr);
		if ((errcode == CR_NOERR) && (*next_tokp == CR_OR))
		{
			orptr = safe_alloc(sizeof(CRuleNode));
			orptr->funcptr = crule__andor;
			orptr->flags |= CRULE_FLAG_AND_OR;
			orptr->numargs = 3;
			orptr->arg[2] = (void *)1;
			if (*orrootp != NULL)
			{
				(*orrootp)->arg[1] = andexpr;
				orptr->arg[0] = *orrootp;
			}
			else
				orptr->arg[0] = andexpr;
			*orrootp = orptr;
		}
		else
		{
			if (*orrootp != NULL)
			{
				if (andexpr != NULL)
				{
					(*orrootp)->arg[1] = andexpr;
					return errcode;
				}
				else
				{
					(*orrootp)->arg[1] = NULL;		/* so free doesn't seg fault */
					return CR_EXPCTAND;
				}
			}
			else
			{
				*orrootp = andexpr;
				return errcode;
			}
		}
		errcode = crule_gettoken(next_tokp, ruleptr);
	}
	return errcode;
}

/** Parse an and expression.
 * @param[out] androotp Receives parsed node.
 * @param[in,out] next_tokp Next input token type.
 * @param[in,out] ruleptr Next input character.
 * @return A crule_errcode value.
 */
static int crule_parseandexpr(CRuleNode **androotp, crule_token *next_tokp, const char **ruleptr)
{
	int errcode = CR_NOERR;
	CRuleNode *primary;
	CRuleNode *andptr;

	*androotp = NULL;
	while (errcode == CR_NOERR)
	{
		errcode = crule_parseprimary(&primary, next_tokp, ruleptr);
		if ((errcode == CR_NOERR) && (*next_tokp == CR_AND))
		{
			andptr = safe_alloc(sizeof(CRuleNode));
			andptr->funcptr = crule__andor;
			andptr->flags |= CRULE_FLAG_AND_OR;
			andptr->numargs = 3;
			andptr->arg[2] = (void *)0;
			if (*androotp != NULL)
			{
				(*androotp)->arg[1] = primary;
				andptr->arg[0] = *androotp;
			}
			else
				andptr->arg[0] = primary;
			*androotp = andptr;
		}
		else
		{
			if (*androotp != NULL)
			{
				if (primary != NULL)
				{
					(*androotp)->arg[1] = primary;
					return errcode;
				}
				else
				{
					(*androotp)->arg[1] = NULL;	 /* so free doesn't seg fault */
					return CR_EXPCTPRIM;
				}
			}
			else
			{
				*androotp = primary;
				return errcode;
			}
		}
		errcode = crule_gettoken(next_tokp, ruleptr);
	}
	return errcode;
}

/** Parse a primary expression.
 * @param[out] primrootp Receives parsed node.
 * @param[in,out] next_tokp Next input token type.
 * @param[in,out] ruleptr Next input character.
 * @return A crule_errcode value.
 */
static int crule_parseprimary(CRuleNode **primrootp, crule_token *next_tokp, const char **ruleptr)
{
	CRuleNode **insertionp;
	int errcode = CR_NOERR;

	*primrootp = NULL;
	insertionp = primrootp;
	while (errcode == CR_NOERR)
	{
		switch (*next_tokp)
		{
			case CR_OPENPAREN:
				if ((errcode = crule_gettoken(next_tokp, ruleptr)) != CR_NOERR)
					break;
				if ((errcode = crule_parseorexpr(insertionp, next_tokp, ruleptr)) != CR_NOERR)
					break;
				if (*insertionp == NULL)
				{
					errcode = CR_EXPCTAND;
					break;
				}
				if (*next_tokp != CR_CLOSEPAREN)
				{
					errcode = CR_EXPCTCLOSE;
					break;
				}
				errcode = crule_gettoken(next_tokp, ruleptr);
				break;
			case CR_NOT:
				*insertionp = safe_alloc(sizeof(CRuleNode));
				(*insertionp)->funcptr = crule__not;
				(*insertionp)->flags |= CRULE_FLAG_NOT;
				(*insertionp)->numargs = 1;
				(*insertionp)->arg[0] = NULL;
				insertionp = (CRuleNode **) & ((*insertionp)->arg[0]);
				if ((errcode = crule_gettoken(next_tokp, ruleptr)) != CR_NOERR)
					break;
				continue;
			case CR_WORD:
				errcode = crule_parsefunction(insertionp, next_tokp, ruleptr);
				break;
			default:
				if (*primrootp == NULL)
					errcode = CR_NOERR;
				else
					errcode = CR_EXPCTPRIM;
				break;
		}
		break; /* loop only continues after a CR_NOT */
	}
	return errcode;
}

/** Parse a function call.
 * @param[out] funcrootp Receives parsed node.
 * @param[in,out] next_tokp Next input token type.
 * @param[in,out] ruleptr Next input character.
 * @return A crule_errcode value.
 */
static int crule_parsefunction(CRuleNode **funcrootp, crule_token *next_tokp, const char **ruleptr)
{
	int errcode = CR_NOERR;
	char funcname[CR_MAXARGLEN];
	int namelen;
	int i;
	struct crule_funclistent *func;

	*funcrootp = NULL;
	crule_getword(funcname, &namelen, CR_MAXARGLEN - 1, ruleptr);
	if ((errcode = crule_gettoken(next_tokp, ruleptr)) != CR_NOERR)
		return errcode;
	if (*next_tokp == CR_OPENPAREN)
	{
		for (i = 0;; i++)
		{
			func = &crule_funclist[i];
			if (!strcasecmp(func->name, funcname))
				break;
			if (func->name[0] == '\0')
				return CR_UNKNWFUNC;
		}
		if ((errcode = crule_gettoken(next_tokp, ruleptr)) != CR_NOERR)
			return errcode;
		*funcrootp = safe_alloc(sizeof(CRuleNode));
		(*funcrootp)->funcptr = NULL;			 /* for freeing aborted trees */
		if ((errcode =
				crule_parsearglist(*funcrootp, next_tokp, ruleptr)) != CR_NOERR)
			return errcode;
		if (*next_tokp != CR_CLOSEPAREN)
			return CR_EXPCTCLOSE;
		if ((func->reqnumargs != (*funcrootp)->numargs) && (func->reqnumargs != -1))
			return CR_ARGMISMAT;
		if ((errcode = crule_gettoken(next_tokp, ruleptr)) != CR_NOERR)
			return errcode;
		if (IsComparisson(*next_tokp))
		{
			char value[CR_MAXARGLEN];
			int valuelen = 0;
			/* a > < or == */
			(*funcrootp)->func_test_type = *next_tokp;
			if ((errcode = crule_gettoken(next_tokp, ruleptr)) != CR_NOERR)
				return errcode;
			/* now expect and get the value to compare against, eg '5' */
			if (*next_tokp != CR_WORD)
				return CR_EXPCTVALUE;
			crule_getword(value, &valuelen, CR_MAXARGLEN - 1, ruleptr);
			if ((errcode = crule_gettoken(next_tokp, ruleptr)) != CR_NOERR)
				return errcode;
			(*funcrootp)->func_test_value = atoi(value);
		}
		(*funcrootp)->funcptr = func->funcptr;
		return CR_NOERR;
	}
	else
		return CR_EXPCTOPEN;
}

/** Parse the argument list to a CRuleNode.
 * @param[in,out] argrootp Node whos argument list is being populated.
 * @param[in,out] next_tokp Next input token type.
 * @param[in,out] ruleptr Next input character.
 * @return A crule_errcode value.
 */
static int crule_parsearglist(CRuleNode *argrootp, crule_token *next_tokp, const char **ruleptr)
{
	int errcode = CR_NOERR;
	char *argelemp = NULL;
	char currarg[CR_MAXARGLEN];
	int arglen = 0;
	char word[CR_MAXARGLEN];
	int wordlen = 0;

	argrootp->numargs = 0;
	currarg[0] = '\0';
	while (errcode == CR_NOERR)
	{
		switch (*next_tokp)
		{
			case CR_WORD:
				crule_getword(word, &wordlen, CR_MAXARGLEN - 1, ruleptr);
				if (currarg[0] != '\0')
				{
					if ((arglen + wordlen) < (CR_MAXARGLEN - 1))
					{
						strcat(currarg, " ");
						strcat(currarg, word);
						arglen += wordlen + 1;
					}
				}
				else
				{
					strcpy(currarg, word);
					arglen = wordlen;
				}
				errcode = crule_gettoken(next_tokp, ruleptr);
				break;
			default:
				/* Syzop/2023-07-16: Removed the collapse() call as all crule
				 * stuff is now about more than just masks and stuff, was this
				 * really needed at all actually?
				 */
				//collapse(currarg);
				if (currarg[0] != '\0')
				{
					argelemp = raw_strdup(currarg);
					argrootp->arg[argrootp->numargs++] = (void *)argelemp;
				}
				if (*next_tokp != CR_COMMA)
					return CR_NOERR;
				currarg[0] = '\0';
				errcode = crule_gettoken(next_tokp, ruleptr);
				break;
		}
	}
	return errcode;
}

/*
 * This function is recursive..  I wish I knew a nonrecursive way but
 * I don't.  Anyway, recursion is fun..  :)
 * DO NOT CALL THIS FUNCTION WITH A POINTER TO A NULL POINTER
 * (i.e.: If *elem is NULL, you're doing it wrong - seg fault)
 */
/** Free a connection rule and all its children.
 * @param[in,out] elem Pointer to pointer to element to free.  MUST NOT BE NULL.
 */
void _crule_free(CRuleNode **elem)
{
	int arg, numargs;

	if ((*(elem))->flags & CRULE_FLAG_NOT)
	{
		/* type conversions and ()'s are fun! ;)	here have an aspirin.. */
		if ((*(elem))->arg[0] != NULL)
			crule_free((CRuleNode**) &((*(elem))->arg[0]));
	}
	else if ((*(elem))->flags & CRULE_FLAG_AND_OR)
	{
		crule_free((CRuleNode**) &((*(elem))->arg[0]));
		if ((*(elem))->arg[1] != NULL)
			crule_free((CRuleNode**) &((*(elem))->arg[1]));
	}
	else
	{
		numargs = (*(elem))->numargs;
		for (arg = 0; arg < numargs; arg++)
			safe_free((*(elem))->arg[arg]);
	}
	safe_free(*elem);
	*elem = 0;
}
