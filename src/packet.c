
/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/packet.c
 *   Copyright (C) 1990  Jarkko Oikarinen and
 *                       University of Oulu, Computing Center
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
#include "msg.h"
#include "h.h"

ID_Copyright
    ("(C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
ID_Notes("2.12 1/30/94");

aCommand	*CommandHash[256]; 


/*
 * inittoken
 * Cheat here, blah. Build the lookup tables from msgtab's,
 * call them msgmap's. Called in main() with other inits.
 * Yes, I know this is not the right module, but I said I cheat ;)
 */
void inittoken(void)
{
	aCommand *p;
	int  loopy;
	int  final;

	/* Find the zero-entry */
	for (final = 0; msgtab[final].cmd; final++)
		;
	/* Point all entries to it */
	for (loopy = 0; loopy < 256; loopy++)
		msgmap[loopy] = &msgtab[final];
	/* Build references to existing commands */
	for (loopy = 0; msgtab[loopy].cmd; loopy++)
	{
		msgmap[msgtab[loopy].token[0]] = &msgtab[loopy];
		add_Command(msgtab[loopy].cmd, msgtab[loopy].token,
		       msgtab[loopy].func, msgtab[loopy].parameters);
	}
}
/*
** dopacket
**	cptr - pointer to client structure for which the buffer data
**	       applies.
**	buffer - pointr to the buffer containing the newly read data
**	length - number of valid bytes of data in the buffer
**
** Note:
**	It is implicitly assumed that dopacket is called only
**	with cptr of "local" variation, which contains all the
**	necessary fields (buffer etc..)
*/
int  dopacket(cptr, buffer, length)
	aClient *cptr;
	char *buffer;
	int  length;
{
	char *ch1;
	char *ch2;
	aClient *acpt = cptr->listener;

	me.receiveB += length;	/* Update bytes received */
	cptr->receiveB += length;
	if (cptr->receiveB > 1023)
	{
		cptr->receiveK += (cptr->receiveB >> 10);
		cptr->receiveB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
	}
	if (acpt != &me)
	{
		acpt->receiveB += length;
		if (acpt->receiveB > 1023)
		{
			acpt->receiveK += (acpt->receiveB >> 10);
			acpt->receiveB &= 0x03ff;
		}
	}
	else if (me.receiveB > 1023)
	{
		me.receiveK += (me.receiveB >> 10);
		me.receiveB &= 0x03ff;
	}
	ch1 = cptr->buffer + cptr->count;
	ch2 = buffer;
		while (--length >= 0)
		{
			char g = (*ch1 = *ch2++);
			/*
			 * Yuck.  Stuck.  To make sure we stay backward compatible,
			 * we must assume that either CR or LF terminates the message
			 * and not CR-LF.  By allowing CR or LF (alone) into the body
			 * of messages, backward compatibility is lost and major
			 * problems will arise. - Avalon
			 */
			if (g < '\16' && (g == '\n' || g == '\r'))
			{
				if (ch1 == cptr->buffer)
					continue;	/* Skip extra LF/CR's */
				*ch1 = '\0';
				me.receiveM += 1;	/* Update messages received */
				cptr->receiveM += 1;
				if (cptr->listener != &me)
					cptr->listener->receiveM += 1;
				cptr->count = 0;	/* ...just in case parse returns with
							   ** FLUSH_BUFFER without removing the
							   ** structure pointed by cptr... --msa
							 */
				if (parse(cptr, cptr->buffer, ch1, msgtab) ==
				    FLUSH_BUFFER)
					/*
					   ** FLUSH_BUFFER means actually that cptr
					   ** structure *does* not exist anymore!!! --msa
					 */
					return FLUSH_BUFFER;
				/*
				 ** Socket is dead so exit (which always returns with
				 ** FLUSH_BUFFER here).  - avalon
				 */
				if (cptr->flags & FLAGS_DEADSOCKET)
					return exit_client(cptr, cptr, &me,
					    "Dead Socket");
				ch1 = cptr->buffer;
			}
			else if (ch1 <
			    cptr->buffer + (sizeof(cptr->buffer) - 1))
				ch1++;	/* There is always room for the null */
		}
	cptr->count = ch1 - cptr->buffer;
	return 0;
}

void	init_CommandHash(void)
{
	bzero(CommandHash, sizeof(CommandHash));
}

void	add_Command_backend(char *cmd, int (*func)(), unsigned char parameters, unsigned char token)
{
	aCommand	*newcmd = (aCommand *) MyMalloc(sizeof(aCommand));
	
	bzero(newcmd, sizeof(aCommand));
	
	newcmd->cmd = (char *) strdup(cmd);
	newcmd->parameters = parameters;
	newcmd->token = token;
	newcmd->func = func;
	
	/* Add in hash with hash value = first byte */
	add_Command_to_list(newcmd, &CommandHash[toupper(*cmd)]);
}

void	add_Command(char *cmd, char *token, int (*func)(), unsigned char parameters)
{
	add_Command_backend(cmd, func, parameters, 0);
	add_Command_backend(token, func, parameters, 1);
}

void	add_Command_to_list(aCommand *item, aCommand **list)
{
	item->next = *list;
	item->prev = NULL;
	if (*list)
		(*list)->prev = item;
	*list = item;
}

aCommand *del_Command_from_list(aCommand *item, aCommand **list)
{
	aCommand *p, *q;
	
	for (p = *list; p; p = p->next)
	{
		if (p == item)
		{
			q = p->next;
			if (p->prev)
				p->prev->next = p->next;
			else
				*list = p->next;
				
			if (p->next)
				p->next->prev = p->prev;
			return q;		
		}
	}
	return NULL;
}


