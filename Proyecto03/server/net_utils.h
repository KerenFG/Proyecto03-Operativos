#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include "../common/protocol.h"

int  net_read_exact(int fd, void *buf, size_t len);
int  net_write_exact(int fd, const void *buf, size_t len);
void net_send_response(int fd, uint32_t status, const void *payload, uint32_t payload_len);
void net_send_ok(int fd);
void net_send_error(int fd, uint32_t status);

#endif
