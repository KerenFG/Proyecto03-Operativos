#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "bucket.h"

void bucket_filepath(const char *storage_dir, const char *name, char *out, size_t len) {
    snprintf(out, len, "%s/%s.bucket", storage_dir, name);
}

int bucket_exists(const char *storage_dir, const char *name) {
    char path[768];
    struct stat st;
    bucket_filepath(storage_dir, name, path, sizeof(path));
    return stat(path, &st) == 0;
}

int bucket_create(const char *storage_dir, const char *name) {
    char path[768];
    bucket_filepath(storage_dir, name, path, sizeof(path));

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    BucketHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, BUCKET_MAGIC_STR, 8);
    hdr.version       = BUCKET_VERSION;
    hdr.dir_capacity  = MAX_DIR_ENTRIES;
    hdr.dir_count     = 0;
    hdr.free_capacity = MAX_FREE_BLOCKS;
    hdr.free_count    = 0;
    hdr.data_start    = BUCKET_DATA_START;
    hdr.bucket_size   = BUCKET_DATA_START;
    hdr.created_at    = (int64_t)time(NULL);
    strncpy(hdr.name, name, sizeof(hdr.name) - 1);

    if (fwrite(&hdr, sizeof(hdr), 1, fp) != 1) { fclose(fp); return -1; }

    DirectoryEntry de;
    memset(&de, 0, sizeof(de));
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (fwrite(&de, sizeof(de), 1, fp) != 1) { fclose(fp); return -1; }
    }

    FreeBlock fb;
    memset(&fb, 0, sizeof(fb));
    for (int i = 0; i < MAX_FREE_BLOCKS; i++) {
        if (fwrite(&fb, sizeof(fb), 1, fp) != 1) { fclose(fp); return -1; }
    }

    fclose(fp);
    return 0;
}

int bucket_open(const char *storage_dir, const char *name, FILE **fp) {
    char path[768];
    bucket_filepath(storage_dir, name, path, sizeof(path));
    *fp = fopen(path, "r+b");
    return *fp ? 0 : -1;
}

int bucket_read_header(FILE *fp, BucketHeader *hdr) {
    rewind(fp);
    return fread(hdr, sizeof(*hdr), 1, fp) == 1 ? 0 : -1;
}

int bucket_write_header(FILE *fp, const BucketHeader *hdr) {
    rewind(fp);
    return fwrite(hdr, sizeof(*hdr), 1, fp) == 1 ? 0 : -1;
}

int bucket_delete(const char *storage_dir, const char *name) {
    char path[768];
    bucket_filepath(storage_dir, name, path, sizeof(path));
    return unlink(path);
}

/* ── Fase 4: validación ─────────────────────────────────────────── */

int bucket_validate(FILE *fp) {
    BucketHeader hdr;
    if (bucket_read_header(fp, &hdr) != 0) return -1;
    if (memcmp(hdr.magic, BUCKET_MAGIC_STR, 8) != 0) return -1;
    if (hdr.version    != BUCKET_VERSION)    return -1;
    if (hdr.data_start != BUCKET_DATA_START) return -1;
    return 0;
}

/* Abre y valida en un solo paso; cierra el archivo si la validación falla. */
int bucket_open_validated(const char *storage_dir, const char *name, FILE **fp) {
    if (bucket_open(storage_dir, name, fp) != 0) return -1;
    if (bucket_validate(*fp) != 0) {
        fclose(*fp);
        *fp = NULL;
        return -1;
    }
    return 0;
}

/* ── Fase 4: estadísticas ───────────────────────────────────────── */

int bucket_get_stats(const char *storage_dir, const char *name, BucketStats *stats) {
    FILE *fp;
    if (bucket_open_validated(storage_dir, name, &fp) != 0) return -1;

    BucketHeader hdr;
    if (bucket_read_header(fp, &hdr) != 0) { fclose(fp); return -1; }

    memset(stats, 0, sizeof(*stats));
    strncpy(stats->name, name, MAX_BUCKET_NAME - 1);
    stats->object_count     = hdr.dir_count;
    stats->free_block_count = hdr.free_count;
    stats->total_file_size  = hdr.bucket_size;
    stats->created_at       = hdr.created_at;

    /* Suma tamaños de bloques libres leyendo directamente la tabla de FreeBlock. */
    uint64_t free_sum = 0;
    for (int i = 0; i < MAX_FREE_BLOCKS; i++) {
        FreeBlock fb;
        long off = (long)(BUCKET_FREE_OFFSET + (uint64_t)i * sizeof(FreeBlock));
        if (fseek(fp, off, SEEK_SET) != 0) continue;
        if (fread(&fb, sizeof(fb), 1, fp) != 1) continue;
        if (fb.active) free_sum += fb.size;
    }

    stats->data_free = free_sum;
    stats->data_used = (hdr.bucket_size > BUCKET_DATA_START + free_sum)
                     ? (hdr.bucket_size - BUCKET_DATA_START - free_sum)
                     : 0;

    fclose(fp);
    return 0;
}
