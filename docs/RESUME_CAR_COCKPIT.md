# 基于 i.MX6ULL 的车载中控设备管理与视频监控系统

## 简历推荐版本

**项目名称：** 基于 i.MX6ULL 的车载中控设备管理与视频监控系统  
**项目环境：** i.MX6ULL、Linux 4.1.15、U-Boot、BusyBox、Qt Widgets、V4L2、DTS、arm-linux-gnueabihf  
**项目描述：**  
面向车载中控场景，基于 i.MX6ULL Linux 平台开发一套嵌入式车机 HMI 原型系统，覆盖板级启动、设备树适配、Linux 驱动、用户态 CLI 验证和 Qt 图形应用。系统集成车身外设控制、串口指令交互、环境/姿态传感器采集、车载摄像头预览、拍照录像和图库管理等功能，形成从 U-Boot、Kernel/DTS、RootFS 到 Qt 应用的完整嵌入式 Linux 闭环。

**主要职责与成果：**

- 负责 i.MX6ULL 平台 BSP bring-up，完成 U-Boot eMMC 启动配置、Linux 内核与设备树裁剪适配、BusyBox 根文件系统构建，以及 Qt 运行环境部署。
- 基于设备树完成 LCDIF 1024x600 屏、PWM 背光、FT5426 触摸、GPIO、I2C、SPI、UART、USB Host、CSI/OV5640、FLEXCAN 等车机常用外设节点适配。
- 编写/适配 platform、I2C、SPI、input、V4L2 等类型驱动，通过 compatible 匹配设备树并导出 `/dev`、input event、`/dev/videoX` 等用户态接口。
- 实现 LED/蜂鸣器车身控制模块，驱动侧采用 platform driver + gpiod + miscdevice，应用侧封装为 Qt `LedDevice` / `BeepDevice`，支持本地按钮和串口命令双通道控制。
- 实现 AP3216C 光照/接近传感器和 ICM20608 六轴 IMU 采集链路，分别基于 I2C/SPI 驱动导出字符设备，Qt 侧使用 `QThread + Worker + QTimer` 周期采集并完成工程单位换算。
- 实现车载摄像头/倒车影像预览模块，封装完整 V4L2 采集流程，包括 `QUERYCAP`、格式/分辨率枚举、`S_FMT`、`REQBUFS`、`mmap`、`QBUF/DQBUF`、`STREAMON/OFF`。
- 基于 V4L2 MMAP 实现采集侧零拷贝，支持 MJPEG/YUYV、480P@30fps 预览、亮度调节、虚拟摄像头 test pattern，并通过丢弃滞后帧和 UI 30fps 节流降低预览延迟。
- 设计 V4L2 虚拟摄像头驱动，基于 `video_device + v4l2_device + videobuf2 + vb2_vmalloc` 管理 buffer，使用 timer 模拟 30fps 出帧，支持 MJPEG/YUYV、brightness control cluster 和动态测试图案。
- 设计 Qt Widgets 多页面中控 HMI，包含主页、车身控制、串口通信、传感器状态、视频预览、图库管理等页面；通过 device 封装层、worker 线程和 page 展示层解耦，避免 UI 阻塞。
- 实现拍照与行车记录功能，支持 JPG 拍照、MJPEG AVI 录像、RIFF/AVI 头部回填、`idx1` 索引生成、媒体目录管理、缩略图展示和文件删除。

## 一页简历精简版

**基于 i.MX6ULL 的车载中控设备管理与视频监控系统**

- 基于 i.MX6ULL + Linux 4.1.15 + Qt Widgets 搭建车载中控 HMI 原型，完成 U-Boot、Kernel/DTS、BusyBox RootFS、Qt 应用交叉编译与开发板部署。
- 适配 LCDIF 1024x600、FT5426 触摸、PWM 背光、UART、USB/UVC、CSI/OV5640、FLEXCAN、GPIO、I2C、SPI 等外设设备树节点，打通 DTS -> Driver -> `/dev`/input/V4L2 -> Qt 应用链路。
- 编写 LED/蜂鸣器 platform misc 驱动、AP3216C I2C 驱动、ICM20608 SPI 驱动，并通过 CLI 程序完成底层读写验证，再封装到 Qt device 层。
- 基于 V4L2 MMAP 实现摄像头采集侧零拷贝，支持 MJPEG/YUYV、格式/分辨率枚举、亮度控制和 480P@30fps 预览；通过最新帧策略和 UI 刷新节流优化预览延迟。
- 实现 V4L2 虚拟摄像头驱动，基于 videobuf2 管理采集缓冲区，使用 timer 模拟 30fps 出帧，支持 MJPEG/YUYV、brightness control cluster 和 test pattern。
- 设计 Qt 多页面 HMI 架构，使用 `QThread + Worker + QTimer` 实现传感器/视频异步采集，支持串口指令交互、传感器显示、车身控制、拍照、MJPEG AVI 录像与图库管理。

## U-Boot 与 RootFS 补充写法

可以单独放在项目经历里，也可以放到“技术能力/项目职责”中。

- 基于 NXP i.MX6ULL U-Boot 进行板级启动适配，使用 `mx6ull_alientek_emmc_defconfig` 配置 eMMC 启动目标，调整 DDR/eMMC 相关配置、启动参数和 `bootcmd`，支持从 eMMC/FAT 分区加载 `zImage` 与 DTB，并保留 TFTP/NFS 调试启动路径。
- 配置 U-Boot 环境变量，包括 `console=ttymxc0,115200`、`mmcroot`、`fdt_file`、`loadimage`、`loadfdt`、`mmcboot`、`netboot` 等，完成 Kernel + DTB + RootFS 启动链路验证。
- 基于 BusyBox 构建轻量级根文件系统，配置 `ash`、`mdev`、`mount`、`ifconfig`、`udhcpc`、`syslogd`、`telnetd` 等基础组件，补齐 `/etc/inittab`、`rcS`、`fstab`、profile、设备节点和启动脚本。
- 在 RootFS 中部署 Qt 运行库、tslib/触摸配置、内核模块、CLI 测试程序和 `LSZ_Terminal` 应用，配置 `QT_QPA_PLATFORM=linuxfb`，实现开发板上电后进入中控 HMI。
- 支持 NFS 根文件系统调试和 eMMC 固化部署两种方式，便于驱动开发阶段快速迭代，以及最终演示阶段稳定启动。

## 面试一分钟讲法

我做的是一个基于 i.MX6ULL 的车载中控原型系统，不只是 Qt 界面，而是把 U-Boot、内核设备树、Linux 驱动、BusyBox 根文件系统和 Qt 应用整条链路打通。底层适配了 LCD、触摸、背光、GPIO、I2C、SPI、UART、USB 摄像头、CSI 摄像头和 FLEXCAN 等外设；驱动层实现了 LED/蜂鸣器、AP3216C、ICM20608 以及 V4L2 虚拟摄像头；应用层用 Qt Widgets 做多页面 HMI，支持车身控制、串口命令、传感器状态、视频预览、拍照录像和图库管理。视频部分基于 V4L2 MMAP 做采集侧零拷贝，并通过丢弃滞后帧和 UI 刷新节流降低延迟。

## 技术关键词

`i.MX6ULL`、`U-Boot`、`Linux Kernel`、`DTS`、`BusyBox RootFS`、`Qt Widgets`、`linuxfb`、`V4L2`、`videobuf2`、`MMAP`、`MJPEG`、`YUYV`、`platform driver`、`miscdevice`、`gpiod`、`I2C driver`、`SPI driver`、`input subsystem`、`QThread`、`QTimer`、`QSerialPort`、`arm-linux-gnueabihf`

## 简历措辞边界

- 推荐说：`基于 V4L2 MMAP 实现采集侧零拷贝`。
- 不建议说：`实现全链路零拷贝`。Qt 显示、YUYV 转 RGB、MJPEG 解码和 AVI 录像编码阶段仍存在转换或拷贝。
- 推荐说：`车载中控 HMI 原型系统` 或 `面向车载中控场景的嵌入式 Linux 项目`。
- 如果面试官追问是否真实量产项目，可以解释为：基于 i.MX6ULL 开发板实现的车载中控功能原型，重点是 BSP、驱动、V4L2 和 Qt HMI 的完整链路。

## 项目支撑点

- Qt HMI：`apps/qt_app/LSZ_Terminal`
- 视频采集：`apps/qt_app/LSZ_Terminal/device/videodevice.*`
- 视频线程：`apps/qt_app/LSZ_Terminal/workers/videoworker.*`
- 拍照录像：`apps/qt_app/LSZ_Terminal/media/mediastore.*`、`apps/qt_app/LSZ_Terminal/media/aviwriter.*`
- 图库：`apps/qt_app/LSZ_Terminal/pages/gallerypage.*`
- 设备树：`dts/imx-emmc.dts`
- LED/蜂鸣器驱动：`drivers/led/led.c`、`drivers/beep/beep.c`
- I2C/SPI 传感器驱动：`drivers/ap3216c/ap3216c.c`、`drivers/icm20608/icm20608.c`
- 触摸/input：`drivers/ft5426/ft5426.c`、`drivers/key/keyinput.c`
- V4L2 虚拟摄像头：`drivers/virtual_vid_drv/virtual_vid_drv.c`
