#include <stdio.h>  // 提供 printf 函数的声明
#include <string.h> // 提供 memset 函数的声明
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/types.h> /* for videodev2.h */
#include <linux/videodev2.h>
#include <sys/mman.h>   // 提供 mmap, munmap, PROT_READ, MAP_SHARED 等宏定义
#include <sys/select.h> // 提供 select, FD_ZERO, FD_SET 等宏和函数
#include <poll.h>
#include <pthread.h>

/* ./video_test </dev/video1>  */
// arm-linux-gnueabihf-gcc -o video_test video_test.c -lpthread

static void *thread_brightness_control(void *args)
{
    if (args == NULL)
        return NULL;
    int fd = *(int *)args;
    int c;
    int brightness;
    int delta;

    /*查询*/
    struct v4l2_queryctrl qctrl;
    memset(&qctrl, 0, sizeof(qctrl));
    qctrl.id = V4L2_CID_BRIGHTNESS; // V4L2_CID_BASE+0;
    if (0 != ioctl(fd, VIDIOC_QUERYCTRL, &qctrl))
    {
        printf("Error querying brightness control\n");
        return NULL;
    }
    delta = (qctrl.maximum - qctrl.minimum) / 10;
    printf("Brightness min = %d,max = %d\n", qctrl.minimum, qctrl.maximum);
    /*获得当前值*/
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_BRIGHTNESS; // V4L2_CID_BASE+0;
    if (0 != ioctl(fd, VIDIOC_G_CTRL, &ctl))
    {
        printf("Error getting brightness control value\n");
        return NULL;
    }
    while (1)
    {
        c = getchar();
        if (c == 'u' || c == 'U')
        {
            // Increase brightness
            ctl.value += delta;
        }
        else if (c == 'd' || c == 'D')
        {
            // Decrease brightness
            ctl.value -= delta;
        }
        else
        {
            continue;
        }
        if (ctl.value < qctrl.minimum)
            ctl.value = qctrl.minimum;
        if (ctl.value > qctrl.maximum)
            ctl.value = qctrl.maximum;
        if (0 != ioctl(fd, VIDIOC_S_CTRL, &ctl))
        {
            printf("Error setting brightness control value\n");
        }
    }
}

int main(int argc, char **argv)
{
    int fd;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum fsenum;
    struct v4l2_format fmt;
    struct v4l2_capability capability;
    struct pollfd fds[1];
    int fmt_index = 0;
    int frame_index = 0;
    int i;
    void *bufs[32];
    int buf_cnt;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    char filename[32];
    int file_cnt = 0;

    if (argc != 2)
    {
        printf("Usage: %s </dev/video1>,print format detail for video device\n", argv[0]);
        return -1;
    }

    /*open*/
    fd = open(argv[1], O_RDWR);
    if (fd < 0)
    {
        printf("can not open %s\n", argv[1]);
        return -1;
    }
    /*ioctl*/
    while (1)
    {
        /*枚举格式*/
        memset(&fmtdesc, 0, sizeof(struct v4l2_fmtdesc));
        fmtdesc.index = fmt_index;                  // 比如从0开始
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // 指定type为"捕获"
        if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0)
        {
            break;
        }
        frame_index = 0;
        while (1)
        {
            /*枚举这种格式所支持的帧大小*/
            memset(&fsenum, 0, sizeof(struct v4l2_frmsizeenum));
            fsenum.pixel_format = fmtdesc.pixelformat;
            fsenum.index = frame_index;

            if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsenum) == 0)
            {
                printf("format %s,%d, framesize %d: %d x %d\n", fmtdesc.description, fmtdesc.pixelformat, frame_index, fsenum.discrete.width, fsenum.discrete.height);
                //
                struct v4l2_frmivalenum fival;
                memset(&fival, 0, sizeof(fival));
                fival.index = 0; // 我们驱动里写了只支持 index 0 (即 30fps)
                fival.pixel_format = fmtdesc.pixelformat;
                fival.width = fsenum.discrete.width;
                fival.height = fsenum.discrete.height;

                // 向驱动发起 "火力侦察"，询问帧率
                if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival) == 0)
                {
                    if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
                    {
                        // 打印帧率 (分母 / 分子，比如 30 / 1 = 30 fps)
                        int fps = fival.discrete.denominator / fival.discrete.numerator;
                        printf("    -> Supported FPS: %d fps\n", fps);
                    }
                }
                else
                {
                    printf("fps enum error");
                }
            }
            else
            {
                break;
            }
            frame_index++;
        }
        fmt_index++;
    }

    /*查询能力*/
    memset(&capability, 0, sizeof(struct v4l2_capability));
    int ret = ioctl(fd, VIDIOC_QUERYCAP, &capability);
    if (ret < 0)
    {
        fprintf(stderr, "Error opening device %s: unable to query device.\n", argv[1]);
        return -1;
    }

    if ((capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0)
    {
        fprintf(stderr, "Error opening device %s: video capture not supported.\n",
                argv[1]);
        return -1;
    }

    if (!(capability.capabilities & V4L2_CAP_STREAMING))
    {
        fprintf(stderr, "%s does not support streaming i/o\n", argv[1]);
        return -1;
    }

    memset(&fmt, 0, sizeof(struct v4l2_format));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 1024;
    fmt.fmt.pix.height = 768;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (0 == ioctl(fd, VIDIOC_S_FMT, &fmt))
    {
        printf("set format success: %d x %d, pixel format: %d\n", fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.pixelformat);
    }
    else
    {
        printf("set format failed\n");
        return -1;
    }

    /*申请buffer*/
    struct v4l2_requestbuffers reqbufs;
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = 4;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &reqbufs) < 0)
    {
        printf("can not request buffer\n");
        perror("request buffer error");
        return -1;
    }
    else
    {
        /*申请成功后,map这些buffer*/
        buf_cnt = reqbufs.count;
        for (i = 0; i < reqbufs.count; i++)
        {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(struct v4l2_buffer));
            buf.index = i;
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            if (0 != ioctl(fd, VIDIOC_QUERYBUF, &buf)) // 查询结果会保存在buf中
            {
                printf("Unable to query buffer\n");
                return -1;
            }

            bufs[i] = mmap(0 /* start anywhere */,
                           buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                           buf.m.offset);
            if (bufs[i] == MAP_FAILED)
            {
                perror("Unable to map buffer");
                return -1;
            }
        }
        printf("map %d buffer success\n", buf_cnt);
    }

    /*把buffer放入"空闲链表"*/
    /*
     * Queue the buffers.
     */
    for (i = 0; i < buf_cnt; ++i)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (0 != ioctl(fd, VIDIOC_QBUF, &buf))
        {
            perror("Unable to queue buffer");
            return -1;
        }
    }
    printf("queue buffer success\n");

    /*启动摄像头*/

    if (0 != ioctl(fd, VIDIOC_STREAMON, &type))
    {
        perror("Unable to start capture");
        return -1;
    }
    printf("start capture success\n");

    /*创建线程来控制亮度*/
    pthread_t thread;
    if (pthread_create(&thread, NULL, thread_brightness_control, &fd) != 0)
    {
        printf("Failed to create thread\n");
    }

    /*使用poll/select监测buffer，然后从"完成链表"中取出buffer，处理后再放入"空闲链表"
    ** poll/select
    * ioctl VIDIOC_DQBUF：从"完成链表"中取出buffer
    * 处理：前面使用mmap映射了每个buffer的地址，把这个buffer的数据存为文件
    * ioctl VIDIOC_QBUF：把buffer放入"空闲链表"
    */
    while (1)
    {
        /*poll*/
        memset(fds, 0, sizeof(struct pollfd) * 1);
        fds[0].fd = fd;
        fds[0].events = POLLIN;

        if (1 == poll(fds, 1, -1))
        {
            /*把buffer取出队列*/
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(struct v4l2_buffer));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (0 != ioctl(fd, VIDIOC_DQBUF, &buf))
            {
                perror("Unable to dequeue buffer");
                return -1;
            }
            /*把buffer的数据存为文件*/
            sprintf(filename, "video_raw_data_%04d.yuv", file_cnt++);
            int fd_file = open(filename, O_RDWR | O_CREAT, 0666);

            if (fd_file < 0)
            {
                printf("can not open file %s\n", filename);
            }
            printf("capture to %s\n", filename);

            write(fd_file, bufs[buf.index], buf.bytesused);
            close(fd_file);

            /*把buffer放入队列*/
            if (0 != ioctl(fd, VIDIOC_QBUF, &buf))
            {
                perror("Unable to queue buffer");
                return -1;
            }

            if (file_cnt >= 100)
            {
                printf("capture %d frames\n", file_cnt);
                break;
            }
        }
    }

    /*关闭摄像头*/

    if (0 != ioctl(fd, VIDIOC_STREAMOFF, &type))
    {
        perror("Unable to stop capture");
        return -1;
    }
    printf("stop capture success\n");

    close(fd);
    return 0;
}
