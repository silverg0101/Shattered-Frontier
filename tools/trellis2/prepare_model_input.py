"""Prepare one opaque studio reference for the Pixal runner using border-connected background removal."""
import argparse, hashlib, json
from pathlib import Path

import cv2
import numpy as np
from PIL import Image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--image', required=True, help='Original single-object reference')
    parser.add_argument('--output', required=True, help='New RGBA PNG path')
    parser.add_argument('--tolerance', type=int, default=12, help='Per-channel flood-fill edge tolerance (0-64)')
    parser.add_argument('--method', choices=['floodfill', 'grabcut'], default='floodfill', help='Optional GrabCut cleanup of connected studio shadows')
    args = parser.parse_args()
    source = Path(args.image).resolve()
    output = Path(args.output).resolve()
    if not source.is_file():
        raise FileNotFoundError(source)
    if output.exists():
        raise FileExistsError(output)
    if not 0 <= args.tolerance <= 64:
        raise ValueError('tolerance must be between 0 and 64')

    image = Image.open(source).convert('RGB')
    rgb = np.asarray(image)
    h, w = rgb.shape[:2]
    flood = np.zeros((h + 2, w + 2), dtype=np.uint8)
    flood_flags = 4 | cv2.FLOODFILL_MASK_ONLY | (1 << 8) | cv2.FLOODFILL_FIXED_RANGE
    scratch = rgb.copy()
    # The reference packages use a clear studio margin. Flood from every edge
    # pixel so only background connected to the image border is removed.
    seeds = [(x, 0) for x in range(w)] + [(x, h - 1) for x in range(w)]
    seeds += [(0, y) for y in range(1, h - 1)] + [(w - 1, y) for y in range(1, h - 1)]
    for x, y in seeds:
        if flood[y + 1, x + 1] == 0:
            cv2.floodFill(scratch, flood, (x, y), (0, 0, 0),
                          loDiff=(args.tolerance,) * 3, upDiff=(args.tolerance,) * 3,
                          flags=flood_flags)
    background = flood[1:-1, 1:-1] != 0
    foreground = ~background
    if args.method == 'grabcut':
        # Treat the connected-background matte as a trimap. Definite background
        # stays fixed; the silhouette is probable foreground and an eroded
        # interior supplies definite foreground color samples. This removes
        # low-contrast floor shadows without any asset-specific coordinates.
        matte = np.full((h, w), cv2.GC_BGD, dtype=np.uint8)
        matte[foreground] = cv2.GC_PR_FGD
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9))
        core = cv2.erode(foreground.astype(np.uint8), kernel, iterations=1) != 0
        matte[core] = cv2.GC_FGD
        bg_model = np.zeros((1, 65), dtype=np.float64)
        fg_model = np.zeros((1, 65), dtype=np.float64)
        cv2.grabCut(rgb, matte, None, bg_model, fg_model, 5, cv2.GC_INIT_WITH_MASK)
        foreground = (matte == cv2.GC_FGD) | (matte == cv2.GC_PR_FGD)
    alpha = np.where(foreground, 255, 0).astype(np.uint8)
    # Keep antialiasing at the extracted silhouette without altering the RGB.
    alpha = cv2.GaussianBlur(alpha, (0, 0), 0.8)
    rgba = np.dstack((rgb, alpha))
    output.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgba, 'RGBA').save(output)
    mask_path = output.with_name(output.stem + '.mask.png')
    Image.fromarray(np.where(alpha > 127, 255, 0).astype(np.uint8), 'L').save(mask_path)
    ys, xs = np.where(alpha > 127)
    method = 'border-connected fixed-range RGB flood fill'
    if args.method == 'grabcut':
        method += '; 5-iteration GrabCut trimap cleanup'
    report = {
        'source': str(source),
        'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
        'output': str(output),
        'output_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
        'mask_preview': str(mask_path),
        'mask_preview_sha256': hashlib.sha256(mask_path.read_bytes()).hexdigest(),
        'dimensions_px': [w, h],
        'method': method + '; Gaussian alpha edge 0.8px',
        'tolerance_rgb': args.tolerance,
        'foreground_fraction': round(float(np.count_nonzero(alpha > 127)) / (w * h), 6),
        'foreground_bounds_px': [int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1],
        'review_status': 'needs-review'
    }
    report_path = output.with_suffix('.segmentation.json')
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
