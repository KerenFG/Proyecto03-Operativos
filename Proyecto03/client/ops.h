#ifndef OPS_H
#define OPS_H

#include "cli.h"

typedef struct {
    const char *host;
    int         port;
} ServerCfg;

int op_ls  (const ServerCfg *cfg, const CliArgs *a);
int op_mb  (const ServerCfg *cfg, const CliArgs *a);
int op_cp  (const ServerCfg *cfg, const CliArgs *a);
int op_mv  (const ServerCfg *cfg, const CliArgs *a);
int op_rm  (const ServerCfg *cfg, const CliArgs *a);
int op_sync(const ServerCfg *cfg, const CliArgs *a);
int op_rb  (const ServerCfg *cfg, const CliArgs *a);

#endif
