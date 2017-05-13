/*
 * Text ban. (C) Copyright 2004-2016 Bram Matthys.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "unrealircd.h"

/** Max number of text bans per channel.
 * This is basically the most important setting. It directly affects
 * how much CPU you want to spend on text processing.
 * For comparison: with 10 textbans of max length (150), and messages said in
 * the channel with max length (~500 bytes), on an A1800+ (1.53GHz) machine
 * this consumes 30 usec per-channel message PEAK/MAX (usec = 1/1000000 of a
 * second), and in normal (non-supersize messages) count on 10-15 usec.
 * Basically this means this allows for like >25000 messages per second at
 * 100% CPU usage in a worth case scenario. Which seems by far sufficient to me.
 * Also note that (naturally) only local clients are processed, only people
 * that do not have halfops or higher, and only channels that have any
 * textbans set.
 * UPDATE: The speed impact for 15 bans per channel is 42 usec PEAK.
 * HINT: If you are hitting the "normal banlimit" before you actually hit this
 *       one, then you might want to tweak the #define MAXBANS and #define
 *       MAXBANLENGTH in include/struct.h. Doubling MAXBANLENGTH is usually
 *       a good idea, and then you can enlarge MAXBANS too a bit if you want to.
 */
#define MAX_EXTBANT_PER_CHAN     15 /* Max number of ~T bans in a channel. */

/** Max length of a ban.
 * NOTE: This is mainly for 'cosmetic' purposes. Lowering it does not
 *       decrease CPU usage for text processing.
 */
#define MAX_LENGTH               150 /* Max length of a ban */

/** Allow user@host in the textban? This changes the syntax! */
#undef UHOSTFEATURE

/** Enable 'censor' support. What this type will do is replace the
 * matched word with "<censored>" (or another word, see later)
 * Like:
 * <Idiot> hey check out my fucking new car
 * will become:
 * <Idiot> hey check out my <censored> new car
 *
 * SPEED: See README
 */
#define CENSORFEATURE

/** Which censor replace word to use when CENSORFEATURE is enabled. */
#define CENSORWORD "<censored>"

/** Benchmark mode.
 * Should never be used on production servers.
 * Mainly meant for debugging/profiling purposes for myself, but if you
 * have a test server and are curious about the speed of this module,
 * then you can enable it of course ;).
 */
#undef BENCHMARK


ModuleHeader MOD_HEADER(textban)
  = {
	"textban",
	"v2.2",
	"ExtBan ~T (textban) by Syzop",
	"3.2-b8-1",
	NULL 
    };

/* Forward declarations */
char *extban_modeT_conv_param(char *para_in);
int extban_modeT_is_banned(aClient *sptr, aChannel *chptr, char *ban, int type);
int extban_modeT_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what, int what2);
char *textban_chanmsg(aClient *, aChannel *, char *, int);

MOD_INIT(textban)
{
	ExtbanInfo req;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(ExtbanInfo));
	req.flag = 'T';
	req.conv_param = extban_modeT_conv_param;
	req.is_banned = extban_modeT_is_banned;
	req.is_ok = extban_modeT_is_ok;
	
	if (!ExtbanAdd(modinfo->handle, req))
	{
		config_error("textban module: adding extban ~T failed! module NOT loaded");
		return MOD_FAILED;
	}

	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_CHANMSG, 0, textban_chanmsg);

	return MOD_SUCCESS;
}

MOD_LOAD(textban)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(textban)
{
	return MOD_SUCCESS;
}

#if defined(CENSORFEATURE) || defined(STRIPFEATURE)
static char *my_strcasestr(char *haystack, char *needle) {
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

#define TEXTBAN_WORD_LEFT	0x1
#define TEXTBAN_WORD_RIGHT	0x2
#define TEXTBAN_WORD_STRIP	0x4

/* textban_replace:
 * a fast replace routine written by Syzop used for replacing.
 * searches in line for huntw and replaces it with replacew,
 * buf is used for the result and max is sizeof(buf).
 * (Internal assumptions: size of 'buf' is 512 characters or more)
 */
int textban_replace(int type, char *badword, char *line, char *buf)
{
char *replacew;
char *pold = line, *pnew = buf; /* Pointers to old string and new string */
char *poldx = line;
int replacen;
int searchn = -1;
char *startw, *endw;
char *c_eol = buf + 510 - 1; /* Cached end of (new) line */
int cleaned = 0;

	if (type & TEXTBAN_WORD_STRIP)
	{
		replacew = "";
		replacen = 0;
	} else {
		replacew = CENSORWORD;
		replacen = sizeof(CENSORWORD)-1;
	}

	while (1) {
		pold = my_strcasestr(pold, badword);
		if (!pold)
			break;
		if (searchn == -1)
			searchn = strlen(badword);
		/* Hunt for start of word */
 		if (pold > line) {
			for (startw = pold; (!iswseperator(*startw) && (startw != line)); startw--);
			if (iswseperator(*startw))
				startw++; /* Don't point at the space/seperator but at the word! */
		} else {
			startw = pold;
		}

		if (!(type & TEXTBAN_WORD_LEFT) && (pold != startw)) {
			/* not matched */
			pold++;
			continue;
		}

		/* Hunt for end of word */
		for (endw = pold; ((*endw != '\0') && (!iswseperator(*endw))); endw++);

		if (!(type & TEXTBAN_WORD_RIGHT) && (pold+searchn != endw)) {
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
#endif

unsigned int counttextbans(aChannel *chptr)
{
Ban *ban;
unsigned int cnt = 0;

	for (ban = chptr->banlist; ban; ban=ban->next)
		if ((ban->banstr[0] == '~') && (ban->banstr[1] == 'T') && (ban->banstr[2] == ':'))
			cnt++;
	for (ban = chptr->exlist; ban; ban=ban->next)
		if ((ban->banstr[0] == '~') && (ban->banstr[1] == 'T') && (ban->banstr[2] == ':'))
			cnt++;
	return cnt;
}


int extban_modeT_is_ok(aClient *sptr, aChannel *chptr, char *para, int checkt, int what, int what2)
{
int n;

	if ((what == MODE_ADD) && (what2 == EXBTYPE_EXCEPT) && MyClient(sptr))
		return 0; /* except is not supported */

	/* We check the # of bans in the channel, may not exceed MAX_EXTBANT_PER_CHAN */
	if ((what == MODE_ADD) && (checkt == EXBCHK_PARAM) &&
	     MyClient(sptr) && !IsOper(sptr) &&
	    ((n = counttextbans(chptr)) >= MAX_EXTBANT_PER_CHAN))
	{
		/* We check the # of bans in the channel, may not exceed MAX_EXTBANT_PER_CHAN */
		sendto_one(sptr, err_str(ERR_BANLISTFULL), me.name, sptr->name, chptr->chname, para);
		sendnotice(sptr, "Too many textbans for this channel");
		return 0;
	}
	return 1;
}

/** Ban callbacks */
char *extban_modeT_conv_param(char *para_in)
{
static char retbuf[MAX_LENGTH+1];
char para[MAX_LENGTH+1], *action, *text, *p;
#ifdef UHOSTFEATURE
char *uhost;
int ap = 0;
#endif

	strlcpy(para, para_in+3, sizeof(para)); /* work on a copy (and truncate it) */

	/* ~T:<action>:<text>
	 * ~T:user@host:<action>:<text> if UHOSTFEATURE is enabled
	 */

#ifdef UHOSTFEATURE
	action = strchr(para, ':');
	if (!action)
		return NULL;
	*action++ = '\0';
	if (!*action)
		return NULL;
	text = strchr(action, ':');
	if (!text || !text[1])
		return NULL;
	*text++ = '\0';
	uhost = para;
	
	for (p = uhost; *p; p++)
	{
		if (*p == '@')
			ap++;
		else if ((*p <= ' ') || (*p > 128))
			return NULL; /* cannot be in a username/host */
	}
	if (ap != 1)
		return NULL; /* no @ */
#else
	text = strchr(para, ':');
	if (!text)
		return NULL;
	*text++ = '\0';
	/* para=action, text=text */
	if (!*text)
		return NULL; /* empty text */
	action = para;
#endif
	
	/* ~T:<action>:<text> */
	if (!strcasecmp(action, "block"))
		action = "block"; /* ok */
#ifdef CENSORFEATURE
	else if (!strcasecmp(action, "censor"))
	{
		char *p;
		action = "censor";
		for (p = text; *p; p++)
			if ((*p == '*') && !(p == text) && !(p[1] == '\0'))
				return NULL; /* can only be *word, word* or *word* or word */
		if (!strcmp(p, "*") || !strcmp(p, "**"))
			return NULL; /* cannot match everything ;p */
	}
#endif
	else
		return NULL; /* unknown action */

	/* check the string.. */
	for (p=text; *p; p++)
		if ((*p == '\003') || (*p == '\002') || 
		    (*p == '\037') || (*p == '\026') ||
		    (*p == ' '))
			return NULL; /* codes not permitted, would be confusing since they are stripped */

	/* Rebuild the string.. */
#ifdef UHOSTFEATURE
	snprintf(retbuf, sizeof(retbuf), "~T:%s:%s:%s", uhost, action, text); /* can be cut off if too long */
#else
	snprintf(retbuf, sizeof(retbuf), "~T:%s:%s", action, text); /* can be cut off if too long */
#endif
	return retbuf;
}

int extban_modeT_is_banned(aClient *sptr, aChannel *chptr, char *ban, int type)
{
	/* Never banned here */
	return 0;
}

#ifdef CENSORFEATURE
void parse_word(const char *s, char **word, int *type)
{
static char buf[512];
const char *tmp;
int len;
int tpe = 0;
char *o = buf;

	for (tmp = s; *tmp; tmp++)
	{
		if (*tmp != '*')
			*o++ = *tmp;
		else
		{
			if (s == tmp)
				tpe |= TEXTBAN_WORD_LEFT;
			if (*(tmp + 1) == '\0')
				tpe |= TEXTBAN_WORD_RIGHT;
		}
	}
	*o = '\0';
	
	*word = buf;
	*type = tpe;
}
#endif

/* Channel message callback */
char *textban_chanmsg(aClient *sptr, aChannel *chptr, char *text, int notice)
{
static char filtered[512]; /* temp buffer */
Ban *ban;
long fl;
int done=0, cleaned=0;
char *p;
#ifdef UHOSTFEATURE
char buf[512], uhost[USERLEN + HOSTLEN + 16];
#endif
char tmp[1024], *word;
int type;
#ifdef BENCHMARK
struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	if (!MyClient(sptr))
		return text; /* Remote and servers are not affected */

	fl = get_access(sptr, chptr);
	if (fl & (CHFL_HALFOP|CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER))
		return text; /* halfop or higher */

	filtered[0] = '\0'; /* NOT needed, but... :P */

#ifdef UHOSTFEATURE
	ircsprintf(uhost, "%s@%s", sptr->user->username, GetHost(sptr));
#endif

	for (ban = chptr->banlist; ban; ban=ban->next)
	{
		if ((ban->banstr[0] == '~') && (ban->banstr[1] == 'T') && (ban->banstr[2] == ':'))
		{
			if (!done)
			{
				/* Prepare the text [done here, to avoid useless CPU time] */
				strlcpy(filtered, StripControlCodes(text), sizeof(filtered));
				done = 1;
			}
			p = ban->banstr + 3;
#ifdef UHOSTFEATURE
			/* First.. deal with userhost... */
			strcpy(buf, p);
			p = strchr(buf, ':');
			if (!p) continue;
			*p++ = '\0';
			if (!_match(buf, uhost))
#else
			if (1)
#endif
			{
				if (!strncasecmp(p, "block:", 6))
				{
					if (!_match(p+6, filtered))
					{
						sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
							me.name, sptr->name, chptr->chname,
							"Message blocked due to a text ban", chptr->chname);
						return NULL;
					}
				}
#ifdef CENSORFEATURE
				else if (!strncasecmp(p, "censor:", 7))
				{
					parse_word(p+7, &word, &type);
					if (textban_replace(type, word, filtered, tmp))
					{
						strcpy(filtered, tmp);
						cleaned = 1;
					}
				}
#endif
			}
		}
	}

#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	ircd_log(LOG_ERROR, "TextBan Timing: %ld microseconds (%s / %s / %d)",
		((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec),
		sptr->name, chptr->chname, strlen(text));
#endif

	if (cleaned)
	{
		/* check for null string */
		char *p;
		for (p = filtered; *p; p++)
			if (*p != ' ')
				return filtered;
		return NULL; /* nothing but spaces found.. */
	}
	else
		return text;
}
