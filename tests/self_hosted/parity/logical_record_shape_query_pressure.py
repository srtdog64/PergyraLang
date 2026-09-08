"""Bounded fixed-count real-owner query timing, before/after binaries separately."""

import pathlib
import statistics
import subprocess
import sys
import time


binary = pathlib.Path(sys.argv[1]).resolve()
for size, fields in (("small", 32), ("medium", 64), ("large", 128)):
    samples = []
    for _ in range(3):
        start = time.perf_counter()
        result = subprocess.run([str(binary), size], capture_output=True, timeout=15, check=False)
        samples.append(time.perf_counter() - start)
        if result.returncode != 0 or result.stdout.replace(b"\r", b"") != b"2048 noncandidate queries preserved\n":
            sys.stderr.buffer.write(result.stderr)
            raise SystemExit(f"{size}: query result differed: {result.stdout[:160]!r}, exit={result.returncode}")
        if result.stderr:
            sys.stderr.buffer.write(result.stderr)
            raise SystemExit(f"{size}: unexpected runtime diagnostic")
    print(f"[record-shape-pressure] fields={fields} queries=2048 median_seconds={statistics.median(samples):.6f}", flush=True)
