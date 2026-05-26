# BMS-DCDC Datacenter

BMS-DCDC Datacenter 是一个面向电池管理系统与 DCDC 模块的数据采集、监控、可视化和远程控制平台。

## 项目目标

本项目用于实现 BMS 与 DCDC 设备的实时数据采集、协议解析、数据存储、曲线显示、单体电压监控、温度监控、数据导出和后期远程控制。

当前仓库同时包含 Qt 6 Widgets 桌面端本地演示应用。第一阶段使用模拟数据，不接入真实 CAN、Modbus、串口设备、数据库或远程服务。

## 核心功能

- BMS 数据汇总
- DCDC 数据汇总
- 实时运行曲线
- 单体电压柱状图
- 单体电压与温度列表
- 历史数据存储
- CSV / Excel 数据导出
- 在线 / 离线状态判断
- 基础告警
- 后期远程控制扩展

## 平台模块

### BMS 数据汇总

- 电池包电压
- 电池包电流
- SOC
- SOH
- 已充电量
- 已放电量
- 单体最高电压
- 单体最低电压
- 单体最高温度
- 单体最低温度
- 充电限流
- 放电限流

### DCDC 数据汇总

- 高压侧电压 F001
- 低压侧电压 F003
- 低压侧电流 F005
- 板上温度 F019
- 充电限流值 F041
- 放电限流值 F043
- 充电开始电压 u203
- 充电截止电压 u204
- 充电目标电压 u205
- 放电开始电压 u206
- 放电截止电压 u207
- 放电目标电压 u209

### 可视化模块

- 运行数据曲线图
- 单体电压柱状图
- 单体电压列表
- 单体温度列表
- 数据导出模块

## 技术路线

### 第一阶段：快速原型

- Frontend: Vue 3 + Element Plus + ECharts
- Backend: FastAPI
- Database: SQLite
- Data Source: Mock Data

### Qt 桌面端 Stage 1

- Language: C++17
- Framework: Qt 6
- UI: Qt Widgets
- Build System: CMake
- Data Source: Mock Data

### 长期方案

- Collector: Python / Qt C++
- Backend: FastAPI
- Database: PostgreSQL / InfluxDB
- Frontend: Vue 3 + ECharts
- Deployment: Mini PC / Industrial PC / Edge Gateway
- CAN 数据采集
- Modbus 数据采集
- WebSocket 实时推送
- 多设备管理
- AI 状态评估与故障诊断

## 项目结构

```text
bms-dcdc-datacenter/
├── app/
│   └── bms_dcdc_monitor/
├── docs/
├── frontend/
├── backend/
├── collector/
├── database/
├── hardware/
├── test-data/
└── scripts/
```

## 本地构建与运行（Qt 桌面端 Stage 1）

> 当前版本为最小可运行 Qt 6 Widgets 应用，不接入真实硬件，不包含数据库、后端或网络功能。

### 环境要求

- CMake 3.16+
- C++17 编译器（GCC / Clang / MSVC）
- Qt 6（包含 Widgets 模块）

### 构建步骤

在仓库根目录执行：

```bash
cmake -S app/bms_dcdc_monitor -B build/bms_dcdc_monitor
cmake --build build/bms_dcdc_monitor
```

### 运行


```bash
./build/bms_dcdc_monitor/bms_dcdc_monitor
```

Windows 下可执行文件路径通常为：

```text
build/bms_dcdc_monitor/Debug/bms_dcdc_monitor.exe
```
