"""Raw ADC sweep with the CPU halted.

Stops the firmware from touching the mux, drives AD0/AD1/AD2 by hand for each
of the 8 mux channels, starts a conversion via ADC_CR2.SWSTART and reads DR.

  ch i  ->  AD0=PB10=bit0, AD1=PB11=bit1, AD2=PA6=bit2

At the end the MCU is reset so the firmware resumes normally.
"""
import re
import subprocess
import time

CLI = r"C:/Users/Administrator/AppData/Local/stm32cube/bundles/programmer/2.23.0/bin/STM32_Programmer_CLI.exe"

GPIOB_ODR = 0x40010C0C
GPIOA_ODR = 0x4001080C
ADC_CR2   = 0x40012408
ADC_SR    = 0x40012400
ADC_DR    = 0x4001244C


def cli(*args):
    p = subprocess.run([CLI, "-c", "port=SWD", *args],
                       capture_output=True, timeout=40)
    out = p.stdout
    for enc in ("utf-8", "gbk"):
        try:
            return out.decode(enc)
        except UnicodeDecodeError:
            continue
    return out.decode("utf-8", errors="replace")


def read32(addr):
    out = cli("-r32", hex(addr), "4")
    m = re.search(rf"0x{addr:08X}\s*:\s*([0-9A-Fa-f]{{8}})", out, re.IGNORECASE)
    if not m:
        print("READ FAIL", hex(addr), out[-300:])
        return None
    return int(m.group(1), 16)


def write32(addr, val):
    out = cli("-w32", hex(addr), hex(val))
    if "rror" in out or "FAIL" in out:
        print("WRITE?", hex(addr), out[-200:])


rb = read32(GPIOB_ODR)
ra = read32(GPIOA_ODR)
cr2 = read32(ADC_CR2)
sr = read32(ADC_SR)
print(f"GPIOB_ODR={rb:08X} GPIOA_ODR={ra:08X} ADC_CR2={cr2:08X} ADC_SR={sr:08X}")

vals = []
for i in range(8):
    pb = (rb & ~0xC00) | (0x400 if (i & 1) else 0) | (0x800 if (i & 2) else 0)
    pa = (ra & ~0x40) | (0x40 if (i & 4) else 0)
    write32(GPIOB_ODR, pb)
    write32(GPIOA_ODR, pa)
    write32(ADC_CR2, cr2 | 0x400000)      # SWSTART
    time.sleep(0.05)
    sr = read32(ADC_SR) or 0
    dr = read32(ADC_DR) or 0
    vals.append(dr & 0xFFFF)
    print(f"ch{i}: addr={i:03b} SR_EOC={(sr >> 1) & 1} DR={dr & 0xFFFF}")

print("raw:", " ".join(str(v) for v in vals))

cli("-rst")
print("MCU reset, firmware running again")
