/* UnrealIRCd configuration preprocessor
 * (C) Copyright 2019 Bram Matthys ("Syzop") and the UnrealIRCd team
 * License: GPLv2
 *
 * Technically this isn't a 100% true preprocessor, but to the end user
 * it will certainly look like it, hence the name.
 */

#include "unrealircd.h"

extern ConfigFile *conf;

NameValueList *config_defines = NULL; /**< List of @defines, only valid during configuration reading */

void skip_whitespace(char **p)
{
	for (; **p == ' ' || **p == '\t'; *p = *p + 1);
}

void read_until(char **p, char *stopchars)
{
	for (; **p && !strchr(stopchars, **p); *p = *p + 1);
}

int evaluate_preprocessor_if(char *statement, char *filename, int linenumber, ConditionalConfig **cc_out)
{
	char *p=statement, *name;
	int negative = 0;
	ConditionalConfig *cc;

	/* Currently we support only 4 things:
	 * $XYZ == "something"
	 * $XYZ != "something"
	 * module-loaded("something")
	 * !module-loaded("something")
	 * defined($XYZ)
	 * !defined($XYZ)
	 * We do not support && or || or anything else at this time.
	 */
	skip_whitespace(&p);
	if (*p == '@')
		p++;
	if (*p == '!')
	{
		negative = 1;
		p++;
		skip_whitespace(&p);
	}

	/* Now comes the keyword or a variable name */
	if (!strncmp(p, "module-loaded", 13))
	{
		p += 13;
		skip_whitespace(&p);
		if (*p != '(')
		{
			config_error("%s:%i: expected '(' for module-loaded(...",
				filename, linenumber);
			return PREPROCESSOR_ERROR;
		}
		p++;
		skip_whitespace(&p);
		if (*p == '"')
			p++;
		name = p;
		read_until(&p, ")\"");
		if (!*p)
		{
			config_error("%s:%i: invalid if statement (termination error): %s",
				filename, linenumber, statement);
			return PREPROCESSOR_ERROR;
		}
		*p = '\0';
		cc = safe_alloc(sizeof(ConditionalConfig));
		cc->condition = IF_MODULE;
		cc->negative = negative;
		safe_strdup(cc->name, name);
		*cc_out = cc;
		return PREPROCESSOR_IF;
	} else
	if (!strncmp(p, "defined", 7))
	{
		p += 7;
		skip_whitespace(&p);
		if (*p != '(')
		{
			config_error("%s:%i: expected '(' for defined(...",
				filename, linenumber);
			return PREPROCESSOR_ERROR;
		}
		p++;
		skip_whitespace(&p);
		if (*p == '"')
			p++;
		name = p;
		read_until(&p, ")\"");
		if (!*p)
		{
			config_error("%s:%i: invalid if statement (termination error): %s",
				filename, linenumber, statement);
			return PREPROCESSOR_ERROR;
		}
		*p = '\0';
		cc = safe_alloc(sizeof(ConditionalConfig));
		cc->condition = IF_DEFINED;
		cc->negative = negative;
		safe_strdup(cc->name, name);
		*cc_out = cc;
		return PREPROCESSOR_IF;
	} else
	{
		char *name_terminate, *name2;
		// Should be one of:
		// $XYZ == "something"
		// $XYZ != "something"
		// Anything else is an error.
		if (*p != '$')
		{
			config_error("%s:%i: invalid @if statement. Either an unknown function, or did you mean $VARNAME?: %s",
				filename, linenumber, statement);
			return PREPROCESSOR_ERROR;
		}
		p++;
		/* variable name starts now */
		name = p;
		read_until(&p, " \t=!");
		if (!*p)
		{
			config_error("%s:%i: invalid if statement (termination error): %s",
				filename, linenumber, statement);
			return PREPROCESSOR_ERROR;
		}
		name_terminate = p;
		skip_whitespace(&p);
		if (!strncmp(p, "==", 2))
		{
			negative = 0;
		} else
		if (!strncmp(p, "!=", 2))
		{
			negative = 1;
		} else
		{
			*name_terminate = '\0';
			config_error("%s:%i: @if: expected == or != after '%s'",
				filename, linenumber, name);
			return PREPROCESSOR_ERROR;
		}
		p += 2;
		*name_terminate = '\0';
		skip_whitespace(&p);
		if (*p != '"')
		{
			config_error("%s:%i: @if: expected double quotes, missing \" perhaps?",
				filename, linenumber);
			return PREPROCESSOR_ERROR;
		}
		p++;
		name2 = p;
		read_until(&p, "\"");
		if (!*p)
		{
			config_error("%s:%i: invalid @if statement, missing \" at end perhaps?",
				filename, linenumber);
			return PREPROCESSOR_ERROR;
		}
		*p = '\0';
		cc = safe_alloc(sizeof(ConditionalConfig));
		cc->condition = IF_VALUE;
		cc->negative = negative;
		safe_strdup(cc->name, name);
		safe_strdup(cc->opt, name2);
		*cc_out = cc;
		return PREPROCESSOR_IF;
	}

	config_error("%s:%i: Error while evaluating '@if' statement '%s'",
		filename, linenumber, statement);
	return PREPROCESSOR_ERROR;
}

int evaluate_preprocessor_define(char *statement, char *filename, int linenumber)
{
	char *p = statement;
	char *name, *name_terminator;
	char *value;

	skip_whitespace(&p);
	name = p;
	read_until(&p, " \t");
	if (!*p)
	{
		config_error("%s:%i: invalid @define statement",
			filename, linenumber);
		return PREPROCESSOR_ERROR;
	}
	name_terminator = p;
	skip_whitespace(&p);
	if (*p != '"')
	{
		config_error("%s:%i: @define: expected double quotes, missing \" perhaps?",
			filename, linenumber);
		return PREPROCESSOR_ERROR;
	}
	p++;
	value = p;
	read_until(&p, "\"");
	if (!*p)
	{
		config_error("%s:%i: invalid @define statement, missing \" at end perhaps?",
			filename, linenumber);
		return PREPROCESSOR_ERROR;
	}

	*p = '\0';
	*name_terminator = '\0';

	NameValueList *d = safe_alloc(sizeof(NameValueList));
	safe_strdup(d->name, name);
	safe_strdup(d->value, value);
	AddListItem(d, config_defines);
	return PREPROCESSOR_DEFINE;
}

int parse_preprocessor_item(char *start, char *end, char *filename, int linenumber, ConditionalConfig **cc)
{
	char buf[512];
	int max;

	*cc = NULL;

	max = end - start + 1;
	if (max > sizeof(buf))
		max = sizeof(buf);
	strlcpy(buf, start, max);

	if (!strncmp(buf, "@define", 7))
		return evaluate_preprocessor_define(buf+7, filename, linenumber);
	else if (!strncmp(buf, "@if ", 4))
		return evaluate_preprocessor_if(buf+4, filename, linenumber, cc);
	else if (!strcmp(buf, "@endif"))
		return PREPROCESSOR_ENDIF;

	config_error("%s:%i: Unknown preprocessor directive: %s", filename, linenumber, buf);
	return PREPROCESSOR_ERROR; /* ??? */
}

/** Free a ConditionalConfig entry.
 * NOTE: be sure to do a DelListItem() before calling this, if necessary.
 */
void preprocessor_cc_free_entry(ConditionalConfig *cc)
{
	safe_free(cc->name);
	safe_free(cc->opt);
	safe_free(cc);
}

/** Free ConditionalConfig entries in a linked list that
 * are equal or above 'level'. This happens during an @endif.
 */
void preprocessor_cc_free_level(ConditionalConfig **cc_list, int level)
{
	ConditionalConfig *cc, *cc_next;
	for (cc = *cc_list; cc; cc = cc_next)
	{
		cc_next = cc->next;
		if (cc->priority >= level)
		{
			DelListItem(cc, *cc_list);
			preprocessor_cc_free_entry(cc);
		}
	}
}

/** Duplicates a linked list of ConditionalConfig entries */
void preprocessor_cc_duplicate_list(ConditionalConfig *r, ConditionalConfig **out)
{
	ConditionalConfig *cc;

	*out = NULL;
	for (; r; r = r->next)
	{
		cc = safe_alloc(sizeof(ConditionalConfig));
		safe_strdup(cc->name, r->name);
		safe_strdup(cc->opt, r->opt);
		cc->priority = r->priority;
		cc->condition = r->condition;
		cc->negative = r->negative;
		AddListItem(cc, *out);
	}
}

void preprocessor_cc_free_list(ConditionalConfig *cc)
{
	ConditionalConfig *cc_next;

	for (; cc; cc = cc_next)
	{
		cc_next = cc->next;
		safe_free(cc->name);
		safe_free(cc->opt);
		safe_free(cc);
	}
}

void free_config_entry(ConfigEntry *ce)
{
	/* free ConditionalConfig */
	if (ce->ce_cond)
		preprocessor_cc_free_list(ce->ce_cond);

	/* free ConfigEntry */
	safe_free(ce->ce_varname);
	safe_free(ce->ce_vardata);
	safe_free(ce);
}

NameValueList *find_config_define(const char *name)
{
	NameValueList *n;

	for (n = config_defines; n; n = n->next)
		if (!strcasecmp(n->name, name))
			return n;
	return NULL;
}

/** Resolve a preprocessor condition to true (=default) or false */
int preprocessor_resolve_if(ConditionalConfig *cc, int phase)
{
	int result = 0;

	if (!cc)
		return 1;

	if (cc->condition == IF_MODULE)
	{
		if (phase == PREPROCESSOR_PHASE_INITIAL)
			return 1; /* we cannot handle @if module-loaded() yet.. */
		if (is_module_loaded(cc->name))
		{
			result = 1;
		}
	} else
	if (cc->condition == IF_DEFINED)
	{
		NameValueList *d = find_config_define(cc->name);
		if (d)
		{
			result = 1;
		}
	} else
	if (cc->condition == IF_VALUE)
	{
		NameValueList *d = find_config_define(cc->name);
		if (d && !strcasecmp(d->value, cc->opt))
		{
			result = 1;
		}
	} else
	{
		config_status("[BUG] unhandled @if type!!");
	}

	if (cc->negative)
		result = result ? 0 : 1;

	return result;
}

void preprocessor_resolve_conditionals_ce(ConfigEntry **ce_list, int phase)
{
	ConfigEntry *ce, *ce_next, *ce_prev;
	ConfigEntry *cep, *cep_next, *cep_prev;

	ce_prev = NULL;
	for (ce = *ce_list; ce; ce = ce_next)
	{
		ce_next = ce->ce_next;
		/* This is for an @if before a block start */
		if (!preprocessor_resolve_if(ce->ce_cond, phase))
		{
			/* Delete this entry */
			if (ce == *ce_list)
			{
				/* we are head, so new head */
				*ce_list = ce->ce_next; /* can be NULL now */
			} else {
				/* non-head */
				ce_prev->ce_next = ce->ce_next; /* can be NULL now */
			}
			// FIXME: this entry may have ce->ce_entries as well, right?
			// if so, we must free those too or we have a memory leak !
			free_config_entry(ce);
			continue;
		}

		cep_prev = NULL;
		for (cep = ce->ce_entries; cep; cep = cep_next)
		{
			cep_next = cep->ce_next;
			if (!preprocessor_resolve_if(cep->ce_cond, phase))
			{
				/* Delete this entry */
				if (cep == ce->ce_entries)
				{
					/* we are head, so new head */
					ce->ce_entries = cep->ce_next; /* can be NULL now */
				} else {
					/* non-head */
					ce_prev->ce_next = cep->ce_next;
				}
				free_config_entry(cep);
				continue;
			}
			preprocessor_resolve_conditionals_ce(&cep, phase);
			cep_prev = cep;
		}
		ce_prev = ce;
	}
}

void preprocessor_resolve_conditionals_all(int phase)
{
	ConfigFile *cfptr;

	for (cfptr = conf; cfptr; cfptr = cfptr->cf_next)
		preprocessor_resolve_conditionals_ce(&cfptr->cf_entries, phase);
}

/** Frees the list of config_defines, so all @defines */
void free_config_defines(void)
{
	NameValueList *e, *e_next;
	for (e = config_defines; e; e = e_next)
	{
		e_next = e->next;
		safe_free(e->name);
		safe_free(e->value);
		safe_free(e);
	}
	config_defines = NULL;
}

/** Return value of defined value */
char *get_config_define(char *name)
{
	NameValueList *e;

	for (e = config_defines; e; e = e->next)
	{
		if (!strcasecmp(e->name, name))
			return e->value;
	}

	return NULL;
}

void preprocessor_replace_defines(char **item)
{
	static char buf[4096];
	char varname[512];
	const char *i, *varstart, *varend;
	char *o;
	int n = sizeof(buf)-2;
	int limit;
	char *value;

	if (!strchr(*item, '$'))
		return; /* quick return in 99% of the cases */

	o = buf;
	for (i = *item; *i; i++)
	{
		if (*i != '$')
		{
			*o++ = *i;
			if (--n == 0)
				break;
			continue;
		}

		/* $ encountered: */
		varstart = i;
		i++;
		for (; *i && (isalnum(*i) || strchr("_", *i)); i++);
		varend = i;
		i--;
		limit = varend - varstart + 1;
		if (limit > sizeof(varname))
			limit = sizeof(varname);
		strlcpy(varname, varstart, limit);
		value = get_config_define(varname+1);
		if (!value)
			value = varname; /* not found? then use varname, including the '$' */
		limit = strlen(value) + 1;
		if (limit > n)
			limit = n;
		strlcpy(o, value, limit);
		o += limit - 1;
		n -= limit;
		if (n == 0)
			break; /* no output buffer left */
		if (*varend == 0)
			break; /* no input buffer left */
	}
	*o = '\0';
	safe_strdup(*item, buf);
}
