#ifndef DISPATCHER_H
#define DISPATCHER_H

/*
 * Módulo: dispatcher
 * Responsabilidad: leer una RequestHeader + RequestMeta del socket y
 *                  enrutar al handler correspondiente según el opcode.
 *
 * Contrato:
 *   - Asume que client_fd es un socket TCP válido y abierto.
 *   - Si el magic no coincide o el opcode es desconocido, envía STATUS_ERROR
 *     y retorna sin cerrar el socket (el cierre es responsabilidad del caller).
 *   - storage_dir debe apuntar al directorio de almacenamiento de buckets.
 */
void dispatch(int client_fd, const char *storage_dir);

#endif
