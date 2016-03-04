/*
 *   IRC - Internet Relay Chat, src/modules/m_sasl.c
 *   (C) 2012 The UnrealIRCd Team
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

#define MSG_AUTHENTICATE "AUTHENTICATE"

#define MSG_SASL "SASL"

#define MSG_SVSLOGIN "SVSLOGIN"

/* returns a server identifier given agent_p */
#define AGENT_SID(agent_p)	(agent_p->user != NULL ? agent_p->user->server : agent_p->name)

ModuleHeader MOD_HEADER(m_sasl)
  = {
	"m_sasl",
	"4.0",
	"SASL", 
	"3.2-b8-1",
	NULL 
    };

/*
 * This is a "lightweight" SASL implementation/stack which uses psuedo-identifiers
 * to identify connecting clients.  In Unreal 3.3, we should use real identifiers
 * so that SASL sessions can be correlated.
 *
 * The following people were involved in making the current iteration of SASL over
 * IRC which allowed psuedo-identifiers:
 *
 * danieldg, Daniel de Graff <danieldg@inspircd.org>
 * jilles, Jilles Tjoelker <jilles@stack.nl>
 * Jobe, Matthew Beeching <jobe@mdbnet.co.uk>
 * gxti, Michael Tharp <gxti@partiallystapled.com>
 * nenolod, William Pitcock <nenolod@dereferenced.org>
 *
 * Thanks also to all of the client authors which have implemented SASL in their
 * clients.  With the backwards-compatibility layer allowing "lightweight" SASL
 * implementations, we now truly have a universal authentication mechanism for
 * IRC.
 */

/*
 * decode_puid
 *
 * Decode PUID sent from a SASL agent.  If the servername in the PUID doesn't match
 * ours, we reject the PUID (by returning NULL).
 */
static aClient *decode_puid(char *puid)
{
	aClient *cptr;
	char *it, *it2;
	int cookie = 0;

	if ((it = strrchr(puid, '!')) == NULL)
		return NULL;

	*it++ = '\0';

	if ((it2 = strrchr(it, '.')) != NULL)
	{
		*it2++ = '\0';
		cookie = atoi(it2);
	}

	if (stricmp(me.name, puid))
		return NULL;

	list_for_each_entry(cptr, &unknown_list, lclient_node)
		if (cptr->local->sasl_cookie == cookie)
			return cptr;

	return NULL;
}

/*
 * encode_puid
 *
 * Encode PUID based on aClient.
 */
static const char *encode_puid(aClient *client)
{
	static char buf[HOSTLEN + 20];

	/* create a cookie if necessary (and in case getrandom16 returns 0, then run again) */
	while (!client->local->sasl_cookie)
		client->local->sasl_cookie = getrandom16();

	snprintf(buf, sizeof buf, "%s!0.%d", me.name, client->local->sasl_cookie);

	return buf;
}

/*
 * SVSLOGIN message
 *
 * parv[1]: propagation mask
 * parv[2]: target PUID
 * parv[3]: ESVID
 */
CMD_FUNC(m_svslogin)
{
	if (!SASL_SERVER || MyClient(sptr) || (parc < 3) || !parv[3])
		return 0;

	if (!stricmp(parv[1], me.name))
	{
		aClient *target_p;

		/* is the PUID valid? */
		if ((target_p = decode_puid(parv[2])) == NULL)
			return 0;

		if (target_p->user == NULL)
			make_user(target_p);

		strlcpy(target_p->user->svid, parv[3], sizeof(target_p->user->svid));

		sendto_one(target_p, err_str(RPL_LOGGEDIN), me.name,
			   BadPtr(target_p->name) ? "*" : target_p->name,
			   BadPtr(target_p->name) ? "*" : target_p->name,
			   BadPtr(target_p->user->username) ? "*" : target_p->user->username,
			   BadPtr(target_p->user->realhost) ? "*" : target_p->user->realhost,
			   target_p->user->svid, target_p->user->svid);

		return 0;
	}

	/* not for us; propagate. */
	sendto_server(cptr, 0, 0, ":%s SVSLOGIN %s %s %s",
	    sptr->name, parv[1], parv[2], parv[3]);

	return 0;
}

/*
 * SASL message
 *
 * parv[1]: distribution mask
 * parv[2]: target PUID
 * parv[3]: mode/state
 * parv[4]: data
 * parv[5]: out-of-bound data
 */
CMD_FUNC(m_sasl)
{
	if (!SASL_SERVER || MyClient(sptr) || (parc < 4) || !parv[4])
		return 0;

	if (!stricmp(parv[1], me.name))
	{
		aClient *target_p;

		/* is the PUID valid? */
		if ((target_p = decode_puid(parv[2])) == NULL)
			return 0;

		if (target_p->user == NULL)
			make_user(target_p);

		/* reject if another SASL agent is answering */
		if (*target_p->local->sasl_agent && stricmp(sptr->name, target_p->local->sasl_agent))
			return 0;
		else
			strlcpy(target_p->local->sasl_agent, sptr->name, sizeof(target_p->local->sasl_agent));

		if (*parv[3] == 'C')
			sendto_one(target_p, "AUTHENTICATE %s", parv[4]);
		else if (*parv[3] == 'D')
		{
			if (*parv[4] == 'F')
				sendto_one(target_p, err_str(ERR_SASLFAIL), me.name, BadPtr(target_p->name) ? "*" : target_p->name);
			else if (*parv[4] == 'S')
			{
				target_p->local->sasl_complete++;
				sendto_one(target_p, err_str(RPL_SASLSUCCESS), me.name, BadPtr(target_p->name) ? "*" : target_p->name);
			}

			*target_p->local->sasl_agent = '\0';
		}

		return 0;
	}

	/* not for us; propagate. */
	sendto_server(cptr, 0, 0, ":%s SASL %s %s %c %s %s",
	    sptr->name, parv[1], parv[2], *parv[3], parv[4], parc > 5 ? parv[5] : "");

	return 0;
}

/*
 * AUTHENTICATE message
 *
 * parv[1]: data
 */
CMD_FUNC(m_authenticate)
{
	aClient *agent_p = NULL;

	/* Failing to use CAP REQ for sasl is a protocol violation. */
	if (!SASL_SERVER || !MyConnect(sptr) || BadPtr(parv[1]) || !CHECKPROTO(sptr, PROTO_SASL))
		return 0;

	if (sptr->local->sasl_complete)
	{
		sendto_one(sptr, err_str(ERR_SASLALREADY), me.name, BadPtr(sptr->name) ? "*" : sptr->name);
		return 0;
	}

	if (strlen(parv[1]) > 400)
	{
		sendto_one(sptr, err_str(ERR_SASLTOOLONG), me.name, BadPtr(sptr->name) ? "*" : sptr->name);
		return 0;
	}

	if (*sptr->local->sasl_agent)
		agent_p = find_client(sptr->local->sasl_agent, NULL);

	if (agent_p == NULL)
		sendto_server(NULL, 0, 0, ":%s SASL %s %s S %s",
		    me.name, SASL_SERVER, encode_puid(sptr), parv[1]);
	else
		sendto_server(NULL, 0, 0, ":%s SASL %s %s C %s",
		    me.name, AGENT_SID(agent_p), encode_puid(sptr), parv[1]);

	sptr->local->sasl_out++;

	return 0;
}

static int abort_sasl(aClient *cptr)
{
	if (cptr->local->sasl_out == 0 || cptr->local->sasl_complete)
		return 0;

	cptr->local->sasl_out = cptr->local->sasl_complete = 0;
	sendto_one(cptr, err_str(ERR_SASLABORTED), me.name, BadPtr(cptr->name) ? "*" : cptr->name);

	if (*cptr->local->sasl_agent)
	{
		aClient *agent_p = find_client(cptr->local->sasl_agent, NULL);

		if (agent_p != NULL)
		{
			sendto_server(NULL, 0, 0, ":%s SASL %s %s D A",
			    me.name, AGENT_SID(agent_p), encode_puid(cptr));
			return 0;
		}
	}

	sendto_server(NULL, 0, 0, ":%s SASL * %s D A", me.name, encode_puid(cptr));
	return 0;
}

int sasl_capability_visible(void)
{
	if (!SASL_SERVER || !find_server(SASL_SERVER, NULL))
		return 0;
	
	return 1;
}

int sasl_connect(aClient *sptr)
{
	return abort_sasl(sptr);
}

int sasl_quit(aClient *sptr, char *comment)
{
	return abort_sasl(sptr);
}

MOD_INIT(m_sasl)
{
	ClientCapability cap;
	
	MARK_AS_OFFICIAL_MODULE(modinfo);

	CommandAdd(modinfo->handle, MSG_SASL, m_sasl, MAXPARA, M_USER|M_SERVER);
	CommandAdd(modinfo->handle, MSG_SVSLOGIN, m_svslogin, MAXPARA, M_USER|M_SERVER);
	CommandAdd(modinfo->handle, MSG_AUTHENTICATE, m_authenticate, MAXPARA, M_UNREGISTERED);

	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, sasl_connect);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, sasl_quit);

	memset(&cap, 0, sizeof(cap));
	cap.name = "sasl";
	cap.cap = PROTO_SASL;
	cap.visible = sasl_capability_visible;
	ClientCapabilityAdd(modinfo->handle, &cap);

	return MOD_SUCCESS;
}

MOD_LOAD(m_sasl)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_sasl)
{
	return MOD_SUCCESS;
}
