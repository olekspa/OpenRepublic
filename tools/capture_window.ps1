# Captures the window of a running OpenRepublic process to a PNG using PrintWindow.
# Works while the window is occluded and does not need computer-use screenshots.
# Usage: ./tools/capture_window.ps1 -ProcessId <pid> -Output <file.png> [-Width 1920 -Height 1080]
param(
	[Parameter(Mandatory = $true)][int]$ProcessId,
	[Parameter(Mandatory = $true)][string]$Output,
	[int]$Width = 1920,
	[int]$Height = 1080
)

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class OrWindowCapture
{
	[StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
	[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rect);
	[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window, IntPtr dc, uint flags);
	[DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr window, int x, int y, int w, int h, bool repaint);
}
'@

$handle = (Get-Process -Id $ProcessId).MainWindowHandle
if ($handle -eq [IntPtr]::Zero) { throw "Process $ProcessId has no main window." }
[OrWindowCapture]::MoveWindow($handle, 0, 0, $Width, $Height, $true) | Out-Null
Start-Sleep -Seconds 3
$rect = New-Object OrWindowCapture+Rect
[OrWindowCapture]::GetWindowRect($handle, [ref]$rect) | Out-Null
$w = $rect.Right - $rect.Left
$h = $rect.Bottom - $rect.Top
$bitmap = New-Object System.Drawing.Bitmap $w, $h
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$dc = $graphics.GetHdc()
[OrWindowCapture]::PrintWindow($handle, $dc, 2) | Out-Null
$graphics.ReleaseHdc($dc)
$bitmap.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()
Write-Output "$w x $h -> $Output"
