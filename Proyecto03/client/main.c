#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli.h"
#include "ops.h"
#include "../common/protocol.h"

int main(int argc, char *argv[]) {
    CliArgs args;
    if (cli_parse(argc, argv, &args) != 0) {
        fprintf(stderr,
            "uso: aws-s3 <comando> [args] [--recursive] [--delete] [--force]\n"
            "comandos: ls  mb  cp  mv  rm  sync  rb\n");
        return 1;
    }

    const char *host = getenv("AWS_S3_HOST");
    if (!host) host = "127.0.0.1";

    const char *port_str = getenv("AWS_S3_PORT");
    int port = port_str ? atoi(port_str) : DEFAULT_PORT;

    ServerCfg cfg = { host, port };

    switch (args.cmd) {
        case CMD_LS:   return op_ls  (&cfg, &args);
        case CMD_MB:   return op_mb  (&cfg, &args);
        case CMD_CP:   return op_cp  (&cfg, &args);
        case CMD_MV:   return op_mv  (&cfg, &args);
        case CMD_RM:   return op_rm  (&cfg, &args);
        case CMD_SYNC: return op_sync(&cfg, &args);
        case CMD_RB:   return op_rb  (&cfg, &args);
        default:       return 1;
    }
}
