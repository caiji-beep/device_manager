# LSZ Device Manager

## 1. 项目简介

`LSZ Device Manager` 是一个运行在 i.MX6ULL Linux 开发板上的 Qt Widgets 综合设备管理终端。

项目覆盖了常见嵌入式 Linux 应用开发链路：

- GPIO 字符设备控制：LED、蜂鸣器
- 串口通信：`/dev/ttymxc2`
- 传感器数据采集：AP3216C、ICM20608
- V4L2 视频采集：真实 UVC 摄像头 `/dev/video1`
- V4L2 虚拟摄像头驱动联调：虚拟节点通常为 `/dev/video2`
- Qt 多页面 UI：主页、控制页、串口页、传感器页、视频页、图库页
- 图片/录像保存：JPG 拍照、MJPEG AVI 录像、图库浏览与删除

这个项目适合在简历中表达两类能力：

- 驱动侧：理解 V4L2、videobuf2、control handler、MMAP buffer、frame interval、虚拟视频设备
- 应用侧：理解 Qt Widgets、线程模型、信号槽、V4L2 用户态采集、低延迟预览、文件保存与图库管理

## 2. 项目目录

```text
apps/qt_app/LSZ_Terminal
├── controller/
│   └── serialcontroller.*          # 串口协议/控制逻辑
├── device/
│   ├── leddevice.*                 # LED 字符设备封装
│   ├── beepdevice.*                # 蜂鸣器字符设备封装
│   ├── serialdevice.*              # 串口设备封装
│   ├── ap3216cdevice.*             # AP3216C 设备封装
│   ├── icm20608device.*            # ICM20608 设备封装
│   └── videodevice.*               # V4L2 用户态视频采集封装
├── media/
│   ├── mediastore.*                # 图片/录像保存目录与文件枚举
│   └── aviwriter.*                 # MJPEG AVI 文件写入器
├── pages/
│   ├── controlpage.*               # LED/蜂鸣器控制页
│   ├── serialpage.*                # 串口通信页
│   ├── sensorpage.*                # 传感器页
│   ├── videopage.*                 # 视频采集、拍照、录像页
│   └── gallerypage.*               # 图库页
├── workers/
│   ├── ap3216cworker.*             # AP3216C 采集线程
│   ├── icm20608worker.*            # ICM20608 采集线程
│   └── videoworker.*               # 视频采集线程
├── mainwindow.*                    # 页面管理与全局设备初始化
├── main.cpp
└── LSZ_Terminal.pro
```

相关驱动与测试程序：

```text
drivers/virtual_vid_drv/
├── virtual_vid_drv.c               # V4L2 虚拟摄像头驱动
├── red.c / green.c / blue.c         # 800x600 MJPEG 数据
├── cyan_640x480.h                  # 640x480 MJPEG 数据
├── magenta_640x480.h
├── yellow_640x480.h
└── Makefile

apps/cli/video/05_video_brightness/
└── video_test.c                    # V4L2 亮度控制与采集测试程序
```

## 3. 运行环境

当前项目面向以下环境：

- 开发板：i.MX6ULL
- 系统：Linux 4.1.15 NXP BSP
- GUI：Qt Widgets
- 视频接口：V4L2
- 摄像头：
  - `/dev/video1`：真实 USB UVC 摄像头
  - `/dev/video2`：虚拟摄像头驱动节点，实际编号由内核分配，应用会扫描 `/dev/video*`

注意：虚拟摄像头使用 `video_register_device(..., -1)` 注册，节点号不一定永远是 `/dev/video2`。Qt 应用会通过 `VIDIOC_QUERYCAP` 扫描可用视频节点。

## 4. 编译 Qt 应用

在 `apps/qt_app/LSZ_Terminal` 目录下执行：

```bash
qmake LSZ_Terminal.pro
make -j2
```

如果使用交叉编译 Qt，请使用开发板 Qt 工具链对应的 `qmake`，例如：

```bash
/home/lsz/linux/IMX6ULL/tool/arm-qt/bin/qmake LSZ_Terminal.pro
make -j2
```

生成的程序为：

```text
apps/qt_app/LSZ_Terminal/LSZ_Terminal
```

## 5. 编译虚拟摄像头驱动

进入驱动目录：

```bash
cd drivers/virtual_vid_drv
make build
```

驱动 Makefile 使用的内核路径：

```makefile
KERNELDIR := /home/lsz/linux/IMX6ULL/linux/linux-imx-rel_imx_4.1.15_2.1.0_ga
```

加载驱动：

```bash
insmod virtual_camera.ko
dmesg | tail
ls /dev/video*
```

卸载驱动：

```bash
rmmod virtual_camera
```

## 6. Qt 应用功能说明

### 6.1 主页

主页由 `MainWindow::createHomePage()` 创建，包含以下模块入口：

- `Led/Beep`
- `Serial`
- `Sensor`
- `Video`
- `Gallery`

每个子页面通过 `QStackedWidget` 切换，页面左上角有 `back home` 按钮。

### 6.2 LED/蜂鸣器控制页

相关文件：

- `pages/controlpage.*`
- `device/leddevice.*`
- `device/beepdevice.*`

功能：

- 打开 `/dev/led`
- 打开 `/dev/beep`
- 通过按钮控制 LED 和蜂鸣器状态
- 设备打开失败时在 UI 上禁用对应按钮

### 6.3 串口页

相关文件：

- `pages/serialpage.*`
- `device/serialdevice.*`
- `controller/serialcontroller.*`

功能：

- 默认打开 `/dev/ttymxc2`
- 波特率：`115200`
- 支持串口发送、接收、日志显示
- 串口数据通过 `SerialController` 统一处理

### 6.4 传感器页

相关文件：

- `pages/sensorpage.*`
- `device/ap3216cdevice.*`
- `device/icm20608device.*`
- `workers/ap3216cworker.*`
- `workers/icm20608worker.*`

功能：

- AP3216C 数据采集
- ICM20608 数据采集
- 采集逻辑放在 worker 线程中，避免阻塞 UI
- 通过 Qt 信号槽把采集结果回传到页面

### 6.5 视频页

相关文件：

- `pages/videopage.*`
- `device/videodevice.*`
- `workers/videoworker.*`

功能：

- 扫描可用 `/dev/video*`
- 支持真实 UVC 摄像头和虚拟摄像头
- 支持格式选择：
  - `MJPEG`
  - `YUYV`
- 支持分辨率选择
- 支持亮度控制：
  - 设备支持 `V4L2_CID_BRIGHTNESS` 时启用
  - 不支持时自动禁用
- 支持虚拟摄像头 YUYV 测试模式：
  - 只有当前设备支持 `V4L2_CID_TEST_PATTERN`
  - 且当前格式为 `YUYV`
  - 才显示 `Pattern` 控件
- 支持拍照保存 JPG
- 支持录像保存 MJPEG AVI

设计原则：

- 主界面对 `/dev/video1` 和虚拟摄像头保持兼容
- 公共能力直接显示，例如设备、格式、分辨率、亮度
- 虚拟摄像头扩展能力动态显示，例如 YUYV test pattern
- 不强迫真实 USB 摄像头支持虚拟驱动专属功能

### 6.6 图库页

相关文件：

- `pages/gallerypage.*`
- `media/mediastore.*`

功能：

- 展示已保存照片和录像
- 照片显示缩略图
- 录像显示 AVI 卡片
- 支持刷新
- 支持删除照片/录像

保存目录：

```text
~/LSZ_Terminal_Media/photos
~/LSZ_Terminal_Media/videos
```

如果程序在开发板上以 `root` 用户运行，实际路径通常为：

```text
/root/LSZ_Terminal_Media/photos
/root/LSZ_Terminal_Media/videos
```

## 7. V4L2 用户态采集流程

`VideoDevice` 封装了用户态 V4L2 采集流程。

典型调用链如下：

```text
open("/dev/videoX")
  ↓
VIDIOC_QUERYCAP
  ↓
VIDIOC_ENUM_FMT
  ↓
VIDIOC_ENUM_FRAMESIZES
  ↓
VIDIOC_S_FMT
  ↓
VIDIOC_G_PARM / VIDIOC_S_PARM
  ↓
VIDIOC_REQBUFS
  ↓
VIDIOC_QUERYBUF
  ↓
mmap
  ↓
VIDIOC_QBUF
  ↓
VIDIOC_STREAMON
  ↓
select
  ↓
VIDIOC_DQBUF
  ↓
decode MJPEG or convert YUYV
  ↓
VIDIOC_QBUF
  ↓
VIDIOC_STREAMOFF
  ↓
munmap / close
```

关键点：

- 使用 `V4L2_MEMORY_MMAP`
- 应用通过 `mmap` 映射内核缓冲区
- 采集线程中等待可读事件后 `DQBUF`
- 处理完成后尽快 `QBUF` 归还 buffer
- 当前实现会尽量清空已经 ready 的 buffer，只保留最新帧用于显示，降低预览延迟

低延迟处理逻辑：

```text
select 等待一帧 ready
  ↓
循环 DQBUF 取出当前已完成的 buffer
  ↓
旧帧立即 QBUF 归还
  ↓
只解码最新一帧
  ↓
最新帧显示后 QBUF
```

这样做的目的：

- 避免 UI 处理慢时不断显示历史帧
- 减少视频预览堆积造成的端到端延迟
- 保证切换页面、返回主页时 UI 不被视频帧拖慢

## 8. 零拷贝说明

面试中要谨慎表述“零拷贝”。

本项目当前可以这样描述：

```text
基于 V4L2 MMAP 实现采集侧零拷贝，应用层通过 mmap 直接访问驱动缓冲区，避免 read 系统调用带来的内核态到用户态整帧拷贝。
```

不能直接说：

```text
全链路零拷贝。
```

原因：

- V4L2 MMAP 阶段：内核 buffer 映射到用户态，属于采集侧零拷贝
- MJPEG 显示阶段：需要 `QImage::fromData()` 解码，会生成新的图像对象
- YUYV 显示阶段：需要 YUYV 到 RGB 转换，会写入新的 `QImage`
- 录像阶段：当前把 `QImage` 编码为 JPEG 后写入 AVI，也存在编码和内存拷贝

更标准的简历表达：

```text
基于 V4L2 MMAP 实现 UVC 摄像头采集侧零拷贝，支持 480P@30fps 视频预览；通过缓冲区队列管理、丢弃滞后帧和 UI 刷新节流，将端到端预览延迟控制在可接受范围内。
```

如果后续要实现更严格的全链路低拷贝，可以优化：

- 对 MJPEG 摄像头保留原始 JPEG payload
- 录像时直接把 MJPEG payload 写入 AVI，避免 `QImage -> JPEG` 二次编码
- 使用独立录像线程，避免 UI 线程做 JPEG 编码
- 使用硬件显示层、DRM/KMS 或 EGLImage，减少 CPU 图像转换

## 9. 虚拟摄像头驱动设计

相关文件：

- `drivers/virtual_vid_drv/virtual_vid_drv.c`

驱动基于 V4L2 + videobuf2：

- `video_device`
- `v4l2_device`
- `vb2_queue`
- `vb2_vmalloc_memops`
- `v4l2_ctrl_handler`
- timer 模拟 30fps 出帧

### 9.1 支持格式

虚拟驱动当前支持：

- `MJPEG`
- `YUYV`

MJPEG 静态图片数据：

- `640x480`
  - cyan
  - magenta
  - yellow
- `800x600`
  - red
  - green
  - blue

Qt 应用对虚拟摄像头的 MJPEG 分辨率做了过滤：

```text
640x480
800x600
```

原因：虚拟驱动虽然可能枚举了更多尺寸，但当前真实准备好的 MJPEG 静态数据主要是这两组。

YUYV 数据由驱动实时生成，因此可以用于测试亮度和动态图案。

### 9.2 YUYV 三种模式

虚拟驱动通过 `V4L2_CID_TEST_PATTERN` 暴露 YUYV 测试模式：

```text
0 - Disabled / Gray Background
1 - Moving White Square
2 - Color Bars
```

Qt 端显示逻辑：

- 当前设备支持 `V4L2_CID_TEST_PATTERN`
- 当前格式为 `YUYV`
- 才显示 `Pattern` 下拉框

这样既兼容真实 UVC 摄像头，也保留了虚拟驱动的测试能力。

### 9.3 亮度控制与控制簇

虚拟驱动中亮度相关控制：

- `V4L2_CID_AUTOBRIGHTNESS`
- `V4L2_CID_BRIGHTNESS`

驱动使用 `v4l2_ctrl_auto_cluster()` 绑定控制簇。

重要理解：

```text
绑定控制簇后，s_ctrl 通常由 master control 触发。
```

本项目中 master 是：

```text
V4L2_CID_AUTOBRIGHTNESS
```

因此在 `s_ctrl` 中应该通过：

```c
ctrl->cluster[1]->val
```

读取 manual brightness，而不是只处理 `V4L2_CID_BRIGHTNESS` 分支。

这是面试中很好的驱动细节亮点：

- 理解 V4L2 control handler
- 理解 auto/manual 控制簇
- 理解应用层 `VIDIOC_S_CTRL` 和驱动 `s_ctrl` 之间不一定是一对一 control id 回调

## 10. 拍照与录像

### 10.1 拍照

相关文件：

- `pages/videopage.*`
- `media/mediastore.*`

流程：

```text
VideoWorker 采集 QImage
  ↓
VideoPage 保存最近一帧
  ↓
点击 Save
  ↓
QImage::save(..., "JPG", 90)
  ↓
保存到 photos 目录
  ↓
通知 GalleryPage 刷新
```

### 10.2 录像

相关文件：

- `media/aviwriter.*`

当前录像格式：

```text
MJPEG AVI
```

流程：

```text
Start recording
  ↓
创建 video_xxx.avi
  ↓
每帧 QImage 编码成 JPEG
  ↓
写入 AVI movi chunk
  ↓
Stop recording
  ↓
写入 idx1 索引
  ↓
回填 AVI 头部帧数和长度
```

注意：

- 当前录像实现偏功能完整，不是性能最优
- 编码发生在 Qt 端，有 CPU 开销
- 如果录制时 UI 卡顿，可以把编码放入独立线程
- 如果输入是 MJPEG，可以进一步优化为直接写原始 JPEG payload

## 11. 线程模型

### 11.1 UI 主线程

负责：

- 页面显示
- 按钮响应
- QLabel/QPixmap 更新
- 图库刷新

### 11.2 视频采集线程

相关文件：

- `workers/videoworker.*`

负责：

- 打开 V4L2 设备
- 初始化格式与 buffer
- 循环读取帧
- 通过 `frameReady(QImage)` 发送给 UI
- 接收亮度和 test pattern 控制请求

### 11.3 传感器采集线程

相关文件：

- `workers/ap3216cworker.*`
- `workers/icm20608worker.*`

负责周期性读取传感器数据，然后通过信号发送到 UI。

线程设计原则：

- 慢操作不放 UI 线程
- 设备采集放 worker
- UI 更新只在主线程执行
- 跨线程交互使用 Qt 信号槽

## 12. 常用调试命令

查看视频设备：

```bash
ls /dev/video*
```

查看驱动日志：

```bash
dmesg | tail -n 50
```

查看 V4L2 设备能力：

```bash
v4l2-ctl -d /dev/video1 --all
v4l2-ctl -d /dev/video2 --all
```

查看支持格式：

```bash
v4l2-ctl -d /dev/video1 --list-formats-ext
v4l2-ctl -d /dev/video2 --list-formats-ext
```

设置亮度：

```bash
v4l2-ctl -d /dev/video2 --set-ctrl=brightness=180
```

设置虚拟摄像头 YUYV 测试模式：

```bash
v4l2-ctl -d /dev/video2 --set-ctrl=test_pattern=1
```

查看保存文件：

```bash
ls ~/LSZ_Terminal_Media/photos
ls ~/LSZ_Terminal_Media/videos
```

如果在开发板上以 root 运行：

```bash
ls /root/LSZ_Terminal_Media/photos
ls /root/LSZ_Terminal_Media/videos
```

## 13. 常见问题

### 13.1 为什么 video2 显示模式一直是 0？

原因可能是应用层没有设置 `V4L2_CID_TEST_PATTERN`。

当前 Qt 端只有在以下条件满足时显示 Pattern 控件：

- 设备支持 `V4L2_CID_TEST_PATTERN`
- 当前格式选择为 `YUYV`

如果选择 `MJPEG`，驱动中的 YUYV test pattern 不会影响画面。

### 13.2 为什么虚拟摄像头 MJPEG 只显示部分分辨率？

因为当前静态 MJPEG 数据只准备了：

```text
640x480
800x600
```

应用层过滤掉了虚拟摄像头 MJPEG 的其他尺寸，避免用户选择后出现无效画面。

### 13.3 为什么录像时 CPU 占用会上升？

当前录像是：

```text
QImage -> JPEG 编码 -> AVI 写入
```

JPEG 编码需要 CPU。后续优化可以改成独立线程或 MJPEG 原始 payload 直写。

### 13.4 为什么说采集侧零拷贝，而不是全链路零拷贝？

因为 V4L2 MMAP 避免的是采集 buffer 从内核态到用户态的整帧拷贝。

但显示、YUYV 转 RGB、MJPEG 解码、录像编码仍然可能产生新的内存写入。

### 13.5 为什么返回主页后视频仍然不卡？

当前设计中：

- 视频采集在 worker 线程
- UI 只在可见时刷新预览
- 预览刷新做了 30fps 节流
- 采集侧会丢弃滞后帧，只处理最新帧

这些设计共同减少了 UI 卡顿。

## 14. 面试讲解提纲

### 14.1 一分钟版本

可以这样说：

```text
我做了一个基于 i.MX6ULL 的 Qt 设备管理终端，包含 LED、蜂鸣器、串口、传感器和 V4L2 视频采集。视频部分支持真实 UVC 摄像头和自写 V4L2 虚拟摄像头驱动，应用层基于 V4L2 MMAP 做采集侧零拷贝，支持 MJPEG/YUYV、分辨率选择、亮度控制、虚拟摄像头 YUYV 测试模式、拍照和 MJPEG AVI 录像。同时我做了图库页面，可以浏览和删除保存的照片/视频。
```

### 14.2 三分钟版本

可以按这个顺序展开：

1. 项目运行在 i.MX6ULL Linux 开发板，使用 Qt Widgets 做多页面 UI。
2. 底层设备包括 LED、蜂鸣器、串口、AP3216C、ICM20608、UVC 摄像头和虚拟 V4L2 摄像头。
3. 视频应用层封装了完整 V4L2 采集流程：`QUERYCAP`、`S_FMT`、`REQBUFS`、`mmap`、`QBUF`、`STREAMON`、`DQBUF`。
4. 使用 MMAP 实现采集侧零拷贝，并通过丢弃滞后帧降低预览延迟。
5. 虚拟摄像头驱动基于 videobuf2，使用 timer 模拟 30fps 出帧，支持 MJPEG/YUYV、亮度控制和 test pattern。
6. Qt 端根据设备能力动态显示控件，兼容真实摄像头和虚拟摄像头。
7. 后续又加入了拍照、MJPEG AVI 录像和图库管理功能。

### 14.3 简历写法

推荐写法：

```text
基于 i.MX6ULL Linux 平台开发 Qt 设备管理终端，完成 LED/蜂鸣器控制、串口通信、AP3216C/ICM20608 传感器采集和 V4L2 视频预览模块。
```

```text
基于 V4L2 MMAP 实现 UVC 摄像头采集侧零拷贝，支持 MJPEG/YUYV 格式枚举、分辨率选择、亮度控制和 480P@30fps 视频预览；通过最新帧策略和 UI 刷新节流降低端到端延迟。
```

```text
设计并实现 V4L2 虚拟摄像头驱动，基于 videobuf2 管理采集缓冲区，使用 timer 模拟 30fps 出帧，支持 MJPEG/YUYV、亮度 control cluster 和 test pattern 控制。
```

```text
实现 Qt 端拍照、MJPEG AVI 录像与图库管理功能，支持照片/录像保存、缩略图浏览和文件删除。
```

### 14.4 避免这样写

不推荐：

```text
实现全链路零拷贝视频系统。
```

原因：当前 Qt 显示和录像阶段仍有解码、颜色转换和编码。

更准确：

```text
实现 V4L2 MMAP 采集侧零拷贝，并对显示链路进行低延迟优化。
```

## 15. 后续优化方向

可以作为面试中“如果继续优化你会怎么做”的回答。

### 15.1 录像优化

- 将 AVI 编码放入独立线程
- 对 MJPEG 摄像头直接保存原始 JPEG payload
- 增加录像时长、帧率和文件大小显示
- 支持录像暂停/继续

### 15.2 视频显示优化

- YUYV 转 RGB 使用 NEON 优化
- 使用硬件加速解码 MJPEG
- 使用 DRM/KMS 或 OpenGL 显示减少 CPU 绘制压力
- UI 与采集解耦，增加有界帧队列

### 15.3 驱动优化

- 虚拟驱动从 `vb2_vmalloc` 扩展到更贴近真实硬件的 DMA buffer
- 支持更多 V4L2 controls
- 完善 `enum_frameintervals`
- 增加多实例虚拟摄像头

### 15.4 工程化优化

- 增加日志等级
- 增加配置文件保存上次设备/格式选择
- 增加单元测试或 CLI 自动化测试
- 增加启动时设备自检页面

## 16. 关键源码索引

视频采集应用层：

```text
device/videodevice.cpp
workers/videoworker.cpp
pages/videopage.cpp
```

图库与媒体保存：

```text
media/mediastore.cpp
media/aviwriter.cpp
pages/gallerypage.cpp
```

虚拟摄像头驱动：

```text
drivers/virtual_vid_drv/virtual_vid_drv.c
```

V4L2 CLI 测试程序：

```text
apps/cli/video/05_video_brightness/video_test.c
```

主窗口页面管理：

```text
mainwindow.cpp
```
