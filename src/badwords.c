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

#ifdef FAST_BADWORD_REPLACE
/*
 * our own strcasestr implementation because strcasestr is often not
 * available or is not working correctly (??).
 */
static char *our_strcasestr(char *haystack, char *needle) {
int i;
int nlength = strlen (needle);
int hlength = strlen (haystack);

	if (nlength > hlength) return NULL;
	if (hlength <= 0) return NULL;
	if (nlength <= 0) return haystack;
	for (i = 0; i <= (hlength - nlength); i++) {
		if (strncasecmp (haystack + i, needle, nlength) == 0)
			return haystack + i;
	}
  return NULL; /* not found */
}

/* fast_badword_replace:
 * a fast replace routine written by Syzop used for replacing badwords.
 * searches in line for huntw and replaces it with replacew,
 * buf is used for the result and max is sizeof(buf).
 * (Internal assumptions: max > 0 AND max > strlen(line)+1)
 */
inline int fast_badword_replace(ConfigItem_badword *badword, char *line, char *buf, int max)
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
		cleaned = 1;
		if (replacen == -1)
			replacen = strlen(replacew);
		if (searchn == -1)
			searchn = strlen(badword->word);
		/* Hunt for start of word */
 		if (pold > line) {
			for (startw = pold; ((*startw != ' ') && (startw != line)); startw--);
			if (*startw == ' ')
				startw++; /* Don't point at the space but at the word! */
		} else {
			startw = pold;
		}

		if (!(badword->type & BADW_TYPE_FAST_L) && (pold != startw)) {
			/* not matched */
			pold++;
			continue;
		}

		/* Hunt for end of word */
		for (endw = pold; ((*endw != '\0') && (isalnum(*endw))); endw++);

		if (!(badword->type & BADW_TYPE_FAST_R) && (pold+searchn != endw)) {
			/* not matched */
			pold++;
			continue;
		}

		/* Do we have any not-copied-yet data? */
		if (poldx != startw) {
			int tmp_n = startw - poldx;
			if (pnew + tmp_n >= c_eol) {
				/* Partial copy and return... */
				memcpy(pnew, poldx, c_eol - pnew);
				*c_eol = '\0';
				return;
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
				return;
			}
			memcpy(pnew, replacew, replacen);
			pnew += replacen;
		}
		poldx = pold = endw;
		printf("pold=%p, char='%d'/'%c'\n", pold, *pold, *pold);
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
#endif

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
	static char cleanstr[4096];
	char buf[4096];
	char *ptr;
	int  matchlen, stringlen, cleaned;
	ConfigItem_badword *this_word;

	if (!conf_badword_channel)
		return str;

	/*
	 * work on a copy
	 */
	stringlen = strlcpy(cleanstr, StripControlCodes(str), sizeof cleanstr);
	memset(&pmatch, 0, sizeof pmatch);
	matchlen = 0;
	buf[0] = '\0';
	cleaned = 0;

	for (this_word = conf_badword_channel; this_word; this_word = (ConfigItem_badword *)this_word->next)
	{
#ifdef FAST_BADWORD_REPLACE
		if (this_word->type & BADW_TYPE_FAST)
		{
			int n;
			/* fast_badword_replace() does size checking so we can use 512 here instead of 4096 */
			n = fast_badword_replace(this_word, cleanstr, buf, 512);
			if (!cleaned && n)
				cleaned = n;
			strcpy(cleanstr, buf);
			memset(buf, 0, sizeof(buf)); /* regexp likes this somehow */
		} else
		if (this_word->type & BADW_TYPE_REGEX)
		{
#endif
			ptr = cleanstr; /* set pointer to start of string */

			while (regexec(&this_word->expr, ptr, MAX_MATCH, pmatch,0) != REG_NOMATCH)
			{
				if (pmatch[0].rm_so == -1)
					break;
				cleaned = 1;
				matchlen += pmatch[0].rm_eo - pmatch[0].rm_so;
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
#ifdef FAST_BADWORD_REPLACE
		}
#endif
	}
	return (cleaned) ? cleanstr : str;
}

char *stripbadwords_message(char *str)
{
	regmatch_t pmatch[MAX_MATCH];
	static char cleanstr[4096];
	char buf[4096];
	char *ptr;
	int matchlen, stringlen;
	ConfigItem_badword *this_word;
	if (!conf_badword_message)
		return str;

	/*
	 * work on a copy
	 */
	stringlen = strlcpy(cleanstr, str, sizeof cleanstr);
	memset(&pmatch, 0, sizeof pmatch);
	matchlen = 0;
	buf[0] = '\0';

	for (this_word = conf_badword_message; this_word; this_word = (ConfigItem_badword *)this_word->next)
	{
		/*
		 * Set pointer to start of string
		 */
		ptr = cleanstr;

		while (regexec(&this_word->expr, ptr, MAX_MATCH, pmatch,
		    0) != REG_NOMATCH)
		{
			if (pmatch[0].rm_so == -1)
				break;
			matchlen += pmatch[0].rm_eo - pmatch[0].rm_so;
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

	return (cleanstr);
}

#endif
