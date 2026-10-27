Get-Process UnrealEditor -ErrorAction SilentlyContinue | Select-Object Id,MainWindowTitle,MainWindowHandle | ConvertTo-Json
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$r=[System.Windows.Forms.SystemInformation]::VirtualScreen
$b=New-Object System.Drawing.Bitmap $r.Width,$r.Height
$g=[System.Drawing.Graphics]::FromImage($b)
$g.CopyFromScreen($r.Left,$r.Top,0,0,$b.Size)
$b.Save('C:\GitHub\VirtualGameTeamProj\Plugins\DEMPrototype\CommonValidation\desktop_probe.png')
$g.Dispose();$b.Dispose()
