/*
 *   Unreal Internet Relay Chat Daemon, src/modules/message.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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
const char *_StripColors(const char *text);
int ban_version(Client *client, const char *text);
CMD_FUNC(cmd_private);
CMD_FUNC(cmd_notice);
CMD_FUNC(cmd_tagmsg);
void cmd_message(Client *client, MessageTag *recv_mtags, int parc, const char *parv[], SendType sendtype);
int _can_send_to_channel(Client *client, Channel *channel, const char **msgtext, const char **errmsg, SendType sendtype);
int can_send_to_user(Client *client, Client *target, const char **msgtext, const char **errmsg, SendType sendtype);

/* Variables */
long CAP_MESSAGE_TAGS = 0; /**< Looked up at MOD_LOAD, may stay 0 if message-tags support is absent */

ModuleHeader MOD_HEADER
  = {
	"message",	/* Name of module */
	"6.0.2", /* Version */
	"private message and notice", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddConstString(modinfo->handle, EFUNC_STRIPCOLORS, _StripColors);
	EfunctionAdd(modinfo->handle, EFUNC_CAN_SEND_TO_CHANNEL, _can_send_to_channel);
	return MOD_SUCCESS;
}

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, "PRIVMSG", cmd_private, 2, CMD_USER|CMD_SERVER|CMD_RESETIDLE|CMD_VIRUS);
	CommandAdd(modinfo->handle, "NOTICE", cmd_notice, 2, CMD_USER|CMD_SERVER);
	CommandAdd(modinfo->handle, "TAGMSG", cmd_tagmsg, 1, CMD_USER|CMD_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD()
{
	CAP_MESSAGE_TAGS = ClientCapabilityBit("message-tags");

	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

#define CANPRIVMSG_CONTINUE		100
#define CANPRIVMSG_SEND			101
/** Check if PRIVMSG's are permitted from a person to another person.
 * client:	source client
 * target:	target client
 * sendtype:	One of SEND_TYPE_*
 * text:	Pointer to a pointer to a text [in, out]
 * cmd:		Pointer to a pointer which contains the command to use [in, out]
 */
int can_send_to_user(Client *client, Client *target, const char **msgtext, const char **errmsg, SendType sendtype)
{
	int ret;
	Hook *h;
	int n;
	static char errbuf[256];

	*errmsg = NULL;

	if (IsVirus(client))
	{
		ircsnprintf(errbuf, sizeof(errbuf), "You are only allowed to talk in '%s'", SPAMFILTER_VIRUSCHAN);
		*errmsg = errbuf;
		return 0;
	}

	if (MyUser(client) && target_limit_exceeded(client, target, target->name))
	{
		/* target_limit_exceeded() is an exception, in the sense that
		 * it will send a different numeric. So we don't set errmsg.
		 */
		return 0;
	}

	if (is_silenced(client, target))
	{
		RunHook(HOOKTYPE_SILENCED, client, target, sendtype);
		/* Silently discarded, no error message */
		return 0;
	}

	// Possible FIXME: make match_spamfilter also use errmsg, or via a wrapper? or use same numeric?
	if (MyUser(client))
	{
		int spamtype = (sendtype == SEND_TYPE_NOTICE ? SPAMF_USERNOTICE : SPAMF_USERMSG);
		const char *cmd = sendtype_to_cmd(sendtype);

		if (match_spamfilter(client, *msgtext, spamtype, cmd, target->name, 0, NULL))
			return 0;
	}

	n = HOOK_CONTINUE;
	for (h = Hooks[HOOKTYPE_CAN_SEND_TO_USER]; h; h = h->next)
	{
		n = (*(h->func.intfunc))(client, target, msgtext, errmsg, sendtype);
		if (n == HOOK_DENY)
		{
			if (!*errmsg)
			{
				unreal_log(ULOG_ERROR, "main", "BUG_CAN_SEND_TO_USER_NO_ERRMSG", client,
					   "[BUG] Module $module did not set errmsg!!!",
					   log_data_string("module", h->owner->header->name));
				abort();
			}
			return 0;
		}
		if (!*msgtext || !**msgtext)
		{
			if (sendtype != SEND_TYPE_TAGMSG)
				return 0;
			else
				*msgtext = "";
		}
	}

	return 1;
}

/** Check if user is allowed to send to a prefix (eg: @#channel).
 * @param client	The client (sender)
 * @param channel	The target channel
 * @param mode		The member mode to send to (eg: 'o')
 */
int can_send_to_member_mode(Client *client, Channel *channel, char mode)
{
	Membership *lp;

	if (op_can_override("channel:override:message:prefix",client,channel,NULL))
		return 1;

	lp = find_membership_link(client->user->channel, channel);

	/* Check if user is allowed to send. RULES:
	 * Need at least voice (+) in order to send to +,% or @
	 * Need at least ops (@) in order to send to & or ~
	 */
	if (!lp || !check_channel_access_membership(lp, "vhoaq"))
	{
		sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->name);
		return 0;
	}

#if 0
	if (!(prefix & PREFIX_OP) && ((prefix & PREFIX_OWNER) || (prefix & PREFIX_ADMIN)) &&
	    !check_channel_access_membership(lp, "oaq"))
	{
		sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->name);
		return 0;
	}
#endif

	return 1;
}

int has_client_mtags(MessageTag *mtags)
{
	MessageTag *m;

	for (m = mtags; m; m = m->next)
		if (*m->name == '+')
			return 1;
	return 0;
}

/* General message handler to users and channels. Used by PRIVMSG, NOTICE, etc.
 */
void cmd_message(Client *client, MessageTag *recv_mtags, int parc, const char *parv[], SendType sendtype)
{
	Client *target;
	Channel *channel;
	char targets[BUFSIZE];
	char *targetstr, *p, *p2, *pc;
	const char *text, *errmsg;
	int ret;
	int ntargets = 0;
	const char *cmd = sendtype_to_cmd(sendtype);
	int maxtargets = max_targets_for_command(cmd);
	Hook *h;
	MessageTag *mtags;
	int sendflags;

	/* Force a labeled-response, even if we don't send anything
	 * and the request was sent to other servers (which won't
	 * reply either :D).
	 */
	labeled_response_force = 1;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendnumeric(client, ERR_NORECIPIENT, cmd);
		return;
	}

	if ((sendtype != SEND_TYPE_TAGMSG) && (parc < 3 || *parv[2] == '\0'))
	{
		sendnumeric(client, ERR_NOTEXTTOSEND);
		return;
	}

	if (MyConnect(client))
		parv[1] = (char *)canonize(parv[1]);

	strlcpy(targets, parv[1], sizeof(targets));
	for (p = NULL, targetstr = strtoken(&p, targets, ","); targetstr; targetstr = strtoken(&p, NULL, ","))
	{
		if (MyUser(client) && (++ntargets > maxtargets))
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, targetstr, maxtargets, cmd);
			break;
		}

		/* The nicks "ircd" and "irc" are special (and reserved) */
		if (!strcasecmp(targetstr, "ircd") && MyUser(client))
			return;

		if (!strcasecmp(targetstr, "irc") && MyUser(client))
		{
			/* When ban version { } is enabled the IRCd sends a CTCP VERSION request
			 * from the "IRC" nick. So we need to handle CTCP VERSION replies to "IRC".
			 */
			if (!strncmp(parv[2], "\1VERSION ", 9))
				ban_version(client, parv[2] + 9);
			else if (!strncmp(parv[2], "\1SCRIPT ", 8))
				ban_version(client, parv[2] + 8);
			return;
		}

		p2 = strchr(targetstr, '#');

		/* Message to channel */
		if (p2 && (channel = find_channel(p2)))
		{
			char pfixchan[CHANNELLEN + 4];
			int replaced = 0;
			char member_modes_tmp[2];
			char *member_modes = NULL;
			if (p2 - targetstr > 0)
			{
				/* There is (posssibly) a prefix involved... */
				char prefix_tmp[32];
				char prefix;
				strlncpy(prefix_tmp, targetstr, sizeof(prefix_tmp), p2 - targetstr);
				prefix = lowest_ranking_prefix(prefix_tmp);
				if (prefix)
				{
					/* Rewrite the target. Eg: @&~#chan becomes @#chan */
					snprintf(pfixchan, sizeof(pfixchan), "%c%s", prefix, channel->name);
					targetstr = pfixchan;
					replaced = 1;
					/* And set 'member_modes' */
					member_modes_tmp[0] = prefix_to_mode(prefix);
					member_modes_tmp[1] = '\0';
					member_modes = member_modes_tmp;
					/* Oh, and some access check */
					if (MyUser(client) && !can_send_to_member_mode(client, channel, *member_modes))
						continue;
				}
			}
			if (!replaced)
			{
				/* Replace target so the privmsg always goes to the "official" channel name */
				strlcpy(pfixchan, channel->name, sizeof(pfixchan));
				targetstr = pfixchan;
			}

			if (IsVirus(client) && strcasecmp(channel->name, SPAMFILTER_VIRUSCHAN))
			{
				sendnotice(client, "You are only allowed to talk in '%s'", SPAMFILTER_VIRUSCHAN);
				continue;
			}

			text = parv[2];
			errmsg = NULL;
			if (MyUser(client) && !IsULine(client))
			{
				if (!can_send_to_channel(client, channel, &text, &errmsg, sendtype))
				{
					/* Send the error message, but only if:
					 * 1) The user has not been killed
					 * 2) It is not a NOTICE
					 */
					if (IsDead(client))
						return;
					if (!IsDead(client) && (sendtype != SEND_TYPE_NOTICE) && !BadPtr(errmsg))
						sendnumeric(client, ERR_CANNOTSENDTOCHAN, channel->name, errmsg, p2);
					continue; /* skip delivery to this target */
				}
			}
			mtags = NULL;
			sendflags = SEND_ALL;

			if (!strchr(CHANCMDPFX,parv[2][0]))
				sendflags |= SKIP_DEAF;

			if ((*parv[2] == '\001') && strncmp(&parv[2][1], "ACTION ", 7))
				sendflags |= SKIP_CTCP;

			if (MyUser(client))
			{
				int spamtype = (sendtype == SEND_TYPE_NOTICE ? SPAMF_CHANNOTICE : SPAMF_CHANMSG);

				if (match_spamfilter(client, text, spamtype, cmd, channel->name, 0, NULL))
					return;
			}

			new_message(client, recv_mtags, &mtags);

			RunHook(HOOKTYPE_PRE_CHANMSG, client, channel, mtags, text, sendtype);

			if (!text)
			{
				free_message_tags(mtags);
				continue;
			}

			if (sendtype != SEND_TYPE_TAGMSG)
			{
				/* PRIVMSG or NOTICE */
				sendto_channel(channel, client, client->direction,
					       member_modes, 0, sendflags, mtags,
					       ":%s %s %s :%s",
					       client->name, cmd, targetstr, text);
			} else {
				/* TAGMSG:
				 * Only send if the message includes any user message tags
				 * and if the 'message-tags' module is loaded.
				 * Do not allow empty and useless TAGMSG.
				 */
				if (!CAP_MESSAGE_TAGS || !has_client_mtags(mtags))
				{
					free_message_tags(mtags);
					continue;
				}
				sendto_channel(channel, client, client->direction,
					       member_modes, CAP_MESSAGE_TAGS, sendflags, mtags,
					       ":%s TAGMSG %s",
					       client->name, targetstr);
			}

			RunHook(HOOKTYPE_CHANMSG, client, channel, sendflags, member_modes, targetstr, mtags, text, sendtype);

			free_message_tags(mtags);

			continue;
		}
		else if (p2)
		{
			sendnumeric(client, ERR_NOSUCHNICK, p2);
			continue;
		}


		/* Message to $servermask */
		if (*targetstr == '$')
		{
			MessageTag *mtags = NULL;

			if (!ValidatePermissionsForPath("chat:notice:global", client, NULL, NULL, NULL))
			{
				/* Apparently no other IRCd does this, but I think it's confusing not to
				 * send an error message, especially with our new privilege system.
				 * Error message could be more descriptive perhaps.
				 */
				sendnumeric(client, ERR_NOPRIVILEGES);
				continue;
			}
			new_message(client, recv_mtags, &mtags);
			sendto_match_butone(IsServer(client->direction) ? client->direction : NULL,
			    client, targetstr + 1,
			    (*targetstr == '#') ? MATCH_HOST :
			    MATCH_SERVER,
			    mtags,
			    ":%s %s %s :%s", client->name, cmd, targetstr, parv[2]);
			free_message_tags(mtags);
			continue;
		}

		/* nickname addressed? */
		target = hash_find_nickatserver(targetstr, NULL);
		if (target)
		{
			const char *errmsg = NULL;
			text = parv[2];
			if (!can_send_to_user(client, target, &text, &errmsg, sendtype))
			{
				/* Message is discarded */
				if (IsDead(client))
					return;
				if ((sendtype != SEND_TYPE_NOTICE) && !BadPtr(errmsg))
					sendnumeric(client, ERR_CANTSENDTOUSER, target->name, errmsg);
			} else
			{
				/* We may send the message */
				MessageTag *mtags = NULL;

				/* Inform sender that recipient is away, if this is so */
				if ((sendtype == SEND_TYPE_PRIVMSG) && MyConnect(client) && target->user && target->user->away)
					sendnumeric(client, RPL_AWAY, target->name, target->user->away);

				new_message(client, recv_mtags, &mtags);
				if ((sendtype == SEND_TYPE_TAGMSG) && !has_client_mtags(mtags))
				{
					free_message_tags(mtags);
					continue;
				}
				labeled_response_inhibit = 1;
				if (MyUser(target))
				{
					/* Deliver to end-user */
					if (sendtype == SEND_TYPE_TAGMSG)
					{
						if (HasCapability(target, "message-tags"))
						{
							sendto_prefix_one(target, client, mtags, ":%s %s %s",
									  client->name, cmd, target->name);
						}
					} else {
						sendto_prefix_one(target, client, mtags, ":%s %s %s :%s",
								  client->name, cmd, target->name, text);
					}
				} else {
					/* Send to another server */
					if (sendtype == SEND_TYPE_TAGMSG)
					{
						sendto_prefix_one(target, client, mtags, ":%s %s %s",
								  client->id, cmd, target->id);
					} else {
						sendto_prefix_one(target, client, mtags, ":%s %s %s :%s",
								  client->id, cmd, target->id, text);
					}
				}
				labeled_response_inhibit = 0;
				RunHook(HOOKTYPE_USERMSG, client, target, mtags, text, sendtype);
				free_message_tags(mtags);
				continue;
			}
			continue; /* Message has been delivered or rejected, continue with next target */
		}

		/* If nick@server -and- the @server portion was set::services-server then send a special message */
		if (!target && SERVICES_NAME)
		{
			char *server = strchr(targetstr, '@');
			if (server && strncasecmp(server + 1, SERVICES_NAME, strlen(SERVICES_NAME)) == 0)
			{
				sendnumeric(client, ERR_SERVICESDOWN, targetstr);
				continue;
			}
		}

		/* nothing, nada, not anything found */
		sendnumeric(client, ERR_NOSUCHNICK, targetstr);
		continue;
	}
}

/*
** cmd_private
**	parv[1] = receiver list
**	parv[2] = message text
*/
CMD_FUNC(cmd_private)
{
	cmd_message(client, recv_mtags, parc, parv, SEND_TYPE_PRIVMSG);
}

/*
** cmd_notice
**	parv[1] = receiver list
**	parv[2] = notice text
*/
CMD_FUNC(cmd_notice)
{
	cmd_message(client, recv_mtags, parc, parv, SEND_TYPE_NOTICE);
}

/*
** cmd_tagmsg
**	parv[1] = receiver list
*/
CMD_FUNC(cmd_tagmsg)
{
	/* compatibility hack */
	parv[2] = "";
	parv[3] = NULL;
	cmd_message(client, recv_mtags, parc, parv, SEND_TYPE_TAGMSG);
}

/* Taken from xchat by Peter Zelezny
 * changed very slightly by codemastr
 * RGB color stripping support added -- codemastr
 *
 * NOTE: if you change/update/enhance StripColors() then consider changing
 *       the StripControlCodes() function as well (in misc.c) !!
 */
const char *_StripColors(const char *text)
{
	int i = 0, len = strlen(text), save_len=0;
	char nc = 0, col = 0, rgb = 0;
	const char *save_text=NULL;
	static char new_str[4096];

	while (len > 0) 
	{
		if ((col && isdigit(*text) && nc < 2) ||
		    ((col == 1) && (*text == ',') && isdigit(text[1]) && (nc > 0) && (nc < 3)))
		{
			nc++;
			if (*text == ',')
			{
				nc = 0;
				col++;
			}
		}
		/* Syntax for RGB is ^DHHHHHH where H is a hex digit.
		 * If < 6 hex digits are specified, the code is displayed
		 * as text
		 */
		else if ((rgb && isxdigit(*text) && nc < 6) || (rgb && *text == ',' && nc < 7))
		{
			nc++;
			if (*text == ',')
				nc = 0;
		}
		else 
		{
			if (col)
				col = 0;
			if (rgb)
			{
				if (nc != 6)
				{
					text = save_text+1;
					len = save_len-1;
					rgb = 0;
					continue;
				}
				rgb = 0;
			}
			if (*text == '\003') 
			{
				col = 1;
				nc = 0;
			}
			else if (*text == '\004')
			{
				save_text = text;
				save_len = len;
				rgb = 1;
				nc = 0;
			}
			else if (*text != '\026') /* (strip reverse too) */
			{
				new_str[i] = *text;
				i++;
			}
		}
		text++;
		len--;
	}
	new_str[i] = 0;
	if (new_str[0] == '\0')
		return NULL;
	return new_str;
}

/** Check ban version { } blocks, returns 1  if banned and  0 if not. */
int ban_version(Client *client, const char *text)
{
	int len;
	ConfigItem_ban *ban;
	char ctcp_reply[BUFSIZE];

	strlcpy(ctcp_reply, text, sizeof(ctcp_reply));
	len = strlen(ctcp_reply);
	if (!len)
		return 0;
	
	if (ctcp_reply[len-1] == '\1')
		ctcp_reply[len-1] = '\0'; /* remove CTCP REPLY terminator (ASCII 1) */

	if ((ban = find_ban(NULL, ctcp_reply, CONF_BAN_VERSION)))
	{
		if (find_tkl_exception(TKL_BAN_VERSION, client))
			return 0; /* we are exempt */

		return place_host_ban(client, ban->action, ban->reason, BAN_VERSION_TKL_TIME);
	}

	return 0;
}

/** Can user send a message to this channel?
 * @param client    The client
 * @param channel   The channel
 * @param msgtext   The message to send (MAY be changed, even if user is allowed to send)
 * @param errmsg    The error message (will be filled in)
 * @param sendtype  One of SEND_TYPE_*
 * @returns Returns 1 if the user is allowed to send, otherwise 0.
 * (note that this behavior was reversed in UnrealIRCd versions <5.x.
 */
int _can_send_to_channel(Client *client, Channel *channel, const char **msgtext, const char **errmsg, SendType sendtype)
{
	Membership *lp;
	int  member, i = 0;
	Hook *h;

	if (!MyUser(client))
		return 1;

	*errmsg = NULL;

	member = IsMember(client, channel);

	lp = find_membership_link(client->user->channel, channel);

	/* Modules can plug in as well */
	for (h = Hooks[HOOKTYPE_CAN_SEND_TO_CHANNEL]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(client, channel, lp, msgtext, errmsg, sendtype);
		if (i != HOOK_CONTINUE)
		{
			if (!*errmsg)
			{
				unreal_log(ULOG_ERROR, "main", "BUG_CAN_SEND_TO_CHANNEL_NO_ERRMSG", client,
					   "[BUG] Module $module did not set errmsg!!!",
					   log_data_string("module", h->owner->header->name));
				abort();
			}
			break;
		}
		if (!*msgtext || !**msgtext)
		{
			if (sendtype != SEND_TYPE_TAGMSG)
				return 0;
			else
				*msgtext = "";
		}
	}

	if (i != HOOK_CONTINUE)
	{
		if (!*errmsg)
			*errmsg = "You are banned";
		/* Don't send message if the user was previously a member
		 * and isn't anymore, so if the user is KICK'ed, eg by floodprot.
		 */
		if (member && !IsDead(client) && !find_membership_link(client->user->channel, channel))
			*errmsg = NULL;
		return 0;
	}

	/* Now we are going to check bans */

	/* ..but first: exempt ircops */
	if (op_can_override("channel:override:message:ban",client,channel,NULL))
		return 1;

	/* If local client is banned and not +vhoaq... */
	if (MyUser(client) &&
	    !check_channel_access_membership(lp, "vhoaq") &&
	    is_banned(client, channel, BANCHK_MSG, msgtext, errmsg))
	{
		/* Modules can set 'errmsg', otherwise we default to this: */
		if (!*errmsg)
			*errmsg = "You are banned";
		return 0;
	}

	return 1;
}
