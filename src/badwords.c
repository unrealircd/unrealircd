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

static char *channelword[MAX_WORDS];
static char *messageword[MAX_WORDS];
static int channel_wordlist;
static int message_wordlist;

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
	int  errorcode, matchlen, stringlen, this_word;

	if (!channel_wordlist)
		return str;
	strncpy(cleanstr, str, sizeof(cleanstr) - 1);	/* Let's work on a backup */
	memset(&pmatch, 0, sizeof(pmatch));
	stringlen = strlen(cleanstr);
	matchlen = 0;
	buf[0] = '\0';

	for (this_word = 0; channelword[this_word] != NULL; this_word++)
	{
		if ((errorcode =
		    regcomp(&pcomp, channelword[this_word], REG_ICASE)) > 0)
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
			strcat(buf, REPLACEWORD);	/* Who's afraid of the big bad buffer overflow? */
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
	int  errorcode, matchlen, stringlen, this_word;

	if (!message_wordlist)
		return str;
	strncpy(cleanstr, str, sizeof(cleanstr) - 1);	/* Let's work on a backup */
	memset(&pmatch, 0, sizeof(pmatch));
	stringlen = strlen(cleanstr);
	matchlen = 0;
	buf[0] = '\0';

	for (this_word = 0; messageword[this_word] != NULL; this_word++)
	{
		if ((errorcode =
		    regcomp(&pcomp, messageword[this_word], REG_ICASE)) > 0)
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
			strcat(buf, REPLACEWORD);	/* Who's afraid of the big bad buffer overflow? */
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

/*
 * Loads bad words from a file, into a char array. This puts a limitation on 
 * how many words may be slurped (which is probably a good thing).  The words
 * are then called by stripbadwords, to filter unwanted (swear) words.
 */
int  loadbadwords_channel(char *wordfile)
{
	FILE *fin;
	char buf[MAX_WORDLEN];
	char *ptr;
	int  i, j, isregex;

	channel_wordlist = 0;
	memset(channelword, 0, sizeof(messageword));

	if ((fin = fopen(wordfile, "r")) == NULL)
		return 0;

	for (i = 0; i < MAX_WORDS; i++)
	{
		if (fgets(buf, MAX_WORDLEN, fin) == NULL)
			break;
		if ((ptr = strchr(buf, '\r')) != NULL
		    || (ptr = strchr(buf, '\n')) != NULL)
			*ptr = '\0';
		if (buf[0] == '\0')
		{
			i--;
			continue;
		}
		if (buf[0] == '#')
		{
			i--;
			continue;
		}
		for (j = 0, isregex = 0; j < strlen(buf); j++)
		{
			if ((int)buf[j] < 65 || (int)buf[j] > 123)
			{
				isregex++;	/* Probably is */
				break;
			}
		}

		if (isregex)
		{
			/* We don't have to apply a pattern to the word because
			 * it already *is* a pattern.
			 */
			channelword[i] = (char *)MyMalloc(strlen(buf) + 1);
			strncpy(channelword[i], buf, strlen(buf) + 1);
		}
		else
		{
			/* PATTERN contains the %s format specifier so we must
			 * remove 2 chars, and 1 for the \0, which gives us -1
			 */
			channelword[i] =
			    (char *)MyMalloc(strlen(buf) + strlen(PATTERN) - 1);
			sprintf(channelword[i], PATTERN, buf);
		}
		channel_wordlist++;
	}
	fclose(fin);

	return i;
}

int  loadbadwords_message(char *wordfile)
{
	FILE *fin;
	char buf[MAX_WORDLEN];
	char *ptr;
	int  i, j, isregex;

	message_wordlist = 0;
	memset(messageword, 0, sizeof(messageword));

	if ((fin = fopen(wordfile, "r")) == NULL)
		return 0;

	for (i = 0; i < MAX_WORDS; i++)
	{
		if (fgets(buf, MAX_WORDLEN, fin) == NULL)
			break;
		if ((ptr = strchr(buf, '\r')) != NULL
		    || (ptr = strchr(buf, '\n')) != NULL)
			*ptr = '\0';
		if (buf[0] == '\0')
		{
			i--;
			continue;
		}
		if (buf[0] == '#')
		{
			i--;
			continue;
		}

		for (j = 0, isregex = 0; j < strlen(buf); j++)
		{
			if ((int)buf[j] < 65 || (int)buf[j] > 123)
			{
				isregex++;	/* Probably is */
				break;
			}
		}

		if (isregex)
		{
			/* We don't have to apply a pattern to the word because
			 * it already *is* a pattern.
			 */
			messageword[i] = (char *)MyMalloc(strlen(buf) + 1);
			strncpy(messageword[i], buf, strlen(buf) + 1);
		}
		else
		{
			/* PATTERN contains the %s format specifier so we must
			 * remove 2 chars, and 1 for the \0, which gives us -1
			 */
			messageword[i] =
			    (char *)MyMalloc(strlen(buf) + strlen(PATTERN) - 1);
			sprintf(messageword[i], PATTERN, buf);
		}
		message_wordlist++;
	}
	fclose(fin);

	return i;
}


void freebadwords(void)
{
	int  i;

	for (i = 0; channelword[i] != NULL && i < MAX_WORDS; i++)
		free(channelword[i]);
	for (i = 0; messageword[i] != NULL && i < MAX_WORDS; i++)
		free(messageword[i]);
	return;
}
#endif
