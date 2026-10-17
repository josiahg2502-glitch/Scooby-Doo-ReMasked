# Boots straight into each level through a temporary engine.ini mod (StartingLevelPath/StartingLevelToLoad), runs it
# for -Seconds, saves a window capture and a one-line verdict per level. The mod is removed at the end (also on error).
# Usage: level-sweep.ps1 [-Seconds 40] [-Levels W1L0,W1L3]   -> autotest\level-sweep-<time>\REPORT.txt + <level>.png
param([int]$Seconds = 40, [string[]]$Levels = @())
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'Play\Scooby-Doo Re-Masked.exe'
$files = Join-Path $root 'Disc\DATA\files'
$mod = Join-Path $root 'Play\UserData\mods\zz-level-sweep'
$out = Join-Path $root ("autotest\level-sweep-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
if ($Levels.Count -eq 0) { $Levels = Get-ChildItem (Join-Path $files 'level') -Directory | ? Name -ne 'MAINMENU' | % Name }
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class SweepWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  public static IntPtr FindFor(uint id) {
    IntPtr best = IntPtr.Zero; long bestArea = 0;
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); RECT r;
      if (p == id && IsWindowVisible(h) && GetClientRect(h, out r) && (long)r.R * r.B > bestArea) { bestArea = (long)r.R * r.B; best = h; }
      return true; }, IntPtr.Zero);
    return best;
  }
}
"@
[SweepWin]::SetProcessDPIAware() | Out-Null
$engine = Get-Content (Join-Path $files 'engine.ini')
$report = @()
try {
    foreach ($level in $Levels) {
        $ghr = Get-ChildItem (Join-Path $files "level\$level") -Recurse -Filter '*.ghr' | Select -First 1
        if (-not $ghr) { $report += "{0,-9} SKIP no .ghr" -f $level; continue }
        New-Item -ItemType Directory -Force (Join-Path $mod 'files') | Out-Null
        $ini = $engine | % { $_ -replace '^StartingLevelPath = .*', "StartingLevelPath = $level" -replace '^StartingLevelToLoad = .*', "StartingLevelToLoad = $($ghr.Name.ToUpper())" }
        Set-Content (Join-Path $mod 'files\engine.ini') $ini -Encoding ascii
        $err = Join-Path $out "$level.err.log"
        $p = Start-Process $exe -WorkingDirectory (Split-Path $exe) -PassThru -RedirectStandardOutput (Join-Path $out "$level.out.log") -RedirectStandardError $err
        $null = $p.WaitForExit($Seconds * 1000)
        $png = ''
        if (-not $p.HasExited) {
            $h = [SweepWin]::FindFor([uint32]$p.Id); $r = New-Object SweepWin+RECT
            if ($h -ne [IntPtr]::Zero -and [SweepWin]::GetClientRect($h, [ref]$r) -and $r.R -gt 1) {
                $b = New-Object System.Drawing.Bitmap $r.R, $r.B; $g = [System.Drawing.Graphics]::FromImage($b); $dc = $g.GetHdc()
                [SweepWin]::PrintWindow($h, $dc, 3) | Out-Null; $g.ReleaseHdc($dc); $g.Dispose()
                $s = New-Object System.Drawing.Bitmap 640, ([int](640 * $b.Height / $b.Width)); $g = [System.Drawing.Graphics]::FromImage($s)
                $g.DrawImage($b, 0, 0, $s.Width, $s.Height); $g.Dispose(); $b.Dispose()
                $png = Join-Path $out "$level.png"; $s.Save($png); $s.Dispose()
            }
            $p.Kill(); $state = 'RUNNING'
        } else { $state = "EXITED($($p.ExitCode))" }
        Start-Sleep 1
        $e = Get-Content $err -ErrorAction SilentlyContinue
        $fatal = ($e | Select-String 'not translated|not a registered|Access Violation|AccessViolation|FATAL|panic|Unhandled|\[logic\] UNKNOWN|ERROR: DVD' | Select -First 1).Line
        $fps = ($e | Select-String '\[vi\] fps' | Select -Last 1).Line -replace '.*fps ([0-9.]+).*', '$1'
        $last = (($e | Select-String '\[dvdread\]' | Select -Last 1).Line -replace '.*files\\', '') -replace ' len.*', ''
        $logic = ($e | Select-String '\[logic\] module' | Measure).Count
        $report += "{0,-9} {1,-11} fps {2,-5} logic {3,-3} last {4} {5}" -f $level, $state, $fps, $logic, $last, $(if ($fatal) { "| " + $fatal.Substring(0, [Math]::Min(160, $fatal.Length)) } else { '' })
        $report[-1]
    }
} finally {
    Remove-Item -Recurse -Force $mod -ErrorAction SilentlyContinue
    $report | Set-Content (Join-Path $out 'REPORT.txt')
}
"report: $out\REPORT.txt"
