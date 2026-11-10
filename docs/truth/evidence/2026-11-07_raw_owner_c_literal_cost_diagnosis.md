# Frozen receipt: constant Array literal cost diagnosis

Observed 2026-11-07 KST. Scope: default native C generated-output experiment.
HEAD `60fd1e26d8ea3987efbcbfc65f59b892b234701a` plus preserved shared dirty work.
No production source, installed driver, commit or push changed in this run.

## Identity and method

- Reused native emitter SHA-256:
  `0ba9ac10dfb2b37ce61587bdd819478579b4107d161f71f711b7d8aaa4150aa3`.
- Reused runtime: `/root/pgy-oracle-rss-20261106/source`.
- Inputs and original controls:
  `/root/pgy-raw-owner-patterns-20261107/algorithms-final`.
- Results and generated/binary variants:
  `/root/pgy-raw-owner-patterns-20261107/speed-diagnosis-c`.
- Reproduction scripts: `.tmp/raw-owner-speed-diagnosis-20261107/diagnose_c.py`
  and `check_boundaries.py`. Both require fresh output directories.
- Portable receipts: `.tmp/raw-owner-speed-diagnosis-20261107/receipts/`;
  `results.json`, `boundaries.json`, both command manifests, `metadata.json`,
  `inputs.json` and `artifacts.json` record measurements and byte identities.
- All 228 original fixture/runtime/compiler input hashes were rechecked.
- Clang 18.1.3, CPU 15, `-O2 -fwrapv -fno-strict-aliasing -pthread`.
  Timing is uninstrumented, within-process, with a checked result sink.
  CPU was pinned; the shared machine was not otherwise exclusively reserved.
- Only constant integer Array literal construction changed in scratch output:
  same `pgy_array_new_Int(n)`, then constant memcpy and length publication.
  Element access and drops are unchanged. No production emitter was modified.

## Nine interleaved samples, 12,000 Work executions each

| Algorithm | Direct C median ms | Original C median ms | Scratch C median ms |
|---|---:|---:|---:|
| Radix sort | 30.184 | 62.618 | 29.698 |
| Robin Hood | 50.513 | 77.333 | 42.155 |
| Dijkstra | 14.010 | 43.074 | 14.442 |
| A* | 14.355 | 39.422 | 16.390 |
| Union-find | 55.569 | 74.073 | 51.695 |
| LRU | 20.346 | 35.597 | 20.391 |

## Independent longer confirmation

Seven interleaved samples, 120,000 Work executions each:

| Algorithm | Direct C median ms | Original C median ms | Scratch C median ms |
|---|---:|---:|---:|
| Dijkstra | 139.454 | 408.898 | 147.025 |
| A* | 136.095 | 385.638 | 149.462 |

## Checks and limits

- Six changed executables pass 251 independent seed-answer checks each: 1,506.
- Six ASan/UBSan executions of 503 Work each pass. Observed allocation/free
  pairs are 1509/1509, 1509/1509, 3521/3521, 3521/3521, 1006/1006,
  2515/2515; reallocations and live blocks are zero in each case.
- Original and changed read/write probes on `[7,11,13,17,19]` produce the
  expected results and one allocation/free each on 20 successful executions.
  All eight negative probes abort with the intended index-out-of-bounds
  diagnostic at index -1 or 5. These are runtime bounds controls.
- Final C executables originally contain 833 literal push callsites for
  Dijkstra/A* and 512 for Union-find. Changed ones contain none. The whole
  algorithm body and cleanup remain present and execute the checked workload.
- This follow-up does not prove all literal kinds, allocation failure,
  installed-driver equivalence, full compiler input, bootstrap, CI or universal
  safety/performance. It introduces no compiler admission change; earlier
  ownership accept/refuse tests were not rerun as a new compiler gate.
