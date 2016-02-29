/*
 * Channel Mode +G
 * (C) Copyright 2005-current Bram Matthys and The UnrealIRCd team.
 */

#include "unrealircd.h"


ModuleHeader MOD_HEADER(censor)
  = {
	"chanmodes/censor",
	"4.0",
	"Channel Mode +G",
	"3.2-b8-1",
	NULL,
    };


Cmode_t EXTMODE_CENSOR = 0L;

#define IsCensored(x) ((x)->mode.extmode & EXTMODE_CENSOR)

char *censor_pre_chanmsg(aClient *sptr, aChannel *chptr, char *text, int notice);
char *censor_pre_local_part(aClient *sptr, aChannel *chptr, char *text);
char *censor_pre_local_quit(aClient *sptr, char *text);

DLLFUNC int censor_config_test(ConfigFile *, ConfigEntry *, int, int *);
DLLFUNC int censor_config_run(ConfigFile *, ConfigEntry *, int);

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

	for (badword = conf_badword_channel; badword; badword = (ConfigItem_badword *) next)
	{
		next = badword->next;
		safefree(badword->word);
		if (badword->replace)
			safefree(badword->replace);
		regfree(&badword->expr);
		DelListItem(badword, conf_badword_channel);
		MyFree(badword);
	}
	return MOD_SUCCESS;
}

DLLFUNC int censor_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
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
			if ((errbuf = unreal_checkregex(cep->ce_vardata,1,1)))
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


DLLFUNC int censor_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *word = NULL;
	ConfigItem_badword *ca;
	char *tmp;
	short regex = 0;
	int regflags = 0;
	int ast_l = 0, ast_r = 0;

	if (type != CONFIG_MAIN)
		return 0;
	
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "badword"))
		return 0; /* not interested */

	if (strcmp(ce->ce_vardata, "channel") && strcmp(ce->ce_vardata, "all"))
	        return 0; /* not for us */

	ca = MyMallocEx(sizeof(ConfigItem_badword));
	ca->action = BADWORD_REPLACE;
	regflags = REG_ICASE|REG_EXTENDED;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "action"))
		{
			if (!strcmp(cep->ce_vardata, "block"))
			{
				ca->action = BADWORD_BLOCK;
				/* If it is set to just block, then we don't need to worry about
				 * replacements 
				 */
				regflags |= REG_NOSUB;
			}
		}
		else if (!strcmp(cep->ce_varname, "replace"))
		{
			safestrdup(ca->replace, cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "word"))
			word = cep;
	}
	/* The fast badwords routine can do: "blah" "*blah" "blah*" and "*blah*",
	 * in all other cases use regex.
	 */
	for (tmp = word->ce_vardata; *tmp; tmp++) {
		if (!isalnum(*tmp) && !(*tmp >= 128)) {
			if ((word->ce_vardata == tmp) && (*tmp == '*')) {
				ast_l = 1; /* Asterisk at the left */
				continue;
			}
			if ((*(tmp + 1) == '\0') && (*tmp == '*')) {
				ast_r = 1; /* Asterisk at the right */
				continue;
			}
			regex = 1;
			break;
		}
	}
	if (regex)
	{
		ca->type = BADW_TYPE_REGEX;
		safestrdup(ca->word, word->ce_vardata);
		regcomp(&ca->expr, ca->word, regflags);
	}
	else
	{
		char *tmpw;
		ca->type = BADW_TYPE_FAST;
		ca->word = tmpw = MyMallocEx(strlen(word->ce_vardata) - ast_l - ast_r + 1);
		/* Copy except for asterisks */
		for (tmp = word->ce_vardata; *tmp; tmp++)
			if (*tmp != '*')
				*tmpw++ = *tmp;
		*tmpw = '\0';
		if (ast_l)
			ca->type |= BADW_TYPE_FAST_L;
		if (ast_r)
			ca->type |= BADW_TYPE_FAST_R;
	}
	if (!strcmp(ce->ce_vardata, "channel"))
		AddListItem(ca, conf_badword_channel);
	else if (!strcmp(ce->ce_vardata, "all"))
	{
		AddListItem(ca, conf_badword_channel);
		return 0; /* pretend we didn't see it, so other modules can handle 'all' as well */
	}

	return 1;
}

static inline int fast_badword_match(ConfigItem_badword *badword, char *line)
{
 	char *p;
	int bwlen = strlen(badword->word);
	if ((badword->type & BADW_TYPE_FAST_L) && (badword->type & BADW_TYPE_FAST_R))
		return (our_strcasestr(line, badword->word) ? 1 : 0);

	p = line;
	while((p = our_strcasestr(p, badword->word)))
	{
		if (!(badword->type & BADW_TYPE_FAST_L))
		{
			if ((p != line) && !iswseperator(*(p - 1))) /* aaBLA but no *BLA */
				goto next;
		}
		if (!(badword->type & BADW_TYPE_FAST_R))
		{
			if (!iswseperator(*(p + bwlen)))  /* BLAaa but no BLA* */
				goto next;
		}
		/* Looks like it matched */
		return 1;
next:
		p += bwlen;
	}
	return 0;
}
/* fast_badword_replace:
 * a fast replace routine written by Syzop used for replacing badwords.
 * searches in line for huntw and replaces it with replacew,
 * buf is used for the result and max is sizeof(buf).
 * (Internal assumptions: max > 0 AND max > strlen(line)+1)
 */
static inline int fast_badword_replace(ConfigItem_badword *badword, char *line, char *buf, int max)
{
/* Some aliases ;P */
char *replacew = badword->replace ? badword->replace : REPLACEWORD;
char *pold = line, *pnew = buf; /* Pointers to old string and new string */
char *poldx = line;
int replacen = -1; /* Only calculated if needed. w00t! saves us a few nanosecs? lol */
int searchn = -1;
char *startw, *endw;
char *c_eol = buf + max - 1; /* Cached end of (new) line */
int run = 1;
int cleaned = 0;

	Debug((DEBUG_NOTICE, "replacing %s -> %s in '%s'", badword->word, replacew, line));

	while(run) {
		pold = our_strcasestr(pold, badword->word);
		if (!pold)
			break;
		if (replacen == -1)
			replacen = strlen(replacew);
		if (searchn == -1)
			searchn = strlen(badword->word);
		/* Hunt for start of word */
 		if (pold > line) {
			for (startw = pold; (!iswseperator(*startw) && (startw != line)); startw--);
			if (iswseperator(*startw))
				startw++; /* Don't point at the space/seperator but at the word! */
		} else {
			startw = pold;
		}

		if (!(badword->type & BADW_TYPE_FAST_L) && (pold != startw)) {
			/* not matched */
			pold++;
			continue;
		}

		/* Hunt for end of word */
		for (endw = pold; ((*endw != '\0') && (!iswseperator(*endw))); endw++);

		if (!(badword->type & BADW_TYPE_FAST_R) && (pold+searchn != endw)) {
			/* not matched */
			pold++;
			continue;
		}

		cleaned = 1; /* still too soon? Syzop/20050227 */

		/* Do we have any not-copied-yet data? */
		if (poldx != startw) {
			int tmp_n = startw - poldx;
			if (pnew + tmp_n >= c_eol) {
				/* Partial copy and return... */
				memcpy(pnew, poldx, c_eol - pnew);
				*c_eol = '\0';
				return 1;
			}

			memcpy(pnew, poldx, tmp_n);
			pnew += tmp_n;
		}
		/* Now update the word in buf (pnew is now something like startw-in-new-buffer */

		if (replacen) {
			if ((pnew + replacen) >= c_eol) {
				/* Partial copy and return... */
				memcpy(pnew, replacew, c_eol - pnew);
				*c_eol = '\0';
				return 1;
			}
			memcpy(pnew, replacew, replacen);
			pnew += replacen;
		}
		poldx = pold = endw;
	}
	/* Copy the last part */
	if (*poldx) {
		strncpy(pnew, poldx, c_eol - pnew);
		*(c_eol) = '\0';
	} else {
		*pnew = '\0';
	}
	return cleaned;
}

/*
 * Returns a string, which has been filtered by the words loaded via
 * the loadbadwords() function.  It's primary use is to filter swearing
 * in both private and public messages
 */

char *stripbadwords(char *str, ConfigItem_badword *start_bw, int *blocked)
{
	regmatch_t pmatch[MAX_MATCH];
	static char cleanstr[4096];
	char buf[4096];
	char *ptr;
	int  matchlen, m, stringlen, cleaned;
	ConfigItem_badword *this_word;
	
	*blocked = 0;

	if (!start_bw)
		return str;

	/*
	 * work on a copy
	 */
	stringlen = strlcpy(cleanstr, StripControlCodes(str), sizeof cleanstr);
	memset(&pmatch, 0, sizeof pmatch);
	matchlen = 0;
	buf[0] = '\0';
	cleaned = 0;

	for (this_word = start_bw; this_word; this_word = (ConfigItem_badword *)this_word->next)
	{
		if (this_word->type & BADW_TYPE_FAST)
		{
			if (this_word->action == BADWORD_BLOCK)
			{
				if (fast_badword_match(this_word, cleanstr))
				{
					*blocked = 1;
					return NULL;
				}
			}
			else
			{
				int n;
				/* fast_badword_replace() does size checking so we can use 512 here instead of 4096 */
				n = fast_badword_replace(this_word, cleanstr, buf, 512);
				if (!cleaned && n)
					cleaned = n;
				strcpy(cleanstr, buf);
				memset(buf, 0, sizeof(buf)); /* regexp likes this somehow */
			}
		} else
		if (this_word->type & BADW_TYPE_REGEX)
		{
			if (this_word->action == BADWORD_BLOCK)
			{
				if (!regexec(&this_word->expr, cleanstr, 0, NULL, 0))
				{
					*blocked = 1;
					return NULL;
				}
			}
			else
			{
				ptr = cleanstr; /* set pointer to start of string */
				while (regexec(&this_word->expr, ptr, MAX_MATCH, pmatch,0) != REG_NOMATCH)
				{
					if (pmatch[0].rm_so == -1)
						break;
					m = pmatch[0].rm_eo - pmatch[0].rm_so;
					if (m == 0)
						break; /* anti-loop */
					cleaned = 1;
					matchlen += m;
					strlncat(buf, ptr, sizeof buf, pmatch[0].rm_so);
					if (this_word->replace)
						strlcat(buf, this_word->replace, sizeof buf); 
					else
						strlcat(buf, REPLACEWORD, sizeof buf);
					ptr += pmatch[0].rm_eo;	/* Set pointer after the match pos */
					memset(&pmatch, 0, sizeof(pmatch));
				}

				/* All the better to eat you with! */
				strlcat(buf, ptr, sizeof buf);	
				memcpy(cleanstr, buf, sizeof cleanstr);
				memset(buf, 0, sizeof(buf));
				if (matchlen == stringlen)
					break;
			}
		}
	}

	cleanstr[511] = '\0'; /* cutoff, just to be sure */

	return (cleaned) ? cleanstr : str;
}

char *stripbadwords_channel(char *str, int *blocked)
{
	return stripbadwords(str, conf_badword_channel, blocked);
}

char *censor_pre_chanmsg(aClient *sptr, aChannel *chptr, char *text, int notice)
{
int blocked;

	if (!IsCensored(chptr))
		return text;
	
	text = stripbadwords_channel(text, &blocked);
	if (blocked)
	{
		if (!notice)
			sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
				me.name, sptr->name, chptr->chname,
				"Swearing is not permitted in this channel", chptr->chname);
		return NULL;
	}

	return text;
}

char *censor_pre_local_part(aClient *sptr, aChannel *chptr, char *text)
{
int blocked;

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

	if (IsAnyChannelCensored(sptr))
		text = stripbadwords_channel(text, &blocked);
	return blocked ? NULL : text;
}

// TODO: when stats is modular, make it call this for badwords
int stats_badwords(aClient *sptr, char *para)
{
	ConfigItem_badword *words;

	for (words = conf_badword_channel; words; words = (ConfigItem_badword *) words->next)
	{
		sendto_one(sptr, ":%s %i %s :c %c %s%s%s %s",
		           me.name, RPL_TEXT, sptr->name, words->type & BADW_TYPE_REGEX ? 'R' : 'F',
		           (words->type & BADW_TYPE_FAST_L) ? "*" : "", words->word,
		           (words->type & BADW_TYPE_FAST_R) ? "*" : "",
		           words->action == BADWORD_REPLACE ? (words->replace ? words->replace : "<censored>") : "");
	}
	return 0;
}
