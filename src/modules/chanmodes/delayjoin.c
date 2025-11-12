/*
 * Channel mode +D/+d: delayed join
 * except from opers, U-lines and servers.
 * Copyright 2014 Travis Mcarthur <Heero> and UnrealIRCd Team
 */
#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"chanmodes/delayjoin",   /* Name of module */
	"5.0", /* Version */
	"delayed join (+D,+d)", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

#define MOD_DATA_STR "delayjoin"
#define MOD_DATA_INVISIBLE "1"

static long UMODE_PRIVDEAF = 0;
static Cmode *CmodeDelayed = NULL;
static Cmode *CmodePostDelayed = NULL;
static Cmode_t EXTMODE_DELAYED;
static Cmode_t EXTMODE_POST_DELAYED;

int visible_in_channel(Client *client, Channel *channel, Member *client_member);
int moded_join(Client *client, Channel *channel);
int moded_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment);
int moded_quit(Client *client, MessageTag *mtags, const char *comment);
int delayjoin_is_ok(Client *client, Channel *channel, char mode, const char *para, int checkt, int what);
int moded_chanmode(Client *client, Channel *channel,
                   MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode);
int moded_prechanmsg(Client *client, Channel *channel, MessageTag **mtags, const char *text, SendType sendtype);

MOD_INIT()
{
	CmodeInfo req;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM_RELOADABLE, 1);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.is_ok = extcmode_default_requirechop;
	req.letter = 'D';
	CmodeDelayed = CmodeAdd(modinfo->handle, req, &EXTMODE_DELAYED);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.is_ok = delayjoin_is_ok;
	req.letter = 'd';
	req.local = 1;
	CmodePostDelayed = CmodeAdd(modinfo->handle, req, &EXTMODE_POST_DELAYED);

	if (!CmodeDelayed || !CmodePostDelayed)
	{
		/* I use config_error() here because it's printed to stderr in case of a load
		 * on cmd line, and to all opers in case of a /rehash.
		 */
		config_error("delayjoin: Could not add channel mode '+D' or '+d': %s", ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}

	HookAdd(modinfo->handle, HOOKTYPE_JOIN_DATA, 0, moded_join);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_PART, 0, moded_part);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_PART, 0, moded_part);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, moded_quit);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_QUIT, 0, moded_quit);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CHANMODE, 0, moded_chanmode);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_REMOTE_CHANMODE, 0, moded_chanmode);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_CHANMSG, 0, moded_prechanmsg);

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

void set_post_delayed(Channel *channel)
{
	MessageTag *mtags = NULL;

	channel->mode.mode |= EXTMODE_POST_DELAYED;

	new_message(&me, NULL, &mtags);
	sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags, ":%s MODE %s +d", me.name, channel->name);
	free_message_tags(mtags);
}

void clear_post_delayed(Channel *channel)
{
	MessageTag *mtags = NULL;

	channel->mode.mode &= ~EXTMODE_POST_DELAYED;

	new_message(&me, NULL, &mtags);
	sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags, ":%s MODE %s -d", me.name, channel->name);
	free_message_tags(mtags);
}

bool channel_is_post_delayed(Channel *channel)
{
	if (channel->mode.mode & EXTMODE_POST_DELAYED)
		return true;
	return false;
}

bool channel_is_delayed(Channel *channel)
{
	if (channel->mode.mode & EXTMODE_DELAYED)
		return true;
	return false;
}

void clear_user_invisible_announce(Channel *channel, Client *client, MessageTag *recv_mtags)
{
	Member *i;
	MessageTag *mtags = NULL;
	char joinbuf[512];
	char exjoinbuf[512];
	long CAP_EXTENDED_JOIN = ClientCapabilityBit("extended-join");

	set_user_invisible(client, channel, 0);

	ircsnprintf(joinbuf, sizeof(joinbuf), ":%s!%s@%s JOIN %s",
				client->name, client->user->username, GetHost(client), channel->name);

	ircsnprintf(exjoinbuf, sizeof(exjoinbuf), ":%s!%s@%s JOIN %s %s :%s",
		client->name, client->user->username, GetHost(client), channel->name,
		IsLoggedIn(client) ? client->user->account : "*",
		client->info);

	new_message_special(client, recv_mtags, &mtags, ":%s JOIN %s", client->name, channel->name);
	for (i = channel->members; i; i = i->next)
	{
		Client *acptr = i->client;
		if (!check_channel_access(acptr, channel, "hoaq") && acptr != client && MyConnect(acptr))
		{
			if (HasCapabilityFast(acptr, CAP_EXTENDED_JOIN))
				sendto_one(acptr, mtags, "%s", exjoinbuf);
			else
				sendto_one(acptr, mtags, "%s", joinbuf);
		}
	}
	free_message_tags(mtags);

	/* If this was the last invisible user to become visible, then set -d */
	if ((channel->mode.mode & EXTMODE_POST_DELAYED) && !channel_has_invisible_users(channel))
	        clear_post_delayed(channel);
}

int delayjoin_is_ok(Client *client, Channel *channel, char mode, const char *para, int checkt, int what)
{
	return EX_ALWAYS_DENY;
}

int moded_join(Client *client, Channel *channel)
{
	if (channel_is_delayed(channel))
		set_user_invisible(client, channel, 1);

	return 0;
}

int moded_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment)
{
	/* If this was the last invisible user to become visible, then set -d */
	if (channel->mode.mode & EXTMODE_POST_DELAYED)
	{
		set_user_invisible(client, channel, 0);
		if (!channel_has_invisible_users(channel))
		        clear_post_delayed(channel);
	}
	return 0;
}

int moded_quit(Client *client, MessageTag *mtags, const char *comment)
{
	Membership *membership;
	Channel *channel;

	for (membership = client->user->channel; membership; membership=membership->next)
	{
		channel = membership->channel;
		/* Identical to moded_part() */
		if (channel->mode.mode & EXTMODE_POST_DELAYED)
		{
			set_user_invisible(client, channel, 0);
			if (!channel_has_invisible_users(channel))
				clear_post_delayed(channel);
		}
	}
	return 0;
}

// moded_kick ??

int moded_parsemode(const char *m)
{
	int what = MODE_ADD;
	int removed = 0;

	for (; *m; m++)
	{
		if (*m == '+')
			what = MODE_ADD;
		else if (*m == '-')
			what = MODE_DEL;
		else if (*m == 'D')
		{
			if (what == MODE_DEL)
				removed = 1;
			else
				removed = 0;
		}
	}
	return removed;
}

int moded_chanmode(Client *client, Channel *channel, MessageTag *recv_mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode)
{
	long CAP_EXTENDED_JOIN = ClientCapabilityBit("extended-join");
	int removed_delayed_mode = moded_parsemode(modebuf);

	// Handle case where channel went -D but has invisible users
	if (removed_delayed_mode && !channel_is_delayed(channel) && !channel_is_post_delayed(channel) && channel_has_invisible_users(channel))
		set_post_delayed(channel);
	// And even going from +d (back) to +D again by user request
	else if (channel_is_delayed(channel) && channel_is_post_delayed(channel))
		clear_post_delayed(channel);

	if ((channel_is_delayed(channel) || channel_is_post_delayed(channel)))
	{
		ParseMode pm;
		int ret;
		for (ret = parse_chanmode(&pm, modebuf, parabuf); ret; ret = parse_chanmode(&pm, NULL, NULL))
		{
			if (pm.what == MODE_ADD && (pm.modechar == 'o' || pm.modechar == 'h' || pm.modechar == 'a' || pm.modechar == 'q' || pm.modechar == 'v'))
			{
				Member* i;
				Client *user = find_client(pm.param,NULL);
				if (!user)
					continue;

				if (invisible_user_in_channel(user, channel))
					clear_user_invisible_announce(channel, user, recv_mtags);

				if (pm.modechar == 'v' || !MyConnect(user))
					continue;

				/* Our user 'user' just got ops (oaq) - send the joins for all the users (s)he doesn't know about */
				for (i = channel->members; i; i = i->next)
				{
					if (i->client == user)
						continue;
					if (invisible_user_in_channel(i->client, channel))
					{
						MessageTag *mtags = NULL;
						new_message_special(i->client, recv_mtags, &mtags, ":%s JOIN %s", i->client->name, channel->name);
						if (HasCapabilityFast(user, CAP_EXTENDED_JOIN))
						{
							sendto_one(user, mtags, ":%s!%s@%s JOIN %s %s :%s",
							           i->client->name, i->client->user->username, GetHost(i->client),
							           channel->name,
							           IsLoggedIn(i->client) ? i->client->user->account : "*",
							           i->client->info);
						} else {
							sendto_one(user, mtags, ":%s!%s@%s JOIN :%s", i->client->name, i->client->user->username, GetHost(i->client), channel->name);
						}
						free_message_tags(mtags);
					}
				}

			}
			if (pm.what == MODE_DEL && (pm.modechar == 'o' || pm.modechar == 'h' || pm.modechar == 'a' || pm.modechar == 'q' || pm.modechar == 'v'))
			{
				Member* i;
				Client *user = find_client(pm.param,NULL);
				if (!user)
					continue;

				if (invisible_user_in_channel(user, channel))
					clear_user_invisible_announce(channel, user, recv_mtags);

				if (pm.modechar == 'v' || !MyConnect(user))
					continue;

				/* Our user 'user' just lost ops (oaq) - send the parts for all users (s)he won't see anymore */
				for (i = channel->members; i; i = i->next)
				{
					if (i->client == user)
						continue;
					if (invisible_user_in_channel(i->client, channel))
					{
						MessageTag *mtags = NULL;
						new_message_special(i->client, recv_mtags, &mtags, ":%s PART %s", i->client->name, channel->name);
						sendto_one(user, mtags, ":%s!%s@%s PART :%s", i->client->name, i->client->user->username, GetHost(i->client), channel->name);
						free_message_tags(mtags);
					}
				}

			}
		}
	}

	return 0;
}

int moded_prechanmsg(Client *client, Channel *channel, MessageTag **mtags, const char *text, SendType sendtype)
{
	if (IsUser(client) && (channel_is_delayed(channel) || channel_is_post_delayed(channel)) && (invisible_user_in_channel(client, channel)))
		clear_user_invisible_announce(channel, client, *mtags);

	return 0;
}
