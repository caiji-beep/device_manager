#include "serial_port.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <sys/select.h>

/* ==========================================================
 * [内部辅助函数] 获取波特率对应的底层宏定义
 * ========================================================== */
static speed_t get_baud_flag(int baud)
{
    // 在 Linux 的 termios 结构体系中，波特率不能直接赋数字，必须转换成 B 开头的宏
    switch (baud) {
    case 1200:   return B1200;
    case 2400:   return B2400;
    case 4800:   return B4800;
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    default:     return 0; // 不支持的波特率返回 0
    }
}

/* ==========================================================
 * [底层 API] 打开串口设备
 * ========================================================== */
int serial_open(const char *dev)
{
    // O_RDWR:   可读可写模式
    // O_NOCTTY: 核心防坑！告诉内核“这个串口只是普通数据通道，千万别把它当成主控制终端”。
    //           否则串口收到 Ctrl+C (0x03) 等特殊字符时，系统可能会直接杀掉你的进程。
    int fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("open serial device failed");
        return -1;
    }
    return fd;
}

/* ==========================================================
 * [底层 API] 配置串口核心参数 (波特率、数据位、校验位、停止位)
 * 这是整个 Linux 串口编程中最复杂、最容易踩坑的地方
 * ========================================================== */
int serial_config(int fd, int baud, int databits, char parity, int stopbits)
{
    struct termios tty;
    speed_t speed_flag = get_baud_flag(baud);

    if (speed_flag == 0) {
        fprintf(stderr, "unsupported baud rate: %d\n", baud);
        return -1;
    }

    // 1. 获取当前串口的配置，基于当前配置进行修改，而不是凭空捏造
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr failed");
        return -1;
    }

    /* 2. 强行进入“原始模式 (Raw Mode)” */
    // 剥离所有的“人类终端打字机”属性，把串口变成纯粹的二进制数据管道
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // 关闭按行缓冲、关闭回显、关闭信号(如Ctrl+C)
    tty.c_oflag &= ~OPOST;                          // 关闭输出处理(不要自作聪明地把 \n 变成 \r\n)
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);         // 关闭软件流控(XON/XOFF)
    tty.c_iflag &= ~(INLCR | ICRNL | IGNCR);        // 关闭回车符与换行符的自动转换
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP); // 关闭各种输入错误拦截和第8位剥离
    tty.c_cflag |= (CLOCAL | CREAD);                // CLOCAL: 忽略调制解调器控制线；CREAD: 开启接收器

    /* 3. 配置数据位 (先清零 CSIZE 掩码，再设置新的值) */
    tty.c_cflag &= ~CSIZE;
    switch (databits) {
    case 7:
        tty.c_cflag |= CS7;
        break;
    case 8:
        tty.c_cflag |= CS8;
        break;
    default:
        fprintf(stderr, "unsupported data bits: %d\n", databits);
        return -1;
    }

    /* 4. 配置奇偶校验位 */
    switch (parity) {
    case 'N': case 'n': // 无校验 (None)
        tty.c_cflag &= ~PARENB; // 关校验使能
        tty.c_iflag &= ~INPCK;  // 关输入奇偶检查
        break;
    case 'E': case 'e': // 偶校验 (Even)
        tty.c_cflag |= PARENB;  // 开校验
        tty.c_cflag &= ~PARODD; // 清除奇校验标志(即设置为偶校验)
        tty.c_iflag |= INPCK;
        break;
    case 'O': case 'o': // 奇校验 (Odd)
        tty.c_cflag |= PARENB;  // 开校验
        tty.c_cflag |= PARODD;  // 设为奇校验
        tty.c_iflag |= INPCK;
        break;
    default:
        fprintf(stderr, "unsupported parity: %c\n", parity);
        return -1;
    }

    /* 5. 配置停止位 */
    switch (stopbits) {
    case 1:
        tty.c_cflag &= ~CSTOPB; // 清除2个停止位标志(即设置为1个停止位)
        break;
    case 2:
        tty.c_cflag |= CSTOPB;  // 设置为2个停止位
        break;
    default:
        fprintf(stderr, "unsupported stop bits: %d\n", stopbits);
        return -1;
    }

    /* 6. 设置输入输出波特率 */
    if (cfsetispeed(&tty, speed_flag) != 0) {
        perror("cfsetispeed failed");
        return -1;
    }
    if (cfsetospeed(&tty, speed_flag) != 0) {
        perror("cfsetospeed failed");
        return -1;
    }

    /* 7. 配置非阻塞读取模式 (配合 select 使用的关键) */
    tty.c_cc[VMIN]  = 0; // 最少读取字节数设为 0
    tty.c_cc[VTIME] = 0; // 读取超时设为 0。这两个为 0 意味着 read() 无论有没有数据都会立刻返回

    /* 8. 刷新旧数据并立即生效新配置 */
    if (tcflush(fd, TCIOFLUSH) != 0) { // 把设置生效前残留在缓冲区里的垃圾数据冲刷掉
        perror("tcflush failed");
        return -1;
    }

    if (tcsetattr(fd, TCSANOW, &tty) != 0) { // TCSANOW 表示立刻让配置生效
        perror("tcsetattr failed");
        return -1;
    }

    return 0;
}

/* ==========================================================
 * [底层 API] 前哨雷达：利用 select 阻塞等待数据到来
 * 返回 1 表示有数据可读，返回 0 表示超时，返回 -1 表示出错
 * ========================================================== */
int serial_wait_readable(int fd, int timeout_ms)
{
    fd_set readfds;
    struct timeval tv;
    int ret;

    FD_ZERO(&readfds);      // 清空监控清单
    FD_SET(fd, &readfds);   // 把我们的串口 fd 加入监控清单

    // 毫秒转为秒和微秒
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    // 让出 CPU 阻塞等待，直到串口来数据或者超时
    ret = select(fd + 1, &readfds, NULL, NULL, &tv);
    if (ret < 0) {
        perror("select failed");
        return -1;
    }
    if (ret == 0) {
        return 0; // 闹钟响了，还没数据
    }
    
    // 安全检查：确认唤醒我们的真的是这个串口 fd
    return FD_ISSET(fd, &readfds) ? 1 : 0;
}

/* ==========================================================
 * [底层 API] 非阻塞尝试读取部分数据
 * ========================================================== */
ssize_t serial_read_some(int fd, void *buf, size_t len)
{
    // 因为底层配置了 VMIN=0/VTIME=0，所以这里的 read 是瞬间返回的
    ssize_t n = read(fd, buf, len);
    if (n < 0) {
        perror("read failed");
        return -1;
    }
    return n;
}

/* ==========================================================
 * [底层 API] 坚决发送完所有数据
 * ========================================================== */
int serial_write_all(int fd, const void *buf, size_t len)
{
    const unsigned char *p = (const unsigned char *)buf;
    size_t total = 0;
    ssize_t n;

    // 核心防坑：Linux 的 write() 不保证一次性把所有数据发完
    // 所以必须用 while 循环，发了多少，指针就往后移多少，直到发完为止
    while (total < len) {
        n = write(fd, p + total, len - total);
        if (n < 0) {
            perror("write failed");
            return -1;
        }
        total += (size_t)n;
    }

    return 0;
}

/* ==========================================================
 * [底层 API] 发送字符串包装函数
 * ========================================================== */
int serial_send_text(int fd, const char *text)
{
    return serial_write_all(fd, text, strlen(text));
}

/* ==========================================================
 * [底层 API] 接收完整的一行 (以 \n 结束)
 * 注意：这里的 timeout_ms 实际上是“字节间超时”，每次收到一个字节都会重置闹钟
 * ========================================================== */
ssize_t serial_recv_line(int fd, char *buf, size_t maxlen, int timeout_ms)
{
    size_t total = 0;
    int ret;
    char ch;
    ssize_t n;

    if (buf == NULL || maxlen < 2) {
        return -1; // 缓冲区无效或太小
    }

    // 留出最后 1 个位置给字符串结束符 '\0'
    while (total < maxlen - 1) {
        // 1. 放出雷达去侦测，带有 timeout_ms 的倒计时
        ret = serial_wait_readable(fd, timeout_ms);
        if (ret < 0) {
            return -1; // 侦测出错
        }
        if (ret == 0) {
            // 超时处理分支
            if (total == 0) {
                return 0;   // 连第一个字节都没收到，纯粹的总体超时
            }
            break;          // 已经收到了一半的数据，但是对方迟迟不发下半句，认定接收结束
        }

        // 2. 雷达报告有数据，瞬间读 1 个字节出来
        n = serial_read_some(fd, &ch, 1);
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            continue; // 极小概率事件：雷达误报，继续侦测
        }

        // 3. 把收到的字节放进缓冲区，并将游标后移
        buf[total++] = ch;

        // 4. 判断是否遇到了行结束符 (文本协议的核心)
        if (ch == '\n') {
            break;
        }
    }

    // 手动加上 C 语言标准的字符串结束符，方便上层用 printf 等函数打印
    buf[total] = '\0';
    return (ssize_t)total;
}

/* ==========================================================
 * [底层 API] 安全关闭串口
 * ========================================================== */
void serial_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

/* ==========================================================
 * [工具 API] 将二进制数据以十六进制打印 (调试神器)
 * ========================================================== */
void hex_dump(const unsigned char *buf, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        printf("%02X ", buf[i]);
    }
    printf("\n");
}