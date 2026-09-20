CC = gcc
CFLAGS = -Wall -Wextra -g

BUILD_DIR = build

all: $(BUILD_DIR)/server \
     $(BUILD_DIR)/client \
     $(BUILD_DIR)/htonl_test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/server: server.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/client: client.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/htonl_test: htonl_test.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean