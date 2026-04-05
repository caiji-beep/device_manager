# 驱动与设备树总览

## 1. 总体思路

这个仓库里的外设接入遵循同一条主线：

1. 在设备树中描述硬件
2. 驱动匹配设备
3. 驱动导出字符设备 `/dev/...`
4. 用户态程序访问 `/dev/...`
5. Qt 应用进一步封装和显示

---

## 2. LED

### 设备树
- 节点名：`led`
- compatible：`lsz,atkalpha-led`
- GPIO：`gpio1_3`
- 极性：`GPIO_ACTIVE_LOW`

### 驱动
- 位于 `drivers/led/`
- 导出 `/dev/led`

### 用户态
- Qt 控制页中提供 LED 开关按钮

---

## 3. Beep

### 设备树
- 节点名：`beep`
- compatible：`lsz,atkalpha-beep`
- GPIO：`gpio5_1`
- 极性：`GPIO_ACTIVE_LOW`

### 驱动
- 位于 `drivers/beep/`
- 导出 `/dev/beep`

### 用户态
- Qt 控制页中提供蜂鸣器开关按钮

---

## 4. AP3216C

### 设备树
- 节点位置：`&i2c1`
- compatible：`lsz,ap3216c`
- I2C 地址：`0x1e`

### 驱动
- 位于 `drivers/ap3216c/`
- 导出 `/dev/ap3216c`

### 用户态
- CLI：读取 `IR / ALS / PS`
- Qt：`Ap3216cDevice` + `Ap3216cWorker`

---

## 5. ICM20608

### 设备树
- 节点位置：`&ecspi3`
- compatible：`lsz,icm20608`
- 片选号：`reg = <0>`
- SPI 频率：`spi-max-frequency = <8000000>`

### 驱动
- 位于 `drivers/icm20608/`
- 导出 `/dev/icm20608`

### 用户态
- Qt：`Icm20608Device` + `Icm20608Worker`
- 设备类中进一步完成单位换算

---

## 6. 如何理解“改设备树”和“写驱动”的关系

并不是每个设备都必须从零实现一个复杂驱动，项目中常见的工作其实包括：

- 设备树适配
- compatible 对齐
- pinctrl 配置
- 总线挂接关系确认
- 用户态接口验证
- 上层应用集成

因此，“只改设备树”不代表没有做驱动工作。  
真正重要的是：你能不能把 **DTS → 驱动 → `/dev` → 用户态 → Qt** 整条链路讲清楚。
