/* UnrealIRCd crash reporter code.
 * (C) Copyright 2015-2019 Bram Matthys ("Syzop") and the UnrealIRCd Team.
 * License: GPLv2 or later
 */

#include "unrealircd.h"
#ifdef _WIN32
extern void StartUnrealAgain(void);
#endif
#include "version.h"

extern char *getosname(void);

char *find_best_coredump(void)
{
	static char best_fname[512];
	time_t best_time = 0, t;
	struct dirent *dir;
#ifndef _WIN32
	DIR *fd = opendir(TMPDIR);

	if (!fd)
		return NULL;
	
	*best_fname = '\0';
	
	while ((dir = readdir(fd)))
	{
		char *fname = dir->d_name;
		if (strstr(fname, "core") && !strstr(fname, ".so") &&
		    !strstr(fname, ".conf") && !strstr(fname, ".txt") &&
		    !strstr(fname, ".done"))
		{
			char buf[512];
			
			snprintf(buf, sizeof(buf), "%s/%s", TMPDIR, fname);
			t = get_file_time(buf);
			if (t && (t > best_time))
			{
				best_time = t;
				strlcpy(best_fname, buf, sizeof(best_fname));
			}
		}
	}
	closedir(fd);
#else
	/* Windows */
	WIN32_FIND_DATA hData;
	HANDLE hFile;
	
	hFile = FindFirstFile("unrealircd.*.core", &hData);
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;
	
	do
	{
		char *fname = hData.cFileName;
		if (!strstr(fname, ".done"))
		{
			char buf[512];
			strlcpy(buf, fname, sizeof(buf));
			t = get_file_time(buf);
			if (t && (t > best_time))
			{
				best_time = t;
				strlcpy(best_fname, buf, sizeof(best_fname));
			}
		}
	} while (FindNextFile(hFile, &hData));
	FindClose(hFile);
#endif	
	
	if (*best_fname)
		return best_fname;
	
	return NULL; /* none found */
}

/** Find the latest AddressSanitizer log file */
char *find_best_asan_log(void)
{
#ifndef _WIN32
	static char best_fname[512];
	time_t best_time = 0, t;
	struct dirent *dir;
	DIR *fd = opendir(TMPDIR);

	if (!fd)
		return NULL;

	*best_fname = '\0';

	while ((dir = readdir(fd)))
	{
		char *fname = dir->d_name;
		if (strstr(fname, "unrealircd_asan.") && !strstr(fname, ".so") &&
		    !strstr(fname, ".conf") && !strstr(fname, ".txt") &&
		    !strstr(fname, ".done"))
		{
			char buf[512];

			snprintf(buf, sizeof(buf), "%s/%s", TMPDIR, fname);
			t = get_file_time(buf);
			if (t && (t > best_time))
			{
				best_time = t;
				strlcpy(best_fname, buf, sizeof(best_fname));
			}
		}
	}
	closedir(fd);
	return *best_fname ? best_fname : NULL;
#else
	return NULL;
#endif
}

#define EL_AR_MAX MAXPARA
char **explode(char *str, char *delimiter)
{
	static char *ret[EL_AR_MAX+1];
	static char buf[1024];
	char *p, *name;
	int cnt = 0;
	
	memset(&ret, 0, sizeof(ret)); /* make sure all elements are NULL */
	
	strlcpy(buf, str, sizeof(buf));
	for (name = strtoken(&p, buf, delimiter); name; name = strtoken(&p, NULL, delimiter))
	{
		ret[cnt++] = name;
		if (cnt == EL_AR_MAX)
			break;
	}
	ret[cnt] = NULL;
	
	return ret;
}

void crash_report_fix_libs(char *coredump, int *thirdpartymods)
{
#ifndef _WIN32
	FILE *fd;
	char cmd[512], buf[1024];

	/* This is needed for this function to work, but we keep it since it's
	 * useful in general to have the bug report in English as well.
	 */
	setenv("LANG", "C", 1);
	setenv("LC_ALL", "C", 1);

	snprintf(cmd, sizeof(cmd), "echo info sharedlibrary|gdb %s/unrealircd %s 2>&1",
		BINDIR, coredump);

	fd = popen(cmd, "r");
	if (!fd)
		return;

	while((fgets(buf, sizeof(buf), fd)))
	{
		char *file, *path;
		char target[512];
		char **arr;

		stripcrlf(buf);

		if (strstr(buf, ".third."))
		    *thirdpartymods = 1;

		/* Output we are interested is something like this:
		 * <many spaces>    No        /home/blabla/unrealircd/tmp/5114DF16.m_kick.so
		 */
		if (!strstr(buf, " No "))
			continue;
		
		path = strchr(buf, '/');
		if (!path)
			continue;

		if (!strstr(path, TMPDIR))
			continue; /* we only care about our TMPDIR stuff */
		
		file = strrchr(path, '/');
		if (!file)
			continue;
		file++;
		
		/* files have the following two formats:
		 * 5BE7DF9.m_svsnline.so          for modules/m_svsnline.so
		 * 300AA138.chanmodes.nokick.so   for modules/chanmodes/nokick.so
		 */
		arr = explode(file, ".");
		if (!arr[3])
			snprintf(target, sizeof(target), "%s/%s.%s", MODULESDIR, arr[1], arr[2]);
		else
			snprintf(target, sizeof(target), "%s/%s/%s.%s", MODULESDIR, arr[1], arr[2], arr[3]);
		
		if (!file_exists(target))
		{
			printf("WARNING: could not resolve %s: %s does not exist\n", path, target);
		} else {
			if (symlink(target, path) < 0)
				printf("WARNING: could not create symlink %s -> %s\n", path, target);
		}
		
	}
	pclose(fd);
#endif
}

int crash_report_backtrace(FILE *reportfd, char *coredump)
{
	FILE *fd;
	char cmd[512], buf[1024];
	int n;

#ifndef _WIN32
	snprintf(buf, sizeof(buf), "%s/gdb.commands", TMPDIR);
	fd = fopen(buf, "w");
	if (!fd)
	{
		printf("ERROR: Could not write to %s.\n", buf);
		return 0;
	}
	fprintf(fd, "frame\n"
	            "echo \\n\n"
	            "list\n"
	            "echo \\n\n"
	            "x/s our_mod_version\n"
	            "echo \\n\n"
	            "x/s backupbuf\n"
	            "echo \\n\n"
	            "bt\n"
	            "echo \\n\n"
	            "bt full\n"
	            "echo \\n\n"
	            "quit\n");
	fclose(fd);

	
	snprintf(cmd, sizeof(cmd), "gdb -batch -x %s %s/unrealircd %s 2>&1",
		buf, BINDIR, coredump);
	
	fd = popen(cmd, "r");
	if (!fd)
		return 0;
	
	fprintf(reportfd, "START OF BACKTRACE\n");
	while((fgets(buf, sizeof(buf), fd)))
	{
		stripcrlf(buf);
		fprintf(reportfd, " %s\n", buf);
	}
	n = pclose(fd);

	fprintf(reportfd, "END OF BACKTRACE\n");

	if (WEXITSTATUS(n) == 127)
		return 0;

	return 1;
#else
	fd = fopen(coredump, "r");
	if (!fd)
		return 0;
	fprintf(reportfd, "START OF CRASH DUMP\n");
	while((fgets(buf, sizeof(buf), fd)))
	{
		stripcrlf(buf);
		fprintf(reportfd, " %s\n", buf);
	}
	fclose(fd);
	fprintf(reportfd, "END OF CRASH DUMP\n");
	return 1;
#endif
}

int crash_report_asan_log(FILE *reportfd, char *coredump)
{
#ifndef _WIN32
	time_t coretime, asantime;
	FILE *fd;
	char buf[1024];
	char *asan_log = find_best_asan_log();
	int n;

	if (!asan_log)
		return 0;

	coretime = get_file_time(coredump);
	asantime = get_file_time(asan_log);

	fprintf(reportfd, "ASan log file found '%s' which is %ld seconds older than core file\n",
		asan_log,
		(long)((long)(coretime) - (long)asantime));

	fd = fopen(asan_log, "r");
	if (!fd)
	{
		fprintf(reportfd, "Could not open ASan log (%s)\n", strerror(errno));
		return 0;
	}
	fprintf(reportfd, "START OF ASAN LOG\n");
	while((fgets(buf, sizeof(buf), fd)))
	{
		stripcrlf(buf);
		fprintf(reportfd, " %s\n", buf);
	}
	n = fclose(fd);
	fprintf(reportfd, "END OF ASAN LOG\n");

	if (WEXITSTATUS(n) == 127)
		return 0;

	return 1;
#else
	return 0;
#endif
}

void crash_report_header(FILE *reportfd, char *coredump)
{
	time_t t;
	
	fprintf(reportfd, "== UNREALIRCD CRASH REPORT ==\n"
	                  "\n"
	                  "SYSTEM INFORMATION:\n");
	
	fprintf(reportfd, "UnrealIRCd version: %s\n", VERSIONONLY);
#if defined(__VERSION__)
	fprintf(reportfd, "          Compiler: %s\n", __VERSION__);
#endif
	
	fprintf(reportfd, "  Operating System: %s\n", MYOSNAME);

	
	fprintf(reportfd, "Using core file: %s\n", coredump);
	
	t = get_file_time(coredump);
	if (t != 0)
	{
		fprintf(reportfd, "Crash date/time: %s\n", myctime(t) ? myctime(t) : "???");
		fprintf(reportfd, " Crash secs ago: %ld\n",
			(long)(time(NULL) - t));
	} else {
		fprintf(reportfd, "Crash date/time: UNKNOWN\n");
		fprintf(reportfd, " Crash secs ago: UNKNOWN\n");
	}
	
	fprintf(reportfd, "\n");
}

/** Checks if the binary is newer than the coredump.
 * If that's the case (1) then the core dump is likely not very usable.
 */
int corefile_vs_binary_mismatch(char *coredump)
{
#ifndef _WIN32
	time_t core, binary;
	char fname[512];
	
	snprintf(fname, sizeof(fname), "%s/unrealircd", BINDIR);
	
	core = get_file_time(coredump);
	binary = get_file_time(fname);
	
	if (!core || !binary)
		return 0; /* don't know then */
	
	if (binary > core)
		return 1; /* yup, mismatch ;/ */
	
	return 0; /* GOOD! */
#else
	return 0; /* guess we don't check this on Windows? Or will we check UnrealIRCd.exe... hmm.. yeah maybe good idea */
#endif
}

int attach_file(FILE *fdi, FILE *fdo)
{
	char binbuf[60];
	char printbuf[100];
	size_t n, total = 0;

	fprintf(fdo, "\n*** ATTACHMENT ****\n");
	while((n = fread(binbuf, 1, sizeof(binbuf), fdi)) > 0)
	{
		b64_encode(binbuf, n, printbuf, sizeof(printbuf));
		fprintf(fdo, "%s\n", printbuf);

		total += strlen(printbuf);

		if (total > 15000000)
			return 0; /* Safety limit */
	}

	fprintf(fdo, "*** END OF ATTACHMENT ***\n");
	return 1;
}

/** Figure out the libc library name (.so file), copy it to tmp/
 * to include it in the bug report. This can improve the backtrace
 * a lot (read: make it actually readable / useful) in case we
 * crash in a libc function.
 */
char *copy_libc_so(void)
{
#ifdef _WIN32
	return "";
#else
	FILE *fd;
	char buf[1024];
	static char ret[512];
	char *basename = NULL, *libcname = NULL, *p, *start;

	snprintf(buf, sizeof(buf), "ldd %s/unrealircd 2>/dev/null", BINDIR);
	fd = popen(buf, "r");
	if (!fd)
		return "";

	while ((fgets(buf, sizeof(buf), fd)))
	{
		stripcrlf(buf);
		p = strstr(buf, "libc.so");
		if (!p)
			continue;
		basename = p;
		p = strchr(p, ' ');
		if (!p)
			continue;
		*p++ = '\0';
		p = strstr(p, "=> ");
		if (!p)
			continue;
		start = p += 3; /* skip "=> " */
		p = strchr(start, ' ');
		if (!p)
			continue;
		*p = '\0';
		libcname = start;
		break;
	}
	pclose(fd);

	if (!basename || !libcname)
		return ""; /* not found, weird */

	snprintf(ret, sizeof(ret), "%s/%s", TMPDIR, basename);
	if (!unreal_copyfile(libcname, ret))
		return ""; /* copying failed */

	return ret;
#endif
}
int attach_coredump(FILE *fdo, char *coredump)
{
	FILE *fdi;
	char fname[512];
	char *libcname = copy_libc_so();

#ifndef _WIN32
	/* On *NIX we create a .tar.bz2 / .tar.gz (may take a couple of seconds) */
	printf("Please wait...\n");
	snprintf(fname, sizeof(fname), "tar c %s/unrealircd %s %s %s 2>/dev/null|(bzip2 || gzip) 2>/dev/null",
		BINDIR, coredump, MODULESDIR, libcname);

	fdi = popen(fname, "r");
#else
	/* On Windows we attach de .mdmp, the small minidump file */
	strlcpy(fname, coredump, sizeof(fname));
	if (strlen(fname) > 5)
		fname[strlen(fname)-5] = '\0'; /* cut off the '.core' part */
	strlcat(fname, ".mdmp", sizeof(fname)); /* and add '.mdmp' */
	fprintf(fdo, "Windows MINIDUMP: %s\n", fname);
	fdi = fopen(fname, "rb");
#endif
	if (!fdi)
		return 0;

	attach_file(fdi, fdo);

#ifndef _WIN32
	pclose(fdi);
#else
	fclose(fdi);
#endif
	return 1;
}

char *generate_crash_report(char *coredump, int *thirdpartymods)
{
	static char reportfname[512];
	FILE *reportfd;

	*thirdpartymods = 0;

	if (coredump == NULL)
		coredump = find_best_coredump();
	
	if (coredump == NULL)
		return NULL; /* nothing available */

	if (corefile_vs_binary_mismatch(coredump))
		return NULL;
	
	snprintf(reportfname, sizeof(reportfname), "%s/crash.report.%s.%ld.txt",
		TMPDIR, unreal_getfilename(coredump), (long)time(NULL));

	reportfd = fopen(reportfname, "w");
	if (!reportfd)
	{
		printf("ERROR: could not open '%s' for writing\n", reportfname);
		return NULL;
	}

	crash_report_header(reportfd, coredump);
	crash_report_fix_libs(coredump, thirdpartymods);
	
	crash_report_backtrace(reportfd, coredump);
	crash_report_asan_log(reportfd, coredump);
	attach_coredump(reportfd, coredump);

	fclose(reportfd);

	return reportfname;
}

#define REPORT_NEVER	-1
#define REPORT_ASK		0
#define REPORT_AUTO		1

#define CRASH_REPORT_HOST "crash.unrealircd.org"

SSL_CTX *crashreport_init_tls(void)
{
	SSL_CTX *ctx_client;
	char buf[512];
	
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	ctx_client = SSL_CTX_new(SSLv23_client_method());
	if (!ctx_client)
		return NULL;
#ifdef HAS_SSL_CTX_SET_MIN_PROTO_VERSION
	SSL_CTX_set_min_proto_version(ctx_client, TLS1_2_VERSION);
#endif
	SSL_CTX_set_options(ctx_client, SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3|SSL_OP_NO_TLSv1|SSL_OP_NO_TLSv1_1);

	/* Verify peer certificate */
	snprintf(buf, sizeof(buf), "%s/tls/curl-ca-bundle.crt", CONFDIR);
	SSL_CTX_load_verify_locations(ctx_client, buf, NULL);
	SSL_CTX_set_verify(ctx_client, SSL_VERIFY_PEER, NULL);

	/* Limit ciphers as well */
	SSL_CTX_set_cipher_list(ctx_client, UNREALIRCD_DEFAULT_CIPHERS);

	return ctx_client;
}	

int crashreport_send(char *fname)
{
	char buf[1024];
	char header[512], footer[512];
	char delimiter[41];
	int filesize;
	int n;
	FILE *fd;
	SSL_CTX *ctx_client;
	SSL *ssl = NULL;
	BIO *socket = NULL;
	int xfr = 0;
	char *errstr = NULL;
	
	filesize = get_file_size(fname);
	if (filesize < 0)
		return 0;
	
	for (n = 0; n < sizeof(delimiter); n++)
		delimiter[n] = getrandom8()%26 + 'a';
	delimiter[sizeof(delimiter)-1] = '\0';
	
	snprintf(header, sizeof(header), "--%s\r\n"
	                           "Content-Disposition: form-data; name=\"upload\"; filename=\"crash.txt\"\r\n"
	                           "Content-Type: text/plain\r\n"
	                           "\r\n",
	                           delimiter);
	snprintf(footer, sizeof(footer), "\r\n--%s--\r\n", delimiter);

	ctx_client = crashreport_init_tls();
	if (!ctx_client)
	{
		printf("ERROR: TLS initalization failure (I)\n");
		return 0;
	}
	
	socket = BIO_new_ssl_connect(ctx_client);
	if (!socket)
	{
		printf("ERROR: TLS initalization failure (II)\n");
		return 0;
	}
	
	BIO_set_conn_hostname(socket, CRASH_REPORT_HOST ":443");

	if (BIO_do_connect(socket) != 1)
	{
		printf("ERROR: Could not connect to %s\n", CRASH_REPORT_HOST);
		return 0;
	}
	
	if (BIO_do_handshake(socket) != 1)
	{
		printf("ERROR: Could not connect to %s (TLS handshake failed)\n", CRASH_REPORT_HOST);
		return 0;
	}

	BIO_get_ssl(socket, &ssl);
	if (!ssl)
	{
		printf("ERROR: Could not get TLS connection from BIO\n");
		return 0;
	}

	if (!verify_certificate(ssl, CRASH_REPORT_HOST, &errstr))
	{
		printf("Certificate problem with crash.unrealircd.org: %s\n", errstr);
		printf("Fatal error. See above.\n");
		return 0;
	}

	snprintf(buf, sizeof(buf), "POST /crash.php HTTP/1.1\r\n"
	                    "User-Agent: UnrealIRCd %s\r\n"
	                    "Host: %s\r\n"
	                    "Accept: */*\r\n"
	                    "Content-Length: %d\r\n"
	                    "Expect: 100-continue\r\n"
	                    "Content-Type: multipart/form-data; boundary=%s\r\n"
	                    "\r\n",
	                    VERSIONONLY,
	                    CRASH_REPORT_HOST,
	                    (int)(filesize+strlen(header)+strlen(footer)),
	                    delimiter);
	
	BIO_puts(socket, buf);
	
	memset(buf, 0, sizeof(buf));
	n = BIO_read(socket, buf, 255);
	if ((n < 0) || strncmp(buf, "HTTP/1.1 100", 12))
	{
		printf("Error transmitting bug report (stage II, n=%d)\n", n);
		if (!strncmp(buf, "HTTP/1.1 403", 12))
		{
			printf("Your crash report was rejected automatically.\n"
			       "This normally means your UnrealIRCd version is too old and unsupported.\n"
			       "Chances are that your crash issue is already fixed in a later release.\n"
			       "Check https://www.unrealircd.org/ for latest releases!\n");
		}
		return 0;
	}
	
	fd = fopen(fname, "rb");
	if (!fd)
		return 0;

	BIO_puts(socket, header);

#ifndef _WIN32
	printf("Sending...");
#endif
	while ((fgets(buf, sizeof(buf), fd)))
	{
		BIO_puts(socket, buf);
#ifndef _WIN32
		if ((++xfr % 1000) == 0)
		{
			printf(".");
			fflush(stdout);
		}
#endif
	}
	fclose(fd);

	BIO_puts(socket, footer);

	do { } while(BIO_should_retry(socket)); /* make sure we are really finished (you never know with TLS) */

#ifndef _WIN32
	printf("\n");
#endif
	BIO_free_all(socket);
	
	SSL_CTX_free(ctx_client);
	
	return 1;
}

void mark_coredump_as_read(char *coredump)
{
	char buf[512];
	
	snprintf(buf, sizeof(buf), "%s.%ld.done", coredump, (long)time(NULL));
	
	(void)rename(coredump, buf);
}

static int report_pref = REPORT_ASK;

void report_crash_not_sent(char *fname)
{
		printf("Crash report will not be sent to UnrealIRCd Team.\n"
		       "\n"
		       "Feel free to read the report at %s and delete it.\n"
		       "Or, if you change your mind, you can submit it anyway at https://bugs.unrealircd.org/\n"
		       " (if you do, please set the option 'View Status' at the end of the bug report page to 'private'!!)\n", fname);
}

/** This checks if there are indications that 3rd party modules are
 * loaded. This is used to provide a small warning to the user that
 * the crash may be likely due to that.
 */
int check_third_party_mods_present(void)
{
#ifndef _WIN32
	struct dirent *dir;
	DIR *fd = opendir(TMPDIR);

	if (!fd)
		return 0;

	/* We search for files like tmp/FC5C3116.third.somename.so */
	while ((dir = readdir(fd)))
	{
		char *fname = dir->d_name;
		if (strstr(fname, ".third.") && strstr(fname, ".so"))
		{
			closedir(fd);
			return 1;
		}
	}
	closedir(fd);
#endif
	return 0;
}

void report_crash(void)
{
	char *coredump, *fname;
	int thirdpartymods = 0;
	int crashed_secs_ago;

	if (!running_interactively() && (report_pref != REPORT_AUTO))
		exit(0); /* don't bother if we run through cron or something similar */

	coredump = find_best_coredump();
	if (!coredump)
		return; /* no crashes */

	crashed_secs_ago = time(NULL) - get_file_time(coredump);
	if (crashed_secs_ago > 86400*7)
		return; /* stop bothering about it after a while */

	fname = generate_crash_report(coredump, &thirdpartymods);
	
	if (!fname)
		return;

	if (thirdpartymods == 0)
		thirdpartymods = check_third_party_mods_present();
#ifndef _WIN32
	printf("The IRCd has been started now (and is running), but it did crash %d seconds ago.\n", crashed_secs_ago);
	printf("Crash report generated in: %s\n\n", fname);

	if (thirdpartymods)
	{
	    printf("** IMPORTANT **\n"
               "Your UnrealIRCd crashed and you have 3rd party modules loaded (modules created\n"
               "by someone other than the UnrealIRCd team). If you installed new 3rd party\n"
               "module(s) in the past few weeks we suggest to unload these modules and see if\n"
               "the crash issue dissapears. If so, that module is probably to blame.\n"
               "If you keep crashing without any 3rd party modules loaded then please do report\n"
               "it to the UnrealIRCd team.\n"
               "The reason we ask you to do this is because MORE THAN 95%% OF ALL CRASH ISSUES\n"
               "ARE CAUSED BY 3RD PARTY MODULES and not by an UnrealIRCd bug.\n"
               "\n");
	}
		
	if (report_pref == REPORT_NEVER)
	{
		report_crash_not_sent(fname);
		return;
	} else
	if (report_pref == REPORT_ASK)
	{
		char answerbuf[64], *answer;
		printf("Shall I send a crash report to the UnrealIRCd developers?\n");
		if (!thirdpartymods)
			printf("Crash reports help us greatly with fixing bugs that affect you and others\n");
		else
			printf("NOTE: If the crash is caused by a 3rd party module then UnrealIRCd devs can't fix that.\n");
		printf("\n");
		
		do
		{
			printf("Answer (Y/N): ");
			*answerbuf = '\0';
			answer = fgets(answerbuf, sizeof(answerbuf), stdin);
			
			if (answer && (toupper(*answer) == 'N'))
			{
				report_crash_not_sent(fname);
				return;
			}
			if (answer && (toupper(*answer) == 'Y'))
			{
				break;
			}
			
			printf("Invalid response. Please enter either Y or N\n\n");
		} while(1);
	} else if (report_pref != REPORT_AUTO)
	{
		printf("Huh. report_pref setting is weird. Aborting.\n");
		return;
	}

	if (running_interactively())
	{
		char buf[8192], *line;

		printf("\nDo you want to give your e-mail address so we could ask for additional information or "
		       "give you feedback about the crash? This is completely optional, just press ENTER to skip.\n\n"
		       "E-mail address (optional): ");
		line = fgets(buf, sizeof(buf), stdin);
		
		if (line && *line && (*line != '\n'))
		{
			FILE *fd = fopen(fname, "a");
			if (fd)
			{
				fprintf(fd, "\nUSER E-MAIL ADDRESS: %s\n", line);
				fclose(fd);
			}
		}

		printf("\nDo you have anything else to tell about the crash, maybe the circumstances? You can type 1 single line\n"
		       "Again, this is completely optional. Just press ENTER to skip.\n\n"
		       "Additional information (optional): ");
		line = fgets(buf, sizeof(buf), stdin);
		
		if (line && *line && (*line != '\n'))
		{
			FILE *fd = fopen(fname, "a");
			if (fd)
			{
				fprintf(fd, "\nCOMMENT BY USER: %s\n", line);
				fclose(fd);
			}
		}
	}
	if (crashreport_send(fname))
	{
		printf("\nThe crash report has been sent to the UnrealIRCd developers. "
		       "Thanks a lot for helping to make UnrealIRCd a better product!\n\n");
	}

#else
	/* Windows */
	if (MessageBox(NULL, "UnrealIRCd crashed. May I send a report about this to the UnrealIRCd developers? This helps us a lot.",
	                     "UnrealIRCd crash",
	                     MB_YESNO|MB_ICONQUESTION) == IDYES)
	{
		/* Yay */
		
		if (crashreport_send(fname))
		{
			MessageBox(NULL, "The crash report has been sent to the UnrealIRCd developers. "
			                 "If you have any additional information (like details surrounding "
			                 "the crash) then please e-mail syzop@unrealircd.org, such "
			                 "information is most welcome. Thanks!",
			           "UnrealIRCd crash report sent", MB_ICONINFORMATION|MB_OK);
		}
	}
#endif
	mark_coredump_as_read(coredump);
	
#ifdef _WIN32
	if (MessageBox(NULL, "Start UnrealIRCd again?",
	                     "UnrealIRCd crash",
	                     MB_YESNO|MB_ICONQUESTION) == IDYES)
	{
		StartUnrealAgain();
	}
#endif
}
