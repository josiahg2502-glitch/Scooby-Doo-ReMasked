# Saves a PNG of one process's game window (PrintWindow, no desktop input). Used by level-sweep.py.
#   shot-pid.ps1 -ProcessId <pid> -Out <png>
param([int]$ProcessId, [string]$Out)
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
$h = [Win]::FindFor([uint32]$ProcessId)
if ($h -eq [IntPtr]::Zero) { 'no window'; exit 1 }
$r = New-Object Win+RECT
[Win]::GetClientRect($h, [ref]$r) | Out-Null
$bmp = New-Object System.Drawing.Bitmap ([Math]::Max(1, $r.R)), ([Math]::Max(1, $r.B))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
[Win]::PrintWindow($h, $dc, 3) | Out-Null
$g.ReleaseHdc($dc); $g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
