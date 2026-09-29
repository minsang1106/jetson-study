CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g
BUILD_DIR = build

all: $(BUILD_DIR)/server $(BUILD_DIR)/client

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%: %.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $< $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
