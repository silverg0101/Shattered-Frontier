param([string]$Do,[string]$Out="",[int]$X=0,[int]$Y=0,[string]$Key="")
# Like drive.ps1, but the shot is the window's client area (the game viewport) and 'place' sizes the client to 1920x1080.
$h=@'
using System;using System.Runtime.InteropServices;using System.Drawing;using System.Drawing.Imaging;
public class W{
[DllImport("user32.dll")]public static extern bool SetProcessDPIAware();
[DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr h);
[DllImport("user32.dll")]public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int hh,bool r);
[DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr h,out RECT r);
[DllImport("user32.dll")]public static extern bool GetClientRect(IntPtr h,out RECT r);
[DllImport("user32.dll")]public static extern bool ClientToScreen(IntPtr h,ref POINT p);
[DllImport("user32.dll")]public static extern bool SetCursorPos(int x,int y);
[DllImport("user32.dll")]public static extern void keybd_event(byte vk,byte sc,int fl,int ex);
[DllImport("user32.dll")]public static extern void mouse_event(int fl,int dx,int dy,int d,int ex);
public struct RECT{public int L,T,R,B;}
public struct POINT{public int X,Y;}
public static string Client(IntPtr h){RECT c;GetClientRect(h,out c);POINT p;p.X=0;p.Y=0;ClientToScreen(h,ref p);return p.X+","+p.Y+","+c.R+","+c.B;}
public static void Shot(IntPtr h,string path){RECT c;GetClientRect(h,out c);POINT p;p.X=0;p.Y=0;ClientToScreen(h,ref p);using(var b=new Bitmap(c.R,c.B))using(var g=Graphics.FromImage(b)){g.CopyFromScreen(p.X,p.Y,0,0,new Size(c.R,c.B));b.Save(path,ImageFormat.Png);}}
}
'@
Add-Type -TypeDefinition $h -ReferencedAssemblies System.Drawing,System.Drawing.Primitives
[W]::SetProcessDPIAware()|Out-Null
$pr=Get-Process -Id ([int](Get-Content "$PSScriptRoot\client.pid")); $hw=$pr.MainWindowHandle
[W]::SetForegroundWindow($hw)|Out-Null
switch($Do){
 'place' { [W]::MoveWindow($hw,0,0,1960,1160,$true)|Out-Null; Start-Sleep 1; $c=([W]::Client($hw)).Split(','); $r=New-Object W+RECT; [W]::GetWindowRect($hw,[ref]$r)|Out-Null; $ow=$r.R-$r.L-([int]$c[2]-1920); $oh=$r.B-$r.T-([int]$c[3]-1080); [W]::MoveWindow($hw,0,0,$ow,$oh,$true)|Out-Null }
 'key'   { $vk=$(if($Key -match "^F[0-9]+$"){[byte](0x6F+[int]$Key.Substring(1))}else{[byte][char]$Key.ToUpper()}); [W]::keybd_event($vk,0,0,0); Start-Sleep -Milliseconds 80; [W]::keybd_event($vk,0,2,0) }
 'move'  { $c=([W]::Client($hw)).Split(','); [W]::SetCursorPos([int]$c[0]+$X,[int]$c[1]+$Y)|Out-Null }
 'click' { $c=([W]::Client($hw)).Split(','); [W]::SetCursorPos([int]$c[0]+$X,[int]$c[1]+$Y)|Out-Null; Start-Sleep -Milliseconds 100; [W]::mouse_event(2,0,0,0,0); Start-Sleep -Milliseconds 80; [W]::mouse_event(4,0,0,0,0) }
}
Start-Sleep -Milliseconds 800
if($Out){ [W]::Shot($hw,$Out) }
[W]::Client($hw)
