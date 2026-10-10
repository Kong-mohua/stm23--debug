"""Read the 5-way digital tracking state from the running MCU via SWD.

Usage:  python read_track.py [samples]     (default 1 sample, 0.5 s apart)

Symbol addresses are parsed from the Keil map file, so it keeps working
after every rebuild.  Channel order: L2 L1 M R1 R2 (bit0..bit4).  "raw" is
the level pattern straight off the pins; "line" is the same after
g_track_pol (bit set = "on the line").
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
    raise SystemExit("symbol not found in map: " + name)


BITS_A = sym_addr("g_track_bits")
MASK_A = sym_addr("g_track_mask")
POS_A = sym_addr("g_track_pos")
POL_A = sym_addr("g_track_pol")


def read_mem(addr, nbytes):
    out = subprocess.run(
        [CLI, "-c", "port=SWD", "mode=hotplug", "-r32", hex(addr), str(nbytes)],
        capture_output=True).stdout.decode("utf-8", "ignore")
    data = bytearray()
    for line in out.splitlines():
        m = re.match(r"\s*0x[0-9A-Fa-f]{8}\s*:\s*(.*)$", line)
        if m:
            for tok in m.group(1).split():
                if re.fullmatch(r"[0-9A-Fa-f]{8}", tok):
                    data += int(tok, 16).to_bytes(4, "little")
    if len(data) < nbytes:
        raise SystemExit("short read: %d/%d bytes" % (len(data), nbytes))
    return bytes(data[:nbytes])


def u16(buf):
    return buf[0] | (buf[1] << 8)


def bits_str(v, n):
    return "".join("1" if v & (1 << i) else "0" for i in range(n))


def sample():
    bits = read_mem(BITS_A, 1)[0] & 0x1F
    mask = read_mem(MASK_A, 1)[0] & 0x1F
    pos = u16(read_mem(POS_A, 2))
    pol = read_mem(POL_A, 1)[0]
    return bits, mask, pos, pol


n = int(sys.argv[1]) if len(sys.argv) > 1 else 1
for k in range(n):
    bits, mask, pos, pol = sample()
    shown = pos if pos != 999 else -999
    print("raw  (L2 L1 M R1 R2) = %s" % bits_str(bits, 5))
    print("line (L2 L1 M R1 R2) = %s   pos=%d  pol=%d" % (bits_str(mask, 5), shown, pol))
    if k != n - 1:
        print("-")
        time.sleep(0.5)
