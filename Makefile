CC := gcc
CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS := -Wall -Wextra -Wpedantic -std=c17
BUILD_DIR := build
TARGET := $(BUILD_DIR)/mpmc

SRCS := src/main.c \
	src/mensagens/mensagens.c \
	src/memoria/memoria.c \
	src/memoria/memoria_buffer.c \
	src/memoria/memoria_ops.c

.PHONY: all clean

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRCS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)
