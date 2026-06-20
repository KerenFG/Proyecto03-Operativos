#include <stdio.h>
#include <string.h>
#include "handlers.h"
#include "net_utils.h"
#include "../common/protocol.h"

void dispatch(int client_fd, const char *storage_dir) {
    RequestHeader rhdr;
    if (net_read_exact(client_fd, &rhdr, sizeof(rhdr)) != 0) return;

    if (rhdr.magic != PROTO_MAGIC) {
        net_send_error(client_fd, STATUS_ERROR);
        return;
    }

    RequestMeta meta;
    memset(&meta, 0, sizeof(meta));
    if (net_read_exact(client_fd, &meta, sizeof(meta)) != 0) {
        net_send_error(client_fd, STATUS_ERROR);
        return;
    }

    switch (rhdr.opcode) {
        case OP_LS:     handle_ls    (client_fd, &meta, rhdr.flags, storage_dir); break;
        case OP_MB:     handle_mb    (client_fd, &meta, rhdr.flags, storage_dir); break;
        case OP_CP_PUT: handle_cp_put(client_fd, &meta, rhdr.flags, storage_dir); break;
        case OP_CP_GET: handle_cp_get(client_fd, &meta, rhdr.flags, storage_dir); break;
        case OP_CP_CP:  handle_cp_cp (client_fd, &meta, rhdr.flags, storage_dir); break;
        case OP_RM:     handle_rm    (client_fd, &meta, rhdr.flags, storage_dir); break;
        case OP_RB:     handle_rb    (client_fd, &meta, rhdr.flags, storage_dir); break;
        default:        net_send_error(client_fd, STATUS_ERROR); break;
    }
}
