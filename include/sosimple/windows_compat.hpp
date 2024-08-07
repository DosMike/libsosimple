#if !defined SOSIMPLE_WINDOWS_COMPAT_HPP
#define SOSIMPLE_WINDOWS_COMPAT_HPP

#include "sosimple/exports.hpp"

#define WINGLUE_AGAIN WSA_

// create a bunch of compat macros for endian.h
#if BYTE_ORDER == LITTLE_ENDIAN
    #define htobe16(x) htons(x)
    #define htole16(x) (x)
    #define be16toh(x) ntohs(x)
    #define le16toh(x) (x)
    
    #define htobe32(x) htonl(x)
    #define htole32(x) (x)
    #define be32toh(x) ntohl(x)
    #define le32toh(x) (x)
    
    #define htobe64(x) htonll(x)
    #define htole64(x) (x)
    #define be64toh(x) ntohll(x)
    #define le64toh(x) (x)
#elif BYTE_ORDER == BIG_ENDIAN
    #define htobe16(x) (x)
    #define htole16(x) _byteswap_ushort(x)
    #define be16toh(x) (x)
    #define le16toh(x) _byteswap_ushort(x)
    
    #define htobe32(x) (x)
    #define htole32(x) _byteswap_ulong(x)
    #define be32toh(x) (x)
    #define le32toh(x) _byteswap_ulong(x)
    
    #define htobe64(x) (x)
    #define htole64(x) _byteswap_uint64(x)
    #define be64toh(x) (x)
    #define le64toh(x) _byteswap_uint64(x)
#else
    #error System byte order not supported
#endif

#endif