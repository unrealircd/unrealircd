/* 
 * IRC - Internet Relay Chat, src/modules/usermodes/privdeaf.c
 * usermode +D: makes it so you cannot receive private messages/notices
 * except from opers, U-lines and servers. -- Syzop
 * GPLv2 or later
 */


#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"usermodes/privdeaf",
	"1.2",
	"Private Messages Deaf (+D) -- by Syzop",
	"UnrealIRCd Team",
	"unrealircd-6",
};

static long UMODE_PRIVDEAF = 0;
static Umode *UmodePrivdeaf = NULL;

int privdeaf_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	UmodePrivdeaf = UmodeAdd(modinfo->handle, 'D', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_PRIVDEAF);
	if (!UmodePrivdeaf)
	{
		/* I use config_error() here because it's printed to stderr in case of a load
		 * on cmd line, and to all opers in case of a /rehash.
		 */
		config_error("privdeaf: Could not add usermode 'D': %s", ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	
	 HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_USER, 0, privdeaf_can_send_to_user);

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

int privdeaf_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype)
{
	if ((target->umodes & UMODE_PRIVDEAF) && !IsOper(client) &&
	    !IsULine(client) && !IsServer(client) && (client != target))
	{
		*errmsg = "User does not accept private messages";
		return HOOK_DENY;
	}
	return HOOK_CONTINUE;
}
