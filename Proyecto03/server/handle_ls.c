#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "handlers.h"
#include "bucket.h"
#include "directory.h"
#include "net_utils.h"

#define LS_BUF 131072

/*
 * Formato del payload de respuesta:
 *   Lista de buckets : "nombre\tcreated_at\n"
 *   Lista de objetos : "relpath\tsize\tmodified_at\n"
 *
 * El cliente interpreta y formatea estos campos para mostrarlos al usuario.
 */

static void list_buckets(int fd, const char *sdir) {
    DIR *dir = opendir(sdir);
    if (!dir) { net_send_error(fd, STATUS_ERROR); return; }

    char buf[LS_BUF];
    size_t w = 0;
    buf[0] = '\0';

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        size_t nlen = strlen(ent->d_name);
        if (nlen <= 7) continue;
        if (strcmp(ent->d_name + nlen - 7, ".bucket") != 0) continue;

        char bname[MAX_BUCKET_NAME];
        size_t blen = nlen - 7;
        if (blen >= MAX_BUCKET_NAME) blen = MAX_BUCKET_NAME - 1;
        memcpy(bname, ent->d_name, blen);
        bname[blen] = '\0';

        /* Leer created_at del header del bucket */
        int64_t created_at = 0;
        FILE *bfp;
        if (bucket_open(sdir, bname, &bfp) == 0) {
            BucketHeader hdr;
            if (bucket_read_header(bfp, &hdr) == 0)
                created_at = hdr.created_at;
            fclose(bfp);
        }

        size_t rem = LS_BUF - w - 1;
        int n = snprintf(buf + w, rem, "%s\t%lld\n", bname, (long long)created_at);
        if (n > 0 && (size_t)n < rem) w += (size_t)n;
    }
    closedir(dir);
    net_send_response(fd, STATUS_OK, buf, (uint32_t)w);
}

void handle_ls(int fd, const RequestMeta *m, uint32_t flags, const char *sdir) {
    (void)flags;

    if (m->src_bucket[0] == '\0') {
        list_buckets(fd, sdir);
        return;
    }

    if (!bucket_exists(sdir, m->src_bucket)) {
        net_send_error(fd, STATUS_NOT_FOUND);
        return;
    }

    FILE *fp;
    if (bucket_open_validated(sdir, m->src_bucket, &fp) != 0) {
        net_send_error(fd, STATUS_ERROR);
        return;
    }

    BucketHeader hdr;
    if (bucket_read_header(fp, &hdr) != 0) {
        fclose(fp);
        net_send_error(fd, STATUS_ERROR);
        return;
    }

    char buf[LS_BUF];
    dir_list_prefix(fp, &hdr, m->src_path, buf, sizeof(buf));
    fclose(fp);
    net_send_response(fd, STATUS_OK, buf, (uint32_t)strlen(buf));
}
