/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/issecure.c
 * Channel Is Secure UnrealIRCd module (Channel Mode +Z)
 * (C) Copyright 2010-.. Bram Matthys (Syzop) and the UnrealIRCd team
 *
 * This module will indicate if a channel is secure, and if so will set +Z.
 * Secure is defined as: all users on the channel are connected through TLS
 * Additionally, the channel has to be +z (only allow secure users to join).
 * Suggested on http://bugs.unrealircd.org/view.php?id=3720
 * Thanks go to fez for pushing us for some kind of method to indicate
 * this 'secure channel state', and to Stealth for suggesting this method.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

CMD_FUNC(issecure);

ModuleHeader MOD_HEADER
  = {
	"chanmodes/issecure",
	"4.2",
	"Channel Mode +Z", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_ISSECURE;

#define IsSecureChanIndicated(channel)	(channel->mode.mode & EXTCMODE_ISSECURE)

int IsSecureJoin(Channel *channel);
int modeZ_is_ok(Client *client, Channel *channel, char mode, const char *para, int checkt, int what);
int issecure_join(Client *client, Channel *channel, MessageTag *mtags);
int issecure_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment);
int issecure_quit(Client *client, MessageTag *mtags, const char *comment);
int issecure_kick(Client *client, Client *victim, Channel *channel, MessageTag *mtags, const char *comment);
int issecure_chanmode(Client *client, Channel *channel, MessageTag *mtags,
                             const char *modebuf, const char *parabuf, time_t sendts, int samode, int *destroy_channel);
                             

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
CmodeInfo req;

	/* Channel mode */
	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.is_ok = modeZ_is_ok;
	req.letter = 'Z';
	req.local = 1; /* local channel mode */
	CmodeAdd(modinfo->handle, req, &EXTCMODE_ISSECURE);
	
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, issecure_join);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_JOIN, 0, issecure_join);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_PART, 0, issecure_part);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_PART, 0, issecure_part);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, issecure_quit);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_QUIT, 0, issecure_quit);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_KICK, 0, issecure_kick);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_KICK, 0, issecure_kick);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CHANMODE, 0, issecure_chanmode);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CHANMODE, 0, issecure_chanmode);
	
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

int IsSecureJoin(Channel *channel)
{
	Hook *h;
	int i = 0;

	for (h = Hooks[HOOKTYPE_IS_CHANNEL_SECURE]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(channel);
		if (i != 0)
			break;
	}

	return i;
}

int modeZ_is_ok(Client *client, Channel *channel, char mode, const char *para, int checkt, int what)
{
	/* Reject any attempt to set or unset our mode. Even to IRCOps */
	return EX_ALWAYS_DENY;
}

int channel_has_insecure_users_butone(Channel *channel, Client *skip)
{
Member *member;

	for (member = channel->members; member; member = member->next)
	{
		if (member->client == skip)
			continue;
		if (IsULine(member->client))
			continue;
		if (!IsSecureConnect(member->client))
			return 1;
	}
	return 0;
}

#define channel_has_insecure_users(x) channel_has_insecure_users_butone(x, NULL)

/* Set channel status of 'channel' to be no longer secure (-Z) due to 'client'.
 * client MAY be null!
 */
void issecure_unset(Channel *channel, Client *client, MessageTag *recv_mtags, int notice)
{
	Hook *h;
	MessageTag *mtags;

	if (notice)
	{
		mtags = NULL;
		new_message_special(&me, recv_mtags, &mtags, "NOTICE %s :setting -Z", channel->name);
		sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
		               ":%s NOTICE %s :User '%s' joined and is not connected through TLS, setting channel -Z (insecure)",
		               me.name, channel->name, client->name);
		free_message_tags(mtags);
	}
		
	channel->mode.mode &= ~EXTCMODE_ISSECURE;
	mtags = NULL;
	new_message_special(&me, recv_mtags, &mtags, "MODE %s -Z", channel->name);
	sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags, ":%s MODE %s -Z", me.name, channel->name);
	free_message_tags(mtags);
}


/* Set channel status of 'channel' to be secure (+Z).
 * Channel might have been insecure (or might not have been +z) and is
 * now considered secure. If 'client' is non-NULL then we are now secure
 * thanks to this user leaving the chat.
 */
void issecure_set(Channel *channel, Client *client, MessageTag *recv_mtags, int notice)
{
	MessageTag *mtags;

	mtags = NULL;
	new_message_special(&me, recv_mtags, &mtags, "NOTICE %s :setting +Z", channel->name);
	if (notice && client)
	{
		/* note that we have to skip 'client', since when this call is being made
		 * he is still considered a member of this channel.
		 */
		sendto_channel(channel, &me, client, 0, 0, SEND_LOCAL, NULL,
		               ":%s NOTICE %s :Now all users in the channel are connected through TLS, setting channel +Z (secure)",
		               me.name, channel->name);
	} else if (notice)
	{
		/* note the missing word 'now' in next line */
		sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, NULL,
		               ":%s NOTICE %s :All users in the channel are connected through TLS, setting channel +Z (secure)",
		               me.name, channel->name);
	}
	free_message_tags(mtags);

	channel->mode.mode |= EXTCMODE_ISSECURE;

	mtags = NULL;
	new_message_special(&me, recv_mtags, &mtags, "MODE %s +Z", channel->name);
	sendto_channel(channel, &me, client, 0, 0, SEND_LOCAL, mtags,
	               ":%s MODE %s +Z",
	               me.name, channel->name);
	free_message_tags(mtags);
}

/* Note: the routines below (notably the 'if's) are written with speed in mind,
 *       so while they can be written shorter, they would only take longer to execute!
 */

int issecure_join(Client *client, Channel *channel, MessageTag *mtags)
{
	/* Check only if chan already +zZ and the user joining is insecure (no need to count) */
	if (IsSecureJoin(channel) && IsSecureChanIndicated(channel) && !IsSecureConnect(client) && !IsULine(client))
		issecure_unset(channel, client, mtags, 1);

	/* Special case for +z in modes-on-join and first user creating the channel */
	if ((channel->users == 1) && IsSecureJoin(channel) && !IsSecureChanIndicated(channel) && !channel_has_insecure_users(channel))
		issecure_set(channel, NULL, mtags, 0);

	return 0;
}

int issecure_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment)
{
	/* Only care if chan is +z-Z and the user leaving is insecure, then count */
	if (IsSecureJoin(channel) && !IsSecureChanIndicated(channel) && !IsSecureConnect(client) &&
	    !channel_has_insecure_users_butone(channel, client))
		issecure_set(channel, client, mtags, 1);
	return 0;
}

int issecure_quit(Client *client, MessageTag *mtags, const char *comment)
{
Membership *membership;
Channel *channel;

	for (membership = client->user->channel; membership; membership=membership->next)
	{
		channel = membership->channel;
		/* Identical to part */
		if (IsSecureJoin(channel) && !IsSecureChanIndicated(channel) && 
		    !IsSecureConnect(client) && !channel_has_insecure_users_butone(channel, client))
			issecure_set(channel, client, mtags, 1);
	}
	return 0;
}

int issecure_kick(Client *client, Client *victim, Channel *channel, MessageTag *mtags, const char *comment)
{
	/* Identical to part&quit, except we care about 'victim' and not 'client' */
	if (IsSecureJoin(channel) && !IsSecureChanIndicated(channel) &&
	    !IsSecureConnect(victim) && !channel_has_insecure_users_butone(channel, victim))
		issecure_set(channel, victim, mtags, 1);
	return 0;
}

int issecure_chanmode(Client *client, Channel *channel, MessageTag *mtags,
                             const char *modebuf, const char *parabuf, time_t sendts, int samode, int *destroy_channel)
{
	if (!strchr(modebuf, 'z'))
		return 0; /* don't care */

	if (IsSecureJoin(channel))
	{
		/* +z is set, check if we need to +Z
		 * Note that we need to be careful as there is a possibility that we got here
		 * but the channel is ALREADY +z. Due to server2server MODE's.
		 */
		if (channel_has_insecure_users(channel))
		{
			/* Should be -Z, if not already */
			if (IsSecureChanIndicated(channel))
				issecure_unset(channel, NULL, mtags, 0); /* would be odd if we got here ;) */
		} else {
			/* Should be +Z, but check if it isn't already.. */
			if (!IsSecureChanIndicated(channel))
				issecure_set(channel, NULL, mtags, 0);
		}
	} else {
		/* there was a -z, check if the channel is currently +Z and if so, set it -Z */
		if (IsSecureChanIndicated(channel))
			issecure_unset(channel, NULL, mtags, 0);
	}
	return 0;
}
