#ifndef BUCKET_H
#define BUCKET_H

#include <stdio.h>
#include "../common/types.h"

/* Operaciones básicas de archivo .bucket */
void bucket_filepath(const char *storage_dir, const char *name, char *out, size_t len);
int  bucket_exists(const char *storage_dir, const char *name);
int  bucket_create(const char *storage_dir, const char *name);
int  bucket_open(const char *storage_dir, const char *name, FILE **fp);
int  bucket_read_header(FILE *fp, BucketHeader *hdr);
int  bucket_write_header(FILE *fp, const BucketHeader *hdr);
int  bucket_delete(const char *storage_dir, const char *name);

/* Fase 4: validación e integridad */
int  bucket_validate(FILE *fp);
int  bucket_open_validated(const char *storage_dir, const char *name, FILE **fp);

/* Fase 4: estadísticas del bucket */
int  bucket_get_stats(const char *storage_dir, const char *name, BucketStats *stats);

#endif
