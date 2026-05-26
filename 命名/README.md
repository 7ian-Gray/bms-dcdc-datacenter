# BMS-DCDC Datacenter

BMS-DCDC Datacenter 是一个面向电池管理系统与 DCDC 模块的数据采集、监控、可视化和远程控制平台。

## 项目目标

本项目用于实现 BMS 与 DCDC 设备的实时数据采集、协议解析、数据存储、曲线显示、单体电压监控、温度监控、数据导出和后期远程控制。

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

## 技术路线

### 第一阶段：快速原型

- Frontend: Vue 3 + Element Plus + ECharts
- Backend: FastAPI
- Database: SQLite
- Data Source: Mock Data

### 长期方案

- Collector: Python / Qt C++
- Backend: FastAPI
- Database: PostgreSQL / InfluxDB
- Frontend: Vue 3 + ECharts
- Deployment: Mini PC / Industrial PC / Edge Gateway

## 项目结构

```text
bms-dcdc-datacenter/
├── docs/
├── frontend/
├── backend/
├── collector/
├── database/
├── hardware/
├── test-data/
└── scripts/
