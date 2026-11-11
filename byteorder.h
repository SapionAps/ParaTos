#pragma once
#include <stdint.h>

// Guest byte order is independent of native OS headers.
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define paratos_htobe16(x) __builtin_bswap16((uint16_t)(x))
#define paratos_htole16(x) ((uint16_t)(x))
#define paratos_htobe32(x) __builtin_bswap32((uint32_t)(x))
#define paratos_htole32(x) ((uint32_t)(x))
#define paratos_htobe64(x) __builtin_bswap64((uint64_t)(x))
#define paratos_htole64(x) ((uint64_t)(x))
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define paratos_htobe16(x) ((uint16_t)(x))
#define paratos_htole16(x) __builtin_bswap16((uint16_t)(x))
#define paratos_htobe32(x) ((uint32_t)(x))
#define paratos_htole32(x) __builtin_bswap32((uint32_t)(x))
#define paratos_htobe64(x) ((uint64_t)(x))
#define paratos_htole64(x) __builtin_bswap64((uint64_t)(x))
#else
#error Unsupported native byte order
#endif

#define paratos_be16toh(x) paratos_htobe16(x)
#define paratos_le16toh(x) paratos_htole16(x)
#define paratos_be32toh(x) paratos_htobe32(x)
#define paratos_le32toh(x) paratos_htole32(x)
#define paratos_be64toh(x) paratos_htobe64(x)
#define paratos_le64toh(x) paratos_htole64(x)
