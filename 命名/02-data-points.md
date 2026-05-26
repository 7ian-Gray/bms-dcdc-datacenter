# 数据点表

| 分类 | 参数名 | 英文字段名 | 来源 | 协议地址 | 单位 | 刷新频率 | 备注 |
|---|---|---|---|---|---|---|---|
| BMS | 电池包电压 | pack_voltage | CAN | 待定 | V | 1s | 总压 |
| BMS | 电池包电流 | pack_current | CAN | 待定 | A | 1s | 正负表示充放电 |
| BMS | SOC | soc | CAN | 待定 | % | 1s | 剩余电量 |
| BMS | SOH | soh | CAN | 待定 | % | 5s | 健康状态 |
| DCDC | 高压侧电压 | hv_voltage | Modbus | F001 | V | 1s | 高压侧 |
| DCDC | 低压侧电压 | lv_voltage | Modbus | F003 | V | 1s | 低压侧 |
| DCDC | 低压侧电流 | lv_current | Modbus | F005 | A | 1s | 低压输出电流 |
| DCDC | 板上温度 | board_temperature | Modbus | F019 | ℃ | 1s | DCDC 板温 |
