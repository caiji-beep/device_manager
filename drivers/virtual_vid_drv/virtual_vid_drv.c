#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-vmalloc.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define VIRTUAL_VIDEO_BUFFER_SIZE (2 * 800 * 600)

extern unsigned char red[8230];
extern unsigned char green[8265];
extern unsigned char blue[8267];

// 定义一个静态的全局指针，充当锚点。主要是由于我们是虚拟摄像头驱动
static struct virtual_video *g_vvid = NULL;

struct virtual_video_framesize
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

struct virtual_video_format
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

	int inst_id; // 实例ID，用于区分 /dev/video0, /dev/video1

	struct timer_list my_timer;

	u32 sequence; // 帧计数器：0, 1, 2, 3...
	u32 frame_index;
};

/* Videobuf2 operations */
static int virtual_video_queue_setup(struct vb2_queue *vq,
									 const struct v4l2_format *fmt, unsigned int *nbuffers,
									 unsigned int *nplanes, unsigned int sizes[], void *alloc_ctxs[])
{
	struct virtual_video *vvid = vb2_get_drv_priv(vq);

	/*check fmt*/
	if (vvid->current_fmt == NULL)
	{
		pr_err("fmt not OK\n");
		return -EINVAL;
	}

	/* Need at least 8 buffers */
	if (vq->num_buffers + *nbuffers < 8)
		*nbuffers = 8 - vq->num_buffers;
	*nplanes = 1;

	if (vvid->current_fmt->pixelformat == V4L2_PIX_FMT_YUYV)
	{
		sizes[0] = PAGE_ALIGN(vvid->current_width * vvid->current_height * 2);
	}
	else
	{
		sizes[0] = PAGE_ALIGN(vvid->current_fmt->buffersize);
	}

	pr_info("nbuffers=%d sizes[0]=%d\n", *nbuffers, sizes[0]);
	return 0;
}

static void virtual_video_buf_queue(struct vb2_buffer *vb)
{
	struct virtual_video *vvid = vb2_get_drv_priv(vb->vb2_queue);
	struct virtual_video_frame_buf *buf =
		container_of(vb, struct virtual_video_frame_buf, vb);
	unsigned long flags;

	/*因为“中断上下文”可以无条件地抢占“进程上下文”，所以我们在进程上下文中操作共享资源时，必须使用 spin_lock_irqsave*/
	spin_lock_irqsave(&vvid->queued_bufs_lock, flags);
	list_add_tail(&buf->list, &vvid->queued_bufs);
	spin_unlock_irqrestore(&vvid->queued_bufs_lock, flags);
}

static int virtual_video_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct virtual_video *vvid = vb2_get_drv_priv(vq);

	vvid->sequence = 0;

	mod_timer(&vvid->my_timer, jiffies + HZ / 30);

	pr_info("virtual_video: Stream started at ~30FPS!\n");

	return 0;
}

static void virtual_video_stop_streaming(struct vb2_queue *vq)
{
	struct virtual_video *vvid = vb2_get_drv_priv(vq);
	unsigned long flags;

	

	/* stop hardware streaming */
	


	del_timer_sync(&vvid->my_timer);
	/*清空buffer*/

	spin_lock_irqsave(&vvid->queued_bufs_lock, flags);
	while (!list_empty(&vvid->queued_bufs)) {
		struct virtual_video_frame_buf *buf;

		buf = list_entry(vvid->queued_bufs.next,
				struct virtual_video_frame_buf, list);//container_of
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}
	spin_unlock_irqrestore(&vvid->queued_bufs_lock, flags);

	
}

static struct vb2_ops virtual_video_vb2_ops = {
	.queue_setup = virtual_video_queue_setup,
	.buf_queue = virtual_video_buf_queue,
	.start_streaming = virtual_video_start_streaming,
	.stop_streaming = virtual_video_stop_streaming,
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

static int virtual_video_enum_framesizes(struct file *file, void *priv,
										 struct v4l2_frmsizeenum *fsize)
{
	//struct virtual_video *vvid = video_drvdata(file);

	struct virtual_video_format *format = NULL;
	int i;
	for (i = 0; i < NUM_FORMATS; i++)
	{
		if (formats[i].pixelformat == fsize->pixel_format)
		{
			format = &formats[i];
			break;
		}
	}
	if (format == NULL)
		return -EINVAL;

	if (fsize->index >= format->num_size)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = format->framesize[fsize->index].width;
	fsize->discrete.height = format->framesize[fsize->index].height;

	return 0;
}

static int virtual_video_g_fmt_vid_cap(struct file *file, void *priv,
									   struct v4l2_format *f)
{
	struct virtual_video *vvid = video_drvdata(file);
	if (vvid->current_fmt == NULL)
		return -EINVAL;

	f->fmt.pix.pixelformat = vvid->current_fmt->pixelformat;
	f->fmt.pix.sizeimage = vvid->current_fmt->buffersize; // 为什么是sizeimage

	f->fmt.pix.width = vvid->current_width;
	f->fmt.pix.height = vvid->current_height;

	f->fmt.pix.field = V4L2_FIELD_NONE; // 逐行扫描，无隔行

	// 计算每行字节数（MJPEG为0，YUYV为宽*2）
	if (vvid->current_fmt->pixelformat == V4L2_PIX_FMT_YUYV)
		f->fmt.pix.bytesperline = vvid->current_width * 2;
	else
		f->fmt.pix.bytesperline = 0;

	return 0;
}

static int virtual_video_s_fmt_vid_cap(struct file *file, void *priv,
									   struct v4l2_format *f)
{
	struct virtual_video *vvid = video_drvdata(file);
	struct vb2_queue *q = &vvid->vb_queue;
	struct virtual_video_format *fmt = NULL;

	int i, match;

	if (vb2_is_busy(q))
		return -EBUSY;

	for (i = 0; i < NUM_FORMATS; i++)
	{
		if (f->fmt.pix.pixelformat == formats[i].pixelformat)
		{
			fmt = &formats[i];
			break;
		}
	}

	// 1.协商格式
	if (fmt == NULL)
	{
		f->fmt.pix.pixelformat = formats[0].pixelformat;
		fmt = &formats[0];
	}

	// 2.协商帧大小
	match = 0;
	for (i = 0; i < fmt->num_size; i++)
	{
		if (f->fmt.pix.width == fmt->framesize[i].width && f->fmt.pix.height == fmt->framesize[i].height)
		{

			match = 1;
			break;
		}
	}
	if (!match)
	{
		f->fmt.pix.width = fmt->framesize[0].width;
		f->fmt.pix.height = fmt->framesize[0].height;
	}

	f->fmt.pix.field = V4L2_FIELD_NONE; // 强制逐行扫描

	if (fmt->pixelformat == V4L2_PIX_FMT_YUYV)
	{
		f->fmt.pix.bytesperline = f->fmt.pix.width * 2;
		f->fmt.pix.sizeimage = f->fmt.pix.bytesperline * f->fmt.pix.height;
	}
	else
	{ // MJPEG
		f->fmt.pix.bytesperline = 0;
		f->fmt.pix.sizeimage = fmt->buffersize;
	}

	vvid->current_fmt = fmt;
	vvid->current_width = f->fmt.pix.width;
	vvid->current_height = f->fmt.pix.height;

	return 0;
}

static int virtual_querycap(struct file *file, void *fh,
							struct v4l2_capability *cap)
{
	struct virtual_video *vvid = video_drvdata(file);

	strlcpy(cap->driver, KBUILD_MODNAME, sizeof(cap->driver));
	strlcpy(cap->card, vvid->vdev.name, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s-%03d", "virt_cam", vvid->inst_id);
	/*cap->device_caps描述了当前这个设备节点所特有的能力*/
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING |
					   V4L2_CAP_READWRITE;
	// capabilities这个字段描述整个物理设备的总能力
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	memset(cap->reserved, 0, sizeof(cap->reserved)); /// 清空保留字段

	return 0;
}

static const struct v4l2_ioctl_ops virtual_video_ioctl_ops = {
	.vidioc_querycap = virtual_querycap,

	.vidioc_enum_fmt_vid_cap = virtual_video_enum_fmt_vid_cap,
	.vidioc_enum_framesizes = virtual_video_enum_framesizes,
	.vidioc_g_fmt_vid_cap = virtual_video_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = virtual_video_s_fmt_vid_cap,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,

	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,

};

static const struct v4l2_file_operations virtual_video_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.read = vb2_fop_read,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
	.unlocked_ioctl = video_ioctl2,
};

static struct video_device virtual_video_template = {
	.name = "Virtual Video",
	.release = video_device_release_empty,
	.fops = &virtual_video_fops,
	.ioctl_ops = &virtual_video_ioctl_ops,
};

static void virtual_video_release(struct v4l2_device *v)
{
	// virtual_video_exit中已做
}

static void virtual_video_timer_callback(unsigned long data)
{
	struct virtual_video *vvid = (struct virtual_video *)data;
	struct virtual_video_frame_buf *buf;
	void *vaddr;
	unsigned int payload_size = 0;
	unsigned long flags;

	spin_lock_irqsave(&vvid->queued_bufs_lock, flags);

	if (list_empty(&vvid->queued_bufs))
	{
		spin_unlock_irqrestore(&vvid->queued_bufs_lock, flags);
		goto out;
	}

	buf = list_first_entry(&vvid->queued_bufs,
						   struct virtual_video_frame_buf,
						   list);
	list_del(&buf->list);

	spin_unlock_irqrestore(&vvid->queued_bufs_lock, flags);

	vaddr = vb2_plane_vaddr(&buf->vb, 0);

	if (vvid->current_fmt->pixelformat == V4L2_PIX_FMT_YUYV)
	{
		payload_size = vvid->current_width * vvid->current_height * 2;
		memset(vaddr, 0x80, payload_size);
	}
	else
	{
		if (vvid->frame_index < 30)
		{
			payload_size = sizeof(red);
			memcpy(vaddr, red, payload_size);
		}
		else if (vvid->frame_index < 60)
		{
			payload_size = sizeof(green);
			memcpy(vaddr, green, payload_size);
		}
		else if (vvid->frame_index < 90)
		{
			payload_size = sizeof(blue);
			memcpy(vaddr, blue, payload_size);
		}
		else
		{
			payload_size = sizeof(red);
			memcpy(vaddr, red, payload_size);	
			vvid->frame_index = 0;
		}
	}
	vvid->frame_index++;
	

	v4l2_get_timestamp(&buf->vb.v4l2_buf.timestamp);	 // 打上时间戳
	buf->vb.v4l2_buf.sequence = vvid->sequence++; // 打上序号
	vb2_set_plane_payload(&buf->vb, 0, payload_size);
	vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);

out:
	mod_timer(&vvid->my_timer, jiffies + HZ / 30);
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
	vvid->frame_index = 0;

	setup_timer(&vvid->my_timer, virtual_video_timer_callback, (unsigned long)vvid);

	vvid->current_fmt = &formats[0];					   // 默认MJPEG
	vvid->current_width = formats[0].framesize[1].width;   // 默认800
	vvid->current_height = formats[0].framesize[1].height; // 默认600

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
	vvid->vb_queue.drv_priv = vvid; // 私有上下文传递  通过调用vb2_get_drv_priv(vq)获取vvid
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
	video_set_drvdata(&vvid->vdev, vvid); // 存入私有数据  调用video_drvdata(file)获取vvid

	/* Register the v4l2_device structure */
	vvid->v4l2_dev.release = virtual_video_release;
	strlcpy(vvid->v4l2_dev.name, "virtual_video_v4l2", sizeof(vvid->v4l2_dev.name));
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

	vvid->inst_id = vvid->vdev.num;

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
		del_timer_sync(&vvid->my_timer);
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
