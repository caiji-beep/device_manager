#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-vmalloc.h>

#define VIRTUAL_VIDEO_BUFFER_SIZE (2 * 800 * 600)

// 定义一个静态的全局指针，充当锚点。主要是由于我们是虚拟摄像头驱动
static struct virtual_video *g_vvid = NULL;

static struct virtual_video_framesize
{
	u32 width;
	u32 height;
};

static struct virtual_video_framesize mjpeg_framesize[] = {
	{640, 480},
	{800, 600},
	{1920, 1080},
};
static struct virtual_video_framesize yuyv_framesize[] = {
	{640, 480},
	{800, 600},
	{1280, 720},
	{1920, 1080},
};

static struct virtual_video_format
{
	char *name;
	u32 pixelformat;
	u32 buffersize;
	struct virtual_video_framesize *framesize;
	unsigned int num_size;
};

static struct virtual_video_format formats[] = {
	{
		.name = "MJPEG",
		.pixelformat = V4L2_PIX_FMT_MJPEG,
		.buffersize = VIRTUAL_VIDEO_BUFFER_SIZE,
		.framesize = mjpeg_framesize,
		.num_size = ARRAY_SIZE(mjpeg_framesize),
	},
	{
		.name = "YUYV",
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.buffersize = VIRTUAL_VIDEO_BUFFER_SIZE,
		.framesize = yuyv_framesize,
		.num_size = ARRAY_SIZE(yuyv_framesize),
	},
};

static const unsigned int NUM_FORMATS = ARRAY_SIZE(formats);

struct virtual_video_frame_buf
{
	struct vb2_buffer vb; /* common v4l buffer stuff -- must be first */
	struct list_head list;
};

struct virtual_video
{

	struct device *dev;

	struct video_device vdev;
	struct v4l2_device v4l2_dev;

	/* videobuf2 queue and queued buffers list */
	struct vb2_queue vb_queue;
	struct list_head queued_bufs;
	spinlock_t queued_bufs_lock; /* Protects queued_bufs */

	/* Note if taking both locks v4l2_lock must always be locked first! */
	struct mutex v4l2_lock;		/* Protects everything else */
	struct mutex vb_queue_lock; /* Protects vb_queue and capt_file */

	// 用于保存 APP 最终决定的格式和分辨率
	const struct virtual_video_format *current_fmt;
	u32 current_width;
	u32 current_height;
};
static struct vb2_ops virtual_video_vb2_ops = {
	// .queue_setup            = virtual_video_queue_setup,
	// .buf_queue              = virtual_video_buf_queue,
	// .start_streaming        = virtual_video_start_streaming,
	// .stop_streaming         = virtual_video_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
};

static int virtual_video_enum_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_fmtdesc *f)
{
	if (f->index >= NUM_FORMATS)
		return -EINVAL;

	strlcpy(f->description, formats[f->index].name, sizeof(f->description));
	f->pixelformat = formats[f->index].pixelformat;

	return 0;
}



static const struct v4l2_file_operations virtual_video_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.read = vb2_fop_read,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
	.unlocked_ioctl = video_ioctl2,
};

static const struct v4l2_ioctl_ops virtual_video_ioctl_ops = {
	// .vidioc_querycap          = airspy_querycap,

	.vidioc_enum_fmt_vid_cap     = virtual_video_enum_fmt_vid_cap,
	// .vidioc_enum_fmt_sdr_cap  = airspy_enum_fmt_sdr_cap,
	// .vidioc_g_fmt_sdr_cap     = airspy_g_fmt_sdr_cap,
	// .vidioc_s_fmt_sdr_cap     = airspy_s_fmt_sdr_cap,
	// .vidioc_try_fmt_sdr_cap   = airspy_try_fmt_sdr_cap,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,

	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,

};

static struct video_device virtual_video_template = {
	.name = "Virtual Video",
	.release = video_device_release_empty,
	.fops = &virtual_video_fops,
	.ioctl_ops = &virtual_video_ioctl_ops,
};

static void virtual_video_release(struct v4l2_device *v)
{
	//virtual_video_exit中已做
}

static int __init virtual_video_init(void)
{
	struct virtual_video *vvid;
	int ret;
	printk("%s %s %d\n", __FILE__, __func__, __LINE__);
	vvid = kzalloc(sizeof(struct virtual_video), GFP_KERNEL);

	if (vvid == NULL)
	{
		pr_info("Could not allocate memory for state\n");
		return -ENOMEM;
	}

	g_vvid = vvid;

	/**
	 * APP 1 正在调用 ioctl(VIDIOC_S_FMT) 设置分辨率为 1080p（这个过程可能涉及到很复杂的计算，耗时较长）。
	 * 此时 APP 2 试图调用 ioctl(VIDIOC_G_FMT) 读取当前分辨率。
	 */
	mutex_init(&vvid->v4l2_lock);

	mutex_init(&vvid->vb_queue_lock); // 流控锁 防止APP1 VIDIOC_REQBUFS时,APP2 VIDIOC_STREAMON

	spin_lock_init(&vvid->queued_bufs_lock); // 解决的是应用层（进程）与底层定时器（中断）争抢资源的问题
	INIT_LIST_HEAD(&vvid->queued_bufs);		 // 链表初始化

	/* Init videobuf2 queue structure */
	vvid->vb_queue.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vvid->vb_queue.io_modes = VB2_MMAP | VB2_USERPTR | VB2_READ;
	vvid->vb_queue.drv_priv = vvid; // 私有上下文传递
	vvid->vb_queue.buf_struct_size = sizeof(struct virtual_video_frame_buf);
	vvid->vb_queue.ops = &virtual_video_vb2_ops;  // vb2_ops, 硬件相关的操作函数
	vvid->vb_queue.mem_ops = &vb2_vmalloc_memops; // vb2_mem_ops, 辅助结构体,用于mem ops(alloc、mmap)
	vvid->vb_queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	ret = vb2_queue_init(&vvid->vb_queue); // 新版内核会调用vb2_queue_init函数，里面的q->buf_ops = &v4l2_buf_ops;vb2_buf_ops, 用于APP和驱动传递参数

	if (ret)
	{
		pr_err("virtual_video: Could not initialize...\n");
		goto err_free_mem;
	}

	/* Init video_device structure */
	vvid->vdev = virtual_video_template; // video_device结构体
	vvid->vdev.queue = &vvid->vb_queue;	 // vvid的vdev.queue指向vvid的vb_queue，vb_queue是videobuf2的核心结构体，里面有很多函数指针，驱动需要实现这些函数指针来完成数据流的传输
	vvid->vdev.queue->lock = &vvid->vb_queue_lock;
	video_set_drvdata(&vvid->vdev, vvid);

	/* Register the v4l2_device structure */
	vvid->v4l2_dev.release = virtual_video_release;
	ret = v4l2_device_register(NULL, &vvid->v4l2_dev); // 虚拟设备要传NULL
	if (ret)
	{
		pr_err("Failed to register v4l2-device\n");
		goto err_free_mem;
	}

	vvid->vdev.v4l2_dev = &vvid->v4l2_dev;
	vvid->vdev.lock = &vvid->v4l2_lock;

	ret = video_register_device(&vvid->vdev, VFL_TYPE_GRABBER, -1); //
	if (ret)
	{
		pr_err("Failed to register as video device\n");
		goto err_unregister_v4l2_dev;
	}
	pr_info("init OK");

	return 0;

err_unregister_v4l2_dev:
	v4l2_device_unregister(&vvid->v4l2_dev);
err_free_mem:
	kfree(vvid);
	g_vvid = NULL;
	return ret;
}
static void __exit virtual_video_exit(void)
{
	struct virtual_video *vvid = g_vvid;

	if (vvid)
	{
		video_unregister_device(&vvid->vdev);	 // 先注销暴露给应用层的 /dev/videoX 节点
		v4l2_device_unregister(&vvid->v4l2_dev); // 注销底层的 v4l2_device
		kfree(vvid);
		g_vvid = NULL;
		pr_info("virtual_video: driver unloaded.\n");
	}
}

module_init(virtual_video_init);
module_exit(virtual_video_exit);

MODULE_AUTHOR("lsz <lsz@hfut>");
MODULE_DESCRIPTION("VIRTUAL VIDEO DRIVER");
MODULE_LICENSE("GPL");