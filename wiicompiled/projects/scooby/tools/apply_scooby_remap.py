"""Retarget the runtime's HLE hooks from Super Paper Mario (R8PE01) to Scooby-Doo! Unmasked (G5DE78), by symbol name.
Run once on a clean copy of the SPM runtime. Names come from the release ELF on the disc (files/engine_ret.elf),
whose sections are byte-identical to main.dol. Hooks whose function is not in Scooby are wrapped in #if 0.
Usage: apply_scooby_remap.py <repo root>   -> rewrites runtime/src and runtime/include, writes projects/scooby/remap_actions.csv"""
import csv, re, sys
from collections import defaultdict
from pathlib import Path
root = Path(sys.argv[1]); rt = root / "runtime"

# SPM address -> name: the symbol each hook was retargeted for, then SPM's function map.
spm = {}
for l in (root / "projects/spm/MAP.txt").read_text().splitlines():
    a = l.split(None, 1)
    if len(a) == 2 and not re.match(r"(fn|lbl|zz)_", a[1]): spm[int(a[0], 16)] = a[1].strip()
for r in csv.DictReader(open(root / "projects/spm/remap_actions.csv")):
    if r["spm_addr"] and r["symbol"]: spm[int(r["spm_addr"], 16)] = r["symbol"]

scooby = defaultdict(set)
for l in (root / "projects/scooby/MAP.txt").read_text().splitlines():
    a, n = l.split(None, 1); scooby[n.strip()].add(int(a, 16))

HOOK = re.compile(r"\b(PPC_NATIVE_OVERRIDE_VOID|PPC_NATIVE_OVERRIDE|REGISTER_NATIVE_FUNCTION_AS|REGISTER_NATIVE_FUNCTION|"
                  r"REGISTER_TRANSLATED_FUNCTION|MKW_KNOWN_TYPED_NATIVE|MKW_KNOWN_NATIVE_CPU_CALL)\s*\(\s*((?:0x)?)([0-9A-Fa-f]{8})(u?)")

def statement_end(lines, start, col):
    depth, started = 0, False
    for i in range(start, len(lines)):
        for ch in (lines[i][col:] if i == start else lines[i]):
            if ch == "(": depth += 1; started = True
            elif ch == ")":
                depth -= 1
                if started and depth == 0: return i
    raise ValueError(f"unterminated macro at line {start + 1}")

actions, stats = [], defaultdict(int)
for path in sorted(list((rt / "src").rglob("*")) + list((rt / "include").rglob("*"))):
    if path.suffix not in (".cpp", ".h", ".hpp", ".inc") or "gx_fatal_stubs" in path.name or "third_party" in path.parts: continue
    lines = path.read_text(encoding="utf-8", errors="surrogateescape").splitlines(keepends=True)
    disable, dirty, dead = [], False, 0
    for i, line in enumerate(lines):
        s = line.strip()
        # hooks already inside #if 0 stay as they are
        if dead:
            if s.startswith("#if"): dead += 1
            elif s.startswith("#endif"): dead -= 1
            continue
        if re.match(r"#if\s+0\b", s): dead = 1; continue
        for m in HOOK.finditer(line):
            old = int(m.group(3), 16); name = spm.get(old)
            cands = sorted(scooby.get(name, ())) if name else []
            rel = str(path.relative_to(rt)).replace("\\", "/")
            if len(cands) == 1:
                new = f"{cands[0]:08X}"
                if m.group(3).lower() == m.group(3): new = new.lower()
                lines[i] = lines[i][:m.start(3)] + new + lines[i][m.end(3):]; dirty = True
                stats["rewritten"] += 1; actions.append((rel, i + 1, f"{old:08X}", name, new.upper(), "rewritten"))
            else:
                why = "name not in Scooby" if name and not cands else "ambiguous in Scooby" if cands else "no SPM symbol at this address"
                disable.append((i, statement_end(lines, i, m.start()), f"{name or hex(old)}: {why}"))
                stats["disabled (" + why + ")"] += 1; actions.append((rel, i + 1, f"{old:08X}", name or "", "", "disabled: " + why))
            break
    for start, end, why in sorted(set(disable), reverse=True):
        indent = re.match(r"\s*", lines[start]).group(0)
        lines.insert(end + 1, f"{indent}#endif\n"); lines.insert(start, f"{indent}#if 0  // SCOOBY: {why}\n"); dirty = True
    if dirty: path.write_text("".join(lines), encoding="utf-8", errors="surrogateescape")
with open(root / "projects/scooby/remap_actions.csv", "w", newline="") as fh:
    w = csv.writer(fh); w.writerow(["file", "line", "spm_addr", "symbol", "scooby_addr", "action"]); w.writerows(actions)
print(dict(stats))
