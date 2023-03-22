/* src/modules/svso.c - Grant IRCOp rights (for Services)
 * (C) Copyright 2022 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */
#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"svso",
	"6.0.0",
	"Grant oper privileges via SVSO services command",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
CMD_FUNC(cmd_svso);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	CommandAdd(modinfo->handle, "SVSO", cmd_svso, MAXPARA, CMD_USER|CMD_SERVER);
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

/* Syntax: SVSO <uid|nick> <oper account> <operclass> <class> <modes> <snomask> <vhost>
 * All these parameters need to be set, you cannot leave any of them out,
 * HOWEVER some can be set to "-" to skip setting them, this is true for:
 * <class>, <modes>, <snomask>, <vhost>
 *
 * In UnrealIRCd the <operclass> will be prefixed by "services:" if not already
 * present. It is up to you to include or omit it.
 */
CMD_FUNC(cmd_svso)
{
	Client *acptr;
	char oper_account[64];
	const char *operclass;
	const char *clientclass;
	ConfigItem_class *clientclass_c;
	const char *modes;
	long modes_i = 0;
	const char *snomask;
	const char *vhost;

	if (!IsSvsCmdOk(client))
		return;

	if ((parc < 8) || BadPtr(parv[7]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SVSO");
		return;
	}

	operclass = parv[3];
	clientclass = parv[4];
	modes = parv[5];
	snomask = parv[6];
	vhost = parv[7];

	acptr = find_user(parv[1], NULL);
	if (!acptr)
	{
		sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
		return;
	}

	if (!MyUser(acptr))
	{
		/* Forward it to the correct server, and we are done... */
		sendto_one(acptr, recv_mtags, ":%s SVSO %s %s %s %s %s %s %s",
		           client->name, acptr->id, parv[2], parv[3], parv[4], parv[5], parv[6], parv[7]);
		return;
	}

	/* CAVEAT ALERT !
	 * Don't mix up 'client' and 'acptr' below...
	 * 'client' is the server or services pseudouser that requests the change
	 * 'acptr' is the person that will be made OPER
	 */

	/* If we get here, we validate the request and then make the user oper. */
	if (!find_operclass(operclass))
	{
		sendnumeric(client, ERR_CANNOTDOCOMMAND, "SVSO", "Operclass does not exist");
		return;
	}

	/* Set any items to NULL if they are skipped (on request) */
	if (!strcmp(clientclass, "-"))
		clientclass = NULL;
	if (!strcmp(modes, "-"))
		modes = NULL;
	if (!strcmp(snomask, "-"))
		snomask = NULL;
	if (!strcmp(vhost, "-"))
		vhost = NULL;

	/* First, maybe the user is oper already? Then de-oper them.. */
	if (IsOper(acptr))
	{
		int was_hidden_oper = IsHideOper(acptr) ? 1 : 0;

		list_del(&acptr->special_node);
		RunHook(HOOKTYPE_LOCAL_OPER, acptr, 0, NULL, NULL);
		remove_oper_privileges(acptr, 1);

		if (!was_hidden_oper)
			irccounts.operators--;
		VERIFY_OPERCOUNT(acptr, "svso");

	}

	if (vhost)
		sendnumeric(client, ERR_CANNOTDOCOMMAND, "SVSO", "Failed to make user oper: vhost contains illegal characters or is too long");

	/* Prefix the oper block name with "remote:" if it hasn't already */
	if (!strncmp(parv[2], "remote:", 7))
		strlcpy(oper_account, parv[2], sizeof(oper_account));
	else
		snprintf(oper_account, sizeof(oper_account), "remote:%s", parv[2]);

	/* These needs to be looked up... */
	clientclass_c = find_class(clientclass); /* NULL is fine! */
	if (modes)
		modes_i = set_usermode(modes);

	if (!make_oper(acptr, oper_account, operclass, clientclass_c, modes_i, snomask, vhost))
		sendnumeric(client, ERR_CANNOTDOCOMMAND, "SVSO", "Failed to make user oper");
}
