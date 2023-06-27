/*
 * User Mode +G
 * (C) Copyright 2005-current Bram Matthys and The UnrealIRCd team.
 */

#include "unrealircd.h"


ModuleHeader MOD_HEADER
  = {
	"usermodes/censor",
	"4.2",
	"User Mode +G",
	"UnrealIRCd Team",
	"unrealircd-6",
    };


long UMODE_CENSOR = 0L;

#define IsCensored(x) (x->umodes & UMODE_CENSOR)

int censor_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype);

int censor_config_test(ConfigFile *, ConfigEntry *, int, int *);
int censor_config_run(ConfigFile *, ConfigEntry *, int);

ModuleInfo *ModInfo = NULL;

ConfigItem_badword *conf_badword_message = NULL;

static ConfigItem_badword *copy_badword_struct(ConfigItem_badword *ca, int regex, int regflags);

int censor_stats_badwords_user(Client *client, const char *para);

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, censor_config_test);
	return MOD_SUCCESS;
}
	
MOD_INIT()
{
	ModInfo = modinfo;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	UmodeAdd(modinfo->handle, 'G', UMODE_GLOBAL, 0, NULL, &UMODE_CENSOR);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_USER, 0, censor_can_send_to_user);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, censor_stats_badwords_user);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, censor_config_run);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}


MOD_UNLOAD()
{
ConfigItem_badword *badword, *next;

	for (badword = conf_badword_message; badword; badword = next)
	{
		next = badword->next;
		DelListItem(badword, conf_badword_message);
		badword_config_free(badword);
	}
	return MOD_SUCCESS;
}

int censor_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;
	char has_word = 0, has_replace = 0, has_action = 0, action = 'r';

	if (type != CONFIG_MAIN)
		return 0;
	
	if (!ce || !ce->name || strcmp(ce->name, "badword"))
		return 0; /* not interested */

	if (!ce->value)
	{
		config_error("%s:%i: badword without type",
			ce->file->filename, ce->line_number);
		*errs = 1;
		return -1;
	}
	else if (strcmp(ce->value, "message") && strcmp(ce->value, "all")) {
/*			config_error("%s:%i: badword with unknown type",
				ce->file->filename, ce->line_number); -- can't do that.. */
		return 0; /* unhandled */
	}
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (config_is_blankorempty(cep, "badword"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->name, "word"))
		{
			const char *errbuf;
			if (has_word)
			{
				config_warn_duplicate(cep->file->filename, 
					cep->line_number, "badword::word");
				continue;
			}
			has_word = 1;
			if ((errbuf = badword_config_check_regex(cep->value,1,1)))
			{
				config_error("%s:%i: badword::%s contains an invalid regex: %s",
					cep->file->filename,
					cep->line_number,
					cep->name, errbuf);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "replace"))
		{
			if (has_replace)
			{
				config_warn_duplicate(cep->file->filename, 
					cep->line_number, "badword::replace");
				continue;
			}
			has_replace = 1;
		}
		else if (!strcmp(cep->name, "action"))
		{
			if (has_action)
			{
				config_warn_duplicate(cep->file->filename, 
					cep->line_number, "badword::action");
				continue;
			}
			has_action = 1;
			if (!strcmp(cep->value, "replace"))
				action = 'r';
			else if (!strcmp(cep->value, "block"))
				action = 'b';
			else
			{
				config_error("%s:%d: Unknown badword::action '%s'",
					cep->file->filename, cep->line_number,
					cep->value);
				errors++;
			}
				
		}
		else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"badword", cep->name);
			errors++;
		}
	}

	if (!has_word)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"badword::word");
		errors++;
	}
	if (has_action)
	{
		if (has_replace && action == 'b')
		{
			config_error("%s:%i: badword::action is block but badword::replace exists",
				ce->file->filename, ce->line_number);
			errors++;
		}
	}
	
	*errs = errors;
	return errors ? -1 : 1;
}


int censor_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *word = NULL;
	ConfigItem_badword *ca;

	if (type != CONFIG_MAIN)
		return 0;
	
	if (!ce || !ce->name || strcmp(ce->name, "badword"))
		return 0; /* not interested */

	if (strcmp(ce->value, "message") && strcmp(ce->value, "all"))
	        return 0; /* not for us */

	ca = safe_alloc(sizeof(ConfigItem_badword));
	ca->action = BADWORD_REPLACE;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "action"))
		{
			if (!strcmp(cep->value, "block"))
			{
				ca->action = BADWORD_BLOCK;
			}
		}
		else if (!strcmp(cep->name, "replace"))
		{
			safe_strdup(ca->replace, cep->value);
		}
		else if (!strcmp(cep->name, "word"))
		{
			word = cep;
		}
	}

	badword_config_process(ca, word->value);

	if (!strcmp(ce->value, "message"))
	{
		AddListItem(ca, conf_badword_message);
	} else
	if (!strcmp(ce->value, "all"))
	{
		AddListItem(ca, conf_badword_message);
		return 0; /* pretend we didn't see it, so other modules can handle 'all' as well */
	}

	return 1;
}

const char *stripbadwords_message(const char *str, int *blocked)
{
	return stripbadwords(str, conf_badword_message, blocked);
}

int censor_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype)
{
	int blocked = 0;

	if (MyUser(client) && IsCensored(target))
	{
		*text = stripbadwords_message(*text, &blocked);
		if (blocked)
		{
			*errmsg = "User does not accept private messages containing swearing";
			return HOOK_DENY;
		}
	}

	return HOOK_CONTINUE;
}

int censor_stats_badwords_user(Client *client, const char *para)
{
	ConfigItem_badword *words;

	if (!para || !(!strcmp(para, "b") || !strcasecmp(para, "badword")))
		return 0;

	for (words = conf_badword_message; words; words = words->next)
	{
		sendtxtnumeric(client, "m %c %s%s%s %s", words->type & BADW_TYPE_REGEX ? 'R' : 'F',
		           (words->type & BADW_TYPE_FAST_L) ? "*" : "", words->word,
		           (words->type & BADW_TYPE_FAST_R) ? "*" : "",
		           words->action == BADWORD_REPLACE ? (words->replace ? words->replace : "<censored>") : "");
	}
	return 1;
}
