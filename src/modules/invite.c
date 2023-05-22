/*
 *   IRC - Internet Relay Chat, src/modules/invite.c
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

#define MSG_INVITE 	"INVITE"

#define CLIENT_INVITES(client)		(moddata_local_client(client, userInvitesMD).ptr)
#define CHANNEL_INVITES(channel)	(moddata_channel(channel, channelInvitesMD).ptr)

ModDataInfo *userInvitesMD;
ModDataInfo *channelInvitesMD;
long CAP_INVITE_NOTIFY = 0L;
int invite_always_notify = 0;

CMD_FUNC(cmd_invite);

void invite_free(ModData *md);
int invite_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int invite_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
void add_invite(Client *from, Client *to, Channel *channel, MessageTag *mtags);
void del_invite(Client *client, Channel *channel);
static int invite_channel_destroy(Channel *channel, int *should_destroy);
int invite_user_quit(Client *client, MessageTag *mtags, const char *comment);
int invite_user_join(Client *client, Channel *channel, MessageTag *mtags);
int invite_is_invited(Client *client, Channel *channel, int *invited);

ModuleHeader MOD_HEADER
  = {
	"invite",
	"5.0",
	"command /invite", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, invite_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	CommandAdd(modinfo->handle, MSG_INVITE, cmd_invite, MAXPARA, CMD_USER|CMD_SERVER);	

	memset(&cap, 0, sizeof(cap));
	cap.name = "invite-notify";
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_INVITE_NOTIFY);
	if (!c)
	{
		config_error("[%s] Failed to request invite-notify cap: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}

	memset(&mreq, 0 , sizeof(mreq));
	mreq.type = MODDATATYPE_LOCAL_CLIENT;
	mreq.name = "invite",
	mreq.free = invite_free;
	userInvitesMD = ModDataAdd(modinfo->handle, mreq);
	if (!userInvitesMD)
	{
		config_error("[%s] Failed to request user invite moddata: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	
	memset(&mreq, 0 , sizeof(mreq));
	mreq.type = MODDATATYPE_CHANNEL;
	mreq.name = "invite",
	mreq.free = invite_free;
	channelInvitesMD = ModDataAdd(modinfo->handle, mreq);
	if (!channelInvitesMD)
	{
		config_error("[%s] Failed to request channel invite moddata: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}

	invite_always_notify = 0; /* the default */
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, invite_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, 1000000, invite_channel_destroy);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, invite_user_quit);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, invite_user_join);
	HookAdd(modinfo->handle, HOOKTYPE_IS_INVITED, 0, invite_is_invited);
	
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

void invite_free(ModData *md)
{
	Link **inv, *tmp;

	if (!md->ptr)
		return; // was not set

	for (inv = (Link **)md->ptr; (tmp = *inv); inv = &tmp->next)
	{
		*inv = tmp->next;
		free_link(tmp);
	}
}

int invite_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name || strcmp(ce->name, "normal-user-invite-notification"))
		return 0;

	if (!ce->value)
	{
		config_error_empty(ce->file->filename, ce->line_number, "set", ce->name);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int invite_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name || strcmp(ce->name, "normal-user-invite-notification"))
		return 0;

	invite_always_notify = config_checkval(ce->value, CFG_YESNO);

	return 1;
}

static int invite_channel_destroy(Channel *channel, int *should_destroy)
{
	Link *lp;
	while ((lp = CHANNEL_INVITES(channel)))
		del_invite(lp->value.client, channel);
	return 0;
}

int invite_user_quit(Client *client, MessageTag *mtags, const char *comment)
{
	Link *lp;
	/* Clean up invitefield */
	while ((lp = CLIENT_INVITES(client)))
		del_invite(client, lp->value.channel);
	return 0;
}

int invite_user_join(Client *client, Channel *channel, MessageTag *mtags)
{
	del_invite(client, channel);
	return 0;
}

/* Send the user their list of active invites */
void send_invite_list(Client *client)
{
	Link *inv;

	for (inv = CLIENT_INVITES(client); inv; inv = inv->next)
	{
		sendnumeric(client, RPL_INVITELIST,
			   inv->value.channel->name);	
	}
	sendnumeric(client, RPL_ENDOFINVITELIST);
}

int invite_is_invited(Client *client, Channel *channel, int *invited)
{
	Link *lp;
	
	if (!MyConnect(client))
		return 0; // not handling invite lists for remote clients

	for (lp = CLIENT_INVITES(client); lp; lp = lp->next)
		if (lp->value.channel == channel)
		{
			*invited = 1;
			return 0;
		}
	return 0;
}

void invite_process(Client *client, Client *target, Channel *channel, MessageTag *recv_mtags, int override)
{
	MessageTag *mtags = NULL;

	new_message(client, recv_mtags, &mtags);

	/* broadcast to other servers */
	sendto_server(client, 0, 0, mtags, ":%s INVITE %s %s %d", client->id, target->id, channel->name, override);

	/* send chanops notifications */
	if (IsUser(client) && (check_channel_access(client, channel, "oaq")
	    || IsULine(client)
	    || ValidatePermissionsForPath("channel:override:invite:self",client,NULL,channel,NULL)
	    || invite_always_notify
	    ))
	{
		if (override == 1)
		{
			sendto_channel(channel, &me, NULL, "o",
				0, SEND_LOCAL, mtags,
				":%s NOTICE @%s :OperOverride -- %s invited him/herself into the channel.",
				me.name, channel->name, client->name);
		}
		if (override == 0)
		{
			sendto_channel(channel, &me, NULL, "o",
				CAP_INVITE_NOTIFY | CAP_INVERT, SEND_LOCAL, mtags,
				":%s NOTICE @%s :%s invited %s into the channel.",
				me.name, channel->name, client->name, target->name);
		}
		/* always send IRCv3 invite-notify if possible */
		sendto_channel(channel, client, NULL, "o",
			CAP_INVITE_NOTIFY, SEND_LOCAL, mtags,
			":%s INVITE %s %s",
			client->name, target->name, channel->name);
	}

	/* add to list and notify the person who got invited */
	if (MyConnect(target))
	{
		if (IsUser(client) && (check_channel_access(client, channel, "oaq")
			|| IsULine(client)
			|| ValidatePermissionsForPath("channel:override:invite:self",client,NULL,channel,NULL)
			))
		{
			add_invite(client, target, channel, mtags);
		}

		if (!is_silenced(client, target))
		{
			sendto_prefix_one(target, client, mtags, ":%s INVITE %s :%s", client->name,
				target->name, channel->name);
		}
	}
	free_message_tags(mtags);
}

void invite_operoverride_msg(Client *client, Channel *channel, char *override_mode, char *override_mode_text)
{
	unreal_log(ULOG_INFO, "operoverride", "OPEROVERRIDE_INVITE", client,
		   "OperOverride: $client.details invited him/herself into $channel (Overriding $override_mode_text)",
		   log_data_string("override_type", "join"),
		   log_data_string("override_mode", override_mode),
		   log_data_string("override_mode_text", override_mode_text),
		   log_data_channel("channel", channel));
}

/*
** cmd_invite
**	parv[1] - user to invite
**	parv[2] - channel name
**  parv[3] - override (S2S only)
*/
CMD_FUNC(cmd_invite)
{
	Client *target = NULL;
	Channel *channel = NULL;
	int override = 0;
	int i = 0;
	int params_ok = 0;
	Hook *h;

	if (parc >= 3 && *parv[1] != '\0')
	{
		params_ok = 1;
		target = find_user(parv[1], NULL);
		channel = find_channel(parv[2]);
	}
	
	if (!MyConnect(client))
	/*** remote invite ***/
	{
		if (!params_ok)
			return;
		/* the client or channel may be already gone */
		if (!target)
		{
			sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
			return;
		}
		if (!channel)
		{
			sendnumeric(client, ERR_NOSUCHCHANNEL, parv[2]);
			return;
		}
		if (parc >= 4 && !BadPtr(parv[3]))
		{
			override = atoi(parv[3]);
		}

		/* no further checks */

		invite_process(client, target, channel, recv_mtags, override);
		return;
	}

	/*** local invite ***/

	/* the client requested own invite list */
	if (parc == 1)
	{
		send_invite_list(client);
		return;
	}

	/* notify user about bad parameters */
	if (!params_ok)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "INVITE");
		return;
	}

	if (!target)
	{
		sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
		return;
	}

	if (!channel)
	{
		sendnumeric(client, ERR_NOSUCHCHANNEL, parv[2]);
		return;
	}

	/* proceed with the command */
	for (h = Hooks[HOOKTYPE_PRE_INVITE]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(client,target,channel,&override);
		if (i == HOOK_DENY)
			return;
		if (i == HOOK_ALLOW)
			break;
	}

	if (!IsMember(client, channel) && !IsULine(client))
	{
		if (ValidatePermissionsForPath("channel:override:invite:notinchannel",client,NULL,channel,NULL) && client == target)
		{
			override = 1;
		} else {
			sendnumeric(client, ERR_NOTONCHANNEL, parv[2]);
			return;
		}
	}

	if (IsMember(target, channel))
	{
		sendnumeric(client, ERR_USERONCHANNEL, parv[1], parv[2]);
		return;
	}

	if (has_channel_mode(channel, 'i'))
	{
		if (!check_channel_access(client, channel, "oaq") && !IsULine(client))
		{
			if (ValidatePermissionsForPath("channel:override:invite:invite-only",client,NULL,channel,NULL) && client == target)
			{
				override = 1;
			} else {
				sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->name);
				return;
			}
		}
		else if (!IsMember(client, channel) && !IsULine(client))
		{
			if (ValidatePermissionsForPath("channel:override:invite:invite-only",client,NULL,channel,NULL) && client == target)
			{
				override = 1;
			} else {
				sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->name);
				return;
			}
		}
	}

	if (SPAMFILTER_VIRUSCHANDENY && SPAMFILTER_VIRUSCHAN &&
	    !strcasecmp(channel->name, SPAMFILTER_VIRUSCHAN) &&
	    !check_channel_access(client, channel, "oaq") && !ValidatePermissionsForPath("immune:server-ban:viruschan",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->name);
		return;
	}

	if (target_limit_exceeded(client, target, target->name))
		return;

	if (!ValidatePermissionsForPath("immune:invite-flood",client,NULL,NULL,NULL) &&
	    flood_limit_exceeded(client, FLD_INVITE))
	{
		sendnumeric(client, RPL_TRYAGAIN, "INVITE");
		return;
	}

	if (!override)
	{
		sendnumeric(client, RPL_INVITING, target->name, channel->name);
		if (target->user->away)
		{
			sendnumeric(client, RPL_AWAY, target->name, target->user->away);
		}
	}
	else
	{
		/* Send OperOverride messages */
		char override_what = '\0';
		if (is_banned(client, channel, BANCHK_JOIN, NULL, NULL))
			invite_operoverride_msg(client, channel, "b", "ban");
		else if (has_channel_mode(channel, 'i'))
			invite_operoverride_msg(client, channel, "i", "invite only");
		else if (has_channel_mode(channel, 'l'))
			invite_operoverride_msg(client, channel, "l", "user limit");
		else if (has_channel_mode(channel, 'k'))
			invite_operoverride_msg(client, channel, "k", "key");
		else if (has_channel_mode(channel, 'z'))
			invite_operoverride_msg(client, channel, "z", "secure only");
#ifdef OPEROVERRIDE_VERIFY
		else if (channel->mode.mode & MODE_SECRET || channel->mode.mode & MODE_PRIVATE)
		       override = -1;
#endif
		else
			return;
	}

	/* allowed to proceed */
	invite_process(client, target, channel, recv_mtags, override);
}

/** Register an invite from someone to a channel - so they can bypass +i etc.
 * @param from		The person sending the invite
 * @param to		The person who is invited to join
 * @param channel	The channel
 * @param mtags		Message tags associated with this INVITE command
 */
void add_invite(Client *from, Client *to, Channel *channel, MessageTag *mtags)
{
	Link *inv, *tmp;

	del_invite(to, channel);
	/* If too many invite entries then delete the oldest one */
	if (link_list_length(CLIENT_INVITES(to)) >= get_setting_for_user_number(from, SET_MAX_CHANNELS_PER_USER))
	{
		for (tmp = CLIENT_INVITES(to); tmp->next; tmp = tmp->next)
			;
		del_invite(to, tmp->value.channel);

	}
	/* We get pissy over too many invites per channel as well now,
	 * since otherwise mass-inviters could take up some major
	 * resources -Donwulff
	 */
	if (link_list_length(CHANNEL_INVITES(channel)) >= get_setting_for_user_number(from, SET_MAX_CHANNELS_PER_USER))
	{
		for (tmp = CHANNEL_INVITES(channel); tmp->next; tmp = tmp->next)
			;
		del_invite(tmp->value.client, channel);
	}
	/*
	 * add client to the beginning of the channel invite list
	 */
	inv = make_link();
	inv->value.client = to;
	inv->next = CHANNEL_INVITES(channel);
	CHANNEL_INVITES(channel) = inv;
	/*
	 * add channel to the beginning of the client invite list
	 */
	inv = make_link();
	inv->value.channel = channel;
	inv->next = CLIENT_INVITES(to);
	CLIENT_INVITES(to) = inv;

	RunHook(HOOKTYPE_INVITE, from, to, channel, mtags);
}

/** Delete a previous invite of someone to a channel.
 * @param client	The client who was invited
 * @param channel	The channel to which the person was invited
 */
void del_invite(Client *client, Channel *channel)
{
	Link **inv, *tmp;

	for (inv = (Link **)&CHANNEL_INVITES(channel); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.client == client)
		{
			*inv = tmp->next;
			free_link(tmp);
			break;
		}

	for (inv = (Link **)&CLIENT_INVITES(client); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.channel == channel)
		{
			*inv = tmp->next;
			free_link(tmp);
			break;
		}
}

