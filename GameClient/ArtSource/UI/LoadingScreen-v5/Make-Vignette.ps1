# Writes source/ls_vignette.png: black with alpha = edge vignette + soft dark ellipse behind emblem/title + bottom band for tip/bar.
# 960x540, stretched to the screen by the loading widget. Re-run, then Import-LoadingScreen-v5.py.
Add-Type -AssemblyName System.Drawing
Add-Type @'
public static class LsVignette {
  static float S(float a, float b, float x) { float t = System.Math.Max(0f, System.Math.Min(1f, (x - a) / (b - a))); return t * t * (3f - 2f * t); }
  public static void Fill(byte[] px, int w, int h, int stride) {
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      float u = (x + .5f) / w, v = (y + .5f) / h;
      float dx = (u - .5f) / .5f, dy = (v - .5f) / .5f;
      float edge = .5f * S(.55f, 1.25f, (float)System.Math.Sqrt(dx * dx + dy * dy));
      float ex = (u - .5f) / .32f, ey = (v - .56f) / .32f;                          // behind emblem + title + rule
      float centre = .5f * (1f - S(.35f, 1f, (float)System.Math.Sqrt(ex * ex + ey * ey)));
      float band = .72f * S(.68f, .86f, v);                                          // tip, bar, spinner
      float a = 1f - (1f - edge) * (1f - centre) * (1f - band);
      int i = y * stride + x * 4; px[i] = 0; px[i + 1] = 0; px[i + 2] = 0; px[i + 3] = (byte)(a * 255f + .5f);
    }
  }
}
'@
$bmp = New-Object Drawing.Bitmap 960, 540, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
$d = $bmp.LockBits((New-Object Drawing.Rectangle 0, 0, 960, 540), 'ReadWrite', $bmp.PixelFormat)
$bytes = New-Object byte[] ($d.Stride * 540); [LsVignette]::Fill($bytes, 960, 540, $d.Stride)
[Runtime.InteropServices.Marshal]::Copy($bytes, 0, $d.Scan0, $bytes.Length); $bmp.UnlockBits($d)
$bmp.Save((Join-Path $PSScriptRoot 'source\ls_vignette.png'), [Drawing.Imaging.ImageFormat]::Png)
