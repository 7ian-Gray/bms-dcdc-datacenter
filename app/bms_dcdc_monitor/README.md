# BMS-DCDC 通信模块迁移包

本迁移包适用于现有项目：

```text
app/bms_dcdc_monitor
```

迁移目标是将旧项目中可复用的 CAN、Modbus 和协议解析功能接入当前项目，
同时保留现有的 14:9 BMS-DCDC 数据监控仪表盘，不使用旧版界面覆盖当前界面。

---

## 一、原文件迁移方案

| 原始文件 | 处理结论 | 新位置或替代文件 |
|---|---|---|
| `candataanalyzer.cpp`、`CanDataAnalyzer.h` | 重构后迁移 | `src/protocol/BmsCanParser.*` |
| `canthread.cpp`、`canthread.h` | 不直接复制 | 使用 `src/communication/ControlCanWorker.*` 替代 |
| `ControlCAN.h` | 保留为 Windows 厂商接口文件 | `vendor/windows/ControlCAN.h` |
| `ControlCAN.dll` | 仅供 Windows 运行时使用 | `vendor/windows/ControlCAN.dll` |
| `modbusthread.cpp`、`modbusthread.h` | 不直接复制 | 使用 `src/communication/ModbusRtuClient.*` 替代 |
| `mainwindow.cpp`、`mainwindow.h`、`mainwindow.ui` | 不迁移 | 保留当前项目仪表盘，按中文接入说明修改 |
| `main.cpp` | 不迁移 | 保留当前项目入口文件 |

---

## 二、为什么不能直接覆盖 `mainwindow`

当前 `bms-dcdc-datacenter` 项目的界面是通过 C++ 动态创建的，
已经实现以下布局：

- 顶部：系统标题、CAN/Modbus 状态、时间、导出按钮；
- 左上：BMS 和 DCDC 核心数据；
- 右上：单体电压、温度和统计信息；
- 左下：运行数据曲线；
- 右下：单体电压柱状图和一致性判断。

旧项目的 `mainwindow.ui` 是另一套界面结构。直接覆盖会导致：

1. 当前 14:9 仪表盘布局丢失；
2. Qt Designer 控件名称与现有代码不一致；
3. 当前 `DashboardData` 数据模型失效；
4. CAN、Modbus 代码重新与界面强耦合；
5. 后续跨平台维护更加困难。

因此本迁移包只迁移通信层和协议层。

---

## 三、推荐目录结构

将迁移包中的文件放入：

```text
app/bms_dcdc_monitor/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── mainwindow.cpp
│   ├── mainwindow.h
│   ├── DataModel.h
│   ├── MockDataGenerator.cpp
│   ├── MockDataGenerator.h
│   ├── communication/
│   │   ├── CanFrame.h
│   │   ├── ControlCanWorker.cpp
│   │   ├── ControlCanWorker.h
│   │   ├── ModbusRtuClient.cpp
│   │   └── ModbusRtuClient.h
│   └── protocol/
│       ├── BmsCanParser.cpp
│       └── BmsCanParser.h
└── vendor/
    └── windows/
        ├── ControlCAN.h
        └── ControlCAN.dll
```

---

## 四、平台使用策略

### 1. macOS

macOS 可以编译和使用：

- 当前仪表盘界面；
- BMS CAN 协议解析器；
- Modbus RTU 串口通信；
- Mock 模拟数据。

macOS 不能直接加载 `ControlCAN.dll`，因此真实 CAN 通信暂时保持关闭或 Mock 模式。

### 2. Windows x64

Windows x64 可以启用：

- `ControlCAN.dll`；
- USB-CAN 设备；
- 双通道 CAN 接收；
- BMS 报文解析；
- 当前仪表盘实时显示。

需要使用与 DLL 架构一致的 **Qt MSVC 64 位套件**。
Windows 用户需自行将 ControlCAN.dll 放入
app/bms_dcdc_monitor/vendor/windows/

### 3. Linux 或嵌入式平台

Linux/嵌入式平台后续应增加：

- SocketCAN 后端；
- 或目标硬件厂商提供的 CAN 驱动后端。

协议解析层 `BmsCanParser` 不依赖具体操作系统，可以继续复用。

---

## 五、复制步骤

将以下目录复制到：

```text
app/bms_dcdc_monitor
```

需要复制：

```text
src/communication
src/protocol
vendor/windows
```

然后使用：

```text
patches/DataModel.updated.h
```

更新项目中的：

```text
src/DataModel.h
```

再参考：

```text
patches/CMakeLists.recommended.txt
```

修改现有 `CMakeLists.txt`。

最后按照：

```text
patches/MainWindowIntegration_中文版.md
```

将通信状态、CAN 数据和 Modbus 数据接入现有主界面。

---

## 六、macOS 编译方法

```bash
cmake -S app/bms_dcdc_monitor \
      -B build/bms_dcdc_monitor \
      -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.0/macos" \
      -DENABLE_CONTROLCAN=OFF

cmake --build build/bms_dcdc_monitor
```

macOS 构建时：

- 不编译 Windows CAN 驱动；
- 不复制 `ControlCAN.dll`；
- 仍然编译 Modbus RTU 和 BMS 协议解析模块。

---

## 七、Windows 编译方法

```powershell
cmake -S app/bms_dcdc_monitor `
      -B build/bms_dcdc_monitor `
      -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/msvc2022_64" `
      -DENABLE_CONTROLCAN=ON

cmake --build build/bms_dcdc_monitor --config Release
```

构建完成后，CMake 会将：

```text
vendor/windows/ControlCAN.dll
```

自动复制到程序可执行文件所在目录。

---

## 八、当前支持的 BMS CAN 报文

迁移后的 `BmsCanParser` 支持以下报文组：

| CAN ID 范围 | 解析内容 |
|---|---|
| `0x18E1xxxx` | 电池包总压、总流、SOC、SOH |
| `0x18E2xxxx` | 充电限流、放电限流、充电限压、放电限压 |
| `0x18E3xxxx` | 充电可用能量、放电可用能量、状态字、SOP |
| `0x18E4xxxx` | 最高/最低单体电压、最高/最低温度 |

目前按旧代码中的数据比例进行解析：

- 总压：原始值 × 0.1 V；
- 总流：有符号原始值 × 0.1 A；
- SOC/SOH：原始值 × 0.1%；
- 单体电压：原始值 × 0.001 V；
- 温度：有符号原始值 × 0.1 ℃。

正式接入设备前，仍应使用 BMS 通信协议文件复核字节序、比例系数和 CAN ID。

---

## 九、Modbus 当前状态

原始 Modbus 代码只包含一个温度数据解析示例，没有提供完整的 DCDC 寄存器表。

当前缺少：

- 寄存器地址；
- 数据长度；
- 缩放系数；
- 有符号或无符号定义；
- 大端、小端或字交换规则；
- 不同寄存器对应的 DCDC 参数。

因此迁移包中的 `ModbusRtuClient` 只负责：

1. 打开和关闭串口；
2. 发送功能码 `0x03` 的读保持寄存器请求；
3. 计算和校验 Modbus CRC16；
4. 处理变长响应帧；
5. 返回原始寄存器数组；
6. 报告异常响应和通信错误。

在获得 DCDC Modbus 协议或寄存器表之后，再增加：

```text
src/protocol/DcdcModbusParser.h
src/protocol/DcdcModbusParser.cpp
```

不得在没有协议依据时猜测 DCDC 数据映射。

---

## 十、迁移版修复的问题

本迁移包修复了旧代码中的以下问题：

1. 删除 CAN 协议解析器对 `windows.h` 的依赖；
2. 使用值类型 `CanFrame` 代替跨线程裸指针；
3. 避免每次循环申请两个 2500 帧 CAN 缓冲区；
4. 检查 `ControlCAN.dll` 中所有必要函数是否存在；
5. 正确复位 CAN 通道 0 和通道 1；
6. 阻止 macOS 调用 Windows DLL；
7. 避免在错误线程访问 `QSerialPort`；
8. 删除重复创建的 `QSerialPort` 对象；
9. 增加 Modbus CRC 校验；
10. 支持 Modbus 变长响应帧；
11. 将通信层、协议层和界面层解耦；
12. 保留现有 14:9 数据监控仪表盘。

---

## 十一、推荐的数据流结构

```text
ControlCAN.dll / USB-CAN / RS485 串口
                    ↓
             通信驱动层
                    ↓
        CanFrame / Modbus 寄存器
                    ↓
   BmsCanParser / DcdcModbusParser
                    ↓
              DashboardData
                    ↓
          当前 14:9 仪表盘界面
```

这种结构便于后续增加：

- Windows ControlCAN；
- Linux SocketCAN；
- 嵌入式 CAN 驱动；
- 不同型号 DCDC 的 Modbus 协议；
- CSV 数据导出；
- 数据库存储；
- 远程通信和控制。
