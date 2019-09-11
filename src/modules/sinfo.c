/*
 * m_sinfo - Server information
 * (C) Copyright 2019 Bram Matthys (Syzop) and the UnrealIRCd team.
 * License: GPLv2
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER(sinfo)
  = {
	"sinfo",
	"5.0",
	"Server information",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

/* Forward declarations */
CMD_FUNC(m_sinfo);

MOD_INIT(sinfo)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, "SINFO", m_sinfo, MAXPARA, M_USER|M_SERVER);

	return MOD_SUCCESS;
}

MOD_LOAD(sinfo)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(sinfo)
{
	return MOD_SUCCESS;
}

/** SINFO server-to-server command.
 * Technical documentation is available at:
 * https://www.unrealircd.org/docs/Server_protocol:SINFO_command
 * ^ contains important remarks regarding when to send it and when not.
 */
int sinfo_server(Client *cptr, Client *sptr, int parc, char *parv[])
{
	char buf[512];

	if (cptr == sptr)
	{
		/* It is a protocol violation to send an SINFO for yourself,
		 * eg if you are server 001, then you cannot send :001 SINFO ....
		 * Exiting the client may seem harsh, but this way we force users
		 * to use the correct protocol. If we would not do this then some
		 * services coders may think they should use only SINFO while in
		 * fact for directly connected servers they should use things like
		 * PROTOCTL CHANMODES=... USERMODES=... NICKCHARS=.... etc, and
		 * failure to do so will lead to potential desyncs or other major
		 * issues.
		 */
		return exit_client(cptr, sptr, &me, NULL, "Protocol error: you cannot send SINFO about yourself");
	}

	/* :SID SINFO up_since protocol umodes chanmodes nickchars :software name
	 *               1        2        3      4        5        6 (last one)
	 * If we extend it then 'software name' will still be the last one, so
	 * it may become 7, 8 or 9. New elements are inserted right before it.
	 */

	if ((parc < 6) || BadPtr(parv[6]))
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "SINFO");
		return 0;
	}

	sptr->serv->boottime = atol(parv[1]);
	sptr->serv->features.protocol = atoi(parv[2]);

	if (!strcmp(parv[3], "*"))
		safefree(sptr->serv->features.usermodes);
	else
		safestrdup(sptr->serv->features.usermodes, parv[3]);

	if (!strcmp(parv[4], "*"))
	{
		safefree(sptr->serv->features.chanmodes[0]);
		safefree(sptr->serv->features.chanmodes[1]);
		safefree(sptr->serv->features.chanmodes[2]);
		safefree(sptr->serv->features.chanmodes[3]);
	} else {
		parse_chanmodes_protoctl(sptr, parv[4]);
	}

	if (!strcmp(parv[5], "*"))
		safefree(sptr->serv->features.nickchars);
	else
		safestrdup(sptr->serv->features.nickchars, parv[5]);

	/* Software is always the last parameter. It is currently parv[6]
	 * but may change later. So always use parv[parc-1].
	 */
	if (!strcmp(parv[parc-1], "*"))
		safefree(sptr->serv->features.software);
	else
		safestrdup(sptr->serv->features.software, parv[parc-1]);

	/* Broadcast to 'the other side' of the net */
	concat_params(buf, sizeof(buf), parc, parv);
	sendto_server(cptr, 0, 0, NULL, ":%s SINFO %s", sptr->name, buf);

	return 0;
}

#define SafeDisplayStr(x)  ((x && *(x)) ? (x) : "-")
int sinfo_user(Client *cptr, Client *sptr, int parc, char *parv[])
{
	Client *acptr;

	if (!IsOper(sptr))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		sendtxtnumeric(sptr, "*** Server %s:", acptr->name);
		sendtxtnumeric(sptr, "Protocol: %d",
		               acptr->serv->features.protocol);
		sendtxtnumeric(sptr, "Software: %s",
		               SafeDisplayStr(acptr->serv->features.software));
		if (!acptr->serv->boottime)
		{
			sendtxtnumeric(sptr, "Up since: -");
			sendtxtnumeric(sptr, "Uptime: -");
		} else {
			sendtxtnumeric(sptr, "Up since: %s",
			               pretty_date(acptr->serv->boottime));
			sendtxtnumeric(sptr, "Uptime: %s",
			               pretty_time_val(TStime() - acptr->serv->boottime));
		}
		sendtxtnumeric(sptr, "User modes: %s",
		               SafeDisplayStr(acptr->serv->features.usermodes));
		if (!acptr->serv->features.chanmodes[0])
		{
			sendtxtnumeric(sptr, "Channel modes: -");
		} else {
			sendtxtnumeric(sptr, "Channel modes: %s,%s,%s,%s",
			               SafeDisplayStr(acptr->serv->features.chanmodes[0]),
			               SafeDisplayStr(acptr->serv->features.chanmodes[1]),
			               SafeDisplayStr(acptr->serv->features.chanmodes[2]),
			               SafeDisplayStr(acptr->serv->features.chanmodes[3]));
		}
		sendtxtnumeric(sptr, "Allowed nick characters: %s",
		               SafeDisplayStr(acptr->serv->features.nickchars));
	}

	return 0;
}

CMD_FUNC(m_sinfo)
{
	if (IsServer(sptr))
		return sinfo_server(cptr, sptr, parc, parv);
	else if (MyUser(sptr))
		return sinfo_user(cptr, sptr, parc, parv);
	return 0;
}
