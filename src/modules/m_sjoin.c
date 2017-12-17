/*
 *   IRC - Internet Relay Chat, src/modules/m_sjoin.c
 *   (C) 2004 The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

CMD_FUNC(m_sjoin);

#define MSG_SJOIN 	"SJOIN"	

ModuleHeader MOD_HEADER(m_sjoin)
  = {
	"m_sjoin",
	"4.0",
	"command /sjoin", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_sjoin)
{
	CommandAdd(modinfo->handle, MSG_SJOIN, m_sjoin, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_sjoin)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_sjoin)
{
	return MOD_SUCCESS;
}

typedef struct xParv aParv;
struct xParv {
	int  parc;
	char *parv[256];
};

aParv pparv;

aParv *mp2parv(char *xmbuf, char *parmbuf)
{
	int  c;
	char *p, *s;

	pparv.parv[0] = xmbuf;
	c = 1;
	
	for (s = strtoken(&p, parmbuf, " "); s; s = strtoken(&p, NULL, " "))
	{
		pparv.parv[c] = s;
		c++; /* in my dreams */
	}
	pparv.parv[c] = NULL;
	pparv.parc = c;
	return (&pparv);
}

/*
 * m_sjoin
 * Synchronize channel modes, +beI lists and users (server-to-server command)
 *
 *  parv[1] = channel timestamp
 *  parv[2] = channel name
 *
 *  if parc == 3:
 *  parv[3] = nick names + modes - all in one parameter
 *
 *  if parc == 4:
 *  parv[3] = channel modes
 *  parv[4] = nick names + modes - all in one parameter
 *
 *  if parc > 4:
 *  parv[3] = channel modes
 *  parv[4 to parc - 2] = mode parameters
 *  parv[parc - 1] = nick names + modes
 */

/* Some ugly macros, but useful */
#define Addit(mode,param) if ((strlen(parabuf) + strlen(param) + 11 < MODEBUFLEN) && (b <= MAXMODEPARAMS)) { \
	if (*parabuf) \
		strcat(parabuf, " ");\
	strcat(parabuf, param);\
	modebuf[b++] = mode;\
	modebuf[b] = 0;\
}\
else {\
	sendto_server(cptr, 0, PROTO_SJOIN, ":%s MODE %s %s %s %lu", sptr->name, chptr->chname,\
		modebuf, parabuf, chptr->creationtime); \
	sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s", sptr->name, chptr->chname,\
		modebuf, parabuf);\
	strcpy(parabuf,param);\
	/* modebuf[0] should stay what it was ('+' or '-') */ \
	modebuf[1] = mode;\
	modebuf[2] = '\0';\
	b = 2;\
}
#define Addsingle(x) do { modebuf[b] = x; b++; modebuf[b] = '\0'; } while(0)
#define CheckStatus(x,y) do { if (modeflags & (y)) { Addit((x), acptr->name); } } while(0)

CMD_FUNC(m_sjoin)
{
	unsigned short nopara;
	unsigned short nomode; /**< An SJOIN without MODE? */
	unsigned short removeours; /**< Remove our modes */
	unsigned short removetheirs; /**< Remove their modes (or actually: do not ADD their modes, the MODE -... line will be sent later by the other side) */
	unsigned short merge;	/**< same timestamp: merge their & our modes */
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	char cbuf[1024];
	char buf[1024];
	char nick[1024]; /**< nick or ban/invex/exempt being processed */
	char prefix[16]; /**< prefix of nick for server to server traffic (eg: @) */
	char nick_buf[BUFSIZE]; /**< Buffer for server-to-server traffic which will be broadcasted to others (using nick names) */
	char uid_buf[BUFSIZE];  /**< Buffer for server-to-server traffic which will be broadcasted to others (servers supporting SID/UID) */
	char sj3_parabuf[BUFSIZE]; /**< Prefix for the above SJOIN buffers (":xxx SJOIN #channel +mode :") */
	char *s = NULL;
	aChannel *chptr; /**< Channel */
	aParv *ap;
	int pcount, i;
	Hook *h;
	time_t ts, oldts;
	unsigned short b=0, c;
	char *t, *bp, *tp, *p, *saved = NULL;
	long modeflags;
	char queue_s=0, queue_c=0; /* oh this is soooooo ugly :p */
	
	if (IsClient(sptr) || parc < 4 || !IsServer(sptr))
		return 0;

	if (!IsChannelName(parv[2]))
		return 0;

	merge = nopara = nomode = removeours = removetheirs = 0;

	if (SupportSJOIN(cptr) && !SupportSJ3(cptr) && !strncmp(parv[4], "<none>", 6))
		nopara = 1;

	if (SupportSJOIN2(cptr) && !SupportSJ3(cptr) && !strncmp(parv[4], "<->", 6))
		nopara = 1;

	if (SupportSJ3(cptr) && (parc < 6))
		nopara = 1;

	if (SupportSJ3(cptr))
	{
		if (parc < 5)
			nomode = 1;
	}
	else
	{
		if (parv[3][0] && (parv[3][1] == '\0'))
			nomode = 1;
	}

	chptr = get_channel(cptr, parv[2], CREATE);

	ts = (time_t)atol(parv[1]);

	if (chptr->creationtime > ts)
	{
		removeours = 1;
		oldts = chptr->creationtime;
		chptr->creationtime = ts;
	}
	else if ((chptr->creationtime < ts) && (chptr->creationtime != 0))
	{
		removetheirs = 1;
	}
	else if (chptr->creationtime == ts)
	{
		merge = 1;
	}

	if (chptr->creationtime == 0)
	{
		oldts = -1;
		chptr->creationtime = ts;
	}
	else
	{
		oldts = chptr->creationtime;
	}

	if (ts < 750000)
	{
		if (ts != 0)
			sendto_ops
			    ("Warning! Possible desynch: SJOIN for channel %s has a fishy timestamp (%ld) [%s/%s]",
			    chptr->chname, ts, sptr->name, cptr->name);
	}

	parabuf[0] = '\0';
	modebuf[0] = '+';
	modebuf[1] = '\0';

	/* Grab current modes -> modebuf & parabuf */
	channel_modes(cptr, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), chptr);

	/* Do we need to remove all our modes, bans/exempt/inves lists and -vhoaq our users? */
	if (removeours)
	{
		Member *lp;
		Membership *lp2;

		modebuf[0] = '-';

		/* remove our modes if any */
		if (modebuf[1] != '\0')
		{

			ap = mp2parv(modebuf, parabuf);
			set_mode(chptr, cptr, ap->parc, ap->parv, &pcount,
			    pvar, 0);
			sendto_server(cptr, 0, PROTO_SJOIN,
			    ":%s MODE %s %s %s %lu",
			    sptr->name, chptr->chname, modebuf, parabuf,
			    chptr->creationtime);
			sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
			    sptr->name, chptr->chname, modebuf, parabuf);

		}
		/* remove bans */
		/* reset the buffers */
		modebuf[0] = '-';
		modebuf[1] = '\0';
		parabuf[0] = '\0';
		b = 1;
		while(chptr->banlist)
		{
			Ban *ban = chptr->banlist;
			Addit('b', ban->banstr);
			chptr->banlist = ban->next;
			MyFree(ban->banstr);
			MyFree(ban->who);
			free_ban(ban);
		}
		while(chptr->exlist)
		{
			Ban *ban = chptr->exlist;
			Addit('e', ban->banstr);
			chptr->exlist = ban->next;
			MyFree(ban->banstr);
			MyFree(ban->who);
			free_ban(ban);
		}
		while(chptr->invexlist)
		{
			Ban *ban = chptr->invexlist;
			Addit('I', ban->banstr);
			chptr->invexlist = ban->next;
			MyFree(ban->banstr);
			MyFree(ban->who);
			free_ban(ban);
		}
		for (lp = chptr->members; lp; lp = lp->next)
		{
			lp2 = find_membership_link(lp->cptr->user->channel, chptr);
			if (!lp2)
			{
				sendto_realops("Oops! chptr->members && !find_membership_link");
				continue;
			}
			if (lp->flags & MODE_CHANOWNER)
			{
				lp->flags &= ~MODE_CHANOWNER;
				Addit('q', lp->cptr->name);
			}
			if (lp->flags & MODE_CHANPROT)
			{
				lp->flags &= ~MODE_CHANPROT;
				Addit('a', lp->cptr->name);
			}
			if (lp->flags & MODE_CHANOP)
			{
				lp->flags &= ~MODE_CHANOP;
				Addit('o', lp->cptr->name);
			}
			if (lp->flags & MODE_HALFOP)
			{
				lp->flags &= ~MODE_HALFOP;
				Addit('h', lp->cptr->name);
			}
			if (lp->flags & MODE_VOICE)
			{
				lp->flags &= ~MODE_VOICE;
				Addit('v', lp->cptr->name);
			}
			/* Those should always match anyways  */
			lp2->flags = lp->flags;
		}
		if (b > 1)
		{
			modebuf[b] = '\0';
			sendto_server(cptr, 0, PROTO_SJOIN,
			    ":%s MODE %s %s %s %lu",
			    sptr->name, chptr->chname,
			    modebuf, parabuf, chptr->creationtime);
			sendto_channel_butserv(chptr,
			    sptr, ":%s MODE %s %s %s",
			    sptr->name, chptr->chname, modebuf, parabuf);

		}

		/* since we're dropping our modes, we want to clear the mlock as well. --nenolod */
		set_channel_mlock(cptr, sptr, chptr, NULL, FALSE);
	}
	/* Mode setting done :), now for our beloved clients */
	parabuf[0] = 0;
	modebuf[0] = '+';
	modebuf[1] = '\0';
	t = parv[parc - 1];
	b = 1;
	c = 0;
	bp = buf;
	strlcpy(cbuf, parv[parc-1], sizeof cbuf);

	sj3_parabuf[0] = '\0';
	for (i = 2; i <= (parc - 2); i++)
	{
		if (!parv[i])
		{
			sendto_ops("Got null parv in SJ3 code");
			continue;
		}
		strlcat(sj3_parabuf, parv[i], sizeof sj3_parabuf);
		if (((i + 1) <= (parc - 2)))
			strlcat(sj3_parabuf, " ", sizeof sj3_parabuf);
	}

	/* Now process adding of users & adding of list modes (bans/exempt/invex) */

	snprintf(nick_buf, sizeof nick_buf, ":%s SJOIN %ld %s :", sptr->name, ts, sj3_parabuf);
	snprintf(uid_buf, sizeof uid_buf, ":%s SJOIN %ld %s :", ID(sptr), ts, sj3_parabuf);

	for (s = strtoken(&saved, cbuf, " "); s; s = strtoken(&saved, NULL, " "))
	{
		c = 0;
		modeflags = 0;
		i = 0;
		tp = s;
		while (
		    (*tp == '@') || (*tp == '+') || (*tp == '%')
		    || (*tp == '*') || (*tp == '~') || (*tp == '&')
		    || (*tp == '"') || (*tp == '\''))
		{
			switch (*(tp++))
			{
			  case '@':
				  modeflags |= CHFL_CHANOP;
				  break;
			  case '%':
				  modeflags |= CHFL_HALFOP;
				  break;
			  case '+':
				  modeflags |= CHFL_VOICE;
				  break;
			  case '*':
				  modeflags |= CHFL_CHANOWNER;
				  break;
			  case '~':
				  modeflags |= CHFL_CHANPROT;
				  break;
			  case '&':
				  modeflags = CHFL_BAN;
				  goto getnick;
			  case '"':
				  modeflags = CHFL_EXCEPT;
				  goto getnick;
			  case '\'':
				  modeflags = CHFL_INVEX;
				  goto getnick;
			}
		}
getnick:

		/* First, set the appropriate prefix for server to server traffic.
		 * Note that 'prefix' is a 16 byte buffer but it's safe due to the limited
		 * number of choices as can be seen below:
		 */
		*prefix = '\0';
		p = prefix;
		if (modeflags == CHFL_INVEX)
			*p++ = '\'';
		else if (modeflags == CHFL_EXCEPT)
			*p++ = '\"';
		else if (modeflags == CHFL_BAN)
			*p++ = '&';
		else
		{
			/* multiple options possible at the same time */
			if (modeflags & CHFL_CHANOWNER)
				*p++ = '*';
			if (modeflags & CHFL_CHANPROT)
				*p++ = '~';
			if (modeflags & CHFL_CHANOP)
				*p++ = '@';
			if (modeflags & CHFL_HALFOP)
				*p++ = '%';
			if (modeflags & CHFL_VOICE)
				*p++ = '+';
		}
		*p = '\0';

		/* Now copy the "nick" (which can actually be a ban/invex/exempt).
		 * There's no size checking here but nick is 1024 bytes and we
		 * have 512 bytes input max.
		 */
		i = 0;
		while ((*tp != ' ') && (*tp != '\0'))
			nick[i++] = *(tp++);	/* get nick */
		nick[i] = '\0';
		if (nick[0] == ' ')
			continue;
		if (nick[0] == '\0')
			continue;
		Debug((DEBUG_DEBUG, "Got nick: %s", nick));
		if (!(modeflags & CHFL_BAN) && !(modeflags & CHFL_EXCEPT) && !(modeflags & CHFL_INVEX))
		{
			aClient *acptr;

			/* A person joining */

			/* The user may no longer exist. This can happen in case of a
			 * SVSKILL traveling in the other direction. Nothing to worry about.
			 */
			if (!(acptr = find_person(nick, NULL)))
				continue;

			if (acptr->from != sptr->from)
			{
				if (IsMember(acptr, chptr))
				{
					/* Nick collision, don't kick or it desynchs -Griever*/
					continue;
				}
			
				sendto_one(sptr,
				    ":%s KICK %s %s :Fake direction",
				    me.name, chptr->chname,
				    acptr->name);
				sendto_realops
				    ("Fake direction from user %s in SJOIN from %s(%s) at %s",
				    nick, sptr->srvptr->name,
				    sptr->name, chptr->chname);
				continue;
			}

			if (removetheirs)
			{
				modeflags = 0;
			}

			if (!IsMember(acptr, chptr))
			{
				add_user_to_channel(chptr, acptr, modeflags);
				RunHook4(HOOKTYPE_REMOTE_JOIN, cptr, acptr, chptr, NULL);
				send_join_to_local_users(acptr, chptr);
			}

			sendto_server(cptr, 0, PROTO_SJOIN, ":%s JOIN %s", acptr->name, chptr->chname);

			CheckStatus('q', CHFL_CHANOWNER);
			CheckStatus('a', CHFL_CHANPROT);
			CheckStatus('o', CHFL_CHANOP);
			CheckStatus('h', CHFL_HALFOP);
			CheckStatus('v', CHFL_VOICE);

			if (strlen(nick_buf) + strlen(prefix) + strlen(acptr->name) > BUFSIZE - 10)
			{
				/* Send what we have and start a new buffer */
				sendto_server(cptr, PROTO_SJOIN | PROTO_SJ3, PROTO_SID, "%s", nick_buf);
				snprintf(nick_buf, sizeof(nick_buf), ":%s SJOIN %ld %s :", sptr->name, ts, sj3_parabuf);
				/* Double-check the new buffer is sufficient to concat the data */
				if (strlen(nick_buf) + strlen(prefix) + strlen(acptr->name) > BUFSIZE - 5)
				{
					ircd_log(LOG_ERROR, "Oversized SJOIN: '%s' + '%s%s'",
						nick_buf, prefix, acptr->name);
					sendto_realops("Oversized SJOIN for %s -- see ircd log", chptr->chname);
					continue;
				}
			}
			sprintf(nick_buf+strlen(nick_buf), "%s%s ", prefix, acptr->name);

			if (strlen(uid_buf) + strlen(prefix) + IDLEN > BUFSIZE - 10)
			{
				/* Send what we have and start a new buffer */
				sendto_server(cptr, PROTO_SJOIN | PROTO_SJ3 | PROTO_SID, 0, "%s", uid_buf);
				snprintf(uid_buf, sizeof(uid_buf), ":%s SJOIN %ld %s :", ID(sptr), ts, sj3_parabuf);
				/* Double-check the new buffer is sufficient to concat the data */
				if (strlen(uid_buf) + strlen(prefix) + strlen(ID(acptr)) > BUFSIZE - 5)
				{
					ircd_log(LOG_ERROR, "Oversized SJOIN: '%s' + '%s%s'",
						uid_buf, prefix, ID(acptr));
					sendto_realops("Oversized SJOIN for %s -- see ircd log", chptr->chname);
					continue;
				}
			}
			sprintf(uid_buf+strlen(uid_buf), "%s%s ", prefix, ID(acptr));
		}
		else
		{
			if (removetheirs)
				continue;

			/* For list modes (beI): validate the syntax */
			if (modeflags & (CHFL_BAN|CHFL_EXCEPT|CHFL_INVEX))
			{
				char *str;
				
				/* non-extbans: prevent bans without ! or @. a good case of "should never happen". */
				if ((nick[0] != '~') && (!strchr(nick, '!') || !strchr(nick, '@') || (nick[0] == '!')))
					continue;
				
				str = clean_ban_mask(nick, MODE_ADD, sptr);
				if (!str)
					continue; /* invalid ban syntax */
				strlcpy(nick, str, sizeof(nick));
			}
			
			/* Adding of list modes */
			if (modeflags & CHFL_BAN)
			{
				if (add_listmode(&chptr->banlist, sptr, chptr, nick) != -1)
				{
					Addit('b', nick);
				}
			}
			if (modeflags & CHFL_EXCEPT)
			{
				if (add_listmode(&chptr->exlist, sptr, chptr, nick) != -1)
				{
					Addit('e', nick);
				}
			}
			if (modeflags & CHFL_INVEX)
			{
				if (add_listmode(&chptr->invexlist, sptr, chptr, nick) != -1)
				{
					Addit('I', nick);
				}
			}

			if (strlen(nick_buf) + strlen(prefix) + strlen(nick) > BUFSIZE - 10)
			{
				/* Send what we have and start a new buffer */
				sendto_server(cptr, PROTO_SJOIN | PROTO_SJ3, PROTO_SID, "%s", nick_buf);
				snprintf(nick_buf, sizeof(nick_buf), ":%s SJOIN %ld %s :", sptr->name, ts, sj3_parabuf);
				/* Double-check the new buffer is sufficient to concat the data */
				if (strlen(nick_buf) + strlen(prefix) + strlen(nick) > BUFSIZE - 5)
				{
					ircd_log(LOG_ERROR, "Oversized SJOIN: '%s' + '%s%s'",
						nick_buf, prefix, nick);
					sendto_realops("Oversized SJOIN for %s -- see ircd log", chptr->chname);
					continue;
				}
			}
			sprintf(nick_buf+strlen(nick_buf), "%s%s ", prefix, nick);

			if (strlen(uid_buf) + strlen(prefix) + strlen(nick) > BUFSIZE - 10)
			{
				/* Send what we have and start a new buffer */
				sendto_server(cptr, PROTO_SJOIN | PROTO_SJ3 | PROTO_SID, 0, "%s", uid_buf);
				snprintf(uid_buf, sizeof(uid_buf), ":%s SJOIN %ld %s :", ID(sptr), ts, sj3_parabuf);
				/* Double-check the new buffer is sufficient to concat the data */
				if (strlen(uid_buf) + strlen(prefix) + strlen(nick) > BUFSIZE - 5)
				{
					ircd_log(LOG_ERROR, "Oversized SJOIN: '%s' + '%s%s'",
						uid_buf, prefix, nick);
					sendto_realops("Oversized SJOIN for %s -- see ircd log", chptr->chname);
					continue;
				}
			}
			sprintf(uid_buf+strlen(uid_buf), "%s%s ", prefix, nick);
		}
		continue;
	}

	/* Send out any possible remainder.. */
	Debug((DEBUG_DEBUG, "Sending '%li %s :%s' to sj3", ts, parabuf, parv[parc - 1]));
	sendto_server(cptr, PROTO_SJOIN | PROTO_SJ3, PROTO_SID, "%s", nick_buf);
	sendto_server(cptr, PROTO_SID | PROTO_SJOIN | PROTO_SJ3, 0, "%s", uid_buf);

	if (modebuf[1])
	{
		modebuf[b] = '\0';
		sendto_server(cptr, 0, PROTO_SJOIN,
		    ":%s MODE %s %s %s %lu",
		    sptr->name, chptr->chname, modebuf, parabuf,
		    chptr->creationtime);
		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
		    sptr->name, chptr->chname, modebuf, parabuf);
	}
	
	if (!merge && !removetheirs && !nomode)
	{
		char paraback[1024];

		strlcpy(modebuf, parv[3], sizeof modebuf);
		parabuf[0] = '\0';
		if (!nopara)
		{
			for (b = 4; b <= (parc - 2); b++)
			{
				strlcat(parabuf, parv[b], sizeof parabuf);
				strlcat(parabuf, " ", sizeof parabuf);
			}
		}
		strlcpy(paraback, parabuf, sizeof paraback);
		ap = mp2parv(modebuf, parabuf);

		set_mode(chptr, cptr, ap->parc, ap->parv, &pcount, pvar, 0);

		sendto_server(cptr, 0, PROTO_SJOIN,
		    ":%s MODE %s %s %s %lu",
		    sptr->name, chptr->chname, modebuf, paraback,
		    chptr->creationtime);

		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
		    sptr->name, chptr->chname, modebuf, parabuf);
	}

	if (merge && !nomode)
	{
		aCtab *acp;
		Mode oldmode; /**< The old mode (OUR mode) */

		/* Copy current mode to oldmode (need to duplicate all extended mode params too..) */
		bcopy(&chptr->mode, &oldmode, sizeof(Mode));
		memset(&oldmode.extmodeparams, 0, sizeof(oldmode.extmodeparams));
		extcmode_duplicate_paramlist(chptr->mode.extmodeparams, oldmode.extmodeparams);

		/* Now merge the modes */
		strlcpy(modebuf, parv[3], sizeof modebuf);
		parabuf[0] = '\0';
		if (!nopara)
		{
			for (b = 4; b <= (parc - 2); b++)
			{
				strlcat(parabuf, parv[b], sizeof parabuf);
				strlcat(parabuf, " ", sizeof parabuf);
			}
		}
		ap = mp2parv(modebuf, parabuf);
		set_mode(chptr, cptr, ap->parc, ap->parv, &pcount, pvar, 0);

		/* Good, now we got modes, now for the differencing and outputting of modes
		 * We first see if any para modes are set.
		 */
		strlcpy(modebuf, "-", sizeof modebuf);
		parabuf[0] = '\0';
		b = 1;
		/* however, is this really going to happen at all? may be unneeded */
		if (oldmode.limit && !chptr->mode.limit)
		{
			Addsingle('l');
		}
		if (oldmode.key[0] && !chptr->mode.key[0])
		{
			Addit('k', oldmode.key);
		}

		/* First, check if we have something they don't have..
		 * note that: oldmode.* = us, chptr->mode.* = them.
		 */
		for (i=0; i <= Channelmode_highest; i++)
		{
			if ((Channelmode_Table[i].flag) &&
			    (oldmode.extmode & Channelmode_Table[i].mode) &&
			    !(chptr->mode.extmode & Channelmode_Table[i].mode))
			{
				if (Channelmode_Table[i].paracount)
				{
					char *parax = cm_getparameter_ex(oldmode.extmodeparams, Channelmode_Table[i].flag);
					//char *parax = Channelmode_Table[i].get_param(extcmode_get_struct(oldmode.extmodeparam, Channelmode_Table[i].flag));
					Addit(Channelmode_Table[i].flag, parax);
				} else {
					Addsingle(Channelmode_Table[i].flag);
				}
			}
		}

		/* Check if we had +s and it became +p, then revert it... */
		if ((oldmode.mode & MODE_SECRET) && (chptr->mode.mode & MODE_PRIVATE))
		{
			/* stay +s ! */
			chptr->mode.mode &= ~MODE_PRIVATE;
			chptr->mode.mode |= MODE_SECRET;
			Addsingle('p'); /* - */
			queue_s = 1;
		}
		/* Add single char modes... */
		for (acp = cFlagTab; acp->mode; acp++)
		{
			if ((oldmode.mode & acp->mode) && !(chptr->mode.mode & acp->mode) && !acp->parameters)
			{
				Addsingle(acp->flag);
			}
		}
		if (b > 1)
		{
			Addsingle('+');
		}
		else
		{
			strlcpy(modebuf, "+", sizeof modebuf);
			b = 1;
		}

		if (queue_s)
			Addsingle('s');

		if (queue_c)
			Addsingle('c');

		for (acp = cFlagTab; acp->mode; acp++)
		{
			if (!(oldmode.mode & acp->mode) && (chptr->mode.mode & acp->mode) && !acp->parameters)
			{
				Addsingle(acp->flag);
			}
		}

		/* Now, check if they have something we don't have..
		 * note that: oldmode.* = us, chptr->mode.* = them.
		 */
		for (i=0; i <= Channelmode_highest; i++)
		{
			if ((Channelmode_Table[i].flag) &&
			    !(oldmode.extmode & Channelmode_Table[i].mode) &&
			    (chptr->mode.extmode & Channelmode_Table[i].mode))
			{
				if (Channelmode_Table[i].paracount)
				{
					char *parax = cm_getparameter(chptr, Channelmode_Table[i].flag);
					if (parax)
						Addit(Channelmode_Table[i].flag, parax);
				} else {
					Addsingle(Channelmode_Table[i].flag);
				}
			}
		}

		/* now, if we had diffent para modes - this loop really could be done better, but */

		/* +l (limit) difference? */
		if (oldmode.limit && chptr->mode.limit && (oldmode.limit != chptr->mode.limit))
		{
			chptr->mode.limit = MAX(oldmode.limit, chptr->mode.limit);
			if (oldmode.limit != chptr->mode.limit)
			{
				Addit('l', my_itoa(chptr->mode.limit));
			}
		}

		/* +k (key) difference? */
		if (oldmode.key[0] && chptr->mode.key[0] && strcmp(oldmode.key, chptr->mode.key))
		{
			if (strcmp(oldmode.key, chptr->mode.key) > 0)			
			{
				strlcpy(chptr->mode.key, oldmode.key, sizeof chptr->mode.key);
			}
			else
			{
				Addit('k', chptr->mode.key);
			}
		}

		/* Now, check for any param differences in extended channel modes..
		 * note that: oldmode.* = us, chptr->mode.* = them.
		 * if we win: copy oldmode to chptr mode, if they win: send the mode
		 */
		for (i=0; i <= Channelmode_highest; i++)
		{
			if (Channelmode_Table[i].flag && Channelmode_Table[i].paracount &&
			    (oldmode.extmode & Channelmode_Table[i].mode) &&
			    (chptr->mode.extmode & Channelmode_Table[i].mode))
			{
				int r;
				char *parax;
				char flag = Channelmode_Table[i].flag;
				void *ourm = GETPARASTRUCTEX(oldmode.extmodeparams, flag);
				void *theirm = GETPARASTRUCT(chptr, flag);
				//CmodeParam *ourm = extcmode_get_struct(oldmode.extmodeparam,Channelmode_Table[i].flag);
				//CmodeParam *theirm = extcmode_get_struct(chptr->mode.extmodeparam, Channelmode_Table[i].flag);
				
				r = Channelmode_Table[i].sjoin_check(chptr, ourm, theirm);
				switch (r)
				{
					case EXSJ_WEWON:
						parax = cm_getparameter_ex(oldmode.extmodeparams, flag); /* grab from old */
						cm_putparameter(chptr, flag, parax); /* put in new (won) */
						break;

					case EXSJ_THEYWON:
						parax = cm_getparameter(chptr, flag);
						Debug((DEBUG_DEBUG, "sjoin: they won: '%s'", parax));
						Addit(Channelmode_Table[i].flag, parax);
						break;

					case EXSJ_SAME:
						Debug((DEBUG_DEBUG, "sjoin: equal"));
						break;

					case EXSJ_MERGE:
						parax = cm_getparameter_ex(oldmode.extmodeparams, flag); /* grab from old */
						cm_putparameter(chptr, flag, parax); /* put in new (won) */
						Addit(flag, parax);
						break;

					default:
						ircd_log(LOG_ERROR, "channel.c:m_sjoin:param diff checker: got unk. retval 0x%x??", r);
						break;
				}
			}
		}

		Addsingle('\0');

		if (modebuf[1])
		{
			sendto_server(cptr, 0, PROTO_SJOIN,
			    ":%s MODE %s %s %s %lu",
			    sptr->name, chptr->chname, modebuf, parabuf,
			    chptr->creationtime);
			sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
			    sptr->name, chptr->chname, modebuf, parabuf);
		}

		/* free the oldmode.* crap :( */
		extcmode_free_paramlist(oldmode.extmodeparams);
		/* memset(&oldmode.extmodeparams, 0, sizeof(oldmode.extmodeparams)); -- redundant? */
	}

	for (h = Hooks[HOOKTYPE_CHANNEL_SYNCED]; h; h = h->next)
	{
		(*(h->func.voidfunc))(chptr,merge,removetheirs,nomode);
	}

	/* we should be synched by now, */
	if ((oldts != -1) && (oldts != chptr->creationtime))
	{
		sendto_channel_butserv(chptr, &me,
			":%s NOTICE %s :*** TS for %s changed from %ld to %ld",
			me.name, chptr->chname, chptr->chname,
			oldts, chptr->creationtime);
	}

	/* If something went wrong with processing of the SJOIN above and
	 * the channel actually has no users in it at this point,
	 * then destroy the channel.
	 */
	if (!chptr->users)
	{
		sub1_from_channel(chptr);
		return -1;
	}

	return 0;
}
