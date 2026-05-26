# 数据点表

## 1. BMS 数据点

| 分类 | 中文名称 | 英文字段名 | 来源 | 协议地址 | 单位 | 刷新频率 | 说明 |
|---|---|---|---|---|---|---|---|
| BMS | 电池包电压 | pack_voltage | CAN | 待定 | V | 1s | 电池包总压 |
| BMS | 电池包电流 | pack_current | CAN | 待定 | A | 1s | 正负表示充放电方向 |
| BMS | 剩余电量 SOC | soc | CAN | 待定 | % | 1s | State of Charge |
| BMS | 健康度 SOH | soh | CAN | 待定 | % | 5s | State of Health |
| BMS | 已充电量 | charged_capacity | CAN | 待定 | Ah/kWh | 5s | 累计充电量 |
| BMS | 已放电量 | discharged_capacity | CAN | 待定 | Ah/kWh | 5s | 累计放电量 |
| BMS | 单体最高电压 | max_cell_voltage | CAN | 待定 | V | 1s | 最高单体电压 |
| BMS | 单体最低电压 | min_cell_voltage | CAN | 待定 | V | 1s | 最低单体电压 |
| BMS | 单体最高温度 | max_cell_temperature | CAN | 待定 | ℃ | 1s | 最高温度 |
| BMS | 单体最低温度 | min_cell_temperature | CAN | 待定 | ℃ | 1s | 最低温度 |
| BMS | 充电限流 | charge_current_limit | CAN | 待定 | A | 1s | 允许最大充电电流 |
| BMS | 放电限流 | discharge_current_limit | CAN | 待定 | A | 1s | 允许最大放电电流 |

## 2. DCDC 数据点

| 分类 | 中文名称 | 英文字段名 | 来源 | 地址/编号 | 单位 | 刷新频率 | 说明 |
|---|---|---|---|---|---|---|---|
| DCDC | 高压侧电压 | hv_voltage | Modbus | F001 | V | 1s | 高压侧输入/输出电压 |
| DCDC | 低压侧电压 | lv_voltage | Modbus | F003 | V | 1s | 低压侧电压 |
| DCDC | 低压侧电流 | lv_current | Modbus | F005 | A | 1s | 低压侧电流 |
| DCDC | 板上温度 | board_temperature | Modbus | F019 | ℃ | 1s | DCDC 板载温度 |
| DCDC | 充电限流值 | dcdc_charge_current_limit | Modbus | F041 | A | 1s | DCDC 充电限流 |
| DCDC | 放电限流值 | dcdc_discharge_current_limit | Modbus | F043 | A | 1s | DCDC 放电限流 |
| DCDC | 充电开始电压 | charge_start_voltage | Modbus | u203 | V | 5s | 充电启动阈值 |
| DCDC | 充电截止电压 | charge_stop_voltage | Modbus | u204 | V | 5s | 充电停止阈值 |
| DCDC | 充电目标电压 | charge_target_voltage | Modbus | u205 | V | 5s | 充电目标电压 |
| DCDC | 放电开始电压 | discharge_start_voltage | Modbus | u206 | V | 5s | 放电启动阈值 |
| DCDC | 放电截止电压 | discharge_stop_voltage | Modbus | u207 | V | 5s | 放电停止阈值 |
| DCDC | 放电目标电压 | discharge_target_voltage | Modbus | u209 | V | 5s | 放电目标电压 |

## 3. 单体电压数组

| 字段名 | 类型 | 示例 | 说明 |
|---|---|---|---|
| cell_voltages | array | [3.321, 3.318, 3.326] | 单体电压数组 |

## 4. 温度数组

| 字段名 | 类型 | 示例 | 说明 |
|---|---|---|---|
| cell_temperatures | array | [28.5, 29.1, 30.2] | 单体温度数组 |
