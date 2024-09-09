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
	"unrealircd-6",
    };

char modebuf[BUFSIZE], parabuf[BUFSIZE];

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
	const char *nextbanstr;
	Ban *ban, *bnext;
	Ban **banlist;
	BanContext *b;
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

	b = safe_alloc(sizeof(BanContext));

	/* SELECT BANLIST */

	switch (chmode)
	{
		case 'b':
			banlist = &channel->banlist;
			b->ban_type = EXBTYPE_BAN;
			break;
		case 'e':
			banlist = &channel->exlist;
			b->ban_type = EXBTYPE_EXCEPT;
			break;
		case 'I':
			banlist = &channel->invexlist;
			b->ban_type = EXBTYPE_INVEX;
			break;
		default:
			abort();
	}

	/* DO THE ACTUAL WORK */
	b->client = acptr;
	b->channel = channel;
	b->ban_check_types = BANCHK_JOIN;
	b->ban_type = EXBTYPE_BAN;

	for (ban = *banlist; ban; ban = bnext)
	{
		bnext = ban->next;
		if (match_simple(ban->banstr, uhost) ||
		    (*vhost && match_simple(ban->banstr, vhost)) ||
		    (*ihost && match_simple(ban->banstr, ihost)) ||
		    (*chost && match_simple(ban->banstr, chost)))
		{
			add_send_mode_param(channel, client, '-',  chmode, ban->banstr);
			del_listmode(banlist, channel, ban->banstr);
		}
		else if (chmode != 'I' && *ban->banstr == '~' && (extban = findmod_by_bantype(ban->banstr, &nextbanstr)))
		{
			if (extban->is_banned_events & b->ban_check_types)
			{
				b->banstr = nextbanstr;
				if (extban->is_banned(b))
				{
					add_send_mode_param(channel, acptr, '-', chmode, ban->banstr);
					del_listmode(banlist, channel, ban->banstr);
				}
			}
		}
	}
	safe_free(b);
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
		if (chmode != 'I' && (*ban->banstr == '~') && (extban = findmod_by_bantype(ban->banstr, NULL)))
		{
			if (!(extban->is_banned_events & BANCHK_JOIN))
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
void channel_svsmode(Client *client, int parc, const char *parv[]) 
{
	Channel *channel;
	Client *target;
	const char *m;
	int what = MODE_ADD;
	int i = 4; // wtf is this
	Member *member;
	int channel_flags;

	*parabuf = *modebuf = '\0';

	if ((parc < 3) || BadPtr(parv[2]))
		return;

	if (!(channel = find_channel(parv[1])))
		return;

	for (m = parv[2]; *m; m++)
	{
		if (*m == '+') 
		{
			what = MODE_ADD;
		} else
		if (*m == '-')
		{
			what = MODE_DEL;
		} else
		if ((*m == 'b') || (*m == 'e') || (*m == 'I'))
		{
			if (parc >= i)
			{
				if (!(target = find_user(parv[i-1], NULL)))
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
		} else
		{
			/* Find member mode handler (vhoaq) */
			Cmode *cm = find_channel_mode_handler(*m);
			if (!cm || (cm->type != CMODE_MEMBER))
			{
				unreal_log(ULOG_WARNING, "svsmode", "INVALID_SVSMODE", client,
				           "Invalid SVSMODE for mode '$mode_character' in channel $channel from $client.",
				           log_data_char("mode_character", *m),
				           log_data_channel("channel", channel));
				continue;
			}
			if (what != MODE_DEL)
			{
				unreal_log(ULOG_WARNING, "svsmode", "INVALID_SVSMODE", client,
				           "Invalid SVSMODE from $client trying to add '$mode_character' in $channel.",
				           log_data_char("mode_character", *m),
				           log_data_channel("channel", channel));
				continue;
			}
			for (member = channel->members; member; member = member->next)
			{
				if (check_channel_access_letter(member->member_modes, *m))
				{
					Membership *mb = find_membership_link(member->client->user->channel, channel);
					if (!mb)
						continue; /* bug */
					
					/* Send the -x out */
					add_send_mode_param(channel, client, '-', *m, member->client->name);
					
					/* And remove from memory */
					del_member_mode_fast(member, mb, *m);
				}
			}
		}
	}

	/* only send message if modes have changed */
	if (*parabuf)
	{
		MessageTag *mtags = NULL;
		int destroy_channel = 0;
		/* NOTE: cannot use 'recv_mtag' here because MODE could be rewrapped. Not ideal :( */
		new_message(client, NULL, &mtags);

		sendto_channel(channel, client, client, 0, 0, SEND_LOCAL, mtags,
		               ":%s MODE %s %s %s",
		               client->name, channel->name,  modebuf, parabuf);
		sendto_server(NULL, 0, 0, mtags, ":%s MODE %s %s %s%s", client->id, channel->name, modebuf, parabuf, IsServer(client)?" 0":"");

		/* Activate this hook just like cmd_mode.c */
		RunHook(HOOKTYPE_REMOTE_CHANMODE, client, channel, mtags, modebuf, parabuf, 0, 0, &destroy_channel);

		free_message_tags(mtags);

		*parabuf = 0;
	}
}

/** Special User MODE command for Services.
 * @note
 * This is used by both SVSMODE and SVS2MODE, when dealing with users (not channels).
 * parv[1] - nick to change mode for
 * parv[2] - modes to change
 * parv[3] - account name (if mode contains 'd')
 *
 * show_change can be 0 (for svsmode) or 1 (for svs2mode).
 */
void do_svsmode(Client *client, MessageTag *recv_mtags, int parc, const char *parv[], int show_change)
{
	Umode *um;
	const char *m;
	Client *target;
	int  what;
	long oldumodes = 0;

	if (!IsSvsCmdOk(client))
		return;

	what = MODE_ADD;

	if (parc < 3)
		return;

	if (parv[1][0] == '#') 
	{
		channel_svsmode(client, parc, parv);
		return;
	}

	if (!(target = find_user(parv[1], NULL)))
		return;

	userhost_save_current(target);

	oldumodes = target->umodes;

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
					if (MyUser(client))
						RunHook(HOOKTYPE_LOCAL_OPER, client, 0, NULL, NULL);
					remove_oper_privileges(target, 0);
				}
				goto setmodex;
			case 'H':
				if (what == MODE_ADD && !(target->umodes & UMODE_HIDEOPER))
				{
					if (!IsOper(target) && !strchr(parv[2], 'o')) /* (ofcoz this strchr() is flawed) */
					{
						/* isn't an oper, and would not become one either.. abort! */
						unreal_log(ULOG_WARNING, "svsmode", "SVSMODE_INVALID", client,
						           "[BUG] Server $client tried to set user mode +H (hidden ircop) "
						           "on a user that is not +o (not ircop)! "
						           "Please fix your services, or if you think it is our fault, then "
						           "report at https://bugs.unrealircd.org/. "
						           "Parameters: $para1 $para2. Target: $target.",
						           log_data_string("para1", parv[1]),
						           log_data_string("para2", parv[2]),
						           log_data_client("target", target));
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
					strlcpy(target->user->account, parv[3], sizeof(target->user->account));
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
				for (um = usermodes; um; um = um->next)
				{
					if (um->letter == *m)
					{
						if (what == MODE_ADD)
							target->umodes |= um->mode;
						else
							target->umodes &= ~um->mode;
						break;
					}
				}
				break;
		} /*switch*/

	if (parc > 3)
		sendto_server(client, 0, 0, recv_mtags, ":%s %s %s %s %s",
		    client->id, show_change ? "SVS2MODE" : "SVSMODE",
		    parv[1], parv[2], parv[3]);
	else
		sendto_server(client, 0, 0, recv_mtags, ":%s %s %s %s",
		    client->id, show_change ? "SVS2MODE" : "SVSMODE",
		    parv[1], parv[2]);

	/* Here we trigger the same hooks that cmd_mode does and, likewise,
	   only if the old flags (oldumodes) are different than the newly-
	   set ones */
	if (oldumodes != target->umodes)
		RunHook(HOOKTYPE_UMODE_CHANGE, target, oldumodes, target->umodes);

	if (show_change)
	{
		char buf[BUFSIZE];
		build_umode_string(target, oldumodes, ALL_UMODES, buf);
		if (MyUser(target) && *buf)
		{
			MessageTag *mtags = NULL;
			new_message(client, recv_mtags, &mtags);
			sendto_one(target, mtags, ":%s MODE %s :%s", client->name, target->name, buf);
			safe_free_message_tags(mtags);
		}
	}

	userhost_changed(target); /* we can safely call this, even if nothing changed */

	VERIFY_OPERCOUNT(target, "svsmodeX");
}

/*
 * cmd_svsmode() added by taz
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - account name (if mode contains 'd')
 */
CMD_FUNC(cmd_svsmode)
{
	do_svsmode(client, recv_mtags, parc, parv, 0);
}

/*
 * cmd_svs2mode() added by Potvin
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 * parv[3] - account name (if mode contains 'd')
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
		               from->name, channel->name, modebuf, parabuf);
		sendto_server(NULL, 0, 0, mtags, ":%s MODE %s %s %s%s", from->id, channel->name, modebuf, parabuf, IsServer(from)?" 0":"");
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
