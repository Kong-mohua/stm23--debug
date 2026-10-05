"""Read the 8-way tracking sensor state from the running MCU via SWD.

Usage:  python read_track.py [samples]     (default 1 sample, 0.5 s apart)

Symbol addresses are parsed from the Keil map file, so it keeps working
after every rebuild.  Channel order: ch0 = X1 (left) .. ch7 = X8 (right).
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


MASK_A = sym_addr("g_track_mask")
RAW_OFF = sym_addr("g_track_raw") - MASK_A
POL_A = sym_addr("g_track_pol")
POS_OFF = sym_addr("g_track_pos") - POL_A
THR_OFF = sym_addr("g_track_thr") - POL_A


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


def u16(buf, off):
    return buf[off] | (buf[off + 1] << 8)


def sample():
    wa = read_mem(MASK_A, RAW_OFF + 16)          # mask + 8 raw u16
    wb = read_mem(POL_A, THR_OFF + 2)            # pol, pos, thr
    mask = wa[0]
    raw = [u16(wa, RAW_OFF + i * 2) for i in range(8)]
    return (raw, mask, u16(wb, POS_OFF), u16(wb, THR_OFF), wb[0])


n = int(sys.argv[1]) if len(sys.argv) > 1 else 1
for k in range(n):
    raw, mask, pos, thr, pol = sample()
    bits = "".join("1" if mask & (1 << i) else "0" for i in range(8))
    print("ch0..ch7: " + " ".join("%4d" % v for v in raw))
    print("mask=%s (bit0=X1 .. bit7=X8)  pos=%d  thr=%d  pol=%d" %
          (bits, pos if pos != 999 else -999, thr, pol))
    if k != n - 1:
        print("-")
        time.sleep(0.5)
