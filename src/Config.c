/*
 * $Id$
 */
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#ifdef _WIN32
#include <sys\stat.h>
#endif
#include <time.h>

int  _FD_SETSIZE = 1024;
char _NS_ADDRESS[256], _KLINE_ADDRESS[256];


char Makefile[] =
    "CC=cl\n"
    "FD_SETSIZE=/D FD_SETSIZE=$FD_SETSIZE\n"
    "CFLAGS=/MT /O2 /I ./INCLUDE /Fosrc/ /nologo $(FD_SETSIZE) $(NS_ADDRESS) /D NOSPOOF=1 /c\n"
    "INCLUDES=./include/struct.h ./include/config.h ./include/sys.h \\\n"
    " ./include/common.h ./include/settings.h ./include/h.h ./include/numeric.h \\\n"
    " ./include/msg.h ./include/setup.h\n"
    "LINK=link.exe\n"
    "LFLAGS=kernel32.lib user32.lib gdi32.lib shell32.lib wsock32.lib \\\n"
    " oldnames.lib libcmt.lib /nodefaultlib /nologo /out:SRC/WIRCD.EXE\n"
    "OBJ_FILES=SRC/CHANNEL.OBJ SRC/USERLOAD.OBJ SRC/SEND.OBJ SRC/BSD.OBJ \\\n"
    " SRC/CIO_MAIN.OBJ SRC/S_CONF.OBJ SRC/DBUF.OBJ SRC/RES.OBJ \\\n"
    " SRC/HASH.OBJ SRC/CIO_INIT.OBJ SRC/PARSE.OBJ SRC/IRCD.OBJ \\\n"
    " SRC/S_NUMERIC.OBJ SRC/WHOWAS.OBJ SRC/RES_COMP.OBJ SRC/S_AUTH.OBJ \\\n"
    " SRC/HELP.OBJ SRC/S_MISC.OBJ SRC/MATCH.OBJ SRC/CRULE.OBJ \\\n"
    " SRC/S_DEBUG.OBJ SRC/RES_INIT.OBJ SRC/SUPPORT.OBJ SRC/LIST.OBJ \\\n"
    " SRC/S_ERR.OBJ SRC/PACKET.OBJ SRC/CLASS.OBJ SRC/S_BSD.OBJ \\\n"
    " SRC/MD5.OBJ SRC/S_SERV.OBJ SRC/S_USER.OBJ SRC/WIN32.OBJ \\\n"
    " SRC/DYNCONF.OBJ\\\n"
    " SRC/VERSION.OBJ SRC/WIN32.RES SRC/CLOAK.OBJ SRC/S_UNREAL.OBJ\n"
    "RC=rc.exe\n"
    "\n"
    "ALL: SRC/WIRCD.EXE SRC/CHKCONF.EXE\n"
    "        @echo Complete.\n"
    "\n"
    "CLEAN:\n"
    "        -@erase src\\*.exe 2>NUL\n"
    "        -@erase src\\*.obj 2>NUL\n"
    "        -@erase src\\win32.res 2>NUL\n"
    "        -@erase src\\version.c 2>NUL\n"
    "\n"
    "include/setup.h:\n"
    "        @echo Hmm...doesn't look like you've run Config...\n"
    "        @echo Doing so now.\n"
    "        @config.exe\n"
    "\n"
    "src/version.c: dummy\n"
    "        @config.exe -v\n"
    "\n"
    "src/version.obj: src/version.c\n"
    "        $(CC) $(CFLAGS) src/version.c\n"
    "\n"
    "SRC/WIRCD.EXE: $(OBJ_FILES) src/version.obj\n"
    "        $(LINK) $(LFLAGS) $(OBJ_FILES)\n"
    "\n"
    "SRC/CHKCONF.EXE: ./include/struct.h ./include/config.h ./include/sys.h \\\n"
    "                 ./include/common.h ./src/crule.c ./src/match.c ./src/chkconf.c\n"
    "        $(CC) /nologo /I ./include /D CR_CHKCONF /Fosrc/chkcrule.obj /c src/crule.c\n"
    "        $(CC) /nologo /I ./include /D CR_CHKCONF /Fosrc/chkmatch.obj /c src/match.c\n"
    "        $(CC) /nologo /I ./include /D CR_CHKCONF /Fosrc/chkconf.obj /c src/chkconf.c\n"
    "        $(LINK) /nologo /out:src/chkconf.exe src/chkconf.obj src/chkmatch.obj \\\n"
    "                src/chkcrule.obj\n"
    "\n"
    "src/parse.obj: src/parse.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/parse.c\n"
    "\n"
    "src/bsd.obj: src/bsd.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/bsd.c\n"
    "\n"
    "src/dbuf.obj: src/dbuf.c $(INCLUDES) ./include/dbuf.h\n"
    "        $(CC) $(CFLAGS) src/dbuf.c\n"
    "\n"
    "src/packet.obj: src/packet.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/packet.c\n"
    "\n"
    "src/send.obj: src/send.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/send.c\n"
    "\n"
    "src/match.obj: src/match.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/match.c\n"
    "\n"
    "src/support.obj: src/support.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/support.c\n"
    "\n"
    "src/channel.obj: src/channel.c $(INCLUDES) ./include/channel.h\n"
    "        $(CC) $(CFLAGS) src/channel.c\n"
    "\n"
    "src/class.obj: src/class.c $(INCLUDES) ./include/class.h\n"
    "        $(CC) $(CFLAGS) src/class.c\n"
    "\n"
    "src/ircd.obj: src/ircd.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/ircd.c\n"
    "\n"
    "src/list.obj: src/list.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/list.c\n"
    "\n"
    "src/res.obj: src/res.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/res.c\n"
    "\n"
    "src/s_bsd.obj: src/s_bsd.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/s_bsd.c\n"
    "\n"
    "src/s_auth.obj: src/s_auth.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/s_auth.c\n"
    "\n"
    "src/s_conf.obj: src/s_conf.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/s_conf.c\n"
    "\n"
    "src/s_debug.obj: src/s_debug.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/s_debug.c\n"
    "\n"
    "src/s_err.obj: src/s_err.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/s_err.c\n"
    "\n"
    "src/s_misc.obj: src/s_misc.c $(INCLUDES) ./include/dbuf.h\n"
    "        $(CC) $(CFLAGS) src/s_misc.c\n"
    "\n"
    "src/s_user.obj: src/s_user.c $(INCLUDES) ./include/dbuf.h \\\n"
    "                ./include/channel.h ./include/whowas.h\n"
    "        $(CC) $(CFLAGS) src/s_user.c\n"
    "\n"
    "src/dynconf.obj: src/dynconf.c $(INCLUDES) ./include/dbuf.h \\\n"
    "                ./include/channel.h ./include/whowas.h ./include/dynconf.h\n"
    "        $(CC) $(CFLAGS) src/dynconf.c\n"
    "\n"
    "src/s_unreal.obj: src/s_unreal.c $(INCLUDES) ./include/dbuf.h \\\n"
    "                ./include/channel.h ./include/whowas.h\n"
    "        $(CC) $(CFLAGS) src/s_unreal.c\n"
    "\n"
    "src/cloak.obj: src/cloak.c $(INCLUDES) ./include/dbuf.h \\\n"
    "                ./include/channel.h ./include/whowas.h\n"
    "        $(CC) $(CFLAGS) src/s_unreal.c\n"
    "\n"
    "src/s_serv.obj: src/s_serv.c $(INCLUDES) ./include/dbuf.h ./include/whowas.h\n"
    "        $(CC) $(CFLAGS) src/s_serv.c\n"
    "\n"
    "src/s_numeric.obj: src/s_numeric.c $(INCLUDES) ./include/dbuf.h\n"
    "        $(CC) $(CFLAGS) src/s_numeric.c\n"
    "\n"
    "src/whowas.obj: src/whowas.c $(INCLUDES) ./include/dbuf.h ./include/whowas.h\n"
    "        $(CC) $(CFLAGS) src/whowas.c\n"
    "\n"
    "src/hash.obj: src/hash.c $(INCLUDES) ./include/hash.h\n"
    "        $(CC) $(CFLAGS) src/hash.c\n"
    "\n"
    "src/crule.obj: src/crule.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/crule.c\n"
    "\n"
    "src/win32.obj: src/win32.c $(INCLUDES) ./include/resource.h\n"
    "        $(CC) $(CFLAGS) src/win32.c\n"
    "\n"
    "src/cio_main.obj: src/cio_main.c $(INCLUDES) ./include/cio.h ./include/ciofunc.h\n"
    "        $(CC) $(CFLAGS) src/cio_main.c\n"
    "\n"
    "src/cio_init.obj: src/cio_init.c $(INCLUDES) ./include/cio.h ./include/ciofunc.h\n"
    "        $(CC) $(CFLAGS) src/cio_init.c\n"
    "\n"
    "src/res_comp.obj: src/res_comp.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/res_comp.c\n"
    "\n"
    "src/res_init.obj: src/res_init.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/res_init.c\n"
    "\n"
    "src/help.obj: src/help.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/help.c\n"
    "\n"
    "src/md5.obj: src/md5.c $(INCLUDES)\n"
    "        $(CC) $(CFLAGS) src/md5.c\n"
    "\n"
    "src/win32.res: src/win32.rc\n"
    "        $(RC) /l 0x409 /fosrc/win32.res /i ./include /i ./src \\\n"
    "              /d NDEBUG src/win32.rc\n" "\n" "dummy:\n" "\n";


char SetupH[] =
    "/* This is only a wrapper.. --Stskeeps */\n"
    "#include \"win32/setup.h\"\n";


int  main(int argc, char *argv[])
{
	if (argc > 1)
	{
		if (!strcmp(argv[1], "-v"))
			return do_version();

		if (!strcmp(argv[1], "-n"))
			return do_config(1);
	}
	printf
	    ("To do win32 compiling copy include/win32/setup.h to include/\n");
	printf("Copy include/win32/settings.h to include/ and modify it\n");
	printf("and copy makefile.win32 to Makefile\n");
}


int  do_config(int autoconf)
{
	int  fd;
	char str[128];


	if ((fd =
	    open("include\\setup.h", O_CREAT | O_TRUNC | O_WRONLY | O_TEXT,
	    S_IREAD | S_IWRITE)) == -1)
		printf("Error opening include\\setup.h\n\r");
	else
	{
		write(fd, SetupH, strlen(SetupH));
		close(fd);
	}

	while (1)
	{
		/*
		 * FD_SETSIZE
		 */
		printf("\n");
		printf
		    ("How many file descriptors (or sockets) can the irc server use?");
		printf("\n");
		printf("[%d] -> ", _FD_SETSIZE);
		gets(str);
		if (*str != '\n' && *str != '\r')
			sscanf(str, "%d", &_FD_SETSIZE);

		if (_FD_SETSIZE >= 100)
		{
			printf("\n");
			printf("FD_SETSIZE will be overridden using -D "
			    "FD_SETSIZE=%d when compiling ircd.", _FD_SETSIZE);
			printf("\n");
			break;
		}
		printf("\n");
		printf
		    ("You need to enter a number here, greater or equal to 100.\n");
	}
	while (1)
	{
	}
	/*
	 * Now write the makefile out.
	 */
	write_makefile();

	return 0;
}


int  write_makefile(void)
{
	int  fd, makfd, len;
	char *buffer, *s;

	buffer = (char *)malloc(strlen(Makefile) + 4096);
	memcpy(buffer, Makefile, strlen(Makefile) + 1);

	s = (char *)strstr(buffer, "$FD_SETSIZE");
	if (s)
	{
		itoa(_FD_SETSIZE, s, 10);
		memmove(s + strlen(s), s + 11, strlen(s + 11) + 1);
	}


	if ((makfd = open("Makefile", O_CREAT | O_TRUNC | O_WRONLY | O_TEXT,
	    S_IREAD | S_IWRITE)) == -1)
	{
		printf("Error creating Makefile\n\r");
		return 1;
	}
	write(makfd, buffer, strlen(buffer));
	close(makfd);

	free(buffer);
	return 0;
}


int  do_version(void)
{
	int  fd, verfd, generation = 0, len, doingvernow = 0;
	char buffer[16384], *s;

	if ((verfd = open("src\\version.c", O_RDONLY | O_TEXT)) != -1)
	{
		while (!eof(verfd))
		{
			len = read(verfd, buffer, sizeof(buffer) - 1);
			if (len == -1)
				break;
			buffer[len] = 0;
			s = (char *)strstr(buffer, "char *generation = \"");
			if (s)
			{
				s += 20;
				generation = atoi(s);
				break;
			}
		}

		close(verfd);
	}

	if ((fd = open("src\\version.c.SH", O_RDONLY | O_TEXT)) == -1)
	{
		printf("Error opening src\\version.c.SH\n\r");
		return 1;
	}

	if ((verfd =
	    open("src\\version.c", O_CREAT | O_TRUNC | O_WRONLY | O_TEXT,
	    S_IREAD | S_IWRITE)) == -1)
	{
		printf("Error opening src\\version.c\n\r");
		return 1;
	}

	generation++;

	printf("Extracting IRC/ircd/version.c...\n\r");

	while (!eof(fd))
	{
		len = read(fd, buffer, sizeof(buffer) - 1);
		if (len == -1)
			break;
		buffer[len] = 0;
		if (!doingvernow)
		{
			s = (char *)strstr(buffer, "/*");
			if (!s)
				continue;
			memmove(buffer, s, strlen(s) + 1);
			doingvernow = 1;
		}
		s = (char *)strstr(buffer, "$generation");
		if (s)
		{
			itoa(generation, s, 10);
			memmove(s + strlen(s), s + 11, strlen(s + 11) + 1);
		}
		s = (char *)strstr(buffer, "$creation");
		if (s)
		{
			time_t t = time(0);
			char *ct = ctime(&t);

			memmove(s + strlen(ct) - 1, s + 9, strlen(s + 9) + 1);
			memmove(s, ct, strlen(ct) - 1);
		}
		s = (char *)strstr(buffer, "$package");
		if (s)
		{
			memmove(s, "IRC", 3);
			memmove(s + 3, s + 8, strlen(s + 8) + 1);
		}

		s = (char *)strstr(buffer, "!SUB!THIS!");
		if (s)
			*s = 0;

		write(verfd, buffer, strlen(buffer));
	}

	close(fd);
	close(verfd);
	return 0;
}
