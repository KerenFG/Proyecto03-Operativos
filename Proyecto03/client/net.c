#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "net.h"

int net_connect(const char *host, int port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) return -1;

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
        close(fd); freeaddrinfo(res); return -1;
    }
    freeaddrinfo(res);
    return fd;
}

int net_read_exact(int fd, void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, (char *)buf + total, len - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

int net_write_exact(int fd, const void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, (const char *)buf + total, len - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

int net_send_request(int fd, uint32_t opcode, uint32_t flags, const RequestMeta *meta) {
    RequestHeader hdr;
    hdr.magic       = PROTO_MAGIC;
    hdr.opcode      = opcode;
    hdr.flags       = flags;
    hdr.payload_len = (uint32_t)sizeof(RequestMeta) + (uint32_t)meta->file_size;
    if (net_write_exact(fd, &hdr, sizeof(hdr)) != 0) return -1;
    return net_write_exact(fd, meta, sizeof(RequestMeta));
}

int net_recv_response(int fd, ResponseHeader *rhdr) {
    return net_read_exact(fd, rhdr, sizeof(*rhdr));
}

void net_close(int fd) {
    close(fd);
}
