/* UnrealIRCd configuration preprocessor
 * (C) Copyright 2019 Bram Matthys ("Syzop") and the UnrealIRCd team
 * License: GPLv2 or later
 *
 * Technically this isn't a 100% true preprocessor, but to the end user
 * it will certainly look like it, hence the name.
 */

#include "unrealircd.h"

extern ConfigFile *conf;

NameValuePrioList *config_defines = NULL; /**< List of @defines, only valid during configuration reading */

/* Forward declarations */
NameValuePrioList *find_config_define(const char *name);
void preprocessor_cc_free_entry(ConditionalConfig *cc);

typedef struct {
	const char *keyword;
	ConfigIfCondition condition;
	int returns_value; /**< 1 if function returns a string for comparison, 0 if boolean */
} IfFunction;

/** Table of function-style @if conditions.
 * NOTE: If two keywords share a prefix, put the longer one first,
 * since we use strncmp for matching.
 */
static IfFunction if_functions[] = {
	{ "module-loaded",    IF_MODULE_LOADED,    0 },
	{ "module-exists",    IF_MODULE_EXISTS,    0 },
	{ "minimum-version",  IF_MINIMUM_VERSION,  0 },
	{ "file-exists",      IF_FILE_EXISTS,      0 },
	{ "module-version",   IF_MODULE_VERSION,    1 },
	{ "environment",      IF_ENVIRONMENT,      1 },
	{ "defined",          IF_DEFINED,          0 },
};

static inline int ValidVarCharacter(char x)
{
	if (isupper(x) || isdigit(x) || strchr("_", x))
		return 1;
	return 0;
}

/** Parse a comparison operator and value: op "value"
 * @param p              Current position (should point at or before operator)
 * @param statement      Full statement (for error messages)
 * @param filename       Config filename (for error messages)
 * @param linenumber     Line number (for error messages)
 * @param compare_op_out Parsed operator stored here
 * @param value_out      Parsed value string stored here
 * @returns 1 on success, 0 on error
 */
static int parse_compare_op_and_value(char **p, char *statement,
                                      const char *filename, int linenumber,
                                      CompareOp *compare_op_out, char **value_out)
{
	int op_len;

	skip_whitespace(p);
	if (!strncmp(*p, "==", 2))
	{
		*compare_op_out = COMPARE_EQ;
		op_len = 2;
	} else
	if (!strncmp(*p, "!=", 2))
	{
		*compare_op_out = COMPARE_NE;
		op_len = 2;
	} else
	if (!strncmp(*p, ">=", 2))
	{
		*compare_op_out = COMPARE_GE;
		op_len = 2;
	} else
	if (!strncmp(*p, "<=", 2))
	{
		*compare_op_out = COMPARE_LE;
		op_len = 2;
	} else
	if (**p == '>')
	{
		*compare_op_out = COMPARE_GT;
		op_len = 1;
	} else
	if (**p == '<')
	{
		*compare_op_out = COMPARE_LT;
		op_len = 1;
	} else
	{
		config_error("%s:%i: @if: expected comparison operator (==, !=, >, >=, <, <=): %s",
			filename, linenumber, statement);
		return 0;
	}
	*p += op_len;
	skip_whitespace(p);
	if (**p == '"')
	{
		(*p)++;
		*value_out = *p;
		read_until(p, "\"");
		if (!**p)
		{
			config_error("%s:%i: invalid @if statement, missing \" at end perhaps?",
				filename, linenumber);
			return 0;
		}
		**p = '\0';
	} else
	{
		*value_out = *p;
		read_until(p, " \t");
		if (**p)
			**p = '\0';
	}
	return 1;
}

/** Parse a function-style @if condition: funcname("argument")
 * For value-returning functions, also parses: funcname("argument") op "value"
 * @param p              Position right after the keyword
 * @param funcname       Function name (for error messages only)
 * @param condition      The ConfigIfCondition enum value to set
 * @param negative       Whether the condition was negated with !
 * @param returns_value  Whether this function returns a value for comparison
 * @param statement      Full statement string (for error messages)
 * @param filename       Config filename (for error messages)
 * @param linenumber     Line number (for error messages)
 * @param cc_out         Result stored here on success
 * @returns PREPROCESSOR_IF on success, PREPROCESSOR_ERROR on failure
 */
static PreprocessorItem parse_if_function(char *p, const char *funcname, ConfigIfCondition condition,
                                          int negative, int returns_value, char *statement,
                                          const char *filename, int linenumber,
                                          ConditionalConfig **cc_out)
{
	char *name;
	ConditionalConfig *cc;

	skip_whitespace(&p);
	if (*p != '(')
	{
		config_error("%s:%i: expected '(' for %s(...",
			filename, linenumber, funcname);
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
	cc->condition = condition;
	cc->negative = negative;
	safe_strdup(cc->name, name);

	if (returns_value)
	{
		p++;
		if (*p == ')')
			p++;
		skip_whitespace(&p);
		if (*p)
		{
			/* Value-returning function with comparison: func("arg") op "value" */
			char *value = NULL;

			if (negative)
			{
				config_error("%s:%i: @if: the '!' prefix cannot be used with %s() comparisons, use != instead: %s",
					filename, linenumber, funcname, statement);
				preprocessor_cc_free_entry(cc);
				return PREPROCESSOR_ERROR;
			}
			if (!parse_compare_op_and_value(&p, statement, filename, linenumber, &cc->compare_op, &value))
			{
				preprocessor_cc_free_entry(cc);
				return PREPROCESSOR_ERROR;
			}
			safe_strdup(cc->opt, value);
		}
		/* else: no operator after ), treat as boolean (is it set?) */
	}

	*cc_out = cc;
	return PREPROCESSOR_IF;
}

PreprocessorItem evaluate_preprocessor_if(char *statement, const char *filename, int linenumber, ConditionalConfig **cc_out)
{
	char *p=statement;
	int i, negative = 0;
	ConditionalConfig *cc;

	/* Currently we support:
	 * $XYZ == "something"
	 * $XYZ != "something"
	 * $XYZ > "something"
	 * $XYZ >= "something"
	 * $XYZ < "something"
	 * $XYZ <= "something"
	 * module-loaded("something")
	 * !module-loaded("something")
	 * module-exists("something")
	 * !module-exists("something")
	 * minimum-version("6.2.0")
	 * !minimum-version("6.2.0")
	 * file-exists("something")
	 * !file-exists("something")
	 * environment("VARNAME")
	 * environment("VARNAME") == "value"
	 * module-version("name") >= "2.0"
	 * defined($XYZ)
	 * !defined($XYZ)
	 * @else is also supported (handled in conf.c, not here).
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

	/* Check function-style conditions from the table */
	for (i = 0; i < ARRAY_SIZEOF(if_functions); i++)
	{
		int len = strlen(if_functions[i].keyword);
		if (!strncmp(p, if_functions[i].keyword, len))
			return parse_if_function(p + len, if_functions[i].keyword, if_functions[i].condition, negative, if_functions[i].returns_value, statement, filename, linenumber, cc_out);
	}

	/* Otherwise it should be a $VARIABLE comparison */
	{
		char *name, *name_terminate, *value = NULL;
		CompareOp compare_op;

		if (*p != '$')
		{
			config_error("%s:%i: invalid @if statement. Either an unknown function, or did you mean $VARNAME?: %s",
				filename, linenumber, statement);
			return PREPROCESSOR_ERROR;
		}
		if (negative)
		{
			config_error("%s:%i: @if: the '!' prefix cannot be used with variable comparisons, use != instead: %s",
				filename, linenumber, statement);
			return PREPROCESSOR_ERROR;
		}
		p++;
		/* variable name starts now */
		name = p;
		read_until(&p, " \t=!<>");
		if (!*p)
		{
			config_error("%s:%i: invalid if statement (termination error): %s",
				filename, linenumber, statement);
			return PREPROCESSOR_ERROR;
		}
		name_terminate = p;
		if (!parse_compare_op_and_value(&p, statement, filename, linenumber, &compare_op, &value))
			return PREPROCESSOR_ERROR;
		*name_terminate = '\0';
		cc = safe_alloc(sizeof(ConditionalConfig));
		cc->condition = IF_VALUE;
		cc->compare_op = compare_op;
		safe_strdup(cc->name, name);
		safe_strdup(cc->opt, value);
		*cc_out = cc;
		return PREPROCESSOR_IF;
	}

	config_error("%s:%i: Error while evaluating '@if' statement '%s'",
		filename, linenumber, statement);
	return PREPROCESSOR_ERROR;
}

/** Resolve a value-returning function for use in @define.
 * Matches against if_functions[] entries that have returns_value=1.
 * @param p          Position at the function name
 * @param statement  Full statement (for error messages)
 * @param filename   Config filename (for error messages)
 * @param linenumber Line number (for error messages)
 * @returns The resolved string value, or NULL on error.
 */
static const char *resolve_define_function(char *p, char *statement,
                                           const char *filename, int linenumber)
{
	int i;
	char *arg;

	for (i = 0; i < ARRAY_SIZEOF(if_functions); i++)
	{
		int len = strlen(if_functions[i].keyword);
		if (strncmp(p, if_functions[i].keyword, len))
			continue;

		if (!if_functions[i].returns_value)
		{
			config_error("%s:%i: @define: function '%s' does not return a value",
				filename, linenumber, if_functions[i].keyword);
			return NULL;
		}

		/* Parse ("argument") */
		p += len;
		skip_whitespace(&p);
		if (*p != '(')
		{
			config_error("%s:%i: @define: expected '(' for %s(...",
				filename, linenumber, if_functions[i].keyword);
			return NULL;
		}
		p++;
		skip_whitespace(&p);
		if (*p == '"')
			p++;
		arg = p;
		read_until(&p, ")\"");
		if (!*p)
		{
			config_error("%s:%i: @define: invalid function call (termination error): %s",
				filename, linenumber, statement);
			return NULL;
		}
		*p = '\0';

		/* Resolve based on condition type */
		if (if_functions[i].condition == IF_ENVIRONMENT)
		{
			const char *env = getenv(arg);
			if (!env)
			{
				config_error("%s:%i: @define: environment variable '%s' is not set",
					filename, linenumber, arg);
				return NULL;
			}
			return env;
		}

		if (if_functions[i].condition == IF_MODULE_VERSION)
		{
			/* Find module using same logic as is_module_loaded() */
			Module *mod;
			for (mod = Modules; mod; mod = mod->next)
			{
				if (mod->flags & MODFLAG_DELAYED)
					continue;
				if ((loop.config_status < CONFIG_STATUS_LOAD) &&
				    (loop.config_status >= CONFIG_STATUS_POSTTEST) &&
				    (mod->flags == MODFLAG_LOADED))
				{
					continue;
				}
				if (!strcasecmp(mod->relpath, arg))
					break;
			}
			if (!mod)
			{
				config_error("%s:%i: @define: module '%s' is not loaded",
					filename, linenumber, arg);
				return NULL;
			}
			if (!mod->header->version)
			{
				config_error("%s:%i: @define: module '%s' has no version information",
					filename, linenumber, arg);
				return NULL;
			}
			return mod->header->version;
		}

		/* Future value-returning functions go here */
		config_error("%s:%i: @define: [BUG] unhandled value-returning function '%s'",
			filename, linenumber, if_functions[i].keyword);
		return NULL;
	}

	config_error("%s:%i: @define: expected a quoted string or a value-returning function like environment(): %s",
		filename, linenumber, statement);
	return NULL;
}

PreprocessorItem  evaluate_preprocessor_define(char *statement, const char *filename, int linenumber)
{
	char *p = statement;
	char *name, *name_terminator;
	const char *value;

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
	*name_terminator = '\0';
	if (*p == '"')
	{
		/* Literal string: @define $VAR "value" */
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
	} else
	{
		/* Value-returning function: @define $VAR environment("VARNAME") */
		value = resolve_define_function(p, statement, filename, linenumber);
		if (!value)
			return PREPROCESSOR_ERROR;
	}

	if (*name != '$')
	{
		config_error("%s:%i: the defined variable should start with a dollar sign ($), "
		             "so: @define $something \"123\" and not @define something \"123\"",
		             filename, linenumber);
		return PREPROCESSOR_ERROR;
	}
	/* Skip dollar sign */
	name++;
	for (p = name; *p; p++)
	{
		if (!ValidVarCharacter(*p))
		{
			config_error("%s:%i: A $VARIABLE name may only contain UPPERcase characters, "
			             "digits, and the _ character. Illegal character: '%c'",
			             filename, linenumber, *p);
			return PREPROCESSOR_ERROR;
		}
	}

	if (strlen(value) > 512)
	{
		config_error("%s:%i: Value of defined variable is extremely large (%ld characters)!",
		             filename, linenumber, (long)strlen(value));
		return PREPROCESSOR_ERROR;
	}

	add_nvplist(&config_defines, 0, name, value);
	return PREPROCESSOR_DEFINE;
}

PreprocessorItem  parse_preprocessor_item(char *start, char *end, const char *filename, int linenumber, ConditionalConfig **cc)
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
	else if (!strncmp(buf, "@else", 5))
		return PREPROCESSOR_ELSE;
	else if (!strncmp(buf, "@endif", 6))
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
		cc->compare_op = r->compare_op;
		cc->had_else = r->had_else;
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

/** Resolve a preprocessor condition to true (=default) or false */
int preprocessor_resolve_if(ConditionalConfig *cc, PreprocessorPhase phase)
{
	int result;

	for (; cc; cc = cc->next)
	{
		result = 0;

		if (cc->condition == IF_MODULE_LOADED)
		{
			if (phase == PREPROCESSOR_PHASE_INITIAL)
			{
				/* We cannot handle @if module-loaded() yet.. */
				result = 1;
			} else
			if (phase == PREPROCESSOR_PHASE_SECONDARY)
			{
				/* We can only handle blacklisted modules at this point, so: */
				if (!is_blacklisted_module(cc->name))
					result = 1;
			} else
			if (is_module_loaded(cc->name))
			{
				result = 1;
			}
		} else
		if (cc->condition == IF_MODULE_VERSION)
		{
			if (phase == PREPROCESSOR_PHASE_INITIAL || phase == PREPROCESSOR_PHASE_SECONDARY)
			{
				/* Modules are not loaded yet, default to true */
				result = 1;
			} else
			{
				/* Find the module using the same logic as is_module_loaded(),
				 * so during REHASH we find the new module, not the old one.
				 */
				Module *mod;
				for (mod = Modules; mod; mod = mod->next)
				{
					if (mod->flags & MODFLAG_DELAYED)
						continue; /* unloading (delayed) */
					if ((loop.config_status < CONFIG_STATUS_LOAD) &&
					    (loop.config_status >= CONFIG_STATUS_POSTTEST) &&
					    (mod->flags == MODFLAG_LOADED))
					{
						continue;
					}
					if (!strcasecmp(mod->relpath, cc->name))
						break;
				}
				if (mod && mod->header->version)
				{
					if (cc->opt)
					{
						int cmp = strnatcasecmp(mod->header->version, cc->opt);
						switch (cc->compare_op)
						{
							case COMPARE_EQ: result = (cmp == 0); break;
							case COMPARE_NE: result = (cmp != 0); break;
							case COMPARE_GT: result = (cmp > 0);  break;
							case COMPARE_GE: result = (cmp >= 0); break;
							case COMPARE_LT: result = (cmp < 0);  break;
							case COMPARE_LE: result = (cmp <= 0); break;
						}
					} else
					{
						/* Boolean: module is loaded (has a version) */
						result = 1;
					}
				}
			}
		} else
		if (cc->condition == IF_MODULE_EXISTS)
		{
			const char *fullpath = Module_TransformPath(cc->name);
			if (file_exists(fullpath))
				result = 1;
		} else
		if (cc->condition == IF_MINIMUM_VERSION)
		{
			if (strnatcasecmp(VERSIONONLY, cc->name) >= 0)
				result = 1;
		} else
		if (cc->condition == IF_FILE_EXISTS)
		{
			char *fullpath = convert_to_absolute_path_duplicate(cc->name, CONFDIR);
			if (file_exists(fullpath))
				result = 1;
			safe_free(fullpath);
		} else
		if (cc->condition == IF_DEFINED)
		{
			NameValuePrioList *d = find_config_define(cc->name);
			if (d)
			{
				result = 1;
			}
		} else
		if (cc->condition == IF_ENVIRONMENT)
		{
			const char *env = getenv(cc->name);
			if (env)
			{
				if (cc->opt)
				{
					int cmp = strnatcasecmp(env, cc->opt);
					switch (cc->compare_op)
					{
						case COMPARE_EQ: result = (cmp == 0); break;
						case COMPARE_NE: result = (cmp != 0); break;
						case COMPARE_GT: result = (cmp > 0);  break;
						case COMPARE_GE: result = (cmp >= 0); break;
						case COMPARE_LT: result = (cmp < 0);  break;
						case COMPARE_LE: result = (cmp <= 0); break;
					}
				} else
				{
					/* Boolean: environment variable is set */
					result = 1;
				}
			}
		} else
		if (cc->condition == IF_VALUE)
		{
			NameValuePrioList *d = find_config_define(cc->name);
			if (d)
			{
				int cmp = strnatcasecmp(d->value, cc->opt);
				switch (cc->compare_op)
				{
					case COMPARE_EQ: result = (cmp == 0); break;
					case COMPARE_NE: result = (cmp != 0); break;
					case COMPARE_GT: result = (cmp > 0);  break;
					case COMPARE_GE: result = (cmp >= 0); break;
					case COMPARE_LT: result = (cmp < 0);  break;
					case COMPARE_LE: result = (cmp <= 0); break;
				}
			}
		} else
		{
			config_status("[BUG] unhandled @if type!!");
		}

		if (cc->negative)
			result = result ? 0 : 1;

		/* All conditions must be true (logical AND for nested @if) */
		if (!result)
			return 0;
	}

	return 1;
}

void preprocessor_resolve_conditionals_ce(ConfigEntry **ce_list, PreprocessorPhase phase)
{
	ConfigEntry *ce, *next, *ce_prev;
	ConfigEntry *cep, *cep_next, *cep_prev;

	ce_prev = NULL;
	for (ce = *ce_list; ce; ce = next)
	{
		next = ce->next;
		/* This is for an @if before a block start */
		if (!preprocessor_resolve_if(ce->conditional_config, phase))
		{
			/* Delete this entry */
			if (ce == *ce_list)
			{
				/* we are head, so new head */
				*ce_list = ce->next; /* can be NULL now */
			} else {
				/* non-head */
				ce_prev->next = ce->next; /* can be NULL now */
			}
			config_entry_free(ce);
			continue;
		}
		preprocessor_resolve_conditionals_ce(&ce->items, phase);
		ce_prev = ce;
	}
}

void preprocessor_resolve_conditionals_all(PreprocessorPhase phase)
{
	ConfigFile *cfptr;

	for (cfptr = conf; cfptr; cfptr = cfptr->next)
		preprocessor_resolve_conditionals_ce(&cfptr->items, phase);
}

/** Frees the list of config_defines, so all @defines and then initialize the
 * build-in ones.
 */
void init_config_defines(void)
{
	safe_free_nvplist(config_defines);
	add_nvplist(&config_defines, 0, "UNREALIRCD_VERSION", VERSIONONLY);
	add_nvplist(&config_defines, 0, "UNREALIRCD_VERSION_GENERATION", PATCH1);
	add_nvplist(&config_defines, 0, "UNREALIRCD_VERSION_MAJOR", PATCH2);
	add_nvplist(&config_defines, 0, "UNREALIRCD_VERSION_MINOR", PATCH3);
	add_nvplist(&config_defines, 0, "UNREALIRCD_VERSION_SUFFIX", PATCH4);
#ifdef GEOIP_ENGINE
	add_nvplist(&config_defines, 0, "GEOIP_ENGINE", GEOIP_ENGINE);
#else
	add_nvplist(&config_defines, 0, "GEOIP_ENGINE", "none");
#endif
	/* Directory paths */
	add_nvplist(&config_defines, 0, "CONFDIR", CONFDIR);
	add_nvplist(&config_defines, 0, "DATADIR", PERMDATADIR);
	add_nvplist(&config_defines, 0, "LOGDIR", LOGDIR);
	add_nvplist(&config_defines, 0, "TMPDIR", TMPDIR);
	add_nvplist(&config_defines, 0, "DOCDIR", DOCDIR);
	add_nvplist(&config_defines, 0, "MODULESDIR", MODULESDIR);
	/* Max connections */
	add_nvplist(&config_defines, 0, "MAXCONNECTIONS", macro_to_str(MAXCONNECTIONS));
}

/** Return the complete struct for a defined value */
NameValuePrioList *find_config_define(const char *name)
{
	return find_nvplist(config_defines, name);
}

/** Return value of defined value */
char *get_config_define(char *name)
{
	NameValuePrioList *e = find_nvplist(config_defines, name);
	return e ? e->value : NULL;
}

void preprocessor_replace_defines(char **item, ConfigEntry *ce)
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
		for (; *i && ValidVarCharacter(*i); i++);
		varend = i;
		i--;
		limit = varend - varstart + 1;
		if (limit > sizeof(varname))
			limit = sizeof(varname);
		strlcpy(varname, varstart, limit);
		if (!strncmp(varstart, "$$", 2))
		{
			/* If we get here then we encountered "$$"
			 * and varname is just "$".
			 * This means we are hitting a $ escape sequence,
			 * the $$ is used as a literal $.
			 * Would be very rare if you need it, but.. we support it.
			 */
			value = varname; /* bit confusing, but no +1 here */
			i++; /* skip extra $ in input */
		} else
		{
			value = get_config_define(varname+1);
			if (!value)
			{
#if 0
				/* Complain about $VARS if they are not defined, but don't bother
				 * for cases where it's clearly not a macro, eg. contains illegal
				 * variable characters.
				 */
				if ((limit > 2) && ((*varend == '\0') || strchr("\t ,.", *varend)))
				{
					config_warn("%s:%d: Variable %s used here but there's no @define for it earlier.",
						    ce->file->filename, ce->line_number, varname);
				}
#endif
				value = varname; /* not found? then use varname, including the '$' */
			}
		}
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
