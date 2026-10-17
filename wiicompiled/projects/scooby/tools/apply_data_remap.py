"""Retarget SDK global-variable literals from SPM to Scooby-Doo! Unmasked by the variable name in the line's comment.
A literal is rewritten only when the comment's first identifier is a unique data symbol in the Scooby ELF and the
comment does not describe a field offset (X.field, X+0xN). Everything else is listed for review, unchanged.
Usage: apply_data_remap.py <repo root> -> rewrites runtime files, writes projects/scooby/data_actions.csv"""
import csv, re, sys
from collections import defaultdict
from pathlib import Path
root = Path(sys.argv[1]); rt = root / "runtime"
syms = defaultdict(set)
for l in (root / "projects/scooby/elf_data_symbols.txt").read_text().splitlines():
    a, _sz, _b, n = l.split(None, 3); syms[n].add(int(a, 16))
rows = list(csv.DictReader(open(root / "projects/scooby/literals.csv")))
by_file = defaultdict(list)
for r in rows: by_file[r["file"]].append(r)
actions = []
for rel, items in by_file.items():
    path = rt / rel
    lines = path.read_text(encoding="utf-8", errors="surrogateescape").splitlines(keepends=True)
    for r in items:
        i = int(r["line"]) - 1; line = lines[i]
        comment = line.split("//", 1)[1] if "//" in line else ""
        m = re.match(r"\s*([A-Za-z_]\w*)", comment)
        name = m.group(1) if m else ""
        field = re.match(r"\s*[A-Za-z_]\w*\s*(\.|\+\s*0x|\[)", comment)
        cands = syms.get(name, set())
        if name and len(cands) == 1 and not field and len(re.findall(r"0x8[01][0-9A-Fa-f]{6}", line.split("//")[0])) == 1:
            new = next(iter(cands)); old = r["addr"]
            lit = re.search(r"0x(" + old + r")", line, re.I)
            if lit:
                txt = f"{new:08X}" if lit.group(1).isupper() or lit.group(1).isdigit() else f"{new:08x}"
                lines[i] = line[:lit.start(1)] + txt + line[lit.end(1):]
                actions.append((rel, r["line"], old, name, f"{new:08X}", "rewritten")); continue
        actions.append((rel, r["line"], r["addr"], name, "", "review"))
    new_text = "".join(lines)
    if new_text != path.read_text(encoding="utf-8", errors="surrogateescape"):
        path.write_text(new_text, encoding="utf-8", errors="surrogateescape")
with open(root / "projects/scooby/data_actions.csv", "w", newline="") as fh:
    w = csv.writer(fh); w.writerow(["file", "line", "spm_addr", "name", "scooby_addr", "action"]); w.writerows(actions)
print(sum(a[5] == "rewritten" for a in actions), "rewritten,", sum(a[5] == "review" for a in actions), "to review")
