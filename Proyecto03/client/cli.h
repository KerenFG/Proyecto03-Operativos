#ifndef CLI_H
#define CLI_H

#define CMD_LS   1
#define CMD_MB   2
#define CMD_CP   3
#define CMD_MV   4
#define CMD_RM   5
#define CMD_SYNC 6
#define CMD_RB   7

typedef struct {
    int  cmd;
    char src[512];
    char dst[512];
    int  recursive;
    int  delete_flag;
    int  force;
} CliArgs;

int  cli_parse(int argc, char *argv[], CliArgs *out);
int  is_s3_uri(const char *s);
void parse_s3_uri(const char *uri, char *bucket, size_t blen, char *path, size_t plen);
const char *path_basename(const char *path);

#endif
