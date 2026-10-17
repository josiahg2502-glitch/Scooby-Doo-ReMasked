"""Retarget guest function-address literals (handlers, InvokeIndirectCpu targets) from SPM to Scooby by SPM symbol name.
Reads projects/scooby/data_actions.csv rows still marked "review"; a row whose SPM address is a named SPM function that
exists once in the Scooby MAP is rewritten. Usage: apply_func_literal_remap.py <repo root>"""
import csv, re, sys
from collections import defaultdict
from pathlib import Path
root = Path(sys.argv[1]); rt = root / "runtime"
spm = {}
for l in (root / "projects/spm/MAP.txt").read_text().splitlines():
    a = l.split(None, 1)
    if len(a) == 2 and not re.match(r"(fn|lbl|zz)_", a[1]): spm[int(a[0], 16)] = a[1].strip()
scooby = defaultdict(set)
for l in (root / "projects/scooby/MAP.txt").read_text().splitlines():
    a, n = l.split(None, 1); scooby[n.strip()].add(int(a, 16))
done, missing = [], []
for r in csv.DictReader(open(root / "projects/scooby/data_actions.csv")):
    if r["action"] != "review": continue
    name = spm.get(int(r["spm_addr"], 16))
    if not name: continue
    c = scooby.get(name, set())
    if len(c) != 1: missing.append(f'{r["file"]}:{r["line"]} {name}'); continue
    path = rt / r["file"]; lines = path.read_text(encoding="utf-8", errors="surrogateescape").splitlines(keepends=True)
    i = int(r["line"]) - 1; m = re.search(r"0x(" + r["spm_addr"] + r")", lines[i], re.I)
    if not m: continue
    new = f"{next(iter(c)):08X}"; new = new if m.group(1).upper() == m.group(1) else new.lower()
    lines[i] = lines[i][:m.start(1)] + new + lines[i][m.end(1):]
    path.write_text("".join(lines), encoding="utf-8", errors="surrogateescape")
    done.append(f'{r["file"]}:{r["line"]} {name} -> {new}')
print("rewritten:", *done, sep="\n  "); print("not in Scooby:", *missing, sep="\n  ")
