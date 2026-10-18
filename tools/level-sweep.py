"""Boot every level of Scooby-Doo! Unmasked straight in (engine.ini StartingLevelPath/StartingLevelToLoad, through a mod),
run it, then save a window screenshot and the log, and report faults. Several game windows run at once.

    python tools/level-sweep.py [--levels W1L0,W3L5] [--seconds 35] [--workers 7]

Output: autotest/level-sweep-<time>/ with REPORT.md, results.json, shots/<level>.png and logs/<level>.err.log.
Each worker runs its own copy of Play/ (so the per-level mod and the settings don't collide), windowed 1280x720.
"""
import argparse, json, os, re, shutil, subprocess, sys, threading, time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLAY, DISC = ROOT / "Play", ROOT / "Disc" / "DATA" / "files"
EXE = "Scooby-Doo Re-Masked.exe"


def all_levels():
    return sorted(d.name for d in (DISC / "level").iterdir() if d.is_dir() and d.name != "MAINMENU" and any(d.rglob("*.ghr")))


def make_worker(base: Path, n: int) -> Path:
    w = base / f"w{n}"
    (w / "UserData" / "mods" / "zz-sweep" / "files").mkdir(parents=True, exist_ok=True)
    for f in PLAY.iterdir():
        if f.is_file() and (f.suffix.lower() in (".exe", ".dll", ".bin") or f.name == "portable.txt"):
            shutil.copy2(f, w / f.name)
    dvd = str(ROOT / "Disc" / "DATA").replace("\\", "\\\\")
    (w / "UserData" / "Config.toml").write_text(
        f"[paths]\ndvd_root = '{dvd}'\n\n[video]\ndisplay_mode = \"windowed\"\nwindow_width = 1280\nwindow_height = 720\n"
        "resolution_multiplier = 1.5\naspect_mode = 2\nskip_intro = true\n", encoding="utf-8")
    return w


def blank_shot(png: Path) -> bool:
    """No picture: the capture is missing or almost entirely black."""
    if not png.exists():
        return True
    from PIL import Image, ImageStat
    return max(ImageStat.Stat(Image.open(png).convert("L")).extrema[0]) < 16


def run_level(w: Path, level: str, seconds: float, out: Path) -> dict:
    ghr = next((DISC / "level" / level).rglob("*.ghr")).name.upper()
    ini = (DISC / "engine.ini").read_text()
    ini = re.sub(r"(?m)^StartingLevelPath = .*$", f"StartingLevelPath = {level}", ini)
    ini = re.sub(r"(?m)^StartingLevelToLoad = .*$", f"StartingLevelToLoad = {ghr}", ini)
    (w / "UserData" / "mods" / "zz-sweep" / "files" / "engine.ini").write_text(ini)
    shutil.rmtree(w / "UserData" / "Saves", ignore_errors=True)
    env = dict(os.environ, NSMBW_LOG_FPS="1", RUNTIME_NO_POPUP="1")  # crashes are logged, no popup window
    log = out / "logs" / f"{level}.err.log"
    with open(log, "w", encoding="utf-8", errors="replace") as err:
        p = subprocess.Popen([str(w / EXE)], cwd=w, env=env, stdout=subprocess.DEVNULL, stderr=err)
        start = time.time()
        while time.time() - start < seconds and p.poll() is None:
            time.sleep(0.5)
        alive = p.poll() is None
        if alive:
            subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tools" / "shot-pid.ps1"),
                            "-ProcessId", str(p.pid), "-Out", str(out / "shots" / f"{level}.png")], capture_output=True)
            p.kill()
        p.wait()
    text = log.read_text(encoding="utf-8", errors="replace")
    fps = [float(m) for m in re.findall(r"\[vi\] fps ([\d.]+)", text)]
    return {
        "level": level,
        "alive": alive,
        "exit": None if alive else p.returncode,
        "faults": text.count("AccessViolation"),
        "panic": bool(re.search(r"OS panic|missing_target|Unhandled|FATAL|not translated|not a registered|\[logic\] UNKNOWN|ERROR: DVD", text)),
        # judged from the shot: the runtime's "nothing visible yet" dump fires at 30 s regardless
        "watchdog": blank_shot(out / "shots" / f"{level}.png"),
        "fps_min": min(fps) if fps else 0.0,
        "fps_last": fps[-1] if fps else 0.0,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--levels", default="")
    ap.add_argument("--seconds", type=float, default=30)
    ap.add_argument("--workers", type=int, default=7)
    a = ap.parse_args()
    levels = a.levels.split(",") if a.levels else all_levels()
    out = ROOT / "autotest" / time.strftime("level-sweep-%Y%m%d-%H%M%S")
    (out / "shots").mkdir(parents=True)
    (out / "logs").mkdir()
    workers = [make_worker(out / "work", i) for i in range(a.workers)]
    queue, results, lock = list(levels), [], threading.Lock()

    def worker(w):
        while True:
            with lock:
                if not queue:
                    return
                level = queue.pop(0)
            r = run_level(w, level, a.seconds, out)
            with lock:
                results.append(r)
                flag = "" if r["alive"] and not r["faults"] and not r["panic"] else "  <-- PROBLEM"
                print(f"{len(results):3}/{len(levels)} {level}: alive={r['alive']} faults={r['faults']} panic={r['panic']} "
                      f"fps={r['fps_last']}{flag}", flush=True)

    threads = [threading.Thread(target=worker, args=(w,)) for w in workers]
    for t in threads: t.start()
    for t in threads: t.join()
    results.sort(key=lambda r: r["level"])
    (out / "results.json").write_text(json.dumps(results, indent=1))
    bad = [r for r in results if not r["alive"] or r["faults"] or r["panic"] or r["watchdog"]]
    lines = [f"# Level sweep {out.name}", "", f"{len(results)} levels, {a.seconds:.0f} s each, {len(bad)} with problems.", "",
             "| Level | Alive | Faults | Panic | No picture | FPS |", "|---|---|---|---|---|---|"]
    lines += [f"| {r['level']} | {r['alive']} | {r['faults']} | {r['panic']} | {r['watchdog']} | {r['fps_last']} |" for r in results]
    (out / "REPORT.md").write_text("\n".join(lines) + "\n")
    shutil.rmtree(out / "work", ignore_errors=True)
    print(f"DONE: {out} ({len(bad)} with problems)")


if __name__ == "__main__":
    main()
