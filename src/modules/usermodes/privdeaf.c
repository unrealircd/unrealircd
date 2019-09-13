/*
 * usermode +D: makes it so you cannot receive private messages/notices
 * except from opers, U-lines and servers. -- Syzop
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"usermodes/privdeaf",
	"1.2",
	"Private Messages Deaf (+D) -- by Syzop",
	"UnrealIRCd Team",
	"unrealircd-5",
};

static long UMODE_PRIVDEAF = 0;
static Umode *UmodePrivdeaf = NULL;

char *privdeaf_checkmsg(Client *, Client *, char *, int);

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
	
	 HookAddPChar(modinfo->handle, HOOKTYPE_PRE_USERMSG, 0, privdeaf_checkmsg);

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

char *privdeaf_checkmsg(Client *sptr, Client *acptr, char *text, int notice)
{
	if ((acptr->umodes & UMODE_PRIVDEAF) && !IsOper(sptr) &&
	    !IsULine(sptr) && !IsServer(sptr) && (sptr != acptr))
	{
		sendnotice(sptr, "Message to '%s' not delivered: User does not accept private messages", acptr->name);
		return NULL;
	} else
		return text;
}
