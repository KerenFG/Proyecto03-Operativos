#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "directory.h"

static long de_file_offset(int index) {
    return (long)(BUCKET_DIR_OFFSET + (uint64_t)index * sizeof(DirectoryEntry));
}

int dir_read_entry(FILE *fp, int index, DirectoryEntry *entry) {
    if (fseek(fp, de_file_offset(index), SEEK_SET) != 0) return -1;
    return fread(entry, sizeof(*entry), 1, fp) == 1 ? 0 : -1;
}

int dir_write_entry(FILE *fp, int index, const DirectoryEntry *entry) {
    if (fseek(fp, de_file_offset(index), SEEK_SET) != 0) return -1;
    return fwrite(entry, sizeof(*entry), 1, fp) == 1 ? 0 : -1;
}

int dir_find(FILE *fp, const char *path, int *out_index, DirectoryEntry *out_entry) {
    DirectoryEntry e;
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (dir_read_entry(fp, i, &e) != 0) return -1;
        if (e.active && strcmp(e.path, path) == 0) {
            if (out_index) *out_index = i;
            if (out_entry) *out_entry = e;
            return 0;
        }
    }
    return -1;
}

int dir_add(FILE *fp, BucketHeader *hdr, const char *path, uint64_t offset, uint64_t size) {
    DirectoryEntry e;
    int existing;

    if (dir_find(fp, path, &existing, &e) == 0) {
        e.offset      = offset;
        e.size        = size;
        e.modified_at = (int64_t)time(NULL);
        return dir_write_entry(fp, existing, &e);
    }

    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (dir_read_entry(fp, i, &e) != 0) return -1;
        if (!e.active) {
            memset(&e, 0, sizeof(e));
            strncpy(e.path, path, MAX_OBJECT_PATH - 1);
            e.offset      = offset;
            e.size        = size;
            e.active      = 1;
            e.created_at  = (int64_t)time(NULL);
            e.modified_at = e.created_at;
            if (dir_write_entry(fp, i, &e) != 0) return -1;
            hdr->dir_count++;
            return 0;
        }
    }
    return -1;
}

int dir_remove(FILE *fp, BucketHeader *hdr, int index) {
    DirectoryEntry e;
    if (dir_read_entry(fp, index, &e) != 0) return -1;
    if (!e.active) return 0;
    e.active = 0;
    if (dir_write_entry(fp, index, &e) != 0) return -1;
    if (hdr->dir_count > 0) hdr->dir_count--;
    return 0;
}

void dir_list_prefix(FILE *fp, const BucketHeader *hdr, const char *prefix,
                     char *buf, size_t buf_len) {
    DirectoryEntry e;
    size_t plen    = strlen(prefix);
    size_t written = 0;
    buf[0] = '\0';

    for (uint32_t i = 0; i < hdr->dir_capacity && written + 2 < buf_len; i++) {
        if (dir_read_entry(fp, (int)i, &e) != 0) continue;
        if (!e.active) continue;
        if (plen > 0 && strncmp(e.path, prefix, plen) != 0) continue;

        const char *display = e.path + plen;
        size_t remaining    = buf_len - written - 1;
        int n = snprintf(buf + written, remaining, "%s\t%llu\t%lld\n",
                         display,
                         (unsigned long long)e.size,
                         (long long)e.modified_at);
        if (n > 0 && (size_t)n < remaining)
            written += (size_t)n;
    }
}

int dir_remove_prefix(FILE *fp, BucketHeader *hdr, const char *prefix,
                      uint64_t *out_offsets, uint64_t *out_sizes, int *out_count,
                      int capacity) {
    DirectoryEntry e;
    size_t plen = strlen(prefix);
    *out_count = 0;

    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (dir_read_entry(fp, i, &e) != 0) continue;
        if (!e.active) continue;
        if (plen > 0 && strncmp(e.path, prefix, plen) != 0) continue;
        if (plen == 0 || strncmp(e.path, prefix, plen) == 0) {
            if (*out_count < capacity) {
                out_offsets[*out_count] = e.offset;
                out_sizes[*out_count]   = e.size;
                (*out_count)++;
            }
            e.active = 0;
            dir_write_entry(fp, i, &e);
            if (hdr->dir_count > 0) hdr->dir_count--;
        }
    }
    return 0;
}
