# apps

`apps/` 存放 `device_manager` 的用户态程序，主要分成两条线：

- `cli/`：命令行最小验证程序
- `qt_app/LSZ_Terminal/`：Qt Widgets 图形化设备管理终端

用户态程序的核心作用是承接驱动层导出的接口，把 `/dev/...`、input event、V4L2 video node 这些底层能力验证并产品化展示出来。

## 1. cli

目录：

```text
apps/cli/
├── i2c/ap3216c/                 # AP3216C CLI
├── spi/icm20608/                # ICM20608 CLI
├── serial/                      # 串口 CLI
└── video/                       # V4L2 CLI 测试
```

CLI 程序的定位是“最小闭环验证”：

- 驱动刚写完时，先用 CLI 验证 `/dev` 节点是否可用
- Qt 应用出问题时，用 CLI 判断问题在驱动层还是应用层
- 面试讲项目时，可以证明自己不是只写 UI，而是完成了驱动到用户态的联调

典型模块：

| 程序 | 作用 |
|---|---|
| `i2c/ap3216c/ap3216cAPP.c` | 读取 AP3216C 的 IR/ALS/PS 数据 |
| `spi/icm20608/icm20608APP.c` | 读取 ICM20608 加速度、角速度、温度 |
| `serial/` | 串口打开、配置、收发测试 |
| `video/03_video_params` | 枚举 V4L2 参数 |
| `video/04_video_get_data` | V4L2 采集数据 |
| `video/05_video_brightness` | V4L2 MMAP 采集与亮度控制 |

示例编译：

```bash
arm-linux-gnueabihf-gcc -o ap3216cAPP ap3216cAPP.c
arm-linux-gnueabihf-gcc -o icm20608APP icm20608APP.c
arm-linux-gnueabihf-gcc -o video_test video_test.c -lpthread
```

## 2. qt_app / LSZ_Terminal

目录：

```text
apps/qt_app/LSZ_Terminal/
```

这是当前项目的主要图形化应用，采用 Qt Widgets 实现。

当前页面：

- Home：主页导航
- Control：LED/蜂鸣器控制
- Serial：串口日志与发送
- Sensor：AP3216C、ICM20608 周期采集
- Video：V4L2 视频预览、格式选择、亮度、拍照、录像
- Gallery：照片/录像浏览与删除

核心结构：

```text
controller/      # 串口控制逻辑
device/          # 用户态设备封装
workers/         # 采集线程 worker
pages/           # 页面
media/           # 拍照、录像、图库文件管理
```

编译：

```bash
cd apps/qt_app/LSZ_Terminal
qmake LSZ_Terminal.pro
make -j2
```

开发板 linuxfb 运行：

```bash
export QT_QPA_PLATFORM=linuxfb
./LSZ_Terminal
```

后台运行：

```bash
export QT_QPA_PLATFORM=linuxfb
./LSZ_Terminal >/dev/null 2>&1 &
```

详细说明：

```text
apps/qt_app/LSZ_Terminal/README.md
docs/QT_APP_LSZ_TERMINAL.md
```

## 3. 推荐调试顺序

当某个模块不工作时，建议按下面顺序排查：

```text
驱动是否加载
  ↓
/dev 节点是否存在
  ↓
CLI 是否能读写
  ↓
Qt device 封装是否打开成功
  ↓
worker/page 是否正确显示
```

这个顺序能避免把底层问题误判成 Qt 问题。
