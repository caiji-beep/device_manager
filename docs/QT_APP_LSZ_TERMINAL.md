# LSZ_Terminal Qt 应用说明

## 1. 工程定位

`LSZ_Terminal` 是当前仓库中的图形界面应用工程。  
它不是单纯的界面练习，而是底层设备在 Qt 上的集成入口。

---

## 2. 工程结构

```text
LSZ_Terminal/
├── controller/
│   └── serialcontroller.*
├── device/
│   ├── ap3216cdevice.*
│   ├── beepdevice.*
│   ├── icm20608device.*
│   ├── leddevice.*
│   ├── outputdevice.*
│   └── serialdevice.*
├── pages/
│   ├── controlpage.*
│   ├── serialpage.*
│   └── sensorpage.*
├── workers/
│   ├── ap3216cworker.*
│   └── icm20608worker.*
├── mainwindow.*
├── main.cpp
└── LSZ_Terminal.pro
```

---

## 3. MainWindow 的职责

`MainWindow` 当前主要承担：

- 创建页面
- 组织左侧导航与右侧堆叠页
- 初始化本地设备（LED / Beep）
- 初始化串口
- 把状态消息统一送到状态栏

也就是说，`MainWindow` 已经不再直接堆所有按钮和传感器控件，而是做成“主壳”。

---

## 4. 页面划分

### ControlPage
负责：
- LED 开关
- Beep 开关

### SerialPage
负责：
- 串口状态
- 日志显示
- 命令发送

### SensorPage
负责：
- AP3216C 显示
- ICM20608 显示
- 传感器启动/停止
- 周期刷新

---

## 5. 设备封装层

`device/*` 负责把 `/dev/...` 访问包装成 C++ 类。  
这样页面层不直接写裸 `open/read/write`，而是使用统一接口。

例如：

- `Ap3216cDevice::readData(...)`
- `Icm20608Device::readData(...)`
- `LedDevice::turnOn()/turnOff()`

这种做法的优点是：
- UI 更干净
- 错误处理更集中
- 后续扩展更容易

---

## 6. 传感器线程模型

当前 `SensorPage` 使用的是：

- `QThread`
- `QObject Worker`
- `QTimer`

也就是：

1. 创建 Worker
2. 将 Worker `moveToThread()`
3. 在 Worker 内部创建 `QTimer`
4. 定时调用 `onTimeout()`
5. 读取设备并发出 `dataReady(...)`
6. UI 线程收到信号后更新标签

这种结构比“UI 主线程里直接定时读设备”更适合继续扩展。

---

## 7. 当前阶段的评价

当前 Qt 结构已经从“演示式界面”迈向“可扩展工程”：

- 主窗口壳化
- 页面分离
- 设备访问单独封装
- 传感器进入 Worker 线程

后续如果继续增加新设备，建议继续保持：
- 新增 `device/*`
- 新增页面或页面内卡片
- 需要周期采集时新增 worker
