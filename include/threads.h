/*
 *   IRC - Internet Relay Chat, include/threads.h
 *   (C) 2001 The UnrealIRCd Team - coders@lists.unrealircd.org
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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

#ifdef HAVE_NO_THREADING
#error "You need threading for this to work"
#endif 

#ifndef _INCLUDE_THREADS_H
#define _INCLUDE_THREADS_H
/* Allow it to work on Windows and linux easily -- codemastr */
#ifndef THREAD_DEBUGGING
#define TDebug(x) 
#else
#define TDebug(x) ircd_log(LOG_ERROR, "%s:%i: %s", __FILE__, __LINE__, #x)
#endif
#if !defined(_WIN32) || defined(USE_PTHREADS)
#include <pthread.h>
typedef pthread_attr_t THREAD_ATTR;
typedef pthread_t THREAD;
typedef pthread_mutex_t MUTEX;
#define IRCCreateThread(thread, attr, start, arg) TDebug(CreateThread); pthread_attr_init(&attr); pthread_create(&thread, &attr, (void*)start, arg)
#define IRCMutexLock(mutex) TDebug(MutexLock); pthread_mutex_lock(&mutex)
#define IRCMutexTryLock(mutex) TDebug(MutexTryLock); pthread_mutex_trylock(&mutex);
#define IRCMutexUnlock(mutex) TDebug(MutexUnlcok); pthread_mutex_unlock(&mutex)
#define IRCCreateMutex(mutex) TDebug(CreateMutex); mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define IRCMutexDestroy(mutex) TDebug(MutexDestroy); pthread_mutex_destroy(&mutex)
#define IRCJoinThread(thread,return) TDebug(JoinThread); pthread_join(thread, return)
#define IRCExitThread(value) TDebug(ExitThread); pthread_exit(value)
#define IRCTerminateThread(thread, value) pthread_cancel(&thread)
#define IRCThreadSelf() pthread_self()
#define IRCThreadEqual(thread1, thread2) pthread_equal(thread1,thread2)
#else
typedef short THREAD_ATTR; /* Not needed but makes porting easier */
typedef unsigned long THREAD;
typedef HANDLE MUTEX;
#define IRCCreateThread(thread, attr, start, arg) thread = _beginthread((void *)start, 0, arg)
#define IRCMutexLock(mutex) WaitForSingleObject(mutex, INFINITE)
#define IRCMutexTryLock(mutex) WaitForSingleObject(mutex, 0)
#define IRCMutexUnlock(mutex) ReleaseMutex(mutex)
#define IRCCreateMutex(mutex) mutex = CreateMutex(NULL, FALSE, NULL)
#define IRCMutexDestroy(mutex) CloseHandle(mutex)
#define IRCJoinThread(thread,return) WaitForSingleObject((HANDLE)thread, INFINITE); GetExitCodeThread((HANDLE)thread, (DWORD)return);
#define IRCExitThread(value) _endthread()
#define IRCTerminateThread(thread, value) TerminateThread((HANDLE)thread, value)
#define IRCThreadSelf() GetCurrentThread()
#define IRCThreadEqual(thread1, thread2) thread1 == thread2 ? 1 : 0
#endif

#endif

