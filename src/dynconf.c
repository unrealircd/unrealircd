/************************************************************************
 *   IRC - Internet Relay Chat, dynconf.c
 *   (C) 1999 Carsten Munk (Techie/Stskeeps) <cmunk@toybox.flirt.org>
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

#define DYNCONF_C
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "userload.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <utmp.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

ID_CVS("$Id$");
ID_Copyright("(C) 1999 Carsten Munk");
#define DoDebug fprintf(stderr, "[%s] %s | %li\n", babuf, __FILE__, __LINE__);
#define AllocCpy(x,y) if ((x) && type == 1) MyFree((x)); x = (char *) MyMalloc(strlen(y) + 1); strcpy(x,y)
#define XtndCpy(x,y) x = (char *) MyMalloc(strlen(y) + 2); *x = '\0'; strcat(x, "*"); strcpy(x,y)

aConfiguration iConf;
int	 icx = 0;
void iCstrip(char *line)
{
        char *c;

        if ((c = strchr(line, '\n')))   *c = '\0';
        if ((c = strchr(line, '\r')))   *c = '\0';
}

/*
        	for (q = p; *q; q++) {
                	printf("%x ", *q);
                }
*/


/* Get the unrealircd.conf/*.network file version and check it */
char *get_version(char *file) {
	FILE *fd = fopen(file, "r");
	char buf[24], *tmp = '\0', *buf2 = '\0', *version = '\0';

	/* This should never happen, but we'll keep it just to be safe */
	if (!fd) {
#ifdef _WIN32
	        MessageBox(NULL, "UnrealIRCD/32 Init Error", "Unable to load dynamic config (or network) file !! ", MB_OK);
#else
		fprintf(stderr, "[error] Couldn't load %s !!!\n", file);
#endif        
		exit(-1);
	}
/* We only want to read the first line */
	fgets(buf, 24, fd);
	tmp = strtok(buf, "\n");
/* Make sure that it is a valid version line */
	buf2 = strtok(tmp,"^");
/* If it isn't exit */
	if (strcmp(buf2, "ver")) {
#ifdef _WIN32
		MessageBox(NULL, "Dynamic config file (or network file) is not valid, visit http://unreal.tspre.org to learn how to upgrade", "UnrealIRCD/32 Init Error", MB_OK);
#else
		fprintf(stderr, "%s is invalid, visit http://unreal.tspre.org to learn how to upgrade", file);
#endif
		exit(-1);
	}
	/* If it is we get the version number */
	version = strtok(NULL, "");
	fclose(fd);
	/* Now return the version */
	return version;
}

int		load_conf(char	*file, int type)
{
	FILE	*zConf;
        char    *stat;
	char	*buf = MyMalloc(1024);
	char	*babuf = MyMalloc(1024);
	char	*p, *q;
	char	*var, *setto;          
    	long    aint;
	int 	v1, v2;
	/* Since we have no real way of knowing what file we are dealing with, we will check
	 * for *.conf then get the version */
	if(!match("*.conf",file)) {
		if (strcmp(get_version(file), "1.1")) {
#ifdef _WIN32
	
			MessageBox(NULL, "Dynamic config file is not valid, visit http://unreal.tspre.org to learn how to upgrade", "UnrealIRCD/32 Init Error", MB_OK);
#else
			fprintf(stderr, "%s is invalid, visit http://unreal.tspre.org to learn how to upgrade\n", file);
#endif
			exit(-1);
		}
}

	zConf = fopen(file, "r");
	if (zConf == NULL && (type == 0)) {
#ifdef _WIN32
	        MessageBox(NULL, "UnrealIRCD/32 Init Error", "Unable to load dynamic config (or network) file !! ", MB_OK);
#else
		fprintf(stderr, "[error] Couldn't load %s !!!\n", file);
#endif        
		exit(-1);
	}
	else
	 if (zConf == NULL && (type == 1)) 
	 {
	 	sendto_ops("[error] Couldnt load dynconf file %s !!", file);
	 }
	*buf = '\1';
        stat = buf;
		
	while (stat != NULL)
	{
		stat = fgets(buf,1020, zConf);
		if (*buf == '#')       /* comment */
			continue;
		if (*buf == '/')       /* comment */
			continue;
		iCstrip(buf);    /* strip crlf .. */
		strcpy(babuf, buf);
		if (*buf == '\0')
			continue;
		
	      	p = strtok(buf, " ");
		if (strcmp(p, "Include")==0)
		{
	 			strtok(NULL, " ");
                setto = strtok(NULL, "");
				/* We need this for STATS S */
				AllocCpy(INCLUDE, setto);
#ifndef _WIN32
#endif
				/* Check the version before we read the file */
			
				if (strcmp(get_version(setto),"2.2"))
				{
#ifdef _WIN32
					MessageBox(NULL, "Network file is not valid, visit http://unreal.tspre.org to learn how to upgrade", "UnrealIRCD/32 Init Error", MB_OK);
#else
					fprintf(stderr, "%s is invalid, visit http://unreal.tspre.org to learn how to upgrade\n", setto);
#endif
					exit(-1);
				}

	 			load_conf(setto,type);
		}
		
		 else
		if (strcmp(p, "Set")==0)
		{
			var = strtok(NULL, " ");
			/* Yes it was a good idea when i moved it. */
			if (var == NULL)
				continue;
			if (*var == '\0')
				continue;

			strtok(NULL, " ");
			setto = strtok(NULL, "");
			if (setto == NULL)
				continue;
			if (*setto == '\0')
				continue;
	
			if (*setto >= '0' && *setto <= '9') {
				aint = atol(setto);
				if (strcmp(var, "MODE_X")==0)
					MODE_X = aint;
					else
				if (strcmp(var, "MODE_I")==0)
					MODE_I = aint;
					else
				if (strcmp(var, "TRUEHUB")==0) {
					TRUEHUB = aint;
#ifndef HUB	
					/* Only display if _not_ called thru /rehash */
				if (aint == 1 && type == 0) {
#ifdef _WIN32
				        MessageBox(NULL, "UnrealIRCD/32 Init Error", "I'm a leaf NOT an hub! ", MB_OK);
												
#else
					fprintf(stderr, "[error] I'm a leaf NOT a hub!\n");
#endif
					return 0;
				}
#endif
			}
				if (strcmp(var, "CONFIG_FILE_STOP")==0) {
					if (aint == 1) {
#ifdef _WIN32
				        MessageBox(NULL, "UnrealIRCD/32 Init Error", "Read Config stop code in file", MB_OK);											
#else
						fprintf(stderr, "[fatal error] File stop code recieved in %s - RTFM\n", file);
#endif
						exit(-1);
					}
				}

				if (strcmp(var, "SHOWOPERS")==0) {
					SHOWOPERS = aint;
				}
				if (strcmp(var, "SHOWOPERMOTD")==0) {
					SHOWOPERMOTD = aint;
				}
				if (strcmp(var, "SOCKSBANTIME")==0) {
					iConf.socksbantime = aint;
				}
		       	     	if (strcmp(var, "iNAH")==0) {
       	     				iNAH = aint;
       				}
				if (strcmp(var, "ALLOW_CHATOPS")==0) {
					ALLOW_CHATOPS = aint;
				}
				if (strcmp(var, "HIDE_ULINES")==0) {
					HIDE_ULINES = aint;
				}				
				if (strcmp(var, "KILLDIFF")==0) {
             			KILLDIFF = aint;
             	}
                		continue;
        	 }
/*			 if (strcmp(var, "DOMAINNAME")==0) {
			 	AllocCpy(DOMAINNAME, setto);
			 	XtndCpy(DOMAINNAMEMASK, setto); 
			 }
*/
			 /* uhm networks */
			 if (strcmp(var, "ircnetwork")==0) {
			 	AllocCpy(ircnetwork, setto);
			 } 
			 	else
			 if (strcmp(var, "defserv")==0) {
			 	AllocCpy(defserv, setto);
			 }
			 	else
			 if (strcmp(var, "SERVICES_NAME")==0) {
			 	AllocCpy(SERVICES_NAME, setto);
			 }
			 	else
			 if (strcmp(var, "oper_host")==0) {
			 	AllocCpy(oper_host, setto);
			 }
				else
			 if (strcmp(var, "admin_host")==0) {
			 	AllocCpy(admin_host, setto);
			 }
				else
			 if (strcmp(var, "locop_host")==0) {
			 	AllocCpy(locop_host, setto);
			 }
			 	else
			 if (strcmp(var, "sadmin_host")==0) {
			 	AllocCpy(sadmin_host, setto);
			 }
				else
			 if (strcmp(var, "netadmin_host")==0) {
			 	AllocCpy(netadmin_host, setto);
			 }			 
             	else
			 if (strcmp(var, "techadmin_host")==0) {
			 	AllocCpy(techadmin_host, setto);
			 }			 
             	else
             if (strcmp(var, "coadmin_host")==0) {
             	AllocCpy(coadmin_host, setto);
             }
             	else
             if (strcmp(var, "hidden_host")==0) {
             	AllocCpy(hidden_host, setto);
             }
             	else
             if (strcmp(var, "netdomain")==0) {
             	AllocCpy(netdomain, setto);
             }
				else
			 if (strcmp(var, "helpchan")==0) {
			 	AllocCpy(helpchan, setto);
			 }
			 	else
			 if (strcmp(var, "STATS_SERVER")==0) {
			 	AllocCpy(STATS_SERVER, setto);
			 }
			 	else
			if (strcmp(var, "KLINE_ADDRESS")==0) {
				AllocCpy(KLINE_ADDRESS, setto);
			}
				else
			if (strcmp(var, "SOCKS_BAN_MESSAGE")==0) {
				AllocCpy(iConf.socksbanmessage, setto);
			}
				else
			if (strcmp(var, "SOCKS_QUIT_MESSAGE")==0) {
				AllocCpy(iConf.socksquitmessage, setto);
			}
			   			
         }
	}
#ifndef _WIN32
	fprintf(stderr, "* Loaded %s ..\n", file);
#endif	
}

void	doneconf(void) {
	
}

/* Report the unrealircd.conf info -codemastr*/
void report_dynconf (aClient *sptr) 
{
	sendto_one(sptr, ":%s %i %s :*** Dynamic Configuration Report ***", me.name, RPL_TEXT, sptr->name);
	sendto_one(sptr, ":%s %i %s :INCLUDE: %s", me.name, RPL_TEXT, sptr->name, INCLUDE);
	sendto_one(sptr, ":%s %i %s :KLINE_ADDRESS: %s", me.name, RPL_TEXT, sptr->name, KLINE_ADDRESS);
	sendto_one(sptr, ":%s %i %s :MODE_X: %i", me.name, RPL_TEXT, sptr->name, MODE_X);
	sendto_one(sptr, ":%s %i %s :MODE_I: %i", me.name, RPL_TEXT, sptr->name, MODE_I);
	sendto_one(sptr, ":%s %i %s :TRUEHUB: %i", me.name, RPL_TEXT, sptr->name, TRUEHUB);
	sendto_one(sptr, ":%s %i %s :SHOWOPERS: %i", me.name, RPL_TEXT, sptr->name, SHOWOPERS);
	sendto_one(sptr, ":%s %i %s :KILLDIFF: %i", me.name, RPL_TEXT, sptr->name, KILLDIFF);
	sendto_one(sptr, ":%s %i %s :SHOWOPERMOTD: %i", me.name, RPL_TEXT, sptr->name, SHOWOPERMOTD);
	sendto_one(sptr, ":%s %i %s :HIDE_ULINES: %i", me.name, RPL_TEXT, sptr->name, HIDE_ULINES);
	sendto_one(sptr, ":%s %i %s :ALLOW_CHATOPS: %i", me.name, RPL_TEXT, sptr->name, ALLOW_CHATOPS);
	sendto_one(sptr, ":%s %i %s :SOCKS_BAN_MESSAGE: %s", me.name, RPL_TEXT, sptr->name, iConf.socksbanmessage);
	sendto_one(sptr, ":%s %i %s :SOCKS_QUIT_MESSAGE: %s", me.name, RPL_TEXT, sptr->name, iConf.socksquitmessage);
	sendto_one(sptr, ":%s %i %s :SOCKSBANTIME: %i",me.name, RPL_TEXT, sptr->name, iConf.socksbantime);
}

/* Report the network file info -codemastr */
void report_network (aClient *sptr)
{
sendto_one(sptr, ":%s %i %s :*** Network Configuration Report ***", me.name, RPL_TEXT, sptr->name);
sendto_one(sptr, ":%s %i %s :NETWORK: %s", me.name, RPL_TEXT, sptr->name, ircnetwork);
sendto_one(sptr, ":%s %i %s :DEFAULT_SERVER: %s", me.name, RPL_TEXT, sptr->name, defserv);
sendto_one(sptr, ":%s %i %s :SERVICES_NAME: %s", me.name, RPL_TEXT, sptr->name, SERVICES_NAME);
sendto_one(sptr, ":%s %i %s :OPER_HOST: %s", me.name, RPL_TEXT, sptr->name, oper_host);
sendto_one(sptr, ":%s %i %s :ADMIN_HOST: %s", me.name, RPL_TEXT, sptr->name, admin_host);
sendto_one(sptr, ":%s %i %s :LOCOP_HOST: %s", me.name, RPL_TEXT, sptr->name, locop_host);
sendto_one(sptr, ":%s %i %s :SADMIN_HOST: %s", me.name, RPL_TEXT, sptr->name, sadmin_host);
sendto_one(sptr, ":%s %i %s :NETADMIN_HOST: %s", me.name, RPL_TEXT, sptr->name, netadmin_host);
sendto_one(sptr, ":%s %i %s :COADMIN_HOST: %s", me.name, RPL_TEXT, sptr->name, coadmin_host);
sendto_one(sptr, ":%s %i %s :TECHADMIN_HOST: %s", me.name, RPL_TEXT, sptr->name, techadmin_host);
sendto_one(sptr, ":%s %i %s :HIDDEN_HOST: %s", me.name, RPL_TEXT, sptr->name, hidden_host);
sendto_one(sptr, ":%s %i %s :NETDOMAIN: %s", me.name, RPL_TEXT, sptr->name, netdomain);
sendto_one(sptr, ":%s %i %s :HELPCHAN: %s", me.name, RPL_TEXT, sptr->name, helpchan);
sendto_one(sptr, ":%s %i %s :STATS_SERVER: %s", me.name, RPL_TEXT, sptr->name, STATS_SERVER);
sendto_one(sptr, ":%s %i %s :INAH: %i", me.name, RPL_TEXT, sptr->name, iNAH);
}
