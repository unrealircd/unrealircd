/*
 * Ok, this may not make sence to many, but it is, i do beleve time that we
 * stopped hacking the irc server and protocall, which was written MANY moons
 * ago, and should take the initiative, and stop using code thats 20 odd years
 * old.  Just to explain, when the original ircd was written, the host OS did
 * not (usually) have functions such as gethostbyaddres() and gethostbyname()
 * and as a result we have a fully fleged caching name server sitting in the
 * server, which now doesnt work on win32 platforms, and seems dogey else where
 * so this is an attempt to write it all in 1 file (plus a headder) using
 * moddern functions, which all platforms now have !
 *
 * David Flynn (September 2000)
 * 
 * This code is Copyright (C) the Unreal Development team
 *                        (C) David Flynn (Sept 2000)
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
 *
 */

/************************************************************/
/** WARNING :: WE ARE NOT LOOKING UP CACHED NAMES (YET) !! **/
/************************************************************/

/* Firstly, as this is a first version and i dont know how it is going to work
 * we will possibly have some caching support, but done in the simplest (possibly
 * best solution)
 */

/*
 * Things to note: The windows version of gethostbyaddr/name () uses a STATIC variable
 * to return to ... ie It reuses what it returned last time each time it is called !!!
 */

#ifndef _WIN32

#include <netdb.h>
#include <sys/socket.h>

#else /*_WIN32*/

#include <winsock2.h>
#include <windows.h>

#endif /*_WIN32*/

#include "struct.h"

/* These are because we havnt included the ircd's h files*/
extern char REPORT_FIN_DNS[128];
extern int R_fin_dns;
/* end */


typedef struct dnamecache
{
        unsigned long int ipaddr;
        struct hostent *cachedhost;
        struct dnamecache *next;
        struct dnamecache *prev;
}
DNSCache;

 DNSCache *first;
 DNSCache *last;


/*Prototypes*/
void newdns_lookupfromip(int);
struct hostent *newdns_checkcacheip(aClient *);
struct hostent *newdns_checkcachename(char *);

struct hostent *newdns_checkcachename(char *name)
{
	struct hostent *retval;
	/* Funnily enough, i am not (at the moment) going to cache this ... */

	retval = gethostbyname(name);

	return (retval);
}

struct hostent *newdns_checkcacheip(aClient *ac)
{
	DNSCache *i;
	int sendon=0;
	unsigned long int ipaddr = ac->ip.s_addr;

	SetDNS(ac);
    
	/* search through the cached names */

/*	if (first)
        for(i=first; i ;i=i->next)
        {
                if (ipaddr == i->ipaddr)
                {
                        /* Whoopy Do ... we have found a cached name !!! lets return it*/
                       // ac->hostp = i->cachedhost;
	//					ClearDNS(ac);
	//				return i->cachedhost;
      //          }
     //   }

	/* if we are here, then we either havnt got a cache table, OR, the look up wasnt successfull */
	sendon = ac;
	_beginthread(newdns_lookupfromip, 0, sendon); /* Start us up a thread */

	return (NULL);
}

//TODO::ADD Time Lookup functions ... for debug ... to see how long the DNS took !

#ifdef _WIN32

void newdns_lookupfromip(int senton)
{
        DNSCache *i;
		struct hostent *resolvedhost; /* What we will eventually return*/
		                              /*filled in by gethostbyaddr() */

		aClient *  ac=senton;   /* We are given the address of the struct, as i am a mong, i
					* I managed to differenciate the pointer twice .. hence the
					* /  *** / 's  That was a bugger ... anyway this all works*/

		unsigned long int ipaddr = (/ac)->ip.s_addr;

        /*We will need to check for cached names first, but code that second*/
        resolvedhost = gethostbyaddr(&(*ac).ip,4,AF_INET);

        /*ok we need to check if we actually managed to recieve anything*/
        if (!resolvedhost)  /*Ie its NULL*/
        {
			int e = WSAGetLastError();
			//windebug(WINDEBUG_FORCE,"newdns_lookupfromip: WSAGetLastError=%d",e);
            switch (h_errno)
            {
		case HOST_NOT_FOUND :
                        break;
                case NO_ADDRESS  :
                        break;
                case NO_RECOVERY :
			break;
                case TRY_AGAIN :
                        break;
                }
			_endthread();

        }

        /* if we are here, then all went well, so lets add the thing to the */
		
		/* Check that no one is fiddling with this ... if they are, wait for them to stop */

		{
			CRITICAL_SECTION critsec;

			InitializeCriticalSection(&critsec);

			EnterCriticalSection(&critsec);


			/* Set the Make Safe directive, so that nothing else fiddles with the thing while we do ..*/

		if (!first) /*For the first time, ie when we have not created our cache */
		{
		        /* we need to initialize the list */
		        last = first = malloc(sizeof(struct dnamecache));
	
			/* Put in the ipaddress ... we search using that */
			last->ipaddr = ipaddr;
            
			/* Allocate memory for the hostent */

			last->cachedhost = malloc(sizeof(struct hostent));

			/* Copy it */

			last->cachedhost->h_name = malloc(strlen(resolvedhost->h_name));
			memcpy (last->cachedhost->h_name, resolvedhost->h_name, strlen(resolvedhost->h_name));
			if(*resolvedhost->h_aliases)
			{
				last->cachedhost->h_aliases = malloc(strlen(*resolvedhost->h_aliases));
				memcpy (&last->cachedhost->h_aliases,&resolvedhost->h_aliases, 
						strlen(*resolvedhost->h_aliases));
			}
			last->cachedhost->h_addrtype = resolvedhost->h_addrtype;
			last->cachedhost->h_length = resolvedhost->h_length;
			last->cachedhost->h_addr_list = malloc(resolvedhost->h_length);
			memcpy (&last->cachedhost->h_addr_list, &resolvedhost->h_addr_list, resolvedhost->h_length);	

			(ac)->hostp= last->cachedhost;

#ifdef SHOWCONNECTINFO
			send((ac)->fd, REPORT_FIN_DNS, R_fin_dns, 0);
#endif
			ClearDNS(ac);
			if (!DoingAuth(ac))
				SetAccess(ac);

			LeaveCriticalSection(&critsec);

			_endthread();

		        return;
	        }

	        if (!last->next) /*If we have already initialised the cache, and point to the end*/
	        {
	           	/* we need to initialize the list */
		        last->next = malloc(sizeof(struct dnamecache));
			last->next->prev = last;
			last		= last->next;

			/* Put in the ipaddress ... we search using that */
			last->ipaddr = ipaddr;
           
			/* Allocate memory for the hostent */

			last->cachedhost = malloc(sizeof(struct hostent));
		
			/* Copy it */

			last->cachedhost->h_name = malloc(strlen(resolvedhost->h_name));
			memcpy (last->cachedhost->h_name, resolvedhost->h_name, strlen(resolvedhost->h_name));
			if(*resolvedhost->h_aliases)
			{
				last->cachedhost->h_aliases = malloc(strlen(*resolvedhost->h_aliases));
				memcpy (&last->cachedhost->h_aliases,&resolvedhost->h_aliases, strlen(*resolvedhost->h_aliases));
			}
			last->cachedhost->h_addrtype = resolvedhost->h_addrtype;
			last->cachedhost->h_length = resolvedhost->h_length;
			last->cachedhost->h_addr_list = malloc(resolvedhost->h_length);
			memcpy (&last->cachedhost->h_addr_list, &resolvedhost->h_addr_list, resolvedhost->h_length);

			(ac)->hostp= last->cachedhost;
			
#ifdef SHOWCONNECTINFO
			send((ac)->fd, REPORT_FIN_DNS, R_fin_dns, 0);
#endif
			ClearDNS(ac);
			if (!DoingAuth(ac))
				SetAccess(ac);

			LeaveCriticalSection(&critsec);

			_endthread();

		       return;
	        }

		LeaveCriticalSection(&critsec);
		_endthread();

	}

			/*Shit ... we have a problem if we reach here !!!*/
}

#else /*_WIN32*/
void newdns_lookupfromip(int senton)
{
        DNSCache *i;
	struct hostent *resolvedhost; /* What we will eventually return*/
	                              /*filled in by gethostbyaddr() */

	aClient *  ac=senton;   /* We are given the address of the struct, as i am a mong, i
				* I managed to differenciate the pointer twice .. hence the
				* /  *** / 's  That was a bugger ... anyway this all works*/

	unsigned long int ipaddr = (/ac)->ip.s_addr;

        /*We will need to check for cached names first, but code that second*/
        resolvedhost = gethostbyaddr(&(*ac).ip,4,AF_INET);

        /*ok we need to check if we actually managed to recieve anything*/
        if (!resolvedhost)  /*Ie its NULL*/
        {
            switch (h_errno)
            {
		case HOST_NOT_FOUND :
                        break;
                case NO_ADDRESS  :
                        break;
                case NO_RECOVERY :
			break;
                case TRY_AGAIN :
                        break;
                }
            _endthread();

        }

        /* if we are here, then all went well, so lets add the thing to the */
		
		/* Check that no one is fiddling with this ... if they are, wait for them to stop */

		{
			CRITICAL_SECTION critsec;

			InitializeCriticalSection(&critsec);

			EnterCriticalSection(&critsec);


			/* Set the Make Safe directive, so that nothing else fiddles with the thing while we do ..*/

		    if (!first) /*For the first time, ie when we have not created our cache */
		    {
		        /* we need to initialize the list */
		        last = first = malloc(sizeof(struct dnamecache));
	
				/* Put in the ipaddress ... we search using that */
				last->ipaddr = ipaddr;
            
				/* Allocate memory for the hostent */

				last->cachedhost = malloc(sizeof(struct hostent));

				/* Copy it */

				last->cachedhost->h_name = malloc(strlen(resolvedhost->h_name));
				memcpy (last->cachedhost->h_name, resolvedhost->h_name, strlen(resolvedhost->h_name));
				if(*resolvedhost->h_aliases)
				{
					last->cachedhost->h_aliases = malloc(strlen(*resolvedhost->h_aliases));
					memcpy (&last->cachedhost->h_aliases,&resolvedhost->h_aliases, 
							strlen(*resolvedhost->h_aliases));
				}
				last->cachedhost->h_addrtype = resolvedhost->h_addrtype;
				last->cachedhost->h_length = resolvedhost->h_length;
				last->cachedhost->h_addr_list = malloc(resolvedhost->h_length);
				memcpy (&last->cachedhost->h_addr_list, &resolvedhost->h_addr_list, resolvedhost->h_length);	

				(ac)->hostp= last->cachedhost;

#ifdef SHOWCONNECTINFO
				write((ac)->fd, REPORT_FIN_DNS, R_fin_dns);
#endif
				ClearDNS(ac);
				if (!DoingAuth(ac))
					SetAccess(ac);

				LeaveCriticalSection(&critsec);

				_endthread();

	        return;
	        }

	        if (!last->next) /*If we have already initialised the cache, and point to the end*/
	        {
	            /* we need to initialize the list */
	            last->next = malloc(sizeof(struct dnamecache));
				last->next->prev = last;
				last		= last->next;
		
				/* Put in the ipaddress ... we search using that */
				last->ipaddr = ipaddr;
	            
				/* Allocate memory for the hostent */
	
				last->cachedhost = malloc(sizeof(struct hostent));
	
				/* Copy it */
	
				last->cachedhost->h_name = malloc(strlen(resolvedhost->h_name));
				memcpy (last->cachedhost->h_name, resolvedhost->h_name, strlen(resolvedhost->h_name));
				if(*resolvedhost->h_aliases)
				{
					last->cachedhost->h_aliases = malloc(strlen(*resolvedhost->h_aliases));
					memcpy (&last->cachedhost->h_aliases,&resolvedhost->h_aliases, strlen(*resolvedhost->h_aliases));
				}
				last->cachedhost->h_addrtype = resolvedhost->h_addrtype;
				last->cachedhost->h_length = resolvedhost->h_length;
				last->cachedhost->h_addr_list = malloc(resolvedhost->h_length);
				memcpy (&last->cachedhost->h_addr_list, &resolvedhost->h_addr_list, resolvedhost->h_length);
	
				(ac)->hostp= last->cachedhost;
				
#ifdef SHOWCONNECTINFO
				write((ac)->fd, REPORT_FIN_DNS, R_fin_dns);
#endif
				ClearDNS(ac);
				if (!DoingAuth(ac))
					SetAccess(ac);

				LeaveCriticalSection(&critsec);

				_endthread();
	
		       return;
	        }

			LeaveCriticalSection(&critsec);
			_endthread();

		}

			/*Shit ... we have a problem if we reach here !!!*/
}
#endif /*_WIN32*/
