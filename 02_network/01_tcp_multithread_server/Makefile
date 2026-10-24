CC = gcc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g
BUILD_DIR = build
SOURCES = main.c server.c connection.c client_manager.c message_queue.c broadcast.c
HEADERS = server.h connection.h client_manager.h message_queue.h broadcast.h

all: $(BUILD_DIR)/server $(BUILD_DIR)/client

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/server: $(SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/client: client.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
