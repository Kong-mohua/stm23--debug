# 诊断工具脚本（配合 ST-Link / SWD 与串口使用）

传感器、外设"读数不动"类故障的排查脚本。依赖 python3 + pyserial。

- `read_track.py` —— 解析 Keil 生成的 map 文件找到 `g_track_*` 符号地址，
  经 SWD 读出五路数字状态 raw / line / pos / pol（用法：`python read_track.py [采样次数]`）。
- `slide_log.py` —— SWD 高速实时采样（约 2~4 次/秒），校准电位器时当"示波器"用：
  `python slide_log.py 90 > slide_log.txt` 连续记录 90 秒。
- `addr_pins.py`（已退役）—— 八路模拟模块时代：采样多路开关地址脚（PB10/PB11/PA6），
  验证固件在轮询 8 个通道。
- `raw_adc_sweep.py`（已退役）—— 八路时代：停核后手动给 mux 设地址、触发 ADC 并读 DR。

注意：脚本内工程路径与 STM32_Programmer_CLI 路径按本机环境写死，换机器需修改。
