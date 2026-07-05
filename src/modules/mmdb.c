/*
 * mmdb.c - Minimal MMDB (MaxMind DB) reader library
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

#include "mmdb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#ifdef _WIN32
 #include <windows.h>
 #include <io.h>
#else
 #include <fcntl.h>
 #include <unistd.h>
 #include <sys/stat.h>
 #include <sys/mman.h>
 #include <arpa/inet.h>
#endif

/* Constants */

static const uint8_t METADATA_MARKER[] = "\xAB\xCD\xEF"
                                         "MaxMind.com";
#define METADATA_MARKER_LEN 14
#define METADATA_MAX_SIZE   (128 * 1024)
#define DATA_SEPARATOR_SIZE 16
#define MAX_DECODE_DEPTH    64

/* MMDB data types from the spec */
enum {
	DT_EXTENDED = 0,
	DT_POINTER = 1,
	DT_STRING = 2,
	DT_FLOAT64 = 3,
	DT_BYTES = 4,
	DT_UINT16 = 5,
	DT_UINT32 = 6,
	DT_MAP = 7,
	DT_INT32 = 8,
	DT_UINT64 = 9,
	DT_UINT128 = 10,
	DT_ARRAY = 11,
	DT_BOOL = 14,
	DT_FLOAT32 = 15,
};

/* Platform: mmap / file I/O */

#ifdef _WIN32

static uint8_t *mmdb_mmap_file(const char *filename, size_t *size_out, int *is_mmap)
{
	HANDLE hFile, hMap;
	LARGE_INTEGER fileSize;
	uint8_t *data;

	hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
	                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;

	if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0 ||
	    (uint64_t)fileSize.QuadPart > SIZE_MAX)
	{
		CloseHandle(hFile);
		return NULL;
	}

	hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	CloseHandle(hFile);
	if (!hMap)
		return NULL;

	data = (uint8_t *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
	CloseHandle(hMap);
	if (!data)
		return NULL;

	*size_out = (size_t)fileSize.QuadPart;
	*is_mmap = 1;
	return data;
}

static void mmdb_munmap(uint8_t *data, size_t size)
{
	(void)size;
	UnmapViewOfFile(data);
}

#else /* Unix */

static uint8_t *mmdb_mmap_file(const char *filename, size_t *size_out, int *is_mmap)
{
	int fd;
	struct stat st;
	uint8_t *data;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;

	if (fstat(fd, &st) < 0 || st.st_size <= 0)
	{
		close(fd);
		return NULL;
	}

	data = (uint8_t *)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (data == MAP_FAILED)
		return NULL;

	*size_out = (size_t)st.st_size;
	*is_mmap = 1;
	return data;
}

static void mmdb_munmap(uint8_t *data, size_t size)
{
	munmap(data, size);
}

#endif

/* Data section decoder
 *
 * These functions decode values from the MMDB data section.
 * The "buffer" and "buflen" refer to the data section (or
 * metadata section) only, not the whole file.
 */

/* Decode size from control byte per the spec's payload size rules. */
static int decode_size(const uint8_t *buf, size_t buflen,
                       uint32_t raw_size, size_t offset,
                       uint32_t *size_out, size_t *new_offset)
{
	if (raw_size < 29)
	{
		*size_out = raw_size;
		*new_offset = offset;
		return MMDB_OK;
	}
	if (raw_size == 29)
	{
		if (offset >= buflen)
			return MMDB_ERR_CORRUPT;
		*size_out = 29 + (uint32_t)buf[offset];
		*new_offset = offset + 1;
		return MMDB_OK;
	}
	if (raw_size == 30)
	{
		if (offset + 2 > buflen)
			return MMDB_ERR_CORRUPT;
		*size_out = 285 + ((uint32_t)buf[offset] << 8) + (uint32_t)buf[offset + 1];
		*new_offset = offset + 2;
		return MMDB_OK;
	}
	/* raw_size == 31 */
	if (offset + 3 > buflen)
		return MMDB_ERR_CORRUPT;
	*size_out = 65821 +
	            ((uint32_t)buf[offset] << 16) +
	            ((uint32_t)buf[offset + 1] << 8) +
	            (uint32_t)buf[offset + 2];
	*new_offset = offset + 3;
	return MMDB_OK;
}

/* Decode a control byte: returns data type, payload size, and new offset. */
static int decode_ctrl(const uint8_t *buf, size_t buflen, size_t offset,
                       int *type_out, uint32_t *size_out, size_t *data_offset)
{
	int type;
	uint32_t raw_size;
	size_t off;

	if (offset >= buflen)
		return MMDB_ERR_CORRUPT;

	type = (buf[offset] >> 5) & 0x7;
	raw_size = buf[offset] & 0x1F;
	off = offset + 1;

	if (type == DT_EXTENDED)
	{
		if (off >= buflen)
			return MMDB_ERR_CORRUPT;
		type = (int)buf[off] + 7;
		off++;
	}

	if (type == DT_POINTER)
	{
		/* For pointers, the size field encodes the pointer value, not a payload size.
		 * We return raw_size in size_out for the caller to decode. */
		*type_out = DT_POINTER;
		*size_out = raw_size;
		*data_offset = off;
		return MMDB_OK;
	}

	*type_out = type;
	return decode_size(buf, buflen, raw_size, off, size_out, data_offset);
}

/* Decode a pointer value per the spec. Returns the offset it points to. */
static int decode_pointer(const uint8_t *buf, size_t buflen,
                          uint32_t ctrl_size, size_t offset,
                          size_t *pointer_out, size_t *new_offset)
{
	uint32_t ptr_size = ((ctrl_size >> 3) & 0x3) + 1;
	size_t end = offset + ptr_size;
	size_t pointer;
	uint32_t prefix;
	size_t i;

	if (end > buflen)
		return MMDB_ERR_CORRUPT;

	prefix = (ptr_size == 4) ? 0 : (ctrl_size & 0x7);

	/* Build pointer from prefix + pointer bytes (big-endian) */
	pointer = prefix;
	for (i = offset; i < end; i++)
		pointer = (pointer << 8) | (size_t)buf[i];

	/* Add base offset per pointer size */
	switch (ptr_size)
	{
		case 1:
			break;
		case 2:
			pointer += 2048;
			break;
		case 3:
			pointer += 526336;
			break;
		case 4:
			break;
	}

	*pointer_out = pointer;
	*new_offset = end;
	return MMDB_OK;
}

/* Read a uint32 from variable-length big-endian bytes in the data section. */
static int decode_uint32(const uint8_t *buf, size_t buflen,
                         uint32_t size, size_t offset, uint32_t *out)
{
	uint32_t i;

	if (size > 4 || offset + size > buflen)
		return MMDB_ERR_CORRUPT;
	*out = 0;
	for (i = 0; i < size; i++)
		*out = (*out << 8) | (uint32_t)buf[offset + i];
	return MMDB_OK;
}

/* Read a uint64 from variable-length big-endian bytes. */
static int decode_uint64(const uint8_t *buf, size_t buflen,
                         uint32_t size, size_t offset, uint64_t *out)
{
	uint32_t i;

	if (size > 8 || offset + size > buflen)
		return MMDB_ERR_CORRUPT;
	*out = 0;
	for (i = 0; i < size; i++)
		*out = (*out << 8) | (uint64_t)buf[offset + i];
	return MMDB_OK;
}

/* Read a uint16 from variable-length big-endian bytes. */
static int decode_uint16(const uint8_t *buf, size_t buflen,
                         uint32_t size, size_t offset, uint16_t *out)
{
	uint32_t i;

	if (size > 2 || offset + size > buflen)
		return MMDB_ERR_CORRUPT;
	*out = 0;
	for (i = 0; i < size; i++)
		*out = (uint16_t)((*out << 8) | (uint16_t)buf[offset + i]);
	return MMDB_OK;
}

/* Read a float64 (double) stored as IEEE-754 big-endian. */
static int decode_float64(const uint8_t *buf, size_t buflen,
                          uint32_t size, size_t offset, double *out)
{
	union {
		uint64_t u;
		double d;
	} conv;
	int i;

	if (size != 8 || offset + 8 > buflen)
		return MMDB_ERR_CORRUPT;
	conv.u = 0;
	for (i = 0; i < 8; i++)
		conv.u = (conv.u << 8) | (uint64_t)buf[offset + i];
	*out = conv.d;
	return MMDB_OK;
}

/* Read a float32 stored as IEEE-754 big-endian. */
static int decode_float32(const uint8_t *buf, size_t buflen,
                          uint32_t size, size_t offset, float *out)
{
	union {
		uint32_t u;
		float f;
	} conv;
	int i;

	if (size != 4 || offset + 4 > buflen)
		return MMDB_ERR_CORRUPT;
	conv.u = 0;
	for (i = 0; i < 4; i++)
		conv.u = (conv.u << 8) | (uint32_t)buf[offset + i];
	*out = conv.f;
	return MMDB_OK;
}

/* Skip over a data field without decoding it. Used to skip values
 * in maps when searching for a specific key. */
static int skip_value(const uint8_t *buf, size_t buflen, size_t offset,
                      size_t *new_offset, int depth)
{
	int type;
	uint32_t size;
	size_t off;
	int err;
	uint32_t i;
	uint32_t ptr_size;

	if (depth > MAX_DECODE_DEPTH)
		return MMDB_ERR_CORRUPT;

	err = decode_ctrl(buf, buflen, offset, &type, &size, &off);
	if (err)
		return err;

	if (type == DT_POINTER)
	{
		/* Pointer is self-contained; just skip past the pointer bytes */
		ptr_size = ((size >> 3) & 0x3) + 1;
		if (off + ptr_size > buflen)
			return MMDB_ERR_CORRUPT;
		*new_offset = off + ptr_size;
		return MMDB_OK;
	}

	if (type == DT_MAP)
	{
		/* Skip 2*size entries (key + value for each pair) */
		for (i = 0; i < size * 2; i++)
		{
			err = skip_value(buf, buflen, off, &off, depth + 1);
			if (err)
				return err;
		}
		*new_offset = off;
		return MMDB_OK;
	}

	if (type == DT_ARRAY)
	{
		for (i = 0; i < size; i++)
		{
			err = skip_value(buf, buflen, off, &off, depth + 1);
			if (err)
				return err;
		}
		*new_offset = off;
		return MMDB_OK;
	}

	if (type == DT_BOOL)
	{
		/* Bool has no payload; size encodes the value */
		*new_offset = off;
		return MMDB_OK;
	}

	/* Scalar types: payload is 'size' bytes */
	if (off + size > buflen)
		return MMDB_ERR_CORRUPT;
	*new_offset = off + size;
	return MMDB_OK;
}

/* Resolve a data entry at a given offset, following pointers
 * if needed. Returns the type, size, and offset of the actual
 * data payload. */
static int resolve_entry(const uint8_t *buf, size_t buflen, size_t offset,
                         int *type_out, uint32_t *size_out, size_t *data_offset)
{
	int type;
	uint32_t size;
	size_t off;
	int err;
	size_t pointer;

	err = decode_ctrl(buf, buflen, offset, &type, &size, &off);
	if (err)
		return err;

	if (type == DT_POINTER)
	{
		err = decode_pointer(buf, buflen, size, off, &pointer, &off);
		if (err)
			return err;
		/* Follow the pointer (only one level allowed per spec) */
		err = decode_ctrl(buf, buflen, pointer, &type, &size, &off);
		if (err)
			return err;
		if (type == DT_POINTER)
			return MMDB_ERR_CORRUPT; /* pointer to pointer is illegal */
	}

	*type_out = type;
	*size_out = size;
	*data_offset = off;
	return MMDB_OK;
}

/* Find a key in a map.
 *
 * Given the data section buffer and an offset pointing to a map,
 * find the entry with the given key. Returns the offset of the
 * value's control byte. */
static int map_find_key(const uint8_t *buf, size_t buflen,
                        size_t map_offset, const char *key,
                        size_t *value_offset)
{
	int type;
	uint32_t map_size;
	size_t off;
	int err;
	size_t key_len = strlen(key);
	uint32_t i;
	int ktype;
	uint32_t ksize;
	size_t koff;
	size_t next_off;

	/* Decode the map entry at map_offset, following pointers */
	err = resolve_entry(buf, buflen, map_offset, &type, &map_size, &off);
	if (err)
		return err;
	if (type != DT_MAP)
		return MMDB_ERR_TYPE;

	for (i = 0; i < map_size; i++)
	{
		/* Decode key - may be a pointer */
		err = resolve_entry(buf, buflen, off, &ktype, &ksize, &koff);
		if (err)
			return err;
		if (ktype != DT_STRING)
			return MMDB_ERR_CORRUPT;

		/* We need to advance past the key in the stream.
		 * The key's position in the stream is at 'off', so skip it. */
		err = skip_value(buf, buflen, off, &next_off, 0);
		if (err)
			return err;

		/* Compare key */
		if (koff + ksize <= buflen &&
		    ksize == (uint32_t)key_len &&
		    memcmp(buf + koff, key, key_len) == 0)
		{
			*value_offset = next_off;
			return MMDB_OK;
		}

		/* Skip the value */
		err = skip_value(buf, buflen, next_off, &off, 0);
		if (err)
			return err;
	}

	return MMDB_ERR_NODATA;
}

/* Walk a path of keys through nested maps */
static int walk_path(const uint8_t *buf, size_t buflen,
                     size_t start_offset, va_list ap,
                     size_t *final_offset)
{
	size_t offset = start_offset;
	const char *key;
	int err;

	while ((key = va_arg(ap, const char *)) != NULL)
	{
		err = map_find_key(buf, buflen, offset, key, &offset);
		if (err)
			return err;
	}

	*final_offset = offset;
	return MMDB_OK;
}

/* Metadata parsing */

/* Find the metadata marker by searching backwards from the end of file. */
static const uint8_t *find_metadata(const uint8_t *data, size_t data_size)
{
	size_t scan_size;
	const uint8_t *p;

	if (data_size < METADATA_MARKER_LEN)
		return NULL;

	scan_size = data_size;
	if (scan_size > METADATA_MAX_SIZE)
		scan_size = METADATA_MAX_SIZE;

	/* Search backwards for the last occurrence */
	p = data + data_size - METADATA_MARKER_LEN;
	while (p >= data + data_size - scan_size)
	{
		if (memcmp(p, METADATA_MARKER, METADATA_MARKER_LEN) == 0)
			return p;
		if (p == data)
			break;
		p--;
	}
	return NULL;
}

/* Parse metadata from the metadata section into db->metadata.
 * The metadata is itself an MMDB data section containing a map. */
static int parse_metadata(MMDB_DB *db, const uint8_t *meta_buf, size_t meta_len)
{
	int type;
	uint32_t map_size;
	size_t off;
	int err;
	uint32_t i;
	int ktype;
	uint32_t ksize;
	size_t koff;
	size_t val_off;
	int vtype;
	uint32_t vsize;
	size_t voff;
	uint32_t val_u32;
	uint16_t val_u16;
	uint64_t val_u64;
	size_t copy_len;

	err = decode_ctrl(meta_buf, meta_len, 0, &type, &map_size, &off);
	if (err)
		return MMDB_ERR_INVALID_DB;
	if (type != DT_MAP)
		return MMDB_ERR_INVALID_DB;

	memset(&db->metadata, 0, sizeof(db->metadata));

	for (i = 0; i < map_size; i++)
	{
		/* Decode key */
		err = resolve_entry(meta_buf, meta_len, off, &ktype, &ksize, &koff);
		if (err)
			return MMDB_ERR_INVALID_DB;
		if (ktype != DT_STRING)
			return MMDB_ERR_INVALID_DB;
		if (koff + ksize > meta_len)
			return MMDB_ERR_INVALID_DB;

		/* Skip key in stream */
		err = skip_value(meta_buf, meta_len, off, &val_off, 0);
		if (err)
			return MMDB_ERR_INVALID_DB;

		/* Decode value depending on which key this is */
		err = resolve_entry(meta_buf, meta_len, val_off, &vtype, &vsize, &voff);
		if (err)
			return MMDB_ERR_INVALID_DB;

		if (ksize == 10 && memcmp(meta_buf + koff, "node_count", 10) == 0)
		{
			if (vtype != DT_UINT32 && vtype != DT_UINT16)
				return MMDB_ERR_INVALID_DB;
			err = decode_uint32(meta_buf, meta_len, vsize, voff, &val_u32);
			if (err)
				return MMDB_ERR_INVALID_DB;
			db->metadata.node_count = val_u32;
		} else if (ksize == 11 && memcmp(meta_buf + koff, "record_size", 11) == 0)
		{
			if (vtype != DT_UINT16 && vtype != DT_UINT32)
				return MMDB_ERR_INVALID_DB;
			err = decode_uint16(meta_buf, meta_len, vsize, voff, &val_u16);
			if (err)
				return MMDB_ERR_INVALID_DB;
			db->metadata.record_size = val_u16;
		} else if (ksize == 10 && memcmp(meta_buf + koff, "ip_version", 10) == 0)
		{
			if (vtype != DT_UINT16 && vtype != DT_UINT32)
				return MMDB_ERR_INVALID_DB;
			err = decode_uint16(meta_buf, meta_len, vsize, voff, &val_u16);
			if (err)
				return MMDB_ERR_INVALID_DB;
			db->metadata.ip_version = val_u16;
		} else if (ksize == 11 && memcmp(meta_buf + koff, "build_epoch", 11) == 0)
		{
			if (vtype != DT_UINT64 && vtype != DT_UINT32)
				return MMDB_ERR_INVALID_DB;
			err = decode_uint64(meta_buf, meta_len, vsize, voff, &val_u64);
			if (err)
				return MMDB_ERR_INVALID_DB;
			db->metadata.build_epoch = val_u64;
		} else if (ksize == 13 && memcmp(meta_buf + koff, "database_type", 13) == 0)
		{
			if (vtype != DT_STRING)
				return MMDB_ERR_INVALID_DB;
			copy_len = vsize;
			if (copy_len >= sizeof(db->metadata.database_type))
				copy_len = sizeof(db->metadata.database_type) - 1;
			if (voff + copy_len > meta_len)
				return MMDB_ERR_INVALID_DB;
			memcpy(db->metadata.database_type, meta_buf + voff, copy_len);
			db->metadata.database_type[copy_len] = '\0';
		}

		/* Skip the value in the stream to advance to the next key */
		err = skip_value(meta_buf, meta_len, val_off, &off, 0);
		if (err)
			return MMDB_ERR_INVALID_DB;
	}

	/* Validate required fields */
	if (db->metadata.node_count == 0 ||
	    db->metadata.record_size == 0 ||
	    (db->metadata.ip_version != 4 && db->metadata.ip_version != 6))
	{
		return MMDB_ERR_INVALID_DB;
	}

	/* Only 24, 28, 32 bit records are supported */
	if (db->metadata.record_size != 24 &&
	    db->metadata.record_size != 28 &&
	    db->metadata.record_size != 32)
	{
		return MMDB_ERR_INVALID_DB;
	}

	return MMDB_OK;
}

/* Search tree traversal */

/* Read a single record from a node. bit=0 for left, bit=1 for right. */
static int read_node(const uint8_t *buf, size_t buflen,
                     uint32_t node, uint32_t bit, uint32_t record_size,
                     uint32_t node_offset_mult, uint32_t *value)
{
	size_t offset = (size_t)node * node_offset_mult;
	size_t o;

	switch (record_size)
	{
		case 24:
		{
			o = offset + bit * 3;
			if (o + 3 > buflen)
				return MMDB_ERR_CORRUPT;
			*value = ((uint32_t)buf[o] << 16) |
			         ((uint32_t)buf[o + 1] << 8) |
			         (uint32_t)buf[o + 2];
			return MMDB_OK;
		}
		case 28:
		{
			if (offset + 7 > buflen)
				return MMDB_ERR_CORRUPT;
			if (bit == 0)
			{
				*value = (((uint32_t)buf[offset + 3] & 0xF0) << 20) |
				         ((uint32_t)buf[offset] << 16) |
				         ((uint32_t)buf[offset + 1] << 8) |
				         (uint32_t)buf[offset + 2];
			} else
			{
				*value = (((uint32_t)buf[offset + 3] & 0x0F) << 24) |
				         ((uint32_t)buf[offset + 4] << 16) |
				         ((uint32_t)buf[offset + 5] << 8) |
				         (uint32_t)buf[offset + 6];
			}
			return MMDB_OK;
		}
		case 32:
		{
			o = offset + bit * 4;
			if (o + 4 > buflen)
				return MMDB_ERR_CORRUPT;
			*value = ((uint32_t)buf[o] << 24) |
			         ((uint32_t)buf[o + 1] << 16) |
			         ((uint32_t)buf[o + 2] << 8) |
			         (uint32_t)buf[o + 3];
			return MMDB_OK;
		}
	}
	return MMDB_ERR_CORRUPT;
}

/* Traverse the search tree for a 128-bit IP (IPv6 or IPv4-mapped). */
static int traverse_tree(MMDB_DB *db, const uint8_t ip[16],
                         int start_bit, uint32_t start_node,
                         uint32_t *result_node, int *prefix_len)
{
	uint32_t node = start_node;
	uint32_t node_count = db->metadata.node_count;
	uint32_t record_size = db->metadata.record_size;
	uint32_t node_offset_mult = record_size / 4;
	uint32_t bit;
	int i;
	int err;

	for (i = start_bit; i < 128 && node < node_count; i++)
	{
		bit = (ip[i >> 3] >> (7 - (i & 7))) & 1;
		err = read_node(db->data, db->data_size, node, bit,
		                record_size, node_offset_mult, &node);
		if (err)
			return err;
	}

	*result_node = node;
	*prefix_len = i;
	return MMDB_OK;
}

/* Pre-walk the first 96 zero bits to find the IPv4 subtree start
 * in an IPv6 database. */
static int find_ipv4_start(MMDB_DB *db)
{
	uint32_t node = 0;
	uint32_t node_count = db->metadata.node_count;
	uint32_t record_size = db->metadata.record_size;
	uint32_t node_offset_mult = record_size / 4;
	int i;
	int err;

	db->ipv4_start_bit_depth = 96;

	for (i = 0; i < 96 && node < node_count; i++)
	{
		err = read_node(db->data, db->data_size, node, 0,
		                record_size, node_offset_mult, &node);
		if (err)
			return err;
	}

	db->ipv4_start_node = node;
	db->ipv4_start_bit_depth = i;
	return MMDB_OK;
}

/* Lookup core */

static int lookup_ip128(MMDB_DB *db, const uint8_t ip[16], int is_ipv4,
                        MMDB_Result *result)
{
	uint32_t node;
	int prefix_len;
	int err;
	int start_bit;
	uint32_t start_node;
	size_t data_offset;

	result->db = db;
	result->offset = 0;
	result->has_data = 0;

	if (is_ipv4)
	{
		start_bit = db->ipv4_start_bit_depth;
		start_node = db->ipv4_start_node;
	} else
	{
		start_bit = 0;
		start_node = 0;
	}

	err = traverse_tree(db, ip, start_bit, start_node, &node, &prefix_len);
	if (err)
		return err;

	if (node == db->metadata.node_count)
	{
		/* No data for this IP */
		return MMDB_OK;
	}
	if (node > db->metadata.node_count)
	{
		/* Pointer into data section */
		data_offset = (size_t)(node - db->metadata.node_count) - DATA_SEPARATOR_SIZE;
		if (data_offset >= db->data_section_size)
			return MMDB_ERR_CORRUPT;
		result->offset = data_offset;
		result->has_data = 1;
		return MMDB_OK;
	}

	return MMDB_ERR_CORRUPT;
}

/* Public API */

MMDB_Status mmdb_open(MMDB_DB *db, const char *filename)
{
	const uint8_t *meta_start;
	size_t meta_offset;
	size_t search_tree_size;
	size_t meta_marker_offset;
	int err;

	if (!db || !filename)
		return MMDB_ERR_BADARG;

	memset(db, 0, sizeof(*db));

	db->data = mmdb_mmap_file(filename, &db->data_size, &db->is_mmap);
	if (!db->data)
		return MMDB_ERR_OPEN;

	/* Find metadata marker */
	meta_start = find_metadata(db->data, db->data_size);
	if (!meta_start)
	{
		mmdb_close(db);
		return MMDB_ERR_INVALID_DB;
	}

	meta_offset = (size_t)(meta_start - db->data) + METADATA_MARKER_LEN;

	/* Parse metadata */
	err = parse_metadata(db, db->data + meta_offset,
	                     db->data_size - meta_offset);
	if (err)
	{
		mmdb_close(db);
		return err;
	}

	/* Calculate section offsets. Per the spec:
	 * search_tree_size = (record_size * 2 / 8) * node_count
	 *                  = (record_size / 4) * node_count
	 */
	if (db->metadata.node_count > SIZE_MAX / (db->metadata.record_size / 4))
	{
		mmdb_close(db);
		return MMDB_ERR_INVALID_DB;
	}
	search_tree_size = (size_t)(db->metadata.record_size / 4) *
	                   (size_t)db->metadata.node_count;
	db->data_section_offset = search_tree_size + DATA_SEPARATOR_SIZE;

	/* data section ends where the metadata marker begins */
	meta_marker_offset = (size_t)(meta_start - db->data);
	if (db->data_section_offset > meta_marker_offset)
	{
		mmdb_close(db);
		return MMDB_ERR_INVALID_DB;
	}
	db->data_section_size = meta_marker_offset - db->data_section_offset;

	/* For IPv6 databases, find the IPv4 subtree start */
	if (db->metadata.ip_version == 6)
	{
		err = find_ipv4_start(db);
		if (err)
		{
			mmdb_close(db);
			return err;
		}
	} else
	{
		db->ipv4_start_node = 0;
		db->ipv4_start_bit_depth = 96;
	}

	return MMDB_OK;
}

void mmdb_close(MMDB_DB *db)
{
	if (!db)
		return;
	if (db->data)
	{
		if (db->is_mmap)
			mmdb_munmap(db->data, db->data_size);
		else
			free(db->data);
		db->data = NULL;
	}
	db->data_size = 0;
}

MMDB_Status mmdb_lookup(MMDB_DB *db, const char *ip_str, MMDB_Result *result)
{
	uint8_t ip128[16];
	struct in_addr addr4;
	struct in6_addr addr6;

	if (!db || !db->data || !ip_str || !result)
		return MMDB_ERR_BADARG;

	memset(ip128, 0, sizeof(ip128));

	if (inet_pton(AF_INET, ip_str, &addr4) == 1)
	{
		/* MMDB always uses a 128-bit search buffer.
		 * IPv4 goes in the last 4 bytes (offset 12).
		 * The is_ipv4 flag skips the first 96 bits
		 * so traversal begins at the IPv4 address.
		 */
		memcpy(ip128 + 12, &addr4.s_addr, 4);
		return lookup_ip128(db, ip128, 1, result);
	}

	if (inet_pton(AF_INET6, ip_str, &addr6) == 1)
	{
		if (db->metadata.ip_version == 4)
			return MMDB_ERR_IPV6_IN_V4;
		memcpy(ip128, addr6.s6_addr, 16);
		return lookup_ip128(db, ip128, 0, result);
	}

	return MMDB_ERR_BADARG;
}

MMDB_Status mmdb_lookup_sockaddr(MMDB_DB *db, const struct sockaddr *sa,
                                 MMDB_Result *result)
{
	uint8_t ip128[16];
	const struct sockaddr_in *sa4;
	const struct sockaddr_in6 *sa6;

	if (!db || !db->data || !sa || !result)
		return MMDB_ERR_BADARG;

	memset(ip128, 0, sizeof(ip128));

	if (sa->sa_family == AF_INET)
	{
		sa4 = (const struct sockaddr_in *)sa;
		memcpy(ip128 + 12, &sa4->sin_addr.s_addr, 4);
		return lookup_ip128(db, ip128, 1, result);
	}

	if (sa->sa_family == AF_INET6)
	{
		sa6 = (const struct sockaddr_in6 *)sa;
		if (db->metadata.ip_version == 4)
			return MMDB_ERR_IPV6_IN_V4;
		memcpy(ip128, sa6->sin6_addr.s6_addr, 16);
		return lookup_ip128(db, ip128, 0, result);
	}

	return MMDB_ERR_BADARG;
}

static MMDB_Status mmdb_get_str_raw_va(MMDB_Result *result, const char **out, size_t *len, va_list ap)
{
	size_t offset;
	int err;
	int type;
	uint32_t size;
	size_t data_off;
	const uint8_t *dsec;
	size_t dsec_len;

	if (!result || !result->db || !out || !len)
		return MMDB_ERR_BADARG;

	*out = NULL;
	*len = 0;

	if (!result->has_data)
		return MMDB_ERR_NODATA;

	dsec = result->db->data + result->db->data_section_offset;
	dsec_len = result->db->data_section_size;

	/* Walk the path */
	err = walk_path(dsec, dsec_len, result->offset, ap, &offset);
	if (err)
		return err;

	/* Resolve the final value */
	err = resolve_entry(dsec, dsec_len, offset, &type, &size, &data_off);
	if (err)
		return err;
	if (type != DT_STRING)
		return MMDB_ERR_TYPE;
	if (data_off + size > dsec_len)
		return MMDB_ERR_CORRUPT;

	*out = (const char *)(dsec + data_off);
	*len = (size_t)size;
	return MMDB_OK;
}

MMDB_Status mmdb_do_get_str(MMDB_Result *result, char **out, ...)
{
	const char *ptr;
	size_t len;
	va_list ap;
	int err;

	if (!out)
		return MMDB_ERR_BADARG;

	*out = NULL;
	va_start(ap, out);
	err = mmdb_get_str_raw_va(result, &ptr, &len, ap);
	va_end(ap);
	if (err)
		return err;

	*out = malloc(len + 1);
	if (!*out)
		return MMDB_ERR_OPEN;
	memcpy(*out, ptr, len);
	(*out)[len] = '\0';
	return MMDB_OK;
}

MMDB_Status mmdb_do_get_uint32(MMDB_Result *result, uint32_t *out, ...)
{
	va_list ap;
	size_t offset;
	int err;
	int type;
	uint32_t size;
	size_t data_off;
	const uint8_t *dsec;
	size_t dsec_len;

	if (!result || !result->db || !out)
		return MMDB_ERR_BADARG;

	*out = 0;

	if (!result->has_data)
		return MMDB_ERR_NODATA;

	dsec = result->db->data + result->db->data_section_offset;
	dsec_len = result->db->data_section_size;

	/* Walk the path */
	va_start(ap, out);
	err = walk_path(dsec, dsec_len, result->offset, ap, &offset);
	va_end(ap);
	if (err)
		return err;

	/* Resolve the final value */
	err = resolve_entry(dsec, dsec_len, offset, &type, &size, &data_off);
	if (err)
		return err;

	/* Accept both uint16 and uint32 */
	if (type == DT_UINT32 || type == DT_UINT16)
	{
		return decode_uint32(dsec, dsec_len, size, data_off, out);
	}

	return MMDB_ERR_TYPE;
}

MMDB_Status mmdb_do_get_bool(MMDB_Result *result, int *out, ...)
{
	va_list ap;
	size_t offset;
	int err;
	int type;
	uint32_t size;
	size_t data_off;
	const uint8_t *dsec;
	size_t dsec_len;

	if (!result || !result->db || !out)
		return MMDB_ERR_BADARG;

	*out = 0;

	if (!result->has_data)
		return MMDB_ERR_NODATA;

	dsec = result->db->data + result->db->data_section_offset;
	dsec_len = result->db->data_section_size;

	va_start(ap, out);
	err = walk_path(dsec, dsec_len, result->offset, ap, &offset);
	va_end(ap);
	if (err)
		return err;

	err = resolve_entry(dsec, dsec_len, offset, &type, &size, &data_off);
	if (err)
		return err;

	if (type != DT_BOOL)
		return MMDB_ERR_TYPE;

	/* Per spec, boolean size is 0 (false) or 1 (true), no payload */
	*out = (size != 0) ? 1 : 0;
	return MMDB_OK;
}

MMDB_Status mmdb_do_get_double(MMDB_Result *result, double *out, ...)
{
	va_list ap;
	size_t offset;
	int err;
	int type;
	uint32_t size;
	size_t data_off;
	const uint8_t *dsec;
	size_t dsec_len;
	float f;

	if (!result || !result->db || !out)
		return MMDB_ERR_BADARG;

	*out = 0;

	if (!result->has_data)
		return MMDB_ERR_NODATA;

	dsec = result->db->data + result->db->data_section_offset;
	dsec_len = result->db->data_section_size;

	va_start(ap, out);
	err = walk_path(dsec, dsec_len, result->offset, ap, &offset);
	va_end(ap);
	if (err)
		return err;

	err = resolve_entry(dsec, dsec_len, offset, &type, &size, &data_off);
	if (err)
		return err;

	if (type == DT_FLOAT64)
	{
		return decode_float64(dsec, dsec_len, size, data_off, out);
	}
	if (type == DT_FLOAT32)
	{
		/* Promote float32 to double */
		err = decode_float32(dsec, dsec_len, size, data_off, &f);
		if (err)
			return err;
		*out = (double)f;
		return MMDB_OK;
	}

	return MMDB_ERR_TYPE;
}

const char *mmdb_strerror(MMDB_Status err)
{
	switch (err)
	{
		case MMDB_OK:
			return "Success";
		case MMDB_ERR_OPEN:
			return "Could not open database file";
		case MMDB_ERR_INVALID_DB:
			return "Invalid MMDB database";
		case MMDB_ERR_BADARG:
			return "Invalid argument";
		case MMDB_ERR_CORRUPT:
			return "Corrupt database (search tree or data section)";
		case MMDB_ERR_NODATA:
			return "No data found for the requested path";
		case MMDB_ERR_TYPE:
			return "Data type mismatch";
		case MMDB_ERR_IPV6_IN_V4:
			return "Cannot look up IPv6 address in IPv4-only database";
		default:
			return "Unknown error";
	}
}
