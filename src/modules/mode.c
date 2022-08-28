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

ModuleHeader MOD_HEADER
  = {
	"mode",
	"5.0",
	"command /mode",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
int list_mode_request(Client *client, Channel *channel, const char *req);
CMD_FUNC(cmd_mode);
CMD_FUNC(cmd_mlock);
void _do_mode(Channel *channel, Client *client, MessageTag *recv_mtags, int parc, const char *parv[], time_t sendts, int samode);
MultiLineMode *_set_mode(Channel *channel, Client *client, int parc, const char *parv[], u_int *pcount,
                       char pvar[MAXMODEPARAMS][MODEBUFLEN + 3]);
void _set_channel_mode(Channel *channel, char *modes, char *parameters);
CMD_FUNC(_cmd_umode);

/* local: */
int do_mode_char(Channel *channel, long modetype, char modechar, const char *param,
                 u_int what, Client *client,
                 u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3]);
int do_extmode_char(Channel *channel, Cmode *handler, const char *param, u_int what,
                    Client *client, u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3]);
void do_mode_char_member_mode_new(Channel *channel, Cmode *handler, const char *param, u_int what,
                    Client *client, u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3]);
MultiLineMode *make_mode_str(Client *client, Channel *channel, Cmode_t oldem, int pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3]);

static char *mode_cutoff(const char *s);
void mode_operoverride_msg(Client *client, Channel *channel, char *modebuf, char *parabuf);

static int samode_in_progress = 0;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_DO_MODE, _do_mode);
	EfunctionAddPVoid(modinfo->handle, EFUNC_SET_MODE, TO_PVOIDFUNC(_set_mode));
	EfunctionAddVoid(modinfo->handle, EFUNC_CMD_UMODE, _cmd_umode);
	EfunctionAddVoid(modinfo->handle, EFUNC_SET_CHANNEL_MODE, _set_channel_mode);
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
				CALL_CMD_FUNC(cmd_umode);
				return;
			}
		} else
		{
			CALL_CMD_FUNC(cmd_umode);
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
		char modebuf[BUFSIZE], parabuf[BUFSIZE];
		*modebuf = *parabuf = '\0';

		modebuf[1] = '\0';
		channel_modes(client, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), channel, 0);
		sendnumeric(client, RPL_CHANNELMODEIS, channel->name, modebuf, parabuf);
		sendnumeric(client, RPL_CREATIONTIME, channel->name, (long long)channel->creationtime);
		return;
	}

	/* List mode request? Eg: "MODE #channel b" to list all bans */
	if (MyUser(client) && BadPtr(parv[3]) && list_mode_request(client, channel, parv[2]))
		return;

	opermode = 0;

#ifndef NO_OPEROVERRIDE
	if (IsUser(client) && !IsULine(client) && !check_channel_access(client, channel, "oaq") &&
	    !check_channel_access(client, channel, "h") && ValidatePermissionsForPath("channel:override:mode",client,NULL,channel,NULL))
	{
		sendts = 0;
		opermode = 1;
		goto aftercheck;
	}

	if (IsUser(client) && !IsULine(client) && !check_channel_access(client, channel, "oaq") &&
	    check_channel_access(client, channel, "h") && ValidatePermissionsForPath("channel:override:mode",client,NULL,channel,NULL))
	{
		opermode = 2;
		goto aftercheck;
	}
#endif

	/* User does not have permission to use the MODE command */
	if (MyUser(client) && !IsULine(client) && !check_channel_access(client, channel, "hoaq") &&
	    !ValidatePermissionsForPath("channel:override:mode",client,NULL,channel,NULL))
	{
		sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->name);
		return;
	}

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
		parv[2] = mode_cutoff(parv[2]);
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
 * @returns The cleaned up string
 */
static char *mode_cutoff(const char *i)
{
	static char newmodebuf[BUFSIZE];
	char *o;
	unsigned short modesleft = MAXMODEPARAMS * 2; /* be generous... */

	strlcpy(newmodebuf, i, sizeof(newmodebuf));

	for (o = newmodebuf; *o && modesleft; o++)
		if ((*o != '-') && (*o != '+'))
			modesleft--;
	*o = '\0';
	return newmodebuf;
}

/* do_mode -- written by binary
 *	User or server is authorized to do the mode.  This takes care of
 * setting the mode and relaying it to other users and servers.
 */
void _do_mode(Channel *channel, Client *client, MessageTag *recv_mtags, int parc, const char *parv[], time_t sendts, int samode)
{
	Client *orig_client = client; /* (needed for samode replacement in a loop) */
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	int  pcount;
	int i;
	char tschange = 0;
	MultiLineMode *m;

	/* Please keep the next 3 lines next to each other */
	samode_in_progress = samode;
	m = set_mode(channel, client, parc, parv, &pcount, pvar);
	samode_in_progress = 0;

	if (IsServer(client))
	{
		if (sendts > 0)
		{
			if (IsInvalidChannelTS(sendts))
			{
				unreal_log(ULOG_WARNING, "mode", "MODE_INVALID_TIMESTAMP", client,
				           "MODE for channel $channel has invalid timestamp $send_timestamp (from $client.name)\n"
				           "Buffer: $modebuf $parabuf",
				           log_data_channel("channel", channel),
				           log_data_integer("send_timestamp", sendts),
				           log_data_string("modebuf", m?m->modeline[0]:""),
				           log_data_string("parabuf", m?m->modeline[0]:""));
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

	if (!m)
	{
		/* No modes changed (empty mode change) */
		if (tschange && !m)
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
		}
		/* Nothing to send */
		safe_free_multilinemode(m);
		opermode = 0;
		return;
	}

	/* Now loop through the multiline modes... */
	for (i = 0; i < m->numlines; i++)
	{
		char *modebuf = m->modeline[i];
		char *parabuf = m->paramline[i];
		MessageTag *mtags = NULL;
		int should_destroy = 0;

		if (m->numlines == 1)
		{
			/* Single mode lines are easy: retain original msgid etc */
			new_message(client, recv_mtags, &mtags);
		} else {
			/* We have a multi-mode line:
			 * This only happens when the input was a single mode line
			 * that got expanded into a multi mode line due to expansion
			 * issues. The sender could be a local client, but could also
			 * be a remote server like UnrealIRCd 5.
			 * We can't use the same msgid multiple times, and (if the
			 * sender was a server) then we can't use the original msgid
			 * either, not for both events and not for the first event
			 * (since the modeline differs for all events, including first).
			 * Obviously message ids must be unique for the event...
			 * So here is our special version again, just like we use in
			 * SJOIN and elsewhere sporadically for cases like this:
			 */
			new_message_special(client, recv_mtags, &mtags, ":%s MODE %s %s %s", client->name, channel->name, modebuf, parabuf);
		}

		/* IMPORTANT: if you return, don't forget to free mtags!! */

		if (MyConnect(client))
			RunHook(HOOKTYPE_PRE_LOCAL_CHANMODE, client, channel, mtags, modebuf, parabuf, sendts, samode);
		else
			RunHook(HOOKTYPE_PRE_REMOTE_CHANMODE, client, channel, mtags, modebuf, parabuf, sendts, samode);

		/* opermode for twimodesystem --sts */
#ifndef NO_OPEROVERRIDE
		if ((opermode == 1) && IsUser(client))
		{
			mode_operoverride_msg(client, channel, modebuf, parabuf);

			sendts = 0;
		}
#endif

		if (IsUser(orig_client) && samode && MyUser(orig_client))
		{
			if (!sajoinmode)
			{
				char buf[512];
				snprintf(buf, sizeof(buf), "%s%s%s", modebuf, *parabuf ? " " : "", parabuf);
				unreal_log(ULOG_INFO, "samode", "SAMODE_COMMAND", orig_client,
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

		if (IsServer(client) || IsMe(client))
		{
			sendto_server(client, 0, 0, mtags,
				      ":%s MODE %s %s %s %lld",
				      client->id, channel->name,
				      modebuf, parabuf,
				      (sendts != -1) ? (long long)sendts : 0LL);
		} else
		{
			sendto_server(client, 0, 0, mtags,
				      ":%s MODE %s %s %s",
				      client->id, channel->name,
				      modebuf, parabuf);
		}

		if (MyConnect(client))
			RunHook(HOOKTYPE_LOCAL_CHANMODE, client, channel, mtags, modebuf, parabuf, sendts, samode, &should_destroy);
		else
			RunHook(HOOKTYPE_REMOTE_CHANMODE, client, channel, mtags, modebuf, parabuf, sendts, samode, &should_destroy);

		free_message_tags(mtags);

		if (should_destroy)
			break; /* eg channel went -P with nobody in it. 'channel' is freed now */
	}
	safe_free_multilinemode(m);
	opermode = 0;
}

/* make_mode_str -- written by binary
 *	Reconstructs the mode string, to make it look clean.  mode_buf will
 *  contain the +x-y stuff, and the parabuf will contain the parameters.
 */
MultiLineMode *make_mode_str(Client *client, Channel *channel, Cmode_t oldem, int pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3])
{
	Cmode *cm;
	int what;
	int cnt, z, i;
	MultiLineMode *m = safe_alloc(sizeof(MultiLineMode));
	int curr = 0;
	int initial_len;

	if (client->user)
		initial_len = strlen(client->name) + strlen(client->user->username) + strlen(GetHost(client)) + strlen(channel->name) + 11;
	else
		initial_len = strlen(client->name) + strlen(channel->name) + 11;

	/* Reserve room for the first element */
	curr = 0;
	m->modeline[curr] = safe_alloc(BUFSIZE);
	m->paramline[curr] = safe_alloc(BUFSIZE);
	m->numlines = curr+1;
	what = 0;

	/* The first element will be filled with all paramless modes.
	 * That is: both the ones that got set, and the ones that got unset.
	 * This will always fit.
	 */

	/* Which paramless modes got set? Eg +snt */
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
				strlcat_letter(m->modeline[curr], '+', BUFSIZE);
				what = MODE_ADD;
			}
			strlcat_letter(m->modeline[curr], cm->letter, BUFSIZE);
		}
	}

	/* Which paramless modes got unset? Eg -r */
	for (cm=channelmodes; cm; cm = cm->next)
	{
		if (!cm->letter || cm->unset_with_param)
			continue;
		/* don't have it now and did have it before */
		if (!(channel->mode.mode & cm->mode) && (oldem & cm->mode))
		{
			if (what != MODE_DEL)
			{
				strlcat_letter(m->modeline[curr], '-', BUFSIZE);
				what = MODE_DEL;
			}
			strlcat_letter(m->modeline[curr], cm->letter, BUFSIZE);
		}
	}

	/* Now for parameter modes we do both addition and removal. Eg +b-e ban!x@y exempt!z@z */
	for (cnt = 0; cnt < pcount; cnt++)
	{
		if ((strlen(m->modeline[curr]) + strlen(m->paramline[curr]) + strlen(&pvar[cnt][2])) > 507)
		{
			if (curr == MAXMULTILINEMODES)
			{
				/* Should be impossible.. */
				unreal_log(ULOG_ERROR, "mode", "MODE_MULTILINE_EXCEEDED", client,
				           "A mode string caused an avalanche effect of more than $max_multiline_modes modes "
				           "in channel $channel. Caused by client $client. Expect a desync.",
				           log_data_integer("max_multiline_modes", MAXMULTILINEMODES),
				           log_data_channel("channel", channel));
				break;
			}
			curr++;
			m->modeline[curr] = safe_alloc(BUFSIZE);
			m->paramline[curr] = safe_alloc(BUFSIZE);
			m->numlines = curr+1;
			what = 0;
		}
		if ((*(pvar[cnt]) == '+') && what != MODE_ADD)
		{
			strlcat_letter(m->modeline[curr], '+', BUFSIZE);
			what = MODE_ADD;
		}
		if ((*(pvar[cnt]) == '-') && what != MODE_DEL)
		{
			strlcat_letter(m->modeline[curr], '-', BUFSIZE);
			what = MODE_DEL;
		}
		strlcat_letter(m->modeline[curr], *(pvar[cnt] + 1), BUFSIZE);
		strlcat(m->paramline[curr], &pvar[cnt][2], BUFSIZE);
		strlcat_letter(m->paramline[curr], ' ', BUFSIZE);
	}

	for (i = 0; i <= curr; i++)
	{
		char *para_buf = m->paramline[i];
		/* Strip off useless space character (' ') at the end, if there is any */
		z = strlen(para_buf);
		if ((z > 0) && (para_buf[z - 1] == ' '))
			para_buf[z - 1] = '\0';
	}

	/* Now check for completely empty mode: */
	if ((curr == 0) && empty_mode(m->modeline[0]))
	{
		/* And convert it to a NULL result */
		safe_free_multilinemode(m);
		return NULL;
	}

	return m;
}

const char *mode_ban_handler(Client *client, Channel *channel, const char *param, int what, int extbtype, Ban **banlist)
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
			b->is_ok_check = EXBCHK_PARAM;
			b->what = what;
			b->ban_type = extbtype;
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
				b->ban_type = extbtype;

				b->is_ok_check = EXBCHK_ACCESS;
				b->banstr = nextbanstr;
				if (!extban->is_ok(b))
				{
					if (ValidatePermissionsForPath("channel:override:mode:extban",client,NULL,channel,NULL))
					{
						/* TODO: send operoverride notice */
					} else {
						b->banstr = nextbanstr;
						b->is_ok_check = EXBCHK_ACCESS_ERR;
						extban->is_ok(b);
						safe_free(b);
						return NULL;
					}
				}
				b->banstr = nextbanstr;
				b->is_ok_check = EXBCHK_PARAM;
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

int do_mode_char_list_mode(Channel *channel, long modetype, char modechar, const char *param,
                           u_int what, Client *client,
                           u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3])
{
	const char *tmpstr;

	/* Check if there is a parameter present */
	if (!param || *pcount >= MAXMODEPARAMS)
		return 0;

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

/** Check access and if granted, set the extended chanmode to the requested value in memory.
  * @returns amount of params eaten (0 or 1)
  */
int do_extmode_char(Channel *channel, Cmode *handler, const char *param, u_int what,
                    Client *client, u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3])
{
	int paracnt = (what == MODE_ADD) ? handler->paracount : 0;
	char mode = handler->letter;
	int x;
	const char *morphed;

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
		if ((x == EX_DENY) && !samode_in_progress)
			opermode = 1; /* override in progress... */
	} else {
		/* remote user: we only need to check if we need to generate an operoverride msg */
		if (!IsULine(client) && IsUser(client) && op_can_override("channel:override:mode:del",client,channel,handler) &&
		    (handler->is_ok(client, channel, mode, param, EXCHK_ACCESS, what) != EX_ALLOW))
		{
			opermode = 1; /* override in progress... */
		}
	}

	if (handler->type == CMODE_MEMBER)
	{
		do_mode_char_member_mode_new(channel, handler, param, what, client, pcount, pvar);
		return 1;
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
				const char *now, *requested;
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
		RunHook(HOOKTYPE_MODECHAR_ADD, channel, (int)mode);
	} else
	{	/* - */
		channel->mode.mode &= ~(handler->mode);
		RunHook(HOOKTYPE_MODECHAR_DEL, channel, (int)mode);
		if (handler->paracount)
			cm_freeparameter(channel, handler->letter);
	}
	return paracnt;
}

/** Set or unset a mode on a member (eg +vhoaq/-vhoaq) */
void do_mode_char_member_mode_new(Channel *channel, Cmode *handler, const char *param, u_int what,
                    Client *client, u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3])
{
	Member *member = NULL;
	Membership *membership = NULL;
	Client *target;
	int chasing = 0;
	Hook *h;
	char c[2];
	char modechar = handler->letter;

	if (!(target = find_chasing(client, param, &chasing)))
		return;

	if (!target->user)
		return;

	if (!(membership = find_membership_link(target->user->channel, channel)))
	{
		sendnumeric(client, ERR_USERNOTINCHANNEL, target->name, channel->name);
		return;
	}
	member = find_member_link(channel->members, target);
	if (!member)
	{
		/* should never happen */
		unreal_log(ULOG_ERROR, "mode", "BUG_FIND_MEMBER_LINK_FAILED", target,
			   "[BUG] Client $target.details on channel $channel: "
			   "found via find_membership_link() but NOT found via find_member_link(). "
			   "This should never happen! Please report on https://bugs.unrealircd.org/",
			   log_data_client("target", target),
			   log_data_channel("channel", channel));
		return;
	}

	if ((what == MODE_ADD) && strchr(member->member_modes, modechar))
		return; /* already set */
	if ((what == MODE_DEL) && !strchr(member->member_modes, modechar))
		return; /* already unset */

	/* HOOKTYPE_MODE_DEOP code */
	if (what == MODE_DEL)
	{
		int ret = EX_ALLOW;
		const char *badmode = NULL;
		Hook *h;
		const char *my_access;
		Membership *my_membership;

		/* Set "my_access" to access flags of the requestor */
		if (IsUser(client) && (my_membership = find_membership_link(client->user->channel, channel)))
			my_access = my_membership->member_modes; /* client */
		else
			my_access = ""; /* server */

		for (h = Hooks[HOOKTYPE_MODE_DEOP]; h; h = h->next)
		{
			int n = (*(h->func.intfunc))(client, target, channel, what, modechar, my_access, member->member_modes, &badmode);
			if (n == EX_DENY)
			{
				ret = n;
			} else
			if (n == EX_ALWAYS_DENY)
			{
				ret = n;
				break;
			}
		}

		if (ret == EX_ALWAYS_DENY)
		{
			if (MyUser(client) && badmode)
				sendto_one(client, NULL, "%s", badmode); /* send error message, if any */

			if (MyUser(client))
				return; /* stop processing this mode */
		}

		/* This probably should work but is completely untested (the operoverride stuff, I mean): */
		if (ret == EX_DENY)
		{
			if (!op_can_override("channel:override:mode:del",client,channel,handler))
			{
				if (badmode)
					sendto_one(client, NULL, "%s", badmode); /* send error message, if any */
				return; /* stop processing this mode */
			} else {
				opermode = 1;
			}
		}
	}

	if (what == MODE_ADD)
	{
		if (strchr(member->member_modes, modechar))
			return; /* already set */
		/* Set the mode */
		add_member_mode_fast(member, membership, modechar);
	} else {
		if (!strchr(member->member_modes, modechar))
			return; /* already unset */
		del_member_mode_fast(member, membership, modechar);
	}

	/* And write out the mode */
	do_mode_char_write(pvar, pcount, what, modechar, target->name);
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

MultiLineMode *_set_mode(Channel *channel, Client *client, int parc, const char *parv[], u_int *pcount,
                        char pvar[MAXMODEPARAMS][MODEBUFLEN + 3])
{
	Cmode *cm = NULL;
	MultiLineMode *mlm = NULL;
	const char *curchr;
	const char *argument;
	char argumentbuf[MODEBUFLEN+1];
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
	int checkrestr = 0, warnrestr = 1;
	Cmode_t oldem;
	paracount = 1;
	*pcount = 0;

	oldem = channel->mode.mode;
	if (RESTRICT_CHANNELMODES && !ValidatePermissionsForPath("immune:restrict-channelmodes",client,NULL,channel,NULL)) /* "cache" this */
		checkrestr = 1;

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

				if ((paracount < parc) && parv[paracount])
				{
					strlcpy(argumentbuf, parv[paracount], sizeof(argumentbuf));
					argument = argumentbuf;
				} else {
					argument = NULL;
				}

				if (found == 1)
					paracount += do_mode_char_list_mode(channel, modetype, *curchr, argument, what, client, pcount, pvar);
				else if (found == 2)
					paracount += do_extmode_char(channel, cm, argument, what, client, pcount, pvar);
				break;
		} /* switch(*curchr) */
	} /* for loop through mode letters */

	mlm = make_mode_str(client, channel, oldem, *pcount, pvar);
	return mlm;
}

/*
 * cmd_umode() added 15/10/91 By Darren Reed.
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
CMD_FUNC(_cmd_umode)
{
	Umode *um;
	const char *m;
	Client *acptr;
	int what;
	long oldumodes = 0;
	char oldsnomask[64];
	/* (small note: keep 'what' as an int. -- Syzop). */
	short rpterror = 0, umode_restrict_err = 0, chk_restrict = 0, modex_err = 0;

	what = MODE_ADD;
	*oldsnomask = '\0';

	if (parc < 2)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "MODE");
		return;
	}

	if (!(acptr = find_user(parv[1], NULL)))
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
			sendnumeric(client, RPL_SNOMASK, client->user->snomask);
		return;
	}

	userhost_save_current(client); /* save host, in case we do any +x/-x or similar */

	oldumodes = client->umodes;

	if (RESTRICT_USERMODES && MyUser(client) && !ValidatePermissionsForPath("immune:restrict-usermodes",client,NULL,NULL,NULL))
		chk_restrict = 1;

	if (client->user->snomask)
		strlcpy(oldsnomask, client->user->snomask, sizeof(oldsnomask));

	/*
	 * parse mode change string(s)
	 */
	for (m = parv[2]; *m; m++)
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
						if (client->user->snomask == NULL)
							goto def;
						break;
					} else {
						set_snomask(client, NULL);
						goto def;
					}
				}
				if ((what == MODE_ADD) && IsOper(client))
				{
					if (parc < 4)
						set_snomask(client, OPER_SNOMASKS);
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
					           "QUARANTINE: Oper $client.details on server $client.user.servername killed, due to quarantine");
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
				/* set::anti-flood::vhost-flood */
				if (MyUser(client))
				{
					if ((what == MODE_DEL) && !ValidatePermissionsForPath("immune:vhost-flood",client,NULL,NULL,NULL) &&
							flood_limit_exceeded(client, FLD_VHOST))
					{
						/* Throttle... */
						if (!modex_err)
						{
							sendnotice(client, "*** Setting -%c too fast. Please try again later.", *m);
							modex_err = 1;
						}
						break;
					}
				}

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
				for (um = usermodes; um; um = um->next)
				{
					if (um->letter == *m)
					{
						if (um->allowed && !um->allowed(client,what))
							break;
						if (what == MODE_ADD)
							client->umodes |= um->mode;
						else
							client->umodes &= ~um->mode;
						break;
					}
				}
				if (!um && MyConnect(client) && !rpterror)
				{
					sendnumeric(client, ERR_UMODEUNKNOWNFLAG);
					rpterror = 1;
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
			for (um = usermodes; um; um = um->next)
			{
				if (um->unset_on_deoper)
				{
					/* This is an oper mode. Is it set now and wasn't earlier?
					 * then it needs to be stripped, as setting it is not
					 * permitted.
					 */
					if ((client->umodes & um->mode) && !(oldumodes & um->mode))
						client->umodes &= ~um->mode; /* remove */
				}
			}

			/* SNOMASKS: user can delete existing but not add new ones */
			if (client->user->snomask)
			{
				char rerun;
				do {
					char *p;

					rerun = 0;
					for (p = client->user->snomask; *p; p++)
					{
						if (!strchr(oldsnomask, *p))
						{
							/* It is set now, but was not earlier?
							 * Then it needs to be stripped, as setting is not permitted.
							 * And re-run the loop
							 */
							delletterfromstring(client->user->snomask, *p);
							rerun = 1;
							break;
						}
					}
				} while(rerun);
				/* And make sure an empty snomask ("") becomes a NULL pointer */
				if (client->user->snomask && !*client->user->snomask)
					remove_all_snomasks(client);
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
		if (MyUser(client))
			RunHook(HOOKTYPE_LOCAL_OPER, client, 0, NULL, NULL);
		remove_oper_privileges(client, 0);
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
		RunHook(HOOKTYPE_UMODE_CHANGE, client, oldumodes, client->umodes);
	if (dontspread == 0)
		send_umode_out(client, 1, oldumodes);

	if (MyConnect(client) && client->user->snomask && strcmp(oldsnomask, client->user->snomask))
		sendnumeric(client, RPL_SNOMASK, client->user->snomask);
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

/* Deal with information requests from local users, such as:
 * MODE #chan b    Show the ban list
 * MODE #chan e    Show the ban exemption list
 * MODE #chan I    Show the invite exception list
 * MODE #chan q    Show list of channel owners
 * MODE #chan a    Show list of channel admins
 * @returns 1 if processed as a mode list (please return),
 *          0 if not (continue with the MODE as it likely is a set request).
 */
int list_mode_request(Client *client, Channel *channel, const char *req)
{
	const char *p;
	Ban *ban;
	Member *member;

	for (p = req; *p; p++)
		if (strchr("beIqa", *p))
			break;

	if (!*p)
		return 0; /* not handled, proceed with the MODE set attempt */

	/* First, check access */
	if (strchr("beI", *p))
	{
		if (!IsMember(client, channel) && !ValidatePermissionsForPath("channel:see:mode:remotebanlist",client,NULL,channel,NULL))
		{
			sendnumeric(client, ERR_NOTONCHANNEL, channel->name);
			return 1; /* handled */
		}
	} else {
		if (!IsMember(client, channel) && !ValidatePermissionsForPath("channel:see:mode:remoteownerlist",client,NULL,channel,NULL))
		{
			sendnumeric(client, ERR_NOTONCHANNEL, channel->name);
			return 1; /* handled */
		}
	}

	switch(*p)
	{
		case 'b':
			for (ban = channel->banlist; ban; ban = ban->next)
				sendnumeric(client, RPL_BANLIST, channel->name, ban->banstr, ban->who, (long long)ban->when);
			sendnumeric(client, RPL_ENDOFBANLIST, channel->name);
			break;
		case 'e':
			for (ban = channel->exlist; ban; ban = ban->next)
				sendnumeric(client, RPL_EXLIST, channel->name, ban->banstr, ban->who, (long long)ban->when);
			sendnumeric(client, RPL_ENDOFEXLIST, channel->name);
			break;
		case 'I':
			for (ban = channel->invexlist; ban; ban = ban->next)
				sendnumeric(client, RPL_INVEXLIST, channel->name, ban->banstr, ban->who, (long long)ban->when);
			sendnumeric(client, RPL_ENDOFINVEXLIST, channel->name);
			break;
		case 'q':
			for (member = channel->members; member; member = member->next)
				if (strchr(member->member_modes, 'q'))
					sendnumeric(client, RPL_QLIST, channel->name, member->client->name);
			sendnumeric(client, RPL_ENDOFQLIST, channel->name);
			break;
		case 'a':
			for (member = channel->members; member; member = member->next)
				if (strchr(member->member_modes, 'a'))
					sendnumeric(client, RPL_ALIST, channel->name, member->client->name);
			sendnumeric(client, RPL_ENDOFALIST, channel->name);
			break;
	}

	return 1; /* handled */
}

void _set_channel_mode(Channel *channel, char *modes, char *parameters)
{
	char buf[512];
	char *p, *param;
	int myparc = 1, i;
	char *myparv[512];

	memset(&myparv, 0, sizeof(myparv));
	myparv[0] = raw_strdup(modes);

	strlcpy(buf, parameters, sizeof(buf));
	for (param = strtoken(&p, buf, " "); param; param = strtoken(&p, NULL, " "))
		myparv[myparc++] = raw_strdup(param);
	myparv[myparc] = NULL;

	SetULine(&me); // hack for crash.. set ulined so no access checks.
	do_mode(channel, &me, NULL, myparc, (const char **)myparv, 0, 0);
	ClearULine(&me); // and clear it again..

	for (i = 0; i < myparc; i++)
		safe_free(myparv[i]);
}
