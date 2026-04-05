# device_manager

基于 **正点原子 i.MX6ULL 开发板** 的 Linux 驱动与 Qt 应用一体化练习项目。


这个仓库不是单一的驱动示例，也不是单一的 Qt 界面示例，而是围绕“**设备树 → 驱动 → `/dev` 字符设备 → CLI 验证 → Qt 应用集成**”这条完整链路逐步搭建起来的工程。当前项目已经包含：

- GPIO 类本地设备：`led`、`beep`
- 传感器设备：`AP3216C`（I2C）、`ICM20608`（SPI）
- CLI 验证程序
- Qt 终端/控制台应用 `LSZ_Terminal`
- 传感器页面中的 `QThread + Worker` 周期采集结构

---

## 1. 项目目标

本项目的核心目标不是只“点亮一个 LED”或“读到一个传感器值”，而是把嵌入式 Linux 开发中常见的几个层次串起来：

1. **硬件与设备树**
   - 明确外设挂接在哪条总线、哪个引脚组
   - 在 DTS 中描述节点、compatible、地址与 pinctrl

2. **驱动层**
   - 基于 platform / I2C / SPI 等框架完成设备匹配
   - 导出字符设备接口，供用户态访问

3. **用户态验证层**
   - 使用最小 CLI 程序验证 `/dev/...` 节点是否可读写
   - 将问题先隔离在“驱动层”还是“应用层”

4. **Qt 应用层**
   - 将底层设备封装为 `device/*`
   - 将界面拆分为 `pages/*`
   - 将周期采集放入 `workers/*`
   - 最终形成可展示、可交互、可扩展的应用

---

## 2. 仓库结构

```text
device_manager/
├── apps/
│   ├── cli/                    # 用户态 CLI 测试程序
│   ├── qt_app/
│   │   └── LSZ_Terminal/       # Qt 应用
│   └── README.md
├── docs/                       # 说明文档
├── drivers/                    # 驱动源码
│   ├── ap3216c/
│   ├── beep/
│   ├── icm20608/
│   └── led/
├── dts/                        # 设备树
├── scripts/                    # 构建/部署辅助脚本
├── .gitignore
└── README.md
```

---

## 3. 当前已完成的主要模块

### 3.1 LED / Beep

- 通过设备树描述 GPIO 引脚
- 驱动导出 `/dev/led`、`/dev/beep`
- Qt 控制页中提供开关控制

### 3.2 AP3216C

- 挂接在 `i2c1`
- 设备树节点地址为 `0x1e`
- 驱动导出 `/dev/ap3216c`
- CLI 可循环读取 `IR / ALS / PS`
- Qt 传感器页可启动 Worker 周期采集并显示

### 3.3 ICM20608

- 挂接在 `ecspi3`
- 设备树节点片选号为 `reg = <0>`
- 驱动导出 `/dev/icm20608`
- Qt 侧已完成工程值封装与周期采集页面显示
  - Gyro：`°/s`
  - Accel：`g`
  - Temp：`°C`

### 3.4 Qt 应用 `LSZ_Terminal`

当前 Qt 工程不是“一个窗口堆所有控件”的原型结构，而是已经整理成：

- `MainWindow`：主壳、导航、页面切换、状态栏
- `pages/ControlPage`：LED / Beep
- `pages/SerialPage`：串口日志与发送
- `pages/SensorPage`：AP3216C / ICM20608
- `device/*`：设备访问封装
- `workers/*`：传感器周期采集线程工作对象

---

## 4. Qt 应用架构说明

### 4.1 页面组织

主界面使用：

- 左侧：`QListWidget`
- 右侧：`QStackedWidget`

形成三页结构：

1. `Device Control`
2. `Serial Monitor`
3. `Sensors`

这种组织方式比把所有控件都堆在一个 `MainWindow` 上更容易维护，也更适合后续继续扩展新的外设页面。

### 4.2 设备封装层

`device/` 目录中的类负责与 `/dev/...` 节点直接交互，例如：

- `LedDevice`
- `BeepDevice`
- `SerialDevice`
- `Ap3216cDevice`
- `Icm20608Device`

这一层只负责：

- `open()`
- `close()`
- `readData() / turnOn() / turnOff()`
- 错误信息返回

不直接操作 UI。

### 4.3 传感器多线程采集层

在 `SensorPage` 中，AP3216C 与 ICM20608 各自对应一个 Worker：

- `Ap3216cWorker`
- `Icm20608Worker`

每个 Worker：

- 继承 `QObject`
- 被 `moveToThread()` 到独立 `QThread`
- 内部使用 `QTimer` 定时触发 `onTimeout()`
- 读取成功后通过 `dataReady(...)` 把数据发回 UI

这种写法比“主线程里定时读设备”更接近正式项目结构，也为后续：

- 高频采集
- 数据缓存
- 曲线显示
- 报警判断
- 文件保存

预留了更清晰的扩展路径。

---

## 5. 当前项目阅读顺序建议

如果你是第一次阅读这个仓库，建议按下面顺序：

### 路线 A：从底层往上看
1. `dts/imx-emmc.dts`
2. `drivers/led` / `drivers/beep`
3. `drivers/ap3216c`
4. `drivers/icm20608`
5. `apps/cli`
6. `apps/qt_app/LSZ_Terminal`

### 路线 B：从应用往下倒推
1. `apps/qt_app/LSZ_Terminal/mainwindow.*`
2. `pages/*`
3. `workers/*`
4. `device/*`
5. 对应 `/dev/...`
6. 回到驱动与设备树

如果你的目标是“真正吃透一条链路”，推荐按**模块**阅读，而不是按目录全扫。例如：

- 只围绕 AP3216C 把 DTS、驱动、CLI、Qt 一口气看完
- 再围绕 ICM20608 重复同样方式

---

## 6. 文档索引

详细说明放在 `docs/` 中：

- `docs/PROJECT_GUIDE.md`  
  项目整体说明与阅读路线

- `docs/DRIVER_DTS_SUMMARY.md`  
  驱动与设备树总览

- `docs/QT_APP_LSZ_TERMINAL.md`  
  Qt 应用结构说明

- `docs/modules/AP3216C.md`  
  AP3216C 模块完整链路

- `docs/modules/ICM20608.md`  
  ICM20608 模块完整链路

---

## 7. 当前状态

当前仓库已经形成一条比较完整的练习路线：

- 本地 GPIO 设备控制
- I2C / SPI 传感器接入
- 字符设备导出
- CLI 验证
- Qt 页面集成
- 传感器多线程周期采集

它既可以作为个人学习记录，也可以继续往“完整嵌入式 Linux 应用项目”方向扩展。

---

## 8. 后续可扩展方向

后续可以继续完善：

- `SensorPage` 中更稳妥的线程停止/异常处理
- 采样频率与 UI 刷新频率分离
- 传感器数据缓存与历史记录
- 曲线绘制 / 简单数据可视化
- 配置文件与日志系统
- 新设备页面（例如存储、网络、相机等）

---

## 9. 说明

本仓库是一个持续迭代中的学习项目。  
README 会优先保持“项目全貌”和“阅读入口”的角色；更详细的模块说明请进入 `docs/` 查看。
