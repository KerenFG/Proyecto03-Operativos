#include <unistd.h>
#include <string.h>
#include "net_utils.h"

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

void net_send_response(int fd, uint32_t status, const void *payload, uint32_t payload_len) {
    ResponseHeader hdr;
    hdr.status      = status;
    hdr.payload_len = payload_len;
    hdr.reserved    = 0;
    net_write_exact(fd, &hdr, sizeof(hdr));
    if (payload && payload_len > 0)
        net_write_exact(fd, payload, payload_len);
}

void net_send_ok(int fd) {
    net_send_response(fd, STATUS_OK, NULL, 0);
}

void net_send_error(int fd, uint32_t status) {
    net_send_response(fd, status, NULL, 0);
}
