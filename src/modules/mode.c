/*
 *   IRC - Internet Relay Chat, src/modules/mode.c
 *   (C) 2005-.. The UnrealIRCd Team
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

/* Forward declarations */
int list_mode_request(Client *client, Channel *channel, char *req);
CMD_FUNC(cmd_mode);
CMD_FUNC(cmd_mlock);
void _do_mode(Channel *channel, Client *client, MessageTag *recv_mtags, int parc, char *parv[], time_t sendts, int samode);
void _set_mode(Channel *channel, Client *client, int parc, char *parv[], u_int *pcount,
                       char pvar[MAXMODEPARAMS][MODEBUFLEN + 3]);
CMD_FUNC(_cmd_umode);

/* local: */
int do_mode_char(Channel *channel, long modetype, char modechar, char *param,
                 u_int what, Client *client,
                 u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], long my_access);
int do_extmode_char(Channel *channel, Cmode *handler, char *param, u_int what,
                    Client *client, u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3]);
void make_mode_str(Channel *channel, Cmode_t oldem, int pcount,
                   char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char *mode_buf, char *para_buf,
                   size_t mode_buf_size, size_t para_buf_size);

static void mode_cutoff(char *s);
static void mode_cutoff2(Client *client, Channel *channel, int *parc_out, char *parv[]);
void mode_operoverride_msg(Client *client, Channel *channel, char *modebuf, char *parabuf);

static int samode_in_progress = 0;

ModuleHeader MOD_HEADER
  = {
	"mode",
	"5.0",
	"command /mode",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_DO_MODE, _do_mode);
	EfunctionAddVoid(modinfo->handle, EFUNC_SET_MODE, _set_mode);
	EfunctionAddVoid(modinfo->handle, EFUNC_CMD_UMODE, _cmd_umode);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CommandAdd(modinfo->handle, "MODE", cmd_mode, MAXPARA, CMD_USER|CMD_SERVER);
	CommandAdd(modinfo->handle, MSG_MLOCK, cmd_mlock, MAXPARA, CMD_SERVER);
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

/*
 * cmd_mode -- written by binary (garryb@binary.islesfan.net)
 * Completely rewrote it.  The old mode command was 820 lines of ICKY
 * coding, which is a complete waste, because I wrote it in 570 lines of
 * *decent* coding.  This is also easier to read, change, and fine-tune.  Plus,
 * everything isn't scattered; everything's grouped where it should be.
 *
 * parv[1] - channel
 */
CMD_FUNC(cmd_mode)
{
	long unsigned sendts = 0;
	Ban *ban;
	Channel *channel;

	/* Now, try to find the channel in question */
	if (parc > 1)
	{
		if (*parv[1] == '#')
		{
			channel = find_channel(parv[1]);
			if (!channel)
			{
				cmd_umode(client, recv_mtags, parc, parv);
				return;
			}
		} else
		{
			cmd_umode(client, recv_mtags, parc, parv);
			return;
		}
	} else
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "MODE");
		return;
	}

	if (MyConnect(client) && !valid_channelname(parv[1]))
	{
		sendnumeric(client, ERR_NOSUCHCHANNEL, parv[1]);
		return;
	}

	if (parc < 3)
	{
		*modebuf = *parabuf = '\0';

		modebuf[1] = '\0';
		channel_modes(client, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), channel, 0);
		sendnumeric(client, RPL_CHANNELMODEIS, channel->name, modebuf, parabuf);
		sendnumeric(client, RPL_CREATIONTIME, channel->name, channel->creationtime);
		return;
	}

	/* List mode request? Eg: "MODE #channel b" to list all bans */
	if (MyUser(client) && BadPtr(parv[3]) && list_mode_request(client, channel, parv[2]))
		return;

	opermode = 0;

#ifndef NO_OPEROVERRIDE
	if (IsUser(client) && !IsULine(client) && !is_chan_op(client, channel) &&
	    !is_half_op(client, channel) && ValidatePermissionsForPath("channel:override:mode",client,NULL,channel,NULL))
	{
		sendts = 0;
		opermode = 1;
		goto aftercheck;
	}

	if (IsUser(client) && !IsULine(client) && !is_chan_op(client, channel) &&
	    is_half_op(client, channel) && ValidatePermissionsForPath("channel:override:mode",client,NULL,channel,NULL))
	{
		opermode = 2;
		goto aftercheck;
	}
#endif

	if (parv[2] && (*parv[2] == '&'))
	{
		/* We don't do any bounce-mode handling anymore since UnrealIRCd 6 */
		return;
	}

	if (IsServer(client) && (sendts = atol(parv[parc - 1])) &&
	    !IsULine(client) && (sendts > channel->creationtime))
	{
		unreal_log(ULOG_INFO, "mode", "MODE_TS_IGNORED", client,
		           "MODE change ignored for $channel from $client: "
		           "timestamp mismatch, ours=$channel.creationtime, theirs=$their_ts",
		           log_data_channel("channel", channel),
		           log_data_integer("their_ts", sendts));
		return;
	}
	if (IsServer(client) && !sendts && *parv[parc - 1] != '0')
		sendts = -1;
	if (IsServer(client) && sendts != -1)
		parc--;		/* server supplied a time stamp, remove it now */

aftercheck:

	/* This is to prevent excess +<whatever> modes. -- Syzop */
	if (MyUser(client) && parv[2])
	{
		mode_cutoff(parv[2]);
		mode_cutoff2(client, channel, &parc, parv);
	}

	/* Filter out the unprivileged FIRST. *
	 * Now, we can actually do the mode.  */

	(void)do_mode(channel, client, recv_mtags, parc - 2, parv + 2, sendts, 0);
	/* After this don't touch 'channel' anymore, as permanent module may have destroyed the channel */
	opermode = 0; /* Important since sometimes forgotten. -- Syzop */
}

/** Cut off mode string (eg: +abcdfjkdsgfgs) at MAXMODEPARAMS modes.
 * @param s The mode string (modes only, no parameters)
 * @note Should only used on local clients
 * @author Syzop
 */
static void mode_cutoff(char *s)
{
unsigned short modesleft = MAXMODEPARAMS * 2; /* be generous... */

	for (; *s && modesleft; s++)
		if ((*s != '-') && (*s != '+'))
			modesleft--;
	*s = '\0';
}

/** Another mode cutoff routine - this one for the server-side
 * amplification/enlargement problem that happens with bans/exempts/invex
 * as explained in #2837. -- Syzop
 */
static void mode_cutoff2(Client *client, Channel *channel, int *parc_out, char *parv[])
{
	int len, i;
	int parc = *parc_out;

	if (parc-2 <= 3)
		return; /* Less than 3 mode parameters? Then we don't even have to check */

	/* Calculate length of MODE if it would go through fully as-is */
	/* :nick!user@host MODE #channel +something param1 param2 etc... */
	len = strlen(client->name) + strlen(client->user->username) + strlen(GetHost(client)) +
	      strlen(channel->name) + 11;

	len += strlen(parv[2]);

	if (*parv[2] != '+' && *parv[2] != '-')
		len++;

	for (i = 3; parv[i]; i++)
	{
		len += strlen(parv[i]) + 1; /* (+1 for the space character) */
		/* +4 is another potential amplification (per-param).
		 * If we were smart we would only check this for b/e/I and only for
		 * relevant cases (not for all extended), but this routine is dumb,
		 * so we just +4 for any case where the full mask is missing.
		 * It's better than assuming +4 for all cases, though...
		 */
		if (!match_simple("*!*@*", parv[i]))
			len += 4;
	}

	/* Now check if the result is acceptable... */
	if (len < 510)
		return; /* Ok, no problem there... */

	/* Ok, we have a potential problem...
	 * we just dump the last parameter... check how much space we saved...
	 * and try again if that did not help
	 */
	for (i = parc-1; parv[i] && (i > 3); i--)
	{
		len -= strlen(parv[i]);
		if (!match_simple("*!*@*", parv[i]))
			len -= 4; /* must adjust accordingly.. */
		parv[i] = NULL;
		(*parc_out)--;
		if (len < 510)
			break;
	}
	/* This may be reached if like the first parameter is really insane long..
	 * which is no problem, as other layers (eg: ban) takes care of that.
	 * We're done...
	 */
}

/* do_mode -- written by binary
 *	User or server is authorized to do the mode.  This takes care of
 * setting the mode and relaying it to other users and servers.
 */
void _do_mode(Channel *channel, Client *client, MessageTag *recv_mtags, int parc, char *parv[], time_t sendts, int samode)
{
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	int  pcount;
	char tschange = 0;
	MessageTag *mtags = NULL;

	new_message(client, recv_mtags, &mtags);

	/* IMPORTANT: if you return, don't forget to free mtags!! */

	/* Please keep the next 3 lines next to each other */
	samode_in_progress = samode;
	set_mode(channel, client, parc, parv, &pcount, pvar);
	samode_in_progress = 0;

	if (MyConnect(client))
		RunHook7(HOOKTYPE_PRE_LOCAL_CHANMODE, client, channel, mtags, modebuf, parabuf, sendts, samode);
	else
		RunHook7(HOOKTYPE_PRE_REMOTE_CHANMODE, client, channel, mtags, modebuf, parabuf, sendts, samode);

	if (IsServer(client))
	{
		if (sendts > 0)
		{
			if (IsInvalidChannelTS(sendts))
			{
				unreal_log(ULOG_WARNING, "mode", "MODE_INVALID_TIMESTAMP", NULL,
				           "MODE for channel $channel has invalid timestamp $send_timestamp (from $client.name on $client.user.servername)\n"
				           "Buffer: $modebuf $parabuf",
				           log_data_channel("channel", channel),
				           log_data_integer("send_timestamp", sendts),
				           log_data_string("modebuf", modebuf),
				           log_data_string("parabuf", parabuf));
				/* Yeah, so what to do in this case?
				 * Don't set channel->creationtime
				 * and assume merging.
				 */
				sendts = channel->creationtime;
			} else
			if (sendts < channel->creationtime)
			{
				/* Our timestamp is wrong or this is a new channel */
				tschange = 1;
				channel->creationtime = sendts;

			}
			if (sendts > channel->creationtime && channel->creationtime)
			{
				/* Their timestamp is wrong */
				sendts = channel->creationtime;
				sendto_one(client, NULL, ":%s MODE %s + %lld", me.name,
				    channel->name, (long long)channel->creationtime);
			}
		}
		if (sendts == -1)
			sendts = channel->creationtime;
	}

	if (tschange && empty_mode(modebuf))
	{
		/* Message from the other server is an empty mode, BUT they
		 * did change the channel->creationtime to an earlier TS
		 * (see above "Our timestamp is wrong or this is a new channel").
		 * We need to relay this MODE message to all other servers
		 * (all except from where it came from, client).
		 */
		sendto_server(client, 0, 0, NULL, ":%s MODE %s + %lld",
		              me.id, channel->name,
		              (long long)channel->creationtime);
		free_message_tags(mtags);
		/* Return here, as there isn't anything else to send */
		return;
	}

	/* opermode for twimodesystem --sts */
#ifndef NO_OPEROVERRIDE
	if ((opermode == 1) && IsUser(client))
	{
		mode_operoverride_msg(client, channel, modebuf, parabuf);

		sendts = 0;
	}
#endif

	/* If we have nothing to do, we can stop here. */
	if (empty_mode(modebuf))
	{
		free_message_tags(mtags);
		return;
	}

	if (IsUser(client) && samode && MyUser(client))
	{
		if (!sajoinmode)
		{
			char buf[512];
			snprintf(buf, sizeof(buf), "%s%s%s", modebuf, *parabuf ? " " : "", parabuf);
			unreal_log(ULOG_INFO, "samode", "SAMODE_COMMAND", client,
			           "Client $client used SAMODE $channel ($mode)",
			           log_data_channel("channel", channel),
			           log_data_string("mode", buf));
		}

		client = &me;
		sendts = 0;
	}

	sendto_channel(channel, client, NULL, 0, 0, SEND_LOCAL, mtags,
	               ":%s MODE %s %s %s",
	               client->name, channel->name, modebuf, parabuf);

	if (IsServer(client) && sendts != -1)
	{
		sendto_server(client, 0, 0, mtags,
		              ":%s MODE %s %s %s %lld",
		              client->id, channel->name,
		              modebuf, parabuf,
		              (long long)sendts);
	} else
	if (samode && IsMe(client))
	{
		/* SAMODE is a special case: always send a TS of 0 (omitting TS==desync) */
		sendto_server(client, 0, 0, mtags,
		              ":%s MODE %s %s %s 0",
		              client->id, channel->name,
		              modebuf, parabuf);
	} else
	{
		sendto_server(client, 0, 0, mtags,
		              ":%s MODE %s %s %s",
		              client->id, channel->name,
		              modebuf, parabuf);
		/* tell them it's not a timestamp, in case the last param is a number. */
	}

	if (MyConnect(client))
		RunHook7(HOOKTYPE_LOCAL_CHANMODE, client, channel, mtags, modebuf, parabuf, sendts, samode);
	else
		RunHook7(HOOKTYPE_REMOTE_CHANMODE, client, channel, mtags, modebuf, parabuf, sendts, samode);

	/* After this, don't touch 'channel' anymore! As permanent module may have destroyed the channel. */

	free_message_tags(mtags);

}
/* make_mode_str -- written by binary
 *	Reconstructs the mode string, to make it look clean.  mode_buf will
 *  contain the +x-y stuff, and the parabuf will contain the parameters.
 */
void make_mode_str(Channel *channel, Cmode_t oldem, int pcount,
    char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char *mode_buf, char *para_buf,
    size_t mode_buf_size, size_t para_buf_size)
{
	Cmode *cm;
	char tmpbuf[MODEBUFLEN+3], *tmpstr;
	char *x = mode_buf;
	int  what, cnt, z;
	int i;
	char *m;
	what = 0;

	*tmpbuf = '\0';
	*mode_buf = '\0';
	*para_buf = '\0';
	what = 0;

	/* + paramless extmodes... */
	for (cm=channelmodes; cm; cm = cm->next)
	{
		if (!cm->letter || cm->paracount)
			continue;
		/* have it now and didn't have it before? */
		if ((channel->mode.mode & cm->mode) &&
		    !(oldem & cm->mode))
		{
			if (what != MODE_ADD)
			{
				*x++ = '+';
				what = MODE_ADD;
			}
			*x++ = cm->letter;
		}
	}

	*x = '\0';

	/* - extmodes (both "param modes" and paramless don't have
	 * any params when unsetting... well, except one special type, that is (we skip those here)
	 */
	for (cm=channelmodes; cm; cm = cm->next)
	{
		if (!cm->letter || cm->unset_with_param)
			continue;
		/* don't have it now and did have it before */
		if (!(channel->mode.mode & cm->mode) &&
		    (oldem & cm->mode))
		{
			if (what != MODE_DEL)
			{
				*x++ = '-';
				what = MODE_DEL;
			}
			*x++ = cm->letter;
		}
	}

	*x = '\0';
	/* reconstruct bkov chain */
	for (cnt = 0; cnt < pcount; cnt++)
	{
		if ((*(pvar[cnt]) == '+') && what != MODE_ADD)
		{
			*x++ = '+';
			what = MODE_ADD;
		}
		if ((*(pvar[cnt]) == '-') && what != MODE_DEL)
		{
			*x++ = '-';
			what = MODE_DEL;
		}
		*x++ = *(pvar[cnt] + 1);
		tmpstr = &pvar[cnt][2];
		z = (MODEBUFLEN * MAXMODEPARAMS);
		m = para_buf;
		while ((*m)) { m++; }
		while ((*tmpstr) && ((m-para_buf) < z))
		{
			*m = *tmpstr;
			m++;
			tmpstr++;
		}
		*m++ = ' ';
		*m = '\0';
	}
	z = strlen(para_buf);
	if ((z > 0) && (para_buf[z - 1] == ' '))
		para_buf[z - 1] = '\0';
	*x = '\0';
	if (*mode_buf == '\0')
	{
		*mode_buf = '+';
		mode_buf++;
		*mode_buf = '\0';
		/* Don't send empty lines. */
	}
	return;
}

const char *mode_ban_handler(Client *client, Channel *channel, char *param, int what, int extbtype, Ban **banlist)
{
	const char *tmpstr;
	BanContext *b;

	tmpstr = clean_ban_mask(param, what, client, 0);
	if (BadPtr(tmpstr))
	{
		/* Invalid ban. See if we can send an error about that (only for extbans) */
		if (MyUser(client) && is_extended_ban(param))
		{
			const char *nextbanstr;
			Extban *extban = findmod_by_bantype(param, &nextbanstr);
			BanContext *b;

			b = safe_alloc(sizeof(BanContext));
			b->client = client;
			b->channel = channel;
			b->banstr = nextbanstr;
			b->is_ok_checktype = EXBCHK_PARAM;
			b->what = what;
			b->what2 = extbtype;
			if (extban && extban->is_ok)
				extban->is_ok(b);
			safe_free(b);
		}

		return NULL;
	}
	if (MyUser(client) && is_extended_ban(param))
	{
		/* extban: check access if needed */
		const char *nextbanstr;
		Extban *extban = findmod_by_bantype(tmpstr, &nextbanstr);
		if (extban)
		{
			if ((extbtype == EXBTYPE_INVEX) && !(extban->options & EXTBOPT_INVEX))
				return NULL; /* this extended ban type does not support INVEX */
			if (extban->is_ok)
			{
				BanContext *b = safe_alloc(sizeof(BanContext));
				b->client = client;
				b->channel = channel;
				b->what = what;
				b->what2 = extbtype;

				b->is_ok_checktype = EXBCHK_ACCESS;
				b->banstr = nextbanstr;
				if (!extban->is_ok(b))
				{
					if (ValidatePermissionsForPath("channel:override:mode:extban",client,NULL,channel,NULL))
					{
						/* TODO: send operoverride notice */
					} else {
						b->banstr = nextbanstr;
						b->is_ok_checktype = EXBCHK_ACCESS_ERR;
						extban->is_ok(b);
						safe_free(b);
						return NULL;
					}
				}
				b->banstr = nextbanstr;
				b->is_ok_checktype = EXBCHK_PARAM;
				if (!extban->is_ok(b))
				{
					safe_free(b);
					return NULL;
				}
				safe_free(b);
			}
		}
	}

	if ( (what == MODE_ADD && add_listmode(banlist, client, channel, tmpstr)) ||
	     (what == MODE_DEL && del_listmode(banlist, channel, tmpstr)))
	{
		return NULL;	/* already exists */
	}

	return tmpstr;
}

/** Write the result of a mode change.
 * This is used by do_mode_char_list_mode(), do_mode_char_member_mode()
 * and do_extmode_char().
 * The result is later used by make_mode_str() to create the
 * actual MODE line to be broadcasted to the channel and other servers.
 */
void do_mode_char_write(char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], u_int *pcount, u_int what, char modeletter, const char *str)
{
	/* Caller should have made sure there was room! */
	if (*pcount >= MAXMODEPARAMS)
#ifdef DEBUGMODE
		abort();
#else
		return;
#endif

	ircsnprintf(pvar[*pcount], MODEBUFLEN + 3,
	            "%c%c%s",
	            (what == MODE_ADD) ? '+' : '-',
	            modeletter,
	            str);
	(*pcount)++;
}

#ifdef PREFIX_AQ
#define is_xchanop(x) ((x & (CHFL_CHANOP|CHFL_CHANADMIN|CHFL_CHANOWNER)))
#else
#define is_xchanop(x) ((x & CHFL_CHANOP))
#endif

int do_mode_char_list_mode(Channel *channel, long modetype, char modechar, char *param,
                           u_int what, Client *client,
                           u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3],
                           long my_access)
{
	const char *tmpstr;

	switch (modetype)
	{
		case MODE_BAN:
			if (!(tmpstr = mode_ban_handler(client, channel, param, what, EXBTYPE_BAN, &channel->banlist)))
				break; /* rejected or duplicate */
			do_mode_char_write(pvar, pcount, what, modechar, tmpstr);
			break;
		case MODE_EXCEPT:
			if (!(tmpstr = mode_ban_handler(client, channel, param, what, EXBTYPE_EXCEPT, &channel->exlist)))
				break; /* rejected or duplicate */
			do_mode_char_write(pvar, pcount, what, modechar, tmpstr);
			break;
		case MODE_INVEX:
			if (!(tmpstr = mode_ban_handler(client, channel, param, what, EXBTYPE_INVEX, &channel->invexlist)))
				break; /* rejected or duplicate */
			do_mode_char_write(pvar, pcount, what, modechar, tmpstr);
			break;
	}
	return 1;
}

/** Called for [+-]vhoaq */
int do_mode_char_member_mode(Channel *channel, long modetype, char modechar, char *param,
                             u_int what, Client *client,
                             u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3],
                             long my_access)
{
	Member *member = NULL;
	Membership *membership = NULL;
	Client *target;
	unsigned int tmp = 0;
	int chasing = 0;
	Hook *h;

	if (modetype == MODE_CHANOWNER)
	{
		if (!IsULine(client) && !IsServer(client) && !is_chanowner(client, channel) && !samode_in_progress)
		{
			if (MyUser(client) && !op_can_override("channel:override:mode",client,channel,&modetype))
			{
				sendnumeric(client, ERR_CHANOWNPRIVNEEDED, channel->name);
				return 1;
			}
			if (!is_half_op(client, channel)) /* htrig will take care of halfop override notices */
				opermode = 1;
		}
	} else
	if (modetype == MODE_CHANADMIN)
	{
		/* not uline, not server, not chanowner, not an samode, not -a'ing yourself... */
		if (!IsULine(client) && !IsServer(client) && !is_chanowner(client, channel) && !samode_in_progress &&
		    !(param && (what == MODE_DEL) && (find_client(param, NULL) == client)))
		{
			if (MyUser(client) && !op_can_override("channel:override:mode",client,channel,&modetype))
			{
				sendnumeric(client, ERR_CHANOWNPRIVNEEDED, channel->name);
				return 1;
			}
			if (!is_half_op(client, channel)) /* htrig will take care of halfop override notices */
				opermode = 1;
		}
	}

	/* Halfop access check */
	if ((my_access & CHFL_HALFOP) && !is_xchanop(my_access) && !IsULine(client) &&
	    !op_can_override("channel:override:mode",client,channel,&modetype) && !samode_in_progress)
	{
		if (MyUser(client) && (modetype == MODE_HALFOP) && (what == MODE_DEL) &&
		    param && (find_client(param, NULL) == client))
		{
			/* halfop doing -h on self */
		} else
		if (MyUser(client) && (modetype != MODE_VOICE))
		{
			sendnumeric(client, ERR_NOTFORHALFOPS, modechar);
		}
	}

	if (!(target = find_chasing(client, param, &chasing)))
		return 1;

	if (!target->user)
		return 1;

	if (!(membership = find_membership_link(target->user->channel, channel)))
	{
		sendnumeric(client, ERR_USERNOTINCHANNEL, target->name, channel->name);
		return 1;
	}
	member = find_member_link(channel->members, target);
	if (!member)
	{
		/* should never happen */
		unreal_log(ULOG_ERROR, "mode", "BUG_FIND_MEMBER_LINK_FAILED", target,
			   "[BUG] Client $target.details on channel $channel: "
			   "found via find_membership_link() but NOT found via find_member_link(). "
			   "This should never happen! Please report on https://bugs.unrealircd.org/",
			   log_data_channel("channel", channel));
		return 1;
	}

	/* More access checks (unless server or ulined)... */
	if (!IsServer(client) && !IsULine(client))
	{
		/* This code checks permissions when removing a member mode (-vhoaq), it is quite... long... */
		if (what == MODE_DEL)
		{
			int ret = EX_ALLOW;
			char *badmode = NULL;

			for (h = Hooks[HOOKTYPE_MODE_DEOP]; h; h = h->next)
			{
				int n = (*(h->func.intfunc))(client, member->client, channel, what, modechar, my_access, &badmode);
				if (n == EX_DENY)
					ret = n;
				else if (n == EX_ALWAYS_DENY)
				{
					ret = n;
					return 1;
				}
			}

			if (ret == EX_ALWAYS_DENY)
			{
				if (MyUser(client) && badmode)
					sendto_one(client, NULL, "%s", badmode); /* send error message, if any */

				if (MyUser(client))
				return 1; /* stop processing this mode */
			}

			/* This probably should work but is completely untested (the operoverride stuff, I mean): */
			if (ret == EX_DENY)
			{
				if (!op_can_override("channel:override:mode:del",client,channel,&modetype))
				{
					if (badmode)
						sendto_one(client, NULL, "%s", badmode); /* send error message, if any */
					return 1; /* stop processing this mode */
				} else {
					opermode = 1;
				}
			}
		}

		/* This check not only prevents unprivileged users from doing a -q on chanowners,
		 * it also protects against -o/-h/-v on them.
		 */
		if (is_chanowner(member->client, channel)
		    && member->client != client
		    && !is_chanowner(client, channel) && !IsServer(client)
		    && !IsULine(client) && !opermode && !samode_in_progress && (what == MODE_DEL))
		{
			if (MyUser(client))
			{
				/* Need this !op_can_override() here again, even with the !opermode
				 * check a few lines up, all due to halfops. -- Syzop
				 */
				if (!op_can_override("channel:override:mode:del",client,channel,&modetype))
				{
					char errbuf[NICKLEN+30];
					ircsnprintf(errbuf, sizeof(errbuf), "%s is a channel owner", member->client->name);
					sendnumeric(client, ERR_CANNOTCHANGECHANMODE, modechar, errbuf);
					return 1;
				}
			} else {
				if (IsOper(client))
					opermode = 1;
			}
		}

		/* This check not only prevents unprivileged users from doing a -a on chanadmins,
		 * it also protects against -o/-h/-v on them.
		 */
		if (is_chanadmin(member->client, channel)
		    && member->client != client
		    && !is_chanowner(client, channel) && !IsServer(client) && !opermode && !samode_in_progress
		    && modetype != MODE_CHANOWNER && (what == MODE_DEL))
		{
			if (MyUser(client))
			{
				/* Need this !op_can_override() here again, even with the !opermode
				 * check a few lines up, all due to halfops. -- Syzop
				 */
				if (!op_can_override("channel:override:mode:del",client,channel,&modetype))
				{
					char errbuf[NICKLEN+30];
					ircsnprintf(errbuf, sizeof(errbuf), "%s is a channel admin", member->client->name);
					sendnumeric(client, ERR_CANNOTCHANGECHANMODE, modechar, errbuf);
					return 1;
				}
			} else {
				if (IsOper(client))
					opermode = 1;
			}
		}
	} // !IsServer() and !IsULine()

	/* Save current flags and set the new flag */
	tmp = member->flags;
	if (what == MODE_ADD)
		member->flags |= modetype;
	else
		member->flags &= ~modetype;

	/* If there was no change, then don't do the mode.
	 * Except if this came from services, then always set it explicitly,
	 * even if it was there.
	 */
	if ((tmp == member->flags) && !IsULine(client))
		return 1; /* already set */

	/* Make sure membership->flags and member->flags is the same */
	membership->flags = member->flags;
	do_mode_char_write(pvar, pcount, what, modechar, target->name);
	return 1;
}

/* Called for +vhoaq and +beI, simply delegates it to sub-handlers
 * (but first checks if there is a parameter present)
 */
int do_mode_char(Channel *channel, long modetype, char modechar, char *param,
                 u_int what, Client *client,
                 u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3],
                 long my_access)
{
	/* Check if there is a parameter present */
	if (!param || *pcount >= MAXMODEPARAMS)
		return 0;

	if ((modetype == MODE_BAN) || (modetype == MODE_EXCEPT) || (modetype == MODE_INVEX))
		return do_mode_char_list_mode(channel, modetype, modechar, param, what, client, pcount, pvar, my_access);

	return do_mode_char_member_mode(channel, modetype, modechar, param, what, client, pcount, pvar, my_access);
}

/** Check access and if granted, set the extended chanmode to the requested value in memory.
  * @returns amount of params eaten (0 or 1)
  */
int do_extmode_char(Channel *channel, Cmode *handler, char *param, u_int what,
                    Client *client, u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3])
{
	int paracnt = (what == MODE_ADD) ? handler->paracount : 0;
	char mode = handler->letter;
	int x;
	char *morphed;

	if ((what == MODE_DEL) && handler->unset_with_param)
		paracnt = 1; /* there's always an exception! */

	/* Expected a param and it isn't there? */
	if (paracnt && (!param || (*pcount >= MAXMODEPARAMS)))
		return 0;

	/* Prevent remote users from setting local channel modes */
	if (handler->local && !MyUser(client))
		return paracnt;

	if (MyUser(client))
	{
		x = handler->is_ok(client, channel, mode, param, EXCHK_ACCESS, what);
		if ((x == EX_ALWAYS_DENY) ||
		    ((x == EX_DENY) && !op_can_override("channel:override:mode:del",client,channel,handler) && !samode_in_progress))
		{
			handler->is_ok(client, channel, mode, param, EXCHK_ACCESS_ERR, what);
			return paracnt; /* Denied & error msg sent */
		}
		if (x == EX_DENY)
			opermode = 1; /* override in progress... */
	} else {
		/* remote user: we only need to check if we need to generate an operoverride msg */
		if (!IsULine(client) && IsUser(client) && op_can_override("channel:override:mode:del",client,channel,handler) &&
		    (handler->is_ok(client, channel, mode, param, EXCHK_ACCESS, what) != EX_ALLOW))
		{
			opermode = 1; /* override in progress... */
		}
	}

	/* Check for multiple changes in 1 command (like +y-y+y 1 2, or +yy 1 2). */
	for (x = 0; x < *pcount; x++)
	{
		if (pvar[x][1] == handler->letter)
		{
			/* this is different than the old chanmode system, coz:
			 * "mode #chan +kkL #a #b #c" will get "+kL #a #b" which is wrong :p.
			 * we do eat the parameter. -- Syzop
			 */
			return paracnt;
		}
	}

	/* w00t... a parameter mode */
	if (handler->paracount)
	{
		if (what == MODE_DEL)
		{
			if (!(channel->mode.mode & handler->mode))
				return paracnt; /* There's nothing to remove! */
			if (handler->unset_with_param)
			{
				/* Special extended channel mode requiring a parameter on unset.
				 * Any provided parameter is ok, the current one (that is set) will be used.
				 */
				do_mode_char_write(pvar, pcount, what, handler->letter, cm_getparameter(channel, handler->letter));
			} else {
				/* Normal extended channel mode: deleting is just -X, no parameter.
				 * Nothing needs to be done here.
				 */
			}
		} else {
			/* add: is the parameter ok? */
			if (handler->is_ok(client, channel, mode, param, EXCHK_PARAM, what) == FALSE)
				return paracnt; /* rejected by is_ok */

			morphed = handler->conv_param(param, client, channel);
			if (!morphed)
				return paracnt; /* rejected by conv_param */

			/* is it already set at the same value? if so, ignore it. */
			if (channel->mode.mode & handler->mode)
			{
				char *now, *requested;
				char flag = handler->letter;
				now = cm_getparameter(channel, flag);
				requested = handler->conv_param(param, client, channel);
				if (now && requested && !strcmp(now, requested))
					return paracnt; /* ignore... */
			}
			do_mode_char_write(pvar, pcount, what, handler->letter, handler->conv_param(param, client, channel));
			param = morphed; /* set param to converted parameter. */
		}
	}

	if (what == MODE_ADD)
	{	/* + */
		channel->mode.mode |= handler->mode;
		if (handler->paracount)
			cm_putparameter(channel, handler->letter, param);
		RunHook2(HOOKTYPE_MODECHAR_ADD, channel, (int)mode);
	} else
	{	/* - */
		channel->mode.mode &= ~(handler->mode);
		RunHook2(HOOKTYPE_MODECHAR_DEL, channel, (int)mode);
		if (handler->paracount)
			cm_freeparameter(channel, handler->letter);
	}
	return paracnt;
}

/** In 2003 I introduced PROTOCTL CHANMODES= so remote servers (and services)
 * could deal with unknown "parameter eating" channel modes, minimizing desyncs.
 * Now, in 2015, I finally added the code to deal with this. -- Syzop
 */
int paracount_for_chanmode_from_server(Client *client, u_int what, char mode)
{
	if (MyUser(client))
		return 0; /* no server, we have no idea, assume 0 paracount */

	if (!client->server)
	{
		/* If it's from a remote client then figure out from which "uplink" we
		 * received this MODE. The uplink is the directly-connected-server to us
		 * and may differ from the server the user is actually on. This is correct.
		 */
		if (!client->direction || !client->direction->server)
			return 0;
		client = client->direction;
	}

	if (client->server->features.chanmodes[0] && strchr(client->server->features.chanmodes[0], mode))
		return 1; /* 1 parameter for set, 1 parameter for unset */

	if (client->server->features.chanmodes[1] && strchr(client->server->features.chanmodes[1], mode))
		return 1; /* 1 parameter for set, 1 parameter for unset */

	if (client->server->features.chanmodes[2] && strchr(client->server->features.chanmodes[2], mode))
		return (what == MODE_ADD) ? 1 : 0; /* 1 parameter for set, no parameter for unset */

	if (client->server->features.chanmodes[3] && strchr(client->server->features.chanmodes[3], mode))
		return 0; /* no parameter for set, no parameter for unset */

	if (mode == '&')
		return 0; /* & indicates bounce, it is not an actual mode character */

	if (mode == 'F')
		return (what == MODE_ADD) ? 1 : 0; /* Future compatibility */

	/* If we end up here it means we have no idea if it is a parameter-eating or paramless
	 * channel mode. That's actually pretty bad. This shouldn't happen since CHANMODES=
	 * is sent since 2003 and the (often also required) EAUTH PROTOCTL is in there since 2010.
	 */
	unreal_log(ULOG_WARNING, "mode", "REMOTE_UNKNOWN_CHANNEL_MODE", client,
	           "Server $client sent us an unknown channel mode $what$mode_character!",
	           log_data_string("what", ((what == MODE_ADD) ? "+" : "-")),
	           log_data_char("mode_character", mode));

	return 0;
}

/** Quick way to find out parameter count for a channel mode.
 * Only for LOCAL mode requests. For remote modes use
 * paracount_for_chanmode_from_server() instead.
 */
int paracount_for_chanmode(u_int what, char mode)
{
	if (me.server->features.chanmodes[0] && strchr(me.server->features.chanmodes[0], mode))
		return 1; /* 1 parameter for set, 1 parameter for unset */

	if (me.server->features.chanmodes[1] && strchr(me.server->features.chanmodes[1], mode))
		return 1; /* 1 parameter for set, 1 parameter for unset */

	if (me.server->features.chanmodes[2] && strchr(me.server->features.chanmodes[2], mode))
		return (what == MODE_ADD) ? 1 : 0; /* 1 parameter for set, no parameter for unset */

	if (me.server->features.chanmodes[3] && strchr(me.server->features.chanmodes[3], mode))
		return 0; /* no parameter for set, no parameter for unset */

	/* Not found: */
	return 0;
}

/* set_mode
 *	written by binary
 */
void _set_mode(Channel *channel, Client *client, int parc, char *parv[], u_int *pcount,
               char pvar[MAXMODEPARAMS][MODEBUFLEN + 3])
{
	Cmode *cm = NULL;
	char *curchr;
	char *argument;
	u_int what = MODE_ADD;
	long modetype = 0;
	int paracount = 1;
#ifdef DEVELOP
	char *tmpo = NULL;
#endif
	CoreChannelModeTable *tab = &corechannelmodetable[0];
	CoreChannelModeTable foundat;
	int found = 0;
	int sent_mlock_warning = 0;
	unsigned int htrig = 0;
	int checkrestr = 0, warnrestr = 1;
	Cmode_t oldem;
	long my_access;
	paracount = 1;
	*pcount = 0;

	oldem = channel->mode.mode;
	if (RESTRICT_CHANNELMODES && !ValidatePermissionsForPath("immune:restrict-channelmodes",client,NULL,channel,NULL)) /* "cache" this */
		checkrestr = 1;

	/* Set access to the status we have */
	my_access = IsUser(client) ? get_access(client, channel) : 0;

	for (curchr = parv[0]; *curchr; curchr++)
	{
		switch (*curchr)
		{
			case '+':
				what = MODE_ADD;
				break;
			case '-':
				what = MODE_DEL;
				break;
			default:
				if (MyUser(client) && channel->mode_lock && strchr(channel->mode_lock, *curchr) != NULL)
				{
					if (!IsOper(client) || find_server(SERVICES_NAME, NULL) ||
					    !ValidatePermissionsForPath("channel:override:mlock",client,NULL,channel,NULL))
					{
						if (!sent_mlock_warning)
						{
							sendnumeric(client, ERR_MLOCKRESTRICTED, channel->name, *curchr, channel->mode_lock);
							sent_mlock_warning++;
						}
						continue;
					}
				}
				found = 0;
				tab = &corechannelmodetable[0];
				while ((tab->mode != 0x0) && found == 0)
				{
					if (tab->flag == *curchr)
					{
						found = 1;
						foundat = *tab;
					}
					tab++;
				}
				if (found == 1)
				{
					modetype = foundat.mode;
				} else {
					/* Maybe in extmodes */
					for (cm=channelmodes; cm; cm = cm->next)
					{
						if (cm->letter == *curchr)
						{
							found = 2;
							break;
						}
					}
				}
				if (found == 0) /* Mode char unknown */
				{
					if (!MyUser(client))
						paracount += paracount_for_chanmode_from_server(client, what, *curchr);
					else
						sendnumeric(client, ERR_UNKNOWNMODE, *curchr);
					break;
				}

				if (checkrestr && strchr(RESTRICT_CHANNELMODES, *curchr))
				{
					if (warnrestr)
					{
						sendnotice(client, "Setting/removing of channelmode(s) '%s' has been disabled.",
							RESTRICT_CHANNELMODES);
						warnrestr = 0;
					}
					paracount += paracount_for_chanmode(what, *curchr);
					break;
				}

				if (paracount < parc)
					argument = parv[paracount]; /* can still be NULL */
				else
					argument = NULL;

#ifndef NO_OPEROVERRIDE
				if (found == 1)
				{
					if ((Halfop_mode(modetype) == FALSE) && opermode == 2 && htrig != 1)
					{
						/* YUCK! */
						if ((foundat.flag == 'h') && argument && (find_person(argument, NULL) == client))
						{
							/* ircop with halfop doing a -h on himself. no warning. */
						} else {
							opermode = 0;
							htrig = 1;
						}
					}
				}
				else if (found == 2) {
					/* Extended mode: all override stuff is in do_extmode_char which will set
					 * opermode if appropriate. -- Syzop
					 */
				}
#endif /* !NO_OPEROVERRIDE */

				/* Not sure how useful this is, but I'll let it stay... */
				if (argument && strlen(argument) >= MODEBUFLEN)
					argument[MODEBUFLEN-1] = '\0';

				if (found == 1)
				{
					paracount += do_mode_char(channel, modetype, *curchr,
								  argument, what, client, pcount,
								  pvar, my_access);
				}
				else if (found == 2)
				{
					paracount += do_extmode_char(channel, cm, argument,
								     what, client, pcount, pvar);
				}
				break;
		} /* switch(*curchr) */
	} /* for loop through mode letters */

	make_mode_str(channel, oldem, *pcount, pvar, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf));

#ifndef NO_OPEROVERRIDE
	if ((htrig == 1) && IsUser(client))
	{
		mode_operoverride_msg(client, channel, modebuf, parabuf);
		htrig = 0;
		opermode = 0; /* stop double override notices... but is this ok??? -- Syzop */
	}
#endif
}

/*
 * cmd_umode() added 15/10/91 By Darren Reed.
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
CMD_FUNC(_cmd_umode)
{
	int i;
	char **p, *m;
	Client *acptr;
	int what, setsnomask = 0;
	long oldumodes = 0;
	int oldsnomasks = 0;
	/* (small note: keep 'what' as an int. -- Syzop). */
	short rpterror = 0, umode_restrict_err = 0, chk_restrict = 0, modex_err = 0;

	what = MODE_ADD;

	if (parc < 2)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "MODE");
		return;
	}

	if (!(acptr = find_person(parv[1], NULL)))
	{
		if (MyConnect(client))
		{
			sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
		}
		return;
	}
	if (acptr != client)
	{
		sendnumeric(client, ERR_USERSDONTMATCH);
		return;
	}

	if (parc < 3)
	{
		sendnumeric(client, RPL_UMODEIS, get_usermode_string(client));
		if (client->user->snomask)
			sendnumeric(client, RPL_SNOMASK, get_snomask_string(client));
		return;
	}

	userhost_save_current(client); /* save host, in case we do any +x/-x or similar */

	/* find flags already set for user */
	for (i = 0; i <= Usermode_highest; i++)
		if ((client->umodes & Usermode_Table[i].mode))
			oldumodes |= Usermode_Table[i].mode;

	for (i = 0; i <= Snomask_highest; i++)
		if ((client->user->snomask & Snomask_Table[i].mode))
			oldsnomasks |= Snomask_Table[i].mode;

	if (RESTRICT_USERMODES && MyUser(client) && !ValidatePermissionsForPath("immune:restrict-usermodes",client,NULL,NULL,NULL))
		chk_restrict = 1;

	if (MyConnect(client))
		setsnomask = client->user->snomask;
	/*
	 * parse mode change string(s)
	 */
	p = &parv[2];
	for (m = *p; *m; m++)
	{
		if (chk_restrict && strchr(RESTRICT_USERMODES, *m))
		{
			if (!umode_restrict_err)
			{
				sendnotice(client, "Setting/removing of usermode(s) '%s' has been disabled.",
					RESTRICT_USERMODES);
				umode_restrict_err = 1;
			}
			continue;
		}
		switch (*m)
		{
			case '+':
				what = MODE_ADD;
				break;
			case '-':
				what = MODE_DEL;
				break;
				/* we may not get these,
				 * but they shouldnt be in default
				 */
			case ' ':
			case '\t':
				break;
			case 's':
				if (what == MODE_DEL)
				{
					if (parc >= 4 && client->user->snomask)
					{
						set_snomask(client, parv[3]);
						if (client->user->snomask == 0)
							goto def;
						break;
					} else {
						set_snomask(client, NULL);
						goto def;
					}
				}
				if (what == MODE_ADD)
				{
					if (parc < 4)
						set_snomask(client, IsOper(client) ? SNO_DEFOPER : SNO_DEFUSER);
					else
						set_snomask(client, parv[3]);
					goto def;
				}
				break;
			case 'o':
			case 'O':
				if (IsQuarantined(client->direction))
				{
					unreal_log(ULOG_INFO, "mode", "OPER_KILLED_QUARANTINE", client,
					           "QUARANTINE: Oper $client.detail on server $client.user.servername killed, due to quarantine");
					sendto_server(NULL, 0, 0, NULL, ":%s KILL %s :Quarantined: no oper privileges allowed", me.id, client->name);
					exit_client(client, NULL, "Quarantined: no oper privileges allowed");
					return;
				}
				/* A local user trying to set himself +o/+O is denied here.
				 * A while later (outside this loop) it is handled as well (and +C, +N, etc too)
				 * but we need to take care here too because it might cause problems
				 * that's just asking for bugs! -- Syzop.
				 */
				if (MyUser(client) && (what == MODE_ADD)) /* Someone setting himself +o? Deny it. */
					break;
				goto def;
			case 't':
			case 'x':
				switch (UHOST_ALLOWED)
				{
				case UHALLOW_ALWAYS:
					goto def;
				case UHALLOW_NEVER:
					if (MyUser(client))
					{
						if (!modex_err)
						{
							sendnotice(client, "*** Setting %c%c is disabled",
								what == MODE_ADD ? '+' : '-', *m);
							modex_err = 1;
						}
						break;
					}
					goto def;
				case UHALLOW_NOCHANS:
					if (MyUser(client) && client->user->joined)
					{
						if (!modex_err)
						{
							sendnotice(client, "*** Setting %c%c can not be done while you are on channels",
								what == MODE_ADD ? '+' : '-', *m);
							modex_err = 1;
						}
						break;
					}
					goto def;
				case UHALLOW_REJOIN:
					/* Handled later */
					goto def;
				}
				break;
			default:
			def:
				for (i = 0; i <= Usermode_highest; i++)
				{
					if (*m == Usermode_Table[i].flag)
					{
						if (Usermode_Table[i].allowed)
						if (!Usermode_Table[i].allowed(client,what))
							break;
						if (what == MODE_ADD)
							client->umodes |= Usermode_Table[i].mode;
						else
							client->umodes &= ~Usermode_Table[i].mode;
						break;
					}
					else if (i == Usermode_highest && MyConnect(client) && !rpterror)
					{
						sendnumeric(client, ERR_UMODEUNKNOWNFLAG);
						rpterror = 1;
					}
				}
				break;
		} /* switch */
	} /* for */

	/* Don't let non-ircops set ircop-only modes or snomasks */
	if (!ValidatePermissionsForPath("self:opermodes",client,NULL,NULL,NULL))
	{
		if ((oldumodes & UMODE_OPER) && IsOper(client))
		{
			/* User is an oper but does not have the self:opermodes capability.
			 * This only happens for heavily restricted IRCOps.
			 * Fixes bug https://bugs.unrealircd.org/view.php?id=5130
			 */
			int i;

			/* MODES */
			for (i = 0; i <= Usermode_highest; i++)
			{
				if (!Usermode_Table[i].flag)
					continue;
				if (Usermode_Table[i].unset_on_deoper)
				{
					/* This is an oper mode. Is it set now and wasn't earlier?
					 * then it needs to be stripped, as setting it is not
					 * permitted.
					 */
					long m = Usermode_Table[i].mode;
					if ((client->umodes & m) && !(oldumodes & m))
						client->umodes &= ~Usermode_Table[i].mode; /* remove */
				}
			}

			/* SNOMASKS: user can delete existing but not add new ones */
			for (i = 0; i <= Snomask_highest; i++)
			{
				int sno = Snomask_Table[i].mode;

				if (!Snomask_Table[i].flag)
					continue;
				/* Is it set now and wasn't earlier? Then it
				 * needs to be stripped, as setting it is not
				 * permitted.
				 */
				if ((client->user->snomask & sno) && !(oldsnomasks & sno))
					client->user->snomask &= ~Snomask_Table[i].mode; /* remove */
			}
		} else {
			/* User isn't an ircop at all. The solution is simple: */
			remove_oper_privileges(client, 0);
		}
	}

	if (MyUser(client) && (client->umodes & UMODE_SECURE) && !IsSecure(client))
		client->umodes &= ~UMODE_SECURE;

	/* -x translates to -xt (if applicable) */
	if ((oldumodes & UMODE_HIDE) && !IsHidden(client))
		client->umodes &= ~UMODE_SETHOST;

	/* Vhost unset = unset some other data as well */
	if ((oldumodes & UMODE_SETHOST) && !IsSetHost(client))
	{
		swhois_delete(client, "vhost", "*", &me, NULL);
	}

	/* +x or -t+x */
	if ((IsHidden(client) && !(oldumodes & UMODE_HIDE)) ||
	    ((oldumodes & UMODE_SETHOST) && !IsSetHost(client) && IsHidden(client)))
	{
		if (!dontspread)
			sendto_server(client, PROTO_VHP, 0, NULL, ":%s SETHOST :%s",
				client->name, client->user->virthost);

		/* Set the vhost */
		safe_strdup(client->user->virthost, client->user->cloakedhost);

		/* Notify */
		userhost_changed(client);
		if (MyUser(client))
			sendnumeric(client, RPL_HOSTHIDDEN, client->user->virthost);
	}

	/* -x */
	if (!IsHidden(client) && (oldumodes & UMODE_HIDE))
	{
		/* (Re)create the cloaked virthost, because it will be used
		 * for ban-checking... free+recreate here because it could have
		 * been a vhost for example. -- Syzop
		 */
		safe_strdup(client->user->virthost, client->user->cloakedhost);

		/* Notify */
		userhost_changed(client);
		if (MyUser(client))
			sendnumeric(client, RPL_HOSTHIDDEN, client->user->realhost);
	}
	/*
	 * If I understand what this code is doing correctly...
	 * If the user WAS an operator and has now set themselves -o/-O
	 * then remove their access, d'oh!
	 * In order to allow opers to do stuff like go +o, +h, -o and
	 * remain +h, I moved this code below those checks. It should be
	 * O.K. The above code just does normal access flag checks. This
	 * only changes the operflag access level.  -Cabal95
	 */
	if ((oldumodes & UMODE_OPER) && !IsOper(client) && MyConnect(client))
	{
		list_del(&client->special_node);
		remove_oper_privileges(client, 0);
		RunHook2(HOOKTYPE_LOCAL_OPER, client, 0);
	}

	if (!(oldumodes & UMODE_OPER) && IsOper(client))
		irccounts.operators++;

	/* deal with opercounts and stuff */
	if ((oldumodes & UMODE_OPER) && !IsOper(client))
	{
		irccounts.operators--;
		VERIFY_OPERCOUNT(client, "umode1");
	} else /* YES this 'else' must be here, otherwise we can decrease twice. fixes opercount bug. */
	if (!(oldumodes & UMODE_HIDEOPER) && IsHideOper(client))
	{
		irccounts.operators--;
		VERIFY_OPERCOUNT(client, "umode2");
	}
	/* end of dealing with opercounts */

	if ((oldumodes & UMODE_HIDEOPER) && !IsHideOper(client))
	{
		irccounts.operators++;
	}
	if (!(oldumodes & UMODE_INVISIBLE) && IsInvisible(client))
		irccounts.invisible++;
	if ((oldumodes & UMODE_INVISIBLE) && !IsInvisible(client))
		irccounts.invisible--;

	if (MyConnect(client) && !IsOper(client))
		remove_oper_privileges(client, 0);

	/*
	 * compare new flags with old flags and send string which
	 * will cause servers to update correctly.
	 */
	if (oldumodes != client->umodes)
		RunHook3(HOOKTYPE_UMODE_CHANGE, client, oldumodes, client->umodes);
	if (dontspread == 0)
		send_umode_out(client, 1, oldumodes);

	if (MyConnect(client) && setsnomask != client->user->snomask)
		sendnumeric(client, RPL_SNOMASK, get_snomask_string(client));
}

CMD_FUNC(cmd_mlock)
{
	Channel *channel = NULL;
	time_t t;

	if ((parc < 3) || BadPtr(parv[2]))
		return;

	t = (time_t) atol(parv[1]);

	/* Now, try to find the channel in question */
	channel = find_channel(parv[2]);
	if (!channel)
		return;

	/* Senders' Channel t is higher, drop it. */
	if (t > channel->creationtime)
		return;

	if (IsServer(client))
		set_channel_mlock(client, channel, parv[3], TRUE);
}

void mode_operoverride_msg(Client *client, Channel *channel, char *modebuf, char *parabuf)
{
	char buf[1024];

	if (empty_mode(modebuf))
		return;

	/* Internally we have this distinction between modebuf and parabuf,
	 * but this makes little sense to maintain in JSON.
	 */
	snprintf(buf, sizeof(buf), "%s %s", modebuf, parabuf);

	unreal_log(ULOG_INFO, "operoverride", "OPEROVERRIDE_MODE", client,
		   "OperOverride: $client.details changed channel mode of $channel to: $channel_mode",
		   log_data_string("override_type", "mode"),
		   log_data_string("channel_mode", buf),
		   log_data_channel("channel", channel));
}

/** Send a mode list (+beI) to the user */
void send_list_mode(Client *client, Channel *channel, Ban *list, int list_numeric, int end_of_list_numeric)
{
	Ban *ban;

	if (!IsMember(client, channel) && !ValidatePermissionsForPath("channel:see:mode:remotebanlist",client,NULL,channel,NULL))
	{
		sendnumeric(client, ERR_NOTONCHANNEL, channel->name);
		return;
	}

	for (ban = list; ban; ban = ban->next)
		sendnumeric(client, list_numeric, channel->name, ban->banstr, ban->who, ban->when);

	sendnumeric(client, end_of_list_numeric, channel->name);
}

/** Send a user list (+a or +q) to the user - rarely used */
void send_user_list_mode(Client *client, Channel *channel, long flags, int list_numeric, int end_of_list_numeric)
{
	Member *member;

	if (!IsMember(client, channel) && !ValidatePermissionsForPath("channel:see:mode:remoteownerlist",client,NULL,channel,NULL))
	{
		sendnumeric(client, ERR_NOTONCHANNEL, channel->name);
		return;
	}

	for (member = channel->members; member; member = member->next)
	{
		if (member->flags & flags)
			sendnumeric(client, list_numeric, channel->name, member->client->name);
	}
	sendnumeric(client, end_of_list_numeric, channel->name);
}

/* Deal with information requests from local users, such as:
 * MODE #chan b    Show the ban list
 * MODE #chan e    Show the ban exemption list
 * MODE #chan I    Show the invite exception list
 * MODE #chan q    Show list of channel owners
 * MODE #chan a    Show list of channel admins
 * @returns 1 if processed as a mode list (please return),
 *          0 if not (continue with the MODE as it likely is a set request).
 */
int list_mode_request(Client *client, Channel *channel, char *req)
{
	if (strstr(req, "b"))
	{
		send_list_mode(client, channel, channel->banlist, RPL_BANLIST, RPL_ENDOFBANLIST);
		return 1;
	} else
	if (strstr(req, "e"))
	{
		send_list_mode(client, channel, channel->exlist, RPL_EXLIST, RPL_ENDOFEXLIST);
		return 1;
	} else
	if (strstr(req, "I"))
	{
		send_list_mode(client, channel, channel->invexlist, RPL_INVEXLIST, RPL_ENDOFINVEXLIST);
		return 1;
	} else
	if (strstr(req, "q"))
	{
		send_user_list_mode(client, channel, CHFL_CHANOWNER, RPL_QLIST, RPL_ENDOFQLIST);
		return 1;
	} else
	if (strstr(req, "a"))
	{
		send_user_list_mode(client, channel, CHFL_CHANADMIN, RPL_ALIST, RPL_ENDOFALIST);
		return 1;
	}
	return 0;
}

