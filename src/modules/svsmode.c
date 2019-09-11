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

/* FIXME: this one needs a lot more mtag work !! with _special... */

#include "unrealircd.h"

void add_send_mode_param(Channel *chptr, Client *from, char what, char mode, char *param);
CMD_FUNC(m_svsmode);
CMD_FUNC(m_svs2mode);

#define MSG_SVSMODE 	"SVSMODE"	
#define MSG_SVS2MODE    "SVS2MODE"

ModuleHeader MOD_HEADER(svsmode)
  = {
	"svsmode",
	"5.0",
	"command /svsmode and svs2mode", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT(svsmode)
{
	CommandAdd(modinfo->handle, MSG_SVSMODE, m_svsmode, MAXPARA, M_SERVER|M_USER);
	CommandAdd(modinfo->handle, MSG_SVS2MODE, m_svs2mode, MAXPARA, M_SERVER|M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(svsmode)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(svsmode)
{
	return MOD_SUCCESS;
}

void unban_user(Client *sptr, Channel *chptr, Client *acptr, char chmode)
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
		if (match_simple(ban->banstr, uhost) ||
		    (*vhost && match_simple(ban->banstr, vhost)) ||
		    (*ihost && match_simple(ban->banstr, ihost)) ||
		    (*chost && match_simple(ban->banstr, chost)))
		{
			add_send_mode_param(chptr, sptr, '-',  chmode, 
				ban->banstr);
			del_listmode(banlist, chptr, ban->banstr);
		}
		else if (chmode != 'I' && *ban->banstr == '~' && (extban = findmod_by_bantype(ban->banstr[1])))
		{
			if (extban->options & EXTBOPT_CHSVSMODE) 
			{
				if (extban->is_banned(acptr, chptr, ban->banstr, BANCHK_JOIN, NULL, NULL))
				{
					add_send_mode_param(chptr, acptr, '-', chmode, ban->banstr);
					del_listmode(banlist, chptr, ban->banstr);
				}
			}
		}
	}
}

void clear_bans(Client *sptr, Channel *chptr, char chmode)
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

/** Special Channel MODE command for services, used by SVSMODE and SVS2MODE.
 * @notes
 * This SVSMODE/SVS2MODE for channels is not simply the regular MODE "but for
 * services". No, it does different things.
 *
 *  Syntax: SVSMODE <channel> <mode and params>
 *
 * There are three variants (do NOT mix them!):
 * 1) SVSMODE #chan -[b|e|I]
 *    This will remove all bans/exempts/invex in the channel.
 * 2) SVSMODE #chan -[b|e|I] nickname
 *    This will remove all bans/exempts/invex that affect nickname.
 *    Eg: -b nick may remove bans places on IP, host, cloaked host, vhost,
 *    basically anything that matches this nickname. Note that the
 *    user with the specified nickname needs to be online for this to work.
 * 3) SVSMODE #chan -[v|h|o|a|q]
 *    This will remove the specified mode(s) from all users in the channel.
 *    Eg: -o will result in a MODE -o for every channel operator.
 *
 * OLD syntax had a 'ts' parameter. No services are known to use this.
 */
int channel_svsmode(Client *cptr, Client *sptr, int parc, char *parv[]) 
{
	Channel *chptr;
	Client *acptr;
	char *m;
	int what = MODE_ADD;
	int i = 4; // wtf is this
	Member *cm;
	int channel_flags;

	*parabuf = *modebuf = '\0';

	if ((parc < 3) || BadPtr(parv[2]))
		return 0;

	if (!(chptr = find_channel(parv[1], NULL)))
		return 0;

	for(m = parv[2]; *m; m++)
	{
		switch (*m)
		{
			case '+':
				what = MODE_ADD;
				break;
			case '-':
				what = MODE_DEL;
				break;
			case 'v':
			case 'h':
			case 'o':
			case 'a':
			case 'q':
				if (what != MODE_DEL)
				{
					sendto_realops("Warning! Received SVS(2)MODE with +%c for %s from %s, which is invalid!!",
						*m, chptr->chname, sptr->name);
					continue;
				}
				channel_flags = char_to_channelflag(*m);
				for (cm = chptr->members; cm; cm = cm->next)
				{
					if (cm->flags & channel_flags)
					{
						Membership *mb;
						mb = find_membership_link(cm->cptr->user->channel, chptr);
						add_send_mode_param(chptr, sptr, '-', *m, cm->cptr->name);
						cm->flags &= ~channel_flags;
						if (mb)
							mb->flags = cm->flags;
					}
				}
				break;
			case 'b':
			case 'e':
			case 'I':
				if (parc >= i)
				{
					if (!(acptr = find_person(parv[i-1], NULL)))
					{
						i++;
						break;
					}
					i++;

					unban_user(sptr, chptr, acptr, *m);
				}
				else {
					clear_bans(sptr, chptr, *m);
				}
				break;
			default:
				sendto_realops("Warning! Invalid mode `%c' used with 'SVSMODE %s %s %s' (from %s %s)",
					       *m, chptr->chname, parv[2], parv[3] ? parv[3] : "",
					       cptr->name, sptr->name);
				break;
		}
	}

	/* only send message if modes have changed */
	if (*parabuf)
	{
		MessageTag *mtags = NULL;
		/* NOTE: cannot use 'recv_mtag' here because MODE could be rewrapped. Not ideal :( */
		new_message(sptr, NULL, &mtags);

		sendto_channel(chptr, sptr, sptr, 0, 0, SEND_LOCAL, mtags,
		               ":%s MODE %s %s %s",
		               sptr->name, chptr->chname,  modebuf, parabuf);
		sendto_server(NULL, 0, 0, mtags, ":%s MODE %s %s %s", sptr->name, chptr->chname, modebuf, parabuf);

		/* Activate this hook just like m_mode.c */
		RunHook8(HOOKTYPE_REMOTE_CHANMODE, cptr, sptr, chptr, mtags, modebuf, parabuf, 0, 0);

		free_message_tags(mtags);

		*parabuf = 0;
	}
	return 0;
}

/** Special User MODE command for Services.
 * @notes
 * This is used by both SVSMODE and SVS2MODE, when dealing with users (not channels).
 * parv[1] - nick to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 *
 * show_change can be 0 (for svsmode) or 1 (for svs2mode).
 */
int do_svsmode(Client *cptr, Client *sptr, MessageTag *recv_mtags, int parc, char *parv[], int show_change)
{
	int i;
	char *m;
	Client *acptr;
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

	userhost_save_current(acptr);

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
					ircstats.invisible++;
				if ((what == MODE_DEL) && (acptr->umodes & UMODE_INVISIBLE))
					ircstats.invisible--;
				goto setmodex;
			case 'o':
				if ((what == MODE_ADD) && !(acptr->umodes & UMODE_OPER))
				{
					if (!IsOper(acptr) && MyUser(acptr))
						list_add(&acptr->special_node, &oper_list);

					ircstats.operators++;
				}
				if ((what == MODE_DEL) && (acptr->umodes & UMODE_OPER))
				{
					if (acptr->umodes & UMODE_HIDEOPER)
					{
						/* clear 'H' too, and opercount stays the same.. */
						acptr->umodes &= ~UMODE_HIDEOPER;
					} else {
						ircstats.operators--;
					}

					if (MyUser(acptr) && !list_empty(&acptr->special_node))
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
							"report at https://bugs.unrealircd.org/", sptr->name, parv[1], parv[2], acptr->umodes);
						break; /* abort! */
					}
					ircstats.operators--;
				}
				if (what == MODE_DEL && (acptr->umodes & UMODE_HIDEOPER))
					ircstats.operators++;
				goto setmodex;
			case 'd':
				if (parv[3])
				{
					MessageTag *mtags = NULL;
					strlcpy(acptr->user->svid, parv[3], sizeof(acptr->user->svid));
					new_message(acptr, recv_mtags, &mtags);
					sendto_local_common_channels(acptr, acptr,
					                             ClientCapabilityBit("account-notify"), mtags,
					                             ":%s ACCOUNT %s",
					                             acptr->name,
					                             !isdigit(*acptr->user->svid) ? acptr->user->svid : "*");
					free_message_tags(mtags);
				}
				else
				{
					/* setting deaf */
					goto setmodex;
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
					if (MyUser(acptr) && !strcasecmp(acptr->user->virthost, acptr->user->cloakedhost))
						sendto_server(NULL, PROTO_VHP, 0, NULL, ":%s SETHOST :%s", acptr->name,
							acptr->user->virthost);
				}
				goto setmodex;
			case 't':
				/* We support -t nowadays, which means we remove the vhost and set the cloaked host
				 * (note that +t is a NOOP, that code is in +x)
				 */
				if (what == MODE_DEL)
				{
					/* First, check if there's a change at all. Perhaps it's a -t on someone
					 * with no virthost or virthost being cloakedhost?
					 * Also, check to make sure user->cloakedhost exists at all.
					 * This so we won't crash in weird cases like non-conformant servers.
					 */
					if (acptr->user->virthost && *acptr->user->cloakedhost && strcasecmp(acptr->user->cloakedhost, GetHost(acptr)))
					{
						/* Make the change effective: */
						safestrdup(acptr->user->virthost, acptr->user->cloakedhost);
						/* And broadcast the change to VHP servers */
						if (MyUser(acptr))
							sendto_server(NULL, PROTO_VHP, 0, NULL, ":%s SETHOST :%s", acptr->name,
								acptr->user->virthost);
					}
					goto setmodex;
				}
				break;
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
		sendto_server(cptr, 0, 0, NULL, ":%s %s %s %s %s",
		    sptr->name, show_change ? "SVS2MODE" : "SVSMODE",
		    parv[1], parv[2], parv[3]);
	else
		sendto_server(cptr, 0, 0, NULL, ":%s %s %s %s",
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
		if (MyUser(acptr) && buf[0] && buf[1])
			sendto_one(acptr, NULL, ":%s MODE %s :%s", sptr->name, acptr->name, buf);
	}

	userhost_changed(acptr); /* we can safely call this, even if nothing changed */

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
	return do_svsmode(cptr, sptr, recv_mtags, parc, parv, 0);
}

/*
 * m_svs2mode() added by Potvin
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 */
CMD_FUNC(m_svs2mode)
{
	return do_svsmode(cptr, sptr, recv_mtags, parc, parv, 1);
}

void add_send_mode_param(Channel *chptr, Client *from, char what, char mode, char *param)
{
	static char *modes = NULL, lastwhat;
	static short count = 0;
	short send = 0;
	
	if (!modes) modes = modebuf;
	
	if (!modebuf[0])
	{
		modes = modebuf;
		*modes++ = what;
		*modes = 0;
		lastwhat = what;
		*parabuf = 0;
		count = 0;
	}
	if (lastwhat != what)
	{
		*modes++ = what;
		*modes = 0;
		lastwhat = what;
	}
	if (strlen(parabuf) + strlen(param) + 11 < MODEBUFLEN)
	{
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

	if (send)
	{
		MessageTag *mtags = NULL;
		/* NOTE: cannot use 'recv_mtag' here because MODE could be rewrapped. Not ideal :( */
		new_message(from, NULL, &mtags);
		sendto_channel(chptr, from, from, 0, 0, SEND_LOCAL, mtags,
		               ":%s MODE %s %s %s",
		               from->name, chptr->chname, modebuf, parabuf);
		sendto_server(NULL, 0, 0, mtags, ":%s MODE %s %s %s", from->name, chptr->chname, modebuf, parabuf);
		free_message_tags(mtags);
		send = 0;
		*parabuf = 0;
		modes = modebuf;
		*modes++ = what;
		lastwhat = what;
		if (count != MAXMODEPARAMS)
		{
			strcpy(parabuf, param);
			*modes++ = mode;
			count = 1;
		}
		else 
			count = 0;
		*modes = 0;
	}
}
