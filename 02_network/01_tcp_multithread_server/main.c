#include "server.h"
#include <stdlib.h>

int main(void) {
    server_t server;
    if (initialize_server(&server) < 0) return EXIT_FAILURE;
    int result = run_server(&server);
    cleanup_server(&server);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
