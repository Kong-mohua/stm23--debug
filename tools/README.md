# 诊断工具脚本（配合 ST-Link / SWD 与串口使用）

传感器、外设"读数不动"类故障的排查脚本。依赖 python3 + pyserial。

- `read_track.py` —— 解析 Keil 生成的 map 文件找到 `g_track_*` 符号地址，
  经 SWD 读出 8 路原始值 / mask / pos（用法：`python read_track.py [采样次数]`）。
- `addr_pins.py` —— 连续采样多路开关地址脚（PB10/PB11/PA6）的电平组合，
  验证固件确实在轮询 8 个通道。
- `raw_adc_sweep.py` —— 停核后手动给 mux 设地址、触发一次 ADC 转换并读 DR，
  逐通道取原始值（需 hotplug 连接；本机 normal 模式写入受限）。

注意：脚本内工程路径与 STM32_Programmer_CLI 路径按本机环境写死，换机器需修改。
