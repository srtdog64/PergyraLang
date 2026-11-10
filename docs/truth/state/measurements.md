# Measurements

Owner: GPT root lane. Checked 2026-11-07 KST; timings are observations, not a
general performance claim.

## Current V.001 candidate

| Operation | Wall time | Peak / boundary | Result |
|---|---:|---:|---|
| fixed DRV-2 direct admission | 1,197.06 s | 9,507,424 KiB RSS | PASS: `body_ok=true` |
| canonical full MIR seed | 138.753 s | 14 GiB address-space bulkhead | PASS |
| canonical full MIR oracle | 197.767 s | 14 GiB address-space bulkhead | PASS |
| gen2 C emit | 207.11 s | 10,351,924 KiB RSS | PASS |
| gen2 host C compile | 129.64 s | 1,618,692 KiB RSS | PASS |
| gen3 C emit | 206.58 s | 10,351,632 KiB RSS | PASS |
| installed-driver contract | 60.64 s | 381,016 KiB RSS | PASS |
| four bootstrap execution gates | 97.34 s | 468,820 KiB RSS | PASS |
| policy corpus | 5.34 s | 202,724 KiB RSS | PASS |

The externally bounded full `make` run reached gen2 emission at 2,400 seconds
and was terminated there. Exact continuation completed gen2, host compilation,
bounded preflight and gen3 equality. The GitHub bootstrap job has a 60-minute
whole-job budget; only the pushed exact-SHA run can establish that budget.

The former 21.25 GiB full-MIR failure is no longer reproduced: both current
producers completed under the 14 GiB process bulkhead. This does not prove a
whole-language memory bound or compare against GC.
