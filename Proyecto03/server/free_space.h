#ifndef FREE_SPACE_H
#define FREE_SPACE_H

#include <stdio.h>
#include "../common/types.h"

uint64_t fs_alloc(FILE *fp, BucketHeader *hdr, uint64_t size);
int      fs_free(FILE *fp, BucketHeader *hdr, uint64_t offset, uint64_t size);

#endif
