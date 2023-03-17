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

int visible_in_channel(Client *client, Channel *channel);
int moded_check_part(Client *client, Channel *channel);
int moded_join(Client *client, Channel *channel);
int moded_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment);
int moded_quit(Client *client, MessageTag *mtags, const char *comment);
int delayjoin_is_ok(Client *client, Channel *channel, char mode, const char *para, int checkt, int what);
int moded_chanmode(Client *client, Channel *channel,
                   MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode);
int moded_prechanmsg(Client *client, Channel *channel, MessageTag *mtags, const char *text, SendType sendtype);
const char *moded_serialize(ModData *m);
void moded_unserialize(const char *str, ModData *m);

MOD_INIT()
{
	CmodeInfo req;
	ModDataInfo mreq;

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

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = MOD_DATA_STR;
	mreq.serialize = moded_serialize;
	mreq.unserialize = moded_unserialize;
	mreq.sync = 0;
	mreq.type = MODDATATYPE_MEMBER;
	if (!ModDataAdd(modinfo->handle, mreq))
		abort();

	if (!CmodeDelayed || !CmodePostDelayed)
	{
		/* I use config_error() here because it's printed to stderr in case of a load
		 * on cmd line, and to all opers in case of a /rehash.
		 */
		config_error("delayjoin: Could not add channel mode '+D' or '+d': %s", ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}

	HookAdd(modinfo->handle, HOOKTYPE_VISIBLE_IN_CHANNEL, 0, visible_in_channel);
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

bool moded_member_invisible(Member* m, Channel *channel)
{
	ModDataInfo *md;

	if (!m)
		return false;

	md = findmoddata_byname(MOD_DATA_STR, MODDATATYPE_MEMBER);
	if (!md)
		return false;

	if (!moddata_member(m, md).str)
		return false;

	return true;

}

bool moded_user_invisible(Client *client, Channel *channel)
{
	return moded_member_invisible(find_member_link(channel->members, client), channel);
}

bool channel_has_invisible_users(Channel *channel)
{
	Member* i;
	for (i = channel->members; i; i = i->next)
	{
		if (moded_member_invisible(i, channel))
		{
			return true;
		}
	}
	return false;
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

void clear_user_invisible(Channel *channel, Client *client)
{
	Member *i;
	ModDataInfo *md;
	bool should_clear = true, found_member = false;

	md = findmoddata_byname(MOD_DATA_STR, MODDATATYPE_MEMBER);
	if (!md)
		return;
	for (i = channel->members; i; i = i->next)
	{
		if (i->client == client)
		{

			if (md)
				memset(&moddata_member(i, md), 0, sizeof(ModData));

			found_member = true;

			if (!should_clear)
				break;
		}

		else if (moddata_member(i, md).str)
		{
			should_clear = false;
			if (found_member)
				break;
		}
	}

	if (should_clear && (channel->mode.mode & EXTMODE_POST_DELAYED))
	{
		clear_post_delayed(channel);
	}
}

void clear_user_invisible_announce(Channel *channel, Client *client, MessageTag *recv_mtags)
{
	Member *i;
	MessageTag *mtags = NULL;
	char joinbuf[512];
	char exjoinbuf[512];
	long CAP_EXTENDED_JOIN = ClientCapabilityBit("extended-join");

	clear_user_invisible(channel, client);

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
}

void set_user_invisible(Channel *channel, Client *client)
{
	Member *m = find_member_link(channel->members, client);
	ModDataInfo *md;

	if (!m)
		return;

	md = findmoddata_byname(MOD_DATA_STR, MODDATATYPE_MEMBER);

	if (!md || !md->unserialize)
		return;

	md->unserialize(MOD_DATA_INVISIBLE, &moddata_member(m, md));
}


int delayjoin_is_ok(Client *client, Channel *channel, char mode, const char *para, int checkt, int what)
{
	return EX_ALWAYS_DENY;
}


int visible_in_channel(Client *client, Channel *channel)
{
	return (channel_is_delayed(channel) || channel_is_post_delayed(channel)) && moded_user_invisible(client, channel);
}


int moded_join(Client *client, Channel *channel)
{
	if (channel_is_delayed(channel))
		set_user_invisible(channel, client);

	return 0;
}

int moded_part(Client *client, Channel *channel, MessageTag *mtags, const char *comment)
{
	if (channel_is_delayed(channel) || channel_is_post_delayed(channel))
		clear_user_invisible(channel, client);

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
		if (channel_is_delayed(channel) || channel_is_post_delayed(channel))
			clear_user_invisible(channel, client);
	}

	return 0;
}

int moded_chanmode(Client *client, Channel *channel, MessageTag *recv_mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode)
{
	long CAP_EXTENDED_JOIN = ClientCapabilityBit("extended-join");

	// Handle case where we just unset +D but have invisible users
	if (!channel_is_delayed(channel) && !channel_is_post_delayed(channel) && channel_has_invisible_users(channel))
		set_post_delayed(channel);
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

				if (moded_user_invisible(user, channel))
					clear_user_invisible_announce(channel, user, recv_mtags);

				if (pm.modechar == 'v' || !MyConnect(user))
					continue;

				/* Our user 'user' just got ops (oaq) - send the joins for all the users (s)he doesn't know about */
				for (i = channel->members; i; i = i->next)
				{
					if (i->client == user)
						continue;
					if (moded_user_invisible(i->client, channel))
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

				if (moded_user_invisible(user, channel))
					clear_user_invisible_announce(channel, user, recv_mtags);

				if (pm.modechar == 'v' || !MyConnect(user))
					continue;

				/* Our user 'user' just lost ops (oaq) - send the parts for all users (s)he won't see anymore */
				for (i = channel->members; i; i = i->next)
				{
					if (i->client == user)
						continue;
					if (moded_user_invisible(i->client, channel))
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

int moded_prechanmsg(Client *client, Channel *channel, MessageTag *mtags, const char *text, SendType sendtype)
{
	if ((channel_is_delayed(channel) || channel_is_post_delayed(channel)) && (moded_user_invisible(client, channel)))
		clear_user_invisible_announce(channel, client, mtags);

	return 0;
}

const char *moded_serialize(ModData *m)
{
	return m->i ? "1" : "0";
}

void moded_unserialize(const char *str, ModData *m)
{
	m->i = atoi(str);
}
