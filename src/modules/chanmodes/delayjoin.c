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
	"unrealircd-5",
    };

#define MOD_DATA_STR "delayjoin"
#define MOD_DATA_INVISIBLE "1"

static long UMODE_PRIVDEAF = 0;
static Cmode *CmodeDelayed = NULL;
static Cmode *CmodePostDelayed = NULL;
static Cmode_t EXTMODE_DELAYED;
static Cmode_t EXTMODE_POST_DELAYED;

int visible_in_channel(Client *client, Channel *chptr);
int moded_check_part(Client *client, Channel *chptr);
int moded_join(Client *client, Channel *chptr);
int moded_part(Client *client, Channel *chptr, MessageTag *mtags, char *comment);
int deny_all(Client *client, Channel *chptr, char mode, char *para, int checkt, int what);
int moded_chanmode(Client *client, Channel *chptr,
                   MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode);
char *moded_prechanmsg(Client *client, Channel *chptr, MessageTag *mtags, char *text, int notice);
char *moded_serialize(ModData *m);
void moded_unserialize(char *str, ModData *m);

MOD_INIT()
{
	CmodeInfo req;
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM_RELOADABLE, 1);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.is_ok = extcmode_default_requirechop;
	req.flag = 'D';
	CmodeDelayed = CmodeAdd(modinfo->handle, req, &EXTMODE_DELAYED);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.is_ok = deny_all;
	req.flag = 'd';
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
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CHANMODE, 0, moded_chanmode);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_REMOTE_CHANMODE, 0, moded_chanmode);
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_CHANMSG, 99999999, moded_prechanmsg);

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

void set_post_delayed(Channel *chptr)
{
	MessageTag *mtags = NULL;

	chptr->mode.extmode |= EXTMODE_POST_DELAYED;

	new_message(&me, NULL, &mtags);
	sendto_channel(chptr, &me, NULL, 0, 0, SEND_LOCAL, mtags, ":%s MODE %s +d", me.name, chptr->chname);
	free_message_tags(mtags);
}

void clear_post_delayed(Channel *chptr)
{
	MessageTag *mtags = NULL;

	chptr->mode.extmode &= ~EXTMODE_POST_DELAYED;

	new_message(&me, NULL, &mtags);
	sendto_channel(chptr, &me, NULL, 0, 0, SEND_LOCAL, mtags, ":%s MODE %s -d", me.name, chptr->chname);
	free_message_tags(mtags);
}

bool moded_member_invisible(Member* m, Channel *chptr)
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

bool moded_user_invisible(Client *client, Channel *chptr)
{
	return moded_member_invisible(find_member_link(chptr->members, client), chptr);
}

bool channel_has_invisible_users(Channel *chptr)
{
	Member* i;
	for (i = chptr->members; i; i = i->next)
	{
		if (moded_member_invisible(i, chptr))
		{
			return true;
		}
	}
	return false;
}

bool channel_is_post_delayed(Channel *chptr)
{
	if (chptr->mode.extmode & EXTMODE_POST_DELAYED)
		return true;
	return false;
}

bool channel_is_delayed(Channel *chptr)
{
	if (chptr->mode.extmode & EXTMODE_DELAYED)
		return true;
	return false;
}

void clear_user_invisible(Channel *chptr, Client *client)
{
	Member *i;
	ModDataInfo *md;
	bool should_clear = true, found_member = false;

	md = findmoddata_byname(MOD_DATA_STR, MODDATATYPE_MEMBER);
	if (!md)
		return;
	for (i = chptr->members; i; i = i->next)
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

	if (should_clear && (chptr->mode.extmode & EXTMODE_POST_DELAYED))
	{
		clear_post_delayed(chptr);
	}
}

void clear_user_invisible_announce(Channel *chptr, Client *client, MessageTag *recv_mtags)
{
	Member *i;
	MessageTag *mtags = NULL;
	char joinbuf[512];
	char exjoinbuf[512];
	long CAP_EXTENDED_JOIN = ClientCapabilityBit("extended-join");

	clear_user_invisible(chptr, client);

	ircsnprintf(joinbuf, sizeof(joinbuf), ":%s!%s@%s JOIN %s",
				client->name, client->user->username, GetHost(client), chptr->chname);

	ircsnprintf(exjoinbuf, sizeof(exjoinbuf), ":%s!%s@%s JOIN %s %s :%s",
		client->name, client->user->username, GetHost(client), chptr->chname,
		!isdigit(*client->user->svid) ? client->user->svid : "*",
		client->info);

	new_message_special(client, recv_mtags, &mtags, ":%s JOIN %s", client->name, chptr->chname);
	for (i = chptr->members; i; i = i->next)
	{
		Client *acptr = i->client;
		if (!is_skochanop(acptr, chptr) && acptr != client && MyConnect(acptr))
		{
			if (HasCapabilityFast(acptr, CAP_EXTENDED_JOIN))
				sendto_one(acptr, mtags, "%s", exjoinbuf);
			else
				sendto_one(acptr, mtags, "%s", joinbuf);
		}
	}
	free_message_tags(mtags);
}

void set_user_invisible(Channel *chptr, Client *client)
{
	Member *m = find_member_link(chptr->members, client);
	ModDataInfo *md;

	if (!m)
		return;

	md = findmoddata_byname(MOD_DATA_STR, MODDATATYPE_MEMBER);

	if (!md || !md->unserialize)
		return;

	md->unserialize(MOD_DATA_INVISIBLE, &moddata_member(m, md));
}


int deny_all(Client *client, Channel *chptr, char mode, char *para, int checkt, int what)
{
	return EX_ALWAYS_DENY;
}


int visible_in_channel(Client *client, Channel *chptr)
{
	return channel_is_delayed(chptr) && moded_user_invisible(client, chptr);
}


int moded_join(Client *client, Channel *chptr)
{
	if (channel_is_delayed(chptr))
		set_user_invisible(chptr, client);

	return 0;
}

int moded_part(Client *client, Channel *chptr, MessageTag *mtags, char *comment)
{
	if (channel_is_delayed(chptr) || channel_is_post_delayed(chptr))
		clear_user_invisible(chptr, client);

	return 0;
}

int moded_chanmode(Client *client, Channel *chptr, MessageTag *recv_mtags, char *modebuf, char *parabuf, time_t sendts, int samode)
{
	long CAP_EXTENDED_JOIN = ClientCapabilityBit("extended-join");

	// Handle case where we just unset +D but have invisible users
	if (!channel_is_delayed(chptr) && !channel_is_post_delayed(chptr) && channel_has_invisible_users(chptr))
		set_post_delayed(chptr);
	else if (channel_is_delayed(chptr) && channel_is_post_delayed(chptr))
		clear_post_delayed(chptr);

	if ((channel_is_delayed(chptr) || channel_is_post_delayed(chptr)))
	{
		ParseMode pm;
		int ret;
		for (ret = parse_chanmode(&pm, modebuf, parabuf); ret; ret = parse_chanmode(&pm, NULL, NULL))
		{
			if (pm.what == MODE_ADD && (pm.modechar == 'o' || pm.modechar == 'h' || pm.modechar == 'a' || pm.modechar == 'q' || pm.modechar == 'v'))
			{
				Member* i;
				Client* user = find_client(pm.param,NULL);
				if (!user)
					continue;

				if (moded_user_invisible(user, chptr))
					clear_user_invisible_announce(chptr, user, recv_mtags);

				if (pm.modechar == 'v' || !MyConnect(user))
					continue;

				/* Our user 'user' just got ops (oaq) - send the joins for all the users (s)he doesn't know about */
				for (i = chptr->members; i; i = i->next)
				{
					if (i->client == user)
						continue;
					if (moded_user_invisible(i->client, chptr))
					{
						MessageTag *mtags = NULL;
						new_message_special(i->client, recv_mtags, &mtags, ":%s JOIN %s", i->client->name, chptr->chname);
						if (HasCapabilityFast(user, CAP_EXTENDED_JOIN))
						{
							sendto_one(user, mtags, ":%s!%s@%s JOIN %s %s :%s",
							           i->client->name, i->client->user->username, GetHost(i->client),
							           chptr->chname,
							           !isdigit(*i->client->user->svid) ? i->client->user->svid : "*",
							           i->client->info);
						} else {
							sendto_one(user, mtags, ":%s!%s@%s JOIN :%s", i->client->name, i->client->user->username, GetHost(i->client), chptr->chname);
						}
						free_message_tags(mtags);
					}
				}

			}
			if (pm.what == MODE_DEL && (pm.modechar == 'o' || pm.modechar == 'h' || pm.modechar == 'a' || pm.modechar == 'q' || pm.modechar == 'v'))
			{
				Member* i;
				Client* user = find_client(pm.param,NULL);
				if (!user)
					continue;

				if (moded_user_invisible(user, chptr))
					clear_user_invisible_announce(chptr, user, recv_mtags);

				if (pm.modechar == 'v' || !MyConnect(user))
					continue;

				/* Our user 'user' just lost ops (oaq) - send the parts for all users (s)he won't see anymore */
				for (i = chptr->members; i; i = i->next)
				{
					if (i->client == user)
						continue;
					if (moded_user_invisible(i->client, chptr))
					{
						MessageTag *mtags = NULL;
						new_message_special(i->client, recv_mtags, &mtags, ":%s PART %s", i->client->name, chptr->chname);
						sendto_one(user, mtags, ":%s!%s@%s PART :%s", i->client->name, i->client->user->username, GetHost(i->client), chptr->chname);
						free_message_tags(mtags);
					}
				}

			}
		}
	}

	return 0;
}

char *moded_prechanmsg(Client *client, Channel *chptr, MessageTag *mtags, char *text, int notice)
{

	if ((channel_is_delayed(chptr) || channel_is_post_delayed(chptr)) && (moded_user_invisible(client, chptr)))
		clear_user_invisible_announce(chptr, client, mtags);

	return text;
}

char *moded_serialize(ModData *m)
{
	return m->i ? "1" : "0";
}

void moded_unserialize(char *str, ModData *m)
{
	m->i = atoi(str);
}
