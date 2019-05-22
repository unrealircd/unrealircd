/*
 * Channel Mode +G
 * (C) Copyright 2005-current Bram Matthys and The UnrealIRCd team.
 */

#include "unrealircd.h"


ModuleHeader MOD_HEADER(censor)
  = {
	"chanmodes/censor",
	"4.2",
	"Channel Mode +G",
	"3.2-b8-1",
	NULL,
    };


Cmode_t EXTMODE_CENSOR = 0L;

#define IsCensored(x) ((x)->mode.extmode & EXTMODE_CENSOR)

char *censor_pre_chanmsg(aClient *sptr, aChannel *chptr, char *text, int notice);
char *censor_pre_local_part(aClient *sptr, aChannel *chptr, char *text);
char *censor_pre_local_quit(aClient *sptr, char *text);

int censor_config_test(ConfigFile *, ConfigEntry *, int, int *);
int censor_config_run(ConfigFile *, ConfigEntry *, int);

ModuleInfo *ModInfo = NULL;

ConfigItem_badword *conf_badword_channel = NULL;


MOD_TEST(censor)
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, censor_config_test);
	return MOD_SUCCESS;
}
	
MOD_INIT(censor)
{
	CmodeInfo req;

	ModInfo = modinfo;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.is_ok = extcmode_default_requirechop;
	req.flag = 'G';
	CmodeAdd(modinfo->handle, req, &EXTMODE_CENSOR);

	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_CHANMSG, 0, censor_pre_chanmsg);
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_LOCAL_PART, 0, censor_pre_local_part);
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_LOCAL_QUIT, 0, censor_pre_local_quit);
	
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, censor_config_run);
	return MOD_SUCCESS;
}

MOD_LOAD(censor)
{
	return MOD_SUCCESS;
}


MOD_UNLOAD(censor)
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
	
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "badword"))
		return 0; /* not interested */

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: badword without type",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	else if (strcmp(ce->ce_vardata, "channel") && 
	         strcmp(ce->ce_vardata, "quit") && strcmp(ce->ce_vardata, "all")) {
/*			config_error("%s:%i: badword with unknown type",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum); -- can't do that.. */
		return 0; /* unhandled */
	}
	
	if (!strcmp(ce->ce_vardata, "quit"))
	{
		config_error("%s:%i: badword quit has been removed. We just use the bad words from "
		             "badword channel { } instead.",
		             ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 0; /* pretend unhandled.. ok not just pretend.. ;) */
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (config_is_blankorempty(cep, "badword"))
		{
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "word"))
		{
			char *errbuf;
			if (has_word)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "badword::word");
				continue;
			}
			has_word = 1;
			if ((errbuf = badword_config_check_regex(cep->ce_vardata,1,1)))
			{
				config_error("%s:%i: badword::%s contains an invalid regex: %s",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum,
					cep->ce_varname, errbuf);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "replace"))
		{
			if (has_replace)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "badword::replace");
				continue;
			}
			has_replace = 1;
		}
		else if (!strcmp(cep->ce_varname, "action"))
		{
			if (has_action)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename, 
					cep->ce_varlinenum, "badword::action");
				continue;
			}
			has_action = 1;
			if (!strcmp(cep->ce_vardata, "replace"))
				action = 'r';
			else if (!strcmp(cep->ce_vardata, "block"))
				action = 'b';
			else
			{
				config_error("%s:%d: Unknown badword::action '%s'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_vardata);
				errors++;
			}
				
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"badword", cep->ce_varname);
			errors++;
		}
	}

	if (!has_word)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"badword::word");
		errors++;
	}
	if (has_action)
	{
		if (has_replace && action == 'b')
		{
			config_error("%s:%i: badword::action is block but badword::replace exists",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
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
	
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "badword"))
		return 0; /* not interested */

	if (strcmp(ce->ce_vardata, "channel") && strcmp(ce->ce_vardata, "all"))
	        return 0; /* not for us */

	ca = MyMallocEx(sizeof(ConfigItem_badword));
	ca->action = BADWORD_REPLACE;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "action"))
		{
			if (!strcmp(cep->ce_vardata, "block"))
			{
				ca->action = BADWORD_BLOCK;
			}
		}
		else if (!strcmp(cep->ce_varname, "replace"))
		{
			safestrdup(ca->replace, cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "word"))
		{
			word = cep;
		}
	}

	badword_config_process(ca, word->ce_vardata);

	if (!strcmp(ce->ce_vardata, "channel"))
		AddListItem(ca, conf_badword_channel);
	else if (!strcmp(ce->ce_vardata, "all"))
	{
		AddListItem(ca, conf_badword_channel);
		return 0; /* pretend we didn't see it, so other modules can handle 'all' as well */
	}

	return 1;
}

char *stripbadwords_channel(char *str, int *blocked)
{
	return stripbadwords(str, conf_badword_channel, blocked);
}

char *censor_pre_chanmsg(aClient *sptr, aChannel *chptr, char *text, int notice)
{
	int blocked;
	Hook *h;
	int i;

	if (!IsCensored(chptr))
		return text;

	for (h = Hooks[HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(sptr, chptr, BYPASS_CHANMSG_CENSOR);
		if (i != HOOK_CONTINUE)
			break;
	}
	if (i == HOOK_ALLOW)
		return text; /* bypass */

	text = stripbadwords_channel(text, &blocked);
	if (blocked)
	{
		if (!notice)
			sendnumeric(sptr, ERR_CANNOTSENDTOCHAN, chptr->chname,
				"Swearing is not permitted in this channel", chptr->chname);
		return NULL;
	}

	return text;
}

char *censor_pre_local_part(aClient *sptr, aChannel *chptr, char *text)
{
	int blocked;

	if (!text)
		return NULL;

	if (!IsCensored(chptr))
		return text;

	text = stripbadwords_channel(text, &blocked);
	return blocked ? NULL : text;
}

/** Is any channel where the user is in +G? */
static int IsAnyChannelCensored(aClient *sptr)
{
	Membership *lp;

	for (lp = sptr->user->channel; lp; lp = lp->next)
		if (IsCensored(lp->chptr))
			return 1;
	return 0;
}

char *censor_pre_local_quit(aClient *sptr, char *text)
{
	int blocked = 0;

	if (!text)
		return NULL;

	if (IsAnyChannelCensored(sptr))
		text = stripbadwords_channel(text, &blocked);

	return blocked ? NULL : text;
}

// TODO: when stats is modular, make it call this for badwords
int stats_badwords(aClient *sptr, char *para)
{
	ConfigItem_badword *words;

	for (words = conf_badword_channel; words; words = words->next)
	{
		sendtxtnumeric(sptr, "c %c %s%s%s %s", words->type & BADW_TYPE_REGEX ? 'R' : 'F',
		           (words->type & BADW_TYPE_FAST_L) ? "*" : "", words->word,
		           (words->type & BADW_TYPE_FAST_R) ? "*" : "",
		           words->action == BADWORD_REPLACE ? (words->replace ? words->replace : "<censored>") : "");
	}
	return 0;
}
