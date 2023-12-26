/**  
 * IRC - Internet Relay Chat, src/scache.c
 * License: GPLv1
 */

/** @file
 * @brief String cache - only used for server names.
 */

#include "unrealircd.h"

/*
 * ircd used to store full servernames in User as well as in the
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

typedef struct SCACHE SCACHE;
struct SCACHE {
	char name[HOSTLEN + 1];
	SCACHE *next;
};

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

/** Add a string to the string cache.
 * this takes a server name, and returns a pointer to the same string
 * (up to case) in the server name token list, adding it to the list if
 * it's not there.  care must be taken not to call this with
 * user-supplied arguments that haven't been verified to be a valid,
 * existing, servername.  use the hash in list.c for those.  -orabidoo
 * @param name	A valid server name
 * @returns Pointer to the server name
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
		newptr = scache_hash[hash_index] = safe_alloc(sizeof(SCACHE));
		strlcpy(newptr->name, name, sizeof(newptr->name));
		newptr->next = ptr;
		return (newptr->name);
	}
	else
	{
		ptr = scache_hash[hash_index] = safe_alloc(sizeof(SCACHE));
		strlcpy(ptr->name, name, sizeof(newptr->name));
		ptr->next = (SCACHE *) NULL;
		return (ptr->name);
	}
}
