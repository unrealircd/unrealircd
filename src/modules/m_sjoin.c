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
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_sjoin(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SJOIN 	"SJOIN"	
#define TOK_SJOIN 	"~"	

ModuleHeader MOD_HEADER(m_sjoin)
  = {
	"m_sjoin",
	"$Id$",
	"command /sjoin", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_sjoin)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_SJOIN, TOK_SJOIN, m_sjoin, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_sjoin)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_sjoin)(int module_unload)
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
	
	for (s = (char *)strtoken(&p, parmbuf, " "); s;
		s = (char *)strtoken(&p, NULL, " "))
	{
		pparv.parv[c] = s;
		c++; /* in my dreams */
	}
	pparv.parv[c] = NULL;
	pparv.parc = c;
	return (&pparv);
}

/* Checks if 2 ChanFloodProt modes (chmode +f) are different.
 * This is a bit more complicated than 1 simple memcmp(a,b,..) because
 * counters are also stored in this struct so we have to do
 * it manually :( -- Syzop.
 */
static int compare_floodprot_modes(ChanFloodProt *a, ChanFloodProt *b)
{
	if (memcmp(a->l, b->l, sizeof(a->l)) ||
	    memcmp(a->a, b->a, sizeof(a->a)) ||
	    memcmp(a->r, b->r, sizeof(a->r)))
		return 1;
	else
		return 0;
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
   **      parv[0] = sender prefix
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
	sendto_serv_butone_sjoin(cptr, ":%s MODE %s %s %s %lu", sptr->name, chptr->chname,\
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
#define CheckStatus(x,y) do { if (modeflags & (y)) { Addit((x), nick); } } while(0)
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
	char *s = NULL;
	aClient *acptr;
	aChannel *chptr;
	Member *lp;
	Membership *lp2;
	aParv *ap;
	int pcount, i, f;
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

	if (*parv[1] != '!')
		ts = (time_t)atol(parv[1]);
	else
		ts = (time_t)base64dec(parv[1] + 1);

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
	channel_modes(cptr, modebuf, parabuf, chptr);
	if (removeours)
	{
		modebuf[0] = '-';
		/* remove our modes if any */
		if (modebuf[1] != '\0')
		{

			ap = mp2parv(modebuf, parabuf);
			set_mode(chptr, cptr, ap->parc, ap->parv, &pcount,
			    pvar, 0);
			sendto_serv_butone_sjoin(cptr,
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
			sendto_serv_butone_sjoin(cptr,
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
	for (s = s0 = strtoken(&p, cbuf, " "); s; s = s0 = strtoken(&p, (char *)NULL, " "))
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
				  modeflags |= CHFL_BAN;				
				  goto getnick;
				  break;
			  case '"':
				  modeflags |= CHFL_EXCEPT;
				  goto getnick;
				  break;
			  case '\'':
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
			if (IsMember(acptr, chptr)) {
#if 0
				int i;
				sendto_realops("[BUG] Duplicate user entry in SJOIN! Please report at http://bugs.unrealircd.org !!! Chan='%s', User='%s', modeflags=%ld",
					chptr->chname ? chptr->chname : "<NULL>", acptr->name ? acptr->name : "<NULL>", modeflags);
				ircd_log(LOG_ERROR, "[BUG] Duplicate user entry in SJOIN! Please report to UnrealIrcd team!! Chan='%s', User='%s', modeflags=%ld",
					chptr->chname ? chptr->chname : "<NULL>", acptr->name ? acptr->name : "<NULL>", modeflags);
				ircd_log(LOG_ERROR, "--- Dump of parameters ---");
				for (i=0; i < parc; i++)
					ircd_log(LOG_ERROR, "parv[%d] = '%s'", i, BadPtr(parv[i]) ? "<NULL-or-empty>" : parv[i]);
				ircd_log(LOG_ERROR, "--- End of dump ---");
#endif
			} else {
				add_user_to_channel(chptr, acptr, modeflags);
				RunHook4(HOOKTYPE_REMOTE_JOIN, cptr, acptr, chptr, NULL);
				if (chptr->mode.mode & MODE_AUDITORIUM)
				{
					if (modeflags & (CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER))
						sendto_channel_butserv(chptr, acptr, ":%s JOIN :%s", nick, chptr->chname);
					else
						sendto_chanops_butone(NULL, chptr, ":%s!%s@%s JOIN :%s",
							acptr->name, acptr->user->username, GetHost(acptr), chptr->chname);
				} else
					sendto_channel_butserv(chptr, acptr, ":%s JOIN :%s", nick, chptr->chname);

				if (chptr->mode.floodprot && sptr->serv->flags.synced && !IsULine(sptr))
				        do_chanflood(chptr->mode.floodprot, FLD_JOIN);
			}
			sendto_serv_butone_sjoin(cptr, ":%s JOIN %s",
			    nick, chptr->chname);
			CheckStatus('q', CHFL_CHANOWNER);
			CheckStatus('a', CHFL_CHANPROT);
			CheckStatus('o', CHFL_CHANOP);
			CheckStatus('h', CHFL_HALFOP);
			CheckStatus('v', CHFL_VOICE);
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
		}
docontinue:
		continue;
	}

	if (modebuf[1])
	{
		modebuf[b] = '\0';
		sendto_serv_butone_sjoin(cptr,
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
		sendto_serv_butone_sjoin(cptr,
		    ":%s MODE %s %s %s %lu",
		    sptr->name, chptr->chname, modebuf, paraback,
		    chptr->creationtime);
		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
		    sptr->name, chptr->chname, modebuf, paraback);
		if (chptr->mode.mode & MODE_ONLYSECURE)
			kick_insecure_users(chptr);
	}
	if (merge && !nomode)
	{
		aCtab *acp;
		bcopy(&chptr->mode, &oldmode, sizeof(Mode));

		/* Fun.. we have to duplicate all extended modes too... */
		oldmode.extmodeparam = NULL;
		oldmode.extmodeparam = extcmode_duplicate_paramlist(chptr->mode.extmodeparam);

		if (chptr->mode.floodprot)
		{
			oldmode.floodprot = MyMalloc(sizeof(ChanFloodProt));
			memcpy(oldmode.floodprot, chptr->mode.floodprot, sizeof(ChanFloodProt));
		}

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
		if (oldmode.link[0] && !chptr->mode.link[0])
		{
			Addit('L', oldmode.link);
		}
		if (oldmode.floodprot && !chptr->mode.floodprot)
		{
			char *x = channel_modef_string(oldmode.floodprot);
			Addit('f', x);
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
					char *parax = Channelmode_Table[i].get_param(extcmode_get_struct(oldmode.extmodeparam, Channelmode_Table[i].flag));
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
		/* Check if we had +c and it became +S, then revert it... */
		if ((oldmode.mode & MODE_NOCOLOR) && (chptr->mode.mode & MODE_STRIP))
		{
			chptr->mode.mode &= ~MODE_STRIP;
			chptr->mode.mode |= MODE_NOCOLOR;
			Addsingle('S'); /* - */
			queue_c = 1;
		}
		if (!(oldmode.mode & MODE_ONLYSECURE) && (chptr->mode.mode & MODE_ONLYSECURE))
			kick_insecure_users(chptr);
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
		/* first we check if it has been set, we did unset longer up */
		if (chptr->mode.floodprot && !oldmode.floodprot)
		{
			char *x = channel_modef_string(chptr->mode.floodprot);
			Addit('f', x);
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
					char *parax = Channelmode_Table[i].get_param(extcmode_get_struct(chptr->mode.extmodeparam,Channelmode_Table[i].flag));
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
				Addit('l', (char *)my_itoa(chptr->mode.limit));
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
		/* same as above (except case insensitive #test == #TEST -- codemastr) */
		if (oldmode.link[0] && chptr->mode.link[0]
		    && stricmp(oldmode.link, chptr->mode.link))
		{
			if (strcmp(oldmode.link, chptr->mode.link) > 0)
			{
				strlcpy(chptr->mode.link, oldmode.link, sizeof(chptr->mode.link));
			}
			else
			{
				Addit('L', chptr->mode.link);
			}
		}
		/* 
		 * run a max on each?
		 */
		if (chptr->mode.floodprot && oldmode.floodprot)
		{
			char *x;
			int i;

			if (compare_floodprot_modes(chptr->mode.floodprot, oldmode.floodprot))
			{
				chptr->mode.floodprot->per = MAX(chptr->mode.floodprot->per, oldmode.floodprot->per);
				for (i=0; i < NUMFLD; i++)
				{
					chptr->mode.floodprot->l[i] = MAX(chptr->mode.floodprot->l[i], oldmode.floodprot->l[i]);
					chptr->mode.floodprot->a[i] = MAX(chptr->mode.floodprot->a[i], oldmode.floodprot->a[i]);
					chptr->mode.floodprot->r[i] = MAX(chptr->mode.floodprot->r[i], oldmode.floodprot->r[i]);
				}
				x = channel_modef_string(chptr->mode.floodprot);
				Addit('f', x);
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
				CmodeParam *ourm = extcmode_get_struct(oldmode.extmodeparam,Channelmode_Table[i].flag);
				CmodeParam *theirm = extcmode_get_struct(chptr->mode.extmodeparam, Channelmode_Table[i].flag);
				
				r = Channelmode_Table[i].sjoin_check(chptr, ourm, theirm);
				switch (r)
				{
					case EXSJ_WEWON:
					{
						CmodeParam *r;
						parax = Channelmode_Table[i].get_param(ourm);
						Debug((DEBUG_DEBUG, "sjoin: we won: '%s'", parax));
						r = Channelmode_Table[i].put_param(theirm, parax);
						if (r != theirm) /* confusing eh ;) */
							AddListItem(r, chptr->mode.extmodeparam);
						break;
					}
					case EXSJ_THEYWON:
						parax = Channelmode_Table[i].get_param(theirm);
						Debug((DEBUG_DEBUG, "sjoin: they won: '%s'", parax));
						Addit(Channelmode_Table[i].flag, parax);
						break;
					case EXSJ_SAME:
						Debug((DEBUG_DEBUG, "sjoin: equal"));
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
			sendto_serv_butone_sjoin(cptr,
			    ":%s MODE %s %s %s %lu",
			    sptr->name, chptr->chname, modebuf, parabuf,
			    chptr->creationtime);
			sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
			    sptr->name, chptr->chname, modebuf, parabuf);
		}

		/* free the oldmode.* crap :( */
		extcmode_free_paramlist(oldmode.extmodeparam);
		oldmode.extmodeparam = NULL; /* just to be sure ;) */

		/* and the oldmode.floodprot struct too... :/ */
		if (oldmode.floodprot)
		{
			free(oldmode.floodprot);
			oldmode.floodprot = NULL;
		}
	}

	/* we should be synched by now, */
	if (oldts != -1)
		if (oldts != chptr->creationtime)
			sendto_channel_butserv(chptr, &me,
			    ":%s NOTICE %s :*** Notice -- TS for %s changed from %ld to %ld",
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
	Debug((DEBUG_DEBUG, "Sending '%li %s :%s' to sj3-!sjb64", ts, parabuf,
	    parv[parc - 1]));
	sendto_serv_butone_token_opt(cptr, OPT_SJOIN | OPT_SJ3 | OPT_NOT_SJB64, sptr->name,
	    MSG_SJOIN, TOK_SJOIN, "%li %s :%s", ts, parabuf, parv[parc - 1]);
	Debug((DEBUG_DEBUG, "Sending '%B %s :%s' to sj3-sjb64", (long)ts, parabuf,
	    parv[parc - 1]));
	sendto_serv_butone_token_opt(cptr, OPT_SJOIN | OPT_SJ3 | OPT_SJB64, sptr->name,
	    MSG_SJOIN, TOK_SJOIN, "%B %s :%s", (long)ts, parabuf, parv[parc - 1]);
	 
	return 0;
}
