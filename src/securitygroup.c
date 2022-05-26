/*
 * Mask & security-group routines.
 * (C) Copyright 2015-.. Syzop and the UnrealIRCd team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "unrealircd.h"

/* Global variables */
SecurityGroup *securitygroups = NULL;

/** Free all masks in the mask list */
void unreal_delete_masks(ConfigItem_mask *m)
{
	ConfigItem_mask *m_next;

	for (; m; m = m_next)
	{
		m_next = m->next;

		safe_free(m->mask);

		safe_free(m);
	}
}

/** Internal function to add one individual mask to the list */
static void unreal_add_mask(ConfigItem_mask **head, ConfigEntry *ce)
{
	ConfigItem_mask *m = safe_alloc(sizeof(ConfigItem_mask));

	/* Since we allow both mask "xyz"; and mask { abc; def; };... */
	if (ce->value)
		safe_strdup(m->mask, ce->value);
	else
		safe_strdup(m->mask, ce->name);

	add_ListItem((ListStruct *)m, (ListStruct **)head);
}

/** Add mask entries from config */
void unreal_add_masks(ConfigItem_mask **head, ConfigEntry *ce)
{
	if (ce->items)
	{
		ConfigEntry *cep;
		for (cep = ce->items; cep; cep = cep->next)
			unreal_add_mask(head, cep);
	} else
	{
		unreal_add_mask(head, ce);
	}
}

/** Check if a client matches any of the masks in the mask list.
 * The following rules apply:
 * - If you have only negating entries, like '!abc' and '!def', then
 *   we assume an implicit * rule first, since that is clearly what
 *   the user wants.
 * - If you have a mix, like '*.com', '!irc1*', '!irc2*' then the
 *   implicit * is dropped and we assume you only want to match *.com,
 *   with the exception of irc1*.com and irc2*.com.
 * - If you only have normal entries without ! then things are
 *   as they always are.
 * @param client	The client to run the mask match against
 * @param mask		The mask entry from the config file
 * @returns 1 on match, 0 on non-match.
 */
int unreal_mask_match(Client *client, ConfigItem_mask *mask)
{
	int retval = 1;
	ConfigItem_mask *m;

	if (!mask)
		return 0; /* Empty mask block is no match */

	/* First check normal matches (without ! prefix) */
	for (m = mask; m; m = m->next)
	{
		if (m->mask[0] != '!')
		{
			retval = 0; /* no implicit * */
			if (match_user(m->mask, client, MATCH_CHECK_REAL|MATCH_CHECK_EXTENDED))
			{
				retval = 1;
				break;
			}
		}
	}

	if (retval)
	{
		/* We matched. Check for exceptions (with ! prefix) */
		for (m = mask; m; m = m->next)
		{
			if ((m->mask[0] == '!') && match_user(m->mask+1, client, MATCH_CHECK_REAL|MATCH_CHECK_EXTENDED))
				return 0;
		}
	}

	return retval;
}

/** Check if a string matches any of the masks in the mask list.
 * The following rules apply:
 * - If you have only negating entries, like '!abc' and '!def', then
 *   we assume an implicit * rule first, since that is clearly what
 *   the user wants.
 * - If you have a mix, like '*.com', '!irc1*', '!irc2*' then the
 *   implicit * is dropped and we assume you only want to match *.com,
 *   with the exception of irc1*.com and irc2*.com.
 * - If you only have normal entries without ! then things are
 *   as they always are.
 * @param name	The name to run the mask matching on
 * @param mask	The mask entry from the config file
 * @returns 1 on match, 0 on non-match.
 */
int unreal_mask_match_string(const char *name, ConfigItem_mask *mask)
{
	int retval = 1;
	ConfigItem_mask *m;

	if (!mask)
		return 0; /* Empty mask block is no match */

	/* First check normal matches (without ! prefix) */
	for (m = mask; m; m = m->next)
	{
		if (m->mask[0] != '!')
		{
			retval = 0; /* no implicit * */
			if (match_simple(m->mask, name))
			{
				retval = 1;
				break;
			}
		}
	}

	if (retval)
	{
		/* We matched. Check for exceptions (with ! prefix) */
		for (m = mask; m; m = m->next)
		{
			if ((m->mask[0] == '!') && match_simple(m->mask+1, name))
				return 0;
		}
	}

	return retval;
}

#define CheckNullX(x) if ((!(x)->value) || (!(*((x)->value)))) { config_error("%s:%i: missing parameter", (x)->file->filename, (x)->line_number); *errors = *errors + 1; return 0; }
int test_match_item(ConfigFile *conf, ConfigEntry *cep, int *errors)
{
	if (!strcmp(cep->name, "webirc") || !strcmp(cep->name, "exclude-webirc"))
	{
		CheckNullX(cep);
	} else
	if (!strcmp(cep->name, "identified") || !strcmp(cep->name, "exclude-identified"))
	{
		CheckNullX(cep);
	} else
	if (!strcmp(cep->name, "tls") || !strcmp(cep->name, "exclude-tls"))
	{
		CheckNullX(cep);
	} else
	if (!strcmp(cep->name, "reputation-score") || !strcmp(cep->name, "exclude-reputation-score"))
	{
		const char *str = cep->value;
		int v;
		CheckNullX(cep);
		if (*str == '<')
			str++;
		v = atoi(str);
		if ((v < 1) || (v > 10000))
		{
			config_error("%s:%i: %s needs to be a value of 1-10000",
				cep->file->filename, cep->line_number, cep->name);
			*errors = *errors + 1;
		}
	} else
	if (!strcmp(cep->name, "connect-time") || !strcmp(cep->name, "exclude-connect-time"))
	{
		const char *str = cep->value;
		long v;
		CheckNullX(cep);
		if (*str == '<')
			str++;
		v = config_checkval(str, CFG_TIME);
		if (v < 1)
		{
			config_error("%s:%i: %s needs to be a time value (and more than 0 seconds)",
				cep->file->filename, cep->line_number, cep->name);
			*errors = *errors + 1;
		}
	} else
	if (!strcmp(cep->name, "mask") || !strcmp(cep->name, "include-mask") || !strcmp(cep->name, "exclude-mask"))
	{
	} else
	if (!strcmp(cep->name, "ip"))
	{
	} else
	if (!strcmp(cep->name, "security-group") || !strcmp(cep->name, "exclude-security-group"))
	{
	} else
	{
		/* Let's see if an extended server ban exists for this item... */
		Extban *extban;
		if (!strncmp(cep->name, "exclude-", 8))
			extban = findmod_by_bantype_raw(cep->name+8, strlen(cep->name+8));
		else
			extban = findmod_by_bantype_raw(cep->name, strlen(cep->name));
		if (extban && (extban->options & EXTBOPT_TKL) && (extban->is_banned_events & BANCHK_TKL))
		{
			test_extended_list(extban, cep, errors);
			return 1; /* Yup, handled */
		}
		return 0; /* Unhandled: unknown item for us */
	}
	return 1; /* Handled, but there could be errors */
}

int test_match_block(ConfigFile *conf, ConfigEntry *ce, int *errors_out)
{
	int errors = 0;
	ConfigEntry *cep;

	/* (If there is only a ce->value, trust that it is OK) */

	/* Test ce->items... */
	for (cep = ce->items; cep; cep = cep->next)
	{
		/* Only complain about things with values,
		 * as valueless things like "10.0.0.0/8" are treated as a mask.
		 */
		if (!test_match_item(conf, cep, &errors) && cep->value)
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				ce->name, cep->name);
			errors++;
			continue;
		}
	}

	*errors_out = *errors_out + errors;
	return errors ? 0 : 1;
}

#define tmbbw_is_wildcard(x)	(!strcmp(x, "*") || !strcmp(x, "*@*"))
int test_match_block_too_broad(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;

	// match *;
	if (ce->value && tmbbw_is_wildcard(ce->value))
		return 1;

	for (cep = ce->items; cep; cep = cep->next)
	{
		// match { *; }
		if (!cep->value && tmbbw_is_wildcard(cep->name))
			return 1;
		if (!strcmp(cep->name, "mask") || !strcmp(cep->name, "include-mask") || !strcmp(cep->name, "ip"))
		{
			// match { mask *; }
			if (cep->value && tmbbw_is_wildcard(cep->value))
				return 1;
			// match { mask { *; } }
			for (cepp = cep->items; cepp; cepp = cepp->next)
				if (tmbbw_is_wildcard(cepp->name))
					return 1;
		}
	}

	return 0;
}

int _test_security_group(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	ConfigEntry *cep;

	/* First, check the name of the security group */
	if (!ce->value)
	{
		config_error("%s:%i: security-group block needs a name, eg: security-group web-users {",
			ce->file->filename, ce->line_number);
		errors++;
	} else {
		if (!strcasecmp(ce->value, "unknown-users"))
		{
			config_error("%s:%i: The 'unknown-users' group is a special group that is the "
			             "inverse of 'known-users', you cannot create or adjust it in the "
			             "config file, as it is created automatically by UnrealIRCd.",
			             ce->file->filename, ce->line_number);
			errors++;
			return errors;
		}
		if (!security_group_valid_name(ce->value))
		{
			config_error("%s:%i: security-group block name '%s' contains invalid characters or is too long. "
			             "Only letters, numbers, underscore and hyphen are allowed.",
			             ce->file->filename, ce->line_number, ce->value);
			errors++;
		}
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!test_match_item(conf, cep, &errors))
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"security-group", cep->name);
			errors++;
			continue;
		}
	}

	return errors;
}

int conf_match_item(ConfigFile *conf, ConfigEntry *cep, SecurityGroup **block)
{
	int errors = 0; /* unused */
	SecurityGroup *s = *block;

	/* The following code is there so we don't create a security group
	 * unless there is actually a valid config item for it encountered.
	 * This so the security group '*s' can stay NULL if there are zero
	 * items, so we don't waste any CPU if it is unused.
	 */
	if (*block == NULL)
	{
		/* Yeah we call a TEST routine from a CONFIG RUN routine ;). */
		if (!test_match_item(conf, cep, &errors))
			return 0; /* not for us */
		/* If we are still here then we must create the security group */
		*block = s = safe_alloc(sizeof(SecurityGroup));
	}

	if (!strcmp(cep->name, "webirc"))
		s->webirc = config_checkval(cep->value, CFG_YESNO);
	else if (!strcmp(cep->name, "identified"))
		s->identified = config_checkval(cep->value, CFG_YESNO);
	else if (!strcmp(cep->name, "tls"))
		s->tls = config_checkval(cep->value, CFG_YESNO);
	else if (!strcmp(cep->name, "reputation-score"))
	{
		if (*cep->value == '<')
			s->reputation_score = 0 - atoi(cep->value+1);
		else
			s->reputation_score = atoi(cep->value);
	}
	else if (!strcmp(cep->name, "connect-time"))
	{
		if (*cep->value == '<')
			s->connect_time = 0 - config_checkval(cep->value+1, CFG_TIME);
		else
			s->connect_time = config_checkval(cep->value, CFG_TIME);
	}
	else if (!strcmp(cep->name, "mask") || !strcmp(cep->name, "include-mask"))
	{
		unreal_add_masks(&s->mask, cep);
	}
	else if (!strcmp(cep->name, "ip"))
	{
		unreal_add_names(&s->ip, cep);
	}
	else if (!strcmp(cep->name, "security-group"))
	{
		unreal_add_names(&s->security_group, cep);
	}
	else if (!strcmp(cep->name, "exclude-webirc"))
		s->exclude_webirc = config_checkval(cep->value, CFG_YESNO);
	else if (!strcmp(cep->name, "exclude-identified"))
		s->exclude_identified = config_checkval(cep->value, CFG_YESNO);
	else if (!strcmp(cep->name, "exclude-tls"))
		s->exclude_tls = config_checkval(cep->value, CFG_YESNO);
	else if (!strcmp(cep->name, "exclude-reputation-score"))
	{
		if (*cep->value == '<')
			s->exclude_reputation_score = 0 - atoi(cep->value+1);
		else
			s->exclude_reputation_score = atoi(cep->value);
	}
	else if (!strcmp(cep->name, "exclude-mask"))
	{
		unreal_add_masks(&s->exclude_mask, cep);
	}
	else if (!strcmp(cep->name, "exclude-security-group"))
	{
		unreal_add_names(&s->security_group, cep);
	}
	else
	{
		/* Let's see if an extended server ban exists for this item... this needs to be LAST! */
		Extban *extban;
		const char *name = cep->name;

		if (!strncmp(cep->name, "exclude-", 8))
		{
			/* Extended (exclusive) ? */
			name = cep->name + 8;
			if (findmod_by_bantype_raw(name, strlen(name)))
				unreal_add_name_values(&s->exclude_extended, name, cep);
			else
				return 0; /* Unhandled */
		} else {
			/* Extended (inclusive) */
			if (findmod_by_bantype_raw(name, strlen(name)))
				unreal_add_name_values(&s->extended, name, cep);
			else
				return 0; /* Unhandled */
		}
	}

	add_nvplist(&s->printable_list, s->printable_list_counter++, cep->name, cep->value);

	return 1; /* Handled by us (guaranteed earlier) */
}

int conf_match_block(ConfigFile *conf, ConfigEntry *ce, SecurityGroup **block)
{
	ConfigEntry *cep;
	SecurityGroup *s = *block;

	if (*block == NULL)
		*block = s = safe_alloc(sizeof(SecurityGroup));

	/* Check for simple form: match *; / mask *; */
	if (ce->value)
	{
		unreal_add_masks(&s->mask, ce);
		add_nvplist(&s->printable_list, s->printable_list_counter++, "mask", ce->value);
	}

	/* Check for long form: match { .... } / mask { .... } */
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!conf_match_item(conf, cep, &s) && !cep->value && !cep->items)
		{
			/* Valueless? Then it must be a mask like 10.0.0.0/8 */
			unreal_add_masks(&s->mask, cep);
			add_nvplist(&s->printable_list, s->printable_list_counter++, "mask", cep->name);
		}
	}
	return 1;
}

int _conf_security_group(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	SecurityGroup *s = add_security_group(ce->value, 1);

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "priority"))
		{
			s->priority = atoi(cep->value);
			DelListItem(s, securitygroups);
			AddListItemPrio(s, securitygroups, s->priority);
		} else
			conf_match_item(conf, cep, &s);
	}
	return 1;
}

/** Check if the name of the security-group contains only valid characters.
 * @param name	The name of the group
 * @returns 1 if name is valid, 0 if not (eg: illegal characters)
 */
int security_group_valid_name(const char *name)
{
	const char *p;

	if (strlen(name) > SECURITYGROUPLEN)
		return 0; /* Too long */

	for (p = name; *p; p++)
	{
		if (!isalnum(*p) && !strchr("_-", *p))
			return 0; /* Character not allowed */
	}
	return 1;
}

/** Find a security-group.
 * @param name	The name of the security group
 * @returns A SecurityGroup struct, or NULL if not found.
 */
SecurityGroup *find_security_group(const char *name)
{
	SecurityGroup *s;
	for (s = securitygroups; s; s = s->next)
		if (!strcasecmp(name, s->name))
			return s;
	return NULL;
}

/** Checks if a security-group exists.
 * This function takes the 'unknown-users' magic group into account as well.
 * @param name	The name of the security group
 * @returns 1 if it exists, 0 if not
 */
int security_group_exists(const char *name)
{
	if (!strcmp(name, "unknown-users") || find_security_group(name))
		return 1;
	return 0;
}

/** Add a new security-group and add it to the list, but search for existing one first.
 * @param name	The name of the security group
 * @returns A SecurityGroup struct (already added to the 'securitygroups' linked list)
 */
SecurityGroup *add_security_group(const char *name, int priority)
{
	SecurityGroup *s = find_security_group(name);

	/* Existing? */
	if (s)
		return s;

	/* Otherwise, create a new entry */
	s = safe_alloc(sizeof(SecurityGroup));
	strlcpy(s->name, name, sizeof(s->name));
	s->priority = priority;
	AddListItemPrio(s, securitygroups, priority);
	return s;
}

/** Free a SecurityGroup struct */
void free_security_group(SecurityGroup *s)
{
	if (s == NULL)
		return;
	unreal_delete_masks(s->mask);
	unreal_delete_masks(s->exclude_mask);
	free_entire_name_list(s->security_group);
	free_entire_name_list(s->exclude_security_group);
	free_entire_name_list(s->ip);
	free_entire_name_list(s->exclude_ip);
	free_nvplist(s->extended);
	free_nvplist(s->exclude_extended);
	free_nvplist(s->printable_list);
	safe_free(s);
}

/** Initialize the default security-group blocks */
void set_security_group_defaults(void)
{
	SecurityGroup *s, *s_next;

	/* First free all security groups */
	for (s = securitygroups; s; s = s_next)
	{
		s_next = s->next;
		free_security_group(s);
	}
	securitygroups = NULL;

	/* Default group: webirc */
	s = add_security_group("webirc-users", 50);
	s->webirc = 1;

	/* Default group: known-users */
	s = add_security_group("known-users", 100);
	s->identified = 1;
	s->reputation_score = 25;
	s->webirc = 0;

	/* Default group: tls-and-known-users */
	s = add_security_group("tls-and-known-users", 200);
	s->identified = 1;
	s->reputation_score = 25;
	s->webirc = 0;
	s->tls = 1;

	/* Default group: tls-users */
	s = add_security_group("tls-users", 300);
	s->tls = 1;
}

int user_matches_extended_list(Client *client, NameValuePrioList *e)
{
	Extban *extban;
	BanContext b;

	for (; e; e = e->next)
	{
		extban = findmod_by_bantype_raw(e->name, strlen(e->name));
		if (!extban ||
		    !(extban->options & EXTBOPT_TKL) ||
		    !(extban->is_banned_events & BANCHK_TKL))
		{
			continue; /* extban not found or of incorrect type */
		}

		memset(&b, 0, sizeof(BanContext));
		b.client = client;
		b.banstr = e->value;
		b.ban_check_types = BANCHK_TKL;
		if (extban->is_banned(&b))
			return 1;
	}

	return 0;
}

int test_extended_list(Extban *extban, ConfigEntry *cep, int *errors)
{
	BanContext b;

	if (cep->value)
	{
		memset(&b, 0, sizeof(BanContext));
		b.banstr = cep->value;
		b.ban_check_types = BANCHK_TKL;
		b.what = MODE_ADD;
		if (!extban->conv_param(&b, extban))
		{
			config_error("%s:%i: %s has an invalid value",
			             cep->file->filename, cep->line_number, cep->name);
			*errors = *errors + 1;
			return 0;
		}
	}

	for (cep = cep->items; cep; cep = cep->next)
	{
		memset(&b, 0, sizeof(BanContext));
		b.banstr = cep->name;
		b.ban_check_types = BANCHK_TKL;
		b.what = MODE_ADD;
		if (!extban->conv_param(&b, extban))
		{
			config_error("%s:%i: %s has an invalid value",
			             cep->file->filename, cep->line_number, cep->name);
			*errors = *errors + 1;
			return 0;
		}
	}

	return 1;
}

/** Returns 1 if the user is allowed by any of the security groups in the named list.
 * This is only used by security-group::security-group and
 * security-group::exclude-security-group.
 * @param client	Client to check
 * @param l		The NameList
 * @returns 1 if any of the security groups match, 0 if none of them matched.
 */
int user_allowed_by_security_group_list(Client *client, NameList *l)
{
	for (; l; l = l->next)
		if (user_allowed_by_security_group_name(client, l->name))
			return 1;
	return 0;
}

/** Returns 1 if the user is OK as far as the security-group is concerned.
 * @param client	The client to check
 * @param s		The security-group to check against
 * @retval 1 if user is allowed by security-group, 0 if not.
 */
int user_allowed_by_security_group(Client *client, SecurityGroup *s)
{
	static int recursion_security_group = 0;

	/* Allow NULL securitygroup, makes it easier in the code elsewhere */
	if (!s)
		return 0;

	if (recursion_security_group > 8)
	{
		unreal_log(ULOG_WARNING, "main", "SECURITY_GROUP_LOOP_DETECTED", client,
		           "Loop detected while processing security-group '$security_group' -- "
		           "are you perhaps referencing a security-group from a security-group?",
		           log_data_string("security_group", s->name));
		return 0;
	}
	recursion_security_group++;

	/* DO NOT USE 'return' IN CODE BELOW!!!!!!!!!
	 * - use 'goto user_not_allowed' to reject
	 * - use 'goto user_allowed' to accept
	 */

	/* Process EXCLUSION criteria first... */
	if (s->exclude_identified && IsLoggedIn(client))
		goto user_not_allowed;
	if (s->exclude_webirc && moddata_client_get(client, "webirc"))
		goto user_not_allowed;
	if ((s->exclude_reputation_score > 0) && (GetReputation(client) >= s->exclude_reputation_score))
		goto user_not_allowed;
	if ((s->exclude_reputation_score < 0) && (GetReputation(client) < 0 - s->exclude_reputation_score))
		goto user_not_allowed;
	if (s->exclude_connect_time != 0)
	{
		long connect_time = get_connected_time(client);
		if ((s->exclude_connect_time > 0) && (connect_time >= s->exclude_connect_time))
			goto user_not_allowed;
		if ((s->exclude_connect_time < 0) && (connect_time < 0 - s->exclude_connect_time))
			goto user_not_allowed;
	}
	if (s->exclude_tls && (IsSecureConnect(client) || (MyConnect(client) && IsSecure(client))))
		goto user_not_allowed;
	if (s->exclude_mask && unreal_mask_match(client, s->exclude_mask))
		goto user_not_allowed;
	if (s->exclude_ip && unreal_match_iplist(client, s->exclude_ip))
		goto user_not_allowed;
	if (s->exclude_security_group && user_allowed_by_security_group_list(client, s->exclude_security_group))
		goto user_not_allowed;
	if (s->exclude_extended && user_matches_extended_list(client, s->exclude_extended))
		goto user_not_allowed;

	/* Then process INCLUSION criteria... */
	if (s->identified && IsLoggedIn(client))
		goto user_allowed;
	if (s->webirc && moddata_client_get(client, "webirc"))
		goto user_allowed;
	if ((s->reputation_score > 0) && (GetReputation(client) >= s->reputation_score))
		goto user_allowed;
	if ((s->reputation_score < 0) && (GetReputation(client) < 0 - s->reputation_score))
		goto user_allowed;
	if (s->connect_time != 0)
	{
		long connect_time = get_connected_time(client);
		if ((s->connect_time > 0) && (connect_time >= s->connect_time))
			goto user_allowed;
		if ((s->connect_time < 0) && (connect_time < 0 - s->connect_time))
			goto user_allowed;
	}
	if (s->tls && (IsSecureConnect(client) || (MyConnect(client) && IsSecure(client))))
		goto user_allowed;
	if (s->mask && unreal_mask_match(client, s->mask))
		goto user_allowed;
	if (s->ip && unreal_match_iplist(client, s->ip))
		goto user_allowed;
	if (s->security_group && user_allowed_by_security_group_list(client, s->security_group))
		goto user_allowed;
	if (s->extended && user_matches_extended_list(client, s->extended))
		goto user_allowed;

user_not_allowed:
	recursion_security_group--;
	return 0;

user_allowed:
	recursion_security_group--;
	return 1;
}

/** Returns 1 if the user is OK as far as the security-group is concerned - "by name" version.
 * @param client	The client to check
 * @param secgroupname	The name of the security-group to check against
 * @retval 1 if user is allowed by security-group, 0 if not.
 */
int user_allowed_by_security_group_name(Client *client, const char *secgroupname)
{
	SecurityGroup *s;

	/* Handle the magical 'unknown-users' case. */
	if (!strcmp(secgroupname, "unknown-users"))
	{
		/* This is simply the inverse of 'known-users' */
		s = find_security_group("known-users");
		if (!s)
			return 0; /* that's weird!? pretty impossible. */
		return !user_allowed_by_security_group(client, s);
	}

	/* Find the group and evaluate it */
	s = find_security_group(secgroupname);
	if (!s)
		return 0; /* security group not found: no match */
	return user_allowed_by_security_group(client, s);
}

/** Get comma separated list of matching security groups for 'client'.
 * This is usually only used for displaying purposes.
 * @returns string like "unknown-users,tls-users" from a static buffer.
 */
const char *get_security_groups(Client *client)
{
	SecurityGroup *s;
	static char buf[512];

	*buf = '\0';

	/* We put known-users or unknown-users at the beginning.
	 * The latter is special and doesn't actually exist
	 * in the linked list, hence the special code here,
	 * and again later in the for loop to skip it.
	 */
	if (user_allowed_by_security_group_name(client, "known-users"))
		strlcat(buf, "known-users,", sizeof(buf));
	else
		strlcat(buf, "unknown-users,", sizeof(buf));

	for (s = securitygroups; s; s = s->next)
	{
		if (strcmp(s->name, "known-users") &&
		    user_allowed_by_security_group(client, s))
		{
			strlcat(buf, s->name, sizeof(buf));
			strlcat(buf, ",", sizeof(buf));
		}
	}

	if (*buf)
		buf[strlen(buf)-1] = '\0';
	return buf;
}
