#************************************************************************
#*   IRC - Internet Relay Chat, src/modules/Makefile
#*   Copyright (C) Carsten V. Munk 2001
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

COMMANDS_MODULES=m_sethost.so m_chghost.so

COMMANDS=m_sethost.c m_chghost.c

MODULES=commands.so $(COMMANDS_MODULES)

all: build

build: $(MODULES)

commands.so: l_commands.c $(COMMANDS) $(INCLUDES)
	$(CC) $(CFLAGS) -fPIC -DPIC -shared -o commands.so l_commands.c \
	$(COMMANDS)

m_sethost.so: m_sethost.c $(INCLUDES)
	$(CC) $(CFLAGS) -fPIC -DPIC -shared -DDYNAMIC_LINKING \
		-o m_sethost.so m_sethost.c

m_chghost.so: m_chghost.c $(INCLUDES)
	$(CC) $(CFLAGS) -fPIC -DPIC -shared -DDYNAMIC_LINKING \
		 -o m_chghost.so m_chghost.c

clean:
	$(RM) -f *.o *.so *~ core
