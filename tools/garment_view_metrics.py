"""Shared pixel checks for the depth-guided garment turnaround."""
import numpy as np


def opening_background_px(rgb):
    """White pixels between the outer garment edges, above the loose lower hem.

    A face aperture can connect to the outside in 2D, so flood-filling holes alone
    misses a see-through hood. Row envelopes catch that opening without treating
    white gaps between separate scallops along the cape hem as face openings.
    """
    foreground = rgb.min(axis=2) < 225
    white = rgb.min(axis=2) > 237
    rows = np.flatnonzero(foreground.any(axis=1))
    if not len(rows):
        return 0
    hem_start = int(rows[0] + .85 * (rows[-1] - rows[0]))
    count = 0
    for y in rows:
        if y >= hem_start:
            break
        xs = np.flatnonzero(foreground[y])
        if len(xs) > 4:
            count += int(white[y, xs[0] + 2:xs[-1] - 1].sum())
    return count
