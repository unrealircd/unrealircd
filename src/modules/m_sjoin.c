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
   **      m_sjoin  
   **
   **   SJOIN will synch channels and channelmodes using the new STS1 protocol
   **      that is based on the EFnet TS3 protocol.
   **                           -GZ (gz@starchat.net)
   **         
   **  Modified for UnrealIRCd by Stskeeps
   **  Recoded by Stskeeps
   **      parv[1]	aChannel *chptr;
	aClient *cptr;
	int  parc;
	u_int *pcount;
	char bounce;
	char *parv[], pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
 = channel timestamp
   **      parv[2] = channel name
   ** 	  
   **      if (parc == 3) 
   **		parv[3] = nick names + modes - all in one parameter
   **	   if (parc == 4)
   **		parv[3] = channel modes
   **		parv[4] = nick names + modes - all in one parameter
   **	   if (parc > 4)
   **		parv[3] = channel modes
   **		parv[4 to parc - 2] = mode parameters
   **		parv[parc - 1] = nick names + modes
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
#define AddBan(x) do { strlcat(banbuf, x, sizeof banbuf); strlcat(banbuf, " ", sizeof banbuf); } while(0)
#define AddEx(x) do { strlcat(exbuf, x, sizeof exbuf); strlcat(exbuf, " ", sizeof exbuf); } while(0)
#define AddInvex(x) do { strlcat(invexbuf, x, sizeof invexbuf); strlcat(invexbuf, " ", sizeof invexbuf); } while(0)

CMD_FUNC(m_sjoin)
{
	unsigned short nopara;
	unsigned short nomode;
	unsigned short removeours;
	unsigned short removetheirs;
	unsigned short merge;	/* same timestamp */
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	char paraback[1024];
	char banbuf[1024];
	char exbuf[1024];
	char invexbuf[1024];
	char cbuf[1024];
	char buf[1024];
	char nick[1024];
	char nick_buf[BUFSIZE];
	char uid_buf[BUFSIZE];
	char sj3_parabuf[BUFSIZE];
	size_t buflen;
	char *s = NULL;
	char *nptr, *uptr;
	aClient *acptr;
	aChannel *chptr;
	Member *lp;
	Membership *lp2;
	aParv *ap;
	int pcount, i, f, k =0;
	Hook* h;
	time_t ts, oldts;
	unsigned short b=0,c;
	Mode oldmode;
	char *t, *bp, *tp, *p = NULL;
	 char *s0 = NULL;
	long modeflags;
	Ban *ban=NULL;
	char queue_s=0, queue_c=0; /* oh this is soooooo ugly :p */
	
	if (IsClient(sptr) || parc < 3 || !IsServer(sptr))
		return 0;

	if (!IsChannelName(parv[2]))
		return 0;

	merge = nopara = nomode = removeours = removetheirs = 0;

	if (SupportSJOIN(cptr) && !SupportSJ3(cptr) &&
	    !strncmp(parv[4], "<none>", 6))
		nopara = 1;
	if (SupportSJOIN2(cptr) && !SupportSJ3(cptr) &&
	    !strncmp(parv[4], "<->", 6))
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
		if (parv[3][1] == '\0')
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
		removetheirs = 1;
	else if (chptr->creationtime == ts)
		merge = 1;

	if (chptr->creationtime == 0)
	{
		oldts = -1;
		chptr->creationtime = ts;
	}
	else
		oldts = chptr->creationtime;

	if (ts < 750000)
		if (ts != 0)
			sendto_ops
			    ("Warning! Possible desynch: SJOIN for channel %s has a fishy timestamp (%ld) [%s/%s]",
			    chptr->chname, ts, sptr->name, cptr->name);
	parabuf[0] = '\0';
	modebuf[0] = '+';
	modebuf[1] = '\0';
	banbuf[0] = '\0';
	exbuf[0] = '\0';
	invexbuf[0] = '\0';
	channel_modes(cptr, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), chptr);
	if (removeours)
	{
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
			ban = chptr->banlist;
			Addit('b', ban->banstr);
			chptr->banlist = ban->next;
			MyFree(ban->banstr);
			MyFree(ban->who);
			free_ban(ban);
		}
		while(chptr->exlist)
		{
			ban = chptr->exlist;
			Addit('e', ban->banstr);
			chptr->exlist = ban->next;
			MyFree(ban->banstr);
			MyFree(ban->who);
			free_ban(ban);
		}
		while(chptr->invexlist)
		{
			ban = chptr->invexlist;
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
	f = 1;
	b = 1;
	c = 0;
	bp = buf;
	strlcpy(cbuf, parv[parc-1], sizeof cbuf);

	strlcpy(sj3_parabuf, "", sizeof sj3_parabuf);
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

	buflen = snprintf(nick_buf, sizeof nick_buf, ":%s SJOIN %ld %s :",
		sptr->name, ts, sj3_parabuf);
	nptr = nick_buf + buflen;

	buflen = snprintf(uid_buf, sizeof uid_buf, ":%s SJOIN %ld %s :",
		ID(sptr), ts, sj3_parabuf);
	uptr = uid_buf + buflen;

	for (s = s0 = strtoken(&p, cbuf, " "); s; s = s0 = strtoken(&p, NULL, " "))
	{
		c = f = 0;
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
				  *nptr++ = '@';
				  *uptr++ = '@';
				  modeflags |= CHFL_CHANOP;
				  break;
			  case '%':
				  *nptr++ = '%';
				  *uptr++ = '%';
				  modeflags |= CHFL_HALFOP;
				  break;
			  case '+':
				  *nptr++ = '+';
				  *uptr++ = '+';
				  modeflags |= CHFL_VOICE;
				  break;
			  case '*':
				  *nptr++ = '*';
				  *uptr++ = '*';
				  modeflags |= CHFL_CHANOWNER;
				  break;
			  case '~':
				  *nptr++ = '~';
				  *uptr++ = '~';
				  modeflags |= CHFL_CHANPROT;
				  break;
			  case '&':
				  *nptr++ = '&';
				  *uptr++ = '&';
				  modeflags |= CHFL_BAN;				
				  goto getnick;
				  break;
			  case '"':
				  *nptr++ = '"';
				  *uptr++ = '"';
				  modeflags |= CHFL_EXCEPT;
				  goto getnick;
				  break;
			  case '\'':
				  *nptr++ = '\'';
				  *uptr++ = '\'';
				  modeflags |= CHFL_INVEX;
				  goto getnick;
				  break;
			}
		}
	     getnick:
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
			if (!(acptr = find_person(nick, NULL)))
			{
				sendto_snomask
				    (SNO_JUNK, "Missing user %s in SJOIN for %s from %s (%s)",
				    nick, chptr->chname, sptr->name,
				    backupbuf);
				continue;
			}
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
			/* [old: temporarely added for tracing user-twice-in-channel bugs -- Syzop, 2003-01-24.]
			 * 2003-05-29: now traced this bug down: it's possible to get 2 joins if persons
			 * at 2 different servers kick a target on a 3rd server. This will require >3 servers
			 * most of the time but is also possible with only 3 if there's asynchronic lag.
			 * The general rule here (and at other places as well! see kick etc) is we ignore it
			 * locally (dont send a join to the chan) but propagate it to the other servers.
			 * I'm not sure if the propagation is needed however -- Syzop.
			 */
			for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
				{
					k = (*(h->func.intfunc))(sptr,chptr);
					if (k != 0)
						break;
				}


			if (!IsMember(acptr, chptr)) {
				add_user_to_channel(chptr, acptr, modeflags);
				RunHook4(HOOKTYPE_REMOTE_JOIN, cptr, acptr, chptr, NULL);
				if (k != 0)
				{
					if (modeflags & (CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER|CHFL_HALFOP|CHFL_VOICE))
						sendto_channel_butserv(chptr, acptr, ":%s JOIN :%s", nick, chptr->chname);
					else
						sendto_chanops_butone(NULL, chptr, ":%s!%s@%s JOIN :%s",
							acptr->name, acptr->user->username, GetHost(acptr), chptr->chname);
				} else
					sendto_channel_butserv(chptr, acptr, ":%s JOIN :%s", nick, chptr->chname);
			}
			sendto_server(cptr, 0, PROTO_SJOIN, ":%s JOIN %s",
			    nick, chptr->chname);
			CheckStatus('q', CHFL_CHANOWNER);
			CheckStatus('a', CHFL_CHANPROT);
			CheckStatus('o', CHFL_CHANOP);
			CheckStatus('h', CHFL_HALFOP);
			CheckStatus('v', CHFL_VOICE);

			if (((nptr - nick_buf) + NICKLEN + 10) > BUFSIZE)
			{
				sendto_server(cptr, PROTO_SJOIN | PROTO_SJ3, PROTO_SID, "%s", nick_buf);

				buflen = snprintf(nick_buf, sizeof nick_buf, ":%s SJOIN %ld %s :",
					sptr->name, ts, sj3_parabuf);
				nptr = nick_buf + buflen;
			}

			nptr += sprintf(nptr, "%s ", acptr->name);

			if (((uptr - uid_buf) + IDLEN + 10) > BUFSIZE)
			{
				sendto_server(cptr, PROTO_SJOIN | PROTO_SJ3 | PROTO_SID, 0, "%s", uid_buf);

				buflen = snprintf(uid_buf, sizeof uid_buf, ":%s SJOIN %ld %s :",
					ID(sptr), ts, sj3_parabuf);
				uptr = uid_buf + buflen;
			}

			uptr += sprintf(uptr, "%s ", ID(acptr));
		}
		else
		{
			if (removetheirs)
				continue;
			if (modeflags & CHFL_BAN)
			{
				f = add_listmode(&chptr->banlist, sptr, chptr, nick);
				if (f != -1)
				{
					Addit('b', nick);
					AddBan(nick);
				}
			}
			if (modeflags & CHFL_EXCEPT)
			{
				f = add_listmode(&chptr->exlist, sptr, chptr, nick);
				if (f != -1)
				{
					Addit('e', nick);
					AddEx(nick);
				}
			}
			if (modeflags & CHFL_INVEX)
			{

				f = add_listmode(&chptr->invexlist, sptr, chptr, nick);
				if (f != -1)
				{
					Addit('I', nick);
					AddInvex(nick);
				}
			}

			if (((nptr - nick_buf) + NICKLEN + 10) > BUFSIZE)
			{
				sendto_server(cptr, PROTO_SJOIN | PROTO_SJ3, PROTO_SID, "%s", nick_buf);

				buflen = snprintf(nick_buf, sizeof nick_buf, ":%s SJOIN %ld %s :",
					sptr->name, ts, sj3_parabuf);
				nptr = nick_buf + buflen;
			}

			nptr += sprintf(nptr, "%s ", nick);

			if (((uptr - uid_buf) + IDLEN + 10) > BUFSIZE)
			{
				sendto_server(cptr, PROTO_SJOIN | PROTO_SJ3 | PROTO_SID, 0, "%s", uid_buf);

				buflen = snprintf(uid_buf, sizeof uid_buf, ":%s SJOIN %ld %s :",
					ID(sptr), ts, sj3_parabuf);
				uptr = uid_buf + buflen;
			}

			uptr += sprintf(uptr, "%s ", nick);
		}
		continue;
	}

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
		strlcpy(modebuf, parv[3], sizeof modebuf);
		parabuf[0] = '\0';
		if (!nopara)
			for (b = 4; b <= (parc - 2); b++)
			{
				strlcat(parabuf, parv[b], sizeof parabuf);
				strlcat(parabuf, " ", sizeof parabuf);
			}
		strlcpy(paraback, parabuf, sizeof paraback);
		ap = mp2parv(modebuf, parabuf);
		set_mode(chptr, cptr, ap->parc, ap->parv, &pcount, pvar, 0);
		sendto_server(cptr, 0, PROTO_SJOIN,
		    ":%s MODE %s %s %s %lu",
		    sptr->name, chptr->chname, modebuf, paraback,
		    chptr->creationtime);
		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
		    sptr->name, chptr->chname, modebuf, paraback);
	}
	if (merge && !nomode)
	{
		aCtab *acp;
		bcopy(&chptr->mode, &oldmode, sizeof(Mode));

		/* Fun.. we have to duplicate all extended modes too... */
		memset(&oldmode.extmodeparams, 0, sizeof(oldmode.extmodeparams));
		extcmode_duplicate_paramlist(chptr->mode.extmodeparams, oldmode.extmodeparams);

		/* merge the modes */
		strlcpy(modebuf, parv[3], sizeof modebuf);
		parabuf[0] = '\0';
		if (!nopara)
			for (b = 4; b <= (parc - 2); b++)
			{
				strlcat(parabuf, parv[b], sizeof parabuf);
				strlcat(parabuf, " ", sizeof parabuf);
			}
		ap = mp2parv(modebuf, parabuf);
		set_mode(chptr, cptr, ap->parc, ap->parv, &pcount, pvar, 0);

		/* Good, now we got modes, now for the differencing and outputting of modes
		   We first see if any para modes are set

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
			if ((oldmode.mode & acp->mode) &&
			    !(chptr->mode.mode & acp->mode) && !acp->parameters)
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
			if (!(oldmode.mode & acp->mode) &&
			    (chptr->mode.mode & acp->mode) && !acp->parameters)
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
					//char *parax = Channelmode_Table[i].get_param(extcmode_get_struct(chptr->mode.extmodeparam,Channelmode_Table[i].flag));
					Addit(Channelmode_Table[i].flag, parax);
				} else {
					Addsingle(Channelmode_Table[i].flag);
				}
			}
		}

		/* now, if we had diffent para modes - this loop really could be done better, but */

		/* do we have an difference? */
		if (oldmode.limit && chptr->mode.limit
		    && (oldmode.limit != chptr->mode.limit))
		{
			chptr->mode.limit =
			    MAX(oldmode.limit, chptr->mode.limit);
			if (oldmode.limit != chptr->mode.limit)
			{
				Addit('l', my_itoa(chptr->mode.limit));
			}
		}
		/* sketch, longest key wins */
		if (oldmode.key[0] && chptr->mode.key[0]
		    && strcmp(oldmode.key, chptr->mode.key))
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
					{
					        parax = cm_getparameter_ex(oldmode.extmodeparams, flag); /* grab from old */
					        cm_putparameter(chptr, flag, parax); /* put in new (won) */
						break;
					}
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
	if (oldts != -1)
		if (oldts != chptr->creationtime)
			sendto_channel_butserv(chptr, &me,
			    ":%s NOTICE %s :*** TS for %s changed from %ld to %ld",
			    me.name, chptr->chname, chptr->chname,
			    oldts, chptr->creationtime);


	strlcpy(parabuf, "", sizeof parabuf);
	for (i = 2; i <= (parc - 2); i++)
	{
		if (!parv[i])
		{
			sendto_ops("Got null parv in SJ3 code");
			continue;
		}
		strlcat(parabuf, parv[i], sizeof parabuf);
		if (((i + 1) <= (parc - 2)))
			strlcat(parabuf, " ", sizeof parabuf);
	}
	if (!chptr->users)
	{
		sub1_from_channel(chptr);
		return -1;
	}
	/* This sends out to SJ3 servers .. */
	Debug((DEBUG_DEBUG, "Sending '%li %s :%s' to sj3", ts, parabuf,
	    parv[parc - 1]));
	sendto_server(cptr, PROTO_SJOIN | PROTO_SJ3, PROTO_SID,
	    "%s", nick_buf);
	sendto_server(cptr, PROTO_SID | PROTO_SJOIN | PROTO_SJ3, 0,
	    "%s", uid_buf);

	return 0;
}
