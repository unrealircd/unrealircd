/*
 *   IRC - Internet Relay Chat, badwords.c
 *   Copyleft (C) 2000 meow <csi@gnu.net>
 * 
 *   Provides functions, for loading and filtering unwanted words out of
 *   a string, or in this case part of a message.  Please note that this
 *   is flawed because when mode_strip is set, mode_stripbadwords is not
 *   active.  This is due to the structure of m_message(), and therefore
 *   will not change until I (or someone else) revamps the code.
 *
 *   You can redistribute and/or modify this under the terms of the GNU
 *   General Public License as published by the Free Software Foundation.
 *
 *   Disclaimer: You have no rights.  Use at your own risk.  Don't drink
 *   too much pepsi.
 */

#ifndef _WIN32
#include <unistd.h>
#endif
#include "config.h"
#include "struct.h"
#include "common.h"
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"

/* This was modified a bit in order to use newconf. The loading functions
 * have been trashed and integrated into the config parser. The striping
 * function now only uses REPLACEWORD if no word is specifically defined
 * for the word found. Also the freeing function has been ditched. -- codemastr
 */

/*
 * Returns a string, which has been filtered by the words loaded via
 * the loadbadwords() function.  It's primary use is to filter swearing
 * in both private and public messages
 */

void badwords_stats(aClient *sptr)
{
}

char *stripbadwords_channel(char *str)
{
	regmatch_t pmatch[MAX_MATCH];
	regex_t pcomp;
	static char cleanstr[4096];
	char buf[4096];
	char *ptr;
	int  errorcode, matchlen, stringlen;
	ConfigItem_badword *this_word;

	if (!conf_badword_channel)
		return str;
	strncpy(cleanstr, str, sizeof(cleanstr) - 1);	/* Let's work on a backup */
	memset(&pmatch, 0, sizeof(pmatch));
	stringlen = strlen(cleanstr);
	matchlen = 0;
	buf[0] = '\0';

	for (this_word = conf_badword_channel; this_word; this_word = (ConfigItem_badword *)this_word->next)
	{
		if ((errorcode =
		    regcomp(&pcomp, this_word->word, REG_ICASE)) > 0)
		{
			regfree(&pcomp);
			return cleanstr;
		}

		ptr = cleanstr;	/* Set pointer to start of string */
		while (regexec(&pcomp, ptr, MAX_MATCH, pmatch,
		    0) != REG_NOMATCH)
		{
			if (pmatch[0].rm_so == -1)
				break;
			matchlen += pmatch[0].rm_eo - pmatch[0].rm_so;
			strncat(buf, ptr, pmatch[0].rm_so);
			if (this_word->replace)
				strcat(buf, this_word->replace); 
			else
				strcat(buf, REPLACEWORD);
			ptr += pmatch[0].rm_eo;	/* Set pointer after the match pos */
			memset(&pmatch, 0, sizeof(pmatch));
		}
		strcat(buf, ptr);	/* All the better to eat you with! */
		strncpy(cleanstr, buf, sizeof(cleanstr));
		memset(buf, 0, sizeof(buf));
		regfree(&pcomp);
		if (matchlen == stringlen)
			break;
	}

	return (cleanstr);
}

char *stripbadwords_message(char *str)
{
	regmatch_t pmatch[MAX_MATCH];
	regex_t pcomp;
	static char cleanstr[4096];
	char buf[4096];
	char *ptr;
	ConfigItem_badword *this_word;
	int  errorcode, matchlen, stringlen;

	if (!conf_badword_message)
		return str;
	strncpy(cleanstr, str, sizeof(cleanstr) - 1);	/* Let's work on a backup */
	memset(&pmatch, 0, sizeof(pmatch));
	stringlen = strlen(cleanstr);
	matchlen = 0;
	buf[0] = '\0';

	for (this_word = conf_badword_message; this_word; this_word = (ConfigItem_badword *)this_word->next) 
	{
		if ((errorcode =
		    regcomp(&pcomp, this_word->word, REG_ICASE)) > 0)
		{
			regfree(&pcomp);
			return cleanstr;
		}

		ptr = cleanstr;	/* Set pointer to start of string */
		while (regexec(&pcomp, ptr, MAX_MATCH, pmatch,
		    0) != REG_NOMATCH)
		{
			if (pmatch[0].rm_so == -1)
				break;
			matchlen += pmatch[0].rm_eo - pmatch[0].rm_so;
			strncat(buf, ptr, pmatch[0].rm_so);
			if (this_word->replace)
				strcat(buf, this_word->replace);
			else
				strcat(buf, REPLACEWORD);
			ptr += pmatch[0].rm_eo;	/* Set pointer after the match pos */
			memset(&pmatch, 0, sizeof(pmatch));
		}
		strcat(buf, ptr);	/* All the better to eat you with! */
		strncpy(cleanstr, buf, sizeof(cleanstr));
		memset(buf, 0, sizeof(buf));
		regfree(&pcomp);
		if (matchlen == stringlen)
			break;
	}

	return (cleanstr);
}

#endif
