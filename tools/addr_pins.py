"""Sample the mux address pins (PB10/PB11/PA6) read back as inputs while the
firmware scans: if the MCU is cycling the 8 channels, several combinations
must show up across a dozen quick reads."""
import re
import subprocess
import time

CLI = r"C:/Users/Administrator/AppData/Local/stm32cube/bundles/programmer/2.23.0/bin/STM32_Programmer_CLI.exe"


def cli(*a):
    p = subprocess.run([CLI, "-c", "port=SWD", "mode=hotplug", *a],
                       capture_output=True, timeout=40)
    for enc in ("utf-8", "gbk"):
        try:
            return p.stdout.decode(enc)
        except UnicodeDecodeError:
            pass
    return p.stdout.decode("utf-8", "replace")


def rd(a):
    o = cli("-r32", hex(a), "4")
    m = re.search(rf"0x{a:08X}\s*:\s*([0-9A-Fa-f]{{8}})", o, re.I)
    return int(m.group(1), 16) if m else None


print("sampling GPIOB_IDR (bit10/11) and GPIOA_IDR (bit6) ...")
bset = set()
aset = set()
for i in range(14):
    b = rd(0x40010C08)
    a = rd(0x40010808)
    if b is not None:
        bset.add(((b >> 10) & 1, (b >> 11) & 1))
    if a is not None:
        aset.add((a >> 6) & 1)
    time.sleep(0.05)

print("PB10/PB11 combos seen:", sorted(bset))
print("PA6 levels seen     :", sorted(aset))
print("=>", "firmware IS cycling the mux address" if len(bset) > 1 else "NO address cycling seen")
