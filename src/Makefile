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

CC = danger will robinson

OBJS=auth.o aln.o badwords.o channel.o cloak.o crule.o dbuf.o \
	events.o fdlist.o hash.o help.o ircd.o ircsprintf.o list.o lusers.o \
	match.o modules.o packet.o parse.o $(REGEX) res.o \
	res_init.o res_comp.o res_mkquery.o res_skipname.o s_auth.o \
	s_bsd.o s_conf.o s_debug.o s_err.o s_extra.o s_kline.o \
	s_misc.o s_numeric.o s_serv.o s_svs.o $(STRTOUL) socket.o \
	ssl.o s_user.o scache.o send.o support.o umodes.o s_stats.o \
	version.o webtv.o whowas.o zip.o cidr.o random.o extcmodes.o \
	extbans.o $(URL)

SRC=$(OBJS:%.o=%.c)

MAKEARGS =	'CFLAGS=${CFLAGS}' 'CC=${CC}' 'IRCDLIBS=${IRCDLIBS}' \
		'LDFLAGS=${LDFLAGS}' 'IRCDMODE=${IRCDMODE}' \
		'BINDIR=${BINDIR}' 'INSTALL=${INSTALL}' \
		'INCLUDEDIR=${INCLUDEDIR}' 'IRCDDIR=${IRCDDIR}' \
		'MANDIR=${MANDIR}' 'RM=${RM}' 'CP=${CP}' 'TOUCH=${TOUCH}' \
		'RES=${RES}' 'SHELL=${SHELL}' 'STRTOUL=${STRTOUL}' \
		'CRYPTOLIB=${CRYPTOLIB}' 'REGEX=${REGEX}' \
		'CRYPTOINCLUDES=${CRYPTOINCLUDES}' 'URL=${URL}' \

MAKE = make $(MAKEARGS)

INCLUDES = ../include/struct.h ../include/config.h \
	../include/sys.h ../include/common.h ../include/version.h \
	../include/h.h ../include/numeric.h ../include/msg.h \
	../include/dynconf.h ../include/modules.h ../include/setup.h

all: build

build: ircd mods

custommodule:
	cd modules; $(MAKE) MODULEFILE=$(MODULEFILE) 'EXLIBS=$(EXLIBS)' custommodule

ircd: $(OBJS)
	$(CC) $(CFLAGS) $(CRYPTOLIB) -o ircd $(OBJS) $(LDFLAGS) $(IRCDLIBS) $(CRYPTOLIB)

staticircd: $(OBJS)
	$(CC) $(CFLAGS) $(CRYPTOLIB) -DSTATIC_LINKING -o ircd.static $(OBJS) \
	modules/l_commands.c \
	modules/m_sethost.c \
	modules/m_chghost.c \
	modules/m_chgident.c \
	modules/m_setname.c \
	modules/m_setident.c \
	modules/m_sdesc.c \
	$(LDFLAGS) $(IRCDLIBS) $(CRYPTOLIB)

mods:
	@if [ ! -r include ] ; then \
		ln -s ../include include; \
	fi
	@if [ ! -r modules ] ; then \
		echo "You havent done cvs update -P -d"; \
	fi
	cd modules; $(MAKE) all

version.c: version.c.SH ../Changes
	$(SHELL) version.c.SH

version.o: version.c ../include/version.h
	$(CC) $(CFLAGS) -c version.c

parse.o: parse.c $(INCLUDES)
	$(CC) $(CFLAGS) -c parse.c

socket.o: socket.c $(INCLUDES)
	$(CC) $(CFLAGS) -c socket.c



dbuf.o: dbuf.c $(INCLUDES) ../include/dbuf.h
	$(CC) $(CFLAGS) -c dbuf.c

packet.o: packet.c $(INCLUDES)
	$(CC) $(CFLAGS) -c packet.c

s_stats.o: s_stats.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_stats.c

badwords.o: badwords.c $(INCLUDES)
	$(CC) $(CFLAGS) -c badwords.c

aln.o: aln.c $(INCLUDES)
	$(CC) $(CFLAGS) -c aln.c

auth.o: auth.c $(INCLUDES)
	$(CC) $(CFLAGS) -c auth.c

zip.o: zip.c $(INCLUDES)
	$(CC) $(CFLAGS) -c zip.c

send.o: send.c $(INCLUDES)
	$(CC) $(CFLAGS) -c send.c

webtv.o: webtv.c $(INCLUDES)
	$(CC) $(CFLAGS) -c webtv.c

ssl.o: ssl.c $(INCLUDES)
	$(CC) $(CFLAGS) -c ssl.c

match.o: match.c $(INCLUDES)
	$(CC) $(CFLAGS) -c match.c

support.o: support.c $(INCLUDES)
	$(CC) $(CFLAGS) -c support.c

umodes.o: umodes.c $(INCLUDES)
	$(CC) $(CFLAGS) -c umodes.c

userload.o: userload.c $(INCLUDES)
	$(CC) $(CFLAGS) -c userload.c

s_svs.o: s_svs.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_svs.c
events.o: events.c $(INCLUDES)
	$(CC) $(CFLAGS) -c events.c

help.o: help.c $(INCLUDES)
	$(CC) $(CFLAGS) -c help.c

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
	$(RM) -f *.o *.so *~ core ircd version.c; \
	cd modules; make clean

cleandir: clean

depend:
	makedepend -I${INCLUDEDIR} ${SRC}

channel.o: channel.c $(INCLUDES) ../include/channel.h
	$(CC) $(CFLAGS) -c channel.c

ircd.o: ircd.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c ircd.c

list.o: list.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c list.c

lusers.o: lusers.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c lusers.c

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

s_conf.o: s_conf.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_conf.c

s_debug.o: ../include/sys.h s_debug.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_debug.c

s_err.o: ../include/msg.h s_err.c $(INCLUDES)
	$(CC) $(CFLAGS) -c s_err.c

s_misc.o: s_misc.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c s_misc.c

scache.o: scache.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c scache.c

ircsprintf.o: ircsprintf.c ../include/dbuf.h $(INCLUDES)
	$(CC) $(CFLAGS) -c ircsprintf.c

s_user.o: s_user.c $(INCLUDES) \
	  ../include/dbuf.h ../include/channel.h ../include/whowas.h
	$(CC) $(CFLAGS) -c s_user.c

s_extra.o: s_extra.c $(INCLUDES) \
	  s_numeric.c ../include/dbuf.h ../include/channel.h ../include/whowas.h
	$(CC) $(CFLAGS) -c s_extra.c

s_kline.o: s_kline.c $(INCLUDES) \
	  s_numeric.c ../include/dbuf.h ../include/channel.h ../include/whowas.h
	$(CC) $(CFLAGS) -c s_kline.c

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

cidr.o: cidr.c $(INCLUDES)
	$(CC) $(CFLAGS) -c cidr.c

random.o: random.c $(INCLUDES)
	$(CC) $(CFLAGS) -c random.c

extcmodes.o: extcmodes.c $(INCLUDES)
	$(CC) $(CFLAGS) -c extcmodes.c

extbans.o: extbans.c $(INCLUDES)
	$(CC) $(CFLAGS) -c extbans.c

url.o: url.c $(INCLUDES) ../include/url.h
	$(CC) $(CFLAGS) -c url.c

# DO NOT DELETE THIS LINE -- make depend depends on it.

