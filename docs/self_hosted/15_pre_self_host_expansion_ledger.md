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
| JSON read/emit primitives | `src/self_hosted/lib/json.pgy` | component contract and real-source selfcheck | shared string/number/span reads, document schema checks, document number-field reads, plus JSON string, field, object, and array emission for fact-shaped tools |
| MIR body facts | MIR source-shape / expression / source-local facts | `cfg-body-dataflow-test-smoke`, `ast-read-surface-smoke`, `self-host-mir-json-parity-test-smoke` | fact-only MIR lowering for the supported subset |
| Raw/FFI policy | scoped raw/unsafe boundary documents and runtime gates | `raw-escape-contract-test-smoke` | normal compiler slices stay out of raw pointer escape |
| Bit/layout boundary | `bits(..., order=...)`, `reinterpret(..., layout/endian/abi/world=...)` policy | `abi-ownership-shape-test-smoke`, language contract gates | no hidden logical-bit or backend-local layout defaults |
| Runtime materialization policy | AIR/MIR evidence and runtime-frontier docs | AIR erasure/materialization gates | no hidden runtime calls on static hot paths |
| Target capability envelope | `target_capability_owner.pgy`, `TargetCapabilityZone` | `self-host-compiler-world-contract-test-smoke`, real-source selfcheck | CPU/C/LLVM/self-hosted projections name accepted facts and fallback reasons before emission |
| Self-host C symbol spelling | `src/self_hosted/compiler/symbol_table_owner.pgy` | component contract, real-source selfcheck, codegen parity | function/method/operator/enum names are consumed from the compiler-world row owner inside the current self-host C subset |
| Self-host C ABI type spelling | `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy` | component contract, real-source selfcheck, codegen parity | parameter, return, local, and field C type spellings are consumed from one owner inside the current self-host C subset |
| Self-host typed AST-text bridge | `src/self_hosted/codegen/input/ast_text_inventory_owner.pgy` plus `pgy --ast` parameter-mode preservation | component contract, real-source selfcheck, parser parity, codegen parity | raw `pgy --ast` line splitting, `CodegenAstTextNode` inventory, indentation, blank-line filtering, `[export]` normalization, program-level typed routing, declaration collector prepasses, function signature/header facts, statement body reads, `inout`/`own`/`ref` parameter-mode facts, and cursor expectation checks are consumed from one owner during the transitional text bridge |
| Self-host C collection runtime symbols | `src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | `Array<Int>` / `Array<String>` helper call names and the bootstrap-only `Array<CodegenAstTextNode>` record-array lane are consumed from one owner inside the current self-host C subset |
| Self-host C math/random runtime symbols | `src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | math/random helper call names are consumed from one owner inside the current self-host C subset |
| Self-host C host I/O runtime symbols | `src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | file, directory-walk, and argv helper call names are consumed from one owner inside the current self-host C subset |
| Self-host C Option/Result runtime symbols | `src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | `Option<Int>` / `Result<Int>` helper call names are consumed from one owner inside the current self-host C subset |
| Self-host C string/text runtime symbols | `src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | supported string/text helper call names are consumed from one owner inside the current self-host C subset |
| Basic nominal-record arrays | LLVM array registry `elem_name` facts plus raw record-array runtime exports | `backend_compare/record_array_basic` through C and LLVM | `Array<NominalRecord>` can be created, passed as a parameter, pushed, set, popped, indexed, and used for member access without reopening AST type guessing |

## Active Blockers

These are the surfaces to bring in before claiming broader hard self-hosting.
They are not optional polish; each one prevents a common fallback shape.

| Blocker | Required owner | Why it matters |
|---|---|---|
| Mixed AST-like tree owner | Pergyra record/class/tagged-node owner plus traversal parity | The AST-text bridge now has typed line nodes with parent edges and coarse kind rows. `program_emit` routes program-level declarations through owner-owned kind predicates, declaration collectors consume typed nodes for global env/prototype/struct/enum prepasses, `function_emit` consumes them for function header/parameter/return/body-marker reads, `stmt_emit` consumes them for statement body reads, and parameter mode facts survive into codegen. This blocker remains active because `CodegenAstTextNode.text` is still a line-text payload; it closes only when owned typed/tagged AST data replaces line-text semantics. |
| Stable JSON parse/emit owner | schema-aware JSON reader/writer with diagnostics | Read primitives plus string/field/object/array emission are shared, all current self-hosted report schemas consume the object/array writer, AIR/module validators consume schema or top-level field checks through `lib/json.pgy`, `module_manifest_resolver` consumes top-level array-object row bounds, and `mir_lower/decl_lower` consumes MIR declaration row/object facts through `json_fact_read.pgy` instead of delimiter guessing. This blocker remains active because the owner is still a bounded schema scanner rather than a complete JSON DOM/fact table. |
| Subprocess runner | `src/self_hosted/compiler/subprocess_runner_owner.pgy` | The capability envelope now names executable path, argv, cwd, env allowlist, timeout, stdout/stderr, and exit code facts. `backend_output_comparator` now records the `oracle_compare` stream/exit facts by consuming this owner. It remains active until a Pergyra runner executes against that envelope instead of shell-only logic. |
| Symbol/mangle owner | `src/self_hosted/compiler/symbol_table_owner.pgy` | The self-host C subset now consumes the compiler-world spelling row owner directly and fail-closes unless the symbol row envelope is ready. This remains active until native C, LLVM, and self-hosted projections all consume the same concrete row table. |
| Cross-backend ABI/layout row projection | `src/self_hosted/compiler/abi_layout_row_owner.pgy` plus current C-subset `abi_layout_owner.pgy` | The self-host C subset has one type-spelling owner and now fail-closes unless the compiler-world ABI/layout row envelope is ready. The compiler world owns row facts for field order, tag, niche, ownership shape, target ABI, size/align, and materialization policy. It remains active until native C/LLVM and self-hosted consumers read the same concrete rows. |
| AIR evidence zone | `src/self_hosted/compiler/air_evidence_owner.pgy`, `AirEvidenceZone` | `PgyCompilerWorld` now owns the hard-rung evidence vocabulary for intent/effect/authority/coordination/slot/materialization/loss. It remains active until hard rungs consume live AIR evidence rows rather than the vocabulary envelope. |
| Artifact Zone evidence | `src/self_hosted/compiler/artifact_zone_owner.pgy`, `ArtifactZone` | The comparable artifact vocabulary now covers diagnostics, IR JSON, ABI/layout, emitted C, emitted LLVM, emitted self-hosted output, and run output. `backend_output_comparator` now records `run_output` by consuming this owner. It remains active until all parity artifacts are written and compared from this owner. |
| Test harness substrate | `src/self_hosted/compiler/test_harness_owner.pgy`, `TestHarnessZone` | Fixture and result row vocabulary is now Pergyra-owned. `backend_output_comparator` now records C/LLVM projections by consuming this owner. It remains active until the shell parity scripts are projections of these records instead of the primary harness owner. |

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
3. Implement the subprocess runner against the capability envelope; do not add
   unrestricted shell escape.
4. Repoint C, LLVM, and self-hosted symbol/mangle and ABI/layout consumers to
   the compiler-world row owners before widening backend parity.
5. Repoint AIR evidence, Artifact Zone, and TestHarness consumers from shell
   scripts into the `PgyCompilerWorld` zones before claiming three-way compiler
   self-proof.

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

## Nominal Record Array Scope

The `record_array_basic` backend-compare fixture closes the first
`Array<NominalRecord>` parity seam. The LLVM path now records the nominal
element name in the array registry and uses that fact for indexed member access
such as `rows[0].field`; it does not re-infer the element from the AST payload.
Nominal arrays use raw byte-array runtime exports for the currently supported
mutation surface.

This is intentionally not a claim that every collection algorithm accepts
nominal records. `ArrayMap`, `ArrayFilter`, `ArraySort`, slicing, and broader
generic collection algorithms remain primitive/scalar-owned until their ABI and
runtime facts have explicit owners and parity fixtures.
