# 项目总览与阅读指南

## 1. 项目定位

本项目围绕 i.MX6ULL 开发板上的几个典型外设，练习 Linux 嵌入式开发中的完整链路：

- 设备树配置
- 驱动编写/适配
- 用户态 CLI 验证
- Qt 图形界面集成

项目的重点不是单个文件的技巧，而是“整条链路打通”。

---

## 2. 建议阅读方式

### 方式 A：按层次阅读
1. DTS
2. 驱动
3. CLI
4. Qt 封装
5. Qt 页面
6. Worker 线程

### 方式 B：按模块阅读
例如先只看 AP3216C：
1. `dts/imx-emmc.dts`
2. `drivers/ap3216c`
3. `apps/cli/...`
4. `apps/qt_app/LSZ_Terminal/device/ap3216cdevice.*`
5. `apps/qt_app/LSZ_Terminal/workers/ap3216cworker.*`
6. `apps/qt_app/LSZ_Terminal/pages/sensorpage.*`

第二种更容易真正理解“一个模块如何贯穿全栈”。

---

## 3. 当前仓库的核心模块

### 本地控制
- LED
- Beep

### 传感器
- AP3216C（I2C）
- ICM20608（SPI）

### 应用
- CLI 验证程序
- Qt 应用 `LSZ_Terminal`

---

## 4. 代码结构理解建议

### `drivers/`
看底层接口和 `/dev/...` 的来源。

### `dts/`
看每个设备挂在哪条总线、compatible 是什么、地址/片选是多少。

### `apps/cli/`
看最小用户态访问方式。

### `apps/qt_app/LSZ_Terminal/device/`
看 Qt 侧如何把 `/dev/...` 包装成 C++ 类。

### `apps/qt_app/LSZ_Terminal/pages/`
看 UI 如何使用这些设备封装。

### `apps/qt_app/LSZ_Terminal/workers/`
看传感器如何进入 `QThread + Worker + QTimer` 周期采集模式。

---

## 5. 当前 Qt 结构

当前 Qt 应用已经从“单窗口堆控件”演进为：

- `MainWindow`：整体壳
- `ControlPage`：LED / Beep 控制
- `SerialPage`：串口日志与命令
- `SensorPage`：AP3216C / ICM20608
- `workers/*`：采集线程
- `device/*`：设备封装

后续新增模块时，建议继续保持“主窗口只做导航，功能单独成页”的思路。
