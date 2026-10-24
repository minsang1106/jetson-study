#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

typedef struct {
    char data[BUFFER_SIZE];
    size_t length;
    bool discarding;
} input_t;

static int connect_to_server(const char *ip) {
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(PORT)};
    if (inet_pton(AF_INET, ip, &address.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", ip);
        return -1;
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    struct timeval timeout = {.tv_sec = 2};
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("SO_SNDTIMEO");
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

static int send_message(int fd, const char *data, size_t length) {
    size_t sent_total = 0;
    while (sent_total < length) {
        ssize_t sent = send(fd, data + sent_total, length - sent_total, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) {
            if (sent < 0) perror("send");
            return -1;
        }
        sent_total += (size_t)sent;
    }
    return 0;
}

/* Helpers return 1 to continue, 0 to quit, and -1 on error. */
static int submit_line(int fd, input_t *input) {
    size_t command_length = input->length;
    if (command_length && input->data[command_length - 1] == '\n') --command_length;
    if (command_length && input->data[command_length - 1] == '\r') --command_length;
    if (command_length == 5 && memcmp(input->data, "/quit", 5) == 0) return 0;
    if (send_message(fd, input->data, input->length) < 0) return -1;
    input->length = 0;
    return 1;
}

static int handle_input(int fd, input_t *input) {
    char buffer[BUFFER_SIZE];
    ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (count < 0) {
        if (errno == EINTR) return 1;
        perror("stdin");
        return -1;
    }
    if (count == 0) {
        if (!input->discarding && input->length && submit_line(fd, input) < 0) return -1;
        return 0;
    }
    for (ssize_t i = 0; i < count; ++i) {
        if (input->discarding) {
            if (buffer[i] == '\n') input->discarding = false;
            continue;
        }
        if (input->length == sizeof(input->data)) {
            fprintf(stderr, "Input too long (maximum %d bytes including newline); discarded.\n",
                    BUFFER_SIZE);
            input->length = 0;
            input->discarding = buffer[i] != '\n';
            continue;
        }
        input->data[input->length++] = buffer[i];
        if (buffer[i] == '\n') {
            int result = submit_line(fd, input);
            if (result <= 0) return result;
        }
    }
    return 1;
}

static int receive_message(int fd) {
    char buffer[BUFFER_SIZE];
    ssize_t received = recv(fd, buffer, sizeof(buffer), 0);
    if (received < 0) {
        if (errno == EINTR) return 1;
        perror("recv");
        return -1;
    }
    if (received == 0) {
        fprintf(stderr, "Server disconnected.\n");
        return 0;
    }
    if (fwrite(buffer, 1, (size_t)received, stdout) != (size_t)received || fflush(stdout) == EOF) {
        perror("stdout");
        return -1;
    }
    return 1;
}

static int run_client(int fd) {
    input_t input = {0};
    struct pollfd events[] = {
        {.fd = fd, .events = POLLIN},
        {.fd = STDIN_FILENO, .events = POLLIN}
    };
    for (;;) {
        if (poll(events, 2, -1) < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            return -1;
        }
        if ((events[0].revents | events[1].revents) & POLLNVAL) {
            fprintf(stderr, "Invalid socket or input descriptor.\n");
            return -1;
        }
        /* Drain readable server bytes even when POLLHUP is also reported. */
        if (events[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            int result = receive_message(fd);
            if (result <= 0) return result;
        }
        if (events[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            int result = handle_input(fd, &input);
            if (result <= 0) return result;
        }
    }
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [server IPv4 address]\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *ip = argc == 2 ? argv[1] : "127.0.0.1";
    int fd = connect_to_server(ip);
    if (fd < 0) return EXIT_FAILURE;
    fprintf(stderr, "Connected to %s:%d. Enter a message, /quit or Ctrl+D to leave.\n", ip, PORT);
    int result = run_client(fd);
    shutdown(fd, SHUT_RDWR);
    close(fd);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
