"""Disassemble Scooby-Doo! Unmasked main.dol with ELF symbol and small-data annotations.
usage: dis.py <addr|symbol> [count]     e.g. dis.py CheckErrors__15HKAsyncIOMgr_SPFUlUl 80"""
import re, struct, sys
from pathlib import Path
import capstone

HERE = Path(__file__).resolve().parent.parent           # projects/scooby
_logic = HERE.parents[2] / "game" / "main_logic.dol"  # main.dol + logic overlay, when built
dol = (_logic if _logic.exists() else HERE.parents[2] / "game" / "main.dol").read_bytes()
R13, R2 = 0x8027E0C0, 0x80282020
u = lambda o: struct.unpack(">I", dol[o:o + 4])[0]
segs = [(u(0x48 + i * 4), u(i * 4), u(0x90 + i * 4)) for i in range(18) if u(0x90 + i * 4)]
def read(addr, n):
    for a, off, sz in segs:
        if a <= addr < a + sz: return dol[off + addr - a: off + addr - a + n]
    return b""
syms, names = {}, {}
for f in ("elf_symbols.txt", "elf_data_symbols.txt"):
    for l in (HERE / f).read_text().splitlines():
        p = l.split()
        a, n = int(p[0], 16), p[-1]
        syms.setdefault(a, n); names.setdefault(n, a)
order = sorted(syms)
def sym(a):
    import bisect
    i = bisect.bisect_right(order, a) - 1
    if i < 0: return f"{a:08X}"
    b = order[i]; return syms[b] + (f"+0x{a - b:X}" if a != b else "")
arg = sys.argv[1]
start = names[arg] if arg in names else int(arg, 16)
count = int(sys.argv[2]) if len(sys.argv) > 2 else 60
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
for ins in md.disasm(read(start, count * 4), start):
    note = ""
    m = re.search(r"r(13|2), (-?0x[0-9a-f]+|-?\d+)\)|(-?0x[0-9a-f]+|-?\d+)\(r(13|2)\)", ins.op_str)
    if m:
        reg = m.group(1) or m.group(4); off = int(m.group(2) or m.group(3), 0)
        note = "  ; " + sym((R13 if reg == "13" else R2) + off)
    t = re.search(r"0x(8[0-9a-f]{7})$", ins.op_str)
    if t and ins.mnemonic.startswith("b"): note = "  ; " + sym(int(t.group(1), 16))
    print(f"{ins.address:08X}  {ins.mnemonic:8} {ins.op_str}{note}")
