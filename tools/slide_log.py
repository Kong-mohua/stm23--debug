"""Fast 5-way sensor sampler: reads bits/mask/pos/pol in ONE SWD pass per
sample and prints a timestamped row.  Run for N seconds:
    python slide_log.py 30 > slide_log.txt
"""
import re
import subprocess
import sys
import time

MAP = r"C:\Users\Administrator\Desktop\diangdeng\MDK-ARM\diangdeng\diangdeng.map"
CLI = (r"C:\Users\Administrator\AppData\Local\stm32cube\bundles\programmer"
       r"\2.23.0\bin\STM32_Programmer_CLI.exe")


def sym_addr(name):
    pat = re.compile(r"^\s+%s\s+0x([0-9A-Fa-f]{8})\s" % re.escape(name))
    with open(MAP, "r", errors="ignore") as f:
        for ln in f:
            m = pat.match(ln)
            if m:
                return int(m.group(1), 16)
    raise SystemExit("symbol not found: " + name)


A = {n: sym_addr(n) for n in ("g_track_bits", "g_track_mask",
                              "g_track_pos", "g_track_pol")}
LO, HI = min(A.values()), max(A.values()) + 2
LEN = HI - LO


def read_range():
    out = subprocess.run(
        [CLI, "-c", "port=SWD", "mode=hotplug", "-r32", hex(LO), str(LEN)],
        capture_output=True).stdout.decode("utf-8", "ignore")
    data = bytearray()
    for line in out.splitlines():
        m = re.match(r"\s*0x[0-9A-Fa-f]{8}\s*:\s*(.*)$", line)
        if m:
            for tok in m.group(1).split():
                if re.fullmatch(r"[0-9A-Fa-f]{8}", tok):
                    data += int(tok, 16).to_bytes(4, "little")
    return data if len(data) >= LEN else None


def at(data, name):
    off = A[name] - LO
    return data[off]


def u16(data, name):
    off = A[name] - LO
    return data[off] | (data[off + 1] << 8)


secs = int(sys.argv[1]) if len(sys.argv) > 1 else 30
t0 = time.time()
while time.time() - t0 < secs:
    d = read_range()
    if d is None:
        print("%5.1fs  READ-FAIL" % (time.time() - t0), flush=True)
        continue
    b = at(d, "g_track_bits") & 0x1F
    m = at(d, "g_track_mask") & 0x1F
    p = u16(d, "g_track_pos")
    p = p if p != 999 else -999
    s = "".join("1" if b & (1 << i) else "0" for i in range(5))
    sm = "".join("1" if m & (1 << i) else "0" for i in range(5))
    print("%5.1fs  raw=%s line=%s pos=%4d" % (time.time() - t0, s, sm, p),
          flush=True)
