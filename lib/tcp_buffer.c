#include "tcp_buffer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

tcp_buffer *init_buffer()
{
    tcp_buffer *buf = malloc(sizeof(tcp_buffer));
    if (buf == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    buf->read_index = 0;
    buf->write_index = 0;
    return buf;
}

void adjust_buffer(tcp_buffer *buf)
{
    if (buf->read_index > TCP_BUF_SIZE / 2)
    {
        int len = buf->write_index - buf->read_index;
        if (buf->read_index >= 0 && buf->read_index <= buf->write_index && buf->write_index <= TCP_BUF_SIZE)
        {
            memmove(buf->buf, &buf->buf[buf->read_index], len);
        }
        else
        {
            fprintf(stderr, "Invalid buffer indices: read_index=%d, write_index=%d\n", buf->read_index,
                    buf->write_index);
            return;
        }
        buf->read_index = 0;
        buf->write_index = len;
    }
}

void recycle_write(tcp_buffer *buf, int len)
{
    int j = buf->write_index + len;
    if (j > TCP_BUF_SIZE)
    {
        fprintf(stderr, "recycle write error\n");
        return;
    }
    buf->write_index = j;
    adjust_buffer(buf);
}

void recycle_read(tcp_buffer *buf, int len)
{
    int j = buf->read_index + len;
    if (j > TCP_BUF_SIZE)
    {
        fprintf(stderr, "recycle read error\n");
        return;
    }
    buf->read_index = j;
    adjust_buffer(buf);
}

void buffer_append(tcp_buffer *buf, const char *s, int len)
{
    int writeable = TCP_BUF_SIZE - buf->write_index;
    if (len < 0)
    {
        fprintf(stderr, "invalid length: len cannot be negative\n");
        return;
    }
    if (writeable < len + 4)
    {
        fprintf(stderr, "write buffer full\n");
        return;
    }
    memcpy(&buf->buf[buf->write_index + 4], s, len);
    *(int *)&buf->buf[buf->write_index] = htonl(len);
    recycle_write(buf, len + 4);
}

int read_to_buffer(tcp_buffer *buf, int sockfd)
{
    int read_all = 0;
    int close_flag = 0;
    int count = 0;
    while (!read_all)
    {
        int writeable = TCP_BUF_SIZE - buf->write_index;
        if (writeable == 0)
        {
            fprintf(stderr, "read buffer full\n");
            break;
        }
        int ret = recv(sockfd, &buf->buf[buf->write_index], writeable, 0);
        if (ret > 0)
        {
            recycle_write(buf, ret);
        }
        else if (ret < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
        {
            // non-blocking recv, continue
            continue;
        }
        else
        { // ret <= 0, close
            close_flag = 1;
            break;
        }
        count += ret;
        if (ret < writeable)
        {
            read_all = 1;
            break;
        }
        else if (ret == writeable)
        {
            // not read all
            continue;
        }
    }
    if (close_flag)
    {
        return -1;
    }
    return count;
}

void send_buffer(tcp_buffer *buf, int sockfd)
{
    while (buf->write_index > buf->read_index)
    {
        int readable = buf->write_index - buf->read_index;
        int ret = send(sockfd, &buf->buf[buf->read_index], readable, 0);
        if (ret <= 0)
        {
            perror("send()");
            break;
        }
        recycle_read(buf, ret);
    }
}

inline void reply(tcp_buffer *buf, const char *s, int len) { buffer_append(buf, s, len); }

void reply_with_yes(tcp_buffer *buf, const char *s, int len)
{
    int writeable = TCP_BUF_SIZE - buf->write_index;
    if (len < 0)
    {
        fprintf(stderr, "invalid length: len cannot be negative\n");
        return;
    }
    len += 4; // add 4 bytes for "Yes "
    if (writeable < len + 4)
    {
        fprintf(stderr, "write buffer full\n");
        return;
    }
    memcpy(&buf->buf[buf->write_index + 4], "Yes ", 4);
    if (len > 4)
        memcpy(&buf->buf[buf->write_index + 8], s, len - 4);
    *(int *)&buf->buf[buf->write_index] = htonl(len);
    recycle_write(buf, len + 4);
}

void reply_with_no(tcp_buffer *buf, const char *s, int len)
{
    int writeable = TCP_BUF_SIZE - buf->write_index;
    if (len < 0)
    {
        fprintf(stderr, "invalid length: len cannot be negative\n");
        return;
    }
    len += 3; // add 3 bytes for "No "
    if (writeable < len + 4)
    {
        fprintf(stderr, "write buffer full\n");
        return;
    }
    memcpy(&buf->buf[buf->write_index + 4], "No ", 3);
    if (len > 3)
        memcpy(&buf->buf[buf->write_index + 7], s, len - 3);
    *(int *)&buf->buf[buf->write_index] = htonl(len);
    recycle_write(buf, len + 4);
}
