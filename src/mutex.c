#include "threads.h"

#define MUTEX_MAX_LOCK 30000

#define MUTEX_SUCCESS (0)
#define MUTEX_IN_USE (-1)
#define MUTEX_TOO_MANY_LOCKS (-2)
#define MUTEX_NOT_MINE (-3)

int my_trylock_mutex(MUTEX* mutex)
{

	THREAD me;
	me = IRCThreadSelf();
	if (mutex->lock_index && !IRCThreadEqual(me, mutex->lockedby))
		return MUTEX_IN_USE;
	if (mutex->lock_index > 30000)
		return MUTEX_TOO_MANY_LOCKS;
	mutex->lockedby = me;
	mutex->lock_index++;
	return MUTEX_SUCCESS;
}

int my_lock_mutex(MUTEX* mutex)
{
	int ret;
	ret = my_trylock_mutex(mutex);

	if (ret != MUTEX_IN_USE)
		return ret;

	while(mutex->lock_index > 0)
		;

	return my_trylock_mutex(mutex);
}

int my_unlock_mutex(MUTEX* mutex)
{
	THREAD me;
	me = IRCThreadSelf();

	if (!IRCThreadEqual(me, mutex->lockedby))
		return MUTEX_NOT_MINE;

	mutex->lock_index--;

	if (mutex->lock_index < 0)
		mutex->lock_index = 0;

	return MUTEX_SUCCESS;
}
