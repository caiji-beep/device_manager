#include "serial_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

/* ==========================================================
 * [底层动作执行区] -> 相当于餐厅的“后厨操作台”
 * 这里是纯软件逻辑与物理硬件接轨的地方
 * ========================================================== */

static int control_led(int on)
{
    /* TODO: 真实的物理驱动调用
     * 在 Linux 开发板上，通常会在这里操作 sysfs 文件系统：
     * 1) int fd = open("/sys/class/gpio/gpioXXX/value", O_WRONLY);
     * 2) write(fd, on ? "1" : "0", 1);
     * 3) close(fd);
     */
    printf("[ACTION] LED -> %s\n", on ? "ON" : "OFF");
    return 0;
}

static int control_beep(int on)
{
    /* TODO: 真实的蜂鸣器驱动调用方式同上 */
    printf("[ACTION] BEEP -> %s\n", on ? "ON" : "OFF");
    return 0;
}

/* ==========================================================
 * [数据清洗区] -> 相当于把客人点菜单上的涂抹痕迹擦干净
 * ========================================================== */

/* 清洗行尾：砍掉字符串末尾的所有回车、换行、空格和制表符 */
static void trim_line(char *s)
{
    size_t len;

    if (s == NULL) {
        return;
    }

    len = strlen(s);
    // 从字符串最后一个字符开始往前检查，只要是空白/换行符，就强行改成结束符 '\0'
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' ||
                       s[len - 1] == ' '  || s[len - 1] == '\t')) {
        s[len - 1] = '\0';
        len--;
    }
}

/* 统一小写：将字符串全部转成小写，实现指令的大小写不敏感，提高系统容错率 */
static void str_to_lower(char *s)
{
    while (*s) {
        *s = (char)tolower((unsigned char)*s);
        s++;
    }
}

/* ==========================================================
 * [业务逻辑路由区] -> 相当于餐厅的“大脑”，根据菜单决定做什么
 * ========================================================== */

/*
 * 处理命令并生成回复
 * cmd: 清洗干净的纯文本指令
 * resp: 准备回复给串口的字符串缓冲区
 * resp_size: 缓冲区大小，防止内存溢出
 * led_state/beep_state: 记录当前硬件状态的指针
 */
static int handle_command(const char *cmd, char *resp, size_t resp_size,
                          int *led_state, int *beep_state)
{
    // 1. Ping 测试指令
    if (strcmp(cmd, "ping") == 0) {
        snprintf(resp, resp_size, "PONG\r\n");
        return 0;
    }

    // 2. LED 控制指令
    if (strcmp(cmd, "led on") == 0) {
        if (control_led(1) == 0) {
            *led_state = 1; // 硬件操作成功，更新本地状态机
            snprintf(resp, resp_size, "OK led on\r\n"); // 组装成功回复
        } else {
            snprintf(resp, resp_size, "ERR led on\r\n"); // 组装失败回复
        }
        return 0;
    }

    if (strcmp(cmd, "led off") == 0) {
        if (control_led(0) == 0) {
            *led_state = 0;
            snprintf(resp, resp_size, "OK led off\r\n");
        } else {
            snprintf(resp, resp_size, "ERR led off\r\n");
        }
        return 0;
    }

    // 3. 蜂鸣器控制指令
    if (strcmp(cmd, "beep on") == 0) {
        if (control_beep(1) == 0) {
            *beep_state = 1;
            snprintf(resp, resp_size, "OK beep on\r\n");
        } else {
            snprintf(resp, resp_size, "ERR beep on\r\n");
        }
        return 0;
    }

    if (strcmp(cmd, "beep off") == 0) {
        if (control_beep(0) == 0) {
            *beep_state = 0;
            snprintf(resp, resp_size, "OK beep off\r\n");
        } else {
            snprintf(resp, resp_size, "ERR beep off\r\n");
        }
        return 0;
    }

    // 4. 状态查询指令
    if (strcmp(cmd, "status") == 0) {
        snprintf(resp, resp_size, "STATUS led=%d beep=%d\r\n", *led_state, *beep_state);
        return 0;
    }

    // 5. 兜底处理：如果不认识这个指令
    snprintf(resp, resp_size, "ERR unknown cmd\r\n");
    return 0;
}

/* 帮助菜单：当用户输入参数错误时提示 */
static void print_usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s -d <device> [-b baud] [-t timeout_ms]\n", prog);
    printf("\n");
    printf("Example:\n");
    printf("  %s -d /dev/ttymxc2 -b 115200 -t 30000\n", prog);
}

/* ==========================================================
 * [主程序入口] -> 迎宾与大堂经理
 * ========================================================== */
int main(int argc, char *argv[])
{
    const char *device = NULL;
    int baud = 115200;
    int timeout_ms = 30000; // 默认 30 秒超时
    int fd;
    int opt;

    char line[256]; // 接收指令的缓冲区
    char resp[256]; // 回复消息的缓冲区

    // 系统状态机初始化
    int led_state = 0;
    int beep_state = 0;

    /* 1. 解析命令行参数 (getopt 标准用法) */
    while ((opt = getopt(argc, argv, "d:b:t:")) != -1) {
        switch (opt) {
        case 'd': device = optarg; break;
        case 'b': baud = atoi(optarg); break;
        case 't': timeout_ms = atoi(optarg); break;
        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    // 强制要求必须指定设备路径
    if (device == NULL) {
        print_usage(argv[0]);
        return -1;
    }

    /* 2. 底层硬件初始化 */
    printf("open device: %s\n", device);
    fd = serial_open(device); // 以 O_RDWR | O_NOCTTY 模式打开
    if (fd < 0) {
        return -1;
    }

    if (serial_config(fd, baud, 8, 'N', 1) != 0) { // 配置 8N1 原始非阻塞模式
        serial_close(fd);
        return -1;
    }

    printf("serial config ok: %d 8N1\n", baud);
    printf("serial command service started.\n");
    printf("waiting commands...\n");

    /* 3. 开门迎客，主动向对端发送就绪信号 */
    serial_send_text(fd, "READY\r\n");

    /* 4. 进入后台守护进程循环 (Event Loop) */
    while (1) {
        // [阻塞等待] 这里的 serial_recv_line 底层使用了 select，不会占用 CPU
        // 它会一直等，直到收到换行符 '\n' 或者超时
        ssize_t n = serial_recv_line(fd, line, sizeof(line), timeout_ms);
        
        // A. 底层发生致命错误，跳出循环
        if (n < 0) {
            fprintf(stderr, "recv line failed\n");
            break;
        }

        // B. 规定时间内心跳超时，继续回去等（防呆设计）
        if (n == 0) {
            /* 超时，这里可以继续等，不退出 */
            continue;
        }

        // C. 成功收到完整的一行数据！
        printf("raw recv (%ld bytes): %s", (long)n, line);

        /* 5. 数据清洗 */
        trim_line(line);      // 砍掉尾巴的 \r, \n, 空格
        str_to_lower(line);   // 全部转成小写

        /* 6. 空指令拦截（防御性编程）*/
        // 如果用户只按了一个回车，清洗后就变成了空字符串，直接跳过，防止报错
        if (line[0] == '\0') {
            continue;
        }

        // 巧用中括号 [] 打印，如果指令里夹杂了看不见的空格，能立刻显形
        printf("parsed cmd: [%s]\n", line);

        /* 7. 优雅的后门退出机制 (The Escape Hatch) */
        // 捕获到 quit 指令时，发回确认并主动打破死循环
        if (strcmp(line, "quit") == 0) {
            snprintf(resp, sizeof(resp), "BYE\r\n");
            serial_send_text(fd, resp);
            break; 
        }

        /* 8. 核心业务路由分发 */
        // 把干净的指令交给后厨，执行动作并生成回复字符串
        handle_command(line, resp, sizeof(resp), &led_state, &beep_state);

        /* 9. 结果回传 */
        printf("send resp: %s", resp);
        serial_send_text(fd, resp);
        
    } // 回到 while 循环开头，继续等待下一条指令

    /* 10. 善后工作，安全退出 */
    // 只有发生底层错误，或者用户发送了 'quit' 打破循环后，才会执行到这里
    serial_close(fd);
    return 0;
}