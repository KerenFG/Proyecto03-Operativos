#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include "ops.h"
#include "net.h"
#include "cli.h"
#include "../common/protocol.h"
#include "../common/types.h"

/* ── utilidades generales ────────────────────────────────────────── */

static const char *status_msg(uint32_t s) {
    switch (s) {
        case STATUS_OK:        return "OK";
        case STATUS_NOT_FOUND: return "no encontrado";
        case STATUS_EXISTS:    return "ya existe";
        case STATUS_NOT_EMPTY: return "bucket no vacio (usar --force)";
        case STATUS_FULL:      return "bucket lleno";
        default:               return "error del servidor";
    }
}

/* Convierte unix timestamp a "YYYY-MM-DD HH:MM:SS" en el buffer dado. */
static void format_ts(int64_t ts, char *buf, size_t len) {
    if (ts == 0) { snprintf(buf, len, "--------------------"); return; }
    time_t t = (time_t)ts;
    struct tm *tm = localtime(&t);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm);
}

static int simple_request(const ServerCfg *cfg, uint32_t opcode, uint32_t flags,
                           const RequestMeta *meta) {
    int fd = net_connect(cfg->host, cfg->port);
    if (fd < 0) { fprintf(stderr, "error: no se puede conectar al servidor\n"); return 1; }
    net_send_request(fd, opcode, flags, meta);
    ResponseHeader rh;
    int rc = 1;
    if (net_recv_response(fd, &rh) == 0) {
        rc = (rh.status == STATUS_OK) ? 0 : 1;
        if (rh.status != STATUS_OK)
            fprintf(stderr, "error: %s\n", status_msg(rh.status));
    }
    net_close(fd);
    return rc;
}

/* Crea directorios intermedios de una ruta si no existen. */
static void mkdir_parent(const char *path) {
    char tmp[768];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
}

/* Construye el path del objeto en el bucket. Si dst termina en '/',
   concatena el basename del origen; si no, usa dst tal cual. */
static void build_obj_path(const char *dst_path, const char *src_local,
                            char *out, size_t out_len) {
    size_t dlen = strlen(dst_path);
    if (dlen > 0 && dst_path[dlen - 1] == '/') {
        snprintf(out, out_len, "%s%s", dst_path, path_basename(src_local));
    } else if (dlen == 0) {
        strncpy(out, path_basename(src_local), out_len - 1);
        out[out_len - 1] = '\0';
    } else {
        strncpy(out, dst_path, out_len - 1);
        out[out_len - 1] = '\0';
    }
}

/* ── Fase 6: formato de salida del ls ───────────────────────────── */

/*
 * Formato del servidor:
 *   Lista de buckets : "nombre\tcreated_at\n"
 *   Lista de objetos : "relpath\tsize\tmodified_at\n"
 *
 * El cliente imprime columnas alineadas con timestamps legibles.
 */

static void print_bucket_listing(const char *buf, uint32_t len) {
    char line[512];
    const char *p = buf;
    const char *end = buf + len;

    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        size_t llen = nl ? (size_t)(nl - p) : (size_t)(end - p);
        if (llen == 0) { p = nl ? nl + 1 : end; continue; }

        memcpy(line, p, llen < sizeof(line) - 1 ? llen : sizeof(line) - 1);
        line[llen < sizeof(line) - 1 ? llen : sizeof(line) - 1] = '\0';

        char name[MAX_BUCKET_NAME];
        long long ts = 0;
        if (sscanf(line, "%127[^\t]\t%lld", name, &ts) >= 1) {
            char tsstr[24];
            format_ts((int64_t)ts, tsstr, sizeof(tsstr));
            printf("%s  %s\n", tsstr, name);
        }
        p = nl ? nl + 1 : end;
    }
}

static void print_object_listing(const char *buf, uint32_t len) {
    char line[640];
    const char *p = buf;
    const char *end = buf + len;

    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        size_t llen = nl ? (size_t)(nl - p) : (size_t)(end - p);
        if (llen == 0) { p = nl ? nl + 1 : end; continue; }

        memcpy(line, p, llen < sizeof(line) - 1 ? llen : sizeof(line) - 1);
        line[llen < sizeof(line) - 1 ? llen : sizeof(line) - 1] = '\0';

        char relpath[MAX_OBJECT_PATH];
        unsigned long long size = 0;
        long long ts = 0;
        if (sscanf(line, "%255[^\t]\t%llu\t%lld", relpath, &size, &ts) >= 2) {
            char tsstr[24];
            format_ts((int64_t)ts, tsstr, sizeof(tsstr));
            printf("%s  %10llu  %s\n", tsstr, size, relpath);
        }
        p = nl ? nl + 1 : end;
    }
}

/* ── primitivas de red ───────────────────────────────────────────── */

static int upload_file(const ServerCfg *cfg, const char *local_path,
                        const char *bucket, const char *obj_path) {
    FILE *f = fopen(local_path, "rb");
    if (!f) { fprintf(stderr, "error: no se puede abrir %s\n", local_path); return 1; }

    struct stat st;
    if (stat(local_path, &st) != 0) { fclose(f); return 1; }

    RequestMeta meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.src_bucket, bucket,   MAX_BUCKET_NAME - 1);
    strncpy(meta.src_path,   obj_path, MAX_OBJECT_PATH - 1);
    meta.file_size = (uint64_t)st.st_size;

    int fd = net_connect(cfg->host, cfg->port);
    if (fd < 0) { fclose(f); fprintf(stderr, "error: no se puede conectar\n"); return 1; }

    net_send_request(fd, OP_CP_PUT, 0, &meta);

    char buf[CHUNK_SIZE];
    size_t rd;
    while ((rd = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (net_write_exact(fd, buf, rd) != 0) {
            fclose(f); net_close(fd); return 1;
        }
    }
    fclose(f);

    ResponseHeader rh;
    int rc = 1;
    if (net_recv_response(fd, &rh) == 0) {
        rc = (rh.status == STATUS_OK) ? 0 : 1;
        if (rh.status != STATUS_OK)
            fprintf(stderr, "error subiendo %s: %s\n", obj_path, status_msg(rh.status));
    }
    net_close(fd);
    return rc;
}

static int download_file(const ServerCfg *cfg, const char *bucket,
                          const char *obj_path, const char *local_dst) {
    RequestMeta meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.src_bucket, bucket,   MAX_BUCKET_NAME - 1);
    strncpy(meta.src_path,   obj_path, MAX_OBJECT_PATH - 1);

    int fd = net_connect(cfg->host, cfg->port);
    if (fd < 0) { fprintf(stderr, "error: no se puede conectar\n"); return 1; }
    net_send_request(fd, OP_CP_GET, 0, &meta);

    ResponseHeader rh;
    if (net_recv_response(fd, &rh) != 0 || rh.status != STATUS_OK) {
        fprintf(stderr, "error descargando %s: %s\n",
                obj_path, status_msg(rh.status));
        net_close(fd); return 1;
    }

    mkdir_parent(local_dst);

    FILE *f = fopen(local_dst, "wb");
    if (!f) {
        fprintf(stderr, "error: no se puede crear %s\n", local_dst);
        net_close(fd); return 1;
    }

    char buf[CHUNK_SIZE];
    uint32_t remaining = rh.payload_len;
    while (remaining > 0) {
        size_t n = remaining > CHUNK_SIZE ? CHUNK_SIZE : (size_t)remaining;
        if (net_read_exact(fd, buf, n) != 0) break;
        fwrite(buf, 1, n, f);
        remaining -= (uint32_t)n;
    }
    fclose(f);
    net_close(fd);
    return 0;
}

/* Pide al servidor el listado de objetos bajo un prefijo.
   Formato de cada línea: "relpath\tsize\tmodified_at\n"          */
static int bucket_list(const ServerCfg *cfg, const char *bucket, const char *prefix,
                        char **out_buf, uint32_t *out_len) {
    RequestMeta meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.src_bucket, bucket, MAX_BUCKET_NAME - 1);
    strncpy(meta.src_path,   prefix, MAX_OBJECT_PATH - 1);

    int fd = net_connect(cfg->host, cfg->port);
    if (fd < 0) return -1;
    net_send_request(fd, OP_LS, 0, &meta);

    ResponseHeader rh;
    if (net_recv_response(fd, &rh) != 0 || rh.status != STATUS_OK) {
        net_close(fd); return -1;
    }
    *out_len = rh.payload_len;
    *out_buf = calloc(rh.payload_len + 1, 1);
    if (!*out_buf) { net_close(fd); return -1; }
    if (rh.payload_len > 0) net_read_exact(fd, *out_buf, rh.payload_len);
    net_close(fd);
    return 0;
}

static int remove_object(const ServerCfg *cfg, const char *bucket,
                          const char *obj_path, uint32_t flags) {
    RequestMeta meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.src_bucket, bucket,   MAX_BUCKET_NAME - 1);
    strncpy(meta.src_path,   obj_path, MAX_OBJECT_PATH - 1);
    return simple_request(cfg, OP_RM, flags, &meta);
}

/* ── cp ──────────────────────────────────────────────────────────── */

static int cp_local_to_bucket(const ServerCfg *cfg, const CliArgs *a) {
    char bucket[MAX_BUCKET_NAME], dst_path[MAX_OBJECT_PATH];
    parse_s3_uri(a->dst, bucket, sizeof(bucket), dst_path, sizeof(dst_path));
    char obj_path[MAX_OBJECT_PATH];
    build_obj_path(dst_path, a->src, obj_path, sizeof(obj_path));
    int rc = upload_file(cfg, a->src, bucket, obj_path);
    if (rc == 0) printf("upload: %s -> s3://%s/%s\n", a->src, bucket, obj_path);
    return rc;
}

static int cp_bucket_to_local(const ServerCfg *cfg, const CliArgs *a) {
    char bucket[MAX_BUCKET_NAME], obj_path[MAX_OBJECT_PATH];
    parse_s3_uri(a->src, bucket, sizeof(bucket), obj_path, sizeof(obj_path));
    char local_dst[512];
    size_t dlen = strlen(a->dst);
    if (dlen > 0 && a->dst[dlen - 1] == '/')
        snprintf(local_dst, sizeof(local_dst), "%s%s", a->dst, path_basename(obj_path));
    else {
        strncpy(local_dst, a->dst, sizeof(local_dst) - 1);
        local_dst[sizeof(local_dst) - 1] = '\0';
    }
    int rc = download_file(cfg, bucket, obj_path, local_dst);
    if (rc == 0) printf("download: s3://%s/%s -> %s\n", bucket, obj_path, local_dst);
    return rc;
}

static int cp_bucket_to_bucket(const ServerCfg *cfg, const CliArgs *a) {
    char src_bucket[MAX_BUCKET_NAME], src_path[MAX_OBJECT_PATH];
    char dst_bucket[MAX_BUCKET_NAME], dst_path[MAX_OBJECT_PATH];
    parse_s3_uri(a->src, src_bucket, sizeof(src_bucket), src_path, sizeof(src_path));
    parse_s3_uri(a->dst, dst_bucket, sizeof(dst_bucket), dst_path, sizeof(dst_path));
    char final_dst[MAX_OBJECT_PATH];
    build_obj_path(dst_path, src_path, final_dst, sizeof(final_dst));

    RequestMeta meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.src_bucket, src_bucket, MAX_BUCKET_NAME - 1);
    strncpy(meta.src_path,   src_path,   MAX_OBJECT_PATH - 1);
    strncpy(meta.dst_bucket, dst_bucket, MAX_BUCKET_NAME - 1);
    strncpy(meta.dst_path,   final_dst,  MAX_OBJECT_PATH - 1);

    int rc = simple_request(cfg, OP_CP_CP, 0, &meta);
    if (rc == 0)
        printf("copy: s3://%s/%s -> s3://%s/%s\n",
               src_bucket, src_path, dst_bucket, final_dst);
    return rc;
}

static int cp_recursive_local_bucket(const ServerCfg *cfg, const char *local_dir,
                                      const char *bucket, const char *prefix) {
    DIR *dir = opendir(local_dir);
    if (!dir) { fprintf(stderr, "error: no se puede abrir %s\n", local_dir); return 1; }
    int rc = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char local_path[768];
        snprintf(local_path, sizeof(local_path), "%s/%s", local_dir, ent->d_name);
        char obj_path[MAX_OBJECT_PATH];
        if (prefix[0] != '\0')
            snprintf(obj_path, sizeof(obj_path), "%s%s", prefix, ent->d_name);
        else
            strncpy(obj_path, ent->d_name, sizeof(obj_path) - 1);
        struct stat st;
        if (stat(local_path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            char sub[MAX_OBJECT_PATH];
            snprintf(sub, sizeof(sub), "%s/", obj_path);
            if (cp_recursive_local_bucket(cfg, local_path, bucket, sub) != 0) rc = 1;
        } else {
            if (upload_file(cfg, local_path, bucket, obj_path) == 0)
                printf("upload: %s -> s3://%s/%s\n", local_path, bucket, obj_path);
            else rc = 1;
        }
    }
    closedir(dir);
    return rc;
}

/* Fase 6: descarga recursiva bucket → local.
   Parsea listado "relpath\tsize\tmodified_at\n" y descarga cada objeto. */
static int cp_recursive_bucket_to_local(const ServerCfg *cfg, const char *bucket,
                                         const char *prefix, const char *local_dir) {
    char *list_buf = NULL;
    uint32_t list_len = 0;
    if (bucket_list(cfg, bucket, prefix, &list_buf, &list_len) != 0) {
        fprintf(stderr, "error: no se puede listar s3://%s/%s\n", bucket, prefix);
        return 1;
    }
    if (!list_buf || list_len == 0) { free(list_buf); return 0; }

    char *line = list_buf;
    int rc = 0;
    while (*line) {
        char *nl = strchr(line, '\n');
        if (!nl) break;
        *nl = '\0';

        char relpath[MAX_OBJECT_PATH];
        unsigned long long fsize = 0;
        if (sscanf(line, "%255[^\t]\t%llu", relpath, &fsize) >= 1) {
            char full_obj[MAX_OBJECT_PATH];
            if (prefix[0] != '\0')
                snprintf(full_obj, sizeof(full_obj), "%s%s", prefix, relpath);
            else
                strncpy(full_obj, relpath, sizeof(full_obj) - 1);

            char local_path[768];
            snprintf(local_path, sizeof(local_path), "%s/%s", local_dir, relpath);

            if (download_file(cfg, bucket, full_obj, local_path) == 0)
                printf("download: s3://%s/%s -> %s\n", bucket, full_obj, local_path);
            else rc = 1;
        }
        line = nl + 1;
    }
    free(list_buf);
    return rc;
}

/* ── Fase 6: sync con SyncContext ───────────────────────────────── */

/*
 * Recorre el directorio local de forma recursiva.
 * Busca en el listado del bucket si ya existe el archivo con igual tamaño.
 * Formato de búsqueda: "relpath\tsize\t" (el timestamp varía, se ignora).
 */
static void sync_walk(const ServerCfg *cfg, const char *local_dir,
                       const char *bucket, const char *prefix,
                       const char *rel_base, const char *list_buf,
                       SyncContext *ctx) {
    DIR *dir = opendir(local_dir);
    if (!dir) { fprintf(stderr, "error: no se puede abrir %s\n", local_dir); return; }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char local_path[768];
        snprintf(local_path, sizeof(local_path), "%s/%s", local_dir, ent->d_name);

        char rel_path[MAX_OBJECT_PATH];
        if (rel_base[0] != '\0')
            snprintf(rel_path, sizeof(rel_path), "%s/%s", rel_base, ent->d_name);
        else
            strncpy(rel_path, ent->d_name, sizeof(rel_path) - 1);

        struct stat st;
        if (stat(local_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            sync_walk(cfg, local_path, bucket, prefix, rel_path, list_buf, ctx);
            continue;
        }

        char obj_path[MAX_OBJECT_PATH];
        if (prefix[0] != '\0')
            snprintf(obj_path, sizeof(obj_path), "%s%s", prefix, rel_path);
        else
            strncpy(obj_path, rel_path, sizeof(obj_path) - 1);

        /* Buscar "relpath\tsize\t" en el listado (tamaño debe coincidir). */
        char search[MAX_OBJECT_PATH + 32];
        snprintf(search, sizeof(search), "%s\t%llu\t",
                 rel_path, (unsigned long long)st.st_size);
        int in_bucket = (list_buf && strstr(list_buf, search) != NULL);

        if (in_bucket) {
            ctx->skipped++;
        } else {
            if (upload_file(cfg, local_path, bucket, obj_path) == 0) {
                printf("  upload:  %s -> s3://%s/%s\n", local_path, bucket, obj_path);
                ctx->uploaded++;
            } else {
                ctx->errors++;
            }
        }
    }
    closedir(dir);
}

/* ── comandos públicos ───────────────────────────────────────────── */

int op_ls(const ServerCfg *cfg, const CliArgs *a) {
    RequestMeta meta;
    memset(&meta, 0, sizeof(meta));
    int is_object_listing = 0;

    if (a->src[0] != '\0' && is_s3_uri(a->src)) {
        parse_s3_uri(a->src, meta.src_bucket, sizeof(meta.src_bucket),
                     meta.src_path, sizeof(meta.src_path));
        is_object_listing = 1;
    }

    int fd = net_connect(cfg->host, cfg->port);
    if (fd < 0) { fprintf(stderr, "error: no se puede conectar\n"); return 1; }
    net_send_request(fd, OP_LS, 0, &meta);

    ResponseHeader rh;
    if (net_recv_response(fd, &rh) != 0 || rh.status != STATUS_OK) {
        fprintf(stderr, "error: %s\n", status_msg(rh.status));
        net_close(fd); return 1;
    }

    if (rh.payload_len > 0) {
        char *buf = malloc(rh.payload_len + 1);
        if (!buf) { net_close(fd); return 1; }
        net_read_exact(fd, buf, rh.payload_len);
        buf[rh.payload_len] = '\0';

        /* Fase 6: imprimir con formato alineado según el tipo de listado. */
        if (is_object_listing)
            print_object_listing(buf, rh.payload_len);
        else
            print_bucket_listing(buf, rh.payload_len);

        free(buf);
    }
    net_close(fd);
    return 0;
}

int op_mb(const ServerCfg *cfg, const CliArgs *a) {
    if (!is_s3_uri(a->src)) { fprintf(stderr, "error: se esperaba s3://bucket\n"); return 1; }
    RequestMeta meta;
    memset(&meta, 0, sizeof(meta));
    parse_s3_uri(a->src, meta.src_bucket, sizeof(meta.src_bucket),
                 meta.src_path, sizeof(meta.src_path));
    int rc = simple_request(cfg, OP_MB, 0, &meta);
    if (rc == 0) printf("make_bucket: s3://%s\n", meta.src_bucket);
    return rc;
}

int op_cp(const ServerCfg *cfg, const CliArgs *a) {
    int src_is_s3 = is_s3_uri(a->src);
    int dst_is_s3 = is_s3_uri(a->dst);

    if (!src_is_s3 && dst_is_s3) {
        if (a->recursive) {
            char bucket[MAX_BUCKET_NAME], dst_path[MAX_OBJECT_PATH];
            parse_s3_uri(a->dst, bucket, sizeof(bucket), dst_path, sizeof(dst_path));
            return cp_recursive_local_bucket(cfg, a->src, bucket, dst_path);
        }
        return cp_local_to_bucket(cfg, a);
    }
    if (src_is_s3 && !dst_is_s3) {
        if (a->recursive) {
            char bucket[MAX_BUCKET_NAME], src_path[MAX_OBJECT_PATH];
            parse_s3_uri(a->src, bucket, sizeof(bucket), src_path, sizeof(src_path));
            return cp_recursive_bucket_to_local(cfg, bucket, src_path, a->dst);
        }
        return cp_bucket_to_local(cfg, a);
    }
    if (src_is_s3 && dst_is_s3)
        return cp_bucket_to_bucket(cfg, a);

    fprintf(stderr, "error: al menos un argumento debe ser s3://\n");
    return 1;
}

int op_mv(const ServerCfg *cfg, const CliArgs *a) {
    int src_is_s3 = is_s3_uri(a->src);
    int dst_is_s3 = is_s3_uri(a->dst);
    int rc = op_cp(cfg, a);
    if (rc != 0) return rc;

    if (src_is_s3) {
        char bucket[MAX_BUCKET_NAME], obj_path[MAX_OBJECT_PATH];
        parse_s3_uri(a->src, bucket, sizeof(bucket), obj_path, sizeof(obj_path));
        rc = remove_object(cfg, bucket, obj_path, 0);
    } else if (dst_is_s3) {
        if (unlink(a->src) != 0) { perror(a->src); rc = 1; }
    }
    return rc;
}

int op_rm(const ServerCfg *cfg, const CliArgs *a) {
    if (!is_s3_uri(a->src)) { fprintf(stderr, "error: se esperaba s3://\n"); return 1; }
    char bucket[MAX_BUCKET_NAME], obj_path[MAX_OBJECT_PATH];
    parse_s3_uri(a->src, bucket, sizeof(bucket), obj_path, sizeof(obj_path));
    uint32_t flags = a->recursive ? FLAG_RECURSIVE : 0;
    int rc = remove_object(cfg, bucket, obj_path, flags);
    if (rc == 0) printf("delete: s3://%s/%s\n", bucket, obj_path);
    return rc;
}

int op_sync(const ServerCfg *cfg, const CliArgs *a) {
    if (!is_s3_uri(a->dst)) { fprintf(stderr, "error: dst debe ser s3://\n"); return 1; }

    char bucket[MAX_BUCKET_NAME], prefix[MAX_OBJECT_PATH];
    parse_s3_uri(a->dst, bucket, sizeof(bucket), prefix, sizeof(prefix));

    /* Fase 5: inicializar SyncContext */
    SyncContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    strncpy(ctx.bucket,    bucket,  MAX_BUCKET_NAME - 1);
    strncpy(ctx.prefix,    prefix,  MAX_OBJECT_PATH - 1);
    strncpy(ctx.local_dir, a->src,  MAX_LOCAL_PATH  - 1);
    ctx.delete_flag = a->delete_flag;

    printf("Sincronizando %s -> s3://%s/%s\n", a->src, bucket, prefix);

    char *list_buf = NULL;
    uint32_t list_len = 0;
    if (bucket_list(cfg, bucket, prefix, &list_buf, &list_len) != 0) {
        fprintf(stderr, "error: no se puede listar el bucket\n");
        return 1;
    }

    /* Subir archivos nuevos o modificados */
    sync_walk(cfg, a->src, bucket, prefix, "", list_buf, &ctx);

    /* --delete: eliminar del bucket lo que ya no existe localmente */
    if (a->delete_flag && list_buf && list_len > 0) {
        char *line = list_buf;
        while (*line) {
            char *nl = strchr(line, '\n');
            if (!nl) break;
            *nl = '\0';

            char relpath[MAX_OBJECT_PATH];
            unsigned long long fsize = 0;
            /* Parsear "relpath\tsize\tmodified_at" */
            if (sscanf(line, "%255[^\t]\t%llu", relpath, &fsize) >= 1) {
                char local_check[768];
                snprintf(local_check, sizeof(local_check), "%s/%s", a->src, relpath);
                struct stat st;
                int local_ok = (stat(local_check, &st) == 0 && !S_ISDIR(st.st_mode));

                if (!local_ok) {
                    char full_obj[MAX_OBJECT_PATH];
                    if (prefix[0] != '\0')
                        snprintf(full_obj, sizeof(full_obj), "%s%s", prefix, relpath);
                    else
                        strncpy(full_obj, relpath, sizeof(full_obj) - 1);

                    if (remove_object(cfg, bucket, full_obj, 0) == 0) {
                        printf("  delete:  s3://%s/%s\n", bucket, full_obj);
                        ctx.deleted++;
                    } else {
                        ctx.errors++;
                    }
                }
            }
            line = nl + 1;
        }
    }

    /* Fase 6: resumen de la operación */
    printf("---\n");
    printf("Subidos: %u  |  Eliminados: %u  |  Sin cambios: %u  |  Errores: %u\n",
           ctx.uploaded, ctx.deleted, ctx.skipped, ctx.errors);

    free(list_buf);
    return ctx.errors > 0 ? 1 : 0;
}

int op_rb(const ServerCfg *cfg, const CliArgs *a) {
    if (!is_s3_uri(a->src)) { fprintf(stderr, "error: se esperaba s3://bucket\n"); return 1; }
    char bucket[MAX_BUCKET_NAME], path[MAX_OBJECT_PATH];
    parse_s3_uri(a->src, bucket, sizeof(bucket), path, sizeof(path));
    RequestMeta meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.src_bucket, bucket, MAX_BUCKET_NAME - 1);
    uint32_t flags = a->force ? FLAG_FORCE : 0;
    int rc = simple_request(cfg, OP_RB, flags, &meta);
    if (rc == 0) printf("remove_bucket: s3://%s\n", bucket);
    return rc;
}
