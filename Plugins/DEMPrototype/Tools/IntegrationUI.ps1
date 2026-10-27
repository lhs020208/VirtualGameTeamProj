param([ValidateSet('Focus','Play','Stop','View','FocusView','FocusStop','FocusPlay','FocusDrag','FocusUndo')][string]$Action='Focus')
$ErrorActionPreference='Stop'
$p=Get-Process UnrealEditor -ErrorAction Stop | Select-Object -First 1
$ws=New-Object -ComObject WScript.Shell
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class DEMWindow {
[DllImport("user32.dll")] public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
[DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
[DllImport("user32.dll")] public static extern void mouse_event(uint f,uint x,uint y,uint d,UIntPtr e);
[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
public delegate bool Callback(IntPtr h,IntPtr l);
[DllImport("user32.dll")] public static extern bool EnumWindows(Callback c,IntPtr l);
[DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h,System.Text.StringBuilder s,int n);
[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint id);
public static IntPtr Editor(uint id) {
IntPtr result=IntPtr.Zero;
EnumWindows((h,l)=>{uint pid;GetWindowThreadProcessId(h,out pid);var t=new System.Text.StringBuilder(1024);GetWindowText(h,t,1024);
if(pid==id && t.ToString().Contains("VirtualGameTeamProj") && !t.ToString().Contains("UnrealEditor.exe"))result=h;return true;},IntPtr.Zero);return result;
}
[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int c);
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
}
'@
[DEMWindow]::SetProcessDPIAware() | Out-Null
$p.Refresh()
$h=[DEMWindow]::Editor($p.Id)
if($h -eq 0){throw 'Unreal window not ready'}
function Assert-EditorFocus {
 if([DEMWindow]::GetForegroundWindow() -ne $h){throw 'USER_INTERFERENCE_OR_FOCUS_CHANGE: Input cancelled; ask user before resuming.'}
}
if($Action.StartsWith('Focus')) {
 [DEMWindow]::ShowWindow($h,3) | Out-Null
 [DEMWindow]::keybd_event(18,0,0,[UIntPtr]::Zero)
 [DEMWindow]::SetForegroundWindow($h) | Out-Null
 [DEMWindow]::keybd_event(18,0,2,[UIntPtr]::Zero)
 Start-Sleep -Milliseconds 300
}
Assert-EditorFocus
Start-Sleep -Milliseconds 500
if($Action -eq 'View' -or $Action -eq 'FocusView'){
[DEMWindow]::SetCursorPos(800,1500)|Out-Null
[DEMWindow]::mouse_event(2,0,0,0,[UIntPtr]::Zero);[DEMWindow]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
Assert-EditorFocus
Add-Type -AssemblyName System.Windows.Forms
[System.Windows.Forms.SendKeys]::SendWait('^a')
Start-Sleep -Milliseconds 300
$command='py "C:/GitHub/VirtualGameTeamProj/Plugins/DEMPrototype/Tools/Viewport.py"'
Add-Type -AssemblyName System.Windows.Forms
$previousClipboard=New-Object System.Windows.Forms.DataObject
$original=[System.Windows.Forms.Clipboard]::GetDataObject()
if($original){foreach($format in $original.GetFormats($false)) {try {$previousClipboard.SetData($format,$false,$original.GetData($format,$false))}catch{}}}
try {
 [System.Windows.Forms.Clipboard]::SetText($command)
 Assert-EditorFocus
 [System.Windows.Forms.SendKeys]::SendWait('^v')
 Start-Sleep -Milliseconds 1000
 Assert-EditorFocus
 [System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
 Start-Sleep -Seconds 5
} finally {
 if([System.Windows.Forms.Clipboard]::ContainsText() -and [System.Windows.Forms.Clipboard]::GetText() -eq $command) {
  if($previousClipboard){[System.Windows.Forms.Clipboard]::SetDataObject($previousClipboard,$true,10,100)}else{[System.Windows.Forms.Clipboard]::Clear()}
 }
}
}
if($Action -eq 'FocusDrag') {
 Assert-EditorFocus
 [DEMWindow]::SetCursorPos(468,1150)|Out-Null
 [DEMWindow]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
 try {for($i=1;$i -le 30;$i++){Assert-EditorFocus;[DEMWindow]::SetCursorPos([int](468+(1350-468)*$i/30),[int](1150+(550-1150)*$i/30))|Out-Null;Start-Sleep -Milliseconds 30}}
 finally {[DEMWindow]::mouse_event(4,0,0,0,[UIntPtr]::Zero)}
}
if($Action -eq 'FocusUndo') {
 Assert-EditorFocus
 $ws.SendKeys('{ESC}');Start-Sleep -Milliseconds 200
 $ws.SendKeys('^z')
}
if($Action -eq 'Play' -or $Action -eq 'FocusPlay'){[DEMWindow]::SetCursorPos(694,113)|Out-Null;[DEMWindow]::mouse_event(2,0,0,0,[UIntPtr]::Zero);[DEMWindow]::mouse_event(4,0,0,0,[UIntPtr]::Zero)}
if($Action -eq 'Stop' -or $Action -eq 'FocusStop'){Assert-EditorFocus;[DEMWindow]::SetCursorPos(778,113)|Out-Null;[DEMWindow]::mouse_event(2,0,0,0,[UIntPtr]::Zero);[DEMWindow]::mouse_event(4,0,0,0,[UIntPtr]::Zero)}
Start-Sleep -Seconds 2
& "$PSScriptRoot\DesktopProbe.ps1"
