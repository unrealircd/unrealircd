/*
 * mmdb.h - Minimal MMDB (MaxMind DB) reader library
 *
 * Written from the MaxMind DB file format specification
 * (https://maxmind.github.io/MaxMind-DB/).
 *
 * This C implementation was written by the UnrealIRCd team,
 * using the Go MMDB reader oschwald/maxminddb-golang by
 * Gregory J. Oschwald as a reference during development.
 *
 * Copyright (c) 2015 Gregory J. Oschwald (Go implementation)
 * Copyright (c) 2026 UnrealIRCd team
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in
 * all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef MMDB_H
#define MMDB_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/** Status/error codes returned by mmdb functions */
typedef enum {
	MMDB_OK = 0,              /**< Success */
	MMDB_ERR_OPEN,            /**< Could not open or mmap the file */
	MMDB_ERR_INVALID_DB,      /**< Not a valid MMDB file */
	MMDB_ERR_CORRUPT,         /**< Search tree or data section corruption */
	MMDB_ERR_NODATA,          /**< IP found but requested path doesn't exist */
	MMDB_ERR_TYPE,            /**< Type mismatch (asked for string, got uint, etc) */
	MMDB_ERR_IPV6_IN_V4,     /**< Tried to look up an IPv6 address in an IPv4-only db */
	MMDB_ERR_BADARG,          /**< Invalid argument (e.g. unparseable IP address) */
} MMDB_Status;

/** Database metadata */
typedef struct {
	uint32_t node_count;        /**< Number of nodes in the search tree */
	uint16_t record_size;       /**< Size of each record in bits */
	uint16_t ip_version;        /**< IP version the database covers (4 or 6) */
	uint64_t build_epoch;       /**< Unix timestamp when the database was built */
	char database_type[128];    /**< Database type string (e.g. "GeoLite2-Country") */
} MMDB_Metadata;

/** Database handle */
typedef struct {
	uint8_t *data;              /**< mmap'd (or malloc'd) file contents */
	size_t data_size;           /**< Total file size */
	size_t data_section_offset; /**< Offset where data section starts */
	size_t data_section_size;   /**< Size of data section */
	MMDB_Metadata metadata;     /**< Parsed database metadata */
	uint32_t ipv4_start_node;   /**< Cached start node for IPv4 lookups in IPv6 dbs */
	int ipv4_start_bit_depth;   /**< Bit depth at ipv4_start_node */
	int is_mmap;                /**< 1 if data was mmap'd, 0 if malloc'd */
} MMDB_DB;

/** Lookup result */
typedef struct {
	MMDB_DB *db;                /**< Database this result belongs to */
	size_t offset;              /**< Offset into data section, or 0 if not found */
	int has_data;               /**< 1 if IP was found and has data */
} MMDB_Result;

/** Open an MMDB database file.
 * Uses mmap where available, falls back to malloc+read.
 * @param db        Database handle to initialize
 * @param filename  Path to the .mmdb file
 * @returns MMDB_OK on success, or an error code
 */
MMDB_Status mmdb_open(MMDB_DB *db, const char *filename);

/** Close the database and release resources.
 * Safe to call on an already-closed or zero-initialized handle.
 * @param db  Database handle to close
 */
void mmdb_close(MMDB_DB *db);

/** Look up an IP address given as a string (IPv4 or IPv6).
 * On success, check result->has_data to see if the IP was
 * actually found in the database.
 * @param db      Database handle
 * @param ip_str  IP address string (e.g. "1.2.3.4" or "2001:db8::1")
 * @param result  Lookup result (output)
 * @returns MMDB_OK on success, or an error code
 */
MMDB_Status mmdb_lookup(MMDB_DB *db, const char *ip_str, MMDB_Result *result);

/** Look up an IP address given as a sockaddr.
 * Supports sockaddr_in (IPv4) and sockaddr_in6 (IPv6).
 * @param db      Database handle
 * @param sa      Socket address to look up
 * @param result  Lookup result (output)
 * @returns MMDB_OK on success, or an error code
 */
MMDB_Status mmdb_lookup_sockaddr(MMDB_DB *db, const struct sockaddr *sa,
                                 MMDB_Result *result);

/** Retrieve a string value from a lookup result by path.
 * Returns a malloc'd, null-terminated copy. Caller must free().
 * On error, *out is set to NULL.
 * @param result  Lookup result from mmdb_lookup()
 * @param out     Receives a malloc'd null-terminated string (output)
 * @param ...     Path of map keys (NULL sentinel is added automatically)
 * @returns MMDB_OK on success, or an error code
 * @note Example: mmdb_get_str(&result, &val, "country", "iso_code");
 */
MMDB_Status mmdb_do_get_str(MMDB_Result *result, char **out, ...);
#define mmdb_get_str(result, out, ...) mmdb_do_get_str(result, out, __VA_ARGS__, NULL)

/** Retrieve a uint32 value from a lookup result by path.
 * Also accepts uint16 values (promoted to uint32).
 * On error, *out is set to 0.
 * @param result  Lookup result from mmdb_lookup()
 * @param out     Receives the uint32 value (output)
 * @param ...     Path of map keys (NULL sentinel is added automatically)
 * @returns MMDB_OK on success, or an error code
 * @note Example: mmdb_get_uint32(&result, &asn, "autonomous_system_number");
 */
MMDB_Status mmdb_do_get_uint32(MMDB_Result *result, uint32_t *out, ...);
#define mmdb_get_uint32(result, out, ...) mmdb_do_get_uint32(result, out, __VA_ARGS__, NULL)

/** Retrieve a boolean value from a lookup result by path.
 * On error, *out is set to 0.
 * @param result  Lookup result from mmdb_lookup()
 * @param out     Receives the boolean value (0 or 1) (output)
 * @param ...     Path of map keys (NULL sentinel is added automatically)
 * @returns MMDB_OK on success, or an error code
 * @note Example: mmdb_get_bool(&result, &is_vpn, "is_anonymous_vpn");
 */
MMDB_Status mmdb_do_get_bool(MMDB_Result *result, int *out, ...);
#define mmdb_get_bool(result, out, ...) mmdb_do_get_bool(result, out, __VA_ARGS__, NULL)

/** Retrieve a double (float64) value from a lookup result by path.
 * Also accepts float32 values (promoted to double).
 * On error, *out is set to 0.
 * @param result  Lookup result from mmdb_lookup()
 * @param out     Receives the double value (output)
 * @param ...     Path of map keys (NULL sentinel is added automatically)
 * @returns MMDB_OK on success, or an error code
 * @note Example: mmdb_get_double(&result, &lat, "location", "latitude");
 */
MMDB_Status mmdb_do_get_double(MMDB_Result *result, double *out, ...);
#define mmdb_get_double(result, out, ...) mmdb_do_get_double(result, out, __VA_ARGS__, NULL)

/** Return a human-readable error string for a status code.
 * @param err  Status code to describe
 * @returns Static string describing the error (never NULL)
 */
const char *mmdb_strerror(MMDB_Status err);

#endif /* MMDB_H */
