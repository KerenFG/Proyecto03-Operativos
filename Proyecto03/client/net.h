#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stddef.h>
#include "../common/protocol.h"

int  net_connect(const char *host, int port);
int  net_read_exact(int fd, void *buf, size_t len);
int  net_write_exact(int fd, const void *buf, size_t len);
int  net_send_request(int fd, uint32_t opcode, uint32_t flags, const RequestMeta *meta);
int  net_recv_response(int fd, ResponseHeader *rhdr);
void net_close(int fd);

#endif
