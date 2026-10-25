# Sends left clicks to a running OpenRepublic window at window-relative pixel positions (as seen in a
# tools/capture_window.ps1 capture of the same size), then optionally captures the result.
# Usage: ./tools/click_window.ps1 -ProcessId <pid> -Points "1668,87;300,200" [-Output after.png] [-DelayMs 600]
# Moves the real mouse pointer: only run this when nobody is using the machine.
param(
	[Parameter(Mandatory = $true)][int]$ProcessId,
	[Parameter(Mandatory = $true)][string]$Points,
	[string]$Output = "",
	[int]$DelayMs = 600
)

Add-Type @'
using System;
using System.Runtime.InteropServices;
public class OrClick
{
	[StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
	[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rect);
	[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
	[DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
	[DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extra);
	[DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
	[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
}
'@

$handle = (Get-Process -Id $ProcessId).MainWindowHandle
# A background process may not take the foreground directly; tapping Alt first lifts that restriction.
[OrClick]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
[OrClick]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
[OrClick]::SetForegroundWindow($handle) | Out-Null
Start-Sleep -Milliseconds 500
if ([OrClick]::GetForegroundWindow() -ne $handle) { Write-Warning "OpenRepublic window is not in the foreground; clicks may go elsewhere." }
$rect = New-Object OrClick+Rect
[OrClick]::GetWindowRect($handle, [ref]$rect) | Out-Null
foreach ($point in $Points.Split(';')) {
	$xy = $point.Split(',')
	[OrClick]::SetCursorPos($rect.Left + [int]$xy[0], $rect.Top + [int]$xy[1]) | Out-Null
	Start-Sleep -Milliseconds 700
	[OrClick]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
	Start-Sleep -Milliseconds 250
	[OrClick]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
	Start-Sleep -Milliseconds $DelayMs
}
if ($Output -ne "") {
	& "$PSScriptRoot\capture_window.ps1" -ProcessId $ProcessId -Output $Output -Width ($rect.Right - $rect.Left) -Height ($rect.Bottom - $rect.Top)
}

