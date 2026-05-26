# AGENTS.md

## Project

This repository contains a Qt C++ desktop monitoring application for a BMS-DCDC energy system.

The first milestone is a local desktop demo. It uses mock data only and does not connect to real CAN, Modbus, serial devices, databases, or remote services.

## Application Name

BMS-DCDC 能量系统数据监控平台

## Technology Stack

- Language: C++17 or newer
- Framework: Qt 6
- UI: Qt Widgets
- Charts: Qt Charts
- Build System: CMake

## First Milestone Scope

The first demo must implement:

1. Main window can open.
2. BMS summary cards show mock data.
3. DCDC summary cards show mock data.
4. Runtime line chart refreshes every second.
5. Cell voltage bar chart refreshes every second.
6. Cell voltage and temperature tables refresh every second.
7. CSV export works.
8. Communication status shows Online / Offline / Timeout.
9. Basic alarm style is visible.

## Layout

Top:
- System title
- Device status
- Communication status
- Current time

First row:
- BMS data summary
- DCDC data summary

Second row:
- Runtime data line chart
- Accumulated charge/discharge and export button

Third row:
- Cell voltage bar chart

Fourth row:
- Cell voltage table
- Cell temperature table

## Coding Rules

- Keep UI code organized in MainWindow.
- Put data structures in DataModel.h.
- Put mock data generation in MockDataGenerator.
- Put CSV export logic in CsvExporter.
- Do not implement real CAN or Modbus in the first milestone.
- Do not add a database in the first milestone.
- Do not add a backend or web service in the first milestone.
- Prefer clear, maintainable code over complex abstractions.
- Use English class names and field names.
- UI labels can use Chinese.

## Data Model

BMS data should include:
- pack voltage
- pack current
- SOC
- SOH
- charged capacity
- discharged capacity
- max/min cell voltage
- max/min cell temperature
- charge/discharge current limit
- cell voltage array
- cell temperature array

DCDC data should include:
- high-voltage side voltage
- low-voltage side voltage
- low-voltage side current
- board temperature
- charge/discharge current limit
- charge/discharge start, stop, target voltage

## Verification

After each change:
- The project should configure with CMake.
- The project should build.
- The app should launch.
- Mock data should refresh without crashing.
