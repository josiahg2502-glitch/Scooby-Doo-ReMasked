"""List guest-address literals (0x80xxxxxx) in live runtime code (outside #if 0 and hook macros), per file, with the
SPM symbol at that address when there is one. Usage: literal_inventory.py <repo root> -> projects/scooby/literals.csv"""
import csv, re, sys
from pathlib import Path
root = Path(sys.argv[1]); rt = root / "runtime"
spm = {}
for l in (root / "projects/spm/MAP.txt").read_text().splitlines():
    a = l.split(None, 1)
    if len(a) == 2: spm[int(a[0], 16)] = a[1].strip()
HOOK = re.compile(r"\b(PPC_NATIVE_OVERRIDE\w*|REGISTER_\w+|MKW_KNOWN_\w+)\s*\(")
LIT = re.compile(r"0x(8[01][0-9A-Fa-f]{6})")
rows = []
for path in sorted(list((rt / "src").rglob("*")) + list((rt / "include").rglob("*"))):
    if path.suffix not in (".cpp", ".h", ".hpp", ".inc") or "third_party" in path.parts or "gx_fatal_stubs" in path.name: continue
    dead = 0
    for i, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines()):
        s = line.strip()
        if dead:
            if s.startswith("#if"): dead += 1
            elif s.startswith("#endif"): dead -= 1
            continue
        if re.match(r"#if\s+0\b", s): dead = 1; continue
        if HOOK.search(line) or s.startswith("//"): continue
        for m in LIT.finditer(line.split("//")[0]):
            a = int(m.group(1), 16)
            if a < 0x80003100: continue  # low-memory OS globals are the same on GameCube and Wii
            rows.append([str(path.relative_to(rt)).replace("\\", "/"), i + 1, f"{a:08X}", spm.get(a, ""), s[:140]])
with open(root / "projects/scooby/literals.csv", "w", newline="") as fh:
    w = csv.writer(fh); w.writerow(["file", "line", "addr", "spm_symbol", "code"]); w.writerows(rows)
from collections import Counter
for f, c in Counter(r[0] for r in rows).most_common(): print(f"{c:5} {f}")
print(len(rows), "literals")
