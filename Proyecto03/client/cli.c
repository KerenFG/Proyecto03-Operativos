#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cli.h"

int is_s3_uri(const char *s) {
    return strncmp(s, "s3://", 5) == 0;
}

void parse_s3_uri(const char *uri, char *bucket, size_t blen, char *path, size_t plen) {
    const char *rest  = uri + 5;
    const char *slash = strchr(rest, '/');
    if (!slash) {
        strncpy(bucket, rest, blen - 1);
        bucket[blen - 1] = '\0';
        path[0] = '\0';
    } else {
        size_t n = (size_t)(slash - rest);
        if (n >= blen) n = blen - 1;
        memcpy(bucket, rest, n);
        bucket[n] = '\0';
        strncpy(path, slash + 1, plen - 1);
        path[plen - 1] = '\0';
    }
}

const char *path_basename(const char *path) {
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

int cli_parse(int argc, char *argv[], CliArgs *out) {
    if (argc < 2) return -1;

    memset(out, 0, sizeof(*out));

    const char *cmd = argv[1];
    if      (strcmp(cmd, "ls")   == 0) out->cmd = CMD_LS;
    else if (strcmp(cmd, "mb")   == 0) out->cmd = CMD_MB;
    else if (strcmp(cmd, "cp")   == 0) out->cmd = CMD_CP;
    else if (strcmp(cmd, "mv")   == 0) out->cmd = CMD_MV;
    else if (strcmp(cmd, "rm")   == 0) out->cmd = CMD_RM;
    else if (strcmp(cmd, "sync") == 0) out->cmd = CMD_SYNC;
    else if (strcmp(cmd, "rb")   == 0) out->cmd = CMD_RB;
    else { fprintf(stderr, "comando desconocido: %s\n", cmd); return -1; }

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--recursive") == 0) { out->recursive   = 1; continue; }
        if (strcmp(argv[i], "--delete")    == 0) { out->delete_flag = 1; continue; }
        if (strcmp(argv[i], "--force")     == 0) { out->force       = 1; continue; }
        if (out->src[0] == '\0') strncpy(out->src, argv[i], sizeof(out->src) - 1);
        else                     strncpy(out->dst, argv[i], sizeof(out->dst) - 1);
    }
    return 0;
}
