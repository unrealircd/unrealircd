
/* $Id$ */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "h.h"
#include "proto.h"
#include <string.h>

static int hash(char *);	/*

				 * keep it hidden here 
				 */
/*
 * ircd used to store full servernames in anUser as well as in the
 * whowas info.  there can be some 40k such structures alive at any
 * given time, while the number of unique server names a server sees in
 * its lifetime is at most a few hundred.  by tokenizing server names
 * internally, the server can easily save 2 or 3 megs of RAM.
 * -orabidoo
 */
/*
 * I could have tucked this code into hash.c I suppose but lets keep it
 * separate for now -Dianora
 */

#define SCACHE_HASH_SIZE 257

typedef struct scache_entry {
	char name[HOSTLEN + 1];
	struct scache_entry *next;
} SCACHE;

static SCACHE *scache_hash[SCACHE_HASH_SIZE];

/*
 * renamed to keep it consistent with the other hash functions -Dianora 
 */
/*
 * orabidoo had named it init_scache_hash(); 
 */

void clear_scache_hash_table(void)
{
	memset((char *)scache_hash, '\0', sizeof(scache_hash));
}

static int hash(char *string)
{
	int  hash_value;

	hash_value = 0;
	while (*string)
		hash_value += (*string++ & 0xDF);

	return hash_value % SCACHE_HASH_SIZE;
}
/*
 * this takes a server name, and returns a pointer to the same string
 * (up to case) in the server name token list, adding it to the list if
 * it's not there.  care must be taken not to call this with
 * user-supplied arguments that haven't been verified to be a valid,
 * existing, servername.  use the hash in list.c for those.  -orabidoo
 */

char *find_or_add(char *name)
{
	int  hash_index;
	SCACHE *ptr, *newptr;

	ptr = scache_hash[hash_index = hash(name)];
	while (ptr)
	{
		if (!mycmp(ptr->name, name))
		{
			return (ptr->name);
		}
		else
		{
			ptr = ptr->next;
		}
	}

	/*
	 * not found -- add it 
	 */
	if ((ptr = scache_hash[hash_index]))
	{
		newptr = scache_hash[hash_index] =
		    (SCACHE *) MyMalloc(sizeof(SCACHE));
		strlcpy(newptr->name, name, HOSTLEN);
		newptr->next = ptr;
		return (newptr->name);
	}
	else
	{
		ptr = scache_hash[hash_index] =
		    (SCACHE *) MyMalloc(sizeof(SCACHE));
		strlcpy(ptr->name, name, HOSTLEN);
		ptr->next = (SCACHE *) NULL;
		return (ptr->name);
	}
}

char *find_by_hash(int hash)
{
	SCACHE *ptr;

	ptr = scache_hash[hash];
	if (ptr)
		return (ptr->name);
	else
		return NULL;
}



/*
 * Added so s_debug could check memory usage in here -Dianora 
 */

void count_scache(int *number_servers_cached, u_long *mem_servers_cached)
{
	SCACHE *scache_ptr;
	int  i;

	*number_servers_cached = 0;
	*mem_servers_cached = 0;

	for (i = 0; i < SCACHE_HASH_SIZE; i++)
	{
		scache_ptr = scache_hash[i];
		while (scache_ptr)
		{
			*number_servers_cached = *number_servers_cached + 1;
			*mem_servers_cached = *mem_servers_cached +
			    (strlen(scache_ptr->name) + sizeof(SCACHE *));

			scache_ptr = scache_ptr->next;
		}
	}
}
/*
 * list all server names in scache very verbose 
 */

void list_scache(aClient *sptr)
{
	int  hash_index;
	SCACHE *ptr;

	for (hash_index = 0; hash_index < SCACHE_HASH_SIZE; hash_index++)
	{
		ptr = scache_hash[hash_index];
		while (ptr)
		{
			if (ptr->name)
				sendto_one(sptr,
				    ":%s NOTICE %s :server=%s hash=%i",
				    me.name, sptr->name, ptr->name, hash_index);
			ptr = ptr->next;
		}
	}

}
