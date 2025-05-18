#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "tcp_utils.h"

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

void on_connection(int id) {
    // some code that are executed when a new client is connected
}

int on_recv(int id, tcp_buffer *wb, char *msg, int len) {
    return 0;
}

void cleanup(int id) {
    // some code that are executed when a client is disconnected
}

FILE *log_file;

int main(int argc, char *argv[]) {
    log_init("fs.log");

    // command
    tcp_server server = server_init(666, 1, on_connection, on_recv, cleanup);
    server_run(server);

    // never reached
    log_close();
}
