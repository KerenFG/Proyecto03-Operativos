#include <stdio.h>
#include "handlers.h"
#include "bucket.h"
#include "directory.h"
#include "free_space.h"
#include "net_utils.h"

void handle_rm(int fd, const RequestMeta *m, uint32_t flags, const char *sdir) {
    if (!bucket_exists(sdir, m->src_bucket)) {
        net_send_error(fd, STATUS_NOT_FOUND); return;
    }

    FILE *fp;
    if (bucket_open(sdir, m->src_bucket, &fp) != 0) {
        net_send_error(fd, STATUS_ERROR); return;
    }

    BucketHeader hdr;
    if (bucket_read_header(fp, &hdr) != 0) {
        fclose(fp); net_send_error(fd, STATUS_ERROR); return;
    }

    if (flags & FLAG_RECURSIVE) {
        uint64_t offsets[MAX_DIR_ENTRIES];
        uint64_t sizes[MAX_DIR_ENTRIES];
        int      count = 0;
        dir_remove_prefix(fp, &hdr, m->src_path,
                          offsets, sizes, &count, MAX_DIR_ENTRIES);
        for (int i = 0; i < count; i++)
            fs_free(fp, &hdr, offsets[i], sizes[i]);
    } else {
        int idx;
        DirectoryEntry e;
        if (dir_find(fp, m->src_path, &idx, &e) != 0) {
            fclose(fp); net_send_error(fd, STATUS_NOT_FOUND); return;
        }
        fs_free(fp, &hdr, e.offset, e.size);
        dir_remove(fp, &hdr, idx);
    }

    bucket_write_header(fp, &hdr);
    fclose(fp);
    net_send_ok(fd);
}
