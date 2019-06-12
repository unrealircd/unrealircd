/*
 *   IRC - Internet Relay Chat, src/modules/link-security.c
 *   (C) 2017 Syzop & The UnrealIRCd Team
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

/* Module header */
ModuleHeader MOD_HEADER(link_security)
  = {
	"link-security",
	"4.2",
	"Link Security CAP",
	"3.2-b8-1",
	NULL 
	};

/* Forward declarations */
char *link_security_md_serialize(ModData *m);
void link_security_md_unserialize(char *str, ModData *m);
EVENT(checklinksec);
char *link_security_capability_parameter(aClient *acptr);
CMD_FUNC(m_linksecurity);

/* Global variables */
ModDataInfo *link_security_md;
int local_link_security = -1;
int global_link_security = -1;
int effective_link_security = -1;

/** Module initalization */
MOD_INIT(link_security)
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM_RELOADABLE, 1);
	
	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "link-security";
	mreq.type = MODDATATYPE_CLIENT;
	mreq.serialize = link_security_md_serialize;
	mreq.unserialize = link_security_md_unserialize;
	mreq.sync = 1;
	link_security_md = ModDataAdd(modinfo->handle, mreq);
	if (!link_security_md)
	{
		config_error("Unable to ModDataAdd() -- too many 3rd party modules loaded perhaps?");
		abort();
	}
	
	CommandAdd(modinfo->handle, "LINKSECURITY", m_linksecurity, MAXPARA, M_USER);

	return MOD_SUCCESS;
}

MOD_LOAD(link_security)
{
	ClientCapabilityInfo cap;

	memset(&cap, 0, sizeof(cap));
	cap.name = "unrealircd.org/link-security";
	cap.flags = CLICAP_FLAGS_ADVERTISE_ONLY;
	cap.parameter = link_security_capability_parameter;
	ClientCapabilityAdd(modinfo->handle, &cap, NULL);

	EventAdd(modinfo->handle, "checklinksec", 2, 0, checklinksec, NULL);
	checklinksec(NULL);
	return MOD_SUCCESS;
}

MOD_UNLOAD(link_security)
{
	return MOD_SUCCESS;
}

/* Magic value to differentiate between "not set" and "zero".
 * Only used for internal moddata storage, not exposed
 * outside these two functions.
 */
#define LNKSECMAGIC 100

char *link_security_md_serialize(ModData *m)
{
	static char buf[32];
	if (m->i == 0)
		return NULL; /* not set */
	snprintf(buf, sizeof(buf), "%d", m->i - LNKSECMAGIC);
	return buf;
}

void link_security_md_unserialize(char *str, ModData *m)
{
	m->i = atoi(str) + LNKSECMAGIC;
}

/** Return 1 if the server certificate is verified for
 * server 'acptr', return 0 if not.
 */
int certificate_verification_active(aClient *acptr)
{
	ConfigItem_link *conf;
	
	if (!acptr->serv || !acptr->serv->conf)
		return 0; /* wtf? */
	conf = acptr->serv->conf;
	
	if (conf->verify_certificate)
		return 1; /* yes, verify-certificate is 'yes' */
	
	if ((conf->auth->type == AUTHTYPE_SSL_CLIENTCERT) ||
	    (conf->auth->type == AUTHTYPE_SSL_CLIENTCERTFP) ||
	    (conf->auth->type == AUTHTYPE_SPKIFP))
	{
		/* yes, verified by link::password being a
		 * certificate fingerprint or certificate file.
		 */
	    return 1;
	}

	return 0; /* no, certificate is not verified in any way */
}

/** Calculate our (local) link-security level.
 * This means stepping through the list of directly linked
 * servers and determining if they are linked via SSL and
 * certificate verification is active.
 * @returns value from 0 to 2.
 */
int our_link_security(void)
{
	aClient *acptr;
	int level = 2; /* safest */
	
	list_for_each_entry(acptr, &server_list, special_node)
	{
		if (IsLocal(acptr))
			continue; /* server connected via localhost */
		if (!IsSecure(acptr))
			return 0; /* Any non-SSL server (which is not localhost) results in level 0. */
		if (!certificate_verification_active(acptr))
			level = 1; /* downgrade to level 1 */
	}
	
	return level;
}

char *valtostr(int i)
{
	static char buf[32];
	snprintf(buf, sizeof(buf), "%d", i);
	return buf;
}

/** Check link security. This is called every X seconds to see if there
 * is a change, either local or network-wide.
 */
EVENT(checklinksec)
{
	int last_local_link_security = local_link_security;
	int last_global_link_security = global_link_security;
	aClient *acptr;
	char *s;
	int v;
	int warning_sent = 0;
	
	local_link_security = our_link_security();
	if (local_link_security != last_local_link_security)
	{
		/* Our own link-security changed (for better or worse),
		 * Set and broadcast it immediately to the other servers.
		 */
		moddata_client_set(&me, "link-security", valtostr(local_link_security));
	}

	global_link_security = 2;
	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		s = moddata_client_get(acptr, "link-security");
		if (s)
		{
			v = atoi(s);
			if (v == 0)
			{
				global_link_security = 0;
				break;
			}
			if (v == 1)
				global_link_security = 1;
		}
	}
	
	if (local_link_security < last_local_link_security)
	{
		sendto_realops("Local link-security downgraded from level %d to %d due to just linked in server(s)",
			last_local_link_security, local_link_security);
		warning_sent = 1;
	}
	
	if (global_link_security < last_global_link_security)
	{
		sendto_realops("Global link-security downgraded from level %d to %d due to just linked in server(s)",
			last_global_link_security, global_link_security);
		warning_sent = 1;
	}
	
	effective_link_security = MIN(local_link_security, global_link_security);

	if (warning_sent)
	{
		sendto_realops("Effective (network-wide) link-security is: level %d", effective_link_security);
		sendto_realops("More information about this can be found at https://www.unrealircd.org/docs/Link_security");
	}
}

char *link_security_capability_parameter(aClient *acptr)
{
	return valtostr(effective_link_security);
}

/** /LINKSECURITY command */
CMD_FUNC(m_linksecurity)
{
	aClient *acptr;
	char *s;
	int v;
	
	if (!IsOper(sptr))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}
	
	sendtxtnumeric(sptr, "== Link Security Report ==");
	
	sendtxtnumeric(sptr, "= By server =");
	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		v = -1;
		s = moddata_client_get(acptr, "link-security");
		if (s)
		{
			v = atoi(s);
			sendtxtnumeric(sptr, "%s: level %d", acptr->name, v);
		} else {
			sendtxtnumeric(sptr, "%s: level UNKNOWN", acptr->name);
		}
	}
	
	sendtxtnumeric(sptr, "-");
	sendtxtnumeric(sptr, "= Network =");
	sendtxtnumeric(sptr, "This results in an effective (network-wide) link-security of level %d", effective_link_security);
	sendtxtnumeric(sptr, "-");
	sendtxtnumeric(sptr, "= Legend =");
	sendtxtnumeric(sptr, "Higher level means better link security");
	sendtxtnumeric(sptr, "Level UNKNOWN: Not an UnrealIRCd server (eg: services) or an old version (<4.0.14)");
	sendtxtnumeric(sptr, "Level 0: One or more servers linked insecurely (not using SSL/TLS)");
	sendtxtnumeric(sptr, "Level 1: Servers are linked with SSL/TLS but at least one of them is not verifying certificates");
	sendtxtnumeric(sptr, "Level 2: Servers linked with SSL/TLS and certificates are properly verified");
	sendtxtnumeric(sptr, "-");
	sendtxtnumeric(sptr, "= More information =");
	sendtxtnumeric(sptr, "To understand more about link security and how to improve your level");
	sendtxtnumeric(sptr, "see https://www.unrealircd.org/docs/Link_security");
	return 0;
}
