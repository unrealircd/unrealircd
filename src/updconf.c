/************************************************************************

 *   Unreal Internet Relay Chat Daemon, src/updconf.c
 *   Copyright (C) 2001 UnrealIRCd Team
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

#include <stdio.h>
#include <string.h>
#include "../include/config.h"

#define	BadPtr(x) (!(x) || (*(x) == '\0'))
#define AllocCpy(x,y) if (x) free(x);  x = (char *) malloc(strlen(y) + 1); strcpy(x,y)

struct flags {
	char oldflag;
	char *newflag;
};

struct class *classes = NULL;
struct uline *ulines = NULL;
struct link  *links = NULL;
struct oper *opers = NULL;

struct flags oflags[] = {
	{ 'o', "local" },
	{ 'O', "global" },
	{ 'r', "can_rehash" },
	{ 'e', "eyes" },
	{ 'D', "can_die" },
	{ 'R', "can_restart" },
	{ 'h', "helpop" },
	{ 'g', "can_globops" },
	{ 'w', "can_wallops" },
	{ 'c', "can_localroute" },
	{ 'L', "can_globalroute" },
	{ 'k', "can_localkill" },
	{ 'K', "can_globalkill" },
	{ 'b', "can_kline" },
 	{ 'B', "can_unkline" },
	{ 'n', "can_localnotice" },
	{ 'G', "can_globalnotice" },
	{ 'a', "services-admin"},
	{ 'A', "admin" },
	{ 'N', "netadmin"},
	{ 'C', "coadmin"},
	{ 'T', "techadmin"},
	{ 'u', "get_umodec"},
	{ 'f', "get_umodef"},
	{ 'z', "can_zline" },
	{ 'W', "get_umodew" },
	{ '^', "can_stealth" },
	{ NULL, NULL }
};

struct flags listenflags[] = {
	{ 'S', "serversonly" },
	{ 'C', "clientsonly" },
	{ 'J', "java" },
	{ 's', "ssl" },
	{ '*', "standard" },
	{ NULL, NULL }
};

struct flags linkflags[] = {
	{ 'S', "ssl" },
	{ 'Z', "zip" },
	{ NULL, NULL }
};

char *getfield(char *newline)
{
	static char *line = NULL;
	char *end, *field, *x;

	if (newline)
		line = newline;

	if (line == NULL)
		return NULL;

	field = line;
	if (*field == '"')
	{
		field++;
		x = index(field, '"');
		if (!x)
			return NULL;
		*x = '\0';
		x++;
		if (*x == '\n')
			line = NULL;
		else
			line = x;

		end = x;
		line++;
		goto end1;

	}
	if ((end = (char *)index(line, ':')) == NULL)
	{
		line = NULL;
		if ((end = (char *)index(field, '\n')) == NULL)
			end = field + strlen(field);
	}
	else
		line = end + 1;
	end1:
	*end = '\0';
	return (field);
}

/* Allow naming of classes */

struct oper {
	char *login;
	char *pass;
	char *flags;
	char *class;
	struct host *hosts;
	struct oper *next;
};

struct host {
	char *host;
	struct host *next;
};

struct class {
	int number;
	char *name;
	struct class *next;
};

struct uline {
	char *name;
	struct uline *next;
};

struct link {
	char 	*name;
	char 	*hostmask;
	short 	type;
	int  	port;
	char	*flags;
	char 	*connclass;
	char 	*recclass;
	char 	*connpass;
	char 	*recpass;
	int  	leafdepth;
	char    *leafmask;
	char 	*hubmask;
	struct link *next;
};
	

int add_class(char *name, int number) {
	struct class *cl = (struct class *)malloc(sizeof(struct class));

	if (!cl)
		return 0;
	cl->number = number;
	AllocCpy(cl->name, name);

	cl->next = classes;
	classes = cl;
	return 1;
}

int add_uline(char *name) {
	struct uline *ul = (struct uline *)malloc(sizeof(struct uline));

	if (!ul)
		return 0;
	AllocCpy(ul->name, name);

	ul->next = ulines;
	ulines = ul;
	return 1;
}



struct class *find_class(int number) {
	struct class *cl;

	for (cl = classes; cl; cl = cl->next) {
		if (number == cl->number)
			return cl;
	}
	return NULL;
}	

struct host *add_host(struct oper *oper, char *mask) {
	struct host *host = (struct host *)malloc(sizeof(struct host));

	if (!host)
		return NULL;

	AllocCpy(host->host, mask);

		host->next = oper->hosts;

	oper->hosts = host;

	return host;
}

int find_host(struct oper *oper, char *mask) {
	struct host *host;

	for (host = oper->hosts; host; host = oper->hosts->next) {
		if (!stricmp(host->host, mask))
			return 1;
	}
	return 0;
}
struct oper *add_oper(char *name) {
	struct oper *op = (struct oper *)malloc(sizeof(struct oper));

	if (!op)
		return NULL;
	AllocCpy(op->login, name);

	op->next = opers;
	opers = op;
	return op;
}

struct oper *find_oper(char *name, char *pass) {
	struct oper *op;

	for (op = opers; op; op = op->next) {
		if (!stricmp(op->login, name) && !stricmp(op->pass, pass))
			return op;
	}
	return NULL;
}	

struct link *add_server(char *name) {
	struct link *lk = (struct link *)malloc(sizeof(struct link));

	if (!lk)
		return NULL;
	AllocCpy(lk->name, name);

	lk->next = links;
	links = lk;
	return lk;
}

struct link *find_link(char *name) {
	struct link *lk;

	for (lk = links; lk; lk = lk->next) {
		if (!stricmp(lk->name, name))
			return lk;
	}
	return NULL;
}	



void iCstrip(char *line)
{
	char *c;

	if ((c = strchr(line, '\n')))
		*c = '\0';
	if ((c = strchr(line, '\r')))
		*c = '\0';
}

int main (int argc, char *argv[]) {
	FILE *fd;
	FILE *fd2;
	char buf[1024], *tmp, *param;
	int mainport, i;
	char mainip[256];
	struct class *cl;
	struct uline *ul;
	struct link *lk;
	struct oper *op;
	struct host *operhost;
	struct flags *_flags;
	fd = fopen("ircd.conf", "r");
	fd2 = fopen("ircd.conf.new", "w");
	for (i=1; i < argc; i++) {
		param = argv[i];
		if (*param == '-') {
			++param;
			if (*param == 'Y') {
				char *class;
				int num;
				++i;
			for (tmp = strtok(argv[i], ","); tmp; tmp = strtok(NULL, ",")) {
				char *tmp2;
				int num;
				num = atoi(getfield(tmp));	
				tmp2 = getfield(NULL);
				add_class(tmp2,num);
			}
			}
	
		}
	}

	fprintf(fd2, "/* Created using the UnrealIRCd configuration file updater */\n\n");

	while (fgets(buf, 1023, fd)) {
		if (buf[0] == '#')
			continue;

		iCstrip(buf);
	switch (buf[0]) {
	/* M:line */
	case 'M': 
		fprintf(fd2, "me {\n");
		tmp = getfield(&buf[2]);
		fprintf(fd2, "\tname \"%s\";\n", tmp);
		strcpy(mainip, getfield(NULL));
		tmp = getfield(NULL);
		fprintf(fd2, "\tinfo \"%s\";\n", tmp);
		tmp = getfield(NULL);
		mainport = atoi(tmp);
		tmp = getfield(NULL);
		if (tmp)
			fprintf(fd2, "\tnumeric %s;\n", tmp);
		fprintf(fd2, "};\n\n");

		break;
	/* A:line */
	case 'A': 
		fprintf(fd2, "admin {\n");
		tmp = getfield(&buf[2]);
		if (tmp)
			fprintf(fd2, "\t\"%s\";\n", tmp);
		tmp = getfield(NULL);
		if (tmp)
			fprintf(fd2, "\t\"%s\";\n", tmp);
		tmp = getfield(NULL);
		if (tmp)
			fprintf(fd2, "\t\"%s\";\n", tmp);
		fprintf(fd2,"};\n\n");
		break;
	/* Y:line */
	case 'Y': 
		tmp = getfield(&buf[2]);
		if ((cl = find_class(atoi(tmp))))
			fprintf(fd2, "class %s {\n", cl->name);
		else	
		fprintf(fd2, "class %s {\n",tmp);
		tmp = getfield(NULL);
		fprintf(fd2, "\tpingfreq %s;\n", tmp);
		tmp = getfield(NULL);
		if (atoi(tmp) != 0)
			fprintf(fd2, "\tconnfreq %s;\n", tmp);
		tmp = getfield(NULL);
		fprintf(fd2, "\tmaxclients %s;\n", tmp);
		tmp = getfield(NULL);
		fprintf(fd2, "\tsendq %s;\n", tmp);
		fprintf(fd2, "};\n\n");
		break;	
	case 'I':
		fprintf(fd2, "allow {\n");
		tmp = getfield(&buf[2]);
		fprintf(fd2, "\tip %s;\n", tmp);
		tmp = getfield(NULL);
		if (!BadPtr(tmp))
			fprintf(fd2, "\tpassword \"%s\";\n", tmp);
		tmp = getfield(NULL);
		fprintf(fd2, "\thostname %s;\n", tmp);
		getfield(NULL);
		tmp = getfield(NULL);
		if ((cl = find_class(atoi(tmp))))
			fprintf(fd2, "\tclass %s;\n", cl->name);
		else
		fprintf(fd2, "\tclass %s;\n", tmp);
		fprintf(fd2,"};\n\n");
		break;

	case 'O':
	case 'o':
	{
		char host[256], pass[32];
		struct flags *_oflags;
		char rebuild[128];
		int i = 0;
		strcpy(host, getfield(&buf[2]));
		strcpy(pass, getfield(NULL));
		tmp = getfield(NULL);
		if (!(op = find_oper(tmp, pass)))
			op = add_oper(tmp);
		AllocCpy(op->pass, pass);
		tmp = getfield(NULL);
		if (!find_host(op, host))
			add_host(op, host);
		if (!BadPtr(op->flags)) {
			memset(rebuild, 0, 128);
			for(_oflags = oflags; _oflags->oldflag;_oflags++) {
				if (strchr(op->flags, _oflags->oldflag) || strchr(tmp, _oflags->oldflag)) {
					rebuild[i] = _oflags->oldflag;
					i++;
				}
			}
			AllocCpy(op->flags, rebuild);
		}
		else {
			AllocCpy(op->flags, tmp);
		}
		tmp = getfield(NULL);
		if ((cl = find_class(atoi(tmp)))) {
			AllocCpy(op->class, cl->name);
		}
		else {
			AllocCpy(op->class, tmp);
		}
		break;
	}
	case 'U':
		add_uline(getfield(&buf[2]));
		break;
	case 'X':
		fprintf(fd2, "drpass {\n");
		tmp = getfield(&buf[2]);
		fprintf(fd2, "\tdie \"%s\";\n", tmp);
		tmp = getfield(NULL);
		fprintf(fd2, "\trestart \"%s\";\n", tmp);
		fprintf(fd2, "};\n\n");
		break;
	case 'P':
	{
		char ip[256], flags[20];
		struct flags *_lflags;
		strcpy(ip,getfield(&buf[2]));
		strcpy(flags,getfield(NULL));
		getfield(NULL);
		tmp = getfield(NULL);
		if (!BadPtr(flags)) {
			fprintf(fd2, "listen %s:%s {\n", ip, tmp);
			tmp = flags;
			fprintf(fd2, "\toptions {\n");
			for(; *tmp; tmp++) {
				for(_lflags = listenflags; _lflags->oldflag; _lflags++) {
					if (*tmp == _lflags->oldflag)
						fprintf(fd2, "\t\t%s;\n", _lflags->newflag);
				}
			}
			fprintf(fd2, "\t};\n};\n\n");
		}
		else
			fprintf(fd2, "listen %s:%s;\n\n", ip, tmp);
		break;
	}
	case 'T':
		fprintf(fd2, "tld {\n");
		tmp = getfield(&buf[2]);
		fprintf(fd2, "\tmask %s;\n", tmp);
		tmp = getfield(NULL);
		fprintf(fd2, "\tmotd \"%s\";\n", tmp);
		tmp = getfield(NULL);
		fprintf(fd2, "\trules \"%s\";\n", tmp);
		fprintf(fd2, "};\n\n");
		break;
	case 'E': 
	{
		char *tmp2;
		fprintf(fd2, "except ban {\n");
		tmp = getfield(&buf[2]);
		getfield(NULL);
		tmp2 = getfield(NULL);
		fprintf(fd2, "\tmask %s@%s;\n", tmp2, tmp);
		fprintf(fd2, "};\n\n");
		break;
	}
	case 'e':
		fprintf(fd2, "except socks {\n");
		tmp = getfield(&buf[2]);
		fprintf(fd2, "\tmask %s;\n", tmp);
		fprintf(fd2, "};\n\n");
		break;
	case 'L':
	{
		char *leafmask;
		leafmask = getfield(&buf[2]);
		getfield(NULL);
		tmp = getfield(NULL);
		if (!(lk = find_link(tmp))) 
			lk = add_server(tmp);
		AllocCpy(lk->leafmask, leafmask);
		lk->type = 0;
		tmp = getfield(NULL);
		if (!BadPtr(tmp))
			lk->leafdepth = atoi(tmp);
		else
			lk->leafdepth = 0;
		break;
	}
	case 'H':
	{
		char *hubmask;
		hubmask = getfield(&buf[2]);
		getfield(NULL);
		tmp = getfield(NULL);
		if (!(lk = find_link(tmp)))
			lk = add_server(tmp);
		AllocCpy(lk->hubmask, hubmask);
		lk->type = 1;
		lk->leafdepth = 0;
		break;
	}
	case 'N':
	{
		char *host, *pass;		
		
		host = getfield(&buf[2]);
		pass = getfield(NULL);
		tmp = getfield(NULL);
		if (!(lk = find_link(tmp)))
			lk = add_server(tmp);
		AllocCpy(lk->recpass,pass);
		getfield(NULL);
		tmp = getfield(NULL);
		if ((cl = find_class(atoi(tmp)))) {
			AllocCpy(lk->recclass, cl->name);
		}
		else {
			AllocCpy(lk->recclass,tmp);
		}
		break;
	}			
	case 'C':
	{
		char *host, *pass;

		host = getfield(&buf[2]);
		pass = getfield(NULL);
		tmp = getfield(NULL);
		if (!(lk = find_link(tmp)))
			lk = add_server(tmp);
		AllocCpy(lk->hostmask, host);
		AllocCpy(lk->connpass,pass);
		tmp = getfield(NULL);
		if (!BadPtr(tmp))
			lk->port = atoi(tmp);
		else
			lk->port = 0;
		tmp = getfield(NULL);
		if ((cl = find_class(atoi(tmp)))) {
			AllocCpy(lk->connclass, cl->name);
		}
		else {
			AllocCpy(lk->connclass, tmp);
		}
		tmp = getfield(NULL);
		if (!BadPtr(tmp)) {
			AllocCpy(lk->flags, tmp);
		}
		else
			lk->flags = NULL;
		break;
	}
	case 'Q':
	{
		char *tmp2;
		getfield(&buf[2]);
		tmp2 = getfield(NULL);
		tmp = getfield(NULL);
		fprintf(fd2, "ban nick {\n");
		fprintf(fd2, "\tmask \"%s\";\n", tmp);
		if (!BadPtr(tmp2))
		fprintf(fd2, "\treason \"%s\";\n", tmp2);
		fprintf(fd2, "};\n\n");
		break;
	}
	case 'q':
	{
		char *tmp2;
		getfield(&buf[2]);
		tmp2 = getfield(NULL);
		tmp = getfield(NULL);
		fprintf(fd2, "ban server {\n");
		fprintf(fd2, "\tmask \"%s\";\n", tmp);
		if (!BadPtr(tmp2))
		fprintf(fd2, "\treason \"%s\";\n", tmp2);
		fprintf(fd2, "};\n\n");
		break;
	}
	case 'Z':
		tmp = getfield(&buf[2]);
		fprintf(fd2, "ban ip {\n");
		fprintf(fd2, "\tmask \"%s\";\n", tmp);
		tmp = getfield(NULL);
		if (!BadPtr(tmp))
		fprintf(fd2, "\treason \"%s\";\n", tmp);
		fprintf(fd2, "};\n\n");
		break;
	case 'n':
		tmp = getfield(&buf[2]);
		fprintf(fd2, "ban realname {\n");
		fprintf(fd2, "\tmask \"%s\";\n", tmp);
		tmp = getfield(NULL);
		if (!BadPtr(tmp))
			fprintf(fd2, "\treason \"%s\";\n", tmp);
		fprintf(fd2, "};\n\n");
		break;
	case 'K':
	case 'k':
	{
		char *host, *user, *reason;
		host = getfield(&buf[2]);
		reason = getfield(NULL);
		user = getfield(NULL);
		fprintf(fd2, "ban user {\n");
		fprintf(fd2, "\tmask %s@%s;\n", user, host);
		if (!BadPtr(reason))
			fprintf(fd2, "\treason \"%s\";\n", reason);
		fprintf(fd2, "};\n\n");
		break;
	}
	case 'V':
	case 'v':
	{
		char *version, *flags;
		version = getfield(&buf[2]);
		flags = getfield(NULL);
		tmp = getfield(NULL);
		fprintf(fd2, "deny version {\n");
		fprintf(fd2, "\tmask %s;\n", tmp);
		fprintf(fd2, "\tversion \"%s\";\n", version);
		fprintf(fd2, "\tflags \"%s\";\n", flags);
		fprintf(fd2, "};\n\n");
		break;
	}
	}
	}
	/* Old M:line bind */
	fprintf(fd2, "listen %s:%d;\n\n", mainip, mainport);

	for (op = opers; op; op = op->next) {
		fprintf(fd2, "oper %s {\n", op->login);
		fprintf(fd2, "\tfrom {\n");
		for (operhost = op->hosts; operhost; operhost = operhost->next) {
			fprintf(fd2, "\t\tuserhost %s;\n", operhost->host);
		}
		fprintf(fd2, "\t};\n");
		fprintf(fd2, "\tpassword \"%s\";\n", op->pass);
		fprintf(fd2, "\tflags {\n");
		for (; *op->flags; op->flags++) {
			for (_flags = oflags; _flags->oldflag; _flags++) {
				if (*op->flags == _flags->oldflag)
					fprintf(fd2, "\t\t%s;\n", _flags->newflag);
			}
		}
		fprintf(fd2, "\t};\n");
		fprintf(fd2, "\tclass %s;\n", op->class);
		fprintf(fd2, "};\n\n");
	}

	for (lk = links; lk; lk = lk->next) {
		char *user, *host;
		struct flags *_linkflags;
		fprintf(fd2, "link %s {\n", lk->name);
		user = strtok(lk->hostmask, "@");
		host = strtok(NULL, "");
		if (!BadPtr(host)) {
			fprintf(fd2, "\tusername %s;\n", user);
			fprintf(fd2, "\thostname %s;\n", host);
		}
		else {
			fprintf(fd2, "\tusername *;\n");
			fprintf(fd2, "\thostname %s;\n", user);
		}

		fprintf(fd2, "\tbind-ip %s;\n", mainip);
		if (lk->port)
			fprintf(fd2, "\tport %d;\n", lk->port);
		if (lk->type == 1)
		fprintf(fd2, "\thub %s;\n", lk->hubmask);
		else {
			if (!BadPtr(lk->leafmask))
				fprintf(fd2, "\tleaf %s;\n", lk->leafmask);
			else
				fprintf(fd2, "\tleaf *;\n");
			if (lk->leafdepth)
			fprintf(fd2, "\tleaf-depth %d;\n", lk->leafdepth);
		}
		fprintf(fd2, "\tpassword-connect %s;\n", lk->connpass);
		fprintf(fd2, "\tpassword-receive %s;\n", lk->recpass);
		fprintf(fd2, "\treceive-class %s;\n", lk->recclass);
		fprintf(fd2, "\tconnect-class %s;\n", lk->connclass);
		if (lk->flags != NULL || lk->port) 
			fprintf(fd2, "\toptions {\n");
		if (lk->flags != NULL) {
		for (tmp = lk->flags; *tmp; tmp++) {
			for (_linkflags = linkflags; _linkflags->oldflag; _linkflags++) {
				if (*tmp == _linkflags->oldflag)
					fprintf(fd2, "\t\t%s;\n",_linkflags->newflag);
			}
		}
		}
			if (lk->port)
				fprintf(fd2, "\t\tautoconnect;\n");
			if (lk->port || lk->flags != NULL)
			fprintf(fd2, "\t};\n");
		
		fprintf(fd2, "};\n\n");
	}
		
		
			

	/* Because of how new U:lines are, we have to add them at the end */
	if (ulines)
		fprintf(fd2, "ulines {\n");
	for (ul = ulines; ul; ul = ul->next) {	
		fprintf(fd2, "\t%s;\n", ul->name);
	}
	if (ulines)
		fprintf(fd2, "};\n\n");
	
}

