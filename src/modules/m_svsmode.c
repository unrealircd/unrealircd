/*
 *   IRC - Internet Relay Chat, src/modules/m_svsmode.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   SVSMODE and SVS2MODE commands
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

DLLFUNC int m_svsmode(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int m_svs2mode(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SVSMODE 	"SVSMODE"	
#define TOK_SVSMODE 	"n"	
#define MSG_SVS2MODE    "SVS2MODE"
#define TOK_SVS2MODE	"v"

ModuleHeader MOD_HEADER(m_svsmode)
  = {
	"m_svsmode",
	"$Id$",
	"command /svsmode and svs2mode", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_svsmode)(ModuleInfo *modinfo)
{
	add_Command(MSG_SVSMODE, TOK_SVSMODE, m_svsmode, MAXPARA);
	add_Command(MSG_SVS2MODE, TOK_SVS2MODE, m_svs2mode, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_svsmode)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_svsmode)(int module_unload)
{
	if (del_Command(MSG_SVSMODE, TOK_SVSMODE, m_svsmode) < 0 || del_Command(MSG_SVS2MODE, TOK_SVS2MODE, m_svs2mode) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_svsmode).name);
	}
	return MOD_SUCCESS;
}

extern void add_send_mode_param(aChannel *chptr, aClient *from, char what, char mode, char *param);
int channel_svsmode(aClient *cptr, aClient *sptr, int parc, char *parv[]) 
{
	aChannel *chptr;
	time_t ts;
	aClient *acptr;
	char *m;
	int what;
	
	int i = 4;

	*parabuf = '\0';
	modebuf[0] = 0;
	if(!(chptr = find_channel(parv[1], NULL)))
		return 0;
/*	if (parc >= 4) {
			return 0;
		if (parc > 4) {
			ts = TS2ts(parv[4]);
			if (acptr->since != ts)
				return 0;
		}
	}*/
	ts = TS2ts(parv[parc-1]);
	for(m = parv[2]; *m; m++) {
		switch (*m) {
			case '+':
				what = MODE_ADD;
				break;
			case '-':
				what = MODE_DEL;
				break;
			case 'q': {
				Member *cm;
				for (cm = chptr->members; cm; cm = cm->next) {
					if (cm->flags & CHFL_CHANOWNER) {
						Membership *mb;
						mb = find_membership_link(cm->cptr->user->channel,
							chptr);
						add_send_mode_param(chptr, sptr, '-', 'q', cm->cptr->name);
						cm->flags &= ~CHFL_CHANOWNER;
						if (mb)
							mb->flags = cm->flags;
					}
				}
			}
			break;
			case 'a': {
				Member *cm;
				for (cm = chptr->members; cm; cm = cm->next) {
					if (cm->flags & CHFL_CHANPROT) {
						Membership *mb;
						mb = find_membership_link(cm->cptr->user->channel,
							chptr);
						add_send_mode_param(chptr, sptr, '-', 'a', cm->cptr->name);
						cm->flags &= ~CHFL_CHANPROT;
						if (mb)
							mb->flags = cm->flags;
					}
				}
			}
			break;
			case 'o': {
				Member *cm;
				for (cm = chptr->members; cm; cm = cm->next) {
					if (cm->flags & CHFL_CHANOP) {
						Membership *mb;
						mb = find_membership_link(cm->cptr->user->channel,
							chptr);
						add_send_mode_param(chptr, sptr, '-', 'o', cm->cptr->name);
						cm->flags &= ~CHFL_CHANOP;
						if (mb)
							mb->flags = cm->flags;
					}
				}
			}
			break;
			case 'h': {
				Member *cm;
				for (cm = chptr->members; cm; cm = cm->next) {
					if (cm->flags & CHFL_HALFOP) {
						Membership *mb;
						mb = find_membership_link(cm->cptr->user->channel,
							chptr);
						add_send_mode_param(chptr, sptr, '-', 'h', cm->cptr->name);
						cm->flags &= ~CHFL_HALFOP;
						if (mb)
							mb->flags = cm->flags;
					}
				}
			}
			break;
			case 'v': {
				Member *cm;
				for (cm = chptr->members; cm; cm = cm->next) {
					if (cm->flags & CHFL_VOICE) {
						Membership *mb;
						mb = find_membership_link(cm->cptr->user->channel,
							chptr);
						add_send_mode_param(chptr, sptr, '-', 'v', cm->cptr->name);
						cm->flags &= ~CHFL_VOICE;
						if (mb)
							mb->flags = cm->flags;
					}
				}
			}
			break;
			case 'b': {
				Ban *ban, *bnext;
				if (parc >= i) {
					char uhost[NICKLEN+USERLEN+HOSTLEN+6], vhost[NICKLEN+USERLEN+HOSTLEN+6];
					char ihost[NICKLEN+USERLEN+HOSTLEN+6];
					if (!(acptr = find_person(parv[i-1], NULL))) {
						i++;
						break;
					}
					if (ts && ts != acptr->since) {
						i++;
						break;
					}
					i++;

					strlcpy(uhost, make_nick_user_host(acptr->name, 
						acptr->user->username, acptr->user->realhost),
						sizeof uhost);
					strlcpy(vhost, make_nick_user_host(acptr->name,
						acptr->user->username, GetHost(acptr)),
						sizeof vhost);
					strlcpy(ihost, make_nick_user_host(acptr->name,
						acptr->user->username, GetIP(acptr)),
						sizeof ihost);
					ban = chptr->banlist;
					while (ban) {
						bnext = ban->next;
						if (!match(ban->banstr, uhost) || !match(ban->banstr, vhost) || !match(ban->banstr, ihost)) {
							add_send_mode_param(chptr, sptr, '-',  'b', 
								ban->banstr);
							del_banid(chptr, ban->banstr);
						}
						ban = bnext;
					}
				}
				else {
					ban = chptr->banlist;
					while (ban) {
						bnext = ban->next;
						add_send_mode_param(chptr, sptr, '-',  'b', ban->banstr);
						del_banid(chptr, ban->banstr);
						ban = bnext;
					}
				}
			}
			break;
			case 'e': {
				Ban *ban, *bnext;
				if (parc >= i) {
					char uhost[NICKLEN+USERLEN+HOSTLEN+6], vhost[NICKLEN+USERLEN+HOSTLEN+6];
					char ihost[NICKLEN+USERLEN+HOSTLEN+6];
					if (!(acptr = find_person(parv[i-1], NULL))) {
						i++;
						break;
					}
					if (ts && ts != acptr->since) {
						i++;
						break;
					}
					i++;

					strlcpy(uhost, make_nick_user_host(acptr->name, 
						acptr->user->username, acptr->user->realhost),
						sizeof uhost);
					strlcpy(vhost, make_nick_user_host(acptr->name,
						acptr->user->username, GetHost(acptr)),
						sizeof vhost);
					strlcpy(ihost, make_nick_user_host(acptr->name,
						acptr->user->username, GetIP(acptr)),
						sizeof ihost);

					ban = chptr->exlist;
					while (ban) {
						bnext = ban->next;
						if (!match(ban->banstr, uhost) || !match(ban->banstr, vhost) || !match(ban->banstr, ihost)) {
							add_send_mode_param(chptr, sptr, '-',  'e', 
								ban->banstr);
							del_exbanid(chptr, ban->banstr);
						}
						ban = bnext;
					}
				}
				else {
					ban = chptr->exlist;
					while (ban) {
						bnext = ban->next;
						add_send_mode_param(chptr, sptr, '-',  'e', ban->banstr);
						del_exbanid(chptr, ban->banstr);
						ban = bnext;
					}
				}
			}
			break;
		}
	}
	if (*parabuf) {
		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s", sptr->name, chptr->chname, 
			modebuf, parabuf);
		sendto_serv_butone(NULL, ":%s MODE %s %s %s", sptr->name, chptr->chname, modebuf, parabuf);
		*parabuf = 0;
	}
	return 0;
}

/*
 * do_svsmode() [merge from svsmode/svs2mode]
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 *
 * show_change can be 0 (for svsmode) or 1 (for svs2mode).
 */
int  do_svsmode(aClient *cptr, aClient *sptr, int parc, char *parv[], int show_change)
{
int i;
char *m;
aClient *acptr;
int  what, setflags;
char *xmsg = show_change ? MSG_SVS2MODE : MSG_SVSMODE;
char *xtok = show_change ? TOK_SVS2MODE : TOK_SVSMODE;

	if (!IsULine(sptr))
		return 0;

	what = MODE_ADD;

	if (parc < 3)
		return 0;

	if (parv[1][0] == '#') 
		return channel_svsmode(cptr, sptr, parc, parv);

	if (!(acptr = find_person(parv[1], NULL)))
		return 0;

	setflags = 0;
	if (show_change) /* only used if show_change is set */
	{
		for (i = 0; i <= Usermode_highest; i++)
		if (Usermode_Table[i].flag && (acptr->umodes & Usermode_Table[i].mode))
			setflags |= Usermode_Table[i].mode;
	}

	/* parse mode change string(s) */
	for (m = parv[2]; *m; m++)
		switch (*m)
		{
			case '+':
				what = MODE_ADD;
				break;
			case '-':
				what = MODE_DEL;
				break;

			/* we may not get these, but they shouldnt be in default */
			case ' ':
			case '\n':
			case '\r':
			case '\t':
				break;
			case 'i':
				if ((what == MODE_ADD) && !(acptr->umodes & UMODE_INVISIBLE))
					IRCstats.invisible++;
				if ((what == MODE_DEL) && (acptr->umodes & UMODE_INVISIBLE))
					IRCstats.invisible--;
				goto setmodex;
			case 'O': /* Locops are opers too! */
				if (what == MODE_ADD)
				{
#ifndef NO_FDLIST
					if (!IsAnOper(acptr) && MyClient(acptr))
						addto_fdlist(acptr->slot, &oper_fdlist);
#endif
					acptr->umodes &= ~UMODE_OPER;
				}
#ifndef NO_FDLIST
				if (what == MODE_DEL && (acptr->umodes & UMODE_LOCOP) && MyClient(acptr))
					delfrom_fdlist(acptr->slot, &oper_fdlist);
#endif
				goto setmodex;					
			case 'o':
				if ((what == MODE_ADD) && !(acptr->umodes & UMODE_OPER))
				{
#ifndef NO_FDLIST
					if (MyClient(acptr) && !IsLocOp(acptr))
						addto_fdlist(acptr->slot, &oper_fdlist);
#endif
					acptr->umodes &= ~UMODE_LOCOP; /* can't be both local and global */
					IRCstats.operators++;
				}
				if ((what == MODE_DEL) && (acptr->umodes & UMODE_OPER))
				{
					if (acptr->umodes & UMODE_HIDEOPER)
					{
						/* clear 'H' too, and opercount stays the same.. */
						acptr->umodes &= ~UMODE_HIDEOPER;
					} else {
						IRCstats.operators--;
					}
#ifndef NO_FDLIST
					if (MyClient(acptr))
						delfrom_fdlist(acptr->slot, &oper_fdlist);
#endif
				}
				goto setmodex;
			case 'H':
				if (what == MODE_ADD && !(acptr->umodes & UMODE_HIDEOPER))
				{
					if (!IsAnOper(acptr) && !strchr(parv[2], 'o')) /* (ofcoz this strchr() is flawed) */
					{
						/* isn't an oper, and would not become one either.. abort! */
						sendto_realops(
							"[BUG] server %s tried to set +H while user not an oper, para=%s/%s, "
							"umodes=%ld, please fix your services or if you think it's our fault, "
							"report at http://bugs.unrealircd.org/", sptr->name, parv[1], parv[2], acptr->umodes);
						break; /* abort! */
					}
					if (!IsLocOp(acptr))
						IRCstats.operators--;
				}
				if (what == MODE_DEL && (acptr->umodes & UMODE_HIDEOPER) && !IsLocOp(acptr))
					IRCstats.operators++;
				goto setmodex;
			case 'd':
				if (parv[3] && isdigit(*parv[3]))
				{
					acptr->user->servicestamp = strtoul(parv[3], NULL, 10);
					break;
				}
			default:
				setmodex:
				for (i = 0; i <= Usermode_highest; i++)
				{
					if (!Usermode_Table[i].flag)
						continue;
					if (*m == Usermode_Table[i].flag)
					{
						if (what == MODE_ADD)
							acptr->umodes |= Usermode_Table[i].mode;
						else
							acptr->umodes &= ~Usermode_Table[i].mode;
						break;
					}
				}
				break;
		} /*switch*/

	if (parc > 3)
		sendto_serv_butone_token(cptr, parv[0], xmsg, xtok,
			"%s %s %s", parv[1], parv[2], parv[3]);
	else
		sendto_serv_butone_token(cptr, parv[0], xmsg, xtok,
			"%s %s", parv[1], parv[2]);

	if (show_change)
	{
		char buf[BUFSIZE];
		send_umode(NULL, acptr, setflags, ALL_UMODES, buf);
		if (MyClient(acptr) && buf[0] && buf[1])
			sendto_one(acptr, ":%s MODE %s :%s", parv[0], parv[1], buf);
	}

	VERIFY_OPERCOUNT(acptr, "svsmodeX");
	return 0;
}

/*
 * m_svsmode() added by taz
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 */
int  m_svsmode(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	return do_svsmode(cptr, sptr, parc, parv, 0);
}

/*
 * m_svs2mode() added by Potvin
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 */
int  m_svs2mode(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	return do_svsmode(cptr, sptr, parc, parv, 1);
}
