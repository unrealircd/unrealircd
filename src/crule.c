/**
 * @file
 * @brief Connection rule parser and checker
 * @version $Id$
 *
 * by Tony Vencill (Tonto on IRC) <vencill@bga.com>
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
 * Please direct any connection rule or SmartRoute questions to Tonto on
 * IRC or by email to vencill@bga.com.
 *
 * For parser testing, defining CR_DEBUG generates a stand-alone parser
 * that takes rules from stdin and prints out memory allocation
 * information and the parsed rule.  This stand alone parser is ignorant
 * of the irc server and thus cannot do rule evaluation.  Do not define
 * this flag when compiling the server!  If you wish to generate the
 * test parser, compile from the ircd directory with a line similar to
 * cc -o parser -DCR_DEBUG crule.c
 *
 * The define CR_CHKCONF is provided to generate routines needed in
 * chkconf.  These consist of the parser, a different crule_parse that
 * prints errors to stderr, and crule_free (just for good style and to
 * more closely simulate the actual ircd environment).  crule_eval and
 * the rule functions are made empty functions as in the stand-alone
 * test parser.
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

#ifndef CR_DEBUG
/* ircd functions and types we need */
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include <string.h>

char *collapse(char *pattern);
extern Client *client;

ID_Copyright("(C) Tony Vincell");

#else
/* includes and defines to make the stand-alone test parser */
#include <stdio.h>
#include <string.h>
#define BadPtr(x) (!(x) || (*(x) == '\0'))
#endif

#if defined(CR_DEBUG) || defined(CR_CHKCONF)
#undef safe_free
#undef free
#define safe_free free
#endif

/*
 * Some symbols for easy reading
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

/*
 * Expression tree structure, function pointer, and tree pointer local!
 */

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
static int crule_cap_version(crule_context *, int, void **);
static int crule_cap_set(crule_context *, int, void **);

/* parsing function prototypes - local! */
static int crule_gettoken(crule_token *next_tokp, const char **str);
static void crule_getword(char *, int *, size_t, const char **);
static int crule_parseandexpr(CRuleNodePtr *, crule_token *, const char **);
static int crule_parseorexpr(CRuleNodePtr *, crule_token *, const char **);
static int crule_parseprimary(CRuleNodePtr *, crule_token *, const char **);
static int crule_parsefunction(CRuleNodePtr *, crule_token *, const char **);
static int crule_parsearglist(CRuleNodePtr, crule_token *, const char **);

#if defined(CR_DEBUG) || defined(CR_CHKCONF)
/*
 * Prototypes for the test parser; if not debugging,
 * these are defined in h.h
 */
struct CRuleNode* crule_parse(const char *);
void crule_free(struct CRuleNode**);
#ifdef CR_DEBUG
void print_tree(CRuleNodePtr);
#endif
#endif

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
	char name[15];		/* MAXIMUM FUNCTION NAME LENGTH IS 14 CHARS!! */
	int reqnumargs;
	crule_funcptr funcptr;
};

struct crule_funclistent crule_funclist[] = {
	/* maximum function name length is 14 chars */
	{"connected", 1, crule_connected},
	{"online_time", 0, crule_online_time},
	{"reputation", 0, crule_reputation},
	{"tag", 1, crule_tag},
	{"cap_version", 0, crule_cap_version},
	{"cap_set", 1, crule_cap_set},
	{"directcon", 1, crule_directcon},
	{"via", 2, crule_via},
	{"directop", 0, crule_directop},
	{"", 0, NULL}		/* this must be here to mark end of list */
};

static int crule_connected(crule_context *context, int numargs, void *crulearg[])
{
#if !defined(CR_DEBUG) && !defined(CR_CHKCONF)
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
#endif
}

static int crule_directcon(crule_context *context, int numargs, void *crulearg[])
{
#if !defined(CR_DEBUG) && !defined(CR_CHKCONF)
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
#endif
}

static int crule_via(crule_context *context, int numargs, void *crulearg[])
{
#if !defined(CR_DEBUG) && !defined(CR_CHKCONF)
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
#endif
}

static int crule_directop(crule_context *context, int numargs, void *crulearg[])
{
#if !defined(CR_DEBUG) && !defined(CR_CHKCONF)
	Client *client;

	/* adapted from cmd_trace */
	list_for_each_entry(client, &lclient_list, lclient_node)
	{
		if (!IsOper(client))
			continue;

		return 1;
	}

	return 0;
#endif
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

static int crule_cap_version(crule_context *context, int numargs, void *crulearg[])
{
	if (context && context->client && context->client->local)
		return context->client->local->cap_protocol;
	return 0;
}

static int crule_cap_set(crule_context *context, int numargs, void *crulearg[])
{
	Tag *tag;
	const char *capname = (char *)crulearg[0];

	if (!context || !context->client)
		return 0;

	if (HasCapability(context->client, capname))
		return 1;
	return 0;
}

/** Evaluate a connection rule.
 * @param[in] rule Rule to evalute.
 * @return Non-zero if the rule allows the connection, zero otherwise.
 */
int crule_eval(crule_context *context, struct CRuleNode* rule)
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
						(**ruleptr == '?') || (**ruleptr == '.') || (**ruleptr == '-') || (**ruleptr == '_'))
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

	word_ptr = word;
	while ((size_t)(word_ptr - word) < maxlen
	       && (isalnum(**ruleptr)
	           || **ruleptr == '*' || **ruleptr == '?'
	           || **ruleptr == '.' || **ruleptr == '-'
	           || **ruleptr == '_'))
	{
		*word_ptr++ = *(*ruleptr)++;
	}
	*word_ptr = '\0';
	*wordlenp = word_ptr - word;
}

/** Parse an entire rule.
 * @param[in] rule Text form of rule.
 * @return CRuleNode for rule, or NULL if there was a parse error.
 */
struct CRuleNode* crule_parse(const char *rule)
{
	const char *ruleptr = rule;
	crule_token next_tok;
	struct CRuleNode* ruleroot = 0;
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
#if defined(CR_DEBUG) || defined(CR_CHKCONF)
	fprintf(stderr, "%s in rule: %s\n", crule_errstr[errcode], rule);
#endif
	return 0;
}

/** Test-parse an entire rule.
 * @param[in] rule Text form of rule.
 * @return error code, or 0 for no failure
 */
int crule_test(const char *rule)
{
	const char *ruleptr = rule;
	crule_token next_tok;
	struct CRuleNode* ruleroot = 0;
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
#if defined(CR_DEBUG) || defined(CR_CHKCONF)
	fprintf(stderr, "%s in rule: %s\n", crule_errstr[errcode], rule);
#endif
	return errcode + 1;
}

const char *crule_errstring(int errcode)
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
static int crule_parseorexpr(CRuleNodePtr * orrootp, crule_token *next_tokp, const char **ruleptr)
{
	int errcode = CR_NOERR;
	CRuleNodePtr andexpr;
	CRuleNodePtr orptr;

	*orrootp = NULL;
	while (errcode == CR_NOERR)
	{
		errcode = crule_parseandexpr(&andexpr, next_tokp, ruleptr);
		if ((errcode == CR_NOERR) && (*next_tokp == CR_OR))
		{
			orptr = safe_alloc(sizeof(struct CRuleNode));
#ifdef CR_DEBUG
			fprintf(stderr, "allocating or element at %ld\n", orptr);
#endif
			orptr->funcptr = crule__andor;
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
static int crule_parseandexpr(CRuleNodePtr * androotp, crule_token *next_tokp, const char **ruleptr)
{
	int errcode = CR_NOERR;
	CRuleNodePtr primary;
	CRuleNodePtr andptr;

	*androotp = NULL;
	while (errcode == CR_NOERR)
	{
		errcode = crule_parseprimary(&primary, next_tokp, ruleptr);
		if ((errcode == CR_NOERR) && (*next_tokp == CR_AND))
		{
			andptr = safe_alloc(sizeof(struct CRuleNode));
#ifdef CR_DEBUG
			fprintf(stderr, "allocating and element at %ld\n", andptr);
#endif
			andptr->funcptr = crule__andor;
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
static int crule_parseprimary(CRuleNodePtr *primrootp, crule_token *next_tokp, const char **ruleptr)
{
	CRuleNodePtr *insertionp;
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
				*insertionp = safe_alloc(sizeof(struct CRuleNode));
#ifdef CR_DEBUG
				fprintf(stderr, "allocating primary element at %ld\n", *insertionp);
#endif
				(*insertionp)->funcptr = crule__not;
				(*insertionp)->numargs = 1;
				(*insertionp)->arg[0] = NULL;
				insertionp = (CRuleNodePtr *) & ((*insertionp)->arg[0]);
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
static int crule_parsefunction(CRuleNodePtr *funcrootp, crule_token *next_tokp, const char ** ruleptr)
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
		*funcrootp = safe_alloc(sizeof(struct CRuleNode));
#ifdef CR_DEBUG
		fprintf(stderr, "allocating function element at %ld\n", *funcrootp);
#endif
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
static int crule_parsearglist(CRuleNodePtr argrootp, crule_token *next_tokp, const char **ruleptr)
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
#if !defined(CR_DEBUG) && !defined(CR_CHKCONF)
				collapse(currarg);
#endif
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
void crule_free(struct CRuleNode** elem)
{
	int arg, numargs;

	if ((*(elem))->funcptr == crule__not)
	{
		/* type conversions and ()'s are fun! ;)	here have an aspirin.. */
		if ((*(elem))->arg[0] != NULL)
			crule_free((struct CRuleNode**) &((*(elem))->arg[0]));
	}
	else if ((*(elem))->funcptr == crule__andor)
	{
		crule_free((struct CRuleNode**) &((*(elem))->arg[0]));
		if ((*(elem))->arg[1] != NULL)
			crule_free((struct CRuleNode**) &((*(elem))->arg[1]));
	}
	else
	{
		numargs = (*(elem))->numargs;
		for (arg = 0; arg < numargs; arg++)
			safe_free((*(elem))->arg[arg]);
	}
#ifdef CR_DEBUG
	fprintf(stderr, "freeing element at %ld\n", *elem);
#endif
	safe_free(*elem);
	*elem = 0;
}

#ifdef CR_DEBUG
/** Display a connection rule as text.
 * @param[in] printelem Connection rule to display.
 */
static void print_tree(CRuleNodePtr printelem)
{
	int funcnum, arg;

	if (printelem->funcptr == crule__not)
	{
		printf("!( ");
		print_tree((CRuleNodePtr) printelem->arg[0]);
		printf(") ");
	}
	else if (printelem->funcptr == crule__andor)
	{
		printf("( ");
		print_tree((CRuleNodePtr) printelem->arg[0]);
		if (printelem->arg[2])
			printf("|| ");
		else
			printf("&& ");
		print_tree((CRuleNodePtr) printelem->arg[1]);
		printf(") ");
	}
	else
	{
		for (funcnum = 0;; funcnum++)
		{
			if (printelem->funcptr == crule_funclist[funcnum].funcptr)
				break;
			if (crule_funclist[funcnum].funcptr == NULL)
				MyCoreDump;
		}
		printf("%s(", crule_funclist[funcnum].name);
		for (arg = 0; arg < printelem->numargs; arg++)
		{
			if (arg != 0)
				printf(",");
			printf("%s", (char *)printelem->arg[arg]);
		}
		printf(") ");
	}
}

#endif

#ifdef CR_DEBUG
/** Read connection rules from stdin and display parsed forms as text.
 * @return Zero.
 */
int main(void)
{
	char indata[256];
	CRuleNode* rule;

	printf("rule: ");
	while (fgets(indata, 256, stdin) != NULL)
	{
		indata[strlen(indata) - 1] = '\0';	/* lose the newline */
		if ((rule = crule_parse(indata)) != NULL)
		{
			printf("equivalent rule: ");
			print_tree((CRuleNodePtr) rule);
			printf("\n");
			crule_free(&rule);
		}
		printf("\nrule: ");
	}
	printf("\n");

	return 0;
}

#endif
