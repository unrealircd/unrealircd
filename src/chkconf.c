/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/chkconf.c
 *   Copyright (C) 1993 Darren Reed
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


#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#ifndef _WIN32
#include <sys/socket.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __hpux
#include "inet.h"
#endif
#ifdef PCS
#include <time.h>
#endif

ID_Copyright("(C) 1993 Darren Reed");
ID_Notes("DF version was 1.9 1/30/94");

/* for the connect rule patch..  these really should be in a header,
** but i see h.h isn't included for some reason..  so they're here */
char *crule_parse PROTO((char *rule));
void crule_free PROTO((char **elem));

#undef	free
#define	MyMalloc(x)	malloc(x)

static void new_class();
static char *getfield(), confchar();
static int openconf(), validate();
static aClass *get_class();
static aConfItem *initconf();

static int numclasses = 0, *classarr = (int *)NULL, debugflag = 0;
static char *configfile = CONFIGFILE;
static char nullfield[] = "";
static char maxsendq[12];

main(argc, argv)
	int  argc;
	char *argv[];
{
	new_class(0);

	if (chdir(DPATH))
	{
		perror("chdir");
		exit(-1);
	}
	if (argc > 1 && !strncmp(argv[1], "-d", 2))
	{
		debugflag = 1;
		if (argv[1][2])
			debugflag = atoi(argv[1] + 2);
		argc--, argv++;
	}
	if (argc > 1)
		configfile = argv[1];
	return validate(initconf());
}

/*
 * openconf
 *
 * returns -1 on any error or else the fd opened from which to read the
 * configuration file from.  This may either be th4 file direct or one end
 * of a pipe from m4.
 */
static int openconf()
{
	return open(configfile, O_RDONLY);
}

#define STAR1 OFLAG_SADMIN|OFLAG_ADMIN|OFLAG_NETADMIN|OFLAG_COADMIN
#define STAR2 OFLAG_TECHADMIN|OFLAG_ZLINE|OFLAG_HIDE|OFLAG_WHOIS
static int oper_access[] = {
	~(STAR1 | STAR2), '*',
	OFLAG_LOCAL, 'o',
	OFLAG_GLOBAL, 'O',
	OFLAG_REHASH, 'r',
	OFLAG_EYES, 'e',
	OFLAG_DIE, 'D',
	OFLAG_RESTART, 'R',
	OFLAG_HELPOP, 'h',
	OFLAG_GLOBOP, 'g',
	OFLAG_WALLOP, 'w',
	OFLAG_LOCOP, 'l',
	OFLAG_LROUTE, 'c',
	OFLAG_GROUTE, 'L',
	OFLAG_LKILL, 'k',
	OFLAG_GKILL, 'K',
	OFLAG_KLINE, 'b',
	OFLAG_UNKLINE, 'B',
	OFLAG_LNOTICE, 'n',
	OFLAG_GNOTICE, 'G',
	OFLAG_ADMIN, 'A',
	OFLAG_SADMIN, 'a',
	OFLAG_NETADMIN, 'N',
	OFLAG_COADMIN, 'C',
	OFLAG_TECHADMIN, 'T',
	OFLAG_UMODEC, 'u',
	OFLAG_UMODEF, 'f',
	OFLAG_ZLINE, 'z',
	OFLAG_WHOIS, 'W',
	OFLAG_HIDE, 'H',
	0, 0
};


/*
** initconf() 
**    Read configuration file.
**
**    returns -1, if file cannot be opened
**             0, if file opened
*/

static aConfItem *initconf(opt)
	int  opt;
{
	int  fd;
	char line[512], *tmp, c[80], *s, *crule;
	int  ccount = 0, ncount = 0, dh, flags = 0;
	int  lineno;
	aConfItem *aconf = NULL, *ctop = NULL;

	(void)fprintf(stderr, "initconf(): ircd.conf = %s\n", configfile);
	if ((fd = openconf()) == -1)
	{
		return NULL;
	}

	lineno = 1;
	(void)dgets(-1, NULL, 0);	/* make sure buffer is at empty pos */
	while ((dh = dgets(fd, line, sizeof(line) - 1)) > 0)
	{
		printf("%u:    End of file\r", lineno++);
		if (aconf)
		{
			if (aconf->host)
				(void)free(aconf->host);
			if (aconf->passwd)
				(void)free(aconf->passwd);
			if (aconf->name)
				(void)free(aconf->name);
		}
		else
			aconf = (aConfItem *)malloc(sizeof(*aconf));
		aconf->host = (char *)NULL;
		aconf->passwd = (char *)NULL;
		aconf->name = (char *)NULL;
		aconf->class = (aClass *)NULL;
		if (tmp = (char *)index(line, '\n'))
			*tmp = 0;
		else
			while (dgets(fd, c, sizeof(c) - 1))
				if (tmp = (char *)index(c, '\n'))
				{
					*tmp = 0;
					break;
				}
		/*
		 * Do quoting of characters and # detection.
		 */
		for (tmp = line; *tmp; tmp++)
		{
			if (*tmp == '\\')
			{
				switch (*(tmp + 1))
				{
				  case 'n':
					  *tmp = '\n';
					  break;
				  case 'r':
					  *tmp = '\r';
					  break;
				  case 't':
					  *tmp = '\t';
					  break;
				  case '0':
					  *tmp = '\0';
					  break;
				  default:
					  *tmp = *(tmp + 1);
					  break;
				}
				if (!*(tmp + 1))
					break;
				else
					for (s = tmp; *s = *++s;)
						;
				tmp++;
			}
			else if (*tmp == '#')
				*tmp = '\0';
		}
		if (!*line || *line == '#' || *line == '\n' ||
		    *line == ' ' || *line == '\t')
			continue;
		if (*line == 'X' || *line == 'T')
			continue;
		if (*line == 'x' || *line == 't')
			continue;

		if (line[1] != ':')
		{
			(void)fprintf(stderr, "ERROR: Bad config line (%s)\n",
			    line);
			continue;
		}

		if (debugflag)
			(void)printf("\n%s\n", line);
		(void)fflush(stdout);

		tmp = getfield(line);
		if (!tmp)
		{
			(void)fprintf(stderr, "\tERROR: no fields found\n");
			continue;
		}

		aconf->status = CONF_ILLEGAL;

		switch (*tmp)
		{
		  case 'A':	/* Name, e-mail address of administrator */
			  aconf->status = CONF_ADMIN;
			  break;
		  case 'a':	/* of this server. */
			  aconf->status = CONF_SADMIN;
			  break;
		  case 'C':	/* Server where I should try to connect */
		  case 'c':	/* in case of lp failures             */
			  ccount++;
			  aconf->status = CONF_CONNECT_SERVER;
			  break;
			  /* Connect rule */
		  case 'D':
			  aconf->status = CONF_CRULEALL;
			  break;
			  /* Connect rule - autos only */
		  case 'd':
			  aconf->status = CONF_CRULEAUTO;
			  break;
		  case 'E':
			  aconf->status = CONF_EXCEPT;
			  break;
		  case 'e':
			  aconf->status = CONF_SOCKSEXCEPT;
			  break;
		  case 'H':	/* Hub server line */
		  case 'h':
			  aconf->status = CONF_HUB;
			  break;
		  case 'I':	/* Just plain normal irc client trying  */
		  case 'i':	/* to connect me */
			  aconf->status = CONF_CLIENT;
			  break;
		  case 'K':	/* Kill user line on ircd.conf */
		  case 'k':
			  aconf->status = CONF_KILL;
			  break;
			  /* Operator. Line should contain at least */
			  /* password and host where connection is  */
		  case 'L':	/* guaranteed leaf server */
		  case 'l':
			  aconf->status = CONF_LEAF;
			  break;
			  /* Me. Host field is name used for this host */
			  /* and port number is the number of the port */
		  case 'M':
		  case 'm':
			  aconf->status = CONF_ME;
			  break;
		  case 'N':	/* Server where I should NOT try to     */
		  case 'n':	/* connect in case of lp failures     */
			  /* but which tries to connect ME        */
			  ++ncount;
			  aconf->status = CONF_NOCONNECT_SERVER;
			  break;
		  case 'O':
			  aconf->status = CONF_OPERATOR;
			  break;
			  /* Local Operator, (limited privs --SRB)
			   * Not anymore, OperFlag access levels. -Cabal95 */
		  case 'o':
			  aconf->status = CONF_OPERATOR;
			  break;
		  case 'P':	/* listen port line */
		  case 'p':
			  aconf->status = CONF_LISTEN_PORT;
			  break;
		  case 'Q':	/* a server that you don't want in your */
		  case 'q':	/* network. USE WITH CAUTION! */
			  aconf->status = CONF_QUARANTINED_SERVER;
			  break;
		  case 'S':	/* Service. Same semantics as   */
		  case 's':	/* CONF_OPERATOR                */
			  aconf->status = CONF_SERVICE;
			  break;
		  case 'T':
			  aconf->status = CONF_TLINE;
			  break;
		  case 'U':
		  case 'u':
			  aconf->status = CONF_UWORLD;
			  break;
		  case 'Y':
		  case 'y':
			  aconf->status = CONF_CLASS;
			  break;
		  case 'Z':
		  case 'z':
			  aconf->status = CONF_ZAP;
			  break;
		  default:
			  (void)fprintf(stderr,
			      "\tERROR: unknown conf line letter (%c)\n", *tmp);
			  break;
		}

		if (IsIllegal(aconf))
			continue;

		for (;;)	/* Fake loop, that I can use break here --msa */
		{
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->host, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->passwd, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->name, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			if (aconf->status & CONF_OPERATOR)
			{
				int *i, flag;
				char *m = "*";
				/*
				 * Now we use access flags to define  
				 * what an operator can do with their O.   
				 */
				for (m = (*tmp) ? tmp : m; *m; m++)
				{
					for (i = oper_access; (flag = *i);
					    i += 2)
						if (*m == (char)(*(i + 1)))
						{
							aconf->port |= flag;
							break;
						}
					if (flag == 0)
						fprintf(stderr,
						    "\tWARNING: Unknown oper access level '%c'\n",
						    *m);
				}
				if (!(aconf->port & OFLAG_ISGLOBAL))
					aconf->status = CONF_LOCOP;
			}
			else
				aconf->port = atoi(tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			if (!(aconf->status & CONF_CLASS))
				aconf->class = get_class(atoi(tmp));
			break;
		}
		if (!aconf->class && (aconf->status & (CONF_CONNECT_SERVER |
		    CONF_NOCONNECT_SERVER | CONF_OPS | CONF_CLIENT)))
		{
			(void)fprintf(stderr,
			    "\tWARNING: No class.  Default 0\n");
			aconf->class = get_class(0);
		}
		/* Check for bad Z-lines */
		if (aconf->status == CONF_ZAP)
		{
			char *tempc = aconf->host;
			if (!tempc)
			{
				fprintf(stderr, "\tERROR: Bad Z-line\n");
			}
			for (; *tempc; tempc++)
				if ((*tempc >= '0') && (*tempc <= '9'))
					goto zap_safe;
			fprintf(stderr, "\tERROR: Z-line mask too broad\n");
		      zap_safe:;
		}
		/*
		   ** If conf line is a class definition, create a class entry
		   ** for it and make the conf_line illegal and delete it.
		 */
		if (aconf->status & CONF_CLASS)
		{
			if (!aconf->host)
			{
				(void)fprintf(stderr, "\tERROR: no class #\n");
				continue;
			}
			if (!tmp)
			{
				(void)fprintf(stderr,
				    "\tWARNING: missing sendq field\n");
				(void)fprintf(stderr, "\t\t default: %d\n",
				    MAXSENDQLENGTH);
				(void)sprintf(maxsendq, "%d", MAXSENDQLENGTH);
			}
			else
				(void)sprintf(maxsendq, "%d", atoi(tmp));
			new_class(atoi(aconf->host));
			aconf->class = get_class(atoi(aconf->host));
			goto print_confline;
		}

		if (aconf->status & CONF_LISTEN_PORT)
		{
			if (!aconf->host)
				(void)fprintf(stderr, "\tERROR: %s\n",
				    "null host field in P-line");
			else if (index(aconf->host, '/'))
				(void)fprintf(stderr, "\t%s %s\n",
				    "WARNING: / present in P-line",
				    "for non-UNIXPORT configuration");
			aconf->class = get_class(0);
			goto print_confline;
		}

		if (aconf->status & CONF_SERVER_MASK &&
		    (!aconf->host || index(aconf->host, '*') ||
		    index(aconf->host, '?')))
		{
			(void)fprintf(stderr, "\tERROR: bad host field\n");
			continue;
		}

		if (aconf->status & CONF_SERVER_MASK && BadPtr(aconf->passwd))
		{
			(void)fprintf(stderr,
			    "\tERROR: empty/no password field\n");
			continue;
		}

		if (aconf->status & CONF_SERVER_MASK && !aconf->name)
		{
			(void)fprintf(stderr, "\tERROR: bad name field\n");
			continue;
		}

		if (aconf->status & (CONF_SERVER_MASK | CONF_OPS))
			if (!index(aconf->host, '@'))
			{
				char *newhost;
				int  len = 3;	/* *@\0 = 3 */

				len += strlen(aconf->host);
				newhost = (char *)MyMalloc(len);
				(void)sprintf(newhost, "*@%s", aconf->host);
				(void)free(aconf->host);
				aconf->host = newhost;
			}

		/* parse the connect rules to detect errors, but free
		   ** any allocated storage immediately -- we're just looking
		   ** for errors..  */
		if (aconf->status & CONF_CRULE)
			if ((crule = (char *)crule_parse(aconf->name)) != NULL)
				crule_free(&crule);

		if (!aconf->class)
			aconf->class = get_class(0);
		(void)sprintf(maxsendq, "%d", aconf->class->class);

		if (!aconf->name)
			aconf->name = nullfield;
		if (!aconf->passwd)
			aconf->passwd = nullfield;
		if (!aconf->host)
			aconf->host = nullfield;
		if (aconf->status & (CONF_ME | CONF_ADMIN))
		{
			if (flags & aconf->status)
				(void)fprintf(stderr,
				    "ERROR: multiple %c-lines\n",
				    toupper(confchar(aconf->status)));
			else
				flags |= aconf->status;
		}
	      print_confline:
		if (debugflag > 8)
			(void)printf("(%d) (%s) (%s) (%s) (%d) (%s)\n",
			    aconf->status, aconf->host, aconf->passwd,
			    aconf->name, aconf->port, maxsendq);
		(void)fflush(stdout);
		if (aconf->status & (CONF_SERVER_MASK | CONF_HUB | CONF_LEAF))
		{
			aconf->next = ctop;
			ctop = aconf;
			aconf = NULL;
		}
	}
	printf("\n");
	(void)close(fd);
	return ctop;
}

static aClass *get_class(cn)
	int  cn;
{
	static aClass cls;
	int  i = numclasses - 1;

	cls.class = -1;
	for (; i >= 0; i--)
		if (classarr[i] == cn)
		{
			cls.class = cn;
			break;
		}
	if (i == -1)
		(void)fprintf(stderr, "\tWARNING: class %d not found\n", cn);
	return &cls;
}

static void new_class(cn)
	int  cn;
{
	numclasses++;
	if (classarr)
		classarr = (int *)realloc(classarr, sizeof(int) * numclasses);
	else
		classarr = (int *)malloc(sizeof(int));
	classarr[numclasses - 1] = cn;
}

/*
 * field breakup for ircd.conf file.
 */
static char *getfield(newline)
	char *newline;
{
	static char *line = NULL;
	char *end, *field;

	if (newline)
		line = newline;
	if (line == NULL)
		return (NULL);

	field = line;
	if ((end = (char *)index(line, ':')) == NULL)
	{
		line = NULL;
		if ((end = (char *)index(field, '\n')) == NULL)
			end = field + strlen(field);
	}
	else
		line = end + 1;
	*end = '\0';
	return (field);
}


/*
** read a string terminated by \r or \n in from a fd
**
** Created: Sat Dec 12 06:29:58 EST 1992 by avalon
** Returns:
**	0 - EOF
**	-1 - error on read
**     >0 - number of bytes returned (<=num)
** After opening a fd, it is necessary to init dgets() by calling it as
**	dgets(x,y,0);
** to mark the buffer as being empty.
*/
int  dgets(fd, buf, num)
	int  fd, num;
	char *buf;
{
	static char dgbuf[8192];
	static char *head = dgbuf, *tail = dgbuf;
	register char *s, *t;
	register int n, nr;

	/*
	   ** Sanity checks.
	 */
	if (head == tail)
		*head = '\0';
	if (!num)
	{
		head = tail = dgbuf;
		*head = '\0';
		return 0;
	}
	if (num > sizeof(dgbuf) - 1)
		num = sizeof(dgbuf) - 1;
      dgetsagain:
	if (head > dgbuf)
	{
		for (nr = tail - head, s = head, t = dgbuf; nr > 0; nr--)
			*t++ = *s++;
		tail = t;
		head = dgbuf;
	}
	/*
	   ** check input buffer for EOL and if present return string.
	 */
	if (head < tail &&
	    ((s = index(head, '\n')) || (s = index(head, '\r'))) && s < tail)
	{
		n = MIN(s - head + 1, num);	/* at least 1 byte */
	      dgetsreturnbuf:
		bcopy(head, buf, n);
		head += n;
		if (head == tail)
			head = tail = dgbuf;
		return n;
	}

	if (tail - head >= num)	/* dgets buf is big enough */
	{
		n = num;
		goto dgetsreturnbuf;
	}

	n = sizeof(dgbuf) - (tail - dgbuf) - 1;
	nr = read(fd, tail, n);
	if (nr == -1)
	{
		head = tail = dgbuf;
		return -1;
	}
	if (!nr)
	{
		if (head < tail)
		{
			n = MIN(head - tail, num);
			goto dgetsreturnbuf;
		}
		head = tail = dgbuf;
		return 0;
	}
	tail += nr;
	*tail = '\0';
	for (t = head; (s = index(t, '\n'));)
	{
		if ((s > head) && (s > dgbuf))
		{
			t = s - 1;
			for (nr = 0; *t == '\\'; nr++)
				t--;
			if (nr & 1)
			{
				t = s + 1;
				s--;
				nr = tail - t;
				while (nr--)
					*s++ = *t++;
				tail -= 2;
				*tail = '\0';
			}
			else
				s++;
		}
		else
			s++;
		t = s;
	}
	*tail = '\0';
	goto dgetsagain;
}


static int validate(top)
	aConfItem *top;
{
	aConfItem *aconf, *bconf;
	u_int otype, valid = 0;

	if (!top)
		return 0;

	for (aconf = top; aconf; aconf = aconf->next)
	{
		if (aconf->status & CONF_MATCH)
			continue;

		if (aconf->status & CONF_SERVER_MASK)
		{
			if (aconf->status & CONF_CONNECT_SERVER)
				otype = CONF_NOCONNECT_SERVER;
			else if (aconf->status & CONF_NOCONNECT_SERVER)
				otype = CONF_CONNECT_SERVER;

			for (bconf = top; bconf; bconf = bconf->next)
			{
				if (bconf == aconf || !(bconf->status & otype))
					continue;
				if (bconf->class == aconf->class &&
				    !mycmp(bconf->name, aconf->name) &&
				    !mycmp(bconf->host, aconf->host))
				{
					aconf->status |= CONF_MATCH;
					bconf->status |= CONF_MATCH;
					break;
				}
			}
		}
		else
			for (bconf = top; bconf; bconf = bconf->next)
			{
				if ((bconf == aconf) ||
				    !(bconf->status & CONF_SERVER_MASK))
					continue;
				if (!mycmp(bconf->name, aconf->name))
				{
					aconf->status |= CONF_MATCH;
					break;
				}
			}
	}

	(void)fprintf(stderr, "\n");
	for (aconf = top; aconf; aconf = aconf->next)
		if (aconf->status & CONF_MATCH)
			valid++;
		else
			(void)fprintf(stderr, "Unmatched %c:%s:%s:%s\n",
			    confchar(aconf->status), aconf->host,
			    aconf->passwd, aconf->name);
	return valid ? 0 : -1;
}

static char confchar(status)
	u_int status;
{
	static char letrs[] = "QICNoOMKARYSLPHXT";
	char *s = letrs;

	status &= ~(CONF_MATCH | CONF_ILLEGAL);

	for (; *s; s++, status >>= 1)
		if (status & 1)
			return *s;
	return '-';
}

outofmemory()
{
	(void)write(2, "Out of memory\n", 14);
	exit(-1);
}
