CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -std=c17 -Iinclude
BUILD_DIR := build

.PHONY: all mensagens memoria completo clean

all: mensagens

mensagens: $(BUILD_DIR)/mpmc-msg

memoria: $(BUILD_DIR)/mpmc-shm

completo: $(BUILD_DIR)/mpmc

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/mpmc-msg: src/main.c src/mensagens/mensagens.c src/stubs/memoria_stub.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/mpmc-shm: src/main.c src/memoria/memoria.c src/stubs/mensagens_stub.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/mpmc: src/main.c src/mensagens/mensagens.c src/memoria/memoria.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)
