
/*-
 * Copyright (c) 1985, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_init.c	6.14.1 (Berkeley) 6/27/90";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <stdio.h>
#include "config.h"		/* To get #define SOL20         Vesa */
#include "sys.h"
#include "common.h"
#include "nameser.h"
#include "resolv.h"

/*
 * Resolver state default settings
 */

struct state _res = {
	RES_TIMEOUT,		/* retransmition time interval */
	4,			/* number of times to retransmit */
	RES_DEFAULT,		/* options flags */
	1,			/* number of name servers */
};

/*
 * Set up default settings.  If the configuration file exist, the values
 * there will have precedence.  Otherwise, the server address is set to
 * INADDR_ANY and the default domain name comes from the gethostname().
 *
 * The configuration file should only be used if you want to redefine your
 * domain or run without a server on your machine.
 *
 * Return 0 if completes successfully, -1 on error
 */
res_init()
{
#ifndef _WIN32
	register FILE *fp;
	register char *cp, *dp, **pp;
	extern u_long inet_addr();
#else
	register char *cp, **pp;
#endif
	register int n;
	char buf[BUFSIZ];
	extern char *getenv();
	int  nserv = 0;		/* number of nameserver records read from file */
	int  norder = 0;
	int  haveenv = 0;
	int  havesearch = 0;

	_res.nsaddr.SIN_ADDR.S_ADDR = INADDR_ANY;
	_res.nsaddr.SIN_FAMILY = AFINET;
	_res.nsaddr.SIN_PORT = htons(NAMESERVER_PORT);
	_res.nscount = 1;

	/* Allow user to override the local domain definition */
	if ((cp = getenv("LOCALDOMAIN")) != NULL)
	{
		(void)strncpy(_res.defdname, cp, sizeof(_res.defdname));
		haveenv++;
	}

#ifndef _WIN32
	if ((fp = fopen(_PATH_RESCONF, "r")) != NULL)
	{
		/* read the config file */
		while (fgets(buf, sizeof(buf), fp) != NULL)
		{
			/* read default domain name */
			if (!strncmp(buf, "domain", sizeof("domain") - 1))
			{
				if (haveenv)	/* skip if have from environ */
					continue;
				cp = buf + sizeof("domain") - 1;
				while (*cp == ' ' || *cp == '\t')
					cp++;
				if ((*cp == '\0') || (*cp == '\n'))
					continue;

				(void)strncpy(_res.defdname, cp,
				    sizeof(_res.defdname) - 1);
				if ((cp = index(_res.defdname, '\n')) != NULL)
					*cp = '\0';
				havesearch = 0;
				continue;
			}
			/* set search list */
			if (!strncmp(buf, "search", sizeof("search") - 1))
			{
				if (haveenv)	/* skip if have from environ */
					continue;
				cp = buf + sizeof("search") - 1;
				while (*cp == ' ' || *cp == '\t')
					cp++;
				if ((*cp == '\0') || (*cp == '\n'))
					continue;

				(void)strncpy(_res.defdname, cp,
				    sizeof(_res.defdname) - 1);
				if ((cp = index(_res.defdname, '\n')) != NULL)
					*cp = '\0';
				/*
				 * Set search list to be blank-separated strings
				 * on rest of line.
				 */
				cp = _res.defdname;
				pp = _res.dnsrch;
				*pp++ = cp;
				for (n = 0; *cp && pp < _res.dnsrch + MAXDNSRCH;
				    cp++)
				{
					if (*cp == ' ' || *cp == '\t')
					{
						*cp = 0;
						n = 1;
					}
					else if (n)
					{
						*pp++ = cp;
						n = 0;
					}
				}
				/* null terminate last domain if there are excess */
				while (*cp != '\0' && *cp != ' ' && *cp != '\t')
					cp++;
				*cp = '\0';
				*pp++ = 0;
				havesearch = 1;
				continue;
			}
			/* read nameservers to query */
			if (!strncmp(buf, "nameserver",
			    sizeof("nameserver") - 1) && nserv < MAXNS)
			{
				cp = buf + sizeof("nameserver") - 1;
				while (*cp == ' ' || *cp == '\t')
					cp++;
				if ((*cp == '\0') || (*cp == '\n'))
					continue;
				if ((_res.nsaddr_list[nserv].SIN_ADDR.S_ADDR =
				    inet_addr(cp)) == (unsigned)-1)
				{
					_res.nsaddr_list[nserv].SIN_ADDR.S_ADDR
					    = INADDR_ANY;
					continue;
				}
				_res.nsaddr_list[nserv].SIN_FAMILY = AFINET;
				_res.nsaddr_list[nserv].SIN_PORT =
				    htons(NAMESERVER_PORT);
				nserv++;
				continue;
			}
			/* read service order */
			if (!strncmp(buf, "order", sizeof("order") - 1))
			{
				cp = buf + sizeof("order") - 1;
				while (*cp == ' ' || *cp == '\t')
					cp++;
				if ((*cp == '\0') || (*cp == '\n'))
					continue;
				norder = 0;
				do
				{
					if ((dp = index(cp, ',')) != NULL)
						*dp = '\0';
					if (norder >= MAXSERVICES)
						continue;
					if (!strncmp(cp, "bind",
					    sizeof("bind") - 1))
						_res.order[norder++] =
						    RES_SERVICE_BIND;
					else if (!strncmp(cp, "local",
					    sizeof("local") - 1))
						_res.order[norder++] =
						    RES_SERVICE_LOCAL;
					cp = dp + 1;
				}
				while (dp != NULL);
				_res.order[norder] = RES_SERVICE_NONE;
				continue;
			}
		}
		if (nserv > 1)
			_res.nscount = nserv;
		(void)fclose(fp);
	}
#endif /*_WIN32*/
	if (_res.defdname[0] == 0)
	{
		if (gethostname(buf, sizeof(_res.defdname)) == 0 &&
		    (cp = index(buf, '.')))
			(void)strcpy(_res.defdname, cp + 1);
	}

	/* find components of local domain that might be searched */
	if (havesearch == 0)
	{
		pp = _res.dnsrch;
		*pp++ = _res.defdname;
		for (cp = _res.defdname, n = 0; *cp; cp++)
			if (*cp == '.')
				n++;
		cp = _res.defdname;
		for (; n >= LOCALDOMAINPARTS && pp < _res.dnsrch + MAXDFLSRCH;
		    n--)
		{
			cp = index(cp, '.');
			*pp++ = ++cp;
		}
		*pp++ = 0;
	}
	/* default search order to bind only */
	if (norder == 0)
	{
		_res.order[0] = RES_SERVICE_BIND;
		_res.order[1] = RES_SERVICE_NONE;
	}
	_res.options |= RES_INIT;
	return (0);
}
