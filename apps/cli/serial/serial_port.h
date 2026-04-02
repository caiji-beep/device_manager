#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include <stddef.h>
#include <sys/types.h>

int serial_open(const char *dev);
int serial_config(int fd, int baud, int databits, char parity, int stopbits);
int serial_wait_readable(int fd, int timeout_ms);
ssize_t serial_read_some(int fd, void *buf, size_t len);
int serial_write_all(int fd, const void *buf, size_t len);
int serial_send_text(int fd, const char *text);

/* 按行接收：
 * 读取直到遇到 '\n'，或者超时
 * 返回值：
 *  >0 : 实际读取字节数
 *   0 : 超时
 *  -1 : 出错
 */
ssize_t serial_recv_line(int fd, char *buf, size_t maxlen, int timeout_ms);

void serial_close(int fd);
void hex_dump(const unsigned char *buf, int len);

#endif