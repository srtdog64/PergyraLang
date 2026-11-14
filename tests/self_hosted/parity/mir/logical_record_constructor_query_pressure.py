"""Fixed table/query count; vary only the reached constructor argument count."""
from pathlib import Path
import statistics
import subprocess
import sys
import time

binary = Path(sys.argv[1]).resolve()
for size, arity in (("small", 4), ("medium", 8), ("large", 16)):
    samples = []
    for _ in range(3):
        start = time.perf_counter()
        result = subprocess.run([str(binary), size], capture_output=True, timeout=15, check=False)
        samples.append(time.perf_counter() - start)
        if result.returncode != 0 or result.stdout.replace(b"\r", b"") != b"128 ordered constructor queries preserved\n" or result.stderr:
            sys.stderr.buffer.write(result.stderr)
            raise SystemExit(f"{size}: output/exit differed: {result.stdout[:160]!r}, exit={result.returncode}")
    print(f"[constructor-query] fields=129 queries=128 arity={arity} median_seconds={statistics.median(samples):.6f}", flush=True)
