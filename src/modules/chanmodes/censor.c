/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/censor.c
 * Channel Mode +G
 * (C) Copyright 2005-current Bram Matthys and The UnrealIRCd team.
 */

#include "unrealircd.h"


ModuleHeader MOD_HEADER
  = {
	"chanmodes/censor",
	"4.2",
	"Channel Mode +G",
	"UnrealIRCd Team",
	"unrealircd-6",
    };


Cmode_t EXTMODE_CENSOR = 0L;

#define IsCensored(x) ((x)->mode.mode & EXTMODE_CENSOR)

int censor_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);
const char *censor_pre_local_part(Client *client, Channel *channel, const char *text);
const char *censor_pre_local_quit(Client *client, const char *text);
int censor_stats_badwords_channel(Client *client, const char *para);
int censor_config_test(ConfigFile *, ConfigEntry *, int, int *);
int censor_config_run(ConfigFile *, ConfigEntry *, int);

ModuleInfo *ModInfo = NULL;

ConfigItem_badword *conf_badword_channel = NULL;


MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, censor_config_test);
	return MOD_SUCCESS;
}
	
MOD_INIT()
{
	CmodeInfo req;

	ModInfo = modinfo;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.is_ok = extcmode_default_requirechop;
	req.letter = 'G';
	CmodeAdd(modinfo->handle, req, &EXTMODE_CENSOR);

	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, censor_can_send_to_channel);
	HookAddConstString(modinfo->handle, HOOKTYPE_PRE_LOCAL_PART, 0, censor_pre_local_part);
	HookAddConstString(modinfo->handle, HOOKTYPE_PRE_LOCAL_QUIT, 0, censor_pre_local_quit);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, censor_stats_badwords_channel);
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

	for (badword = conf_badword_channel; badword; badword = next)
	{
		next = badword->next;
		DelListItem(badword, conf_badword_channel);
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
	else if (strcmp(ce->value, "channel") && 
	         strcmp(ce->value, "quit") && strcmp(ce->value, "all")) {
/*			config_error("%s:%i: badword with unknown type",
				ce->file->filename, ce->line_number); -- can't do that.. */
		return 0; /* unhandled */
	}
	
	if (!strcmp(ce->value, "quit"))
	{
		config_error("%s:%i: badword quit has been removed. We just use the bad words from "
		             "badword channel { } instead.",
		             ce->file->filename, ce->line_number);
		return 0; /* pretend unhandled.. ok not just pretend.. ;) */
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

	if (strcmp(ce->value, "channel") && strcmp(ce->value, "all"))
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
		} else
		if (!strcmp(cep->name, "word"))
		{
			word = cep;
		}
	}

	badword_config_process(ca, word->value);

	if (!strcmp(ce->value, "channel"))
		AddListItem(ca, conf_badword_channel);
	else if (!strcmp(ce->value, "all"))
	{
		AddListItem(ca, conf_badword_channel);
		return 0; /* pretend we didn't see it, so other modules can handle 'all' as well */
	}

	return 1;
}

const char *stripbadwords_channel(const char *str, int *blocked)
{
	return stripbadwords(str, conf_badword_channel, blocked);
}

int censor_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	int blocked;
	Hook *h;
	int i;

	if (!IsCensored(channel))
		return HOOK_CONTINUE;

	for (h = Hooks[HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(client, channel, BYPASS_CHANMSG_CENSOR);
		if (i == HOOK_ALLOW)
			return HOOK_CONTINUE; /* bypass censor restriction */
		if (i != HOOK_CONTINUE)
			break;
	}

	*msg = stripbadwords_channel(*msg, &blocked);
	if (blocked)
	{
		*errmsg = "Swearing is not permitted in this channel";
		return HOOK_DENY;
	}

	return HOOK_CONTINUE;
}

const char *censor_pre_local_part(Client *client, Channel *channel, const char *text)
{
	int blocked;

	if (!text)
		return NULL;

	if (!IsCensored(channel))
		return text;

	text = stripbadwords_channel(text, &blocked);
	return blocked ? NULL : text;
}

/** Is any channel where the user is in +G? */
static int IsAnyChannelCensored(Client *client)
{
	Membership *lp;

	for (lp = client->user->channel; lp; lp = lp->next)
		if (IsCensored(lp->channel))
			return 1;
	return 0;
}

const char *censor_pre_local_quit(Client *client, const char *text)
{
	int blocked = 0;

	if (!text)
		return NULL;

	if (IsAnyChannelCensored(client))
		text = stripbadwords_channel(text, &blocked);

	return blocked ? NULL : text;
}

int censor_stats_badwords_channel(Client *client, const char *para)
{
	ConfigItem_badword *words;

	if (!para || !(!strcmp(para, "b") || !strcasecmp(para, "badword")))
		return 0;

	for (words = conf_badword_channel; words; words = words->next)
	{
		sendtxtnumeric(client, "c %c %s%s%s %s", words->type & BADW_TYPE_REGEX ? 'R' : 'F',
		           (words->type & BADW_TYPE_FAST_L) ? "*" : "", words->word,
		           (words->type & BADW_TYPE_FAST_R) ? "*" : "",
		           words->action == BADWORD_REPLACE ? (words->replace ? words->replace : "<censored>") : "");
	}
	return 1;
}
