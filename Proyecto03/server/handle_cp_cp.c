#include <stdio.h>
#include <string.h>
#include "handlers.h"
#include "bucket.h"
#include "directory.h"
#include "free_space.h"
#include "net_utils.h"

void handle_cp_cp(int fd, const RequestMeta *m, uint32_t flags, const char *sdir) {
    (void)flags;

    if (!bucket_exists(sdir, m->src_bucket)) { net_send_error(fd, STATUS_NOT_FOUND); return; }
    if (!bucket_exists(sdir, m->dst_bucket)) { net_send_error(fd, STATUS_NOT_FOUND); return; }

    FILE *src_fp, *dst_fp;
    if (bucket_open(sdir, m->src_bucket, &src_fp) != 0) {
        net_send_error(fd, STATUS_ERROR); return;
    }

    BucketHeader src_hdr;
    if (bucket_read_header(src_fp, &src_hdr) != 0) {
        fclose(src_fp); net_send_error(fd, STATUS_ERROR); return;
    }

    DirectoryEntry src_e;
    if (dir_find(src_fp, m->src_path, NULL, &src_e) != 0) {
        fclose(src_fp); net_send_error(fd, STATUS_NOT_FOUND); return;
    }

    if (bucket_open(sdir, m->dst_bucket, &dst_fp) != 0) {
        fclose(src_fp); net_send_error(fd, STATUS_ERROR); return;
    }

    BucketHeader dst_hdr;
    if (bucket_read_header(dst_fp, &dst_hdr) != 0) {
        fclose(src_fp); fclose(dst_fp); net_send_error(fd, STATUS_ERROR); return;
    }

    const char *dst_key = m->dst_path[0] != '\0' ? m->dst_path : m->src_path;

    int exist_idx;
    DirectoryEntry exist_e;
    int exists = (dir_find(dst_fp, dst_key, &exist_idx, &exist_e) == 0);

    uint64_t write_at;
    if (exists && exist_e.size == src_e.size) {
        write_at = exist_e.offset;
    } else {
        if (exists) fs_free(dst_fp, &dst_hdr, exist_e.offset, exist_e.size);
        write_at = fs_alloc(dst_fp, &dst_hdr, src_e.size);
        if (write_at == (uint64_t)-1) {
            write_at = dst_hdr.bucket_size;
            dst_hdr.bucket_size += src_e.size;
        }
    }

    if (fseek(src_fp, (long)src_e.offset, SEEK_SET) != 0 ||
        fseek(dst_fp, (long)write_at, SEEK_SET) != 0) {
        fclose(src_fp); fclose(dst_fp);
        net_send_error(fd, STATUS_ERROR); return;
    }

    char buf[CHUNK_SIZE];
    uint64_t remaining = src_e.size;
    while (remaining > 0) {
        size_t n  = remaining > CHUNK_SIZE ? CHUNK_SIZE : (size_t)remaining;
        size_t rd = fread(buf, 1, n, src_fp);
        if (rd == 0) break;
        if (fwrite(buf, 1, rd, dst_fp) != rd) {
            fclose(src_fp); fclose(dst_fp);
            net_send_error(fd, STATUS_ERROR); return;
        }
        remaining -= rd;
    }

    fclose(src_fp);
    dir_add(dst_fp, &dst_hdr, dst_key, write_at, src_e.size);
    bucket_write_header(dst_fp, &dst_hdr);
    fclose(dst_fp);
    net_send_ok(fd);
}
