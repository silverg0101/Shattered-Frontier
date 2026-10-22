"""Edge sharpness (r4/r5 review metric): mean |dx| + |dy| of luminance (0-255) in a window. Run: py -3.12 sharp.py <image> x y w h"""
import sys
import numpy as np
from PIL import Image
x, y, w, h = map(int, sys.argv[2:6])
a = np.asarray(Image.open(sys.argv[1]).convert('RGB'), dtype=np.float64)[y:y + h, x:x + w] @ [0.2126, 0.7152, 0.0722]
print(f'SHARPNESS {sys.argv[1]} [{x},{y},{w},{h}] {np.abs(np.diff(a, axis=1)).mean() + np.abs(np.diff(a, axis=0)).mean():.2f}')
