CC := gcc
CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS := -Wall -Wextra -Wpedantic -std=c17
BUILD_DIR := build

MEMORIA_SRCS := src/memoria/memoria.c src/memoria/memoria_buffer.c src/memoria/memoria_ops.c

.PHONY: all mensagens memoria completo clean

all: mensagens

mensagens: $(BUILD_DIR)/mpmc-msg

memoria: $(BUILD_DIR)/mpmc-shm

completo: $(BUILD_DIR)/mpmc

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/mpmc-msg: src/main.c src/mensagens/mensagens.c src/stubs/memoria_stub.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/mpmc-shm: src/main.c $(MEMORIA_SRCS) src/stubs/mensagens_stub.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/mpmc: src/main.c src/mensagens/mensagens.c $(MEMORIA_SRCS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)
