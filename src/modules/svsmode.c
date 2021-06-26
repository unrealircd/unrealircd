/*
 *   IRC - Internet Relay Chat, src/modules/svsmode.c
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

void add_send_mode_param(Channel *channel, Client *from, char what, char mode, char *param);
CMD_FUNC(cmd_svsmode);
CMD_FUNC(cmd_svs2mode);

#define MSG_SVSMODE 	"SVSMODE"	
#define MSG_SVS2MODE    "SVS2MODE"

ModuleHeader MOD_HEADER
  = {
	"svsmode",
	"5.0",
	"command /svsmode and svs2mode", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSMODE, cmd_svsmode, MAXPARA, CMD_SERVER|CMD_USER);
	CommandAdd(modinfo->handle, MSG_SVS2MODE, cmd_svs2mode, MAXPARA, CMD_SERVER|CMD_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
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

void unban_user(Client *client, Channel *channel, Client *acptr, char chmode)
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
			banlist = &channel->banlist;
			break;
		case 'e':
			banlist = &channel->exlist;
			break;
		case 'I':
			banlist = &channel->invexlist;
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
			add_send_mode_param(channel, client, '-',  chmode, 
				ban->banstr);
			del_listmode(banlist, channel, ban->banstr);
		}
		else if (chmode != 'I' && *ban->banstr == '~' && (extban = findmod_by_bantype(ban->banstr[1])))
		{
			if (extban->options & EXTBOPT_CHSVSMODE) 
			{
				if (extban->is_banned(acptr, channel, ban->banstr, BANCHK_JOIN, NULL, NULL))
				{
					add_send_mode_param(channel, acptr, '-', chmode, ban->banstr);
					del_listmode(banlist, channel, ban->banstr);
				}
			}
		}
	}
}

void clear_bans(Client *client, Channel *channel, char chmode)
{
	Extban *extban;
	Ban *ban, *bnext;
	Ban **banlist;

	switch (chmode)
	{
		case 'b':
			banlist = &channel->banlist;
			break;
		case 'e':
			banlist = &channel->exlist;
			break;
		case 'I':
			banlist = &channel->invexlist;
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
		add_send_mode_param(channel, client, '-',  chmode, ban->banstr);
		del_listmode(banlist, channel, ban->banstr);
	}
}

/** Special Channel MODE command for services, used by SVSMODE and SVS2MODE.
 * @note
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
void channel_svsmode(Client *client, int parc, char *parv[]) 
{
	Channel *channel;
	Client *target;
	char *m;
	int what = MODE_ADD;
	int i = 4; // wtf is this
	Member *cm;
	int channel_flags;

	*parabuf = *modebuf = '\0';

	if ((parc < 3) || BadPtr(parv[2]))
		return;

	if (!(channel = find_channel(parv[1], NULL)))
		return;

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
						*m, channel->chname, client->name);
					continue;
				}
				channel_flags = char_to_channelflag(*m);
				for (cm = channel->members; cm; cm = cm->next)
				{
					if (cm->flags & channel_flags)
					{
						Membership *mb;
						mb = find_membership_link(cm->client->user->channel, channel);
						add_send_mode_param(channel, client, '-', *m, cm->client->name);
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
					if (!(target = find_person(parv[i-1], NULL)))
					{
						i++;
						break;
					}
					i++;

					unban_user(client, channel, target, *m);
				}
				else {
					clear_bans(client, channel, *m);
				}
				break;
			default:
				sendto_realops("Warning! Invalid mode `%c' used with 'SVSMODE %s %s %s' (from %s %s)",
					       *m, channel->chname, parv[2], parv[3] ? parv[3] : "",
					       client->direction->name, client->name);
				break;
		}
	}

	/* only send message if modes have changed */
	if (*parabuf)
	{
		MessageTag *mtags = NULL;
		/* NOTE: cannot use 'recv_mtag' here because MODE could be rewrapped. Not ideal :( */
		new_message(client, NULL, &mtags);

		sendto_channel(channel, client, client, 0, 0, SEND_LOCAL, mtags,
		               ":%s MODE %s %s %s",
		               client->name, channel->chname,  modebuf, parabuf);
		sendto_server(NULL, 0, 0, mtags, ":%s MODE %s %s %s", client->id, channel->chname, modebuf, parabuf);

		/* Activate this hook just like cmd_mode.c */
		RunHook7(HOOKTYPE_REMOTE_CHANMODE, client, channel, mtags, modebuf, parabuf, 0, 0);

		free_message_tags(mtags);

		*parabuf = 0;
	}
}

/** Special User MODE command for Services.
 * @note
 * This is used by both SVSMODE and SVS2MODE, when dealing with users (not channels).
 * parv[1] - nick to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 *
 * show_change can be 0 (for svsmode) or 1 (for svs2mode).
 */
void do_svsmode(Client *client, MessageTag *recv_mtags, int parc, char *parv[], int show_change)
{
	int i;
	char *m;
	Client *target;
	int  what;
	long setflags = 0;

	if (!IsULine(client))
		return;

	what = MODE_ADD;

	if (parc < 3)
		return;

	if (parv[1][0] == '#') 
	{
		channel_svsmode(client, parc, parv);
		return;
	}

	if (!(target = find_person(parv[1], NULL)))
		return;

	userhost_save_current(target);

	/* initialize setflag to be the user's pre-SVSMODE flags */
	for (i = 0; i <= Usermode_highest; i++)
		if (Usermode_Table[i].flag && (target->umodes & Usermode_Table[i].mode))
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
				if ((what == MODE_ADD) && !(target->umodes & UMODE_INVISIBLE))
					irccounts.invisible++;
				if ((what == MODE_DEL) && (target->umodes & UMODE_INVISIBLE))
					irccounts.invisible--;
				goto setmodex;
			case 'o':
				if ((what == MODE_ADD) && !(target->umodes & UMODE_OPER))
				{
					if (!IsOper(target) && MyUser(target))
						list_add(&target->special_node, &oper_list);

					irccounts.operators++;
				}
				if ((what == MODE_DEL) && (target->umodes & UMODE_OPER))
				{
					if (target->umodes & UMODE_HIDEOPER)
					{
						/* clear 'H' too, and opercount stays the same.. */
						target->umodes &= ~UMODE_HIDEOPER;
					} else {
						irccounts.operators--;
					}

					if (MyUser(target) && !list_empty(&target->special_node))
						list_del(&target->special_node);
					
					/* User is no longer oper (after the goto below, anyway)...
					 * so remove all oper-only modes and snomasks.
					 */
					remove_oper_privileges(target, 0);
				}
				goto setmodex;
			case 'H':
				if (what == MODE_ADD && !(target->umodes & UMODE_HIDEOPER))
				{
					if (!IsOper(target) && !strchr(parv[2], 'o')) /* (ofcoz this strchr() is flawed) */
					{
						/* isn't an oper, and would not become one either.. abort! */
						sendto_realops(
							"[BUG] server %s tried to set +H while user not an oper, para=%s/%s, "
							"umodes=%ld, please fix your services or if you think it's our fault, "
							"report at https://bugs.unrealircd.org/", client->name, parv[1], parv[2], target->umodes);
						break; /* abort! */
					}
					irccounts.operators--;
				}
				if (what == MODE_DEL && (target->umodes & UMODE_HIDEOPER))
					irccounts.operators++;
				goto setmodex;
			case 'd':
				if (parv[3])
				{
					int was_logged_in = IsLoggedIn(target) ? 1 : 0;
					strlcpy(target->user->svid, parv[3], sizeof(target->user->svid));
					if (!was_logged_in && !IsLoggedIn(target))
					{
						/* We don't care about users going from not logged in
						 * to not logged in, which is something that can happen
						 * from 0 to 123456, eg from no account to unconfirmed account.
						 */
					} else {
						/* LOGIN or LOGOUT (or account change) */
						user_account_login(recv_mtags, target);
					}
					if (MyConnect(target) && IsDead(target))
						return; /* was killed due to *LINE on ~a probably */
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
					if (target->user->virthost)
					{
						/* Removing mode +x and virthost set... recalculate host then (but don't activate it!) */
						safe_strdup(target->user->virthost, target->user->cloakedhost);
					}
				} else
				{
					/* +x */
					if (!target->user->virthost)
					{
						/* Hmm... +x but no virthost set, that's bad... use cloakedhost.
						 * Not sure if this could ever happen, but just in case... -- Syzop
						 */
						safe_strdup(target->user->virthost, target->user->cloakedhost);
					}
					/* Announce the new host to VHP servers if we're setting the virthost to the cloakedhost.
					 * In other cases, we can assume that the host has been broadcasted already (after all,
					 * how else could it have been changed...?).
					 * NOTES: we're doing a strcasecmp here instead of simply checking if it's a "+x but
					 * not -t"-case. The reason for this is that the 't' might follow ("+xt" instead of "+tx"),
					 * in which case we would have needlessly announced it. Ok I didn't test it but that's
					 * the idea behind it :P. -- Syzop
					 */
					if (MyUser(target) && !strcasecmp(target->user->virthost, target->user->cloakedhost))
						sendto_server(NULL, PROTO_VHP, 0, NULL, ":%s SETHOST :%s", target->id,
							target->user->virthost);
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
					if (target->user->virthost && *target->user->cloakedhost && strcasecmp(target->user->cloakedhost, GetHost(target)))
					{
						/* Make the change effective: */
						safe_strdup(target->user->virthost, target->user->cloakedhost);
						/* And broadcast the change to VHP servers */
						if (MyUser(target))
							sendto_server(NULL, PROTO_VHP, 0, NULL, ":%s SETHOST :%s", target->id,
								target->user->virthost);
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
							target->umodes |= Usermode_Table[i].mode;
						else
							target->umodes &= ~Usermode_Table[i].mode;
						break;
					}
				}
				break;
		} /*switch*/

	if (parc > 3)
		sendto_server(client, 0, 0, NULL, ":%s %s %s %s %s",
		    client->id, show_change ? "SVS2MODE" : "SVSMODE",
		    parv[1], parv[2], parv[3]);
	else
		sendto_server(client, 0, 0, NULL, ":%s %s %s %s",
		    client->id, show_change ? "SVS2MODE" : "SVSMODE",
		    parv[1], parv[2]);

	/* Here we trigger the same hooks that cmd_mode does and, likewise,
	   only if the old flags (setflags) are different than the newly-
	   set ones */
	if (setflags != target->umodes)
		RunHook3(HOOKTYPE_UMODE_CHANGE, target, setflags, target->umodes);

	if (show_change)
	{
		char buf[BUFSIZE];
		build_umode_string(target, setflags, ALL_UMODES, buf);
		if (MyUser(target) && *buf)
			sendto_one(target, NULL, ":%s MODE %s :%s", client->name, target->name, buf);
	}

	userhost_changed(target); /* we can safely call this, even if nothing changed */

	VERIFY_OPERCOUNT(target, "svsmodeX");
}

/*
 * cmd_svsmode() added by taz
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 */
CMD_FUNC(cmd_svsmode)
{
	do_svsmode(client, recv_mtags, parc, parv, 0);
}

/*
 * cmd_svs2mode() added by Potvin
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - Service Stamp (if mode == d)
 */
CMD_FUNC(cmd_svs2mode)
{
	do_svsmode(client, recv_mtags, parc, parv, 1);
}

void add_send_mode_param(Channel *channel, Client *from, char what, char mode, char *param)
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
		sendto_channel(channel, from, from, 0, 0, SEND_LOCAL, mtags,
		               ":%s MODE %s %s %s",
		               from->name, channel->chname, modebuf, parabuf);
		sendto_server(NULL, 0, 0, mtags, ":%s MODE %s %s %s", from->id, channel->chname, modebuf, parabuf);
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
