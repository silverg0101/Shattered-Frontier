"""Run one Blender stage, accepting only freshly produced declared outputs."""
import json
from pathlib import Path
import runpy
import shutil
import sys


def run(script, args, inputs, outputs):
    root = Path.cwd().resolve()

    def confined(name):
        path = Path(name)
        candidate = root / path
        resolved = candidate.resolve()
        if path.is_absolute() or ".." in path.parts or resolved == root or root not in resolved.parents:
            raise ValueError(f"{script}: path outside stage folder: {name!r}")
        if any(p.is_symlink() for p in [candidate, *candidate.parents] if p != root and root in p.parents):
            raise ValueError(f"{script}: stage path must not follow a symlink: {name!r}")
        return resolved

    # Validate the entire declaration before removing anything. Inputs may contain
    # old outputs (a library directory), but an output must never contain an input.
    ins = [confined(p) for p in [script, *inputs]]
    outs = [(name, confined(name)) for name in outputs]
    for name, path in outs:
        if any(p == path or path in p.parents for p in ins):
            raise ValueError(f"{script}: output would remove an input: {name!r}")
    for _, path in outs:
        if path.is_dir():
            shutil.rmtree(path)
        elif path.exists():
            path.unlink()

    argv = sys.argv
    sys.argv = [script, "--", *args]
    try:
        try:
            runpy.run_path(script, run_name="__main__")
        except SystemExit as error:
            if error.code not in (None, 0):
                raise
        missing = [name for name, path in outs if not (
            path.is_file() and path.stat().st_size > 0 or path.is_dir() and any(path.iterdir()))]
        if missing:
            raise RuntimeError(f"{script}: missing or empty declared outputs: {', '.join(missing)}")
    finally:
        sys.argv = argv


if __name__ == "__main__":
    spec = json.loads(sys.argv[sys.argv.index("--") + 1])
    run(spec["script"], spec["args"], spec["inputs"], spec["outputs"])
