# Starts Play\Scooby-Doo Re-Masked.exe, waits, saves a PNG of the game window only (PrintWindow, no desktop input), stops it.
# Usage: capture-window.ps1 [-Seconds 20] [-Out <png>]
param([int]$Seconds = 20, [string]$Out = '')
$root = Split-Path -Parent $PSScriptRoot
if (-not $Out) { $Out = Join-Path $root 'autotest\window.png' }
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class Win {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  // The biggest visible top-level window of a process (MainWindowHandle can be 0 for large windows).
  public static IntPtr FindFor(uint id) {
    IntPtr best = IntPtr.Zero; long bestArea = 0;
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); RECT r;
      if (p == id && IsWindowVisible(h) && GetClientRect(h, out r) && (long)r.R * r.B > bestArea) { bestArea = (long)r.R * r.B; best = h; }
      return true; }, IntPtr.Zero);
    return best;
  }
}
"@
[Win]::SetProcessDPIAware() | Out-Null
$p = Start-Process (Join-Path $root 'Play\Scooby-Doo Re-Masked.exe') -WorkingDirectory (Join-Path $root 'Play') -PassThru `
    -RedirectStandardOutput (Join-Path $root 'autotest\capture.log') -RedirectStandardError (Join-Path $root 'autotest\capture.err.log')
Start-Sleep -Seconds $Seconds
$p.Refresh()
$h = [Win]::FindFor([uint32]$p.Id)
$r = New-Object Win+RECT
[Win]::GetClientRect($h, [ref]$r) | Out-Null
$bmp = New-Object System.Drawing.Bitmap ([Math]::Max(1, $r.R)), ([Math]::Max(1, $r.B))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
[Win]::PrintWindow($h, $dc, 3) | Out-Null   # PW_CLIENTONLY | PW_RENDERFULLCONTENT
$g.ReleaseHdc($dc); $g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
if (-not $p.HasExited) { $p.Kill() }
"saved $Out ($($r.R)x$($r.B)), window title [$($p.MainWindowTitle)]"
