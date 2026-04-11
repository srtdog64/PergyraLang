# Spray Device Probe

`spray_device_probe` checks two adjacent layers together:

- `use spray;` batch-dispatch surface
- `DeviceSlot<Int>` + `RemoteFuture<Int>` readback surface

## What It Proves

- the stdlib module `spray` is importable through `use spray;`
- `SprayAll(...)` works as a batch helper over multiple buffers
- device-style readback is currently available through `DeviceSlot` and
  `RemoteFuture`, even though `spray` itself is still a CPU-simulated dispatch
  layer

## Important Interpretation

Current status is intentionally split:

- `spray`:
  batch-dispatch library surface
- `DeviceSlot` / `RemoteFuture`:
  device/GPU-like resource path
- real GPU kernel backend:
  not wired yet; current probe reports `real-gpu-backend=false`

So the expected conclusion of this probe is not "Pergyra already has a real GPU
backend". The expected conclusion is:

- `spray` works as a stable library surface
- device-slot readback works
- the bridge from `spray` to a real Vulkan/CUDA/Metal backend is still future
  integration work, and the probe says so explicitly in stdout/results

## Current Output Contract

The probe is expected to print:

- three simulated spray dispatch lines
- a success summary with `all=true`
- device readback values `910 911 912`
- capability summary with `real-gpu-backend=false`

That means this probe is no longer a placeholder document. It is a regression
description for the current simulated-dispatch plus device-readback contract.

## Regression Coverage

- exact stdout:
  [`examples/spray_device_probe/expected_stdout.txt`](/mnt/e/PergyraLang/examples/spray_device_probe/expected_stdout.txt)
- exact results:
  [`examples/spray_device_probe/expected_results.txt`](/mnt/e/PergyraLang/examples/spray_device_probe/expected_results.txt)
- example smoke:
  [`tests/example_contract_smoke.sh`](/mnt/e/PergyraLang/tests/example_contract_smoke.sh)
