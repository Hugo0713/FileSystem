#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "disk.h"
#include "log.h"
#include "tcp_utils.h"

int handle_i(tcp_buffer *wb, char *args, int len)
{
    int ncyl, nsec;
    cmd_i(&ncyl, &nsec);
    static char buf[64];
    sprintf(buf, "%d %d", ncyl, nsec);

    // including the null terminator
    reply(wb, buf, strlen(buf) + 1);
    return 0;
}

int handle_r(tcp_buffer *wb, char *args, int len)
{
    int cyl;
    int sec;
    char buf[512];
    if (cmd_r(cyl, sec, buf) == 0)
    {
        reply_with_yes(wb, buf, 512);
    }
    else
    {
        reply_with_no(wb, NULL, 0);
    }
    return 0;
}

int handle_w(tcp_buffer *wb, char *args, int len)
{
    int cyl;
    int sec;
    int datalen;
    char *data;

    if (cmd_w(cyl, sec, datalen, data) == 0)
    {
        reply_with_yes(wb, NULL, 0);
    }
    else
    {
        reply_with_no(wb, NULL, 0);
    }
    return 0;
}

int handle_e(tcp_buffer *wb, char *args, int len)
{
    const char *msg = "Bye!";
    reply(wb, msg, strlen(msg) + 1);
    return -1;
}

static struct
{
    const char *name;
    int (*handler)(tcp_buffer *wb, char *, int);
} cmd_table[] = {
    {"I", handle_i},
    {"R", handle_r},
    {"W", handle_w},
    {"E", handle_e},
};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

void on_connection(int id)
{
    // some code that are executed when a new client is connected
    // you don't need this now
}

int on_recv(int id, tcp_buffer *wb, char *msg, int len)
{
    char *p = strtok(msg, " \r\n");
    int ret = 1;
    for (int i = 0; i < NCMD; i++)
        if (p && strcmp(p, cmd_table[i].name) == 0)
        {
            ret = cmd_table[i].handler(wb, p + strlen(p) + 1, len - strlen(p) - 1);
            break;
        }
    if (ret == 1)
    {
        static char unk[] = "Unknown command";
        buffer_append(wb, unk, sizeof(unk));
    }
    if (ret < 0)
    {
        return -1;
    }
    return 0;
}

void cleanup(int id)
{
    // some code that are executed when a client is disconnected
    // you don't need this now
}

FILE *log_file;

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        fprintf(stderr,
                "Usage: %s <disk file name> <cylinders> <sector per cylinder> "
                "<track-to-track delay> <port>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    // args
    char *filename = argv[1];
    int ncyl = atoi(argv[2]);
    int nsec = atoi(argv[3]);
    int ttd = atoi(argv[4]); // ms
    int port = atoi(argv[5]);

    log_init("disk.log");

    int ret = init_disk(filename, ncyl, nsec, ttd);
    if (ret != 0)
    {
        fprintf(stderr, "Failed to initialize disk\n");
        exit(EXIT_FAILURE);
    }

    // command
    tcp_server server = server_init(port, 1, on_connection, on_recv, cleanup);
    server_run(server);

    // never reached
    close_disk();
    log_close();
}
