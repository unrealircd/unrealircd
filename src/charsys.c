/*
 * Unreal Internet Relay Chat Daemon, src/charsys.c
 * (C) Copyright 2005 Bram Matthys and The UnrealIRCd Team.
 *
 * Character system: This subsystem deals with finding out wheter a
 * character should be allowed or not in nicks (nicks only for now).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "macros.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif

#ifdef _WIN32
#include "version.h"
#endif

/* NOTE: it is guaranteed that char is unsigned by compiling options
 *       (-funsigned-char @ gcc, /J @ MSVC)
 * NOTE2: Original credit for supplying the correct chinese
 *        coderanges goes to: RexHsu, Mr.WebBar and Xuefer
 */

/** Our multibyte structure */
typedef struct _mblist MBList;
struct _mblist
{
	MBList *next;
	char s1, e1, s2, e2;
};
MBList *mblist = NULL, *mblist_tail = NULL;

static int do_nick_name_multibyte(char *nick);
static int do_nick_name_standard(char *nick);

/* Use this to prevent mixing of certain combinations
 * (such as GBK & high-ascii, etc)
 */
static int langav;
/* bitmasks: */
#define LANGAV_ASCII	0x0001 /* 8 bit ascii */
#define LANGAV_LATIN1	0x0002 /* latin1 (western europe) */
#define LANGAV_LATIN2	0x0004 /* latin2 (eastern europe, eg: hungarian) */
#define LANGAV_LATIN7	0x0008 /* latin7 (greek) */
#define LANGAV_GBK		0x1000 /* (Chinese) GBK encoding */

/** Called on boot and just before config run */
void charsys_reset(void)
{
int i;
MBList *m, *m_next;

	/* First, reset everything */
	for (i=0; i < 256; i++)
		char_atribs[i] &= ~ALLOWN;
	for (m=mblist; m; m=m_next)
	{
		m_next = m->next;
		MyFree(m);
	}
	mblist=mblist_tail=NULL;
	/* Then add the default which will always be allowed */
	charsys_addallowed("0123456789-ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyzy{|}");
	langav = 0;
}

void charsys_reset_pretest(void)
{
	langav = 0;
}

/** Add a character range to the multibyte list.
 * @param s1 Start of highest byte
 * @param e1 End of highest byte
 * @param s2 Start of lowest byte
 * @param e2 End of lowest byte
 * @example charsys_addmultibyterange(0xaa, 0xbb, 0x00, 0xff) for 0xaa00-0xbbff
 */
void charsys_addmultibyterange(char s1, char e1, char s2, char e2)
{
MBList *m = MyMallocEx(sizeof(m));

	m->s1 = s1;
	m->e1 = e1;
	m->s2 = s2;
	m->e2 = e2;

	if (mblist_tail)
		mblist_tail->next = m;
	else
		mblist = m;
	mblist_tail = m;
}

/** Adds all characters in the specified string to the allowed list. */
void charsys_addallowed(char *s)
{
	for (; *s; s++)
		char_atribs[(unsigned int)*s] |= ALLOWN;
}

int do_nick_name(char *nick)
{
	if (mblist)
		return do_nick_name_multibyte(nick);
	else
		return do_nick_name_standard(nick);
}

static int do_nick_name_standard(char *nick)
{
int len;
char *ch;

	if ((*nick == '-') || isdigit(*nick))
		return 0;

	for (ch=nick,len=0; *ch && len <= NICKLEN; ch++, len++)
		if (!isvalid(*ch))
			return 0; /* reject the full nick */
	*ch = '\0';
	return len;
}

static int isvalidmbyte(unsigned char c1, unsigned char c2)
{
MBList *m;

	for (m=mblist; m; m=m->next)
	{
		if ((c1 >= m->s1) && (c1 <= m->e1) &&
		    (c2 >= m->s2) && (c2 <= m->e2))
		    return 1;
	}
	return 0;
}

/* hmmm.. there must be some problems with multibyte &
 * other high ascii characters I think (such as german etc).
 * Not sure if this can be solved? I don't think so... -- Syzop.
 */
static int do_nick_name_multibyte(char *nick)
{
int len;
char *ch;
MBList *m;
int firstmbchar = 0;

	if ((*nick == '-') || isdigit(*nick))
		return 0;

	for (ch=nick,len=0; *ch && len <= NICKLEN; ch++, len++)
	{
		if (firstmbchar)
		{
			if (!isvalidmbyte(ch[-1], *ch))
				return 0;
			firstmbchar = 0;
		} else if ((*ch) & 0x80)
			firstmbchar = 1;
		else if (!isvalid(*ch))
			return 0;
	}
	if (firstmbchar)
		ch--;
	*ch = '\0';
	return len;
}

/** Check if the specified charsets during the TESTING phase can be
 * premitted without getting into problems.
 * RETURNS: -1 in case of failure, 1 if ok
 */
int charsys_postconftest(void)
{
int x=0;
	if ((langav & LANGAV_ASCII) && (langav & LANGAV_GBK))
	{
		config_error("ERROR: set::accept-language specifies incorrect combination "
		             "of languages: high-ascii languages (such as german, french, etc) "
		             "cannot be mixed with chinese/..");
		return -1;
	}
	if (langav & LANGAV_LATIN1)
		x++;
	if (langav & LANGAV_LATIN2)
		x++;
	if (langav & LANGAV_LATIN7)
		x++;	
	if (x > 1)
	{
		config_status("WARNING: set::accept-language: "
		            "Mixing of charsets (eg: latin1+latin2+latin7) can cause display problems");
	}
	return 1;
}

/** Check if language is available. */
int charsys_test_language(char *name)
{
	if (!strcmp(name, "latin1") || !strcmp(name, "german") ||
	    !strcmp(name, "dutch") || !strcmp(name, "swedish") ||
	    !strcmp(name, "french") || !strcmp(name, "spanish") ||
	    !strcmp(name, "catalan"))
	{
		langav |= (LANGAV_ASCII|LANGAV_LATIN1);
	} else
	if (!strcmp(name, "latin2") || !strcmp(name, "hungarian"))
	{
		langav |= (LANGAV_ASCII|LANGAV_LATIN2);
	} else
	if (!strcmp(name, "latin7") || !strcmp(name, "greek"))
	{
		langav |= (LANGAV_ASCII|LANGAV_LATIN7);
	} else
	if (!strcmp(name, "chinese") || !strcmp(name, "gbk") ||
	    !strcmp(name, "chinese-trad") || !strcmp(name, "chinese-simp") ||
	    !strcmp(name, "chinese-ja"))
	{
		langav |= LANGAV_GBK;
	} else
	if (!strcmp(name, "euro-west"))
	{
		config_error("set::accept-language: ERROR: 'euro-west' got renamed to 'latin1'");
		return 0;
	} else
		return 0;
	return 1;
}

void charsys_add_language(char *name)
{
char latin1=0, latin2=0, chinese=0;

	/** Note: there could well be some characters missing in the lists below.
	 *        While I've seen other altnernatives that just allow pretty much
	 *        every accent that exists even for dutch (where we rarely use
	 *        accents except for like 3 types), I rather prefer to use a bit more
	 *        reasonable aproach ;). That said, anyone is welcome to make
	 *        suggestions about characters that should be added (or removed)
	 *        of course. -- Syzop
	 */

	/* GROUPS */
	if (!strcmp(name, "latin1"))
		latin1 = 1;
	if (!strcmp(name, "latin2"))
		latin2 = 1;
	if (!strcmp(name, "chinese") || !strcmp(name, "gbk"))
		chinese = 1;
	
	/* INDIVIDUAL CHARSETS */

	if (latin1 || !strcmp(name, "german"))
	{
		/* a", A", e', o", O", u", U" and es-zett */
		charsys_addallowed("äÄéöÖüÜß");
	}
	if (latin1 || !strcmp(name, "dutch"))
	{
		/* Ok, even though I'm Dutch myself, I've trouble getting
		 * a proper list of this ;). I think I got them all now, but
		 * I did not include "borrow-words" like words we use in Dutch
		 * that are literal French. So if you really want to use them all,
		 * I suggest you to use just latin1 :P.
		 */
		/* e', e", o", i", u", e`. */
		charsys_addallowed("éëöïüè");
	}
	if (latin1 || !strcmp(name, "french"))
	{
		/* A`, A^, a`, a^, weird-C, weird-c, E`, E', E^, E", e`, e', e^, e",
		 * I^, I", i^, i", O^, o^, U`, U^, U", u`, u", u`, y" [not in that order, sry]
		 * Hmm.. there might be more, but I'm not sure how common they are
		 * and I don't think they are always displayed correctly (?).
		 */
		charsys_addallowed("ÀÂàâÇçÈÉÊËèéêëÎÏîïÔôÙÛÜùûüÿ");
	}
	if (latin1 || !strcmp(name, "spanish"))
	{
		/* a', A', e', E', i', I', o', O', u', U', u", U", n~, N~ */
		charsys_addallowed("áÁéÉíÍóÓúÚüÜñÑ");
	}
	if (latin1 || !strcmp(name, "italian"))
	{
		/* A`, E`, E', I`, I', O`, O', U`, U', a`, e`, e', i`, i', o`, o', u`, u' */
		charsys_addallowed("ÀÈÉÌÍÒÓÙÚàèéìíòóùú");
	}
	if (latin1 || !strcmp(name, "catalan"))
	{
		/* supplied by Trocotronic */
		/* a`, A`, a', A', e`, E`, e', E', i', I', o`, O`, o', O', u', U', i", I", u", U" */
		charsys_addallowed("àÀáÁèÈéÉíÍòÒóÓúÚïÏüÜ");
	}
	if (latin1 || !strcmp(name, "swedish"))
	{
		/* supplied by Tank */
		/* ao, Ao, a", A", o", O" */ 
		charsys_addallowed("åÅäÄöÖ");
	}
	if (latin2 || !strcmp(name, "hungarian"))
	{
		/* supplied by AngryWolf */
		/* a', e', i', o', o", o~, u', u", u~, A', E', I', O', O", O~, U', U", U~ */
		charsys_addallowed("áéíóöõúüûÁÉÍÓÖÕÚÜÛ");
	}
	if (!strcmp(name, "latin7") || !strcmp(name, "greek"))
	{
		/* supplied by GSF */
		/* ranges from rfc1947 / iso 8859-7 */
		charsys_addallowed("¶¸¹º¼¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóô");
	}
	if (chinese || !strcmp(name, "chinese-ja"))
	{
		charsys_addmultibyterange(0xa4, 0xa4, 0xa1, 0xf3); /* JIS_PIN */
		charsys_addmultibyterange(0xa5, 0xa5, 0xa1, 0xf6); /* JIS_PIN */
	}
	if (chinese || !strcmp(name, "chinese-simp"))
	{
		charsys_addmultibyterange(0xb0, 0xd6, 0xa1, 0xfe); /* GBK/2 BC with GB2312 */
		charsys_addmultibyterange(0xd7, 0xd7, 0xa1, 0xf9); /* GBK/2 BC with GB2312 */
		charsys_addmultibyterange(0xd8, 0xf7, 0xa1, 0xfe); /* GBK/2 BC with GB2312 */
	}
	if (chinese || !strcmp(name, "chinese-trad"))
	{
		charsys_addmultibyterange(0x81, 0xa0, 0x40, 0xfe); /* GBK/3 */
		charsys_addmultibyterange(0xaa, 0xfe, 0x40, 0xa0); /* GBK/4 */
	}
}
