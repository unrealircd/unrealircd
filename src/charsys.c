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

/* Use this to prevent mixing of certain combinations
 * (such as GBK & high-ascii, etc)
 */
static int langav;
char langsinuse[4096];

/* bitmasks: */
#define LANGAV_ASCII		0x0001 /* 8 bit ascii */
#define LANGAV_LATIN1		0x0002 /* latin1 (western europe) */
#define LANGAV_LATIN2		0x0004 /* latin2 (eastern europe, eg: polish) */
#define LANGAV_ISO8859_7	0x0008 /* greek */
#define LANGAV_ISO8859_8I	0x0010 /* hebrew */
#define LANGAV_ISO8859_9	0x0020 /* turkish */
#define LANGAV_W1250		0x0040 /* windows-1250 (eg: polish-w1250) */
#define LANGAV_W1251		0x0080 /* windows-1251 (eg: russian) */
#define LANGAV_LATIN2W1250	0x0100 /* Compatible with both latin2 AND windows-1250 (eg: hungarian) */
#define LANGAV_GBK			0x1000 /* (Chinese) GBK encoding */

typedef struct _langlist LangList;
struct _langlist
{
	char *directive;
	char *code;
	int setflags;
};

/* MUST be alphabetized (first column) */
static LangList langlist[] = {
	{ "belarussian-w1251", "blr", LANGAV_ASCII|LANGAV_W1251 },
	{ "catalan",      "cat", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "chinese",      "chi-j,chi-s,chi-t", LANGAV_GBK },
	{ "chinese-ja",   "chi-j", LANGAV_GBK },
	{ "chinese-simp", "chi-s", LANGAV_GBK },
	{ "chinese-trad", "chi-t", LANGAV_GBK },
	{ "czech",        "cze-m", LANGAV_ASCII|LANGAV_W1250 },
	{ "danish",       "dan", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "dutch",        "dut", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "french",       "fre", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "gbk",          "chi-s,chi-t,chi-j", LANGAV_GBK },
	{ "german",       "ger", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "greek",        "gre", LANGAV_ASCII|LANGAV_ISO8859_7 },
	{ "hebrew",       "heb", LANGAV_ASCII|LANGAV_ISO8859_8I },
	{ "hungarian",    "hun", LANGAV_ASCII|LANGAV_LATIN2W1250 },
	{ "icelandic",    "ice", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "italian",      "ita", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "latin1",       "cat,dut,fre,ger,ita,spa,swe", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "latin2",       "hun,pol,rum", LANGAV_ASCII|LANGAV_LATIN2 },
	{ "polish",       "pol", LANGAV_ASCII|LANGAV_LATIN2 },
	{ "polish-w1250", "pol-m", LANGAV_ASCII|LANGAV_W1250 },
	{ "romanian",     "rum", LANGAV_ASCII|LANGAV_LATIN2W1250 },
	{ "russian-w1251","rus", LANGAV_ASCII|LANGAV_W1251 },
	{ "slovak",       "slo-m", LANGAV_ASCII|LANGAV_W1250 },
	{ "spanish",      "spa", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "swedish",      "swe", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "swiss-german", "swg", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "turkish",      "tur", LANGAV_ASCII|LANGAV_ISO8859_9 },
	{ "ukrainian-w1251", "ukr", LANGAV_ASCII|LANGAV_W1251 },
	{ "windows-1250", "cze-m,pol-m,rum,slo-m,hun",  LANGAV_ASCII|LANGAV_W1250 },
	{ "windows-1251", "rus,ukr,blr", LANGAV_ASCII|LANGAV_W1251 },
	{ NULL, NULL, 0 }
};

/* For temporary use during config_run */
typedef struct _ilanglist ILangList;
struct _ilanglist
{
	ILangList *prev, *next;
	char *name;
};
ILangList *ilanglist = NULL;

static int do_nick_name_multibyte(char *nick);
static int do_nick_name_standard(char *nick);

/* These characters are ALWAYS disallowed... from remote, in
 * multibyte, etc.. even though this might mean a certain
 * (legit) character cannot be used (eg: in chinese GBK).
 * - no breaking space
 * - ! (nick!user seperator)
 * - prefix chars: +, %, @, &, ~
 * - channel chars: #
 * - scary chars: $, :, ', ", ?, *, ',', '.'
 * NOTE: the caller should also check for ascii <= 32.
 * [CHANGING THIS WILL CAUSE SECURITY/SYNCH PROBLEMS AND WILL
 *  VIOLATE YOUR ""RIGHT"" ON SUPPORT IMMEDIATELY]
 */
const char *illegalnickchars = "\xA0!+%@&~#$:'\"?*,.";

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
	langsinuse[0] = '\0';
#ifdef DEBUGMODE
	if (ilanglist)
		abort();
#endif
}

void charsys_reset_pretest(void)
{
	langav = 0;
}

static inline void ilang_swap(ILangList *one, ILangList *two)
{
char *tmp = one->name;
	one->name = two->name;
	two->name = tmp;
}

static void ilang_sort(void)
{
ILangList *outer, *inner;
char *tmp;

	/* Selection sort -- perhaps optimize to qsort/whatever if
     * possible? ;)
     */
	for (outer=ilanglist; outer; outer=outer->next)
	{
		for (inner=outer->next; inner; inner=inner->next)
		{
			if (strcmp(outer->name, inner->name) > 0)
				ilang_swap(outer, inner);
		}
	}
}

void charsys_finish(void)
{
ILangList *e, *e_next;

	/* Sort alphabetically */
	ilang_sort();

	/* [note: this can be optimized] */
	langsinuse[0] = '\0';
	for (e=ilanglist; e; e=e->next)
	{
		strlcat(langsinuse, e->name, sizeof(langsinuse));
		if (e->next)
			strlcat(langsinuse, ",", sizeof(langsinuse));
	}
	
	/* Free everything */
	for (e=ilanglist; e; e=e_next)
	{
		e_next=e->next;
		MyFree(e->name);
		MyFree(e);
	}
	ilanglist = NULL;
#ifdef DEBUGMODE
	ircd_log(LOG_ERROR, "[Debug] langsinuse: '%s'", langsinuse);
	if (strlen(langsinuse) > 490)
		abort();
#endif
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
MBList *m = MyMallocEx(sizeof(MBList));

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
	{
		if ((*s <= 32) || strchr(illegalnickchars, *s))
		{
			config_error("INTERNAL ERROR: charsys_addallowed() called for illegal characters: %s", s);
#ifdef DEBUGMODE
			abort();
#endif
		}
		char_atribs[(unsigned int)*s] |= ALLOWN;
	}
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
		/* Some characters are ALWAYS illegal, so they have to be disallowed here */
		if ((*ch <= 32) || strchr(illegalnickchars, *ch))
			return 0;
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

/** Does some very basic checking on remote nickname.
 * It's only purpose is not to cause the whole network
 * to fall down in pieces, that's all. Display problems
 * are not really handled here. They are assumed to have been
 * checked by PROTOCTL NICKCHARS= -- Syzop.
 */
int do_remote_nick_name(char *nick)
{
char *c;

	for (c=nick; *c; c++)
		if ((*c <= 32) || strchr(illegalnickchars, *c))
			return 0;

	return (c - nick);
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
		config_error("ERROR: set::allowed-nickchars specifies incorrect combination "
		             "of languages: high-ascii languages (such as german, french, etc) "
		             "cannot be mixed with chinese/..");
		return -1;
	}
	if (langav & LANGAV_LATIN1)
		x++;
	if (langav & LANGAV_LATIN2)
		x++;
	if (langav & LANGAV_ISO8859_7)
		x++;
	if (langav & LANGAV_ISO8859_9)
		x++;
	if (langav & LANGAV_W1250)
		x++;
	if (langav & LANGAV_W1251)
		x++;
	if ((langav & LANGAV_LATIN2W1250) && !(langav & LANGAV_LATIN2) && !(langav & LANGAV_W1250))
	    x++;
	if (x > 1)
	{
		config_status("WARNING: set::allowed-nickchars: "
		            "Mixing of charsets (eg: latin1+latin2) can cause display problems");
	}
	return 1;
}

static LangList *charsys_find_language(char *name)
{
int start = 0;
int stop = ARRAY_SIZEOF(langlist)-1;
int mid;

	while (start <= stop)
	{
		mid = (start+stop)/2;
		if (!langlist[mid].directive || smycmp(name, langlist[mid].directive) < 0)
			stop = mid-1;
		else if (strcmp(name, langlist[mid].directive) == 0)
			return &langlist[mid];
		else
			start = mid+1;
	}
	return NULL;
}

/** Check if language is available. */
int charsys_test_language(char *name)
{
LangList *l = charsys_find_language(name);

	if (l)
	{
		langav |= l->setflags;
		return 1;
	}
	if (!strcmp(name, "euro-west"))
	{
		config_error("set::allowed-nickchars: ERROR: 'euro-west' got renamed to 'latin1'");
		return 0;
	}
	return 0;
}

static void charsys_doadd_language(char *name)
{
LangList *l;
ILangList *li;
int found;
char tmp[512], *lang, *p;

	l = charsys_find_language(name);
	if (!l)
	{
#ifdef DEBUGMODE
		abort();
#endif
		return;
	}

	strlcpy(tmp, l->code, sizeof(tmp));
	for (lang = strtoken(&p, tmp, ","); lang; lang = strtoken(&p, NULL, ","))
	{
		/* Check if present... */
		found=0;
		for (li=ilanglist; li; li=li->next)
			if (!strcmp(li->name, lang))
			{
				found = 1;
				break;
			}
		if (!found)
		{
			/* Add... */
			li = MyMallocEx(sizeof(ILangList));
			li->name = strdup(lang);
			AddListItem(li, ilanglist);
		}
	}
}

void charsys_add_language(char *name)
{
char latin1=0, latin2=0, w1250=0, w1251=0, chinese=0;

	/** Note: there could well be some characters missing in the lists below.
	 *        While I've seen other altnernatives that just allow pretty much
	 *        every accent that exists even for dutch (where we rarely use
	 *        accents except for like 3 types), I rather prefer to use a bit more
	 *        reasonable aproach ;). That said, anyone is welcome to make
	 *        suggestions about characters that should be added (or removed)
	 *        of course. -- Syzop
	 */

	/* Add our language to our list */
	charsys_doadd_language(name);

	/* GROUPS */
	if (!strcmp(name, "latin1"))
		latin1 = 1;
	else if (!strcmp(name, "latin2"))
		latin2 = 1;
	else if (!strcmp(name, "windows-1250"))
		w1250 = 1;
	else if (!strcmp(name, "windows-1251"))
		w1251 = 1;
	else if (!strcmp(name, "chinese") || !strcmp(name, "gbk"))
		chinese = 1;
	
	/* INDIVIDUAL CHARSETS */

	/* [LATIN1] */
	if (latin1 || !strcmp(name, "german"))
	{
		/* a", A", o", O", u", U" and es-zett */
		charsys_addallowed("‰ƒˆ÷¸‹ﬂ");
	}
	if (latin1 || !strcmp(name, "swiss-german"))
	{
		/* a", A", o", O", u", U"  */
		charsys_addallowed("‰ƒˆ÷¸‹");
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
		charsys_addallowed("ÈÎˆÔ¸Ë");
	}
	if (latin1 || !strcmp(name, "danish"))
	{
		/* supplied by klaus:
		 * <ae>, <AE>, ao, Ao, o/, O/ */
		charsys_addallowed("Ê∆Â≈¯ÿ");
	}
	if (latin1 || !strcmp(name, "french"))
	{
		/* A`, A^, a`, a^, weird-C, weird-c, E`, E', E^, E", e`, e', e^, e",
		 * I^, I", i^, i", O^, o^, U`, U^, U", u`, u", u`, y" [not in that order, sry]
		 * Hmm.. there might be more, but I'm not sure how common they are
		 * and I don't think they are always displayed correctly (?).
		 */
		charsys_addallowed("¿¬‡‚«Á»… ÀËÈÍÎŒœÓÔ‘ÙŸ€‹˘˚¸ˇ");
	}
	if (latin1 || !strcmp(name, "spanish"))
	{
		/* a', A', e', E', i', I', o', O', u', U', u", U", n~, N~ */
		charsys_addallowed("·¡È…ÌÕÛ”˙⁄¸‹Ò—");
	}
	if (latin1 || !strcmp(name, "italian"))
	{
		/* A`, E`, E', I`, I', O`, O', U`, U', a`, e`, e', i`, i', o`, o', u`, u' */
		charsys_addallowed("¿»…ÃÕ“”Ÿ⁄‡ËÈÏÌÚÛ˘˙");
	}
	if (latin1 || !strcmp(name, "catalan"))
	{
		/* supplied by Trocotronic */
		/* a`, A`, e`, weird-c, weird-C, E`, e', E', i', I', o`, O`, o', O', u', U', i", I", u", U", weird-dot */
		charsys_addallowed("‡¿Á«Ë»È…ÌÕÚ“Û”˙⁄Ôœ¸‹");
	}
	if (latin1 || !strcmp(name, "swedish"))
	{
		/* supplied by Tank */
		/* ao, Ao, a", A", o", O" */ 
		charsys_addallowed("Â≈‰ƒˆ÷");
	}
	if (latin1 || !strcmp(name, "icelandic"))
	{
		/* supplied by Saevar */
		charsys_addallowed("∆Ê÷ˆ¡·ÕÌ–⁄˙”Û›˝ﬁ˛");
	}

	/* [LATIN2] */
	/* actually hungarian is a special case, include it in both w1250 and latin2 ;p */
	if (latin2 || w1250 || !strcmp(name, "hungarian"))
	{
		/* supplied by AngryWolf */
		/* a', e', i', o', o", o~, u', u", u~, A', E', I', O', O", O~, U', U", U~ */
		charsys_addallowed("·ÈÌÛˆı˙¸˚¡…Õ”÷’⁄‹€");
	}
	/* same is true for romanian: latin2 & w1250 compatible */
	if (latin2 || w1250 || !strcmp(name, "romanian"))
	{
		/* With some help from crazytoon */
		/* 'S,' 's,' 'A^' 'A<' 'I^' 'T,' 'a^' 'a<' 'i^' 't,' */
		charsys_addallowed("™∫¬√Œﬁ‚„Ó˛");
	}
	
	if (latin2 || !strcmp(name, "polish"))
	{
		/* supplied by k4be */
		charsys_addallowed("±ÊÍ≥ÒÛ∂øº°∆ £—”¶Ø¨");
	}

	/* [windows 1250] */
	if (w1250 || !strcmp(name, "polish-w1250"))
	{
		/* supplied by k4be */
		charsys_addallowed("πÊÍ≥ÒÛúøü•∆ £—”åØè");
	}
	if (w1250 || !strcmp(name, "czech-w1250"))
	{
		/* Syzop [probably incomplete] */
		charsys_addallowed("äçéöùû¡»…ÃÕœ“”ÿŸ⁄›·ËÈÏÌÔÚÛ¯˘˙˝");
	}
	if (w1250 || !strcmp(name, "slovak-w1250"))
	{
		/* Syzop [probably incomplete] */
		charsys_addallowed("äçéöùûºæ¿¡ƒ≈»…Õœ‡·‰ÂËÈÌÔÚÛÙ˙˝");
	}

	/* [windows 1251] */
	if (w1251 || !strcmp(name, "russian-w1251"))
	{
		/* supplied by Roman Parkin:
		 * 128-159 and 223-254
		 */
		charsys_addallowed("¿¡¬√ƒ≈∆«»… ÀÃÕŒœ–—“”‘’÷◊ÿŸ⁄€‹›ﬁﬂ‡·‚„‰ÂÊÁËÈÍÎÏÌÓÔÒÚÛÙıˆ˜¯˘˙˚¸˝˛ˇ®∏");
	}
	
	if (w1251 || !strcmp(name, "belarussian-w1251"))
	{
		/* supplied by Bock (Samets Anton) & ss:
		 * 128-159, 161, 162, 178, 179 and 223-254
		 * Corrected 01.11.2006 to more "correct" behavior by Bock
		 */
		charsys_addallowed("¿¡¬√ƒ≈®∆«≤… ÀÃÕŒœ–—“”°‘’÷◊ÿ€‹›ﬁﬂ‡·‚„‰Â∏ÊÁ≥ÈÍÎÏÌÓÔÒÚÛ¢Ùıˆ˜¯˚¸˝˛ˇ");
	}	
	
	if (w1251 || !strcmp(name, "ukrainian-w1251"))
	{
		/* supplied by Anton Samets & ss:
		 * 128-159, 170, 175, 178, 179, 186, 191 and 223-254
		 * Corrected 01.11.2006 to more "correct" behavior by core
		 */
		charsys_addallowed("¿¡¬√•ƒ≈™∆«»≤Ø… ÀÃÕŒœ–—“”‘’÷◊ÿŸ‹ﬁﬂ‡·‚„¥‰Â∫ÊÁË≥øÈÍÎÏÌÓÔÒÚÛÙıˆ˜¯˘¸˛ˇ");
	}	

	/* [GREEK] */	
	if (!strcmp(name, "greek"))
	{
		/* supplied by GSF */
		/* ranges from rfc1947 / iso 8859-7 */
		charsys_addallowed("∂∏π∫ºæø¿¡¬√ƒ≈∆«»… ÀÃÕŒœ–—”‘’÷◊ÿŸ⁄€‹›ﬁﬂ‡·‚„‰ÂÊÁËÈÍÎÏÌÓÔÒÚÛÙ");
	}

	/* [TURKISH] */
	if (!strcmp(name, "turkish"))
	{
		/* Supplied by Ayberk Yancatoral */
		charsys_addallowed("ˆ÷Á«˛ﬁ¸‹–˝");
	}

	/* [HEBREW] */
	if (!strcmp(name, "hebrew"))
	{
		/* Supplied by PHANTOm. */
		/* 0xE0 - 0xFE */
		charsys_addallowed("‡·‚„‰ÂÊÁËÈÍÎÏÌÓÔÒÚÛÙıˆ˜¯˘˙˚¸˝˛");
	}

	/* [CHINESE] */
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
		charsys_addmultibyterange(0x81, 0xa0, 0x40, 0x7e); /* GBK/3 - lower half */
		charsys_addmultibyterange(0x81, 0xa0, 0x80, 0xfe); /* GBK/3 - upper half */
		charsys_addmultibyterange(0xaa, 0xfe, 0x40, 0x7e); /* GBK/4 - lower half */
		charsys_addmultibyterange(0xaa, 0xfe, 0x80, 0xa0); /* GBK/4 - upper half */
	}
}
