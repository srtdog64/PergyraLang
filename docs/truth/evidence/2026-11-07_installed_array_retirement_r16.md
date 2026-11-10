# Installed Array provenance, retirement and release performance

Observed 2026-11-07 KST at shared HEAD
`60fd1e26d8ea3987efbcbfc65f59b892b234701a`, with dirty work preserved.
This closes the fixed eight-input comparison, not V1, whole-language memory
safety, a new gen2 == gen3 or published CI.

## Fixed source and installed pair

Source: `/root/pgy-owner-production-20261107/candidate-r16`.
Evidence: `/root/pgy-owner-production-20261107/r16-joint-runtime`.
Handoff: `D:\PergyraLang\.tmp\owner-production-20261107\final-handoff`.

| Artifact | SHA-256 |
|---|---|
| Native launcher | `a655563114dfc0d5810862669d01d4a14e944f71d8eb46d6fb96f511d469da3a` |
| Installed driver | `74768fffba4276c6c7f9ea99c5277c1fd554fdafffc40943654b35b21a380711` |
| Machine manifest | `44872a80153f54b29a207a5b96abd746ef289b0ebbfdc4220bdfd28c8789b4f2` |
| Admitted codegen seed | `954bd3a7905dbbd42bb7fd438d1b76ff101d55df8754dc6b6d10e2f6097a6f7e` |

The launcher was rebuilt without old objects. The repository's official
`self_host_compiler_build.sh` installed the Pergyra-built driver in 279.321 s.
No native-built substitute driver was installed. Source manifests and
`identity-verification.json` confirm unchanged compiler C/H/Pergyra inputs,
matching observation/timing identities and unchanged original r7 trial inputs.

## Repaired chain and retained boundaries

Main owns Array member provenance: normalized statement positions distinguish
completed reads from later root transfers; repeated/deferred hazards remain
opaque. Earlier shared Slice consumers use the typed index owner and preserve
default Slice mutation refusal. The earlier provenance receipt records 64/64
and 17/17 observer decisions. This record adds installed downstream behavior.

The final delta preserves the indexed assignment use cursor after consuming
receiver/index, carries foreign scalar declaration identity and physical ABI
facts through both general emitters, and admits Slice<Int> phi/store uses under
existing LocalRef checks. It calls the checked public Slice ABI; no Array
descriptor cast is used. The supported typed source family now selects the
general C emitter without requiring name collisions. Claimed failures remain
terminal; unsupported Float families retain their existing owner before claim.

One admitted CFG generation owns fresh local plain-element Array retirement.
Both emitters consume it at exits after evaluating the return value. Unknown
retention, transfers, returned storage, caller-owned parameters, Array aliases
and repeated allocation do not authorize this retirement. Manual drop clears
the descriptor. This conservative boundary does not cover every collection
lifetime or owning element type.

Int modulo uses the existing checked i32 export through ABI row 270. Int wrap
arithmetic and comparisons operate at i32 width before restoring the existing
i64 carrier; Long stays 64-bit. No invented range/alias/overflow premise or
removed arithmetic/bounds check is involved.

The final LLVM bottleneck was combining runtime bodies after optimizing IR.
The runtime-cache owner now publishes compiler-specific bitcode and returns
its representation explicitly. The installed release consumer links that
module before Clang optimization. Native/dev and explicit prebuilt-object
contracts remain intact. No CPU-specific flag, edited emitted IR, new user
switch, per-workload route or manual drop was added to the eight sources.

## Same eight inputs: physical release and performance

All eight original sources, support, raw C comparators and work counts remain
unchanged. Installed C/LLVM compile and execute 16/16 with correct checksums.
At 503 calls per artifact, the 16 Pergyra runs observe **29,174 allocations,
29,174 frees and zero live blocks**; raw C runs are excluded from these totals.

Timing uses Clang/LLVM 18, CPU 15 and nine shuffled interleaved repetitions.
The denominator is the fastest median of ordinary, whole-module and
native-target whole-module raw C. Algorithms use 120,000 calls, buffers 200,000;
every result is checked. This is a local measurement on a shared machine.

| Input | Installed C / fastest C | Installed LLVM / fastest C |
|---|---:|---:|
| fixed_owner | 1.090297 | 1.150376 |
| fixed_view | 1.104494 | 1.145130 |
| radix_sort | 1.059538 | 1.090102 |
| robin_hood | 0.858823 | 0.868735 |
| dijkstra | 1.022054 | 0.971253 |
| astar | 1.087519 | 1.029689 |
| union_find | 0.968557 | 0.986330 |
| lru | 1.043286 | 1.060244 |

**16/16 meet 1.3x.** R15's LLVM buffer failures (1.610657/1.481304) remain
red receipts. R16's result is from the actual installed launcher, not the
joint-module probe, and is not a cross-machine performance guarantee.

## Observed gates and explicit limits

- Original installed Array index admission, default C/LLVM route, checked Int
  division/modulo and scalar Array index gates: PASS.
- Same 23 forged MIR inputs: **46/46** C/LLVM refusals, plus two valid emission
  controls. Portable replay passes **48/48**. These cover LocalRef, predecessor
  use order, foreign ABI identity, Slice phi, double drop, use after drop/
  transfer and borrowed forwarding. Unreadable input cannot count as refusal.
- Installed Int/Long width, comparison/modulo, division panic and Slice bounds:
  **10/10**. Runtime representation/cache/toolchain/prebuilt contracts: **9/9**.
- Additional source positives: **23/24**. Returned Array LLVM remains refused;
  the same source and diagnostic reproduce on installed r11, r13 and r16.
  C accepts it. This is an existing unsupported route, not a green generality
  result. Other controls cover aliases, early returns, repeated blocks, manual
  drop, indexed stores, Slice read/write/copy and foreign Int/Long/Bool values.
- Supporting unchanged native analysis ran on r15: 3,268 pass, two existing
  diagnostic-wording failures; currency replay 13,688 checks, zero failures.
  These are r15 evidence, not a final whole-suite green claim.
- ABI expected rows were regenerated through native C and LLVM with byte-equal
  output, not edited by hand.

Invalid early harness results are excluded and retained: absolute MIR paths
produced unreadable-input refusals; injecting support C source into the joint
module command linked the runtime twice. Corrected runs use readable relative
MIR paths and a separately compiled external support object.

## Patch and main boundary

Full `installed-array-r7-to-r16.patch`: 64 files against the preserved dirty r7
source, not clean HEAD. SHA-256:
`2cce5a2314b939e9cd0c82df2adb71cc2ca4c2333eb1917069967b1006a88d96`.
Check, isolated replay and all resulting file hashes pass.

Four shared fixtures already match final content. The other 60 files match
the baseline; `installed-array-main-pending.patch` passes `git apply --check`
against the shared tree. SHA-256:
`d253aadb3910005cb3ad515e6748ecec8627a25118a7a5513850c756ad994f8d`.
All 16 earlier native optimization dependencies already match shared bytes.
Previous dependency patches/manifests are included. No shared compiler source
was overwritten in final packaging; no staging, commit or push occurred.

Main is performing its owner-file cap migration. Sixteen touched owners remain
above unchanged caps, including pre-existing excess; the bundled cap receipt
records old/new sizes. Main must integrate these deltas with that migration
and validate canonical input, fixed point and required CI on the final source.
Its earlier frozen candidate's success does not certify r16.

## Reproduction

Run `tests/raw_owner_driver_trials.py --route installed` with the fixed root,
driver and fresh output, once with `--observe` and once without. Preserve cases
`fixed_owner fixed_view radix_sort robin_hood dijkstra astar union_find lru`.
The bundle contains source/binary/input identities, command/result receipts,
MIR/source controls, permanent installed boundary tests and portable
`replay_refusals.py --root <candidate-root>`. Recorded installer/build scripts
identify the original source and seed. See current lane state for integration.
