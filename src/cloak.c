/************************************************************************
 *   IRC - Internet Relay Chat, src/cloak.c
 *   (C) VirtualWorld code made originally by RogerY (rogery@austnet.org)
 *   Some coding by Potvin (potvin@shadownet.org)
 *   Modified by Stskeeps with some TerraX codebits 
 *    TerraX (devcom@terrax.net) - great job guys!
 *    Stskeeps (stskeeps@tspre.org)
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

/*
#ifdef lint
static char sccxid[] = "@(#)cloak.c		9.00 7/12/99 UnrealIRCd";
#endif
*/
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "userload.h"
#include <sys/stat.h>
#include <fcntl.h>
#ifndef _WIN32
#include <utmp.h>
#endif
#include "h.h"

ID_CVS("$Id$");

// #define iNAH /* networks.h now */
/* Hidden host code below */

#define MAXVIRTSIZE     (3 + 5 + 1)
#define HASHVAL_TOTAL   30011
#define HASHVAL_PARTIAL 211

extern  aClient me;
extern  int     seed;
int     match(char *, char *), find_exception(char *);

extern unsigned char tolowertab[];

int str2array(char **pparv, char *string, char *delim)
{
    char *tok;
    int pparc=0;

    tok=(char *) strtok((char *) string,delim);
    while(tok != NULL)
    {
        pparv[pparc++]=tok;
        tok= (char *) strtok((char *) NULL,(char *) delim);
    }

    return pparc;
}


void truncstring(char *stringvar, int firstlast, int amount){
   if (firstlast)
   {
    stringvar+=amount;
    *stringvar=0;
    stringvar-=amount;
   }
    else
   {
    stringvar+=strlen(stringvar);
    stringvar-=amount;
   }
}

#define B_BASE                  1000

int Maskchecksum (char *data, int len)
{
	int                     i;
	int                     j;

	j=0;
	for (i=0 ; i<len ; i++)
	{
	  j += *data++ * (i < 16 ? (i+1)*(i+1) : i*(i-15));
	}

	return (j+B_BASE)%0xffff;
}


/* hidehost
 * command takes the realhost of a user
 * and changes the content of it.
 * new hidehost by vmlinuz
 * added some extra fixes by stskeeps
 * originally based on TerraIRCd
 */

char *hidehost (char *s, int useless)
{
//	static char mask[128];
	char		*mask;
	static char ipmask[64];
	int         csum;
	char        *dot,*tmp;
    char	    *cp;
	int			i, isdns;
    int 		dots = 0;
	
	mask = MyMalloc(129);
	memset (mask, 0, 128);

	csum = Maskchecksum (s, strlen(s));

	if (strlen (s) > 127)           /* this isn't likely to happen: s is limited to HOSTLEN+1 (64) */
	{
		s[128] = 0;
	}

	isdns = 0;
	cp = s;
	for (i=0; i < strlen(s); i++)
	{
		if (*cp == '.') {
			dots++;
		}
        cp++;
	}

	for (i=0 ; i<strlen(s) ; i++)
	{
		if (s[i] == '.') {
    	  	continue;
        }
		
		if (isalpha(s[i])) 
		{
			isdns = 1;
			break;
		}
	}
	
	if (isdns)
	{
		/* it is a resolved yes.. */
		if (dots == 1) {       /* mystro.org f.x */
			sprintf(mask, "%s%c%d.%s",
							hidden_host,
       	                 	(csum < 0 ? '=' : '-'),
			 				(csum < 0 ? -csum : csum), s);
        }
		if (dots == 0) {       /* localhost */
			sprintf(mask, "%s%c%d",
							s,
                        	(csum < 0 ? '=' : '-'),
						(csum < 0 ? -csum : csum));
		}

		if (dots > 1) {
			dot = (char *) strchr((char *) s, '.');
			
			/* mask like *<first dot> */
			sprintf(mask, "%s%c%d.%s",
						hidden_host,
                        (csum < 0 ? '=' : '-'),
						(csum < 0 ? -csum : csum), dot+1);
        }
 	}
	else 
	{
		strncpy(ipmask, s, sizeof(ipmask));
		ipmask[sizeof(ipmask)-1]='\0';      /* safety check */
		dot = (char *) strrchr((char *) ipmask, '.');
		*dot = '\0';
		
		if (dot == NULL)     /* dot should never be NULL: IP needs dots */
			  sprintf (mask, "%s%c%i",
			  		hidden_host,
				 	(csum < 0 ? '=' : '-'),
		 		 	(csum < 0 ? -csum : csum));
			  else
		  			sprintf (mask, "%s.%s%c%i",
		  							ipmask,
									hidden_host,
									(csum < 0 ? '=' : '-'),
									(csum < 0 ? -csum : csum));
	}

ok1:		
	return mask;
}

						  		      				        	        									      								                					                	                																						       	                 			                 					                        																                        			                 				  				  		    		  			   														  		  		  				  								
/* Regular user host */
void    make_virthost(char *curr, char *new)
{
	char *mask;
	if (curr == NULL)
		return;
	if (new == NULL)
		return;
		
	mask = hidehost(curr, 0);
	
	strncpyzt(new, mask, HOSTLEN); /* */
	return;
}

/* Netadmin host */
void	make_netadminhost(char *new)
{
    char     tmpnew[HOSTLEN];

#ifndef iNAH2
        sprintf(tmpnew, "%s", netadmin_host);
        strncpyzt(new, tmpnew, HOSTLEN);
#endif
        return;

}
/* Coadmin host */
void    make_coadminhost(char *new)
{
    char     tmpnew[HOSTLEN];

#ifndef iNAH2
        sprintf(tmpnew, "%s", coadmin_host);
        strncpyzt(new, tmpnew, HOSTLEN);
#endif
        return;
}
/* Techadmin host */
void    make_techadminhost(char *new)
{
    char     tmpnew[HOSTLEN];
#ifndef iNAH2
        sprintf(tmpnew, "%s", techadmin_host);
        strncpyzt(new, tmpnew, HOSTLEN);
#endif
        return;
}
/* Server admin host */
void    make_adminhost(char *new)
{
    char     tmpnew[HOSTLEN];
#ifndef iNAH2
        sprintf(tmpnew, "%s", admin_host);
        strncpyzt(new, tmpnew, HOSTLEN);
#endif
        return;
}
/* Service admin host */
void    make_sadminhost(char *new)
{
    char     tmpnew[HOSTLEN];
#ifndef iNAH2
        sprintf(tmpnew, "%s", sadmin_host);
        strncpyzt(new, tmpnew, HOSTLEN);
#endif
        return;
}
/* Global Oper host */
void    make_operhost(char *new)
{
    char     tmpnew[HOSTLEN];
#ifndef iNAH2
        sprintf(tmpnew, "%s", oper_host);
        strncpyzt(new, tmpnew, HOSTLEN);
#endif
        return;
}
/* Local Oper host */
void    make_locophost(char *new)
{
    char     tmpnew[HOSTLEN];
#ifndef iNAH2
        sprintf(tmpnew, "%s", locop_host);
        strncpyzt(new, tmpnew, HOSTLEN);
#endif
        return;
}

/* Make SETHOST */

void	make_sethost(char *new, char *new2)
{
		char	tmpnew[HOSTLEN];
		
		sprintf(tmpnew,"%s", new2);
		strncpyzt(new, tmpnew, HOSTLEN);
		return;
}
/* make setname */
void	make_setname(char *new, char *new2)
{
		char	tmpnew[REALLEN];
		
		sprintf(tmpnew,"%s", new2);
		strncpyzt(new, tmpnew, REALLEN);
		return;
}
/* make setident */

void	make_setident(char *new, char *new2)
{
		char	tmpnew[USERLEN];
		
		sprintf(tmpnew, "%s", new2);
		strncpyzt(new, tmpnew, USERLEN);
		return;
}