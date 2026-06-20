#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "types.h"

#define PROTO_MAGIC     0x41574353u
#define DEFAULT_PORT    8080

#define OP_LS           1
#define OP_MB           2
#define OP_CP_PUT       3
#define OP_CP_GET       4
#define OP_CP_CP        5
#define OP_RM           6
#define OP_RB           7

#define FLAG_RECURSIVE  0x01u
#define FLAG_DELETE     0x02u
#define FLAG_FORCE      0x04u

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t opcode;
    uint32_t flags;
    uint32_t payload_len;
} RequestHeader;

typedef struct __attribute__((packed)) {
    uint32_t status;
    uint32_t payload_len;
    uint32_t reserved;
} ResponseHeader;

typedef struct __attribute__((packed)) {
    char     src_bucket[MAX_BUCKET_NAME];
    char     src_path[MAX_OBJECT_PATH];
    char     dst_bucket[MAX_BUCKET_NAME];
    char     dst_path[MAX_OBJECT_PATH];
    uint64_t file_size;
} RequestMeta;

#endif
