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

ModuleHeader MOD_HEADER
  = {
	"extbans/textban",
	"2.2",
	"ExtBan ~T (textban) by Syzop",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
const char *extban_modeT_conv_param(BanContext *b, Extban *extban);
int textban_check_ban(Client *client, Channel *channel, const char *ban, const char **msg, const char **errmsg);
int textban_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);
int extban_modeT_is_ok(BanContext *b);
void parse_word(const char *s, char **word, int *type);

MOD_INIT()
{
	ExtbanInfo req;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(ExtbanInfo));
	req.letter = 'T';
	req.name = "text";
	req.options = EXTBOPT_NOSTACKCHILD; /* disallow things like ~n:~T, as we only affect text. */
	req.conv_param = extban_modeT_conv_param;
	req.is_ok = extban_modeT_is_ok;

	if (!ExtbanAdd(modinfo->handle, req))
	{
		config_error("textban module: adding extban ~T failed! module NOT loaded");
		return MOD_FAILED;
	}

	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, textban_can_send_to_channel);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

#if defined(CENSORFEATURE) || defined(STRIPFEATURE)
static char *my_strcasestr(char *haystack, char *needle)
{
	int i;
	int nlength = strlen (needle);
	int hlength = strlen (haystack);

	if (nlength > hlength)
		return NULL;
	if (hlength <= 0)
		return NULL;
	if (nlength <= 0)
		return haystack;
	for (i = 0; i <= (hlength - nlength); i++)
	{
		if (strncasecmp (haystack + i, needle, nlength) == 0)
			return haystack + i;
	}
	return NULL; /* not found */
}

#define TEXTBAN_WORD_LEFT	0x1
#define TEXTBAN_WORD_RIGHT	0x2

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

	replacew = CENSORWORD;
	replacen = sizeof(CENSORWORD)-1;

	while (1)
	{
		pold = my_strcasestr(pold, badword);
		if (!pold)
			break;
		if (searchn == -1)
			searchn = strlen(badword);
		/* Hunt for start of word */
 		if (pold > line)
 		{
			for (startw = pold; (!iswseperator(*startw) && (startw != line)); startw--);
			if (iswseperator(*startw))
				startw++; /* Don't point at the space/seperator but at the word! */
		} else {
			startw = pold;
		}

		if (!(type & TEXTBAN_WORD_LEFT) && (pold != startw))
		{
			/* not matched */
			pold++;
			continue;
		}

		/* Hunt for end of word
		 * Fix for bug #4909: word will be at least 'searchn' long so we can skip
		 * 'searchn' bytes and avoid stopping half-way the badword.
		 */
		for (endw = pold+searchn; ((*endw != '\0') && (!iswseperator(*endw))); endw++);

		if (!(type & TEXTBAN_WORD_RIGHT) && (pold+searchn != endw))
		{
			/* not matched */
			pold++;
			continue;
		}

		cleaned = 1; /* still too soon? Syzop/20050227 */

		/* Do we have any not-copied-yet data? */
		if (poldx != startw)
		{
			int tmp_n = startw - poldx;
			if (pnew + tmp_n >= c_eol)
			{
				/* Partial copy and return... */
				memcpy(pnew, poldx, c_eol - pnew);
				*c_eol = '\0';
				return 1;
			}

			memcpy(pnew, poldx, tmp_n);
			pnew += tmp_n;
		}
		/* Now update the word in buf (pnew is now something like startw-in-new-buffer */

		if (replacen)
		{
			if ((pnew + replacen) >= c_eol)
			{
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
	if (*poldx)
	{
		strncpy(pnew, poldx, c_eol - pnew);
		*(c_eol) = '\0';
	} else {
		*pnew = '\0';
	}
	return cleaned;
}
#endif

unsigned int counttextbans(Channel *channel)
{
	Ban *ban;
	unsigned int cnt = 0;

	for (ban = channel->banlist; ban; ban=ban->next)
		if ((ban->banstr[0] == '~') && (ban->banstr[1] == 'T') && (ban->banstr[2] == ':'))
			cnt++;
	for (ban = channel->exlist; ban; ban=ban->next)
		if ((ban->banstr[0] == '~') && (ban->banstr[1] == 'T') && (ban->banstr[2] == ':'))
			cnt++;
	return cnt;
}


int extban_modeT_is_ok(BanContext *b)
{
	int n;

	if ((b->what == MODE_ADD) && (b->ban_type == EXBTYPE_EXCEPT) && MyUser(b->client))
		return 0; /* except is not supported */

	/* We check the # of bans in the channel, may not exceed MAX_EXTBANT_PER_CHAN */
	if ((b->what == MODE_ADD) && (b->is_ok_check == EXBCHK_PARAM) &&
	     MyUser(b->client) && !IsOper(b->client) &&
	    ((n = counttextbans(b->channel)) >= MAX_EXTBANT_PER_CHAN))
	{
		/* We check the # of bans in the channel, may not exceed MAX_EXTBANT_PER_CHAN */
		sendnumeric(b->client, ERR_BANLISTFULL, b->channel->name, b->banstr); // FIXME: wants b->full_banstr here
		sendnotice(b->client, "Too many textbans for this channel");
		return 0;
	}
	return 1;
}

char *conv_pattern_asterisks(const char *pattern)
{
	static char buf[512];
	char missing_prefix = 0, missing_suffix = 0;
	if (*pattern != '*')
		missing_prefix = 1;
	if (*pattern && (pattern[strlen(pattern)-1] != '*'))
		missing_suffix = 1;
	snprintf(buf, sizeof(buf), "%s%s%s",
		missing_prefix ? "*" : "",
		pattern,
		missing_suffix ? "*" : "");
	return buf;
}

/** Ban callbacks */
const char *extban_modeT_conv_param(BanContext *b, Extban *extban)
{
	static char retbuf[MAX_LENGTH+1];
	char para[MAX_LENGTH+1], *action, *text, *p;
#ifdef UHOSTFEATURE
	char *uhost;
	int ap = 0;
#endif

	strlcpy(para, b->banstr, sizeof(para)); /* work on a copy (and truncate it) */

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
	{
		action = "block"; /* ok */
		text = conv_pattern_asterisks(text);
	}
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
	{
		if ((*p == '\003') || (*p == '\002') || 
		    (*p == '\037') || (*p == '\026') ||
		    (*p == ' '))
		{
			return NULL; /* codes not permitted, would be confusing since they are stripped */
		}
	}

	/* Rebuild the string.. can be cut off if too long. */
#ifdef UHOSTFEATURE
	snprintf(retbuf, sizeof(retbuf), "%s:%s:%s", uhost, action, text);
#else
	snprintf(retbuf, sizeof(retbuf), "%s:%s", action, text);
#endif
	return retbuf;
}

/** Check for text bans (censor and block) */
int textban_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	Ban *ban;

	/* +h/+o/+a/+q users bypass textbans */
	if (check_channel_access(client, channel, "hoaq"))
		return HOOK_CONTINUE;

	/* IRCOps with these privileges bypass textbans too */
	if (op_can_override("channel:override:message:ban", client, channel, NULL))
		return HOOK_CONTINUE;

	/* Now we have to manually walk the banlist and check if things match */
	for (ban = channel->banlist; ban; ban=ban->next)
	{
		char *banstr = ban->banstr;

		/* Pretend time does not exist... */
		if (!strncmp(banstr, "~t:", 3))
		{
			banstr = strchr(banstr+3, ':');
			if (!banstr)
				continue;
			banstr++;
		}
		else if (!strncmp(banstr, "~time:", 6))
		{
			banstr = strchr(banstr+6, ':');
			if (!banstr)
				continue;
			banstr++;
		}

		if (!strncmp(banstr, "~T:", 3) || !strncmp(banstr, "~text:", 6))
		{
			/* text ban */
			if (textban_check_ban(client, channel, banstr, msg, errmsg))
				return HOOK_DENY;
		}
	}

	return HOOK_CONTINUE;
}


int textban_check_ban(Client *client, Channel *channel, const char *ban, const char **msg, const char **errmsg)
{
	static char retbuf[512];
	char filtered[512]; /* temp input buffer */
	long fl;
	int cleaned=0;
	const char *p;
#ifdef UHOSTFEATURE
	char buf[512], uhost[USERLEN + HOSTLEN + 16];
#endif
	char tmp[1024], *word;
	int type;

	/* We can only filter on non-NULL text of course */
	if ((msg == NULL) || (*msg == NULL))
		return 0;

	filtered[0] = '\0'; /* NOT needed, but... :P */

#ifdef UHOSTFEATURE
	ircsprintf(uhost, "%s@%s", client->user->username, GetHost(client));
#endif
	strlcpy(filtered, StripControlCodes(*msg), sizeof(filtered));

	p = strchr(ban, ':');
	if (!p)
		return 0; /* "impossible" */
	p++;
#ifdef UHOSTFEATURE
	/* First.. deal with userhost... */
	strcpy(buf, p);
	p = strchr(buf, ':');
	if (!p)
		return 0; /* invalid format */
	*p++ = '\0';

	if (match_simple(buf, uhost))
#else
	if (1)
#endif
	{
		if (!strncasecmp(p, "block:", 6))
		{
			if (match_simple(p+6, filtered))
			{
				if (errmsg)
					*errmsg = "Message blocked due to a text ban";
				return 1; /* BLOCK */
			}
		}
#ifdef CENSORFEATURE
		else if (!strncasecmp(p, "censor:", 7))
		{
			parse_word(p+7, &word, &type);
			if (textban_replace(type, word, filtered, tmp))
			{
				strlcpy(filtered, tmp, sizeof(filtered));
				cleaned = 1;
			}
		}
#endif
	}

	if (cleaned)
	{
		/* check for null string */
		char *p;
		for (p = filtered; *p; p++)
		{
			if (*p != ' ')
			{
				strlcpy(retbuf, filtered, sizeof(retbuf));
				*msg = retbuf;
				return 0; /* allow through, but filtered */
			}
		}
		return 1; /* nothing but spaces found.. */
	}
	return 0; /* nothing blocked */
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
