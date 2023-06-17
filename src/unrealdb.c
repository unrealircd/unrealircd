/************************************************************************
 * src/unrealdb.c
 * Functions for dealing easily with (encrypted) database files.
 * (C) Copyright 2021 Bram Matthys (Syzop)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

/** @file
 * @brief UnrealIRCd database API - see @ref UnrealDBFunctions
 */

/**
 * Read and write to database files - encrypted and unencrypted.
 * This provides functions for dealing with (encrypted) database files.
 * - File format: https://www.unrealircd.org/docs/Dev:UnrealDB
 * - KDF: Argon2: https://en.wikipedia.org/wiki/Argon2
 * - Cipher: XChaCha20 from libsodium: https://libsodium.gitbook.io/doc/advanced/stream_ciphers/xchacha20
 * @defgroup UnrealDBFunctions Database functions
 */

/* Benchmarking results:
 * On standard hardware as of 2021 speeds of 150-200 megabytes per second
 * are achieved realisticly for both reading and writing encrypted
 * database files. Of course, YMMV, depending on record sizes, CPU,
 * and I/O speeds of the underlying hardware.
 */

/* In UnrealIRCd 5.2.x we didn't write the v1 header yet for unencrypted
 * database files, this so users using unencrypted could easily downgrade
 * to version 5.0.9 and older.
 * We DO support READING encypted, unencrypted v1, and unencrypted raw (v0)
 * in 5.2.0 onwards, though.
 * Starting with UnrealIRCd 6 we now write the header, so people can only
 * downgrade from UnrealIRCd 6 to 5.2.0 and later (not 5.0.9).
 */
#define UNREALDB_WRITE_V1

/* If a key is specified, it must be this size */
#define UNREALDB_KEY_LEN	crypto_secretstream_xchacha20poly1305_KEYBYTES

/** Default 'time cost' for Argon2id */
#define UNREALDB_ARGON2_DEFAULT_TIME_COST             4
/** Default 'memory cost' for Argon2id. Note that 15 means 1<<15=32M */
#define UNREALDB_ARGON2_DEFAULT_MEMORY_COST           15
/** Default 'parallelism cost' for Argon2id. */
#define UNREALDB_ARGON2_DEFAULT_PARALLELISM_COST      2

#ifdef _WIN32
/* Ignore this warning on Windows as it is a false positive */
#pragma warning(disable : 6029)
#endif

/* Forward declarations - only used for internal (static) functions, of course */
static SecretCache *find_secret_cache(Secret *secr, UnrealDBConfig *cfg);
static void unrealdb_add_to_secret_cache(Secret *secr, UnrealDBConfig *cfg);
static void unrealdb_set_error(UnrealDB *c, UnrealDBError errcode, FORMAT_STRING(const char *pattern), ...) __attribute__((format(printf,3,4)));

UnrealDBError unrealdb_last_error_code;
static char *unrealdb_last_error_string = NULL;

/** Set error condition on unrealdb 'c' (internal function).
 * @param c		The unrealdb file handle
 * @param pattern	The format string
 * @param ...		Any parameters to the format string
 * @note this will also set c->failed=1 to prevent any further reading/writing.
 */
static void unrealdb_set_error(UnrealDB *c, UnrealDBError errcode, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	char buf[512];
	va_start(vl, pattern);
	vsnprintf(buf, sizeof(buf), pattern, vl);
	va_end(vl);
	if (c)
	{
		c->error_code = errcode;
		safe_strdup(c->error_string, buf);
	}
	unrealdb_last_error_code = errcode;
	safe_strdup(unrealdb_last_error_string, buf);
}

/** Free a UnrealDB struct (internal function). */
static void unrealdb_free(UnrealDB *c)
{
	unrealdb_free_config(c->config);
	safe_free(c->error_string);
	safe_free_sensitive(c);
}

static int unrealdb_kdf(UnrealDB *c, Secret *secr)
{
	if (c->config->kdf != UNREALDB_KDF_ARGON2ID)
	{
		unrealdb_set_error(c, UNREALDB_ERROR_INTERNAL, "Unknown KDF 0x%x", (int)c->config->kdf);
		return 0;
	}
	/* Need to run argon2 to generate key */
	if (argon2id_hash_raw(c->config->t_cost,
			      1 << c->config->m_cost,
			      c->config->p_cost,
			      secr->password, strlen(secr->password),
			      c->config->salt, c->config->saltlen,
			      c->config->key, c->config->keylen) != ARGON2_OK)
	{
		/* out of memory or some other very unusual error */
		unrealdb_set_error(c, UNREALDB_ERROR_INTERNAL, "Could not generate argon2 hash - out of memory or something weird?");
		return 0;
	}
	return 1;
}

/**
 * @addtogroup UnrealDBFunctions
 * @{
 */

/** Get the error string for last failed unrealdb operation.
 * @returns The error string
 * @note Use the return value only for displaying of errors
 *       to the end-user.
 *       For programmatically checking of error conditions
 *       use unrealdb_get_error_code() instead.
 */
const char *unrealdb_get_error_string(void)
{
	return unrealdb_last_error_string;
}

/** Get the error code for last failed unrealdb operation
 * @returns An UNREAL_DB_ERROR_*
 */
UnrealDBError unrealdb_get_error_code(void)
{
	return unrealdb_last_error_code;
}

/** Open an unrealdb file.
 * @param filename	The filename to open
 * @param mode		Either UNREALDB_MODE_READ or UNREALDB_MODE_WRITE
 * @param secret_block	The name of the secret xx { } block (so NOT the actual password!!)
 * @returns A pointer to a UnrealDB structure that can be used in subsequent calls for db read/writes,
 *          and finally unrealdb_close(). Or NULL in case of failure.
 * @note Upon error (NULL return value) you can call unrealdb_get_error_code() and
 *       unrealdb_get_error_string() to see the actual error.
 */
UnrealDB *unrealdb_open(const char *filename, UnrealDBMode mode, char *secret_block)
{
	UnrealDB *c = safe_alloc_sensitive(sizeof(UnrealDB));
	char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
	char buf[32]; /* don't change this */
	Secret *secr=NULL;
	SecretCache *dbcache;
	int cached = 0;
	char *err;

	errno = 0;

	if ((mode != UNREALDB_MODE_READ) && (mode != UNREALDB_MODE_WRITE))
	{
		unrealdb_set_error(c, UNREALDB_ERROR_API, "unrealdb_open request for neither read nor write");
		goto unrealdb_open_fail;
	}

	/* Do this check early, before we try to create any file */
	if (secret_block != NULL)
	{
		secr = find_secret(secret_block);
		if (!secr)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_SECRET, "Secret block '%s' not found or invalid", secret_block);
			goto unrealdb_open_fail;
		}

		if (!valid_secret_password(secr->password, &err))
		{
			unrealdb_set_error(c, UNREALDB_ERROR_SECRET, "Password in secret block '%s' does not meet complexity requirements", secr->name);
			goto unrealdb_open_fail;
		}
	}

	c->mode = mode;
	c->fd = fopen(filename, (c->mode == UNREALDB_MODE_WRITE) ? "wb" : "rb");
	if (!c->fd)
	{
		if (errno == ENOENT)
			unrealdb_set_error(c, UNREALDB_ERROR_FILENOTFOUND, "File not found: %s", strerror(errno));
		else
			unrealdb_set_error(c, UNREALDB_ERROR_IO, "Could not open file: %s", strerror(errno));
		goto unrealdb_open_fail;
	}

	if (secret_block == NULL)
	{
		if (mode == UNREALDB_MODE_READ)
		{
			/* READ: read header, if any, lots of fallback options here... */
			if (fgets(buf, sizeof(buf), c->fd))
			{
				if (str_starts_with_case_sensitive(buf, "UnrealIRCd-DB-Crypted"))
				{
					unrealdb_set_error(c, UNREALDB_ERROR_CRYPTED, "file is encrypted but no password provided");
					goto unrealdb_open_fail;
				} else
				if (!strcmp(buf, "UnrealIRCd-DB-v1"))
				{
					/* Skip over the 32 byte header, directly to the creationtime */
					if (fseek(c->fd, 32L, SEEK_SET) < 0)
					{
						unrealdb_set_error(c, UNREALDB_ERROR_HEADER, "file header too short");
						goto unrealdb_open_fail;
					}
					if (!unrealdb_read_int64(c, &c->creationtime))
					{
						unrealdb_set_error(c, UNREALDB_ERROR_HEADER, "Header is too short (A4)");
						goto unrealdb_open_fail;
					}
					/* SUCCESS = fallthrough */
				} else
				if (str_starts_with_case_sensitive(buf, "UnrealIRCd-DB")) /* any other version than v1 = not supported by us */
				{
					/* We don't support this format, so refuse clearly */
					unrealdb_set_error(c, UNREALDB_ERROR_HEADER,
							   "Unsupported version of database. Is this database perhaps created on "
							   "a new version of UnrealIRCd and are you trying to use it on an older "
							   "UnrealIRCd version? (Downgrading is not supported!)");
					goto unrealdb_open_fail;
				} else
				{
					/* Old db format, no header, seek back to beginning */
					fseek(c->fd, 0L, SEEK_SET);
					/* SUCCESS = fallthrough */
				}
			}
		} else {
#ifdef UNREALDB_WRITE_V1
			/* WRITE */
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "UnrealIRCd-DB-v1");
			if ((fwrite(buf, 1, sizeof(buf), c->fd) != sizeof(buf)) ||
			    !unrealdb_write_int64(c, TStime()))
			{
				unrealdb_set_error(c, UNREALDB_ERROR_IO, "Unable to write header (A1)");
				goto unrealdb_open_fail;
			}
#endif
		}
		safe_free(unrealdb_last_error_string);
		unrealdb_last_error_code = UNREALDB_ERROR_SUCCESS;
		return c;
	}

	c->crypted = 1;

	if (c->mode == UNREALDB_MODE_WRITE)
	{
		/* Write the:
		 * - generic header ("UnrealIRCd-DB" + some zeroes)
		 * - the salt
		 * - the crypto header
		 */
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "UnrealIRCd-DB-Crypted-v1");
		if (fwrite(buf, 1, sizeof(buf), c->fd) != sizeof(buf))
		{
			unrealdb_set_error(c, UNREALDB_ERROR_IO, "Unable to write header (1)");
			goto unrealdb_open_fail; /* Unable to write header nr 1 */
		}

		if (secr->cache && secr->cache->config)
		{
			/* Use first found cached config for this secret */
			c->config = unrealdb_copy_config(secr->cache->config);
			cached = 1;
		} else {
			/* Create a new config */
			c->config = safe_alloc(sizeof(UnrealDBConfig));
			c->config->kdf = UNREALDB_KDF_ARGON2ID;
			c->config->t_cost = UNREALDB_ARGON2_DEFAULT_TIME_COST;
			c->config->m_cost = UNREALDB_ARGON2_DEFAULT_MEMORY_COST;
			c->config->p_cost = UNREALDB_ARGON2_DEFAULT_PARALLELISM_COST;
			c->config->saltlen = UNREALDB_SALT_LEN;
			c->config->salt = safe_alloc(c->config->saltlen);
			randombytes_buf(c->config->salt, c->config->saltlen);
			c->config->cipher = UNREALDB_CIPHER_XCHACHA20;
			c->config->keylen = UNREALDB_KEY_LEN;
			c->config->key = safe_alloc_sensitive(c->config->keylen);
		}

		if (c->config->kdf == 0)
			abort();

		/* Write KDF and cipher parameters */
		if ((fwrite(&c->config->kdf, 1, sizeof(c->config->kdf), c->fd) != sizeof(c->config->kdf)) ||
		    (fwrite(&c->config->t_cost, 1, sizeof(c->config->t_cost), c->fd) != sizeof(c->config->t_cost)) ||
		    (fwrite(&c->config->m_cost, 1, sizeof(c->config->m_cost), c->fd) != sizeof(c->config->m_cost)) ||
		    (fwrite(&c->config->p_cost, 1, sizeof(c->config->p_cost), c->fd) != sizeof(c->config->p_cost)) ||
		    (fwrite(&c->config->saltlen, 1, sizeof(c->config->saltlen), c->fd) != sizeof(c->config->saltlen)) ||
		    (fwrite(c->config->salt, 1, c->config->saltlen, c->fd) != c->config->saltlen) ||
		    (fwrite(&c->config->cipher, 1, sizeof(c->config->cipher), c->fd) != sizeof(c->config->cipher)) ||
		    (fwrite(&c->config->keylen, 1, sizeof(c->config->keylen), c->fd) != sizeof(c->config->keylen)))
		{
			unrealdb_set_error(c, UNREALDB_ERROR_IO, "Unable to write header (2)");
			goto unrealdb_open_fail;
		}
		
		if (cached)
		{
#ifdef DEBUGMODE
			unreal_log(ULOG_DEBUG, "unrealdb", "DEBUG_UNREALDB_CACHE_HIT", NULL,
			           "Cache hit for '$secret_block' while writing",
			           log_data_string("secret_block", secr->name));
#endif
		} else
		{
#ifdef DEBUGMODE
			unreal_log(ULOG_DEBUG, "unrealdb", "DEBUG_UNREALDB_CACHE_MISS", NULL,
			           "Cache miss for '$secret_block' while writing, need to run argon2",
			           log_data_string("secret_block", secr->name));
#endif
			if (!unrealdb_kdf(c, secr))
			{
				/* Error already set by called function */
				goto unrealdb_open_fail;
			}
		}

		crypto_secretstream_xchacha20poly1305_init_push(&c->st, header, c->config->key);
		if (fwrite(header, 1, sizeof(header), c->fd) != sizeof(header))
		{
			unrealdb_set_error(c, UNREALDB_ERROR_IO, "Unable to write header (3)");
			goto unrealdb_open_fail; /* Unable to write crypto header */
		}
		if (!unrealdb_write_str(c, "UnrealIRCd-DB-Crypted-Now") ||
		    !unrealdb_write_int64(c, TStime()))
		{
			/* error is already set by unrealdb_write_str() */
			goto unrealdb_open_fail; /* Unable to write crypto header */
		}
		if (!cached)
			unrealdb_add_to_secret_cache(secr, c->config);
	} else
	{
		char *validate = NULL;
		
		/* Read file header */
		if (fread(buf, 1, sizeof(buf), c->fd) != sizeof(buf))
		{
			unrealdb_set_error(c, UNREALDB_ERROR_NOTCRYPTED, "Not a crypted file (file too small)");
			goto unrealdb_open_fail; /* Header too short */
		}
		if (strncmp(buf, "UnrealIRCd-DB-Crypted-v1", 24))
		{
			unrealdb_set_error(c, UNREALDB_ERROR_NOTCRYPTED, "Not a crypted file");
			goto unrealdb_open_fail; /* Invalid header */
		}
		c->config = safe_alloc(sizeof(UnrealDBConfig));
		if ((fread(&c->config->kdf, 1, sizeof(c->config->kdf), c->fd) != sizeof(c->config->kdf)) ||
		    (fread(&c->config->t_cost, 1, sizeof(c->config->t_cost), c->fd) != sizeof(c->config->t_cost)) ||
		    (fread(&c->config->m_cost, 1, sizeof(c->config->m_cost), c->fd) != sizeof(c->config->m_cost)) ||
		    (fread(&c->config->p_cost, 1, sizeof(c->config->p_cost), c->fd) != sizeof(c->config->p_cost)) ||
		    (fread(&c->config->saltlen, 1, sizeof(c->config->saltlen), c->fd) != sizeof(c->config->saltlen)))
		{
			unrealdb_set_error(c, UNREALDB_ERROR_HEADER, "Header is corrupt/unknown/invalid");
			goto unrealdb_open_fail;
		}
		if (c->config->kdf != UNREALDB_KDF_ARGON2ID) 
		{
			unrealdb_set_error(c, UNREALDB_ERROR_HEADER, "Header contains unknown KDF 0x%x", (int)c->config->kdf);
			goto unrealdb_open_fail;
		}
		if (c->config->saltlen > 1024)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_HEADER, "Header is corrupt (saltlen=%d)", (int)c->config->saltlen);
			goto unrealdb_open_fail; /* Something must be wrong, this makes no sense. */
		}
		c->config->salt = safe_alloc(c->config->saltlen);
		if (fread(c->config->salt, 1, c->config->saltlen, c->fd) != c->config->saltlen)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_HEADER, "Header is too short (2)");
			goto unrealdb_open_fail; /* Header too short (read II) */
		}
		if ((fread(&c->config->cipher, 1, sizeof(c->config->cipher), c->fd) != sizeof(c->config->cipher)) ||
		    (fread(&c->config->keylen, 1, sizeof(c->config->keylen), c->fd) != sizeof(c->config->keylen)))
		{
			unrealdb_set_error(c, UNREALDB_ERROR_HEADER, "Header is corrupt/unknown/invalid (3)");
			goto unrealdb_open_fail;
		}
		if (c->config->cipher != UNREALDB_CIPHER_XCHACHA20)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_HEADER, "Header contains unknown cipher 0x%x", (int)c->config->cipher);
			goto unrealdb_open_fail;
		}
		if (c->config->keylen > 1024)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_HEADER, "Header is corrupt (keylen=%d)", (int)c->config->keylen);
			goto unrealdb_open_fail; /* Something must be wrong, this makes no sense. */
		}
		c->config->key = safe_alloc_sensitive(c->config->keylen);

		dbcache = find_secret_cache(secr, c->config);
		if (dbcache)
		{
			/* Use cached key, no need to run expensive argon2.. */
			memcpy(c->config->key, dbcache->config->key, c->config->keylen);
#ifdef DEBUGMODE
			unreal_log(ULOG_DEBUG, "unrealdb", "DEBUG_UNREALDB_CACHE_HIT", NULL,
			           "Cache hit for '$secret_block' while reading",
			           log_data_string("secret_block", secr->name));
#endif
		} else {
#ifdef DEBUGMODE
			unreal_log(ULOG_DEBUG, "unrealdb", "DEBUG_UNREALDB_CACHE_MISS", NULL,
			           "Cache miss for '$secret_block' while reading, need to run argon2",
			           log_data_string("secret_block", secr->name));
#endif
			if (!unrealdb_kdf(c, secr))
			{
				/* Error already set by called function */
				goto unrealdb_open_fail;
			}
		}
		/* key is now set */
		if (fread(header, 1, sizeof(header), c->fd) != sizeof(header))
		{
			unrealdb_set_error(c, UNREALDB_ERROR_HEADER, "Header is too short (3)");
			goto unrealdb_open_fail; /* Header too short */
		}
		if (crypto_secretstream_xchacha20poly1305_init_pull(&c->st, header, c->config->key) != 0)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_PASSWORD, "Crypto error - invalid password or corrupt file");
			goto unrealdb_open_fail; /* Unusual */
		}
		/* Now to validate the key we read a simple string */
		if (!unrealdb_read_str(c, &validate))
		{
			unrealdb_set_error(c, UNREALDB_ERROR_PASSWORD, "Invalid password");
			goto unrealdb_open_fail; /* Incorrect key, probably */
		}
		if (strcmp(validate, "UnrealIRCd-DB-Crypted-Now"))
		{
			safe_free(validate);
			unrealdb_set_error(c, UNREALDB_ERROR_PASSWORD, "Invalid password");
			goto unrealdb_open_fail; /* Incorrect key, probably */
		}
		safe_free(validate);
		if (!unrealdb_read_int64(c, &c->creationtime))
		{
			unrealdb_set_error(c, UNREALDB_ERROR_HEADER, "Header is too short (4)");
			goto unrealdb_open_fail;
		}
		unrealdb_add_to_secret_cache(secr, c->config);
	}
	sodium_stackzero(1024);
	safe_free(unrealdb_last_error_string);
	unrealdb_last_error_code = UNREALDB_ERROR_SUCCESS;
	return c;

unrealdb_open_fail:
	if (c->fd)
		fclose(c->fd);
	unrealdb_free(c);
	sodium_stackzero(1024);
	return NULL;
}

/** Close an unrealdb file.
 * @param c	The struct pointing to an unrealdb file
 * @returns 1 if the final close was graceful and 0 if not (eg: out of disk space on final flush).
 *          In all cases the file handle is closed and 'c' is freed.
 * @note Upon error (NULL return value) you can call unrealdb_get_error_code() and
 *       unrealdb_get_error_string() to see the actual error.
 */
int unrealdb_close(UnrealDB *c)
{
	/* If this is file was opened for writing then flush the remaining data with a TAG_FINAL
	 * (or push a block of 0 bytes with TAG_FINAL)
	 */
	if (c->crypted && (c->mode == UNREALDB_MODE_WRITE))
	{
		char buf_out[UNREALDB_CRYPT_FILE_CHUNK_SIZE + crypto_secretstream_xchacha20poly1305_ABYTES];
		unsigned long long out_len = sizeof(buf_out);

		crypto_secretstream_xchacha20poly1305_push(&c->st, buf_out, &out_len, c->buf, c->buflen, NULL, 0, crypto_secretstream_xchacha20poly1305_TAG_FINAL);
		if (out_len > 0)
		{
			if (fwrite(buf_out, 1, out_len, c->fd) != out_len)
			{
				/* Final write failed, error condition */
				unrealdb_set_error(c, UNREALDB_ERROR_IO, "Write error: %s", strerror(errno));
				fclose(c->fd);
				unrealdb_free(c);
				return 0;
			}
		}
	}

	if (fclose(c->fd) != 0)
	{
		/* Final close failed, error condition */
		unrealdb_set_error(c, UNREALDB_ERROR_IO, "Write error: %s", strerror(errno));
		unrealdb_free(c);
		return 0;
	}

	unrealdb_free(c);
	return 1;
}

/** Test if there is something fatally wrong with the configuration of the DB file,
 * in which case we suggest to reject the /rehash or boot request.
 * This tests for "wrong password" and for "trying to open an encrypted file without providing a password"
 * which are clear configuration errors on the admin part.
 * It does NOT test for any other conditions such as missing file, corrupted file, etc.
 * since that usually needs different handling anyway, as they are I/O issues and don't
 * always have a clear solution (if any is needed at all).
 * @param filename	The filename to open
 * @param secret_block	The name of the secret xx { } block (so NOT the actual password!!)
 * @returns 1 if the password was wrong, 0 for any other error or succes.
 */
char *unrealdb_test_db(const char *filename, char *secret_block)
{
	static char buf[512];
	UnrealDB *db = unrealdb_open(filename, UNREALDB_MODE_READ, secret_block);
	if (!db)
	{
		if (unrealdb_get_error_code() == UNREALDB_ERROR_PASSWORD)
		{
			snprintf(buf, sizeof(buf), "Incorrect password specified in secret block '%s' for file %s",
				secret_block, filename);
			return buf;
		}
		if (unrealdb_get_error_code() == UNREALDB_ERROR_CRYPTED)
		{
			snprintf(buf, sizeof(buf), "File '%s' is encrypted but no secret block provided for it",
				filename);
			return buf;
		}
		return NULL;
	} else
	{
		unrealdb_close(db);
	}
	return NULL;
}

/** @} */

/** Write to an unrealdb file.
 * This code uses extra buffering to avoid writing small records
 * and wasting for example a 32 bytes encryption block for a 8 byte write request.
 * @param c		Database file open for writing
 * @param wbuf		The data to be written (plaintext)
 * @param len		The length of the data to be written
 * @note This is the internal function, api users must use one of the
 *       following functions instead:
 *       unrealdb_write_int64(), unrealdb_write_int32(), unrealdb_write_int16(),
 *       unrealdb_write_char(), unrealdb_write_str().
 */
static int unrealdb_write(UnrealDB *c, const void *wbuf, int len)
{
	char buf_out[UNREALDB_CRYPT_FILE_CHUNK_SIZE + crypto_secretstream_xchacha20poly1305_ABYTES];
	unsigned long long out_len;
	const char *buf = wbuf;

	if (c->error_code)
		return 0;

	if (c->mode != UNREALDB_MODE_WRITE)
	{
		unrealdb_set_error(c, UNREALDB_ERROR_API, "Write operation requested on a file opened for reading");
		return 0;
	}

	if (!c->crypted)
	{
		if (fwrite(buf, 1, len, c->fd) != len)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_IO, "Write error: %s", strerror(errno));
			return 0;
		}
		return 1;
	}

	do {
		if (c->buflen + len < UNREALDB_CRYPT_FILE_CHUNK_SIZE)
		{
			/* New data fits in new buffer. Then we are done with writing.
			 * This can happen both for the first block (never write)
			 * or the remainder (tail after X writes which is less than
			 * UNREALDB_CRYPT_FILE_CHUNK_SIZE, a common case)
			 */
			memcpy(c->buf + c->buflen, buf, len);
			c->buflen += len;
			break; /* Done! */
		} else
		{
			/* Fill up c->buf with UNREALDB_CRYPT_FILE_CHUNK_SIZE
			 * Note that 'av_bytes' can be 0 here if c->buflen
			 * happens to be exactly UNREALDB_CRYPT_FILE_CHUNK_SIZE,
			 * that's okay.
			 */
			int av_bytes = UNREALDB_CRYPT_FILE_CHUNK_SIZE - c->buflen;
			if (av_bytes > 0)
				memcpy(c->buf + c->buflen, buf, av_bytes);
			buf += av_bytes;
			len -= av_bytes;
		}
		if (crypto_secretstream_xchacha20poly1305_push(&c->st, buf_out, &out_len, c->buf, UNREALDB_CRYPT_FILE_CHUNK_SIZE, NULL, 0, 0) != 0)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_INTERNAL, "Failed to encrypt a block");
			return 0;
		}
		if (fwrite(buf_out, 1, out_len, c->fd) != out_len)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_IO, "Write error: %s", strerror(errno));
			return 0;
		}
		/* Buffer is now flushed for sure */
		c->buflen = 0;
	} while(len > 0);

	return 1;
}

/**
 * @addtogroup UnrealDBFunctions
 * @{
 */

/** Write a string to a database file.
 * @param c	UnrealDB file struct
 * @param x	String to be written
 * @note  This function can write a string up to 65534
 *        characters, which should be plenty for usage
 *        in UnrealIRCd.
 *        Note that 'x' can safely be NULL.
 * @returns 1 on success, 0 on failure.
 */
int unrealdb_write_str(UnrealDB *c, const char *x)
{
	uint16_t len;

	/* First, make sure the string is not too large (would be very unusual, though) */
	if (x)
	{
		int stringlen = strlen(x);
		if (stringlen >= 0xffff)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_API,
					   "unrealdb_write_str(): string has length %d, while maximum allowed is 65534",
					   stringlen);
			return 0;
		}
		len = stringlen;
	} else {
		len = 0xffff;
	}

	/* Write length to db as 16 bit integer */
	if (!unrealdb_write_int16(c, len))
		return 0;

	/* Then, write the actual string (if any), without NUL terminator. */
	if ((len > 0) && (len < 0xffff))
	{
		if (!unrealdb_write(c, x, len))
			return 0;
	}

	return 1;
}

/** Write a 64 bit integer to a database file.
 * @param c	UnrealDB file struct
 * @param t	The value to write
 * @returns 1 on success, 0 on failure.
 */
int unrealdb_write_int64(UnrealDB *c, uint64_t t)
{
#ifdef NATIVE_BIG_ENDIAN
	t = bswap_64(t);
#endif
	return unrealdb_write(c, &t, sizeof(t));
}

/** Write a 32 bit integer to a database file.
 * @param c	UnrealDB file struct
 * @param t	The value to write
 * @returns 1 on success, 0 on failure.
 */
int unrealdb_write_int32(UnrealDB *c, uint32_t t)
{
#ifdef NATIVE_BIG_ENDIAN
	t = bswap_32(t);
#endif
	return unrealdb_write(c, &t, sizeof(t));
}

/** Write a 16 bit integer to a database file.
 * @param c	UnrealDB file struct
 * @param t	The value to write
 * @returns 1 on success, 0 on failure.
 */
int unrealdb_write_int16(UnrealDB *c, uint16_t t)
{
#ifdef NATIVE_BIG_ENDIAN
	t = bswap_16(t);
#endif
	return unrealdb_write(c, &t, sizeof(t));
}

/** Write a single 8 bit character to a database file.
 * @param c	UnrealDB file struct
 * @param t	The value to write
 * @returns 1 on success, 0 on failure.
 */
int unrealdb_write_char(UnrealDB *c, char t)
{
	return unrealdb_write(c, &t, sizeof(t));
}

/** @} */

/** Read from an UnrealDB file.
 * This code deals with buffering, block reading, etc. so the caller doesn't
 * have to worry about that.
 * @param c		Database file open for reading
 * @param rbuf		The data to be read (will be plaintext)
 * @param len		The length of the data to be read
 * @note This is the internal function, api users must use one of the
 *       following functions instead:
 *       unrealdb_read_int64(), unrealdb_read_int32(), unrealdb_read_int16(),
 *       unrealdb_read_char(), unrealdb_read_str().
 */
static int unrealdb_read(UnrealDB *c, void *rbuf, int len)
{
	char buf_in[UNREALDB_CRYPT_FILE_CHUNK_SIZE + crypto_secretstream_xchacha20poly1305_ABYTES];
	unsigned long long out_len;
	unsigned char tag;
	size_t rlen;
	char *buf = rbuf;

	if (c->error_code)
		return 0;

	if (c->mode != UNREALDB_MODE_READ)
	{
		unrealdb_set_error(c, UNREALDB_ERROR_API, "Read operation requested on a file opened for writing");
		return 0;
	}

	if (!c->crypted)
	{
		rlen = fread(buf, 1, len, c->fd);
		if (rlen < len)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_IO, "Short read - premature end of file (want:%d, got:%d bytes)",
				len, (int)rlen);
			return 0;
		}
		return 1;
	}

	/* First, fill 'buf' up with what we have */
	if (c->buflen)
	{
		int av_bytes = MIN(c->buflen, len);
		memcpy(buf, c->buf, av_bytes);
		if (c->buflen - av_bytes > 0)
			memmove(c->buf, c->buf + av_bytes, c->buflen - av_bytes);
		c->buflen -= av_bytes;
		len -= av_bytes;
		if (len == 0)
			return 1; /* Request completed entirely */
		buf += av_bytes;
	}

	if (c->buflen != 0)
		abort();

	/* If we get here then we need to read some data */
	do {
		rlen = fread(buf_in, 1, sizeof(buf_in), c->fd);
		if (rlen == 0)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_IO, "Short read - premature end of file??");
			return 0;
		}
		if (crypto_secretstream_xchacha20poly1305_pull(&c->st, c->buf, &out_len, &tag, buf_in, rlen, NULL, 0) != 0)
		{
			unrealdb_set_error(c, UNREALDB_ERROR_IO, "Failed to decrypt a block - either corrupt or wrong key");
			return 0;
		}

		/* This should be impossible as this is guaranteed not to happen by libsodium */
		if (out_len > UNREALDB_CRYPT_FILE_CHUNK_SIZE)
			abort();

		if (len > out_len)
		{
			/* We eat a big block, but want more in next iteration of the loop */
			memcpy(buf, c->buf, out_len);
			buf += out_len;
			len -= out_len;
		} else {
			/* This is the only (or last) block we need, we are satisfied */
			memcpy(buf, c->buf, len);
			c->buflen = out_len - len;
			if (c->buflen > 0)
				memmove(c->buf, c->buf+len, c->buflen);
			return 1; /* Done */
		}
	} while(!feof(c->fd));

	unrealdb_set_error(c, UNREALDB_ERROR_IO, "Short read - premature end of file?");
	return 0;
}

/**
 * @addtogroup UnrealDBFunctions
 * @{
 */

/** Read a 64 bit integer from a database file.
 * @param c	UnrealDB file struct
 * @param t	The value to read
 * @returns 1 on success, 0 on failure.
 */
int unrealdb_read_int64(UnrealDB *c, uint64_t *t)
{
	if (!unrealdb_read(c, t, sizeof(uint64_t)))
		return 0;
#ifdef NATIVE_BIG_ENDIAN
	*t = bswap_64(*t);
#endif
	return 1;
}

/** Read a 32 bit integer from a database file.
 * @param c	UnrealDB file struct
 * @param t	The value to read
 * @returns 1 on success, 0 on failure.
 */
int unrealdb_read_int32(UnrealDB *c, uint32_t *t)
{
	if (!unrealdb_read(c, t, sizeof(uint32_t)))
		return 0;
#ifdef NATIVE_BIG_ENDIAN
	*t = bswap_32(*t);
#endif
	return 1;
}

/** Read a 16 bit integer from a database file.
 * @param c	UnrealDB file struct
 * @param t	The value to read
 * @returns 1 on success, 0 on failure.
 */
int unrealdb_read_int16(UnrealDB *c, uint16_t *t)
{
	if (!unrealdb_read(c, t, sizeof(uint16_t)))
		return 0;
#ifdef NATIVE_BIG_ENDIAN
	*t = bswap_16(*t);
#endif
	return 1;
}

/** Read a string from a database file.
 * @param c    UnrealDB file struct
 * @param x    Pointer to string pointer
 * @note  This function will allocate memory for the data
 *        and set the string pointer to this value.
 *        If a NULL pointer was written via write_str()
 *        then read_str() may also return a NULL pointer.
 * @returns 1 on success, 0 on failure.
 */
int unrealdb_read_str(UnrealDB *c, char **x)
{
	uint16_t len;
	size_t size;

	*x = NULL;

	if (!unrealdb_read_int16(c, &len))
		return 0;

	if (len == 0xffff)
	{
		/* Magic value meaning NULL */
		*x = NULL;
		return 1;
	}

	if (len == 0)
	{
		/* 0 means empty string */
		safe_strdup(*x, "");
		return 1;
	}

	if (len > 10000)
		return 0;

	size = len;
	*x = safe_alloc(size + 1);
	if (!unrealdb_read(c, *x, size))
	{
		safe_free(*x);
		return 0;
	}
	(*x)[len] = 0;
	return 1;
}

/** Read a single 8 bit character from a database file.
 * @param c	UnrealDB file struct
 * @param t	The value to read
 * @returns 1 on success, 0 on failure.
 */
int unrealdb_read_char(UnrealDB *c, char *t)
{
	if (!unrealdb_read(c, t, sizeof(char)))
		return 0;
	return 1;
}

/** @} */

#if 0
void fatal_error(FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	va_start(vl, pattern);
	vfprintf(stderr, pattern, vl);
	va_end(vl);
	fprintf(stderr, "\n");
	fprintf(stderr, "Exiting with failure\n");
	exit(-1);
}

void unrealdb_test_simple(void)
{
	UnrealDB *c;
	char *key = "test";
	int i;
	char *str;


	fprintf(stderr, "*** WRITE TEST ***\n");
	c = unrealdb_open("/tmp/test.db", UNREALDB_MODE_WRITE, key);
	if (!c)
		fatal_error("Could not open test db for writing: %s", strerror(errno));

	if (!unrealdb_write_str(c, "Hello world!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"))
		fatal_error("Error on write 1");
	if (!unrealdb_write_str(c, "This is a test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"))
		fatal_error("Error on write 2");
	if (!unrealdb_close(c))
		fatal_error("Error on close");
	c = NULL;
	fprintf(stderr, "Done with writing.\n\n");

	fprintf(stderr, "*** READ TEST ***\n");
	c = unrealdb_open("/tmp/test.db", UNREALDB_MODE_READ, key);
	if (!c)
		fatal_error("Could not open test db for reading: %s", strerror(errno));
	if (!unrealdb_read_str(c, &str))
		fatal_error("Error on read 1: %s", c->error_string);
	fprintf(stderr, "Got: '%s'\n", str);
	safe_free(str);
	if (!unrealdb_read_str(c, &str))
		fatal_error("Error on read 2: %s", c->error_string);
	fprintf(stderr, "Got: '%s'\n", str);
	safe_free(str);
	if (!unrealdb_close(c))
		fatal_error("Error on close");
	fprintf(stderr, "All good.\n");
}

#define UNREALDB_SPEED_TEST_BYTES 100000000
void unrealdb_test_speed(char *key)
{
	UnrealDB *c;
	int i, len;
	char *str;
	char buf[1024];
	int written = 0, read = 0;
	struct timeval tv_start, tv_end;

	fprintf(stderr, "*** WRITE TEST ***\n");
	gettimeofday(&tv_start, NULL);
	c = unrealdb_open("/tmp/test.db", UNREALDB_MODE_WRITE, key);
	if (!c)
		fatal_error("Could not open test db for writing: %s", strerror(errno));
	do {
		
		len = getrandom32() % 500;
		//gen_random_alnum(buf, len);
		for (i=0; i < len; i++)
			buf[i] = 'a';
		buf[i] = '\0';
		if (!unrealdb_write_str(c, buf))
			fatal_error("Error on writing a string of %d size", len);
		written += len + 2; /* +2 for length */
	} while(written < UNREALDB_SPEED_TEST_BYTES);
	if (!unrealdb_close(c))
		fatal_error("Error on close");
	c = NULL;
	gettimeofday(&tv_end, NULL);
	fprintf(stderr, "Done with writing: %lld usecs\n\n",
		(long long)(((tv_end.tv_sec - tv_start.tv_sec) * 1000000) + (tv_end.tv_usec - tv_start.tv_usec)));

	fprintf(stderr, "*** READ TEST ***\n");
	gettimeofday(&tv_start, NULL);
	c = unrealdb_open("/tmp/test.db", UNREALDB_MODE_READ, key);
	if (!c)
		fatal_error("Could not open test db for reading: %s", strerror(errno));
	do {
		if (!unrealdb_read_str(c, &str))
			fatal_error("Error on read at position %d/%d: %s", read, written, c->error_string);
		read += strlen(str) + 2; /* same calculation as earlier */
		safe_free(str);
	} while(read < written);
	if (!unrealdb_close(c))
		fatal_error("Error on close");
	gettimeofday(&tv_end, NULL);
	fprintf(stderr, "Done with reading: %lld usecs\n\n",
		(long long)(((tv_end.tv_sec - tv_start.tv_sec) * 1000000) + (tv_end.tv_usec - tv_start.tv_usec)));

	fprintf(stderr, "All good.\n");
}

void unrealdb_test(void)
{
	//unrealdb_test_simple();
	fprintf(stderr, "**** TESTING ENCRYPTED ****\n");
	unrealdb_test_speed("test");
	fprintf(stderr, "**** TESTING UNENCRYPTED ****\n");
	unrealdb_test_speed(NULL);
}
#endif

/** TODO: document and implement
 */
const char *unrealdb_test_secret(const char *name)
{
	// FIXME: check if exists, if not then return an error, with a nice FAQ reference etc.
	return NULL; /* no error */
}

UnrealDBConfig *unrealdb_copy_config(UnrealDBConfig *src)
{
	UnrealDBConfig *dst = safe_alloc(sizeof(UnrealDBConfig));

	dst->kdf = src->kdf;
	dst->t_cost = src->t_cost;
	dst->m_cost = src->m_cost;
	dst->p_cost = src->p_cost;
	dst->saltlen = src->saltlen;
	dst->salt = safe_alloc(dst->saltlen);
	memcpy(dst->salt, src->salt, dst->saltlen);

	dst->cipher = src->cipher;
	dst->keylen = src->keylen;
	if (dst->keylen)
	{
		dst->key = safe_alloc_sensitive(dst->keylen);
		memcpy(dst->key, src->key, dst->keylen);
	}

	return dst;
}

UnrealDBConfig *unrealdb_get_config(UnrealDB *db)
{
	return unrealdb_copy_config(db->config);
}

void unrealdb_free_config(UnrealDBConfig *c)
{
	if (!c)
		return;
	safe_free(c->salt);
	safe_free_sensitive(c->key);
	safe_free(c);
}

static int unrealdb_config_identical(UnrealDBConfig *one, UnrealDBConfig *two)
{
	/* NOTE: do not compare 'key' here or all cache lookups will fail */
	if ((one->kdf == two->kdf) &&
	    (one->t_cost == two->t_cost) &&
	    (one->m_cost == two->m_cost) &&
	    (one->p_cost == two->p_cost) &&
	    (one->saltlen == two->saltlen) &&
	    (memcmp(one->salt, two->salt, one->saltlen) == 0) &&
	    (one->cipher == two->cipher) &&
	    (one->keylen == two->keylen))
	{
		return 1;
	}
	return 0;
}

static SecretCache *find_secret_cache(Secret *secr, UnrealDBConfig *cfg)
{
	SecretCache *c;

	for (c = secr->cache; c; c = c->next)
	{
		if (unrealdb_config_identical(c->config, cfg))
		{
			c->cache_hit = TStime();
			return c;
		}
	}
	return NULL;
}

static void unrealdb_add_to_secret_cache(Secret *secr, UnrealDBConfig *cfg)
{
	SecretCache *c = find_secret_cache(secr, cfg);

	if (c)
		return; /* Entry already exists in cache */

	/* New entry, add! */
	c = safe_alloc(sizeof(SecretCache));
	c->config = unrealdb_copy_config(cfg);
	c->cache_hit = TStime();
	AddListItem(c, secr->cache);
}

#ifdef DEBUGMODE
#define UNREALDB_EXPIRE_SECRET_CACHE_AFTER	1200
#else
#define UNREALDB_EXPIRE_SECRET_CACHE_AFTER	86400
#endif

/** Expire cached secret entries (previous Argon2 runs) */
EVENT(unrealdb_expire_secret_cache)
{
	Secret *s;
	SecretCache *c, *c_next;
	for (s = secrets; s; s = s->next)
	{
		for (c = s->cache; c; c = c_next)
		{
			c_next = c->next;
			if (c->cache_hit < TStime() - UNREALDB_EXPIRE_SECRET_CACHE_AFTER)
			{
				DelListItem(c, s->cache);
				free_secret_cache(c);
			}
		}
	}
}
