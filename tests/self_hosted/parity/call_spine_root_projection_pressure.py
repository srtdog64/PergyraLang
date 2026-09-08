"""Measure a bounded real-owner query; this is not full-compiler timing."""

import pathlib
import statistics
import subprocess
import sys
import time


binary = pathlib.Path(sys.argv[1]).resolve()
for label, copies in (("small", 64), ("medium", 128), ("large", 256)):
    timings = {}
    for mode in ("scan", "indexed"):
        samples = []
        for _ in range(3):
            start = time.perf_counter()
            result = subprocess.run(
                [str(binary), mode, label], capture_output=True,
                timeout=15, check=False,
            )
            samples.append(time.perf_counter() - start)
            expected = f"{44 * copies * copies}\n".encode()
            if result.returncode != 0 or result.stdout.replace(b"\r", b"") != expected:
                sys.stderr.buffer.write(result.stderr)
                raise SystemExit(
                    f"{mode}/{label}: call identity digest differed; "
                    f"exit={result.returncode}, output={result.stdout[:160]!r}"
                )
            if result.stderr:
                sys.stderr.buffer.write(result.stderr)
                raise SystemExit(f"{mode}/{label}: unexpected runtime diagnostic")
        timings[mode] = statistics.median(samples)
    print(
        f"[call-spine-pressure] nodes={22 * copies} calls={4 * copies} "
        f"scan_seconds={timings['scan']:.6f} "
        f"indexed_seconds={timings['indexed']:.6f}", flush=True,
    )
# Timings are evidence, not a machine-dependent speed threshold. The source
# residue gate forbids the consumer's old scan; exact views/digests own results.
