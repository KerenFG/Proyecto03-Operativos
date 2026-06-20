#include <string.h>
#include "free_space.h"

static long fb_file_offset(int index) {
    return (long)(BUCKET_FREE_OFFSET + (uint64_t)index * sizeof(FreeBlock));
}

static int fb_read(FILE *fp, int index, FreeBlock *fb) {
    if (fseek(fp, fb_file_offset(index), SEEK_SET) != 0) return -1;
    return fread(fb, sizeof(*fb), 1, fp) == 1 ? 0 : -1;
}

static int fb_write(FILE *fp, int index, const FreeBlock *fb) {
    if (fseek(fp, fb_file_offset(index), SEEK_SET) != 0) return -1;
    return fwrite(fb, sizeof(*fb), 1, fp) == 1 ? 0 : -1;
}

uint64_t fs_alloc(FILE *fp, BucketHeader *hdr, uint64_t size) {
    FreeBlock fb;
    for (int i = 0; i < MAX_FREE_BLOCKS; i++) {
        if (fb_read(fp, i, &fb) != 0) continue;
        if (!fb.active || fb.size < size) continue;

        uint64_t alloc_at = fb.offset;
        if (fb.size == size) {
            fb.active = 0;
            fb_write(fp, i, &fb);
            if (hdr->free_count > 0) hdr->free_count--;
        } else {
            fb.offset += size;
            fb.size   -= size;
            fb_write(fp, i, &fb);
        }
        return alloc_at;
    }
    return (uint64_t)-1;
}

int fs_free(FILE *fp, BucketHeader *hdr, uint64_t offset, uint64_t size) {
    if (size == 0) return 0;
    FreeBlock fb;
    for (int i = 0; i < MAX_FREE_BLOCKS; i++) {
        if (fb_read(fp, i, &fb) != 0) return -1;
        if (!fb.active) {
            memset(&fb, 0, sizeof(fb));
            fb.offset = offset;
            fb.size   = size;
            fb.active = 1;
            if (fb_write(fp, i, &fb) != 0) return -1;
            hdr->free_count++;
            return 0;
        }
    }
    return -1;
}
