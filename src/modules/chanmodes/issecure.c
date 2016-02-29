/*
 * Channel Is Secure UnrealIRCd module (Channel Mode +Z)
 * (C) Copyright 2010-.. Bram Matthys (Syzop) and the UnrealIRCd team
 *
 * This module will indicate if a channel is secure, and if so will set +Z.
 * Secure is defined as: all users on the channel are connected through SSL/TLS
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

ModuleHeader MOD_HEADER(issecure)
  = {
	"chanmodes/issecure",
	"4.0",
	"Channel Mode +Z", 
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_ISSECURE;

#define IsSecureChanIndicated(chptr)	(chptr->mode.extmode & EXTCMODE_ISSECURE)

int IsSecureJoin(aChannel *chptr);
int modeZ_is_ok(aClient *sptr, aChannel *chptr, char mode, char *para, int checkt, int what);
DLLFUNC int issecure_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *parv[]);
DLLFUNC int issecure_part(aClient *cptr, aClient *sptr, aChannel *chptr, char *comment);
DLLFUNC int issecure_quit(aClient *acptr, char *comment);
DLLFUNC int issecure_kick(aClient *cptr, aClient *sptr, aClient *acptr, aChannel *chptr, char *comment);
DLLFUNC int issecure_chanmode(aClient *cptr, aClient *sptr, aChannel *chptr,
                             char *modebuf, char *parabuf, time_t sendts, int samode);
                             

MOD_TEST(issecure)
{
	return MOD_SUCCESS;
}

MOD_INIT(issecure)
{
CmodeInfo req;

	/* Channel mode */
	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.is_ok = modeZ_is_ok;
	req.flag = 'Z';
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

MOD_LOAD(issecure)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(issecure)
{
	return MOD_SUCCESS;
}

int IsSecureJoin(aChannel *chptr)
{
	Hook *h;
	int i = 0;

	for (h = Hooks[HOOKTYPE_IS_CHANNEL_SECURE]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(chptr);
		if (i != 0)
			break;
	}

	return i;
}

int modeZ_is_ok(aClient *sptr, aChannel *chptr, char mode, char *para, int checkt, int what)
{
	/* Reject any attempt to set or unset our mode. Even to IRCOps */
	return EX_ALWAYS_DENY;
}

int channel_has_insecure_users_butone(aChannel *chptr, aClient *skip)
{
Member *member;

	for (member = chptr->members; member; member = member->next)
	{
		if (member->cptr == skip)
			continue;
		if (IsULine(member->cptr))
			continue;
		if (!IsSecureConnect(member->cptr))
			return 1;
	}
	return 0;
}

#define channel_has_insecure_users(x) channel_has_insecure_users_butone(x, NULL)

/* Set channel status of 'chptr' to be no longer secure (-Z) due to 'sptr'.
 * sptr MAY be null!
 */
void issecure_unset(aChannel *chptr, aClient *sptr, int notice)
{
	Hook *h;
	int i = 0;

	if (notice)
	{
		for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(sptr,chptr);
			if (i != 0)
				break;
		}

		if (i != 0)
			sendto_channel_butserv(chptr, &me, ":%s NOTICE %s :User joined and is not connected through SSL, setting channel -Z (insecure)",
				me.name, chptr->chname);
		else
			sendto_channel_butserv(chptr, &me, ":%s NOTICE %s :User '%s' joined and is not connected through SSL, setting channel -Z (insecure)",
				me.name, chptr->chname, sptr->name);
	}
		
	chptr->mode.extmode &= ~EXTCMODE_ISSECURE;
	sendto_channel_butserv(chptr, &me, ":%s MODE %s -Z", me.name, chptr->chname);
}


/* Set channel status of 'chptr' to be secure (+Z).
 * Channel might have been insecure (or might not have been +z) and is
 * now considered secure. If 'sptr' is non-NULL then we are now secure
 * thanks to this user leaving the chat.
 */
void issecure_set(aChannel *chptr, aClient *sptr, int notice)
{
	if (notice && sptr)
	{
		/* note that we have to skip 'sptr', since when this call is being made
		 * he is still considered a member of this channel.
		 */
		sendto_channel_butserv_butone(chptr, &me, sptr, ":%s NOTICE %s :Now all users in the channel are connected through SSL, setting channel +Z (secure)",
			me.name, chptr->chname);
	} else if (notice)
	{
		/* note the missing word 'now' in next line */
		sendto_channel_butserv(chptr, &me, ":%s NOTICE %s :All users in the channel are connected through SSL, setting channel +Z (secure)",
			me.name, chptr->chname);
	}
	chptr->mode.extmode |= EXTCMODE_ISSECURE;
	sendto_channel_butserv_butone(chptr, &me, sptr, ":%s MODE %s +Z", me.name, chptr->chname);
}

/* Note: the routines below (notably the 'if's) are written with speed in mind,
 *       so while they can be written shorter, they would only take longer to execute!
 */

DLLFUNC int issecure_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *parv[])
{
	/* Only care if chan already +zZ and the user joining is insecure (no need to count) */
	if (IsSecureJoin(chptr) && IsSecureChanIndicated(chptr) && !IsSecureConnect(sptr) && !IsULine(sptr))
		issecure_unset(chptr, sptr, 1);
	return 0;
}

DLLFUNC int issecure_part(aClient *cptr, aClient *sptr, aChannel *chptr, char *comment)
{
	/* Only care if chan is +z-Z and the user leaving is insecure, then count */
	if (IsSecureJoin(chptr) && !IsSecureChanIndicated(chptr) && !IsSecureConnect(sptr) &&
	    !channel_has_insecure_users_butone(chptr, sptr))
		issecure_set(chptr, sptr, 1);
	return 0;
}

DLLFUNC int issecure_quit(aClient *sptr, char *comment)
{
Membership *membership;
aChannel *chptr;

	for (membership = sptr->user->channel; membership; membership=membership->next)
	{
		chptr = membership->chptr;
		/* Identical to part */
		if (IsSecureJoin(chptr) && !IsSecureChanIndicated(chptr) && 
		    !IsSecureConnect(sptr) && !channel_has_insecure_users_butone(chptr, sptr))
			issecure_set(chptr, sptr, 1);
	}
	return 0;
}

DLLFUNC int issecure_kick(aClient *cptr, aClient *sptr, aClient *victim, aChannel *chptr, char *comment)
{
	/* Identical to part&quit, except we care about 'victim' and not 'sptr' */
	if (IsSecureJoin(chptr) && !IsSecureChanIndicated(chptr) &&
	    !IsSecureConnect(victim) && !channel_has_insecure_users_butone(chptr, victim))
		issecure_set(chptr, victim, 1);
	return 0;
}

DLLFUNC int issecure_chanmode(aClient *cptr, aClient *sptr, aChannel *chptr,
                             char *modebuf, char *parabuf, time_t sendts, int samode)
{
	if (!strchr(modebuf, 'z'))
		return 0; /* don't care */

	if (IsSecureJoin(chptr))
	{
		/* +z is set, check if we need to +Z
		 * Note that we need to be careful as there is a possibility that we got here
		 * but the channel is ALREADY +z. Due to server2server MODE's.
		 */
		if (channel_has_insecure_users(chptr))
		{
			/* Should be -Z, if not already */
			if (IsSecureChanIndicated(chptr))
				issecure_unset(chptr, NULL, 0); /* would be odd if we got here ;) */
		} else {
			/* Should be +Z, but check if it isn't already.. */
			if (!IsSecureChanIndicated(chptr))
				issecure_set(chptr, NULL, 0);
		}
	} else {
		/* there was a -z, check if the channel is currently +Z and if so, set it -Z */
		if (IsSecureChanIndicated(chptr))
			issecure_unset(chptr, NULL, 0);
	}
	return 0;
}
