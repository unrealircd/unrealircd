/*
 *   IRC - Internet Relay Chat, src/modules/sjoin.c
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

CMD_FUNC(cmd_sjoin);

#define MSG_SJOIN 	"SJOIN"	

ModuleHeader MOD_HEADER
  = {
	"sjoin",
	"5.1",
	"command /sjoin", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

char modebuf[BUFSIZE], parabuf[BUFSIZE];

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SJOIN, cmd_sjoin, MAXPARA, CMD_SERVER);
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

typedef struct xParv aParv;
struct xParv {
	int  parc;
	const char *parv[256];
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

static void send_local_chan_mode(MessageTag *recv_mtags, Client *client, Channel *channel, char *modebuf, char *parabuf)
{
	MessageTag *mtags = NULL;
	int destroy_channel = 0;

	new_message_special(client, recv_mtags, &mtags, ":%s MODE %s %s %s", client->name, channel->name, modebuf, parabuf);
	sendto_channel(channel, client, NULL, 0, 0, SEND_LOCAL, mtags,
	               ":%s MODE %s %s %s", client->name, channel->name, modebuf, parabuf);
	if (MyConnect(client))
		RunHook(HOOKTYPE_LOCAL_CHANMODE, client, channel, mtags, modebuf, parabuf, 0, -1, &destroy_channel);
	else
		RunHook(HOOKTYPE_REMOTE_CHANMODE, client, channel, mtags, modebuf, parabuf, 0, -1, &destroy_channel);
	free_message_tags(mtags);
}

/** Call send_local_chan_mode() for multiline modes */
static void send_local_chan_mode_mlm(MessageTag *recv_mtags, Client *client, Channel *channel, MultiLineMode *mlm)
{
	if (mlm)
	{
		int i;
		for (i = 0; i < mlm->numlines; i++)
			send_local_chan_mode(recv_mtags, client, channel, mlm->modeline[i], mlm->paramline[i]);
	}
}

/** SJOIN: Synchronize channel modes, +beI lists and users (server-to-server command)
 * Extensive technical documentation is available at:
 * https://www.unrealircd.org/docs/Server_protocol:SJOIN_command
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

/* Note: with regards to message tags we use new_message_special()
 *       here extensively. This because one SJOIN command can (often)
 *       generate multiple events that are sent to clients,
 *       for example 1 SJOIN can cause multiple joins, +beI, etc.
 *       -- Syzop
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
	send_local_chan_mode(recv_mtags, client, channel, modebuf, parabuf); \
	strcpy(parabuf,param);\
	/* modebuf[0] should stay what it was ('+' or '-') */ \
	modebuf[1] = mode;\
	modebuf[2] = '\0';\
	b = 2;\
}
#define Addsingle(x) do { modebuf[b] = x; b++; modebuf[b] = '\0'; } while(0)
#define CheckStatus(x,y) do { if (modeflags & (y)) { Addit((x), acptr->name); } } while(0)

CMD_FUNC(cmd_sjoin)
{
	unsigned short nopara;
	unsigned short nomode; /**< An SJOIN without MODE? */
	unsigned short removeours; /**< Remove our modes */
	unsigned short removetheirs; /**< Remove their modes (or actually: do not ADD their modes, the MODE -... line will be sent later by the other side) */
	unsigned short merge;	/**< same timestamp: merge their & our modes */
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	char cbuf[1024];
	char scratch_buf[1024]; /**< scratch buffer */
	char item[1024]; /**< nick or ban/invex/exempt being processed */
	char item_modes[MEMBERMODESLEN]; /**< item modes, eg "b" or "vhoaq" */
	char prefix[16]; /**< SJOIN prefix of item for server to server traffic (eg: @) */
	char uid_buf[BUFSIZE];  /**< Buffer for server-to-server traffic which will be broadcasted to others (servers supporting SID/UID) */
	char uid_sjsby_buf[BUFSIZE];  /**< Buffer for server-to-server traffic which will be broadcasted to others (servers supporting SID/UID and SJSBY) */
	char sj3_parabuf[BUFSIZE]; /**< Prefix for the above SJOIN buffers (":xxx SJOIN #channel +mode :") */
	char *s = NULL;
	Channel *channel; /**< Channel */
	aParv *ap;
	int pcount, i;
	Hook *h;
	Cmode *cm;
	time_t ts, oldts;
	unsigned short b=0;
	char *tp, *p, *saved = NULL;
	
	if (!IsServer(client) || parc < 4)
		return;

	if (!IsChannelName(parv[2]))
		return;

	merge = nopara = nomode = removeours = removetheirs = 0;

	if (parc < 6)
		nopara = 1;

	if (parc < 5)
		nomode = 1;

	channel = find_channel(parv[2]);
	if (!channel)
	{
		channel = make_channel(parv[2]);
		oldts = -1;
	} else {
		oldts = channel->creationtime;
	}

	ts = (time_t)atol(parv[1]);

	if (IsInvalidChannelTS(ts))
	{
		unreal_log(ULOG_WARNING, "sjoin", "SJOIN_INVALID_TIMESTAMP", client,
			   "SJOIN for channel $channel has invalid timestamp $send_timestamp (from $client)",
			   log_data_channel("channel", channel),
			   log_data_integer("send_timestamp", ts));
		/* Pretend they match our creation time (matches U6 behavior in m_mode.c) */
		ts = channel->creationtime;
	}

	if (oldts == -1)
	{
		/* Newly created channel (from our POV), so set the correct creationtime here */
		channel->creationtime = ts;
	} else
	if (channel->creationtime > ts)
	{
		removeours = 1;
		channel->creationtime = ts;
	}
	else if (channel->creationtime < ts)
	{
		removetheirs = 1;
	}
	else if (channel->creationtime == ts)
	{
		merge = 1;
	}

	parabuf[0] = '\0';
	modebuf[0] = '+';
	modebuf[1] = '\0';

	/* Grab current modes -> modebuf & parabuf */
	channel_modes(client, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), channel, 1);

	/* Do we need to remove all our modes, bans/exempt/inves lists and -vhoaq our users? */
	if (removeours)
	{
		Member *lp;

		modebuf[0] = '-';

		/* remove our modes if any */
		if (!empty_mode(modebuf))
		{
			MessageTag *mtags = NULL;
			MultiLineMode *mlm;
			ap = mp2parv(modebuf, parabuf);
			mlm = set_mode(channel, client, ap->parc, ap->parv, &pcount, pvar);
			send_local_chan_mode_mlm(recv_mtags, client, channel, mlm);
			safe_free_multilinemode(mlm);
		}
		/* remove bans */
		/* reset the buffers */
		modebuf[0] = '-';
		modebuf[1] = '\0';
		parabuf[0] = '\0';
		b = 1;
		while(channel->banlist)
		{
			Ban *ban = channel->banlist;
			Addit('b', ban->banstr);
			channel->banlist = ban->next;
			safe_free(ban->banstr);
			safe_free(ban->who);
			free_ban(ban);
		}
		while(channel->exlist)
		{
			Ban *ban = channel->exlist;
			Addit('e', ban->banstr);
			channel->exlist = ban->next;
			safe_free(ban->banstr);
			safe_free(ban->who);
			free_ban(ban);
		}
		while(channel->invexlist)
		{
			Ban *ban = channel->invexlist;
			Addit('I', ban->banstr);
			channel->invexlist = ban->next;
			safe_free(ban->banstr);
			safe_free(ban->who);
			free_ban(ban);
		}
		for (lp = channel->members; lp; lp = lp->next)
		{
			Membership *lp2 = find_membership_link(lp->client->user->channel, channel);

			/* Remove all our modes, one by one */
			for (p = lp->member_modes; *p; p++)
			{
				Addit(*p, lp->client->name);
			}
			/* And clear all the flags in memory */
			*lp->member_modes = *lp2->member_modes = '\0';
		}
		if (b > 1)
		{
			modebuf[b] = '\0';
			send_local_chan_mode(recv_mtags, client, channel, modebuf, parabuf);
		}

		/* since we're dropping our modes, we want to clear the mlock as well. --nenolod */
		set_channel_mlock(client, channel, NULL, FALSE);
	}
	/* Mode setting done :), now for our beloved clients */
	parabuf[0] = 0;
	modebuf[0] = '+';
	modebuf[1] = '\0';
	b = 1;
	strlcpy(cbuf, parv[parc-1], sizeof cbuf);

	sj3_parabuf[0] = '\0';
	for (i = 2; i <= (parc - 2); i++)
	{
		strlcat(sj3_parabuf, parv[i], sizeof sj3_parabuf);
		if (((i + 1) <= (parc - 2)))
			strlcat(sj3_parabuf, " ", sizeof sj3_parabuf);
	}

	/* Now process adding of users & adding of list modes (bans/exempt/invex) */

	snprintf(uid_buf, sizeof uid_buf, ":%s SJOIN %lld %s :", client->id, (long long)ts, sj3_parabuf);
	snprintf(uid_sjsby_buf, sizeof uid_sjsby_buf, ":%s SJOIN %lld %s :", client->id, (long long)ts, sj3_parabuf);

	for (s = strtoken(&saved, cbuf, " "); s; s = strtoken(&saved, NULL, " "))
	{
		char *setby = client->name; /**< Set by (nick, nick!user@host, or server name) */
		time_t setat = TStime(); /**< Set at timestamp */
		int sjsby_info = 0; /**< Set to 1 if we receive SJSBY info to alter the above 2 vars */

		*item_modes = 0;
		i = 0;
		tp = s;

		/* UnrealIRCd 4.2.2 and later support "SJSBY" which allows communicating
		 * setat/setby information for bans, ban exempts and invite exceptions.
		 */
		if (SupportSJSBY(client->direction) && (*tp == '<'))
		{
			/* Special prefix to communicate timestamp and setter:
			 * "<" + timestamp + "," + nick[!user@host] + ">" + normal SJOIN stuff
			 * For example: "<12345,nick>&some!nice@ban"
			 */
			char *end = strchr(tp, '>'), *p;
			if (!end)
			{
				/* this obviously should never happen */
				unreal_log(ULOG_WARNING, "sjoin", "SJOIN_INVALID_SJSBY", client,
					   "SJOIN for channel $channel has invalid SJSBY in item '$item' (from $client)",
					   log_data_channel("channel", channel),
					   log_data_string("item", s));
				continue;
			}
			*end++ = '\0';

			p = strchr(tp, ',');
			if (!p)
			{
				/* missing setby parameter */
				unreal_log(ULOG_WARNING, "sjoin", "SJOIN_INVALID_SJSBY", client,
					   "SJOIN for channel $channel has invalid SJSBY in item '$item' (from $client)",
					   log_data_channel("channel", channel),
					   log_data_string("item", s));
				continue;
			}
			*p++ = '\0';

			setat = atol(tp+1);
			setby = p;
			sjsby_info = 1;

			tp = end; /* the remainder is used for the actual ban/exempt/invex */
		}

		/* Process the SJOIN prefixes... */
		for (p = tp; *p; p++)
		{
			char m = sjoin_prefix_to_mode(*p);
			if (!m)
				break; /* end of prefix stuff, or so we hope anyway :D */
			// TODO: do we want safety here for if one side has prefixmodes loaded
			// and the other does not? and if so, in what way do we want this?

			strlcat_letter(item_modes, m, sizeof(item_modes));

			/* For list modes (+beI) stop processing immediately,
			 * so we don't accidentally eat additional prefix chars.
			 */
			if (strchr("beI", m))
			{
				p++;
				break;
			}
		}

		/* Now set 'prefix' to the prefixes we encountered.
		 * This is basically the range tp..p
		 */
		strlncpy(prefix, tp, sizeof(prefix), p - tp);

		/* Now copy the "nick" (which can actually be a ban/invex/exempt) */
		strlcpy(item, p, sizeof(item));
		if (*item == '\0')
			continue;

		/* If not a list mode... then we deal with users... */
		if (!strchr(item_modes, 'b') && !strchr(item_modes, 'e') && !strchr(item_modes, 'I'))
		{
			Client *acptr;

			/* The user may no longer exist. This can happen in case of a
			 * SVSKILL traveling in the other direction. Nothing to worry about.
			 */
			if (!(acptr = find_user(item, NULL)))
				continue;

			if (acptr->direction != client->direction)
			{
				if (IsMember(acptr, channel))
				{
					/* Nick collision, don't kick or it desyncs -Griever*/
					continue;
				}
			
				sendto_one(client, NULL,
				    ":%s KICK %s %s :Fake direction",
				    me.id, channel->name, acptr->name);
				unreal_log(ULOG_WARNING, "sjoin", "SJOIN_FAKE_DIRECTION", client,
				           "Fake direction from server $client in SJOIN "
				           "for user $existing_client on $existing_client.user.servername "
				           "(item: $buf)",
				           log_data_client("existing_client", acptr),
				           log_data_string("buf", item));
				continue;
			}

			if (removetheirs)
				*item_modes = '\0';

			if (!IsMember(acptr, channel))
			{
				/* User joining the channel, send JOIN to local users.
				 */
				MessageTag *mtags = NULL;

				add_user_to_channel(channel, acptr, item_modes);
				if (!(acptr->uplink && !IsSynched(acptr->uplink)))
				{
					unreal_log(ULOG_INFO, "join", "REMOTE_CLIENT_JOIN", acptr,
						   "User $client joined $channel",
						   log_data_channel("channel", channel),
						   log_data_string("modes", item_modes));
				}
				RunHook(HOOKTYPE_REMOTE_JOIN, acptr, channel, recv_mtags);
				new_message_special(acptr, recv_mtags, &mtags, ":%s JOIN %s", acptr->name, channel->name);
				send_join_to_local_users(acptr, channel, mtags);
				free_message_tags(mtags);
			}

			/* Set the +vhoaq */
			for (p = item_modes; *p; p++)
				Addit(*p, acptr->name);

			if (strlen(uid_buf) + strlen(prefix) + IDLEN > BUFSIZE - 10)
			{
				/* Send what we have and start a new buffer */
				sendto_server(client, 0, PROTO_SJSBY, recv_mtags, "%s", uid_buf);
				snprintf(uid_buf, sizeof(uid_buf), ":%s SJOIN %lld %s :", client->id, (long long)ts, channel->name);
				/* Double-check the new buffer is sufficient to concat the data */
				if (strlen(uid_buf) + strlen(prefix) + strlen(acptr->id) > BUFSIZE - 5)
				{
					unreal_log(ULOG_ERROR, "sjoin", "BUG_OVERSIZED_SJOIN", client,
					           "Oversized SJOIN [$sjoin_place] in channel $channel when adding '$str$str2' to '$buf'",
						   log_data_channel("channel", channel),
					           log_data_string("sjoin_place", "UID-MEMBER"),
					           log_data_string("str", prefix),
					           log_data_string("str2", acptr->id),
					           log_data_string("buf", uid_buf));
					continue;
				}
			}
			sprintf(uid_buf+strlen(uid_buf), "%s%s ", prefix, acptr->id);

			if (strlen(uid_sjsby_buf) + strlen(prefix) + IDLEN > BUFSIZE - 10)
			{
				/* Send what we have and start a new buffer */
				sendto_server(client, PROTO_SJSBY, 0, recv_mtags, "%s", uid_sjsby_buf);
				snprintf(uid_sjsby_buf, sizeof(uid_sjsby_buf), ":%s SJOIN %lld %s :", client->id, (long long)ts, channel->name);
				/* Double-check the new buffer is sufficient to concat the data */
				if (strlen(uid_sjsby_buf) + strlen(prefix) + strlen(acptr->id) > BUFSIZE - 5)
				{
					unreal_log(ULOG_ERROR, "sjoin", "BUG_OVERSIZED_SJOIN", client,
					           "Oversized SJOIN [$sjoin_place] in channel $channel when adding '$str$str2' to '$buf'",
						   log_data_channel("channel", channel),
					           log_data_string("sjoin_place", "SJS-MEMBER"),
					           log_data_string("str", prefix),
					           log_data_string("str2", acptr->id),
					           log_data_string("buf", uid_sjsby_buf));
					continue;
				}
			}
			sprintf(uid_sjsby_buf+strlen(uid_sjsby_buf), "%s%s ", prefix, acptr->id);
		}
		else
		{
			/* It's a list mode................ */
			const char *str;
			
			if (removetheirs)
				continue;

			/* Validate syntax */

			/* non-extbans: prevent bans without ! or @. a good case of "should never happen". */
			if ((item[0] != '~') && (!strchr(item, '!') || !strchr(item, '@') || (item[0] == '!')))
				continue;

			str = clean_ban_mask(item, MODE_ADD, client, 0);
			if (!str)
				continue; /* invalid ban syntax */
			strlcpy(item, str, sizeof(item));
			
			/* Adding of list modes */
			if (*item_modes == 'b')
			{
				if (add_listmode_ex(&channel->banlist, client, channel, item, setby, setat) == 1)
				{
					Addit('b', item);
				}
			}
			if (*item_modes == 'e')
			{
				if (add_listmode_ex(&channel->exlist, client, channel, item, setby, setat) == 1)
				{
					Addit('e', item);
				}
			}
			if (*item_modes == 'I')
			{
				if (add_listmode_ex(&channel->invexlist, client, channel, item, setby, setat) == 1)
				{
					Addit('I', item);
				}
			}

			if (strlen(uid_buf) + strlen(prefix) + strlen(item) > BUFSIZE - 10)
			{
				/* Send what we have and start a new buffer */
				sendto_server(client, 0, PROTO_SJSBY, recv_mtags, "%s", uid_buf);
				snprintf(uid_buf, sizeof(uid_buf), ":%s SJOIN %lld %s :", client->id, (long long)ts, channel->name);
				/* Double-check the new buffer is sufficient to concat the data */
				if (strlen(uid_buf) + strlen(prefix) + strlen(item) > BUFSIZE - 5)
				{
					unreal_log(ULOG_ERROR, "sjoin", "BUG_OVERSIZED_SJOIN", client,
					           "Oversized SJOIN [$sjoin_place] in channel $channel when adding '$str$str2' to '$buf'",
						   log_data_channel("channel", channel),
					           log_data_string("sjoin_place", "UID-LMODE"),
					           log_data_string("str", prefix),
					           log_data_string("str2", item),
					           log_data_string("buf", uid_buf));
					continue;
				}
			}
			sprintf(uid_buf+strlen(uid_buf), "%s%s ", prefix, item);

			*scratch_buf = '\0';
			if (sjsby_info)
				add_sjsby(scratch_buf, setby, setat);
			strcat(scratch_buf, prefix);
			strcat(scratch_buf, item);
			strcat(scratch_buf, " ");
			if (strlen(uid_sjsby_buf) + strlen(scratch_buf) > BUFSIZE - 10)
			{
				/* Send what we have and start a new buffer */
				sendto_server(client, PROTO_SJSBY, 0, recv_mtags, "%s", uid_sjsby_buf);
				snprintf(uid_sjsby_buf, sizeof(uid_sjsby_buf), ":%s SJOIN %lld %s :", client->id, (long long)ts, channel->name);
				/* Double-check the new buffer is sufficient to concat the data */
				if (strlen(uid_sjsby_buf) + strlen(scratch_buf) > BUFSIZE - 5)
				{
					unreal_log(ULOG_ERROR, "sjoin", "BUG_OVERSIZED_SJOIN", client,
					           "Oversized SJOIN [$sjoin_place] in channel $channel when adding '$str' to '$buf'",
						   log_data_channel("channel", channel),
					           log_data_string("sjoin_place", "SJS-LMODE"),
					           log_data_string("str", scratch_buf),
					           log_data_string("buf", uid_sjsby_buf));
					continue;
				}
			}
			strcpy(uid_sjsby_buf+strlen(uid_sjsby_buf), scratch_buf); /* size already checked above */
		}
		continue;
	}

	/* Send out any possible remainder.. */
	sendto_server(client, 0, PROTO_SJSBY, recv_mtags, "%s", uid_buf);
	sendto_server(client, PROTO_SJSBY, 0, recv_mtags, "%s", uid_sjsby_buf);

	if (!empty_mode(modebuf))
	{
		modebuf[b] = '\0';
		send_local_chan_mode(recv_mtags, client, channel, modebuf, parabuf);
	}
	
	if (!merge && !removetheirs && !nomode)
	{
		MessageTag *mtags = NULL;
		MultiLineMode *mlm;

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
		mlm = set_mode(channel, client, ap->parc, ap->parv, &pcount, pvar);
		send_local_chan_mode_mlm(recv_mtags, client, channel, mlm);
		safe_free_multilinemode(mlm);
	}

	if (merge && !nomode)
	{
		CoreChannelModeTable *acp;
		MultiLineMode *mlm;
		Mode oldmode; /**< The old mode (OUR mode) */

		/* Copy current mode to oldmode (need to duplicate all extended mode params too..) */
		memcpy(&oldmode, &channel->mode, sizeof(oldmode));
		memset(&oldmode.mode_params, 0, sizeof(oldmode.mode_params));
		extcmode_duplicate_paramlist(channel->mode.mode_params, oldmode.mode_params);

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

		/* First we set the mode (in memory) BUT we don't send the
		 * mode change out to anyone, hence the immediate freeing
		 * of 'mlm'. We do the actual rebuilding of the string and
		 * sending it out a few lines further down.
		 */
		ap = mp2parv(modebuf, parabuf);
		mlm = set_mode(channel, client, ap->parc, ap->parv, &pcount, pvar);
		safe_free_multilinemode(mlm);

		/* Good, now we got modes, now for the differencing and outputting of modes
		 * We first see if any para modes are set.
		 */
		strlcpy(modebuf, "-", sizeof modebuf);
		parabuf[0] = '\0';
		b = 1;

		/* Check if we had +s and it became +p, then revert it silently (as it is no-change) */
		if (has_channel_mode_raw(oldmode.mode, 's') && has_channel_mode(channel, 'p'))
		{
			/* stay +s ! */
			long mode_p = get_extmode_bitbychar('p');
			long mode_s = get_extmode_bitbychar('s');
			channel->mode.mode &= ~mode_p;
			channel->mode.mode |= mode_s;
			/* TODO: all the code of above would ideally be in a module */
		}
		/* (And the other condition, +p to +s, is already handled below by the generic code) */

		/* First, check if we had something that is now gone
		 * note that: oldmode.* = us, channel->mode.* = merged.
		 */
		for (cm=channelmodes; cm; cm = cm->next)
		{
			if (cm->letter &&
			    !cm->local &&
			    (oldmode.mode & cm->mode) &&
			    !(channel->mode.mode & cm->mode))
			{
				if (cm->paracount)
				{
					const char *parax = cm_getparameter_ex(oldmode.mode_params, cm->letter);
					//char *parax = cm->get_param(extcmode_get_struct(oldmode.modeparam, cm->letter));
					Addit(cm->letter, parax);
				} else {
					Addsingle(cm->letter);
				}
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

		/* Now, check if merged modes contain something we didn't have before.
		 * note that: oldmode.* = us before, channel->mode.* = merged.
		 *
		 * First the simple single letter modes...
		 */
		for (cm=channelmodes; cm; cm = cm->next)
		{
			if ((cm->letter) &&
			    !(oldmode.mode & cm->mode) &&
			    (channel->mode.mode & cm->mode))
			{
				if (cm->paracount)
				{
					const char *parax = cm_getparameter(channel, cm->letter);
					if (parax)
					{
						Addit(cm->letter, parax);
					}
				} else {
					Addsingle(cm->letter);
				}
			}
		}

		/* now, if we had diffent para modes - this loop really could be done better, but */

		/* Now, check for any param differences in extended channel modes..
		 * note that: oldmode.* = us before, channel->mode.* = merged.
		 * if we win: copy oldmode to channel mode, if they win: send the mode
		 */
		for (cm=channelmodes; cm; cm = cm->next)
		{
			if (cm->letter && cm->paracount &&
			    (oldmode.mode & cm->mode) &&
			    (channel->mode.mode & cm->mode))
			{
				int r;
				const char *parax;
				char flag = cm->letter;
				void *ourm = GETPARASTRUCTEX(oldmode.mode_params, flag);
				void *theirm = GETPARASTRUCT(channel, flag);
				
				r = cm->sjoin_check(channel, ourm, theirm);
				switch (r)
				{
					case EXSJ_WEWON:
						parax = cm_getparameter_ex(oldmode.mode_params, flag); /* grab from old */
						cm_putparameter(channel, flag, parax); /* put in new (won) */
						break;

					case EXSJ_THEYWON:
						parax = cm_getparameter(channel, flag);
						Addit(cm->letter, parax);
						break;

					case EXSJ_SAME:
						break;

					case EXSJ_MERGE:
						parax = cm_getparameter_ex(oldmode.mode_params, flag); /* grab from old */
						cm_putparameter(channel, flag, parax); /* put in new (won) */
						Addit(flag, parax);
						break;

					default:
						unreal_log(ULOG_ERROR, "sjoin", "BUG_SJOIN_CHECK", client,
						           "[BUG] channel.c:m_sjoin:param diff checker: unknown return value $return_value",
						           log_data_integer("return_value", r));
						break;
				}
			}
		}

		Addsingle('\0');

		if (!empty_mode(modebuf))
			send_local_chan_mode(recv_mtags, client, channel, modebuf, parabuf);

		/* free the oldmode.* crap :( */
		extcmode_free_paramlist(oldmode.mode_params);
	}

	for (h = Hooks[HOOKTYPE_CHANNEL_SYNCED]; h; h = h->next)
	{
		int i = (*(h->func.intfunc))(channel,merge,removetheirs,nomode);
		if (i == 1)
			return; /* channel no longer exists */
	}

	/* we should be synced by now, */
	if ((oldts != -1) && (oldts != channel->creationtime))
	{
		unreal_log(ULOG_INFO, "channel", "CHANNEL_SYNC_TS_CHANGE", client,
		           "Channel $channel: timestamp changed from $old_ts -> $new_ts "
		           "after syncing with server $client.",
		           log_data_channel("channel", channel),
		           log_data_integer("old_ts", oldts),
		           log_data_integer("new_ts", channel->creationtime));
	}

	/* If something went wrong with processing of the SJOIN above and
	 * the channel actually has no users in it at this point,
	 * then destroy the channel.
	 */
	if (!channel->users)
	{
		sub1_from_channel(channel);
		return;
	}
}
