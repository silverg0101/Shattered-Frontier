"""Compare opt-in Unreal pilot boundary output with the exact Python oracle."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.construction_compatibility import Cell, compatibility
from tools.construction_geometry import ROTATIONS, SUPPORTED

FIELDS = ['own_shape', 'own_rotation', 'neighbor_shape', 'neighbor_rotation', 'side', 'area_cm2']
SIDES = ['-X', '+X', '-Y', '+Y', '-Z', '+Z']
COUNT = 3 * 24 * 3 * 24 * 6


def compare(path):
    keys = list(SUPPORTED)
    seen = set()
    maximum_error = 0.0
    with path.open(newline='', encoding='utf-8-sig') as stream:
        rows = csv.DictReader(stream)
        if rows.fieldnames != FIELDS:
            raise ValueError('Unexpected oracle CSV columns')
        for row in rows:
            if list(row) != FIELDS or any(value is None for value in row.values()):
                raise ValueError('Malformed oracle CSV row')
            identity = tuple(int(row[field]) for field in FIELDS[:5])
            own, rotation, other, other_rotation, side = identity
            if not (0 <= own < 3 and 0 <= other < 3 and 0 <= rotation < 24
                    and 0 <= other_rotation < 24 and 0 <= side < 6):
                raise ValueError(f'Invalid case ID: {identity}')
            if identity in seen:
                raise ValueError(f'Duplicate case: {identity}')
            seen.add(identity)
            direction = [0, 0, 0]
            direction[side // 2] = 1 if side % 2 else -1
            a = Cell(keys[own], keys[own], (0, 0, 0), ROTATIONS[rotation])
            b = Cell(keys[other], keys[other], tuple(direction), ROTATIONS[other_rotation])
            expected = float(compatibility(a, SIDES[side], b)['cell_exposed_area'] * 2500)
            actual = float(row['area_cm2'])
            error = abs(actual - expected)
            if not math.isfinite(actual) or error > 1.e-7:
                raise ValueError(f'Boundary mismatch {identity}: {actual} vs {expected} cm2')
            maximum_error = max(maximum_error, error)
    if len(seen) != COUNT:
        raise ValueError(f'Incomplete cases: {len(seen)} of {COUNT}')
    return {'cases': len(seen), 'maximum_absolute_error_cm2': maximum_error,
            'tolerance_cm2': 1.e-7, 'csv_sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    parser.add_argument('--report', type=Path, help='New output file; refuses overwrite')
    args = parser.parse_args()
    result = compare(args.csv)
    output = json.dumps(result, indent=2) + '\n'
    if args.report:
        with args.report.open('x', encoding='utf-8') as stream:
            stream.write(output)
    print(output, end='')
