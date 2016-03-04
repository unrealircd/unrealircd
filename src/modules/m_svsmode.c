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

#include "unrealircd.h"

void add_send_mode_param(aChannel *chptr, aClient *from, char what, char mode, char *param);
CMD_FUNC(m_svsmode);
CMD_FUNC(m_svs2mode);

#define MSG_SVSMODE 	"SVSMODE"	
#define MSG_SVS2MODE    "SVS2MODE"

ModuleHeader MOD_HEADER(m_svsmode)
  = {
	"m_svsmode",
	"4.0",
	"command /svsmode and svs2mode", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_svsmode)
{
	CommandAdd(modinfo->handle, MSG_SVSMODE, m_svsmode, MAXPARA, M_SERVER|M_USER);
	CommandAdd(modinfo->handle, MSG_SVS2MODE, m_svs2mode, MAXPARA, M_SERVER|M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_svsmode)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_svsmode)
{
	return MOD_SUCCESS;
}

void unban_user(aClient *sptr, aChannel *chptr, aClient *acptr, char chmode)
{
	Extban *extban;
	Ban *ban, *bnext;
	Ban **banlist;
	char uhost[NICKLEN+USERLEN+HOSTLEN+6], vhost[NICKLEN+USERLEN+HOSTLEN+6];
	char ihost[NICKLEN+USERLEN+HOSTLEN+6], chost[NICKLEN+USERLEN+HOSTLEN+6];

	/* BUILD HOSTS */

	*uhost = *vhost = *ihost = *chost = '\0';

	strlcpy(uhost, make_nick_user_host(acptr->name, 
		acptr->user->username, acptr->user->realhost),
		sizeof uhost);

	if (GetIP(acptr)) /* only if we actually have an IP */
		strlcpy(ihost, make_nick_user_host(acptr->name,
			acptr->user->username, GetIP(acptr)),
			sizeof ihost);

	/* The next could have been an IsSetHost(), but I'm playing it safe with regards to backward compat. */
	if (IsHidden(acptr) &&
	    !(*acptr->user->cloakedhost && !strcasecmp(acptr->user->virthost, acptr->user->cloakedhost))) 
	{
		strlcpy(vhost, make_nick_user_host(acptr->name,
			acptr->user->username, acptr->user->virthost),
			sizeof vhost);
	}

	if (*acptr->user->cloakedhost) /* only if we know the cloaked host */
		strlcpy(chost, make_nick_user_host(acptr->name, 
			acptr->user->username, acptr->user->cloakedhost),
			sizeof chost);

	/* SELECT BANLIST */

	switch (chmode)
	{
		case 'b':
			banlist = &chptr->banlist;
			break;
		case 'e':
			banlist = &chptr->exlist;
			break;
		case 'I':
			banlist = &chptr->invexlist;
			break;
		default:
			abort();
	}

	/* DO THE ACTUAL WORK */

	for (ban = *banlist; ban; ban = bnext)
	{
		bnext = ban->next;
		if (!match(ban->banstr, uhost) ||
		    (*vhost && !match(ban->banstr, vhost)) ||
		    (*ihost && !match(ban->banstr, ihost)) ||
		    (*chost && !match(ban->banstr, chost)))
		{
			add_send_mode_param(chptr, sptr, '-',  chmode, 
				ban->banstr);
			del_listmode(banlist, chptr, ban->banstr);
		}
		else if (chmode != 'I' && *ban->banstr == '~' && (extban = findmod_by_bantype(ban->banstr[1])))
		{
			if (extban->options & EXTBOPT_CHSVSMODE) 
			{
				if (extban->is_banned(acptr, chptr, ban->banstr, BANCHK_JOIN))
				{
					add_send_mode_param(chptr, acptr, '-', chmode, ban->banstr);
					del_listmode(banlist, chptr, ban->banstr);
				}
			}
		}
	}
}

void clear_bans(aClient *sptr, aChannel *chptr, char chmode)
{
	Extban *extban;
	Ban *ban, *bnext;
	Ban **banlist;

	switch (chmode)
	{
		case 'b':
			banlist = &chptr->banlist;
			break;
		case 'e':
			banlist = &chptr->exlist;
			break;
		case 'I':
			banlist = &chptr->invexlist;
			break;
		default:
			abort();
	}
	
	for (ban = *banlist; ban; ban = bnext)
	{
		bnext = ban->next;
		if (chmode != 'I' && (*ban->banstr == '~') && (extban = findmod_by_bantype(ban->banstr[1])))
		{
			if (!(extban->options & EXTBOPT_CHSVSMODE))							
				continue;
		}
		add_send_mode_param(chptr, sptr, '-',  chmode, ban->banstr);
		del_listmode(banlist, chptr, ban->banstr);
	}
}

int channel_svsmode(aClient *cptr, aClient *sptr, int parc, char *parv[]) 
{
	aChannel *chptr;
	time_t ts;
	aClient *acptr;
	char *m;
	int what = MODE_ADD;
	int i = 4;

	*parabuf = '\0';
	modebuf[0] = 0;
	if(!(chptr = find_channel(parv[1], NULL)))
		return 0;

	ts = atol(parv[parc-1]);
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
				Extban *extban;
				Ban *ban, *bnext;
				if (parc >= i) {
					if (!(acptr = find_person(parv[i-1], NULL))) {
						i++;
						break;
					}
					if (ts && ts != acptr->local->since) {
						i++;
						break;
					}
					i++;

					unban_user(sptr, chptr, acptr, 'b');
				}
				else {
					clear_bans(sptr, chptr, 'b');
				}
			}
			break;
			case 'e': {
				Extban *extban;
				Ban *ban, *bnext;
				if (parc >= i) {
					if (!(acptr = find_person(parv[i-1], NULL))) {
						i++;
						break;
					}
					if (ts && ts != acptr->local->since) {
						i++;
						break;
					}
					i++;

					unban_user(sptr, chptr, acptr, 'e');
				}
				else {
					clear_bans(sptr, chptr, 'e');
				}
			}
			break;
			case 'I': {
				Ban *ban, *bnext;
				if (parc >= i) {
					if (!(acptr = find_person(parv[i-1], NULL))) {
						i++;
						break;
					}
					if (ts && ts != acptr->local->since) {
						i++;
						break;
					}
					i++;

					unban_user(sptr, chptr, acptr, 'I');
				}
				else {
					clear_bans(sptr, chptr, 'I');
				}
			}
			break;

#ifdef DEBUGMODE
		default:
			sendto_realops("Warning! Invalid mode `%c' used with 'SVSMODE %s %s %s' (from %s %s)",
				       *m, chptr->chname, parv[2], parv[3] ? parv[3] : "",
				       cptr->name, sptr->name);
			break;
#endif
		}
	}

	/* only send message if modes have changed */
	if (*parabuf) {
		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s", sptr->name, chptr->chname, 
			modebuf, parabuf);
		sendto_server(NULL, 0, 0, ":%s MODE %s %s %s", sptr->name, chptr->chname, modebuf, parabuf);

		/* Activate this hook just like m_mode.c */
		RunHook7(HOOKTYPE_REMOTE_CHANMODE, cptr, sptr, chptr, modebuf, parabuf, ts, 0);

		*parabuf = 0;
	}
	return 0;
}

/*
 * do_svsmode() [merge from svsmode/svs2mode]
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
	int  what;
	long setflags = 0;

	if (!IsULine(sptr))
		return 0;

	what = MODE_ADD;

	if (parc < 3)
		return 0;

	if (parv[1][0] == '#') 
		return channel_svsmode(cptr, sptr, parc, parv);

	if (!(acptr = find_person(parv[1], NULL)))
		return 0;

	/* initialize setflag to be the user's pre-SVSMODE flags */
	for (i = 0; i <= Usermode_highest; i++)
		if (Usermode_Table[i].flag && (acptr->umodes & Usermode_Table[i].mode))
			setflags |= Usermode_Table[i].mode;

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
			case 'o':
				if ((what == MODE_ADD) && !(acptr->umodes & UMODE_OPER))
				{
					if (!IsOper(acptr) && MyClient(acptr))
						list_add(&acptr->special_node, &oper_list);

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

					if (MyClient(acptr) && !list_empty(&acptr->special_node))
						list_del(&acptr->special_node);
					
					/* User is no longer oper (after the goto below, anyway)...
					 * so remove all oper-only modes and snomasks.
					 */
					remove_oper_privileges(acptr, 0);
				}
				goto setmodex;
			case 'H':
				if (what == MODE_ADD && !(acptr->umodes & UMODE_HIDEOPER))
				{
					if (!IsOper(acptr) && !strchr(parv[2], 'o')) /* (ofcoz this strchr() is flawed) */
					{
						/* isn't an oper, and would not become one either.. abort! */
						sendto_realops(
							"[BUG] server %s tried to set +H while user not an oper, para=%s/%s, "
							"umodes=%ld, please fix your services or if you think it's our fault, "
							"report at http://bugs.unrealircd.org/", sptr->name, parv[1], parv[2], acptr->umodes);
						break; /* abort! */
					}
					IRCstats.operators--;
				}
				if (what == MODE_DEL && (acptr->umodes & UMODE_HIDEOPER))
					IRCstats.operators++;
				goto setmodex;
			case 'd':
				if (parv[3])
				{
					strlcpy(acptr->user->svid, parv[3], sizeof(acptr->user->svid));
					sendto_common_channels_local_butone(acptr, PROTO_ACCOUNT_NOTIFY, ":%s ACCOUNT %s",
									    acptr->name,
									    !isdigit(*acptr->user->svid) ? acptr->user->svid : "*");
				}
				break;
			case 'x':
				if (what == MODE_DEL)
				{
					/* -x */
					if (acptr->user->virthost)
					{
						/* Removing mode +x and virthost set... recalculate host then (but don't activate it!) */
						MyFree(acptr->user->virthost);
						acptr->user->virthost = strdup(acptr->user->cloakedhost);
					}
				} else
				{
					/* +x */
					if (!acptr->user->virthost)
					{
						/* Hmm... +x but no virthost set, that's bad... use cloakedhost.
						 * Not sure if this could ever happen, but just in case... -- Syzop
						 */
						acptr->user->virthost = strdup(acptr->user->cloakedhost);
					}
					/* Announce the new host to VHP servers if we're setting the virthost to the cloakedhost.
					 * In other cases, we can assume that the host has been broadcasted already (after all,
					 * how else could it have been changed...?).
					 * NOTES: we're doing a strcasecmp here instead of simply checking if it's a "+x but
					 * not -t"-case. The reason for this is that the 't' might follow ("+xt" instead of "+tx"),
					 * in which case we would have needlessly announced it. Ok I didn't test it but that's
					 * the idea behind it :P. -- Syzop
					 */
					if (MyClient(acptr) && !strcasecmp(acptr->user->virthost, acptr->user->cloakedhost))
						sendto_server(NULL, PROTO_VHP, 0, ":%s SETHOST :%s", acptr->name,
							acptr->user->virthost);
				}
				goto setmodex;
			case 'z':
				/* Setting and unsetting user mode 'z' remotely is not supported */
				break;
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
		sendto_server(cptr, 0, 0, ":%s %s %s %s %s",
		    sptr->name, show_change ? "SVS2MODE" : "SVSMODE",
		    parv[1], parv[2], parv[3]);
	else
		sendto_server(cptr, 0, 0,  ":%s %s %s %s",
		    sptr->name, show_change ? "SVS2MODE" : "SVSMODE",
		    parv[1], parv[2]);

	/* Here we trigger the same hooks that m_mode does and, likewise,
	   only if the old flags (setflags) are different than the newly-
	   set ones */
	if (setflags != acptr->umodes)
		RunHook3(HOOKTYPE_UMODE_CHANGE, sptr, setflags, acptr->umodes);

	if (show_change)
	{
		char buf[BUFSIZE];
		send_umode(NULL, acptr, setflags, ALL_UMODES, buf);
		if (MyClient(acptr) && buf[0] && buf[1])
			sendto_one(acptr, ":%s MODE %s :%s", sptr->name, acptr->name, buf);
	}

	VERIFY_OPERCOUNT(acptr, "svsmodeX");
	return 0;
}

/*
 * m_svsmode() added by taz
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 */
CMD_FUNC(m_svsmode)
{
	return do_svsmode(cptr, sptr, parc, parv, 0);
}

/*
 * m_svs2mode() added by Potvin
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 */
CMD_FUNC(m_svs2mode)
{
	return do_svsmode(cptr, sptr, parc, parv, 1);
}

void add_send_mode_param(aChannel *chptr, aClient *from, char what, char mode, char *param) {
	static char *modes = NULL, lastwhat;
	static short count = 0;
	short send = 0;
	
	if (!modes) modes = modebuf;
	
	if (!modebuf[0]) {
		modes = modebuf;
		*modes++ = what;
		*modes = 0;
		lastwhat = what;
		*parabuf = 0;
		count = 0;
	}
	if (lastwhat != what) {
		*modes++ = what;
		*modes = 0;
		lastwhat = what;
	}
	if (strlen(parabuf) + strlen(param) + 11 < MODEBUFLEN) {
		if (*parabuf) 
			strcat(parabuf, " ");
		strcat(parabuf, param);
		*modes++ = mode;
		*modes = 0;
		count++;
	}
	else if (*parabuf) 
		send = 1;

	if (count == MAXMODEPARAMS)
		send = 1;

	if (send) {
		sendto_channel_butserv(chptr, from, ":%s MODE %s %s %s",
			from->name, chptr->chname, modebuf, parabuf);
		sendto_server(NULL, 0, 0, ":%s MODE %s %s %s", from->name, chptr->chname, modebuf, parabuf);
		send = 0;
		*parabuf = 0;
		modes = modebuf;
		*modes++ = what;
		lastwhat = what;
		if (count != MAXMODEPARAMS) {
			strcpy(parabuf, param);
			*modes++ = mode;
			count = 1;
		}
		else 
			count = 0;
		*modes = 0;
	}
}
