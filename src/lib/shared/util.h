/**
 * Copyright (C) 2013  Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * @author Lucas De Marchi <lucas.demarchi@intel.com>
 */

#include <byteswap.h>
#include <endian.h>
#include <inttypes.h>
#include <stddef.h>

#define get_unaligned(ptr)			\
    ({						\
	struct __attribute__((packed)) {	\
            typeof(*(ptr)) __v;                 \
	} *__p = (typeof(__p)) (ptr);		\
	__p->__v;				\
    })

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint32_t get_le32(const void *ptr)
{
	return get_unaligned((const uint32_t *) ptr);
}

static inline uint32_t get_be32(const void *ptr)
{
	return bswap_32(get_unaligned((const uint32_t *) ptr));
}

static inline uint16_t get_le16(const void *ptr)
{
    return get_unaligned((const uint16_t *) ptr);
}

static inline uint16_t get_be16(const void *ptr)
{
    return bswap_16(get_unaligned((const uint16_t *) ptr));
}

#elif __BYTE_ORDER == __BIG_ENDIAN

static inline uint32_t get_le32(const void *ptr)
{
	return bswap_32(get_unaligned((const uint32_t *) ptr));
}

static inline uint32_t get_be32(const void *ptr)
{
	return get_unaligned((const uint32_t *) ptr);
}

static inline uint16_t get_le16(const void *ptr)
{
	return bswap_16(get_unaligned((const uint16_t *) ptr));
}

static inline uint16_t get_be16(const void *ptr)
{
	return get_unaligned((const uint16_t *) ptr);
}

#else
#error "Unknown byte order"
#endif
