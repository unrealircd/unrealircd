#************************************************************************
#*   IRC - Internet Relay Chat, src/Makefile
#*   Copyright (C) 1990 Jarkko Oikarinen
#*
#*   This program is free software; you can redistribute it and/or modify
#*   it under the terms of the GNU General Public License as published by
#*   the Free Software Foundation; either version 1, or (at your option)
#*   any later version.
#*
#*   This program is distributed in the hope that it will be useful,
#*   but WITHOUT ANY WARRANTY; without even the implied warranty of
#*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#*   GNU General Public License for more details.
#*
#*   You should have received a copy of the GNU General Public License
#*   along with this program; if not, write to the Free Software
#*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#*   
#*   $Id$
#*/

CC=danger will robinson

LINTFLAGS=-hba

OBJS=bsd.o dbuf.o packet.o send.o match.o parse.o support.o channel.o \
	class.o hash.o ircd.o list.o res.o cloak.o s_auth.o s_bsd.o s_conf.o \
	s_debug.o s_err.o s_extra.o s_misc.o s_numeric.o s_serv.o s_user.o s_unreal.o \
	whowas.o userload.o crule.o help.o md5.o version.o dynconf.o \
        s_socks.o s_kline.o fdlist.o \
	$(RES) $(STRTOUL)

SRC=$(OBJS:%.o=%.c)

MAKE = make 'CFLAGS=${CFLAGS}' 'CC=${CC}' 'IRCDLIBS=${IRCDLIBS}' \
	    'LDFLAGS=${LDFLAGS}' 'IRCDMODE=${IRCDMODE}'

INCLUDES = ../include/struct.h ../include/config.h ../include/settings.h \
	../include/sys.h ../include/common.h ../include/version.h \
	../include/h.h ../include/numeric.h ../include/msg.h \
	../include/dynconf.h

all: build

build: ircd chkconf

version.c:
	$(SHELL) version.c.SH

version.o: version.c ../include/version.h
	$(CC) $(CFLAGS) -c version.c

ircd: $(OBJS) 
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(IRCDLIBS) -o ircd
	chmod $(IRCDMODE) ircd

chkconf: ../include/struct.h ../include/config.h ../include/settings.h ../include/sys.h \
	 ../include/common.h crule.c chkconf.c
	$(CC) $(CFLAGS) -DCR_CHKCONF -o chkcrule.o -c crule.c
	$(CC) $(CFLAGS) -DCR_CHKCONF -o chkmatch.o -c match.c
	$(CC) $(CFLAGS) chkconf.c chkmatch.o chkcrule.o \
	$(LDFLAGS) $(IRCDLIBS) -o chkconf

saber: $(SRC)
	#load -I../include $(SRC) version.c $(IRCDLIBS)

lint:
	lint $(LINTFLAGS) -I../include $(SRC) | egrep -v 'sendto_|debug'

parse.o: parse.c $(INCLUDES)
bsd.o: bsd.c $(INCLUDES)
dbuf.o: dbuf.c $(INCLUDES) ../include/dbuf.h
packet.o: packet.c $(INCLUDES)
send.o: send.c $(INCLUDES)
match.o: match.c $(INCLUDES)
support.o: support.c $(INCLUDES)

#install: all
#	-if [ ! -d ${IRCDDIR} -a ! -f ${IRCDDIR} ] ; then \
#		mkdir ${IRCDDIR}; \
#	fi
#	../bsdinstall -m ${IRCDMODE} ircd ${BINDIR}
#	../bsdinstall -m 700 chkconf ${BINDIR}
#	$(CP) ../doc/example.conf ${IRCDDIR}
#	$(TOUCH) ${IRCDDIR}/ircd.motd
#	$(RM) -f ${IRCDDIR}/ircd.m4
#	$(TOUCH) ${IRCDDIR}/ircd.m4
#	chmod +x buildm4
#	./buildm4 ${IRCDDIR}

clean:
	$(RM) -f *.o *~ core ircd version.c chkconf

cleandir: clean

depend:
	makedepend -I${INCLUDEDIR} ${SRC}

channel.o: channel.c $(INCLUDES) ../include/channel.h
	$(CC) $(CFLAGS) -c channel.c

class.o: class.c $(INCLUDES) ../include/class.h
	$(CC) $(CFLAGS) -c class.c

ircd.o: ircd.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c ircd.c

list.o: list.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c list.c

res.o: res.c ../include/res.h $(INCLUDES)
	$(CC) $(CFLAGS) -c res.c

cloak.o: cloak.c ../include/res.h $(INCLUDES)
	$(CC) $(CFLAGS) -c cloak.c

fdlist.o: fdlist.c ../include/fdlist.h $(INCLUDES)
	$(CC) $(CFLAGS) -c fdlist.c

s_bsd.o: s_bsd.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_bsd.c

s_auth.o: s_auth.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_auth.c

s_socks.o: s_socks.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_socks.c

s_conf.o: s_conf.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_conf.c

s_debug.o: ../include/sys.h s_debug.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_debug.c

s_err.o: ../include/msg.h s_err.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_err.c

s_misc.o: s_misc.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_misc.c

s_user.o: s_user.c $(INCLUDES) \
	  ../include/dbuf.h ../include/channel.h ../include/whowas.h
	$(CC) $(CFLAGS) -c s_user.c

s_extra.o: s_extra.c $(INCLUDES) \
	  s_numeric.c ../include/dbuf.h ../include/channel.h ../include/whowas.h
	$(CC) $(CFLAGS) -c s_extra.c

s_kline.o: s_kline.c $(INCLUDES) \
	  s_numeric.c ../include/dbuf.h ../include/channel.h ../include/whowas.h
	$(CC) $(CFLAGS) -c s_kline.c

s_unreal.o: s_unreal.c $(INCLUDES) \
	  s_numeric.c ../include/dbuf.h ../include/channel.h ../include/whowas.h
	$(CC) $(CFLAGS) -c s_unreal.c

s_serv.o: s_serv.c $(INCLUDES) \
	  ../include/dbuf.h ../include/whowas.h
	$(CC) $(CFLAGS) -c s_serv.c

s_numeric.o: s_numeric.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_numeric.c

whowas.o: whowas.c ../include/dbuf.h ../include/whowas.h $(INCLUDES)
	$(CC) $(CFLAGS) -c whowas.c

hash.o: hash.c ../include/hash.h $(INCLUDES)
	$(CC) $(CFLAGS) -c hash.c

crule.o: crule.c $(INCLUDES)
	$(CC) $(CFLAGS) -c crule.c

dynconf.o: dynconf.c $(INCLUDES) \
	  s_numeric.c ../include/dbuf.h ../include/channel.h ../include/whowas.h ../include/dynconf.h
	$(CC) $(CFLAGS) -c dynconf.c

# DO NOT DELETE THIS LINE -- make depend depends on it.
