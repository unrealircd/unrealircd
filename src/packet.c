
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

#ifdef CRYPTOIRCD
char *ep_encrypt(aClient *cptr, char *string, int *len)
{
	static unsigned char cryptobuffer[8192];
	char ivec[9];
	int  length;
	char *c;
	int  num;

	if (!cptr->cryptinfo)
		return string;

	bzero(cryptobuffer, sizeof(cryptobuffer));
	bzero(ivec, sizeof(ivec));
	num = 0;

	if ((c = (char *)strchr(string, '\n')))
		*c = '\0';
	if ((c = (char *)strchr(string, '\r')))
		*c = '\0';

	length = strlen(string) + 1;
	cryptobuffer[0] = (unsigned char)length / 256;
	cryptobuffer[1] = (unsigned char)length - (cryptobuffer[0] * 256);

	if (cptr->cryptinfo->method == METHOD_BLOWFISH)
	{
		BF_cfb64_encrypt(string, &cryptobuffer[2], length,
		    cptr->cryptinfo->key, ivec, &num, BF_ENCRYPT);
		*len = length + 2;
		return (cryptobuffer);
	}
}

char *ep_decrypt(aClient *cptr, char *string)
{
	static char decryptbuffer[8192];
	int  num;
	char ivec[9];
	int  length;

	if (!cptr->cryptinfo)
		return string;

	bzero(decryptbuffer, sizeof(decryptbuffer));
	bzero(ivec, sizeof(ivec));
	num = 0;
	length = (*(string) * 256) + (*(string + 1));

	if (cptr->cryptinfo->method == METHOD_BLOWFISH)
	{
		BF_cfb64_encrypt(string + 2, decryptbuffer, length,
		    cptr->cryptinfo->key, ivec, &num, BF_DECRYPT);
		return (decryptbuffer);
	}
}
#endif
/*
 * inittoken
 * Cheat here, blah. Build the lookup tables from msgtab's,
 * call them msgmap's. Called in main() with other inits.
 * Yes, I know this is not the right module, but I said I cheat ;)
 */
void inittoken(void)
{
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
		msgmap[msgtab[loopy].token[0]] = &msgtab[loopy];
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
	aClient *acpt = cptr->acpt;
#ifdef CRYPTOIRCD
	int  lengthweneed, num, lengthbackup, li;
	char *s;
	char f[4096], x[512];
#endif

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
#ifdef CRYPTOIRCD
	if (IsSecure(cptr))
	{
		while (--length >= 0)
		{
			*ch1 = *ch2++;
			if (ch1 - cptr->buffer > 2)
			{
				lengthweneed =
				    ((unsigned char)*cptr->buffer) * 256 +
				    ((unsigned char)*(cptr->buffer + 1));
				lengthbackup = ch1 - cptr->buffer;
				if (lengthbackup >= (lengthweneed + 1))
				{
					ch1 = cptr->buffer;
					cptr->count = 0;
					Debug((DEBUG_ERROR,
					    "packet recieved len %i",
					    lengthweneed));
					s =
					    (char *)ep_decrypt(cptr,
					    cptr->buffer);
					me.receiveM += 1;
					cptr->receiveM += 1;
					if (cptr->acpt != &me)
						cptr->acpt->receiveM += 1;
					if (parse(cptr, s, s + lengthweneed,
					    msgtab) == FLUSH_BUFFER)
						return FLUSH_BUFFER;
					if (cptr->flags & FLAGS_DEADSOCKET)
						return exit_client(cptr, cptr,
						    &me, "Dead socket");
				}
				else
				    if (ch1 <
				    cptr->buffer + (sizeof(cptr->buffer) - 1))
					ch1++;
			}
			else
			    if (ch1 < cptr->buffer + (sizeof(cptr->buffer) - 1))
				ch1++;

		}

	}
	else
#endif
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
				if (cptr->acpt != &me)
					cptr->acpt->receiveM += 1;
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
