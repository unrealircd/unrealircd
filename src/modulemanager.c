/* UnrealIRCd module manager.
 * (C) Copyright 2019 Bram Matthys ("Syzop") and the UnrealIRCd Team.
 * License: GPLv2
 * See https://www.unrealircd.org/docs/Module_manager for user documentation.
 */

#include "unrealircd.h"
#ifndef _WIN32

#define MODULEMANAGER_CONNECT_TIMEOUT	7
#define MODULEMANAGER_READ_TIMEOUT	20

typedef struct ManagedModule ManagedModule;

struct ManagedModule
{
	ManagedModule *prev, *next;
	char *repo_url;
	char *name;
	char *author;
	char *troubleshooting;
	char *documentation;
	char *version;
	char *source;
	char *sha256sum;
	char *min_unrealircd_version;
	char *max_unrealircd_version;
	char *description;
	MultiLine *post_install_text;
};

static ManagedModule *managed_modules = NULL;

/* We normally do a 'make install' after upgrading a module.
 * However we will skip it if --no-install is done.
 * This is only done during 'make', as it is unexpected to
 * already install modules at the final location before
 * 'make install' was issued.
 */
static int no_make_install = 0;

/* Forward declarations */
int mm_valid_module_name(char *name);
void free_managed_module(ManagedModule *m);


SSL_CTX *mm_init_tls(void)
{
	SSL_CTX *ctx_client;
	char buf1[512], buf2[512];
	char *curl_ca_bundle = buf1;
	
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
	snprintf(buf1, sizeof(buf1), "%s/tls/curl-ca-bundle.crt", CONFDIR);
	if (!file_exists(buf1))
	{
		snprintf(buf2, sizeof(buf2), "%s/doc/conf/tls/curl-ca-bundle.crt", BUILDDIR);
		if (!file_exists(buf2))
		{
			fprintf(stderr, "ERROR: Neither %s nor %s exist.\n"
			                "Cannot use module manager without curl-ca-bundle.crt\n",
			                buf1, buf2);
			exit(-1);
		}
		curl_ca_bundle = buf2;
	}
	SSL_CTX_load_verify_locations(ctx_client, curl_ca_bundle, NULL);
	SSL_CTX_set_verify(ctx_client, SSL_VERIFY_PEER, NULL);

	/* Limit ciphers as well */
	SSL_CTX_set_cipher_list(ctx_client, UNREALIRCD_DEFAULT_CIPHERS);

	return ctx_client;
}	

int parse_url(const char *url, char **host, int *port, char **document)
{
	char *p;
	static char hostbuf[256];
	static char documentbuf[512];

	if (strncmp(url, "https://", 8))
	{
		fprintf(stderr, "ERROR: URL Must start with https! URL: %s\n", url);
		return 0;
	}
	url += 8; /* skip over https:// part */

	p = strchr(url, '/');
	if (!p)
		return 0;

	strlncpy(hostbuf, url, sizeof(hostbuf), p - url);

	strlcpy(documentbuf, p, sizeof(documentbuf));

	*host = hostbuf;
	*document = documentbuf;
	// TODO: parse port, rather than hardcode:
	*port = 443;
	return 1;
}

int mm_http_request(char *url, char *fname, int follow_redirects)
{
	char *host = NULL;
	int port = 0;
	char *document = NULL;
	char hostandport[256];
	char buf[1024];
	int n;
	FILE *fd;
	SSL_CTX *ctx_client;
	SSL *ssl = NULL;
	BIO *socket = NULL;
	char *errstr = NULL;
	int got_data = 0, first_line = 1;
	int http_redirect = 0;

	if (!parse_url(url, &host, &port, &document))
		return 0;

	snprintf(hostandport, sizeof(hostandport), "%s:%d", host, port);

	ctx_client = mm_init_tls();
	if (!ctx_client)
	{
		fprintf(stderr, "ERROR: TLS initalization failure (I)\n");
		return 0;
	}

	alarm(MODULEMANAGER_CONNECT_TIMEOUT);

	socket = BIO_new_ssl_connect(ctx_client);
	if (!socket)
	{
		fprintf(stderr, "ERROR: TLS initalization failure (II)\n");
		goto out1;
	}

	BIO_get_ssl(socket, &ssl);
	if (!ssl)
	{
		fprintf(stderr, "ERROR: Could not get TLS connection from BIO -- strange\n");
		goto out2;
	}
	
	BIO_set_conn_hostname(socket, hostandport);
	SSL_set_tlsext_host_name(ssl, host);

	if (BIO_do_connect(socket) != 1)
	{
		fprintf(stderr, "ERROR: Could not connect to %s\n", hostandport);
		//config_report_ssl_error(); FIXME?
		goto out2;
	}
	
	if (BIO_do_handshake(socket) != 1)
	{
		fprintf(stderr, "ERROR: Could not connect to %s (TLS handshake failed)\n", hostandport);
		//config_report_ssl_error(); FIXME?
		goto out2;
	}

	if (!verify_certificate(ssl, host, &errstr))
	{
		fprintf(stderr, "Certificate problem for %s: %s\n", host, errstr);
		goto out2;
	}

	snprintf(buf, sizeof(buf), "GET %s HTTP/1.1\r\n"
	                    "User-Agent: UnrealIRCd %s\r\n"
	                    "Host: %s\r\n"
	                    "Connection: close\r\n"
	                    "\r\n",
	                    document,
	                    VERSIONONLY,
	                    hostandport);

	BIO_puts(socket, buf);
	
	fd = fopen(fname, "wb");
	if (!fd)
	{
		fprintf(stderr, "Could not write to temporary file '%s': %s\n",
			fname, strerror(errno));
		goto out2;
	}

	alarm(MODULEMANAGER_READ_TIMEOUT);
	while ((n = BIO_read(socket, buf, sizeof(buf)-1)) > 0)
	{
		buf[n] = '\0';
		if (got_data)
		{
			fwrite(buf, n, 1, fd); // TODO: check for write errors
		} else {
			/* Still need to parse header */
			char *line, *p = NULL;

			for (line = strtoken(&p, buf, "\n"); line; line = strtoken(&p, NULL, "\n"))
			{
				if (first_line)
				{
					if (http_redirect)
					{
						if (!strncmp(line, "Location: ", 10))
						{
							line += 10;
							stripcrlf(line);
							fclose(fd);
							BIO_free_all(socket);
							SSL_CTX_free(ctx_client);
							if (strncmp(line, "https://", 8))
							{
								fprintf(stderr, "Invalid HTTP Redirect to '%s' -- must start with https://\n", line);
								return 0;
							}
							printf("Following redirect to %s\n", line);
							return mm_http_request(line, fname, 0);
						}
						continue;
					}
					if (!strncmp(line, "HTTP/1.1 301", 12) ||
					    !strncmp(line, "HTTP/1.1 302", 12) ||
					    !strncmp(line, "HTTP/1.1 303", 12) ||
					    !strncmp(line, "HTTP/1.1 307", 12) ||
					    !strncmp(line, "HTTP/1.1 308", 12))
					{
						if (!follow_redirects)
						{
							fprintf(stderr, "ERROR: received HTTP(S) redirect while already following another HTTP(S) redirect.\n");
							goto out3;
						}
						http_redirect = 1;
						continue;
					}
					if (strncmp(line, "HTTP/1.1 200", 12))
					{
						stripcrlf(line);
						if (strlen(line) > 128)
							line[128] = '\0';
						fprintf(stderr, "Error while fetching %s: %s\n", url, line);
						goto out3;
					}
					first_line = 0;
				}
				if (!*line || !strcmp(line, "\r"))
				{
					int remaining_bytes;

					got_data = 1;
					/* Bit of a hack here to write part of the data
					 * that is not part of the header but the data response..
					 * We need to jump over the NUL byte and then check
					 * to see if we can access it.. since it could be either
					 * a NUL byte due to the strtoken() or a NUL byte simply
					 * because that was all the data from BIO_read() at this point.
					 */
					if (*line == '\0')
						line += 1; /* jump over \0 */
					else
						line += 2; /* jump over \r\0 */
					remaining_bytes = n - (line - buf);
					if (remaining_bytes > 0)
						fwrite(line, remaining_bytes, 1, fd);
					break; /* must break the for loop here */
				}
			}
		}
	}

	if (!got_data)
	{
		fprintf(stderr, "Error while fetching %s: unable to retrieve data\n", url);
		goto out3;
	}

	fclose(fd);
	BIO_free_all(socket);
	SSL_CTX_free(ctx_client);
	return 1;
out3:
	fclose(fd);
out2:
	BIO_free_all(socket);
out1:
	SSL_CTX_free(ctx_client);
	alarm(0);
	return 0;
}

typedef enum ParseModuleHeaderStage {
	PMH_STAGE_LOOKING		= 0,
	PMH_STAGE_MODULEHEADER		= 1,
	PMH_STAGE_MOD_HEADER		= 2,
	PMH_STAGE_GOT_NAME		= 3,
	PMH_STAGE_GOT_VERSION		= 4,
	PMH_STAGE_GOT_DESCRIPTION	= 5,
	PMH_STAGE_GOT_AUTHOR		= 6,
	PMT_STAGE_DONE			= 7,
} ParseModuleHeaderStage;

typedef enum ParseModuleConfigStage {
	PMC_STAGE_LOOKING	= 0,
	PMC_STAGE_STARTED	= 1,
	PMC_STAGE_FINISHED	= 2,
} ParseModuleConfigStage;

int parse_quoted_string(char *buf, char *dest, size_t destlen)
{
	char *p, *p2;
	size_t max;
	char *i, *o;

	p = strchr(buf, '"');
	if (!p)
		return 0;
	p2 = strrchr(p+1, '"');
	if (!p2)
		return 0;
	max = p2 - p;
	if (max > destlen)
		max = destlen;
	strlcpy(dest, p+1, max);
	unreal_del_quotes(dest);
	return 1;
}

#undef CheckNull
#define CheckNull(x) if ((!(x)->value) || (!(*((x)->value)))) { config_error("%s:%i: missing parameter", m->name, (x)->line_number); return 0; }

/** Parse a module { } line from a module (not repo!!) */
int mm_module_file_config(ManagedModule *m, ConfigEntry *ce)
{
	ConfigEntry *cep;

	if (ce->value)
	{
		config_error("%s:%d: module { } block should not have a name.",
			m->name, ce->line_number);
		return 0;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "source") ||
		    !strcmp(cep->name, "version") ||
		    !strcmp(cep->name, "author") ||
		    !strcmp(cep->name, "sha256sum") || 
		    !strcmp(cep->name, "description")
		    )
		{
			config_error("%s:%d: module::%s should not be in here (it only exists in repository entries)",
				m->name, cep->line_number, cep->name);
			return 0;
		}
		else if (!strcmp(cep->name, "troubleshooting"))
		{
			CheckNull(cep);
			safe_strdup(m->troubleshooting, cep->value);
		}
		else if (!strcmp(cep->name, "documentation"))
		{
			CheckNull(cep);
			safe_strdup(m->documentation, cep->value);
		}
		else if (!strcmp(cep->name, "min-unrealircd-version"))
		{
			CheckNull(cep);
			safe_strdup(m->min_unrealircd_version, cep->value);
		}
		else if (!strcmp(cep->name, "max-unrealircd-version"))
		{
			CheckNull(cep);
			safe_strdup(m->max_unrealircd_version, cep->value);
		}
		else if (!strcmp(cep->name, "post-install-text"))
		{
			if (cep->items)
			{
				ConfigEntry *cepp;
				for (cepp = cep->items; cepp; cepp = cepp->next)
					addmultiline(&m->post_install_text, cepp->name);
			} else {
				CheckNull(cep);
				addmultiline(&m->post_install_text, cep->value);
			}
		}
		/* unknown items are silently ignored for future compatibility */
	}

	if (!m->documentation)
	{
		config_error("%s:%d: module::documentation missing", m->name, ce->line_number);
		return 0;
	}

	if (!m->troubleshooting)
	{
		config_error("%s:%d: module::troubleshooting missing", m->name, ce->line_number);
		return 0;
	}

	if (!m->min_unrealircd_version)
	{
		config_error("%s:%d: module::min-unrealircd-version missing", m->name, ce->line_number);
		return 0;
	}

	/* max_unrealircd_version is optional */

	/* post_install_text is optional */

	return 1;
}

#undef CheckNull

int mm_parse_module_file(ManagedModule *m, char *buf, unsigned int line_offset)
{
	ConfigFile *cf;
	ConfigEntry *ce;

	cf = config_parse_with_offset(m->name, buf, line_offset);
	if (!cf)
		return 0; /* eg: parse errors */

	/* Parse the module { } block (only one!) */
	for (ce = cf->items; ce; ce = ce->next)
	{
		if (!strcmp(ce->name, "module"))
		{
			int n = mm_module_file_config(m, ce);
			config_free(cf);
			return n;
		}
	}

	config_free(cf);
	config_error("No module block found within module source file. Contact author.\n");
	return 1;
}

#define MODULECONFIGBUFFER 16384
ManagedModule *mm_parse_module_c_file(char *modulename, char *fname)
{
	char buf[1024];
	FILE *fd;
	ParseModuleHeaderStage parse_module_header = PMH_STAGE_LOOKING;
	ParseModuleConfigStage parse_module_config = PMC_STAGE_LOOKING;
	char *moduleconfig = NULL;
	int linenr = 0, module_config_start_line = 0;
	char module_header_name[128];
	char module_header_version[64];
	char module_header_description[256];
	char module_header_author[128];
	ManagedModule *m = NULL;

	*module_header_name = *module_header_version = *module_header_description = *module_header_author = '\0';

	if (!mm_valid_module_name(modulename))
	{
		fprintf(stderr, "Module file '%s' contains forbidden characters\n", modulename);
		return NULL;
	}

	fd = fopen(fname, "r");
	if (!fd)
	{
		fprintf(stderr, "Unable to open module '%s', file '%s': %s\n",
			modulename, fname, strerror(errno));
		return NULL;
	}

	moduleconfig = safe_alloc(MODULECONFIGBUFFER); /* should be sufficient */
	while ((fgets(buf, sizeof(buf), fd)))
	{
		linenr++;
		stripcrlf(buf);
		/* parse module header stuff: */
		switch (parse_module_header)
		{
			case PMH_STAGE_LOOKING:
				if (strstr(buf, "ModuleHeader"))
					parse_module_header = PMH_STAGE_MODULEHEADER;
				else
					break;
				/*fallthrough*/
			case PMH_STAGE_MODULEHEADER:
				if (strstr(buf, "MOD_HEADER"))
					parse_module_header = PMH_STAGE_MOD_HEADER;
				break;
			case PMH_STAGE_MOD_HEADER:
				if (parse_quoted_string(buf, module_header_name, sizeof(module_header_name)))
					parse_module_header = PMH_STAGE_GOT_NAME;
				break;
			case PMH_STAGE_GOT_NAME:
				if (parse_quoted_string(buf, module_header_version, sizeof(module_header_version)))
					parse_module_header = PMH_STAGE_GOT_VERSION;
				break;
			case PMH_STAGE_GOT_VERSION:
				if (parse_quoted_string(buf, module_header_description, sizeof(module_header_description)))
					parse_module_header = PMH_STAGE_GOT_DESCRIPTION;
				break;
			case PMH_STAGE_GOT_DESCRIPTION:
				if (parse_quoted_string(buf, module_header_author, sizeof(module_header_author)))
					parse_module_header = PMH_STAGE_GOT_AUTHOR;
				break;
			default:
				break;
		}
		/* parse module config stuff: */
		switch (parse_module_config)
		{
			case PMC_STAGE_LOOKING:
				if (strstr(buf, "<<<MODULE MANAGER START>>>")){
					module_config_start_line = linenr;
					parse_module_config = PMC_STAGE_STARTED;
				}
				break;
			case PMC_STAGE_STARTED:
				if (!strstr(buf, "<<<MODULE MANAGER END>>>"))
				{
					strlcat(moduleconfig, buf, MODULECONFIGBUFFER);
					strlcat(moduleconfig, "\n", MODULECONFIGBUFFER);
				} else
				{
					parse_module_config = PMC_STAGE_FINISHED;
				}
				break;
			default:
				/* Nothing to be done anymore */
				break;
		}
	}
	fclose(fd);

	if (!*module_header_name || !*module_header_version ||
	    !*module_header_description || !*module_header_author)
	{
		fprintf(stderr, "Error parsing module header in %s\n", modulename);
		safe_free(moduleconfig);
		return NULL;
	}

	if (strcmp(module_header_name, modulename))
	{
		fprintf(stderr, "ERROR: Mismatch in module name in header (%s) and filename (%s)\n",
			module_header_name, modulename);
		safe_free(moduleconfig);
		return NULL;
	}

	if (!*moduleconfig)
	{
		fprintf(stderr, "ERROR: Module does not contain module config data (<<<MODULE MANAGER START>>>)\n"
		                "This means it is not meant to be managed by the module manager\n");
		safe_free(moduleconfig);
		return NULL;
	}

	/* Fill in the fields from MOD_HEADER() */
	m = safe_alloc(sizeof(ManagedModule));
	safe_strdup(m->name, module_header_name);
	safe_strdup(m->version, module_header_version);
	safe_strdup(m->description, module_header_description);
	safe_strdup(m->author, module_header_author);

	if (!mm_parse_module_file(m, moduleconfig, module_config_start_line))
	{
		fprintf(stderr, "ERROR: Problem with module manager data block within the %s module C source file.\n"
		                "You are suggested to contact the module author and paste the above to him/her\n",
		                m->name);
		free_managed_module(m);
		safe_free(moduleconfig);
		return NULL;
	}
	
	safe_free(moduleconfig);
	return m;
}

void print_documentation(void)
{
	fprintf(stderr, "See https://www.unrealircd.org/docs/Module_manager for more information.\n");
}

char *mm_sourceslist_file(void)
{
	static char buf1[512], buf2[512];
	snprintf(buf1, sizeof(buf1), "%s/modules.sources.list", CONFDIR);
	if (!file_exists(buf1))
	{
		/* Possibly UnrealIRCd is not installed yet, so use this one */
		snprintf(buf2, sizeof(buf2), "%s/doc/conf/modules.sources.list", BUILDDIR);
		if (!file_exists(buf2))
		{
			fprintf(stderr, "ERROR: Neither '%s' nor '%s' exist.\n"
			                "No module repositories configured.\n",
			                buf1, buf2);
			print_documentation();
			exit(-1);
		}
		return buf2;
	}
	return buf1;
}

/** Free a managed module struct */
void free_managed_module(ManagedModule *m)
{
	safe_free(m->repo_url);
	safe_free(m->name);
	safe_free(m->source);
	safe_free(m->sha256sum);
	safe_free(m->version);
	safe_free(m->author);
	safe_free(m->troubleshooting);
	safe_free(m->documentation);
	safe_free(m->min_unrealircd_version);
	safe_free(m->max_unrealircd_version);
	safe_free(m->description);
	freemultiline(m->post_install_text);
	safe_free(m);
}

/** Check for valid module name */
int mm_valid_module_name(char *name)
{
	char *p;

	if (strncmp(name, "third/", 6))
		return 0;
	name += 6;
	if (strstr(name, ".."))
		return 0;
	for (p = name; *p; p++)
		if (!isalnum(*p) && !strchr("._-", *p))
			return 0;
	return 1;
}

#undef CheckNull
#define CheckNull(x) if ((!(x)->value) || (!(*((x)->value)))) { config_error("%s:%i: missing parameter", repo_url, (x)->line_number); goto fail_mm_repo_module_config; }

/** Parse a module { } line from a repository */
ManagedModule *mm_repo_module_config(char *repo_url, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ManagedModule *m = safe_alloc(sizeof(ManagedModule));

	if (!ce->value)
	{
		config_error("%s:%d: module { } with no name",
			repo_url, ce->line_number);
		goto fail_mm_repo_module_config;
	}
	if (strncmp(ce->value, "third/", 6))
	{
		config_error("%s:%d: module { } name must start with: third/",
			repo_url, ce->line_number);
		goto fail_mm_repo_module_config;
	}
	if (!mm_valid_module_name(ce->value))
	{
		config_error("%s:%d: module { } with illegal name: %s",
			repo_url, ce->line_number, ce->value);
		goto fail_mm_repo_module_config;
	}
	safe_strdup(m->name, ce->value);
	safe_strdup(m->repo_url, repo_url);

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "source"))
		{
			CheckNull(cep);
			safe_strdup(m->source, cep->value);
		}
		else if (!strcmp(cep->name, "sha256sum"))
		{
			CheckNull(cep);
			safe_strdup(m->sha256sum, cep->value);
		}
		else if (!strcmp(cep->name, "version"))
		{
			CheckNull(cep);
			safe_strdup(m->version, cep->value);
		}
		else if (!strcmp(cep->name, "author"))
		{
			CheckNull(cep);
			safe_strdup(m->author, cep->value);
		}
		else if (!strcmp(cep->name, "troubleshooting"))
		{
			CheckNull(cep);
			safe_strdup(m->troubleshooting, cep->value);
		}
		else if (!strcmp(cep->name, "documentation"))
		{
			CheckNull(cep);
			safe_strdup(m->documentation, cep->value);
		}
		else if (!strcmp(cep->name, "min-unrealircd-version"))
		{
			CheckNull(cep);
			safe_strdup(m->min_unrealircd_version, cep->value);
		}
		else if (!strcmp(cep->name, "max-unrealircd-version"))
		{
			CheckNull(cep);
			safe_strdup(m->max_unrealircd_version, cep->value);
		}
		else if (!strcmp(cep->name, "description"))
		{
			CheckNull(cep);
			safe_strdup(m->description, cep->value);
		}
		else if (!strcmp(cep->name, "post-install-text"))
		{
			if (cep->items)
			{
				ConfigEntry *cepp;
				for (cepp = cep->items; cepp; cepp = cepp->next)
					addmultiline(&m->post_install_text, cepp->name);
			} else {
				CheckNull(cep);
				addmultiline(&m->post_install_text, cep->value);
			}
		}
		/* unknown items are silently ignored for future compatibility */
	}

	if (!m->source)
	{
		config_error("%s:%d: module::source missing", repo_url, ce->line_number);
		goto fail_mm_repo_module_config;
	}
	if (!m->sha256sum)
	{
		config_error("%s:%d: module::sha256sum missing", repo_url, ce->line_number);
		goto fail_mm_repo_module_config;
	}
	if (!m->version)
	{
		config_error("%s:%d: module::version missing", repo_url, ce->line_number);
		goto fail_mm_repo_module_config;
	}
	if (!m->author)
	{
		config_error("%s:%d: module::author missing", repo_url, ce->line_number);
		goto fail_mm_repo_module_config;
	}
	if (!m->documentation)
	{
		config_error("%s:%d: module::documentation missing", repo_url, ce->line_number);
		goto fail_mm_repo_module_config;
	}
	if (!m->troubleshooting)
	{
		config_error("%s:%d: module::troubleshooting missing", repo_url, ce->line_number);
		goto fail_mm_repo_module_config;
	}
	if (!m->min_unrealircd_version)
	{
		config_error("%s:%d: module::min-unrealircd-version missing", repo_url, ce->line_number);
		goto fail_mm_repo_module_config;
	}
	/* max_unrealircd_version is optional */
	if (!m->description)
	{
		config_error("%s:%d: module::description missing", repo_url, ce->line_number);
		goto fail_mm_repo_module_config;
	}
	/* post_install_text is optional */

	return m;

fail_mm_repo_module_config:
	free_managed_module(m);
	return NULL;
}

#undef CheckNull

int mm_parse_repo_db(char *url, char *filename)
{
	ConfigFile *cf;
	ConfigEntry *ce;
	ManagedModule *m;

	cf = config_load(filename, url);
	if (!cf)
		return 0; /* eg: parse errors */

	for (ce = cf->items; ce; ce = ce->next)
	{
		if (!strcmp(ce->name, "module"))
		{
			m = mm_repo_module_config(url, ce);
			if (!m)
			{
				config_free(cf);
				return 0;
			}
			AddListItem(m, managed_modules);
		}
	}
	config_free(cf);
	return 1;
}

int mm_refresh_repository(void)
{
	char *sourceslist = mm_sourceslist_file();
	FILE *fd;
	char buf[512];
	char *tmpfile;
	int linenr = 0;
	int success = 0;
	int numrepos = 0;

	if (!file_exists(TMPDIR))
	{
		(void)mkdir(TMPDIR, S_IRUSR|S_IWUSR|S_IXUSR); /* Create the tmp dir, if it doesn't exist */
		if (!file_exists(TMPDIR))
		{
			/* This is possible if the directory structure does not exist,
			 * eg if ~/unrealircd does not exist at all then ~/unrealircd/tmp
			 * cannot be mkdir'ed either.
			 */
			fprintf(stderr, "ERROR: %s does not exist (yet?), cannot use module manager\n", TMPDIR);
			fprintf(stderr, "       This can only happen if you did not use ./Config or if you rm -rf'ed after running ./Config.\n");
			exit(-1);
		}
	}

	printf("Reading module repository list from '%s'...\n", mm_sourceslist_file());
	fd = fopen(sourceslist, "r");
	if (!fd)
	{
		fprintf(stderr, "ERROR: Could not open '%s': %s\n", sourceslist, strerror(errno));
		return 0;
	}

	while ((fgets(buf, sizeof(buf), fd)))
	{
		char *line = buf;
		linenr++;
		stripcrlf(line);
		/* Skip whitespace */
		while (*line == ' ')
			line++;
		/* Skip empty lines and ones that start with a hash mark (#) */
		if (!*line || (*line == '#'))
			continue;
		if (strncmp(line, "https://", 8))
		{
			fprintf(stderr, "ERROR in %s on line %d: URL should start with https://",
				sourceslist, linenr);
			fclose(fd);
			return 0;
		}
		printf("Checking module repository %s...\n", line);
		numrepos++;
		tmpfile = unreal_mktemp(TMPDIR, "mm");
		if (mm_http_request(line, tmpfile, 1))
		{
			if (!mm_parse_repo_db(line, tmpfile))
			{
				fclose(fd);
				return 0;
			}
			success++;
		}
	}
	fclose(fd);

	if (numrepos == 0)
	{
		fprintf(stderr, "ERROR: No repositories listed in module repository list. "
		                "Did you remove the default UnrealIRCd repository?\n"
		                "All commands, except for './unrealircd module uninstall third/name-of-module', are unavailable.\n");
		return 0;
	}

	return success ? 1 : 0;
}

#define COLUMN_STATUS	0
#define COLUMN_NAME	1
#define COLUMN_VERSION	2

void mm_list_print(char *status, char *name, char *version, char *description, int largest_column[3])
{
	int padstatus = MAX(largest_column[COLUMN_STATUS] - strlen(status), 0);
	int padname = MAX(largest_column[COLUMN_NAME] - strlen(name), 0);
	int padversion = MAX(largest_column[COLUMN_VERSION] - strlen(version), 0);

	printf("| %s%*s | %s%*s | %s%*s | %s\n",
	       status,
	       padstatus, "",
	       name,
	       padname, "",
	       version,
	       padversion, "",
	       description);
}

int mm_check_module_compatibility(ManagedModule *m)
{
	if (strchr(m->min_unrealircd_version, '*'))
	{
		/* By wildcard, eg: "5.*" */
		if (!match_simple(m->min_unrealircd_version, VERSIONONLY))
			return 0;
	} else
	{
		/* By strcmp, eg: "5.0.0" */
		if (strnatcasecmp(m->min_unrealircd_version, VERSIONONLY) > 0)
			return 0;
	}
	if (m->max_unrealircd_version)
	{
		if (strchr(m->max_unrealircd_version, '*'))
		{
			/* By wildcard, eg: "5.*" */
			if (!match_simple(m->max_unrealircd_version, VERSIONONLY))
				return 0;
		} else
		{
			/* By strcmp, eg: "5.0.5" */
			if (strnatcasecmp(m->max_unrealircd_version, VERSIONONLY) <= 0)
				return 0;
		}
	}
	return 1;
}

#define MMMS_INSTALLED		0x0001
#define MMMS_UPGRADE_AVAILABLE	0x0002
#define MMMS_UNAVAILABLE	0x0004

int mm_get_module_status(ManagedModule *m)
{
	FILE *fd;
	char fname[512];
	const char *our_sha256sum;

	snprintf(fname, sizeof(fname), "%s/src/modules/%s.c", BUILDDIR, m->name);
	if (!file_exists(fname))
	{
		if (!mm_check_module_compatibility(m))
			return MMMS_UNAVAILABLE;
		return 0;
	}

	our_sha256sum = sha256sum_file(fname);
	if (!strcasecmp(our_sha256sum, m->sha256sum))
	{
		return MMMS_INSTALLED;
	} else {
		if (!mm_check_module_compatibility(m))
			return MMMS_INSTALLED|MMMS_UNAVAILABLE;
		return MMMS_INSTALLED|MMMS_UPGRADE_AVAILABLE;
	}

	return 0;
}

char *mm_get_module_status_string(ManagedModule *m)
{
	int status = mm_get_module_status(m);
	if (status == 0)
		return "";
	else if (status == MMMS_UNAVAILABLE)
		return "unav";
	else if (status == MMMS_INSTALLED)
		return "inst";
	else if (status == (MMMS_INSTALLED|MMMS_UNAVAILABLE))
		return "inst/UNAV";
	else if (status == (MMMS_INSTALLED|MMMS_UPGRADE_AVAILABLE))
		return "inst/UPD";
	return "UNKNOWN?";
}

char *mm_get_module_status_string_long(ManagedModule *m)
{
	int status = mm_get_module_status(m);
	if (status == 0)
		return "Not installed";
	else if (status == MMMS_UNAVAILABLE)
		return "Unavailable for your UnrealIRCd version";
	else if (status == MMMS_INSTALLED)
		return "Installed and up to date";
	else if (status == (MMMS_INSTALLED|MMMS_UNAVAILABLE))
		return "Installed, an upgrade is available but not for your UnrealIRCd version";
	else if (status == (MMMS_INSTALLED|MMMS_UPGRADE_AVAILABLE))
		return "Installed, upgrade available";
	return "UNKNOWN?";
}

/** Find a module by name, return NULL if not found. */
ManagedModule *mm_find_module(char *name)
{
	ManagedModule *m;

	for (m = managed_modules; m; m = m->next)
		if (!strcasecmp(name, m->name))
			return m;
	return NULL;
}

/** Count the unknown modules (untracked modules) */
int count_unknown_modules(void)
{
	DIR *fd;
	struct dirent *dir;
	int count = 0;
	char dirname[512];

	snprintf(dirname, sizeof(dirname), "%s/src/modules/third", BUILDDIR);

	fd = opendir(dirname);
	if (fd)
	{
		while ((dir = readdir(fd)))
		{
			char *fname = dir->d_name;
			if (filename_has_suffix(fname, ".c"))
			{
				char modname[512], *p;
				snprintf(modname, sizeof(modname), "third/%s", filename_strip_suffix(fname, ".c"));
				if (!mm_find_module(modname))
					count++;
			}
		}
		closedir(fd);
	}
	return count;
}

void mm_list(char *searchname)
{
	ManagedModule *m;
	int largest_column[3];
	int padname;
	int padversion;
	struct dirent *dir;
	DIR *fd;
	char dirname[512];
	char *status;
	int first_unknown = 1;

	if (searchname)
		printf("Searching for '%s' in names of all available modules...\n", searchname);

	memset(&largest_column, 0, sizeof(largest_column));
	largest_column[COLUMN_STATUS] = strlen("inst/UNAV");
	largest_column[COLUMN_NAME] = strlen("Name:");
	largest_column[COLUMN_VERSION] = strlen("Version:");

	for (m = managed_modules; m; m = m->next)
	{
		if (strlen(m->name) > largest_column[COLUMN_NAME])
			largest_column[COLUMN_NAME] = strlen(m->name);
		if (strlen(m->version) > largest_column[COLUMN_VERSION])
			largest_column[COLUMN_VERSION] = strlen(m->version);
	}
	/* We try to produce neat output, but not at all costs */
	if (largest_column[COLUMN_NAME] > 32)
		largest_column[COLUMN_NAME] = 32;
	if (largest_column[COLUMN_VERSION] > 16)
		largest_column[COLUMN_VERSION] = 16;

	mm_list_print("Status:", "Name:", "Version:", "Description:", largest_column);
	printf("|=======================================================================================\n");

	for (m = managed_modules; m; m = m->next)
	{
		if (searchname && !strstr(m->name, searchname))
			continue;

		status = mm_get_module_status_string(m);
		mm_list_print(status, m->name, m->version, m->description, largest_column);
	}

	snprintf(dirname, sizeof(dirname), "%s/src/modules/third", BUILDDIR);
	fd = opendir(dirname);
	if (fd)
	{
		while ((dir = readdir(fd)))
		{
			char *fname = dir->d_name;
			if (filename_has_suffix(fname, ".c"))
			{
				char modname[512], *p;
				snprintf(modname, sizeof(modname), "third/%s", filename_strip_suffix(fname, ".c"));
				if (searchname && !strstr(searchname, modname))
					continue;
				if (!mm_find_module(modname))
				{
					if (first_unknown)
					{
						printf("|---------------------------------------------------------------------------------------\n");
						first_unknown = 0;
					}
					mm_list_print("UNKNOWN", modname, "", "", largest_column);
				}
			}
		}
		closedir(fd);
	}

	printf("|=======================================================================================\n");

	printf("\nStatus column legend:\n"
	       "          : not installed\n"
	       "inst      : module installed\n"
	       "inst/UPD  : module installed, upgrade available (latest version differs from yours)\n"
	       "unav      : module not available for your UnrealIRCd version\n"
	       "inst/UNAV : module installed, upgrade exists but is not available for your UnrealIRCd version (too old UnrealIRCd version?)\n"
	       "UNKNOWN   : module does not exist in any repository (perhaps you installed it manually?), module will be left untouched\n");

	printf("\nFor more information about a particular module, use './unrealircd module info name-of-module'\n\n");
	print_documentation();
}

int mm_compile(ManagedModule *m, char *tmpfile, int test)
{
	char newpath[512];
	char cmd[512];
	const char *basename;
	char *p;
	FILE *fd;
	char buf[512];
	int n;

	if (test)
		printf("Test compiling %s...\n", m->name);
	else
		printf("Compiling %s...\n", m->name);

	basename = unreal_getfilename(test ? tmpfile : m->name);
	snprintf(newpath, sizeof(newpath), "%s/src/modules/third/%s%s", BUILDDIR, basename, test ? "" : ".c");
	if (!test)
	{
		/* If the file already exists then we are upgrading.
		 * It's a good idea to backup the file rather than
		 * just delete it. Perhaps the user had local changes
		 * he/she wished to preserve and accidently went
		 * through the upgrade procedure.
		 */
		char backupfile[512];
		snprintf(backupfile, sizeof(backupfile), "%s.bak", newpath);
		unlink(backupfile);
		(void)rename(newpath, backupfile);
	}
	if (!unreal_copyfileex(tmpfile, newpath, 0))
		return 0;

	snprintf(cmd, sizeof(cmd),
	         "cd \"%s\"; make custommodule MODULEFILE=\"%s\"",
	         BUILDDIR,
	         filename_strip_suffix(basename, ".c")
	         );
	fd = popen(cmd, "r");
	if (!fd)
	{
		fprintf(stderr, "ERROR: Could not issue command: %s\n", cmd);
		unlink(newpath);
		return 0;
	}
	while((fgets(buf, sizeof(buf), fd)))
	{
		printf("%s", buf);
	}
	n = pclose(fd);

	if (test)
	{
		/* Remove the XXXXXXX.modname.c file */
		unlink(newpath);
		/* Remove the XXXXXXX.modname.so file */
		newpath[strlen(newpath)-2] = '\0'; // cut off .c
		strlcat(newpath, ".so", sizeof(newpath));
		unlink(newpath);
	}

	if (WIFEXITED(n) && (WEXITSTATUS(n) == 0))
		return 1;

	fprintf(stderr, "ERROR: Compile errors encountered while compiling module '%s'\n"
	                "You are suggested to contact the author (%s) of this module:\n%s\n",
	                m->name, m->author, m->troubleshooting);
	return 0;
}

/** Actually download and install the module.
 * This assumes compatibility checks have already been done.
 */
void mm_install_module(ManagedModule *m)
{
	const char *basename = unreal_getfilename(m->source);
	char *tmpfile;
	const char *sha256;

	if (!basename)
		basename = "mod.c";
	tmpfile = unreal_mktemp(TMPDIR, basename);

	printf("Downloading %s from %s...\n", m->name, m->source);
	if (!mm_http_request(m->source, tmpfile, 1))
	{
		fprintf(stderr, "Repository %s seems to list a module file that cannot be retrieved (%s).\n", m->repo_url, m->source);
		fprintf(stderr, "Fatal error encountered. Contact %s: %s\n", m->author, m->troubleshooting);
		exit(-1);
	}

	sha256 = sha256sum_file(tmpfile);
	if (!sha256)
	{
		fprintf(stderr, "ERROR: Temporary file '%s' has disappeared -- strange\n", tmpfile);
		fprintf(stderr, "Fatal error encountered. Check for errors above. Perhaps try running the command again?\n");
		exit(-1);
	}
	if (strcasecmp(sha256, m->sha256sum))
	{
		fprintf(stderr, "ERROR: SHA256 Checksum mismatch\n"
		                "Expected (value in repository list): %s\n"
		                "Received (value of downloaded file): %s\n",
		                m->sha256sum, sha256);
		fprintf(stderr, "Fatal error encountered, see above. Try running the command again in 5-10 minutes.\n"
		                "If the issue persists, contact the repository manager of %s\n",
		                m->repo_url);
		exit(-1);
	}
	if (!mm_compile(m, tmpfile, 1))
	{
		fprintf(stderr, "Fatal error encountered, see above.\n");
		exit(-1);
	}

	if (!mm_compile(m, tmpfile, 0))
	{
		fprintf(stderr, "The test compile went OK earlier, but the final compile did not. BAD!!\n");
		exit(-1);
	}
	printf("Module %s compiled successfully\n", m->name);
}

/** Uninstall a module.
 * This function takes a string rather than a ManagedModule
 * because it also allows uninstalling of unmanaged (local) modules.
 */
void mm_uninstall_module(char *modulename)
{
	struct dirent *dir;
	DIR *fd;
	char dirname[512], fullname[512];
	int found = 0;

	snprintf(dirname, sizeof(dirname), "%s/src/modules/third", BUILDDIR);
	fd = opendir(dirname);
	if (fd)
	{
		while ((dir = readdir(fd)))
		{
			char *fname = dir->d_name;
			if (filename_has_suffix(fname, ".c") || filename_has_suffix(fname, ".so") || filename_has_suffix(fname, ".dll"))
			{
				char modname[512], *p;
				snprintf(modname, sizeof(modname), "third/%s", filename_strip_suffix(fname, NULL));
				if (!strcasecmp(modname, modulename))
				{
					found = 1;
					snprintf(fullname, sizeof(fullname), "%s/%s", dirname, fname);
					//printf("Deleting '%s'\n", fullname);
					unlink(fullname);
				}
			}
		}
		closedir(fd);
	}

	if (!found)
	{
		fprintf(stderr, "ERROR: Module '%s' is not installed, so can't uninstall.\n", modulename);
		exit(-1);
	}

	printf("Module '%s' uninstalled successfully\n", modulename);
}

void mm_make_install(void)
{
	char cmd[512];
	int n;
	if (no_make_install)
		return;
	printf("Running 'make install'...\n");
	snprintf(cmd, sizeof(cmd), "cd \"%s\"; make install 1>/dev/null 2>&1", BUILDDIR);
	n = system(cmd);
}

void mm_install(int argc, char *args[], int upgrade)
{
	ManagedModule *m;
	MultiLine *l;
	char *name = args[1];
	int status;

	if (!name)
	{
		fprintf(stderr, "ERROR: Use: module install third/name-of-module\n");
		exit(-1);
	}

	if (strncmp(name, "third/", 6))
	{
		fprintf(stderr, "ERROR: Use: module install third/name-of-module\nYou must prefix the modulename with third/\n");
		exit(-1);
	}

	m = mm_find_module(name);
	if (!m)
	{
		fprintf(stderr, "ERROR: Module '%s' not found\n", name);
		exit(-1);
	}
	status = mm_get_module_status(m);
	if (status == MMMS_UNAVAILABLE)
	{
		fprintf(stderr, "ERROR: Module '%s' exists, but is not compatible with your UnrealIRCd version:\n"
		                "Your UnrealIRCd version  : %s\n"
		                "Minimum version required : %s\n",
		                name,
		                VERSIONONLY,
		                m->min_unrealircd_version);
		if (m->max_unrealircd_version)
			fprintf(stderr, "Maximum version          : %s\n", m->max_unrealircd_version);
		exit(-1);
	}
	if (upgrade && (status == MMMS_INSTALLED))
	{
		/* If updating, and we are already on latest version, then don't upgrade */
		printf("Module %s is the latest version, no upgrade needed\n", m->name);
	}
	mm_install_module(m);
	mm_make_install();
	if (m->post_install_text)
	{
		printf("Post-installation information for %s from the author:\n", m->name);
		printf("---\n");
		for (l = m->post_install_text; l; l = l->next)
			printf(" %s\n", l->line);
		printf("---\n");
	} else {
		printf("Don't forget to add a 'loadmodule' line for the module and rehash\n");
	}
}

void mm_uninstall(int argc, char *args[])
{
	ManagedModule *m;
	char *name = args[1];

	if (!name)
	{
		fprintf(stderr, "ERROR: Use: module uninstall third/name-of-module\n");
		exit(-1);
	}

	if (strncmp(name, "third/", 6))
	{
		fprintf(stderr, "ERROR: Use: module uninstall third/name-of-module\nYou must prefix the modulename with third/\n");
		exit(-1);
	}

	mm_uninstall_module(name);
	mm_make_install();
	exit(0);
}

void mm_upgrade(int argc, char *args[])
{
	ManagedModule *m;
	char *name = args[1];
	int upgraded = 0;
	int uptodate_already = 0;
	int update_unavailable = 0;
	int i = 1, n;

	if (args[i] && !strcmp(args[i], "--no-install"))
	{
		no_make_install = 1;
		i++;
	}

	name = args[i];
	if (name)
	{
		// TODO: First check if it needs an upgrade? ;)
		mm_install(argc, args, 1);
		exit(0);
	}

	/* Without arguments means: check all installed modules */
	for (m = managed_modules; m; m = m->next)
	{
		int status = mm_get_module_status(m);
		if (status == (MMMS_INSTALLED|MMMS_UPGRADE_AVAILABLE))
		{
			args[1] = m->name;
			mm_install(1, args, 1);
			upgraded++;
		} else
		if (status == MMMS_INSTALLED)
		{
			uptodate_already++;
		} else
		if (status == (MMMS_INSTALLED|MMMS_UNAVAILABLE))
		{
			update_unavailable++;
		}
	}
	printf("All actions were successful. %d module(s) upgraded, %d already up-to-date\n",
		upgraded, uptodate_already);
	if (update_unavailable)
		printf("%d module(s) have updates but not for your UnrealIRCd version\n", update_unavailable);
	if ((n = count_unknown_modules()))
		printf("%d module(s) are unknown/untracked\n", n);

	printf("For more details, you can always run ./unrealircd module list\n");
}

void mm_info(int argc, char *args[])
{
	ManagedModule *m;
	MultiLine *l;
	char *name = args[1];

	if (!name)
	{
		fprintf(stderr, "ERROR: Use: unrealircd module info name-of-module\n");
		exit(-1);
	}

	m = mm_find_module(name);
	if (!m)
	{
		// TODO: we should probably be a bit more specific if the module exists locally (UNAV) */
		fprintf(stderr, "ERROR: Module '%s' not found in any repository\n", name);
		exit(-1);
	}
	printf("Name:                    %s\n"
	       "Version:                 %s\n"
	       "Description:             %s\n"
	       "Author:                  %s\n"
	       "Documentation:           %s\n"
	       "Troubleshooting:         %s\n"
	       "Source:                  %s\n"
	       "Min. UnrealIRCd version: %s\n",
	       m->name,
	       m->version,
	       m->description,
	       m->author,
	       m->documentation,
	       m->troubleshooting,
	       m->source,
	       m->min_unrealircd_version);
	if (m->max_unrealircd_version)
		printf("Min. UnrealIRCd version: %s\n", m->max_unrealircd_version);
	printf("Status:                  %s\n", mm_get_module_status_string_long(m));
	if (m->post_install_text)
	{
		printf("------  Post-installation text  ------\n");
		for (l = m->post_install_text; l; l = l->next)
			printf(" %s\n", l->line);
		printf("------ End of post-install text ------\n");
	}
}

void mm_usage(void)
{
	fprintf(stderr, "Use any of the following actions:\n"
	                "unrealircd module list                     List all the available and installed modules\n"
	                "unrealircd module info name-of-module      Show more information about the module\n"
	                "unrealircd module install name-of-module   Install the specified module\n"
	                "unrealircd module uninstall name-of-module Uninstall the specified module\n"
	                "unrealircd module upgrade name-of-module   Upgrade the specified module (if needed)\n"
	                "unrealircd module upgrade                  Upgrade all modules (if needed)\n"
	                "unrealircd module generate-repository      Generate a repository index (you are\n"
	                "                                           unlikely to need this, only for repo admins)\n");
	print_documentation();
	exit(-1);
}

void print_md_block(FILE *fdo, ManagedModule *m)
{
	fprintf(fdo, "module \"%s\"\n{\n", m->name);
	fprintf(fdo, "\tdescription \"%s\";\n", unreal_add_quotes(m->description));
	fprintf(fdo, "\tversion \"%s\";\n", unreal_add_quotes(m->version));
	fprintf(fdo, "\tauthor \"%s\";\n", unreal_add_quotes(m->author));
	fprintf(fdo, "\tdocumentation \"%s\";\n", unreal_add_quotes(m->documentation));
	fprintf(fdo, "\ttroubleshooting \"%s\";\n", unreal_add_quotes(m->troubleshooting));
	fprintf(fdo, "\tsource \"%s\";\n", unreal_add_quotes(m->source));
	fprintf(fdo, "\tsha256sum \"%s\";\n", unreal_add_quotes(m->sha256sum));
	fprintf(fdo, "\tmin-unrealircd-version \"%s\";\n", unreal_add_quotes(m->min_unrealircd_version));
	if (m->max_unrealircd_version)
		fprintf(fdo, "\tmax-unrealircd-version \"%s\";\n", unreal_add_quotes(m->max_unrealircd_version));
	if (m->post_install_text)
	{
		MultiLine *l;
		fprintf(fdo, "\tpost-install-text\n"
		             "\t{\n");
		for (l = m->post_install_text; l; l = l->next)
			fprintf(fdo, "\t\t\"%s\";\n", unreal_add_quotes(l->line));
		fprintf(fdo, "\t}\n");
	}
	fprintf(fdo, "}\n\n");
}

void mm_generate_repository_usage(void)
{
	fprintf(stderr, "Usage: ./unrealircd module generate-repository <url base path> <directory-with-modules> <name of output file> [optional-minimum-version-filter]\n");
	fprintf(stderr, "For example: ./unrealircd module generate-repository https://www.unrealircd.org/modules/ src/modules/third modules.lst\n");
}

void mm_generate_repository(int argc, char *args[])
{
	DIR *fd;
	struct dirent *dir;
	int count = 0;
	char *urlbasepath;
	char *dirname;
	char *outputfile;
	char *minversion;
	char modname[128];
	char fullname[512];
	ManagedModule *m;
	FILE *fdo;

	urlbasepath = args[1];
	dirname = args[2];
	outputfile = args[3];
	minversion = args[4];

	if (!urlbasepath || !dirname || !outputfile)
	{
		mm_generate_repository_usage();
		exit(-1);
	}

	if ((strlen(urlbasepath) < 2) || (urlbasepath[strlen(urlbasepath)-1] != '/'))
	{
		fprintf(stderr, "Error: the URL base path must end with a slash\n");
		mm_generate_repository_usage();
		exit(-1);
	}

	fd = opendir(dirname);
	if (!fd)
	{
		fprintf(stderr, "Cannot open directory '%s': %s\n", dirname, strerror(errno));
		exit(-1);
	}

	fdo = fopen(outputfile, "w");
	if (!fdo)
	{
		fprintf(stderr, "Could not open file '%s' for writing: %s\n", outputfile, strerror(errno));
		exit(-1);
	}

	while ((dir = readdir(fd)))
	{
		char *fname = dir->d_name;
		if (filename_has_suffix(fname, ".c"))
		{
			int hide = 0;
			snprintf(fullname, sizeof(fullname), "%s/%s", dirname, fname);
			snprintf(modname, sizeof(modname), "third/%s", filename_strip_suffix(fname, ".c"));
			printf("Processing: %s\n", modname);
			m = mm_parse_module_c_file(modname, fullname);
			if (!m)
			{
				fprintf(stderr, "WARNING: Skipping module '%s' due to errors\n", modname);
				continue;
			}
			m->sha256sum = strdup(sha256sum_file(fullname));
			m->source = safe_alloc(512);
			snprintf(m->source, 512, "%s%s.c", urlbasepath, modname + 6);
			/* filter */
			if (minversion && m->min_unrealircd_version && strncmp(minversion, m->min_unrealircd_version, strlen(minversion)))
				hide = 1;
			/* /filter */
			if (!hide)
				print_md_block(fdo, m);
			free_managed_module(m);
			m = NULL;
		}
	}
	closedir(fd);
	fclose(fdo);
}

void mm_parse_c_file(int argc, char *args[])
{
	char *fullname = args[1];
	const char *basename;
	char modname[256];
	ManagedModule *m;

	if (!fullname)
	{
		fprintf(stderr, "Usage: ./unrealircd module parse-c-file path/to/file.c\n");
		exit(-1);
	}
	if (!file_exists(fullname))
	{
		fprintf(stderr, "ERROR: Unable to open C file '%s'\n", fullname);
		exit(-1);
	}
	basename = unreal_getfilename(fullname);

	snprintf(modname, sizeof(modname), "third/%s", filename_strip_suffix(basename, ".c"));
	printf("Processing: %s\n", modname);
	m = mm_parse_module_c_file(modname, fullname);
	if (!m)
	{
		fprintf(stderr, "Errors encountered. See above\n");
		exit(-1);
	}
	m->sha256sum = strdup(sha256sum_file(fullname));
	m->source = strdup("...");
	print_md_block(stdout, m);
	free_managed_module(m);
	exit(0);
}

void mm_self_test(void)
{
	char name[512];

	if (!file_exists(BUILDDIR))
	{
		fprintf(stderr, "ERROR: Directory %s does not exist.\n"
				"The UnrealIRCd source is required for the module manager to work!\n",
				BUILDDIR);
		exit(-1);
	} else {
		snprintf(name, sizeof(name), "%s/src/modules/third/Makefile", BUILDDIR);
		if (!file_exists(name))
		{
			fprintf(stderr, "ERROR: Directory %s exists, but your UnrealIRCd is not compiled yet.\n"
					"You must compile your UnrealIRCd first (run './Config', then 'make install')\n",
					BUILDDIR);
			exit(-1);
		}
	}
}

void modulemanager(int argc, char *args[])
{
	if (!args[0])
		mm_usage();

	mm_self_test();

	/* The following operations do not require reading
	 * of the repository list and are always available:
	 */
	if (!strcasecmp(args[0], "uninstall") ||
	    !strcasecmp(args[0], "remove"))
	{
		mm_uninstall(argc, args);
		exit(0);
	}
	else if (!strcasecmp(args[0], "generate-repository"))
	{
		mm_generate_repository(argc, args);
		exit(0);
	}
	else if (!strcasecmp(args[0], "parse-c-file"))
	{
		mm_parse_c_file(argc, args);
		exit(0);
	}

	/* Fetch the repository list */
	if (!mm_refresh_repository())
	{
		fprintf(stderr, "Fatal error encountered\n");
		exit(-1);
	}

	if (!strcasecmp(args[0], "list"))
		mm_list(args[1]);
	else if (!strcasecmp(args[0], "info"))
		mm_info(argc, args);
	else if (!strcasecmp(args[0], "install"))
	{
		mm_install(argc, args, 0);
		fprintf(stderr, "All actions were successful.\n");
	}
	else if (!strcasecmp(args[0], "upgrade"))
		mm_upgrade(argc, args);
	else
		mm_usage();
}
#endif
