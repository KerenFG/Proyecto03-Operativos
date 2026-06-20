#include <stdio.h>
#include <string.h>
#include "handlers.h"
#include "bucket.h"
#include "directory.h"
#include "net_utils.h"

void handle_cp_get(int fd, const RequestMeta *m, uint32_t flags, const char *sdir) {
    (void)flags;

    if (!bucket_exists(sdir, m->src_bucket)) {
        net_send_error(fd, STATUS_NOT_FOUND);
        return;
    }

    FILE *fp;
    if (bucket_open(sdir, m->src_bucket, &fp) != 0) {
        net_send_error(fd, STATUS_ERROR);
        return;
    }

    BucketHeader hdr;
    if (bucket_read_header(fp, &hdr) != 0) {
        fclose(fp); net_send_error(fd, STATUS_ERROR); return;
    }

    DirectoryEntry e;
    if (dir_find(fp, m->src_path, NULL, &e) != 0) {
        fclose(fp); net_send_error(fd, STATUS_NOT_FOUND); return;
    }

    ResponseHeader rhdr;
    rhdr.status      = STATUS_OK;
    rhdr.payload_len = (uint32_t)e.size;
    rhdr.reserved    = 0;
    net_write_exact(fd, &rhdr, sizeof(rhdr));

    if (fseek(fp, (long)e.offset, SEEK_SET) != 0) {
        fclose(fp); return;
    }

    char buf[CHUNK_SIZE];
    uint64_t remaining = e.size;
    while (remaining > 0) {
        size_t n = remaining > CHUNK_SIZE ? CHUNK_SIZE : (size_t)remaining;
        size_t rd = fread(buf, 1, n, fp);
        if (rd == 0) break;
        if (net_write_exact(fd, buf, rd) != 0) break;
        remaining -= rd;
    }

    fclose(fp);
}
