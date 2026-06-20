#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#define MAX_BUCKET_NAME     128
#define MAX_OBJECT_PATH     256
#define MAX_LOCAL_PATH      512
#define MAX_DIR_ENTRIES     256
#define MAX_FREE_BLOCKS     128
#define CHUNK_SIZE          65536
#define BUCKET_VERSION      1
#define BUCKET_MAGIC_STR    "AWSS3BKT"

#define STATUS_OK           0
#define STATUS_ERROR        1
#define STATUS_NOT_FOUND    2
#define STATUS_EXISTS       3
#define STATUS_NOT_EMPTY    4
#define STATUS_FULL         5

/* ── Fase 4: estructuras del modelo físico del bucket ───────────── */

/* Cabecera principal del archivo .bucket — offset 0, 128 bytes fijos. */
typedef struct __attribute__((packed)) {
    uint8_t  magic[8];       /* "AWSS3BKT"                          */
    uint32_t version;        /* siempre BUCKET_VERSION               */
    uint32_t dir_capacity;   /* MAX_DIR_ENTRIES (constante)          */
    uint32_t dir_count;      /* entradas de directorio activas       */
    uint32_t free_capacity;  /* MAX_FREE_BLOCKS (constante)          */
    uint32_t free_count;     /* bloques libres activos               */
    uint64_t data_start;     /* offset absoluto del área de datos    */
    uint64_t bucket_size;    /* tamaño total del archivo en bytes    */
    int64_t  created_at;     /* unix timestamp de creación           */
    char     name[64];       /* nombre del bucket                    */
    uint8_t  reserved[12];   /* reservado para versiones futuras     */
} BucketHeader;              /* total: 128 bytes                     */

/* Entrada de directorio interno — tabla de MAX_DIR_ENTRIES, 320 bytes c/u. */
typedef struct __attribute__((packed)) {
    char     path[MAX_OBJECT_PATH]; /* key del objeto (nul-term)     */
    uint64_t offset;                /* offset absoluto en el archivo */
    uint64_t size;                  /* tamaño en bytes               */
    int64_t  created_at;            /* unix timestamp                */
    int64_t  modified_at;           /* unix timestamp última escritura*/
    uint8_t  active;                /* 1=activo, 0=eliminado         */
    uint8_t  reserved[31];
} DirectoryEntry;            /* total: 320 bytes                     */

/* Bloque de espacio libre — tabla de MAX_FREE_BLOCKS, 24 bytes c/u. */
typedef struct __attribute__((packed)) {
    uint64_t offset;         /* offset absoluto del bloque libre     */
    uint64_t size;           /* tamaño en bytes                      */
    uint8_t  active;         /* 1=bloque válido                      */
    uint8_t  reserved[7];
} FreeBlock;                 /* total: 24 bytes                      */

/* Offsets fijos del layout del bucket (calculados en compilación). */
#define BUCKET_DIR_OFFSET  ((uint64_t)sizeof(BucketHeader))
#define BUCKET_FREE_OFFSET (BUCKET_DIR_OFFSET  + (uint64_t)MAX_DIR_ENTRIES * sizeof(DirectoryEntry))
#define BUCKET_DATA_START  (BUCKET_FREE_OFFSET + (uint64_t)MAX_FREE_BLOCKS  * sizeof(FreeBlock))

/* ── Fase 5: estructuras de soporte en memoria ──────────────────── */

/* Estadísticas de un bucket (en memoria, solo lectura). */
typedef struct {
    char     name[MAX_BUCKET_NAME];
    uint32_t object_count;    /* objetos activos                     */
    uint32_t free_block_count;/* bloques libres registrados          */
    uint64_t total_file_size; /* tamaño del archivo .bucket en disco */
    uint64_t data_used;       /* bytes ocupados por objetos activos  */
    uint64_t data_free;       /* bytes en bloques libres             */
    int64_t  created_at;      /* unix timestamp de creación          */
} BucketStats;

/* Contexto de una transferencia de datos en curso. */
typedef struct {
    int      net_fd;                /* descriptor de socket           */
    uint64_t total_bytes;           /* total de bytes a transferir    */
    uint64_t transferred;           /* bytes ya enviados/recibidos    */
    char     src_path[MAX_LOCAL_PATH];
    char     dst_path[MAX_LOCAL_PATH];
} TransferContext;

/* Contexto de una operación sync (acumula métricas). */
typedef struct {
    char     bucket[MAX_BUCKET_NAME];
    char     prefix[MAX_OBJECT_PATH];
    char     local_dir[MAX_LOCAL_PATH];
    int      delete_flag;
    uint32_t uploaded;              /* archivos subidos               */
    uint32_t deleted;               /* objetos eliminados del bucket  */
    uint32_t skipped;               /* sin cambios (mismo tamaño)     */
    uint32_t errors;                /* fallos de transferencia        */
} SyncContext;

#endif
