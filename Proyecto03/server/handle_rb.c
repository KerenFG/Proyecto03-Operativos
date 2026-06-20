#include <stdio.h>
#include "handlers.h"
#include "bucket.h"
#include "directory.h"
#include "net_utils.h"

void handle_rb(int fd, const RequestMeta *m, uint32_t flags, const char *sdir) {
    if (!bucket_exists(sdir, m->src_bucket)) {
        net_send_error(fd, STATUS_NOT_FOUND); return;
    }

    if (!(flags & FLAG_FORCE)) {
        FILE *fp;
        if (bucket_open(sdir, m->src_bucket, &fp) != 0) {
            net_send_error(fd, STATUS_ERROR); return;
        }
        BucketHeader hdr;
        if (bucket_read_header(fp, &hdr) != 0) {
            fclose(fp); net_send_error(fd, STATUS_ERROR); return;
        }
        uint32_t count = hdr.dir_count;
        fclose(fp);
        if (count > 0) {
            net_send_error(fd, STATUS_NOT_EMPTY); return;
        }
    }

    if (bucket_delete(sdir, m->src_bucket) != 0) {
        net_send_error(fd, STATUS_ERROR); return;
    }
    net_send_ok(fd);
}
