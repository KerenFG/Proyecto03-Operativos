#include <sys/stat.h>
#include "handlers.h"
#include "bucket.h"
#include "net_utils.h"

void handle_mb(int fd, const RequestMeta *m, uint32_t flags, const char *sdir) {
    (void)flags;

    if (bucket_exists(sdir, m->src_bucket)) {
        net_send_error(fd, STATUS_EXISTS);
        return;
    }

    mkdir(sdir, 0755);

    if (bucket_create(sdir, m->src_bucket) != 0) {
        net_send_error(fd, STATUS_ERROR);
        return;
    }
    net_send_ok(fd);
}
