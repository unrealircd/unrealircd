/*
 * fdlist.c   maintain lists of certain important fds 
 */

/* $Id$ */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "config.h"
#include "fdlist.h"

void
addto_fdlist(int fd, fdlist * listp)
{
   int index;

   if ((index = ++listp->last_entry) >= MAXCONNECTIONS) {
      /*
       * list too big.. must exit 
       */
      --listp->last_entry;

#ifdef	USE_SYSLOG
      (void) syslog(LOG_CRIT, "fdlist.c list too big.. must exit");
#endif
      abort();
   }
   else
      listp->entry[index] = fd;
   return;
}

void
delfrom_fdlist(int fd, fdlist * listp)
{
   int i;

   for (i = listp->last_entry; i; i--) {
      if (listp->entry[i] == fd)
	 break;
   }
   if (!i)
      return;			/*
				 * could not find it! 
				 */
   /*
    * swap with last_entry 
    */
   if (i == listp->last_entry) {
      listp->entry[i] = 0;
      listp->last_entry--;
      return;
   }
   else {
      listp->entry[i] = listp->entry[listp->last_entry];
      listp->entry[listp->last_entry] = 0;
      listp->last_entry--;
      return;
   }
}

void
init_fdlist(fdlist * listp)
{
   listp->last_entry = 0;
   memset((char *) listp->entry, '\0', sizeof(listp->entry));
   return;
}
