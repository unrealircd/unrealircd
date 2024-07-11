/*
 * Unreal Internet Relay Chat Daemon, src/charsys.c
 * (C) Copyright 2005-2017 Bram Matthys and The UnrealIRCd Team.
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

#include "unrealircd.h"

#ifndef ARRAY_SIZEOF
 #define ARRAY_SIZEOF(x) (sizeof((x))/sizeof((x)[0]))
#endif

ModuleHeader MOD_HEADER
= {
	"charsys",	/* Name of module */
	"5.0", /* Version */
	"Character System (set::allowed-nickchars)", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* NOTE: it is guaranteed that char is unsigned by compiling options
 *       (-funsigned-char @ gcc, /J @ MSVC)
 * NOTE2: Original credit for supplying the correct chinese
 *        coderanges goes to: RexHsu, Mr.WebBar and Xuefer
 */

/** Our multibyte structure */
typedef struct MBList MBList;
struct MBList
{
	MBList *next;
	char s1, e1, s2, e2;
};
MBList *mblist = NULL, *mblist_tail = NULL;

/* Use this to prevent mixing of certain combinations
 * (such as GBK & high-ascii, etc)
 */
static int langav = 0;
char langsinuse[4096];

/* bitmasks: */
#define LANGAV_ASCII			0x000001 /* 8 bit ascii */
#define LANGAV_LATIN1			0x000002 /* latin1 (western europe) */
#define LANGAV_LATIN2			0x000004 /* latin2 (eastern europe, eg: polish) */
#define LANGAV_ISO8859_7		0x000008 /* greek */
#define LANGAV_ISO8859_8I		0x000010 /* hebrew */
#define LANGAV_ISO8859_9		0x000020 /* turkish */
#define LANGAV_W1250			0x000040 /* windows-1250 (eg: polish-w1250) */
#define LANGAV_W1251			0x000080 /* windows-1251 (eg: russian) */
#define LANGAV_LATIN2W1250		0x000100 /* Compatible with both latin2 AND windows-1250 (eg: hungarian) */
#define LANGAV_ISO8859_6		0x000200 /* arabic */
#define LANGAV_GBK			0x001000 /* (Chinese) GBK encoding */
#define LANGAV_UTF8			0x002000 /* any UTF8 encoding */
#define LANGAV_LATIN_UTF8		0x004000 /* UTF8: latin script */
#define LANGAV_CYRILLIC_UTF8		0x008000 /* UTF8: cyrillic script */
#define LANGAV_GREEK_UTF8		0x010000 /* UTF8: greek script */
#define LANGAV_HEBREW_UTF8		0x020000 /* UTF8: hebrew script */
#define LANGAV_ARABIC_UTF8		0x040000 /* UTF8: arabic script */
typedef struct LangList LangList;
struct LangList
{
	char *directive;
	char *code;
	int setflags;
};

/* MUST be alphabetized (first column) */
static LangList langlist[] = {
	{ "arabic-utf8", "ara-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_ARABIC_UTF8 },
	{ "belarussian-utf8", "blr-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_CYRILLIC_UTF8 },
	{ "belarussian-w1251", "blr", LANGAV_ASCII|LANGAV_W1251 },
	{ "catalan",      "cat", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "catalan-utf8", "cat-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "chinese",      "chi-j,chi-s,chi-t", LANGAV_GBK },
	{ "chinese-ja",   "chi-j", LANGAV_GBK },
	{ "chinese-simp", "chi-s", LANGAV_GBK },
	{ "chinese-trad", "chi-t", LANGAV_GBK },
	{ "cyrillic-utf8", "blr-utf8,rus-utf8,ukr-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_CYRILLIC_UTF8 },
	{ "czech",        "cze-m", LANGAV_ASCII|LANGAV_W1250 },
	{ "czech-utf8",   "cze-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "danish",       "dan", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "danish-utf8",  "dan-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "dutch",        "dut", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "dutch-utf8",   "dut-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "estonian-utf8","est-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "french",       "fre", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "french-utf8",  "fre-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "gbk",          "chi-s,chi-t,chi-j", LANGAV_GBK },
	{ "german",       "ger", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "german-utf8",  "ger-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "greek",        "gre", LANGAV_ASCII|LANGAV_ISO8859_7 },
	{ "greek-utf8",   "gre-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_GREEK_UTF8 },
	{ "hebrew",       "heb", LANGAV_ASCII|LANGAV_ISO8859_8I },
	{ "hebrew-utf8",  "heb-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_HEBREW_UTF8 },
	{ "hungarian",    "hun", LANGAV_ASCII|LANGAV_LATIN2W1250 },
	{ "hungarian-utf8","hun-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "icelandic",    "ice", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "icelandic-utf8","ice-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "italian",      "ita", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "italian-utf8", "ita-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "latin-utf8",   "cat-utf8,cze-utf8,dan-utf8,dut-utf8,fre-utf8,ger-utf8,hun-utf8,ice-utf8,ita-utf8,pol-utf8,rum-utf8,slo-utf8,spa-utf8,swe-utf8,tur-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "latin1",       "cat,dut,fre,ger,ita,spa,swe", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "latin2",       "hun,pol,rum", LANGAV_ASCII|LANGAV_LATIN2 },
	{ "latvian-utf8", "lav-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "lithuanian-utf8","lit-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "polish",       "pol", LANGAV_ASCII|LANGAV_LATIN2 },
	{ "polish-utf8",  "pol-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "polish-w1250", "pol-m", LANGAV_ASCII|LANGAV_W1250 },
	{ "romanian",     "rum", LANGAV_ASCII|LANGAV_LATIN2W1250 },
	{ "romanian-utf8","rum-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "russian-utf8", "rus-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_CYRILLIC_UTF8 },
	{ "russian-w1251","rus", LANGAV_ASCII|LANGAV_W1251 },
	{ "slovak",       "slo-m", LANGAV_ASCII|LANGAV_W1250 },
	{ "slovak-utf8",  "slo-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "spanish",      "spa", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "spanish-utf8", "spa-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "swedish",      "swe", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "swedish-utf8", "swe-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "swiss-german", "swg", LANGAV_ASCII|LANGAV_LATIN1 },
	{ "swiss-german-utf8", "swg-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "turkish",      "tur", LANGAV_ASCII|LANGAV_ISO8859_9 },
	{ "turkish-utf8", "tur-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_LATIN_UTF8 },
	{ "ukrainian-utf8", "ukr-utf8", LANGAV_ASCII|LANGAV_UTF8|LANGAV_CYRILLIC_UTF8 },
	{ "ukrainian-w1251", "ukr", LANGAV_ASCII|LANGAV_W1251 },
	{ "windows-1250", "cze-m,pol-m,rum,slo-m,hun",  LANGAV_ASCII|LANGAV_W1250 },
	{ "windows-1251", "rus,ukr,blr", LANGAV_ASCII|LANGAV_W1251 },
	{ NULL, NULL, 0 }
};

/* For temporary use during config_run */
typedef struct ILangList ILangList;
struct ILangList
{
	ILangList *prev, *next;
	char *name;
};
ILangList *ilanglist = NULL;

/* These characters are ALWAYS disallowed... from remote, in
 * multibyte, etc.. even though this might mean a certain
 * (legit) character cannot be used (eg: in chinese GBK).
 * - ! (nick!user seperator)
 * - prefix chars: +, %, @, &, ~
 * - channel chars: #
 * - scary chars: $, :, ', ", ?, *, ',', '.'
 * NOTE: the caller should also check for ascii <= 32.
 * [CHANGING THIS WILL CAUSE SECURITY/SYNCH PROBLEMS AND WILL
 *  VIOLATE YOUR ""RIGHT"" ON SUPPORT IMMEDIATELY]
 */
const char *illegalnickchars = "!+%@&~#$:'\"?*,.";

/* Forward declarations */
void charsys_free_mblist(void);
int _do_nick_name(char *nick);
int _do_remote_nick_name(char *nick);
static int do_nick_name_multibyte(char *nick);
static int do_nick_name_standard(char *nick);
void charsys_reset(void);
void charsys_reset_pretest(void);
void charsys_finish(void);
void charsys_addmultibyterange(char s1, char e1, char s2, char e2);
void charsys_addallowed(char *s);
int charsys_test_language(char *name);
void charsys_add_language(char *name);
static void charsys_doadd_language(char *name);
int charsys_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int charsys_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
int charsys_config_posttest(int *errs);
char *_charsys_get_current_languages(void);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_DO_NICK_NAME, _do_nick_name);
	EfunctionAdd(modinfo->handle, EFUNC_DO_REMOTE_NICK_NAME, _do_remote_nick_name);
	EfunctionAddString(modinfo->handle, EFUNC_CHARSYS_GET_CURRENT_LANGUAGES, _charsys_get_current_languages);
	charsys_reset();
	charsys_reset_pretest();
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, charsys_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, charsys_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, charsys_config_run);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD()
{
	charsys_finish();
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD()
{
	charsys_free_mblist();
	return MOD_SUCCESS;
}

int charsys_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::allowed-nickchars... */
	if (!ce || !ce->name || strcmp(ce->name, "allowed-nickchars"))
		return 0;

	if (ce->value)
	{
		config_error("%s:%i: set::allowed-nickchars: please use 'allowed-nickchars { name; };' "
					 "and not 'allowed-nickchars name;'",
					 ce->file->filename, ce->line_number);
		/* Give up immediately. Don't bother the user with any other errors. */
		errors++;
		*errs = errors;
		return -1;
	}

	for (cep = ce->items; cep; cep=cep->next)
	{
		if (!charsys_test_language(cep->name))
		{
			config_error("%s:%i: set::allowed-nickchars: Unknown (sub)language '%s'",
				ce->file->filename, ce->line_number, cep->name);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int charsys_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::allowed-nickchars... */
	if (!ce || !ce->name || strcmp(ce->name, "allowed-nickchars"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
		charsys_add_language(cep->name);

	return 1;
}

/** Check if the specified charsets during the TESTING phase can be
 * premitted without getting into problems.
 * RETURNS: -1 in case of failure, 1 if ok
 */
int charsys_config_posttest(int *errs)
{
	int errors = 0;
	int x=0;

	if ((langav & LANGAV_ASCII) && (langav & LANGAV_GBK))
	{
		config_error("ERROR: set::allowed-nickchars specifies incorrect combination "
		             "of languages: high-ascii languages (such as german, french, etc) "
		             "cannot be mixed with chinese/..");
		return -1;
	}
	if (langav & LANGAV_LATIN_UTF8)
		x++;
	if (langav & LANGAV_GREEK_UTF8)
		x++;
	if (langav & LANGAV_CYRILLIC_UTF8)
		x++;
	if (langav & LANGAV_HEBREW_UTF8)
		x++;
	if (langav & LANGAV_LATIN1)
		x++;
	if (langav & LANGAV_LATIN2)
		x++;
	if (langav & LANGAV_ISO8859_6)
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
#if 0
// I don't think this should be hard error, right? Some combinations may be problematic, but not all.
		if (langav & LANGAV_LATIN_UTF8)
		{
			config_error("ERROR: set::allowed-nickchars: you cannot combine 'latin-utf8' with any other character set");
			errors++;
		}
		if (langav & LANGAV_GREEK_UTF8)
		{
			config_error("ERROR: set::allowed-nickchars: you cannot combine 'greek-utf8' with any other character set");
			errors++;
		}
		if (langav & LANGAV_CYRILLIC_UTF8)
		{
			config_error("ERROR: set::allowed-nickchars: you cannot combine 'cyrillic-utf8' with any other character set");
			errors++;
		}
		if (langav & LANGAV_HEBREW_UTF8)
		{
			config_error("ERROR: set::allowed-nickchars: you cannot combine 'hebrew-utf8' with any other character set");
			errors++;
		}
		if (langav & LANGAV_ARABIC_UTF8)
		{
			config_error("ERROR: set::allowed-nickchars: you cannot combine 'arabic-utf8' with any other character set");
			errors++;
		}
#endif
		config_status("WARNING: set::allowed-nickchars: Mixing of charsets (eg: latin1+latin2) may cause display problems");
	}

	*errs = errors;
	return errors ? -1 : 1;
}

void charsys_free_mblist(void)
{
	MBList *m, *m_next;
	for (m=mblist; m; m=m_next)
	{
		m_next = m->next;
		safe_free(m);
	}
	mblist=mblist_tail=NULL;
}

/** Called on boot and just before config run */
void charsys_reset(void)
{
	int i;
	MBList *m, *m_next;

	/* First, reset everything */
	for (i=0; i < 256; i++)
		char_atribs[i] &= ~ALLOWN;
	charsys_free_mblist();
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
	non_utf8_nick_chars_in_use = 0;
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
		safe_free(e->name);
		safe_free(e);
	}
	ilanglist = NULL;
#ifdef DEBUGMODE
	if (strlen(langsinuse) > 490)
		abort();
#endif
	charsys_check_for_changes();
}

/** Add a character range to the multibyte list.
 * Eg: charsys_addmultibyterange(0xaa, 0xbb, 0x00, 0xff) for 0xaa00-0xbbff.
 * @param s1 Start of highest byte
 * @param e1 End of highest byte
 * @param s2 Start of lowest byte
 * @param e2 End of lowest byte
 */
void charsys_addmultibyterange(char s1, char e1, char s2, char e2)
{
MBList *m = safe_alloc(sizeof(MBList));

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
		char_atribs[(unsigned char)*s] |= ALLOWN;
	}
}

void charsys_addallowed_range(unsigned char from, unsigned char to)
{
	unsigned char i;

	for (i = from; i != to; i++)
		char_atribs[i] |= ALLOWN;
}

int _do_nick_name(char *nick)
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
	{
		ch--;
		len--;
	}
	*ch = '\0';
	return len;
}

/** Does some very basic checking on remote nickname.
 * It's only purpose is not to cause the whole network
 * to fall down in pieces, that's all. Display problems
 * are not really handled here. They are assumed to have been
 * checked by PROTOCTL NICKCHARS= -- Syzop.
 */
int _do_remote_nick_name(char *nick)
{
	char *c;

	/* Don't allow nicks to start with a digit, ever. */
	if ((*nick == '-') || isdigit(*nick))
		return 0;

	/* Now the other, more relaxed checks.. */
	for (c=nick; *c; c++)
		if ((*c <= 32) || strchr(illegalnickchars, *c))
			return 0;

	return (c - nick);
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

static LangList *charsys_find_language_code(char *code)
{
	int i;
	for (i = 0; langlist[i].code; i++)
		if (!strcasecmp(langlist[i].code, code))
			return &langlist[i];
	return NULL;
}

/** Check if language is available. */
int charsys_test_language(char *name)
{
	LangList *l = charsys_find_language(name);

	if (l)
	{
		langav |= l->setflags;
		if (!(l->setflags & LANGAV_UTF8))
			non_utf8_nick_chars_in_use = 1;
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
			li = safe_alloc(sizeof(ILangList));
			safe_strdup(li->name, lang);
			AddListItem(li, ilanglist);
		}
	}
}

void charsys_add_language(char *name)
{
	char latin1=0, latin2=0, w1250=0, w1251=0, chinese=0;
	char latin_utf8=0, cyrillic_utf8=0;

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
	if (!strcmp(name, "latin-utf8"))
		latin_utf8 = 1;
	else if (!strcmp(name, "cyrillic-utf8"))
		cyrillic_utf8 = 1;
	else if (!strcmp(name, "latin1"))
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

	/* [LATIN1] and [LATIN-UTF8] */
	if (latin1 || !strcmp(name, "german"))
	{
		/* a", A", o", O", u", U" and es-zett */
		charsys_addallowed("‰ƒˆ÷¸‹ﬂ");
	}
	if (latin_utf8 || !strcmp(name, "german-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x84, 0x84);
		charsys_addmultibyterange(0xc3, 0xc3, 0x96, 0x96);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9c, 0x9c);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9f, 0x9f);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa4, 0xa4);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb6, 0xb6);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbc, 0xbc);
	}
	if (latin1 || !strcmp(name, "swiss-german"))
	{
		/* a", A", o", O", u", U"  */
		charsys_addallowed("‰ƒˆ÷¸‹");
	}
	if (latin_utf8 || !strcmp(name, "swiss-german-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x84, 0x84);
		charsys_addmultibyterange(0xc3, 0xc3, 0x96, 0x96);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9c, 0x9c);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa4, 0xa4);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb6, 0xb6);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbc, 0xbc);
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
	if (latin_utf8 || !strcmp(name, "dutch-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0xa8, 0xa9);
		charsys_addmultibyterange(0xc3, 0xc3, 0xab, 0xab);
		charsys_addmultibyterange(0xc3, 0xc3, 0xaf, 0xaf);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb6, 0xb6);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbc, 0xbc);
	}
	if (latin1 || !strcmp(name, "danish"))
	{
		/* supplied by klaus:
		 * <ae>, <AE>, ao, Ao, o/, O/ */
		charsys_addallowed("Ê∆Â≈¯ÿ");
	}
	if (latin_utf8 || !strcmp(name, "danish-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x85, 0x86);
		charsys_addmultibyterange(0xc3, 0xc3, 0x98, 0x98);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa5, 0xa6);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb8, 0xb8);
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
	if (latin_utf8 || !strcmp(name, "french-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x80, 0x80);
		charsys_addmultibyterange(0xc3, 0xc3, 0x82, 0x82);
		charsys_addmultibyterange(0xc3, 0xc3, 0x87, 0x8b);
		charsys_addmultibyterange(0xc3, 0xc3, 0x8e, 0x8f);
		charsys_addmultibyterange(0xc3, 0xc3, 0x94, 0x94);
		charsys_addmultibyterange(0xc3, 0xc3, 0x99, 0x99);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9b, 0x9c);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa0, 0xa0);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa2, 0xa2);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa7, 0xab);
		charsys_addmultibyterange(0xc3, 0xc3, 0xae, 0xaf);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb4, 0xb4);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb9, 0xb9);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbb, 0xbc);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbf, 0xbf);
	}
	if (latin1 || !strcmp(name, "spanish"))
	{
		/* a', A', e', E', i', I', o', O', u', U', u", U", n~, N~ */
		charsys_addallowed("·¡È…ÌÕÛ”˙⁄¸‹Ò—");
	}
	if (latin_utf8 || !strcmp(name, "spanish-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x81, 0x81);
		charsys_addmultibyterange(0xc3, 0xc3, 0x89, 0x89);
		charsys_addmultibyterange(0xc3, 0xc3, 0x8d, 0x8d);
		charsys_addmultibyterange(0xc3, 0xc3, 0x91, 0x91);
		charsys_addmultibyterange(0xc3, 0xc3, 0x93, 0x93);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9a, 0x9a);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9c, 0x9c);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa1, 0xa1);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa9, 0xa9);
		charsys_addmultibyterange(0xc3, 0xc3, 0xad, 0xad);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb1, 0xb1);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb3, 0xb3);
		charsys_addmultibyterange(0xc3, 0xc3, 0xba, 0xba);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbc, 0xbc);
	}
	if (latin1 || !strcmp(name, "italian"))
	{
		/* A`, E`, E', I`, I', O`, O', U`, U', a`, e`, e', i`, i', o`, o', u`, u' */
		charsys_addallowed("¿»…ÃÕ“”Ÿ⁄‡ËÈÏÌÚÛ˘˙");
	}
	if (latin_utf8 || !strcmp(name, "italian-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x80, 0x80);
		charsys_addmultibyterange(0xc3, 0xc3, 0x88, 0x89);
		charsys_addmultibyterange(0xc3, 0xc3, 0x8c, 0x8d);
		charsys_addmultibyterange(0xc3, 0xc3, 0x92, 0x93);
		charsys_addmultibyterange(0xc3, 0xc3, 0x99, 0x9a);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa0, 0xa0);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa8, 0xa9);
		charsys_addmultibyterange(0xc3, 0xc3, 0xac, 0xad);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb2, 0xb3);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb9, 0xba);
	}
	if (latin1 || !strcmp(name, "catalan"))
	{
		/* supplied by Trocotronic */
		/* a`, A`, e`, weird-c, weird-C, E`, e', E', i', I', o`, O`, o', O', u', U', i", I", u", U", weird-dot */
		charsys_addallowed("‡¿Á«Ë»È…ÌÕÚ“Û”˙⁄Ôœ¸‹");
	}
	if (latin_utf8 || !strcmp(name, "catalan-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x80, 0x80);
		charsys_addmultibyterange(0xc3, 0xc3, 0x87, 0x89);
		charsys_addmultibyterange(0xc3, 0xc3, 0x8d, 0x8d);
		charsys_addmultibyterange(0xc3, 0xc3, 0x8f, 0x8f);
		charsys_addmultibyterange(0xc3, 0xc3, 0x92, 0x93);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9a, 0x9a);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9c, 0x9c);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa0, 0xa0);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa7, 0xa9);
		charsys_addmultibyterange(0xc3, 0xc3, 0xad, 0xad);
		charsys_addmultibyterange(0xc3, 0xc3, 0xaf, 0xaf);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb2, 0xb3);
		charsys_addmultibyterange(0xc3, 0xc3, 0xba, 0xba);
	}
	if (latin1 || !strcmp(name, "swedish"))
	{
		/* supplied by Tank */
		/* ao, Ao, a", A", o", O" */
		charsys_addallowed("Â≈‰ƒˆ÷");
	}
	if (latin_utf8 || !strcmp(name, "swedish-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x84, 0x85);
		charsys_addmultibyterange(0xc3, 0xc3, 0x96, 0x96);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa4, 0xa5);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb6, 0xb6);
	}
	if (latin1 || !strcmp(name, "icelandic"))
	{
		/* supplied by Saevar */
		charsys_addallowed("∆Ê÷ˆ¡·ÕÌ–⁄˙”Û›˝ﬁ˛");
	}
	if (latin_utf8 || !strcmp(name, "icelandic-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x81, 0x81);
		charsys_addmultibyterange(0xc3, 0xc3, 0x86, 0x86);
		charsys_addmultibyterange(0xc3, 0xc3, 0x8d, 0x8d);
		charsys_addmultibyterange(0xc3, 0xc3, 0x90, 0x90);
		charsys_addmultibyterange(0xc3, 0xc3, 0x93, 0x93);
		charsys_addmultibyterange(0xc3, 0xc3, 0x96, 0x96);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9a, 0x9a);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9d, 0x9e);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa1, 0xa1);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa6, 0xa6);
		charsys_addmultibyterange(0xc3, 0xc3, 0xad, 0xad);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb0, 0xb0);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb3, 0xb3);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb6, 0xb6);
		charsys_addmultibyterange(0xc3, 0xc3, 0xba, 0xba);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbd, 0xbe);
	}

	/* [LATIN2] and rest of [LATIN-UTF8] */
	/* actually hungarian is a special case, include it in both w1250 and latin2 ;p */
	if (latin2 || w1250 || !strcmp(name, "hungarian"))
	{
		/* supplied by AngryWolf */
		/* a', e', i', o', o", o~, u', u", u~, A', E', I', O', O", O~, U', U", U~ */
		charsys_addallowed("·ÈÌÛˆı˙¸˚¡…Õ”÷’⁄‹€");
	}
	if (latin_utf8 || !strcmp(name, "hungarian-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x81, 0x81);
		charsys_addmultibyterange(0xc3, 0xc3, 0x89, 0x89);
		charsys_addmultibyterange(0xc3, 0xc3, 0x8d, 0x8d);
		charsys_addmultibyterange(0xc3, 0xc3, 0x93, 0x93);
		charsys_addmultibyterange(0xc3, 0xc3, 0x96, 0x96);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9a, 0x9a);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9c, 0x9c);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa1, 0xa1);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa9, 0xa9);
		charsys_addmultibyterange(0xc3, 0xc3, 0xad, 0xad);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb3, 0xb3);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb6, 0xb6);
		charsys_addmultibyterange(0xc3, 0xc3, 0xba, 0xba);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbc, 0xbc);
		charsys_addmultibyterange(0xc5, 0xc5, 0x90, 0x91);
		charsys_addmultibyterange(0xc5, 0xc5, 0xb0, 0xb1);
	}
	/* same is true for romanian: latin2 & w1250 compatible */
	if (latin2 || w1250 || !strcmp(name, "romanian"))
	{
		/* With some help from crazytoon */
		/* 'S,' 's,' 'A^' 'A<' 'I^' 'T,' 'a^' 'a<' 'i^' 't,' */
		charsys_addallowed("™∫¬√Œﬁ‚„Ó˛");
	}
	if (latin_utf8 || !strcmp(name, "romanian-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x82, 0x82);
		charsys_addmultibyterange(0xc3, 0xc3, 0x8e, 0x8e);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa2, 0xa2);
		charsys_addmultibyterange(0xc3, 0xc3, 0xae, 0xae);
		charsys_addmultibyterange(0xc4, 0xc4, 0x82, 0x83);
		charsys_addmultibyterange(0xc5, 0xc5, 0x9e, 0x9f);
		charsys_addmultibyterange(0xc5, 0xc5, 0xa2, 0xa3);
	}

	if (latin2 || !strcmp(name, "polish"))
	{
		/* supplied by k4be */
		charsys_addallowed("±ÊÍ≥ÒÛ∂øº°∆ £—”¶Ø¨");
	}
	if (latin_utf8 || !strcmp(name, "polish-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x93, 0x93);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb3, 0xb3);
		charsys_addmultibyterange(0xc4, 0xc4, 0x84, 0x87);
		charsys_addmultibyterange(0xc4, 0xc4, 0x98, 0x99);
		charsys_addmultibyterange(0xc5, 0xc5, 0x81, 0x84);
		charsys_addmultibyterange(0xc5, 0xc5, 0x9a, 0x9b);
		charsys_addmultibyterange(0xc5, 0xc5, 0xb9, 0xbc);
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
	if (latin_utf8 || !strcmp(name, "czech-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x81, 0x81);
		charsys_addmultibyterange(0xc3, 0xc3, 0x89, 0x89);
		charsys_addmultibyterange(0xc3, 0xc3, 0x8d, 0x8d);
		charsys_addmultibyterange(0xc3, 0xc3, 0x93, 0x93);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9a, 0x9a);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9d, 0x9d);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa1, 0xa1);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa9, 0xa9);
		charsys_addmultibyterange(0xc3, 0xc3, 0xad, 0xad);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb3, 0xb3);
		charsys_addmultibyterange(0xc3, 0xc3, 0xba, 0xba);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbd, 0xbd);
		charsys_addmultibyterange(0xc4, 0xc4, 0x8c, 0x8f);
		charsys_addmultibyterange(0xc4, 0xc4, 0x9a, 0x9b);
		charsys_addmultibyterange(0xc5, 0xc5, 0x87, 0x88);
		charsys_addmultibyterange(0xc5, 0xc5, 0x98, 0x99);
		charsys_addmultibyterange(0xc5, 0xc5, 0xa0, 0xa1);
		charsys_addmultibyterange(0xc5, 0xc5, 0xa4, 0xa5);
		charsys_addmultibyterange(0xc5, 0xc5, 0xae, 0xaf);
		charsys_addmultibyterange(0xc5, 0xc5, 0xbd, 0xbe);
	}
	if (w1250 || !strcmp(name, "slovak-w1250"))
	{
		/* Syzop [probably incomplete] */
		charsys_addallowed("äçéöùûºæ¿¡ƒ≈»…Õœ‡·‰ÂËÈÌÔÚÛÙ˙˝");
	}
	if (latin_utf8 || !strcmp(name, "slovak-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x81, 0x81);
		charsys_addmultibyterange(0xc3, 0xc3, 0x84, 0x84);
		charsys_addmultibyterange(0xc3, 0xc3, 0x89, 0x89);
		charsys_addmultibyterange(0xc3, 0xc3, 0x8d, 0x8d);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa1, 0xa1);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa4, 0xa4);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa9, 0xa9);
		charsys_addmultibyterange(0xc3, 0xc3, 0xad, 0xad);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb3, 0xb4);
		charsys_addmultibyterange(0xc3, 0xc3, 0xba, 0xba);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbd, 0xbd);
		charsys_addmultibyterange(0xc4, 0xc4, 0x8c, 0x8f);
		charsys_addmultibyterange(0xc4, 0xc4, 0xb9, 0xba);
		charsys_addmultibyterange(0xc4, 0xc4, 0xbd, 0xbe);
		charsys_addmultibyterange(0xc5, 0xc5, 0x88, 0x88);
		charsys_addmultibyterange(0xc5, 0xc5, 0x94, 0x95);
		charsys_addmultibyterange(0xc5, 0xc5, 0xa0, 0xa1);
		charsys_addmultibyterange(0xc5, 0xc5, 0xa4, 0xa5);
		charsys_addmultibyterange(0xc5, 0xc5, 0xbd, 0xbe);
	}

	/* [windows 1251] */
	if (w1251 || !strcmp(name, "russian-w1251"))
	{
		/* supplied by Roman Parkin:
		 * 128-159 and 223-254
		 */
		charsys_addallowed("¿¡¬√ƒ≈∆«»… ÀÃÕŒœ–—“”‘’÷◊ÿŸ⁄€‹›ﬁﬂ‡·‚„‰ÂÊÁËÈÍÎÏÌÓÔÒÚÛÙıˆ˜¯˘˙˚¸˝˛ˇ®∏");
	}
	if (cyrillic_utf8 || !strcmp(name, "russian-utf8"))
	{
		charsys_addmultibyterange(0xd0, 0xd0, 0x81, 0x81);
		charsys_addmultibyterange(0xd0, 0xd0, 0x90, 0xbf);
		charsys_addmultibyterange(0xd1, 0xd1, 0x80, 0x8f);
		charsys_addmultibyterange(0xd1, 0xd1, 0x91, 0x91);
	}

	if (w1251 || !strcmp(name, "belarussian-w1251"))
	{
		/* supplied by Bock (Samets Anton) & ss:
		 * 128-159, 161, 162, 178, 179 and 223-254
		 * Corrected 01.11.2006 to more "correct" behavior by Bock
		 */
		charsys_addallowed("¿¡¬√ƒ≈®∆«≤… ÀÃÕŒœ–—“”°‘’÷◊ÿ€‹›ﬁﬂ‡·‚„‰Â∏ÊÁ≥ÈÍÎÏÌÓÔÒÚÛ¢Ùıˆ˜¯˚¸˝˛ˇ");
	}
	if (cyrillic_utf8 || !strcmp(name, "belarussian-utf8"))
	{
		charsys_addmultibyterange(0xd0, 0xd0, 0x81, 0x81);
		charsys_addmultibyterange(0xd0, 0xd0, 0x86, 0x86);
		charsys_addmultibyterange(0xd0, 0xd0, 0x8e, 0x8e);
		charsys_addmultibyterange(0xd0, 0xd0, 0x90, 0x97);
		charsys_addmultibyterange(0xd0, 0xd0, 0x99, 0xa8);
		charsys_addmultibyterange(0xd0, 0xd0, 0xab, 0xb7);
		charsys_addmultibyterange(0xd0, 0xd0, 0xb9, 0xbf);
		charsys_addmultibyterange(0xd1, 0xd1, 0x80, 0x88);
		charsys_addmultibyterange(0xd1, 0xd1, 0x8b, 0x8f);
		charsys_addmultibyterange(0xd1, 0xd1, 0x91, 0x91);
		charsys_addmultibyterange(0xd1, 0xd1, 0x96, 0x96);
		charsys_addmultibyterange(0xd1, 0xd1, 0x9e, 0x9e);
	}

	if (w1251 || !strcmp(name, "ukrainian-w1251"))
	{
		/* supplied by Anton Samets & ss:
		 * 128-159, 170, 175, 178, 179, 186, 191 and 223-254
		 * Corrected 01.11.2006 to more "correct" behavior by core
		 */
		charsys_addallowed("¿¡¬√•ƒ≈™∆«»≤Ø… ÀÃÕŒœ–—“”‘’÷◊ÿŸ‹ﬁﬂ‡·‚„¥‰Â∫ÊÁË≥øÈÍÎÏÌÓÔÒÚÛÙıˆ˜¯˘¸˛ˇ");
	}
	if (cyrillic_utf8 || !strcmp(name, "ukrainian-utf8"))
	{
		charsys_addmultibyterange(0xd0, 0xd0, 0x84, 0x84);
		charsys_addmultibyterange(0xd0, 0xd0, 0x86, 0x87);
		charsys_addmultibyterange(0xd0, 0xd0, 0x90, 0xa9);
		charsys_addmultibyterange(0xd0, 0xd0, 0xac, 0xac);
		charsys_addmultibyterange(0xd0, 0xd0, 0xae, 0xbf);
		charsys_addmultibyterange(0xd1, 0xd1, 0x80, 0x89);
		charsys_addmultibyterange(0xd1, 0xd1, 0x8c, 0x8c);
		charsys_addmultibyterange(0xd1, 0xd1, 0x8e, 0x8f);
		charsys_addmultibyterange(0xd1, 0xd1, 0x94, 0x94);
		charsys_addmultibyterange(0xd1, 0xd1, 0x96, 0x97);
		charsys_addmultibyterange(0xd2, 0xd2, 0x90, 0x91);
	}

	/* [GREEK] */
	if (!strcmp(name, "greek"))
	{
		/* supplied by GSF */
		/* ranges from rfc1947 / iso 8859-7 */
		charsys_addallowed("∂∏π∫ºæø¿¡¬√ƒ≈∆«»… ÀÃÕŒœ–—”‘’÷◊ÿŸ⁄€‹›ﬁﬂ‡·‚„‰ÂÊÁËÈÍÎÏÌÓÔÒÚÛÙ");
	}
	if (!strcmp(name, "greek-utf8"))
	{
		charsys_addmultibyterange(0xce, 0xce, 0x86, 0x86);
		charsys_addmultibyterange(0xce, 0xce, 0x88, 0x8a);
		charsys_addmultibyterange(0xce, 0xce, 0x8c, 0x8c);
		charsys_addmultibyterange(0xce, 0xce, 0x8e, 0xa1);
		charsys_addmultibyterange(0xce, 0xce, 0xa3, 0xbf);
		charsys_addmultibyterange(0xcf, 0xcf, 0x80, 0x84);
	}

	/* [TURKISH] */
	if (!strcmp(name, "turkish"))
	{
		/* Supplied by Ayberk Yancatoral */
		charsys_addallowed("ˆ÷Á«˛ﬁ¸‹–˝");
	}
	if (!strcmp(name, "turkish-utf8"))
	{
		charsys_addmultibyterange(0xc3, 0xc3, 0x87, 0x87);
		charsys_addmultibyterange(0xc3, 0xc3, 0x96, 0x96);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9c, 0x9c);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa7, 0xa7);
		charsys_addmultibyterange(0xc3, 0xc3, 0xb6, 0xb6);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbc, 0xbc);
		charsys_addmultibyterange(0xc4, 0xc4, 0x9e, 0x9f);
		charsys_addmultibyterange(0xc4, 0xc4, 0xb1, 0xb1);
		charsys_addmultibyterange(0xc5, 0xc5, 0x9e, 0x9f);
	}

	/* [HEBREW] */
	if (!strcmp(name, "hebrew"))
	{
		/* Supplied by PHANTOm. */
		/* 0xE0 - 0xFE */
		charsys_addallowed("‡·‚„‰ÂÊÁËÈÍÎÏÌÓÔÒÚÛÙıˆ˜¯˘˙˚¸˝˛");
	}
	if (!strcmp(name, "hebrew-utf8"))
	{
		/* Supplied by Lion-O */
		charsys_addmultibyterange(0xd7, 0xd7, 0x90, 0xaa);
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

	/* [LATVIAN] */
	if (latin_utf8 || !strcmp(name, "latvian-utf8"))
	{
		/* A a, C c, E e, G g, I i, K k, ä ö, U u, é û */
		charsys_addmultibyterange(0xc4, 0xc4, 0x80, 0x81);
		charsys_addmultibyterange(0xc4, 0xc4, 0x92, 0x93);
		charsys_addmultibyterange(0xc4, 0xc4, 0x8c, 0x8d);
		charsys_addmultibyterange(0xc4, 0xc4, 0x92, 0x93);
		charsys_addmultibyterange(0xc4, 0xc4, 0xa2, 0xa3);
		charsys_addmultibyterange(0xc4, 0xc4, 0xaa, 0xab);
		charsys_addmultibyterange(0xc4, 0xc4, 0xb6, 0xb7);
		charsys_addmultibyterange(0xc5, 0xc5, 0xa0, 0xa1);
		charsys_addmultibyterange(0xc5, 0xc5, 0xaa, 0xab);
		charsys_addmultibyterange(0xc5, 0xc5, 0xbd, 0xbe);
	}

	/* [ESTONIAN] */
	if (latin_utf8 || !strcmp(name, "estonian-utf8"))
	{
		/* ı, ‰, ˆ, ¸,  ’, ƒ, ÷, ‹ */
		charsys_addmultibyterange(0xc3, 0xc3, 0xb5, 0xb6);
		charsys_addmultibyterange(0xc3, 0xc3, 0xa4, 0xa4);
		charsys_addmultibyterange(0xc3, 0xc3, 0xbc, 0xbc);
		charsys_addmultibyterange(0xc3, 0xc3, 0x95, 0x96);
		charsys_addmultibyterange(0xc3, 0xc3, 0x84, 0x84);
		charsys_addmultibyterange(0xc3, 0xc3, 0x9c, 0x9c);
	}

	/* [LITHUANIAN] */
	if (latin_utf8 || !strcmp(name, "lithuanian-utf8"))
	{
		/* a, c, e, e, i, ö, u, u, û, A, C, E, E, I, ä, U, U, é */
		charsys_addmultibyterange(0xc4, 0xc4, 0x84, 0x85);
		charsys_addmultibyterange(0xc4, 0xc4, 0x8c, 0x8d);
		charsys_addmultibyterange(0xc4, 0xc4, 0x96, 0x99);
		charsys_addmultibyterange(0xc4, 0xc4, 0xae, 0xaf);
		charsys_addmultibyterange(0xc4, 0xc4, 0xae, 0xaf);
		charsys_addmultibyterange(0xc5, 0xc5, 0xa0, 0xa1);
		charsys_addmultibyterange(0xc5, 0xc5, 0xb2, 0xb3);
		charsys_addmultibyterange(0xc5, 0xc5, 0xaa, 0xab);
		charsys_addmultibyterange(0xc5, 0xc5, 0xbd, 0xbe);
	}

	/* [ARABIC] */
	if (latin_utf8 || !strcmp(name, "arabic-utf8"))
	{
		/* Supplied by Sensiva */
		/*charsys_addallowed("ÿßÿ£ÿ•ÿ¢ÿ°ÿ®ÿ™ÿ´ÿ¨ÿ≠ÿÆÿØÿ∞ÿ±ÿ≤ÿ≥ÿ¥ÿµÿ∂ÿ∑ÿ∏ÿπÿ∫ŸÅŸÇŸÉŸÑŸÖŸÜŸáÿ§ÿ©ŸàŸäŸâÿ¶");*/
		/*- From U+0621 to U+063A (Regex: [\u0621-\u063A])*/
		/* 0xd8a1 - 0xd8ba */
		charsys_addmultibyterange(0xd8, 0xd8, 0xa1, 0xba);
		/*- From U+0641 to U+064A (Regex: [\u0641-\u064A])*/
		/* 0xd981 - 0xd98a */
		charsys_addmultibyterange(0xd9, 0xd9, 0x81, 0x8a);
	}
}

/** This displays all the nick characters that are permitted */
char *charsys_displaychars(void)
{
#if 0
	MBList *m;
	unsigned char hibyte, lobyte;
#endif
	static char buf[512];
	int n = 0;
	int i, j;

	// 		char_atribs[(unsigned char)*s] |= ALLOWN;
	for (i = 0; i <= 255; i++)
	{
		if (char_atribs[i] & ALLOWN)
			buf[n++] = i;
		/* (no bounds checking: first 255 characters always fit a 512 byte buffer) */
	}

#if 0
	for (m=mblist; m; m=m->next)
	{
		for (hibyte = m->s1; hibyte <= m->e1; hibyte++)
		{
			for (lobyte = m->s2; lobyte <= m->e2; lobyte++)
			{
				if (n >= sizeof(buf) - 3)
					break; // break, or an attempt anyway
				buf[n++] = hibyte;
				buf[n++] = lobyte;
			}
		}
	}
#endif
	/* above didn't work due to multiple overlapping ranges permitted.
	 * try this instead (lazy).. this is only used in DEBUGMODE
	 * via a command line option anyway:
	 */
	for (i=0; i <= 255; i++)
	{
		for (j=0; j <= 255; j++)
		{
			if (isvalidmbyte(i, j))
			{
				if (n >= sizeof(buf) - 3)
					break; // break, or an attempt anyway
				buf[n++] = i;
				buf[n++] = j;
			}
		}
	}

	buf[n] = '\0'; /* there's always room for a NUL */

	return buf;
}

char *charsys_group(int v)
{
	if (v & LANGAV_LATIN_UTF8)
		return "Latin script";
	if (v & LANGAV_CYRILLIC_UTF8)
		return "Cyrillic script";
	if (v & LANGAV_GREEK_UTF8)
		return "Greek script";
	if (v & LANGAV_HEBREW_UTF8)
		return "Hebrew script";
	if (v & LANGAV_ARABIC_UTF8)
		return "Arabic script";

	return "Other";
}

void charsys_dump_table(char *filter)
{
	int i = 0;

	for (i = 0; langlist[i].directive; i++)
	{
		char *charset = langlist[i].directive;

		if (!match_simple(filter, charset))
			continue; /* skip */

		charsys_reset();
		charsys_add_language(charset);
		charsys_finish();
		printf("%s;%s;%s\n", charset, charsys_group(langlist[i].setflags), charsys_displaychars());
	}
}

/** Get current languages (the 'langsinuse' variable) */
char *_charsys_get_current_languages(void)
{
	return langsinuse;
}
