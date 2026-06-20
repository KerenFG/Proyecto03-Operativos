#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <stdio.h>
#include "../common/types.h"

int  dir_read_entry(FILE *fp, int index, DirectoryEntry *entry);
int  dir_write_entry(FILE *fp, int index, const DirectoryEntry *entry);
int  dir_find(FILE *fp, const char *path, int *out_index, DirectoryEntry *out_entry);
int  dir_add(FILE *fp, BucketHeader *hdr, const char *path, uint64_t offset, uint64_t size);
int  dir_remove(FILE *fp, BucketHeader *hdr, int index);
void dir_list_prefix(FILE *fp, const BucketHeader *hdr, const char *prefix,
                     char *buf, size_t buf_len);
int  dir_remove_prefix(FILE *fp, BucketHeader *hdr, const char *prefix,
                       uint64_t *out_offsets, uint64_t *out_sizes, int *out_count,
                       int capacity);

#endif
