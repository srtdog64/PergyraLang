# Pre-Self-Host Expansion Ledger

Status: `pre-self-host-expansion-ledger` (2026-06-26)

Hard self-hosting should not start by copying the C compiler's fragmentation.
Before a Pergyra-written compiler slice can grow, the language and compiler
surface it needs must already have a named owner, a gate, and a no-hidden-
fallback rule.

This ledger is the "bring it now" list. It records which expansion surfaces are
ready, which are active blockers, and which are deliberately held out of the
hard self-host path.

## Expansion Import Rule

Every surface needed by hard self-hosting must be in exactly one state:

| State | Meaning |
|---|---|
| `READY` | A named owner exists, a gate proves the contract, and a hard rung may consume it. |
| `ACTIVE` | The owner shape is known, but a Pergyra compiler slice must not rely on it as complete. |
| `HOLD` | The surface is intentionally excluded from the hard self-host path for now. |

An unclassified surface is not allowed. If a hard rung needs it, add the owner
and gate first or reject the input visibly. This is the no-hidden-fallback
rule for pre-self-host expansion.

## Ready Surfaces

| Surface | Owner | Gate | Self-host use |
|---|---|---|---|
| Compiler world shape | `PgyCompilerWorld`, `CompilePergyraProgram` | `self-host-compiler-world-contract-test-smoke` | one visible compiler flow, not a C folder graph |
| Path facts | `path_manifest_owner.pgy`, `SelfHostPath` | `self-host-preparation-contract-test-smoke` | stable source/test/parity paths and import-relative paths |
| File IO basics | `FileExists`, `ReadFile`, `WriteFile`, `Exit`, `Args` | `self-host-codegen-parity-test-smoke`, semantic parity fixtures | standalone tools and compiler slices |
| Directory walk | `DirWalk(String)` sorted snapshot | `filesystem-directory-walk-test-smoke` | live inventories without committed file-list aliases |
| Deterministic collections | `MapKeys`, `SetValues` over scalar compiler keys | `stage4-determinism-test-smoke` | stable diagnostics, codegen, MIR JSON, cache keys |
| Allocator lanes | `AllocatorScratch`, `AllocatorResult`, `AllocatorPersistent`, `AllocatorDestroy` | `runtime-abi-lifetime-test-smoke`, `abi-ownership-shape-test-smoke` | scratch/result/persistent compiler-pass lanes |
| Diagnostic rendering | `src/self_hosted/lib/diagnostic.pgy` | diagnostic catalog and semantic parity gates | no raw diagnostic construction in entrypoints |
| JSON read primitives | `src/self_hosted/lib/json.pgy` | component contract and real-source selfcheck | shared string/number/span reads for fact-shaped tools |
| MIR body facts | MIR source-shape / expression / source-local facts | `cfg-body-dataflow-test-smoke`, `ast-read-surface-smoke`, `self-host-mir-json-parity-test-smoke` | fact-only MIR lowering for the supported subset |
| Raw/FFI policy | scoped raw/unsafe boundary documents and runtime gates | `raw-escape-contract-test-smoke` | normal compiler slices stay out of raw pointer escape |
| Bit/layout boundary | `bits(..., order=...)`, `reinterpret(..., layout/endian/abi/world=...)` policy | `abi-ownership-shape-test-smoke`, language contract gates | no hidden logical-bit or backend-local layout defaults |
| Runtime materialization policy | AIR/MIR evidence and runtime-frontier docs | AIR erasure/materialization gates | no hidden runtime calls on static hot paths |
| Target capability envelope | `target_capability_owner.pgy`, `TargetCapabilityZone` | `self-host-compiler-world-contract-test-smoke`, real-source selfcheck | CPU/C/LLVM/self-hosted projections name accepted facts and fallback reasons before emission |
| Self-host C symbol spelling | `src/self_hosted/codegen/symbol_facts/symbol_mangle_owner.pgy` | component contract, real-source selfcheck, codegen parity | function/method/operator/enum names are consumed from one owner inside the current self-host C subset |
| Self-host C ABI type spelling | `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy` | component contract, real-source selfcheck, codegen parity | parameter, return, local, and field C type spellings are consumed from one owner inside the current self-host C subset |
| Self-host AST-text line inventory | `src/self_hosted/codegen/input/ast_text_inventory_owner.pgy` | component contract, real-source selfcheck, codegen parity | raw `pgy --ast` line splitting, indentation, blank-line filtering, and `[export]` normalization are consumed from one owner during the transitional text bridge |
| Self-host C collection runtime symbols | `src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | `Array<Int>` / `Array<String>` helper call names are consumed from one owner inside the current self-host C subset |
| Self-host C math/random runtime symbols | `src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | math/random helper call names are consumed from one owner inside the current self-host C subset |
| Self-host C host I/O runtime symbols | `src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | file, directory-walk, and argv helper call names are consumed from one owner inside the current self-host C subset |
| Self-host C Option/Result runtime symbols | `src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | `Option<Int>` / `Result<Int>` helper call names are consumed from one owner inside the current self-host C subset |
| Self-host C string/text runtime symbols | `src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | supported string/text helper call names are consumed from one owner inside the current self-host C subset |

## Active Blockers

These are the surfaces to bring in before claiming broader hard self-hosting.
They are not optional polish; each one prevents a common fallback shape.

| Blocker | Required owner | Why it matters |
|---|---|---|
| Mixed AST-like tree owner | Pergyra record/class/tagged-node owner plus traversal parity | The raw AST-text line inventory now has one owner, but codegen still consumes text lines; this blocker closes only when owned typed/tagged AST data replaces text-line consumption. |
| Stable JSON parse/emit owner | schema-aware JSON reader/writer with diagnostics | Read primitives are shared; schema validation, object/array iteration, and emit ownership still need one owner. |
| Subprocess runner | capability-gated process owner | Lets Pergyra runners invoke C/LLVM oracles without shell-only logic. |
| Symbol/mangle owner | canonical C/LLVM/self-hosted symbol and name-mangling fact table | The self-host C subset now has one spelling owner; full C/LLVM ABI parity still needs a shared symbol fact table consumed by every backend. |
| Cross-backend ABI/layout row projection | self-hosted and native consumers of shared ABI layout facts | The self-host C subset now has one type-spelling owner; full C/LLVM/self-hosted ABI parity still needs shared layout rows for field order, niche, tag, and ownership shape. |
| AIR evidence zone | owned AIR evidence facts in `PgyCompilerWorld` | Makes intent/effect/authority/coordination evidence consumable by hard rungs. |
| Artifact Zone evidence | one parity sink for diagnostics, IR JSON, ABI/layout, emitted artifacts, and run output | Keeps C, LLVM, and self-hosted outputs comparable by owned artifact. |
| Test harness substrate | Pergyra-owned fixture/run/result records | Stops hard rungs from being permanently shell-owned after the first bridge. |

## Held Surfaces

These are explicitly not imported as default hard-self-host dependencies.

| Surface | State | Reason |
|---|---|---|
| General raw pointer API | `HOLD` | Would reopen the normal domain/codegen path to unmanaged aliasing. |
| Broad `extern "C"` FFI for compiler internals | `HOLD` | Parallel binaries and artifact comparison are safer until ABI contracts are stable. |
| Native runtime rewrite in Pergyra | `HOLD` | The runtime kernel is the target program's native support layer; rewriting it creates a bootstrap cycle. |
| Zero-runtime erasure claim | `HOLD` | Pergyra requires no-hidden-runtime evidence, not universal zero-cost erasure. |
| Direct WASM backend as self-host prerequisite | `HOLD` | C/LLVM remain the first projections; direct wasm is post-beta. |

## Work Order

1. Promote the mixed AST-like tree owner first. It is the largest remaining
   reason self-hosted parser/codegen slices still consume text artifacts.
2. Add a stable JSON fact owner before adding more IR/AIR tools.
3. Add subprocess as a capability-gated owner, not as unrestricted shell escape.
4. Finish cross-backend symbol/mangle and ABI/layout consumers before widening backend parity.
5. Add AIR evidence and Artifact Zone evidence to `PgyCompilerWorld` before
   claiming three-way compiler self-proof.

## Rejection Rule

A hard rung must fail if it needs an `ACTIVE` or `HOLD` surface and tries to
continue by:

- parsing text or JSON to recover a fact whose owner should already exist;
- choosing C-like behavior when C and LLVM disagree;
- inventing ABI layout, symbol spelling, authority evidence, slot ownership, or
  materialization policy locally;
- building diagnostics or JSON in `main.pgy`;
- adding a fake zone around a function group that does not own a distinct
  resource.

This is the expansion guard: add the surface now, or reject the program. Do not
grow self-hosting by hiding another compatibility path.
