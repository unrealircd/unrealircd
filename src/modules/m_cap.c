/*
 *   IRC - Internet Relay Chat, src/modules/m_cap.c
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

typedef int (*bqcmp)(const void *, const void *);

CMD_FUNC(m_cap);

#define MSG_CAP 	"CAP"

ModuleHeader MOD_HEADER(m_cap)
  = {
	"m_cap",	/* Name of module */
	"4.0", /* Version */
	"command /cap", /* Short description of module */
	"3.2-b8-1",
	NULL 
	};

MOD_INIT(m_cap)
{
	ClientCapability c;
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, MSG_CAP, m_cap, MAXPARA, M_UNREGISTERED|M_USER);

	memset(&c, 0, sizeof(c));
	c.name = "account-notify";
	c.cap = PROTO_ACCOUNT_NOTIFY;
	ClientCapabilityAdd(modinfo->handle, &c);
	
	memset(&c, 0, sizeof(c));
	c.name = "away-notify";
	c.cap = PROTO_AWAY_NOTIFY;
	ClientCapabilityAdd(modinfo->handle, &c);

	memset(&c, 0, sizeof(c));
	c.name = "multi-prefix";
	c.cap = PROTO_NAMESX;
	ClientCapabilityAdd(modinfo->handle, &c);

	memset(&c, 0, sizeof(c));
	c.name = "userhost-in-names";
	c.cap = PROTO_UHNAMES;
	ClientCapabilityAdd(modinfo->handle, &c);

	memset(&c, 0, sizeof(c));
	c.name = "cap-notify";
	c.cap = PROTO_CAP_NOTIFY;
	ClientCapabilityAdd(modinfo->handle, &c);

	memset(&c, 0, sizeof(c));
	c.name = "chghost";
	c.cap = PROTO_CAP_CHGHOST;
	ClientCapabilityAdd(modinfo->handle, &c);

	memset(&c, 0, sizeof(c));
	c.name = "extended-join";
	c.cap = PROTO_CAP_EXTENDED_JOIN;
	ClientCapabilityAdd(modinfo->handle, &c);

	return MOD_SUCCESS;
}

MOD_LOAD(m_cap)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_cap)
{
	return MOD_SUCCESS;
}

static ClientCapability *clicap_find(aClient *sptr, const char *data, int *negate, int *finished, int *errors)
{
	static char buf[BUFSIZE];
	static char *p;
	ClientCapability *cap;
	char *s;

	*negate = 0;

	if (data)
	{
		strlcpy(buf, data, sizeof(buf));
		p = buf;
		*finished = 0;
		*errors = 0;
	}

	if (*finished)
		return NULL;

	/* skip any whitespace */
	while(*p && isspace(*p))
		p++;

	if (BadPtr(p))
	{
		*finished = 1;
		return NULL;
	}

	if(*p == '-')
	{
		*negate = 1;
		p++;

		/* someone sent a '-' without a parameter.. */
		if(*p == '\0')
			return NULL;
	}

	if((s = strchr(p, ' ')))
		*s++ = '\0';

	cap = ClientCapabilityFind(p, sptr);
	if (!s)
		*finished = 1;
	
	p = s; /* point to next token for next iteration */

	if (cap && (cap->flags & CLICAP_FLAGS_ADVERTISE_ONLY))
		cap = NULL;

	if (!cap)
		*errors = 1;

	return cap;
}

static void clicap_generate(aClient *sptr, const char *subcmd, int flags)
{
	ClientCapability *cap;
	char buf[BUFSIZE];
	char capbuf[BUFSIZE];
	char *p;
	int buflen = 0;
	int curlen, mlen;
	size_t i;

	mlen = snprintf(buf, BUFSIZE, ":%s CAP %s %s", me.name,	BadPtr(sptr->name) ? "*" : sptr->name, subcmd);

	p = capbuf;
	buflen = mlen;

	if (flags == -1)
	{
		sendto_one(sptr, "%s :", buf);
		return;
	}

	for (cap = clicaps; cap; cap = cap->next)
	{
		char name[256];
		char *param;

		if (cap->visible && !cap->visible(sptr))
			continue; /* hidden */

		if ((sptr->local->cap_protocol >= 302) && cap->parameter && (param = cap->parameter(sptr)))
			snprintf(name, sizeof(name), "%s=%s", cap->name, param);
		else
			strlcpy(name, cap->name, sizeof(name));

		if (flags)
		{
			if (!cap->cap || !CHECKPROTO(sptr, cap->cap))
				continue;
		}

		/* \r\n\0, possible "-~=", space, " *" */
		if (buflen + strlen(name) >= BUFSIZE - 10)
		{
			if (buflen != mlen)
				*(p - 1) = '\0';
			else
				*p = '\0';

			sendto_one(sptr, "%s * :%s", buf, capbuf);
			p = capbuf;
			buflen = mlen;
		}

		curlen = snprintf(p, (capbuf + BUFSIZE) - p, "%s ", name);
		p += curlen;
		buflen += curlen;
	}

	if (buflen != mlen)
		*(p - 1) = '\0';
	else
		*p = '\0';

	sendto_one(sptr, "%s :%s", buf, capbuf);
}

static int cap_end(aClient *sptr, const char *arg)
{
	if (IsRegisteredUser(sptr))
		return 0;

	sptr->local->proto &= ~PROTO_CLICAP;

	if (*sptr->name && sptr->user && *sptr->user->username && IsNotSpoof(sptr))
		return register_user(sptr, sptr, sptr->name, sptr->user->username, NULL, NULL, NULL);

	return 0;
}

static int cap_list(aClient *sptr, const char *arg)
{
	clicap_generate(sptr, "LIST", sptr->local->proto ? sptr->local->proto : -1);
	return 0;
}

static int cap_ls(aClient *sptr, const char *arg)
{
	if (!IsRegisteredUser(sptr))
		sptr->local->proto |= PROTO_CLICAP;

	if (arg)
		sptr->local->cap_protocol = atoi(arg);

	/* Since the client did a "CAP LS" it apparently supports CAP
	 * and thus at least protocol version 300.
	 */
	if (sptr->local->cap_protocol < 300)
		sptr->local->cap_protocol = 300;

	if (sptr->local->cap_protocol >= 302)
		sptr->local->proto |= PROTO_CAP_NOTIFY; /* Implicit support (JIT) */

	clicap_generate(sptr, "LS", 0);
	return 0;
}

static int cap_req(aClient *sptr, const char *arg)
{
	char buf[BUFSIZE];
	char pbuf[2][BUFSIZE];
	ClientCapability *cap;
	int buflen, plen;
	int i = 0;
	int capadd = 0, capdel = 0;
	int finished = 0, negate;
	int errors = 0;

	if (!IsRegisteredUser(sptr))
		sptr->local->proto |= PROTO_CLICAP;

	if (BadPtr(arg))
		return 0;

	buflen = snprintf(buf, sizeof(buf), ":%s CAP %s ACK",
			  me.name, BadPtr(sptr->name) ? "*" : sptr->name);

	pbuf[0][0] = '\0';
	plen = 0;

	for(cap = clicap_find(sptr, arg, &negate, &finished, &errors); cap;
	    cap = clicap_find(sptr, NULL, &negate, &finished, &errors))
	{
		/* filled the first array, but cant send it in case the
		 * request fails.  one REQ should never fill more than two
		 * buffers --fl
		 */
		if (buflen + plen + strlen(cap->name) + 6 >= BUFSIZE)
		{
			pbuf[1][0] = '\0';
			plen = 0;
			i = 1;
		}

		if (negate)
		{
			strcat(pbuf[i], "-");
			plen++;

			capdel |= cap->cap;
		}
		else
		{
			capadd |= cap->cap;
		}

		strcat(pbuf[i], cap->name);
		strcat(pbuf[i], " ");
		plen += (strlen(cap->name) + 1);
	}

	/* This one is special */
	if ((sptr->local->cap_protocol >= 302) && (capdel & PROTO_CAP_NOTIFY))
		errors++; /* Reject "CAP REQ -cap-notify" */

	if (errors)
	{
		sendto_one(sptr, ":%s CAP %s NAK :%s", me.name, BadPtr(sptr->name) ? "*" : sptr->name, arg);
		return 0;
	}

	if (i)
	{
		sendto_one(sptr, "%s * :%s", buf, pbuf[0]);
		sendto_one(sptr, "%s :%s", buf, pbuf[1]);
	}
	else
		sendto_one(sptr, "%s :%s", buf, pbuf[0]);

	sptr->local->proto |= capadd;
	sptr->local->proto &= ~capdel;
	return 0;
}

struct clicap_cmd {
	const char *cmd;
	int (*func)(struct Client *source_p, const char *arg);
};

static struct clicap_cmd clicap_cmdtable[] = {
	{ "END",	cap_end		},
	{ "LIST",	cap_list	},
	{ "LS",		cap_ls		},
	{ "REQ",	cap_req		},
};

static int clicap_cmd_search(const char *command, struct clicap_cmd *entry)
{
	return strcmp(command, entry->cmd);
}

CMD_FUNC(m_cap)
{
	struct clicap_cmd *cmd;

	if (DISABLE_CAP)
	{
		/* I know nothing! */
		if (IsPerson(sptr))
			sendto_one(sptr, err_str(ERR_UNKNOWNCOMMAND), me.name, sptr->name, "CAP");
		else
			sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "CAP");
		return 0;
	}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, BadPtr(sptr->name) ? "*" : sptr->name,
			   "CAP");

		return 0;
	}

	if(!(cmd = bsearch(parv[1], clicap_cmdtable,
			   sizeof(clicap_cmdtable) / sizeof(struct clicap_cmd),
			   sizeof(struct clicap_cmd), (bqcmp) clicap_cmd_search)))
	{
		sendto_one(sptr, err_str(ERR_INVALIDCAPCMD),
			   me.name, BadPtr(sptr->name) ? "*" : sptr->name,
			   parv[1]);

		return 0;
	}

	return (cmd->func)(sptr, parv[2]);
}

