"""Print the authored Silverbrook claim band and clearance for a 1 m cell."""
import argparse
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def distance(point, path, closed=False):
    pairs = zip(path, path[1:] + path[:1] if closed else path[1:])
    result = math.inf
    for a, b in pairs:
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = dx * dx + dy * dy
        t = min(1, max(0, ((point[0] - a[0]) * dx + (point[1] - a[1]) * dy) / length)) if length else 0
        result = min(result, math.hypot(point[0] - a[0] - t * dx, point[1] - a[1] - t * dy))
    return result


def inside(point, polygon):
    result = False
    for a, b in zip(polygon, polygon[1:] + polygon[:1]):
        if (a[1] > point[1]) != (b[1] > point[1]) and point[0] < (b[0] - a[0]) * (point[1] - a[1]) / (b[1] - a[1]) + a[0]:
            result = not result
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("x", type=int)
    parser.add_argument("y", type=int)
    args = parser.parse_args()
    point = (args.x + .5, args.y + .5)
    zone = json.loads((ROOT / "content/world/zones/greenlands_silverbrook.json").read_text())
    layout = json.loads((ROOT / "GameClient/ArtSource/Greenlands/silverbrook-town-layout.json").read_text())
    towns = [a["polygon"] for a in zone["areas"] if a.get("purpose") == "settlement" and a.get("safety") == "protected"]
    town_distance = min(0 if inside(point, polygon) else distance(point, polygon, True) for polygon in towns)
    band = "near" if town_distance <= 150 else "middle" if town_distance <= 500 else "wilderness"
    extent = zone["extent"]
    reasons = []
    if not (extent[0][0] + 16 <= point[0] <= extent[1][0] - 16 and extent[0][1] + 16 <= point[1] <= extent[1][1] - 16):
        reasons.append("border")
    if any(inside(point, a["polygon"]) or distance(point, a["polygon"], True) < 5 for a in zone["areas"] if a.get("safety") == "protected"):
        reasons.append("protected")
    if any(math.dist(point, p["position"]) < 25 for p in zone["respawn_points"]) or math.dist(point, zone["default_spawn"]["position"]) < 50:
        reasons.append("spawn")
    if any(distance(point, path) < 6 for group in ("primary", "secondary", "trail") for path in layout["roads"][group].values()):
        reasons.append("road")
    print(json.dumps({"cell": [args.x, args.y], "band": band, "town_boundary_m": round(town_distance, 2), "blocked": reasons}))


if __name__ == "__main__":
    main()
