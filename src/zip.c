/*
 *   IRC - Internet Relay Chat, ircd/s_zip.c
 *   Copyright (C) 1996  Christophe Kalt
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
 *   $Id$
 */
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "version.h"

#include <string.h>
#include <stdlib.h>

#ifdef  ZIP_LINKS
/*
** Important note:
**      The provided buffers for uncompression and compression *MUST* be big
**      enough for any operation to complete.
**
**      s_bsd.c current settings are that the biggest packet size is 16k
**      (but socket buffers are set to 8k..)
*/

/*
** size of the buffer holding compressed data
**
** outgoing data:
**      must be enough to hold compressed data resulting of the compression
**      of up to ZIP_MAXIMUM bytes
** incoming data:
**      must be enough to hold cptr->zip->inbuf + what was just read
**      (cptr->zip->inbuf should never hold more than ONE compression block.
**      The biggest block allowed for compression is ZIP_MAXIMUM bytes)
*/
#define ZIP_BUFFER_SIZE         (ZIP_MAXIMUM + READBUF_SIZE)

/*
** size of the buffer where zlib puts compressed data
**      must be enough to hold uncompressed data resulting of the
**      uncompression of zibuffer
**
**      I'm assuming that at best, ratio will be 25%. (tests show that
**      best ratio is around 40%).
*/

/*
 * On an hybrid test net, we kept filling up unzipbuf
 * original was 4*ZIP_BUFFER_SIZE
 *
 * -Dianora
 */

#define UNZIP_BUFFER_SIZE       6 * ZIP_BUFFER_SIZE

/* buffers */
static  char    unzipbuf[UNZIP_BUFFER_SIZE];
static  char    zipbuf[ZIP_BUFFER_SIZE];

/*
** zip_init
**      Initialize compression structures for a server.
**      If failed, zip_free() has to be called.
*/
int     zip_init(aClient *cptr)
{
  cptr->zip  = (aZdata *) MyMalloc(sizeof(aZdata));
  cptr->zip->incount = 0;
  cptr->zip->outcount = 0;

  cptr->zip->in  = (z_stream *) MyMalloc(sizeof(z_stream));
  bzero(cptr->zip->in, sizeof(z_stream)); /* Just to be sure -- Syzop */
  cptr->zip->in->total_in = 0;
  cptr->zip->in->total_out = 0;
  cptr->zip->in->zalloc = NULL;
  cptr->zip->in->zfree = NULL;
  cptr->zip->in->data_type = Z_ASCII;
  if (inflateInit(cptr->zip->in) != Z_OK)
    {
      cptr->zip->out = NULL;
      return -1;
    }

  cptr->zip->out = (z_stream *) MyMalloc(sizeof(z_stream));
  bzero(cptr->zip->out, sizeof(z_stream)); /* Just to be sure -- Syzop */
  cptr->zip->out->total_in = 0;
  cptr->zip->out->total_out = 0;
  cptr->zip->out->zalloc = NULL;
  cptr->zip->out->zfree = NULL;
  cptr->zip->out->data_type = Z_ASCII;
  if (deflateInit(cptr->zip->out, ZIP_LEVEL) != Z_OK)
    return -1;

  return 0;
}

/*
** zip_free
*/
void    zip_free(aClient *cptr)
{
  ircd_log(LOG_ERROR, "Hi! This is a debug message generated from zip_free");
  if (cptr->zip)
    {
      if (cptr->zip->in) {
        inflateEnd(cptr->zip->in);
        MyFree(cptr->zip->in);
        cptr->zip->in = NULL;
      }
      if (cptr->zip->out) {
        deflateEnd(cptr->zip->out);
        MyFree(cptr->zip->out);
        cptr->zip->out = NULL;
      }
      MyFree(cptr->zip);
      cptr->zip = NULL;
    }
  ircd_log(LOG_ERROR, "Hi! This is a debug message generated from zip_free, we are done!");
}

/*
** unzip_packet
**      Unzip the buffer,
**      put anything left in cptr->zip->inbuf, update cptr->zip->incount
**
**      will return the uncompressed buffer, length will be updated.
**      if a fatal error occurs, length will be set to -1
*/
char *unzip_packet(aClient *cptr, char *buffer, int *length)
{
  z_stream *zin = cptr->zip->in;
  int   r;
  char  *p;

  if(cptr->zip->incount)
    {
      /* There was a "chunk" of uncompressed data without a newline
       * left over from last unzip_packet. So pick that up, and unzip
       * some more data. Note, buffer parameter isn't used in this case.
       * -Dianora
       */
      memcpy((void *)unzipbuf,(void *)cptr->zip->inbuf,cptr->zip->incount);
      zin->avail_out = UNZIP_BUFFER_SIZE - cptr->zip->incount;
      zin->next_out = (Bytef *) (unzipbuf + cptr->zip->incount);
      cptr->zip->incount = 0;
      cptr->zip->inbuf[0] = '\0'; /* again unnecessary but nice for debugger */
    }
  else
    {
      /* Start unzipping this buffer, if I fill up output buffer,
       * then snag whatever uncompressed incomplete chunk I have at
       * the top of the uncompressed buffer, save it for next pass.
       * -Dianora
       */
      if(!buffer)       /* Sanity test never hurts */
        {
          *length = -1;
          return((char *)NULL);
        }
      zin->next_in = (Bytef *) buffer;
      zin->avail_in = *length;
      zin->next_out = (Bytef *) unzipbuf;
      zin->avail_out = UNZIP_BUFFER_SIZE;
    }

  switch (r = inflate(zin, Z_NO_FLUSH))
    {
    case Z_OK:
      if (zin->avail_in)
        {
          cptr->zip->incount = 0;

          if(zin->avail_out == 0)
            {
              /* ok, filled up output buffer, complain about it, but go on.
               * I need to find any incomplete "chunk" at the top of
               * the uncompressed output buffer, and save it for next call.
               * N.B. That cptr->zip->inbuf isn't really necessary
               * i.e. re-entrancy is not required since I know
               * there is more uncompressed data to do, and dopacket()
               * will not return until its all parsed. -db
               */
              sendto_realops("Overflowed unzipbuf increase UNZIP_BUFFER_SIZE");
              /*
               * I used to just give up here....
               * length = -1;
               * return((char *)NULL);
               */

              /* Check for pathological case where output
               * just happened to have finished with a newline
               * and there is still input to do
               * Just stuff a newline in for now, it will be discarded
               * anyway, and continue parsing. -db
               */

              if((zin->next_out[0] == '\n') || (zin->next_out[0] == '\r'))
                {
                  cptr->zip->inbuf[0] = '\n';
                  cptr->zip->incount = 1;
                }
              else
                {
                  /* Scan from the top of output, look for a complete
                   * "chunk" N.B. there should be a check for p hitting
                   * the bottom of the unzip buffer. -db
                   */

                  for(p = (char *) zin->next_out;p >= unzipbuf;)
                    {
                      if((*p == '\r') || (*p == '\n'))
                        break;
                      zin->avail_out++;
                      p--;
                      cptr->zip->incount++;
                    }
                  /* A little sanity test never hurts -db */
                  if(p == unzipbuf)
                    {
                      cptr->zip->incount = 0;
                      cptr->zip->inbuf[0] = '\0';       /* only for debugger */
                      *length = -1;
                      return((char *)NULL);
                    }
                  /* Ok, stuff this "chunk" into inbuf
                   * for next call -Dianora 
                   */
                  p++;
                  cptr->zip->incount--;
                  memcpy((void *)cptr->zip->inbuf,
                         (void *)p,cptr->zip->incount);
                }
            }
          else
            {
              /* outbuf buffer is not full, but still
               * input to do. I suppose its just bad data.
               * However I don't have much other choice here but to
               * give up in complete disgust -db
               */
              *length = -1;
              return((char *)NULL);
            }
        }

      *length = UNZIP_BUFFER_SIZE - zin->avail_out;
      return unzipbuf;

    case Z_BUF_ERROR:
      if (zin->avail_out == 0)
        {
          sendto_realops("inflate() returned Z_BUF_ERROR: %s",
                      (zin->msg) ? zin->msg : "?");
          *length = -1;
        }
      break;

    case Z_DATA_ERROR: /* the buffer might not be compressed.. */
      if (!strncmp("ERROR ", buffer, 6))
        {
          cptr->zip->first = 0;
          ClearZipped(cptr);
          /*
           * This is not sane at all.  But if other server
           * has sent an error now, it is probably closing
           * the link as well.
           */
          return buffer;
        }
        /* Let's be nice and give them a hint ;) -- Syzop */
        sendto_realops("inflate() error: * Are you perhaps linking zipped with non-zipped? *");
        sendto_realops("Hint: link::options::zip should be the same at both sides (either both disabled or both enabled)");
        /* no break */

    default: /* error ! */
      /* should probably mark link as dead or something... */
      sendto_realops("inflate() error(%d): %s", r,
                  (zin->msg) ? zin->msg : "?");
      *length = -1; /* report error condition */
      break;
    }
  return((char *)NULL);
}

/*
** zip_buffer
**      Zip the content of cptr->zip->outbuf and of the buffer,
**      put anything left in cptr->zip->outbuf, update cptr->zip->outcount
**
**      if flush is set, then all available data will be compressed,
**      otherwise, compression only occurs if there's enough to compress,
**      or if we are reaching the maximum allowed size during a connect burst.
**
**      will return the uncompressed buffer, length will be updated.
**      if a fatal error occurs, length will be set to -1
*/
char *zip_buffer(aClient *cptr, char *buffer, int *length, int flush)
{
  z_stream *zout = cptr->zip->out;
  int   r;

  if (buffer)
    {
      /* concatenate buffer in cptr->zip->outbuf */
      memcpy((void *)(cptr->zip->outbuf + cptr->zip->outcount), (void *)buffer,
             *length );
      cptr->zip->outcount += *length;
    }
  *length = 0;

#if 0
  if (!flush && ((cptr->zip->outcount < ZIP_MINIMUM) ||
                 ((cptr->zip->outcount < (ZIP_MAXIMUM - BUFSIZE)) &&
                  CBurst(cptr))))
	/* Implement this? more efficient? or not? -- Syzop */
#else
  if (!flush && (cptr->zip->outcount < ZIP_MINIMUM))
#endif
    return((char *)NULL);

  zout->next_in = (Bytef *) cptr->zip->outbuf;
  zout->avail_in = cptr->zip->outcount;
  zout->next_out = (Bytef *) zipbuf;
  zout->avail_out = ZIP_BUFFER_SIZE;

  switch (r = deflate(zout, Z_PARTIAL_FLUSH))
    {
    case Z_OK:
      if (zout->avail_in)
        {
          /* can this occur?? I hope not... */
          sendto_realops("deflate() didn't process all available data!");
        }
      cptr->zip->outcount = 0;
      *length = ZIP_BUFFER_SIZE - zout->avail_out;
      return zipbuf;

    default: /* error ! */
      sendto_realops("deflate() error(%d): %s", r, (zout->msg) ? zout->msg : "?");
      *length = -1;
      break;
    }
  return((char *)NULL);
}

#endif  /* ZIP_LINKS */
