#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include "handlers.h"
#include "bucket.h"
#include "directory.h"
#include "free_space.h"
#include "net_utils.h"

static void drain(int fd, uint64_t bytes) {
    char buf[CHUNK_SIZE];
    while (bytes > 0) {
        size_t n = bytes > CHUNK_SIZE ? CHUNK_SIZE : (size_t)bytes;
        if (net_read_exact(fd, buf, n) != 0) break;
        bytes -= n;
    }
}

void handle_cp_put(int fd, const RequestMeta *m, uint32_t flags, const char *sdir) {
    (void)flags;

    if (!bucket_exists(sdir, m->src_bucket)) {
        drain(fd, m->file_size);
        net_send_error(fd, STATUS_NOT_FOUND);
        return;
    }

    FILE *fp;
    if (bucket_open(sdir, m->src_bucket, &fp) != 0) {
        drain(fd, m->file_size);
        net_send_error(fd, STATUS_ERROR);
        return;
    }

    BucketHeader hdr;
    if (bucket_read_header(fp, &hdr) != 0) {
        fclose(fp); drain(fd, m->file_size);
        net_send_error(fd, STATUS_ERROR);
        return;
    }

    uint64_t fsize = m->file_size;
    int      exist_idx = -1;
    DirectoryEntry exist_e;
    int exists = (dir_find(fp, m->src_path, &exist_idx, &exist_e) == 0);

    uint64_t write_at;

    if (exists && exist_e.size == fsize) {
        write_at = exist_e.offset;
    } else {
        if (exists) fs_free(fp, &hdr, exist_e.offset, exist_e.size);
        write_at = fs_alloc(fp, &hdr, fsize);
        if (write_at == (uint64_t)-1) {
            write_at = hdr.bucket_size;
            hdr.bucket_size += fsize;
        }
    }

    if (fseek(fp, (long)write_at, SEEK_SET) != 0) {
        fclose(fp); drain(fd, fsize);
        net_send_error(fd, STATUS_ERROR);
        return;
    }

    char buf[CHUNK_SIZE];
    uint64_t remaining = fsize;
    while (remaining > 0) {
        size_t n = remaining > CHUNK_SIZE ? CHUNK_SIZE : (size_t)remaining;
        if (net_read_exact(fd, buf, n) != 0) {
            fclose(fp); net_send_error(fd, STATUS_ERROR); return;
        }
        if (fwrite(buf, 1, n, fp) != n) {
            fclose(fp); net_send_error(fd, STATUS_ERROR); return;
        }
        remaining -= n;
    }

    if (dir_add(fp, &hdr, m->src_path, write_at, fsize) != 0) {
        fclose(fp); net_send_error(fd, STATUS_ERROR); return;
    }
    bucket_write_header(fp, &hdr);
    fclose(fp);
    net_send_ok(fd);
}
