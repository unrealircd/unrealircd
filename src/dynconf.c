/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/dynconf.c
 *   (C) 1999-2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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
#include "msg.h"
#include "channel.h"
#include "userload.h"
#include "proto.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#include "setup.h"

ID_Copyright("(C) 1999-2000 Carsten Munk");

#define DoDebug fprintf(stderr, "[%s] %s | %li\n", babuf, __FILE__, __LINE__);
#define AllocCpy(x,y) if ((x) && type == 1) MyFree((x)); x = (char *) MyMalloc(strlen(y) + 1); strcpy(x,y)
#define XtndCpy(x,y) x = (char *) MyMalloc(strlen(y) + 2); *x = '\0'; strcat(x, "*"); strcpy(x,y)

/* externals */
extern int un_uid, un_gid;
/* internals */
aConfiguration iConf;
int  icx = 0;
char buf[1024];


/* strips \r and \n's from the line */
void iCstrip(char *line)
{
	char *c;

	if ((c = strchr(line, '\n')))
		*c = '\0';
	if ((c = strchr(line, '\r')))
		*c = '\0';
}

/* this loads dynamic ZCONF */
int  load_conf(char *filename, int type)
{
	FILE *zConf;
	char *version = NULL;
	char *i;

	zConf = fopen(filename, "r");

	if (!zConf)
	{
		if (type == 1)
		{
			sendto_ops("[error] Could not load %s !", filename);
			return -1;
		}
		else
		{
#ifdef _WIN32
			ircsprintf(buf, "Unable to load dynamic config %s",
			    filename);
			MessageBox(NULL, "UnrealIRCD/32 Init Error", buf,
			    MB_OK);
#else
			fprintf(stderr, "[error] Couldn't load %s !!!\n",
			    filename);
#endif
			exit(-1);
		}
	}
	i = fgets(buf, 1023, zConf);
	if (!i)
	{
		if (type == 1)
		{
			sendto_ops("[error] Error reading from %s !", filename);
			return -1;
		}
		else
		{
#ifdef _WIN32
			ircsprintf(buf, "Unable to read dynamic config %s",
			    filename);
			MessageBox(NULL, "UnrealIRCD/32 Init Error", buf,
			    MB_OK);
#else
			fprintf(stderr, "[error] Couldn't read %s !!!\n",
			    filename);
#endif
			exit(-1);
		}
	}
	iCstrip(buf);
	version = strtok(buf, "^");
	version = strtok(NULL, "");
	if (!version)
		goto malformed;
	/* is this a unrealircd.conf file? */
	if (!match("1.*", version))
	{
		/* wrong version */
		if (strcmp(version, DYNCONF_CONF_VERSION))
		{
			if (type == 1)
			{
				sendto_ops
				    ("[error] %s got a non-compatible version (%s) !",
				    filename, version);
				sendto_ops
				    ("[error] Please go to http://www.unrealircd.com and learn how to upgrade");
				return -1;
			}
			else
			{
#ifdef _WIN32
				ircsprintf(buf,
				    "Incompatible version (%s) in %s - please upgrade",
				    version, filename);
				MessageBox(NULL, "UnrealIRCD/32 Init Error",
				    buf, MB_OK);
#else
				fprintf(stderr,
				    "[error] Incompatible version (%s) in %s - please upgrade !!!\n",
				    version, filename);
#endif
				exit(-1);
			}
		}
		load_conf2(zConf, filename, type);
		return -1;
	}
	else if (!match("2.*", version))
	{
		/* network file */
		/* wrong version */
		if (strcmp(version, DYNCONF_NETWORK_VERSION))
		{
			if (type == 1)
			{
				sendto_ops
				    ("[error] %s got a non-compatible network file version (%s) !",
				    filename, version);
				sendto_ops
				    ("[error] Please go to http://www.unrealircd.com and learn how to upgrade");
				return -1;
			}
			else
			{
#ifdef _WIN32
				ircsprintf(buf,
				    "Incompatible network file version (%s) in %s - please upgrade",
				    version, filename);
				MessageBox(NULL, "UnrealIRCD/32 Init Error",
				    buf, MB_OK);
#else
				fprintf(stderr,
				    "[error] Incompatible network file version (%s) in %s - please upgrade !!!\n",
				    version, filename);
#endif
				exit(-1);
			}
		}
		load_conf3(zConf, filename, type);
		return -1;
	}
	else
	{
malformed:
		if (type == 1)
		{
			sendto_ops
			    ("[Error] Malformed version header in %s. Please read Unreal.nfo on info how to get support",
			    filename);
			return -1;
		}
		else
		{
#ifdef _WIN32
			ircsprintf(buf,
			    "Malformed version header in %s. Please read Unreal.nfo on info how to get support",
			    filename);
			MessageBox(NULL, "UnrealIRCD/32 Init Error", buf,
			    MB_OK);
#else
			fprintf(stderr,
			    "[error] Malformed version header in %s. Please read Unreal.nfo on info how to get support!!\n",
			    filename);
#endif
			exit(-1);
		}
		return -1;
	}
	return -1;
}

int  load_conf2(FILE * conf, char *filename, int type)
{
	char *databuf = buf;
	char *stat = databuf;
	char *p, *setto, *var;

	if (!conf)
		return -1;

	*databuf = '\0';

	/* loop to read data */
	while (stat != NULL)
	{
		stat = fgets(buf, 1020, conf);
		if ((*buf == '#') || (*buf == '/'))
			continue;

		iCstrip(buf);
		if (*buf == '\0')
			continue;
		p = strtok(buf, " ");
		if (strcmp(p, "Include") == 0)
		{
			strtok(NULL, " ");
			setto = strtok(NULL, "");
			/* We need this for STATS S -codemastr 
			   Isn't there a better way to show all include files? --Stskeeps */
			AllocCpy(INCLUDE, setto);
			load_conf(setto, type);
		}
		else if (strcmp(p, "Set") == 0)
		{
			var = strtok(NULL, " ");
			if (var == NULL)
				continue;
			if (*var == '\0')
				continue;

			setto = strtok(NULL, " ");
			if (!setto)
				continue;
			setto = strtok(NULL, "");
			if (!setto)
				continue;
			/* Is it a aint variable */
			if (strcmp(var, "MODE_X") == 0)
			{
				MODE_X = atoi(setto);
			}
			else if (strcmp(var, "MODE_STRIPWORDS") == 0)
			{
				MODE_STRIPWORDS = atoi(setto);
			}
			else if (strcmp(var, "MODE_I") == 0)
			{
				MODE_I = atoi(setto);
			}
			else if (strcmp(var, "TRUEHUB") == 0)
			{
				TRUEHUB = atoi(setto);
#ifndef HUB
				if ((TRUEHUB == 1) && (type == 0))
				{
#ifdef _WIN32
					MessageBox(NULL,
					    "UnrealIRCd/32 Init Error",
					    "TRUEHUB value set to 1, but I'm not a hub!",
					    MB_OK);
#else
					fprintf(stderr,
					    "[error] I'm a leaf NOT a hub!\n");
#endif
					exit(-1);
				}
				else if ((type == 1) && (TRUEHUB == 1))
				{
					sendto_ops
					    ("TRUEHUB value set to 1, but I'm not a hub!");
				}
#endif
			}
			else if (strcmp(var, "CONFIG_FILE_STOP") == 0)
			{

#ifndef DEVELOP
				if (atoi(setto) == 1)
				{
#ifdef _WIN32
					MessageBox(NULL,
					    "UnrealIRCD/32 Init Error",
					    "Read Config stop code in file",
					    MB_OK);
#else
					fprintf(stderr,
					    "[fatal error] File stop code recieved in %s - RTFM\n",
					    filename);
#endif
					exit(-1);
				}
#endif
			}
			else if (strcmp(var, "SHOWOPERS") == 0)
			{
				SHOWOPERS = atoi(setto);
			}
			else if (strcmp(var, "WEBTV_SUPPORT") == 0)
			{
				WEBTV_SUPPORT = atoi(setto);
			}
			else if (strcmp(var, "SHOWOPERMOTD") == 0)
			{
				SHOWOPERMOTD = atoi(setto);
			}
			else if (strcmp(var, "SOCKSBANTIME") == 0)
			{
				iConf.socksbantime = atol(setto);
			}
			else if (strcmp(var, "MAXCHANNELSPERUSER") == 0)
			{
				MAXCHANNELSPERUSER = atol(setto);
			}
			else if (strcmp(var, "ALLOW_CHATOPS") == 0)
			{
				ALLOW_CHATOPS = atoi(setto);
			}
			else if (strcmp(var, "HIDE_ULINES") == 0)
			{
				HIDE_ULINES = atoi(setto);
			}
			else if (strcmp(var, "NO_OPER_HIDING") == 0)
			{
				NO_OPER_HIDING = atoi(setto);
			}
			else if (strcmp(var, "KILLDIFF") == 0)
			{
				KILLDIFF = atoi(setto);
			}
			if (strcmp(var, "KLINE_ADDRESS") == 0)
			{
				AllocCpy(KLINE_ADDRESS, setto);
			}
			else if (strcmp(var, "SOCKS_BAN_MESSAGE") == 0)
			{
				AllocCpy(iConf.socksbanmessage, setto);
			}
			else if (strcmp(var, "SOCKS_QUIT_MESSAGE") == 0)
			{
				AllocCpy(iConf.socksquitmessage, setto);
			}
			else if (strcmp(var, "AUTO_JOIN_CHANS") == 0)
			{
				AllocCpy(AUTO_JOIN_CHANS, setto);
			}
			else if (strcmp(var, "OPER_AUTO_JOIN_CHANS") == 0)
			{
				AllocCpy(OPER_AUTO_JOIN_CHANS, setto);
			}
			else if (strcmp(var, "HOST_TIMEOUT") == 0)
			{
				HOST_TIMEOUT = atol(setto);
			}
			else if (strcmp(var, "HOST_RETRIES") == 0)
			{
				HOST_RETRIES = atoi(setto);
			}
#ifndef BIG_SECURITY_HOLE
			else if (strcmp(var, "SETUID") == 0)
			{
				un_uid = atoi(setto);
			}
			else if (strcmp(var, "SETGID") == 0)
			{
				un_gid = atoi(setto);
			}
#endif
		}
	}
	if (type == 0)
	{
		fprintf(stderr, "* Loaded %s ..\n", filename);
	}
	else
	{
		sendto_realops("Loaded %s ..", filename);
	}
return 0;
}

/* Load .network options */
int  load_conf3(FILE * conf, char *filename, int type)
{
	char *databuf = buf;
	char *stat = databuf;
	char *p, *setto, *var;
	if (!conf)
		return -1;

	*databuf = '\0';

	/* loop to read data */
	while (stat != NULL)
	{
		stat = fgets(buf, 1020, conf);
		if ((*buf == '#') || (*buf == '/'))
			continue;

		iCstrip(buf);
		if (*buf == '\0')
			continue;

		p = strtok(buf, " ");
		if (strcmp(p, "Set") == 0)
		{
			var = strtok(NULL, " ");
			if (var == NULL)
				continue;
			if (*var == '\0')
				continue;

			setto = strtok(NULL, " ");
			if (!setto)
				continue;
			setto = strtok(NULL, "");
			if (!setto)
				continue;
			/* Is it a aint variable */
			if (strcmp(var, "iNAH") == 0)
			{
				iNAH = atoi(setto);
			}
			else
				/* uhm networks */
			if (strcmp(var, "ircnetwork") == 0)
			{
				AllocCpy(ircnetwork, setto);
			}
			else if (strcmp(var, "defserv") == 0)
			{
				AllocCpy(defserv, setto);
			}
			else if (strcmp(var, "SERVICES_NAME") == 0)
			{
				AllocCpy(SERVICES_NAME, setto);
			}
			else if (strcmp(var, "oper_host") == 0)
			{
				AllocCpy(oper_host, setto);
			}
			else if (strcmp(var, "admin_host") == 0)
			{
				AllocCpy(admin_host, setto);
			}
			else if (strcmp(var, "locop_host") == 0)
			{
				AllocCpy(locop_host, setto);
			}
			else if (strcmp(var, "sadmin_host") == 0)
			{
				AllocCpy(sadmin_host, setto);
			}
			else if (strcmp(var, "netadmin_host") == 0)
			{
				AllocCpy(netadmin_host, setto);
			}
			else if (strcmp(var, "coadmin_host") == 0)
			{
				AllocCpy(coadmin_host, setto);
			}
			else if (strcmp(var, "hidden_host") == 0)
			{
				AllocCpy(hidden_host, setto);
			}
			else if (strcmp(var, "netdomain") == 0)
			{
				AllocCpy(netdomain, setto);
			}
			else if (strcmp(var, "helpchan") == 0)
			{
				AllocCpy(helpchan, setto);
			}
			else if (strcmp(var, "STATS_SERVER") == 0)
			{
				AllocCpy(STATS_SERVER, setto);
			}
		}
	}
	if (type == 0)
	{
		fprintf(stderr, "* Loaded %s ..\n", INCLUDE);
	}
	else
	{
		sendto_realops("Loaded %s ..", INCLUDE);
	}
	return 0;
}


void doneconf(int type)
{
	/* calculate on errors */
	char *errormsg = MyMalloc(16384);

	*errormsg = '\0';
	if (!KLINE_ADDRESS || (*KLINE_ADDRESS == '\0'))
		strcat(errormsg, "- Missing KLINE_ADDRESS\n");
#ifndef DEVELOP
	if (KLINE_ADDRESS)
		if (!strchr(KLINE_ADDRESS, '@') && !strchr(KLINE_ADDRESS, ':'))
		{
			strcat(errormsg,
			    "- KLINE_ADDRESS is not an e-mail or an URL\n");
		}
#endif
	if ((MAXCHANNELSPERUSER < 1))
		strcat(errormsg,
		    "- MAXCHANNELSPERUSER is an invalid value, must be > 0\n");
	if ((iNAH < 0) || (iNAH > 1))
		strcat(errormsg, "- iNAH is an invalid value\n");
	if (AUTO_JOIN_CHANS == '\0')
		strcat(errormsg, "- AUTO_JOIN_CHANS is an invalid value\n");
	if (OPER_AUTO_JOIN_CHANS == '\0')
		strcat(errormsg,
		    "- OPER_AUTO_JOIN_CHANS is an invalid value\n");
	if (HOST_TIMEOUT < 0 || HOST_TIMEOUT > 180)
		strcat(errormsg, "- HOST_TIMEOUT is an invalid value\n");
	if (HOST_RETRIES < 0 || HOST_RETRIES > 10)
		strcat(errormsg, "- HOST_RETRIES is an invalid value\n");
#define Missing(x) !(x) || (*(x) == '\0')
	if (Missing(defserv))
		strcat(errormsg, "- Missing defserv field\n");
	if (Missing(SERVICES_NAME))
		strcat(errormsg, "- Missing SERVICES_NAME field\n");
	if (Missing(oper_host))
		strcat(errormsg, "- Missing oper_host field\n");
	if (Missing(admin_host))
		strcat(errormsg, "- Missing admin_host field\n");
	if (Missing(locop_host))
		strcat(errormsg, "- Missing locop_host field\n");
	if (Missing(sadmin_host))
		strcat(errormsg, "- Missing sadmin_host field\n");
	if (Missing(netadmin_host))
		strcat(errormsg, "- Missing netadmin_host field\n");
	if (Missing(coadmin_host))
		strcat(errormsg, "- Missing coadmin_host field\n");
	if (Missing(hidden_host))
		strcat(errormsg, "- Missing hidden_host field\n");
	if (Missing(netdomain))
		strcat(errormsg, "- Missing netdomain field\n");
	if (Missing(helpchan))
		strcat(errormsg, "- Missing helpchan field\n");
	if (Missing(STATS_SERVER))
		strcat(errormsg, "- Missing STATS_SERVER field\n");
	if (Missing(iConf.socksbanmessage))
		strcat(errormsg, "- Missing SOCKSBANMESSAGE field\n");
	if (Missing(iConf.socksquitmessage))
		strcat(errormsg, "- Missing SOCKSQUITMESSAGE field\n");
	if (*errormsg != '\0')
	{
		fprintf(stderr,
		    "\n*** ERRORS IN DYNAMIC CONFIGURATION (listed below) ***\n");
		fprintf(stderr, errormsg);
		MyFree(errormsg);
		exit(-1);
	}
	MyFree(errormsg);
}

void init_dynconf(void)
{
	bzero(&iConf, sizeof(iConf));
}

/* Report the unrealircd.conf info -codemastr*/
void report_dynconf(aClient *sptr)
{
	sendto_one(sptr, ":%s %i %s :*** Dynamic Configuration Report ***",
	    me.name, RPL_TEXT, sptr->name);
	sendto_one(sptr, ":%s %i %s :INCLUDE: %s", me.name, RPL_TEXT,
	    sptr->name, INCLUDE);
	sendto_one(sptr, ":%s %i %s :KLINE_ADDRESS: %s", me.name, RPL_TEXT,
	    sptr->name, KLINE_ADDRESS);
	sendto_one(sptr, ":%s %i %s :MODE_X: %d", me.name, RPL_TEXT, sptr->name,
	    MODE_X);
	sendto_one(sptr, ":%s %i %s :MODE_STRIPWORDS: %d", me.name, RPL_TEXT,
	    sptr->name, MODE_STRIPWORDS);
	sendto_one(sptr, ":%s %i %s :MODE_I: %d", me.name, RPL_TEXT, sptr->name,
	    MODE_I);
	sendto_one(sptr, ":%s %i %s :TRUEHUB: %d", me.name, RPL_TEXT,
	    sptr->name, TRUEHUB);
	sendto_one(sptr, ":%s %i %s :SHOWOPERS: %d", me.name, RPL_TEXT,
	    sptr->name, SHOWOPERS);
	sendto_one(sptr, ":%s %i %s :KILLDIFF: %d", me.name, RPL_TEXT,
	    sptr->name, KILLDIFF);
	sendto_one(sptr, ":%s %i %s :SHOWOPERMOTD: %d", me.name, RPL_TEXT,
	    sptr->name, SHOWOPERMOTD);
	sendto_one(sptr, ":%s %i %s :HIDE_ULINES: %d", me.name, RPL_TEXT,
	    sptr->name, HIDE_ULINES);
	sendto_one(sptr, ":%s %i %s :ALLOW_CHATOPS: %d", me.name, RPL_TEXT,
	    sptr->name, ALLOW_CHATOPS);
	sendto_one(sptr, ":%s %i %s :SOCKS_BAN_MESSAGE: %s", me.name, RPL_TEXT,
	    sptr->name, iConf.socksbanmessage);
	sendto_one(sptr, ":%s %i %s :SOCKS_QUIT_MESSAGE: %s", me.name, RPL_TEXT,
	    sptr->name, iConf.socksquitmessage);
	sendto_one(sptr, ":%s %i %s :SOCKSBANTIME: %i", me.name, RPL_TEXT,
	    sptr->name, iConf.socksbantime);
	sendto_one(sptr, ":%s %i %s :MAXCHANNELSPERUSER: %i", me.name, RPL_TEXT,
	    sptr->name, MAXCHANNELSPERUSER);
	sendto_one(sptr, ":%s %i %s :WEBTV_SUPPORT: %d", me.name, RPL_TEXT,
	    sptr->name, WEBTV_SUPPORT);
	sendto_one(sptr, ":%s %i %s :NO_OPER_HIDING: %d", me.name, RPL_TEXT,
	    sptr->name, NO_OPER_HIDING);
	sendto_one(sptr, ":%s %i %s :AUTO_JOIN_CHANS: %s", me.name, RPL_TEXT,
	    sptr->name, AUTO_JOIN_CHANS);
	sendto_one(sptr, ":%s %i %s :OPER_AUTO_JOIN_CHANS: %s", me.name,
	    RPL_TEXT, sptr->name, OPER_AUTO_JOIN_CHANS);
	sendto_one(sptr, ":%s %i %s :HOST_TIMEOUT: %li", me.name, RPL_TEXT,
	    sptr->name, HOST_TIMEOUT);
	sendto_one(sptr, ":%s %i %s :HOST_RETRIES: %d", me.name, RPL_TEXT,
	    sptr->name, HOST_RETRIES);
}

/* Report the network file info -codemastr */
void report_network(aClient *sptr)
{
	sendto_one(sptr, ":%s %i %s :*** Network Configuration Report ***",
	    me.name, RPL_TEXT, sptr->name);
	sendto_one(sptr, ":%s %i %s :NETWORK: %s", me.name, RPL_TEXT,
	    sptr->name, ircnetwork);
	sendto_one(sptr, ":%s %i %s :DEFAULT_SERVER: %s", me.name, RPL_TEXT,
	    sptr->name, defserv);
	sendto_one(sptr, ":%s %i %s :SERVICES_NAME: %s", me.name, RPL_TEXT,
	    sptr->name, SERVICES_NAME);
	sendto_one(sptr, ":%s %i %s :OPER_HOST: %s", me.name, RPL_TEXT,
	    sptr->name, oper_host);
	sendto_one(sptr, ":%s %i %s :ADMIN_HOST: %s", me.name, RPL_TEXT,
	    sptr->name, admin_host);
	sendto_one(sptr, ":%s %i %s :LOCOP_HOST: %s", me.name, RPL_TEXT,
	    sptr->name, locop_host);
	sendto_one(sptr, ":%s %i %s :SADMIN_HOST: %s", me.name, RPL_TEXT,
	    sptr->name, sadmin_host);
	sendto_one(sptr, ":%s %i %s :NETADMIN_HOST: %s", me.name, RPL_TEXT,
	    sptr->name, netadmin_host);
	sendto_one(sptr, ":%s %i %s :COADMIN_HOST: %s", me.name, RPL_TEXT,
	    sptr->name, coadmin_host);
	sendto_one(sptr, ":%s %i %s :HIDDEN_HOST: %s", me.name, RPL_TEXT,
	    sptr->name, hidden_host);
	sendto_one(sptr, ":%s %i %s :NETDOMAIN: %s", me.name, RPL_TEXT,
	    sptr->name, netdomain);
	sendto_one(sptr, ":%s %i %s :HELPCHAN: %s", me.name, RPL_TEXT,
	    sptr->name, helpchan);
	sendto_one(sptr, ":%s %i %s :STATS_SERVER: %s", me.name, RPL_TEXT,
	    sptr->name, STATS_SERVER);
	sendto_one(sptr, ":%s %i %s :INAH: %i", me.name, RPL_TEXT, sptr->name,
	    iNAH);
}
