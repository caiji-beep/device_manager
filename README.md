# device_manager

## 1. 项目定位

`device_manager` 是一个基于正点原子 i.MX6ULL Linux 开发板的嵌入式 Linux 综合项目。

这个仓库不是单一驱动 demo，也不是单一 Qt 界面 demo，而是围绕下面这条完整工程链路持续迭代：

```text
设备树 DTS
  ↓
Linux 内核驱动
  ↓
/dev 设备节点或 input/video 子系统节点
  ↓
CLI 最小验证程序
  ↓
Qt 图形化应用 LSZ_Terminal
  ↓
面向演示和面试的完整设备管理终端
```

当前项目覆盖了：

- GPIO 本地控制：LED、蜂鸣器
- I2C 传感器：AP3216C、FT5426 触摸
- SPI 传感器：ICM20608
- input 子系统：按键、触摸
- V4L2 视频：真实 UVC 摄像头、虚拟摄像头驱动
- 串口通信：`/dev/ttymxc2`
- CLI 验证程序：I2C、SPI、串口、V4L2
- Qt Widgets 应用：多页面设备管理终端
- 媒体功能：拍照、MJPEG AVI 录像、图库浏览与删除

项目目标是把“驱动能力”和“应用能力”放在同一个工程里验证，而不是只停留在某一层。

## 2. 仓库结构

```text
device_manager/
├── apps/
│   ├── cli/                              # 用户态最小验证程序
│   │   ├── i2c/ap3216c/                  # AP3216C CLI
│   │   ├── spi/icm20608/                 # ICM20608 CLI
│   │   ├── serial/                       # 串口 CLI
│   │   └── video/                        # V4L2 CLI 测试
│   ├── qt_app/
│   │   └── LSZ_Terminal/                 # Qt Widgets 综合终端
│   └── README.md
├── docs/                                 # 项目文档与模块说明
│   ├── PROJECT_GUIDE.md
│   ├── DRIVER_DTS_SUMMARY.md
│   ├── QT_APP_LSZ_TERMINAL.md
│   └── modules/
├── drivers/                              # Linux 驱动源码
│   ├── led/                              # platform + misc LED
│   ├── beep/                             # platform + misc 蜂鸣器
│   ├── ap3216c/                          # I2C + misc 光距传感器
│   ├── icm20608/                         # SPI + misc 六轴 IMU
│   ├── key/                              # input 按键
│   ├── ft5426/                           # I2C + input 触摸屏
│   └── virtual_vid_drv/                  # V4L2 虚拟摄像头
├── dts/                                  # i.MX6ULL 设备树
│   ├── imx-emmc.dts
│   └── imx6ull.dtsi
├── scripts/
│   ├── generate_mjpeg/                   # MJPEG 数组生成脚本
│   └── test/snake/                       # Qt 小测试程序
├── README.md                             # 当前总览文档
└── .gitignore
```

## 3. 分层架构

### 3.1 设备树层

目录：

```text
dts/
```

核心文件：

```text
dts/imx-emmc.dts
dts/imx6ull.dtsi
```

设备树负责描述硬件事实：

- 外设挂在哪条总线
- GPIO 使用哪个 bank 和 pin
- I2C 地址是多少
- SPI 片选是多少
- pinctrl 怎么复用
- compatible 字符串如何与驱动匹配

当前项目中比较重要的设备树节点：

| 模块 | 设备树位置 | compatible | 用户态节点 |
|---|---|---|---|
| LED | 根节点 `led` | `lsz,atkalpha-led` | `/dev/led` |
| Beep | 根节点 `beep` | `lsz,atkalpha-beep` | `/dev/beep` |
| AP3216C | `&i2c1` | `lsz,ap3216c` | `/dev/ap3216c` |
| FT5426 | `&i2c1` | `edt,edt-ft5426` | input event |
| ICM20608 | `&ecspi3` | `lsz,icm20608` | `/dev/icm20608` |
| Key | GPIO keys | `gpio-keys` | input event |
| OV5640 | camera 节点 | `ovti,ov5640` | V4L2 相关 |

面试时要能讲清楚：

```text
DTS 的 compatible 不是摆设，它是 platform/i2c/spi driver match 的入口。
```

### 3.2 驱动层

目录：

```text
drivers/
```

当前驱动模块：

| 目录 | 驱动类型 | 主要机制 | 当前用途 |
|---|---|---|---|
| `drivers/led` | platform driver | GPIO + miscdevice | 导出 `/dev/led` |
| `drivers/beep` | platform driver | GPIO + miscdevice | 导出 `/dev/beep` |
| `drivers/ap3216c` | i2c driver | regmap 思路/寄存器读写 + miscdevice | 导出 `/dev/ap3216c` |
| `drivers/icm20608` | spi driver | SPI 寄存器读写 + miscdevice | 导出 `/dev/icm20608` |
| `drivers/key` | input driver | input_report_key | 按键输入事件 |
| `drivers/ft5426` | i2c input driver | input multitouch | 触摸屏输入事件 |
| `drivers/virtual_vid_drv` | v4l2 driver | video_device + videobuf2 + v4l2_ctrl | 虚拟摄像头 |

多数驱动目录都使用类似 Makefile：

```makefile
KERNELDIR := /home/lsz/linux/IMX6ULL/linux/linux-imx-rel_imx_4.1.15_2.1.0_ga
CURRENT_PATH := $(shell pwd)

obj-m := xxx.o

build:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
```

编译方式：

```bash
cd drivers/led
make build
```

加载方式：

```bash
insmod led.ko
dmesg | tail
ls /dev/led
```

卸载方式：

```bash
rmmod led
```

### 3.3 CLI 验证层

目录：

```text
apps/cli/
```

CLI 的定位是“最小闭环验证”，作用是先确认驱动是否可用，再接 Qt。

当前 CLI 模块：

```text
apps/cli/i2c/ap3216c/ap3216cAPP.c
apps/cli/spi/icm20608/icm20608APP.c
apps/cli/serial/
apps/cli/video/03_video_params/video_test.c
apps/cli/video/04_video_get_data/video_test.c
apps/cli/video/05_video_brightness/video_test.c
```

典型交叉编译：

```bash
arm-linux-gnueabihf-gcc -o ap3216cAPP ap3216cAPP.c
arm-linux-gnueabihf-gcc -o icm20608APP icm20608APP.c
arm-linux-gnueabihf-gcc -o video_test video_test.c -lpthread
```

CLI 层的价值：

- 驱动没通时，不让 Qt 背锅
- Qt 出问题时，可以用 CLI 反证底层是否正常
- 面试时可以说明自己做过驱动到用户态的完整验证

### 3.4 Qt 应用层

目录：

```text
apps/qt_app/LSZ_Terminal/
```

Qt 应用是当前项目的最终展示入口。

主要子目录：

```text
controller/      # 串口协议/控制器
device/          # 对 /dev 节点和 V4L2 设备的 C++ 封装
workers/         # 采集线程 worker
pages/           # Qt 页面
media/           # 拍照、录像、图库文件管理
```

当前页面：

| 页面 | 功能 |
|---|---|
| Home | 模块入口 |
| ControlPage | LED/Beep 控制 |
| SerialPage | 串口发送、接收、日志 |
| SensorPage | AP3216C、ICM20608 周期采集 |
| VideoPage | V4L2 视频预览、亮度、格式、拍照、录像 |
| GalleryPage | 照片/录像浏览与删除 |

Qt 应用详细说明见：

```text
apps/qt_app/LSZ_Terminal/README.md
docs/QT_APP_LSZ_TERMINAL.md
```

## 4. 主要模块复习

### 4.1 LED / Beep

链路：

```text
dts/imx-emmc.dts
  ↓
drivers/led/led.c 或 drivers/beep/beep.c
  ↓
misc_register
  ↓
/dev/led 或 /dev/beep
  ↓
Qt LedDevice / BeepDevice
  ↓
ControlPage 按钮控制
```

关键点：

- 使用 platform driver
- 通过 compatible 匹配设备树节点
- 使用 GPIO descriptor 或 of_get_named_gpio 类接口获取 GPIO
- 使用 miscdevice 简化字符设备注册
- 用户态通过写 `/dev/led`、`/dev/beep` 控制状态

面试表达：

```text
我用 platform driver 接入板载 LED 和蜂鸣器，通过设备树描述 GPIO，驱动侧注册 misc 设备导出 /dev 节点，Qt 侧封装成 LedDevice 和 BeepDevice，最终在控制页实现开关控制。
```

### 4.2 AP3216C

链路：

```text
dts/imx-emmc.dts 中 &i2c1 节点
  ↓
compatible = "lsz,ap3216c"
  ↓
drivers/ap3216c/ap3216c.c
  ↓
/dev/ap3216c
  ↓
apps/cli/i2c/ap3216c/ap3216cAPP.c
  ↓
Qt Ap3216cDevice
  ↓
Ap3216cWorker
  ↓
SensorPage
```

AP3216C 数据：

- IR：红外数据
- ALS：环境光
- PS：接近检测

关键点：

- I2C client 由设备树实例化
- 驱动 probe 中初始化传感器
- 用户态读取 `/dev/ap3216c`
- Qt worker 线程周期读取，避免阻塞 UI

### 4.3 ICM20608

链路：

```text
dts/imx-emmc.dts 中 &ecspi3 节点
  ↓
compatible = "lsz,icm20608"
  ↓
drivers/icm20608/icm20608.c
  ↓
/dev/icm20608
  ↓
apps/cli/spi/icm20608/icm20608APP.c
  ↓
Qt Icm20608Device
  ↓
Icm20608Worker
  ↓
SensorPage
```

ICM20608 数据：

- 加速度
- 角速度
- 温度

关键点：

- SPI driver 通过 compatible 匹配
- 设备树中需要关注 `reg`、`spi-max-frequency`、片选和 pinctrl
- Qt 侧把原始值转换成工程单位显示

### 4.4 Key 与 FT5426

这两个模块偏 input 子系统练习。

Key：

```text
drivers/key/keyinput.c
```

核心点：

- 注册 input device
- 上报按键事件
- 用户态通过 input event 读取

FT5426：

```text
drivers/ft5426/ft5426.c
```

核心点：

- I2C 触摸芯片驱动
- 注册 input device
- 上报多点触摸事件

当前它们不是 Qt 主应用的核心展示模块，但很适合作为“我也接触过 input 子系统”的补充。

### 4.5 V4L2 虚拟摄像头

目录：

```text
drivers/virtual_vid_drv/
```

核心文件：

```text
virtual_vid_drv.c
red.c
green.c
blue.c
cyan_640x480.h
magenta_640x480.h
yellow_640x480.h
```

驱动机制：

- `video_device`
- `v4l2_device`
- `vb2_queue`
- `vb2_vmalloc_memops`
- `v4l2_ctrl_handler`
- timer 模拟 30fps 出帧

支持格式：

- MJPEG
- YUYV

MJPEG 数据：

- `640x480`：cyan、magenta、yellow
- `800x600`：red、green、blue

YUYV 测试模式：

```text
0 - 灰度背景，亮度可调
1 - 运动白色方块，用于观察延迟
2 - 彩条
```

控制项：

- `V4L2_CID_BRIGHTNESS`
- `V4L2_CID_AUTOBRIGHTNESS`
- `V4L2_CID_TEST_PATTERN`

重要细节：

驱动使用了 V4L2 control cluster。对于 auto/manual 亮度控制，`s_ctrl` 里要理解 master control 和 cluster member 的关系，不能简单认为应用层设置哪个 control，驱动就一定以哪个 control id 回调。

### 4.6 V4L2 Qt 视频应用

相关文件：

```text
apps/qt_app/LSZ_Terminal/device/videodevice.*
apps/qt_app/LSZ_Terminal/workers/videoworker.*
apps/qt_app/LSZ_Terminal/pages/videopage.*
```

功能：

- 扫描 `/dev/video*`
- 支持真实 UVC 摄像头 `/dev/video1`
- 支持虚拟摄像头
- 枚举格式和分辨率
- 支持 MJPEG/YUYV
- 支持亮度控制
- 虚拟摄像头 YUYV 模式下动态显示 Pattern 控件
- 支持拍照保存 JPG
- 支持录像保存 MJPEG AVI

V4L2 采集流程：

```text
open
  ↓
VIDIOC_QUERYCAP
  ↓
VIDIOC_ENUM_FMT / VIDIOC_ENUM_FRAMESIZES
  ↓
VIDIOC_S_FMT
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
decode/convert
  ↓
VIDIOC_QBUF
```

低延迟策略：

- 使用 V4L2 MMAP，采集侧避免 read 带来的整帧拷贝
- 采集线程中尽量清空 ready buffer
- 丢弃滞后帧，只显示最新帧
- UI 刷新节流到约 30fps
- 页面不可见时不刷新 QLabel

### 4.7 拍照、录像、图库

相关文件：

```text
apps/qt_app/LSZ_Terminal/media/mediastore.*
apps/qt_app/LSZ_Terminal/media/aviwriter.*
apps/qt_app/LSZ_Terminal/pages/gallerypage.*
```

保存路径：

```text
~/LSZ_Terminal_Media/photos
~/LSZ_Terminal_Media/videos
```

如果 Qt 程序在开发板上以 root 运行，路径通常是：

```text
/root/LSZ_Terminal_Media/photos
/root/LSZ_Terminal_Media/videos
```

拍照：

```text
QImage -> JPG -> photos
```

录像：

```text
QImage -> JPEG frame -> MJPEG AVI -> videos
```

图库：

- 扫描 photos/videos
- 图片显示缩略图
- 视频显示 AVI 卡片
- 支持删除文件
- 删除后刷新 UI

## 5. 编译与运行

### 5.1 编译内核驱动

示例：

```bash
cd drivers/led
make build
```

其他驱动目录类似：

```bash
cd drivers/beep && make build
cd drivers/ap3216c && make build
cd drivers/icm20608 && make build
cd drivers/virtual_vid_drv && make build
```

加载示例：

```bash
insmod led.ko
insmod beep.ko
insmod ap3216c.ko
insmod icm20608.ko
insmod virtual_camera.ko
```

查看：

```bash
dmesg | tail -n 50
ls /dev/led /dev/beep /dev/ap3216c /dev/icm20608
ls /dev/video*
```

### 5.2 编译 CLI

AP3216C：

```bash
cd apps/cli/i2c/ap3216c
arm-linux-gnueabihf-gcc -o ap3216cAPP ap3216cAPP.c
```

ICM20608：

```bash
cd apps/cli/spi/icm20608
arm-linux-gnueabihf-gcc -o icm20608APP icm20608APP.c
```

V4L2 brightness 测试：

```bash
cd apps/cli/video/05_video_brightness
arm-linux-gnueabihf-gcc -o video_test video_test.c -lpthread
```

### 5.3 编译 Qt 应用

进入 Qt 工程：

```bash
cd apps/qt_app/LSZ_Terminal
qmake LSZ_Terminal.pro
make -j2
```

如果使用 ARM Qt 工具链：

```bash
/home/lsz/linux/IMX6ULL/tool/arm-qt/bin/qmake LSZ_Terminal.pro
make -j2
```

开发板上运行：

```bash
export QT_QPA_PLATFORM=linuxfb
./LSZ_Terminal
```

后台运行：

```bash
export QT_QPA_PLATFORM=linuxfb
./LSZ_Terminal >/dev/null 2>&1 &
```

## 6. 调试命令

### 6.1 通用

```bash
dmesg | tail -n 50
lsmod
ls /dev
```

### 6.2 字符设备

```bash
ls -l /dev/led /dev/beep /dev/ap3216c /dev/icm20608
```

### 6.3 input 设备

```bash
cat /proc/bus/input/devices
hexdump /dev/input/event0
```

### 6.4 V4L2

```bash
ls /dev/video*
v4l2-ctl -d /dev/video1 --all
v4l2-ctl -d /dev/video2 --all
v4l2-ctl -d /dev/video2 --list-formats-ext
```

设置虚拟摄像头亮度：

```bash
v4l2-ctl -d /dev/video2 --set-ctrl=brightness=180
```

设置虚拟摄像头 YUYV 测试模式：

```bash
v4l2-ctl -d /dev/video2 --set-ctrl=test_pattern=1
```

查看媒体文件：

```bash
ls ~/LSZ_Terminal_Media/photos
ls ~/LSZ_Terminal_Media/videos
```

## 7. 面试复习路线

### 7.1 总体讲法

可以这样介绍项目：

```text
我做了一个基于 i.MX6ULL 的嵌入式 Linux 设备管理项目，覆盖设备树、内核驱动、CLI 验证和 Qt 应用。底层包括 GPIO、I2C、SPI、input 和 V4L2；上层 Qt 应用实现了 LED/蜂鸣器控制、串口通信、传感器周期采集、UVC/虚拟摄像头预览、拍照录像和图库管理。
```

### 7.2 驱动侧亮点

可以重点讲：

- platform driver + DTS 匹配
- miscdevice 导出 `/dev` 节点
- I2C driver 读取 AP3216C
- SPI driver 读取 ICM20608
- input 子系统上报 key/touch 事件
- V4L2 虚拟摄像头、videobuf2 buffer 队列
- V4L2 control handler 和 control cluster

### 7.3 应用侧亮点

可以重点讲：

- Qt 页面模块化
- device 层封装 `/dev` 和 V4L2
- worker 线程采集，避免 UI 阻塞
- V4L2 MMAP 采集侧零拷贝
- 视频低延迟策略：丢弃滞后帧，只显示最新帧
- 拍照、MJPEG AVI 录像、图库删除

### 7.4 简历推荐写法

```text
基于 i.MX6ULL Linux 平台实现设备管理系统，完成 LED/蜂鸣器、AP3216C、ICM20608、串口、UVC 摄像头和 V4L2 虚拟摄像头的驱动联调与 Qt 应用集成。
```

```text
编写/适配 platform、I2C、SPI、input、V4L2 等类型驱动，通过设备树完成硬件描述与驱动匹配，并使用 CLI 程序完成用户态最小闭环验证。
```

```text
基于 V4L2 MMAP 实现视频采集侧零拷贝，支持 MJPEG/YUYV 格式枚举、分辨率选择、亮度控制、虚拟摄像头 test pattern 和 480P@30fps 视频预览。
```

```text
设计 Qt Widgets 多页面应用架构，采用 device 封装层、worker 采集线程和页面展示层解耦，实现传感器周期采集、视频预览、拍照录像和图库管理。
```

### 7.5 零拷贝表述边界

推荐说：

```text
基于 V4L2 MMAP 实现采集侧零拷贝。
```

不要直接说：

```text
全链路零拷贝。
```

原因：

- V4L2 MMAP 阶段确实避免了 read 带来的整帧拷贝
- Qt 显示阶段仍有 MJPEG 解码或 YUYV 到 RGB 转换
- 当前录像阶段会把 QImage 编码成 JPEG 后写入 AVI

更严谨的表达：

```text
采集侧使用 MMAP 零拷贝，显示和录像链路根据格式存在解码、颜色转换或编码开销。
```

## 8. 推荐阅读顺序

### 8.1 按层次阅读

```text
dts/imx-emmc.dts
  ↓
drivers/
  ↓
apps/cli/
  ↓
apps/qt_app/LSZ_Terminal/device/
  ↓
apps/qt_app/LSZ_Terminal/workers/
  ↓
apps/qt_app/LSZ_Terminal/pages/
```

### 8.2 按模块阅读

AP3216C：

```text
dts/imx-emmc.dts
drivers/ap3216c/
apps/cli/i2c/ap3216c/
apps/qt_app/LSZ_Terminal/device/ap3216cdevice.*
apps/qt_app/LSZ_Terminal/workers/ap3216cworker.*
apps/qt_app/LSZ_Terminal/pages/sensorpage.*
```

ICM20608：

```text
dts/imx-emmc.dts
drivers/icm20608/
apps/cli/spi/icm20608/
apps/qt_app/LSZ_Terminal/device/icm20608device.*
apps/qt_app/LSZ_Terminal/workers/icm20608worker.*
apps/qt_app/LSZ_Terminal/pages/sensorpage.*
```

视频：

```text
drivers/virtual_vid_drv/
apps/cli/video/
apps/qt_app/LSZ_Terminal/device/videodevice.*
apps/qt_app/LSZ_Terminal/workers/videoworker.*
apps/qt_app/LSZ_Terminal/pages/videopage.*
apps/qt_app/LSZ_Terminal/media/
apps/qt_app/LSZ_Terminal/pages/gallerypage.*
```

## 9. 当前项目状态

已经形成闭环的模块：

- LED / Beep：DTS -> driver -> `/dev` -> Qt
- AP3216C：DTS -> I2C driver -> CLI -> Qt
- ICM20608：DTS -> SPI driver -> CLI -> Qt
- 串口：CLI/Qt 应用
- V4L2 视频：真实 UVC + 虚拟摄像头 -> Qt
- 媒体保存：Qt 拍照/录像 -> 图库

偏驱动练习或后续扩展模块：

- Key input
- FT5426 touch
- scripts/test/snake Qt 测试程序

## 10. 后续优化方向

驱动侧：

- 整理各驱动 README，补充设备树节点、probe/remove、用户态接口
- 将 key 和 FT5426 纳入 Qt 应用展示页
- 虚拟摄像头从 `vb2_vmalloc` 扩展到更贴近真实硬件的 DMA buffer 思路
- 增加 V4L2 controls 和多实例支持

应用侧：

- 增加配置文件，记住上次设备、格式、分辨率
- 录像编码放到独立线程
- MJPEG 输入时直接写原始 JPEG payload 到 AVI，减少二次编码
- 图库增加打开预览、播放录像、批量删除
- 传感器增加曲线显示和数据导出

工程侧：

- 根目录增加统一 build/deploy 脚本
- 清理编译产物和 `.mod.c/.o/.ko` 管理策略
- 增加模块级测试说明
- 统一中英文命名风格

## 11. 文档索引

总览：

```text
README.md
docs/PROJECT_GUIDE.md
docs/DRIVER_DTS_SUMMARY.md
```

Qt 应用：

```text
apps/qt_app/LSZ_Terminal/README.md
docs/QT_APP_LSZ_TERMINAL.md
```

传感器模块：

```text
docs/modules/AP3216C.md
docs/modules/ICM20608.md
```

应用与脚本：

```text
apps/README.md
apps/cli/README.md
scripts/README.md
```

## 12. 一句话总结

这个仓库最重要的价值不是某一个单独模块，而是它把 i.MX6ULL 上常见的嵌入式 Linux 开发链路串起来了：

```text
DTS 配硬件
  ↓
驱动管设备
  ↓
CLI 验底层
  ↓
Qt 做产品化展示
  ↓
结合 V4L2/传感器/串口/媒体功能形成完整项目
```
