#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/input.h>

/**
 * argc:应用程序参数个数
 * argv[]:具体的参数内容，字符串形式
 * ./ap3216cAPP <filename>
 * ./ap3216cAPP /dev/ap3216c
 */

int main(int argc, char *argv[])
{
    int fd = 0, err = 0;
    const char *filename;
    filename = argv[1];
    unsigned short databuf[3];
    unsigned short ir, als, ps;

    if (argc != 2)
    {
        printf("Error usage\r\n");
        return -1;
    }

    fd = open(filename, O_RDWR);
    if (fd < 0)
    {
        printf("open failed\r\n");
        return -1;
    }
    while (1)
    {
        err = read(fd, &databuf, sizeof(databuf));
        if (err == sizeof(databuf))
        {
            ir = databuf[0];
            als = databuf[1];
            ps = databuf[2];
            printf("ir = %d, als = %d, ps = %d\r\n", ir, als, ps);
        }
        usleep(200000); /* 200ms */
    }

    // 4. 关闭文件
    err = close(fd);
    if (err < 0)
    {
        printf("close file failed\r\n");
        return -1;
    }
    return 0;
}