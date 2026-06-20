#ifndef HANDLERS_H
#define HANDLERS_H

#include <stdint.h>
#include "../common/protocol.h"

void handle_ls    (int fd, const RequestMeta *m, uint32_t flags, const char *sdir);
void handle_mb    (int fd, const RequestMeta *m, uint32_t flags, const char *sdir);
void handle_cp_put(int fd, const RequestMeta *m, uint32_t flags, const char *sdir);
void handle_cp_get(int fd, const RequestMeta *m, uint32_t flags, const char *sdir);
void handle_cp_cp (int fd, const RequestMeta *m, uint32_t flags, const char *sdir);
void handle_rm    (int fd, const RequestMeta *m, uint32_t flags, const char *sdir);
void handle_rb    (int fd, const RequestMeta *m, uint32_t flags, const char *sdir);

#endif
