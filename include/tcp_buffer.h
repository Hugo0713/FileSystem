#ifndef _TCP_BUFFER_
#define _TCP_BUFFER_

#define TCP_BUF_SIZE 4096

typedef struct tcp_buffer {
    int read_index;
    int write_index;
    char buf[TCP_BUF_SIZE];
} tcp_buffer;

/**
 * @brief  Initialize a buffer
 *
 * Initializes a buffer.
 *
 * @return tcp_buffer    created buffer
 */
tcp_buffer *init_buffer();

/**
 * @brief Append a string to the buffer
 *
 * Append a string to the buffer.
 * The string will be regarded as a whole data packet.
 * The first 4 bytes of the message will be the length of the message.
 *
 * @param  buf   buffer to be written
 * @param  s     string to be written
 * @param  len   length of the string
 */
void buffer_append(tcp_buffer *buf, const char *s, int len);

/**
 * @brief  Read to buffer
 *
 * Read all the data from the socket and write to the buffer.
 *
 * @param  buf     buffer to be written
 * @param  sockfd  socket to be read
 *
 * @return int     the number of bytes read, -1 if error or closed
 */
int read_to_buffer(tcp_buffer *buf, int sockfd);

/**
 * @brief  Send buffer
 *
 * Write all the data in the buffer to the socket.
 *
 * @param  buf     buffer to be read
 * @param  sockfd  socket to be written
 */
void send_buffer(tcp_buffer *buf, int sockfd);

/**
 * @brief  Adjust buffer
 *
 * If read_index is larger than TCP_BUF_SIZE / 2,
 * move the data to the beginning of the buffer.
 * Used after recycle_read and recycle_write.
 *
 * @param  buf   buffer to be adjusted
 */
void adjust_buffer(tcp_buffer *buf);

/**
 * @brief  Recycle write
 *
 * Move the write_index by len.
 *
 * @param  buf   buffer to be adjusted
 * @param  len   length to be moved
 */
void recycle_write(tcp_buffer *buf, int len);

/**
 * @brief  Recycle read
 *
 * Move the read_index by len.
 *
 * @param  buf   buffer to be adjusted
 * @param  len   length to be moved
 */
void recycle_read(tcp_buffer *buf, int len);


/**
 * @brief  Reply
 *
 * Append a string to the buffer.
 *
 * @param  buf   buffer to be written
 * @param  s     string to be written
 * @param  len   length of the string
 */
void reply(tcp_buffer *buf, const char *s, int len);

/**
 * @brief  Reply with "Yes"
 *
 * Append a string to the buffer.
 * The first 4 bytes of the message will be "Yes ".
 *
 * @param  buf   buffer to be written
 * @param  s     string to be written
 * @param  len   length of the string
 */

void reply_with_yes(tcp_buffer *buf, const char *s, int len);

/**
 * @brief  Reply with "No"
 *
 * Append a string to the buffer.
 * The first 3 bytes of the message will be "No ".
 *
 * @param  buf   buffer to be written
 * @param  s     string to be written
 * @param  len   length of the string
 */

void reply_with_no(tcp_buffer *buf, const char *s, int len);

#endif
