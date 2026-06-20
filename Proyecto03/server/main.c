#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/protocol.h"
#include "dispatcher.h"

static void usage(const char *prog) {
    fprintf(stderr, "uso: %s [--port PORT] [--storage-dir DIR]\n", prog);
    exit(1);
}

int main(int argc, char *argv[]) {
    int         port        = DEFAULT_PORT;
    const char *storage_dir = "./buckets";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--storage-dir") == 0 && i + 1 < argc) {
            storage_dir = argv[++i];
        } else {
            usage(argv[0]);
        }
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); return 1;
    }
    if (listen(server_fd, 16) < 0) {
        perror("listen"); close(server_fd); return 1;
    }

    printf("aws-s3_server escuchando en puerto %d, almacenamiento: %s\n", port, storage_dir);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }

        dispatch(client_fd, storage_dir);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
