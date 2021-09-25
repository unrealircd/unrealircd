/*
 *   IRC - Internet Relay Chat, src/modules/silence.c
 *   (C) 2004- The UnrealIRCd Team
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

CMD_FUNC(cmd_silence);

ModuleHeader MOD_HEADER
  = {
	"silence",
	"5.0",
	"command /silence", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Structs */
typedef struct Silence Silence;
/** A /SILENCE entry */
struct Silence
{
	Silence *prev, *next;
	char mask[1]; /**< user!nick@host mask of silence entry */
};

/* Global variables */
ModDataInfo *silence_md = NULL;

/* Macros */
#define SILENCELIST(x)       ((Silence *)moddata_local_client(x, silence_md).ptr)

/* Forward declarations */
int _is_silenced(Client *, Client *);
int _del_silence(Client *client, const char *mask);
int _add_silence(Client *client, const char *mask, int senderr);
void silence_md_free(ModData *md);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_ADD_SILENCE, _add_silence);
	EfunctionAdd(modinfo->handle, EFUNC_DEL_SILENCE, _del_silence);
	EfunctionAdd(modinfo->handle, EFUNC_IS_SILENCED, _is_silenced);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "silence";
	mreq.type = MODDATATYPE_LOCAL_CLIENT;
	mreq.free = silence_md_free;
	silence_md = ModDataAdd(modinfo->handle, mreq);
	if (!silence_md)
	{
		config_error("could not register silence moddata");
		return MOD_FAILED;
	}
	CommandAdd(modinfo->handle, "SILENCE", cmd_silence, MAXPARA, CMD_USER);
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

/** The /SILENCE command - server-side ignore list.
 * Syntax:
 * SILENCE +user  To add a user from the silence list
 * SILENCE -user  To remove a user from the silence list
 * SILENCE        To send the current silence list
 *
 */

CMD_FUNC(cmd_silence)
{
	Silence *s;
	const char *p;
	char action;

	if (MyUser(client))
	{
		if (parc < 2 || BadPtr(parv[1]))
		{
			for (s = SILENCELIST(client); s; s = s->next)
				sendnumeric(client, RPL_SILELIST, s->mask);
			sendnumeric(client, RPL_ENDOFSILELIST);
			return;
		}
		p = parv[1];
		action = *p;
		if (action == '-' || action == '+')
		{
			p++;
		} else
		if (!strchr(p, '@') && !strchr(p, '.') && !strchr(p, '!') && !strchr(p, '*') && !find_user(p, NULL))
		{
			sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
			return;
		} else
		{
			action = '+';
		}
		p = pretty_mask(p);
		if ((action == '-' && del_silence(client, p)) ||
		    (action != '-' && add_silence(client, p, 1)))
		{
			sendto_prefix_one(client, client, NULL, ":%s SILENCE %c%s",
			    client->name, action, p);
		}
		return;
	}

	/* Probably server to server traffic.
	 * We don't care about this anymore on UnrealIRCd 5 and later.
	 */
}

/** Delete item from the silence list.
 * @param client The client.
 * @param mask The mask to delete from the list.
 * @returns 1 if entry was found and deleted, 0 if not found.
 */
int _del_silence(Client *client, const char *mask)
{
	Silence *s;

	for (s = SILENCELIST(client); s; s = s->next)
	{
		if (mycmp(mask, s->mask) == 0)
		{
			DelListItemUnchecked(s, moddata_local_client(client, silence_md).ptr);
			safe_free(s);
			return 1;
		}
	}
	return 0;
}

/** Add item to the silence list.
 * @param client The client.
 * @param mask The mask to add to the list.
 * @returns 1 if silence entry added,
 *          0 if not added, eg: full or already covered by an existing silence entry.
 */
int _add_silence(Client *client, const char *mask, int senderr)
{
	Silence *s;
	int cnt = 0;

	if (!MyUser(client))
		return 0;

	for (s = SILENCELIST(client); s; s = s->next)
	{
		if ((strlen(s->mask) > MAXSILELENGTH) || (++cnt >= SILENCE_LIMIT))
		{
			if (senderr)
				sendnumeric(client, ERR_SILELISTFULL, mask);
			return 0;
		}
		else
		{
			if (match_simple(s->mask, mask))
				return 0;
		}
	}

	/* Add the new entry */
	s = safe_alloc(sizeof(Silence)+strlen(mask));
	strcpy(s->mask, mask); /* safe, allocated above */
	AddListItemUnchecked(s, moddata_local_client(client, silence_md).ptr);
	return 1;
}

/** Check whether sender is silenced by receiver.
 * @param sender    The client that intends to send a message.
 * @param receiver  The client that would receive the message.
 * @returns 1 if sender is silenced by receiver (do NOT send the message),
 *          0 if not silenced (go ahead and send).
 */
int _is_silenced(Client *sender, Client *receiver)
{
	Silence *s;
	char mask[HOSTLEN + NICKLEN + USERLEN + 5];

	if (!MyUser(receiver) || !receiver->user || !sender->user || !SILENCELIST(receiver))
		return 0;

	ircsnprintf(mask, sizeof(mask), "%s!%s@%s", sender->name, sender->user->username, GetHost(sender));

	for (s = SILENCELIST(receiver); s; s = s->next)
	{
		if (match_simple(s->mask, mask))
			return 1;
	}

	return 0;
}

/** Called on client exit: free the silence list of this user */
void silence_md_free(ModData *md)
{
	Silence *b, *b_next;

	for (b = md->ptr; b; b = b_next)
	{
		b_next = b->next;
		safe_free(b);
	}
	md->ptr = NULL;
}
