# 2026-11-17: A1 flat MIR JSON projection and reached array allocation failure

Observed from main parent `7ea3c781f` plus this candidate. Edits are in
`D:\PergyraLang`; Linux validation is the existing WSL `/root/mg-main`.
The WSL Git base remains `5b0d354fb`; it is not relabeled as the main commit.
The mirrored production Pergyra sources were compared by normalized content.

## Source and executable identity

The 2842 production .pgy paths are mapped to SHA-256 of decoded UTF-8
with normalized newlines. SHA-256 of that sorted, compact JSON map:
`5f4974f5fac1719da12a93cccd7e2b1a46f68bf3746b7960e62cf3f6c9f27d31`.

| Artifact | SHA-256 |
|---|---|
| Native validator, LLVM 18.1.3 | `f44310784eb710ac708abea794e9db517b94a1f08096b2088e55c62ad628878a` |
| Installed final Pergyra-built driver | `5f3ed7376a61ec96a4bbf40e5b49c5cd0f5d53cb0e10698e0d4c80801cacfd32` |
| Codegen gen2.c and gen3.c (equal) | `0024f19efb69a6e74198f7b15d5dd9b63a4b9e6e39572ed6fd04c6069a91a423` |
| Final semantic MIR input | `5e9b60cf808bf5e2ddbf908875836affb2dd28df6efcc09796059b8deaf7c737` |
| Final semantic C | `c49265cc8be021cdfca79cf7f80960bc79e790faa127a7f1f303901d5817e552` |
| Final MIR-lower MIR input | `55903ede8c965ff11112da1c259d01e7f72fd7eee2e8a1e2e881b085bbeb0a1d` |
| Final MIR-lower source-to-C and MIR-to-C (equal) | `eb3288481b0222d18b33c4edcf431061ba557fc6c34e29109018b4c60b5d96a4` |

## Changed chain

The six sequential file writers remain the wire-schema source. A bounded
generator specializes their output formal and write calls into six pure
memory consumers. These append independently owned fragments to one flat
buffer, join once, and retire that buffer at one exit. The old nested
program/instruction String projection is deleted and its callers migrated.
Bounded field renderers still exist; this is not compiler-wide String cleanup.

Readonly source-to-MIR still compiles without write authority. Owned
renderer fragments establish their cleanup owner before being borrowed.
An indexed-String source lattice literal base case fixes the conditional
join's unknown-source result; actual conditional loans and static frees
remain refused. No StringJoin name exception or weaker drop rule was added.

The complete source-to-C probe also reached unchecked AST-C array growth.
The existing collection runtime owner now emits one checked push body for
Int, Long, String, owned String, Bool, Float, Double, AST node and nominal
record arrays. Capacity and byte-size overflow are checked before realloc;
backing/capacity publish only on success, using the existing OOM panic.
The source owner shrank from 600 to 582 code lines. No new owner family or
SoT CLOSED claim is made.

## Observed gates

- Codegen bootstrap: actual gen0 -> gen1 -> gen2 -> gen3; gen2 == gen3,
  123,563 lines. The final driver was then emitted by that Pergyra-built
  codegen and installed by the ordinary installer (not an old stamp reuse).
- Wire projection: nine complete source inputs, String/file bytes equal,
  invalid fact projections refused and pre-existing outputs preserved.
  Native C release and native LLVM dev outputs equal all nine independently
  saved pre-change goldens. Evidence: `.tmp/self_hosted/mir_json_writer_byte_parity.tQXs1n`.
- The LLVM dev probe is an actual compiled/running LLVM leg. Its successful
  boundary receipt is `llvm-dev-build.receipt.json`. The same probe's default
  release build FAILED in LLVM 18 InstCombine after 21:27.71; it is not counted
  as passed or silently replaced. LLVM 18's implementation does not consult
  the function attribute used by the existing backend workaround
  ([upstream 18.1.8 source](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-18.1.8/llvm/lib/Transforms/InstCombine/InstructionCombining.cpp)).
  No optimizer policy was changed in this patch.
- Sink C/LLVM at -O0: ASan+LSan PASS; changing the typed pool copy to a borrow
  produces heap-use-after-free in both. Evidence: `json-projection-lifetime.bXxUzp`.
  This isolates the sink with an empty pooled literal. Existing nonempty
  escaper substring/result temporaries still leak and are not covered by it.
- Native/installed C/LLVM join lifetime: four positives and eight refusals
  (conditional element loan and freeing static String storage), no refused
  executable published. Evidence: `json-join-contract.7orDfl`.
- Array allocation failure: the old installed emitter's same nine-case
  program preserved normal values; injected backing realloc failure caused
  SIGSEGV in eight families and typed OOM in owned String. New installed
  emitter: all nine preserve values and all nine abort with class=oom,
  exit 134, with core dumps disabled. Evidence: `array-growth-allocation.CJXKLM`
  (old matrix: `array-growth-allocation.aryfiO`).
- Component/source-size and MIR generator drift: PASS, 3,096 cap requests.
  SoT registry, source inventory, keyword generator and documentation gates
  passed earlier in this edit; the final publication checks are rerun before commit.

## Memory and complete-byte comparison

All final gate phases retain the existing 14 GiB address-space and 30-minute
process boundary. No allowance or correctness input was reduced/increased.

| Final installed-driver phase | Peak RSS KiB | Ceiling | Result |
|---|---:|---:|---|
| Complete semantic MIR -> C | 797,588 | 819,200 | PASS |
| Complete MIR-lower source -> C, one process | 5,552,288 | 8,388,608 | PASS |
| Complete MIR-lower source -> MIR -> C | separate phases | existing boundary | bytes equal |

Reproduce with `tests/self_hosted/parity/driver/selfhost_compile_memory_owner.sh`.
Receipts, inputs and outputs: `.tmp/self_hosted/selfhost-memory.QhnW4A/`.

An earlier same-input study, before the array panic repair, used normalized
production source map `3018e0ade6fa425a00990d87bb309f6de9d112f7e82955343d1e3eaad64d7dfc`.
Baseline driver `81497d8b93c80da4127e12314327ee45aec54bb93a94a547cdeb66e3193c171f`
peaked at 8,746,168 KiB; pure A1 driver
`adafe527cc089e62347777e024aea492e74b601df6264608064c24e2fa7a8b66` peaked at
5,555,788 KiB. Both generated C SHA-256
`bb02e0f891b88a4b07f1ae49911012769a641a23f944aa3f51b56a043c366df0`.
This is 36.48% less RSS for the combined A2+A1 changes, not attribution to
A1 alone and not evidence of lower wall time.

## Still open

The complete DRV-2 one-process source-to-C probe did NOT complete within the
unchanged 14 GiB boundary: peak 14,107,288 KiB, signal 11. GDB stopped at a
backing store in `SemanticAstCollectionExclusiveAddDep` during consumer
ownership verification; the emitted ordinary push had unchecked realloc.
The small allocation-failure matrix confirms the old and repaired behavior;
a final complete-input rerun is still required after this record.
Logs: `.tmp/selfhost-memory-20261117/a1-drv2-source-c.*` and `a1-drv2-gdb.*`.

Owned-value facts, retire instructions, their verifier, complete direct-MIR
compiler admission and the installed driver fixed point are not inferred
from the codegen fixed point. Continue the dependency order in the
[memory plan](../../agent_work_directives/selfhost_compile_memory_plan_2026-11-17.md).
Required CI was last observed fully green at parent `7ea3c781f`, run
`44024724037` (33 jobs, bootstrap/installed checks executed). This local
candidate's publication and exact-commit CI result are not claimed here.
