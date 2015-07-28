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

CC = "==== DO NOT RUN MAKE FROM THIS DIRECTORY ===="

OBJS=timesynch.o res.o s_bsd.o auth.o aln.o channel.o cloak.o crule.o dbuf.o \
	events.o fdlist.o hash.o ircd.o ircsprintf.o list.o \
	match.o modules.o packet.o parse.o s_auth.o mempool.o operclass.o \
	s_conf.o s_debug.o s_dispatch.o s_err.o s_extra.o s_kline.o \
	s_misc.o s_numeric.o s_serv.o s_svs.o $(STRTOUL) socket.o \
	ssl.o s_user.o charsys.o scache.o send.o support.o umodes.o \
	version.o whowas.o random.o extcmodes.o moddata.o uid.o \
	extbans.o api-isupport.o api-command.o api-clicap.o md5.o crypt_blowfish.o \
	updconf.o crashreport.o $(URL)

SRC=$(OBJS:%.o=%.c)

MAKEARGS =	'CFLAGS=${CFLAGS}' 'CC=${CC}' 'IRCDLIBS=${IRCDLIBS}' \
		'LDFLAGS=${LDFLAGS}' 'IRCDMODE=${IRCDMODE}' \
		'BINDIR=${BINDIR}' 'INSTALL=${INSTALL}' \
		'INCLUDEDIR=${INCLUDEDIR}' \
		'MANDIR=${MANDIR}' 'RM=${RM}' 'CP=${CP}' 'TOUCH=${TOUCH}' \
		'RES=${RES}' 'SHELL=${SHELL}' 'STRTOUL=${STRTOUL}' \
		'CRYPTOLIB=${CRYPTOLIB}' \
		'CRYPTOINCLUDES=${CRYPTOINCLUDES}' 'URL=${URL}' \

MY_MAKE = $(MAKE) $(MAKEARGS)

INCLUDES = ../include/auth.h ../include/channel.h \
	../include/class.h ../include/common.h ../include/config.h ../include/dbuf.h \
	../include/dynconf.h ../include/events.h ../include/fdlist.h ../include/h.h \
	../include/hash.h ../include/inet.h ../include/ircsprintf.h \
	../include/license.h ../include/macros.h \
	../include/modules.h ../include/modversion.h ../include/msg.h \
	../include/numeric.h ../include/proto.h \
	../include/resource.h ../include/setup.h ../include/sjoin.h \
	../include/sock.h ../include/ssl.h ../include/struct.h ../include/sys.h \
	../include/threads.h ../include/types.h ../include/url.h \
	../include/version.h ../include/whowas.h

all: build

build: ircd mods

custommodule:
	+cd modules/third; $(MY_MAKE) MODULEFILE=$(MODULEFILE) 'EXLIBS=$(EXLIBS)' custommodule

ircd: $(OBJS)
	$(CC) $(CFLAGS) $(CRYPTOLIB) -o ircd $(OBJS) $(LDFLAGS) $(IRCDLIBS) $(CRYPTOLIB)

mods:
	@if [ ! -r include ] ; then \
		ln -s ../include include; \
	fi
	@if [ ! -r modules ] ; then \
		echo "You havent done cvs update -P -d"; \
	fi
	+cd modules; $(MY_MAKE) all

version.c: version.c.SH
	$(SHELL) version.c.SH

version.o: version.c $(INCLUDES)
	$(CC) $(CFLAGS) -c version.c

parse.o: parse.c $(INCLUDES)
	$(CC) $(CFLAGS) -c parse.c

socket.o: socket.c $(INCLUDES)
	$(CC) $(CFLAGS) -c socket.c

s_dispatch.o: s_dispatch.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_dispatch.c

dbuf.o: dbuf.c $(INCLUDES)
	$(CC) $(CFLAGS) -c dbuf.c

packet.o: packet.c $(INCLUDES)
	$(CC) $(CFLAGS) -c packet.c

aln.o: aln.c $(INCLUDES)
	$(CC) $(CFLAGS) -c aln.c

auth.o: auth.c $(INCLUDES)
	$(CC) $(CFLAGS) -c auth.c

send.o: send.c $(INCLUDES)
	$(CC) $(CFLAGS) -c send.c

ssl.o: ssl.c $(INCLUDES)
	$(CC) $(CFLAGS) -c ssl.c

match.o: match.c $(INCLUDES)
	$(CC) $(CFLAGS) -c match.c

modules.o: modules.c $(INCLUDES)

support.o: support.c $(INCLUDES)
	$(CC) $(CFLAGS) -c support.c

umodes.o: umodes.c $(INCLUDES)
	$(CC) $(CFLAGS) -c umodes.c

userload.o: userload.c $(INCLUDES)
	$(CC) $(CFLAGS) -c userload.c

uid.o: uid.c $(INCLUDES)
	$(CC) $(CFLAGS) -c uid.c

s_svs.o: s_svs.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_svs.c
events.o: events.c $(INCLUDES)
	$(CC) $(CFLAGS) -c events.c

clean:
	$(RM) -f *.o *.so *~ core ircd version.c; \
	cd modules; make clean

cleandir: clean

depend:
	makedepend -I${INCLUDEDIR} ${SRC}

channel.o: channel.c $(INCLUDES)
	$(CC) $(CFLAGS) -c channel.c

ircd.o: ircd.c $(INCLUDES)
	$(CC) $(CFLAGS) -c ircd.c

list.o: list.c $(INCLUDES)
	$(CC) $(CFLAGS) -c list.c

res.o: res.c $(INCLUDES) ../include/res.h
	$(CC) $(CFLAGS) -c res.c

timesynch.o: timesynch.c $(INCLUDES)
	$(CC) $(CFLAGS) -c timesynch.c

cloak.o: cloak.c $(INCLUDES)
	$(CC) $(CFLAGS) -c cloak.c

fdlist.o: fdlist.c $(INCLUDES)
	$(CC) $(CFLAGS) -c fdlist.c

s_bsd.o: s_bsd.c $(INCLUDES) ../include/res.h
	$(CC) $(CFLAGS) -c s_bsd.c

s_auth.o: s_auth.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_auth.c

s_conf.o: s_conf.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_conf.c

s_debug.o: s_debug.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_debug.c

s_err.o: s_err.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_err.c

s_misc.o: s_misc.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_misc.c

scache.o: scache.c $(INCLUDES)
	$(CC) $(CFLAGS) -c scache.c

ircsprintf.o: ircsprintf.c $(INCLUDES)
	$(CC) $(CFLAGS) -c ircsprintf.c

s_user.o: s_user.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_user.c

charsys.o: charsys.c $(INCLUDES)
	$(CC) $(CFLAGS) -c charsys.c

s_extra.o: s_extra.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_extra.c

s_kline.o: s_kline.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_kline.c

s_serv.o: s_serv.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_serv.c

s_numeric.o: s_numeric.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_numeric.c

whowas.o: whowas.c $(INCLUDES)
	$(CC) $(CFLAGS) -c whowas.c

hash.o: hash.c $(INCLUDES)
	$(CC) $(CFLAGS) -c hash.c

crule.o: crule.c $(INCLUDES)
	$(CC) $(CFLAGS) -c crule.c

random.o: random.c $(INCLUDES)
	$(CC) $(CFLAGS) -c random.c

extcmodes.o: extcmodes.c $(INCLUDES)
	$(CC) $(CFLAGS) -c extcmodes.c

operclass.o: operclass.c $(INCLUDES)
	$(CC) $(CFLAGS) -c operclass.c

moddata.o: moddata.c $(INCLUDES)
	$(CC) $(CFLAGS) -c moddata.c

extbans.o: extbans.c $(INCLUDES)
	$(CC) $(CFLAGS) -c extbans.c

md5.o: md5.c $(INCLUDES)
	$(CC) $(CFLAGS) -c md5.c

api-command.o: api-command.c $(INCLUDES)
	$(CC) $(CFLAGS) -c api-command.c

api-isupport.o: api-isupport.c $(INCLUDES)
	$(CC) $(CFLAGS) -c api-isupport.c

api-clicap.o: api-clicap.c $(INCLUDES)
	$(CC) $(CFLAGS) -c api-clicap.c

crypt_blowfish.o: crypt_blowfish.c $(INCLUDES)
	$(CC) $(CFLAGS) -c crypt_blowfish.c

updconf.o: updconf.c $(INCLUDES)
	$(CC) $(CFLAGS) -c updconf.c

crashreport.o: crashreport.c $(INCLUDES)
	$(CC) $(CFLAGS) -c crashreport.c

url.o: url.c $(INCLUDES)
	$(CC) $(CFLAGS) -c url.c

# DO NOT DELETE THIS LINE -- make depend depends on it.

