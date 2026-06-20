CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -pedantic -D_POSIX_C_SOURCE=200809L -Icommon
PREFIX  ?= /usr/local/bin

SERVER_SRCS = \
	server/main.c          \
	server/net_utils.c     \
	server/bucket.c        \
	server/directory.c     \
	server/free_space.c    \
	server/dispatcher.c    \
	server/handle_ls.c     \
	server/handle_mb.c     \
	server/handle_cp_put.c \
	server/handle_cp_get.c \
	server/handle_cp_cp.c  \
	server/handle_rm.c     \
	server/handle_rb.c

CLIENT_SRCS = \
	client/main.c \
	client/net.c  \
	client/cli.c  \
	client/ops.c

SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)
DEPS        = $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)

.PHONY: all clean install uninstall check storage

all: aws-s3_server aws-s3

aws-s3_server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

aws-s3: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compilación separada: genera .o y .d (dependencias de headers) juntos
%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Incluir dependencias automáticas de headers (silencia si no existen aún)
-include $(DEPS)

# Crea el directorio de almacenamiento por defecto
storage:
	mkdir -p buckets

install: all
	install -m 755 aws-s3_server $(PREFIX)/
	install -m 755 aws-s3        $(PREFIX)/

uninstall:
	rm -f $(PREFIX)/aws-s3_server $(PREFIX)/aws-s3

# Ejecuta el plan de pruebas automatizado (Fase 9)
check: all
	@bash tests/test_all.sh

clean:
	rm -f aws-s3_server aws-s3 $(SERVER_OBJS) $(CLIENT_OBJS) $(DEPS)
