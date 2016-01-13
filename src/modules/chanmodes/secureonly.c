/*
 * Only allow secure users to join UnrealIRCd Module (Channel Mode +z)
 * (C) Copyright 2014 Travis McArthur (Heero) and the UnrealIRCd team
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

ModuleHeader MOD_HEADER(sslonly)
  = {
	"chanmodes/sslonly",
	"4.0",
	"Channel Mode +z",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_SSLONLY;

#define IsSecureOnly(chptr)    (chptr->mode.extmode & EXTCMODE_SSLONLY)

int secureonly_check_join (aClient *sptr, aChannel *chptr, char *key, char *parv[]);
void secureonly_channel_sync (aChannel* chptr, unsigned short merge, unsigned short removetheirs, unsigned short nomode);
int secureonly_check_send (aClient *acptr, aChannel* chptr);
int secureonly_check_secure (aChannel* chptr);
int secureonly_check_sajoin (aClient *acptr, aChannel* chptr, aClient *sptr);

MOD_TEST(sslonly)
{
	return MOD_SUCCESS;
}

MOD_INIT(sslonly)
{
	CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'z';
	req.is_ok = extcmode_default_requirechop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_SSLONLY);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_JOIN, 0, secureonly_check_join);
	HookAddVoid(modinfo->handle, HOOKTYPE_CHANNEL_SYNCED, 0, secureonly_channel_sync);
	HookAdd(modinfo->handle, HOOKTYPE_IS_CHANNEL_SECURE, 0, secureonly_check_secure);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_SECURE, 0, secureonly_check_send);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SAJOIN, 0, secureonly_check_sajoin);


	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(sslonly)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(sslonly)
{
	return MOD_SUCCESS;
}


/** Kicks all insecure users on a +z channel */
static void secureonly_kick_insecure_users(aChannel *chptr)
{
	Member *member, *mb2;
	aClient *cptr;
	int i = 0;
	Hook *h;
	char *comment = "Insecure user not allowed on secure channel (+z)";

	if (!IsSecureOnly(chptr))
		return;

	for (member = chptr->members; member; member = mb2)
	{
		mb2 = member->next;
		cptr = member->cptr;
		if (MyClient(cptr) && !IsSecureConnect(cptr) && !IsULine(cptr))
		{
			RunHook5(HOOKTYPE_LOCAL_KICK, &me, &me, cptr, chptr, comment);

			i = 0;
			for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
			{
				i = (*(h->func.intfunc))(cptr,chptr);
				if (i != 0)
					break;
			}

			if (i != 0 && !(is_skochanop(cptr, chptr) || has_voice(cptr,chptr)))
			{
				sendto_chanops_butone(cptr, chptr, ":%s KICK %s %s :%s", me.name, chptr->chname, cptr->name, comment);
				sendto_prefix_one(cptr, &me, ":%s KICK %s %s :%s", me.name, chptr->chname, cptr->name, comment);
			}
			else
			{
				sendto_channel_butserv(chptr, &me, ":%s KICK %s %s :%s", me.name, chptr->chname, cptr->name, comment);
			}

			sendto_server(&me, 0, 0, ":%s KICK %s %s :%s", me.name, chptr->chname, cptr->name, comment);

			remove_user_from_channel(cptr, chptr);
		}
	}
}

int secureonly_check_join (aClient *sptr, aChannel *chptr, char *key, char *parv[])
{
	Link *lp;

	if (IsSecureOnly(chptr) && !(sptr->umodes & UMODE_SECURE))
		{
			if (ValidatePermissionsForPath("immune:secureonly",sptr,NULL,chptr,NULL))
			{
				/* if the channel is +z we still allow an ircop to bypass it
				 * if they are invited.
				 */
				for (lp = sptr->user->invited; lp; lp = lp->next)
					if (lp->value.chptr == chptr)
						return HOOK_CONTINUE;
			}
			return (ERR_SECUREONLYCHAN);
		}
	return 0;
}

int secureonly_check_secure (aChannel* chptr)
{
	if (IsSecureOnly(chptr))
	{
		return 1;
	}

	return 0;
}

void secureonly_channel_sync (aChannel* chptr, unsigned short merge, unsigned short removetheirs, unsigned short nomode)
{
	if ((!merge && !removetheirs && !nomode) || (merge && !nomode))
	{
		secureonly_kick_insecure_users(chptr);
	}
}

int secureonly_check_send (aClient *acptr, aChannel* chptr)
{
	if (IsSecureOnly(chptr))
		if (!IsSecure(acptr))
			return HOOK_DENY;

	return HOOK_CONTINUE;
}

int secureonly_check_sajoin (aClient *acptr, aChannel* chptr, aClient *sptr)
{
	if (IsSecureOnly(chptr) && !IsSecure(acptr))
	{
		sendnotice(sptr, "You cannot SAJOIN %s to %s because the channel is +z and the user is not connected via SSL",
			acptr->name, chptr->chname);
		return HOOK_DENY;
	}

	return HOOK_CONTINUE;
}

