/*
 *   IRC - Internet Relay Chat, src/modules/m_protoctl.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(m_protoctl);

#define MSG_PROTOCTL 	"PROTOCTL"	

ModuleHeader MOD_HEADER(m_protoctl)
  = {
	"m_protoctl",
	"4.0",
	"command /protoctl", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_protoctl)
{
	CommandAdd(modinfo->handle, MSG_PROTOCTL, m_protoctl, MAXPARA, M_UNREGISTERED|M_SERVER|M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_protoctl)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_protoctl)
{
	return MOD_SUCCESS;
}

/*
 * m_protoctl
 *	parv[1+] = Options
 */
CMD_FUNC(m_protoctl)
{
	int  i;
#ifndef PROTOCTL_MADNESS
	int  remove = 0;
#endif
	int first_protoctl = (GotProtoctl(sptr)) ? 0 : 1; /**< First PROTOCTL we receive? Special ;) */
	char proto[128], *s;
/*	static char *dummyblank = "";	Yes, it is kind of ugly */

	if (!MyConnect(sptr))
		return 0; /* Remote PROTOCTL's are not supported at this time */

#ifdef PROTOCTL_MADNESS
	if (GotProtoctl(sptr))
	{
		/*
		 * But we already GOT a protoctl msg!
		 */
		if (!IsServer(sptr))
			sendto_one(cptr,
			    "ERROR :Already got a PROTOCTL from you.");
		return 0;
	}
#endif
	cptr->flags |= FLAGS_PROTOCTL;
	/* parv[parc - 1] */
	for (i = 1; i < parc; i++)
	{
		strlcpy(proto, parv[i], sizeof proto);
		s = proto;
#ifndef PROTOCTL_MADNESS
		if (*s == '-')
		{
			s++;
			remove = 1;
		}
		else
			remove = 0;
#endif
/*		equal = (char *)index(proto, '=');
		if (equal == NULL)
			options = dummyblank;
		else
		{
			options = &equal[1];
			equal[0] = '\0';
		}
*/
		if (!strcmp(s, "NAMESX"))
		{
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s", proto, cptr->name));
			SetNAMESX(cptr);
		}
		if (!strcmp(s, "UHNAMES") && UHNAMES_ENABLED)
		{
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s", proto, cptr->name));
			SetUHNAMES(cptr);
		}
		else if (strcmp(s, "NOQUIT") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearNoQuit(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetNoQuit(cptr);

		}
		else if (strcmp(s, "SJOIN") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearSJOIN(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetSJOIN(cptr);
		}
		else if (strcmp(s, "SJOIN2") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearSJOIN2(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetSJOIN2(cptr);
		}
		else if (strcmp(s, "NICKv2") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearNICKv2(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetNICKv2(cptr);
		}
		else if (strcmp(s, "UMODE2") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearUMODE2(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetUMODE2(cptr);
		}
		else if (strcmp(s, "VL") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearVL(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetVL(cptr);
		}
		else if (strcmp(s, "VHP") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearVHP(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetVHP(cptr);
		}
		else if (strcmp(s, "CLK") == 0)
		{
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetCLK(cptr);
		}
		else if (strcmp(s, "SJ3") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearSJ3(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetSJ3(cptr);
		}
		else if (strcmp(s, "TKLEXT") == 0)
		{
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s", proto, cptr->name));
			SetTKLEXT(cptr);
		}
		else if (strcmp(s, "TKLEXT2") == 0)
		{
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s", proto, cptr->name));
			SetTKLEXT2(cptr);
			SetTKLEXT(cptr); /* TKLEXT is implied as well. always. */
		}
		else if (strcmp(s, "NICKIP") == 0)
		{
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s", proto, cptr->name));
			cptr->local->proto |= PROTO_NICKIP;
		}
		else if (strncmp(s, "NICKCHARS=", 10) == 0)
		{
			if (!IsServer(cptr) && !IsEAuth(cptr) && !IsHandshake(cptr))
				continue;
			/* Ok, server is either authenticated, or is an outgoing connect...
			 * We now compare the character sets to see if we should warn opers about any mismatch...
			 */
			if (strcmp(s+10, langsinuse))
			{
				sendto_realops("\002WARNING!!!!\002 Link %s does not have the same set::allowed-nickchars settings (or is "
							"a different UnrealIRCd version), this MAY cause display issues. Our charset: '%s', theirs: '%s'",
					get_client_name(cptr, FALSE), langsinuse, s+10);
				/* return exit_client(cptr, cptr, &me, "Nick charset mismatch"); */
			}
		}
		else if (strncmp(s, "SID=", 4) == 0)
		{
			aClient *acptr;
			char *sid = s + 4;

			if (!IsServer(cptr) && !IsEAuth(cptr) && !IsHandshake(cptr))
				return exit_client(cptr, cptr, &me, "Got PROTOCTL SID before EAUTH, that's the wrong order!");

			if ((acptr = hash_find_id(sid, NULL)) != NULL)
			{
				sendto_one(sptr, "ERROR :SID %s already exists from %s", acptr->id, acptr->name);
				sendto_snomask(SNO_SNOTICE, "Link %s rejected - SID %s already exists from %s",
						get_client_name(cptr, FALSE), acptr->id, acptr->name);
				return exit_client(cptr, cptr, &me, "SID collision");
			}

			strlcpy(cptr->id, sid, IDLEN);
			add_to_id_hash_table(cptr->id, cptr);
			cptr->local->proto |= PROTO_SID;
		}
		else if ((strncmp(s, "EAUTH=", 6) == 0) && NEW_LINKING_PROTOCOL)
		{
			/* Early authorization: EAUTH=servername,protocol,flags,versiontext
			 * (Only servername is mandatory, rest is optional)
			 */
			int ret;
			char *p;
			char *servername = NULL, *protocol = NULL, *flags = NULL, *versiontext = NULL;
			char buf[512];
			ConfigItem_link *aconf = NULL;

			strlcpy(buf, s+6, sizeof(buf));
			p = strchr(buf, ' ');
			if (p)
			{
				*p = '\0';
				p = NULL;
			}
			
			servername = strtoken(&p, buf, ",");
			if (!servername || (strlen(servername) > HOSTLEN) || !index(servername, '.'))
			{
				sendto_one(sptr, "ERROR :Bogus server name in EAUTH (%s)", servername);
				sendto_snomask
				    (SNO_JUNK,
				    "WARNING: Bogus server name (%s) from %s in EAUTH (maybe just a fishy client)",
				    servername, get_client_name(cptr, TRUE));

				return exit_client(cptr, sptr, &me, "Bogus server name");
			}
			
			
			protocol = strtoken(&p, NULL, ",");
			if (protocol)
			{
				flags = strtoken(&p, NULL, ",");
				if (flags)
				{
					versiontext = strtoken(&p, NULL, ",");
				}
			}
			
			ret = verify_link(cptr, sptr, servername, &aconf);
			if (ret < 0)
				return ret; /* FLUSH_BUFFER */

			/* note: versiontext, protocol and flags may be NULL */
			ret = check_deny_version(sptr, versiontext, protocol ? atoi(protocol) : 0, flags);
			if (ret < 0)
				return ret; /* FLUSH_BUFFER */

			SetEAuth(cptr);
			make_server(cptr); /* allocate and set cptr->serv */
			if (protocol)
				cptr->serv->features.protocol = atoi(protocol);
			if (!IsHandshake(cptr) && aconf) /* Send PASS early... */
				sendto_one(sptr, "PASS :%s", (aconf->auth->type == AUTHTYPE_PLAINTEXT) ? aconf->auth->data : "*");
		}
		else if ((strncmp(s, "SERVERS=", 8) == 0) && NEW_LINKING_PROTOCOL)
		{
			aClient *acptr, *srv;
			char *sid = NULL;
			
			if (!IsEAuth(cptr))
				continue;
				
			if (cptr->serv->features.protocol < 2351)
				continue; /* old SERVERS= version */
			
			/* Other side lets us know which servers are behind it.
			 * SERVERS=<sid-of-server-1>[,<sid-of-server-2[,..etc..]]
			 * Eg: SERVER=001,002,0AB,004,005
			 */

			add_pending_net(sptr, s+8);

			acptr = find_non_pending_net_duplicates(sptr);
			if (acptr)
			{
				sendto_one(sptr, "ERROR :Server with SID %s (%s) already exists",
					acptr->id, acptr->name);
				sendto_realops("Link %s cancelled, server with SID %s (%s) already exists",
					get_client_name(acptr, TRUE), acptr->id, acptr->name);
				return exit_client(sptr, sptr, sptr, "Server Exists (or non-unique me::sid)");
			}
			
			acptr = find_pending_net_duplicates(sptr, &srv, &sid);
			if (acptr)
			{
				sendto_one(sptr, "ERROR :Server with SID %s is being introduced by another server as well. "
				                 "Just wait a moment for it to synchronize...", sid);
				sendto_realops("Link %s cancelled, server would introduce server with SID %s, which "
				               "server %s is also about to introduce. Just wait a moment for it to synchronize...",
				               get_client_name(acptr, TRUE), sid, get_client_name(srv, TRUE));
				return exit_client(sptr, sptr, sptr, "Server Exists (just wait a moment)");
			}

			/* Send our PROTOCTL SERVERS= back if this was NOT a response */
			if (s[8] != '*')
				send_protoctl_servers(sptr, 1);
		}
		else if ((strncmp(s, "TS=",3) == 0) && (IsServer(sptr) || IsEAuth(sptr)))
		{
			long t = atol(s+3);
			char msg[512], linkerr[512];
			
			if (t < 10000)
				continue; /* ignore */
			
			*msg = *linkerr = '\0';
#define MAX_SERVER_TIME_OFFSET 60
			
			if ((TStime() - t) > MAX_SERVER_TIME_OFFSET)
			{
				snprintf(linkerr, sizeof(linkerr), "Your clock is %ld seconds behind my clock. Please verify both your clock and mine, fix it and try linking again.", TStime() - t);
				snprintf(msg, sizeof(msg), "Rejecting link %s: our clock is %ld seconds ahead. Correct time is very important in IRC. Please verify the clock on both %s (them) and %s (us), fix it and then try linking again",
					get_client_name(cptr, TRUE), TStime() - t, sptr->name, me.name);
			} else
			if ((t - TStime()) > MAX_SERVER_TIME_OFFSET)
			{
				snprintf(linkerr, sizeof(linkerr), "Your clock is %ld seconds ahead of my clock. Please verify both your clock and mine, fix it, and try linking again.", t - TStime());
				snprintf(msg, sizeof(msg), "Rejecting link %s: our clock is %ld seconds behind. Correct time is very important in IRC. Please verify the clock on both %s (them) and %s (us), fix it and then try linking again",
					get_client_name(cptr, TRUE), t - TStime(), sptr->name, me.name);
			}
			
			if (*msg)
			{
				sendto_realops("%s", msg);
				ircd_log(LOG_ERROR, "%s", msg);
				return exit_client(sptr, sptr, sptr, linkerr);
			}
		}
		else if ((strcmp(s, "MLOCK")) == 0)
		{
#ifdef PROTOCTL_MADNESS
			if (remove)
			{
				cptr->local->proto &= ~PROTO_MLOCK;
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s", proto, cptr->name));
			cptr->local->proto |= PROTO_MLOCK;
		}
		else if ((strncmp(s, "CHANMODES=", 10) == 0) && sptr->serv)
		{
			char *ch = s + 4;
			char *modes, *p;
			char copy[256];
			
			strlcpy(copy, s+10, sizeof(copy));
			
			modes = strtoken(&p, copy, ",");
			if (modes)
			{
				safestrdup(sptr->serv->features.chanmodes[0], modes);
				modes = strtoken(&p, NULL, ",");
				if (modes)
				{
					safestrdup(sptr->serv->features.chanmodes[1], modes);
					modes = strtoken(&p, NULL, ",");
					if (modes)
					{
						safestrdup(sptr->serv->features.chanmodes[2], modes);
						modes = strtoken(&p, NULL, ",");
						if (modes)
						{
							safestrdup(sptr->serv->features.chanmodes[3], modes);
						}
					}
				}
			}
		}
		else if (!strcmp(s, "EXTSWHOIS"))
		{
#ifdef PROTOCTL_MADNESS
			if (remove)
			{
				cptr->local->proto &= ~PROTO_EXTSWHOIS;
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s", proto, cptr->name));
			cptr->local->proto |= PROTO_EXTSWHOIS;
		}

		/*
		 * Add other protocol extensions here, with proto
		 * containing the base option, and options containing
		 * what it equals, if anything.
		 *
		 * DO NOT error or warn on unknown proto; we just don't
		 * support it.
		 */
	}

	if (first_protoctl && IsHandshake(cptr) && sptr->serv && !IsServerSent(sptr)) /* first & outgoing connection to server */
	{
		/* SERVER message moved from completed_connection() to here due to EAUTH/SERVERS PROTOCTL stuff,
		 * which needed to be delayed until after both sides have received SERVERS=xx (..or not.. in case
		 * of older servers).
		 * Actually, this is often not reached, as the PANGPANG stuff in numeric.c is reached first.
		 */
		send_server_message(cptr);
	}

	return 0;
}
