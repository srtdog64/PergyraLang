# Pergyra TODO (Beta Closure)

English anchor for tooling/doc gates:

- Self-host work begins after BETA closure and final dogfood.
- Self-host handoff docs live in `docs/self_hosted/README.md`; future agents should start there only after reading the beta source-of-truth docs.
- Self-host is not a beta blocker; partial self-host is the recommended first trajectory.
- First dogfood targets are compiler-adjacent tools: diagnostic catalog checker, AIR graph JSON validator, MIR dump diff tool, C/LLVM backend output comparator, and module/package resolver helper.
- full self-host remains a long-term proof target, not a current capability.
- The slot model removes the Rust-style lifetime annotation tax from the self-host path.
- Missing for full self-host: stable module/package resolver, richer stdlib, debugger/bootstrap story, and Stable C escape hatch policy.
- DWARF/CodeView debug information is a post-BETA but pre-self-host requirement:
  current `pgy debug` is an AST-walking source debugger only. Before serious
  Pergyra-debug-Pergyra work, LLVM DIBuilder Phase 1 must wire
  compile-unit/file/line/function/variable-scope metadata and prove it through
  a GDB/LLDB smoke. Do not market the current debugger as binary debugging.
- Generic monomorphization explosion is a measurement risk, not a beta blocker:
  after dogfood, record compile time and binary size for generic-heavy examples
  before considering dictionary passing or selective monomorphization.
- WebGL dogfood is not core language surface. The beta path is `Pergyra -> C backend --emit-c -> optional Emscripten/WebGL bridge`; it validates host bridge viability only. `pgy.render.webgl`, richer render APIs, native LLVM wasm, and GPU/Spray work stay module ecosystem tracks after beta closure.

External review risk intake, 2026-05-08:

- Build/toolchain portability is a beta trust item, not a language feature:
  Unix-style `make` remains the source of truth, but Windows users need an
  explicit MSYS2/MinGW or WSL path. Add a preflight target that checks `gcc`,
  `bash`, `make`/`mingw32-make`, optional `llvm-config`, and optional assembler
  tools before compiling. Failure should be a clear diagnostic, not a partial
  build error. Support matrix stays Linux C+LLVM, Windows C-first with LLVM
  only when executable toolchain evidence exists.
- Release/debug hygiene needs a gate:
  `PGY_DEBUG_LLVM_DETAIL`, `PGY_DEBUG_LLVM_STAGE`, and similar developer trace
  flags must be off by default in release/profiled builds. Add a smoke that
  rejects unconditional debug tracing in hot production code paths and documents
  the supported environment-variable debug surface.
- Memory/string safety audit remains open:
  scan `memcpy`, `strncpy`, `strncat`, manual buffer writes, and fixed-size
  formatting paths for explicit capacity proofs. This is not a claim that every
  use is unsafe; it is a required beta audit bucket because compiler/runtime C
  code must make bounds reasoning visible.
- MIR-missing diagnostics are intentional hard gates:
  messages such as `MIR-only LLVM path missing routine` are acceptable only as
  structured backend errors proving no silent fallback occurred. They must stay
  smoke-gated, use actionable `Reason:` / `Fix:` text, and never represent
  successful partial code generation.
- Security/runtime portability needs narrower claims:
  Slot/SecureSlot token, HMAC/AES, generation, and authority checks must be
  described as implemented runtime validation plus targeted smoke coverage, not
  as a fully audited cryptographic platform. Add Linux/Windows smoke coverage
  for secure-slot invalid token, released slot, generation mismatch, and
  authority rejection; macOS remains support-matrix work until CI evidence is
  present.
- Documentation/implementation drift must be treated as a failing gate:
  README/status/checklist wording must distinguish stable beta surface,
  explicit reject, experimental, and beta-out-of-scope. Quantum/QubitSlot stays
  experimental/v2; WASM/WebGL stays module/dogfood bridge, not core language
  surface; debugger remains source-level until DWARF/CodeView work lands.
- Ownership failure coverage remains a beta blocker:
  anchored own/ref, Slot/Pin cleanup, channel/world transfer, and authority
  boundary failures need regression coverage for both static rejection and
  runtime hard-fail/queryable failure cases. Do not market this as a Rust
  borrow checker; it is layered static boundary verification plus runtime
  handle validation.

Progress log, 2026-05-08:

- Hardened semantic symbol-table storage:
  symbol constructors now reject null names and allocation failures instead of
  leaking unnamed symbols into the scope index; paired slot/token names fail
  atomically on duplication failure; hash-index lookup now guards the
  power-of-two capacity assumption and falls back to linear lookup on malformed
  index shape. `symbol_table.h` comments were normalized to ASCII to keep the
  UTF/source hygiene gate quiet. Gates: `test-datastructures`,
  `test-semantic` (2500/0), `git diff --check` (CRLF warnings only).
- Hardened runtime intent-trace append growth:
  inline and exported trace append helpers now guard `old_len + add_len + 1`
  before `realloc`/`memcpy`, so trace/history observability cannot wrap the
  allocation size on extreme input. Gates: `test-abi`,
  `runtime-abi-lifetime-test-smoke`.
- Hardened LLVM string-rendering capacity proofs:
  generic type-name rendering and nested projection source-path rendering now
  guard `strlen` addition before scratch-arena allocation, making the remaining
  LLVM name/path concat sites explicit about overflow behavior. Gate:
  `LLVM_ENABLED=1 bin/pgy.exe`.
- Hardened semantic/core string-name construction:
  class method mangling, Slot/ReadView/WriteView/MoveToken type-name
  construction, and path extension replacement now guard null inputs and
  `strlen` addition before allocating/copying. Gate: `test-semantic` (2500/0).
- Consolidated LLVM member-call method-name construction:
  repeated `Class_Method` name assembly now goes through one helper with
  null/overflow/allocation diagnostics instead of five local `strlen` sum sites.
  Gate: `LLVM_ENABLED=1 bin/pgy.exe`.
- Closed the remaining LLVM name-render overflow audit hits:
  operator-overload lookup names and statement type-argument rendering now
  check scratch string growth before allocation/copying. Gate:
  `LLVM_ENABLED=1 bin/pgy.exe`.
- Strengthened the memory/string safety smoke:
  `memory-string-safety-test-smoke` now gates the concrete overflow-proof terms
  for intent trace append, LLVM type/projection/member/operator/type-arg
  rendering, Slot view type-name rendering, and path extension replacement.
  Gate: `memory-string-safety-test-smoke`.
- Moved AIR authority-evidence completeness lookup behind the evidence owner:
  `air_verify.c` no longer owns authoritative-evidence selection or scans
  participant-specific evidence directly. The verifier now asks
  `air_boundary_missing_authority_evidence(...)`, implemented in
  `air_validate_evidence.c`, and only formats the resulting drift diagnostic.
  `air-drift-test-smoke` rejects reintroducing direct
  `air_evidence_inventory_is_authoritative(...)` or
  `air_boundary_has_evidence_kind_subject(...)` reads in the verifier. Gates:
  `test-air` (87/0), `air-drift-test-smoke`, `perf-contract-test-smoke`.
- Moved AIR boundary evidence lookup out of the verifier:
  `air_boundary_has_evidence(...)` now lives in `air_validate_evidence.c`, so
  legacy summary fallback remains inside the evidence owner instead of being
  duplicated in the global verifier. `air_verify.c` consumes the same accessor
  as dumps/driver diagnostics, and the smoke gate rejects direct legacy summary
  flag reads from verifier code. Gates: `test-air` (87/0),
  `air-drift-test-smoke`, `perf-contract-test-smoke`.
- Centralized AIR step AST provenance:
  AIR synthesis now uses `air_step_provenance_ast(...)` for intent, zone
  boundary, and world boundary source spans instead of repeating the
  `step->ast` / owner-AST fallback expression at each assignment site. The
  drift smoke gates the named seam, keeping parsed-source diagnostics and AIR
  JSON source locations on one provenance rule. Gates: `test-air` (87/0),
  `air-drift-test-smoke`.
- Hardened AIR synthesis sizing:
  intent/boundary pre-counting and boundary authority-name array allocation now
  reject `size_t` overflow before `calloc`, returning structured AIR synthesis
  diagnostics instead of relying on wrapped allocation sizes for malformed DIR
  fixtures. The drift smoke gates the shared `air_count_add(...)` seam and the
  boundary overflow diagnostic. Gates: `test-air` (87/0),
  `air-drift-test-smoke`.
- Hardened DIR intent collection ownership/capacity:
  intent participant/step/name appenders now guard geometric capacity growth
  before `realloc`, and the collector now releases the current in-flight
  intent-step name arrays when edge insertion fails before the step is appended
  into `DIRIntentInfo`. This removes a small malformed-input leak path in the
  DIR -> AIR/DAG input seam. Gates: `test-air` (87/0),
  `type-resolution-dag-test-smoke`.
- Hardened DIR graph storage capacity:
  `DIRNode`, `DIREdge`, and DIR-owned-name array growth now share explicit
  overflow checks before `realloc`, so oversized or malformed declaration graph
  inputs fail at the DIR owner instead of wrapping capacity. Gates:
  `test-air` (87/0), `type-resolution-dag-test-smoke`.
- Hardened HIR top-level inventory capacity:
  HIR top-level AST lists, item inventory, callee-id sets, and routine-name
  index construction now guard null/malformed arrays and capacity overflow
  before allocation. This keeps HIR classification and direct-call reachability
  from becoming a pre-CFG wrap/crash seam. Gates: `test-air` (87/0),
  `test-mir` (60/0), `type-resolution-dag-test-smoke`.
- Hardened MIR base helper capacity/name construction:
  MIR instruction/name/block/routine append helpers now guard geometric
  capacity growth before `realloc`, copy helpers reject null or oversized
  index/version arrays, `mir_add_def_instruction(...)` no longer advances the
  routine instruction counter on failed insertion, and
  `mir_make_versioned_name(...)` now uses sized formatting instead of a fixed
  128-byte buffer. This removes a silent SSA-name truncation path and keeps
  malformed/huge MIR fixtures as explicit allocation failures. Gates:
  `test-mir` (60/0),
  `cfg-body-dataflow-test-smoke`.
- Tightened LLVM role method body emission:
  role impl method bodies now consume the same `LLVMHostedMethodView` /
  `MIRDeclMethod.routine_index` path as class/enum/domain host methods instead
  of doing AST/name-based MIR routine rediscovery. Ability vtable/operator
  bridge generation still reads role impl syntax because that surface is not
  routine-body inventory, and `mir-declaration-inventory-test-smoke` now gates
  the role body path against regression.
- Tightened LLVM role method forward declarations:
  role method prototypes now use `MIRDeclMethod` name/signature metadata first
  while preserving the existing `i8* self` role ABI. Direct AST
  `param_count`/`return_type` reads are now smoke-rejected in the role forward
  helper just like the domain method forward helper; the remaining role impl
  AST reads are isolated to operator/vtable bridge surfaces.
- Removed the dead LLVM AST/name routine-lookup helper:
  `llvm_find_mir_method_routine_local(...)` is gone from the public LLVM
  internal API and implementation. LLVM hosted method bodies now have one
  allowed routine path: `LLVMHostedMethodView` -> `MIRDeclMethod` ->
  `llvm_mir_decl_method_routine(...)`; the declaration-inventory smoke rejects
  reintroducing the old compatibility helper under `src/codegen`.
- Narrowed LLVM ability/operator signature reads:
  ability vtable construction and role operator bridge construction still own
  syntax-level ability/impl traversal, but their method name/param/return
  access now goes through the same local method accessors used by
  `MIRDeclMethod`-first hosted method forwarding. The smoke gate now rejects
  direct AST `param_count` / `return_type` reads in those bridge bodies.
- Hardened MIR declaration-header construction:
  declaration-header array growth and hosted-method metadata allocation now
  guard `size_t` capacity/multiplication overflow before `realloc` / `calloc`.
  Role impl method counting also fails explicitly on overflow instead of
  wrapping the flattened `MIRDeclMethod` count. Gate:
  `mir-declaration-inventory-test-smoke`.
- Narrowed LLVM role bridge method-name reads:
  role operator bridge and vtable initialization still traverse role impl
  syntax, but method names now pass through `llvm_role_method_name_from_ast(...)`
  instead of open-coded `method->data.func_decl.name` reads in each bridge
  site. Gate: `mir-declaration-inventory-test-smoke`.
- Tightened AIR evidence ownership one more step:
  `air-drift-test-smoke` now rejects direct reads of the legacy
  `AIRBoundaryNode.has_*_evidence` summary booleans outside the AIR evidence /
  validation / verification owners. New compiler/backend consumers must use
  `air_boundary_has_evidence(...)` or EvidenceNode inventory, so legacy summary
  flags cannot become a second source of truth again. Gate:
  `air-drift-test-smoke` (`test-air` 87/0).
- Narrowed AIR verification to the shared evidence accessor:
  `air_verify(...)` no longer carries a local
  `air_boundary_has_authoritative_evidence(...)` compatibility helper. RIR
  boundary, HIR routine/CFG, authority, and MIR pin-cleanup checks now consume
  `air_boundary_has_evidence(...)` / subject-specific EvidenceNode lookup
  directly. The drift smoke rejects reintroducing the removed helper and also
  restricts legacy summary flag reads in `air_verify.c` to the shared accessor
  body, so verifier checks cannot bypass the EvidenceNode seam again. Gate:
  `air-drift-test-smoke` (`test-air` 87/0).
- Hardened CFG/MIR contract validation against malformed MIR:
  `mir_validate_cfg_contract_state(...)` now rejects a block that reports a
  nonzero instruction count with a null instruction inventory before walking
  instructions. This turns a potential validator crash into an explicit MIR
  contract diagnostic and is gated by `cfg-body-dataflow-test-smoke`.
- Extended the same malformed-MIR boundary to cleanup/codegen consumers:
  cleanup fact helpers, pin cleanup fact helpers, `mir_destroy()`,
  intent-observability usage scanning, thread-pool usage scanning, and the C
  MIR emission contract now avoid dereferencing a null instruction inventory
  when `instruction_count > 0`. This keeps helper/codegen ordering from
  reintroducing a pre-validator crash path. Gates:
  `cfg-body-dataflow-test-smoke`, `test-mir`, `perf-contract-test-smoke`.
- Extended the malformed-MIR guard to MIR DCE:
  `mir_run_dce_on_routine(...)` now refuses to scan a block that reports a
  nonzero instruction count without an instruction inventory. DCE no longer
  becomes a pre-validator crash path for hand-built or corrupted MIR fixtures.
  Gate: `cfg-body-dataflow-test-smoke`.
- Extended the malformed-MIR guard to MIR fact validation:
  statement-inventory validation, instruction surface-usage validation, and
  terminator provenance validation now share an explicit instruction-inventory
  shape check before walking block instructions. This keeps fact validation
  independently safe even when called outside the usual CFG-contract ordering.
  Gate: `cfg-body-dataflow-test-smoke`.
- Extended the malformed-MIR guard to MIR liveness analysis:
  liveness def/use collection, successor phi/live-in reads, value-summary
  construction, and use validation now refuse nonzero instruction counts with a
  null instruction inventory. The analysis/DCE path now shares the same
  malformed-CFG boundary as validation and codegen. Gate:
  `cfg-body-dataflow-test-smoke`.
- Extended MIR inventory-shape validation to routine-level arrays:
  `mir_validate(...)` now rejects a nonzero routine count without a routine
  inventory, nonzero block count without block inventory, and nonzero
  value-summary count without value-summary inventory before later CFG/liveness
  consumers walk those arrays. `mir_destroy(...)` and `mir_dump(...)` also
  tolerate these malformed shapes without dereferencing null inventories. Gates:
  `test-mir` (60/0), `cfg-body-dataflow-test-smoke`,
  `test-inc-size-test-smoke`, and `build-source-inventory-test-smoke`.
- Hardened MIR dump for malformed blocks:
  `mir_dump(...)` now prints an explicit invalid-inventory line and skips the
  instruction walk when a block reports instructions without storage. Failed
  validation can therefore still be inspected without the debug dump becoming
  another crash path. Gate: `cfg-body-dataflow-test-smoke`.
- Split MIR lifecycle/dump/names out of the public-surface implementation
  header: `mir_destroy(...)` and `mir_dump(...)` now live in
  `src/compiler/mir_lifecycle.c`, while MIR name rendering lives in
  `src/compiler/mir_names.c`; `mir_public_surface.h` dropped from the 600 LOC
  gate edge to 267 LOC. The Makefile's full compiler source inventory and MIR
  test core object list both include the new owners, and dump/name-oriented
  smoke assertions now point at the lifecycle/name owners instead of the public
  surface header. Gates: `test-mir` (60/0),
  `cfg-body-dataflow-test-smoke`, `test-inc-size-test-smoke`,
  `build-source-inventory-test-smoke`, and `perf-contract-test-smoke`.
- Narrowed LLVM hosted-method routine lookup:
  LLVM now mirrors the C backend's declaration-inventory seam with
  `llvm_mir_decl_method_routine(...)` and
  `llvm_hosted_method_view_routine(...)`. Domain method emission, domain method
  local lookup, and class-method pipeline emission no longer read
  `MIRDeclMethod.has_routine` / `routine_index` directly; that metadata is
  owned by the hosted-method view helper. Gate:
  `mir-declaration-inventory-test-smoke`.
- Revalidated DAG source-of-truth gates after the AIR/CFG pass:
  retired resolver calls remain `0`, resolver body fallbacks remain `0`,
  metadata dead-ends remain `0`, and materializer unresolved remains `0`;
  current metadata inventory is `metadata_entries=3498`, `metadata_owned=258`,
  and `metadata_hits=8380`. Gates:
  `type-resolution-resolver-inventory-test-smoke` and
  `type-resolution-dag-test-smoke`.
- Revalidated DAG gates after the CFG/MIR source-fact tightening:
  `type-resolution-dag-test-smoke` still reports
  `retired_resolver_calls=0`, `retired_resolver_body_fallbacks=0`,
  `metadata_dead_ends=0`, `materializer_unresolved=0`,
  `metadata_entries=3498`, `metadata_owned=258`, and
  `metadata_hits=8380`; resolver inventory remains `fallback seams=0` and
  `type-ref helper refs=2`.
- Split DAG generic-contract evidence from compat materialization counters:
  `SemanticResult.type_resolution_dag_generic_contract_evidence_count` now
  comes from a DAG-named context counter recorded while staging generic
  defaults, constraints, and where-bounds. The old
  `type_resolution_stage_compat_generic_contract_count` remains a compatibility
  stats/debt counter and stays `0`, so AIR can consume DAG evidence without
  making stage-materialize fallback accounting look active. The DAG smoke now
  prints and gates `dag_generic_contract_evidence=165` and
  `dag_ability_consumer_evidence=71` separately from
  `stage_materialize_non_alias=0`. Gates: `perf-contract-test-smoke`,
  `type-resolution-dag-test-smoke`, and
  `type-resolution-resolver-inventory-test-smoke`.
- Moved AIR legacy summary evidence-shape validation behind the evidence owner:
  `air_validate(...)` no longer reads `AIRBoundaryNode.has_*_evidence` summary
  booleans directly. Boundary evidence provenance/shape checks now live in
  `air_validate_boundary_legacy_evidence_shape(...)` inside
  `src/compiler/air_validate_evidence.c`, and `air-drift-test-smoke` rejects
  summary-flag reads escaping the evidence / synthesis / verification owners.
  This keeps AIR validation closer to the first-class `AIREvidenceNode` source
  of truth while preserving compatibility summaries for dumps and driver
  diagnostics. Gates: `test-air` (87/0), `air-drift-test-smoke`, and
  `perf-contract-test-smoke`.
- Tightened the C backend MIR source-statement emit seam:
  `transpiler_mir_def_uses_source_statement_emit(...)` now requires both the
  preserved AST payload and the MIR `source_ast_type` fact to match the expected
  statement kind before source-compatible emission is allowed. This keeps a
  stale or incorrectly attached AST payload from driving C backend emission
  without the matching MIR fact. Gates: `test-transpile` (745/0),
  `cfg-body-dataflow-test-smoke`, and `perf-contract-test-smoke`.
- Tightened the LLVM MIR source-compatible emit seam:
  source-branch emission now requires an attached source AST payload with a MIR
  source-location fact, and source-local-decl emission additionally requires
  `source_ast_type == AST_LET_DECL`. This keeps LLVM aligned with the fact-first
  MIR emission contract without reintroducing direct `inst->ast->type`
  inspection. Local gate: `perf-contract-test-smoke`,
  `cfg-body-dataflow-test-smoke`; local LLVM compile was not run because no
  `llvm-config` toolchain is installed in the current workspace shell.
- Tightened MIR source-branch fact validation:
  source-compatible branch emission now validates the branch shape against the
  recorded MIR `source_ast_type` fact (`AST_MATCH_CASE` for match-case branches,
  `AST_BLOCK` for select-dispatch branches) before codegen can consume the AST
  payload. This moves another source-compatibility guard from backend convention
  into MIR validation. Gates: `test-mir` (60/0),
  `cfg-body-dataflow-test-smoke`, and `perf-contract-test-smoke`.
- Tightened C backend residual resource suppression:
  `transpiler_mir_stmt_is_mirrored_resource(...)` now requires a MIR
  source-location fact and matching `source_ast_type` before using the preserved
  AST pointer to suppress a residual source statement. Mirrored resource
  elision therefore depends on MIR facts first and pointer identity second,
  rather than on AST identity alone. Gates: `test-transpile` (745/0) and
  `perf-contract-test-smoke`.
- Tightened C backend source-branch condition rendering:
  match-case and select-dispatch branch rendering now require the
  `requires_source_branch_emit` fact plus matching `source_ast_type`
  (`AST_MATCH_CASE` / `AST_BLOCK`) before consuming the preserved AST payload.
  This keeps C backend branch lowering aligned with the MIR validator's
  source-branch fact contract. Gates: `test-transpile` (745/0) and
  `perf-contract-test-smoke`.
- Tightened the C MIR emission contract around source branches:
  `transpiler_validate_mir_emission_block_shape(...)` now rejects
  source-branch emission when the preserved AST payload is missing or the
  recorded `source_ast_type` does not match the branch shape. C backend
  source-compatible branch lowering is therefore guarded by the same MIR fact
  shape checked by the validator and renderer. Gates: `test-transpile` (745/0),
  `cfg-body-dataflow-test-smoke`, and `perf-contract-test-smoke`.
- Removed the last CFG-container AST fallback from MIR block emission:
  LLVM no longer falls back to `llvm_mir_stmt_is_cfg_container(inst->ast)` when
  source-location facts are missing, and the C backend now skips CFG-owned
  source statements only through `transpiler_mir_inst_is_cfg_container(...)`,
  which requires matching `source_ast_type` facts. This keeps CFG-owned
  statement suppression fact-driven instead of AST-shape rediscovery. Gates:
  `test-transpile` (745/0) and `perf-contract-test-smoke`.
- Locked the source-branch drift regression:
  the MIR negative test for source-compatible branches now covers mismatched
  `source_ast_type` in addition to missing source-branch facts and missing AST
  payloads. The C CFG policy helper also now accepts `const ASTNode *`, removing
  a const-cast introduced by the fact-first CFG-container guard. Gates:
  `test-mir` (60/0), `test-transpile` (745/0),
  `cfg-body-dataflow-test-smoke`, and `perf-contract-test-smoke`.
- Reduced intent owner declaration drift:
  intent action-contract, contract-summary, on-inference, and shared helper
  owners now consume intent helper prototypes from the existing internal
  headers instead of carrying local forward declarations for
  `intent_involves_type_name(...)` / subject-role ability lookup. This keeps
  compressed-intent inference work on one declared semantic seam. Gate:
  `test-semantic`.
- Extended malformed-MIR guarding into LLVM emission:
  `llvm_mir_validate_cleanup_contract(...)` and
  `llvm_emit_mir_block_with_exprs(...)` now reject a non-empty MIR block with a
  null instruction inventory before cleanup validation or instruction emission.
  This matches the C MIR emission contract and prevents LLVM from becoming a
  post-validator crash path. Gates: `cfg-body-dataflow-test-smoke` and
  `llvm-test-smoke`.
- Added a CI-oriented build preflight:
  `check-build-tools` now verifies a runnable `bash`, a working `CC`, and an
  LLVM installation when `LLVM_ENABLED` is active before CI enters the long
  build/test sequence. `nasm` remains optional and reports a non-fatal
  fast-path-disabled note. The Linux/macOS/Windows CI targets call this
  preflight with their platform compiler settings, making missing toolchain
  failures explicit instead of surfacing as partial compile/link errors. Gates:
  `check-build-tools LLVM_ENABLED=0`, `check-windows-toolchain`.
- Added release/debug hygiene smoke coverage:
  `debug-hygiene-test-smoke` rejects production sources or build flags that
  default `PGY_DEBUG` / developer trace env vars on, while preserving explicit
  opt-in debug surfaces such as `PGY_DEBUG_LLVM_STAGE`,
  `PGY_DEBUG_LLVM_DETAIL`, `PGY_DEBUG_LLVM_VERIFY`,
  `PGY_DEBUG_PIPELINE_STAGE`, and `PGY_DEBUG_MIR_LOWER`. Linux/macOS/Windows
  CI now runs the gate near the documentation/status checks. Gate:
  `debug-hygiene-test-smoke`.
- Added a first memory/string safety regression gate:
  `memory-string-safety-test-smoke` rejects unbounded production C string APIs
  (`sprintf`, `vsprintf`, `strcpy`, `strcat`, `strncat`, `gets`) outside test
  fixtures and keeps the historical buffer-overflow regression coverage
  discoverable. The remaining production `strncat` users were moved to the
  shared `pergyra_str_append(...)` bounded append helper. This does not claim
  the full `memcpy`/`memmove` audit is closed; it prevents the highest-risk
  string API class from re-entering production code while the capacity-proof
  audit continues. Gate: `memory-string-safety-test-smoke`.
- Rechecked the MIR-missing diagnostic gate:
  `mir-declaration-inventory-test-smoke` already requires LLVM MIR inventory
  misses to route through `llvm_set_mir_inventory_missing(...)` /
  `llvm_set_mir_topology_invalid(...)` / `llvm_set_mir_intent_carrier_missing`
  with stable diagnostic code/cause/fix fields. The external-review concern is
  therefore gated as a fail-closed backend diagnostic path, not a silent
  fallback path. Gate: `mir-declaration-inventory-test-smoke`.
- Added a security-test dependency preflight:
  `check-security-toolchain` now probes `openssl/evp.h` plus linkable
  `-lssl -lcrypto` before `test-security` starts compiling runtime objects.
  Minimal C-only toolchains now fail with an explicit OpenSSL development
  dependency diagnostic instead of a late header/linker error. Local result in
  the current Windows/Git Bash environment: preflight correctly reports missing
  OpenSSL development headers/libraries.
- Added a security portability/claim-scope contract gate:
  `docs/03_security_mode_design.md` now states that current security is runtime
  validation plus targeted regression evidence, not a completed third-party
  cryptographic audit, and ties platform claims to executable CI evidence.
  `security-portability-contract-test-smoke` keeps that wording connected to
  the OpenSSL preflight and TODO risk intake.
- Removed production `strncat` usage:
  parser diagnostics, effect/match diagnostic list rendering, MIR type-name
  rendering, REPL block accumulation, and world-roster visualization now use
  the shared `pergyra_str_append(...)` bounded append helper instead of repeated
  `strlen`/remaining-capacity `strncat` calls. The memory/string safety smoke
  now rejects `strncat` in production code as well. Gates:
  `memory-string-safety-test-smoke`, `test-parser`, `test-mir`, and a direct
  compile probe for `src/runtime/world_roster.c`.
- Removed the first `snprintf` stack-buffer overread pattern:
  runtime scalar/map integer formatting now uses `pergyra_strdup_printf(...)`
  instead of copying `(size_t)len + 1` bytes from a fixed `stack_buf` after
  `snprintf`. The shared string helper also owns bounded copy/append and exact
  formatted duplication. The memory/string safety smoke now rejects this
  truncated-stack-copy pattern in production code. Gates:
  `memory-string-safety-test-smoke`, `tooling-conformance-test-smoke`,
  `test-transpile`.
- Removed production `strncpy` usage:
  C backend symbol tables, generic specialization binding names, and parallel
  capture name buffers now use `pergyra_str_copy(...)`. The memory/string
  safety smoke rejects `strncpy` as well as the unbounded C string APIs, so new
  fixed-buffer copies must go through project-owned bounded helpers. Gates:
  `memory-string-safety-test-smoke`, `test-transpile`.
- Tightened LLVM runtime cache dependency freshness:
  runtime headers now depend on the shared `common/string_compat.h` helper and
  several inline runtime split headers. `compiler_runtime_cache.c` now includes
  those common/inline dependencies in the freshness list so prebuilt runtime
  objects cannot stay stale after helper/runtime-header edits. Gate:
  `runtime-panic-contract-test-smoke`.
- Closed the raw `snprintf` offset-accumulator class in production code:
  tuple specialization names, tuple literal type names, tuple C type names,
  effect-conflict diagnostic text, and function-signature diagnostic text now
  use `pergyra_str_append(...)` / `pergyra_str_appendf(...)` instead of
  `off += snprintf(...)`. This prevents truncated writes from advancing an
  offset past the end of a fixed buffer and underflowing later capacity checks.
  `memory-string-safety-test-smoke` now rejects new production raw offset
  accumulation, and `build-source-inventory-test-smoke` rejects typo-like C
  tokens such as `retun` / `stncmp` in production source. Gates:
  `memory-string-safety-test-smoke`, `build-source-inventory-test-smoke`,
  `test-semantic`, `test-transpile`, and a direct runtime-lib compile probe.
- Tightened AIR authority drift diagnostics at the driver boundary:
  strict AIR already rejects partial authority evidence, and the driver now
  reports the expected authority participant list even when one participant's
  RIR authority evidence exists and another is missing. This keeps CLI/JSON
  diagnostics aligned with the first-class `AIREvidenceNode` source of truth.
  Gate: `air-drift-test-smoke`.
- Fixed diagnostic layer routing for assignability errors:
  `semantic:assignability_check` is routed to the `type` layer before domain
  substring matching, preventing the embedded `ability` suffix from
  misclassifying type mismatch diagnostics as domain failures. Gates:
  `diagnostics-json-test-smoke` and
  `layered-diagnostics-contract-test-smoke`.
- Tightened LLVM collection/channel missing-runtime diagnostics:
  missing collection/channel runtime exports now use
  `inspect-mir-inventory` rather than `annotate-concrete-type`, because the
  frontend type is already concrete and the failure is backend runtime
  inventory closure. Gate: `diagnostics-json-test-smoke`.
- Tightened MIR non-CFG fallback accounting:
  `used_non_cfg_body_fallback` is now set only when a fallback STMT is actually
  emitted, and MIR validation rejects a fallback flag with zero fallback count.
  This keeps CFG/body safety debt metrics factual instead of counting a
  compatibility helper call as fallback usage. Gates: `test-mir`,
  `cfg-body-dataflow-test-smoke`, and `perf-contract-test-smoke`.
- Reclosed owner-size gates after the AIR/MIR test growth:
  oversized AIR cleanup transfer tests were split by boundary/source
  responsibility, and the LLVM collection/call expression owners were narrowed
  by moving collection requirement checks and call diagnostic recovery into
  dedicated owners. Gates: `test-inc-size-test-smoke`,
  `build-source-inventory-test-smoke`, `test-air`, `air-drift-test-smoke`, and
  `llvm-test-smoke`.
- Revalidated DAG source-of-truth gates:
  `type-resolution-dag-test-smoke` reports `retired_resolver_calls=0`,
  `retired_resolver_body_fallbacks=0`, `metadata_dead_ends=0`,
  `materializer_unresolved=0`, `metadata_entries=3498`, `metadata_owned=258`,
  and `metadata_hits=8380`; `type-resolution-resolver-inventory-test-smoke`
  keeps fallback seams at `0` and type-ref helper refs at `2`.
- Tightened intent observability surface facts:
  MIR/AST surface usage now checks the exact stable observability builtin list
  instead of treating every `Intent*` call as observability. This preserves the
  no-trace fast path for user-defined intent-prefixed functions while keeping
  builtin `IntentLast`/`IntentHistory`/`IntentActive`/`IntentRecent` calls
  trace-enabled. The perf contract now also compares common/codegen/semantic/
  resolver/LLVM observability name sets and codegen/semantic resolver bsearch
  ordering to prevent future table drift. MIR intent inventory carriers
  (`IntentStep`, `IntentWho`, `IntentDispatch`, etc.) are now classified through
  an exact sorted `mir_instruction_is_intent_semantic_carrier(...)` table instead
  of sharing the observability prefix rule or growing another comparison chain.
  Parser builtin-like expression names (`ClaimSlot`, `Read`, `Write`, etc.)
  now use sorted dispatch tables as well, with smoke-gated ordering so bsearch
  tables cannot silently drift.
  AIR evidence-kind classification is now centralized behind
  `air_evidence_kind_is_known(...)` and
  `air_evidence_kind_is_boundary_scoped(...)`; global-evidence validation and
  evidence inventory validation consume those helpers instead of maintaining
  separate kind lists.
  Direct regressions:
  `MIR does not treat Intent-prefixed user calls as observability` and
  `MIR DCE does not preserve user Intent-prefixed statements`. Gates:
  `test-parser`, `test-mir`, `test-transpile`,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`,
  `build-source-inventory-test-smoke`, `source-utf8-test-smoke`,
  `cfg-body-dataflow-test-smoke`, `air-drift-test-smoke`, and
  `llvm-test-smoke`.
- Tightened AIR/RIR stable IO boundary classification:
  the beta-stable IO/time boundary set (`FileOpen`, `FileRead`, `FileWrite`,
  `FileClose`, `ReadFile`, `WriteFile`, `Input`, `ReadLine`, `Now`, `Sleep`)
  now lives in `src/compiler/io_boundary_builtin.c` and is consumed by both
  AIR boundary synthesis and RIR lowering. This removes the duplicated
  `io_names[]` linear scans from `air_boundary.c` / `rir_builder_walk.c` and
  smoke-gates sorted `bsearch` ordering so the AIR/RIR boundary vocabulary
  cannot drift. The same slice tightened `codegen_slot_type_policy.c` claim-slot
  classification through a sorted spec table instead of a hand loop. Intent
  header value-binding classification now uses the same sorted-table rule in
  `parser_intent.c`, preparing the parser side for compressed intent inference
  without growing another linear surface list. Gates: `test-parser`, `test-rir`,
  `test-air`, `test-transpile`, `air-drift-test-smoke`,
  `perf-contract-test-smoke`, `build-source-inventory-test-smoke`, and
  `source-utf8-test-smoke`.
- Tightened driver diagnostic code/cause/fix mapping:
  `driver_diag.c` now uses a single `DriverDiagCodeMap` table for stage-fail
  JSON code extraction, `cause_ir`, and `fix_source` mapping instead of keeping
  parallel if-chains. This keeps parser/lexer/AIR/runtime-none driver errors on
  one user-facing diagnostic contract. Gates: `diagnostics-json-test-smoke`,
  `layered-diagnostics-contract-test-smoke`, and `perf-contract-test-smoke`.
- Tightened DAG evidence naming at the AIR boundary:
  `SemanticResult` now exposes
  `type_resolution_dag_generic_contract_evidence_count` and
  `type_resolution_dag_ability_consumer_evidence_count`; AIR consumes those
  DAG-named fields instead of the compatibility counter names. The older
  `type_resolution_stage_compat_*` fields remain as telemetry/stat-parser
  compatibility mirrors, but they no longer define AIR DAG evidence. Gates:
  `test-air`, `type-resolution-dag-test-smoke`,
  `type-resolution-resolver-inventory-test-smoke`, and
  `perf-contract-test-smoke`.
- Tightened MIR cleanup fact ownership for AIR:
  `mir_block_has_expected_cleanup_edge_fact(...)` now centralizes the
  block-index to cleanup-fact-name mapping used by MIR validation and AIR
  cleanup evidence collection. AIR no longer treats a cleanup successor alone
  as proof; the source block must also carry the expected MIR cleanup-edge
  fact payload. Synthetic AIR fixtures now model both routine-level cleanup
  and pin-unpin facts explicitly. C and LLVM MIR emission contracts also
  consume the expected-fact helper, so validator, AIR, and both backend
  contract gates share the same cleanup proof predicate. Gates: `test-mir`,
  `test-air`, `test-transpile`, `llvm-test-smoke`,
  `cfg-body-dataflow-test-smoke`, `air-drift-test-smoke`,
  `build-source-inventory-test-smoke`, and `perf-contract-test-smoke`.
- Removed the C MIR block-emission let-lookup fallback:
  `transpiler_mir_find_stmt_for_inst(...)` now consumes only the
  instruction-carried AST payload instead of reopening the function body with
  `transpiler_find_let_decl_by_name(...)`. MIR validation already requires
  source-emission instructions to carry their source AST, so C backend block
  emission no longer has a name-based AST rescan seam for that path. The helper
  signature no longer accepts `func_decl`, making the forbidden lookup seam
  impossible to call from this path. Gates: `test-transpile` and
  `cfg-body-dataflow-test-smoke`.
- Removed the remaining C MIR pending-use let-lookup fallback:
  `transpiler_materialize_pending_inst_uses(...)` now materializes pending SSA
  values only from MIR instruction-carried source facts in the current block.
  The dead `transpiler_mir_let_lookup` owner was removed from the Makefile
  source inventory, and CFG smoke now rejects reintroducing name-based
  function-body let lookup under `src/codegen`. Gates: `test-transpile`,
  `cfg-body-dataflow-test-smoke`, and `build-source-inventory-test-smoke`.

Progress log, 2026-05-04:

- Tightened AIR evidence-node duplicate handling:
  boundary-scoped evidence append is now idempotent only for exactly one
  non-fallback fact. Duplicate appends carrying fallback counts now fail
  instead of being silently ignored, keeping strict AIR's no-fallback evidence
  contract consistent across first append and duplicate append paths. Gates:
  `perf-contract-test-smoke`, `test-air`, `air-drift-test-smoke`.
  Direct regression: `AIR append rejects boundary evidence duplicate fallback`.
- Tightened LLVM member/slot/slice call error propagation:
  member-call allocation/argument/receiver failures, slot method token/runtime
  helper failures, and Slice() metadata failures now set structured diagnostics
  and propagate `NULL` instead of retuning `i32 0` as a recovery value. The
  member-call dispatcher now checks `ctx->has_error` after vtable, slot, and
  slice helper attempts so helper `NULL` means either "not handled" or
  "diagnosed failure" unambiguously. Gates: `perf-contract-test-smoke`,
  `llvm-test-smoke`, `llvm-test-backend-compare`.
- Tightened LLVM expression error propagation:
  event, log, identifier slot auto-read, assignment, channel, call-dispatch,
  member-access, projection-path, Result/Option, Rc/Weak, scalar/stdlib,
  spawn/await, direct slot read, and the top-level expression safety-net
  lowering now propagate diagnosed failures with `NULL` instead of
  materializing `i32 0`/`i1 false`/null-task-handle recovery values.
  Boolean/void success paths still retun backend ABI sentinel values where the
  expression contract requires a value. The perf contract now rejects
  regression of these error helpers back to constant recovery. Gates:
  `perf-contract-test-smoke`, `llvm-test-smoke`,
  `llvm-test-backend-compare` (`196/196` ABI pipeline, `69/69` backend
  compare).
- Tightened LLVM try-operator error propagation:
  postfix `?` no longer rebuilds an incompatible `Result<T, E>` early-retun
  value by silently inserting a null error payload. If the callee error payload
  cannot be coerced to the current function error type, LLVM lowering now emits
  `align-result-error-type` and stops. The perf contract rejects regression to
  `err_val = LLVMConstNull(dst_ty)`.
- Tightened MIR cleanup-root validation:
  registered cleanup, rollback, and invalidation roots must now be reachable
  cleanup blocks. The MIR suite corrupts a generated pin cleanup root to
  `is_reachable=false` and verifies the validator rejects it, closing another
  CFG/body topology drift path. The validator also rejects unreachable
  non-cleanup blocks that still carry cleanup/rollback/invalidation successor
  metadata, so exceptional successor facts cannot survive outside the reachable
  body graph. Gates: `test-mir`, `cfg-body-dataflow-test-smoke`, and
  `perf-contract-test-smoke`.
- Tightened AIR cleanup-root evidence consumption:
  AIR now counts MIR cleanup and pin-cleanup evidence only when the registered
  cleanup root is both a cleanup block and reachable, and only when the source
  cleanup edge comes from a reachable non-cleanup block. This matches the MIR
  validator contract instead of treating unreachable cleanup topology as proof.
  Gates:
  `test-air`, `air-drift-test-smoke`, `air-json-schema-test-smoke`,
  `air-backend-nonimpact-full-test-smoke`, and `perf-contract-test-smoke`.
  Direct regressions: `AIR ignores unreachable MIR cleanup root evidence` and
  `AIR ignores unreachable MIR cleanup source evidence`.
- Tightened AIR pin-cleanup evidence dependency:
  boundary-scoped `AIR_EVIDENCE_MIR_PIN_CLEANUP` must now have matching
  provider-level global `AIR_EVIDENCE_MIR_CLEANUP`, so a pin boundary cannot
  claim cleanup safety without the routine-level MIR cleanup proof. Gates:
  `test-air`, `air-drift-test-smoke`, and `perf-contract-test-smoke`.
  Direct regression: `AIR rejects MIR pin cleanup without global cleanup
  evidence`.
- Tightened AIR authority participant evidence completeness:
  strict AIR now requires every declared `authorized by` participant on a
  boundary to have matching `AIR_EVIDENCE_RIR_AUTHORITY`, not merely any
  authority evidence on the boundary. RIR evidence collection also no longer
  stops at the first matching authority fact/op, so multi-authority boundaries
  can carry complete evidence. Gates: `test-air`, `air-drift-test-smoke`, and
  `perf-contract-test-smoke`. Direct regressions: `AIR synthesis collects all
  RIR authority evidence` and `AIR strict evidence requires all authority
  participants`.
- Connected runtime frontier policy to AIR evidence:
  AIR now emits global `AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY` with provider
  `pgy.runtime.frontier-policy.v1` and validates its bounded-pass-limit policy
  fact count. This does not claim the full transitive frontier scheduler is
  complete; it gives AIR a first-class evidence hook for the runtime policy
  source of truth. Gates: `test-air`, `air-drift-test-smoke`,
  `air-json-schema-test-smoke`, and `perf-contract-test-smoke`. Direct
  regressions: `AIR rejects invalid runtime frontier policy provider` and
  `AIR rejects empty runtime frontier policy evidence`.
- Tightened AIR DAG drift wording:
  strict AIR DAG drift now reports unresolved metadata dead-ends instead of the
  retired materializer fallback wording. The regression keeps
  `type_resolution_metadata_materializer_fallbacks=0` while
  `type_resolution_metadata_dead_ends>0`, proving the drift is keyed to the
  current DAG evidence field. Gates: `test-air`, `air-drift-test-smoke`, and
  `perf-contract-test-smoke`.
- Tightened MIR-required LLVM type failure propagation:
  `llvm_mir_required_type_from_ast(...)` no longer retuns `i32` after
  diagnosing missing/unsupported required type metadata. MIR function emission
  and MIR local/parameter binding now stop on `ctx->has_error` or `NULL` type
  before building `LLVMFunctionType`, pointer wrappers, or allocas. Gates:
  `perf-contract-test-smoke`, `llvm-test-smoke`,
  `llvm-test-backend-compare` (`196/196` ABI pipeline, `69/69` backend
  compare).
- Tightened declaration-side LLVM parameter type failure propagation:
  `llvm_decl_required_param_type(...)` now retuns `NULL` after a required
  function parameter type diagnostic, and forward declaration/body parameter
  binding stops before constructing ABI signatures or allocas from the old
  `i32` recovery type. Gates: `perf-contract-test-smoke`, `llvm-test-smoke`,
  `llvm-test-backend-compare` (`196/196` ABI pipeline, `69/69` backend
  compare).
- Tightened LLVM registration type failure propagation:
  `llvm_register_required_ast_type(...)` now retuns `NULL` after missing
  enum/class/exten type metadata diagnostics, and enum payload/method, class
  field/method, and exten prototype registration stop before constructing
  ABI shapes from the old `i32` recovery type. Array/Slice typed-var registry
  also stops after a failed element type resolution instead of storing a
  malformed element type. Gates: `perf-contract-test-smoke`,
  `llvm-test-smoke`, `llvm-test-backend-compare` (`196/196` ABI pipeline,
  `69/69` backend compare).
- Tightened C aggregate expression lowering:
  Array/Slice indexing, array literals, and tuple literals now require concrete
  element/layout metadata before materializing C helper/type names. These
  paths now emit structured diagnostics instead of allowing `Unknown` metadata
  to become `PgyArray_Unknown`, `PgySlice_Unknown`, or an unsupported tuple
  layout at the emit boundary. Slot SSA auto-read now also requires concrete
  payload metadata before materializing `pgy_read_*` helpers, and slot
  assignment sugar applies the same guard before materializing `pgy_write_*`
  helpers. Await expressions now require concrete `Future<T>` result metadata
  before selecting the C await ABI helper. C spawn/channel lowering now rejects
  missing spawn targets, non-concrete spawn wrapper argument/retun metadata,
  and `Channel<Unknown>` payload metadata before materializing wrapper structs
  or channel runtime helper names. The shared C stdlib constructed-inner
  resolver now rejects `Array<Unknown>` / `Slice<Unknown>` /
  `Channel<Unknown>` before any builtin helper suffix is selected. Gates:
  `perf-contract-test-smoke`, `test-transpile`.
- Tightened C type-name mapping:
  constructed runtime types with `Unknown` generic arguments now preserve the
  `"Unknown"` sentinel instead of materializing names such as
  `PgyArray_Unknown`, `PgyHashMap_Unknown`, or `PgyBoxArray_Unknown`. Direct
  type-mapping regressions cover `Array<Unknown>`, whitespace-trimmed
  `Array<Unknown >`, `HashMap<String, Unknown>`, `Box<Array<Unknown>>`, and
  the non-sentinel user type `Box<Array<UnknownError>>`. Gate:
  `test-transpile`.
- Tightened C match lowering failure handling:
  match subjects inferred as missing/`Unknown` now stop before emitting a
  fallback match temporary, and Some/Ok/Err destructor bindings only emit local
  bindings when the subject carries concrete Option/Result payload metadata.
  The perf contract rejects reintroducing `subject_type`/binding `"Unknown"`
  assignments in the match emitter. The same contract now rejects the dead
  nominal host-call `"Unknown"` receiver-name fallback in C call lowering.
  Gate: `perf-contract-test-smoke`.
- Hardened LLVM domain projection value lowering:
  sync-time domain projection field/path failures now propagate structured
  diagnostics instead of inserting `i32 0` into arbitrary target fields. The
  projection sync body stops before storing a malformed projected aggregate when
  value construction fails. Gate: `perf-contract-test-smoke`.
- Tightened LLVM typed-variable registry materialization:
  type-name rendering no longer converts malformed generic metadata into the
  string `"Unknown"`, and List/Set/Queue/HashMap/Future/Channel/Rc/Weak
  registry enrollment now requires concrete rendered type names before storing
  backend metadata. The perf contract rejects reintroducing the old Unknown
  strdup fallback. The renderer now also rejects failed base-name allocation and
  malformed NULL generic parameter entries instead of creating partial generic
  names. LLVM primitive suffix mapping now retuns `NULL` for unsupported LLVM
  types instead of the `"Unknown"` sentinel. LLVM array literals also reject
  `Unknown` element metadata at the expression boundary, and the top-level LLVM
  expression emitter now null-guards its context before reading backend types.
  Gate: `perf-contract-test-smoke`.
- Hardened LLVM generic spawn specialization setup:
  parameter type-array allocation now reports a structured diagnostic and
  restores the generic substitution state instead of writing through a NULL
  scratch buffer. Gate: `perf-contract-test-smoke`.
- Closed the remaining LLVM extended collection builtin silent fallbacks:
  `ListPush`, `ListGet`, `ListSet`, `ListRemove`, `MapSet`, `MapGet`,
  `MapHas`, `MapRemove`, and `MapKeys` now route expression-lowering and
  scratch temporary failures through structured diagnostics instead of
  retuning successful zero/null defaults. The owner was kept at the 600 LOC
  production gate. Gates: `llvm-test-smoke`, targeted collection backend
  compare command exit 0, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`.
- Hardened LLVM scalar math builtins:
  `Abs`, `Min`, and `Max` now reject failed operand lowering before invoking
  LLVM builders, preventing null LLVM values from reaching `LLVMBuildNeg`,
  `LLVMBuildICmp`, or `LLVMBuildSelect`. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`. Fixture gap:
  backend-compare currently has no dedicated `Abs`/`Min`/`Max` case.
- Hardened LLVM await expression failure handling:
  missing await operands and failed task-handle lowering now report structured
  diagnostics instead of retuning a successful `i32 0` await value. Gates:
  `llvm-test-smoke`, `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Hardened LLVM intent observability argument lowering:
  observability builtins with arguments now convert a failed shared call-arg
  lowering result into an explicit diagnostic recovery value instead of
  retuning `true` with `*out == NULL`. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Removed the LLVM expression-level TaskGroup sequential fallback:
  `AST_TASK_GROUP` no longer emits its tasks sequentially in the expression
  emitter before failing. It now reports that TaskGroup must lower through the
  AIR/RIR/MIR task-group boundary path. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Fixed LLVM array literal lowering side effects:
  array literals now reuse the first lowered element for type inference instead
  of lowering element 0 twice, reject later element-lowering failures instead
  of skipping them, and diagnose array temporary allocation failure. Gates:
  `llvm-test-smoke`, targeted array/collection backend compare command exit 0,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Hardened LLVM boundary call argument materialization:
  boundary calls now reject failed slot argument lowering and missing secure
  token bindings instead of passing NULL values or null token capabilities into
  generated calls, and reject source argument count mismatches before building
  a backend call with the wrong ABI shape. The generic call dispatcher now
  stops when the boundary argument helper has already raised a diagnostic.
  Gates: `llvm-test-smoke`, targeted slot/generic-spawn backend compare command exit 0,
  full `llvm-test-backend-compare` (`196/196` ABI pipeline, `69/69` backend
  compare), `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Hardened LLVM spawn expression target validation:
  malformed spawn expressions without a target now report an explicit
  diagnostic instead of retuning a null task handle as a successful expression.
  Gates: `llvm-test-smoke`, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`.
- Fixed LLVM intent forward-declaration ABI shape:
  forward declarations for intent calls now use the same `forward_param_count`
  for parameter type materialization and `LLVMFunctionType`, avoiding a
  mismatch when explicit binding inventory is the active source. Gates:
  `llvm-test-smoke`, targeted intent backend compare command exit 0,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Closed another LLVM expression-tail silent fallback seam:
  tuple literals now diagnose scratch allocation or element-lowering failure
  instead of inserting an `i32 0` element, and lambda expressions now avoid
  null retun operands by routing body-lowering failures through structured
  diagnostics plus type-shaped zero terminators. The perf contract gates the
  tuple/lambda diagnostic strings and the shared LLVM zero-value helper. Gates:
  `llvm-test-smoke`, `llvm-test-backend-compare`,
  `perf-contract-test-smoke`.
- Hardened more LLVM expression side-effect nodes:
  context access, party instance construction, empty await, and event
  subscribe/unsubscribe/invoke now reject missing receiver/storage/handler/arg
  metadata with structured diagnostics instead of silently retuning null or
  `0`. This keeps these edge emitters aligned with the no-silent-fallback LLVM
  cleanup track. Event expression lowering stays in
  `llvm_expr_event_calls.c`, keeping `llvm_expr.c` below the 600 LOC owner
  gate. Gates: `llvm-test-smoke`, `llvm-test-backend-compare`,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`,
  `build-source-inventory-test-smoke`.
- Tightened LLVM Result/Option operand failure handling:
  `Ok`, `Err`, `Some`, and `UnwrapOr` now reject failed payload/default
  expression lowering instead of feeding null values or `i32 0` into aggregate
  construction/select. Checked unwrap also fails when the panic runtime export
  or active function insertion block is missing, instead of extracting the
  payload without the runtime invariant guard. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Aligned LLVM Rc/Weak builtin error propagation with the Result/Option path:
  the call dispatcher now stops immediately when an Rc/Weak builtin has already
  raised a structured diagnostic, and `RcNew` reports payload-lowering failure
  instead of retuning the default `i32 0` recovery value as a successful
  expression. Gates: `llvm-test-smoke`, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`.
- Hardened LLVM subject projection path lowering:
  `ToObject` / `ToTObject` now stop when projection lowering raises a
  diagnostic, and nested projection source-field lookup now distinguishes
  missing source fields, ambiguous vessel paths, missing class metadata, and
  non-loadable path segments instead of inserting `i32 0` into the projected
  target field. Gates: `llvm-test-smoke`, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`.
- Hardened projection-borrowed identifier materialization:
  projection literal bindings now require target/source metadata and source
  storage, propagate nested projection path diagnostics, and no longer retun
  `i32 0` as a successful projected value when metadata is missing. Gates:
  `llvm-test-smoke`, targeted projection backend compare,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Hardened LLVM task/channel builtin fallback handling:
  `Cancel`, `TrySend`, `TrySendStatus`, `SendTimeout`,
  `SendTimeoutStatus`, `TryRecv`, and `RecvTimeout` now report structured
  diagnostics for operand/type/temporary-lowering failures, and recognized
  task/channel builtin names with unsupported arity no longer fall through to
  the generic function-call path. Gates: `llvm-test-smoke`,
  targeted channel/cancel/async backend compare, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`.
- Hardened LLVM vtable member-call dispatch:
  once a party/role vtable method slot is resolved, missing receiver
  registration, missing function-pointer metadata, call-argument allocation,
  and argument-lowering failures now report structured diagnostics instead of
  falling through into other member-call emitters. Gates: `llvm-test-smoke`,
  targeted member/vtable backend compare, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`.
- Hardened two LLVM generic call-dispatch edge paths:
  hosted-method call argument allocation now reports a structured diagnostic
  instead of retuning `i32 0`, and callable-variable calls now reject a failed
  callee expression before any `LLVMTypeOf` query. Intent forward-declare and
  event-handler callable signature parameter allocation failures now also stop
  with diagnostics before writing through NULL scratch arrays. Gates:
  `llvm-test-smoke`, `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Hardened LLVM identifier lowering:
  missing host-field `self` receivers and completely unresolved identifiers now
  report structured backend diagnostics instead of producing `i32 0` as a
  successful expression value. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Hardened LLVM slot-method and Slice() operand failures:
  `slot.Write(...)` now reports value-lowering failure, and `Slice(...)` now
  reports receiver/start/length or data-storage lowering failure before any
  `LLVMTypeOf` query on missing storage. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Hardened LLVM channel/event expression fallback handling:
  channel send expressions now diagnose failed value lowering, and event
  invocation calls now stop as recognized event calls when generated storage,
  runtime function, scratch argument allocation, or argument lowering is
  missing instead of falling through to generic function-call dispatch. Gates:
  `llvm-test-smoke`, targeted event/channel backend compare,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Hardened LLVM scalar expression failure handling:
  binary/unary operand lowering, operator overload name allocation,
  string concat/compare coercion failure, checked arithmetic helper failure,
  unsupported binary operators, and `?` try-operator malformed operands now
  report structured diagnostics. The try operator no longer queries
  `LLVMTypeOf` before checking for a missing lowered operand. Gates:
  `llvm-test-smoke`, `llvm-test-backend-compare` (`196/196` ABI pipeline,
  `69/69` backend compare), targeted scalar/try backend compare,
  `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Hardened LLVM member-call name allocation failures:
  all member-call method-name synthesis paths now route scratch allocation
  failure through `llvm_member_call_error_recovery(...)` instead of retuning
  `i32 0` silently. Gates: `llvm-test-smoke`, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`.
- Hardened LLVM constructor partial-object fallback:
  enum variant and class constructors now diagnose payload/field argument
  lowering failure instead of silently skipping the field and constructing a
  defaulted partial object. Call dispatch now stops immediately when constructor
  lowering raises a diagnostic. Gates: `llvm-test-smoke`, targeted constructor
  backend compare, `perf-contract-test-smoke`, `test-inc-size-test-smoke`.
- Hardened LLVM queue builtin fallback handling:
  `QueuePush` now diagnoses failed value lowering or element-temporary
  allocation, and `QueuePop` now diagnoses result-temporary allocation failure
  instead of retuning default queue values silently. Gates: `llvm-test-smoke`,
  targeted queue backend compare, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`.
- Hardened LLVM base collection builtin fallback handling:
  `ListNew` / `SetNew` now diagnose missing element LLVM type metadata and
  temporary allocation failure, while `SetAdd` / `SetHas` / `SetRemove` now
  diagnose value-lowering and element-temporary allocation failures instead of
  retuning default collection results silently. Gates: `llvm-test-smoke`,
  targeted collection backend compare, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`.
- Removed another anonymous backend specialization fallback:
  LLVM `Some(value)` now requires contextual `Option<T>` layout just like
  `None()`, instead of synthesizing an ad-hoc `{ tag, LLVMTypeOf(value) }`
  struct. This keeps C/LLVM parity aligned with the concrete Option policy and
  prevents value-type-only inference from silently defining a backend-local
  Option layout. The perf contract now gates the diagnostic string. Gates:
  `llvm-test-smoke`.
- Hardened LLVM Result/Option consumers against malformed operands:
  `IsOk` / `IsErr` / `Unwrap` / `UnwrapOr` and
  `IsSome` / `IsNone` / `UnwrapOption` now validate aggregate shape before
  `extractvalue`. Non-aggregate drift is reported as a structured LLVM type
  diagnostic instead of reaching backend IR builder failure paths. Gates:
  `llvm-test-smoke`.
- Reordered LLVM Result/Option constructors to be context-first:
  `Ok`, `Err`, and `Some` now validate the contextual `Result<T,E>` /
  `Option<T>` layout before emitting the payload expression. This matches the
  C backend's concrete-specialization policy and avoids producing partial IR
  on rejected anonymous constructor calls. Gates: `llvm-test-smoke`.
- Removed LLVM collection-constructor implicit `i32` fallback:
  expression-level `ListNew()` and `SetNew()` now require contextual
  `List<T>` / `Set<T>` metadata and use that concrete element type for raw
  runtime initialization. Missing context now produces a structured LLVM type
  diagnostic instead of allocating an `i32` placeholder or probing struct
  layout on a non-struct type. Gates: `llvm-test-smoke`.
- Started replacing LLVM collection-operation silent receiver fallbacks:
  List operations now share `llvm_collection_required_receiver_var(...)`, so a
  non-identifier receiver or missing collection local reports a structured LLVM
  diagnostic instead of retuning `0` as if the operation succeeded. Gates:
  `llvm-test-smoke`, `test-inc-size-test-smoke`.
- Extended the same structured receiver check to LLVM Set operations:
  `SetAdd`, `SetHas`, `SetRemove`, and `SetSize` now reject invalid receivers
  or missing set locals through the same shared receiver helper instead of
  silently producing `0`/`false`. Gates: `llvm-test-smoke`,
  `test-inc-size-test-smoke`.
- Extended receiver validation to LLVM Queue operations:
  `QueuePush`, `QueuePop`, `QueueSize`, and `QueueEmpty` now use the shared
  receiver helper with a `queue` kind label, so invalid receivers and missing
  queue locals are structured diagnostics instead of silent `0` / `true`
  fallbacks. Gates:
  `llvm-test-smoke`, `test-inc-size-test-smoke`.
- Reused the LLVM collection receiver helper for HashMap operations:
  `MapSet`, `MapGet`, `MapHas`, `MapRemove`, `MapSize`, and `MapKeys` now reject
  invalid receivers or missing map locals through the same structured
  diagnostic path instead of retuning empty values as if the operation had
  succeeded. Gates: `llvm-test-smoke`, `test-inc-size-test-smoke`.
- Stopped materializing backend-local `Unknown` constructed generic layouts in
  LLVM type mapping:
  `llvm_required_constructed_arg_name_at(...)` now retuns `NULL` after setting
  a structured diagnostic, and `pergyra_type_to_llvm(...)` exits through the
  recovery type without registering `List<Unknown>` / `Slot<Unknown>` /
  similar constructed placeholders. The perf contract gates that this helper
  does not retun `"Unknown"` again. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`.
- Replaced LLVM Array builtin silent `0` fallbacks with structured diagnostics:
  `ArrayPush`, `ArraySet`, and `ArrayPop` now require identifier receivers,
  registered `Array<T>` locals, and concrete element metadata before emitting
  runtime calls. `ArrayLength` now reports a concrete aggregate requirement
  instead of retuning `0` for non-array operands. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`.
- Tightened LLVM channel expression binding checks:
  send/receive expression lowering now uses `llvm_channel_required_binding(...)`
  so a channel metadata/scope mismatch reports a structured `Channel<T>` local
  diagnostic instead of falling through to a silent `0` value. Gates:
  `llvm-test-smoke`, `perf-contract-test-smoke`.
- Removed LLVM `Slice()` anonymous layout fallback:
  member-call slice lowering now requires concrete receiver storage and
  concrete element metadata, and always uses the registered `Slice<T>` layout
  instead of synthesizing a backend-local `{ ptr, len }` struct when the suffix
  is unknown. Gates: `llvm-test-smoke`, `perf-contract-test-smoke`.
- Tightened LLVM slot member-call ownership:
  `slot.Write` / `slot.Read` / `slot.Release` now reject slot metadata without a
  matching registered local binding instead of falling through to unrelated
  method dispatch after the receiver was already classified as a slot. Gates:
  `llvm-test-smoke`, `perf-contract-test-smoke`.
- Tightened LLVM call dispatch recovery:
  invalid call callee shapes now report structured diagnostics, hosted method
  calls require a self receiver, and generic/intent call argument lowering stops
  before emitting a call if any argument fails to lower. This prevents null
  `LLVMValueRef` arguments from reaching `LLVMBuildCall2(...)`. Gates:
  `llvm-test-smoke`, `perf-contract-test-smoke`.
- Tightened the shared LLVM call-argument helper:
  `llvm_emit_function_call_args(...)` now rejects missing argument AST payloads,
  allocation failure, and argument-expression lowering failure before building
  the call. This protects static/member/builtin paths that reuse the helper.
  Gates: `llvm-test-smoke`, `perf-contract-test-smoke`.
- Tightened LLVM member-call lowering:
  nominal/member dispatch paths now reject missing self receivers, argument
  allocation failure, receiver lowering failure, and argument lowering failure
  before reaching `LLVMBuildCall2(...)`. This removes another class of null
  call-argument recovery from subject/class/member calls. Gates:
  `llvm-test-smoke`, `perf-contract-test-smoke`.
- Removed the final LLVM member-call silent `0` fallback:
  unsupported member calls now report that the method is not declared in the
  backend method registry instead of retuning an integer zero expression.
  Gates: `llvm-test-smoke`, `perf-contract-test-smoke`.
- Tightened LLVM slot/device builtin diagnostics:
  slot target resolution now rejects known slot metadata without a matching
  registered local binding, and `Write` / `Read` / `Release` plus device-slot
  builtins now report argument-count diagnostics instead of silently retuning
  `0` on too few arguments. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`.
- Tightened LLVM generic specialization:
  `llvm_resolve_callee_entry(...)` now requires lowered arguments and concrete
  suffix metadata before mangling or binding generic substitutions, instead of
  silently substituting `Int` / `Unknown` for missing specialization evidence.
  Gates: `llvm-test-smoke`, `perf-contract-test-smoke`.
- Tightened LLVM spawn expression lowering:
  spawn targets now require identifier callees, successful argument lowering,
  and successful argument/loaded-argument buffer allocation before wrapper
  emission, instead of allowing null `LLVMValueRef` arguments to flow into the
  generated spawn wrapper. Gates: `llvm-test-smoke`, `perf-contract-test-smoke`.
- Tightened LLVM member access fallback:
  member access lowering now reports structured diagnostics for missing receiver
  metadata, missing class registrations, missing fields, projection-source
  metadata gaps, and incompatible receiver layouts instead of retuning integer
  zero as a successful expression. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`.
- Tightened LLVM assignment fallback:
  assignment lowering now reports structured diagnostics for missing targets,
  non-identifier indexed array receivers, missing `Array<T>` locals, failed
  index/value lowering, missing writable member lvalues, and missing local/host
  field targets instead of retuning integer zero as a successful assignment.
  Gates: `llvm-test-smoke`, `perf-contract-test-smoke`.
- Tightened LLVM array-access fallback:
  array/slice/string/pointer access now reports structured diagnostics when
  receiver/index lowering fails, collection element metadata is missing, or the
  receiver layout is not indexable, instead of defaulting aggregate element
  loads to `Int`. Gates: `llvm-test-smoke`, `perf-contract-test-smoke`.
- Tightened LLVM log-family fallback:
  `Log`, `LogRaw`, and `LogBanner` now report argument-count and argument
  lowering/stringification diagnostics instead of treating malformed log calls
  as successful no-op integer-zero expressions. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`.
- Tightened LLVM stdlib scalar/io fallback:
  string/file/scalar/runtime stdlib helpers now convert failed argument
  lowering/stringification into structured diagnostics, and direct
  `StringLength`, `ToString`, `Print`, and `Sleep` lowering no longer pass null
  `LLVMValueRef` arguments into runtime calls. Gates: `llvm-test-smoke`,
  `perf-contract-test-smoke`.
- Added structured LLVM preflight result diagnostics for
  `llvm_validate_mir_for_codegen(...)`: null MIR programs, nameless routines,
  and MIR emission-topology failures now retun `LLVMGenResult` with
  stable code/cause/fix fields instead of message-only errors. The result
  helper preserves the existing owning-string ABI by arena-copying diagnostic
  tags. Gates: `mir-declaration-inventory-test-smoke`,
  `llvm-test-smoke`, and `test-inc-size-test-smoke`.
- Fixed LLVM native object codegen's intent-observability runtime selection:
  `llvm_codegen_to_object_core(...)` now derives
  `ctx->uses_intent_observability` from MIR before emission, matching the LLVM
  IR path and preventing observability-using programs from selecting the
  non-observability runtime cache object. LLVM context-result conversion now
  uses the same arena-owning structured-result helper, removing a duplicate
  code/cause/fix copy path. The perf contract now requires both
  LLVM codegen paths to set the MIR-derived flag. Gates:
  `perf-contract-test-smoke` and `llvm-test-smoke`.
- Centralized AIR authoritative-evidence policy:
  `air_validate_evidence_inventory(...)`, `air_verify(...)`, and public
  `air_boundary_has_evidence(...)` now consume the same intenal
  `air_evidence_inventory_is_authoritative(...)` helper instead of carrying
  duplicate static predicates. Boundary evidence-kind lookup also lives with
  the evidence validation owner now, so `air_verify.c` consumes inventory
  facts rather than owning the lookup loop. This keeps strict evidence
  validation and drift checking aligned as AIR becomes the
  abstraction-boundary verification layer. Gates: `test-air` (`77/0`),
  `air-drift-test-smoke`,
  `air-json-schema-test-smoke`, and `air-backend-nonimpact-full-test-smoke`.
- Tightened CFG/MIR use-edge population toward fact-first body safety:
  `mir_populate_use_edges(...)` now consumes `expr0` directly for DEF
  instruction use collection, while non-DEF instructions still prefer MIR
  expression facts (`expr0` / `expr1`) before explicit source/provenance
  payloads. This keeps SSA/use-edge analysis aligned with the existing MIR
  initializer/terminator fact validators and reduces the remaining AST-first
  body-safety seam. Gates: `test-mir` (`48/0`),
  `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`, and
  `test-inc-size-test-smoke`.
- Isolated the C backend pending-use source compatibility path:
  `transpiler_find_block_binding_from_mir_insts(...)` now keeps the fact-first
  `expr0` / `expr1` binding path as the normal route and routes the remaining
  source-AST let-binding emit seam through
  `transpiler_pending_binding_from_source_statement_emit(...)`. This does not
  remove the seam, but it gives the remaining fallback a named owner and keeps
  future cleanup from spreading statement payload reads back into the MIR block
  binding path. Gates: `cfg-body-dataflow-test-smoke`,
  `perf-contract-test-smoke`, and `test-mir` (`48/0`).
- Narrowed the C backend pending-use source compatibility seam further:
  both the normal pending-binding lookup and the residual source compatibility
  helper now require `MIRInstruction.requires_source_statement_emit`, so a
  source-shape-only `AST_LET_DECL` fact cannot reopen source payload reads.
  The perf/CFG smokes gate the requirement and `test-transpile` keeps the C
  backend behavior stable. Gates: `test-transpile` (`722/0`),
  `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`, and
  `test-inc-size-test-smoke`.
- Lifted the DEF source-statement compatibility decision into MIR:
  `MIRInstruction.requires_source_statement_emit` is now populated by
  `mir_attach_def_initializer_call_fact(...)`, validated by
  `mir_validate_instruction_surface_usage(...)`, dumped by the MIR public
  surface, and consumed by both `llvm_mir_def_uses_source_statement_emit(...)`
  and the C backend's `transpiler_mir_def_uses_source_statement_emit(...)`.
  The validator now enforces the fact in both directions: LET/ASSIGN DEFs must
  carry it, and instructions carrying it must have valid source statement
  shape/provenance.
  C/LLVM still emit the original source statement for the compatibility path,
  but backend code no longer decides that path by locally checking
  `source_ast_type == AST_LET_DECL || AST_ASSIGNMENT`; the remaining
  compatibility path is now an explicit MIR fact. Gates:
  `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`, `test-mir`
  (`48/0`), `test-inc-size-test-smoke`, `llvm-test-smoke`, and
  `build-source-inventory-test-smoke`.
- Split local declaration source emission out of the broad source-statement
  fact: `MIRInstruction.requires_source_local_decl_emit` now marks LET-backed
  DEFs that still need source-local-declaration emission, is validated as a
  LET-only MIR fact, is dumped as `source-local-decl-emit`, and is consumed by
  C/LLVM as a distinct compatibility branch. Pending-use reconstruction in
  the C backend now also requires the local-decl fact, so a broad
  source-statement fact can no longer reopen local declaration payload reads
  by itself. This does not remove the residual source statement emission path
  yet, but it narrows the remaining seam from "any source statement" to "local
  declaration source emit" for the next backend cleanup slice. Gates:
  `test-mir` (`51/0`).
- Made the remaining LLVM match/select branch source-compatibility seam
  explicit in MIR validation: `mir_validate_instruction_surface_usage(...)`
  now rejects `MIR_BRANCH_MATCH_CASE` / `MIR_BRANCH_SELECT_DISPATCH` branches
  that have no source payload, matching the current LLVM codegen requirement
  instead of leaving the failure to backend emission. The MIR suite now carries
  a negative drift fixture for this source-compatible branch path, and the
  CFG/perf smokes gate the contract string. Gates: `test-mir` (`48/0`),
  `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`, and
  `test-inc-size-test-smoke`.
- Lifted the source-compatible branch path into an explicit MIR fact:
  `MIRInstruction.requires_source_branch_emit` is populated for match/select
  branch shapes, validated for both missing and invalid source-branch fact
  drift, dumped as
  `source-branch-emit`, and consumed by LLVM's branch condition gate and the C
  backend MIR emission contract instead of locally recomputing the match/select
  shape pair for source-required branch validation. Gates: `test-mir` (`48/0`),
  `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`,
  `test-transpile` (`729/0`), `test-inc-size-test-smoke`, and
  `llvm-test-smoke`.
- Matched the C MIR select-dispatch branch readiness path to the LLVM branch
  path: C MIR branch condition rendering can now derive `pgy_channel_ready_T`
  from a select case source block or from the target block receive DEF payload
  instead of falling through to a default expression condition. A dedicated
  transpile regression covers the C output path and the perf contract gates the
  renderer terms. Gates: `test-transpile` (`729/0`), `perf-contract-test-smoke`,
  and `test-inc-size-test-smoke`.
- Closed the C MIR select bound-receive local materialization seam:
  `case v = <-ch` now has a narrow C backend path that treats receive
  assignments as select-local bindings, infers `v` from `Channel<T>` as `T`,
  and emits the receive assignment through the SSA local (`_pgy_ssa_v_*`)
  instead of leaking the source name `v` into generated C. The regression also
  rejects the old `v = pgy_channel_recv_val_T(...)` output. Gates:
  `test-transpile` (`729/0`), `test-mir` (`48/0`),
  `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`, `build-source-inventory-test-smoke`, and
  `llvm-test-smoke`.
- Made LLVM MIR local alloca type selection fact-first: the local allocator now
  consumes `MIRInstruction.type_layout`, then `expr1` type facts, then `expr0`
  value facts, instead of branching on `source_ast_type == AST_LET_DECL`.
  This keeps source AST shape as provenance while the actual local type
  decision follows MIR facts. Gates: `llvm-test-smoke` and
  `perf-contract-test-smoke`.
- LLVM MIR DEF statement compatibility is still a real remaining seam, not a
  removable flag check: a trial restriction that used statement compatibility
  only when `expr0 == NULL` broke the `break_continue` LLVM smoke. Closing this
  requires first lifting base-name scope declaration and statement side-effect
  lowering into explicit MIR facts; `expr0` alone is not enough to replace the
  source-statement compatibility path safely.
- Rechecked the tempting LLVM channel-receive DEF shortcut after the C select
  bound-receive cleanup: routing `expr0 == AST_CHANNEL_RECV` through raw value
  store instead of `llvm_emit_statement(inst->ast, ...)` broke the
  `select_fainess` LLVM smoke (`1,3,3,3` instead of the expected fair consume
  sequence). This confirms that the remaining LLVM source-statement emit seam
  still owns select consume/fainess side effects, not just local value storage.
  Do not remove it until select consume state and source binding aliasing are
  represented as explicit MIR facts. Gates after revert: `llvm-test-smoke` and
  `perf-contract-test-smoke`.
- Gated the remaining LLVM select-receive source-statement seam explicitly:
  `perf-contract-test-smoke` now requires the `MIR_INST_DEF` path to keep
  `llvm_mir_declare_recv_target(...)` plus `llvm_emit_statement(inst->ast, ...)`,
  and `cfg-body-dataflow-test-smoke` ties that contract to the `select_fainess`
  two-channel smoke fixture. This tuns the failed shortcut into a named
  blocker instead of a hidden backend assumption. Gates:
  `perf-contract-test-smoke` and `cfg-body-dataflow-test-smoke`.
- Split the first select receive/fainess reason out of the broad channel
  receive source-statement fact: HIR select case body blocks now carry
  `is_select_case_body`, MIR copies that block fact, and a receive DEF at
  source statement index 0 is marked with
  `requires_select_receive_statement_emit` / `select-recv-stmt-emit`. The MIR
  validator rejects both missing select receive facts and invalid facts on
  non-select receives, LLVM now names the select receive DEF path through
  `llvm_mir_def_uses_select_receive_statement_emit(...)`, and the C backend
  mirrors the same classification with
  `transpiler_mir_def_uses_select_receive_statement_emit(...)` before using the
  existing channel receive/source-statement lowering. AIR now also collects this as global
  `AIR_EVIDENCE_MIR_SELECT_RECEIVE` / `mir_select_receive` evidence and exposes
  `mir_select_receive_evidence_count` in `pgy.air.graph.v1`, so the
  abstraction verifier can see the remaining select consume/fainess seam. This
  does not remove
  `llvm_emit_statement(inst->ast, ctx)` yet; it gives the remaining select
  consume/fainess seam a narrower MIR reason before backend behavior is
  changed. Gates: `test-mir` (`52/0`),
  `test-air` (`79/0`), `cfg-body-dataflow-test-smoke`,
  `perf-contract-test-smoke`, `air-drift-test-smoke`,
  `air-json-schema-test-smoke` with a real select receive fixture,
  `test-inc-size-test-smoke`,
  `build-source-inventory-test-smoke`, and `llvm-test-smoke`.
- Removed C backend anonymous `Option<Unknown>` specializations:
  `Some(value)` now first uses the payload's concrete type, then contextual
  `Option<T>` payload type, and otherwise emits a structured diagnostic
  recovery instead of generating `Some_Unknown(...)`. `IsSome` / `IsNone` /
  `UnwrapOption` now also require concrete `Option<T>` inner metadata instead
  of accepting `Option<Unknown>`, and C type inference now also uses contextual
  `Option<T>` inner metadata when inferring `Some(...)`. The perf contract
  gates the shared helper, contextual inference hook, and regression fixtures.
  The same pass now rejects `Result<Unknown, ...>` suffix derivation before
  `Ok` / `Err` can generate anonymous result specializations, using exact
  identifier-token matching so user type names such as `MyUnknownType` remain
  valid. LLVM result layout materialization now mirrors the same exact-token
  `Unknown` rejection before creating named `PgyResult_*` layouts. Gates:
  `test-transpile` (`740/0`), `llvm-test-smoke`, and
  `perf-contract-test-smoke`.
- Removed the C MIR explicit-local prepass `Slot<Unknown>` fallback for
  `with slot` aliases. If the alias slot type cannot be rendered as concrete
  metadata, the prepass now records a structured C backend diagnostic and does
  not register an anonymous slot type. Gate: `perf-contract-test-smoke`.
- Fixed a C/LLVM select-dispatch parity edge: when LLVM cannot materialize a
  select readiness condition from the case source or target receive DEF, it now
  falls back to `false` like the C backend instead of taking the true branch by
  default. The perf/CFG smokes gate the fallback constant and
  `llvm-test-smoke` keeps the `select_ready` / `select_fainess` runtime path
  green. Gates: `perf-contract-test-smoke`, `cfg-body-dataflow-test-smoke`,
  and `llvm-test-smoke`.
- Removed the residual AST-shape initializer fallback from MIR DEF use-edge
  collection: `mir_def_instruction_source_expr(...)` now retuns only the
  materialized `expr0` fact. The MIR validator already rejects LET/ASSIGN DEFs
  without initializer facts, so SSA/use-edge analysis no longer reopens
  `inst->ast->type` for that path. Gates: `test-mir` (`48/0`),
  `perf-contract-test-smoke`, and `cfg-body-dataflow-test-smoke`.
- Rechecked DAG/source inventory after the CFG/MIR cleanup slice:
  `type-resolution-resolver-inventory-test-smoke` still reports
  `fallback seams=0 cap=0` and `type-ref helper refs=2 cap=2`; source remains
  `.inc=0` and the Makefile build inventory includes the new transpile owner.
  Gates: `type-resolution-resolver-inventory-test-smoke`,
  `test-inc-size-test-smoke`, and `build-source-inventory-test-smoke`.
- Centralized LLVM select runtime-function diagnostics behind
  `llvm_select_required_runtime_function(...)`: bound receive, readiness, and
  consume now share one required-runtime lookup seam while preserving the
  `receive` / `readiness` / `consume` diagnostic family labels. This is a
  preparation step for lifting select consume/fainess into explicit MIR facts;
  it does not remove the source-statement emit seam yet. Gates:
  `llvm-test-smoke` and `perf-contract-test-smoke`.
- Tightened the C select emitter's local runtime-call seams:
  `transpiler_select.c` now routes select channel type lookup, guard emission,
  and unbound receive consume through named local helpers instead of repeating
  `pgy_channel_try_recv_T` / `pgy_channel_ready_T` strings in the loop body.
  The unbound consume path no longer allocates a temporary `AST_CHANNEL_RECV`
  node, so select emission does not reopen AST ownership just to call the
  channel receive renderer. Gates: `test-transpile` (`729/0`) and
  `perf-contract-test-smoke`.
- Named the LLVM MIR select readiness runtime lookup seam:
  `llvm_mir_emit_channel_ready_condition(...)` now delegates the registered
  `pgy_channel_ready_T` lookup and diagnostic to
  `llvm_mir_required_channel_ready_function(...)`, matching the C select
  cleanup direction while preserving the MIR select readiness diagnostic text.
  Gates: `llvm-test-smoke` and `perf-contract-test-smoke`.
- Narrowed the LLVM statement select emitter toward a future MIR select-consume
  fact: parsed select cases now flow through `LLVMSelectCaseInfo`, and bound
  receive / ready-consume emission are split into
  `llvm_select_emit_bound_receive_case(...)` and
  `llvm_select_emit_ready_consume_case(...)`. This preserves the full
  `select_fainess` behavior while giving the remaining AST statement path one
  replacement point for future MIR consume/fainess facts. Gates:
  `llvm-test-smoke`, `perf-contract-test-smoke`, and `test-inc-size-test-smoke`.
- Split channel-receive DEF emission evidence out of the broad
  source-statement emit fact: `MIRInstruction` now carries
  `requires_channel_receive_statement_emit`, the MIR validator rejects missing
  or invalid receive-emission facts, and the LLVM MIR DEF path only declares a
  receive target when that narrower fact is present. This still preserves the
  source-statement emit seam for select fainess/consume, but makes the reason
  explicit and gives the next cleanup pass a concrete fact to consume. Gates:
  `test-mir` (`49/0`), `cfg-body-dataflow-test-smoke`,
  `perf-contract-test-smoke`, and `llvm-test-smoke`.
- Aligned the C backend with the new channel-receive DEF fact:
  `transpiler_mir_def_uses_channel_receive_statement_emit(...)` now gates
  channel-receive assignment emission before the C backend renders
  `pgy_channel_recv_val_T(...)`. This keeps C and LLVM consuming the same MIR
  receive evidence instead of leaving the new fact LLVM-only. Gates:
  `test-transpile` (`729/0`), `cfg-body-dataflow-test-smoke`, and
  `perf-contract-test-smoke`.
- Removed the LLVM channel-receive DEF dependency on full source-statement
  emission: `llvm_mir_emit_channel_receive_def(...)` now emits the
  `pgy_channel_recv_val_T(...)` call directly from MIR receive evidence,
  stores it into the named target alloca, and mirrors it into the MIR SSA
  alloca when present. The broader `llvm_emit_statement(inst->ast, ctx)` seam
  remains only for other source-statement emit cases; select readiness and
  fainess stay covered by LLVM smoke. Gates: `llvm-test-smoke` and
  `test-inc-size-test-smoke`.
- Hardened the same LLVM channel-receive DEF path against silent false
  retuns: missing target names, non-identifier channels, missing channel
  allocas, and missing receive target allocas now set structured LLVM
  diagnostics before emission stops. This keeps direct MIR evidence lowering
  from reintroducing an unexplained backend stop. Gates:
  `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`, and `llvm-test-smoke`.
- Moved LLVM MIR `with slot` claim setup off AST payload reads:
  `llvm_mir_emit_with_claim_only(...)` now consumes `slot_anchor`,
  `arg1`, and `MIRTypeLayout.abi_type_name` to derive the alias, secure/plain
  slot family, and inner type. The AST payload is retained only as diagnostic
  source location. The emitter now lives in the compiled
  `llvm_mir_resource_claim.c` owner, so the MIR block emitter only dispatches
  the Claim resource op instead of owning slot-allocation setup. The perf/CFG
  smokes reject reintroducing `node->data.with_stmt` in the MIR block emitter
  or the resource-claim owner. Gates:
  `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`, and `llvm-test-smoke`.
- Added the upstream MIR validator contract for that `with slot` claim path:
  CFG-backed Claim resource ops sourced from `with` statements must carry a
  slot-family `MIRTypeLayout`, and corrupt/missing layout facts now fail before
  LLVM can derive a wrong slot ABI. Gates: `test-mir` (`50/0`),
  `cfg-body-dataflow-test-smoke`, and `perf-contract-test-smoke`.
- Split LLVM MIR borrow-view alias setup out of the MIR block emitter into the
  compiled `llvm_mir_resource_view.c` owner. `BorrowRead` / `BorrowWrite`
  resource ops now keep slot alias, secure-slot token alias, and slot registry
  mirroring in a resource-specific owner, while the block emitter only
  dispatches the MIR resource op. Gates: `llvm-test-smoke`,
  `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`,
  `test-inc-size-test-smoke`, and `build-source-inventory-test-smoke`.
- Split LLVM MIR pin-region enter/exit emission out of the MIR block emitter
  into the compiled `llvm_mir_pin_region.c` owner. Pin allocation, view alias
  registration, secure-token alias registration, runtime pin lookup, and
  unpin cleanup emission now live beside the Slot/Pin ABI contract instead of
  the generic block walker. The ABI/perf/declaration-inventory smokes were
  updated from stale header/block-emitter expectations to the current compiled
  owners. Gates: `llvm-test-smoke`, `perf-contract-test-smoke`,
  `abi-ownership-shape-test-smoke`, `mir-declaration-inventory-test-smoke`,
  `test-inc-size-test-smoke`, and `build-source-inventory-test-smoke`.
- Split C backend MIR function reason classification out of
  `transpiler_func_class_flow_emit.h` into
  `transpiler_mir_reason_classifier.{h,c}`. Function emission now consumes a
  typed `TranspilerMirReasonDiagnostic` instead of inspecting reason strings
  inline. The classifier is table-driven through `kMirFunctionReasonPattens`,
  including the `no matching MIR routine` and generic `MIR contract invalid`
  topology families emitted by the MIR emission contract validator,
  and the declaration-inventory smoke rejects reintroducing inline
  `strstr(reason, ...)` classification in the emitter. Gates:
  `mir-declaration-inventory-test-smoke`, `test-transpile`,
  `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Centralized C/LLVM MIR intent-carrier diagnostics behind
  `transpiler_set_mir_intent_carrier_missing(...)` and
  `llvm_set_mir_intent_carrier_missing(...)`: intent cleanup and step-context
  emission no longer duplicate the carrier-missing code/cause/fix routing
  locally, and the declaration-inventory smoke now rejects reintroducing local
  carrier code/hint paths. LLVM MIR cleanup/pin contract validation also routes
  topology failures through `llvm_set_mir_topology_invalid(...)` instead of
  open-coded topology hints. Gates:
  `mir-declaration-inventory-test-smoke`, `perf-contract-test-smoke`, and
  `test-inc-size-test-smoke`.
- Centralized AIR evidence-authority selection in `air_verify.c`: both public
  evidence lookup and strict drift verification now use the same
  `air_evidence_inventory_is_authoritative(...)` policy, so real HIR/RIR/MIR
  input and explicit evidence inventories cannot drift through separate legacy
  summary-flag checks. Follow-up validator tightening makes the same rule an
  `air_verify(...)` invariant: with real HIR/RIR/MIR input, legacy summary flags
  without matching `AIREvidenceNode` inventory are rejected before drift
  checking. Gates: `test-air` (`77/0`), `air-drift-test-smoke`,
  `air-json-schema-test-smoke`, and `air-backend-nonimpact-full-test-smoke`.
- Refreshed `cfg-body-dataflow-test-smoke` after the C backend MIR intent query
  owner split: the gate now checks `transpiler_mir_intent_query.c` instead of
  the declaration-only `transpiler_helpers.h` shim for
  `mir_instruction_intent_payload(...)` / step matching consumption. Gates:
  `cfg-body-dataflow-test-smoke` and `test-mir` (`44/0`).
- Tightened MIR DEF payload validation for CFG/body source-of-truth: the
  validator now decides whether a DEF requires an initializer fact from
  `source_ast_type` shape metadata instead of reopening `inst->ast->data`.
  The MIR fixture clears `inst->ast` while preserving the shape fact to prove
  the validator no longer depends on the AST payload. Gates: `test-mir`
  (`44/0`), `cfg-body-dataflow-test-smoke`, and `perf-contract-test-smoke`.
- Lifted retun terminator value presence into `MIRInstruction` as
  `source_terminator_has_value`, so MIR validation rejects missing retun
  expression facts from HIR terminator shape instead of using `inst->ast` as the
  valued-retun discriminator. The MIR terminator fixture now clears
  `inst->ast` for both branch and retun negative checks. Gates: `test-mir`
  (`44/0`), `cfg-body-dataflow-test-smoke`, and `perf-contract-test-smoke`.
- Tightened LLVM MIR statement fallback one step: `defer` registration now
  consumes `source_ast_type == AST_DEFER_STMT` plus the `expr0` defer-body fact
  instead of requiring `inst->ast`, and CFG-container classification can use
  source AST shape facts through `llvm_mir_ast_type_is_cfg_container(...)`.
  Remaining statement AST emission is explicitly source/provenance compatibility.
  Gates: `llvm-test-smoke` and `perf-contract-test-smoke`.
- Tightened MIR DCE as a body-safety consumer: statement side-effect decisions
  now prefer MIR source shape and call-name facts (`source_ast_type`, `arg0`)
  before falling back to AST payload. The new regression proves an AST-less
  `Log` statement is preserved while AST-less `ChannelLength` is removed as a
  pure query. Gates: `test-mir` (`45/0`), `perf-contract-test-smoke`, and
  `cfg-body-dataflow-test-smoke`.
- Named the remaining LLVM MIR block source compatibility seams instead of
  leaving them as inline AST predicates: match/select branch lowering now flows
  through `llvm_mir_branch_requires_source_emit(...)`, and DEF
  statement fallback flows through
  `llvm_mir_def_uses_source_statement_emit(...)`. This does not claim
  the seam is removed; it narrows the future fact-replacement point and keeps
  the debt visible. Gates: `llvm-test-smoke` and `perf-contract-test-smoke`.
- Tightened CFG contract validation as a body-safety consumer: CFG-owned
  fallback STMT rejection now prefers `source_ast_type` via
  `mir_stmt_ast_type_is_cfg_owned_control(...)` and falls back to `inst->ast`
  only when source shape metadata is absent. Gates: `test-mir` (`45/0`),
  `perf-contract-test-smoke`, and `cfg-body-dataflow-test-smoke`.
- Named the MIR surface-payload/shape validation guard as
  `mir_instruction_has_surface_payload_or_shape(...)`, making the validator's
  remaining AST acceptance explicit as source/provenance compatibility rather
  than an inline semantic shortcut. Gates: `test-mir` (`45/0`) and
  `perf-contract-test-smoke`.
- Made the non-CFG body fallback visible on `MIRRoutine` as
  `used_non_cfg_body_fallback` / `non_cfg_body_fallback_count`; MIR dumps now
  print `noncfg=...`, and the normal CFG-backed pure-query fixture asserts zero
  fallback usage. This tuns the legacy body-population seam into a measurable
  beta debt instead of a hidden compatibility path. Gates: `test-mir` (`45/0`),
  `perf-contract-test-smoke`, `cfg-body-dataflow-test-smoke`,
  `source-utf8-test-smoke`, `build-source-inventory-test-smoke`, and
  `test-inc-size-test-smoke`.
- Promoted the non-CFG fallback counter from visibility to validation:
  `mir_validate_non_cfg_fallback_state(...)` now rejects CFG-backed routines
  that report non-CFG body fallback use, and the MIR suite has a negative drift
  fixture for that state. Gates: `test-mir` (`48/0`),
  `perf-contract-test-smoke`, `cfg-body-dataflow-test-smoke`, and
  `test-inc-size-test-smoke`.
- Strengthened `pgy.air.graph.v1` as a verification-consumer surface: each AIR
  evidence JSON node now carries additive `boundary_kind`, `boundary_owner`, and
  `boundary_source` fields, so LSP/CI tools can consume evidence-to-boundary
  provenance without rebuilding the join from `boundaries[]`. Gates: `test-air`
  (`77/0`), `air-json-schema-test-smoke`, and `air-drift-test-smoke`.
- Rechecked DAG source-of-truth gates after AIR/CFG validator tightening:
  recursive resolver calls, resolver body fallbacks, metadata dead-ends,
  materializer unresolved, stage materializer calls, and alias diagnostic
  resolver calls all remain `0`; the active graph-backed counters are
  `metadata_entries=3498`, `metadata_owned=258`, and `metadata_hits=8380`.
  Gates: `type-resolution-dag-test-smoke` and
  `type-resolution-resolver-inventory-test-smoke`.
- Rechecked MIR declaration inventory / parallel surface gates after the AIR and
  CFG validator changes. Declaration inventory remains helper-gated in both C
  and LLVM, and remaining `source_ast` access is still named compatibility /
  provenance rather than hidden inventory state. Gates:
  `mir-declaration-inventory-test-smoke`, `parallel-core-contract-test-smoke`,
  and `perf-contract-test-smoke`.
- Mirrored LLVM's MIR-missing diagnostic seam in the C backend with
  `transpiler_set_mir_inventory_missing(...)`. Class, enum, intent, and
  hosted role/domain method emission now route missing declaration
  metadata/routine/step/participant inventory failures through the same stable
  code/cause/fix policy instead of ad-hoc backend errors; the
  declaration-inventory smoke now rejects direct
  `PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING` use outside the C context helper.
  Gates: `pgy`, `test-transpile` (`717/0`),
  `mir-declaration-inventory-test-smoke`, `perf-contract-test-smoke`,
  `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Split the remaining C MIR emission diagnostic seam into inventory-missing
  and topology-invalid helpers. Host-scoped method inventory gaps now use
  `transpiler_set_mir_inventory_missing(...)`, while missing MIR method owner
  metadata, block emission failure, and pin cleanup emission failure use
  `transpiler_set_mir_topology_invalid(...)` with stable code/cause/fix hints.
  Gates: `pgy`, `test-transpile` (`717/0`),
  `mir-declaration-inventory-test-smoke`, `perf-contract-test-smoke`, and
  `test-inc-size-test-smoke`.
- Collapsed repeated C MIR SSA-local capacity errors behind
  `transpiler_mir_ssa_local_limit_fail(...)`, so SSA limit failures now report
  `PGY_CODE_MIR_SSA_LIMIT` / `PGY_CAUSE_MIR_SSA_CAPACITY_EXCEEDED` instead of
  ad-hoc backend strings. MIR parameter/local/destructure/resource-op type
  failures now route through stable C type unsupported hints, and the C MIR
  emitter family no longer assigns `ctx->backend_error = strdup_fmt(...)`
  directly. Gates: `pgy`, `test-transpile` (`717/0`),
  `perf-contract-test-smoke`, and `test-inc-size-test-smoke`.
- Removed the remaining direct C backend `ctx->backend_error = strdup_fmt(...)`
  assignments from the legacy statement/slot/generic specialization emitters.
  All C backend failures now pass through `transpiler_set_backend_error*`,
  `transpiler_set_mir_inventory_missing(...)`, or
  `transpiler_set_mir_topology_invalid(...)`; the perf/debt smoke rejects
  reintroducing direct backend-error string assignment under `src/codegen`.
  Gates: `pgy`, `test-transpile` (`717/0`),
  `mir-declaration-inventory-test-smoke`, `perf-contract-test-smoke`, and
  `test-inc-size-test-smoke`.
- Completed the matching LLVM MIR-missing diagnostic cleanup for domain/role
  method bodies: direct `PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING` use is now
  centralized in `llvm_set_mir_inventory_missing(...)`, and the declaration
  inventory smoke rejects reintroducing direct cause usage outside
  `llvm_error.c`. Gates: `llvm-test-smoke`, `mir-declaration-inventory-test-smoke`,
  and `perf-contract-test-smoke`. Full backend compare was started as part of a
  combined gate but exceeded the local timeout; no compare failure was observed.
- Added the LLVM-side `llvm_set_mir_topology_invalid(...)` helper and routed
  MIR pin block resolution failures plus missing method owner metadata through
  stable MIR topology code/cause/fix hints. LLVM intent participant metadata
  gaps now route through `llvm_set_mir_inventory_missing(...)`; remaining plain
  `llvm_set_error(...)` calls under LLVM are now OOM/registry failures, not MIR
  inventory/topology diagnostics. Gates: `llvm-test-smoke`,
  `mir-declaration-inventory-test-smoke`, `perf-contract-test-smoke`, and
  `test-inc-size-test-smoke`.
- Moved the shared C backend heap formatting helpers (`strdup_fmt` and `escape_c_string_literal`) out of `transpiler_helpers.h` into `transpiler_format.c`; the helper header no longer duplicates those string-format bodies at include sites, and the Makefile source inventory owns the new formatter. Gates: targeted `gcc -fsyntax-only` for `transpiler_format.c` and `transpiler.c`, `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `pgy`.
- Moved the C backend MIR intent statement query out of `transpiler_helpers.h` into `transpiler_mir_intent_query.c`; intent emission now consumes a linked MIR query helper instead of a static header body, and an unused MIR DEF runtime-call helper was removed from the same header. Gates: targeted `gcc -fsyntax-only` for `transpiler_mir_intent_query.c` and `transpiler.c`, `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `pgy`.
- Moved the C backend concrete MIR resource-op emitter out of `transpiler_helpers.h` into `transpiler_mir_resource_op_core.c`; resource hook emission now calls a linked ABI/resource owner instead of a static header body. Gates: targeted `gcc -fsyntax-only` for `transpiler_mir_resource_op_core.c` and `transpiler.c`, `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `pgy`.
- Moved contextual `Option<T>` lookup and `None()` C emission out of `transpiler_helpers.h` into `transpiler_option_context.c`; `transpiler_helpers.h` is now a small include-order shim rather than an implementation header. Gates: targeted `gcc -fsyntax-only` for `transpiler_option_context.c` and `transpiler.c`, `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, `pgy`, and `test-transpile` (`717/0`).
- Added declaration-only `transpiler_format.h` and moved MIR SSA expression dependencies to the specific MIR emit headers that consume them; `transpiler_helpers.h` no longer acts as the implicit provider for formatter or MIR SSA helper prototypes. Gates: targeted `gcc -fsyntax-only` for `transpiler.c`, `test-inc-size-test-smoke`, and `pgy`.
- Moved C backend `Log` / `LogRaw` / `LogBanner` builtin lowering out of `transpiler_expr_core_builtins_emit.h` into `transpiler_log_builtin_emit.c`; the remaining core-builtin header now starts at wrapper ownership helpers and no longer exports log implementation bodies through the expression include stack. Gates: targeted `gcc -fsyntax-only` for `transpiler_log_builtin_emit.c` and `transpiler.c`, `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `pgy`.
- Moved C backend allocator builtin lowering out of `transpiler_expr_core_builtins_emit.h` into `transpiler_allocator_builtin_emit.c`; the core-builtin header now retains only Rc/Weak/Box bodies because those still depend on the static expression type-inference seam. Gates: targeted `gcc -fsyntax-only` for `transpiler_allocator_builtin_emit.c` and `transpiler.c`, `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `pgy`.
- Added the post-beta self-host handoff folder `docs/self_hosted/` with agent entry rules, staged roadmap, required language surface, and first compiler-adjacent tool candidates. The docs keep self-hosting explicitly post-beta and soft-first: diagnostic catalog checker, AIR graph JSON validator, MIR dump diff, backend comparator, and module/package resolver helper before any compiler rewrite. Gates: `documentation-quality-test-smoke`.
- Split the C backend MIR resource-name helper implementation out of `transpiler_mir_resource_name_helpers.h` into `transpiler_mir_resource_name.c`; the header is now declaration-only and Makefile source inventory tracks the new owner. Broader implementation-header extraction remains ongoing because some wrappers still depend on include-order-local static seams. Gates: targeted `gcc -fsyntax-only` for `transpiler_mir_resource_name.c` and `transpiler.c`, `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `test-transpile` (`717/0`).
- Split C backend event builtin call lowering out of `transpiler_event_builtin_emit.h` into `transpiler_event_builtin_emit.c`; the header is now declaration-only, and the new owner avoids pulling the broader `transpiler_helpers.h` include-order stack by using a local invoke-string formatter. Gates: targeted `gcc -fsyntax-only` for `transpiler_event_builtin_emit.c` and `transpiler.c`, plus `test-transpile` (`717/0`).
- Split C backend intent condition-failure lowering out of `transpiler_intent_failure_emit.h` into `transpiler_intent_failure_emit.c`; the header is now declaration-only and the body consumes only the public context/output helper seam. Gates: targeted `gcc -fsyntax-only` for `transpiler_intent_failure_emit.c` and `transpiler.c`, plus `test-transpile` (`717/0`).
- Split C backend intent emit metadata helpers out of `transpiler_intent_emit_metadata_helpers.h` into `transpiler_intent_emit_metadata_helpers.c`; the remaining header surface is limited to intent-emission control macros that intentionally bind call-site `goto`/context restoration. Gates: targeted `gcc -fsyntax-only` for `transpiler_intent_emit_metadata_helpers.c` and `transpiler.c`, plus `test-transpile` (`717/0`).
- Moved the C backend indentation compatibility wrappers out of `transpiler_context.h` into `transpiler_context.c`; the header now declares `write_indent(...)` / `write_indent_to(...)` instead of embedding inline bodies at every include site. Gates: targeted `gcc -fsyntax-only` for `transpiler_context.c` and `transpiler.c`, plus `test-transpile` (`717/0`).
- Moved ability vtable tag rendering out of `transpiler_role_ability_helpers.h` into the existing `transpiler_role_ability.c` owner, and introduced declaration-only `transpiler_type_mapping.h` so compiled owners can consume type-mapping APIs without pulling implementation headers. Gates: targeted `gcc -fsyntax-only` for `transpiler_role_ability.c` and `transpiler.c`, plus `test-transpile` (`717/0`).
- Moved event declaration/subscription emission out of `transpiler_event_emit.h` into `transpiler_event_emit.c`; the header is now declaration-only, and event lowering now links through a normal C backend owner instead of exporting global function bodies through an implementation header. Gates: targeted `gcc -fsyntax-only` for `transpiler_event_emit.c` and `transpiler.c`, plus `test-transpile` (`717/0`).
- Moved MIR phi-copy emission out of `transpiler_mir_phi_emit.h` into `transpiler_mir_phi_emit.c`; phi lowering now consumes public SSA map/name APIs through a compiled owner, and `transpiler_mir_emit_decls.h` no longer needs a static forward for that seam. Gates: targeted `gcc -fsyntax-only` for `transpiler_mir_phi_emit.c` and `transpiler.c`, plus `test-transpile` (`717/0`).
- Moved function forward-declaration emission out of `transpiler_mir_inventory_intent.h` into `transpiler_func_forward_emit.c`; generic specialization and program bootstrap now share a linked prototype emitter instead of a static header body. Gates: targeted `gcc -fsyntax-only` for `transpiler_func_forward_emit.c` and `transpiler.c`, plus `test-transpile` (`717/0`).
- Moved misc stdlib call lowering out of `transpiler_expr_stdlib_misc_builtin.h` into `transpiler_expr_stdlib_misc_builtin.c`; the new owner uses a local heap-format helper instead of depending on the broader static `strdup_fmt` include-order seam. Gates: targeted `gcc -fsyntax-only` for `transpiler_expr_stdlib_misc_builtin.c` and `transpiler.c`, plus `test-transpile` (`717/0`).
- Tightened `test_inc_size_smoke.sh` so newly declaration-only C backend headers cannot silently grow function bodies again. The first gated headers are `transpiler_context.h`, `transpiler_event_builtin_emit.h`, `transpiler_event_emit.h`, `transpiler_expr_stdlib_misc_builtin.h`, `transpiler_format.h`, `transpiler_helpers.h`, `transpiler_intent_emit_metadata_helpers.h`, `transpiler_intent_failure_emit.h`, `transpiler_mir_inventory_intent.h`, `transpiler_mir_emit_state.h`, `transpiler_mir_phi_emit.h`, `transpiler_mir_resource_name_helpers.h`, and `transpiler_role_ability_helpers.h`; broader implementation-header extraction remains responsibility-first, not a blanket body ban yet. Gates: `bash -n tests/test_inc_size_smoke.sh`, `test-inc-size-test-smoke`, and `build-source-inventory-test-smoke`.
- Lifted MIR resource `Write`, DEF initializer/type, statement binding/defer payloads, loop iterable/range, and CFG terminator expression provenance into instruction payloads: `mir_add_resource_instruction(...)` now records the write value expression in `MIRInstruction.expr0`, C MIR resource emission consumes that payload instead of reopening `inst->ast->data.call`, DEF instructions carry initializer/value `expr0` plus let type annotation `expr1`, statement let/assignment bindings carry `arg0`, for-in LLVM lowering consumes `expr0` for the iterable, defer statements carry body `expr0`, expression branch/retun terminators carry `expr0`, C and LLVM MIR terminator/local/block emission use those payloads where possible, and the MIR validator rejects missing value/initializer/defer-body/terminator expression facts. Codegen now smoke-rejects `inst->ast->data` reopening so AST payload remains source/provenance instead of lowering inventory. Gates: `test-mir` (`44/0`), `test-transpile` (`717/0`), `cfg-body-dataflow-test-smoke`, `llvm-test-smoke`, and `perf-contract-test-smoke`.
- Removed the remaining LLVM range-loop lowering dependency on `inst->ast` for loop init/condition shape checks. The loop-control emitter now uses MIR branch shape plus `arg0`/`expr0`/`expr1` facts for variable/start/end lowering, and the perf contract smoke rejects reintroducing the local `node = inst->ast` probe in `llvm_mir_loop_control.c`. Gates: `llvm-test-smoke` and `perf-contract-test-smoke`.
- Removed the LLVM local alloca type-inference fallback that treated the whole source statement AST as a DEF value expression when `expr0` was absent. `llvm_mir_local_emit.c` now consumes `expr0` for initializer/value and `expr1` for explicit type annotation, while `source_ast_type` remains only a shape discriminator. Gates: `llvm-test-smoke` and `perf-contract-test-smoke`.
- Tightened LLVM MIR block DEF/branch/retun lowering so ordinary expression stores, expression branches, and retun values consume `expr0` directly instead of falling back to the source AST. Let/assignment preserved-statement emission, match-case dispatch, select dispatch, and with-claim compatibility remain explicit source/provenance seams. Gates: `llvm-test-smoke` and `perf-contract-test-smoke`.
- Split LLVM MIR branch condition payload checks by branch shape: expression/range/for-in branches require MIR expression payload, while only match/select compatibility branches are allowed to require the source AST. The perf contract now rejects the broad `inst->ast || inst->expr0` condition gate. Gates: `llvm-test-smoke` and `perf-contract-test-smoke`.
- Matched the C backend branch-condition policy to the LLVM path: range/for-in branches consume MIR loop facts, match-case remains the explicit source compatibility seam, and ordinary expression branches consume `expr0` without falling back to `inst->ast`. Gates: `test-transpile` (`717/0`) and `perf-contract-test-smoke`.
- Lifted intent check/eval expression payloads into MIR `expr0`: `IntentCheck` and `IntentEval` facts now carry their expression payload explicitly, the MIR intent fact validator rejects missing payloads, and C/LLVM intent collectors consume `expr0` instead of treating `inst->ast` as the expression inventory. Step headers and step-scoped metadata still keep the source AST as the explicit provenance seam. Gates: `test-mir` (`44/0`), `test-transpile` (`717/0`), `cfg-body-dataflow-test-smoke`, `llvm-test-smoke`, and `perf-contract-test-smoke`.
- Removed two C backend MIR resource source-pointer guards that were only checking statement shape. Claim-with emission now uses `has_source_location/source_ast_type` instead of `inst->ast == NULL`, and claim materialization suppression uses the same source-shape metadata instead of checking the source pointer. The residual statement mirrored-resource pointer equality remains the intentional provenance seam. Gates: `test-transpile` (`717/0`) and `perf-contract-test-smoke`.
- Tightened the C MIR emission mapping contract to validate branch/retun/write identifier usage from MIR expression payloads instead of rescanning `inst->ast`, and split branch contract requirements by shape: match/select compatibility branches still require source AST, while expression branches require the `expr0` condition fact. Gates: `test-transpile` (`717/0`) and `perf-contract-test-smoke`.
- Removed the C/LLVM intent step-sequence collector that treated `IntentStep` instruction `inst->ast` as the ordered step inventory. Backends now consume MIR `IntentStep` names as the step sequence and map those names back to declaration source steps only for source/provenance and expression emission; missing source mapping is an explicit MIR-only backend error. The perf contract rejects reintroducing `collect_mir_intent_steps`. Gates: `test-transpile` (`717/0`), `llvm-test-smoke`, and `perf-contract-test-smoke`.
- Re-checked the C backend pending-use materialization seam. A direct switch from source `let` AST to DEF `expr0/expr1` regressed implicit `self` host-context emission for class/subject/world methods, so this seam remains intentionally source-backed until MIR carries host-context/implicit-receiver facts alongside the DEF payload. Gates after reverting the unsafe narrowing: `test-transpile` (`717/0`) and `perf-contract-test-smoke`.
- Split MIR declaration-header validator cases out of `test_mir_lowering_part_b.cases.h` into `test_mir_lowering_part_c.cases.h`, bringing part B back under the 990 LOC test-case header gate after the MIR fact regressions were expanded. Gates: `test-inc-size-test-smoke` and `test-mir` (`44/0`).
- Tightened whole-program usage discovery for intent observability and thread-pool runtime requirements: normal lowered MIR now consumes validated surface-usage facts, and the legacy fallback probes only explicit expression payloads (`expr0`/`expr1`) for hand-built MIR fixtures without HIR provenance. Source statement AST scanning is smoke-rejected for both usage paths, and the old `allow_legacy_ast_probe` naming is banned to keep the seam payload-only. Gates: `test-transpile` (`717/0`) and `perf-contract-test-smoke`.
- Renamed the LLVM MIR branch condition gate from `llvm_mir_branch_has_condition_payload(...)` to `llvm_mir_branch_has_required_condition_fact(...)` because match/select compatibility branches still require source AST while ordinary expression/range/for-in branches require MIR expression facts. This keeps the remaining compatibility seam named honestly instead of pretending every branch condition is payload-backed. Gate: `perf-contract-test-smoke`.
- Rechecked DAG source-of-truth closure after the MIR/codegen cleanup: `type-resolution-resolver-inventory-test-smoke` keeps direct resolver and metadata fallback seam inventory at cap 0, and `type-resolution-dag-test-smoke` reports `retired_resolver_calls=0`, `retired_resolver_body_fallbacks=0`, `metadata_dead_ends=0`, and `materializer_unresolved=0` with `metadata_entries=3498` / `metadata_hits=8380`. Remaining DAG work is no longer numeric fallback cleanup; it is reducing owner-local compatibility seams that still need graph evidence in their native owner.
- Refreshed the MIR declaration-inventory smoke after the MIR test-case split: declaration-header validator assertions now live in `test_mir_lowering_part_c.cases.h`, and the smoke follows that owner instead of reporting a false regression against part B. Gates: `mir-declaration-inventory-test-smoke`, `test-inc-size-test-smoke`, and `build-source-inventory-test-smoke`.
- Revalidated AIR after the CFG/MIR/DAG seam tightening. Strict evidence still carries HIR/RIR boundary evidence, MIR cleanup/pin cleanup/terminator evidence, DAG metadata/generic/ability evidence, RIR effect/relation propagation evidence, and observability schema evidence without backend impact. Gates: `test-air` (`77/0`), `air-drift-test-smoke`, and `air-json-schema-test-smoke`.
- Narrowed the C backend pending-use materialization seam without repeating the earlier implicit-self regression: block-local pending bindings now prefer MIR DEF payload facts (`expr0` initializer and `expr1` type annotation) only for `AST_LET_DECL` DEFs, while assignment DEFs are ignored and source let lookup remains a compatibility/provenance fallback. The perf contract gates the payload-first shape and the `AST_LET_DECL` guard. Gates: `test-transpile` (`717/0`) and `perf-contract-test-smoke`.
- Tightened LLVM select readiness lowering to read channel-receive readiness from DEF `expr0` instead of reopening the DEF source assignment AST. The older assignment-AST helper remains only for source select-case compatibility, while MIR CFG target-block readiness now consumes the same expression payload that DEF emission uses. Gates: `llvm-test-smoke` and `perf-contract-test-smoke`.
- Replaced the LLVM MIR receive-target predeclare seam with `llvm_mir_declare_recv_target(arg0, expr0, ...)`, so select/channel DEF lowering no longer needs the source assignment AST to synthesize the receive target alloca. Gates: `llvm-test-smoke` and `perf-contract-test-smoke`.
- Tightened MIR declaration-header inventory one step further: hosted-method views in both C and LLVM now consume MIR declaration metadata directly when a `MIRDeclHeader` exists, and the header no longer carries the AST method-array pointer as inventory state. Remaining AST payload is explicitly named `source_ast` and the old `*_method_ast` accessors are smoke-rejected, so the compatibility seam is visible as source/provenance only until declaration bootstrap is fully MIR-owned. C/LLVM host-decl lookup now uses table-driven owner/type lists instead of open-coded chains, and thread-pool / intent-observability dependency detection now consume program-level MIR surface-usage facts plus neutral `MIRRoutineInventory` before routine fallback so declaration inventory surface does not drift between C and LLVM. Gates: `test-mir` (`41/0`), `test-transpile` (`717/0`), `mir-declaration-inventory-test-smoke`, `parallel-core-contract-test-smoke`, and `perf-contract-test-smoke`.
- Closed the remaining non-runtime include-guard boundary slips found by the guard scanner: `transpiler_async_parallel_emit.h`, `transpiler_block_intent_helpers.h`, `transpiler_control_flow_emit.h`, `transpiler_let_slot_emit.h`, `transpiler_mir_terminator_emit.h`, `transpiler_world_select_event_emit.h`, and `transpiler_mir_emission_contract.h` now keep their full implementation surface inside the declared guard. Gate: guard-boundary scan for `src/codegen`, `src/semantic`, `src/compiler`, and `src/parser`; targeted `gcc -fsyntax-only` for `transpiler.c`; `test-transpile` (`717/0`), `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Split C backend MIR emit-state snapshot/restore helpers out of `transpiler_mir_emit_state.h` into `transpiler_mir_emit_state.c`; the header now exposes the `TranspilerMirEmitState` shape and explicit state APIs while keeping only the include-order forward declarations needed by condition emitters. Removed the now-dead `lookup_generic_binding(...)` static helper from `transpiler_helpers_core_b.h` after type rendering moved generic binding lookup into its own owner. Gates: targeted `gcc -fsyntax-only` for `transpiler_mir_emit_state.c` and `transpiler.c`; `test-transpile` (`717/0`), `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Refreshed runtime panic contract smoke after LLVM expression owners moved from implementation headers into compiled owners. Checked arithmetic now verifies `llvm_expr_scalar_core.c`, checked Result/Option unwrap verifies `llvm_expr_result_option_calls.c`, and array mutation lowering verifies `llvm_expr_array_calls.c`, while the public headers remain declaration-only seams. Gates: `runtime-panic-contract-test-smoke`, `runtime-panic-abi-test-smoke`, `runtime-panic-codegen-test-smoke`, `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Revalidated runtime propagation frontier closure after the runtime/codegen owner cleanup. Bounded zone/world/projection frontier pass-limit arithmetic is gated, generated C frontier code uses the transitive embedded-world limit, and LLVM frontier emitters keep overflow panic/unreachable paths for zone, world, derived-world, and projection recompute. Gates: `runtime-frontier-contract-test-smoke` and `runtime-frontier-policy-test-smoke`.
- Revalidated AIR as a verification-only abstraction boundary layer after the CFG/DAG/runtime owner cleanup. `test-air` remains `77/0`, `air-json-schema-test-smoke` parses `pgy.air.graph.v1`, and `air-backend-nonimpact-full-test-smoke` passes the full C/LLVM backend-compare corpus with AIR enabled, preserving codegen non-impact while strict evidence stays active. Gates: `air-drift-test-smoke`, `air-json-schema-test-smoke`, and `air-backend-nonimpact-full-test-smoke`.
- Refreshed `cfg_body_dataflow_smoke.sh` after MIR CFG/body helpers moved from static headers into compiled owners. The smoke now reads the declaration headers plus their `.c` owners for call facts, non-CFG statement population, CFG contract control, pin/cleanup-fact helpers, and validator bodies instead of reporting false regressions against declaration-only headers. Gate: `cfg-body-dataflow-test-smoke`.
- Revalidated the DAG source-of-truth gates after the owner-split work: retired recursive resolver calls remain `0`, resolver body fallbacks remain `0`, metadata dead-ends remain `0`, materializer unresolved remains `0`, metadata entries are `3498`, owned constructed metadata entries are `258`, and metadata hits are `8380`. Gates: `type-resolution-dag-test-smoke` and `type-resolution-resolver-inventory-test-smoke`.
- Corrected `test_inc_size_smoke.sh` wording so the gate states its actual contract: no production `.inc` files, no `_IMPLEMENTATION` header blocks, production owners <= 600 LOC, and test case headers <= 990 LOC. Runtime public inline headers remain a separate, explicit runtime ABI/codegen contract instead of being hidden by over-broad gate wording. Gates: `test-inc-size-test-smoke`.
- Split LLVM runtime intent active-index helpers out of `pgy_runtime_lib_intent_active_index_exports.h` into private `pgy_runtime_lib_intent_active_index_exports.c`; the compatibility header no longer embeds static helper bodies, and both the runtime cache manifest and Makefile source inventory now track the trace export owner and active-index owner. Gates: targeted `gcc -fsyntax-only` for `pgy_runtime_lib.c` and `compiler_runtime_cache.c`; `llvm-test-smoke`, `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Split LLVM runtime intent trace export implementation out of `pgy_runtime_lib_set_intent_trace_exports.h` into private `pgy_runtime_lib_set_intent_trace_exports.c`; the header is now declaration-only, `pgy_runtime_lib.c` keeps the single-runtime-TU contract by including the private implementation owner, and runtime cache invalidation now tracks the implementation file directly. Removed the now-unused private append helper surfaced by the split. Gates: targeted `gcc -fsyntax-only` for `pgy_runtime_lib.c` and `compiler_runtime_cache.c`; `llvm-test-smoke`, `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Split C backend type rendering out of `transpiler_type_render_helpers.h` into `transpiler_type_render.c`; `transpiler_type_render_helpers.h` is now a compatibility shim over the declaration-only type render API, and the render context binding now has one linked owner instead of a static header global. Gates: targeted `gcc -fsyntax-only` for `transpiler_type_render.c`, `transpiler.c`, and `mir_cleanup_fact_names.c`; `test-transpile` (`717/0`), `test-mir` (`41/0`), `test-air` (`76/0`), `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Promoted `mir_cleanup_edge_fact_name_for_block(...)` from a static inline body in `mir_cleanup_fact_names.h` into `mir_cleanup_fact_names.c`; cleanup fact vocabulary stays in the header, while C/LLVM MIR emission contracts now include the declaration explicitly. Gates: targeted `gcc -fsyntax-only` for `mir_cleanup_fact_names.c`, `transpiler.c`, and `llvm_mir_contract.c`; `test-transpile` (`717/0`), `test-mir` (`41/0`), `test-air` (`76/0`), `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Split non-CFG MIR body statement population out of `mir_non_cfg_stmt_population.h` into `mir_non_cfg_stmt_population.c`; `src/compiler` now has no remaining static implementation bodies in headers by the current static-header scan. Gates: targeted `gcc -fsyntax-only` for `mir_non_cfg_stmt_population.c` and `mir.c`; `test-mir` (`41/0`), `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Split MIR ABI layout table and lookup implementation out of `mir_abi_layout.h` into `mir_abi_layout.c`; the header is now declaration-only, while the ABI type layout table remains the single MIR-side lookup owner backed by `pgy_abi_spec.h`. Gates: targeted `gcc -fsyntax-only` for `mir_abi_layout.c` and `mir.c`; `test-mir` (`41/0`), `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Split MIR CFG contract state validation out of `mir_cfg_contract_validate.h` into `mir_cfg_contract_validate.c`; the header is now declaration-only, and the validator consumes explicit MIR CFG contract APIs instead of embedding the full validation body in `mir.c`. Gates: targeted `gcc -fsyntax-only` for `mir_cfg_contract_validate.c` and `mir.c`; `test-mir` (`41/0`), `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Split MIR call-fact attachment helpers out of `mir_call_fact.h` into `mir_call_fact.c`; MIR statement population and non-CFG fallback population now share one compiled call-fact owner instead of embedding static helper bodies. Gates: targeted `gcc -fsyntax-only` for `mir_call_fact.c`, `mir_stmt_population.c`, and `mir.c`; `test-mir` (`41/0`), `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Split MIR CFG contract pin/control/cleanup-fact helpers out of `mir_cfg_contract_pin.h`, `mir_cfg_contract_control.h`, and `mir_cfg_contract_cleanup_fact.h` into compiled owners. AIR evidence now consumes the same linked pin and cleanup-fact APIs as MIR validation instead of relying on copied static header bodies. Gates: targeted `gcc -fsyntax-only` for `mir_cfg_contract_pin.c`, `mir_cfg_contract_control.c`, `mir_cfg_contract_cleanup_fact.c`, `mir.c`, and `air_evidence.c`; `test-mir` (`41/0`), `test-air` (`76/0`), `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Split MIR cleanup-root membership out of `mir_cfg_contract_cleanup_root_membership.h` into `mir_cfg_contract_cleanup_root_membership.c`; AIR evidence and MIR validation now share one linked membership owner instead of duplicating a static header body. Gates: targeted `gcc -fsyntax-only` for `mir_cfg_contract_cleanup_root_membership.c`, `mir.c`, and `air_evidence.c`; `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, `test-mir` (`41/0`), and `test-air` (`76/0`).
- Split MIR CFG contract root validators out of the single-include `mir_cfg_contract_roots.h` and `mir_cfg_contract_cleanup_roots.h` implementation headers into `mir_cfg_contract_roots.c` and `mir_cfg_contract_cleanup_roots.c`; both headers are now declaration-only, and `mir_cfg_contract_validate.h` now declares its cleanup-root membership dependency directly instead of relying on an indirect include. Gates: targeted `gcc -fsyntax-only` for `mir.c`, `mir_cfg_contract_roots.c`, and `mir_cfg_contract_cleanup_roots.c`; `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `test-mir` (`41/0`).
- Split formatter I/O and parseability helpers out of the single-include `fmt_io.h` implementation header into `fmt_io.c`; `fmt_io.h` is now declaration-only and `fmt.c` keeps only the stream-local formatting roundtrip helper that depends on `format_source_to_stream`. Gates: targeted `gcc -fsyntax-only` for `fmt.c` and `fmt_io.c`, `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `fmt-test-smoke`.
- Completed the remaining non-runtime header guard sweep for `src/codegen`, `src/compiler`, `src/semantic`, and `src/parser`: all previously guard-less implementation/helper headers now reject accidental double inclusion. This is still a safety pass, not a claim that every implementation header has been extracted into a `.c` owner. Gates: targeted `gcc -fsyntax-only` for `transpiler.c`, `type_checker.c`, `mir.c`, and `fmt.c`; `test-transpile` (`717/0`), `test-semantic` (`2500/0`), and `test-mir` (`41/0`).
- Added include guards to another semantic/codegen implementation-helper batch: `type_checker_context_helpers.h`, `transpiler_func_forward_helpers.h`, `transpiler_expr_stdlib_map_builtin.h`, `transpiler_intent_observability_builtin_emit.h`, `transpiler_class_decl_emit.h`, `transpiler_mir_emission_mapping_contract.h`, `transpiler_mir_pending_uses.h`, and `transpiler_projection_method_invalidation.h`. Gates: targeted `gcc -fsyntax-only` for `type_checker.c` and `transpiler.c`, `test-transpile` (`717/0`), and `test-semantic` (`2500/0`).
- Added include guards to ten smaller C backend implementation headers: `transpiler_enum_decl_emit.h`, `transpiler_intent_zone_binding_emit.h`, `transpiler_mir_terminator_emit.h`, `transpiler_overlay_world_projection.h`, `transpiler_mir_func_ssa_locals_emit.h`, `transpiler_intent_prologue_emit.h`, `transpiler_mir_emit_state.h`, `transpiler_let_box_emit.h`, `transpiler_relation_effect_emit.h`, and `transpiler_expr_stdlib_scalar_builtin.h`. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `test-transpile` (`717/0`).
- Added include guards to the single-TU semantic/MIR implementation headers `type_checker_generic_contracts.h`, `type_checker_async_channel.h`, `type_checker_generic_support.h`, `mir_public_surface.h`, `mir_lower_public_api.h`, `mir_abi_layout.h`, and `mir_cfg_contract_validate.h`. These remain implementation headers for now; the guard pass only removes accidental double-include risk before any later `.c` owner extraction. Gates: targeted `gcc -fsyntax-only` for `type_checker.c` and `mir.c`, `test-semantic` (`2500/0`), and `test-mir` (`41/0`).
- Added include guards to the C backend domain/spawn/overlay/MIR implementation-header batch: `transpiler_domain_provenance_emit.h`, `transpiler_spawn_channel_emit.h`, `transpiler_overlay_projection.h`, `transpiler_mir_ssa_emit.h`, and `transpiler_mir_cfg_control_emit.h`. This keeps MIR/CFG-related lowering single-include safe while the actual owner extraction remains a separate debt item. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `test-transpile` (`717/0`).
- Added include guards to the next C backend control/MIR/generic implementation-header batch: `transpiler_block_intent_helpers.h`, `transpiler_mir_match_condition_emit.h`, `transpiler_mir_local_type_lookup.h`, `transpiler_intent_cleanup_emit.h`, `transpiler_generic_class_specialization_emit.h`, `transpiler_statement_dispatch.h`, `transpiler_control_flow_emit.h`, and `transpiler_expr_stdlib_channel_builtin.h`. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `test-transpile` (`717/0`).
- Added include guards to C backend stdlib/match/function-flow implementation headers: `transpiler_expr_stdlib_collection_builtin.h`, `transpiler_match_emit.h`, `transpiler_let_slot_emit.h`, `transpiler_func_class_flow_emit.h`, and `transpiler_expr_stdlib_builtin.h`. This specifically guards the stdlib builtin parent/child include chain before future extraction. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `test-transpile` (`717/0`).
- Added include guards to another C backend implementation-header batch: `transpiler_expr_type_infer.h`, `transpiler_mir_func_emit.h`, `transpiler_world_select_event_emit.h`, `transpiler_domain_nominal_emit.h`, and `transpiler_async_parallel_emit.h`. The pass preserves the current single-include implementation model but removes accidental double-include redefinition risk from more of the C backend surface. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `test-transpile` (`717/0`).
- Added include guards to four more C backend implementation headers: `transpiler_expr_core_builtins_emit.h`, `transpiler_call_constructor_result_emit.h`, `transpiler_projection_sync_helpers.h`, and `transpiler_mir_emission_contract.h`. This keeps the current include-order implementation model stable while future extraction remains responsibility-first instead of size-only. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `test-transpile` (`717/0`).
- Added include guards to the next C backend implementation-header batch: `transpiler_domain_role_ability_emit.h`, `transpiler_specialization_helpers.h`, and `transpiler_expr_call_spawn_emit.h`. These guards do not claim extraction completion; they make the remaining include-order debt safer while responsibility-owned `.c` splits are staged. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `test-transpile` (`717/0`).
- Added include guards to C backend expression/let dispatch implementation headers `transpiler_let_emit.h`, `transpiler_expr_dispatch_emit.h`, and `transpiler_expr_builtin_dispatch.h`. These remain implementation headers, but accidental double-include redefinition is now blocked while the future `.c` owner extraction is staged behind explicit helper APIs. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `test-transpile` (`717/0`).
- Added include guards to high-traffic C backend implementation headers `transpiler_mir_block_emit.h`, `transpiler_helpers.h`, `transpiler_intent_emit.h`, and `transpiler_zone_decl_emit.h`. This is a structural safety pass, not a replacement for future responsibility-owned `.c` extraction. Gates: targeted `gcc -fsyntax-only` for `transpiler.c`, `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `test-transpile` (`717/0`).
- Added include guards to the three C backend include-order shims `transpiler_base_a_emitters.h`, `transpiler_base_b_emitters.h`, and `transpiler_domain_role_emit.h`. This does not remove the shim debt, but it prevents accidental double-include redefinition while the remaining C backend helper APIs are extracted responsibility-first. Gates: targeted `gcc -fsyntax-only` for `transpiler.c`, `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `test-transpile` (`717/0`).
- Refreshed `mir_declaration_inventory_smoke.sh` after declaration-inventory implementation headers moved into compiled owners. The gate now distinguishes declaration-only headers from implementation owners for LLVM inventory lookup, LLVM hosted method views, C hosted method views, C role MIR lookup, and transpiler inventory views. Gates: `mir-declaration-inventory-test-smoke`, `test-mir` (`41/0`), `build-source-inventory-test-smoke`, and `test-inc-size-test-smoke`.
- Split codegen frontier policy AST wrappers out of `domain_frontier_policy.h` into `domain_frontier_policy.c`; the runtime frontier arithmetic remains the source of truth in `pgy_frontier_policy.h`, while C/LLVM domain emitters now consume a declaration-only codegen compatibility seam. Gates: `runtime-frontier-contract-test-smoke`, `runtime-frontier-policy-test-smoke`, `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `test-transpile` (`717/0`).
- Split LLVM Result/Option expression call lowering out of the static include stack into `llvm_expr_result_option_calls.c`; the header is now declaration-only. Gates: `llvm-test-smoke`, `build-source-inventory-test-smoke`.
- Split LLVM Rc/Weak expression call lowering out of the static include stack into `llvm_expr_rc_calls.c`; this exposed and removed a hidden dependency on a call-owner-local runtime lookup helper. Gates: `llvm-test-smoke`, `build-source-inventory-test-smoke`.
- Promoted LLVM runtime dependency diagnostics into `llvm_runtime_require.c` / `llvm_required_runtime_function(...)`, replacing a static implementation hidden in `llvm_expr_call_owners.h` with a private API owner consumed by expression call emitters. Gates: `llvm-test-smoke`, `build-source-inventory-test-smoke`.
- Split LLVM expression common helpers out of `llvm_expr_host_spawn_literal_helpers.h` into `llvm_expr_common.c`; self-base lookup, host-method lookup, operator suffix, custom nominal type inference, enum lookup, and literal number/string emission are now private API functions instead of a single-include implementation-header body. The remaining header body is limited to projection binding glue that still depends on local projection-path helpers. Gates: targeted `gcc -fsyntax-only` for `llvm_expr.c` and `llvm_expr_common.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Promoted LLVM generic function-call argument lowering into `llvm_expr_call_args.c` / `llvm_emit_function_call_args(...)`, removing another include-order dependency from expression-call owners and enabling standalone call-family emitters. Gates: `llvm-test-smoke`, `build-source-inventory-test-smoke`.
- Split LLVM intent-observability call lowering out of the static include stack into `llvm_expr_intent_observability_calls.c`; the header is now declaration-only and consumes the shared call-args API. Gates: `llvm-test-smoke`, `build-source-inventory-test-smoke`.
- Split LLVM aggregate expression utilities (`llvm_array_data_ptr`, `llvm_array_length_i64`, `llvm_build_option_value`, device-slot inner lookup) out of `llvm_expr_boundary_projection_helpers.h` into `llvm_expr_aggregate_utils.c`, reducing boundary/projection include responsibility before the next high-risk split. Gates: `llvm-test-smoke`, `build-source-inventory-test-smoke`.
- Split LLVM domain lookup and host-context helpers out of `llvm_expr_boundary_projection_helpers.h` into `llvm_domain_lookup.c`, moving named domain lookup, zone/world state lookup, world-zone resolution, zone slot lookup, nominal host-method lookup, and current host-class name behind private API declarations. A follow-up moved pointer-self type policy and current host-field class lookup into the same owner, reducing `llvm_expr_boundary_projection_helpers.h` from 441 LOC / 20 static bodies to 185 LOC / 7 static bodies. Gates: targeted `gcc -fsyntax-only` for `llvm_domain_lookup.c`, `llvm_expr.c`, and `llvm_expr_common.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM slot read/write/release lowering utilities out of `llvm_expr_identifier_slot_helpers.h` into `llvm_expr_slot_runtime_utils.c`, moving structural secure-slot fallback, runtime argument conversion, and token-pair diagnostics behind private API declarations. This reduced `llvm_expr_identifier_slot_helpers.h` from 476 LOC / 19 static bodies to 203 LOC / 4 static bodies. Gates: `llvm-test-smoke`, `build-source-inventory-test-smoke`.
- Split LLVM hosted method inventory view helpers out of `llvm_inventory_host_methods.h` into `llvm_inventory_host_methods.c`; the header is now a 46-line declaration/shape owner instead of a static implementation body. Gates: `llvm-test-smoke`, `build-source-inventory-test-smoke`.
- Split LLVM MIR-backed declaration lookup helpers out of `llvm_inventory_decl_lookup.h` into `llvm_inventory_decl_lookup.c`; the header is now a 25-line declaration owner and keeps party/role/roster host fallback parity in the single lookup owner. Gates: `llvm-test-smoke`, `build-source-inventory-test-smoke`.
- Split C backend intent context/binding lookup helpers out of `transpiler_block_intent_helpers.h` into `transpiler_intent_context.c`, reducing `transpiler_block_intent_helpers.h` from 452 LOC / 18 static bodies to 354 LOC / 11 static bodies. Gates: `test-transpile`, `build-source-inventory-test-smoke`.
- Split C backend hosted method view helpers out of `transpiler_decl_lookup.h` into `transpiler_decl_method_view.c`; the header now declares the MIR-backed view API instead of embedding 13 static inline bodies in every consumer. Gates: `test-transpile`, `build-source-inventory-test-smoke`.
- Removed the LLVM `ChannelClose` fake integer fallback: the channel close lowering now retuns the generated void call instead of materializing `i32 0`, keeping task/channel builtins aligned with the explicit failure-helper policy. Gates: `backend-inc-size-test-smoke`, `llvm-test-smoke`.
- Split LLVM domain-query argument/false-constant utilities out of `llvm_expr_domain_query_calls.h` into `llvm_expr_domain_query_utils.c`, reducing the query header before the next include-order-sensitive domain split. Gates: `llvm-test-smoke`, `build-source-inventory-test-smoke`.
- Split LLVM domain-query call lowering out of `llvm_expr_domain_query_calls.h` into `llvm_expr_domain_query_calls.c`; the header now exposes only `llvm_emit_domain_query_call(...)`, and the implementation consumes explicit LLVM intenal/inventory APIs instead of living in the expression-call include stack. Gates: targeted `gcc -fsyntax-only -DPGY_LLVM_ENABLED` for `llvm_expr.c` and `llvm_expr_domain_query_calls.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM domain projection-value lowering out of the include-order pair `llvm_domain_projection_value_helpers.h` + `llvm_domain_projection_sync_body_helpers.h` into `llvm_domain_projection_value_helpers.c`; the projection-value header is now declaration-only, and zone/domain sync bodies call explicit domain projection APIs instead of completing a function body through textual include order. Gates: targeted `gcc -fsyntax-only -DPGY_LLVM_ENABLED` for `llvm_domain_projection_value_helpers.c`, `llvm_domain_method_emit.c`, and `llvm_domain_zone_sync.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM expression projection path/`SubjectProjection` lowering out of `llvm_expr_projection_path_helpers.h` into `llvm_expr_projection_path_helpers.c`; boundary projection, host-spawn literal, member-access, and call-dispatch paths now consume declarations instead of relying on a static helper body injected by include order. Gates: targeted `gcc -fsyntax-only -DPGY_LLVM_ENABLED` for `llvm_expr.c`, `llvm_expr_projection_path_helpers.c`, and `llvm_domain_projection_value_helpers.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM constructor-call lowering out of `llvm_expr_constructor_calls.h` into `llvm_expr_constructor_calls.c`; enum variant construction, shared-field defaults, projection dirty defaults, and world dirty defaults now live in one compiled constructor owner while the call-dispatch include stack consumes only `llvm_emit_constructor_call(...)`. Gates: targeted `gcc -fsyntax-only -DPGY_LLVM_ENABLED` for `llvm_expr.c` and `llvm_expr_constructor_calls.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM banner-string normalization out of `llvm_expr_banner_string_helpers.h` into `llvm_expr_banner_string_helpers.c`; log/banner lowering now consumes a declaration-only normalizer seam instead of embedding the string dedent implementation in the expression include stack. Gates: targeted `gcc -fsyntax-only -DPGY_LLVM_ENABLED` for `llvm_expr.c` and `llvm_expr_banner_string_helpers.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM string coercion and log-family lowering out of static expression headers into `llvm_expr_string_coerce.c` and `llvm_expr_log_calls.c`; scalar, stdlib `ToString`, and log/banner calls now consume explicit string-conversion/log APIs instead of sharing include-order static bodies. Gates: targeted `gcc -fsyntax-only -DPGY_LLVM_ENABLED` for `llvm_expr.c`, `llvm_expr_string_coerce.c`, and `llvm_expr_log_calls.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM member-lvalue and boundary-call argument helpers out of expression implementation headers into `llvm_expr_member_lvalue.c` and `llvm_expr_boundary_projection_helpers.c`; assignment, boundary projection, call dispatch, spawn, and member-call emitters now share explicit lvalue/boundary APIs instead of relying on `llvm_expr.c` local forward declarations plus include-order static bodies. Gates: targeted `gcc -fsyntax-only -DPGY_LLVM_ENABLED` for `llvm_expr.c`, `llvm_expr_member_lvalue.c`, and `llvm_expr_boundary_projection_helpers.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM boundary slot parameter classification into neutral `llvm_boundary_slot_param.c`; declaration emission and expression boundary-call lowering now consume one boundary ABI classifier instead of maintaining duplicate `Slot` / `SecureSlot` generic argument logic. Gates: targeted `gcc -fsyntax-only -DPGY_LLVM_ENABLED` for `llvm_boundary_slot_param.c`, `llvm_decl.c`, and `llvm_expr_boundary_projection_helpers.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM scalar expression core out of `llvm_expr_scalar_core.h` into `llvm_expr_scalar_core.c`; binary/unary lowering and callable signature construction now live in a compiled owner while call dispatch and expression dispatch consume declaration-only scalar APIs. Gates: targeted `gcc -fsyntax-only -DPGY_LLVM_ENABLED` for `llvm_expr.c` and `llvm_expr_scalar_core.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM task/channel builtin call lowering out of `llvm_expr_task_channel_calls.h` into `llvm_expr_task_channel_calls.c`; cancellation, channel close/status/timeout/query calls now live in a compiled owner and consume existing intenal API declarations instead of a static call-owner implementation header. Gates: targeted `gcc -fsyntax-only -DPGY_LLVM_ENABLED` for `llvm_expr.c` and `llvm_expr_task_channel_calls.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM active MIR/DIR inventory accessors out of `llvm_inventory_intenal.h` into `llvm_inventory_intenal.c`; the intenal inventory header is now a type/prototype surface instead of embedding accessor bodies in every LLVM consumer. Gates: `llvm-test-smoke`, `build-source-inventory-test-smoke`.
- Split C backend function forward-declaration policy out of `transpiler_func_forward_helpers.h` into `transpiler_func_forward_policy.c`, leaving the helper header focused on prototype emission/generic specialization scaffolding. Gates: `test-transpile`, `build-source-inventory-test-smoke`.
- Split C backend hosted-method forward declaration emission out of `transpiler_func_forward_helpers.h` into `transpiler_func_forward_metadata.c`; MIR declaration metadata now has a compiled C owner for hosted method prototype emission instead of a shared static implementation header body. This reduced `transpiler_func_forward_helpers.h` from 246 LOC / 9 static bodies to 174 LOC / 7 static bodies. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `transpiler_func_forward_metadata.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split C backend type-name mangling out of `transpiler_func_forward_helpers.h` into `transpiler_mangled_name.c`; generic specialization and hosted-method prototype emitters now share one compiled mangling owner instead of a static helper body. Gates: `test-transpile`, `build-source-inventory-test-smoke`.
- Split C backend slot target/device-slot resolution out of `transpiler_slot_builtin_emit.h` into `transpiler_slot_target.c`; slot builtin emission now consumes one compiled slot-target owner shared by builtin and user-call forwarding paths. Gates: `test-transpile`, `build-source-inventory-test-smoke`.
- Split C backend slot builtin expression lowering out of `transpiler_slot_builtin_emit.h` into `transpiler_slot_builtin_emit.c`; the slot builtin header is now declaration-only, user-call emission now includes `transpiler_slot_target.h` directly instead of relying on transitive include order, and duplicate slot builtin prototypes were removed from the public C backend header. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `transpiler_slot_builtin_emit.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split C backend MIR SSA map primitives out of `transpiler_mir_ssa_map.h` into `transpiler_mir_ssa_map.c`; the SSA map type/API is now a real private owner instead of a `transpiler.c` local typedef plus static implementation header. Gates: `test-transpile`, `build-source-inventory-test-smoke`.
- Split C backend MIR SSA classification/list utilities out of `transpiler_mir_ssa_names.h` into `transpiler_mir_ssa_utils.c`; slot/view/claim-shape classification, CFG-shape checks, versioned-name list helpers, and scalar-zero policy now have a compiled owner. Gates: `test-transpile`, `build-source-inventory-test-smoke`.
- Split C backend MIR SSA expression-map binding out of `transpiler_mir_ssa_emit.h` into `transpiler_mir_expr_ssa.c`; MIR block/terminator/assignment emitters now consume a compiled SSA expression seam instead of a static helper body for temporarily binding `ctx->active_ssa_map`. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `transpiler_mir_expr_ssa.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM MIR block emission out of the single-include implementation header into `llvm_mir_block_emit.c`; `llvm_mir_block_emit.h` now exposes only the MIR block emission seam and includes the concrete `LLVMMirVar` type owner. This keeps MIR block lowering in the build inventory instead of relying on static header bodies. Gates: targeted `gcc -fsyntax-only` for `llvm_mir_block_emit.c` and `llvm_mir_emit.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM MIR local/parameter alloca emission and MIR type/boundary-slot helpers out of static implementation headers into `llvm_mir_local_emit.c` and `llvm_mir_type_helpers.c`; their headers now publish declarations only, so MIR function emission no longer depends on include-order static bodies for local allocation/type layout decisions. Gates: targeted `gcc -fsyntax-only` for `llvm_mir_local_emit.c`, `llvm_mir_type_helpers.c`, and `llvm_mir_emit.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM assignment-driven projection invalidation/world-embedded sync out of `llvm_expr_assignment_projection.h` into `llvm_expr_assignment_projection.c`; the header now exposes only the projection invalidation/sync seams consumed by assignment and projection-sync call lowering. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_assignment_projection.c` and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM spawn/await expression lowering out of `llvm_expr_spawn_call_helpers.h` into `llvm_expr_spawn_call_helpers.c`; the header now publishes only await, spawn, and shared generic callee-resolution seams used by expression call dispatch. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_spawn_call_helpers.c` and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM zone effect/relation layer binding out of the two-TU static implementation header `llvm_domain_zone_bind_helpers.h` into `llvm_domain_zone_bind_helpers.c`; zone sync and relation sync now share one compiled owner instead of duplicating the bind lowering body in each translation unit. Gates: targeted `gcc -fsyntax-only` for `llvm_domain_zone_bind_helpers.c`, `llvm_domain_zone_sync.c`, and `llvm_domain_zone_sync_relations.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM identifier/slot auto-read lowering and projection-borrow literal construction out of `llvm_expr_identifier_slot_helpers.h` / `llvm_expr_host_spawn_literal_helpers.h` into compiled owners; their headers now expose only identifier, boolean, slot-target, and projection-binding seams. This also surfaced and fixed a hidden include-order dependency by making `llvm_expr_slot_device_calls.h` include `codegen_slot_type_policy.h` directly. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_identifier_slot_helpers.c`, `llvm_expr_host_spawn_literal_helpers.c`, and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM extended collection call lowering out of `llvm_expr_call_collections_extended.h` into `llvm_expr_call_collections_extended.c`, and split queue-specific extended lowering into `llvm_expr_call_queue_extended.c` instead of letting the collection owner exceed the 600 LOC gate. The collection headers now publish only runtime/type-check helper and call-lowering seams consumed by the expression call owner. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_call_collections_extended.c`, `llvm_expr_call_queue_extended.c`, and `llvm_expr.c`; `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `test-inc-size-test-smoke`, and `LLVM_ENABLED=1 pgy`.
- Split MIR base lowering helpers out of `mir_base_helpers.h` into `mir_base_helpers.c`; the MIR helper header now publishes only append/copy/versioning/RIR-scope lookup seams consumed by the MIR lowering owner and its intenal implementation headers. Gates: targeted `gcc -fsyntax-only` for `mir.c` and `mir_base_helpers.c`; `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, `test-mir` (`41/0`), and `LLVM_ENABLED=1 pgy`.
- Split MIR SSA rename and use-edge population out of `mir_ssa_rename.h` / `mir_ssa_use_edges.h` into `mir_ssa_rename.c`; SSA rename now has a prototype-only seam while DEF instruction construction is owned by `mir_base_helpers.c`. Gates: targeted `gcc -fsyntax-only` for `mir.c`, `mir_base_helpers.c`, and `mir_ssa_rename.c`; `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, `test-mir` (`41/0`), and `LLVM_ENABLED=1 pgy`.
- Split MIR declaration-header inventory helpers out of `mir_decl_headers.h` into `mir_decl_headers.c`; declaration metadata bootstrap now has a compiled owner and the header publishes only declaration recording/linking seams. Gates: targeted `gcc -fsyntax-only` for `mir.c`, `mir_decl_headers.c`, and `mir_ssa_rename.c`; `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, `test-mir` (`41/0`), and `LLVM_ENABLED=1 pgy`.
- Split RIR flow-state merge policy out of `rir_flow_state.h` into `rir_flow_state.c`; RIR validation and CFG flow enrichment now consume a declaration-only state lattice seam instead of duplicating merge policy through static header bodies. Gates: targeted `gcc -fsyntax-only` for `rir_flow.c`, `rir_validation.c`, and `rir_flow_state.c`; `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `test-rir` (`18/0`).
- Split formatter token spacing/layout helpers out of `fmt_layout.h` into `fmt_layout.c`; the formatter header is now guarded and declaration-only while `pgy fmt` layout policy lives in a compiled owner. Gates: targeted `gcc -fsyntax-only` for `fmt.c` and `fmt_layout.c`; `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `pgy`.
- Removed the single-include semantic operator implementation header `type_checker_operator_expr.h`; operator-overload helpers and array-literal checking now live directly in their only compiled owner, `type_checker_expr_ops.c`, while the public array-literal seam remains in `type_checker.h`. Gates: targeted `gcc -fsyntax-only` for `type_checker_expr_ops.c` and `type_checker_expr.c`; `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`.
- Split C backend primitive/constructed type mapping out of `transpiler_type_mapping_helpers.h` into `transpiler_type_mapping.c`; `pergyra_type_to_c`, slot inner parsing, generic suffix sanitization, and constructed-arg extraction now live in a compiled owner while the remaining helper header keeps only the include-order-dependent channel/result/type-render glue. Gates: targeted `gcc -fsyntax-only` for `transpiler_type_mapping.c` and `transpiler.c`; `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, `pgy`, and `test-transpile` (`717/0`).
- Split C backend role/ability AST membership queries out of `transpiler_role_ability_helpers.h` into `transpiler_role_ability.c`; `role_has_ability` and `role_has_method` now link through a compiled owner while the header keeps only the include-order-dependent ability vtable tag renderer. Gates: targeted `gcc -fsyntax-only` for `transpiler_role_ability.c` and `transpiler.c`; `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, and `pgy`.
- Split LLVM world-embedded action/effect sync out of `llvm_expr_call_methods_world_effect_sync.h` into `llvm_expr_call_methods_world_effect_sync.c`; member-call lowering now consumes one declaration-only sync seam while domain inventory lookup and effect-slot synchronization stay in a compiled owner. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_call_methods_world_effect_sync.c` and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM stdlib scalar/string/file/runtime-IO call lowering out of `llvm_expr_stdlib_scalar_io_calls.h` into `llvm_expr_stdlib_scalar_io_calls.c`; call dispatch now consumes declaration-only stdlib seams instead of embedding runtime-call helper bodies through include order. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_stdlib_scalar_io_calls.c` and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM member-call slot-method and Slice lowering out of `llvm_expr_call_methods_domain_slice.h` into `llvm_expr_call_methods_domain_slice.c`; `llvm_member_call_emit.h` now declares its vtable/domain-slice/world-effect dependencies directly instead of inheriting them through domain-slice include order. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_call_methods_domain_slice.c` and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM member-call projection synchronization out of `llvm_expr_call_projection_sync.h` into `llvm_expr_call_projection_sync.c`; member-call lowering now consumes explicit projection-sync seams and no longer relies on expression include order for world/zone projection dirty/ready propagation. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_call_projection_sync.c` and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM Array builtin lowering and base List/Set collection lowering out of `llvm_expr_array_calls.h` / `llvm_expr_collection_base_calls.h` into compiled owners; call dispatch now consumes declaration-only array/collection seams while shared collection runtime diagnostics stay in the extended collection owner. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_array_calls.c`, `llvm_expr_collection_base_calls.c`, and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM member-access expression lowering and vtable member-call dispatch out of `llvm_expr_member_access.h` / `llvm_expr_call_methods_vtable_dispatch.h` into compiled owners; expression/member-call dispatch now consume declaration-only seams for enum-qualified member access, projection-borrow reads, and party/role vtable calls. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_member_access.c`, `llvm_expr_call_methods_vtable_dispatch.c`, and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM domain declaration-part extraction and projection-sync wrapper emission out of `llvm_domain_decl_parts_helpers.h` / `llvm_domain_projection_sync_helpers.h` into compiled owners; domain method/struct registration now share declaration-only seams instead of duplicating AST inventory slicing and sync wrapper emission through headers. Gates: targeted `gcc -fsyntax-only` for `llvm_domain_decl_parts_helpers.c`, `llvm_domain_projection_sync_helpers.c`, `llvm_domain_method_emit.c`, and `llvm_domain_struct_register.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM assignment lowering out of `llvm_expr_assignment_member_projection.h` into `llvm_expr_assignment_member_projection.c`; expression dispatch now consumes explicit assignment/member-access seams instead of getting member access through the assignment include-order tail. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_assignment_member_projection.c` and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM Slot/SecureSlot/DeviceSlot builtin call lowering out of `llvm_expr_slot_device_calls.h` into `llvm_expr_slot_device_calls.c`; call dispatch now consumes a declaration-only slot/device seam while Slot/Pin ABI runtime fallbacks stay in a compiled owner. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_slot_device_calls.c` and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM member-call dispatcher out of `llvm_member_call_emit.h` into `llvm_member_call_emit.c`; expression call lowering now consumes a declaration-only member-call seam while vtable, slot-method, slice, projection-sync, and nominal method dispatch stay in a compiled owner. Gates: targeted `gcc -fsyntax-only` for `llvm_member_call_emit.c` and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM expression call dispatcher out of `llvm_expr_call_dispatch.h` into `llvm_expr_call_dispatch.c`; `llvm_expr_call_owners.h` now shrinks to the call-dispatch declaration seam instead of importing every call-family implementation header into `llvm_expr.c`. Gates: targeted `gcc -fsyntax-only` for `llvm_expr_call_dispatch.c` and `llvm_expr.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split LLVM projection-sync body emission out of `llvm_domain_projection_sync_body_helpers.h` into `llvm_domain_projection_sync_body_helpers.c`; domain method/zone sync owners now share a declaration-only projection body seam while bounded projection recompute code lives in one compiled owner. Gates: targeted `gcc -fsyntax-only` for `llvm_domain_projection_sync_body_helpers.c`, `llvm_domain_projection_sync_helpers.c`, `llvm_domain_zone_sync.c`, and `llvm_domain_method_emit.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split C backend intent zone-slot resolution out of `transpiler_intent_zone_binding_emit.h` into `transpiler_intent_zone_slot.c`; block intent binding and intent trace emit now consume a named compiled owner for alias/zone slot resolution instead of relying on header include order. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `transpiler_intent_zone_slot.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split C backend MIR SSA lookup helpers out of `transpiler_mir_ssa_lookup.h` into `transpiler_mir_ssa_lookup.c`; prior-block, block-exit, renamed-local, and routine-exit SSA name resolution now share one compiled owner while `transpiler_mir_emit_decls.h` only exposes the early declaration seam. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `transpiler_mir_ssa_lookup.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split C backend MIR role-method lookup and SSA entry/name rendering out of `transpiler_mir_ssa_names.h` into `transpiler_mir_role_lookup.c`, `transpiler_mir_ssa_entry.c`, and `transpiler_mir_ssa_names.c`; the SSA names header is now declaration-only, and SSA C-name rendering now receives `TranspilerCtx *` explicitly instead of reading the type-render context global. Gates: targeted `gcc -fsyntax-only` for `transpiler.c`, `transpiler_mir_role_lookup.c`, `transpiler_mir_ssa_entry.c`, and `transpiler_mir_ssa_names.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split C backend MIR pin-region emission out of `transpiler_mir_pin_emit.h` into `transpiler_mir_pin_emit.c`; pin enter/exit, pin-slot lookup, anchor-def probing, and resource-alias seeding are now a compiled owner, while the pin header is declaration-only. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `transpiler_mir_pin_emit.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split C backend MIR signature eligibility policy out of `transpiler_mir_ssa_emit.h` into `transpiler_mir_signature.c`; the emission contract now consumes a compiled signature owner and passes `TranspilerCtx *` explicitly instead of relying on type-render local state. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `transpiler_mir_signature.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Split C backend MIR local-binding discovery and with/destructure alias seeding out of `transpiler_mir_ssa_emit.h` into `transpiler_mir_local_binding.c`, reducing the SSA emit header's AST-walk responsibility before deeper MIR emitter splits. Gates: `test-transpile`, `build-source-inventory-test-smoke`.
- Split C backend projection field-path analysis out of `transpiler_overlay_projection.h` into `transpiler_projection_field_path.c`; assignment-root, subfield, host-field, and method-assignment projection field inference are now a compiled owner shared by overlay invalidation paths. Gates: `test-transpile`, `build-source-inventory-test-smoke`.
- Reduced C backend public-header coupling: `transpiler.h` no longer imports semantic/type-system umbrella headers and no longer exposes `BuiltinKind`-based intenal builtin emitter prototypes. The C backend public surface now depends on AST/HIR/MIR/Arena only. Gates: `test-transpile`, `production-header-size-test-smoke`, `build-source-inventory-test-smoke`.
- Reduced LLVM codegen-to-semantic coupling for slot escape summaries: `slot_summary.h` now owns the slot escape/parameter summary API, so LLVM let-lowering no longer imports the full `slot_analyzer.h` / `SemanticContext` surface. Gates: `test-semantic`, `test-transpile`, `production-header-size-test-smoke`, `build-source-inventory-test-smoke`.
- Split `BuiltinKind` / `builtin_resolve(...)` out of `type_checker.h` into `builtin_kind.h`, so C codegen can consume the builtin dispatch vocabulary without importing the semantic type-checker umbrella. Gates: `test-semantic`, `test-transpile`, `source-utf8-test-smoke`.
- Removed the stale semantic duplicate declaration of `builtin_resolve(...)` from slot builtin headers and made the resolver implementation include only the builtin vocabulary header. Gate: `test-semantic` (`2500/0`).
- Reduced Slot analyzer public-header coupling: `slot_analyzer.h` now forward-declares `SemanticContext` instead of importing the full type-checker umbrella; only the implementation file consumes `type_checker.h`. Gates: `test-semantic`, `source-utf8-test-smoke`.
- Reduced diagnostic payload public-header coupling: `diag_payload.h` now depends on AST plus forward-declared `SemanticContext` / `DiagnosticPayloadSnapshot`, rather than importing the type-checker umbrella. Gate: `test-semantic` (`2500/0`).
- Split semantic diagnostic data into `diagnostic_types.h`, so `semantic.h` exposes `SemanticResult` diagnostics without importing the full type-checker umbrella. Gates: `test-semantic` (`2500/0`), `bin/pgy-lsp.exe`.
- Reduced builtin helper header coupling: `type_checker_builtins_slotops.h`, `type_checker_builtins_query.h`, and `type_checker_builtins_query_domain.h` now declare only their AST/Type/SemanticContext surface instead of importing the full type-checker umbrella. Gate: `test-semantic` (`2500/0`).
- Reduced builtin dispatcher intenal-header coupling: `type_checker_builtins_intenal.h` now depends on `builtin_kind.h` plus forward-declared `SemanticContext`/`Symbol`, and the declaration-only nominal header no longer imports the full type-checker umbrella. Gate: `test-semantic` (`2500/0`).
- Reduced domain semantic intenal-header coupling: `type_checker_domain_intenal.h` now exposes only AST/SymbolKind/SemanticContext-facing declarations instead of importing the public type-checker umbrella. Gate: `test-semantic` (`2500/0`).
- Split LLVM banner string normalization out of `llvm_expr_identifier_slot_helpers.h` into responsibility-owned `llvm_expr_banner_string_helpers.h`, reducing the slot identifier helper from the 600-LOC boundary to 476 LOC and keeping banner/log formatting outside slot lowering. Gates: `production-header-size-test-smoke`, `source-utf8-test-smoke`, `build-source-inventory-test-smoke`, `bin/pgy.exe`.
- Aligned C expression type inference with the frozen nominal host set: constructor-like call inference now recognizes party/role/roster declarations alongside class/subject/relation/effect/zone/world. Gates: `test-transpile` (`717/0`), `mir-declaration-inventory-test-smoke`.
- Removed duplicate role/ability emitter declarations from the C backend public header, keeping `transpiler.h`'s domain emitter surface single-declared while preserving include/impl-ability entry points. Gate: `bin/pgy.exe`.
- Split C stdlib HashMap lowering out of the combined collection builtin owner into `transpiler_expr_stdlib_map_builtin.h`, reducing `transpiler_expr_stdlib_collection_builtin.h` from 502 LOC to 354 LOC and leaving the collection owner focused on List/Set/Queue dispatch. Gates: `test-transpile` (`717/0`), `production-header-size-test-smoke`, `build-source-inventory-test-smoke`.
- Split MIR SSA local type-AST lookup out of `transpiler_mir_ssa_emit.h` into responsibility-owned `transpiler_mir_local_type_ast_lookup.c`; the header is now declaration-only and build-inventoried, reducing the SSA emission owner without changing MIR lowering behavior. Gates: targeted `gcc -fsyntax-only` for `transpiler.c` and `transpiler_mir_local_type_ast_lookup.c`; full make gates are currently blocked locally by shell/WSL permission errors.
- Retired the intermediate MIR let-declaration lookup owner: the earlier
  `transpiler_mir_let_lookup.c` split is now fully removed because block
  emission and pending-use materialization consume MIR instruction-carried
  source facts directly. CFG smoke rejects reintroducing name-based
  function-body let lookup under `src/codegen`. Gates: `test-transpile`,
  `cfg-body-dataflow-test-smoke`, and `build-source-inventory-test-smoke`.
- Split MIR resource runtime-name policy out of `transpiler_helpers.h` into `transpiler_mir_resource_name_helpers.h`, reducing the generic helper owner from 557 LOC to 496 LOC while keeping resource op emission behavior unchanged. Gates: `test-transpile` (`717/0`), `production-header-size-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `bin/pgy.exe`.
- Split C overlay world-embedded projection sync out of `transpiler_overlay_projection.h` into `transpiler_overlay_world_projection.h`, reducing the generic overlay projection owner from 533 LOC to 339 LOC while preserving assignment invalidation and receiver post-sync behavior. Gates: `test-transpile` (`717/0`), `production-header-size-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `bin/pgy.exe`.
- Split C intent emission metadata cleanup and local carrier/context macros out of `transpiler_intent_emit.h` into `transpiler_intent_emit_metadata_helpers.h`, reducing the intent declaration emitter from 520 LOC to 487 LOC without changing MIR-only carrier validation or cleanup behavior. Gates: `test-transpile` (`717/0`), `production-header-size-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `bin/pgy.exe`.
- Split MIR active-inventory inline views out of the public C backend header into `transpiler_inventory_view.h`, reducing `transpiler.h` from 531 LOC to 402 LOC while keeping the same public API available to existing consumers. Gates: `test-transpile` (`717/0`), `production-header-size-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `bin/pgy.exe`.
- Split C relation/effect declaration emission out of `transpiler_domain_nominal_emit.h` into `transpiler_relation_effect_emit.h`, reducing the mixed domain nominal owner from 562 LOC to 382 LOC while preserving relation/effect projection sync and hosted method emission. Gates: `test-transpile` (`717/0`), `production-header-size-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `bin/pgy.exe`.
- Split LLVM defer scope lifecycle out of `llvm_stmt.c` into `llvm_stmt_defer_scope.c`, reducing the statement dispatcher owner from 598 LOC to 497 LOC while preserving defer registration/emission semantics. Gates: `bin/pgy.exe`, `test-transpile` (`717/0`), `production-header-size-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`.
- Split DIR intent metadata materialization out of `dir_collect.c` into `dir_collect_intent.c`, reducing the mixed declaration/edge collector from 578 LOC to 311 LOC and keeping compressed-intent provenance collection in a dedicated owner. Gates: `bin/pgy.exe`, `test-dir` (`9/0`), `build-source-inventory-test-smoke`, `source-utf8-test-smoke`.
- Split LLVM domain struct layout/registration out of `llvm_domain.c` into `llvm_domain_struct_register.c`, reducing `llvm_domain.c` from 552 LOC to 85 LOC and leaving it as pass orchestration only. Gates: `bin/pgy.exe`, `llvm-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`.
- Split DAG graph/stage declarations out of the semantic intenal umbrella into `type_checker_resolution_graph_intenal.h`, reducing `type_checker_intenal.h` from 571 LOC to 486 LOC and making the resolution graph API ownership explicit. Gate: `test-semantic` (`2500/0`).
- Split LLVM secure-slot runtime declarations out of `llvm_runtime.c` into `llvm_runtime_secure_slot_decl.c`, reducing the mixed runtime declaration owner from 540 LOC to 486 LOC while keeping the Slot/Pin ABI declaration order behind `llvm_runtime_intenal.h`. Gates: `bin/pgy.exe`, `llvm-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `production-header-size-test-smoke`.
- Split MIR liveness/use validation out of `mir.c` into `mir_validation.c`, reducing the mixed MIR construction/validation owner from 546 LOC to 427 LOC and making body-safety validation a dedicated owner consumed by the MIR public validator. Gates: `test-mir` (`41/0`), `bin/pgy.exe`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `production-header-size-test-smoke`.
- Split LLVM channel send/receive expression lowering out of `llvm_expr.c` into `llvm_expr_channel.c`, reducing the mixed expression dispatcher from 543 LOC to 477 LOC while preserving ChannelSend/ChannelRecv runtime diagnostics. Gates: `bin/pgy.exe`, `llvm-test-smoke`, `source-utf8-test-smoke`, `production-header-size-test-smoke`.
- Tightened `build_source_inventory_smoke.sh` by batching `git check-ignore --stdin` instead of shelling out once per build source. This keeps the source inventory gate viable as owner splits increase. Gate: `build-source-inventory-test-smoke`.
- Promoted C backend `select` lowering from a static include chunk into `transpiler_select.c`, deleting `transpiler_select_emit.h` and moving select case parsing out of `transpiler.c`. This keeps select/channel lowering in a responsibility-owned translation unit and reduces the main C backend owner to 504 LOC. Gates: `bin/pgy.exe`, `test-transpile` (`717/0`), `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `production-header-size-test-smoke`.
- Split LLVM intent observability trace emission out of `llvm_intent.c` into `llvm_intent_trace.c`, reducing the main intent orchestration owner from 537 LOC to 499 LOC while keeping trace ABI calls behind `llvm_intent_intenal.h`. Gates: `bin/pgy.exe`, `llvm-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `production-header-size-test-smoke`.
- Split MIR CFG edge-topology validation out of `mir_cfg_contract_validate.h` into `mir_cfg_contract_edges.h`, reducing the cleanup/pin contract validator from 538 LOC to 342 LOC while keeping predecessor/successor checks in the CFG body-safety gate. Gates: `test-mir` (`41/0`), `cfg-body-dataflow-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `production-header-size-test-smoke`.
- Promoted MIR CFG edge-topology validation from an implementation header into `mir_cfg_contract_edges.c`; `mir_cfg_contract_edges.h` is now declaration-only, and predecessor lookup is owned by `mir_base_helpers.c`. Gates: targeted `gcc -fsyntax-only` for `mir.c`, `mir_base_helpers.c`, and `mir_cfg_contract_edges.c`; `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, `test-mir` (`41/0`).
- Refreshed CFG/perf smoke contracts after implementation-header removal: `cfg_body_dataflow_smoke.sh` now tracks `mir_ssa_rename.c` / `mir_cfg_contract_edges.c`, and `perf_contract_smoke.sh` now checks the compiled LLVM/C/MIR owners instead of stale declaration headers. Gates: `build-source-inventory-test-smoke`, `test-inc-size-test-smoke`, `cfg-body-dataflow-test-smoke`, `perf-contract-test-smoke`.
- Split C backend MIR emission mapping precheck out of `transpiler_mir_emission_contract.h` into `transpiler_mir_emission_mapping_contract.h`, reducing the main MIR emission contract owner from 545 LOC to 396 LOC while keeping SSA/name mapping fallback rejection in the C backend contract gate. Gates: `test-transpile` (`717/0`), `cfg-body-dataflow-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `production-header-size-test-smoke`.
- Split LLVM MIR ABI/type metadata helpers out of `llvm_mir_emit.c` into `llvm_mir_type_helpers.h`, reducing the main MIR function emitter from 528 LOC to 358 LOC while keeping boundary Slot/SecureSlot parameter typing shared with MIR local emission. Gates: `bin/pgy.exe`, `llvm-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`, `production-header-size-test-smoke`.
- Closed a LLVM declaration inventory compatibility gap: active-inventory fallback lookup now mirrors the frozen host declaration set for class/enum/relation/effect/zone/world/party/role/roster. Gates: `perf-contract-test-smoke`, `mir-declaration-inventory-test-smoke`, `llvm-test-smoke`, and `llvm-test-backend-compare`.
- Closed an AIR boundary evidence drift bug: duplicate boundary-scoped evidence appends are idempotent, while global evidence counters still accumulate. This preserves the invariant that each boundary evidence node carries exactly one boundary fact. Gate: `test-air` (`76/0`).
- Closed a LLVM Slot/Pin ABI parity bug: LLVM lowering now calls out-param `pgy_pin_*_init_*` / `pgy_secure_pin_*_init_*` wrappers instead of relying on struct-by-value retuns. Gates: `abi-ownership-shape-test-smoke`, `test-abi`, `llvm-test-smoke`, `llvm-test-backend-compare` (`69/69`, ABI same-process precheck `196/196`).
- Closed the compressed-intent LLVM success default gap: omitted `success:` lowers to explicit `true`; unsupported non-null success expressions still fail strictly. Gate: `llvm-test-backend-compare`.
- Rechecked AIR verification-layer gates: strict evidence, drift detection, stable JSON graph schema, and backend non-impact all remain green after boundary evidence idempotence. Gates: `test-air` (`76/0`), `air-drift-test-smoke`, `air-json-schema-test-smoke`, `air-backend-nonimpact-full-test-smoke`.
- Rechecked DAG source-of-truth gates: resolver fallback seams remain `0`, direct resolver inventory remains capped, and DAG metadata counters remain above the beta floor (`skips=2033`, `entries=3498`, `owned=258`, `hits=8380`, `materializer_fallbacks=0`). Gates: `type-resolution-resolver-inventory-test-smoke`, `type-resolution-dag-test-smoke`.
- Rechecked CFG/MIR body-safety contract after Slot/Pin ABI changes: pin cleanup facts, CFG-owned control rejection, WriteView exclusivity, all-CFG-exit cleanup, SSA/PHI, DCE, and declaration-header drift gates remain green. Gates: `cfg-body-dataflow-test-smoke`, `test-mir` (`41/0`).
- Closed a 600-LOC owner-size regression: projection invalidation/world-embedded sync logic moved from `llvm_expr_assignment_member_projection.h` into responsibility-owned `llvm_expr_assignment_projection.h` (233/378 LOC). Gates: `test-inc-size-test-smoke`, `production-header-size-test-smoke`, `build-source-inventory-test-smoke`, `llvm-test-smoke`.
- Closed a LLVM host-name consistency gap: `llvm_current_host_decl_name(...)` now mirrors the same host declaration set as host classification and active-inventory fallback (`class/enum/party/role/roster/relation/effect/zone/world`). Gates: `mir-declaration-inventory-test-smoke`, `llvm-test-smoke`.
- Tightened C/LLVM host-name smoke coverage: `mir-declaration-inventory-test-smoke` now gates party/role/roster active-inventory use and C/LLVM declaration-name helpers together.
- Closed the matching LLVM host-struct-name drift: `llvm_current_host_class_name(...)` now includes party/role/roster for self/field/projection helpers that consume the current host as a registered LLVM struct. Gates: `mir-declaration-inventory-test-smoke`, `llvm-test-smoke`.
- Closed a role pointer-self drift: `llvm_type_name_uses_pointer_self(...)` now treats `role` declarations consistently with the rest of the pointer-self domain host set. Gates: `mir-declaration-inventory-test-smoke`, `llvm-test-smoke`, targeted `compare_backends.sh` for `role_operator`, `zone_host_method_abi_combo`, and `host_method_class_retun` (`3/3`).
- Closed a compressed-intent layering seam: C/LLVM codegen no longer re-infers action `causes` from `on` expressions. `causes` must be materialized by semantic/DIR/MIR first, and `intent-compression-contract-test-smoke` now rejects reintroduced codegen-side inference.
- Closed the matching C backend host-lookup drift: `transpiler_find_nominal_host_decl_local(...)` and `transpiler_find_host_decl_from_owner_local(...)` now mirror the frozen domain host set for party/role/roster in addition to class/enum/relation/effect/zone/world, so C and LLVM declaration lookup consume the same host inventory surface. Gates: `mir-declaration-inventory-test-smoke`, `test-transpile`.
- Tightened C known-nominal forwarding: `transpiler_has_known_nominal_type(...)` now includes enum and role declarations, keeping C forward-declaration policy aligned with the frozen host/type inventory instead of treating those names as unknown after zone emission. Gate: `mir-declaration-inventory-test-smoke`.
- Rechecked runtime propagation frontier gates: bounded zone/world/projection frontier contracts, runtime panic overflow path, queryable authority/failure surface, generated embedded-world frontier limit, and shared frontier pass-limit arithmetic remain green. Gates: `runtime-frontier-contract-test-smoke`, `runtime-frontier-policy-test-smoke`.
- Repaired Slot/Pin ABI smoke owner drift after LLVM secure-slot runtime declarations were split: `abi_ownership_shape_smoke.sh` now gates secure pin init declarations in `llvm_runtime_secure_slot_decl.c` instead of the old mixed `llvm_runtime.c` owner. Gates: `abi-ownership-shape-test-smoke`, `runtime-abi-lifetime-test-smoke`, `runtime-panic-contract-test-smoke`, `build-source-inventory-test-smoke`.
- Repaired intent-compression smoke owner drift after DIR intent collection was split: `intent_compression_contract_smoke.sh` now gates derived authority/using provenance in `dir_collect_intent.c` instead of the old mixed `dir_collect.c` owner. Gate: `intent-compression-contract-test-smoke`.
- Rechecked LLVM parity around the current debt slice without claiming a full local compare: `llvm-test-abi-same-process` is green (`196/0`), targeted C/LLVM backend compare for Slot/Pin cleanup, compressed intent, world frontier, and zone host ABI is green (`9/9`), and `air-backend-nonimpact-full-test-smoke` is green across the backend-compare fixture set. A full `llvm-test-backend-compare` run exceeded the local 5-minute tool timeout on Windows and still needs a longer CI/local window for final parity certification.
- Removed a single-include LLVM implementation header: early forward-declaration eligibility now lives in `llvm_backend_forward_declare.c` instead of being injected through `llvm_backend_forward_declare.h` at the end of the type-map owner. Gates: `build-source-inventory-test-smoke`, `llvm-test-smoke`.
- Removed another LLVM implementation-header body: projection slot counting now lives in `llvm_domain_projection_count.c`, while `llvm_domain_projection_count_helpers.h` is declaration-only. The split also made `llvm_domain_struct_register.c`'s projection-target helper dependency explicit instead of relying on a transitive include. Gates: `build-source-inventory-test-smoke`, `production-header-size-test-smoke`, `llvm-test-smoke`.
- Removed the shared LLVM projection-target static implementation header body: `llvm_domain_slot_is_projection_target(...)` now lives in `llvm_domain_projection_target.c`, so struct-field/register/count owners share one compiled implementation instead of duplicating static header code. Gates: `build-source-inventory-test-smoke`, `production-header-size-test-smoke`, `llvm-test-smoke`.
- Removed the LLVM role lookup static implementation header body: role declaration/operator-method lookup now lives in `llvm_domain_role_lookup.c`, so `llvm_domain_forward.c` and `llvm_domain_role_emit.c` share one compiled owner instead of duplicating recursive role include lookup logic. Gates: `build-source-inventory-test-smoke`, `llvm-test-smoke`, targeted backend compare for `role_operator` and `zone_host_method_abi_combo` (`2/2`).
- Removed two LLVM expression call static implementation header bodies: event invocation lowering now lives in `llvm_expr_event_calls.c`, and scalar `Abs`/`Min`/`Max` lowering now lives in `llvm_expr_math_calls.c`. Their headers are declaration-only call-owner seams. Gates: `build-source-inventory-test-smoke`, `production-header-size-test-smoke`, `source-utf8-test-smoke`, `llvm-test-smoke`, targeted backend compare for `basic`, `role_operator`, `try_operator_result`, and `string_io` (`4/4`).
- Rechecked UTF/docs readiness gates after the progress-log and beta-board updates. Gates: `source-utf8-test-smoke`, `documentation-quality-test-smoke`, `beta-readiness-checklist-test-smoke`.
- Split CFG resource snapshot implementation out of `type_checker_flow_resources.h` into `type_checker_flow_resources.c`, leaving the header declaration-only and keeping body-safety resource snapshot/parallel conflict facts in a real semantic owner. Gates: `test-semantic` (`2500/0`), `cfg-body-dataflow-test-smoke`, `production-header-size-test-smoke`.
- Split CFG loop body-flow implementation out of `type_checker_flow_loops.h` into `type_checker_flow_loops.c` with a narrow `type_checker_flow_intenal.h` seam for shared flow facts. Loop fixed-point resource/effect merging now lives in a real semantic owner instead of an implementation header. Gates: `test-semantic` (`2500/0`), `cfg-body-dataflow-test-smoke`, `production-header-size-test-smoke`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`.
- Split MIR fact validation and inventory surface usage out of implementation headers into `mir_fact_validate.c` and `mir_surface_usage.c`, leaving `mir_fact_validate.h` / `mir_surface_usage.h` declaration-only. MIR validation/source-usage facts now link through real compiler owners instead of being textually injected into `mir.c`. Gates: `test-mir` (`41/0`), `cfg-body-dataflow-test-smoke`, `build-source-inventory-test-smoke`, `production-header-size-test-smoke`, `source-utf8-test-smoke`.
- Split MIR statement population out of `mir_stmt_population.h` into `mir_stmt_population.c`, leaving the header declaration-only and making non-CFG statement population consume explicit shared helper prototypes instead of include-order leakage. Gates: `test-mir` (`41/0`), `cfg-body-dataflow-test-smoke`, `build-source-inventory-test-smoke`, `production-header-size-test-smoke`, `source-utf8-test-smoke`.
- Split MIR liveness/use-def summary implementation out of `mir_liveness_dce.h` into `mir_liveness_dce.c`, leaving the header declaration-only and keeping liveness/value-summary ownership in a real compiler source owner. Gates: `test-mir` (`41/0`), `cfg-body-dataflow-test-smoke`, `build-source-inventory-test-smoke`, `production-header-size-test-smoke`, `source-utf8-test-smoke`.
- Split MIR DCE implementation out of `mir_dce.h` into `mir_dce.c`, leaving the header declaration-only and removing the remaining MIR DCE static implementation header wanings from the MIR build. Gates: `test-mir` (`41/0`), `cfg-body-dataflow-test-smoke`, `build-source-inventory-test-smoke`, `production-header-size-test-smoke`, `source-utf8-test-smoke`.
- Split MIR statement source classification out of `mir_stmt_population.c` into `mir_stmt_source.c`, keeping the population owner focused on instruction interleaving and leaving def-source/preservation classification behind the existing `mir_stmt_population.h` API. Gates: `test-mir` (`41/0`), `cfg-body-dataflow-test-smoke`, `build-source-inventory-test-smoke`, `production-header-size-test-smoke`, `source-utf8-test-smoke`.
- Updated MIR/perf declaration smoke contracts to track the new declaration-only headers and responsibility-owned `.c` files (`mir_fact_validate.c`, `mir_stmt_population.c`, `mir_stmt_source.c`, `transpiler_inventory_view.h`, `llvm_expr_channel.c`, `type_checker_flow_resources.c`). Gates: `perf-contract-test-smoke`, `mir-declaration-inventory-test-smoke`.
- Split MIR declaration-header metadata validation out of `mir_fact_validate.c` into `mir_decl_header_validate.c`, reducing the mixed fact validator from 578 LOC to 245 LOC while preserving hosted-method/pointer-self drift checks behind the existing `mir_validate_decl_header_metadata(...)` API. Gates: `test-mir` (`41/0`), `mir-declaration-inventory-test-smoke`, `build-source-inventory-test-smoke`, `production-header-size-test-smoke`, `source-utf8-test-smoke`.
- Split MIR value-summary materialization out of `mir_liveness_dce.c` into `mir_liveness_summary.c`, reducing the liveness owner from 557 LOC to 391 LOC while keeping use-def summary construction behind `mir_build_value_summaries(...)`. Gates: `test-mir` (`41/0`), `cfg-body-dataflow-test-smoke`, `build-source-inventory-test-smoke`, `production-header-size-test-smoke`, `source-utf8-test-smoke`.
- Split AIR evidence-node inventory mutation out of `air.c` into `air_evidence_node.c`, keeping synthesis focused on AIR node construction while evidence append/merge/fact-count overflow handling now has a dedicated owner. Gates: `test-air` (`76/0`), `air-drift-test-smoke`, `build-source-inventory-test-smoke`, `production-header-size-test-smoke`, `source-utf8-test-smoke`.
- Split AIR stable enum vocabulary out of `air_dump.c` into `air_vocabulary.c`, so dump formatting no longer owns the public string mapping for sync/failure/boundary/drift/evidence names. Gates: `test-air` (`76/0`), `air-drift-test-smoke`, `build-source-inventory-test-smoke`, `production-header-size-test-smoke`.
- Tightened LLVM MIR cleanup consumption: `llvm_mir_contract.c` now fail-closes before emission when a cleanup/rollback/invalidation successor lacks the matching MIR cleanup fact, or when a reachable pin block lacks a cleanup successor / `pin-unpin-cleanup-edge` fact. `llvm_emit_func_from_mir(...)` consumes that contract instead of trusting block flags alone, aligning LLVM with the C backend's MIR emission contract. Gates: `test-mir` (`41/0`), `cfg-body-dataflow-test-smoke`, `test-air` (`76/0`), `build-source-inventory-test-smoke`.

## 0-selfhost. Beta 이후 self-hosting 목표

**결정:** self-hosting은 beta blocker가 아니라 beta 이후의 검증 목표다.

Beta까지의 핵심 목표는 compiler core를 닫는 것이다: CFG body safety,
AIR evidence, DAG resolution, MIR/C/LLVM parity, ABI ownership, runtime
frontier, 그리고 dogfood path. 이 축이 닫힌 뒤에야 self-hosting을 시작한다.

**순서:**
1. beta closure를 끝내고 stable core surface를 freeze한다.
2. 첫 dogfood는 compiler 전체 rewrite가 아니라 compiler-adjacent tool부터 한다:
   diagnostic catalog checker, AIR graph JSON validator, MIR dump diff tool,
   C/LLVM backend output comparator, module/package resolver helper.
3. Pergyra로 작성한 도구는 기존 C compiler로 빌드하고, 기존 C 구현 결과와
   golden 비교한다.
4. beta+에서 parser/formatter/diagnostic 일부를 점진 이식한다.
5. full self-hosted compiler는 장기 proof target으로 둔다.

**Slot/ownership 기준:** Pergyra는 Rust-style lifetime 언어가 아니다. 모든
business-object lifetime을 정적으로 예측하려 하지 않는다. Slot은 포인터 주소가
아니라 resource boundary / ownership handle이며, static verifier는 unsafe
boundary transition을 거절하고 runtime은 generation/token/resource state를
검증한다. 이 선택은 borrow checker 누락이 아니라 의도적인 추상화 기준이다.

## ★ Core Goal — 진행 시퀀스 (2026-05-02 명시, 4-step)

**확정 순서 — BDFL 결정:**

1. **BETA closure** — 현재 (§0a 참조). 70% 기능 / strict beta 기준값 60%,
   현재 실무 판단 63%
   → 100% 신뢰도까지 닫기
2. **dogfood (compiler-adjacent first)** — §0-selfhost 의 첫 dogfood
   원칙: diagnostic catalog checker, AIR graph JSON validator, MIR dump
   diff tool, C/LLVM backend output comparator, module/package resolver
   helper 부터. dnd_taven_campaign / 결제 saga mock / AI orchestration
   mock 등 도메인 워크로드도 양 백엔드 회귀. **§0c Intent-Compress
   추론 규칙 설계의 evidence source** (어느 clause가 과잉 required인지
   dogfood가 보여줌)
3. **§0c Intent-Compress sprint** — intent 장황함이 *제거 가능한 유일한
   비용* (§0c 상세). compressed-default + 4-clause 추론 (`who`/`where`/
   `requires`/`authorized by`). 구현 견적은 Phase 1 명세 후 재산정한다.
   self-host 진입 *직전* 자리. self-host가 *verbose intent를 Pergyra로
   다시 쓰는 비용*을 회피
4. **BETA+ self-host 시작** — §0-selfhost 의 점진 이식 경로
   (compiler-adjacent → parser/formatter/diagnostic → full long-term).
   docs/120 §4.4 참조. *aspiration이 아니라 committed 일정*.

**의도:**
- §0a (Strict Beta) → §0b (review/ 메타) → 코드 품질 sprint
  (review/compiler-quality-audit.md) → 이 모든 게 *self-host 진입 자격*
  을 만드는 작업. 1.0 닫기 전에 *컴파일러를 우리 언어로 다시 쓸 수 있는
  상태*가 목표
- 단, BDFL 의지에 *시기 강제*는 없음. trigger (사용자 demand 또는 C
  escape hatch 유지 비용 폭발 또는 Pergyra-only 표현이 필요한 feature)
  발생 시 진입. trigger 없으면 partial이 final form.

**Why slot was the right call (2026-05-02 reflection):**

> "러스트의 본질적 핵심 문제점인 라이프타임을 의도적으로 뺐어 그걸 slot
> 이라고 했지. 그게 좋은 선택이었던 거 같다." — BDFL

→ docs/118 §6 negative-space + memory project_lineage_synthesis.md 의
*substrate borrow* 결정 정합. Pergyra는 Rust의 lifetime annotation 학습
비용을 *의도적으로 회피*하고 generational refs (Vale-style
runtime-validated handles) 로 대체. 진입 비용 낮춘 자리, 자기인식 정합.

이 reflection은 *self-host 진입 시 가장 큰 자산*: lifetime annotation이
없으니 컴파일러 자체를 Pergyra로 다시 쓸 때 그 자리가 *일관되게 표현
가능*. Rust가 self-host 시 lifetime annotation으로 부닥친 자리를 우리는
회피.

## 0. 코어 규칙 — 600 LOC split-review threshold

**모든 production `.c` / `.h` owner는 600 LOC 이하로 유지한다.**
초과 시 *feature-owner split* 필수 — 주석 줄이기 / 함수 인라인이 아니라
*owner 분리*.

| Scope | Cap | Gate |
|---|---|---|
| `src/{codegen,runtime,compiler,semantic,parser,lsp}`의 `.h` | 600 LOC (hard) | `tests/production_header_size_smoke.sh` (env `PRODUCTION_HEADER_MAX_LINES` override 가능) |
| 같은 디렉터리의 `.c` | 600 LOC (split-review threshold) | 진행 노트에서 사람 검수 + 베타 closure sweep |
| `tests/` / generated / `.inc` 파편 | 면제 | — |

**Split 패턴:**
- `.inc` field-fragment 분리 — `docs/92_inc_split_roadmap.md`
- 별개 translation unit + 헬퍼 헤더 — `docs/101_semantic_split_template.md`
- 진행 상태 — `docs/115_inc_cleanup_status.md`

**Split application guide (2026-05-02):**
- This is not a rule change. 600 LOC remains the signal; it is not the
  prescription.
- Checklist when an owner reaches the signal:
  1. Does this owner carry two responsibilities? If yes, split by
     responsibility.
  2. Is it still one responsibility but large? If yes, keep one owner and
     improve intenal structure with `static` helpers in the same `.c`.
  3. If split is needed, does the new owner name express the responsibility?
     If not, do not land the split.
- New `_helpers` owners are forbidden by default because `_helpers` does not
  name a responsibility. Exception: a genuinely cross-owner shared utility with
  a documented caller set.
- Prefer responsibility names such as `intent_types.c`,
  `resolution_graph_intent.c`, or `cleanup_fact.h`.
- If two helpers differ only by message/kind, prefer a data-driven helper with
  an enum or small table over adding another near-duplicate helper.
- Self-hosting is the planned large-scale structure recovery point: the
  Pergyra compiler should move toward feature-owned modules (`intent`,
  `zone`, `mir`, `air`, `dag`) rather than horizontal helper sprawl. Do not do
  a risky feature-folder migration before beta.

**의도:**
- 행 수 자체가 목표 아님. *행동(behavior)이 1개 owner로 응집*하는지 확인.
- 600 LOC를 넘었다는 신호 = "이 owner가 두 가지 책임을 지고 있다." split으로 응답.
- 진행 노트마다 owner 라인 수를 명시하는 컨벤션 유지 (예: `slot_manager.c는 564 LOC`).
- 현재 production scan: 0 `.c/.h` owners above 600 LOC (2026-04-29 기준).

## 0-meta. review/ 폴더 운영 프로세스 검토 (2026-05-01)

- review/ 폴더는 외부 리뷰 + 코드 감사 결과 누적 자리로 사용 중
  (`review/compiler-quality-audit.md` 추가됨 2026-05-01)
- **검토 필요 항목:**
  - review/ 문서가 TODO.md / docs/ 와 *어떻게 연동*되는지 명시적 룰 부재
  - audit 발견 → review/ 작성 → TODO.md sprint entry → 수정 → 검증의
    *closure 절차* 표준화 안 됨
  - review/README.md 가 단순 인덱스 — *프로세스 가이드* 부재
  - review/ 문서가 stale 됐을 때 detection 메커니즘 없음
- **결정 필요:** review/ 를 *living docs* 로 둘지 *snapshot 아카이브*로
  둘지. 현재 README는 "수정 작업의 근거" 표현 — living docs 의도로
  보이지만 운영 룰 부재
- *베타 closure 작업 아님*. 메타 프로세스 정합성 자리. 1.0 전 정리 권고

## 0a. Strict Beta Closure Order — 2026-05-01 재고정

**현재 판정:** 기능 구현률은 약 70%로 본다. strict beta readiness는
60%를 기준값으로 두고, 현재 실무 판단은 63%다. 차이는 기능 수가 아니라
CFG/AIR/DAG/MIR/ABI가 실제 source-of-truth로 소비되는 깊이다.

**명시적 제외:** quantum full model, Rust급 lifetime/borrow checker, HKT/FP,
새 대형 언어 축은 beta 100% 계산에서 제외한다. WASM/WebGL은 실제 dogfooding
경로라 중요하지만, beta closure 자체를 흔드는 새 semantic surface가 아니다.
베타의 1차 dogfood 경로는 `Pergyra -> C backend --emit-c -> optional
Emscripten/WebGL bridge`로 고정하고, native LLVM wasm target과 richer render
module은 beta+1로 둔다.

**닫는 순서:**
1. **AIR evidence producer 정합성.** 빈 evidence node 금지, DAG/MIR/RIR/HIR
   evidence가 실제 fact 또는 explicit fallback debt가 있을 때만 생성되게 한다.
   Gate: `make test-air air-drift-test-smoke air-json-schema-test-smoke`.
   - 2026-05-02 slice: intent zone-authority compression now reaches AIR.
     DIR records `authorized_by_derived_from_zone`, AIR records
     `authority_from_zone`, AIR JSON exposes it, and drift diagnostics include
     `authority_provenance=zone-derived|explicit|none`.
   - 2026-05-02 slice: action-derived intent `causes` now reaches RIR
     propagation evidence. `rir_builder_intent.c` materializes inherited or
     explicit step causes as `RIR_RESOURCE_EFFECT_INSTANCE` +
     `RIR_OP_ATTACH_EFFECT`, preferring the unique zone effect-slot anchor over
     the effect type name when the current zone makes that anchor unambiguous.
     AIR verifies it through
     `AIR_EVIDENCE_RIR_EFFECT_PROPAGATION` instead of keeping
     `causes_from_action` as AIR-only provenance.
   - 2026-05-02 slice: action-derived `authorized by` is now pinned in the same
     parsed AIR fixture through real RIR authority evidence. The on-receiver
     action contract regression requires `authority_from_action`,
     `has_rir_authority_evidence`, `rir_authority_evidence_name == "healer"`,
     and an `AIR_EVIDENCE_RIR_AUTHORITY` node, so action-inherited authority is
     no longer only an AIR boundary flag.
   - 2026-05-02 cleanup evidence repair: AIR global MIR cleanup evidence now
     consumes MIR CFG cleanup successors before instruction-name fallbacks.
     Pin cleanup remains boundary-specific `AIR_EVIDENCE_MIR_PIN_CLEANUP`.
     Gate: `make test-air` (`62/0`).
   - 2026-05-02 MIR population ownership repair: statement reconstruction now
     preserves the rebuilt instruction-array capacity before later cleanup-edge
     materialization. This fixed a real pin-cleanup heap corruption in
     `mir_lower(...)` and restored `test-mir`, `cfg-body-dataflow-test-smoke`,
     `air-json-schema-test-smoke`, and C-only `air-backend-nonimpact` coverage.
   - 2026-05-02 codegen determinism repair: C MIR block mapping comments no
     longer emit raw AST pointer addresses. AIR strict/relaxed backend
     non-impact checks now compare deterministic C artifacts for all backend
     compare fixtures.
   - 2026-05-02 MIR CFG predecessor validation tightening: MIR now validates
     predecessor lists as bidirectional CFG facts. Forward successors must be
     reflected in predecessor lists, and every recorded predecessor must point
     back through a true/false/cleanup/rollback/invalidation edge. Gate:
     `make test-mir cfg-body-dataflow-test-smoke` (`28/0` MIR tests).
   - 2026-05-03 CFG/MIR direct-call fact tightening: direct statement calls now
     carry their callee name as `MIR_INST_STMT.arg0`, and direct initializer
     calls carry their callee name as `MIR_INST_DEF.arg1`. Intent observability
     no-trace detection consumes those MIR facts and HIR routine `direct_calls`
     before structural AST fallback. Gate: `make test-mir
     cfg-body-dataflow-test-smoke test-transpile perf-contract-test-smoke`
     (`32/0` MIR tests, `710/0` transpile tests).
   - 2026-05-03 MIR source-location materialization: C MIR block mapping
     comments now consume scalar `MIRBasicBlock.has_source_location` /
     `source_line` / `source_column` facts instead of reading
     `block->source_ast` in codegen. This keeps source AST pointers as MIR
     construction/debug provenance, not a backend consume path. Gate: manual
     native MinGW `test-mir` (`32/0`), `test-air` (`68/0`), and
     `test-transpile` (`710/0`).
   - 2026-05-03 MIR surface-usage fact materialization: MIR instructions now
     carry `has_surface_usage_facts`, `uses_thread_pool_surface`, and
     `uses_intent_observability_surface`. C/LLVM thread-pool dependency checks
     and intent-observability no-trace detection consume those MIR facts first
     and only scan AST payloads for hand-built or legacy MIR without facts. The
     shared fact materializer is now called by base, cleanup, and intent MIR
     append paths. `mir_validate(...)` also rejects stale thread-pool and
     intent-observability surface facts when instruction payloads disagree with
     the materialized fact bits, so normal lowered MIR cannot silently fall back
     to AST rescans. Thread-pool dependency detection now follows the same
     consumer rule as intent observability: HIR-backed lowered routines consume
     MIR facts only, and AST payload rescans are reserved for hand-built legacy
     MIR without HIR provenance. Gate: manual native MinGW `test-semantic` (`2500/0`),
     `test-mir` (`35/0`), `test-air` (`70/0`), and `test-transpile` (`710/0`).
   - 2026-05-03 MIR branch-shape materialization: branch and loop-init
     instructions now carry `MIRBranchShape` (`FOR_RANGE`, `FOR_IN`,
     `MATCH_CASE`, `SELECT_DISPATCH`). C and LLVM MIR control emitters consume
     that fact instead of classifying branch control by AST node type. AST
     payloads remain only for expression/condition emission. MIR validation and
     MIR lowering regressions also consume `branch_shape` for loop-branch
     completeness, so the fact is now part of the MIR contract, not just a
     backend convenience. Gate: manual native MinGW `test-semantic` (`2500/0`),
     `test-mir` (`32/0`), `test-air` (`68/0`), `test-transpile` (`710/0`), plus
     LLVM control owner compile smoke.
   - 2026-05-03 MIR dump source-location tightening: `mir_dump(...)` now prints
     source locations from `MIRBasicBlock.has_source_location` /
     `source_line` / `source_column` instead of rebuilding them from
     `source_statements[0]` or terminator AST pointers. This keeps public MIR
     dumps aligned with materialized MIR facts. Gate: manual native MinGW
     `test-mir` (`32/0`) and `test-transpile` (`710/0`).
   - 2026-05-03 MIR instruction source-location materialization:
     instructions now carry `has_source_location`, `source_line`,
     `source_column`, and `source_ast_type` facts. `mir_dump(...)` prints
     instruction `ast-type`/`line` from those facts instead of reading
     `inst->ast`. AST payloads remain available to expression emitters, but
     the public MIR dump path no longer consumes AST pointers for instruction
     provenance. Gate: manual native MinGW `test-mir` (`32/0`), `test-air`
     (`68/0`), and `test-transpile` (`710/0`).
   - 2026-05-03 MIR AST-type consumer tightening:
     C/LLVM codegen no longer branches on `inst->ast->type`. Instruction kind
     decisions now consume `source_ast_type` / `has_source_location`; AST
     payloads remain only where expression or statement emission still needs
     the original syntax tree. Gate: manual native MinGW `test-transpile`
     (`710/0`), `test-mir` (`32/0`), `test-air` (`68/0`), and
     `perf_contract_smoke`.
   - 2026-05-03 C backend source-array consumer tightening:
     `transpiler_mir_find_stmt_for_inst(...)` now trusts instruction-carried
     statement AST provenance first and falls back only to function-scope let
     lookup by name. Codegen no longer reads `block->source_statements`,
     `block->source_ast`, `source_terminator_*`, or `inst->ast->type` in the
     scanned C/LLVM backend owners; those block source arrays remain MIR
     construction input, not backend judgement input. Gate: manual native
     MinGW `test-transpile` (`710/0`) and `perf_contract_smoke`.
   - 2026-05-03 MIR construction fact hardening:
     terminator and resource instructions now call
     `mir_instruction_record_surface_usage(...)` at construction time, not only
     through later append/rewrite paths. This keeps branch/retun/resource
     instructions carrying source location, AST type, and thread-pool surface
     facts even if future construction paths bypass a rewrite helper.
     `MIRBasicBlock` also no longer stores `source_ast` or
     `source_terminator_*` pointers; HIR terminator payloads are consumed while
     constructing MIR terminator instructions and then represented by MIR
     instruction facts. Gate: manual native MinGW `test-mir` (`32/0`),
     `test-transpile` (`710/0`), plus PowerShell-equivalent contract/size scans
     for the `perf_contract_smoke` and `test_inc_size_smoke` assertions.
   - 2026-05-03 MIR use-edge provenance tightening:
     DEF use-edge collection no longer walks forward through
     `block->source_statements` looking for the next plausible let/assignment.
     If a DEF instruction has no attached AST payload, the fallback is now an
     exact `source_statement_index` lookup only. This keeps use-edge facts tied
     to instruction provenance instead of implicit source-array ordering. Gate:
     manual native MinGW `test-mir` (`32/0`), `test-transpile` (`710/0`), and
     PowerShell-equivalent `perf_contract_smoke` assertions.
   - 2026-05-03 MIR statement-inventory accessor seam:
     `MIRBasicBlock` now carries
     `MIRStatementInventory source_statement_inventory` instead of raw
     `source_statements` / `source_statement_count` fields. Statement population
     routes through `mir_block_source_inventory_count(...)`,
     `mir_block_source_inventory_at(...)`, and
     `mir_block_source_inventory_items(...)`, while use-edge validation consumes
     the named inventory directly. This does not remove HIR source statements
     from MIR construction yet; it makes the remaining construction input an
     explicit inventory contract instead of an open block array. Gate: manual
     native MinGW `test-mir` (`33/0`), `test-transpile` (`710/0`), and
     PowerShell owner/contract size scans.
   - 2026-05-03 MIR statement-inventory validation:
     `mir_validate(...)` now rejects malformed statement inventory storage
     (`count > 0` with no `items`) and instruction source-statement indexes
     outside the named inventory. The regression fixture corrupts both shapes
     explicitly, so downstream MIR consumers no longer rely only on defensive
     null checks. Gate: manual native MinGW `test-mir` (`33/0`) and
     `test-transpile` (`710/0`).
   - 2026-05-03 MIR HIR-pointer cleanup:
     `MIRBasicBlock` no longer stores the raw `source_hir_block` pointer. The
     MIR contract keeps only `source_hir_block_id`, which is enough for CFG
     mapping validation, MIR dumps, and C block mapping comments. This removes
     another AST/HIR-carried pointer from MIR block state without changing
     emitted code. Gate: manual native MinGW `test-mir` (`33/0`) and
     `test-transpile` (`710/0`).
   - 2026-05-03 CFG loop-flow consumer tightening: `while` and static range
     `for` statements now retun semantic CFG flow flags to their parent body
     instead of being flattened through the generic statement fallback. The
     accepted slice is conservative: `while true { retun ... }` satisfies
     non-`Void` all-path retun, and `for i in 0..1 { retun ... }` satisfies it
     only when the range is statically non-empty and no `break` path exits the
     loop. `for-in`, empty ranges, dynamic ranges/conditions, possible `break`,
     and non-retuning backedges remain fallthrough. Gate:
     `make test-semantic cfg-body-dataflow-test-smoke` (`2497/0` semantic
     tests).
   - 2026-05-03 DAG intent/action-contract seam tightening:
     `type_checker_intent_role_fields.c` no longer owns a second local
     materializing type-ref helper, and `type_checker_func_action_contract.c`
     consumes annotation metadata for action-contract domain-slot/parameter
     reads. This shrank the resolver inventory cap from `12` to `10`; the
     2026-05-03 generic/host/intent-type follow-up below shrinks it again to `5` while keeping
     fallback/materializer counters at zero. Negative probes confirmed
     `type_checker_ownership_let_helpers.c` still needs earlier collection
     shell/key-policy metadata before leaving the compatibility path. Gate:
     `make test-semantic type-resolution-dag-test-smoke
     type-resolution-resolver-inventory-test-smoke`.
   - 2026-05-03 DAG generic materializer seam removal:
     `type_checker_generic_effective_args.c`,
     `type_checker_generic_contracts.h`, and
     `type_checker_generic_validation.c` now consume annotation metadata and
     `semantic_type_resolution_lookup_metadata_type_ref(...)` only; they no
     longer call the materializing type-ref helper. `type_checker_host_helpers.c`
     now follows the same metadata-only path for host/domain slot type reads,
     `type_checker_func_decl.c` now uses metadata-only lookup for function
     parameter/retun signatures, and `type_checker_expr.c` does the same for
     expression-local annotations. `type_checker_ownership_let_helpers.c` now
     consumes metadata type-ref facts plus the stable-shell arity,
     constructed-type, and unknown-bare-name diagnostic helpers; the rejected
     annotation-only probe caused broad semantic drift, so the accepted closure
     is metadata + diagnostics, not annotation-only. Semantic owners no longer
     call the materializing type-ref helper. The resolver inventory cap is now
     `2`, covering only the central API declaration/implementation, with
     fallback/materializer counters still at zero.
     Gate:
     `make test-semantic type-resolution-dag-test-smoke
     type-resolution-resolver-inventory-test-smoke` (`2500/0` semantic).
   - 2026-05-04 DAG direct named resolver seam closure:
     expression/world host access and overlay world-zone slot registration now
     consume metadata-only named-type lookup seams instead of calling
     `resolve_named_type(...)` directly. The retired `resolve_named_type(...)`
     API and prototypes were removed; the resolver inventory smoke rejects
     reintroducing the symbol anywhere under `src/semantic`. The now-unused
     `type_checker_resolution_helpers.h` compatibility header was also removed;
     intenal declarations live in `type_checker_intenal.h`. Gate:
     `make test-semantic type-resolution-dag-test-smoke
     type-resolution-resolver-inventory-test-smoke` (`2500/0` semantic,
     `retired_resolver_calls=0`, `materializer_unresolved=0`).
   - 2026-05-03: intent compression provenance tightened. A `using` binding
     derived from a unique `where`/zone type now records
     `derived_using_from_where`, so AST print and contract summaries no longer
     report it as a local clause. Gate:
     `make test-parser test-semantic cfg-body-dataflow-test-smoke` (`2498/0`).
   - 2026-05-03: intent transfer compression provenance tightened through DIR
     and AIR. Transfer-only steps can now derive both `where` and `using` from
     the transfer target; DIR records `where_derived_from_transfer` and
     `using_derived_from_transfer`, validates that provenance has concrete
     zone/binding facts, and AIR marks the corresponding zone boundary with
     `source_from_transfer`. Gate: `make test-semantic test-dir test-air
     test-transpile cfg-body-dataflow-test-smoke
     intent-compression-contract-test-smoke type-resolution-dag-test-smoke
     type-resolution-resolver-inventory-test-smoke perf-contract-test-smoke`
     (`2500/0` semantic, `9/0` DIR, `67/0` AIR, `710/0` transpile).
2. **CFG body safety source-of-truth 승격.** all-path retun / definite assignment /
   move-use / pin cleanup 이후 ownership/drop/zone/effect transition 소비자가
   CFG/MIR fact를 직접 소비하게 만든다.
3. **DAG source-of-truth 마감.** fallback 수치 0 유지가 아니라 generic/ability/
   alias/module visibility 판단이 DAG metadata/API를 공식 경로로만 통과하게 한다.
4. **MIR/LLVM declaration inventory debt 제거.** frozen subset declaration/bootstrap
   inventory는 AST-carried metadata가 아니라 DIR/RIR/MIR inventory만 소비한다.
5. **Runtime propagation frontier scheduler.** world/zone/projection bounded
   recompute 다음 단계인 full transitive frontier scheduler를 마감한다.
6. **ABI ownership freeze.** Slot/Pin cleanup, Zone-bound handle, runtime-none,
   raw escape, ABI non-leakage를 코드 gate와 문서 계약으로 동시에 고정한다.
7. **WASM/WebGL dogfood feasibility.** beta semantic closure 이후 C backend
   `--emit-c` 산출물을 optional Emscripten으로 빌드하는 최소 WebGL bridge
   smoke를 만든다. native LLVM wasm target은 beta+1이다.

**운영 원칙:** 테스트 스위트를 무작정 넓히기 전에, 위 순서의 한 feature-owner를
먼저 닫고 그 feature의 gate를 초록으로 만든다. 새 키워드/새 축은 추가하지 않는다.

## 0b. Forward Plan — WASM/WebGL 경로 (post-beta 우선순위 2)

**입안일:** 2026-05-01. **상태:** 계획. **scope:** 사용자가 *이미 만들고 싶어하는*
도메인(웹 던전 크롤러)을 Pergyra 표면 안에서 가능하게 하는 최소 경로.

**우선순위 메모 (2026-05-01 재평가):** 직전엔 priority 1로 박았으나 §0b
analysis 후 priority 2로 강등. *언어 정체성 활용도*에서 server backend가
WebGL보다 강함 — `intent` saga / `authority` / `Result<T>`는 transactional
saga에서 *우회 없이* 작동하지만 WebGL은 DOM/JS shim 우회 필수. WebGL은
*개인 동기 + 시각적 마케팅 가치*로 여전히 유의미하나 ecosystem leverage
순서에서 §0b 뒤로.

### 결정 — JS 백엔드 ❌, C→Emscripten/WASM dogfood bridge ✅
- **JS 백엔드 거부 사유:** GC + f64-only + reference-only emit 결과는
  `slot<T>` / zone / intent / authority *모두 흘림*. parity gate가 tri-way로
  분기 → 베타 closure 정합성 무너짐. docs/120 §4 vision territory 위반.
- **베타 채택 사유:** native LLVM wasm backend가 아니라 C backend 산출물을
  Emscripten으로 빌드하는 dogfood bridge를 1차 경로로 둔다. 이 경로는 새
  public syntax 없이 host import/frame callback/resource handle smoke를
  열 수 있고, C backend의 기존 parity discipline을 유지한다.
- **beta+1 대상:** native LLVM wasm target, richer render module
  (`pgy.render.webgl`, `pgy.render.skia`, `pgy.accel.spray`)은 dogfood evidence
  이후 별도 track으로 둔다. `wasm32` target triple wiring을 "한 줄 작업"으로
  과소평가하지 않는다.

### Why WebGL — Pergyra가 *유난히 잘 표현*하는 자리
WebGL은 **불투명 리소스 핸들 API**. JS는 GC에 위임 → 텍스처 누수, 컨텍스트
loss 미스, 프레임 간 상태 누수가 사용자 책임. Pergyra slot/zone/intent가
바로 그 자리:

| WebGL 개념 | Pergyra 자연 표현 |
|---|---|
| `WebGLBuffer` opaque | `slot<Buffer>` |
| `WebGLTexture` opaque | `slot<Texture>` |
| `WebGLProgram` opaque | `slot<ShaderProgram>` |
| Vertex buffer ownership | `authority { ... }` |
| Render pass 경계 | `zone GPUFrame { ... }` |
| Frame 단위 의도 | `intent RenderFrame { precondition ... success ... }` |

memory: `project_killer_usecase_dungeon_crawler.md` 와 1:1 일치.

### 현재 인프라 — dogfood bridge 기준으로 일부 준비됨
- ✅ `exten "ABI" { func ...; }` 파싱 — `src/parser/parser_decl.c:296`
  (`parse_exten_block`)
- ✅ `AST_EXTERN_BLOCK` LLVM 등록 — `src/codegen/llvm_register.c:322-328`
- ✅ C backend `--emit-c` 경로가 dogfood bridge의 1차 source-of-truth
- ⚠️ LLVM 백엔드 자체는 작동하지만 native wasm target은 beta blocker가 아님
- ✅ `make dogfood-webgl-test-smoke` — C 산출물에서 host import/frame callback
  bridge term을 검증하고, `emcc`가 있으면 HTML/JS wasm shell link까지 확인
- ❌ `exten "wasm-import"` ABI 인식 (현재 `exten "C"` 중심) — 설계와 진단 필요
- ❌ `Array<T>::as_raw_view()` stdlib (vertex buffer 데이터 패싱용)
- ❌ JS shim 자체 (수백 LOC, *언어 외부*)

### 진짜 막히는 자리 (3개)
1. **Linear memory pointer 노출** — `slot<Array<f32>>` → `(ptr, len)` 추출.
   JS shim이 zero-copy로 `new Float32Array(wasm.memory.buffer, ptr, len)`
   읽음. `as_raw_view()` 추가 필요.
2. **WASM module exports — frame loop** — JS의
   `requestAnimationFrame`이 wasm 함수를 매 프레임 호출. LLVM `exten "C"`
   symbol export 동작 검증 필요 (표준 LLVM이면 자동).
3. **Texture 업로드 path** — `gl.texImage2D`의 `HTMLImageElement` 입력은
   DOM-only. 우회: JS에서 디코드 → wasm 메모리에 raw pixel 쓰기 → wasm이
   raw 버전 `texImage2D` 호출. glue 30~50 LOC.

### Phase 분할

**Phase 0 (베타 내부) — C→Emscripten feasibility 경로 확보**
- [ ] `examples/wasm_hello/` — `exten "C" fn console_log(...)` 호출하는
  최소 .pgy + 손으로 쓴 wasm-loader HTML
- [ ] 산출물: `pgy --backend=c --emit-c hello.pgy → hello.c`,
  optional `emcc hello.c` link, 브라우저에서 콘솔 출력
- [ ] `make dogfood-webgl-test-smoke` — Emscripten이 없으면 skip 이유를
  명시하고, 있으면 host-import/frame-callback link를 검증. 기본 smoke target은
  이미 존재하며, 남은 일은 repository example과 render-resource fixture 확장
- [ ] *베타 scope 확장 아님* — native LLVM wasm target을 열지 않고 C backend
  dogfood bridge만 검증한다.

**Phase 1 (2-3주, 베타 직후) — WebGL MVP**
- [ ] `exten "wasm-import"` ABI 토큰 인식
- [ ] `Array<T>::as_raw_view()` stdlib 추가
- [ ] `examples/webgl_triangle/` — 화면에 컬러 트라이앵글 1개
- [ ] WebGL JS shim ~200 LOC (triangle 1개에 필요한 surface만)
- [ ] **Falsification 자리 (docs/122 §4):** slot lifecycle이 GL 컨텍스트
  loss와 자연스럽게 상호작용 하는가? authority가 GPU resource ownership을
  잘 표현하는가? F1-F6 신호 기록.

**Phase 2 (4-6주, 베타 후) — Dungeon crawler PoC**
- [ ] 사용자가 Pergyra만으로 던전 1 floor 렌더링 + 캐릭터 이동
- [ ] **WebGPU 직접 고려** — bind group / pipeline state object가 Pergyra
  intent / zone과 *훨씬* 자연스럽게 매핑. WebGL은 stepping stone.
- [ ] 1년 freeze recognition window의 핵심 evidence source
  (docs/122 §2.5 신호 매트릭스 입력).

### Out of scope (이 plan에서 *안* 함)
- DOM 직접 바인딩 (Pergyra 정체성과 안 맞음 — 얇은 JS shell만 손으로)
- HTTP server / 풀 networking stdlib (별도 plan)
- WASM GC types (proposal unstable, docs/120에 명시적 거부)
- JS 백엔드 (위 결정 사유)
- WASI 풀 surface (브라우저 target 우선)

### Verification 체크포인트
- Phase 0: `pgy --backend=c --emit-c` 산출물이 optional Emscripten으로 링크되고
  브라우저 로드/콘솔 출력 OK
- Phase 1: triangle 화면 표시, slot 누수 없음 (Chrome DevTools WebGL
  inspector), F1-F6 falsification 결과 `examples/webgl_triangle/FALSIFICATION.md`
- Phase 2: 던전 floor 60fps 안정, 사용자 피드백 evidence 수집

### 참조
- `docs/117_backend_strategy_positioning.md` — dual-emit 정책. WASM은
  LLVM family target 추가지 새 backend 아님
- `docs/120_vision_and_capability_audit.md` §4 — vision territory 정합성
- `docs/122_managing_intent_drift.md` §4 — falsification 프로토콜 적용
- memory: `project_killer_usecase_dungeon_crawler.md` — 핵심 동기

## 0c. Forward Plan — Intent-Compress (post-beta priority 0, self-host 직전)

**입안일:** 2026-05-02. **상태:** 계획 (★ Core Goal step 3에 박힘).
**scope:** intent block 의 *제거 가능한 유일한 verbosity*. compressed
form을 *디폴트*로, verbose 는 *명시 옵션*으로. 4 clause (`who`/`where`/
`requires`/`authorized by`) 를 컨텍스트에서 추론.

### 왜 이 sprint가 다른 trade-off와 *질적으로 다른가*

다른 trade-off (slot↔lifetime, layer 혼재, Result-first verbose) 는
*thesis나 mandate가 비용을 정당화*. intent verbose 는 그렇지 않음 —
*제거 가능한 비용*. 5가지 차이:

1. **Thesis가 요구하지 않음** — DDD primitive 1급 thesis 는 *intent 가
   언어 시민*이라 말하지, *8 clause 의례*를 요구하지 않음. clause 추론
   은 thesis *약화 아님*, ergonomics 표현. Rust lifetime elision 과 동일
2. **Signature 자리 가림** — intent block 은 학습자가 *5분 안에 만나는*
   Pergyra 의 signature 구문. 첫 인상이 무거우면 *thesis 평가 전에* 떠남
3. **라이브러리 비교 역전** — 현재 syntax 로 Camunda saga / Python 데코
   레이터보다 *길어 보임*. 마케팅 narrative ("언어 차원 강제") 가 syntax
   로 *역행*되는 자리
4. **Minimum floor 높음** — toy intent 불가능 (8 clause 강제). *낮은
   floor + 깊은 ceiling* 자연 확장 막힘. docs/120 §4.5 educational entry
   path 도 이 자리에서 막힘
5. **Self-host 진입 비용** — verbose intent 를 Pergyra 컴파일러 자체에
   서 다시 쓰는 비용. compress 후 self-host 진입 시 *훨씬 깔끔*. 이게
   step 3 가 step 4 *직전* 자리인 이유

### Direction A — compressed-default + 4 clause 추론

**Before (현재):**
```pgy
intent ProcessPayment {
    who: Customer,
    where: PaymentZone,
    requires: customer.balance >= amount,
    authorized by: customer.payment_authority,
    precondition: not order.paid,
    success: order.paid = true,
    failure: order.paid = false,
    compensate: refund_handler(order)
}
```

**After (compressed default):**
```pgy
intent ProcessPayment for Customer in PaymentZone {
    requires balance >= amount;
    authorized;
    success: order.paid = true;
}
```

전체 verbose form 은 명시 가능 — *예외 자리에서만*. 평균 케이스는 5-line
이내.

### 4 clause 추론 규칙 (sketch)

| Clause | 추론 source |
|---|---|
| `who:` | (a) 호출 site receiver (`customer.process_payment(...)` → who=customer) (b) intent 가 subject 안에 선언된 경우 그 subject (c) 명시 안 되면 require error |
| `where:` | (a) 호출 site 의 `zone` 컨텍스트 (b) intent 가 zone 안에 선언된 경우 그 zone (c) 명시 안 되면 world scope (d) world scope 도 ambiguous 하면 require error |
| `requires:` | (a) 본문 분석 — `who.field` 사용 시 자동 `who.field.exists` (b) numeric ops → 범위 추론 (c) ambiguous 자리는 명시 권고 (require 아님 — *strict mode flag* 로 강제 가능) |
| `authorized by:` | (a) 호출 site 의 `authority` 컨텍스트 propagate (b) `who` 가 authority 가지면 self (c) 명시 안 되면 require error (보안 자리이므로 *fail-closed*) |

### 충돌 해소

**explicit > inferred 일관**. 사용자가 명시한 자리는 *항상* 우선. 단
explicit 와 inferred 가 *다르면* waning (silently override 안 함).

### Sprint 분할

**Phase 1 (명세 우선, 견적 보류)** — 추론 규칙 design + AST/HIR 변경 설계
- [ ] 4 clause 추론 규칙 finalize
- [ ] AST 에 `inferred_who` / `inferred_where` 등 메타 필드
- [ ] HIR/MIR 은 *expanded form* 유지 (verification/debug 정합)
- [ ] semantic phase 에서 expansion 위치 결정

**Phase 2 (명세 후 산정)** — 구현
- [ ] semantic phase clause 추론 구현
- [ ] explicit-vs-inferred 충돌 검출
- [ ] backend-compare gate 정합 유지 (양 백엔드 같은 expanded form 사용)

**Phase 3 (1일)** — 진단 + 테스트 + 문서
- [ ] 추론 실패 진단 (*"이 자리에서 `who` 추론 불가, 명시 필요. 호출
  receiver 또는 enclosing subject 가 없음"*)
- [ ] negative test cases (추론 실패 자리들)
- [ ] examples/ 의 verbose intent 들을 compressed form 으로 마이그레이션
- [ ] dnd_taven_campaign / 결제 saga mock 양 backend 회귀
- [ ] docs/121 §3 carrier/coherence 자리에 *compressed form* 정합 추가
- [ ] docs/120 §4.4 self-host 항목에 *intent-compress 가 self-host 진입
  자격* 명시 (이미 박혀 있음, 강화)

### 검증 체크포인트

- 모든 examples/ 양 백엔드 회귀 zero
- AIR drift fact 정합 — expanded form 이 동일하면 AIR fact 도 동일
- backend-compare gate 69/69 유지
- *대표 intent 5개 LOC 측정* — 평균 5-line 이내 도달
- 마이그레이션 후 *educational angle* 가능성 검증 (toy intent 가능)

### Out of scope (이 sprint 에서 *안* 함)

- `precondition` / `success` / `failure` / `compensate` clause 자체
  변경 — 이건 thesis 의 핵심 표현, 추론 안 함
- 새 intent semantic 추가 — 이건 별도 ticket
- Educational entry path full 작업 (docs/120 §4.5 후보) — 이 sprint
  *그것을 unblock* 만 함, 본 작업은 별도

### 의존성 / 정합

- **Step 2 (dogfood) 의 evidence 가 input** — dogfood 가 *어느 clause가
  과잉 required 인지* 보여줘야 추론 규칙 정확. dogfood 없이 시작하면
  *추측*
- **Step 4 (self-host) 가 consumer** — self-host 컴파일러 자체가
  compressed form 사용. step 3 → step 4 순서 강제
- **docs/120 §4.4 (self-host) 와 §4.5 (educational, 후보)** 모두 이
  sprint 후 시작 가능

### 비용 추정 정정

Intent-Compress는 척추 변경이므로 "며칠 컷"으로 고정하지 않는다. Phase 1은
추론 규칙, 충돌 정책, 실패 진단, AST/HIR expansion 위치를 먼저 명세하고,
그 결과로 Phase 2/3 구현 견적을 다시 낸다. AI-assisted 구현은 반복 속도를
높일 수 있지만, 비용을 결정하는 것은 dogfood evidence와 진단 품질이다.

### 참조

- `docs/121_types_as_domain_medium.md` §3 — carrier/coherence/negative-space
- `docs/120_vision_and_capability_audit.md` §4.4 — self-host 진입 자격
- `docs/122_managing_intent_drift.md` §4 — falsification 프로토콜 (sprint
  내 적용)
- memory `feedback_marketing_language_drift.md` — marketing claim 과
  syntax 정합성 자리
- ★ Core Goal step 3 — 시퀀스 자리 anchor

## UTF-8 Progress Note - 2026-05-01 - Dogfood-first WebGL Bridge Gate

2026-05-01 update:
- Beta progress is now tracked as two numbers: user-visible feature progress is
  about 70%, while strict beta readiness is about 60%. The delta is
  CFG/AIR/DAG/MIR/ABI source-of-truth closure, not missing surface syntax.
- WebGL/WASM is no longer framed as "native LLVM wasm before beta". The beta
  dogfood entry path is `Pergyra -> C backend --emit-c -> optional Emscripten`.
  Native LLVM wasm and richer render modules stay beta+1.
- Added `make dogfood-webgl-test-smoke`. The smoke emits C for an `exten "C"`
  host log plus one frame callback and verifies that the generated C preserves
  the bridge calls. If `emcc` is installed, it also links an HTML/JS wasm shell;
  otherwise it reports a skip after the C bridge is validated.
- This is a dogfood-path gate, not a new keyword or new semantic axis. It does
  not change the runtime-none contract, which still explicitly rejects false
  freestanding lowering claims.

## UTF-8 Progress Note - 2026-05-01 - Hot-path Dispatch / Lookup Audit

2026-05-01 update:
- Closed one active Category A seam: `src/semantic/type_checker_builtins_resolve.c`
  no longer uses a 120+ entry sequential `strcmp` chain. It now owns a sorted
  builtin registry table and resolves through `bsearch`.
- Verification: `make test-semantic LLVM_ENABLED=0` and
  `make type-resolution-dag-test-smoke LLVM_ENABLED=0` passed with isolated
  `BUILD_DIR`/`BIN_DIR`. The DAG smoke kept `retired_resolver_calls=0` and
  `materializer_unresolved=0`.
- Intent observability scan status: the old implementation-header/TU
  duplication claim is stale. `intent_observability_usage.h` is declaration-only,
  and both C/LLVM entry points use the scan once per backend compile. The
  structural AST walk is now owned by `src/parser/ast_analysis.c` via
  `ast_contains_identifier_call(...)`; `intent_observability_usage.c` only
  supplies the intent-observability predicate and MIR traversal. Block-level
  source arrays and routine AST payloads are no longer scanned for this fact;
  declaration inventory scans are now materialized once into MIRProgram
  inventory surface facts (`has_inventory_surface_usage_facts`,
  `inventory_uses_intent_observability_surface`). Codegen consumes the MIR
  fact, while `mir_validate(...)` owns the AST-vs-fact stale check.
- Safety note: whole-program intent observability must not use a raw `Intent*`
  prefix rule. Use the exact `pgy_intent_observability_name_is_builtin(...)`
  registry for observability builtins, and use
  `mir_instruction_is_intent_semantic_carrier(...)` only for MIR intent
  inventory instructions. The only remaining `stncmp(name, "Intent", 6)` is
  the safe prefix cutoff inside the exact registry before `bsearch`.
- Remaining high-value follow-up: extend the same inventory fact patten to
  future whole-program feature bits (`uses_parallel` / `uses_async` /
  `uses_unsafe`) so codegen consumes facts instead of rescanning AST payloads.
  `transpiler_expr_type_infer.h` still
  duplicates builtin retun-type knowledge and should be retired behind semantic
  typed facts rather than expanded with more name checks.
- Closed parser AST growth debt: `src/parser/ast.c` no longer uses
  `realloc(count + 1)` for program/block/exten/namespace/parallel/call node
  lists. These lists now carry explicit capacity, grow geometrically, and leave
  the AST unchanged on allocation failure.
- Closed AST analysis owner creep: `src/parser/ast_analysis.c` now owns only
  generic identifier-call traversal plus intent-observability prefix detection.
  Thread-pool surface traversal moved to `src/parser/ast_thread_pool_analysis.c`
  and is listed in `PARSER_SOURCES`, keeping the split responsibility-based
  instead of a mechanical 600 LOC cut.
- Closed semantic scope lookup hot path: `Scope` now keeps an append-only
  open-addressed symbol index for current-scope duplicate checks and lookups
  while preserving the original `symbols` array for whole-scope iteration.
- Closed runtime intent active-handle lookup debt for the stable runtime
  surfaces: generated-C inline runtime and LLVM/export runtime now both keep a
  handle-to-active-slot index. Sequential active scans remain only where the API
  is explicitly index/enumeration based.
- Verification: `make test-semantic LLVM_ENABLED=0`, `make test-abi
  LLVM_ENABLED=0`, and `PGY_OBSERVABILITY_BACKENDS=c make
  observability-schema-test-smoke LLVM_ENABLED=0` passed with isolated
  `BUILD_DIR`/`BIN_DIR`.

post-beta 우선순위 정리 audit. 3개 Explore agent 병렬 실행, codegen
dispatch / semantic lookup / runtime data structure 3축 결과 통합. *정확성
회귀 zero, but 큰 프로그램 컴파일 / 긴 trace / 큰 AST 빌드에서 누적 비용
큰 자리들*. 베타 closure 위협 없음.

### Category A — Stdlib builtin dispatch (`strcmp` 체인 50+ 분기)
- `src/codegen/transpiler_expr_stdlib_scalar_builtin.h` 7-174 (15+ 분기)
- `src/codegen/transpiler_expr_stdlib_collection_builtin.h` 106-460 (23+
  분기). 추가로 line 147-219 부근 `strcmp(key, "Int"/"Long"/"Bool")`
  3-way tenary 5+ 곳 중복
- `src/codegen/transpiler_expr_stdlib_builtin.h` 112-289 (외부 dispatch
  11+ 분기)
- 처방: gperf 또는 정렬 + bsearch 단일 테이블, key_type 사전 분류
  enum 도입. 표 한 번에서 jump

### Category B — Symbol / scope / type lookup 선형 strcmp
- `src/semantic/symbol_table.c:141-143` — `scope_lookup_current` 선형
- `src/semantic/type_env.c:45-56` — `type_env_lookup_variable` scope chain
  × 선형 strcmp
- `src/codegen/transpiler_decl_lookup.c:113-118` — 캐시 있음, cold start
  O(N)
- `src/parser/parser_decl_hints.c:47-55, 99-108` — hint table 선형 strcmp
- `src/codegen/transpiler_statement_dispatch.h:59-62` — typed_var 선형
- `src/codegen/llvm_backend_type_map.c:147-156` — type alias 선형
- `src/semantic/type_checker_builtins_query_domain.c:19-26, 38-45, 62-69,
  76-100, 149+` — zone/relation/effect/world/state 5종 선형
- 처방: `src/runtime/pgy_runtime_builtin_hashmap_inline.h` 패턴을
  컴파일러측 owner로 분리 (e.g. `src/common/compiler_hashmap_inline.h`),
  Scope/TypeEnv/program-level decl index 도입. core symbol resolution이
  컴파일러의 가장 hot path — 영향 큼

### Category C — 다중 pass / 준-quadratic
- ✅ `src/semantic/slot_analyzer.c` — live slot collection is now 1-pass
  geometric growth, and function/branch live-set membership uses sorted
  pointer sets instead of O(after × before) nested scans.
- [~] `src/compiler/air_evidence.c` — MIR pin-cleanup evidence collection now
  iterates MIR routines/blocks first and only matches actual pin-region blocks
  against AIR pin boundaries. Remaining AIR cost item is broader HIR routine /
  boundary matching and typed boundary-id indexing.
- ✅ `src/compiler/air.c` — AIR evidence inventory and owned-name storage now
  use explicit capacity growth instead of `realloc(count+1)`.
- ✅ `src/compiler/air_verify.c` — AIR drift inventory now uses explicit
  capacity growth instead of `realloc(count+1)`.
- ✅ `src/compiler/hir.c` — call-graph closure now builds a sorted
  `HIRRoutineNameIndex` once and resolves direct calls through indexed lookup
  instead of scanning every routine for every call.
- ✅ `src/semantic/type_checker_resolution_metadata.c` — DAG metadata lookup now
  uses an AST-node pointer index instead of scanning every metadata entry on
  each lookup. The raw arrays remain for ordered iteration / ownership cleanup.
- ✅ `src/semantic/semantic.c` — stdlib preload append paths now consume AST
  program capacity and use geometric growth for the loaded-module list instead
  of `count+1` realloc.
- ✅ `src/compiler/dir.{h,c}` / `src/compiler/dir_collect.c` — DIR node,
  edge, owned-name, intent, participant, step, and intent-step name arrays now
  use explicit capacity growth instead of `count+1` realloc. This keeps the
  domain graph storage owner aligned with the same IR-storage contract as AIR
  and semantic preload.
- ✅ `src/compiler/hir.{h,c}` / `src/compiler/hir_routines.c` — HIR top-level
  category arrays, item/decl/routine arrays, and routine callee-id lists now use
  explicit capacity growth. Remaining HIR storage debt is scoped to CFG-local
  block/statement/predecessor/name arrays in the CFG owners.
- ✅ `src/compiler/hir_analysis.c`, `src/compiler/hir_cfg.c`,
  `src/compiler/hir_cfg_phi.c` — HIR signature/direct-call collection and CFG
  fact arrays (predecessors, dominance frontier, dom-tree children, local defs,
  phi candidates) now use explicit capacity growth. Remaining HIR storage debt
  is narrowed to CFG lowering block/statement construction.
- ✅ `src/compiler/hir_lower_intent_cfg.c` — intent CFG block and statement
  construction now uses explicit capacity growth. Remaining HIR lowering debt is
  the general statement CFG builder in `hir_lower_cfg_blocks.c`.
- ✅ `src/compiler/hir_lower_cfg_blocks.c` / `src/compiler/hir_lower_cfg.c` —
  general function-body CFG block and statement construction now uses explicit
  capacity growth and carries `cfg.block_capacity` through lowering. HIR no
  longer has known `count+1` append storage in its stable lowering/analysis
  owners.
- ✅ `src/compiler/rir.{h}` / `src/compiler/rir_facts.c` — RIR scope, fact,
  operation, and state-summary storage now uses explicit capacity growth instead
  of `count+1` realloc. The RIR fact owner now follows the same storage contract
  as HIR/DIR/AIR.
- ✅ `src/compiler/mir.h`, `src/compiler/mir_base_helpers.h`,
  `src/compiler/mir_cleanup.c`, `src/compiler/mir_intent.c` — MIR routine/block
  storage, instruction append/insert, cleanup predecessor append, and intent
  instruction append now use explicit capacity growth.
- ✅ `src/compiler/mir_decl_headers.h` / `src/compiler/mir_liveness_dce.h` —
  MIR declaration-header and value-summary storage now uses explicit capacity
  growth.
- ✅ `src/compiler/mir.h`, `src/compiler/mir_base_helpers.h`,
  `src/compiler/mir_ssa_rename.h`, `src/compiler/mir_ssa_use_edges.h`,
  `src/compiler/mir_liveness_dce.h` — MIR SSA/use/liveness name-list arrays now
  carry explicit capacities and grow geometrically. Remaining MIR reallocs in
  this owner are deliberate DCE shrink operations or fixed-size copies, not
  append-path `count+1` storage.
- 처방: routine/boundary id 인덱싱 (이름 strcmp 매칭 → id 비교), AIR
  evidence 빌드는 outer 1회 인덱스 빌드 후 inner는 hashmap probe. slot
  collect 1-pass

### Category D — Runtime hot path 자료구조
- ✅ `src/runtime/pgy_runtime_intent_trace_inline.h` — intent registry
  handle 조회는 handle→slot inline index로 고정됨.
- ✅ `src/runtime/pgy_runtime_intent_trace_inline.h` — trace append는
  `trace_len`을 추적해 기존 trace 길이 `strlen` 재계산을 피함.
- ✅ `src/runtime/pgy_runtime_intent_trace_events_inline.h` — step begin은
  빈 participant/slot/from/to/failure 필드를 미리 `strdup("")`하지 않고
  필요할 때만 materialize함.
- ✅ `src/parser/ast.c` — AST list append uses explicit capacity and
  geometric growth; the old `realloc(count+1)` O(N²) path is closed.
- ✅ `src/semantic/type_checker_flow_resources.h` /
  `src/semantic/type_checker_flow_loops.h` — body-safety resource snapshots now
  carry explicit capacity and grow through all-or-nothing reserve copies. This
  removes the prior multi-`realloc(count+1)` append path from parallel/loop
  ownership snapshots and avoids partial-realloc pointer loss on OOM.
- ✅ `src/semantic/type_system.h` / `src/semantic/type_env.c` — type environment
  variable/type bindings now carry explicit capacities and grow geometrically
  instead of reallocating on every append.
- ✅ `src/parser/parser_expr.c` — call pipe-prepend now uses the existing
  `AST_CALL.arg_capacity` field instead of reallocating to `old_count + 1`.
  Broader parser AST-list capacity cleanup remains a separate parser-owner
  task because many node variants still expose count-only arrays.
- ✅ `src/parser/parser.c`, `src/parser/parser_async.c`, `src/parser/ast.h` —
  destructuring names, async function parameters, async block statements, and
  select cases now carry explicit capacity fields and grow geometrically.
- ✅ `src/parser/parser_decl.c`, `src/parser/ast_constructors.c`,
  `src/parser/ast.h` — function parameters and nominal field/method lists now
  carry explicit capacities and grow geometrically during parse.
- ✅ `src/parser/parser_decl_function_clause.c`, `src/parser/ast.h` —
  function/action `requires` and `authorized by` clause arrays now use explicit
  capacities instead of count-only append reallocs.
- ✅ `src/parser/parser_type.c`, `src/parser/ast_types.h`,
  `src/parser/ast_domain_tail_constructors.c` — generic parameter lists,
  where-clause constraint lists, type-bound lists, and event-handler function
  type parameter lists now use explicit capacities.
- ✅ `src/parser/ast_domain_data.h` /
  `src/semantic/type_checker_intent_action_contract.c` — inherited intent-step
  `who`, `requires`, and `authorized by` lists now carry explicit capacities
  and avoid semantic `count+1` append paths.
- `src/runtime/pgy_runtime_builtin_hashmap_inline.h:113-120` — open
  addressing linear probing + strcmp per probe
- ✅ `src/runtime/pgy_runtime_queue_inline.h` — queue grow now uses `realloc`
  when `head == 0`; wrap-around queues keep the ordered-copy path.
- 처방: handle→slot inline hashmap, 빈 문자열 단일 sentinel 공유, AST
  capacity 별도 추적 + `next_pow2` grow, secondary hash 또는 quadratic
  probing

### 좋은 패턴 (이미 정확 — 회귀 방지용 기록)
- ✅ `src/common/arena.{c,h}` — bump + linked block + O(1) destroy
- ✅ `src/runtime/pgy_runtime_channel_inline.h` — 링버퍼 정확
- ✅ `src/runtime/pgy_runtime_plain_slot_inline.h` — debug-only safety
  check, 릴리즈 zero-overhead
- ✅ `src/codegen/transpiler_decl_lookup.c` — `last_decl_lookup_*` 캐시
- ✅ `src/parser/ast_destroy.c` — 정확 (단 arena로 옮기면 O(1))

### Sprint 우선순위 (베타 closure 후)

**Sprint Q (1-2주, 베타 closure 직후)**
- Q1. Category A 테이블화 — stdlib builtin dispatch 50+ 분기 → 단일 표
  + bsearch (또는 gperf). `key_type_classify(key)` 단일 함수로 통합
- Q2. Category B hashmap — Scope / TypeEnv / type alias map 도입.
  `compiler_hashmap_inline.h` 신규 owner
- Q3. AST geometric growth — `ast_add_*` capacity 별도 추적 + 기하 grow
- 검증: backend-compare 69/69, `make test-{air,semantic,mir,parser}`
  zero 회귀, 큰 .pgy 컴파일 시간 측정 표

**Sprint R (2-3주, Q 후) — 분석 패스 단축**
- R1. [~] Routine/boundary id 인덱스 (HIR routine-name call graph closed;
  AIR typed boundary-id indexing remains)
- R2. ✅ slot analyzer 1-pass collect + sorted live-set membership

**Sprint S (1주, R 후) — Runtime trace 정리**
- S1. ✅ Intent registry 핸들→슬롯 inline index
- S2. ✅ step begin 빈 문자열 allocation 제거
- S3. ✅ trace string append length tracking (`trace_len`)

### Out of scope (이 audit에서 *안* 다룸)
- AST arena 이행 (별도 큰 ticket)
- step name intened-id (별도 ticket, 언어 차원 변경 가능성)
- LLVM 백엔드 자체 최적화 패스 추가
- mimalloc/jemalloc 등 allocator 교체 (vision territory)
- gperf 의존성 추가 부담 시 정렬 + bsearch로 대체 가능 — 결정 필요

### 우선순위 정합
- §0a (WASM/WebGL) / §0b (Server backend) 와 *직교*. 어느 쪽 path를 먼저
  가든 Sprint Q/R/S 모두 이득
- 사용자 1인 프로젝트면 Sprint Q만 우선 처리해도 큰 데모에서 차이 큼
- 베타 closure 정합성 위협 없음 — 베타 후 우선순위 1 후보

## UTF-8 Progress Note - 2026-04-30 - AIR/DAG/CFG Contract Tightening

- AIR DAG evidence now reports actual generic/ability compatibility fact
  counts and treats any non-zero metadata materializer fallback as
  `AIR_DRIFT_DAG_FALLBACK_PRESENT` under strict evidence. This keeps DAG
  fallback debt visible at the abstraction-safety layer instead of letting it
  hide behind successful metadata hit counters.
- AIR MIR evidence now records global cleanup-block evidence as
  `AIR_EVIDENCE_MIR_CLEANUP` with `cleanup-block` provenance. MIR still owns
  cleanup generation and validation; AIR audits that the MIR cleanup fact exists
  and remains observable through the evidence graph.
- LLVM MIR fallback control is aligned with the compiler CFG-owned control
  contract. `llvm_mir_stmt_is_cfg_container(...)` now rejects fallback emission
  for `with`, `unsafe`, `defer`, `if`, `while`, `for`, `select`, `match`,
  `break`, `continue`, and `retun`, matching the compiler-side
  `mir_stmt_ast_is_cfg_owned_control(...)` policy.
- Deleted the untracked root `ast` grep-output artifact. `src/**/*.inc` remains
  at zero files, and representative production owners stay under the 600 LOC
  split-review threshold.
- Local gates: `make cfg-body-dataflow-test-smoke`, `make test-air`,
  `make air-drift-test-smoke`, `make type-resolution-dag-test-smoke`,
  `make type-resolution-resolver-inventory-test-smoke`, `make test-mir`, and
  `make llvm-test-backend-compare` are green. Backend compare reports `69/69`
  cases passed.
- Follow-up DAG seam cleanup: metadata annotation readers are now centralized
  behind `semantic_type_resolution_lookup_annotation_nullable(...)` and
  `semantic_type_resolution_lookup_annotation_or_unknown(...)`. The resolver
  inventory gate now requires annotation-sensitive direct seams to stay at
  zero; local gates `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`, and `make test-semantic` are green
  (`2359/0` semantic tests).
- Follow-up DAG public seam cleanup: raw resolved-type lookup is no longer
  exported through the semantic mega-header. It now lives behind the private
  metadata-owner header `type_checker_resolution_metadata_intenal.h`, and the
  resolver inventory smoke rejects any re-export through
  `type_checker_intenal.h` or non-metadata owners. This keeps
  `semantic_type_resolution_lookup_annotation_nullable(...)`,
  `semantic_type_resolution_lookup_annotation_or_unknown(...)`, and
  `semantic_type_resolution_lookup_or_materialize(...)` as the stable DAG-facing
  APIs outside metadata materialization owners.
- Follow-up DAG stage materializer gate: `type-resolution-dag-test-smoke` now
  parses and caps `stage-metadata-materialize` totals directly. Calls, failed
  materializations, and suppressed diagnostics must all remain `0`, not merely
  the family counters. This prevents a compatibility materializer from
  reappearing as a hidden successful path.
- Follow-up DAG writer inventory gate: resolved-type metadata recorders are now
  smoke-gated to graph, stage-signature, and metadata materialization owners.
  New semantic owners cannot write DAG resolved-type facts directly without
  failing `type-resolution-resolver-inventory-test-smoke`.
- Follow-up DAG stage-signature fallback removal: signature staging no longer
  calls the metadata materializer after a metadata miss. It now consumes graph
  dependency evidence and existing metadata only, then retuns `TYPE_UNKNOWN`
  for unresolved quiet staging. The retired compatibility-family recorder was
  removed and the inventory smoke rejects reintroducing it or a stage-signature
  materializer call.
- Follow-up DAG diagnostic read-only tightening: metadata diagnostics now use
  `semantic_type_resolution_lookup_metadata_type_ref(...)` for generic argument
  checks instead of the materializing type-ref helper. The resolver inventory
  smoke rejects reintroducing materializer lookup from metadata diagnostics, so
  diagnostic-only paths cannot create new DAG resolved-type facts.
- Follow-up DAG helper inventory cap: metadata-first type-ref helper use is now
  owner-classified and capped at 15 total references, including the central
  declaration and implementation. New
  `semantic_type_resolution_lookup_type_ref_or_materialize(...)` call sites
  fail `type-resolution-resolver-inventory-test-smoke` until they are
  deliberately classified, which prevents silent expansion of materializing DAG
  seams.
- Follow-up DAG generic-param evidence tightening: class, function, ability,
  and nominal staging generic parameters now register as
  `SYMBOL_TYPE_PARAM` with `TYPE_KIND_GENERIC`, not as class-like placeholder
  symbols. `type-resolution-dag-test-smoke` now requires non-zero
  `GENERIC_PARAM` graph evidence (`generic_param_nodes=29` locally), keeping
  generic params visible as generic evidence instead of declaration evidence.
- Follow-up DAG class-field seam removal: class/subject/vessel field signature
  staging now records metadata before graph-backed skip accounting, and
  `type_checker_class_decl.c` consumes annotation metadata instead of the
  materializing type-ref helper. The helper inventory is now capped at 14
  references, with local DAG stats at `graph-backed skips=2450`,
  `metadata_hits=7582`, and all materializer/retired-resolver counters at 0.
- Follow-up DAG domain/world field seam removal: relation/effect/zone/world
  field staging now records metadata before semantic owner checks consume the
  types. `type_checker_decls_domain_helpers.c` and
  `type_checker_world_helpers.c` now read annotation metadata instead of the
  materializing type-ref helper. The helper inventory is capped at 12
  references, with local DAG stats at `graph-backed skips=1980`,
  `metadata_hits=8052`, and all materializer/retired-resolver counters at 0.
- Follow-up DAG effective generic arg seam tightening: ability where validation
  now consumes centralized `collect_effective_generic_arg_types(...)` evidence.
  `type_checker_ability_where.c` is annotation-only again; the one remaining
  materializing lookup for effective ability args lives in
  `type_checker_generic_effective_args.c`, so owner-local ability validation no
  longer creates DAG facts as a side effect. ABI/runtime layout is unchanged,
  and the implementation did not grow the generic support implementation header.
  Intent role-field require checks now consume the same centralized effective
  generic arg type evidence, leaving `type_checker_intent_role_fields.c` under
  the 600 LOC split-review line.
- Follow-up generic class specialization evidence tightening: class
  specialization where-clause validation now consumes the same centralized
  effective generic arg type evidence instead of building a local Type array
  from effective arg nodes. This removes duplicate dependency/materialization
  work while keeping the materializing seam count capped at 12. Local DAG stats:
  `graph-backed skips=1980`, `metadata_hits=8044`, and all
  materializer/retired-resolver counters at 0.
- Follow-up intent binding owner split: intent participant/value lookup and
  transfer-target alias resolution moved to
  `type_checker_intent_bindings.c`. `type_checker_intent_role_fields.c` now
  stays focused on ability require-field validation and zone-binding
  derivation, dropping to 499 LOC without changing ABI/runtime layout or DAG
  fallback policy. Local gates: `make test-semantic`,
  `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`,
  `make build-source-inventory-test-smoke`, and
  `make semantic-tu-size-test-smoke`.
- Follow-up intent type owner split: intent-local type-ref resolution,
  participant/value type resolution, and step where-source labeling moved to
  `type_checker_intent_types.c`. `type_checker_intent_decl.c` dropped to 529
  LOC and now focuses on orchestration validation rather than carrying the
  materializing type helper inline. The resolver inventory still gates the same
  12 metadata-first type-ref helper references and all fallback/materializer
  counters remain at 0.
- Follow-up DAG intent inventory owner split: intent declaration precollect
  moved from the general declaration graph owner to
  `type_checker_resolution_graph_intent.c`. This keeps intent DAG inventory
  facts close to the intent surface and drops
  `type_checker_resolution_graph_decl.c` to 481 LOC without changing DAG stats
  or metadata/fallback counters.
- Follow-up DAG zone command inventory owner split: zone refresh/apply/link/
  detach/unlink/maintain dependency precollect moved to
  `type_checker_resolution_graph_zone_commands.c`. The remaining
  `type_checker_resolution_graph_zone_inventory.c` now owns only zone
  slot/shared/layer type inventory and is 76 LOC. This is a responsibility
  split, not a mechanical line-count split, and preserves the existing DAG
  stats (`graph-backed skips=1980`, `metadata_hits=8044`) with all
  materializer/retired-resolver counters at 0.
- Follow-up DAG graph validation owner split: the old
  `type_checker_resolution_graph_core.h` implementation header is removed.
  Graph cycle validation and topo ordering now live in
  `type_checker_resolution_graph_validate.c`, while
  `type_checker_resolution_graph_core.c` keeps node/edge/path/dependency
  primitives below the 600 LOC split-review signal.
- Follow-up split-policy correction: the 600 LOC rule is now a split-review
  trigger, not a mechanical slicing mandate. New `_helpers` owners are
  discouraged unless they name a real feature/fact owner, and near-duplicate
  helpers should become data-driven dispatch. As a small cleanup proof,
  `llvm_stmt_let_collections.c` now uses one enum-driven
  `llvm_stmt_diag_collection(...)` helper for missing type-argument and missing
  runtime-export diagnostics instead of two parallel helpers. Syntax gate:
  `gcc -DPGY_LLVM_ENABLED -fsyntax-only src/codegen/llvm_stmt_let_collections.c`.
- Follow-up CFG/MIR root identity validation: MIR validation now rejects
  overlapping entry/cleanup/rollback/invalidation roots, not only invalid root
  indexes. This prevents cleanup chain corruption from being hidden behind a
  valid block index. Local gate: `make test-mir cfg-body-dataflow-test-smoke`.
- Follow-up 600 LOC owner closure: `mir_cfg_contract_validate.h` no longer sits
  on the split threshold. Cleanup-edge fact lookup moved to
  `mir_cfg_contract_cleanup_fact.h` (26 LOC), leaving
  `mir_cfg_contract_validate.h` at 584 LOC. Local gates: `make test-mir` and
  `make cfg-body-dataflow-test-smoke`.
- Follow-up AIR/MIR evidence consumption: AIR now records whether MIR input was
  attached and default strict verification requires `AIR_EVIDENCE_MIR_PIN_CLEANUP`
  for `pin` execution boundaries once MIR evidence is available. This keeps MIR
  as the cleanup source of truth while preventing AIR from treating a pin
  boundary as closed without the matching MIR `pin-unpin-cleanup-edge` evidence.
  Local gate: `make test-air` (`77/0`).
- Follow-up AIR/CFG cleanup fact tightening: AIR no longer accepts an orphan
  `pin-unpin-cleanup-edge` instruction as pin cleanup evidence. The MIR pin
  block must also have a real cleanup successor that targets the routine cleanup
  block, so AIR consumes the CFG cleanup fact rather than trusting a standalone
  instruction string.
- Follow-up AIR evidence inventory shape tightening: strict evidence inventory is
  now boundary-shape checked. Global evidence cannot attach to a concrete
  boundary; HIR CFG evidence requires same-boundary HIR routine evidence; RIR
  authority evidence requires same-boundary RIR boundary evidence and a declared
  participant; MIR pin cleanup evidence can only target a `pin` execution
  boundary. Local gates: `make test-air`, `make air-drift-test-smoke`,
  `make air-json-schema-test-smoke`, and `make cfg-body-dataflow-test-smoke`.
- Follow-up AIR observability evidence closure: observability/trace schema is
  now a global `AIR_EVIDENCE_OBSERVABILITY_SCHEMA` node with provider
  `runtime-observability-schema`, subject `pgy.intent.observability.v1`, and
  a fact count derived from the runtime schema vocabulary. This moves the
  trace/observability ABI contract from JSON-only output into AIR evidence
  inventory. Local gates: `make test-air air-drift-test-smoke
  air-json-schema-test-smoke air-backend-nonimpact-full-test-smoke`.
- Follow-up DAG materializing seam reduction: abstract ability method
  signatures, projection path field readers, zone-authority subject-slot readers,
  world domain/shared type readers, expression-level method-call retun,
  host-expression, constructor field, and intent participant/transfer/
  inherited-action readers now use
  `semantic_type_resolution_lookup_metadata_type_ref(...)` instead of the
  materializing type-ref helper. The owner-local fallback seam inventory is now
  gated at `0` (`fallback seams=0 cap=0`) while `retired_resolver_calls=0`,
  `materializer_fallbacks=0`, and `stage_materialize_calls=0` remain gated.
  Remaining DAG gaps are not simple fallback replacements: action-contract
  inheritance, intent role-field derivation, host overlay authority checks,
  generic ability where-clause diagnostics, and generic default validation need
  explicit stage/effective-argument evidence before their materializing
  owner-local seams can be removed without breaking semantic behavior.
- Follow-up AIR JSON schema gate: `make air-json-schema-test-smoke` now runs
  `pgy --air-json` on a stable intent/zone fixture and gates the
  `pgy.air.graph.v1` summary, boundary, evidence, drift, and observability
  shape. Python parses the JSON when present; when Python is absent, the smoke
  falls back to literal schema checks so platform CI does not fail merely
  because an interpreter is missing.
- Post-beta/beta+1 modeling pain point: add a zone-first authoring path that
  lets users model a business graph primarily with `zone` plus passive
  `struct/object/vessel` shapes, then progressively introduce `subject`,
  `authority`, and `projection` only when state-transition auditing, boundary
  mutation, or selective exposure is actually needed. This is not a new keyword
  track; it is a clause-density and progressive-disclosure track. Goal:
  reduce the need to spell `subject`/`authorized by`/projection clauses for
  every rich business object and avoid turning domain modeling into a compiler
  puzzle.
- Follow-up modeling guard: `docs/121_types_as_domain_medium.md`,
  `docs/19_design_philosophy.md`, zone-shape diagnostics, semantic regression,
  and `documentation-quality-test-smoke` now agree that `subject` is an
  identity-bearing state-transition host, not "important information";
  `authority` is boundary/mutation permission, not an importance rank; and
  passive business graph state should remain `struct`/`object`/`vessel` until a
  transition, handoff, authority, or projection contract is actually needed.
  Local gates: `make test-semantic` (`2366/0`) and
  `make documentation-quality-test-smoke`.
- Follow-up CI hardening: `cfg-body-dataflow-test-smoke` no longer hard-fails
  solely because Python is absent. When Python is available it still runs the
  full source/document contract audit; otherwise it falls back to a shell
  literal contract check and still runs the compiler HIR/RIR/MIR smoke. Local
  gates covered both paths.
- Follow-up runtime authority CI hardening:
  `runtime-authority-contract-test-smoke` now has the same shape. Python keeps
  the full raw-literal audit; no-Python runners get a shell literal contract
  fallback that still verifies shared authority code/reason/stderr macros,
  runtime include usage, raw literal bans, and LLVM authority token exports.
- Follow-up runtime panic CI hardening:
  `runtime-panic-contract-test-smoke` now also keeps its Python structural audit
  when available, but no-Python runners execute a shell literal fallback that
  verifies the shared panic class/reason surface, panic emitter ownership,
  released-slot/secure-token/unwrap/checked-arithmetic lowering hooks, and the
  core docs contract. `runtime-panic-abi-test-smoke` intentionally remains a
  Python-required executable ABI test because it validates subprocess exit code
  and stderr panic class behavior.
- Follow-up AIR CI hardening: `air-drift-test-smoke` now keeps the full Python
  source/document/test audit when available, but no-Python runners execute a
  shell literal fallback for the core AIR contract: verification-only
  architecture, strict evidence, `AIREvidenceNode`, MIR pin cleanup evidence,
  DAG generic evidence, driver synthesis hook, diagnostic docs, backend
  non-impact policy, and AIR proof obligations. Local gates covered both paths.
- Follow-up `.inc`/owner-size recheck: source production `.inc` inventory is
  still `0`. Local gates green:
  `semantic-inc-size-test-smoke`, `semantic-tu-size-test-smoke`,
  `production-header-size-test-smoke`, `backend-inc-size-test-smoke`,
  `test-inc-size-test-smoke`, and `inc-sentinel-test-smoke`.
- Follow-up DAG naming cleanup: top-level program placeholder signatures now
  consume `program_lookup_dag_type_annotation_or_unknown(...)` instead of a
  `program_resolve_*` local seam. This is still metadata-only behavior, but the
  owner name now matches the source-of-truth contract and
  `type-resolution-resolver-inventory-test-smoke` blocks resolver-style naming
  or local materialization from retuning to `type_checker_program.c`.
- Follow-up AIR evidence tightening: `air_validate_evidence_inventory(...)` now
  rejects duplicate evidence nodes with the same kind, boundary, provider, and
  subject. AIR evidence is still an inventory, not a multiset; duplicate facts
  must be represented by `fact_count`, not repeated nodes.
- Follow-up AIR evidence producer closure: `air_append_evidence_node_ex(...)`
  now merges duplicate evidence keys into the existing node by incrementing
  `fact_count` / `fallback_count`. The validator still rejects manually-created
  duplicate nodes, but normal synthesis no longer fails real fixtures by
  appending repeated identical evidence. Gate: `make test-air` (`74/0`) and
  `air-drift-test-smoke`.
- Follow-up AIR evidence read closure: `air_boundary_has_evidence(...)` now
  ignores legacy boundary summary booleans when real HIR/RIR/MIR input is
  attached and no evidence inventory exists. Compatibility fixtures without
  real input may still use legacy flags. Gate: `make test-air` (`74/0`) and
  `air-drift-test-smoke`.
- Follow-up CFG/MIR guard: the non-CFG statement population helper now rejects
  CFG-backed HIR routines if it is called accidentally. This keeps non-CFG
  source statement fallback from re-entering the body-safety source-of-truth
  path, and `cfg-body-dataflow-test-smoke` gates the invariant.
- Follow-up MIR intent fact tightening: `mir_validate(...)` now rejects drifted
  intent MIR instruction facts (`IntentStep`, step metadata payloads, and
  `IntentCheck`/`IntentEval` phase facts). Step-scoped facts must also carry the
  MIR step-link fact. C/LLVM may still carry AST expression payloads for
  emission, but step names, step links, metadata payload identity, and check/eval
  phase classification are validator-owned MIR facts. Gate: `make test-mir`
  (`41/0`) plus `cfg-body-dataflow-test-smoke`.
- Follow-up C/LLVM MIR intent consumer tightening: C and LLVM step-name readers
  now consume the validator-owned `IntentStep.arg0` fact directly instead of
  falling back to `inst->name` (`"IntentStep"`). The CFG/body smoke blocks that
  fallback from retuning.
- Follow-up MIR intent fact API split: intent MIR instruction matching now lives
  in `src/compiler/mir_intent_fact.c` (`mir_instruction_is_intent_stmt(...)`,
  `mir_instruction_intent_step_matches(...)`,
  `mir_instruction_intent_phase_matches(...)`, and
  `mir_instruction_intent_payload(...)`, plus
  `mir_instruction_intent_step_name(...)`). C/LLVM consumers share the same
  step-link, phase, and payload semantics, and `mir.c` stays below the 600 LOC
  owner gate. `IntentCheck` / `IntentEval` phase classification and stable
  metadata payload reads are no longer backend-local `inst->arg0` string
  matching/reads.

## UTF-8 Progress Note - 2026-04-30 - AIR Payload Containment

- AIR final scope is now explicit: AIR is the 1.0 abstraction-safety closure
  layer, not the owner of the whole language. Beta keeps AIR Phase 1 narrow
  (`IntentNode`, `BoundaryNode`, strict evidence, drift facts). 1.0 requires
  first-class `EvidenceNode`s that audit HIR CFG, DIR, RIR, MIR cleanup/pin, and
  DAG generic/ability/module facts without letting AIR become a codegen IR or a
  replacement for CFG/DAG/ownership/runtime propagation.
- AIR now has first-class `AIREvidenceNode` inventory in addition to legacy
  per-boundary compatibility flags. HIR routine, HIR CFG, RIR boundary, and RIR
  authority evidence are recorded as provenance-carrying nodes and validated as
  AIR inventory. This is the first code-level step toward the AIR 1.0
  `EvidenceNode` contract while keeping existing driver diagnostics stable.
- AIR evidence inventory also carries the stable observability/trace schema as
  global evidence. `pgy.intent.observability.v1` is no longer only a JSON dump
  literal; it is validated as an evidence node sourced from the runtime schema.
- AIR strict evidence now treats `AIREvidenceNode` as authoritative whenever an
  evidence inventory is present. Legacy per-boundary booleans remain as cached
  summaries for dumps and compatibility fixtures, but they can no longer satisfy
  strict HIR/RIR/MIR evidence by themselves once inventory nodes exist.
- AIR consumer migration step: `air_boundary_has_evidence(...)` is now the
  public boundary evidence query. Driver diagnostics and AIR dumps use it, so
  user-facing AIR output consumes evidence inventory before legacy cached flags.
- AIR now has the first MIR evidence seam: `air_collect_mir_evidence(...)`
  records `pin-unpin-cleanup-edge` as `AIR_EVIDENCE_MIR_PIN_CLEANUP` for the
  matching AIR `pin` execution boundary. AIR still does not create or validate
  MIR cleanup facts; MIR remains the owner and AIR only audits provenance.
- AIR boundary walking and HIR containment now descend through payload carriers
  that can hide already-stable boundary kinds: event subscribe/unsubscribe
  handler payloads, party-instance assignment values, party shared-field
  initializers, world roster/zone initializers, and domain-slot initializers.
  These nodes are not new AIR boundary kinds; they only forward the walker to
  nested IO/parallel/channel/execution boundaries.
- `src/test_air.c` now includes an event-handler payload regression where
  `ReadFile(...)` is nested under an `AST_EVENT_SUBSCRIBE` handler. AIR must
  synthesize the IO boundary at the nested call AST. `AST_EVENT_SUBSCRIBE` and
  `AST_EVENT_UNSUBSCRIBE` are also classified as AIR execution boundaries, so
  event subscription is no longer invisible to the abstraction-safety layer.
  Local gate: `make test-air air-drift-test-smoke` (`51/0` AIR tests).
- AIR evidence provenance is now non-empty by invariant. HIR routine evidence,
  RIR boundary evidence, and RIR authority evidence flags with empty provenance
  names are rejected as `PGY_AIR_INVARIANT_INVALID`. Local gate: `make
  test-air air-drift-test-smoke` (`51/0` AIR tests).

## UTF-8 Progress Note - 2026-04-29 - Runtime Frontier LLVM Owner And Parallel MIR Preservation

- CFG/MIR cleanup validation is tightened: reachable non-cleanup blocks with a
  cleanup successor must now carry a materialized `cleanup-edge` MIR fact, and
  rollback/invalidation cleanup blocks must carry their named cleanup-edge
  facts. This prevents backend consumers from silently relying on topology
  fields without the explicit MIR cleanup fact inventory. `make test-mir` is
  green.
- MIR regression now corrupts rollback and invalidation cleanup fact names and
  requires `mir_validate(...)` to reject both cases. Cleanup topology alone is
  no longer enough for beta body-safety evidence; the named MIR fact inventory
  must stay intact.
- CFG loop fixed-point equality now compares resource `used_states` as well as
  consumed/released state. This closes a narrow body-safety drift where loop
  convergence could ignore borrow/use facts while still merging ownership
  facts. `cfg-body-dataflow-test-smoke` now gates the comparison directly.
- Large-owner cleanup continued after `.inc` closure:
  `transpiler_mir_ssa_emit.h` now delegates SSA lookup helpers to
  `transpiler_mir_ssa_lookup.h`, and runtime Set raw exports now live in
  `pgy_runtime_lib_set_raw_exports.h` with compiler runtime-cache dependency
  tracking. `llvm_backend_type_map.c` now delegates early forward-declaration
  eligibility checks to `llvm_backend_forward_declare.h`, and
  `llvm_expr_assignment_member_projection.h` now delegates read-side member
  access emission to `llvm_expr_member_access.h`. Runtime file-path resolution
  now lives in `pgy_runtime_lib_file_path_core.h`, with runtime-cache
  dependency tracking. `pgy_abi_spec.h` now keeps ABI layouts while
  `pgy_abi_spec_asserts.h` owns compile-time layout assertions. These former
  owners are now below the 600 LOC split-review line. `slot_security.c` now
  delegates crypto/token encryption helpers to `slot_security_crypto_ops.h` and
  context/statistics helpers to `slot_security_context_ops.h`. `world_roster.c`
  now delegates execution-plan/statistics/visualization/free helpers to
  `world_roster_plan_stats.h`. `party_runtime.c` now delegates parallel
  dispatch/thread coordination to `party_runtime_dispatch.h`. `pgy_lsp.c` is
  now dispatch-only; protocol helpers, document navigation handlers, hover
  lookup, and diagnostic publication live in separate `src/lsp/` translation
  units. `ast.h` now delegates domain-heavy AST payload shapes to
  `ast_domain_data.h` as named structs, not `.inc`-style field fragments. The
  current non-test production scan has 0 `.c/.h` owners above the 600 LOC
  split-review line, and `production_header_size_smoke.sh` now uses 600 LOC as
  its default cap for compiler/runtime/codegen/semantic/parser/LSP headers.
- AIR strict evidence now records HIR input presence. When HIR input exists,
  each AIR boundary must have matching HIR routine provenance; RIR-only
  boundary evidence is no longer enough to make a boundary look complete.
  `air_dump()` also prints `hir_input=yes/no`, and `make test-air` is green.
- LLVM declaration inventory helper ownership is now split without changing the
  public include seam: `llvm_inventory_intenal.h` is a 185 LOC facade/domain
  inventory owner, `llvm_inventory_decl_lookup.h` owns MIR header-first
  declaration lookup at 257 LOC, and `llvm_inventory_host_methods.h` owns host
  method metadata accessors at 209 LOC. `make pgy` and
  `make mir-declaration-inventory-test-smoke` are green.
- Runtime slot manager lifecycle ownership is now below the split-review line:
  `slot_manager.c` is 564 LOC, while `slot_manager_query_lock.c` owns query,
  TTL cleanup, locking, stats, and fast wrappers at 240 LOC. `make
  test-security` and `make test-abi` are green.
- Lexer debug ownership is now split: `lexer.c` is 573 LOC and
  `lexer_token_debug.c` owns token stringification/debug printing at 127 LOC.
  `make test-parser` and `make test-semantic` are green.
- Parallel runtime ownership is now split without changing the public runtime
  include: `pgy_parallel.h` is a 494 LOC shared task/await facade,
  `pgy_parallel_blocking.h` owns the blocking pool at 146 LOC, and
  `pgy_parallel_coroutine.h` owns coroutine scheduling at 292 LOC. `make pgy`
  and `make test-abi` are green.
- Intent parser ownership is now split without changing parser exports:
  `parser_intent.c` is a 468 LOC declaration/default propagation owner and
  `parser_intent_step.h` owns step clause parsing at 297 LOC. `make
  test-parser` and `make test-semantic` are green.
- Expression parser string ownership is now split: `parser_expr.c` is a 524 LOC
  precedence/call/primary owner and `parser_expr_string.h` owns
  multiline/interpolation helpers at 150 LOC. `make test-parser` and
  `make test-semantic` are green.
- Slot pool performance ownership is now split: `slot_pool.c` stays focused on
  pool/list allocation below the 600 LOC split-review threshold, while
  `slot_pool_perf.c` owns timestamp, cache prefetch/alignment, and benchmark
  helpers. `make test-datastructures`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.
- RIR builder ownership is now split: `rir_builder.c` is a 281 LOC
  scope-orchestration owner, while `rir_builder_walk.c` owns AST body walking,
  call/resource op materialization, and block-condition walking at 363 LOC.
  `make test-rir`, `make test-air`, `make test-mir`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.
- Runtime LLVM export ownership is split without changing the public runtime
  include seam: `pgy_runtime_lib_slot_array_io_string_exports.h` is now an
  8 LOC facade over secure-slot, device-slot, array/map, and IO/string owners
  at 161/84/239/296 LOC. The runtime object cache freshness list also tracks
  the new leaf owners, so prebuilt LLVM runtime objects cannot stay stale after
  a leaf export edit. `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.
- Runtime channel/qubit export ownership is split without changing the public
  runtime include seam: `pgy_runtime_lib_channel_quantum_exports.h` is now a
  7 LOC facade over channel-int, channel-string, and qubit-state owners at
  327/319/69 LOC. The runtime object cache freshness list tracks those leaf
  owners too, so channel/qubit export edits invalidate cached LLVM runtime
  objects correctly. `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.
- Runtime raw collection export ownership is split without changing the public
  runtime include seam: `pgy_runtime_lib_raw_collection_exports.h` is now an
  8 LOC facade over common helper, raw Queue, raw HashMap, and raw Set owners
  at 13/117/431/153 LOC. The runtime object cache freshness list tracks those
  leaf owners too. `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.
- Type-resolution DAG retired-resolver naming debt is closed: the obsolete
  `type_checker_resolve.c` owner is gone. Retired compatibility counters now
  live in `type_checker_resolution_retired.c`, while assignability and
  constructed-type helpers live in `type_checker_type_helpers.c`. The resolver
  inventory smoke now rejects reintroducing `type_checker_resolve.c` or
  `type_checker_resolve.h`, and `semantic-core-shape` requires the new split
  owners. Local gates: `make type-resolution-resolver-inventory-test-smoke
  type-resolution-dag-test-smoke test-semantic semantic-core-shape-test-smoke
  semantic-tu-size-test-smoke` are green.
- LLVM domain method forward declarations are metadata-first for hosted domain
  methods. `llvm_domain_forward.c` now resolves method name, params, and retun
  type through `MIRDeclMethod` accessors before falling back to the temporary AST
  payload, and `mir-declaration-inventory-test-smoke` rejects direct AST
  param-count/retun-type reads in that forward-declaration section.
- C MIR residual statement emission now keeps executable parallel-family
  statements even when the same AST also carries a `MIR_INST_RESOURCE_OP`
  observability hook. The hook is not allowed to replace task/channel runtime
  lowering. This fixes the C backend hang in `parallel_channel_sum`, where the
  channel receives were emitted but the parallel send body was dropped.
- CFG/MIR pin cleanup has an early-retun regression now:
  `src/test_mir.c` checks that a pin-region block with a `retun` terminator
  still carries the matching `pin-unpin-cleanup-edge` fact, and
  `cfg-body-dataflow-test-smoke` requires that fixture. This tightens the
  `ReleaseAfterUnpin(slot, all_cfg_exits)` bridge beyond normal fallthrough
  pin blocks.
- CFG/MIR pin cleanup also has branch-retun coverage now:
  `PinBranchRetuns` verifies that terminating `if`/`else` arms inside a pin
  region keep cleanup successor routing plus the `pin-unpin-cleanup-edge` fact.
  This closes a concrete all-exit cleanup regression class before the broader
  branch/join ownership lattice work.
- CFG/MIR pin cleanup also covers loop-control exits now: `PinLoopControl`
  verifies that `break` and `continue` lowered as `HIR_BLOCK_GOTO` inside a pin
  region still carry cleanup successor routing plus the `pin-unpin-cleanup-edge`
  fact. This tightens the all-exit cleanup bridge for loop exits without
  claiming full loop ownership/lifetime lattice closure.
- Backend compare now wraps generated executable runs with
  `PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS` (default 30s) when `timeout` is
  available, so parity regressions fail as test failures instead of wedging CI.
  Local gate: `make llvm-test-backend-compare` is green again (`196/0` ABI
  same-process, `65/65` backend compare).

- LLVM world sync no longer owns the whole transitive frontier scheduler body:
  `src/codegen/llvm_domain_world_frontier.c` now owns the bounded world
  frontier loop, derived-state recompute loop, zone-generation dirty detection,
  and overflow abort blocks. `src/codegen/llvm_domain_world_sync.c` is reduced
  to sync orchestration at 164 LOC, while the new frontier owner is 470 LOC.
  `runtime-frontier-contract-test-smoke` now gates the split owner directly.
- A real CFG/MIR drift bug was fixed: `parallel { ... }` was incorrectly listed
  as a CFG-owned control container even though HIR/MIR does not yet lower
  parallel into explicit CFG edges. MIR DCE therefore removed the parallel
  channel send before `select_ready`. `parallel` is now preserved as a
  side-effecting MIR statement until a future true CFG lowering exists.
- `make cfg-body-dataflow-test-smoke` now enforces that split explicitly:
  `AST_PARALLEL_BLOCK` must not appear in the CFG-owned control classifier,
  must remain in the MIR DCE side-effect set, and a parallel-send/select
  fixture must retain the parallel statement in MIR. This keeps the contract
  honest: AIR can observe `parallel`, but CFG does not yet own its execution
  edges.
- AIR inspection is now first-class: `pgy --air <source.pgy>` dumps the AIR
  verification summary after HIR/DIR/RIR evidence collection and before driver
  drift failure. AIR remains verification-only and is still absent from
  `CompilerIRBundle`, but evidence/drift state is now directly inspectable.
- AIR now classifies `AST_TASK_GROUP` as a `parallel` boundary source named
  `task-group`, and strict evidence requires matching same-AST RIR operation
  evidence for every beta-stable parallel boundary. RIR now materializes
  `AwaitRemote`, `Spawn`, `Async`, `Parallel`, and `TaskGroup` ops, so
  `task-group` is no longer a HIR-only exception.
- World handoff evidence is now same-AST specific for parsed/source-backed
  boundaries: a same-alias RIR `Move` / `Claim` in the same scope is not enough
  unless it points at the same intent-step AST. This closes a source-provenance
  false-positive in AIR strict evidence.
- RIR now has explicit channel boundary ops: `ChannelSend`, `ChannelRecv`, and
  `ChannelSelect`. AIR channel evidence requires the matching same-AST op when
  source provenance exists, so channel strict evidence no longer passes through
  a generic RIR scope alone. `make test-rir` now gates parsed-source `ch <-`,
  `<- ch`, and `select` lowering into those ops, not just manually assembled
  RIROp evidence.
- RIR now has explicit IO boundary evidence for the beta-stable IO builtin set:
  `FileOpen`, `FileRead`, `FileWrite`, `FileClose`, `ReadFile`, `WriteFile`,
  `Input`, `ReadLine`, `Now`, and `Sleep` lower to `RIR_OP_IO`. AIR accepts IO
  strict evidence only from a matching source/provenance op; the parsed
  `ReadFile` AIR test is now positive exact-evidence coverage instead of a
  deliberate missing-evidence negative.
- Parser call source spans were tightened for builtin calls and `AST_CALL`
  nodes. AIR no longer needs to treat parsed `ReadFile(...)` as a step-level
  fallback in the common path; containment matching remains only as a defensive
  fallback for older/no-span AST producers.
- AIR HIR CFG evidence is now containment-aware: nested boundary ASTs inside a
  CFG-carried statement or terminator value satisfy HIR evidence only when the
  enclosing CFG statement actually contains the boundary. This closes the
  `with { ReadFile(...) }` execution-boundary seam without accepting
  routine-name-only evidence. The matcher now follows the same core executable
  and expression forms as the AIR boundary walker, including loop conditions,
  parallel/async/task-group bodies, spawn/call/assignment subexpressions,
  arrays/tuples, await/channel/select, match, unsafe/defer, event invoke, and
  lambda body.
- AIR boundary walking and HIR containment now also descend into `let`
  declaration and destructuring initializers. A boundary hidden behind
  `let content = ReadFile(...)` inside an intent-step block is now synthesized
  as an AIR IO boundary instead of being treated as ordinary local syntax.
- Local gates: `make pgy`, `make llvm-test-smoke`,
  `make cfg-body-dataflow-test-smoke`, `make runtime-frontier-contract-test-smoke`,
  `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`, and `make air-drift-test-smoke`.

## UTF-8 Progress Note - 2026-04-29 - HIR Intent CFG Evidence

- Intent routines now get a minimal HIR CFG materializer instead of relying on
  routine-only summaries for AIR boundary evidence.
- `src/compiler/hir_lower_cfg.c` remains the function-body CFG owner at 598
  LOC, while `src/compiler/hir_lower_intent_cfg.c` owns ordered intent-step
  clause CFG materialization at 184 LOC. This keeps the CFG lowerer family
  under the 600 LOC split-review threshold without adding `.inc` files.
- The intent CFG is deliberately an ordered clause/inventory CFG, not a full
  runtime propagation scheduler. It gives AIR strict evidence a concrete HIR
  CFG block containing the same step/clause AST boundary, so parsed-source
  intent boundaries can no longer pass on routine provenance alone.
- MIR population now preserves intent `MIR_INST_STMT` semantic carriers after
  CFG statement reconstruction. Intent participant/zone/authority/causes facts
  stay MIR inventory even when the source intent routine has HIR CFG.
- Local gates: `make test-hir`, `make test-air`,
  `make cfg-body-dataflow-test-smoke`, and `make air-drift-test-smoke`.

## UTF-8 Progress Note - 2026-04-29 - DAG Compatibility Inventory Tightening

- Type-resolution DAG fallback remains closed: `materializer_fallbacks=0`,
  alias/non-alias stage metadata materialization is 0, and direct semantic owner calls
  into `resolve_type_node(...)` stay smoke-gated.
- `compatibility-resolver` calls now report AST-kind inventory:
  `ast_type`, `channel`, `future`, `event_handler`, and `other`. Current
  semantic-suite max inventory is `0` compatibility calls: public semantic
  regression helpers now use `semantic_type_resolution_lookup_type_ref_or_materialize(...)`
  instead of entering the compatibility resolver directly.
- `tests/type_resolution_dag_smoke.sh` now gates that accounting and also
  requires compatibility resolver body fallbacks (`cache misses`) to stay `0`.
  The compatibility API call cap is tightened from 1000 to 0 because the
  global counter is now read as a max/last inventory value instead of summing
  repeated per-context stats lines.
  This makes the remaining DAG debt precise: the compatibility resolver body is
  removed, and the frozen semantic DAG smoke keeps the retired surface at
  `0` calls.
- `resolve_type_node(...)` is no longer exposed from the public
  `type_checker.h` surface. It remains declaration-only in the private
  resolver compatibility header, and
  `type-resolution-resolver-inventory-test-smoke` now rejects both public header
  re-exposure and semantic regression tests that call the compatibility resolver
  directly.
- The private compatibility evaluator body is removed. Only the compatibility audit
  counters remain, so `PGY_TYPE_RES_STATS=1` can continue reporting
  `retired_resolver_calls=0` and resolver body fallbacks at `0`. The resolver inventory
  smoke rejects reintroduced `resolve_type_node` evaluator bodies.

## UTF-8 Progress Note - 2026-04-29 - C Let Slot Owner Split

- C backend let-declaration lowering no longer carries Slot/DeviceSlot,
  ReadView/WriteView/MoveToken, and Slot/SecureSlot sugar logic inside the
  mixed `emit_let_decl(...)` owner. Those paths now live in
  `src/codegen/transpiler_let_slot_emit.h`.
- `src/codegen/transpiler_let_emit.h` is now 505 LOC and
  `src/codegen/transpiler_let_slot_emit.h` is 297 LOC, so the let-declaration
  owner family is below the 600 LOC split-review threshold without adding
  `.inc` files.
- Local gates: `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke` are green.
- C domain role/ability ownership is also split away from propagation
  provenance. `transpiler_domain_provenance_emit.h` now owns hidden
  epoch/cause field emission and projection-chain bounded recompute, while
  `transpiler_domain_role_ability_emit.h` stays focused on role/ability
  lowering. Current sizes are 237 LOC and 452 LOC respectively.
- Parity gate after both C owner splits: `make llvm-test-backend-compare`
  remains green (`196/0` ABI same-process, `65/65` backend compare).
- C function/class/flow ownership is now below the 600 LOC split-review
  threshold. `transpiler_class_decl_emit.h` owns non-generic class declaration
  lowering, while `transpiler_func_class_flow_emit.h` keeps function fallback,
  generic class specialization, with-slot, and retun lowering. Current sizes
  are 138 LOC and 594 LOC respectively. Local gates: `make pgy`,
  `make test-transpile`, `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke` are green. Parity gate:
  `make llvm-test-backend-compare` remains green (`196/0` ABI same-process,
  `65/65` backend compare).
- C MIR block ownership is also back below the split threshold:
  `transpiler_mir_emit_predicates.h` owns the small MIR emission predicate
  wrappers, leaving `transpiler_mir_block_emit.h` focused on block statement
  emission at 589 LOC. Local gates: `make test-mir`,
  `make cfg-body-dataflow-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke` are green.
- C declaration lookup ownership is now split by concen:
  `transpiler_decl_lookup.c` keeps named declaration, alias, inventory, and
  method-list lookup at 419 LOC, while `transpiler_decl_host_lookup.c` owns
  current-host, owner-host, nominal-host, and nominal-method lookup at 216 LOC.
  Local gates: `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  are green (`196/0` ABI same-process, `65/65` backend compare).
- C type mapping ownership is now split by concen:
  `transpiler_type_mapping_helpers.h` keeps primitive/collection/slot/result
  mapping and suffix helpers at 563 LOC, while
  `transpiler_type_render_helpers.h` owns AST type-name rendering at 102 LOC.
  Local gates: `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  are green (`196/0` ABI same-process, `65/65` backend compare).
- CFG contract validation ownership is now below the split threshold:
  `mir_cfg_contract_validate.h` keeps CFG cleanup, successor, and predecessor
  validation at 551 LOC. `mir_cfg_contract_pin.h` owns pin cleanup edge
  validation at 39 LOC, while
  `mir_cfg_contract_control.h` owns CFG-owned AST control classification at
  32 LOC. Local gates: `make test-mir`, `make cfg-body-dataflow-test-smoke`,
  `make abi-ownership-shape-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke` are green.
- MIR SSA/local type ownership is now split by concen:
  `transpiler_mir_ssa_names.h` keeps SSA name resolution, SSA map setup,
  claim-shape predicates, and implicit-field rendering at 357 LOC, while
  `transpiler_mir_local_type_lookup.h` owns AST body local type lookup and
  expression fallback inference at 293 LOC. Local gates: `make pgy`,
  `make test-mir`, `make cfg-body-dataflow-test-smoke`,
  `make test-transpile`, `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  are green (`196/0` ABI same-process, `65/65` backend compare).

## UTF-8 Progress Note - 2026-04-29 - Runtime Frontier Policy And C Owner Split

- 2026-04-30 follow-up: LLVM frontier overflow emission now has one owner.
  World derived overflow, world transitive-frontier overflow, zone overflow,
  and projection-chain overflow all route through
  `llvm_emit_frontier_overflow_abort(...)` instead of open-coded abort blocks.
  `runtime-frontier-contract-test-smoke` includes the shared helper in both
  world/zone and projection contract bundles so future LLVM emitter drift is
  caught at the helper seam, not by duplicated backend-local snippets.
- Stable world outer frontier scheduling now consumes
  `pgy_frontier_world_transitive_pass_limit(...)` in both the C emitter and the
  LLVM world sync emitter. The helper currently delegates to the existing
  monotone `zone_count + state_count + 1` world pass bound, but the contract
  name makes the world zone-sync plus derived-state recompute family a shared
  source-of-truth policy instead of a backend-local constant.
- `make runtime-frontier-contract-test-smoke` now gates the transitive frontier
  policy helper in `src/runtime/pgy_frontier_policy.h`, the dedicated
  `make runtime-frontier-policy-test-smoke` arithmetic check, and both C/LLVM
  world emitters, while keeping the broader world-zone propagation family open
  as the remaining runtime propagation blocker.
- C/LLVM world frontier emitters now carry a separate derived-state
  changed-any fact. A bounded derived recompute that converges after changing a
  world state still causes the outer transitive frontier to run one more pass
  before dirty flags are cleared. This closes the prior dirty-flag-only seam.
- Frontier pass-limit formulas now saturate through the same u32-bounded
  source-of-truth helpers (`pgy_frontier_pass_limit_add*`) before C/LLVM
  emission. This keeps the C `size_t` loops and LLVM i32 loop counters from
  drifting on oversized generated frontier families.
- C backend world/select/event emission is split by owner:
  `transpiler_world_select_event_emit.h` now owns world emission only,
  `transpiler_select_emit.h` owns `select`, and `transpiler_event_emit.h` owns
  event declarations/subscription lowering. Current sizes are 370, 155, and
  103 LOC respectively, keeping this family below the 600 LOC split-review
  threshold without adding `.inc` files.
- Local gates: `make pgy`, `make runtime-frontier-contract-test-smoke`,
  `make runtime-frontier-policy-test-smoke`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`
  are green.

## UTF-8 Progress Note - 2026-04-29 - AIR Evidence Diagnostic Consumer

- AIR evidence requirements are now public AIR policy queries:
  `air_boundary_requires_hir_evidence(...)` and
  `air_boundary_requires_rir_evidence(...)`.
- The driver AIR drift diagnostic consumes those AIR facts and distinguishes
  missing HIR CFG evidence, missing RIR boundary evidence, and missing authority
  evidence. This removes stale RIR-only wording after strict evidence started
  requiring HIR CFG evidence for implementation boundaries.
- AIR HIR evidence now separates routine provenance from CFG provenance:
  `has_hir_routine_evidence` and `has_hir_cfg_evidence` are distinct flags, and
  `hir_cfg_evidence_count` increments only when the matching HIR routine carries
  generated CFG for the same boundary AST when one is available. This keeps
  routine-only intent summaries from masquerading as body CFG proof.
- HIR evidence matching is now source-specific. A `HIR_TOPLEVEL_INTENT` routine
  no longer satisfies every AIR boundary by kind alone; it must match the intent
  owner, step, or boundary source name. `test_air` includes a negative case that
  rejects unmatched top-level intent HIR evidence while accepting matching RIR
  boundary evidence.
- `make air-drift-test-smoke` gates the public policy queries and the
  HIR/RIR-specific diagnostic wording. `make test-air` is green (`28/0`).

## UTF-8 Progress Note - 2026-04-29 - DAG Type-Ref Shortcut Tightening

- `semantic_type_resolution_lookup_metadata_type_ref(...)` now materializes
  stable constructed type refs without a resolver compatibility body.
  This moves constructed stable shells reached through the type-ref API onto the
  DAG metadata path, not the recursive resolver path.
- `semantic_type_resolution_lookup_type_ref_or_materialize(...)` is now the
  named metadata-first API for semantic owners that still need diagnostic
  materialization on unresolved refs.
- `semantic_stage_resolve_type_quiet(...)` now asks the metadata type-ref API
  before recording a compatibility stage fallback. This keeps signature-stage
  compatibility callers metadata-first even when they are not fully graph-skipped.
- Intent participant/value/where type refs and zone authority subject-slot type
  refs now use the same metadata-first helper before the diagnostic materializer
  path. Ability where refs, class/func signature refs, action contract refs,
  domain slot refs, world slot refs, expression/member/operator annotation refs,
  generic defaults/contract refs, async channel parameter refs, ownership refs,
  and projection path refs also consume this helper. This narrows the first
  semantic-owner seam without moving the whole domain AST lowering rewrite into
  beta.
- `type-resolution-resolver-inventory-test-smoke` now fails if a semantic owner
  bypasses the metadata-first helper and calls the diagnostic materializer
  directly. Only central metadata/diagnostic compatibility owners are allowed to
  call `semantic_type_resolution_lookup_or_materialize(...)`.
- Signature-stage quiet resolution no longer keeps a direct diagnostic
  materializer call. After metadata preflight misses it routes through
  `semantic_type_resolution_lookup_type_ref_or_materialize(...)`, and the
  resolver-inventory smoke removed
  `type_checker_resolution_stage_signature.c` from the direct-materializer
  allowlist.
- Stable constructed-type diagnostic argument resolution also uses the
  metadata-first type-ref helper. The only remaining direct
  `semantic_type_resolution_lookup_or_materialize(ctx, ...)` call is the
  central metadata type-ref helper's fallback branch in
  `type_checker_resolution_metadata.c`.
- The direct materializer smoke allowlist is now narrowed to that central
  metadata owner only; `type_checker_resolve.c` remains counter-only and is no
  longer allowlisted for direct materializer calls.
- DAG gates remain green: `type-resolution-dag-test-smoke` reports
  `materializer_fallbacks=0`, `stage_materialize_alias=0`, and `stage_materialize_non_alias=0`.

## UTF-8 Progress Note - 2026-04-28 - LLVM Intent/Domain Owner Split

- LLVM intent declaration ownership is below the 600 LOC split-review
  threshold. `llvm_intent.c` now owns orchestration only, while
  `llvm_intent_setup.c` owns entry/participant binding,
  `llvm_intent_step_context.c` owns MIR/AST step carrier context validation,
  and `llvm_intent_cleanup.c` owns cleanup/rollback/invalidation tail
  emission. The MIR-only carrier diagnostic path remains explicit instead of
  falling back to AST helper inventory.
- LLVM domain declaration ownership is also below the threshold.
  `llvm_domain.c` now owns domain struct registration orchestration,
  `llvm_domain_forward.c` owns sync/method forward declarations plus ability
  vtable and role forward registration, and `llvm_domain_struct_fields.c`
  owns effect-pool and projection-state field helpers.
- Local gate: `make llvm-test-smoke` is green after both splits. Current owner
  sizes: `llvm_intent.c` 555 LOC, `llvm_domain.c` 568 LOC,
  `llvm_domain_forward.c` 306 LOC, and `llvm_domain_struct_fields.c` 80 LOC.
- This closes the immediate LLVM intent/domain review-band slice. Remaining
  backend debt is now concentrated in declaration inventory/bootstrap seams,
  projection overlay/C emitter owners, and C/LLVM parity edge coverage.
- C backend orchestration ownership is also back below the threshold:
  `transpiler.c` now stays at 587 LOC after moving public entry/result
  lifecycle to `transpiler_entry.c`, runtime thread-pool requirement scanning
  to `transpiler_thread_pool.c`, and small include/impl-ability declaration
  emitters to `transpiler_misc_decl.c`.
- Backend parity gate after the C split is green: `make llvm-test-backend-compare`
  reports ABI same-process `196 passed, 0 failed` and backend compare
  `64/64 passed, 0 failed`.
- Projection overlay ownership is also below the threshold:
  `transpiler_overlay_projection.h` now stays at 533 LOC after moving
  host-field/self-cell probes to `transpiler_overlay_host_fields.h` and
  zone relation/effect bind-layer emission to `transpiler_overlay_zone_bind.h`.
- Backend parity gate after the projection overlay split is still green:
  `make llvm-test-backend-compare` reports ABI same-process `196 passed,
  0 failed` and backend compare `64/64 passed, 0 failed`.
- LLVM zone sync ownership is now below the threshold:
  `llvm_domain_zone_sync.c` stays at 510 LOC after moving relation clause
  lowering (`link` / maintained relation / `unlink`) to
  `llvm_domain_zone_sync_relations.c`.
- Local gates after the zone sync split are green: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare).
- LLVM world sync ownership is now below the threshold:
  `llvm_domain_world_sync.c` stays at 592 LOC after moving world command
  directive lowering plus state/zone-slot lookup helpers to
  `llvm_domain_world_sync_directives.c` behind
  `llvm_domain_world_sync_intenal.h`.
- Local gates after the world sync split are green: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare).
- LLVM runtime declaration ownership is now below the threshold:
  `llvm_runtime.c` stays at 533 LOC after moving raw collection export
  declarations to `llvm_runtime_raw_collections.c` and channel export
  declarations to `llvm_runtime_channels.c` behind `llvm_runtime_intenal.h`.
- Local gates after the runtime registry split are green: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare).
- LLVM expression boundary/projection helper ownership is now below the
  threshold: `llvm_expr_boundary_projection_helpers.h` stays at 470 LOC after
  moving projection nominal lookup, nested vessel path resolution,
  projection-path value loading, and `ProjectSubject` emission to
  `llvm_expr_projection_path_helpers.h`.
- Local gates after the expression helper split are green: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare). Projection-path
  helpers are shared by expression, host spawn literal, assignment projection,
  and domain projection sync emission.
- LLVM host/spawn literal helper ownership is now below the threshold:
  `llvm_expr_host_spawn_literal_helpers.h` stays at 345 LOC after moving
  async await-task result materialization, direct function-call argument
  emission, generic callee monomorphization, and spawn-expression wrapper
  lowering to `llvm_expr_spawn_call_helpers.h`.
- Local gates after the spawn/call split are green: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare).
- CFG body-dataflow consumer tightening: MIR statement population no longer
  preserves HIR-expanded control containers (`if`, `while`, `for`, `select`,
  `match`, `break`, `continue`) as fallback `MIR_INST_STMT` instructions when
  the block already has CFG successor edges. `for` preheader initialization is
  now a dedicated `MIR_INST_LOOP_INIT` fact consumed by C and LLVM. For-loop
  condition and backedge emission now consume the header `MIR_INST_BRANCH`
  metadata instead of re-reading `target->source_ast`. The loop variable and
  start/end expressions are carried on MIR instructions (`arg0`, `expr0`,
  `expr1`) and validated by `mir_validate()`. `mir_validate()` rejects
  CFG-owned control statements that reappear as fallback STMTs, preventing
  C/LLVM drift from emitting both MIR CFG edges and AST control flow. MIR DCE
  now consumes the same `mir_stmt_ast_is_cfg_owned_control(...)` classifier
  instead of keeping its own CFG-control AST switch, so statement population,
  validation, and DCE share one source of truth for control-container STMTs.
- Follow-up closure: CFG-owned `for value in List<T>` now uses the same MIR
  facts on both backends. C and LLVM emit a MIR-owned index slot, list-size
  condition, list-get body binding, and backedge increment instead of falling
  through AST loop lowering. `tests/cases/backend_compare/for_in_list_int`
  locks the C/LLVM parity path.
- Gates after this CFG slice are green: `make test-mir`,
  `make cfg-body-dataflow-test-smoke`, and
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `65/65` backend
  compare).
- CFG/AIR handoff tightening: `with`, `unsafe`, and `defer` are now included in
  the CFG-owned boundary set when a MIR block already has successor edges. MIR
  statement population skips these boundary containers in expanded CFG blocks,
  and `mir_validate()` rejects them if they reappear as fallback `MIR_INST_STMT`
  instructions. `parallel` remains AIR-visible and semantic-flow checked, but is
  deliberately not CFG-owned until HIR/MIR has a real parallel CFG lowering.
  MIR DCE now preserves `parallel` as a side-effecting STMT so channel sends and
  task effects cannot be erased before backend emission.
- AIR abstraction-boundary lift: AIR now has an explicit `execution` boundary
  kind for `with`, `unsafe`, `defer`, and pin-block AST metadata. These
  boundaries are sync execution boundaries and strict evidence requires HIR/CFG
  evidence instead of RIR resource-boundary evidence. The AIR walker now also
  descends into `with` bodies, so nested IO/time boundaries inside a `with`
  block are visible to AIR instead of being hidden behind the execution
  container. This closes the first CFG -> AIR handoff seam: AIR can now see
  body/execution boundary facts that CFG already owns, rather than treating
  them as ordinary AST containers.
- ABI ownership shape gate added: `make abi-ownership-shape-test-smoke` now
  ties the implemented Slot/Pin ABI shape, runtime pin generation/thread/token
  invariants, C/LLVM pin/unpin lowering, MIR cleanup evidence, backend compare
  pin fixtures, and Zone-Bound Handle docs contract into one shell-only gate.
- MIR declaration inventory smoke is shell-only now. It still blocks raw
  declaration/routine inventory access outside the helper owners, requires MIR
  method metadata accessors, and keeps `declaration-side MIR-only debt` visible
  without requiring Python on CI runners.
- LLVM hosted domain method body emission no longer uses AST/name-based MIR
  routine search. It must consume the linked `MIRDeclMethod.routine_index`
  metadata or fail with an explicit MIR inventory diagnostic; the declaration
  inventory smoke gates this so the helper-gated MIR path does not regress into
  routine discovery by AST compatibility payload.
- Runtime ABI lifetime smoke is shell-only now. It preserves the borrowed
  runtime string, result-owned string/array, runtime-owned file-handle, macro
  export, and ownership proof-doc checks without requiring Python on CI
  runners.

## UTF-8 Progress Note - 2026-04-28 - Semantic Owner Split

- `src/semantic/type_checker_builtins_stdlib_body.c` is now below the 600 LOC
  split-review threshold after moving `List` / `Set` / `Queue` / `Array`
  builtin typing to `src/semantic/type_checker_builtins_stdlib_collections.c`
  behind `type_check_stdlib_collection_call(...)`.
- Current stdlib builtin owner sizes: `type_checker_builtins_stdlib_body.c`
  510 LOC, `type_checker_builtins_stdlib_collections.c` 356 LOC,
  `type_checker_builtins_stdlib_scalar.c` and
  `type_checker_builtins_stdlib_map.c` remain existing focused owners.
- Local gates for this slice: `make test-semantic semantic-core-shape-test-smoke
  type-resolution-dag-test-smoke type-resolution-resolver-inventory-test-smoke`
  (`2359/0`, `materializer_fallbacks=0`, fallback seams=0).
- Builtin query implementation-header debt is now split into named owners:
  `type_checker_builtins_query.c` owns generic builtin arity, borrowed
  boundary store rejection, `HasProjection`, `HasLayer`, and `HasState`;
  `type_checker_builtins_query_world.c` owns `HasZone` and
  `HasZoneProjection` / `HasZoneLayer` / `HasZoneState`;
  `type_checker_builtins_query_channel.c` owns channel send/recv/close
  builtin typing; and `type_checker_builtins_query_domain.c` owns the shared
  domain lookup helpers. The `type_checker_builtins_query*.h` files are now
  declaration-only guards.
- Slot builtin debt is also split: `type_checker_builtins_slotops.c` owns
  slot lifecycle/view/move/device-slot builtins,
  `type_checker_builtins_secure_token.c` owns secure-token validation, and
  `type_checker_builtins_resolve.c` owns builtin name resolution. The old
  slotops implementation header is now declaration-only.
- Nominal builtin dispatch is now split as a real TU:
  `type_checker_builtins_nominal.c` owns the non-intent-observability builtin
  dispatcher path, `type_checker_builtins_intent_observability.c` owns the
  `IntentLast*` / `IntentHistory*` / `IntentActive*` / `IntentRecent*`
  observability family, and `type_checker_builtins_nominal.h` is
  declaration-only. Both implementation owners are under the 600 LOC
  split-review threshold.
- Slot analyzer summary debt is split: `slot_analyzer_summary.c` now owns
  access/function-alias/parameter summary behavior, while
  `slot_analyzer_escape.c` owns escape record/collect/mask behavior. Both are
  below the 600 LOC split-review threshold and the semantic shape gate tracks
  both owners.
- Function declaration implementation-header debt is closed:
  `type_checker_func_decl.c` owns function type/scope/body orchestration,
  `type_checker_func_action_contract.c` owns action-specific
  within/causes/authorized-by validation, and `type_checker_host_helpers.c`
  owns shared host/overlay/domain-slot helpers. The old
  `type_checker_program.h` and `type_checker_host_helpers.h` implementation
  bodies are gone, and all three owners are below the 600 LOC split-review
  threshold.
- `src/semantic/type_checker_expr.h` is now declaration-only; the expression
  dispatcher/member implementation moved to `type_checker_expr.c`.
- `src/semantic/type_checker_expr_call.c` now owns public call dispatch:
  builtin/stdlib calls, slot method sugar, static-member calls, hosted
  nominal method dispatch, and embedded-world-zone mutation rejection.
- `src/semantic/type_checker_expr_host.c` now owns host-field/method lookup
  and host-method call typing behind explicit `expr_*` seams. This avoids
  reusing the old implementation-header helper names and keeps
  `type_checker_intenal.h` declaration-only.
- `src/semantic/type_checker_resolve.c` no longer owns the retired recursive
  type-node compatibility body that used to rely on an implementation-header
  side effect. The obsolete `type_checker_resolve.h` compatibility header has
  been deleted, and DAG lookup/materialization callers use metadata-first APIs
  instead of linking against a compatibility resolver.
- `src/semantic/type_checker_resolution_helpers.c` now owns the
  metadata-first `resolve_named_type(...)`, alias lookup, symbol-kind labels,
  and embedded-world-zone mutation guard that used to live in
  `type_checker_resolution_helpers.h`. The header is declaration-only and
  `type-resolution-resolver-inventory-test-smoke` now rejects implementation
  bodies in both resolver helper headers.
- DAG named builtin/shell lookup no longer lives as a local string table inside
  `resolve_named_type(...)`. Stable scalar builtins and stable shell names now
  route through the metadata owner
  `semantic_type_resolution_metadata_named_builtin_or_shell_singleton(...)`,
  and `type-resolution-resolver-inventory-test-smoke` rejects reintroducing
  `strcmp(name, "...")` builtin/shell tables in the compatibility helper.
- The retired `resolve_type_node(...)` compatibility entry no longer has an
  evaluator body. Stable builtin/named/constructed refs stay metadata-owned
  through `semantic_type_resolution_lookup_metadata_type_ref(...)` and
  `semantic_type_resolution_lookup_type_ref_or_materialize(...)`.
- The metadata type-ref API records stable constructed types before retuning
  unresolved, and the signature-stage quiet resolver consumes that same API
  before fallback accounting. This keeps the recursive resolver compatibility
  seam absent without hard-crashing valid diagnostic paths.
- Intent/zone/domain/world/action/class/function local type-ref helpers now
  consume `semantic_type_resolution_lookup_type_ref_or_materialize(...)`, the
  named metadata-first API that falls back to diagnostic materialization only
  when metadata cannot answer. The same API now covers expression,
  generic-default, async-channel, ownership, and projection-path refs; direct
  materializer calls are blocked outside central compatibility owners.
- Current sizes: `type_checker_expr.h` 10 LOC,
  `type_checker_expr.c` 353 LOC,
  `type_checker_expr_call.c` 433 LOC,
  `type_checker_expr_host.c` 212 LOC,
  `type_checker_resolve.c` 44 LOC,
  `type_checker_resolution_helpers.c` 282 LOC, and
  `type_checker_resolution_helpers.h` 22 LOC. The expression semantic owner
  family is now below the 600 LOC split-review threshold.
- Local gates: `make test-semantic`,
  `make type-resolution-resolver-inventory-test-smoke`, and
  `make type-resolution-dag-test-smoke` are green (`2359/0`,
  fallback seams=0, `materializer_fallbacks=0`).

## UTF-8 Progress Note - 2026-04-28 - HIR CFG Phi Owner Split

- `src/compiler/hir_cfg.c` now keeps CFG structural analysis only:
  predecessor finalization, reachability, dominance/frontier, dominator tree,
  natural loops, and CFG summary finalization.
- Local-def collection, SSA-name collection, phi-candidate placement, and phi
  materialization moved to `src/compiler/hir_cfg_phi.c` behind the private
  `src/compiler/hir_cfg_intenal.h` seam.
- Current sizes: `hir_cfg.c` 388 LOC, `hir_cfg_phi.c` 222 LOC, and
  `hir_cfg_intenal.h` 8 LOC. This closes the last active HIR CFG owner-size
  review-band item without reintroducing `.inc` files.
- Local gates: `make test-hir test-mir cfg-body-dataflow-test-smoke`
  (HIR 14/0, MIR 14/0).

## UTF-8 Progress Note - 2026-04-28 - Semantic Effects Helper Header Debt Split

- `src/semantic/type_checker_helpers_effects.h` is now declaration-only. The
  former implementation-header body moved to named semantic owners:
  `type_checker_helpers_effects.c` for effect/type helper behavior,
  `type_checker_projection_path.c` for projection source field-path
  resolution, and `type_checker_world_embedding.c` for world constructor
  zone-embedding handoff diagnostics.
- Current sizes: `type_checker_helpers_effects.c` 504 LOC,
  `type_checker_projection_path.c` 177 LOC,
  `type_checker_world_embedding.c` 132 LOC, and
  `type_checker_helpers_effects.h` 11 LOC.
- This removes another implementation-style private header from semantic core
  and keeps these owners under the 600 LOC split-review threshold.
- Local gates: `make test-semantic semantic-core-shape-test-smoke`
  (`2359/0`).

## UTF-8 Progress Note - 2026-04-28 - CFG Flow Effect Owner Split

- `src/semantic/type_checker_flow_effects.h` is now declaration-only. The
  branch-effect conflict, unreachable-statement, and effect-delta merge
  implementation moved to the real owner
  `src/semantic/type_checker_flow_effects.c`.
- `src/semantic/type_checker_flow.c` stays focused on CFG body-flow
  orchestration and fact consumption. Effect diagnostics no longer live in an
  implementation-style private header, which keeps the CFG cleanup direction
  aligned with the no-`.inc` / named-owner rule.
- Current sizes: `type_checker_flow.c` 457 LOC,
  `type_checker_flow_effects.c` 122 LOC, and
  `type_checker_flow_effects.h` 33 LOC.
- Local gates: `make cfg-body-dataflow-test-smoke test-semantic`
  (`2359/0`), `make semantic-core-shape-test-smoke
  backend-inc-size-test-smoke type-resolution-dag-test-smoke`
  (`materializer_fallbacks=0`).

## UTF-8 Progress Note - 2026-04-28 - CFG Body Flow Flag Consumption Tightening

- `src/semantic/type_checker_flow.c` now routes fallthrough/terminator flag
  consumption through named helpers: `flow_record_statement_result()`,
  `flow_has_fallthrough()`, and `flow_terminating_flags()`. This keeps the
  semantic body-flow owner focused on CFG fact consumption instead of open-coded
  flag masks at each join.
- `tests/cfg_body_dataflow_smoke.sh` now gates those helper seams alongside the
  existing all-path retun, unreachable statement, resource snapshot, defer, and
  parallel boundary terms.
- Local gates: `make cfg-body-dataflow-test-smoke test-semantic`
  (`2359/0`).

## UTF-8 Progress Note - 2026-04-28 - MIR Cleanup CFG Shape Validation

- `src/compiler/mir_cfg_contract_validate.h` now rejects cleanup blocks that
  carry normal CFG successors and cleanup blocks that are also marked as pin
  regions. Cleanup/rollback/invalidation must remain exceptional cleanup-chain
  blocks, not normal body-flow blocks.
- `src/test_mir.c` adds a negative corruption regression that mutates an intent
  cleanup block to point at a normal successor and expects `mir_validate()` to
  reject it with the cleanup-block/normal-CFG-successor diagnostic.
- `tests/cfg_body_dataflow_smoke.sh` now gates the validator terms so this
  cannot regress into an undocumented MIR convention.
- Local gates: `make test-mir cfg-body-dataflow-test-smoke` (MIR 14/0).

## UTF-8 Progress Note - 2026-04-28 - Zone Lifecycle Authority Presence Split

- `src/semantic/type_checker_zone_decl.c` no longer owns the repeated
  lifecycle `by <subjectSlot>` presence diagnostics for authority-bearing
  zones. Apply/link/detach/unlink/maintain authority-presence checks now route
  through `type_check_zone_lifecycle_authority_presence()` in
  `src/semantic/type_checker_zone_decl_authority.c`.
- Current zone semantic owner sizes are `type_checker_zone_decl.c` 487 LOC and
  `type_checker_zone_decl_authority.c` 300 LOC. This keeps the zone declaration
  family under the 600 LOC split-review threshold while moving authority policy
  wording into the authority owner.
- Local gates: `make semantic-core-shape-test-smoke`; `make test-semantic`
  (2359/0).

## UTF-8 Progress Note - 2026-04-28 - Intent Authority/Participant Owner Split

- `src/semantic/type_checker_intent_decl.c` no longer owns the full
  authority/authorized-by validation body. The missing `authorized by` contract
  diagnostic and authorized participant-to-zone-authority resolution moved to
  `src/semantic/type_checker_intent_authority.c`.
- Intent `who` participant validation, zone subject-slot matching, transfer
  source subject-slot matching, and action-match detection moved to
  `src/semantic/type_checker_intent_participants.c`.
- Current intent semantic owner sizes are `type_checker_intent_decl.c` 504 LOC,
  `type_checker_intent_authority.c` 242 LOC, and
  `type_checker_intent_participants.c` 115 LOC, keeping the family under the
  600 LOC split-review threshold without adding `.inc` files.
- This is an incremental domain-checker slimming slice, not a full
  Domain-AST-to-Core-AST rewrite. Intent declaration orchestration still owns
  step order and summary flow; authority and participant proof/diagnostic
  ownership are now named semantic owners.
- Local gates: `make semantic-core-shape-test-smoke`; `make test-semantic`
  (2359/0).

## UTF-8 Progress Note - 2026-04-28 - HIR CFG Contract Validation

- HIR routine finishing now validates CFG shape immediately after body lowering
  and validates predecessor mirrors immediately after predecessor
  materialization. `hir_validate_cfg_shape()` rejects open fallthrough blocks,
  invalid successor indices, inconsistent terminator successor flags, missing
  branch conditions, and block-id drift before dominance/frontier/loop/phi
  analysis can consume the graph.
- `hir_validate_cfg_predecessors()` verifies that every successor has the
  matching predecessor edge and every predecessor points back to the block it
  names. This tuns CFG structural consistency into a compiler-owned gate
  rather than an assumption inside dominance/MIR lowering.
- `tests/cfg_body_dataflow_smoke.sh` now gates the validation seam. This does
  not close the larger blocker that semantic body safety must fully consume
  CFG/dataflow facts; it closes the underlying HIR CFG invariant layer those
  future consumers rely on.
- HIR CFG summaries now also materialize `retun_block_count` and
  `normal_exit_block_count`. A reachable `HIR_BLOCK_UNREACHABLE` is the
  lowered normal fallthrough exit, so this gives later semantic/MIR consumers a
  direct CFG fact for "may fall through" instead of rediscovering it from AST
  traversal.

## UTF-8 Progress Note - 2026-04-28 - MIR Active Inventory API Seam

- `MIRProgram` now exposes `mir_active_inventory()` and
  `mir_active_extens()` as the shared declaration inventory read seam.
  C backend `transpiler_active_inventory()` and LLVM backend
  `llvm_active_inventory()` no longer duplicate their own
  `ASTNodeType -> mir->...` declaration-array switch.
- `tests/mir_declaration_inventory_smoke.sh` now gates that both C and LLVM
  active inventory helpers consume the MIR public API seam.
- This does not claim dedicated declaration IR closure yet: `MIRProgram` still
  carries AST declaration arrays. The closed slice is the duplicated backend
  mapping seam, so the future dedicated decl-IR replacement has one compiler
  API boundary to replace instead of two backend-local switches.

## UTF-8 Progress Note - 2026-04-28 - C Intent Emitter Owner Split

- `src/codegen/transpiler_intent_emit.h` is no longer a single 965 LOC
  declaration emitter. Intent signature/runtime-entry emission moved to
  `src/codegen/transpiler_intent_prologue_emit.h`, and cleanup/rollback/
  invalidation tail emission moved to
  `src/codegen/transpiler_intent_cleanup_emit.h`.
- Current C intent owner sizes are `transpiler_intent_emit.h` 577 LOC,
  `transpiler_intent_prologue_emit.h` 186 LOC, and
  `transpiler_intent_cleanup_emit.h` 278 LOC, all below the 600 LOC
  split-review threshold.
- Local gate: `make pgy` and `make llvm-test-backend-compare` are green with
  ABI same-process `196 passed, 0 failed` and backend compare `64/64 passed,
  0 failed`.
- This closes the C intent emitter owner-size slice. Remaining codegen review
  band debt is now concentrated in LLVM/domain/projection owners and
  declaration/top-level inventory bootstrap, not this C intent emitter.

## UTF-8 Progress Note - 2026-04-28 - C MIR CFG Consumer Parity Closure

- C backend MIR emission no longer lets CFG-expanded range `for` or
  `match case` nodes fall through to the generic expression emitter.
  `src/codegen/transpiler_mir_cfg_control_emit.h` now owns C-side MIR branch
  condition rendering for range-loop headers and `Option`/`Result` match
  cases, plus range-loop init/backedge maintenance.
- Explicit CFG containers are now skipped as opaque AST statements on the C
  MIR block path. Range `for` emits only its MIR-owned init in the predecessor
  block, branch conditions are emitted by MIR terminators, and loop backedges
  perform the increment before jumping back to the header.
- Pin-view SSA copies are constrained at CFG edges: `view` bindings from
  `pin ... as view` are region-local resources and are no longer phi-copied
  into break/exit successors. This closes the previous
  `pin_break_cleanup_block` compile failure.
- MIR phi-copy emission now has a named owner in
  `src/codegen/transpiler_mir_phi_emit.h`; `transpiler_mir_ssa_emit.h` is back
  below the 600 LOC split-review threshold.
- MIR terminator emission now has a named owner in
  `src/codegen/transpiler_mir_terminator_emit.h`, and residual MIR statement
  helpers now live in `src/codegen/transpiler_mir_stmt_emit.h`. This brings
  `transpiler_mir_func_emit.h`, `transpiler_mir_block_emit.h`, and
  `transpiler_mir_ssa_emit.h` all below the 600 LOC split-review threshold.
- Local gate: `make llvm-test-backend-compare` is green again with
  ABI same-process `196 passed, 0 failed` and backend compare `64/64 passed,
  0 failed`.
- This closes the C MIR CFG/body emitter owner-size slice for the current
  private-header threshold. Remaining backend debt is now higher-level:
  declaration/top-level inventory bootstrap and broader CFG/AIR semantic
  consumption, not these C MIR emission owners.

## UTF-8 Progress Note - 2026-04-28 - LLVM MIR CFG Control / Declaration Metadata Closure

- LLVM MIR block emission no longer treats CFG-expanded `for`, `select`, and
  `match` containers as opaque AST statement payloads. Range-for init/header/
  backedge increment, select channel-readiness dispatch, receive bind target
  declaration, and match-case condition lowering now live on the MIR CFG
  control path.
- `src/codegen/llvm_mir_block_emit.h` was reduced to 430 LOC by moving the
  CFG-control lowering owner into the real translation unit
  `src/codegen/llvm_mir_cfg_control.c` (363 LOC). No production `.inc` file was
  reintroduced.
- `src/codegen/llvm_intenal.h` is no longer the LLVM backend mega-header for
  every private prototype. Private API declarations moved to
  `src/codegen/llvm_intenal_api.h`, and fixed limits / dynamic-array helpers
  moved to `src/codegen/llvm_limits_intenal.h`. Current sizes are
  `llvm_intenal.h` 574 LOC, `llvm_intenal_api.h` 325 LOC, and
  `llvm_limits_intenal.h` 54 LOC.
- `src/codegen/llvm_registry.c` no longer mixes resource/type registries with
  scope/function/class/callable/enum registry ownership. Slot/view/future/
  channel/Rc/Weak/container variable tracking and Slot/SecureSlot/PinnedSlot/
  array/slice/list/set/queue/hashmap type helper materialization moved to
  `src/codegen/llvm_registry_resources.c`. Current sizes are
  `llvm_registry.c` 532 LOC and `llvm_registry_resources.c` 415 LOC.
- LLVM method signature accessors are now MIR-declaration-metadata only. The
  old AST-method fallback path in `llvm_mir_decl_method_*` accessors is gone;
  missing enum/class method metadata is a hard MIR-only LLVM path error.
- Local gates: `make pgy`, `make llvm-test-smoke`,
  `make mir-declaration-inventory-test-smoke`, and
  `make type-resolution-resolver-inventory-test-smoke`.
- Remaining LLVM/MIR blocker is not this MIR CFG statement slice. The blocker
  is declaration/top-level inventory bootstrap debt: some enum/class method
  iteration still starts from AST-carried inventory before consuming MIR
  declaration metadata. That must move to a dedicated declaration IR/inventory
  owner before beta can claim backend source-of-truth closure.
- Current owner-size caveat: production `.inc` debt under `src/` is closed, but
  several production `.c/.h` owners remain in the 600-1,000 LOC review band
  (`llvm_intent.c`, `transpiler_overlay_projection.h`, `llvm_domain.c`,
  `pgy_lsp.c`, and related backend/tooling owners). These are review-band
  debts, not `.inc` debts.

## UTF-8 Progress Note - 2026-04-28 - World Semantic Owner Split

- `src/semantic/type_checker_world_decl.c` no longer owns world/zone lookup
  helpers or shared domain slot validation. World-zone/state lookup,
  world-state target resolution, zone layer/state lookup, and world lifecycle
  target resolution moved to `src/semantic/type_checker_world_helpers.c`
  behind `src/semantic/type_checker_world_intenal.h`.
- Shared relation/effect/zone domain slot type validation and initializer
  checking moved to `src/semantic/type_checker_domain_slots.c`, so the world
  declaration pass now stays focused on world symbol declaration, roster/zone
  visibility, world state composition, lifecycle direction checks, shared
  fields, and hosted methods.
- Current sizes are `type_checker_world_decl.c` 588 LOC,
  `type_checker_world_helpers.c` 186 LOC, `type_checker_domain_slots.c` 115
  LOC, and `type_checker_world_intenal.h` 28 LOC. The world semantic owner
  family is now below the 600 LOC split-review threshold.
- Local gates: `make pgy`, `make test-semantic` (2359/0),
  `make type-resolution-dag-test-smoke`, and
  `make type-resolution-resolver-inventory-test-smoke`.

## UTF-8 Progress Note - 2026-04-28 - HIR CFG Dispatch/Loop Edge Closure

- `src/compiler/hir_lower_cfg.c` now lowers `break` and `continue` as explicit
  CFG terminators when they appear inside `while` / `for` bodies. A loop
  context carries the loop-exit target for `break` and the loop-header target
  for `continue`, so HIR dominance/frontier/loop-depth facts no longer treat
  loop control as opaque statement payload.
- Labeled loop control is now target-correct in HIR CFG. Nested
  `break outer` / `continue outer` resolve through the loop-context parent chain
  to the named loop's exit/header instead of the nearest loop.
- `match` and `select` no longer remain opaque in HIR CFG. Both lower through
  an explicit dispatch/join helper where each case is a branch condition,
  fallthrough case/default bodies join through CFG edges, and terminating cases
  remain closed. This keeps channel readiness cases visible to later HIR/MIR
  consumers instead of hiding them as a single AST payload.
- `unsafe` blocks no longer hide nested body control flow from HIR CFG. Nested
  `retun` / branch terminators inside `unsafe` now flow through the same
  terminator and reachability model as ordinary blocks.
- `src/test_hir.c` adds `HIR CFG lowers loop break and continue edges
  explicitly`, `HIR CFG lowers match cases and default as explicit edges`,
  `HIR CFG lowers select cases and default as explicit edges`,
  `HIR CFG lowers unsafe block body control flow`, and
  `HIR CFG resolves labeled loop control to the named loop` to lock this
  behavior.
- Local gates: `make test-hir` (14/0), `make cfg-body-dataflow-test-smoke`,
  `make test-rir`, and `make test-mir`.
- HIR owner split continues without adding `.inc`: `hir_destroy()` and the
  synthetic executable teardown path now live in `src/compiler/hir_destroy.c`,
  while declaration/routine construction and hidden method routine extraction
  live in `src/compiler/hir_routines.c` behind `src/compiler/hir_intenal.h`.
  Current HIR owner sizes are `hir.c` 421 LOC, `hir_routines.c` 419 LOC,
  `hir_lower_cfg.c` 598 LOC, and `hir_cfg.c` 599 LOC, so the active HIR owner
  set is under the 600 LOC split-review threshold.
- Compiler driver owner debt also moved below the split-review threshold
  without adding `.inc`: result/diagnostic object ownership now lives in
  `src/compiler/compiler_result.c`, LLVM emission/native-link orchestration and
  disabled-LLVM stubs live in `src/compiler/compiler_llvm.c`, and runtime object
  cache freshness/path construction lives in `src/compiler/compiler_runtime_cache.c`.
  Current owner sizes are `compiler.c` 289 LOC, `compiler_llvm.c` 350 LOC,
  `compiler_result.c` 79 LOC, `compiler_toolchain.c` 543 LOC, and
  `compiler_runtime_cache.c` 138 LOC.
- Driver pipeline owner debt is also below the split-review threshold:
  `src/compiler/driver_app.c` now owns pipeline orchestration only, while JSON
  diagnostic routing, AIR drift diagnostics, Reason/Fix emission, and code/
  cause/fix mapping live in `src/compiler/driver_diag.c`. Current sizes are
  `driver_app.c` 457 LOC, `driver_diag.c` 358 LOC, and `driver_diag.h` 16 LOC.
- Module normalizer owner debt is closed under the same no-`.inc` rule:
  `src/compiler/module_normalizer.c` now owns module-level orchestration,
  namespace shells, explicit export scanning, and statement-list traversal,
  while rename-scope state, shadowed-name tracking, type/generic/call/reference
  rewriting, and AST node reference normalization live in
  `src/compiler/module_normalizer_refs.c` behind
  `src/compiler/module_normalizer_intenal.h`. Current sizes are
  `module_normalizer.c` 261 LOC, `module_normalizer_refs.c` 565 LOC, and
  `module_normalizer_intenal.h` 39 LOC.
- Scaffold owner debt is also split below the 600 LOC review threshold:
  `src/compiler/driver_scaffold.c` now owns filesystem helpers, single-file
  scaffold templates, and command dispatch, while simulator/project directory
  templates live in `src/compiler/driver_scaffold_project.c` behind
  `src/compiler/driver_scaffold_intenal.h`. Current sizes are
  `driver_scaffold.c` 473 LOC, `driver_scaffold_project.c` 351 LOC, and
  `driver_scaffold_intenal.h` 16 LOC.
- RIR builder include debt has been converted into real translation units:
  the old implementation-style `rir_builder.h` is now
  `src/compiler/rir_builder.c`, intent-scope collection lives in
  `src/compiler/rir_builder_intent.c`, RIR fact/utility materialization lives
  in `src/compiler/rir_facts.c`, RIR vocabulary names live in
  `src/compiler/rir_names.c`, RIR dump/destroy public-surface ownership lives
  in `src/compiler/rir_public_surface.c`, RIR validation / DIR contract
  checking lives in `src/compiler/rir_validation.c`, HIR-backed RIR flow
  enrichment lives in `src/compiler/rir_flow.c`, and shared helper seams are
  declared in `src/compiler/rir_intenal.h`. Current sizes are
  `rir_builder.c` 552 LOC, `rir_validation.c` 525 LOC, `rir_facts.c` 487 LOC,
  `rir_flow.c` 457 LOC, `rir.c` 429 LOC, `rir_public_surface.c` 287 LOC,
  `rir_builder_intent.c` 135 LOC, `rir_names.c` 110 LOC, and
  `rir_intenal.h` 64 LOC. The RIR owner family is now under the 600 LOC
  split-review threshold without reintroducing an implementation-style
  include file.
- Local gates for the owner split: `make pgy`, `make LLVM_ENABLED=0 ... pgy`,
  `make test-hir`, `make test-rir`, `make test-air`, `make test-mir`,
  `make air-drift-test-smoke`,
  `make test-semantic`, `make type-resolution-dag-test-smoke`,
  `make semantic-fixture-isolation-test-smoke`,
  direct `pgy scaffold subject|simulator|project` smoke,
  `make backend-inc-size-test-smoke`, `make documentation-quality-test-smoke`,
  and `make beta-readiness-checklist-test-smoke`.

## UTF-8 Progress Note - 2026-04-28 - DAG Fallback Recheck

- Rechecked the type-resolution DAG gates. Current local stats are
  `graph-backed skips=2033 retired_resolver_calls=0 retired_resolver_unique_nodes=0
  metadata_entries=3498 metadata_owned=258 metadata_hits=8380
  materializer_fallbacks=0`.
- Metadata unresolved audit families are all zero:
  `metadata_unresolved_named=0 metadata_unresolved_generic_named=0
  metadata_unresolved_compound=0 metadata_unresolved_other=0
  metadata_named_builtin_shell=0 metadata_named_generic_class=0
  metadata_named_alias=0 metadata_named_non_class_symbol=0
  metadata_named_missing_symbol=0`.
- Stage metadata materialization is now zero for both alias and non-alias replay:
  `stage_materialize_alias=0 stage_materialize_non_alias=0 alias_materialized=6
  alias_diagnostic_unresolved=78 alias_diagnostic_resolver_calls=0`.
- `type-resolution-resolver-inventory-test-smoke` now gates 12
  `semantic_type_resolution_lookup_resolved_annotation(...)` seams outside the
  metadata owners. These are remaining DAG source-of-truth seams for
  annotation-sensitive readers, not recursive resolver fallback.
- DAG stage replay is now split by owner family:
  `type_checker_resolution_stage_nominal.c` owns class/enum/ability/role,
  `type_checker_resolution_stage_systemic.c` owns party/roster/world/intent,
  and `type_checker_resolution_stage_domain_decl.c` owns relation/effect/zone.
  The top-level stage owner is now 88 LOC and split owners are 239 LOC or
  smaller, all under the 600 LOC split-review threshold.
- DAG metadata alias-chain and cycle materialization is now split into
  `type_checker_resolution_metadata_alias.c`. The central metadata owner is
  268 LOC, the alias owner is 315 LOC, and alias/cycle provenance no longer
  shares the central lookup/materializer orchestration body.
- The old recursive alias resolver and `SemanticContext.alias_resolution_*`
  stack are removed. `resolve_named_type(...)` now routes alias names through
  `semantic_type_resolution_lookup_metadata_name_or_alias(...)`, so alias
  chain/cycle semantics have a single metadata owner.
- `resolve_named_type(...)` is now metadata-first for builtin, scope, generic
  parameter, nominal, and alias names. If DAG metadata cannot answer, it falls
  back to the existing user-facing diagnostic path; successful stable names no
  longer bypass metadata before checking local scope.
- Valid alias stage materialization now uses the metadata-only lookup before
  reporting diagnostic unresolved inventory. Valid aliases no longer leak through the
  recursive resolver compatibility body, and
  `tests/type_resolution_dag_smoke.sh` now gates alias compatibility fallback at zero.
- The central metadata materializer no longer falls through to
  `resolve_type_node(type_node, ctx)`. Unsupported metadata shapes are recorded
  as explicit materializer fallback and retun unresolved; the DAG smoke keeps
  that fallback count at zero, while resolver-inventory smoke rejects any
  recursive escape-hatch reintroduction.
- Added semantic regression `graph-backed forward alias materializes nested
  constructed type`, covering a function signature that consumes a later alias
  to `Channel<Slot<Int>>`. This pins the source-order pain point where nested
  constructed aliases could regress into recursive fallback or unknown-type
  behavior.
- Pain point found while checking: semantic cross-module cases still use fixed
  temporary import filenames. `test_semantic` now enters an isolated repo-local
  `.tmp/pgy-semantic-test.*` cwd before running cases, and
  `tests/type_resolution_dag_smoke.sh` runs the semantic binary from its own
  `.tmp/pgy-type-resolution-dag.*` cwd. This isolates fixture files for direct
  parallel semantic binary runs and for `test-semantic` +
  `type-resolution-dag-test-smoke`. Parallel `make test-semantic` targets are
  still not supported because they can relink the same binary while another
  process executes it. `semantic-fixture-isolation-test-smoke` is wired into
  runnable Linux/macOS/Windows CI paths.
- `tests/type_resolution_resolver_inventory_smoke.sh` now also checks that the
  materializer fallback recorder stays in its central owner and recursive
  metadata escape hatches stay at zero. New owner-local fallback users remain
  rejected.
- Remaining DAG blocker is no longer metadata materializer fallback volume.
  The blocker is retiring the recursive resolver implementation as an
  evaluator source and making the graph/topo materializer the only semantic
  evaluation path for stable type refs.
- Local gates: `make type-resolution-dag-test-smoke`,
  `make type-resolution-resolver-inventory-test-smoke`,
  `make semantic-fixture-isolation-test-smoke`, and concurrent
  `make test-semantic` + `make type-resolution-dag-test-smoke`.
- Follow-up DAG consumer seam closure: the materializing type-ref helper is now
  present only at its declaration and implementation (`helper refs=2 cap=2`).
  No semantic consumer calls `semantic_type_resolution_lookup_type_ref_or_materialize(...)`
  directly. Current DAG local stats: `graph-backed skips=2033`,
  `metadata_entries=3498`, `metadata_owned=258`, `metadata_hits=8380`,
  `retired_resolver_calls=0`, `materializer_fallbacks=0`, and all unresolved
  materializer families at 0. Remaining DAG work is evidence/modeling
  completion, not recursive fallback removal.

## UTF-8 Progress Note - 2026-04-28 - AST Destroy Owner Split

- `src/parser/ast.c` no longer owns AST destruction. Mutation helpers remain
  in `ast.c`; generic/where/comment destruction plus non-domain destroy cases
  moved to `src/parser/ast_destroy.c`; domain/world/zone/intent/party/ability/
  event destroy cases moved to `src/parser/ast_destroy_domain.c`.
- Current owner sizes by `wc -l`: `ast.c` 65, `ast_destroy.c` 393,
  `ast_destroy_domain.c` 456, and `ast_destroy_intenal.h` 11. The AST runtime
  owner family is now below the 600 LOC split-review threshold.
- Local gate: `make test-parser`.

## UTF-8 Progress Note - 2026-04-28 - Parser Declaration/Type Owner Split

- `src/parser/parser_decl.c` no longer owns generic parameter parsing, type
  argument parsing, where-clause parsing, type alias parsing, name-token
  helpers, or function/action clause parsing.
- Type/name/generic parsing moved to `src/parser/parser_type.c`, and
  function/action clause parsing moved to
  `src/parser/parser_decl_function_clause.c`.
- Current owner sizes by `wc -l`: `parser_decl.c` 327,
  `parser_type.c` 351, and `parser_decl_function_clause.c` 230. The declaration
  parser family is now below the 600 LOC split-review threshold.
- Local gate: `make test-parser`.

## UTF-8 Progress Note - 2026-04-28 - Parser Statement Dispatch Owner Split

- `src/parser/parser.c` no longer owns top-level statement dispatch. Parser
  lifecycle, token movement, error handling, program parsing, and block/let/
  with/parallel leaf parsers remain in `parser.c`; declaration/statement
  dispatch moved to `src/parser/parser_statement_dispatch.c`.
- Current owner sizes by `wc -l`: `parser.c` 414 and
  `parser_statement_dispatch.c` 460. The core parser owner family is now below
  the 600 LOC split-review threshold except for `ast.h`, which remains the
  AST shape header.
- Local gate: `make test-parser`.

## UTF-8 Progress Note - 2026-04-28 - Zone Declaration Owner Split

- `src/semantic/type_checker_zone_decl.c` no longer owns zone shape wanings,
  projection refresh/publish/bind rule validation, or state alias validation.
- Zone shape wanings moved to `src/semantic/type_checker_zone_shape.c`,
  projection rules moved to `src/semantic/type_checker_zone_projection_rules.c`,
  and maintained-state / state-alias validation moved to
  `src/semantic/type_checker_zone_state.c`.
- Current owner sizes by `wc -l`: `type_checker_zone_decl.c` 558,
  `type_checker_zone_projection_rules.c` 91,
  `type_checker_zone_state.c` 263, and `type_checker_zone_shape.c` 42.
  The zone declaration semantic owner family is now below the 600 LOC
  split-review threshold.
- Local gate: `make test-semantic` (2357/0).

## UTF-8 Progress Note - 2026-04-28 - Function Call Constructor Owner Split

- `src/semantic/type_checker_helpers_late.c` no longer owns constructor-like
  symbol call validation for subject/class, relation/effect/roster/world/zone
  overlay constructors, and world-zone embedding handoff diagnostics.
- Those checks moved to `src/semantic/type_checker_call_constructor.c`, while
  the late helper owner stays focused on callable symbol dispatch, argument
  ownership validation, generic call-site where-clause validation, and retun
  type materialization.
- Current owner sizes by `wc -l`: `type_checker_helpers_late.c` 799 and
  `type_checker_call_constructor.c` 220. The late helper owner is still in the
  600-1,000 LOC review band but is no longer mixing constructor diagnostics
  with function-call argument flow.
- Local gate: `make test-semantic` (2357/0).

## UTF-8 Progress Note - 2026-04-28 - Function Call Late Helper Owner Split

- `src/semantic/type_checker_helpers_late.c` no longer owns active slot-view
  discovery/owner-escape checks, callee parameter contract lookup, callable
  parameter escape-summary lookup, or call-site generic where-clause
  validation.
- These moved to named owners:
  `type_checker_slot_view_active.c`,
  `type_checker_call_contract_helpers.c`, and
  `type_checker_call_generic_where.c`. The late helper owner now stays focused
  on callable symbol dispatch, argument type/ownership flow, and retun type
  materialization.
- Current owner sizes by `wc -l`: `type_checker_helpers_late.c` 488,
  `type_checker_slot_view_active.c` 146,
  `type_checker_call_contract_helpers.c` 60, and
  `type_checker_call_generic_where.c` 160. The function-call late-helper owner
  family is now below the 600 LOC split-review threshold.
- Local gates: `make pgy`, `make test-semantic`,
  `make type-resolution-dag-test-smoke`, and
  `make type-resolution-resolver-inventory-test-smoke`.

## UTF-8 Progress Note - 2026-04-28 - LLVM Statement Owner Split

- `src/codegen/llvm_stmt.c` no longer owns zone-action effect runtime
  propagation helpers or generic type-argument rendering helpers.
- Zone-action effect propagation moved to `llvm_stmt_zone_action.c`; generic
  type-argument rendering moved to `llvm_stmt_type_render.c`. The statement
  owner now stays focused on statement dispatch, defers, retun/if/block
  emission, and expression-statement forwarding.
- Current owner sizes by `wc -l`: `llvm_stmt.c` 573,
  `llvm_stmt_zone_action.c` 275, and `llvm_stmt_type_render.c` 74. The LLVM
  statement owner family is now below the 600 LOC split-review threshold.
- Local gates: `make pgy` and `make llvm-test-smoke`.

## UTF-8 Progress Note - 2026-04-28 - LLVM Let Statement Owner Split

- `src/codegen/llvm_stmt_let_with.c` no longer owns collection/channel/array
  let specializations or callable/lambda registration post-processing.
- Collection-like let lowering moved to `llvm_stmt_let_collections.c`, and
  callable let registration moved to `llvm_stmt_let_callable.c`. The let owner
  now stays focused on Slot/View/MoveToken sugar, class-constructor lets,
  initializer storage/coercion, and typed variable/future registration.
- Current owner sizes by `wc -l`: `llvm_stmt_let_with.c` 562,
  `llvm_stmt_let_collections.c` 258, and `llvm_stmt_let_callable.c` 79. The
  LLVM let statement owner family is now below the 600 LOC split-review
  threshold.
- Local gates: `make pgy` and `make llvm-test-smoke`.

## UTF-8 Progress Note - 2026-04-28 - LLVM MIR CFG Match Destructor Fix

- `src/codegen/llvm_mir_cfg_control.c` now handles `Option` and `Result`
  destructor pattens (`Some/None`, `Ok/Err`) when a source `match` has been
  expanded into MIR CFG case branches. The previous MIR CFG path compared the
  whole aggregate value with `icmp`, which made `projection_abi` fail LLVM
  verification for `match Option<Int>`.
- The same MIR CFG path now materializes the case payload binding
  (`Some(v)`, `Ok(v)`, `Err(e)`) before the case branch body consumes it. This
  restores the `projection_abi` expected output (`49`) instead of defaulting
  the payload to `0`.
- Local gates: `make pgy`, direct `projection_abi` LLVM compile/run probe, and
  the ABI same-process precheck inside `make llvm-test-backend-compare`
  (`196 passed, 0 failed`).
- Follow-up C backend parity debt from this note is now closed by the
  2026-04-28 C MIR CFG consumer parity slice: full
  `make llvm-test-backend-compare` is green with backend compare `64/64`.

## UTF-8 Progress Note - 2026-04-28 - Runtime Slot Pin Owner Split

- `src/runtime/slot_manager.c` no longer owns `PergyraSlotPin` /
  `PergyraSlotUnpin`. Pinned view validation, secure payload open/seal,
  stale-generation rejection, release-while-pinned rejection, and Pin token
  validation moved to `src/runtime/slot_manager_pin.c`.
- Current owner sizes by `wc -l`: `slot_manager.c` 564,
  `slot_manager_query_lock.c` 240, and `slot_manager_pin.c` 185. Query,
  TTL cleanup, locking, stats, and fast wrappers moved to
  `slot_manager_query_lock.c`, so the slot lifecycle owner is now below the
  600 LOC split-review threshold.
- Local gates: `make test-security` (142/0) and `make test-abi` (58/0 plus
  C/LLVM ABI pipeline smoke).

## UTF-8 Progress Note - 2026-04-28 - Type System Inference/Effect Owner Split

- `src/semantic/type_system.c` no longer owns lightweight expression inference
  or function/resource effect mask helpers. `type_infer_expression` /
  `type_unify` moved to `src/semantic/type_infer.c`, and function effect/body
  summary plus effect lattice helpers moved to `src/semantic/type_effects.c`.
- Current owner sizes by `wc -l`: `type_system.c` 598, `type_infer.c` 254,
  and `type_effects.c` 106. The type core owner is now below the 600 LOC
  split-review threshold.
- Local gate: `make test-semantic` (2357/0).

## UTF-8 Progress Note - 2026-04-28 - Intent Transfer Contract Owner Split

- `src/semantic/type_checker_intent_decl.c` no longer owns the full
  transfer/handoff diagnostic block inside the main intent declaration pass.
  Transfer source/target alias validation, zone-binding checks, transfer target
  versus current zone contract checks, and `using` versus transfer-target
  consistency diagnostics moved to `src/semantic/type_checker_intent_transfer.c`.
- Current owner sizes by `wc -l`: `type_checker_intent_decl.c` 797 and
  `type_checker_intent_transfer.c` 207. The main intent declaration owner is
  still in the 600-1,000 LOC review band, but the handoff contract check now
  has a named owner seam.
- Local gate: `make test-semantic` (2357/0).

## UTF-8 Progress Note - 2026-04-28 - Intent Action Contract Helper Split

- `src/semantic/type_checker_intent_helpers.c` no longer owns action-contract
  inheritance, redundant contract diagnostics, or contract-source summary
  formatting. The helper owner is now limited to intent condition/involves and
  projection-adjacent utility routines.
- Action-contract inheritance and redundant-step wanings moved to
  `src/semantic/type_checker_intent_action_contract.c`; contract-source summary
  formatting moved to `src/semantic/type_checker_intent_contract_summary.c`.
- Current owner sizes by `wc -l`: `type_checker_intent_helpers.c` 145,
  `type_checker_intent_action_contract.c` 481, and
  `type_checker_intent_contract_summary.c` 341. This closes the old
  `type_checker_intent_helpers.c` 883 LOC semantic owner-size debt under the
  600 LOC split-review threshold.
- Local gates: `make test-semantic` (2359/0) and
  `make type-resolution-dag-test-smoke` (graph-backed skips=2033,
  retired_resolver_calls=0, metadata_entries=3498, metadata_hits=8380,
  materializer_fallbacks=0).

## UTF-8 Progress Note - 2026-04-28 - Ownership Constructor Diagnostic Split

- `src/semantic/type_checker_ownership_diag.c` no longer owns constructor-field
  escape diagnostic formatting. That path moved to
  `src/semantic/type_checker_ownership_diag_constructor.c`.
- Current owner sizes by `wc -l`: `type_checker_ownership_diag.c` 550 and
  `type_checker_ownership_diag_constructor.c` 70. This closes the previous
  611 LOC ownership diagnostic owner over the 600 LOC split-review threshold.
- Local gate: `make test-semantic` (2359/0).

## UTF-8 Progress Note - 2026-04-28 - Semantic Domain Contract Owner Split

- `src/semantic/type_checker_decls_domain_helpers.c` no longer owns zone
  relation/effect contract validation. Contract arity checks, endpoint-kind
  matching, and provenance-heavy zone relation/effect diagnostics moved to
  `src/semantic/type_checker_domain_contracts.c`.
- Current owner sizes by `wc -l`: `type_checker_decls_domain_helpers.c` 448
  and `type_checker_domain_contracts.c` 537. Both are below the 600 LOC
  split-review threshold.
- Local gate: `make test-semantic` (2357/0).
- Result: the domain helper family is no longer a semantic owner-size blocker.
  The next semantic split candidates are `type_checker_helpers_late.c`,
  `type_checker_intent_decl.c`, `type_system.c`, and
  `type_checker_zone_decl.c`.

## UTF-8 Progress Note - 2026-04-28 - AST Public API Header Split

- `src/parser/ast.h` no longer owns the public AST constructor/manipulation
  prototype surface. Those declarations moved to `src/parser/ast_api.h`, which
  is included by `ast.h` for source compatibility.
- `ast.h` is now 848 LOC and stays focused on the `ASTNode` shape after the
  earlier `ast_types.h` vocabulary split. `ast_api.h` is 137 LOC.
- Local gates: `make test-parser pgy`,
  `make production-header-size-test-smoke inc-sentinel-test-smoke`, and
  touched-file `git diff --check`.
- Result: `ast.h` remains in the 600-1,000 LOC review band but is no longer
  near the hard cap. The next parser owner candidates are `ast.c` 894,
  `parser_decl.c` 887, and `parser.c` 867.

## UTF-8 Progress Note - 2026-04-28 - AST Print Family Owner Split

- AST print ownership is now split below the 600 LOC review threshold across
  the full printer family. Intent printers and intent contract provenance moved
  to `src/parser/ast_print_intent.c`; event printers moved to
  `src/parser/ast_print_event.c`; domain/world/zone printers remain in
  `src/parser/ast_print_domain.c`.
- Current AST print owner sizes by `wc -l`: `ast_print.c` 553,
  `ast_print_domain.c` 539, `ast_print_inline.c` 382,
  `ast_print_intent.c` 253, `ast_print_event.c` 76,
  `ast_print_generics.c` 63, and `ast_print_misc.c` 11.
- Local gates: `make test-parser pgy` and touched-file `git diff --check`.
- Result: the AST print family is no longer in the 600-1,000 LOC
  split-review band. The next parser owner queue stays on `parser.c`,
  `parser_domain.c`, `parser_decl.c`, `ast.h`, and `ast.c`.

## UTF-8 Progress Note - 2026-04-28 - Parser Declaration Hint Owner Split

- `src/parser/parser.c` no longer owns top-level declaration hint inventory.
  Declaration hint name extraction, registration, capacity growth, and lookup
  moved to `src/parser/parser_decl_hints.c`.
- `parser.c` is now 867 LOC and remains focused on parser lifecycle,
  token movement, diagnostics, synchronization, statement finalization, and
  program/statement dispatch. The new `parser_decl_hints.c` owner is 111 LOC.
- Local gates: `make test-parser pgy` and touched-file `git diff --check`.
- Remaining parser 600-1,000 LOC queue after this slice was
  `parser_domain.c`, `parser_decl.c`, `parser.c`, `ast.h`, and `ast.c`;
  `parser_domain.c` is closed by the newer relation/projection split note
  below.

## UTF-8 Progress Note - 2026-04-28 - Parser Domain Relation/Projection Owner Split

- `src/parser/parser_domain.c` no longer owns relation/effect declaration
  parsing or projection-sync helper parsing. Relation/effect declarations
  moved to `src/parser/parser_domain_relation_effect.c`; projection group
  parsing, domain group keyword matching, and projection field maps moved to
  `src/parser/parser_domain_projection.c`.
- Current domain parser owner sizes by `wc -l`: `parser_domain.c` 493,
  `parser_domain_relation_effect.c` 283, `parser_domain_projection.c` 184,
  `parser_domain_world.c` 384, `parser_domain_zone.c` 449,
  `parser_domain_roster.c` 165, and `parser_domain_event.c` 57.
- Local gates: `make test-parser pgy` and touched-file `git diff --check`.
- Result: the parser domain family is below the 600 LOC split-review
  threshold. Remaining parser queue is now `ast.h` 973, `ast.c` 894,
  `parser_decl.c` 887, and `parser.c` 867.

## UTF-8 Progress Note - 2026-04-28 - AST Print Inline/Generic Owner Split

- `src/parser/ast_print.c` no longer owns inline expression printing,
  compact one-line printing, operator spelling, escaped string rendering, or
  generic/where-clause inline rendering. Those moved to
  `src/parser/ast_print_inline.c` and `src/parser/ast_print_generics.c`.
- Current AST print owner sizes by `wc -l`: `ast_print.c` 553,
  `ast_print_inline.c` 382, `ast_print_generics.c` 63. The central AST print
  owner is now below the 600 LOC split-review threshold.
- Local gates: `make test-parser pgy` and touched-file `git diff --check`.
- Follow-up AST print queue item was `ast_print_domain.c`; it is now split in
  the newer progress note above.

## UTF-8 Progress Note - 2026-04-28 - AST Owner Split

- The last 1,000+ LOC production `.c` parser owners are split. AST printing now
  has domain and misc owners (`src/parser/ast_print_domain.c`,
  `src/parser/ast_print_misc.c`), and AST construction now has core,
  domain-constructor, and clone owners (`src/parser/ast_constructors.c`,
  `src/parser/ast_domain_constructors.c`, `src/parser/ast_clone.c`).
- Current parser owner sizes by `wc -l`: `ast.c` 894, `ast_print.c` 553,
  `ast_print_domain.c` 539, `ast_print_inline.c` 382,
  `ast_print_intent.c` 253, `ast_print_event.c` 76,
  `ast_print_generics.c` 63, `ast_constructors.c` 545,
  `ast_domain_constructors.c` 598, `ast_clone.c` 109, `ast.h` 973, and
  `ast_types.h` 272. No production `.c` or `.h` owner remains above the
  1,000 LOC hard risk line.
- Local gates: `make test-parser pgy`, `make test-semantic` (2357/0),
  `make semantic-tu-size-test-smoke production-header-size-test-smoke
  inc-sentinel-test-smoke documentation-quality-test-smoke
  beta-readiness-checklist-test-smoke`, and
  `make runtime-frontier-contract-test-smoke`.
- Next owner-split queue is the 600-1,000 LOC band, not 1,000+ production `.c`
  cleanup: `parser.c`, `parser_domain.c`, selected semantic owners, LLVM
  domain/frontier owners, `slot_manager.c`, and the AST public header split.

## UTF-8 Progress Note - 2026-04-28 - LLVM Backend Type Map Owner Split

- `src/codegen/llvm_backend.c` no longer mixes context lifecycle/type-layout
  bootstrap with AST/Pergyra type-name rendering and LLVM type resolution.
- Type rendering, generic container resolution, `pergyra_type_to_llvm`,
  `ast_type_to_llvm`, and early forward-declare eligibility moved to
  `src/codegen/llvm_backend_type_map.c`.
- `llvm_backend.c` is now 379 LOC and is a context lifecycle / backend entry
  owner. `llvm_backend_type_map.c` is 638 LOC, so it is below the 1,000 LOC
  hard cap but remains in the 600 LOC split-review band.
- Local gates: `make pgy` and `make llvm-test-smoke` remain green. This
  removes `llvm_backend.c` from the leading owner queue and keeps the next LLVM
  priority on domain frontier/parity owners.

## UTF-8 Progress Note - 2026-04-28 - LLVM Zone Frontier State Owner Split

- `src/codegen/llvm_domain_zone_sync.c` no longer owns all bounded-frontier
  bookkeeping inside the main zone sync emitter.
- Previous-state allocation, previous-state snapshotting, state/layer reset,
  and frontier-continue change detection moved to
  `src/codegen/llvm_domain_zone_frontier_state.c`, with declarations in
  `src/codegen/llvm_domain_zone_sync_intenal.h`.
- `llvm_domain_zone_sync.c` is now 776 LOC and remains the zone propagation
  action/maintain/link/unlink orchestration owner. The new frontier-state owner
  is 276 LOC.
- Local gates: `make pgy`, `make runtime-frontier-contract-test-smoke`, and
  `make llvm-test-smoke` remain green. This keeps runtime propagation frontier
  evidence tied to a named LLVM owner instead of one monolithic sync function.

## UTF-8 Progress Note - 2026-04-28 - Stdlib Builtin Semantic Owner Split

- `src/semantic/type_checker_builtins_stdlib_body.c` is no longer a 1,000+ LOC
  owner. Scalar/string/math builtin checks moved to
  `src/semantic/type_checker_builtins_stdlib_scalar.c`, and `HashMap` builtin
  checks moved to `src/semantic/type_checker_builtins_stdlib_map.c`.
- The dispatcher body is now 834 LOC and stays below the hard cap while the
  new owners stay small (`scalar` 182 LOC, `map` 147 LOC).
- Local gate: `make test-semantic pgy` remains green at 2357/0. This removed
  the stdlib builtin dispatcher from the 1,000+ production `.c` owner queue.

## UTF-8 Progress Note - 2026-04-28 - Zone Declaration Authority Owner Split

- `src/semantic/type_checker_zone_decl.c` is no longer a 1,000+ LOC owner.
  Zone authority ability validation, duplicate authority diagnostics,
  layer-slot type validation, and relation/effect pool beta rejects moved to
  `src/semantic/type_checker_zone_decl_authority.c`.
- `type_checker_zone_decl.c` is now 929 LOC and stays focused on
  lifecycle/state rule validation; the new authority/layer owner is 180 LOC.
- Local gate: `make test-semantic pgy` remains green at 2357/0. This removed
  the zone declaration validator from the 1,000+ production `.c` owner queue.

## UTF-8 Progress Note - 2026-04-28 - Intent Helper Owner Split

- `src/semantic/type_checker_intent_helpers.c` is no longer a 1,000+ LOC owner.
  Role require-field validation plus intent transfer/zone-binding derivation
  moved to `src/semantic/type_checker_intent_role_fields.c`, and intent-clause
  control-transfer rejection moved to
  `src/semantic/type_checker_intent_control.c`.
- `type_checker_intent_helpers.c` is now 883 LOC. The new role/transfer owner
  is 544 LOC and the control-transfer owner is 136 LOC, so both remain below
  the 600 LOC split-review threshold.
- Local gates: `make test-semantic pgy` and
  `make semantic-tu-size-test-smoke production-header-size-test-smoke
  inc-sentinel-test-smoke`. The remaining 1,000+ production `.c` owners are
  now `ast.c`, `ast_print.c`, `parser_domain.c`, and
  `type_checker_decls_domain_helpers.c`.

## UTF-8 Progress Note - 2026-04-27 - CI Documentation Gate Portability

- `documentation-quality-test-smoke` no longer depends on Python. The gate is
  now a shell-only UTF/documentation/async-surface checker using `grep`, `find`,
  and optional `iconv` when present.
- This keeps Windows CI from requiring a Python runtime solely for
  documentation wording checks, while preserving the same beta surface checks:
  required docs/examples exist, docs/examples avoid replacement characters,
  `TODO.md` has the readable UTF-8 Korean title, async docs use named `spawn`
  as the beta-stable task creation surface, executable examples do not use
  capture-bearing anonymous `async { ... }`, and AIR/RemoteFuture wording stays
  pinned.
- The old Python heredoc path also carried a mojibake-sensitive string literal,
  so removing it eliminates a hidden syntax-failure path that was masked on
  Python-missing CI runners.
- `runtime-frontier-contract-test-smoke` is also shell-only now. It preserves
  the same bounded frontier C/LLVM/source-of-truth checks, requires the
  dedicated `runtime-frontier-policy-test-smoke` arithmetic gate to stay wired,
  and keeps whitespace-normalized doc terms without requiring Python on CI
  runners.
- `beta-readiness-checklist-test-smoke` is shell-only now as well. The gate
  still checks the stable subset docs, stdlib freeze, module resolver contract,
  unicode policy, test-suite freeze, observability schema, memory/concurrency
  model, async positioning, Pin/Lease diagnostics, ABI ownership shape, CI
  matrix, Makefile support matrix, and README support wording, but no longer
  requires Python on Windows/macOS/Linux CI.
- `formal-semantics-test-smoke` is shell-only for proof-pack contract checks.
  It still runs `coqc docs/semantics/proofs/SlotCalculus.v` when Coq is
  installed, but missing Python can no longer block the formal semantics gate.
- `inc-sentinel-test-smoke` is shell-only now. The `.inc` ban, `.cases.h`
  include ownership, empty-fragment rejection, and orphan-fragment rejection are
  enforced with POSIX shell, `find`, `grep`, `sed`, and `realpath`.

## UTF-8 Progress Note - 2026-04-27 - AIR Global Verification Layer

- AIR now exposes `air_verify(...)` as the global validation entry point. It
  validates AIR inventory shape, authority participant shape, and evidence
  provenance before computing drift/evidence failures.
- `air_check_drift(...)` remains as a compatibility wrapper over
  `air_verify(...)`; new compiler/docs language should describe AIR as the
  verification layer, not just a drift helper.
- `src/test_air.c` now covers invalid boundary inventory rejection and the
  wrapper compatibility path. Local gate: `make test-air air-drift-test-smoke`.
- The verification pass now has a real owner TU, `src/compiler/air_verify.c`,
  so `src/compiler/air.c` stays focused on AIR synthesis and below the 600 LOC
  split-review threshold.
- AIR inventory validation now rejects missing backing arrays for non-zero
  intent/boundary/drift counts and boundary step-index mismatch before drift
  recomputation. `src/test_air.c` covers both cases.
- AIR inventory validation now also rejects empty intent owner/step names, empty
  boundary owner/source names, boundary-owner mismatch against the referenced
  intent owner, and invalid boundary sync-class shape before drift computation.
  `src/test_air.c` covers owner mismatch and world-boundary sync-shape mismatch.
- AIR drift inventory validation now rejects stale placeholder drift nodes,
  invalid intent/boundary references, and empty drift messages before
  recomputation. `src/test_air.c` covers invalid drift inventory.
- AIR evidence validation now rejects authority evidence without boundary
  evidence and authority evidence on non-authority boundaries, keeping authority
  provenance as a layered proof contract.
- AIR synthesis regression coverage now includes the stable execution boundary
  set (`parallel`, `async`, `channel-send`, `channel-recv`, `select`) in
  addition to spawn and IO boundaries.
- AIR invariant failures now use `PGY_AIR_INVARIANT_INVALID` with
  `air:invariant:invalid` and `report-compiler-bug`, so compiler graph
  corruption is not conflated with user-facing intent boundary drift.
- Systems-language identity is now beta-gated through
  `docs/19_design_philosophy.md`, `docs/100_beta_readiness_checklist.md`, and
  `docs/107_beta_stable_subset.md`: Pergyra is a systems language with domain
  extensions, and raw escape / optional runtime / C FFI ABI stability /
  compile-time determinism are non-negotiable substrate obligations.
- `make codegen-determinism-test-smoke` now emits representative frozen
  backend fixtures twice through C and LLVM and compares normalized generated
  artifacts. This is the initial compile-time determinism gate; remaining work
  is expanding it to the full frozen backend fixture set.
- `--runtime=none` is now parsed and beta-gated through
  `PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED`. Runtime-dependent surfaces are
  explicitly rejected, and pure sources are blocked at the freestanding
  lowering gap so the compiler cannot silently link the default runtime while
  claiming no-runtime semantics. Remaining systems-language blocker:
  implement verified freestanding C/LLVM lowering plus system-tier raw pointer
  escape.
- `SlotRawPointer(...)` is now reserved and explicitly rejected through
  `PGY_SEM_RAW_ESCAPE_UNSTABLE`; `unsafe { ... }` remains a lexical marker
  only. This blocks marketing-vs-implementation drift until raw pointer /
  inline-asm / MMIO escape has ABI lowering and diagnostics.

## UTF-8 Progress Note - 2026-04-27 - Slot Security Owner Split

- `slot_security.c` no longer owns platform fingerprint helpers or memory
  primitive fallbacks directly. `slot_security_platform.c` owns Windows/Linux
  hardware fingerprint retrieval, and `slot_security_memory.c` owns secure
  memory lock/unlock/wipe, constant-time compare, memory barrier, and timestamp.
- This keeps token/context/audit orchestration focused while preserving the
  public `slot_security.h` ABI. Follow-up split: `slot_security_crypto.c` owns
  AES/HMAC token encryption and `slot_security_sealed_payload.c` owns sealed
  payload obfuscation, MAC verification, and shadow recovery.
- `slot_security.c` is now below the 1,000 LOC hard cap at 794 LOC. Local gates
  rerun: `make test-security test-abi runtime-abi-lifetime-test-smoke
  backend-inc-size-test-smoke production-header-size-test-smoke
  documentation-quality-test-smoke beta-readiness-checklist-test-smoke`.
- Slot manager security monitoring is also split: `slot_manager_security_stats.c`
  owns security event logging, anomaly detection, and security stats printing.
  `slot_manager_scope.c` owns secure scope lifecycle plus the high-level
  `pergyra_*` secure slot wrappers. `slot_manager.c` drops to 1,329 LOC while
  keeping claim/read/write/release, pin/lease, token validation, and secure slot
  primitives in one owner.

## UTF-8 Progress Note - 2026-04-27 - Semantic Owner TU Size Closure

- `type_checker_helpers_late.c` tripped `semantic-tu-size-test-smoke` at 1,031
  LOC after the late helper migration. The active slot view boundary diagnostic
  now has a named owner TU, `type_checker_slot_view_boundary.c`.
- The split keeps `type_checker_helpers_late.c` focused on late call-path and
  borrowed-boundary argument validation at 974 LOC, while the new boundary
  owner carries the pin/await diagnostic wording and article helper at 66 LOC.
- `make semantic-tu-size-test-smoke semantic-core-shape-test-smoke` and
  `make test-semantic` are green after adding the new TU to `SEMANTIC_SOURCES`.

## UTF-8 Progress Note - 2026-04-27 - Runtime Slot Utility Owner Split

- `slot_manager.c` no longer owns the public type-tag/hash/CAS/memory-barrier
  utility family. Those functions moved to `slot_type_utils.c`, leaving
  `slot_manager.c` focused on table lifecycle, pin/lease, secure slot, scope,
  TTL, and statistics behavior.
- This is a small owner-boundary cleanup rather than a semantic change:
  `SlotHandle`, `SlotEntry`, and runtime ABI layout remain unchanged.
- `make test-security test-abi runtime-abi-lifetime-test-smoke` and the
  include/header gates are green after adding the new runtime TU to
  `RUNTIME_SOURCES`.

## UTF-8 Progress Note - 2026-04-27 - Non-Pin Handle Expiration Model

- Slot pinning is not the general stale-handle answer. Pin/Lease only keeps a
  slot live for lexical repeated access. Non-pin stale-handle cases must be
  handled by a layered contract: arena lane checks, CFG/body dataflow,
  zone/world channel-only crossing, token transport rejection, and runtime
  generation/token validation.
- `docs/118_slot_model_rigor_audit.md` now records the stale-handle scenario
  matrix: function escape, long-lived collection/field storage, async/spawn
  capture, channel/world handoff, and copied-handle release divergence.
- First-class Zone-Bound Handle typing is now a beta-freeze decision item. If
  `SlotHandle<T> in Zone` or `handle@zone` enters beta, the compiler must add
  zone-scope <= handle-scope facts and diagnostics. If it does not enter beta,
  the stable behavior remains conservative `BORROW_TRACKED` / anchored-handle
  rejection plus runtime generation/token hard-fail.

## UTF-8 Progress Note - 2026-04-27 - Slot Pin ABA Wrap Guard

- Slot pin audit found a concrete wording/implementation seam: the runtime ABI
  is not a 64-bit generation handle. It currently uses a 32-bit `slotId` plus a
  32-bit generation field, with fresh `slotId` assignment on each claim.
- To prevent ABA id reuse at the current ABI width, `SlotClaim` now tombstones
  the zero-id sentinel and the manager before `slotId` wrap, retuning
  `SLOT_ERROR_OUT_OF_MEMORY` rather than reusing an old id.
- `make test-security` now covers the zero-id guard, wrap guard, tampered-view
  generation unpin rejection, and double-unpin rejection as part of the Slot
  pin/lease runtime test. `docs/74_slot_pinning_caching.md`,
  `docs/semantics/08_slot_capability_calculus.md`, and
  `docs/118_slot_model_rigor_audit.md` now state this honestly instead of
  implying a 64-bit/tombstone ABI that was not implemented.
- `docs/semantics/proofs/SlotCalculus.v` now mirrors the implementation model:
  claim requires a fresh non-sentinel id, zero/wrap ids cannot be claimed,
  tampered pinned-view generations cannot unpin, and double-unpin is impossible
  in the small-step sketch.

## UTF-8 Progress Note - 2026-04-27 - Formal Semantics Proof Boundary

- Slot capability calculus is now part of the formal proof pack via
  `docs/semantics/08_slot_capability_calculus.md`.
- `docs/semantics/proofs/SlotCalculus.v` is intentionally labeled as a
  proof-sketch, not completed beta mechanized proof. It now models selected
  Slot capability invariants: stale handle read/write/release rejection,
  mode-specific issued-token read/write/pin/release requirements, unissued-token
  read/write/pin/release rejection, pinned-handle release rejection, and pin
  non-eviction.
- `make formal-semantics-test-smoke` now forbids overclaim terms in the Coq
  artifact and runs `coqc` when the local toolchain provides it.
- Linux GitHub Actions now installs `coq`, so the formal semantics smoke becomes
  an actual Coq type-check gate in CI instead of a local optional check.
- Local 2026-04-27 note: WSL smoke passed but skipped Coq type-check because
  `coqc` is not installed locally; CI remains the authoritative Coq gate.
- Runtime evidence for the Slot capability calculus was rechecked with
  `make test-security` (142/142 passed): generation guard coverage now includes stale-generation
  read/write/pin/release rejection, `SlotIsValid` false, zero-id sentinel and
  slot-id wrap tombstone before ABA reuse, tampered-view generation unpin
  rejection, and double-unpin rejection, plus
  release-while-pinned, scope-release-while-pinned, TTL cleanup skip while
  pinned, secure invalid token rejection, revoked-token rejection, concurrent
  secure write rejection, raw secure-slot release rejection, and
  release-after-unpin.
- `runtime-panic-abi-test-smoke` now covers forged zero-token read/write/release
  rejection for inline C runtime and exported C/LLVM-linkable secure-slot
  entrypoints. SecureSlot token ABI is now build-mode stable: inline C,
  exported runtime, and LLVM-linkable runtime use the same `PgyToken<T>` layout
  with read/write capability bits, and no-`PGY_SAFE_SLOTS` invalid-token /
  released-slot secure paths remain hard-fail checked. The old release-mode
  SecureSlot macro has been removed so future inline ABI drift is blocked.
  `pgy_abi_spec.h` now includes debug/release SecureSlot layout rows for all
  stable primitive payloads (`Int`, `Long`, `Float`, `Double`, `Bool`,
  `String`), and `make test-abi` checks runtime size/token offsets against the
  spec.
  Authority-token mismatch is now a real runtime contract surface:
  `authority-token-mismatch` code/reason, queryable snapshot state, `make
  test-security` direct coverage, `authority_failure_abi` C/LLVM ABI coverage,
  and `authority_failure_surface` backend-compare coverage. The remaining
  secure/authority invariant parity work is richer domain-boundary denial.
  Unsupported authority-token transport is now explicitly rejected on the
  current beta transport surfaces: blocking channel send/receive,
  non-blocking/timeout channel helpers, channel close, cancellation payloads,
  and direct named `spawn` boundaries.
- This keeps the beta proof line honest: theorem statements and regression
  evidence are required now; completed machine-checked proof remains a separate
  hardening gate until CI type-checks it.

## UTF-8 Progress Note - 2026-04-26 - DAG Metadata Materialization Tightening

- Non-generic nominal class type references now materialize through
  `semantic_type_resolution_lookup_or_materialize(...)` metadata instead of
  falling through to the central recursive resolver.
- Generic class references with explicit/default type parameters are
  deliberately excluded from this shortcut so default type argument resolution
  and generic mismatch provenance remain owned by the generic contract path.
- Intermediate DAG smoke stats before the follow-up tightening:
  `graph-backed skips=3137 metadata_entries=3248
  metadata_owned=244 metadata_hits=4724 materializer_fallbacks=1601
  metadata_unresolved_named=1594 metadata_unresolved_generic_named=7
  metadata_unresolved_compound=0 metadata_unresolved_other=0 stage_materialize_alias=83
  stage_materialize_non_alias=0 alias_materialized=5 alias_diagnostic_unresolved=78
  alias_diagnostic_resolved=0 alias_diagnostic_cycle_unresolved=78`.
- `type_resolution_dag_smoke.sh` now gates the tighter beta line:
  `metadata_entries>=3000`, `metadata_hits>=4500`, `metadata_owned>=200`, and
  `materializer_fallbacks<=1601`, with unresolved audit family accounting required to
  sum exactly to the total fallback count.
- That slice moved the remaining DAG closure mostly to named-symbol
  materialization (`1594/1601` fallback events), not compound type
  construction. The next target is to split
  imported/non-class nominal, alias-diagnostic, and visibility-sensitive named
  references instead of widening the generic shortcut.
- Follow-up tightening: known non-class scope symbols now materialize through
  metadata using the same `scope-type lookup` contract as `resolve_named_type`.
  Current stats are `metadata_entries=3346 metadata_hits=4935
  materializer_fallbacks=1296 metadata_unresolved_named=1289
  metadata_unresolved_generic_named=7 metadata_named_builtin_shell=2
  metadata_named_generic_class=0 metadata_named_alias=1281
  metadata_named_non_class_symbol=0 metadata_named_missing_symbol=6`.
  The DAG smoke gate now requires `metadata_entries>=3300`,
  `metadata_hits>=4900`, and `materializer_fallbacks<=1296`.
- Verified locally: `make type-resolution-dag-test-smoke` and
  `make type-resolution-resolver-inventory-test-smoke`.
- 2026-04-27 tightening: alias chains now short-circuit in the metadata
  materializer when the chain resolves, and alias cycles are detected in the
  metadata path before falling through to recursive materialization. This keeps
  cycle diagnostics/provenance alive while removing repeated alias fallback
  chun. Current stats are `metadata_entries=3346 metadata_hits=4935
  materializer_fallbacks=15 metadata_unresolved_named=8
  metadata_unresolved_generic_named=7 metadata_unresolved_compound=0
  metadata_unresolved_other=0 metadata_named_builtin_shell=2
  metadata_named_generic_class=0 metadata_named_alias=0
  metadata_named_non_class_symbol=0 metadata_named_missing_symbol=6`.
  The DAG smoke gate now caps `materializer_fallbacks<=15` and requires
  `metadata_named_alias==0`. It also requires the fallback total to equal the
  diagnostic-only family sum (`builtin_shell + generic_named + missing_symbol`)
  and keeps compound/other/generic-class/non-class-symbol fallback at zero. The
  remaining fallback set is diagnostic-only: bare generic shells, invalid
  generic-named forms, and missing symbol negative cases.
- 2026-04-28 tightening: stable constructed type arguments now use a shared
  metadata-only resolver so Slot/collection/Result shell materialization does
  not duplicate generic argument lookup logic, and already-proven invalid stable
  constructed shells stop before the central recursive materializer. Explicit
  user generic class specializations also materialize through DAG metadata while
  preserving where/default validation provenance. At this intermediate point,
  no-arg default generic class specialization intentionally remained on the
  diagnostic path because the validator preserved instantiated provenance such
  as `Box<Item>`.
  Current stats are `metadata_entries=3354 metadata_hits=4950
  metadata_owned=249 materializer_fallbacks=10 metadata_unresolved_named=8
  metadata_unresolved_generic_named=2 metadata_unresolved_compound=0
  metadata_unresolved_other=0 metadata_named_builtin_shell=2
  metadata_named_generic_class=0 metadata_named_alias=0
  metadata_named_non_class_symbol=0 metadata_named_missing_symbol=6`.
  The DAG smoke gate now caps `materializer_fallbacks<=10` and
  `metadata_unresolved_generic_named<=2`.
- 2026-04-28 follow-up tightening: nested stable constructed arguments now
  materialize before fallback, so `Channel<Slot<T>>` / similar wrapper chains no
  longer hit the recursive resolver. Current stats are
  `metadata_entries=3356 metadata_hits=4950 metadata_owned=251
  materializer_fallbacks=8 metadata_unresolved_named=8
  metadata_unresolved_generic_named=0 metadata_unresolved_compound=0
  metadata_unresolved_other=0 metadata_named_builtin_shell=2
  metadata_named_generic_class=0 metadata_named_alias=0
  metadata_named_non_class_symbol=0 metadata_named_missing_symbol=6`.
  The DAG smoke gate now caps `materializer_fallbacks<=8` and requires
  `metadata_unresolved_generic_named==0`. The remaining fallback set is only bare
  `Box`/constructor-shell provenance plus missing-symbol negative diagnostics.
- 2026-04-28 default-specialization tightening: class where diagnostics now
  format the actual path from effective type arguments, so no-arg default
  generic class specializations keep provenance such as `Box<Item>` while still
  materializing through DAG metadata. Current stats are
  `metadata_entries=3358 metadata_hits=6744 metadata_owned=253
  materializer_fallbacks=6 metadata_unresolved_named=6
  metadata_unresolved_generic_named=0 metadata_unresolved_compound=0
  metadata_unresolved_other=0 metadata_named_builtin_shell=0
  metadata_named_generic_class=0 metadata_named_alias=0
  metadata_named_non_class_symbol=0 metadata_named_missing_symbol=6`.
  The DAG smoke gate now caps `materializer_fallbacks<=6` and requires
  `metadata_named_builtin_shell==0`; the only remaining unresolved audit family is
  missing-symbol negative diagnostics.
- 2026-04-28 final materializer tightening: bare unknown named types now emit
  `PGY_SEM_UNKNOWN_TYPE` directly from the metadata path instead of entering the
  recursive resolver. Current stats are `metadata_entries=3358
  metadata_hits=6744 metadata_owned=253 materializer_fallbacks=0
  metadata_unresolved_named=0 metadata_unresolved_generic_named=0
  metadata_unresolved_compound=0 metadata_unresolved_other=0
  metadata_named_builtin_shell=0 metadata_named_generic_class=0
  metadata_named_alias=0 metadata_named_non_class_symbol=0
  metadata_named_missing_symbol=0`. The DAG smoke gate now requires central
  metadata materializer fallback to stay exactly `0`.

## UTF-8 Progress Note - 2026-04-26 - Overall Beta Audit Follow-up

- Tooling conformance is green locally with `make tooling-conformance-test-smoke`.
  The formatter smoke is invoked through `bash`, so Linux execute-bit drift on
  mounted worktrees should not reproduce the old `fmt_smoke.sh Permission
  denied` failure.
- Production runtime/codegen/compiler/semantic `.inc` debt is closed for beta:
  production `.inc` count is 0, and only `src/tests/**/*.inc` fixtures remain.
  The new structural policy is stricter than the old include cleanup:
  production `.c` / private owner `.h` files above 600 LOC require split review
  and a named follow-up seam; 1,000 LOC is only the hard stop / risk line.
  `production-header-size-test-smoke` now caps every production owner header at
  1,000 LOC with no per-file temporary exception; `llvm_intenal.h` was split
  by moving declaration inventory helpers behind `llvm_inventory_intenal.h`,
  and the LLVM inventory helper family is now split into lookup,
  host-method metadata, and domain/routine inventory owners.
  LLVM statement parallel/async/select lowering now lives in
  `llvm_stmt_parallel_async.c`, reducing `llvm_stmt.c` to 3,078 LOC with
  backend compare still green. LLVM domain method/provenance helpers now live
  in `llvm_domain_method_helpers.c`, reducing `llvm_domain.c` to 3,340 LOC
  while keeping backend compare green. LLVM world sync lowering now lives in
  `llvm_domain_world_sync.c`, and LLVM zone sync lowering now lives in
  `llvm_domain_zone_sync.c`; `llvm_domain.c` is down to 1,649 LOC. The former
  `llvm_domain_core_helpers.h` mega-header was split into focused owner
  headers for role lookup, decl parts, projection count/value/sync body, and
  zone binding so the build stays waning-clean instead of hiding unused static
  helpers. Build, header/inc gates, docs gates, and backend compare remain
  green. LLVM statement ownership is now split into focused owners:
  `llvm_stmt_type_infer.c`, `llvm_stmt_let_helpers.c`,
  `llvm_stmt_let_with.c`, `llvm_stmt_with.c`, `llvm_stmt_loop_match.c`, and
  `llvm_stmt_parallel_async.c`, plus `llvm_stmt_zone_action.c` for
  zone-action effect propagation and `llvm_stmt_type_render.c` for generic
  type-argument rendering. `llvm_stmt.c` is down to 573 LOC, the dispatcher
  owner is below the 600 LOC split-review threshold, and backend smoke remains
  green.
  Parser orchestration/declaration debt is also below the 1,000 LOC hard cap:
  `parser.c` is 977 LOC and `parser_decl.c` is 887 LOC after extracting
  doc-comment, export, enum, pin-block, lexical-zone, declaration-clause, and
  declaration-lookahead owners. The next lean cleanup target is large real TUs,
  starting with parser AST / AST-print / parser-domain owners, the largest
  semantic domain helpers, and remaining body-loop subowners inside
  `llvm_emit_intent_decl`.
- Lean debt-slice follow-up: C backend type-alias declaration emission now has
  a real owner in `src/codegen/transpiler_type_alias.c`; the old body was
  removed from `transpiler_emitters_base_b_part_c.inc`. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`.
- Lean debt-slice follow-up: C backend type-requirement checks now have a real
  owner in `src/codegen/transpiler_type_require.c`; the old
  `src/codegen/transpiler_emitters_type_require.inc` include body was deleted,
  reducing the source `.inc` cap to 159 and keeping
  `transpiler_emitters_base_a_part_a.inc` at 905 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: C backend exten declaration emission now has a
  real owner in `src/codegen/transpiler_exten.c`; `emit_exten_block(...)` was
  removed from `transpiler_emitters_base_b_part_b.inc`, reducing that near-cap
  include body from 998 LOC to 957 LOC. `tests/inc_sentinel_smoke.sh` now uses
  the current 159 source-`.inc` cap by default. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: C backend type declarator rendering now has a real
  owner in `src/codegen/transpiler_type_declarator.c`; event-handler
  declarators, function pointer declarators, and function signatures were
  removed from `transpiler_helpers_core_b_part_c.inc`, reducing it from 992 LOC
  to 849 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: C backend LogBanner normalization now has a real
  owner in `src/codegen/transpiler_log_normalize.c`; multiline indentation
  normalization was removed from `transpiler_expr_emitters_part_a.inc`,
  reducing it from 991 LOC to 878 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: generated-C runtime intent exit cleanup now has a
  private inline owner in `src/runtime/pgy_runtime_intent_exit.h`;
  `pgy_intent_exit_export(...)` keeps the same inline ABI name, while
  `pgy_runtime_part_ba_part_b.inc` drops from 996 LOC to 894 LOC. Local gate
  used: `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: generated-C DeviceSlot/SecureSlot macro bodies now
  have a private inline owner in `src/runtime/pgy_runtime_slot_macros.h`;
  built-in instantiation remains in `pgy_runtime_part_ba_part_c.inc`, which
  drops from 996 LOC to 808 LOC. Local gate used:
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: generated-C intent last-history step accessors now
  have a private inline owner in `src/runtime/pgy_runtime_intent_history.h`;
  `pgy_runtime_part_ba_part_a.inc` drops from 989 LOC to 867 LOC while the
  borrowed string ABI remains guarded. `runtime_abi_lifetime_smoke.sh` now reads
  the private inline headers that participate in the generated-C runtime family.
  Local gate used: `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: generated-C intent last/active borrowed exports now
  have a private inline owner in
  `src/runtime/pgy_runtime_intent_active_exports.h`; the registry/state half
  remains in `pgy_runtime_part_ba_part_a.inc`, which drops from 867 LOC to
  558 LOC. `runtime_abi_lifetime_smoke.sh` now tracks active and recent export
  owners separately so future movement cannot hide behind concatenated runtime
  text. Local gate used: `make runtime-abi-lifetime-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke`, plus `make -B pgy
  runtime-panic-codegen-test-smoke runtime-panic-abi-test-smoke test-abi`.
- Lean debt-slice follow-up: LLVM-linkable intent borrowed exports now have a
  matching private owner in `src/runtime/pgy_runtime_lib_intent_exports.h`;
  `pgy_runtime_lib_part_b_part_c.inc` drops from 852 LOC to 315 LOC while
  keeping `intent_active`, `intent_recent`, and `intent_failure` ABI pipeline
  cases green on C and LLVM. This keeps generated-C inline and LLVM-linkable
  runtime export ownership symmetric instead of letting `part_b_part_c.inc`
  carry mixed intent-observability and slot-operation bodies.
- Lean debt-slice follow-up: LLVM method-call projection sync helpers now have
  a private owner in `src/codegen/llvm_expr_call_projection_sync.h`;
  `llvm_expr_call_methods_part_a.inc` drops from 880 LOC to 671 LOC while the
  world/zone projection sync call sites keep the same include order. Local gate
  used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus
  targeted backend compare for `world_embedded_branch_projection_visibility`,
  `world_embedded_action_frontier`, `world_embedded_action_pool_frontier`, and
  `world_zone_projection_visibility`.
- Lean debt-slice follow-up: LLVM method-call domain action sync and
  slice/member-call helpers now have a private owner in
  `src/codegen/llvm_expr_call_methods_domain_slice.h`; the remaining
  `llvm_expr_call_methods_part_a.inc` body is removed while `llvm_expr.c`
  include order remains stable. The production source `.inc` inventory is now
  92 files / 28,467 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM call dispatch now has a private owner in
  `src/codegen/llvm_expr_call_dispatch.h`; the former
  `llvm_expr_calls_main.inc` body is removed while the call-family shim order
  remains stable. The production source `.inc` inventory is now 91 files /
  27,842 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM expression host/self, projection binding,
  spawn expression, operator suffix, enum lookup, and number/string literal
  helpers now have a private owner in
  `src/codegen/llvm_expr_host_spawn_literal_helpers.h`; the former
  `llvm_expr_helpers_part_b.inc` body is removed while `llvm_expr.c` helper
  include order remains stable. The production source `.inc` inventory is now
  90 files / 27,221 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend role method emission, ability/vtable
  emission, hidden provenance helpers, and role operator aliases now have a
  private owner in `src/codegen/transpiler_domain_role_ability_emit.h`; the
  former `transpiler_domain_role_part_a.inc` body is removed while the
  domain-role shim order remains stable. The production source `.inc` inventory
  is now 89 files / 26,601 LOC. Local gate to use for this slice:
  `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM expression boundary call argument helpers,
  projection field helpers, world/zone lookup helpers, and host-class lookup
  helpers now have a private owner in
  `src/codegen/llvm_expr_boundary_projection_helpers.h`; the former
  `llvm_expr_helpers_part_a.inc` body is removed while `llvm_expr.c` helper
  include order remains stable. The production source `.inc` inventory is now
  88 files / 25,996 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend MIR routine lookup, active SSA name
  resolution/rendering, token-local filtering, and local type-name lookup now
  have a private owner in `src/codegen/transpiler_mir_ssa_names.h`; the former
  `transpiler_emitters_mir_inventory_ssa_names.inc` body is removed while the
  MIR inventory/SSA shim order remains stable. The production source `.inc`
  inventory is now 87 files / 25,395 LOC. Local gate to use for this slice:
  `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend primitive, slot/channel, constructed
  generic, and local type-name rendering now have a private owner in
  `src/codegen/transpiler_type_mapping_helpers.h`; the former
  `transpiler_helpers_core_types.inc` body is removed while the helper-core shim
  order remains stable. The production source `.inc` inventory is now 86 files /
  24,796 LOC. Local gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend world sync declaration, select lowering,
  and event declaration/subscription lowering now have a private owner in
  `src/codegen/transpiler_world_select_event_emit.h`; the former
  `transpiler_domain_role_part_d.inc` body is removed while the domain-role shim
  order remains stable. The production source `.inc` inventory is now 85 files /
  24,198 LOC. Local gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM expression assignment, member lvalue/member
  access, projection invalidation, and embedded world projection assignment sync
  now have a private owner in
  `src/codegen/llvm_expr_assignment_member_projection.h`; the former
  `llvm_expr_values.inc` body is removed while `llvm_expr.c` include order
  remains stable. The production source `.inc` inventory is now 84 files /
  23,617 LOC. Local gate to use for this slice: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM-linkable runtime authority rejection state,
  checked arithmetic exports, panic invariant export, and file-path
  normalization helpers now have a private owner in
  `src/runtime/pgy_runtime_lib_authority_file_core.h`; the former
  `pgy_runtime_lib_part_a.inc` body is removed while `pgy_runtime_lib.c` include
  order remains stable. The production source `.inc` inventory is now 83 files /
  23,031 LOC. Local gate to use for this slice: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-abi-test-smoke test-abi
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM-linkable raw set tail exports, intent
  active/recent registry helpers, intent trace mutation, and MIR trace hooks now
  have a private owner in
  `src/runtime/pgy_runtime_lib_set_intent_trace_exports.h`; the former
  `pgy_runtime_lib_part_b_part_b.inc` body is removed while `pgy_runtime_lib.c`
  include order remains stable. The production source `.inc` inventory is now
  82 files / 22,449 LOC. Local gate to use for this slice: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-abi-test-smoke test-abi
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: RIR flow semantic flags, state merge rules, and
  HIR CFG enrichment now have a private owner in `src/compiler/rir_flow.h`; the
  former `rir_flow.inc` body is removed while `rir.c` include order remains
  stable. The production source `.inc` inventory is now 81 files / 21,877 LOC.
  Local gate to use for this slice: `make -B pgy type-resolution-dag-test-smoke
  air-drift-test-smoke cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend MIR SSA identifier contract helpers now
  have a private owner in `src/codegen/transpiler_mir_ssa_contract.h`;
  `transpiler_emitters_base_a_part_d.inc` drops from 849 LOC to 677 LOC. Local
  gate used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  test-transpile`.
- Lean debt-slice follow-up: C backend MIR emission contract/resource-hook
  helpers now have a private owner in
  `src/codegen/transpiler_mir_emission_contract.h`; the remaining
  `transpiler_emitters_base_a_part_d.inc` body is removed while the base-A shim
  keeps include order stable. The production source `.inc` inventory is now
  95 files / 30,368 LOC. Local gate to use for this slice:
  `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: RIR lowering/enrichment now has a private owner in
  `src/compiler/rir_builder.h`; the former `rir_builder.inc` body is removed
  while `rir.c` keeps the flow -> build -> names -> validation include order.
  The production source `.inc` inventory is now 94 files / 29,733 LOC. Local
  gate to use for this slice: `make -B pgy type-resolution-dag-test-smoke
  air-drift-test-smoke cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: semantic function-body checking now has a private
  owner in `src/semantic/type_checker_program.h`; the former
  `type_checker_program.inc` body is removed while the top-level semantic TU
  include order remains stable. The production source `.inc` inventory is now
  93 files / 29,099 LOC. Local gate to use for this slice:
  `make -B pgy test-semantic semantic-core-shape-test-smoke
  cfg-body-dataflow-test-smoke type-resolution-dag-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend slot/device builtin expression emitters
  now have a private owner in `src/codegen/transpiler_slot_builtin_emit.h`;
  `transpiler_expr_emitters_part_a.inc` drops from 797 LOC to 531 LOC while
  preserving slot sugar, secure slot token, and runtime panic codegen smoke.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke test-transpile runtime-panic-codegen-test-smoke`.
- Lean debt-slice follow-up: C backend expression type inference now has a
  private owner in `src/codegen/transpiler_expr_type_infer.h`;
  `transpiler_helpers_core_b_part_c.inc` drops from 797 LOC to 296 LOC. This
  keeps generic/default-retun inference in the same include order while
  separating the expression-type owner from spawn/generic helper tails. Local
  gate used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  test-transpile`.
- Lean debt-slice follow-up: C backend statement dispatch now has a private
  owner in `src/codegen/transpiler_statement_dispatch.h`;
  `transpiler_emitters_base_b_part_c.inc` drops from 803 LOC to 546 LOC. This
  leaves `part_c` focused on block emission and intent helper tails instead of
  carrying the top-level statement switch. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile` plus
  targeted backend compare for `break_continue`, `parallel_channel_sum`, and
  `intent_header_interleaved`.
- Lean debt-slice follow-up: generated-C `HashMap<String>` and map-keys inline
  runtime now has a private owner in `src/runtime/pgy_runtime_map_string_inline.h`;
  `pgy_runtime_part_ba_part_d.inc` drops from 767 LOC to 377 LOC and is now
  focused on List/Set inline runtime. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-codegen-test-smoke test-abi`
  plus targeted backend compare for `map_get_string`, `map_keys`,
  `list_get_string`, `queue_pop_string`, and
  `intent_failure_observability_strings`.
- Lean debt-slice follow-up: C backend MIR function emission now has a private
  owner in `src/codegen/transpiler_mir_func_emit.h`;
  `transpiler_emitters_base_b_part_a.inc` drops from 766 LOC to 162 LOC. This
  keeps the MIR emit-state snapshot helpers in the original part while moving
  the large `emit_func_decl_from_mir_named(...)` body behind a named owner.
- Lean debt-slice follow-up: generated-C runtime array sort kenels and scalar
  std/log/math helpers now have private owners in
  `src/runtime/pgy_runtime_array_sort_inline.h` and
  `src/runtime/pgy_runtime_scalar_std_inline.h`;
  `pgy_runtime_part_ba_part_c.inc` drops from 759 LOC to 535 LOC and is now
  focused on built-in type instantiation plus HashMap core.
- Lean debt-slice follow-up: LLVM-linkable runtime core exports now have a
  private owner in `src/runtime/pgy_runtime_lib_core_exports.h`; logging,
  time/sleep, and `pgy_int_to_string(...)` moved out of
  `pgy_runtime_lib_part_b_part_a.inc`, reducing it from 986 LOC to 909 LOC.
  Local gate used: `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: C backend `let` destructuring lowering now has a
  private owner in `src/codegen/transpiler_destructure_emit.h`;
  `transpiler_emitters_base_b_part_c.inc` drops from 976 LOC to 873 LOC. Local
  gate used: `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  targeted backend compare for `destructure_array` and
  `destructure_tuple_retun`, plus touched path `git diff --check`.
- Lean debt-slice follow-up: generated-C queue macro and built-in queue
  implementations now have a private owner in
  `src/runtime/pgy_runtime_queue_inline.h`; `pgy_runtime_part_ba_part_e.inc`
  drops from 969 LOC to 773 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-panic-codegen-test-smoke runtime-abi-lifetime-test-smoke
  test-abi`, targeted backend compare for `queue_pop_string` and
  `parallel_channel_sum`, plus touched path `git diff --check`.
- Lean debt-slice follow-up: generated-C `HashMap<Int>` key adapters for
  `Int`/`Long`/`Bool` keys now have a private owner in
  `src/runtime/pgy_runtime_map_int_key_inline.h`; `pgy_runtime_part_ba_part_d.inc`
  drops from 963 LOC to 815 LOC. Local gate used: `make -B pgy`,
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke`, targeted backend compare for `map_keys` and
  `map_get_string`, plus touched path `git diff --check`.
- Lean debt-slice follow-up: LLVM-linkable primitive slot exports for
  `Slot<Double>`, `Slot<Bool>`, and `Slot<String>` now have a private owner in
  `src/runtime/pgy_runtime_lib_slot_exports.h`; `pgy_runtime_lib_part_b_part_d.inc`
  drops from 947 LOC to 790 LOC while exported ABI symbol names remain
  unchanged. Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-panic-abi-test-smoke
  runtime-panic-codegen-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- Lean debt-slice follow-up: LLVM-linkable standard string/conversion/math/random
  exports now have a private owner in `src/runtime/pgy_runtime_lib_std_exports.h`;
  `pgy_runtime_lib_part_b_part_e.inc` drops from 817 LOC to 761 LOC and now
  starts at the channel runtime section. `runtime_abi_lifetime_smoke.sh` now
  reads runtime-lib private owner headers so result-owned string checks follow
  the real include order. Local gate used: `make runtime-abi-lifetime-test-smoke
  test-abi backend-inc-size-test-smoke inc-sentinel-test-smoke`.
- Lean debt-slice follow-up: LLVM-linkable raw `List<T>` collection exports now
  have a private owner in `src/runtime/pgy_runtime_lib_list_raw_exports.h`;
  `pgy_runtime_lib_part_b_part_a.inc` drops from 909 LOC to 759 LOC and is now
  focused on raw queue/map exports. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- Lean debt-slice follow-up: MIR declaration-header inventory helpers now have
  a private owner in `src/compiler/mir_decl_headers.h`; `mir_public_part_a.inc`
  drops from 959 LOC to 789 LOC and now starts at `mir_lower(...)`. Local gate
  used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke air-drift-test-smoke test-abi`.
- Lean debt-slice follow-up: RIR public vocabulary name helpers now have a
  private owner in `src/compiler/rir_names.h`; `rir_public.inc` drops from
  911 LOC to 804 LOC while RIR validation/dump vocabulary remains unchanged.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke air-drift-test-smoke
  test-abi`.
- Lean debt-slice follow-up: C backend parallel capture analysis now has a
  private owner in `src/codegen/transpiler_parallel_capture.h`;
  `transpiler_emitters_base_b_part_b.inc` drops from 957 LOC to 730 LOC while
  parallel capture typing and slot capture behavior remain unchanged. Local
  gate used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  parallel-core-contract-test-smoke runtime-panic-codegen-test-smoke` plus
  targeted backend compare for `parallel_channel_sum`.
- Lean debt-slice follow-up: C backend stdlib call lowering now has a private
  owner in `src/codegen/transpiler_expr_stdlib_builtin.h`;
  `transpiler_expr_emitters_part_d.inc` drops from 946 LOC to 26 LOC while
  stdlib/string/collection call behavior remains unchanged. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke` plus targeted backend compare for
  `string_io`, `array_builtins`, `list_get_string`, and `map_get_string`.
- Lean debt-slice follow-up: C backend overlay/projection invalidation and
  zone-layer bind helpers now have a private owner in
  `src/codegen/transpiler_overlay_projection.h`; the old
  `transpiler_helpers_core_a_part_b.inc` include body was removed, lowering the
  source `.inc` count to 158. `runtime_frontier_contract_smoke.sh` now checks
  the real world frontier owner in `transpiler_domain_role_part_d.inc` instead
  of the adjacent zone frontier part. Local gate used:
  `make runtime-frontier-contract-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke` plus targeted backend compare for
  `world_embedded_branch_projection_visibility` and
  `world_embedded_action_frontier`.
- Lean debt-slice follow-up: C backend `let` declaration lowering now has a
  private owner in `src/codegen/transpiler_let_emit.h`;
  `transpiler_emitters_base_a_part_a.inc` drops from 905 LOC to 138 LOC while
  MIR inventory/SSA helper declarations remain in the original base-A part.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke test-transpile` plus targeted backend compare for
  `destructure_array`, `array_builtins`, and `map_keys`.
- Lean debt-slice follow-up: C backend MIR block statement emission now has a
  private owner in `src/codegen/transpiler_mir_block_emit.h`; the old
  `transpiler_emitters_base_a_part_c.inc` include body was removed. Source
  `.inc` total drops to 49,911 LOC, with only `transpiler_emitters_intent.inc`
  still above 900 LOC. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile
  type-resolution-dag-test-smoke air-drift-test-smoke` plus targeted backend
  compare for `destructure_array`, `destructure_tuple_retun`,
  `host_method_class_retun`, and `world_embedded_branch_projection_visibility`.
- Lean debt-slice follow-up: C backend intent declaration emission now has a
  private owner in `src/codegen/transpiler_intent_emit.h`; the old
  `transpiler_emitters_intent.inc` include body was removed. Source `.inc`
  total drops to 48,949 LOC, and no production `.inc` file remains above 900
  LOC. Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke test-transpile runtime-panic-codegen-test-smoke` plus
  targeted backend compare for `intent_authority_snapshot` and
  `intent_failure_observability_strings`.
- Lean debt-slice follow-up: generated-C runtime intent-recent accessors,
  panic helpers, and checked arithmetic exports now have a private owner in
  `src/runtime/pgy_runtime_panic_checked_inline.h`;
  `pgy_runtime_part_ba_part_b.inc` drops from 894 LOC to 705 LOC and the
  runtime ABI lifetime inventory reads the new header in generated-runtime
  include order. Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-panic-codegen-test-smoke
  runtime-panic-abi-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- Current highest-value implementation order is now:
  1. CFG/body dataflow source-of-truth for function/action/intent safety.
     - Latest closure slice: MIR cleanup block creation now consumes RIR policy
       ops, conservative semantics, flow-block summaries, and resource facts
       for rollback/invalidation decisions. The former intent-step AST
       invalidation scanner is removed and gated out by
       `cfg-body-dataflow-test-smoke`.
     - RIR flow owner split: `src/compiler/rir_flow_state.h` owns the
       resource-state merge lattice and helper predicates; `rir_flow.h` is down
       to 420 LOC and stays focused on HIR CFG enrichment / bounded dataflow
       iteration.
  2. DAG source-of-truth completion for named symbols, module contracts, and
     generic consumer paths.
  3. AIR strict-evidence negative expansion for transfer/world/boundary cases.
  4. Runtime frontier scheduler generalization beyond the already-covered
     bounded recompute slices.
  5. ABI ownership/pinning parity and diagnostic quality gate hardening.
  6. Cross-platform support matrix enforcement.
- The first removable blocker under that list is still owner debt that slows
  every P0/P1/DAG/AIR change. MIR ABI layout lookup now has a private owner in
  `src/compiler/mir_abi_layout.h`; `mir_public_part_b.inc` drops from 753 LOC
  to 420 LOC and now focuses on MIR validation/dump surfaces. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke air-drift-test-smoke test-abi`.
- CFG contract validation now has a private owner in
  `src/compiler/mir_cfg_contract_validate.h`; `mir_public_part_a.inc` drops
  from 743 LOC to 290 LOC and no longer mixes public MIR entry points with
  cleanup/rollback/invalidation graph contract checks. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- RIR validation now has a private owner in `src/compiler/rir_validation.h`;
  `rir_public.inc` drops from 741 LOC to 269 LOC and now keeps only
  destroy/dump public surfaces. This makes AIR/CFG evidence validation a named
  owner instead of a mixed public include body. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- C backend MIR intent inventory helpers now have a named owner in
  `src/codegen/transpiler_mir_inventory_intent.h`; the old
  `transpiler_emitters_mir_inventory_intent.inc` include body is gone and the
  existing SSA include-order shim now references the owner header directly.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke air-drift-test-smoke test-abi`.
- C backend call/spawn/channel expression emission now has a named owner in
  `src/codegen/transpiler_expr_call_spawn_emit.h`; the old
  `transpiler_expr_emitters_part_e.inc` body is gone and the expression emitter
  shim includes the owner header directly. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- C backend builtin-call dispatch now has a named owner in
  `src/codegen/transpiler_expr_builtin_dispatch.h`; the old
  `transpiler_expr_emitters_part_b.inc` body is gone and the expression emitter
  shim includes the owner header directly. This keeps builtin dispatch out of
  split `.inc` ownership without changing call lowering order. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- Semantic builtin-query checks now have named owners in
  `src/semantic/type_checker_builtins_query.c`,
  `src/semantic/type_checker_builtins_query_world.c`,
  `src/semantic/type_checker_builtins_query_channel.c`, and
  `src/semantic/type_checker_builtins_query_domain.c`. The corresponding
  query headers are declaration-only guards, so builtin query behavior no
  longer depends on include-order side effects.
- Semantic builtin nominal/type contract checks now have a named owner in
  `src/semantic/type_checker_builtins_nominal.c`; intent observability is split
  further into `src/semantic/type_checker_builtins_intent_observability.c`.
  `type_checker_builtins_nominal.h` is declaration-only while preserving
  `Rc`/`Weak`/`Box`/allocator and intent-observability builtin dispatch order.
- Slot analyzer escape handling moved to
  `src/semantic/slot_analyzer_escape.c`, leaving
  `src/semantic/slot_analyzer_summary.c` below the 600 LOC review threshold
  and focused on access/parameter summary behavior.
- Generated-C runtime pool/FSM/timer helpers now have a named owner in
  `src/runtime/pgy_runtime_pool_fsm_timer_inline.h`; `pgy_runtime_part_ba_part_e.inc`
  now starts at parallel/zone authority support instead of mixing object-pool,
  FSM, timer, cooldown, authority, result, and option helpers in one body.
  Runtime ABI lifetime inventory and compiler runtime-cache freshness track
  the new owner header directly.
- Semantic expression checking now has a named owner in
  `src/semantic/type_checker_expr.h`; the old `type_checker_expr.inc` body is
  gone and CFG body-dataflow smoke follows the new owner path.
- C backend function/class/control-flow emission now has a named owner in
  `src/codegen/transpiler_func_class_flow_emit.h`; the old
  `transpiler_emitters_base_b_part_b.inc` body is gone while preserving the
  base-B include order.
- Generated-C runtime Box/Arena/Allocator/Array/Rc/primitive-slot helpers now
  have a named owner in `src/runtime/pgy_runtime_memory_array_slot_inline.h`;
  the old `pgy_runtime_part_ba_part_b.inc` body is gone. Runtime panic contract,
  ABI lifetime inventory, and compiler runtime-cache freshness track the new
  owner header directly.
- Semantic relation/effect/projection helper logic now has a named owner in
  `src/semantic/type_checker_helpers_effects.h`; the old
  `type_checker_helpers_effects.inc` body is gone and CFG body-dataflow smoke
  tracks the new helper path.
- LLVM domain core helpers now have a named owner in
  `src/codegen/llvm_domain_core_helpers.h`; the old
  `llvm_domain_helpers_part_a.inc` body is gone and `llvm_domain.c` includes
  the owner header directly. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- LLVM-linkable runtime channel/qubit exports now have a named owner in
  `src/runtime/pgy_runtime_lib_channel_quantum_exports.h`; the old
  `pgy_runtime_lib_part_b_part_e.inc` body is gone and
  `runtime_abi_lifetime_smoke.sh` reads the new owner header in generated
  runtime include order. Local gate used: `make backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- LLVM-linkable raw Queue/Map/Set exports now have a named owner in
  `src/runtime/pgy_runtime_lib_raw_collection_exports.h`, and secure/device
  slot, array, file IO, and string helper exports now have a named owner in
  `src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h`. The old
  `pgy_runtime_lib_part_b_part_a.inc` and `pgy_runtime_lib_part_b_part_d.inc`
  bodies are gone. Runtime panic/lifetime smokes now check the new owner
  headers and compiler runtime cache freshness tracks them directly. Local
  gate used: `make backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-contract-test-smoke
  runtime-panic-codegen-test-smoke test-abi`.
- Rejected shortcut: using the alias symbol's already-materialized `sym->type`
  directly inside metadata alias lookup breaks module visibility and generic
  ability provenance tests. Alias DAG closure must preserve export/private
  provenance and effective generic-bound facts instead of trusting the symbol
  cache as the source of truth.
- Sprint process change: beta closure now uses a lean debt-slice loop. Pick one
  owner, complete the implementation slice, run the slice-local gate, and defer
  wider regression to the slice boundary. Full regression is still required
  before closure, but the inner loop must be implementation-first, not
  test-threshold-first.

## UTF-8 Progress Note - 2026-04-26 - DAG Owner Seam Centralization

- All owner-local type resolver seams now route through
  `semantic_type_resolution_lookup_or_materialize(...)` instead of owning direct
  fallback helper calls.
- The old `semantic_type_resolution_resolve_or_fallback(...)` helper is removed;
  `type-resolution-resolver-inventory-test-smoke` caps named fallback seams at 0
  and fails if new owner-local fallback users appear.
- Previous DAG smoke stats before nominal metadata materialization tightening:
  `graph-backed skips=3137 metadata_entries=2044
  metadata_owned=123 metadata_hits=3300 materializer_fallbacks=4135
  stage_materialize_alias=83 stage_materialize_non_alias=0 alias_materialized=5
  alias_diagnostic_unresolved=78 alias_diagnostic_resolved=0
  alias_diagnostic_cycle_unresolved=78`.
- This was not full DAG source-of-truth at the time. The central recursive
  fallback has since been removed; remaining closure is keeping imported
  ability/default/bound/module/nominal consumers and diagnostics aligned with
  graph/topo materialization rather than compatibility wording.
- Verified locally: `make type-resolution-resolver-inventory-test-smoke
  type-resolution-dag-test-smoke` and `make test-semantic`.

마지막 업데이트: 2026-04-25

## 현재 상태 냉정 평가 (2026-04-12 재정렬)

### 종합 판단: Late-Stage Alpha

- 베타 readiness 추정: 약 `60%`
- 현재 표현: `late-stage alpha / beta-closure sprint`
- 보정 이유:
  - 기능 표면만 보면 core/foundation 구현은 넓지만, beta는 기능 개수가 아니라 end-to-end 신뢰도다
  - HIR/MIR CFG skeleton은 이미 있지만, 함수/action/intent body 안전성의 semantic source-of-truth가 아직 CFG/dataflow로 승격되지 않았다. all-path retun, use-before-init, move/borrow join, drop cleanup, zone/effect transition, parallel/channel boundary를 AST/helper traversal만으로 닫으면 strict beta 신뢰도가 부족하다
  - AIR abstraction safety는 Phase 1 데이터 구조 / synthesis / drift checker baseline과 driver semantic-validation wiring이 들어왔다. Intent ↔ implementation drift 검출은 `docs/104_air_compiler_architecture.md`와 `make air-drift-test-smoke`로 gate에 들어왔고, strict evidence는 기본값으로 승격됐다. missing HIR CFG / RIR boundary / RIR authority evidence는 `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`로 hard-fail 되며, `authorized by` participant 이름과 RIR authority fact / authorize op subject가 일치해야 한다. authority evidence 누락 진단은 `Reason:` 안에 expected authority participant list를 포함한다. AIR drift message와 synthesized intent/boundary/authority name은 owned lifetime으로 관리되고, repeated drift check가 이전 message를 안전하게 해제하는 회귀 테스트와 parsed-source AIR teardown-safe boundary source 회귀가 있다. `where + transfer`는 더 이상 zone boundary 하나로 접히지 않고 zone boundary와 world-handoff boundary를 모두 합성한다. world-handoff evidence는 이제 matching RIR intent scope만으로 통과하지 않고 boundary source alias에 대한 RIR `Move`/`Claim` transfer op를 요구한다. implementation boundary evidence는 이제 HIR CFG proof도 요구하므로 `parallel` / `channel` / IO / execution boundary는 RIR evidence만으로 통과하지 않는다. parsed-source missing-authority-evidence negative와 parsed-source IO execution-boundary missing-evidence negative는 full driver JSON path에서 step source span과 `stage/code/cause_ir/fix_source`까지 고정됐다. expression boundary evidence는 더 이상 owner-name-only RIR scope match로 통과하지 않는다. `PGY_AIR_STRICT_EVIDENCE=0`은 개발/디버그 opt-out이다. `make air-backend-nonimpact-test-smoke`는 relaxed AIR와 default strict AIR가 intent/zone, cross-world transfer, handoff frontier, world projection, relation/effect, authority-failure fixture set에서 같은 C/LLVM 텍스트를 생성하는지 비교한다. `make air-backend-nonimpact-full-test-smoke`는 full frozen backend-compare fixture sweep을 같은 방식으로 돌리고 Linux CI gate로 승격됐다. `make air-strict-backend-compare-test-smoke`는 strict evidence 상태에서 C/LLVM 실행 parity까지 검증한다. parser/lexer baseline JSON routing은 `stage`, `code`, `cause_ir`, `fix_source`까지 닫혔다. 남은 blocker는 AIR transfer/world source negative 확장, Windows native evidence, parser-specific code split / multi-error accumulation이다
  - CFG 소비자 정리: `type_checker_flow_match.c`가 match patten binding, match exhaustiveness, redundancy, total-coverage lattice를 소유한다. `type_checker_flow.c`는 branch/join, loop/defer/parallel boundary, body retun/unreachable flow orchestration에 집중하며 435 LOC로 내려갔다. `semantic-core-shape-test-smoke`는 `type_checker_flow.c`와 `type_checker_flow_match.c`가 모두 600 LOC 이하인지 검사한다.
  - 2026-04-27 AIR IO boundary tightening: intent-step execution scan now treats the stable resource IO/time builtin set as AIR `io` boundaries, not only `ReadFile` / `WriteFile` / `ReadLine`. The gated set is `FileOpen`, `FileRead`, `FileWrite`, `FileClose`, `ReadFile`, `WriteFile`, `Input`, `ReadLine`, `Now`, and `Sleep`; `Print` / `Log*` remain observability output calls rather than AIR resource-boundary evidence in Phase 1. `src/test_air.c` keeps the set synchronized with `src/compiler/air_boundary.c`.
  - 2026-04-27 AIR owner split: dump/vocabulary functions moved to `src/compiler/air_dump.c`; `src/compiler/air.c` is back under the 600 LOC split-review threshold and keeps synthesis/drift ownership focused.
  - 2026-04-29 AIR await-boundary closure: `await` is now a synthesized AIR `parallel` boundary source, not just a recursive operand walk. Strict evidence accepts it only when RIR exposes the exact same-AST `AwaitRemote` operation; generic scope-name evidence such as a scope named `await` is rejected. HIR/CFG evidence is still required for implementation-boundary proof. AIR boundary AST traversal moved to `src/compiler/air_boundary_walk.c`; `src/compiler/air_boundary.c` now owns boundary taxonomy/policy only.
  - 2026-04-29 CFG-owned control classifier closure: `mir_cfg_contract_control.h` now has a real include guard and is consumed by both MIR statement population and MIR CFG validation. The duplicated CFG-owned control switch in `mir_stmt_population.h` was removed, so fallback `MIR_INST_STMT` filtering and validator rejection share one classifier.
  - Type-resolution DAG가 아직 semantic source-of-truth가 아니므로 declaration order / module contract / generic consumer path drift 위험이 남아 있다
  - 장기 모듈화 stop condition도 아직 멀다. semantic 800 LOC 초과 `.inc` 조건과 runtime/codegen/compiler 1,000 LOC 초과 `.inc` 조건은 닫혔지만, 여러 split은 아직 include-order 보존 상태라 실제 owner/TU extraction 부채가 남아 있다
  - 따라서 공식 진행률은 “기능 표면 성숙도”가 아니라 “베타 신뢰도 readiness” 기준으로 약 60%로 본다

## Beta taxonomy freeze: core / foundation / style

베타 기준은 이제 기능 나열이 아니라 언어 정체성 기준으로 나눈다.

- Core language: `intent`, `world`, `zone`, `subject`, `relation`, `effect`, `projection`, `authority`, `handoff`, runtime observability, anchored ownership boundary, generic contract system, module visibility/export contract, `parallel`.
- Generic contract는 core다. exact/ability/multi-bound/default type arg actual resolution은 FP/OOP 편의가 아니라 domain contract를 표현하는 타입 언어다.
- Foundation layer: primitive values, `func`, `let`, control flow, callable/lambda baseline, `Option`/`Result`, stable collections, core 실행에 필요한 runtime ABI.
- Style / compatibility surface: OOP convenience, FP combinator libraries, app infra, richer async helpers.
- Execution family split: `parallel`은 core execution primitive이고, `spawn`/`async`/`await`/`select`/`channel`/cancel은 그 아래 execution family다. fiber/coroutine은 language core가 아니라 runtime scheduling/suspension mechanism이다.
- Accelerator split: AI-first/GPU 방향은 `pgy.accel.spray` 논리 모듈로 예약한다. 이는 `parallel` / ownership / module visibility 위에 올라가는 accelerator library/runtime 축이며 core keyword 확장이 아니다.
- Render split: Skia/shader/render graph 방향은 `pgy.render.skia` 논리 모듈로 예약한다. renderer/shader는 core keyword가 아니라 Spray/Execution 위의 생태계 모듈이다.
- Compatibility split: OOP/FP/DOP는 각각 `pgy.compat.oop`, `pgy.compat.fp`, `pgy.compat.dop`로 분리한다. 기존 언어 스타일을 수용하되 core identity로 설명하지 않는다.
- FP compatibility update: Zig `comptime`-style type-level computation,
  user-customizable compile-time errors, and Sbv-style symbolic execution DSLs
  are tracked as post-beta `pgy.compat.fp` research/module work, not beta core
  language work.
- Interop split: 외부 언어 연동(JVM 캐스팅/JNI 브릿지, Python C-API 등)은 `pgy.interop.*` 생태계 모듈로 분류하며, 베타 마일스톤에서는 완전히 제외(Out of Beta)한다.

업데이트 정책:

- `pgy.core`는 가장 자주 개선하되 가장 작고 강하게 검증한다.
- `pgy.foundation`은 core보다 느리게 움직이며 ABI/backend parity를 깨지 않는다.
- `pgy.accel.spray`, `pgy.render.skia`, `pgy.compat.*`, `pgy.std.*`, `pgy.kit.*`는 모듈 생태계로 진화한다. 빠른 실험은 허용하지만 core keyword를 늘리지 않는다.

실행 규칙:

- B0 blocker는 `core + foundation stable subset`에만 붙인다.
- `pgy.fp`식 Functor/HKT 추상화, class-heavy OOP 확장, coroutine/fiber 고도화는 beta identity blocker가 아니다.
- `pgy.accel.spray`는 post-beta design surface다. 베타 전에는 새 GPU 키워드나 backend-specific CUDA/ROCm/Metal 문법을 열지 않고, module boundary와 ownership 원칙만 고정한다.
- `pgy.render.skia`와 `pgy.compat.dop`도 post-beta design surface다. 베타 전에는 shader/layout keyword를 열지 않고 module boundary만 고정한다.
- 단, `parallel`은 core이므로 slot/resource/effect conflict, cancellation/fainess, C/LLVM lowering parity는 beta 품질 기준으로 계속 관리한다.
- Source of truth: `docs/99_language_module_taxonomy.md`
- Machine-readable manifest: `docs/language_module_manifest.json`
- Representative case tags: `docs/language_module_cases.json`
- Drift gate: `make module-taxonomy-test-smoke`
- Parallel core/execution split gate: `make parallel-core-contract-test-smoke`
- Operational beta checklist: `docs/100_beta_readiness_checklist.md`

## Formal semantics / mathematical proof obligations

베타는 “테스트가 통과한다”만으로 닫히지 않는다. stable subset마다 타입 보존, 진행, ownership safety, authority soundness, projection freshness, DAG soundness, module visibility non-interference, backend parity 같은 수학적 불변식이 문서화되어야 한다.

- Source of truth: `docs/semantics/`
- Stable index: `docs/102_formal_semantics_and_proof_obligations.md`
- Drift gate: `make formal-semantics-test-smoke`
- 상태: `IN PROGRESS / BLOCKER-DOC`
- 베타 기준:
  - [x] 수학 library 문서(`docs/45_math_layer_design.md`)와 언어 의미론 증명 문서를 분리한다.
  - [x] stable beta subset의 semantic domain, judgment, theorem/proof-obligation vocabulary를 고정한다.
  - [ ] B0 항목마다 theorem statement + current regression evidence + remaining proof obligation을 최신 코드 상태와 맞춘다.
  - [ ] runtime propagation, DAG, MIR declaration inventory, ABI ownership, C/LLVM parity의 남은 blocker를 proof obligation으로 추적한다.
  - [ ] beta 문구에서 Lean/Coq/기계증명 완료처럼 보이는 표현을 금지한다. 기계증명은 별도 executable model 또는 proof assistant artifact가 생기기 전까지 post-beta/v1 hardening으로 둔다.
  - [~] **[NEW]** Runtime panic / unwinding model (abort vs unwind)의 정책 명시 및 C/LLVM backend parity 증명 추가. Panic class vocabulary와 released-slot / invalid-secure-token / double-release / device-slot / out-of-bounds / authority-mismatch / OOM / divide-by-zero / intenal-invariant hard-fail contract는 `src/runtime/pgy_runtime_panic_contract.h`, `make runtime-panic-contract-test-smoke`, `make runtime-panic-abi-test-smoke`, `make runtime-panic-codegen-test-smoke`로 고정했다. Generated C/LLVM `Array<T>`/`Slice<T>` indexing, temporary function-retun indexing, `ArraySet`, `ListGet`, `QueuePop`, `MapGet`, `ListSet`, `ListRemove`, `MapRemove` invalid access와 `Unwrap(Err)` / `UnwrapOption(None)` misuse도 checked runtime helper / panic contract로 고정했다. 남은 것은 새 panic class가 추가될 때마다 같은 executable parity gate를 요구하는 것이다.
  - [~] **[NEW]** Secure slot 및 authority token의 위변조 불가능성(Unforgeability) 형식 불변성(Formal Invariants) 문서화. Secure slot invalid-token/denied-capability export path는 silent fallback에서 panic contract로 이동했다.
  - [ ] **[NEW]** Intent 시스템의 Rollback/Cleanup 보장에 대한 Formal Closure (상태 기계 증명) 문서화.

운영 규칙:

- 테스트/스모크/백엔드 비교는 proof evidence이지 proof 자체가 아니다.
- undocumented mathematical assumption이 필요한 surface는 stable이 아니라 `IN PROGRESS`, `explicit reject`, 또는 `OUT OF BETA`로 내려야 한다.
- FP functor/HKT, full ownership, full quantum, GPU/Spray, Skia/render graph는 현재 beta proof scope 밖이다.

## Missing beta gate audit

현재 strict beta 기준에서는 다음 항목을 별도 gate로 본다. 이 항목들은 기능 확장이 아니라 이미 있는 core/runtime/tooling 표면의 신뢰도 계약이다.

- [~] Runtime panic / unwinding model: OOM, divide-by-zero, out-of-bounds, slot violation, token mismatch, authority mismatch, invariant break의 abort/unwind/recoverable 정책을 `Runtime Panic Parity` proof obligation으로 올렸다. `src/runtime/pgy_runtime_panic_contract.h`가 panic class vocabulary를 소유하고, inline/exported typed slot read/write/release는 released-slot 및 double-release에서 더 이상 기본값/no-op로 빠지지 않는다. `make runtime-panic-abi-test-smoke`가 released-slot, invalid-secure-token, double-release, device-slot, out-of-bounds, authority-mismatch, OOM, divide-by-zero executable evidence를 제공한다. `make runtime-panic-codegen-test-smoke`는 generated C/LLVM divide/modulo-by-zero와 `Array<T>`/`Slice<T>` index, temporary function-retun index, `ArraySet`, `ListGet`, `QueuePop`, `MapGet`, `ListSet`, `ListRemove`, `MapRemove` invalid access, `Unwrap(Err)`, `UnwrapOption(None)` parity를 검증한다. 남은 것은 새 hard-fail class가 추가될 때마다 같은 executable parity gate를 요구하는 것이다.
- [~] Secure slot / authority secret invariant: token unforgeability, secure-slot mismatch denial, authority token non-forgeability, authority transfer single-owner invariant, runtime snapshot secret non-exposure를 `Secure Token Unforgeability` / `Authority Transfer Single-Owner` proof obligation으로 올렸다. inline/exported secure slot read/write/release invalid-token 및 denied-capability path는 `PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN`로 고정했고 secure-slot double-release도 `PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE`로 고정했다. `make runtime-panic-abi-test-smoke`가 invalid-token/double-release executable evidence를 제공한다. authority-token mismatch는 `authority-token-mismatch` runtime code/reason, `make test-security`, `authority_failure_abi`, `authority_failure_surface`로 C/LLVM parity regression까지 닫았다. unsupported authority-token transport는 channel send/receive/helper/close, cancellation payload, direct named `spawn`에서 explicit reject로 닫았다. 남은 것은 richer domain-boundary denial이다.
- [ ] Intent formal closure: step ordering, compensation/rollback/invalidation, effect propagation, observability ABI stability를 beta-stable contract로 고정한다.
- [ ] Zone/world/authority/handoff formal closure: zone generation, world embedding, handoff frontier, projection freshness, authority rejection query surface를 beta-stable contract로 고정한다.
- [ ] Diagnostic quality gate: 모든 user-facing error가 severity, stable code, source span when available, `Reason:`, `Fix:`를 갖도록 품질 기준을 registry smoke와 별도 gate로 둔다.
  - 진행: intent clause explicit reject 중 `spawn`/channel control-transfer AST가 parser source span을 보존하도록 고쳤고, `make diagnostics-json-test-smoke`가 `on: spawn ...`와 `on: ch <- value`의 `PGY_SEM_INTENT_STEP_INVALID` JSON line/column + `cause_ir` + `fix_source`를 고정한다.
- [ ] Cross-platform CI matrix: Linux/WSL, Windows native/MSYS2/MinGW, macOS의 support level을 stable/experimental/out-of-beta로 명시한다.
  - 진행: Windows LLVM support detection은 executable `llvm-config --libs core` evidence가 있을 때만 `WINDOWS_LLVM_READY=1`이 되도록 좁혔다. `C:/Program Files/LLVM/lib` 같은 library folder 존재만으로 LLVM smoke/backend-compare를 실행하지 않는다. 현재 beta 계약은 Linux C+LLVM, Windows C-only이며 Windows LLVM은 실제 MSYS2 runner green evidence가 생길 때만 승격한다.
  - 진행: README support matrix에 macOS는 dedicated runner/support contract가 생길 때까지 out-of-beta로 명시했다.
- [~] Beta stable subset definition: keyword, syntax, API, AST-visible shape, runtime ABI, backend parity 범위를 `docs/107_beta_stable_subset.md`에서 freeze한다. 남은 일은 이 문서의 각 stable 항목을 해당 semantic/runtime/C/LLVM regression row와 1:1로 연결하는 것이다.
- [~] Stdlib beta freeze list: stable/experimental/out-of-beta API와 breaking-change policy를 명시한다.
  - 진행: `docs/108_stdlib_beta_freeze.md`가 builtin stdlib, stable `use` modules, known experimental modules, out-of-beta ecosystem work를 분리한다. `make stdlib-test-smoke`가 builtin stdlib probe와 stable `use` module probe를 C/LLVM 양쪽에서 고정한다. 남은 일은 third-party package/version/supply-chain policy다.
- [~] Tooling conformance: LSP/fmt/debugger의 beta-stable 범위를 명시한다.
  - 진행: `make tooling-conformance-test-smoke`가 formatter idempotence/compile smoke, LSP initialize/hover/completion capability, debugger CLI parse+semantic+quit path를 executable gate로 고정한다. DAP, binary breakpoint, variable watch, rich refactor, multi-file workspace LSP는 아직 beta-stable tooling subset이 아니다.
- [~] Package/module resolver surface: manifest, version resolution, import path, supply-chain integrity를 stable/experimental/out-of-beta로 분류한다.
  - 진행: `docs/109_package_module_resolver_contract.md`가 beta-stable module surface를 `import "relative/path.pgy";`, importing-file-relative resolution, namespace/export visibility, circular import rejection으로 고정했다. package surface는 `pgy init <name>` scaffolding만 stable이다.
  - 진행: `pgy install`은 더 이상 소스 파일 경로로 오인되지 않고 explicit out-of-beta rejection을 낸다. `make package-module-resolver-test-smoke`가 doc contract, `pgy init`, `pgy install` reject, missing import JSON, circular import JSON을 고정한다.
  - 남음: dependency version solving, lockfile, registry, checksum/signature verification, remote import, supply-chain integrity는 beta 이후 resolver/package-manager track으로 유지한다.
- [~] Test quality gate: pre-beta mandatory suite, fuzz/property status, coverage/perf baseline을 추적한다.
  - 진행: `docs/111_beta_test_suite_freeze.md`가 mandatory pre-beta gates, platform gates, fuzz/property/coverage non-claims, regression policy를 freeze했다. `make beta-test-suite-freeze-test-smoke`가 freeze doc과 Makefile target 존재를 검사한다.
  - 남음: 실제 fuzz corpus, property-based generator, coverage percentage threshold는 beta 이후 품질 트랙으로 유지한다. 현재 beta gate는 named stable-surface coverage다.
- [~] Observability/tracing schema: event schema, intent history, authority failure state, runtime registry, trace format version을 고정한다.
  - 진행: `docs/112_observability_trace_schema.md`가 beta-stable schema를 `IntentLast*`, `IntentHistory*`, `IntentActive*`, `IntentRecent*`, authority failure snapshot(`ok/zone/participant/code/reason`), runtime-borrowed string ABI, C/LLVM identical trace output으로 고정했다.
  - 진행: `make observability-schema-test-smoke`가 `intent_trace_abi`, `intent_recent_abi`, `intent_active_abi`, `intent_failure_abi`, `authority_failure_abi`를 C/LLVM 양쪽에서 expected stdout과 비교한다.
  - 남음: general event streaming, structured JSON trace export, distributed trace correlation, user-code registry hooks, stable binary trace format, richer multi-instance timeline query는 beta 이후로 유지한다.
- [~] Memory/concurrency model: `parallel`, task, channel, cancellation, visibility/happens-before 최소 계약을 문서화한다.
  - 진행: `docs/113_memory_concurrency_model.md`가 beta-stable happens-before, channel, cancellation, explicit out-of-beta memory model 범위를 고정했다. `parallel` join visibility, shared `ref`/`ref` 허용, `ref`/`own` 및 `own`/`own` task-boundary reject, copy-only non-blocking receive/cancel/close를 stable contract로 묶었다.
  - 진행: `make memory-concurrency-model-test-smoke`가 `parallel-core-contract-test-smoke`와 targeted C/LLVM backend compare(`parallel_channel_sum`, `parallel_channel_dual`, `triple_paradigm`)를 실행한다.
  - 남음: full weak-memory ordering, user-selectable memory order, scheduler fainess guarantee, lock-free correctness, anonymous async closure capture/lifetime, cross-thread `Arc<T>` / `Send` / `Sync` trait system은 beta 이후로 유지한다.
- [~] String/unicode policy: normalization, comparison, locale, escape handling, unsupported policy를 명시한다.
  - 진행: `docs/110_string_unicode_policy.md`가 UTF-8 string payload preservation, byte-length `StringLength`, byte-exact/normalization-blind equality/search를 beta-stable로 고정했다.
  - 진행: Unicode identifiers, normalization, locale-sensitive collation/case folding, grapheme iteration, display width, mixed-encoding source files는 explicit out-of-beta로 고정했다. `make unicode-policy-test-smoke`가 C/LLVM UTF-8 string execution과 Unicode identifier reject를 검증한다.
  - 남음: full Unicode text model을 도입하려면 post-beta에 scalar/grapheme/locale vocabulary와 별도 stdlib text module을 설계한다.

Checklist source of truth:

- `docs/100_beta_readiness_checklist.md`
- AIR source of truth: `docs/104_air_compiler_architecture.md`
- Drift gate: `make beta-readiness-checklist-test-smoke`
- AIR drift gate: `make air-drift-test-smoke`
- AIR backend non-impact gate: `make air-backend-nonimpact-test-smoke`
- AIR full backend non-impact hardening: `make air-backend-nonimpact-full-test-smoke`
- AIR strict backend execution parity: `make air-strict-backend-compare-test-smoke`

## 구조/운영 폐인 포인트 보드 (2026-04-20)

이 섹션은 기능 backlog가 아니라, 실제 작업 효율과 베타 신뢰도를 계속 깎는 구조 debt / 운영 pain point를 고정한다.

우선순위 제안:
- `P0`: function/action/intent body CFG + dataflow를 semantic source-of-truth로 승격
- `P1`: `.inc` 분할을 실제 `.c`/`.h` 모듈로 전환
- `P2`: hint namespace (`code` / `cause_ir` / `fix_source`)를 레지스트리 기반으로 고정
- `P3`: type-category vocabulary를 2-3층으로 압축
- `P4`: 빌드/샌드박스/중간-stage JSON/artifact 문제를 공식 경로 기준으로 정리
- `P9`: arena 패턴을 scratch/result lifetime 기준으로 명시 도입
- `P9b`: repeated `Slot` / `SecureSlot` hot-loop access는 Pin/Lease 문서 기준으로 분리한다. 기본 path는 매 접근 검증이고, fast path는 scope-entry capability lease + automatic unpin cleanup이어야 한다. Runtime ABI baseline은 `PgyPinnedView` / `PergyraSlotPin` / `PergyraSlotUnpin` + `make test-security` 회귀로 시작했고, plain token-bearing pin rejection, scope release while pinned, TTL cleanup skip while pinned, secure invalid-token/capability rejection, concurrent secure write rejection, release-after-unpin persistence를 닫았다. Generated inline `PgySlot_*` / `PgySecureSlot_*` ABI now also has typed `PgyPinnedSlotView_*` / `PgyPinnedSecureSlotView_*` wrappers plus LLVM-linkable `pgy_pin_read_*`, `pgy_pin_write_*`, and `pgy_unpin_*` exports; `make test-memory` covers occupied/token validation, cleanup helper behavior, double-unpin hard-fail, and secure invalid-token pin rejection. C source-block emission now lowers pin blocks to typed wrapper variables with GCC cleanup hooks through `src/codegen/transpiler_block_emit.h`, while `src/runtime/pgy_runtime_plain_slot_inline.h` owns the plain Slot wrapper macro under the 600 LOC split threshold. C and LLVM MIR emission now consume MIR pin-region/view-alias metadata on successor/retun exits, emitting explicit typed `pgy_pin_*` / `pgy_unpin_*` calls before control leaves the pin region; `tests/compare_backends.sh` covers plain read, secure read, plain write, secure write, mixed plain+secure source-level pin blocks, normal successor cleanup, direct retun cleanup, conditional branch-to-retun cleanup, loop `break`/`continue` cleanup, and secure boundary-slot parameter pinning. Source syntax `pin slot as view: ReadView<T>|WriteView<T> { ... }`는 parser/semantic surface로 활성화됐고, AST `Pin Block` metadata, HIR/MIR pin-region metadata, MIR `pin-unpin-cleanup-edge` cleanup fact까지 내려간다. MIR validator now rejects reachable pin-region blocks that lack the matching `pin-unpin-cleanup-edge` fact for source slot/view/access mode, and `src/test_mir.c` has a negative corruption regression for that contract. Pin/Lease semantic diagnostic vocabulary는 `PGY_SEM_PIN_ESCAPE`, `PGY_SEM_PIN_PARALLEL_CONFLICT`, `PGY_SEM_PIN_AWAIT_BOUNDARY`, `PGY_SEM_PIN_QUBIT_REJECT`, `PGY_SEM_PIN_TOKEN_INVALID`로 registry/docs에 고정했고 `make diagnostic-registry-test-smoke`, `make beta-readiness-checklist-test-smoke`, `make diagnostics-json-test-smoke`가 drift를 막는다. Existing `ViewRead(...)` / `ViewWrite(...)` semantic surface now enforces `WriteView<T>` exclusive access for the same source slot while keeping shared `ReadView<T>` / `ReadView<T>` accepted. It also emits pin-specific diagnostics for retun escape, await boundary, parallel boundary/acquisition, defer cleanup registration, and QubitSlot rejection, and `make diagnostics-json-test-smoke` verifies their CLI JSON route. Generic ownership baseline은 unresolved `TYPE_KIND_GENERIC`을 `BORROW_TRACKED`로 분류해 generic `own/ref`가 조용히 copy-only로 통과하지 못하게 막는다. 남은 것은 secure-token source diagnostic, exceptional/cancellation all-exit proof expansion, and richer invalid-token source provenance. Source of truth: `docs/74_slot_pinning_caching.md`
- `P9c`: `Rc<T>` / `Weak<T>` 최소 subset은 beta-stable로 닫았다. 범위는 single-thread `Int|Long|Float|Double|Bool|String` payload, explicit lifecycle builtin(`RcNew`, `RcClone`, `RcGet`, `RcDrop`, `RcDowngrade`, `WeakUpgrade`, `WeakDrop`), resolver metadata, semantic builtin typing, C runtime/emitter, LLVM runtime export/lowering, ABI layout smoke, C/LLVM lifecycle backend-compare다. 범위 밖 payload는 backend fallback이 아니라 semantic explicit reject다. `Arc<T>`, cross-thread shared ownership, generic/object payload 확장, default ARC는 beta 밖이다. Source of truth: `docs/100_beta_readiness_checklist.md`, `docs/106_ownership_model_comparison.md`, `src/runtime/pgy_abi_spec.h`
- `P10`: 모듈화/전파 고도화의 compile/runtime 속도 회귀를 별도 baseline으로 추적

### P0. Function CFG / Body Dataflow Closure

판정: `BLOCKER`

핵심 정리:

- CFG가 없는 상태는 아니다. HIR는 function CFG v0, predecessor/reachability, dominator/frontier, loop depth, phi candidate skeleton을 가진다.
- MIR도 HIR CFG와 RIR op를 묶어 routine/block/instruction/cleanup block, SSA version map, def/use, cleanup/rollback/invalidation exceptional CFG, liveness/DCE vertical slice까지 가지고 있다.
- 남은 blocker는 이 CFG/MIR infra를 **함수 본문 의미론의 source-of-truth**로 승격하는 것이다. 현재 body safety의 일부는 여전히 AST/helper traversal, local summary, backend fallback에 기대고 있어 strict beta 기준으로 부족하다.

베타 완료 조건:

- [ ] Function/action/intent body마다 `BasicBlock`, `Edge`, `Terminator`, reachability, exceptional cleanup edge가 semantic pass에서 직접 소비된다.
- [x] 반환형이 있는 routine은 모든 reachable normal path에서 retun/value terminator를 가진다는 all-path retun 검사를 CFG body summary로 고정한다.
- [~] definite assignment/use-before-init 검사를 CFG dataflow로 이동하고 branch/join/loop widening 진단을 고정한다. stable local `let` 표면은 parser `=` 요구와 `PGY_SEM_UNINIT_LOCAL` backstop으로 봉인됐고, wider delayed-assignment lattice는 아직 열려 있다.
- [~] move/use-after-move, borrow/ref lifetime, boundary escape를 CFG join facts로 계산한다. `QubitSlot` loop break/continue join regression, anchored `Slot<T>` branch/join release-state regression, `own subject` branch/join consumed-state regression, parallel subject transfer join/conflict regression, parallel `ref`+`own` boundary conflict regression, parallel `ref`+`ref` shared-read acceptance regression, direct named-call `spawn ref` ownership-boundary rejection regression, anonymous async spawn explicit reject regression은 닫혔고, closure/lambda/general longer-lived borrow lifetime은 남아 있다. `mut ref`/`ref mut` surface가 없으므로 mutable-borrow overlap은 beta-out-of-scope로 봉인한다.
- [~] owned resource drop/cleanup insertion point를 normal retun, early retun, break/continue, intent cancel/rollback/invalidation edge에서 같은 규칙으로 계산한다. `defer` cleanup terminator와 resource-state snapshot/restore 격리, direct `type_check_statement()` fallback convergence, anchored slot branch/join state tracking은 닫혔다. MIR validator now also rejects reachable pin-region blocks that lack the matching `pin-unpin-cleanup-edge` fact, so pin unpin cleanup is no longer just a generated convention. 남은 것은 full drop insertion/validation과 exceptional/cancellation all-exit proof expansion이다.
- [ ] zone/effect/relation transition facts를 path-sensitive summary로 올려 branch/join/handoff에서 stale state와 conflict를 같은 vocabulary로 진단한다.
- [~] `parallel`/channel/task boundary에서 moved value, borrowed reference, authority-bearing token, cancellation cleanup fact를 CFG summary로 검증한다. parallel task-local terminator isolation, moved/released resource/boundary join, duplicate resource/boundary consume diagnostic, `ref`+`own` boundary conflict, blocking channel-send resource consume/join, direct named-call `spawn ref` ownership-boundary rejection, direct named-call `spawn Token<T>` authority-boundary rejection, anonymous async spawn explicit reject, `SendTimeout`/`TrySendStatus`/`SendTimeoutStatus` transport rejection, `TryRecv`/`RecvTimeout` movable receive explicit reject, authority `Token<T>` channel helper rejection, copy-only cancellation payload reject, copy-only channel close는 닫혔고, broader channel receive/backpressure summary, closure/lambda/general borrowed-reference task lifetime, cancellation cleanup fact는 남아 있다.
- [~] Interprocedural body summary를 고정한다: `may_retun`, `may_escape_ref`, `moves_param`, `borrows_param`, `drops_resource`, `effects`, `requires_zone`, `spawns_task`, `sends_channel`. 1차 구조로 function type의 `body_summary_mask`와 semantic recorder는 들어갔다. direct function call은 callee summary 중 caller-relevant transitive facts를 소비하고 declaration-known `own/ref` parameter boundary facts도 기록한다. method/host call도 같은 declaration-known summary facts를 기록한다. lambda body summary는 lambda function type에 격리되어 outer routine으로 새지 않고, function-typed lambda binding 호출은 같은 callee-summary path로 전파된다. 남은 것은 intent/helper call까지 넓히고 zone/effect/runtime propagation과 C/LLVM lowering이 이 summary bit를 직접 소비하게 만드는 일이다.
- [ ] 진단은 block/path provenance를 포함한다: source path, branch/join edge, previous state, Reason, Fix.
- [ ] MIR/C/LLVM lowering은 같은 CFG/dataflow facts를 소비하고, frozen subset parity regression으로 묶는다.

실행 순서:

1. 현재 HIR/MIR CFG fact inventory와 semantic 소비 지점을 표로 만든다.
2. `--hir-cfg`, `--mir`, RIR flow-block dump를 묶는 smoke를 추가해 CFG fact drift를 막는다.
3. all-path retun + reachability + definite assignment를 CFG 기반으로 먼저 승격한다.
4. ownership move/borrow/drop cleanup을 CFG dataflow로 이동한다.
5. zone/effect/relation transition, handoff frontier, projection freshness를 body CFG summary와 runtime propagation scheduler에 연결한다.
6. parallel/channel/task boundary summary를 추가하고 C/LLVM backend compare에 frozen cases를 넣는다.

검증 목표:

- `make test-semantic`
- `make ir-pipeline-test-smoke`
- `make type-resolution-dag-test-smoke`
- `make cfg-body-dataflow-test-smoke`
- `make llvm-test-backend-compare`

Source of truth:

- `docs/103_cfg_body_dataflow_need.md`

### P1. `.inc` 스파게티를 실제 모듈로 절단

- 문제:
  - 현재 `type_checker.c` 및 transpiler/LLVM 일부는 “모듈화”가 아니라 “파일 분할된 단일 translation unit”에 가깝다
  - IDE jump/symbol lookup/forward decl 순서 관리가 모두 수동
  - formatter/linter/외부 edit가 include 순서/파일 갱신 타이밍에 민감하게 깨진다
- 영향:
  - 대형 수정 시 edit conflict / implicit declaration / include ordering failure가 반복됨
  - ownership/generic/provenance 같은 횡단 작업이 불필요하게 느려진다
- 기본 방침:
  - 우선 `semantic/type_checker_*`에서 ownership / generic / module-contract / diagnostics 축부터 실제 `.c`/`.h` export 구조로 절단
  - declaration-side MIR-only hot path도 helper family를 `.c` 경계로 분리
  - 장기 목표선은 `docs/92_inc_split_roadmap.md`의 Target State A-D로 고정한다
  - stop condition: semantic에는 800 LOC 초과 `.inc` 없음, codegen/runtime에는 1,000 LOC 초과 `.inc` 없음, `type_checker.c`는 orchestration-only, backend declaration path는 dedicated inventory reader 또는 hard error만 허용
  - speed stop condition: `test-abi-perf`와 `perf-summary` baseline을 유지하고, 모듈화 slice 후 worst-case compile time이 2배 이상 튀면 회귀 후보로 기록
  - `.inc`는 generated table / local macro table / private test fixture 같은 제한 용도로만 남긴다
- 준비 작업:
  - [ ] `type_checker`를 최소 5축으로 절단
    - [x] diagnostic emission/snapshot: `type_checker_diag.c`
    - [x] ownership classification: `type_checker_ownership_classify.c`
    - [x] channel transport validator: `type_checker_channel_transport.c`
    - [x] ownership diagnostics/consumers: `type_checker_ownership_diag.c`
    - [x] generic contract diagnostics: `type_checker_generic_diag.c`
    - [x] ability reference formatting seam: `type_checker_ability_ref.c`
    - [x] stdlib use validator seam: `type_checker_stdlib_use.c`
    - [x] module contract diagnostic seam: `type_checker_module_contract_diag.c`
    - [x] ability fields validator seam: `type_checker_ability_fields.c`
    - [x] ability matcher / subject ability lookup seam: `type_checker_ability_match.c`
    - [x] ability where-bound validator seam: `type_checker_ability_where.c`
    - generic consumer pipeline
    - [x] module contract / authority consumer: `type_checker_module_contract.c`
  - 진행: ownership 공용 enum/entrypoint를 `type_checker_ownership_intenal.h`로 분리 시작
  - 진행: ownership diagnostics forward declaration도 `type_checker_ownership_diag_intenal.h`로 분리 시작
  - 진행: ownership escape diagnostic renderer/helper family는 `type_checker_ownership_diag.c`로 실제 TU 분리 완료
  - 진행: ownership support helper(`semantic_assignment_target_path`, `semantic_borrowed_boundary_root_name`)도 `type_checker_ownership_support_intenal.h`로 분리 시작
  - 진행: ownership consumer seam(`retun` / `assign` / `call`)도 `type_checker_ownership_consumers_intenal.h`로 분리 시작
  - 진행: `param_summary`도 raw include block이 아니라 `semantic_check_param_summary_escapes(...)` consumer helper로 승격
  - 진행: channel transport seam도 `type_checker_channel_transport_intenal.h`로 분리 시작
  - 진행: channel transport validator/reporters는 `type_checker_channel_transport.c`로 실제 TU 분리 완료
  - 진행: high-arity generic mismatch helper도 `type_checker_generic_diag.c`로 실제 TU 분리 완료
  - 진행: module contract consumer 선행 seam인 ability reference display/name/signature helper는 `type_checker_ability_ref.c`로 실제 TU 분리 완료
  - 진행: stdlib use validator는 `type_checker_stdlib_use.c`로 실제 TU 분리 완료
  - 진행: subject ability mismatch diagnostic은 `type_checker_module_contract_diag.c`로 실제 TU 분리 완료
  - 진행: ability `fields` validator는 `type_checker_ability_fields.c`로 실제 TU 분리 완료
  - 진행: `find_type_decl_by_name`는 include-order static helper에서 `type_checker_intenal.h` intenal API로 승격
  - 진행: ability ref matching / role ability lookup / subject ability lookup은 `type_checker_ability_match.c`로 실제 TU 분리 완료
  - 진행: `find_ability_decl_by_name` / `collect_effective_generic_arg_nodes`는 include-order static helper에서 `type_checker_intenal.h` intenal API로 승격
  - 진행: ability where-bound consumer validation은 `type_checker_ability_where.c`로 실제 TU 분리 완료
  - 진행: `format_type_constraint_bounds`는 include-order static helper에서 `type_checker_intenal.h` intenal API로 승격 후 별도 TU로 분리
  - 진행: `semantic_type_resolution_record_type_ref_dependency`는 graph core TU로 이동해 include-order static helper 의존을 제거
  - 진행: `semantic_type_resolution_collect_type_refs`는 `type_checker_resolution_graph_collect.c`로 이동해 DAG inventory collector의 첫 실제 TU seam을 만들었다
  - 진행: generic contract inventory / string dependency / required ability collector helpers도 `type_checker_resolution_graph_collect.c`로 이동
  - 진행: top-level declaration graph registration도 `type_checker_resolution_graph_collect.c`로 이동해 inventory pass의 bootstrap helper debt를 더 줄였다
  - 진행: local-contract graph node/dependency + zone/world/projection label formatters는 `type_checker_resolution_graph_labels.c`로 이동해 graph inventory `.inc`를 1,835 LOC까지 축소했다
  - 진행: projection source resolver는 `type_checker_resolution_graph_domain.c`로 이동하고 `find_zone_domain_slot`을 intenal API로 승격해 graph/domain split 선행 seam을 만들었다
  - 진행: event declaration precollector는 `type_checker_resolution_graph_decl.c`로 이동해 declaration-kind collector 분리도 시작
  - 진행: enum declaration precollector도 `type_checker_resolution_graph_decl.c`로 이동하고 `semantic_stage_method_array`를 intenal API로 승격해 inventory `.inc`를 1,765 LOC까지 축소
  - 진행: ability declaration precollector와 action-contract precollector도 `type_checker_resolution_graph_decl.c`로 이동해 inventory `.inc`를 1,648 LOC까지 축소
  - 진행: role/class/party/roster declaration precollector도 `type_checker_resolution_graph_decl.c`로 이동하고, relation/effect domain inventory precollector는 `type_checker_resolution_graph_domain.c`로 이동해 inventory `.inc`를 1,299 LOC까지 축소
  - 진행: intent declaration precollector는 `type_checker_resolution_graph_decl.c`로, world inventory precollector는 `type_checker_resolution_graph_world.c`로 이동해 inventory `.inc`를 870 LOC까지 축소
  - 진행: zone refresh projection field-map DAG collector는 `type_checker_resolution_graph_zone.c`로 이동했고, graph inventory body는 `type_checker_resolution_graph_inventory.c`로 승격했다. `type_checker_resolution_graph_inventory.inc`는 제거되어 DAG inventory include-order debt가 닫혔다
  - 진행: projection builtin target-field resolver는 recursive fallback 대신 DAG metadata lookup-only seam으로 낮췄다. projection source/target mismatch 진단은 projection validator가 소유하고, target field type materialization은 DAG metadata가 소유한다. fallback seam cap은 31에서 30으로 내려갔다. 이후 type graph precollect를 top-level symbol pass 앞에 배치하고 `program_resolve_type_quiet(...)`를 metadata lookup-only로 낮춰 event/function placeholder가 recursive fallback 없이 DAG metadata를 쓰게 했다. fallback seam cap은 30에서 29로 내려갔다. domain query projection source-field resolver도 class/vessel field DAG metadata lookup-only로 낮춰 cap은 28로 내려갔다. party/roster shared-field resolver도 declaration metadata lookup-only로 낮춰 cap은 26으로 내려갔다. ability abstract method signature resolver와 role host-type resolver도 lookup-only로 낮춰 cap은 24로 내려갔다. function/action body expression/lambda/event handler precollect 확장 후 event/lambda handler resolver도 lookup-only로 낮춰 cap은 23으로 내려갔다. body flow resolver도 graph metadata lookup-only로 낮춰 cap은 22로 내려갔다. type-alias statement resolver도 DAG metadata lookup-only로 낮춰 cap은 21로 내려갔다. `world_decl` lookup-only 전환은 subject/zone nominal materialization이 아직 부족해 semantic 77개 실패를 만들었으므로 보류했다
  - 진행: world/zone local-contract stage replay는 `type_checker_resolution_stage_domain.c`로 이동했고, top-level DAG stage replay는 `type_checker_resolution_stage.c`로 승격해 `type_checker_resolution_stage.inc`를 제거
  - 진행: `type_checker_ability_decl.c`, `type_checker_zone_decl.c`, `type_checker_world_decl.c`는 standalone TU로 빌드되며 hidden include-order helper 의존을 intenal/header 계약으로 승격
  - 진행: `type_checker_intent_decl.c` standalone TU 승격 중 드러난 implicit helper dependency를 intenal/header 계약으로 승격하고, `-Werror=implicit-function-declaration -Werror=implicit-int`를 기본 CFLAGS로 고정해 같은 종류의 C 모듈화 버그를 빌드 단계에서 차단
  - 진행: `type_checker_role_decl.c`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`도 hard implicit-declaration CFLAGS 아래에서 빌드되도록 helper/header 의존을 명시
  - 진행: `type_checker_class_decl.c`가 class/exten declaration checking을 소유하고, `type_checker_program.c`가 top-level semantic orchestration을 소유한다. 관련 graph/worklist/effect/stats helper를 intenal API로 승격해 `type_checker_program.inc`를 624 LOC까지 축소
  - 진행: `type_checker_builtins_projection.c`가 `ToObject` / `ToTObject` semantic projection checker를 소유하고, `type_checker_builtins_nominal.inc`를 659 LOC까지 축소
  - 진행: expression operator/indexed-access checker를 `type_checker_expr_ops.c`로 분리하고, static member path / consumed-boundary helper를 `type_checker_expr_names.c`로 이동했다. `type_checker_expr.inc`는 758 LOC, `type_checker_helpers_late.inc`는 773 LOC가 되어 둘 다 semantic 800 LOC stop condition 아래로 내려갔다
  - 진행: event declaration/subscription/invoke semantic은 `type_checker_event.c`로 승격했고, QubitSlot compile-time state / entangle pool / movable-resource-use validation은 `type_checker_qubit.c`로 승격했다. `type_checker.c`는 481 LOC로 내려가 600 LOC 이하 stop condition을 만족한다
  - 진행: domain slot/projection/overlay helper body를 `type_checker_decls_domain_helpers.c`로 승격하고, intent inheritance/derivation helper body를 `type_checker_intent_helpers.c`로 승격했다. `type_checker_decls_domain_helpers.inc`는 제거됐고 `type_checker_decls_a.inc`는 1-line forwarding stub으로 축소
  - 완료: semantic `.inc` 800 LOC stop condition은 `make semantic-inc-size-test-smoke`로 고정. 현재 `src/semantic`에는 800 LOC 초과 `.inc`가 없다
  - 완료: semantic core shape stop condition은 `make semantic-core-shape-test-smoke`로 고정. `type_checker.c <= 600 LOC`, event/qubit owner TU, DAG inventory `.c` ownership을 CI에서 검사한다
  - 진행: C backend MIR inventory/SSA emitter include를 5-line shim + `transpiler_emitters_mir_inventory_intent.inc` / `transpiler_emitters_mir_inventory_ssa_names.inc` / `transpiler_emitters_mir_inventory_ssa_emit.inc`로 분리해 해당 debt를 모두 1,000 LOC 아래로 낮췄다
  - 진행: C backend `emit_program(...)` bootstrap은 direct declaration-array reads 대신 `transpiler_active_inventory(...)` / `transpiler_active_extens(...)` view를 소비한다. `make mir-declaration-inventory-test-smoke`가 C/LLVM declaration-side codegen의 raw declaration inventory access를 helper owner로 제한한다
  - 진행: C backend expression emitter include를 7-line shim + `transpiler_expr_emitters_builtins.inc` / `transpiler_expr_emitters_call_a.inc` / `transpiler_expr_emitters_call_b.inc` / `transpiler_expr_emitters_members.inc` / `transpiler_expr_emitters_tail.inc`로 분리해 해당 debt를 모두 1,000 LOC 아래로 낮췄다. 검증: `make test-transpile -j2`, `make llvm-test-backend-compare -j2`
  - 진행: LLVM call emitter include를 17-line shim + `llvm_expr_call_constructors.inc` / `llvm_expr_call_arrays.inc` / `llvm_expr_call_collections_base.inc` / `llvm_expr_call_domain_queries.inc` / `llvm_expr_call_events.inc` / `llvm_expr_call_intent_observability.inc` / `llvm_expr_call_log.inc` / `llvm_expr_call_math.inc` / `llvm_expr_call_result_option.inc` / `llvm_expr_call_slots.inc` / `llvm_expr_call_task_channel.inc` / `llvm_expr_calls_part_a.inc` / `llvm_expr_calls_part_b.inc` / `llvm_expr_calls_part_c.inc` / `llvm_expr_calls_part_d.inc`로 분리해 해당 debt를 모두 1,000 LOC 아래로 낮췄다. enum/class constructor, array builtin, `ListNew`/`Set*` base collection, domain query builtin, event invocation, intent observability, log, scalar math, Result/Option, slot/device-slot builtin, task/channel lowering은 `llvm_emit_call`에서 분리되어 별도 owner include가 됐다. 검증: `make test-transpile -j2`, `make backend-inc-size-test-smoke`, `make llvm-test-backend-compare -j2`
  - 진행: C backend base emitter B include를 6-line shim + `transpiler_emitters_base_b_part_a.inc` / `transpiler_emitters_base_b_part_b.inc` / `transpiler_emitters_base_b_part_c.inc` / `transpiler_emitters_base_b_part_d.inc`로 분리해 해당 debt를 모두 1,000 LOC 아래로 낮췄다. 검증: `make test-transpile -j2`, `make llvm-test-backend-compare -j2`
  - 완료: Tier 1 runtime/codegen/compiler `.inc > 1000 LOC` gate는 닫힘. `pgy_runtime_part_ba.inc`, `pgy_runtime_lib_part_b.inc`, `transpiler_emitters_base_a.inc`, `transpiler_helpers_core_a.inc`, `transpiler_helpers_core_b.inc`, `transpiler_domain_role.inc`, `llvm_expr_helpers.inc`, `mir_public.inc`, `llvm_expr_call_methods.inc`, `llvm_domain_helpers.inc`를 모두 safe mechanical split으로 1,000 LOC 아래로 낮췄다
  - 완료: `tests/backend_inc_size_smoke.sh` / `make backend-inc-size-test-smoke` 추가. `src/runtime`, `src/codegen`, `src/compiler`의 `.inc <= 1000 LOC`를 CI에서 고정
  - 검증: `make backend-inc-size-test-smoke`, `make test-mir test-transpile test-abi -j2`, `make llvm-test-backend-compare -j2`
  - 진행: `type_checker_helpers_late.c` standalone TU 빌드 중 드러난 call-path helper include-order 의존을 `type_checker_intenal.h` prototype과 직접 include 계약으로 고정했다
  - 진행: `type_checker_decls_a.inc -> type_checker_decls_domain_helpers.inc`, `type_checker_decls_intent.inc -> type_checker_world_decl.c`, `type_checker_helpers_effects.inc -> type_checker_helpers_host.inc` 사이 dangling retun-type seams 제거
  - 진행: `type_checker_resolution_graph_core.inc` → inventory include 경계의 dangling `static void` seam 2개를 명시 retun type으로 정리
  - 진행: `generic_params_required_count`는 include-order static helper에서 `type_checker_intenal.h` intenal API로 승격
  - 완료: required ability resolver와 action required-ability validator는 `type_checker_module_contract.c`로 실제 TU 분리 완료
  - 완료: `type_checker_module_contracts.inc` 제거. module contract include-order 구조 debt는 닫힘
  - [ ] `.inc` 내부 static helper 중 교차 참조 심한 심볼 목록 작성
  - [x] include-order에 의존하는 implicit declaration 경로 제거를 빌드 계약으로 승격 (`-Werror=implicit-function-declaration`, `-Werror=implicit-int`)
  - [~] declaration-side MIR-only debt는 helper-gated state까지 닫혔다. 남은 단계는 `MIRProgram` 안 AST-shaped declaration inventory를 dedicated declaration metadata model로 분리하는 일이다

  - 진행: ownership retun / assignment rebind / array literal store / boundary validation / call argument / destructuring / let-binding / parameter escape-summary consumers는 `.inc`에서 실제 TU로 승격했다. 삭제된 파일: `type_checker_ownership_retun.inc`, `type_checker_ownership_assign.inc`, `type_checker_ownership_array_store.inc`, `type_checker_ownership_boundaries.inc`, `type_checker_ownership_call.inc`, `type_checker_ownership_destructure.inc`, `type_checker_ownership_destructure_stmt.inc`, `type_checker_ownership_let.inc`, `type_checker_ownership_let_boundary.inc`, `type_checker_ownership_let_claim.inc`, `type_checker_ownership_let_infer.inc`, `type_checker_ownership_let_slot.inc`, `type_checker_ownership_let_value.inc`, `type_checker_ownership_param_summary.inc`. 현재 `src/semantic/type_checker_ownership_*.inc`는 0개다
  - 원칙 강화: 베타 기준에서는 behavior-owning `.inc`를 beta+1 정리가 아니라 blocker로 본다. generated table / local macro table / private test fixture 외 `.inc`는 owner `.c` 또는 명시적 generated artifact로 옮긴다
  - 원칙 강화: `.inc` 제거 과정에서 여러 behavior family를 하나의 mega-TU로 합치지 않는다. `make semantic-tu-size-test-smoke`가 새 semantic owner TU는 1,000 LOC 이하로 제한하고, 기존 초대형 TU는 개별 cap으로 더 커지지 못하게 막는다
  - 완료: builtin query/slot include-chain seam은 TU owners로 승격됐다. `type_checker_builtins_query*.h`와 `type_checker_builtins_slotops.h`는 declaration-only이고, query/world/channel/domain/slotops/secure-token/builtin-resolve behavior는 named `.c` owner가 소유한다

### P10. 속도 / 빌드 성능 baseline

- 문제:
  - 장기 모듈화가 translation unit 수를 늘리면 incremental build는 좋아질 수 있지만 full build/link 또는 generated backend compile 시간이 튈 수 있다
  - 현재 `test-abi-perf`는 존재하지만 raw log가 길어 worst-case 추적이 어렵다
- 기본 방침:
  - `make test-abi-perf`로 benchmark-only ABI/runtime baseline을 캡처한다
  - `make perf-summary PERF_LOG=<log>`로 C/LLVM compile/run 평균과 worst-case를 요약한다
  - representative case는 `tests/bench_backend.sh <source.pgy> dev`로 C/LLVM wall time + RSS를 직접 확인한다
  - generated/native compile waning은 속도 noise가 아니라 build hygiene bug로 보고 즉시 닫는다
- 현재 baseline (2026-04-24, local WSL):
  - `make test-abi-perf`: 320 passed, 0 failed
  - `perf-summary`: C 32 cases, avg compile 0.569s, max 1.783s (`intent_authority_snapshot_abi`), avg run 0.001s
  - `perf-summary`: LLVM 32 cases, avg compile 0.187s, max 0.251s (`projection_abi`), avg run 0.002s
- 진행: `make perf-contract-test-smoke`가 synthetic `test-abi-perf` log를 통해 `perf_summary` log grammar, C/LLVM case count, average compile/run, worst-case compile/run case selection을 CI에서 고정한다. 이 gate는 baseline 숫자 자체를 고정하지 않고, perf evidence가 machine-readable 상태를 유지하는지 검사한다.
  - representative `relation_effect_propagation/main.pgy`: C dev 1.03s / 46MB, LLVM dev 0.72s / 60MB after `realpath` waning fix
- 진행:
  - [x] `tests/perf_summary.sh` 추가
  - [x] `make perf-summary PERF_LOG=<log>` 추가
  - [x] generated C/LLVM compile path의 POSIX `realpath` implicit declaration 경고 제거
- 남음:
  - [ ] CI에서 benchmark-only 수치를 artifact로 저장할지 결정
  - [ ] release/beta notes에 perf-summary baseline 첨부
  - [ ] worst-case compile 2배 이상 증가 시 regression 후보로 자동 표시

### P2. hint namespace 레지스트리화

- 문제:
  - `cause_ir` / `fix_source` literal이 세션 단위로 계속 늘어나는데 중앙 레지스트리가 없다
  - `docs/72`류 문서는 `code` 위주고, `cause_ir` / `fix_source` variant drift를 강제하지 못한다
- 영향:
  - downstream이 diagnostic routing에 이 값을 쓰기 시작하면 오타/drift가 즉시 breaking change가 된다
- 기본 방침:
  - `code`, `cause_ir`, `fix_source`를 모두 registry/enum-like literal set으로 관리
  - 문서와 코드 리뷰 기준에서 “새 literal 추가 시 registry + docs 동시 갱신”을 강제
- 준비 작업:
  - [x] diagnostic literal registry 초안 추가
    - 완료: `src/semantic/diag_codes.h`가 `PGY_CODE_*`, `PGY_CAUSE_*`, `PGY_FIX_*` registry source of truth로 동작하고 `docs/72_diagnostic_codes.md`가 이를 문서화
  - [x] `cause_ir` / `fix_source` 네이밍 규칙 문서화
    - 완료: `docs/72_diagnostic_codes.md`에 `cause_ir` stage/subsystem/condition 규칙과 `fix_source` source-action token 규칙 고정
  - [x] free-form 문자열 신규 추가 지점에 smoke gate 마련
    - 완료: `tests/diagnostic_registry_smoke.sh` / `make diagnostic-registry-test-smoke`가 semantic diagnostic call-site의 `PGY_CODE_*`, `PGY_CAUSE_*`, `PGY_FIX_*` macro 사용과 diagnostic code 문서 sync를 검사

### P3. 타입/ownership 용어 압축

- 문제:
  - anchored handle / movable resource / subject / subject-host / boundary value / capability-bearing / move token 등 용어가 과다
  - 같은 semantic family가 메시지마다 다른 이름으로 노출된다
- 영향:
  - 사용자도 헷갈리고, 구현자도 메시지/문서/테스트 정렬 시 drift가 난다
- 기본 방침:
  - 사용자-facing 핵심 용어를 2-3층으로 압축
  - 세부 분류는 “X의 하위분류”로만 노출
- 준비 작업:
  - [ ] user-facing canonical vocabulary 정리
  - [ ] diagnostics/README/docs 용어 매핑표 작성
  - [ ] old wording grep inventory 후 치환 계획 수립

### P4. 빌드/샌드박스 경로 단순화

- 문제:
  - bash / PowerShell / cmd / MSYS2 / stale object / path rewrite / sed 기반 stamp가 서로 다른 방식으로 깨진다
  - “Nothing to be done” + stale artifact 같은 회귀가 생산성을 크게 깎는다
  - smoke test가 repo root에 runtime artifact를 남기면 dirty worktree와 실제 소스 변경을 구분하기 어려워진다
- 기본 방침:
  - 단일 공식 빌드 경로를 정하고 나머지는 document-only 또는 best-effort로 내린다
  - stale artifact 회피를 위해 강제 재빌드 경로를 공식화
- 준비 작업:
  - [x] 공식 Windows 빌드 경로 1개로 문서화
    - 기준: GitHub Actions `windows-latest` + `msys2/setup-msys2` native MinGW/MSYS2 runtime
    - plain Linux-hosted `gcc`는 `ci-windows` acceptance line이 아님
  - [x] `llvm_smoke.sh`의 `string_io` smoke가 repo root에 `io.txt`를 남기지 않도록 각 case를 source directory에서 실행하게 정렬
  - [x] LLVM runtime object freshness가 split runtime `.inc` subpart 변경을 보도록 `compiler_runtime_cache_is_fresh(...)` dependency list를 확장. `pgy_runtime_lib_part_b_part_d.inc` 같은 하위 include 수정 후 stale runtime object가 링크되는 문제를 차단
  - [ ] `clean && build` 강제 wrapper / recommended entrypoint 정의
  - [ ] stale `.o` / `.d` 진단 가이드와 강제 재빌드 옵션 정리

### P5. printf-style 진단 포맷팅 축소

- 문제:
  - 일부 semantic diagnostic helper는 인자 개수가 매우 많고, placeholder drift에 취약하다
  - 현재 구조는 `fmt 하드코딩 + structured tags(code/cause/fix)`가 이중으로 공존한다
- 기본 방침:
  - 진단 payload를 struct로 모으고, human-readable render는 renderer/helper layer가 담당
  - 최소한 고인자 helper부터 payload-builder 패턴으로 전환
- 준비 작업:
  - [ ] high-arity diagnostic helper inventory 작성
  - [ ] generic mismatch / authority mismatch / ownership escape에서 payload struct 시범 도입

### P6. channel transport 규칙 공통 validator 수렴

- 문제:
  - `type_checker_async_channel.inc`와 builtin/send-query 계열이 ownership/channel transport 규칙을 중복 구현한다
- 기본 방침:
  - channel transport는 공통 validator 하나로 수렴
  - builtin/send wrappers는 surface adapter만 담당
- 준비 작업:
  - [x] send/try-send/send-timeout/status variants 공통 validator 추출
  - [ ] subject / movable / anchored / boundary mismatch wording 통일
  - 진행: named-transfer requirement와 subject/boundary/anchored borrowed-send/mismatch는 `semantic_channel_transfer_requires_named_binding(...)`, `semantic_report_named_channel_transfer_required(...)`, `semantic_validate_channel_transport_ownership(...)` helper로 1차 수렴
  - 진행: token / move-only send-recv restriction wording도 `semantic_report_channel_transport_policy(...)` helper로 정렬 시작
  - 진행: validator/reporting 구현은 `type_checker_async_channel.inc`에서 제거되고 `type_checker_channel_transport.c`가 source of truth가 됐다

### P7. 중간 stage JSON routing closure

- 문제:
  - HIR/DIR/RIR/MIR 실패 경로 일부가 여전히 plain text 중심이라 `단일 JSON 배열` 계약을 깨뜨린다
- 기본 방침:
  - frontend/backend 끝단뿐 아니라 중간 stage 실패도 structured output 계약에 들어오게 한다
- 준비 작업:
  - [ ] HIR/DIR/RIR/MIR failure emitter inventory 작성
  - [ ] plain-text fallback 제거 우선순위 수립

### P8. stale binary / artifact 회귀 고정

- 문제:
  - stale object/dependency 파일 때문에 소스 수정이 반영되지 않는 경우가 있다
- 기본 방침:
  - “빠른 증분 빌드”보다 “신뢰 가능한 재빌드” 경로를 우선
- 준비 작업:
  - [ ] stale artifact 재현 조건 문서화
  - [ ] 권장 빌드 진입점에서 clean rebuild 선택지를 기본 노출

### P9. arena 패턴 명시 도입

- 문제:
  - transpiler / semantic / diagnostics / type rendering 경로에 임시 문자열/버퍼 chun이 많다
  - `malloc/free`와 context-lifetime scratch allocation이 섞여 있어, early-retun/fail path에서 소유권이 산발적이다
  - cache와 임시 문자열이 섞이면 dangling 또는 과도한 copy chun 위험이 커진다
- 기본 방침:
  - arena는 명시적으로 도입한다
  - 단, 전면 치환이 아니라 `scratch arena`와 `result arena`를 수명 기준으로 분리한다
  - cache / long-lived metadata / AST-owned field에는 arena-owned 포인터를 저장하지 않는다
  - arena 간 교차 참조는 raw pointer보다 `index` / stable handle 참조를 기본으로 한다
  - arena는 최소한 `transpiler`, `semantic scratch`, `semantic result`, 필요 시 `type/render scratch`처럼 역할/수명별로 분리한다
  - 타입/역할별 arena 분리는 “누가 free하느냐”보다 “언제 reset되느냐”를 기준으로 설계한다
  - 첫 단계는 transpiler / semantic diagnostics / type render helper의 scratch allocation 수렴이다
- 이 결정이 맞는 이유:
  - 현재 코드베이스는 long-lived cache와 short-lived formatting string이 강하게 섞여 있어, raw pointer 공유보다 index 참조가 훨씬 안전하다
  - Pergyra는 early-retun/fail path와 pass-local scratch data가 많아서, 단일 arena보다 역할/수명별 arena 분리가 디버깅과 reset 비용 면에서 낫다
  - 즉, `Arena + Index 참조 + 타입별 arena 분리`가 지금 구조 debt를 줄이는 가장 보수적이고 안정적인 방향이다
- 준비 작업:
  - [x] `scratch arena` / `result arena` lifetime 규칙 문서화
  - [x] arena 간 cross-reference를 `index` / stable handle 기준으로 문서화
  - [x] `TranspilerCtx` scratch arena 적용 범위 확정
  - [x] semantic analyze pass용 scratch arena 도입 지점 정리
  - [x] diagnostic payload/result-owned arena 분리 여부 결정
  - [x] 타입/역할별 arena 분할안 초안 작성
  - [x] `strdup_fmt` / type render / projection path / generic formatter helper의 arena 전환 우선순위 작성
  - [x] cache에 arena-owned 포인터 저장 금지 규칙 문서화
  - [x] 첫 vertical slice:
    - transpiler temporary strings
    - semantic diagnostic formatting scratch strings
    - type-name rendering scratch helpers
  - 진행: `docs/94_arena_index_lifetime_plan.md`로 방향 고정
  - 진행: `TranspilerCtx`의 `arena`를 scratch arena로 명시
  - 진행: transpiler scratch-only temporary 1차 vertical slice 완료
    - zone authority temporary expression
    - intent priority default literal
    - projection refresh `source_expr`
    - event declaration `event_type`
  - 진행: semantic diagnostics result seam 1차 도입
    - `Diagnostic`가 optional payload snapshot을 보존
    - payload emit 경로는 result-owned snapshot으로 복사
    - semantic JSON 출력도 payload 필드를 함께 노출 가능
  - 진행: semantic scratch arena 1차 도입
    - `SemanticContext`에 scratch arena 추가
    - ownership diagnostic path string은 scratch arena를 우선 사용
    - payload snapshot이 result로 복사하므로 helper 내부 free chun 제거
  - 진행: LLVM arena lane 1차 closure
    - `LLVMGenCtx`는 `scratch` + `persistent` lane으로 분리
    - `LLVMGenResult`는 result-owned arena를 보유
    - intent MIR collector / projection path / local grow helper / event invoke / type render helper가 scratch로 수렴
    - synthetic event-handler AST field 저장은 callable signature registry로 치환
    - `*error_message` heap retun contract는 result-owned lane으로 수렴
    - 남은 heap 경계는 owner shell(`ctx`, registry destroy, result outer shell)과 runtime ABI contract 수준으로 축소
    - 진행: intent observability(`last/history/active/recent`)와 authority failure snapshot의 stable runtime string exports는 `runtime-borrowed string` ABI로 고정했다. caller는 free하지 않고 다음 runtime registry/snapshot mutation 전까지만 유효하다
    - 진행: `runtime-abi-lifetime-test-smoke`가 stable intent last/history/active/recent 및 authority 문자열 export body에서 allocation/free/strdup이 발생하지 않도록 검사한다
    - 진행: stable string helper retuns는 `result-owned string`, stable string-array helper retuns는 `result-owned array` ABI로 고정했다. `runtime-abi-lifetime-test-smoke`가 helper payload가 borrowed input pointer, stack buffer, string literal을 반환하지 않고 allocation/copy된 payload를 반환하는지 검사한다
    - 진행: stable file descriptor는 `runtime-owned handle` ABI로 고정했다. `pgy_file_open`은 닫힌 runtime table slot을 재사용하고, `pgy_file_close`는 table entry를 NULL로 비워 재사용 가능 상태로 만든다. `runtime-abi-lifetime-test-smoke`가 이 release/reuse contract를 검사한다
    - 남음: file descriptor 외 runtime-owned handle ownership도 같은 수준의 smoke/문서 계약으로 확장해야 한다
  - 주의: 반환 계약이 있는 expression string은 아직 arena로 옮기지 않음
  - 주의: `slot_ref_expr(...)` scratch 전환 시도는 되돌림. 반환 ownership 경계를 먼저 나눠야 함

### 최근 closure 진행 (2026-04-18)

- declaration-side MIR-only host context를 더 정리
  - transpiler host context가 `current_host_decl -> within_zone -> saved host-name inventory` 순으로 복원되도록 정렬
  - class/zone/relation/effect/world field query helper가 raw host-name state보다 inventory-backed host handle을 우선 사용
  - direct `current_*_name` restore chain 일부를 `transpiler_restore_host_context_local(...)` helper로 접어 산발적 context 복구 코드를 축소
  - emitter hot path의 direct `current_*_name` 참조는 대부분 걷어내고, 남은 사용처를 helper/restore layer로 국소화
  - LLVM declaration helper도 current host lookup을 공용 active-inventory host helper로 접어 naming chain을 축소
  - LLVM MIR/domain emission의 direct `current_class_name` save/restore도 host-name bind/restore helper로 접어 state 관리 중복을 줄임
  - LLVM expr/stmt hot path도 `llvm_current_host_decl_name(...)` 기준으로 정렬해 direct raw host-name read를 더 줄임
  - `HasProjection/HasLayer/HasState/HasZone*` 및 method/field helper가 raw `current_class_name` 대신 host helper를 통과하도록 정리
  - LLVM pipeline의 nominal registration / class method emission도 raw nominal AST array보다 `mir->decl_headers`를 직접 순회하도록 정렬
  - LLVM domain pass도 raw `ctx->mir->{relations,effects,zones,...}` 직접 접근 대신 `llvm_active_domain_inventory(...)` helper를 통과하도록 정렬
  - 즉, declaration-side debt는 이제 emitter 본문보다 inventory bootstrap + helper/restore layer 국소 부위로 더 압축됨
  - C transpiler domain/hosted method emission도 `emit_hosted_methods_from_mir_or_error_local(...)` helper로 수렴
  - party / roster / relation / effect / zone / world method emit는 같은 MIR routine gate와 같은 explicit backend error 정책을 사용
  - relation/effect/zone/world method의 dead AST signature fallback 제거
  - party / roster / relation / effect / zone / world declaration emit entrypoint는 inventory decl을 우선 사용
  - bootstrap residual은 이제 per-domain AST array 직접 순회보다 inventory-backed bootstrap helper 본체 쪽으로 더 압축
- generic contract + type-resolution DAG 회귀를 더 넓힘
  - `role impl ability` 경로가 generic default/where-bound cycle provenance regression에 추가됨
  - 즉, action/intent-step/zone-authority/party-role-slot에 더해 role impl consumer도 staged DAG path 회귀 범위에 포함
- 현재 검증선
  - `test-semantic`: `1617 passed, 0 failed`
  - `test-transpile`: `670 passed, 0 failed`
  - `test-abi`: `84 passed, 0 failed`
  - `ci-linux`: full green 유지
  - LLVM expr/stmt host-helper 정리 이후에도 `test-transpile`, `test-abi` 재통과 확인

### 최근 closure 진행 (2026-04-24)

- runtime propagation/provenance 1차 closure
  - C/LLVM domain hidden cell이 `ready/dirty` bool만 가지던 상태에서 `epoch/cause` provenance cell까지 같은 schema로 확장됨
  - relation/effect/zone/world projection, layer, state, world-derived state가 recompute 시점에 cause-stamped provenance를 남기도록 C/LLVM이 정렬됨
  - LLVM domain struct layout이 그동안 빠뜨리고 있던 `__projection_dirty_*` field를 relation/effect/zone에 다시 포함하도록 parity 수정
  - LLVM projection sync도 C와 같은 dirty-gated recompute 경로로 정렬됨
  - LLVM host-field assignment가 zone/relation/effect host method 안에서 projection invalidation을 만들도록 복구
  - LLVM intent step rebound-zone 경로도 effective zone projection cell을 보수적으로 dirty-mark + sync 하도록 보강
  - 결과: `relation_effect_propagation_abi`, `intent_zone_binding`, `intent_cross_world_transfer`, `intent_rich_history_identity` backend compare drift 제거
  - 새 회귀: transpile domain async/world tests가 provenance hidden field와 stamp write까지 직접 확인
  - 새 진행: `world` derived-state recompute가 C/LLVM 양쪽에서 bounded pass loop를 가지도록 올라왔고, single-pass declaration-order replay에만 의존하지 않게 됨
  - 새 진행: bounded recompute pass-limit overflow는 C의 `PGY_PANIC`과 LLVM의 `abort()` 경로로 hard-fail되도록 고정됨
- 새 회귀: transpile world-derived chain test + `world_fixpoint_abi` smoke가 C/LLVM 양쪽에서 녹색
- 현재 해석: runtime propagation provenance baseline(`dirty/ready + epoch/cause`)은 이제 beta 계약의 일부로 간주하고 다시 약화시키지 않음
- 추가 closure: zone lifecycle sync도 이제 C/LLVM 양쪽에서 bounded frontier loop를 가지며, state/layer replay가 single-batch에만 묶이지 않는다
- 추가 closure: embedded world-zone source assignment도 이제 projection dirty mark 뒤에 같은 tun의 zone sync를 태워 stale `ready/value` drift 없이 projection recompute를 닫는다
- 추가 회귀: `world_embedded_projection_abi`, `world_embedded_method_projection_abi`, `world_embedded_branch_projection_abi`가 C/LLVM ABI smoke에서 녹색이며 embedded zone projection read-after-mutate path를 straight-line assignment, method-call, branch-join slice까지 잠근다
- 추가 회귀: `handoff_projection_frontier_abi`가 C/LLVM ABI smoke에서 녹색이고 `handoff_projection_frontier`가 backend-compare에서 녹색이다. v1 handoff materialization 이후 source projection은 source snapshot을, target projection은 target mutation 결과를 보도록 잠근다
- 추가 회귀: `handoff_world_state_frontier_abi`와 `handoff_world_state_frontier`가 C/LLVM에서 녹색이다. active world-owned zone을 `transfer:` 대상으로 넘긴 뒤 projection-backed world state와 `all` composed state가 같은 tick에서 fresh하게 보이는 최소 frontier를 잠근다
- 추가 회귀: `handoff_layer_state_frontier_abi`와 `handoff_layer_state_frontier`가 C/LLVM에서 녹색이다. `transfer:` 이후 action-caused effect가 target zone layer/state와 active world-derived layer/state alias까지 같은 tick에서 fresh하게 전파되는 경로를 잠근다
- 추가 회귀: `world_embedded_action_frontier_abi`와 `world_embedded_action_frontier`가 C/LLVM에서 녹색이다. embedded world-zone subject action call이 action-caused effect layer/state와 active world-derived layer/state alias까지 같은 tick에서 fresh하게 전파되는 경로를 잠근다
- 추가 회귀: `world_embedded_action_pool_frontier_abi`와 `world_embedded_action_pool_frontier`가 C/LLVM에서 녹색이다. embedded world-zone subject action call의 fixed-capacity effect pool 경로도 같은 frontier 계약으로 잠근다
- 추가 closure: authority/failure handoff queryable baseline은 `intent_authority_snapshot(_abi)`와 `authority_failure(_abi|_surface)`가 C/LLVM 양쪽에서 잠그며, authority reject가 process abort 대신 `last_ok / zone / participant / code / reason` 상태와 intent failure trace로 내려오는 최소 recoverable path를 가진다
- 강한 남은 과제: full bounded fixpoint / transitive frontier scheduler는 **명시적 beta blocker**로 유지. 다만 stable world outer frontier는 C/LLVM 양쪽에서 `pgy_frontier_world_transitive_pass_limit(...)`를 소비하도록 올라왔으므로, 남은 debt는 zone/world frontier loop의 부재나 authority/failure handoff 최소 baseline 부재가 아니라 더 넓은 world-zone propagation family를 같은 source-of-truth frontier policy로 일반화하는 일이다
- 추가 closure: relation/effect/zone projection sync도 bounded transitive recompute loop로 올라왔고 declaration order에 기대지 않는다
- 추가 회귀: `projection_chain_abi`가 C/LLVM ABI smoke, `make test-all`, `make llvm-test-backend-compare`에서 잠겼다
- 추가 gate: `make runtime-frontier-contract-test-smoke`가 C emitter와 LLVM emitter에서 world derived-state bounded recompute, zone lifecycle bounded frontier loop, projection-chain bounded recompute, embedded world-zone action-caused layer/state freshness, authority/failure handoff queryable baseline, pass-limit overflow hard-fail, ABI smoke 등록, backend-compare 등록을 검사한다. 또한 `src/codegen/domain_frontier_policy.h`의 pass-limit source-of-truth helper와 `pgy_frontier_world_transitive_pass_limit(...)`를 C/LLVM emitter가 소비하는지 확인하고, `make runtime-frontier-policy-test-smoke`가 saturating pass-limit arithmetic을 실제 컴파일/실행으로 잠근다. 이 gate는 full bounded fixpoint / transitive frontier scheduler가 다시 single-pass 구현이나 non-queryable authority failure로 후퇴하지 못하게 막는 beta blocker gate다. 남은 runtime propagation closure는 broader world-zone propagation family를 같은 source-of-truth frontier policy로 일반화하는 일이다
- Beta readiness source of truth: `docs/100_beta_readiness_checklist.md` records
  the current live blocker map and closure order. `docs/98_beta_closure_readiness_report.md`
  remains a historical snapshot from the earlier 50% readiness line and should
  not be cited as the current verdict.

### 최근 closure 진행 (2026-04-23)

- AST 타입 디스패치 partition 규칙 공식화 — `docs/95_ast_dispatch_partition.md`
  - 전체 AST 타입 (현재 93종) 을 4 카테고리 (type annotation / decl sub-metadata / top-level decl / root) disjoint 분할
  - 각 카테고리별로 "왜 특정 switch 에서 도달 불가인지" 의 **파서 invariant 근거** 를 문서화
  - case label 추가/금지/safety-net 결정 기준 확정
  - 새 AST 타입 추가 시 체크리스트 포함
  - `llvm_stmt.c` 의 top-level decl skip 리스트 + Zone/World forward 가 이 문서 기준으로 정렬됨 (`AST_INTENT_DECL` skip 누락 수정, Zone/World 11종 forward 주석 정확화, `llvm_expr.c` explicit diagnostic 유지)
  - 새 AST 타입 추가 시 docs/95 업데이트 책임 명시

### 최근 closure 진행 (2026-04-22)

- arena scratch slice 3건 추가 흡수 — `docs/94_arena_index_lifetime_plan.md` 업데이트
  - `semantic.c:50` `semantic_preload_stdlib_uses` 의 per-iteration `malloc/free` module path 조립을 function-local `PgyArena` 로 이동. 배치 alloc 하나로 수렴
  - `type_checker.c:1109` enum method name mangling의 `malloc/snprintf/free` 를 `pgy_arena_fmt(&ctx->scratch_arena, ...)` 로 이동. `symbol_create_function` 이 이미 내부 `pergyra_strdup` 으로 이름을 복사하므로 arena 탈출 없음
  - `slot_analyzer.c:1067` `slot_analyze_parallel_block` 의 outer task metadata 배열 3종 (`task_accesses`/`task_counts`/`task_caps`) 을 `sa->ctx->scratch_arena` 로 이동. per-task inner 배열은 여전히 `collect_slot_accesses` 가 heap-owned로 관리
- arena scratch 2차 slice 추가 (같은 날)
  - `type_checker.c:355` type resolution cycle detection 의 `visited`/`path` 배열 → `ctx->scratch_arena`. cycle text는 retun-contract helper라 보류
  - `type_checker_flow.c:499` match redundancy 의 `seen` 배열 → `ctx->scratch_arena`
- arena scratch 3차 slice — HIR/MIR 첫 진입 (같은 날, 이후 4차에서 routine-scope로 통합됨)
  - `hir.c:hir_compute_cfg_dominance` 의 `visited`/`postorder`/`idoms` 3배열 → function-local `PgyArena`
  - `hir.c:hir_mark_natural_loop` 의 `in_loop`/`stack` 2배열 → function-local `PgyArena`
  - `mir_ssa_rename.h:mir_apply_ssa_rename` outer 3배열 → function-local `PgyArena`
- arena scratch 5차 slice — LLVM 백엔드 첫 진입 (같은 날, 이후 6차에서 ctx-scope 로 통합)
  - `llvm_register.c:llvm_register_enum_decl` 의 `enum_fields` + per-variant `payload_fields` type-ref 버퍼를 function-local `PgyArena` 로 수렴
  - `llvm_intent.c:llvm_collect_mir_intent_participants` 는 retun-ownership 계약이라 deferred
- arena scratch 6차 slice — **LLVMGenCtx ctx-scope scratch arena 도입** (같은 날)
  - `LLVMGenCtx` 에 `PgyArena scratch` 필드 추가
  - `llvm_ctx_create` / `llvm_ctx_destroy` 에서 lifecycle 관리
  - 5차에 function-local 로 시작한 enum type-ref arena 를 `ctx->scratch` 로 수렴. LLVMGenCtx 하나당 init/destroy 한 번만
  - 후속 LLVM scratch 사이트 (미래에 발굴되는) 도 이 arena 재사용 가능
- arena scratch 7차 slice — **LLVM 9 사이트 일괄 흡수** (같은 날)
  - tuple literal (`llvm_expr.c`) 의 vals + tys
  - event handler type / tuple type (`llvm_backend.c:ast_type_to_llvm`) 의 param_types + fields
  - event INVOKE (`llvm_domain.c`) 의 inv_params + call_args
  - class/enum/exten 등록 (`llvm_register.c`) 의 4 param-type 버퍼
  - ability vtable (`llvm_domain.c`) 의 outer vt_fields + per-method ptypes
  - 공통: LLVM C API 가 type/value 배열을 내부 복사하므로 scratch-safe
  - 결과: LLVM 전체의 short-lived type 배열 assembly 가 ctx arena 하나로 수렴
- arena scratch 8차 slice — **LLVM 17 사이트 추가 흡수** (같은 날)
  - `llvm_stmt.c`: lambda param, parallel closure ctx/wrapper/handles, async closure fields, select rotation BBs
  - `llvm_intent.c`: intent function param_types, step completion `completed_allocas`, `saved_participant_ptrs`
  - `llvm_domain.c`: world sync `prev_active_addrs`, domain struct `ftypes` (4 분기), role/class method `ptypes` (2 사이트), vtable `vals`
  - LLVM 쪽 scratch-safe calloc/malloc 은 거의 전수 `ctx->scratch` 로 수렴. 남은 것은 retun-ownership 혼재 helper 와 AST-field stored 케이스

- arena scratch 4차 slice — **HIR/MIR routine-scope arena 도입** (같은 날)
  - `hir.h` HIRRoutine / `mir.h` MIRRoutine 에 `PgyArena scratch` 필드 추가
  - 생성: `hir_append_*`, `mir_lower` 루프 내 `memset` 직후 `pgy_arena_init(&routine.scratch, 0)`
  - 파괴: `hir_destroy()` / `mir_destroy()` per-routine cleanup + OOM 경로 (배열 편입 실패 케이스)
  - 3차에 function-local 로 시작한 3개 arena 를 모두 `&routine->scratch` 로 통합 → routine 하나당 init/destroy 한 번만. 여러 HIR/MIR pass 가 같은 arena 를 재사용
  - MIR pass는 `routine->scratch` 만 씀. `routine->hir_routine->scratch` 는 HIR frozen 계약이라 접근 금지 (코멘트로 고정)
- 원칙 유지: `scratch-only local temp 먼저, retuned string 나중`. `slot_ref_expr(...)` 같은 반환 ownership 혼재 helper는 아직 보류
- 베타 acceptance line #8 ("scratch/result lifetime과 cache boundary가 문서/구현 기준으로 설명 가능하다") 에 해당 slice 기여

### 최근 closure 진행 (2026-04-21)

- C/LLVM init idiom 축 감사 + 1차 정비 완료 (`docs/93_codegen_idiom_audit.md`)
  - 6 case × 2 backend 매트릭스 고정
  - **Case 1 HIGH divergence 해소**: 함수-바디 `let x: T;` (annotation + no init)을 `PGY_CODE_SEM_UNINIT_LOCAL` 로 거부. C는 scalar-zero, LLVM은 store 생략으로 첫 read에서 값 의미가 갈라지던 잠복 경로를 semantic 레벨에서 차단
  - **Case 2 C backend L815 정리**: `transpiler_c_type_uses_scalar_zero` helper로 scalar/aggregate 분기. 기존 잠복 버그 (`struct Foo x = 0;` invalid C) 제거 (defense in depth)
  - **Case 3 MEDIUM 의도 비대칭으로 확정**: slot claim은 C가 런타임 helper, LLVM이 IR-direct. 현재 runtime observability 수준에서 관측 side effect 0. runtime observability 확장 시 재감사로 deferral
  - 회귀 3종 추가:
    - `function-body let with annotation and no initializer is rejected`
    - `function-body let with aggregate annotation and no initializer is rejected`
    - `subject field let with no initializer does not trigger the uninit-local guard` (negative)
  - 파서 구조 재확인: class/subject field는 ClassField 경로로 분리되어 `AST_LET_DECL`이 아님 → guard가 field-level 의미를 침범하지 않음
  - docs/72 에 `PGY_SEM_UNINIT_LOCAL` 섹션 + docs/93 cross-link 추가

### 최근 closure 진행 (2026-04-20)

- own/ref broader audit를 helper family 기준으로 더 정렬
  - helper call boundary의 `subject` / general boundary value 경로를 공용 borrowed-boundary validator로 접음
  - container store / array literal store borrow-escape를 공용 ownership diagnostic helper로 통합
  - semantic channel send borrow-escape도 공용 ownership diagnostic helper로 승격
  - 즉, `assignment / helper call / channel send / container store / array literal store / constructor field store`가 점점 같은 provenance wording family로 수렴 중
- intent authority mismatch provenance를 더 직접적으로 노출
  - `authorized by` unknown participant / non-subject participant / zone subject-slot mismatch / zone authority mismatch에 `approval boundary provenance` 섹션 추가
  - provenance가 비어 있으면 `no inherited/derived authority provenance was recorded`를 명시적으로 보고
- relation/effect/projection failure depth를 추가 보강
  - invalid projection source / tobject source rejection이 target/source consumer path와 projection contract origin을 직접 보고
  - 즉, projection diagnostics가 단순 type mismatch가 아니라 `target slot <- source slot` 경로를 기준으로 설명되기 시작함
- 현재 베타 blocker 재정렬
  - Windows backend-compare / LLVM parity 복구
  - declaration-side MIR-only 남은 host/inventory helper debt 제거
  - own/ref 일반화의 broader assignment / container / rebind / summary path closure
  - intent/zone/world 및 relation/effect/projection provenance 마지막 심화
- Windows-native compile hygiene를 추가 정리
  - `type_checker_builtins_query.inc`, `type_checker_builtins_nominal.inc`의 `%zu` / extra-arg formatting drift를 제거
  - `type_checker_decls_world.inc`의 world lifecycle diagnostics placeholder-arg mismatch를 제거
  - `type_checker_builtins.c`는 ownership/channel helper를 full intenal header include 대신 최소 forward declaration으로 고정해 enum/static helper 재선언 충돌을 피함
  - 현재 기준선:
    - `test-semantic`: `1855 passed, 0 failed`
    - `test-transpile`: `601 passed, 0 failed`
  - 남은 Windows blocker는 semantic compile 단계가 아니라 native MSYS2/MinGW 실행 환경에서의 backend/runtime parity 확인 축으로 이동

### 최근 closure 진행 (2026-04-16)

- declaration-side host context를 inventory-backed handle 쪽으로 한 단계 더 정렬
  - transpiler host lookup이 `current_host_decl -> within_zone -> saved host-name inventory` 순으로 복원되도록 조정
  - zone/relation/effect/world field query helper가 raw `current_*_name` 분기보다 inventory-backed `current_host_decl`를 우선 소비
  - 즉, declaration-side C backend context 복원에서 string name state는 점점 restore hint로만 남고, 실제 host truth는 active inventory 기반 handle로 수렴 중
- explicit/compressed canonical pair examples를 intent-first 독해 규칙으로 다시 정렬
  - large/composite pair source에 `intent -> world/zone -> subject` read order를 직접 명시
- world embedding implicit copy를 waning이 아니라 hard contract로 승격 시작
  - world constructor에 zone binding을 그대로 넘기면 explicit `Clone(...)`를 요구
  - hidden copy semantics를 더 이상 benign waning으로 남기지 않음
- generic contract consumer path를 한 단계 더 닫음
  - omitted trailing default type arg가 user-defined generic class specialization path에서도 effective arg 기준으로 검증되도록 정렬
  - role impl / action requires / zone authority / party role slot에서 `default arg omission + where-bound violation` negative regressions 추가
  - multi-bound / omitted-default / consumer provenance 조합 회귀를 semantic 기준으로 고정
  - ability consumer path / class instantiation-specialization path에서 unresolved effective generic arg를 silent skip하지 않고 structured error로 승격
  - role-side ability require-field type resolution에서도 unresolved effective generic arg를 silent skip하지 않고 structured error로 승격
  - malformed impl ability generic arg가 있어도 뒤쪽 where/require-field 검증으로 partial 진행하던 경로를 차단
  - default generic bound validation에서 unknown parameter / unresolved default type도 structured error로 승격
  - generic function call-site where-clause validation에서도 missing/unresolved effective arg를 silent skip하지 않고 structured error로 승격
- own/ref 첫 일반화 vertical slice 시작
  - existing movable resource value(`QubitSlot`)는 function boundary에서 explicit `own` transfer parameter를 허용
  - `ref QubitSlot`는 아직 미닫힘 subset으로 유지하되, 이유/consumer path/fix가 포함된 structured diagnostic으로 고정
  - 즉, `own/ref`는 여전히 전역 closure 전이지만, move semantics가 이미 있는 resource value에 대해서는 explicit transfer boundary가 부분적으로 열리기 시작함
  - retun/channel boundary ownership diagnostics도 `Reason:` / `Fix:` 구조로 정렬
  - function signature anchored-retun rejection도 `Reason:` / `Fix:` 구조로 정렬
  - unnamed movable-resource channel send는 moved-here provenance를 설명하는 hard error로 고정
  - local binding 단계에서도 `recv/await` unnamed boundary use, subject rebinding, released-slot move, anchored-handle rebinding을 `Reason:` / `Fix:` 구조로 정렬
  - slot escape analyzer 경고도 retun/helper-call/channel/unterminated local claim 경로에서 provenance형 `Reason:` / `Fix:` 구조로 정렬
- relation/effect/projection contract를 더 하드하게 조였다
  - `intent step causes`가 zone effect slot 없이 통과하던 경로를 hard error로 승격
  - `action causes`도 zone effect slot 없이 남는 경로를 structured hard error로 승격
  - authority-bearing `apply/link/detach/unlink/maintain`가 `by <subjectSlot>` 없이 남는 경로를 hard error로 승격
  - duplicate authority, unknown layer relation/effect type도 더 이상 benign waning으로 남기지 않음
  - maintain/detach/unlink duplicate/conflict diagnostics는 `Reason:` / `Fix:` 구조로 정렬
- unresolved declaration entrypoint를 더 줄였다
  - role include unknown role, roster slot unknown party, world roster/zone unknown type을 hard error로 승격
  - generic where-clause consumer path에서 unresolved effective arg도 더 이상 silent skip하지 않음
- declaration-side MIR-only domain method gate를 더 조였다
  - party / roster / relation / effect / zone / world method emission이 MIR routine 없이 AST body로 조용히 fallback하지 않도록 C backend를 정렬
  - role / domain method emission에서 MIR routine 미존재를 LLVM backend hard error로 승격
  - 즉, declaration-side domain method는 MIR inventory가 존재하는 빌드에서 silent fallback이 아니라 explicit backend failure를 계약으로 삼음

### 최근 closure 진행 (2026-04-14)

- declaration-side MIR-only intent inventory를 더 밀었다
  - MIR가 `IntentParticipant(alias,type)` metadata를 직접 운반
  - C/LLVM intent declaration emission이 participant alias/type를 AST 재해석 없이 MIR metadata로 우선 소비
- step-level MIR-only validation을 AST field 존재 검사에서 metadata 존재 검사로 옮겼다
  - `IntentCheck`
  - `IntentEval`
  - `IntentZoneWhere/IntentZoneAlias/IntentZoneFrom`
  - `IntentWho/IntentDispatch`
  - `compensate` 존재 판정
- intent emission cleanup/rollback 경로의 metadata gate를 C/LLVM 둘 다 정렬했다
- 관련 회귀:
  - `test-mir` green
  - `test-transpile` green

즉, intent declaration/step emission은 아직 완전 MIR-only 선언이 끝난 것은 아니지만,
`participant/step contract inventory`를 AST presence에 기대던 가장 거친 fallback는 한 단계 더 제거됐다.

### 베타 기준판 추가 (2026-04-15)

- `docs/70_beta_closure_master_board.md` 추가
  - B0 4축, declaration-side MIR-only debt, parity, runtime observability, surface trust를 한 장으로 고정
  - 베타 acceptance line과 exit rule을 명시
  - 앞으로 TODO의 개별 작업은 이 보드 기준으로 우선순위를 따른다

### 베타 최종 관문 (2026-04-18)

- [ ] **declaration-side MIR-only를 구조적으로 닫기**
  - zone/world/relation/effect declaration/method emission에서 남은 AST/HIR-carried inventory dependency를 더 제거
  - `current_*_name` / host-name 추정 helper보다 inventory-backed host handle / metadata 소비를 우선하도록 정렬
  - transpiler/LLVM 양쪽에서 raw host-name read를 helper/restore layer 밖으로 다시 새지 못하게 회귀로 고정
  - declaration emission failure는 comment/skip/fallback retun이 아니라 explicit backend error로 승격
  - C/LLVM 둘 다 declaration-side path에서 `Unknown` / surface-trust-breaking fallback type emission을 계속 제거
  - 문서에서 `MIR-led / HIR-assisted`라고 남겨둔 debt를 실제 구현 기준으로 더 축소하고, 베타 시점 표현과 구현을 일치시킨다

- [x] **AST dispatch / backend fallback trust gate 고정**
  - `docs/95_ast_dispatch_partition.md` 기준으로 AST 타입 partition을 문서화
  - LLVM `stmt/expr` default path는 waning-only가 아니라 structured backend error로 고정
  - Zone/World declaration verb가 expression fallback으로 조용히 `0/null`이 되는 경로를 explicit backend diagnostic으로 차단
  - `tests/ast_dispatch_partition_smoke.sh`와 `make ast-dispatch-test-smoke`를 추가해 partition drift와 silent fallback 회귀를 CI에서 차단
  - Linux `ci-linux` acceptance line에 AST dispatch smoke를 연결

- [x] **type-resolution DAG를 beta blocker로 포함**
  - import resolver와 별개로 semantic type dependency graph를 beta acceptance line에 포함
  - generic default / multi-bound / role impl / action / intent step / party role slot / zone authority / module contract consumer를 같은 graph inventory로 추적
  - alias depth limit / ad-hoc recursive failure보다 path-aware cycle diagnostic을 우선 기준으로 끌어올림
  - 1단계 진행: `topo_order`를 버리지 않고 declaration staged worklist에 연결 시작
  - 반영 문서:
    - `docs/70_beta_closure_master_board.md`
    - `docs/63_feature_depth_matrix.md`
  - 1단계 진행: `world/zone` local contract와 `refresh` projection path를 synthetic graph node로 올리기 시작
  - 1단계 진행: topo worklist가 `LOCAL_CONTRACT` / `PROJECTION_PATH` synthetic node도 다시 소비하기 시작
  - 1단계 진행: synthetic node 소비를 host 전체 재실행이 아니라 label별 narrow handler로 축소
  - 1단계 진행: role impl consumer까지 cycle provenance 회귀를 추가해 ability consumer family를 더 완성
  - 남은 일: staged declaration prepass 범위를 넓히고 graph-backed evaluator를 semantic source-of-truth로 승격
  - ecosystem 확장(`stdlib/pkg/tooling`)은 이 DAG closure 이후 단계로 미룸

- [x] **own/ref 일반화 audit 마감**
  - own/ref는 ownership classifier 기준 stable subset으로 닫힘
  - borrowed value escape는 helper call / channel / retun / container store뿐 아니라 broader assignment/member/store path까지 provenance 기준으로 점검
  - 진행: constructor field store(`Holder(packet)` 같은 boundary-visible store)를 borrowed escape 경로로 승격하고 semantic regression 추가
  - 진행: constructor field store도 borrowed member/aggregate source path provenance(`holder.packet`, `items[0]`)를 직접 보고하도록 정렬
  - 진행: array literal store(`[packet]`)도 borrowed escape 경로로 승격하고 semantic regression 추가
  - 진행: member assignment / array overwrite 진단이 identifier-only가 아니라 `holder.packet`, `items[0]` 같은 target path provenance를 직접 보고하도록 정렬
  - 진행: new-binding escape도 identifier-only가 아니라 borrowed member/aggregate source path provenance(`packet.view`, `items[0]`)까지 추적하도록 확장
  - 진행: new-binding escape regression도 member source path(`packet.items`)와 array source path(`items[0]`)를 fixture로 고정
  - 진행: container store(`ArrayPush`/`ListPush`/`SetAdd`/`QueuePush`/`MapSet`)도 borrowed member/aggregate source path provenance를 직접 보고하도록 정렬
  - 진행: helper forwarding / builtin channel send(`Send`/`TrySend`/`SendTimeout`/status variants)도 unnamed borrowed member/aggregate source path provenance를 직접 보고하도록 정렬
  - 진행: direct `retun` escape도 borrowed member/aggregate source path provenance(`holder.packet`, `items[0]`)를 직접 보고하도록 정렬
  - 진행: slot/resource summary 기반 `retun/channel/helper` diagnostics도 `summary provenance root` vocabulary로 direct semantic wording에 더 가깝게 정렬
  - 진행: summary-based helper escape는 direct callee wording 대신 `helper/function summary in '<fn>'` 경로로 분리해 drift를 줄임
  - 진행: summary-based retun/channel escape도 direct consumer wording 대신 `retun summary in '<fn>'` / `channel summary in '<fn>'` 경로로 분리해 drift를 줄임
  - 진행: anchored-handle summary escape도 direct `retun/channel/helper` wording 대신 summary wording으로 분리해 own/ref bridge 문구를 정렬
  - 진행: helper-call / container-store / array-literal-store / semantic channel-send diagnostic family를 공용 helper로 통합
  - 진행: nested projection + transitive helper + member rebind 조합도 semantic regression fixture로 추가
  - 진행: movable-resource + nested member source + member rebind target 조합도 semantic regression fixture로 추가
  - 진행: declaration-side MIR-only host truth는 `current_host_decl` / inventory 기준으로 더 좁혔고, `within_zone`를 따라가는 transpiler host recovery fallback과 role-owner direct AST lookup을 제거
  - 진행: own/ref anchored-handle wording을 assignment / let-binding / retun / channel / helper family에 맞춰 `boundary-visible handle binding` / `anchored-handle provenance` 기준으로 정렬
  - 완료 판정: direct/summary helper-chain, retun/channel/helper, destructure, assignment/member/container/constructor/array path가 current semantic regression으로 고정됨
  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: region/lifetime solver와 universal ownership lattice

- [ ] **generic contract 전경로 audit 마감**
  - generic contract는 `default type arg`, `multi-bound where`, `ability<T> consumer`, `zone authority`, `party role slot`, `impl/reference`, cross-module consumer path를 마지막까지 audit
  - 진행: `party role slot` generic mismatch consumer도 actual/expected type arg + consumer path provenance regression으로 고정
  - 남은 generic consumer path가 없다는 것을 regression으로 증명하고, partial acceptance처럼 보이는 경로를 남기지 않는다

- [ ] **Intent/Zone/World, relation/effect/projection 진단과 provenance 마감**
  - intent/zone/world의 embedding / handoff / authority mismatch에서 contract source, derived zone/using, transfer edge provenance를 계속 강화
  - relation/effect/projection은 propagation edge failure, contract mismatch, branch/join/handoff path에 `Contract source:` / `Reason:` / `Fix:`와 source/target provenance를 일관되게 부착
  - 진행: world embedding/handoff와 intent transfer/authority mismatch의 핵심 경로를 `Contract source:` / `Reason:` / `Fix:` 구조로 재정렬
  - runtime contract provenance와 diagnostic wording을 더 정렬해 “왜 실패했는지 + 계약이 어디서 왔는지 + 어떻게 고칠지”를 한 번에 보이게 한다
  - helper-heavy edge path를 줄이고, compile-time contract 실패를 silent/best-effort runtime sync로 넘기지 않는다
  - 진행: intent step contract-source summary가 `authorized by`, transfer handoff, derived transfer zone provenance를 더 직접적으로 설명하도록 정렬
  - 진행: zone-within action authority mismatch가 `within` / `causes` header를 contract source로 직접 보고하도록 정렬
  - 진행: world embedding / post-embedding mutation diagnostics가 `world <name> zone slot <slot>` contract source와 world-owned authority/handoff destination을 직접 보고하도록 정렬

- [ ] **C/LLVM parity + full CI green을 베타 최종 관문으로 고정**
  - Linux 기준 `parser / semantic / transpile / ABI / backend-compare / llvm smoke / ir-pipeline / example smoke`를 full green으로 유지
  - Windows는 로컬 Linux host에서 강행하지 않고, MSYS2/MinGW runner의 C regression을 공식 beta line으로 고정한다. Windows LLVM/backend-compare는 executable LLVM evidence가 있는 runner에서만 추가 gate로 승격한다
  - backend compare는 domain semantics 기준 parity를 계속 확대하고, same-process ABI / launch / runtime environment 차이를 재발하지 않게 잡는다
  - 현재 blocker는 Linux C/LLVM parity와 Windows C regression을 각각 support matrix truth에 맞춰 green으로 유지하는 것이다. Windows LLVM parity는 공식 beta support가 아니라 detected-toolchain evidence track이다
  - 베타 선언 전 acceptance line은 “부분 green”이 아니라 support matrix에 맞는 expected stdout/stderr/result parity까지 포함한 CI green으로 둔다

실행 가능한 연구용 컴파일러 단계는 넘겼지만, 아직 베타라고 부를 수는 없다.

판정 기준:
- 베타 원칙인 `부분 구현 상태를 남기지 않는다`를 아직 충족하지 못함
- 키워드 부족이 아니라 `구현 depth 불균형`이 문제임
- parser가 받는 surface 중 일부가 semantic/C/LLVM/runtime/test/documentation까지 완전히 닫히지 않음

### 이미 닫힌 축과 더 이상 베타 차단이 아닌 것

- `public/private/export` module boundary
  - top-level nominal/domain/callable visibility 정렬 완료
  - private `func/intent/event` cross-module call 차단 완료
  - private `zone/effect` action-contract leakage 차단 완료
- nominal token split
  - `subject/class/struct/object/tobject`는 lexer token 레벨에서 이미 분리됨
- ability field surface
  - legacy `require` alias 제거, `fields` canonical surface 고정
- generic ability baseline
- `ability<T>`, `requires Ability<T>`, `impl ability Ability<T>`, zone authority generic ref, mismatch diagnostics baseline 존재
- cross-module imported generic ability의 multi-bound zone-authority consumer regression 추가
- 양자 surface
  - 베타 대상에서 제외
  - `v2 / experimental`로만 추적

### 현재 베타를 막는 실제 B0 갭

#### 1. Intent / Zone / World closure

현재:
- intent orchestration, inherited/derived contract, rollback/cleanup carrier, zone/world declaration과 기본 lowering은 존재
- zone/world projection/layer/state query도 존재
- intent runtime observability baseline도 존재
  - `IntentLast*`
  - `IntentHistoryStep*`
  - `IntentActive*`
  - `IntentRecent*`
  - active/recent handle + active-step field query builtin의 semantic/transpiler/runtime/LLVM baseline 연결 완료
  - runtime 내부 recent ring + active registry + typed step history storage 연결 완료
  - ABI regression: `IntentRecent*` trace/failure baseline, failed-intent provenance, world zone query, relation/effect zone state parity 고정
  - backend parity: embedded world -> zone projection visibility regression 고정

남은 것:
- embedding ownership / handoff policy를 surface trust 수준까지 명확히 고정
- richer multi-instance timeline query와 failure provenance 정교화
- cross-layer propagation policy의 더 깊은 closure
- C/LLVM parity를 declaration/runtime/diagnostic까지 같은 품질로 정렬

#### 2. relation / effect / projection closure

현재:
- declaration, lifecycle shorthand, `refresh/publish/bind`, layer/state query, overlay sync baseline 존재
- effect join/meet/conflict API와 basic closure 존재
- projection contract diagnostics는 target/source/mode/fix를 포함하는 structured error 쪽으로 보강됨
- backend parity:
  - embedded world -> zone projection visibility regression 고정
  - relation/effect layer + state propagation parity regression 고정

남은 것:
- authority/resource와 effect partial order의 더 완전한 통합
- projection propagation policy 심화
- runtime contract와 deeper propagation failure provenance를 더 설명 가능하게 정리
- C/LLVM parity에서 helper-heavy edge path 감소

#### 3. generic contract closure

현재:
- generic ability declaration/reference baseline 존재
- action / intent step / zone authority / party role slot generic mismatch diagnostics stable 존재
- hidden/default-export generic ability visibility는 action/role impl뿐 아니라 zone authority/party role slot consumer path까지 회귀로 고정
- `ability<T> where ...` bound는 `requires` / `impl ability` / party role slot ref에서 다시 검증됨
- default type argument는 semantic + transpiler + backend compare까지 baseline closure 완료
  - user-defined `class/ability<T = ...>`가 omitted arg 경로에서도 effective specialization으로 정렬됨
  - non-deduced trailing generic parameter default도 function call `where` validation 경로에서 회귀로 고정
  - cross-module omitted default generic ability consumer(`party role slot` / `zone authority`)도 회귀로 고정
- multi-bound `where T: A + B` baseline은 현재 동작함
- hidden/default-export와 generic ability ref 규칙 정렬 완료

남은 것:
- broader type-family generalization을 beta 범위 밖으로 명시
- richer generic constraint validation의 beta contract 범위를 문서/board에 일치시켜 고정
- import/use surface와 diagnostics/tooling 표현을 module contract 기준으로 더 일관되게 정리

#### 4. own/ref closure

현재:
- anchored subset은 닫혀 있음
  - `ref Slot<subject-host>`
  - `own SecureSlot<subject-host>`
- first movable-value transfer slice도 시작됨
  - explicit `own QubitSlot` parameter는 허용
  - `ref QubitSlot` borrow boundary baseline 허용
  - call-site는 `own/default`면 consume, `ref`면 borrow 유지로 분기
  - borrowed `ref QubitSlot`의 `retun` / `channel send` escape는 semantic에서 명시 차단
- 관련 진단/예제/문서는 현재 구현 기준으로 정렬됨

판정:
- anchored subset baseline은 이미 있지만, beta-quality 기준에서는 own/ref를 다시 활성 blocker로 본다
- 남은 일은 일반 movable type ownership model, copy vs move-only 분류, assignment/call/retun/channel/container/rebind 전경로 analysis, richer provenance diagnostics를 닫는 것이다
- 특히 borrowed movable-resource ownership는 helper-call/retun/channel-send baseline이 닫혔고, 다음은 wider movable type generalization과 container/rebind provenance를 더 닫아야 한다
- anchored subset만 stable이라고 보고 넘어가면 ownership story가 partial acceptance로 남는다

### 레이어별 현재 진실

#### 시맨틱

- 강한 부분:
  - nominal family
  - subject/action
  - async/channel/select
  - generic ability baseline
  - visibility/export boundary
- 아직 얕은 부분:
  - richer generic constraint validation
  - general own/ref
  - event closure의 잔여 negative path
  - collection semantic depth

#### 코드 생성

- C backend:
  - 코어 surface는 가장 성숙
  - method owner metadata가 HIR->MIR로 내려와 declaration-side zone/relation/effect/world context 복원 시 이름 추정보다 MIR metadata를 우선 사용
  - 진행: `transpiler_emit_host_method_body_local`의 manual save/restore 상태를 `TranspilerMirEmitState` snapshot helper로 축소
  - 진행: `emit_func_decl_from_mir_named` / AST fallback `emit_func_decl_named`도 `TranspilerMirEmitState` snapshot helper로 수렴
  - 진행: `emit_intent_decl`의 function-scope out/render/retun/local-count restore도 `TranspilerMirEmitState` snapshot helper로 수렴
  - 진행: generic class specialization method body도 MIR inventory 존재 시 AST fallback 대신 MIR routine gate / explicit backend error로 정렬
  - 진행: LLVM domain/role missing-routine errors도 `PGY_CODE_LLVM_MIR_ROUTINE_MISSING` / cause / fix structured path로 정렬
- LLVM backend:
  - MIR-led / HIR-assisted hybrid
  - ordinary routine은 MIR 중심이지만 domain declaration과 일부 bootstrap/helper path에 HIR/AST 의존 잔존
  - pure MIR-only라고 부르기에는 아직 이름이 과함

#### 런타임

- 강한 부분:
  - slot / secure baseline
  - async/channel basic runtime
  - basic intent execution/rollback
  - intent observability baseline (`last` / `history` / `active` / `recent`)
- 아직 얕은 부분:
  - richer multi-instance timeline / failure provenance
  - channel backpressure protocol
  - party edge-path completeness
  - richer zone/world runtime policy

### 컬렉션 / 표면 신뢰

- `Map<K, V>`는 현재 `String | Int | Long | Bool` key stable subset까지 올린다
- 이것은 버그가 아니라 현재 contract
- arbitrary key-universal map contract는 아직 generic closure debt로 남는다

### 툴링

- LSP / formatter는 베타 차단 핵심이 아님
- debugger / package manager / WASM도 베타 차단 핵심이 아님
- 이들은 B0 closure 이후에 다루는 것이 맞음

### 베타 직전 정리 원칙

1. 새 키워드/새 축을 더 추가하지 않는다
2. 남은 미완성 surface를 `완성`하거나 `experimental`로 내린다
3. `양자`, `WASM`, `패키지 매니저`, `고급 디버거`는 베타 대상에서 제외한다
4. B0 4개를 닫기 전에는 베타라고 부르지 않는다

---

## 완료 (P0 — Pain Point 수정, 2026-04-12)

- [x] **P0-1: Array for-in `.count` → `.length`** — `transpiler.c`에서 Array는 `.length`, List는 `.count` 사용
- [x] **P0-2: `StringSplit`/`StringJoin` 런타임 구현** — `pgy_runtime.h`에 실제 구현 추가, 시맨틱/C 백엔드 일치
- [x] **P0-3: `None` 심볼 정의** — `type_checker.c`에서 AST_IDENTIFIER 처리, `type_system.c`에서 `Option<unknown>` → `Option<T>` 할당 허용, 코드젠에서 `expected_type` 기반 타입 해결
- [x] **P0-6: defer 변수 스코프 버그 수정** — `type_checker_flow.c`에서 defer body 처리 전/후 resource-state snapshot/restore. cleanup body의 `retun`/`break`/`continue`와 QubitSlot release/move는 검사하지만 주변 CFG path와 outer loop flow를 소비하지 않는다. direct `type_check_statement()` fallback도 같은 helper를 사용한다.
- [x] **P1-7: struct/subject Slot 매크로 waning 억제** — `transpiler.c`에서 `#pragma GCC diagnostic push/pop`으로 `-Wunused-function` 억제
- [x] **P1-emit_call 갭 메우기** — `BUILTIN_BOX_ARRAY`, `BUILTIN_PARALLEL` 케이스 추가
- [x] **P0-4: enum match OR 패턴 수정** — `type_checker_flow.c`에서 named variant OR 패턴 허용 + coverage 체크 수정
- [x] **P2-13: match 기반 함수 default retun 자동 생성** — `transpiler_emitters_base_b.inc`에서 non-void 함수 끝 fallback retun 추가
- [x] **Pain Point 보고서** — `docs/68_pain_point_report.md`에 수정 내역 기록

## 완료 (최근)

- [x] **Windows ABI/backend-compare precheck 실행 경로 정규화**
  - `compiler_run_binary()`가 MSYS 스타일 `/tmp/...` 및 `/<drive>/...` 실행 파일 경로를 그대로 `_spawnvp()`에 넘기던 문제를 수정
  - Windows에서 executable launch는 native Win32 경로로 정규화한 뒤 실행하도록 정렬
- [x] **nested vessel-source projection ambiguity closure**
  - zone `refresh/publish/bind` projection contract 경로에서 ambiguous source path가 `missing`으로 오진되던 분기 순서를 수정
  - builtin `ToObject` / `ToTObject`도 동일한 structured `Reason/Fix` ambiguity diagnostic으로 정렬
  - nested vessel ambiguity semantic regressions 추가
- [x] **generic consumer provenance diagnostics 보강**
  - `action requires` / `zone authority` / `party role slot` / `intent step requires`에서 generic ability mismatch가 `actual type argument` / `actual implementation` provenance를 함께 보고하도록 정렬
  - 관련 semantic 회귀 추가
- [x] **anchored own/ref provenance diagnostics 보강**
  - closed-subset / local-only / missing `own/ref` / `ref` escape 진단에 `Reason/Fix`와 borrowed-here provenance를 추가
  - 관련 semantic 회귀 추가
- [x] **world embedding structured diagnostics 회귀 고정**
  - embedded zone old-binding mutation이 assignment / hosted func-action call 모두에서 `Reason/Fix`와 world-owned-copy provenance를 남기도록 semantic 회귀 강화
- [x] **Windows shell smoke portability 보강**
  - `abi_pipeline_smoke.sh`, `compare_backends.sh`가 `cmp`/`diff` 부재 환경에서도 `git` 또는 Python fallback으로 비교/차이 출력을 수행하도록 정리
- [x] **surface trust docs 정렬 — collection/result/struct baseline**
  - `Array<T>`는 `[]`, `List<T>`는 `ListNew()`, `HashMap<K,V>`는 `MapNew()`를 canonical 생성 surface로 고정
  - `Result<T>` 추출 API는 `Unwrap` / `UnwrapOr` / postfix `?`로 고정, `UnwrapResult()` 표면은 비채택
  - `struct` field의 legacy `let`은 불변 표식이 아니라 declaration introducer임을 문서화하고, 읽기 전용 계약은 `object/tobject`에만 둔다
- [x] **generic default-arg closure 1차 복구** — declaration acceptance만이 아니라 user-defined generic class omission, generic ability impl-reference omission, arity diagnostics range화, semantic/backend parity까지 다시 녹색으로 정렬
- [x] **ABI Unification Infrastructure** — `pgy_abi_spec.h`, `test_abi_spec.c` (28 PASS), `MIRTypeLayout`, `mir_abi_lookup()`, `rir_dump_json()`, dumb emitter Visitor
- [x] **Windows CI Fix** — `TOKEN_TYPE` → `PGY_TOKEN_TYPE`, `TokenType` → `PgyTokenType` (~20개 파일)
- [x] **v2 Quantum Planning** — 양자 연산 미지원 명시, v2 계획 문서화
- [x] **Documentation Index** — `docs/INDEX.md` 생성, 전체 문서 체계화
- [x] **`HashMap<K, V>` stable key subset surface trust 정렬** — semantic annotation/builtins/runtime comment/test를 `String | Int | Long | Bool` key 지원으로 일치시킴
- [x] **mixed `ability + zone` module export 충돌 수정** — default-export `ability`가 sibling zone visibility를 깨뜨리던 정규화 버그 제거, module smoke 회귀 추가
- [x] **nominal host receiver type 오염 수정** — C backend member-call emit 중 static type-name overwrite를 제거해 `Int_Advance`류 오발행 복구
- [x] **MIR cleanup exceptional topology 회귀 복구** — cleanup/rollback/invalidation block edge materialization과 test expectation 정렬
- [x] **`order_analytics` example 실전화** — sketch 수준 surface를 정리하고 compile-smoke covered example로 승격
- [x] **declaration name surface tightening** — declaration name을 일반 식별자로만 제한하고 reserved keyword 재사용 surface 제거
- [x] **anchored-handle diagnostics/test 정렬** — `own/ref` closed-subset 진단 문구와 `DeviceSlot`/anchored-handle semantic test expectation을 현재 구현 기준으로 일치시킴
- [x] **계층형 stdlib/domain kit v0 고정** — `money`, `datetime(Duration/Instant)`, `timer`, `versioning`, `ledger`, `obligation`, `device_adapter` 모듈과 probe 예제 추가, 코어 추가 금지 원칙 문서화

## 베타 클로저 보드

베타 전 원칙:
- `부분 구현` 상태를 남기지 않는다
- 완료시키지 못하는 surface는 내리거나 experimental로 격리한다
- parser가 받는 표면은 semantic/C/LLVM/runtime/test/documentation까지 닫는다

### B0 — 의미론 클로저 필수

- [ ] **Intent/Zone/World semantics 완전 closure**
  - contract reuse/derivation / authority / lifecycle / embedding ownership / runtime observability / C/LLVM parity / regression
  - 이미 존재: intent orchestration, inherited/derived contract, zone/world query, observability baseline
  - 진행: runtime zone/world propagation cell에 `epoch/cause` provenance baseline이 들어갔고, LLVM intent rebound-zone sync도 같은 truth로 정렬됨
  - 진행: world derived-state chain은 이제 bounded recompute loop를 통해 C/LLVM 양쪽에서 같은 규칙으로 계산됨
  - 강한 기준: 이 축은 이제 "얕은 single-pass sync로도 beta 가능" 같은 해석을 허용하지 않음
- 남음: embedding ownership/handoff policy, **handoff와 더 넓은 world-zone propagation family까지 일반화된 bounded fixpoint 기반 cross-layer propagation policy**, richer provenance query surface, declaration/runtime/diagnostic parity
  - 이 축은 언어 정체성 자체이므로 beta 직전까지 열어두지 않는다
- [ ] **relation/effect/projection semantics 완전 closure**
  - effect lattice, authority-resource partial order 통합, refresh/publish/bind/causes 일관화, diagnostics, C/LLVM parity
  - 이미 존재: declaration, lifecycle shorthand, `refresh/publish/bind`, layer/state query, overlay sync, effect join/meet/conflict, projection contract diagnostics baseline
- 진행: relation/effect/zone projection hidden cell도 C/LLVM 모두 `dirty/ready + epoch/cause` schema로 정렬됐고 runtime contract provenance baseline이 생김
- 진행: world-derived recompute는 bounded pass loop로 올라왔고, relation/effect/zone projection chain도 bounded transitive recompute loop로 올라왔다
- 강한 기준: projection propagation은 더 이상 "helper replay가 대체로 맞음" 수준으로 두지 않고, transitive semantics가 닫히기 전까지 beta blocker로 유지
- 남음: authority-resource partial order 통합, projection/layer/state를 넘어선 **authority/failure handoff와 더 넓은 world-zone propagation family까지의 full transitive frontier propagation policy**, helper-heavy edge path 감소, declaration/runtime/diagnostic/backend parity의 마지막 shrink
  - 이 축은 domain semantics 핵심이므로 partial 상태로 beta에 올리지 않는다
  - projection diagnostics는 `target/source/projection kind/field path/fix`를 포함하고 `Reason:` / `Fix:` 포맷으로 고정한다
- [x] **generic contract 완전 closure**
  - strict beta-quality 기준으로 stable subset closure에서 재개방
  - `default type arg` actual resolution, `where T: A + B` 전경로 enforcement, `ability<T>` mismatch provenance, instantiation-path parity까지 닫는다
  - 완료: default type arg declaration acceptance / omitted trailing default resolution / generic ability impl-reference omission / arity diagnostics provenance
  - 이미 존재: `ability<T>` baseline, default type arg baseline, omitted trailing default resolution, generic mismatch provenance baseline
  - 진행: `party role slot` generic mismatch도 `consumer path / expected type args / actual type args` vocabulary 회귀로 고정
  - 남음: multi-bound 전경로 enforcement, module-contract propagation, instantiation-path parity, richer mismatch diagnostics, wider C/LLVM regression 확대
  - generic mismatch는 `generic subject / expected type args / actual type args / broken bound / consumer path / fix`를 포함하고 `Reason:` / `Fix:` 포맷으로 고정한다
  - generic은 partial acceptance를 beta에 올리지 않는다
- [x] **own/ref 완전 closure**
  - strict beta-quality 기준으로 anchored subset closure에서 재개방했고, classifier-backed stable subset으로 마감
  - 일반 movable type ownership, move/borrow/escape/rebind/channel/retun provenance, diagnostics/test parity까지 닫음
  - 이미 존재: anchored slot subset, anchored diagnostics baseline, anchored regression/docs alignment
  - 완료: summary/direct path family audit와 classifier/docs 최종 정렬
  - 진행: constructor field store escape 경로를 boundary-visible store로 고정하고 회귀 추가
  - 진행: array literal store escape 경로를 boundary-visible store로 고정하고 회귀 추가
  - 진행: assignment rebind escape diagnostic이 member/aggregate target path(`holder.packet`, `items[0]`) provenance를 직접 보고하도록 정렬
  - 진행: nested projection provenance가 constructor field store / member rebind / list/set/queue/map store / array overwrite / helper retun summary / channel send / direct retun까지 회귀로 고정됨
  - 진행: class/subject consumer matrix는 retun / channel / helper / list / set / queue / map / array push / array overwrite / member rebind / constructor field store까지 거의 동형으로 정렬
  - 진행: tuple/object 경로는 기존 `test_semantic.c` 회귀 축에서 channel/new-binding/rebind/retun/helper forwarding/queue-map-array overwrite/projection provenance coverage 유지
  - 진행: slot-handle/class helper-chain 회귀도 ownership-boundaries 계열에 추가돼 direct helper/function call family가 transitive chain까지 고정됨
  - 진행: helper/retun/channel wording family를 `through ...` 기준으로 정렬
  - ownership diagnostics는 `value / ownership mode / moved|borrowed here / escaped|rebound here / consumer path / fix`를 포함하고 `Reason:` / `Fix:` 포맷으로 고정한다
  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: region/lifetime solver와 universal ownership lattice

### B1 — 베타 신뢰도 필수

- [x] **surface trust 문서 재분류**
  - 완료: `docs/18_language_status.md`, `docs/63_feature_depth_matrix.md`, `README.md`에서 `stable subset / explicit reject / beta-out-of-scope` 기준으로 정렬
  - 규칙: "컴파일은 되지만 partial"인 표면을 stable처럼 쓰지 않고, 어디까지를 닫힌 계약으로 약속하는지 먼저 명시
  - 규칙: broader generalization, arbitrary key support, general ownership, richer observability query 같은 항목은 `beta-out-of-scope`로 분리
- [ ] **stable example / smoke source of truth 확대**
  - canonical examples와 closure examples를 smoke에 직접 연결
  - explicit surface vs compressed surface를 같은 의미로 보여주는 pair example 최소 4쌍 고정
  - 대상: app/web orchestration, game/simulation, async/worker/device, world-handoff/domain propagation
- [ ] **Backend parity final closure**
  - C/LLVM이 domain semantics 기준으로 같은 결과를 내는지 고정
  - 대상: intent/zone/world, relation/effect/projection, ownership boundary, refresh/publish/bind, world embedding/handoff
  - 기준: Linux에서 backend compare / llvm smoke / example smoke / ABI-runtime probe 녹색. Windows는 C regression line을 공식 beta gate로 유지하고, LLVM smoke/backend compare는 executable toolchain evidence가 있을 때만 추가 실행
- [ ] **experimental surface 제거 또는 격리**
  - 닫지 못한 parser surface는 명시 거부 또는 문법 제거

## Pain point freeze board

원칙:
- 기능을 더 넓히기 전에 반복해서 다시 깨지는 작성/진단 pain point를 먼저 고정한다
- 각 pain point는 `stable contract + regression + docs wording`까지 같이 잠근다
- recoverable failure와 invariant break를 같은 방식으로 처리하지 않는다

### Failure handling policy freeze

분류:
- `recoverable failure`
  - 사용자 코드가 예상 가능한 실패
  - 예: intent failure, authority/boundary rejection, timeout, remote failure, empty/closed operational state
  - 원칙:
    - 프로세스를 죽이지 않는다
    - `Bool` / `Result<T>` / queryable runtime state로 드러낸다
    - reason / boundary / authority / step provenance를 조회 가능하게 남긴다
- `contract violation`
  - 원칙적으로 semantic 단계에서 차단
  - 런타임까지 오면 structured panic
  - 예: released slot access, invalid secure token, ownership boundary 위반
- `intenal compiler/runtime bug`
  - 즉시 중단
  - intenal error / panic로 명확히 분리
  - 사용자 코드 실패처럼 위장하지 않는다

현재 고정:
- intent/zone/world 쪽 실패는 장기적으로 `recoverable failure`로 수렴시킨다
- slot/token/invariant 계열은 계속 hard fail로 둔다
- `Unwrap(...)`는 panic 성격의 sharp tool로 유지하고, recoverable path의 기본 계약으로 쓰지 않는다

- [ ] **large canonical pair 예제 추가**
  - 큰 예제에서 `explicit`와 `compressed`를 둘 다 stable source of truth로 유지한다
  - 최소 4개 파일 기준으로 관리한다
    - `calendar manage-event`: explicit/compressed
    - `composite intent orchestration`: explicit/compressed
  - 목적:
    - 큰 예제의 전체 계약을 명시형으로 읽을 수 있게 유지
    - 같은 의미를 축약형으로도 바로 복사해 시작할 수 있게 유지
    - smoke에서 두 예제가 모두 실행 가능하도록 고정
- 이 보드는 sugar backlog가 아니라 beta surface trust를 지키기 위한 고정판이다
- P0 pain point가 잠기기 전에는 declaration-side MIR-only debt를 국소 복구 외에는 넓게 건드리지 않는다
- backend 내부 정리는 pain point 기준선과 회귀가 먼저 고정된 뒤에만 다시 확장한다

### P0 — 작성/계약 pain point

- [ ] **contract clause density 고정**
  - 대상: `requires / within / authorized by / causes / refresh / publish / bind`
  - 문제: 같은 의미를 action / intent step / zone에서 중복 기술하게 되어 작성 피로가 커짐
  - 고정 기준:
    - 어디까지 inherited/derived 되는지 vocabulary를 고정
    - 길게 쓰는 버전과 압축 버전의 의미 차이가 문서/진단/예제에서 같아야 함
    - canonical pair와 minimal subset example의 역할을 분리해 source-of-truth를 고정
  - 회귀 기준:
    - semantic regression: inherited/derived contract source가 진단에 노출
    - example smoke: long-form vs compressed-form 예제 둘 다 유지

현재 source-of-truth:
- canonical pair
  - `examples/intent_contract_pair_minimal.pgy`
  - `examples/authority_contract_pair_minimal.pgy`
  - `examples/transfer_contract_pair_minimal.pgy`
- stable minimal subset
  - `examples/action_contract_inheritance_minimal.pgy`
  - `examples/intent_contract_derivation_minimal.pgy`
  - `examples/transfer_move_minimal.pgy`
  - `examples/transfer_move_typed_minimal.pgy`
  - `examples/zone_context_minimal.pgy`

- [x] **contract provenance vocabulary 고정**
  - 완료: beta closure 문서에 contract provenance 표준어를 `derived / inherited`로 고정
  - 규칙: contract source 설명에서는 `inferred`를 쓰지 않고, action에서 재사용된 step clause는 `inherited`, `using/transfer` 등 현재 step에서 계산된 clause는 `derived`로 부른다
  - 규칙: diagnostics / AST print / docs가 같은 용어를 쓰도록 맞추고, `inferred`는 일반 타입 계산이나 non-contract intenal analysis 문맥에만 남긴다
  - 대상: contract provenance 잔여 표현, contract source wording, docs/example terminology
  - 문제: compiler type/effect inference와 domain contract 상속/파생이 같은 단어로 섞이면 설명력이 무너짐
  - 고정 기준:
    - domain contract는 `상속 / 파생`과 `inherited / derived`로만 부른다
    - 일반 compiler 의미는 type/effect `inference`에만 남긴다
  - 회귀 기준:
    - parser/semantic diagnostics 기대 문자열 고정

### P0.5 — recoverable failure 분류/고정

- [x] **failure class inventory 정리**
  - 완료: `docs/07_error_handling.md`, `docs/18_language_status.md`, `README.md` 기준으로 `recoverable failure / contract violation / intenal bug` inventory를 정리
  - 완료: 현재 recoverable 유지 항목, hard-fail 유지 항목, 후속 downshift 대상(authority rejection 등)을 구분
  - 규칙: runtime invariant guard와 real domain rejection을 같은 실패 층으로 섞지 않음
- 현재 inventory baseline:
  - recoverable 유지:
    - `Result<T>` / `?`
    - `RemoteFuture<T> -> Result<T>`
    - channel timeout / non-blocking / closed state
    - world roster timeout
    - `IntentLast* / History* / Active* / Recent*`
  - hard-fail 유지:
    - released slot / invalid token / token permission mismatch
    - `Unwrap(...)` on `Err`, option unwrap on `None`
    - allocator / box / rc / weak invariant break
    - array / slice bounds violation
    - current runtime zone authority null-guard
      - 참고: 이건 아직 real authority rejection이 아니라 invariant check라서 hard-fail 유지 쪽이 맞다
  - first-wave conversion targets:
    - future real runtime authority rejection
    - intent boundary/authority mismatch provenance at runtime
- [ ] **intent/zone/world recoverable failure baseline**
  - intent failure, authority rejection, boundary mismatch는 process abort 대신 queryable reason/state로 노출
  - runtime observability와 diagnostics wording을 같은 provenance vocabulary로 정렬
  - 참고: runtime propagation provenance(`epoch/cause`) baseline은 완료로 본다
  - 진행: runtime zone authority invariant guard는 `last_ok / zone / participant / code / reason` thread-local snapshot을 남기도록 정렬되어, hard-fail guard와 별개로 최소 queryable failure snapshot baseline은 생겼다
  - 진행: authority failure code/reason/stderr format은 `src/runtime/pgy_runtime_authority_contract.h`로 승격했다. inline C runtime과 LLVM runtime library export가 같은 contract macro를 사용하고 `runtime-authority-contract-test-smoke`가 raw literal drift를 차단한다
  - 진행: intent emitter는 MIR `IntentAuthorizedBy` metadata를 C/LLVM 양쪽에서 수집하고, step-local approval을 `pgy_zone_authority_validate_flags_export(...)`로 검증해 `authority:<step>` recoverable intent failure와 runtime authority snapshot을 같은 경로로 남긴다
  - 진행: intent `authorized by`는 concrete zone subject slot으로 해석되며, 같은 타입의 non-authority slot 또는 ambiguous same-type slot mapping은 semantic hard error로 닫혔다
  - 진행: concrete direct-slot participant alias는 ambiguous same-type 후보보다 우선한다. `subject slot rogue: Adventurer`가 존재하면 `authorized by rogue`는 concrete authority slot으로 닫히며, 이전 후보가 세운 stale ambiguity flag는 무시된다
  - 회귀: `intent authorized participant must resolve to authority slot`, `intent authorized participant reports ambiguous authority slot`
  - 회귀: `dnd_taven_campaign` example smoke가 multi-subject same-type zone에서 direct authority aliases를 end-to-end로 고정한다
  - 회귀: `intent_authority_snapshot_abi`, `intent_authority_snapshot`
  - 회귀: `authority_failure_abi`, `authority_failure_surface`, `runtime-authority-contract-test-smoke`
  - 남음: missing-zone/missing-participant 이후의 richer authority mismatch/domain-boundary denial reason도 같은 queryable contract로 확장해야 한다
- [ ] **runtime authority guard downshift**
  - 현재 `pgy_zone_authority_check_export(...)`는 null self/null participant invariant guard다
  - 이 guard 자체는 hard-fail 유지
  - 진행: C inline validator, LLVM runtime export, intent step-local `authorized by` validation 모두 마지막 authority validation 결과를 같은 vocabulary(`last_ok`, `zone`, `participant`, `code`, `reason`)로 남긴다
  - 별도 real authority rejection runtime path가 생기면 그쪽을 `recoverable authority failure` 경로로 설계
- [x] **hard-fail boundary 명시**
  - 완료: `README.md`와 `docs/07_error_handling.md`에 hard-fail boundary를 명시
  - 고정 내용: released slot, invalid token, ownership invariant break, unwrap misuse, bounds violation, runtime invariant guard는 계속 panic / hard-fail territory로 둔다
  - 고정 내용: recoverable authority rejection과 invariant guard를 같은 층으로 섞지 않는다는 점을 문서 wording으로 못박음

- [ ] **projection contract diagnostics 고정**
  - 대상: `refresh/publish/bind` source/target/path/field-map 실패
  - 문제: projection은 언어 강점인데 실패 이유가 약하면 가장 먼저 피로를 줌
  - 고정 기준:
    - target slot / source slot / projection kind / field path / fix가 모두 진단에 들어감
    - structured `Reason:` / `Fix:` formatting을 source-of-truth로 고정
  - 회귀 기준:
    - semantic regression: missing source field / ambiguous path / wrong projection kind / duplicate field map
  - 진행: `projection-diagnostic-contract-test-smoke`가 위 4개 베타 필수 진단 케이스와 `Reason:` / `Fix:` / projection consumer path vocabulary를 semantic regression, implementation, proof doc 기준으로 함께 검사한다

현재 source-of-truth:
- stable example
  - `examples/projection_bind_group_minimal.pgy`
  - `examples/projection_refresh_publish_group_minimal.pgy`
- semantic regression
  - `src/test_semantic.c:test_projection_contract_diagnostics`
  - `make projection-diagnostic-contract-test-smoke`

- [x] **surface trust subset 분류 고정**
  - 대상: generics, own/ref, collections, runtime observability
  - 문제: 되는 것처럼 보이는데 실제로는 subset만 되는 surface가 가장 큰 신뢰 손상 지점
  - 고정 기준:
    - `stable subset / explicit reject / beta-out-of-scope`를 TODO/docs/diagnostic에서 같은 말로 쓴다
  - 회귀 기준:
    - semantic tests와 depth docs가 같은 subset을 가리킴
  - 현재 기준 문서:
    - `README.md`의 `Surface trust policy`
    - `docs/18_language_status.md`
    - `docs/63_feature_depth_matrix.md`
    - `docs/64_depth_filling_roadmap.md`

현재 고정하려는 baseline:
- generics
  - stable subset: exact/ability/multi-bound baseline
  - stable subset extension: default type argument actual resolution on implemented declaration/call/module-consumer paths
  - beta-out-of-scope: broader generic generalization
- own/ref
  - stable subset: classifier-backed own/ref surface on copy values + boundary-visible aggregates + movable values + slot handles
  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: arbitrary universal ownership lattice beyond current classifier/summary model
  - beta blocker: 없음
- collections
  - stable subset: `List<T>`, `Set<T>`, `HashMap<String, T>`, `HashMap<Int, T>`, `HashMap<Long, T>`, `HashMap<Bool, T>`
  - explicit reject: unsupported map key kinds
  - beta-out-of-scope: arbitrary key-universal collection contracts
- runtime observability
  - stable subset: `last / history / active / recent`
  - explicit reject: 없음
  - beta-out-of-scope: richer multi-instance timeline query와 deeper failure provenance query

### P1 — 내부 구조 pain point

- [ ] **declaration-side MIR-only debt 고정**
  - 대상: declaration inventory / metadata helper / duplicated named-decl lookup
  - 문제: routine body는 MIR로 정리돼도 decl-side helper debt가 남으면 parity bug가 반복됨
  - 고정 기준:
    - backend lookup은 공통 inventory helper를 사용
    - 남은 debt는 “기능 미구현”이 아니라 “AST-carried decl metadata 구조 debt”로 분리해서 기록
  - 회귀 기준:
    - LLVM/C backend helper duplication 감소
    - debt ledger와 TODO 표현 정렬
  - 현황:
    - 진행: MIR declaration emit state restore는 helper 하나로 묶였고, role host lookup은 active inventory-only 쪽으로 더 좁아졌다
    - 진행: 조기 retun 경로의 `current_host_decl` / `current_func_decl` 복구가 emitter 본문 중복 대신 공용 restore helper를 타게 됐다
    - role / party / roster / relation / effect / zone / world declaration method body의 AST fallback는 제거됨
    - 남은 debt는 declaration inventory / naming helper / named-decl lookup의 구조 정리 쪽으로 축소됨
    - 진행: `emit_func_decl_from_mir_named(...)`가 outer host restore에서 raw saved host-name fallback보다 `saved_host_decl + current_func_decl`를 우선 쓰도록 정렬
    - 진행: host restore/current-host lookup이 inventory에서 host decl을 못 찾으면 raw `current_*_name` 상태를 유지하지 않고 host handle을 비우도록 정렬
    - 진행: `transpiler_restore_host_context_local(...)` 시그니처도 `saved_host_decl` 중심으로 축소해 decl-side restore에서 raw name 인자를 제거
    - 현재 inventory:
      - `src/codegen/transpiler_helpers_core_b.inc`: `current_host_decl_name` 상태 자체와 일부 host naming helper 정리
      - `src/codegen/llvm_pipeline.c`: AST-carried declaration inventory를 담는 `MIRProgram` bootstrap 경로
      - 공통 과제: current_* name 상태와 ad-hoc named lookup를 MIR declaration metadata query로 치환
    - 최근 정리:
      - `current_field_type_name`, `current_host_method_decl`, `find_nominal_host_method_decl`는 active inventory 경유 lookup로 정렬됨
      - transpiler host context 복구는 `current_host_decl -> within_zone -> saved host-name inventory` 순으로 정렬됨
      - transpiler emitter hot path의 direct `current_*_name` 참조는 helper/restore layer 위주로 축소됨
      - LLVM declaration helper / MIR-domain emission / expr-call builtin path도 `llvm_current_host_decl_name(...)`와 bind/restore helper 쪽으로 이동함
      - LLVM `llvm_current_host_decl(...)`는 더 이상 `current_class_name` 재조회 fallback에 의존하지 않고 bound host handle / `within_zone`만을 truth로 사용함
      - `llvm_pipeline.c`의 nominal declaration registration과 class-method enumeration도 raw `decl_header->methods` 직접 접근보다 active nominal inventory / `llvm_find_host_decl_methods_in_context(...)` 경유로 이동함
      - `llvm_register.c`의 active nominal registration도 `mir->decl_headers` 직접 순회 대신 active nominal inventory 기준으로 정렬됨
      - `make mir-declaration-inventory-test-smoke`를 추가해 C/LLVM declaration/domain/nominal active inventory helper seam과 pipeline/domain 소비 경로를 static gate로 고정했다. 새 raw MIR declaration array access는 owner 파일 밖에서 조용히 늘어날 수 없다
      - C backend `emit_program(...)`의 executable metadata도 `mir->has_*` / `mir_find_function_decl(...)` 직접 접근 대신 `transpiler_active_*` helper를 통과하도록 정렬했다
      - C backend `emit_program(...)`의 ability/type/exten/function/intent/domain/event declaration bootstrap 순회도 direct `mir->...` array/count 접근 대신 `transpiler_active_inventory(...)` / `transpiler_active_extens(...)` view를 사용하도록 정렬했다
      - `MIRDeclMethod`는 hosted method identity, routine link, signature metadata까지 담고 LLVM nominal/enum prototype registration은 `llvm_mir_decl_method_*` helper를 통해 이 row를 먼저 소비한다
      - C backend hosted-method forward declarations now consume `MIRDeclMethod`
        signature metadata through `transpiler_mir_decl_method_param_count(...)`,
        `transpiler_mir_decl_method_param(...)`, and
        `transpiler_mir_decl_method_retun_type(...)`. The old AST-only forward
        declaration helper is removed; AST method payload remains only as
        compatibility input for body emission and legacy non-MIR rows.
      - `mir_validate(...)` now rejects declaration-header method metadata drift:
        declaration header name/type/method-list compatibility must match the
        temporary AST payload while that payload remains, hosted method metadata
        count must match the AST compatibility count, row AST payload and
        owner/name/signature fields must match the compatibility method payload,
        and linked routine indexes must stay within the active MIR routine
        inventory.
      - MIR declaration headers now record subject nominal declarations as
        pointer-self ABI hosts, matching C/LLVM hosted self semantics. Roster
        hosted methods are also recorded in `MIRDeclHeader` instead of being
        omitted from the declaration metadata rows.
      - `mir_validate(...)` now rejects pointer-self ABI shape drift against
        the temporary AST compatibility payload while that payload remains.
      - `mir_validate(...)` also rejects duplicate declaration header names so
        `mir_find_decl_header(...)` cannot silently resolve an ambiguous
        declaration inventory row.
      - 남은 핵심 debt는 LLVM pipeline의 AST-carried declaration inventory bootstrap와 helper/restore layer 바깥의 raw host-name state 제거

- [x] **ownership vocabulary / payload cleanup 1차 고정**
  - 대상: semantic ownership diagnostics / payload helper family / wording drift
  - 완료:
    - `src/semantic/type_checker_ownership_boundaries.inc`의 ownership helper 9종이 `DiagPayload`/`semantic_emit_payload(...)` 패턴으로 정렬됨
    - semantic direct `semantic_error_with_hints(...)` 호출은 ownership-boundary helper 내부에서 제거됨
    - vocabulary 1차 정리:
      - `anchored handle` → `slot handle (anchored)`
      - `movable resource handle` / `movable resource` → `slot handle (movable)`
      - `capability-bearing` → `authority-bearing` (ownership/domain wording 기준)
    - semantic 회귀는 현재 wording 기준으로 다시 고정됨
  - 검증:
    - `make test-semantic` → `1872 passed, 0 failed`
    - `make test-transpile` → `601 passed, 0 failed`
  - 남은 것:
    - P3 잔여 세분류(`boundary value (subject)` 등) 추가 압축
    - payload/helper family를 ownership 바깥 semantic diagnostics로 더 확장
    - own/ref call/consumer path에서 classifier 기반 trivial copy-only semantics를 더 넓게 적용
    - destructure target binding / nested projection / helper-chain wording을 consumer kind 기준으로 더 세분화

- [ ] **type-resolution DAG 엔진 도입**
  - 대상: semantic type resolution / generic consumer resolution / declaration dependency scheduling
  - 문제: 현재는 `resolve_type_node(...)` 중심의 재귀 해석 + scope lookup + ad-hoc validation이 주축이라, module import graph는 분명하지만 type dependency 자체는 compiler-wide DAG로 관리되지 않는다
  - 최근 진행:
    - `TypeResolutionGraph` inventory + cycle diagnostic + topo derivation은 실제 활성 상태
    - staged worklist는 provider-first 역순 topo 순회로 고정됨
    - local contract / projection synthetic node는 label별 narrow handler로 소비됨
    - generic `default_type` / generic constraint / `where` bound는 staged DAG resolver 경로에 편입됨
    - graph regression은 world lifecycle / relation-effect propagation / generic consumer schedule / alias cycle provenance / generic default-bound cycle provenance / action-intent-zone-party ability consumer provenance까지 포함
    - graph validator cycle과 compatibility alias-resolution cycle이 모두 `Contract source:` / `Reason:` / `Fix:` 구조로 정렬됨
    - 진행: type constraint bound formatter는 `type_checker_type_constraint.c`로 실제 TU 분리 완료
    - 진행: graph node/edge/path/cycle-format primitive는 `type_checker_resolution_graph_core.c`로 실제 TU 분리 완료
    - 진행: named dependency edge recorder와 즉시 cycle diagnostic 발행 경로는 `type_checker_resolution_graph_core.c`로 실제 TU 분리 완료
    - 진행: type-ref dependency recorder도 `type_checker_resolution_graph_core.c`로 이동했고, `find_type_alias_decl`의 cross-include dangling retun-type seam을 명시 선언으로 정리
    - 진행: type-ref collector는 `type_checker_resolution_graph_collect.c`로 이동했고, graph core/include 경계의 dangling `static void` seam을 제거
    - 진행: generic contract inventory / string dependency / required ability collector helpers는 `type_checker_resolution_graph_collect.c`로 이동해 declaration collector들의 공통 의존을 TU 경계로 승격
    - 진행: top-level declaration graph registration은 `type_checker_resolution_graph_collect.c`로 이동해 inventory `.inc`를 1,962 LOC까지 축소
    - 진행: local-contract graph node/dependency + zone/world/projection label formatters는 `type_checker_resolution_graph_labels.c`로 이동해 inventory `.inc`를 1,835 LOC까지 축소
    - 진행: projection source resolver는 `type_checker_resolution_graph_domain.c`로 이동하고 `find_zone_domain_slot`을 intenal API로 승격해 inventory `.inc`를 1,809 LOC까지 축소
    - 진행: event declaration precollector는 `type_checker_resolution_graph_decl.c`로 이동해 inventory 본체에서 declaration-kind collector를 첫 절단
    - 진행: enum declaration precollector도 `type_checker_resolution_graph_decl.c`로 이동하고 `semantic_stage_method_array`를 intenal API로 승격해 inventory `.inc`를 1,765 LOC까지 축소
    - 진행: ability declaration precollector와 action-contract precollector도 `type_checker_resolution_graph_decl.c`로 이동해 inventory `.inc`를 1,648 LOC까지 축소
    - 진행: role/class/party/roster declaration precollector도 `type_checker_resolution_graph_decl.c`로 이동하고, relation/effect domain inventory precollector는 `type_checker_resolution_graph_domain.c`로 이동해 inventory `.inc`를 1,299 LOC까지 축소
    - 진행: intent declaration precollector와 world inventory precollector를 각각 `type_checker_resolution_graph_decl.c`, `type_checker_resolution_graph_world.c`로 이동해 inventory `.inc`를 870 LOC까지 축소
    - 진행: zone projection field-map collector를 `type_checker_resolution_graph_zone.c`로 분리했고, 남은 inventory body를 `type_checker_resolution_graph_inventory.c`로 승격해 inventory `.inc`를 제거
    - 진행: world/zone local-contract stage replay를 `type_checker_resolution_stage_domain.c`로 분리하고, 남은 stage 본체를 `type_checker_resolution_stage.c`로 승격해 stage `.inc` 제거
    - 진행: class/exten declaration checker를 `type_checker_class_decl.c`로, top-level semantic orchestration을 `type_checker_program.c`로 분리해 program `.inc`를 624 LOC까지 축소
    - 진행: `ToObject` / `ToTObject` projection checker를 `type_checker_builtins_projection.c`로 분리해 builtins nominal `.inc`를 659 LOC까지 축소
    - 진행: domain helper와 intent helper를 각각 `type_checker_decls_domain_helpers.c`, `type_checker_intent_helpers.c`로 승격해 semantic `.inc` 800 LOC stop condition을 달성하고 `make semantic-inc-size-test-smoke`로 회귀 방지
    - 진행: C backend `transpiler_emitters_mir_inventory_ssa.inc`를 3개 하위 slice로 분리하고 `make test-transpile`, `make llvm-test-backend-compare`로 parity 회귀 통과
    - 진행: standalone TU 승격 중 드러난 dangling retun-type seams와 implicit helper dependency를 제거해 `make test-all`, `make llvm-test-backend-compare` 회귀 통과
    - 진행: implicit declaration / implicit int는 기본 CFLAGS에서 에러로 고정되어 이후 DAG/semantic split 중 hidden helper dependency가 즉시 실패하도록 정렬
    - 진행: `type_resolution_inten_node` / `type_resolution_add_edge` / `type_resolution_find_path` / `type_resolution_format_cycle`는 include-order static helper에서 `type_checker_intenal.h` intenal API로 승격
    - 진행: DAG stage 안에서 retired resolver compatibility surface를 `PGY_TYPE_RES_STATS=1` 통계에 노출했다. 현재 beta gate는 `stage-compat-family`의 alias/non-alias fallback을 모두 0으로 고정하고, graph stats와 topo validation을 함께 확인한다. 중앙 metadata materializer의 마지막 recursive escape hatch도 제거되어 unsupported shape는 explicit fallback inventory로만 기록된다
    - 진행: type-alias stage는 metadata-only lookup으로 성공 경로를 materialize하고, 실패 경로는 recursive resolver 없이 diagnostic unresolved inventory로 분리한다. `make type-resolution-dag-test-smoke`는 alias compatibility fallback 0, alias materialization 존재, alias diagnostic unresolved accounting을 함께 gate한다
    - 진행: DAG edge가 이미 존재하는 named type-ref는 generic argument를 포함해 stage에서 `resolve_type_node(...)`를 다시 호출하지 않고 graph-backed skip으로 처리한다. `stage-graph-backed: skips=N` 통계가 추가됐고 `type-resolution-dag-test-smoke`가 skip 합계가 0으로 퇴행하지 않는지 검사한다
    - 진행: graph precollect TU 안에서 enum methods가 `semantic_stage_method_array(...)`를 호출하던 impurity를 제거했다. 이제 enum method signature/contract도 precollect action contract 경로로만 graph edge를 수집한다
    - 진행: DAG stage helper를 `type_checker_resolution_stage_lookup.c` / `type_checker_resolution_stage_stats.c`로 분리했고, 이후 alias/nominal/systemic/domain-decl replay owner를 각각 `type_checker_resolution_stage_alias.c`, `type_checker_resolution_stage_nominal.c`, `type_checker_resolution_stage_systemic.c`, `type_checker_resolution_stage_domain_decl.c`로 분리했다. `type_checker_resolution_stage.c`는 88 LOC top-level dispatch owner가 됐고, graph precollect, stage lookup, stage stats, alias diagnostics, and stage replay families are now separated by file boundary
    - 진행: generic where/default validation은 `type_checker_generic_validation.c`로 이동했다. `type_checker_resolution_graph_*.c`와 `type_checker_resolution_graph_core.inc`는 더 이상 `resolve_type_node(...)`를 직접 호출하지 않으며, `semantic-core-shape-test-smoke`가 이 resolver-free graph-layer 경계를 검사한다
    - 진행: graph precollect가 context-independent builtin type refs(`Int`, `Long`, `Float`, `Double`, `Bool`, `String`, `QubitSlot`, `Void`)를 `SemanticContext.type_resolution_metadata`에 기록한다. owner resolver seams는 이 metadata를 먼저 조회하고, unsupported shape는 explicit fallback inventory로 기록될 뿐 recursive fallback으로 내려가지 않는다
    - 진행: graph metadata가 resolver-stable constructed/anchored-handle shells(`Array<T>`, `Slice<T>`, `List<T>`, `Queue<T>`, `Set<T>`, `Box<T>`, `Rc<T>`, `Weak<T>`, `Channel<T>`, `Future<T>`, `RemoteFuture<T>`, `Token<T>`, `DeviceSlot<T>`, `HashMap<String|Int|Long|Bool, T>`, `Option<T>`, `Result<T,E>`, `Slot<T>`, `SecureSlot<T>`, `ReadView<T>`, `WriteView<T>`, `MoveToken<T>`)를 materialize할 수 있다. graph가 만든 `Type` shell은 metadata owned lane으로 기록하고 semantic context destroy에서 해제한다
    - 진행: graph metadata가 tuple shell과 event-handler/function shell도 materialize한다. channel/future AST node는 inner fact collect 직후 constructed shell을 기록하므로 recursive fallback에 덜 의존한다
    - 진행: `resolve_type_node(...)` wrapper 자체가 metadata-first가 되어, 남은 explicit compatibility allowlist도 recursive materialization 전에 DAG facts를 먼저 소비한다
    - 진행: `resolve_generic_type_arg(...)`도 metadata-first 조회 후 fallback으로 내려간다. constructed builtin/generic consumer path의 recursive resolver 의존 면적을 줄였다
    - 진행: owner-local resolver seams는 `semantic_type_resolution_lookup_or_materialize(...)` 공용 materializer로 수렴했다. resolver 구현체 밖에서 직접 `resolve_type_node(...)`를 호출하면 `type-resolution-resolver-inventory-test-smoke`가 실패한다. Central metadata owner도 `type_checker_resolution_metadata_diagnostics.c`를 분리해 stable-shell arity, invalid constructed HashMap key, unknown bare named diagnostics를 별도 owner가 맡고, alias-chain/cycle materialization은 `type_checker_resolution_metadata_alias.c`가 맡는다. central metadata materializer recursive escape hatch는 제거됐고 central metadata owner는 268 LOC, alias owner는 315 LOC로 분리됐다. 낡은 `resolve_type_alias_decl(...)`와 `SemanticContext.alias_resolution_*` stack도 제거되어 direct named alias resolution은 metadata alias owner만 통과한다. `resolve_named_type(...)` itself is now metadata-first for stable builtin/scope/generic/nominal/alias names, and the resolver-inventory smoke rejects recursive alias resolver debt if it reappears
    - 진행: party/role ability lookup은 `type_checker_domain_role_lookup.c`로 분리했다. 이후 projection contract diagnostics와 overlay scope setup도 각각 `type_checker_domain_projection.c` / `type_checker_overlay_common.c`로 분리되어 `type_checker_decls_domain_helpers.c`는 972 LOC까지 낮아졌다. 남은 helper owner는 zone/effect/relation slot helper 책임에 집중한다
    - 진행: `type-resolution-dag-test-smoke`가 graph-backed skips뿐 아니라 retired compatibility resolver call cap, metadata entries/owned/hits, metadata materializer fallback count, zero stage metadata materialization, alias-stage split accounting을 검사한다. 최신 local stats: `graph-backed skips=2033 retired_resolver_calls=0 retired_resolver_unique_nodes=0 metadata_entries=3498 metadata_owned=258 metadata_hits=8380 materializer_fallbacks=0 stage_materialize_alias=0 stage_materialize_non_alias=0 alias_materialized=6 alias_diagnostic_unresolved=78 alias_diagnostic_resolver_calls=0 alias_diagnostic_resolved=0 alias_diagnostic_cycle_unresolved=78`
    - 진행: DAG smoke는 이제 graph-backed skip/metadata entry/metadata hit/owned metadata가 단순히 0보다 큰지만 보지 않고 beta floor(`skips>=1900`, `entries>=3400`, `hits>=7500`, `owned>=240`)와 retired compatibility resolver cap(`retired_resolver_calls<=0`)를 검사한다. DAG source-of-truth 사용량이 크게 후퇴하면 CI에서 즉시 잡는다
    - 진행: 중앙 metadata materializer의 마지막 recursive fallback은 0으로 닫혔다. `type-resolution-dag-test-smoke`는 `materializer_fallbacks==0`과 모든 metadata unresolved audit family 0을 고정한다
    - 진행: stage metadata materialization surface는 alias/non-alias 모두 0으로 고정됐다. `type_checker_resolution_stage_alias.c`가 unique alias diagnostic unresolved accounting과 optional trace를 소유한다. 성공 alias materialization과 diagnostic unresolved inventory를 별도 계측하고, 남은 78건은 recursive resolver 재진입이 아니라 alias-cycle diagnostic coverage에서 나오는 unresolved inventory다. `alias_diagnostic_resolver_calls==0` gate가 이 경계를 차단한다
    - 진행: program-level symbol inventory가 ability declarations도 predeclare한다. `type_check_ability_decl(...)`은 자기 자신의 predeclare만 재사용하고 같은 이름의 다른 ability는 기존처럼 duplicate diagnostic으로 처리한다. forward source order에서 generic default/where, zone authority, party role-slot ability consumer가 provider 후행이어도 통과하는 regression을 추가했다
    - 진행: `tests/cases/backend_compare/forward_ability_order/main.pgy`를 backend compare suite에 추가했다. provider-after-consumer generic default/alias/zone-authority/party-role-slot ability ordering이 semantic-only가 아니라 C/LLVM 출력 동등성까지 유지되는지 검사한다
    - 진행: `tests/compare_backends.sh` 기본 실행은 `tests/cases/backend_compare/*/main.pgy`가 default case array에 빠져 있으면 실패한다. 명시 인자 기반 targeted run은 유지하되, CI/default path에서 새 parity case가 조용히 누락되는 drift를 차단했다. 이 gate로 기존 passing case 8개(array builtins/inline access, slice inline access, intent observability rollback, list/map/queue get-string, try-operator result)를 default C/LLVM parity suite에 편입했다
    - 진행: `type-resolution-resolver-inventory-test-smoke`가 direct resolver allowlist와 함께 metadata-first wrapper, execution/anchored-handle metadata materializer coverage를 static gate로 고정한다
    - 진행: `type-resolution-resolver-inventory-test-smoke`가 새 `semantic_type_resolution_resolve_or_fallback(...)` 사용자를 금지하고 named fallback seam 총량을 0개로 고정한다. gate 출력은 현재 fallback seam count를 직접 보여주며, `semantic_type_resolution_lookup_or_materialize(...)` 내부의 central recursive escape hatch도 0개로 고정한다
    - 진행: fallback seam gate의 기존 하한선(`30개 미만이면 실패`)을 debt-reduction에 맞지 않는 규칙으로 보고 제거했다. 이제 0개 상한만 growth guard로 유지하며, seam 축소는 CI 성공 경로다
    - 진행: `type_checker_module_contract.c`의 ability contract bookkeeping은 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only seam으로 낮췄다. ability 존재/visibility/generic arity/where provenance는 ability-specific validator가 계속 소유하며, fallback seam inventory는 39에서 38로 감소했다
    - 진행: `type_checker_ability_fields.c`의 ability `fields` requirement validation도 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only로 낮췄다. field contract diagnostics는 ability-specific validator가 계속 소유하며, fallback seam cap은 32에서 31로 감소했다
    - 진행: `type_checker_builtins_projection.c`의 projection target-field resolver도 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only로 낮췄다. projection field diagnostics는 projection validator가 계속 소유하며, fallback seam cap은 31에서 30으로 감소했다
    - 진행: `type_checker_program.c`의 quiet top-level placeholder resolver는 graph precollect 이후 metadata lookup-only로 전환했다. event/function forward placeholders가 recursive fallback 없이 precollected DAG facts를 소비하면서 fallback seam cap은 30에서 29로 감소했다
    - 진행: `type_checker_builtins_query_domain.inc`의 projection source-field resolver도 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only로 낮췄다. HasProjection/HasZoneProjection 계열 field diagnostics는 domain query validator가 계속 소유하며, fallback seam cap은 29에서 28로 감소했다
    - 진행: `type_checker_party_decl.c`와 `type_checker_roster_decl.c`의 shared-field type resolver도 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only로 낮췄다. party/roster shared field diagnostics는 각 declaration validator가 계속 소유하며, fallback seam cap은 28에서 26으로 감소했다
    - 진행: `type_checker_ability_decl.c`의 abstract method signature resolver와 `type_checker_role_decl.c`의 host-type resolver도 recursive fallback helper를 호출하지 않고 DAG metadata lookup-only로 낮췄다. ability/role declaration diagnostics는 각 owner validator가 계속 소유하며, fallback seam cap은 26에서 24로 감소했다
    - 진행: function/action body precollector가 local let / with-slot annotation뿐 아니라 expression subtree, call type args, lambda param/retun/body, event subscription handler, spawn/channel/retun/branch expressions까지 따라간다. 이 기반으로 `type_checker_event.c`의 event/lambda handler type-ref resolver를 DAG metadata lookup-only로 낮췄고 fallback seam cap은 24에서 23으로 감소했다. `type_checker_flow.c`의 flow-local type resolver도 DAG metadata lookup-only로 낮춰 cap은 22로 감소했다. `type_checker.c`의 type-alias statement resolver도 DAG metadata lookup-only로 낮춰 cap은 21로 감소했다
    - 확인된 남은 blocker: `type_checker_program.inc`의 function body param/retun/domain-slot materialization seam은 단순 lookup-only로 낮추면 direct semantic unit path에서 graph metadata bootstrap 없이 segfault가 난다. 이 seam은 direct semantic unit bootstrap 또는 null-safe diagnostic path가 먼저 필요하다
    - 확인된 남은 blocker: `type_checker_intent_decl.c`의 intent participant/value/where resolver seam은 단순 lookup-only로 낮추면 semantic suite 후반 parallel execution path에서 segfault가 난다. intent declaration은 graph precollect가 있지만 direct semantic/bootstrap path와 step/local binding materialization이 아직 lookup-only 계약을 만족하지 않으므로 explicit fallback seam으로 남긴다
    - 확인된 남은 blocker: `type_checker_helpers_host.inc`의 host helper resolver는 단순 lookup-only로 낮추면 intent/zone authority positive path가 subject-slot type metadata 부족으로 무너진다. 이 seam은 zone/world/host subject-slot nominal metadata를 DAG에 보존한 뒤 제거해야 한다
    - 완료: `type_checker_generic_validation.c`의 generic where/default validation resolver는 generic default effective-arg fact와 where-bound provenance가 DAG metadata에 올라온 뒤 metadata-only lookup으로 전환했다. resolver inventory gate가 이 owner의 materializing helper 재도입을 차단한다
    - 확인된 남은 blocker: `type_checker_generic_support.inc`의 boundary type helper seam은 단순 lookup-only로 낮추면 `ref class` / `ref subject` escape diagnostics 150개가 빠진다. 이 seam은 generic/nominal boundary category fact와 ref/own escape classifier가 DAG metadata에서 같은 type category를 볼 수 있을 때 제거해야 한다
    - 확인된 남은 blocker: `type_checker_ability_where.c`의 ability where-bound resolver는 단순 lookup-only로 낮추면 generic ability multi-bound mismatch provenance가 사라져 `Cloneable` bound mismatch 진단 회귀가 난다. 이 seam은 ability where-bound effective-arg / multi-bound provenance fact를 DAG metadata에 올린 뒤 제거해야 한다
    - 확인된 남은 blocker: `type_checker_operator_expr.inc`의 operator overload method signature resolver는 단순 lookup-only로 낮추면 semantic suite가 event/misc path 진입 전후에 segfault할 수 있다. 이 seam은 method param/retun signature metadata와 operator overload candidate summary를 DAG에 올린 뒤 제거해야 한다
    - 확인된 남은 blocker: `type_checker_zone_decl.c`의 zone authority subject-slot type seam은 단순 lookup-only로 낮추면 generic ability mismatch provenance가 사라진다. 이 seam은 zone authority generic ability fact를 DAG metadata에 올린 뒤 제거해야 한다
    - 확인된 남은 blocker: `type_checker_class_decl.c`의 class/vessel field resolver는 단순 lookup-only로 낮추면 vessel/subject-vessel field acceptance가 깨진다. 이 seam은 class/vessel field nominal flavor metadata를 DAG에 보존한 뒤 제거해야 한다
    - 확인된 남은 blocker: `type_checker_world_decl.c`의 shared/domain-slot resolver는 단순 lookup-only로 낮추면 zone/world/intent positive paths가 `subject slot ... requires a subject type`로 무너진다. 이 seam은 world domain-slot subject/zone nominal materialization을 DAG metadata에 올린 뒤 제거해야 한다
    - 확인된 남은 blocker: `type_checker_ownership_let.c`의 let annotation resolver는 단순 lookup-only로 낮추면 direct semantic unit path에서 graph metadata 없이 `ClaimSlot` annotation이 들어와 segfault할 수 있고, broader program path에서는 `Slot`/`ReadView`/`WriteView`/`QubitSlot`/anchored own-ref paths가 `<unknown>`으로 무너질 수 있다. 이 seam은 direct semantic unit bootstrap 또는 null-safe diagnostic path와 anchored-handle constructed-type metadata coverage를 같이 닫은 뒤 제거해야 한다
    - 진행: domain/intent declaration resolver는 owner-local type-ref seam으로 수렴했다. slot/shared/named domain refs와 intent involves/value/where refs가 각각 하나의 owner seam을 공유하면서 fallback seam inventory는 38에서 34로 감소했다
    - 진행: alias/generic-parameter helper와 resolution-stage diagnostic fallback도 owner-local seam으로 수렴했다. fallback seam inventory는 34에서 32로 감소했다
    - 진행: zone authority participant resolver가 exact/qualified-tail direct slot match를 먼저 인정하고, direct match 반환 시 stale ambiguity flag를 지운다. 같은 타입 subject slot이 여럿 있어도 `authorized by rogue`가 실제 `subject slot rogue: Adventurer`로 concrete하게 닫히면 false-positive ambiguous로 떨어지지 않는다
    - 진행: `type_checker_intent_decl.c`의 participant/value/where local seam 3개는 graph metadata-first 조회 후 recursive fallback으로 내려간다
    - 진행: `type_checker_decls_domain_helpers.c`의 slot/shared/named-ref local seam 3개는 graph metadata-first 조회 후 recursive fallback으로 내려간다
    - 진행: `type_checker_intent_helpers.c`의 direct resolver 호출은 `intent_helper_resolve_type_ref(...)` 단일 seam으로 수렴했다. transfer-derived using/where, ability generic arg, role-field checks는 이 seam을 통해 다음 DAG metadata 전환을 탄다
    - 진행: `type_checker_helpers_host.inc`의 direct resolver 호출은 `host_helper_resolve_type_ref(...)` 단일 seam으로 수렴했다. projection source fields, hosted method retun/param, zone authority/domain slot checks는 이 seam을 통해 다음 DAG metadata 전환을 탄다
    - 진행: `type_checker_program.c`의 forward-declaration type materialization은 quiet resolver seam 1개로 수렴했고, `type_checker_program.inc`의 function-body param/retun/domain-slot materialization body resolver seam은 graph metadata-first 조회 후 fallback으로 내려간다
    - 진행: `type_checker_event.c`의 event signature/lambda handler materialization은 graph-backed metadata lookup-only로 전환됐다. 다음 DAG slice는 ownership let / zone authority / world domain-slot / ability where-bound처럼 semantic provenance가 남은 owner seams다
    - 진행: `type_checker_world_decl.c`의 shared field/domain slot materialization은 `world_resolve_type_ref(...)` / `world_resolve_domain_slot_type(...)` seam으로 수렴했다. world shared/slot checks는 이 seam에서 graph-backed metadata로 교체할 수 있다
    - 진행: `type_checker_role_decl.c`, `type_checker_generic_contracts.inc`, `type_checker_helpers_late.c`, `type_checker_expr.inc`의 직접 resolver 호출도 각각 role/generic-contract/late-helper/expr local seam 1개로 수렴했다
    - 진행: `type_checker_generic_validation.c`, `type_checker_ability_where.c`, `type_checker_module_contract.c`, `type_checker_ability_decl.c`, `type_checker_class_decl.c`, `type_checker_operator_expr.inc`, `type_checker_ownership_destructure_stmt.inc`도 local resolver seam으로 수렴했다. 남은 direct count는 대부분 resolver 본체, 주석, 또는 명시 seam이다
    - 진행: `type_checker.c`, `type_checker_ability_fields.c`, `type_checker_builtins_projection.c`, `type_checker_builtins_query_domain.inc`, `type_checker_flow.c`, `type_checker_generic_support.inc`, `type_checker_helpers_effects.inc`, `type_checker_ownership_let*.inc`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`, `type_checker_zone_decl.c`의 단발 direct resolver 호출도 local seam으로 수렴했고, zone domain-slot seam은 graph metadata-first 조회를 사용한다
    - 완료: `make type-resolution-resolver-inventory-test-smoke`를 추가해 새 `resolve_type_node(...)` 직접 호출이 resolver 본체/stage metadata materialization/core fallback/local seam allowlist 밖에 생기면 실패하도록 고정했다. `ci-linux`에도 연결했다
    - 검증: 2026-04-25 local WSL/Linux `make ci-linux` full green. Windows/MSYS2 native runner는 이 머신에 없으므로 별도 CI 환경 acceptance line으로 유지
  - 목표:
    - import graph와 별개로 `type provider -> type consumer` 그래프를 분리 구축한다
    - declaration / alias / generic default / where-bound / ability consumer / zone authority consumer를 DAG node/edge로 승격한다
    - namespace-only reference나 declaration inventory 조회가 불필요한 concrete type materialization을 강제하지 않게 한다
    - cycle는 generic/alias/type consumer path 기준으로 path-aware diagnostic으로 보고한다
    - incremental compile 시 invalidation 범위를 declaration/type dependency 단위로 줄인다
  - 1차 구현 원칙:
    - 기존 `resolve_type_node(...)`를 한 번에 폐기하지 않는다
    - 먼저 graph inventory + topo scheduling + cycle diagnostic을 추가하고, 그 다음 recursive resolver를 graph-backed evaluator로 치환한다
    - import/module loader의 DFS cycle detection과 type-resolution DAG를 혼합하지 않는다
  - 단계:
    - Phase A: declaration/type provider inventory와 consumer edge 수집
    - Phase B: topo evaluation + SCC/cycle diagnostic 고정
    - Phase C: generic default arg / multi-bound / ability consumer / zone authority를 DAG consumer로 편입
    - Phase D: incremental invalidation / cache / backend-facing resolved metadata 재사용
  - 회귀 기준:
    - dependency loop diagnostic에 cycle path/provenance가 나온다
    - graph-backed cycle과 alias fallback cycle 모두 `Contract source:`를 포함한다
    - namespace-only reference는 불필요한 full type materialization을 유발하지 않는다
    - generic consumer/default/bound resolution이 graph-backed evaluation에서도 기존 semantic 계약과 같은 결과를 낸다
    - C/LLVM compile path가 동일한 resolved-type metadata를 재사용한다
    - `PGY_TYPE_RES_STATS=1`에서 stage graph-backed skip 수, compatibility fallback 호출량, family breakdown, suppressed diagnostic 수가 보인다. 이 값은 남은 DAG migration debt의 직접 지표이며 숨겨진 fallback을 추가하면 smoke에서 즉시 드러나야 한다

- [x] **runtime observability baseline vs richer query 구분 고정**
  - 대상: `IntentLast* / IntentHistory* / IntentActive* / IntentRecent*`, zone/world inspection
  - 문제: baseline이 이미 있는데 문서가 thin이라고 쓰면 반대로 surface trust를 깎음
  - 고정 기준:
    - baseline observability는 complete로, richer timeline/provenance는 open debt로 분리
  - 회귀 기준:
    - docs/board/status 문구 일치
    - observability regression이 baseline API를 계속 고정

## 완료 (P0 — 즉시 수정)

- [x] **`system()` 명령 주입 제거** — `_spawnvp`/`execvp`로 교체, 경로 검증 추가 (`pgy_path_is_safe`)
- [x] **AES-256 실구현** — XOR 가짜 암호를 FIPS 197 AES-256-CTR + HMAC-SHA256 인증으로 교체 (외부 의존성 없음)
- [x] **`auto __tmp` 제거** — `PGY_RESULT_TRY` 매크로에서 GCC 확장 `auto` 제거, C11 호환 (명시적 타입 파라미터)
- [x] **REPL 고정 파일명** — `_pgy_repl_tmp.*` → `TMPDIR/pgy_repl_{pid}.*` (PID 기반 유니크 경로)
- [x] **`type alias` vertical slice** — `type UserId = Int;` parser/semantic/C/LLVM lowering 연결, 실전 annotation/typedef 경로 확보

## P1 — 다음 단계

- [ ] **CI 하드닝** — Ubuntu + Windows 빌드 매트릭스 유지, AddressSanitizer/UBSan, 더 촘촘한 smoke coverage
- [ ] **CodeQL + secret scanning 활성화** — C/C++ 분석 모드, push protection
- [x] **CHANGELOG.md + 버전 정책 수립** — SemVer, 릴리스 태깅 규칙
  - 완료: `CHANGELOG.md` 존재, Keep a Changelog 포맷, SemVer 명시
- [x] **SECURITY.md** — 보안 취약점 제보 채널, 책임 있는 공개 정책
  - 완료: `SECURITY.md` 생성 (2026-04-18). 지원 버전, 보고 채널, in/out scope, 공격 표면별 mitigation, advisory format 포함

## P1.5 — 언어/컴파일러 보강

- [ ] **MIR DCE statement-level 확장**
  - 현재는 dead SSA/PHI 제거 + `HasState`/`ChannelLength`류 pure-query stmt 제거까지는 동작함
  - 남은 단계: pure expression stmt / dead call / dead resource-op / carrier stmt를 더 세분화하고, side-effect lattice 기준으로 제거 정책을 정교화
  - 목표: MIR-only emitter가 기대하는 metadata carrier를 잃지 않으면서도 불필요한 stmt 제거 범위를 넓힘

- [x] **IR 계층 설계 검토** — HIR/DIR/RIR/MIR 분리 타당성 평가
  - **DIR 유지 결정**: intent domain structure 검증에 필수 (step dependency, zone binding, post-condition)
  - **RIR 유지 결정**: resource state lattice (20-state)는 slot/projection/authority lifecycle 검증에 필요
  - **MIR 유지 결정**: SSA/CFG/cleanup edge는 intent compensation execution path에 필수
  - ~~남은 과제~~: Backend를 HIR 기반 → MIR 기반으로 전환해야 IR 투자 ROI 실현 → **완료**
  - 참고: Rust도 AST→THIR→MIR→LLVM 4단계, Pergyra는 AST→HIR→DIR→RIR→MIR→Backend 6단계
  - DIR은 domain graph로 HIR와 구조가 달라 별도 IR로 유지하는 것이 타당
  - RIR 20-state lattice는 단순화 가능성 검토 (현재: Owned/Borrowed/Synced/Dirty/Stale/Published/Authorized 등)
- [ ] **ability 기반 연산자 dispatch 고도화** — 현재는 `role/impl ability` 메서드에서 `operator_<suffix>_<Type>` alias를 합성해 C/LLVM이 정적으로 호출하는 방식. 장기적으로는 ability/vtable 기반의 직접 dispatch와 더 정교한 overload 우선순위 규칙이 필요
- [ ] **LLVM 연산자 오버로드 회귀 테스트 확장** — 현재 스모크는 `role IntMath for Int` 1건 중심. 비교 연산, 포함된 role, enum/custom type, namespace 경로까지 자동 테스트 확대

## P1.58 — 표준 라이브러리 인프라

- [x] **`use datetime;` 실제 stdlib module화**
- [x] **`use http;` v0.1**
  - `HttpRequest`, `HttpResponse`, `RouteSpec`
  - `OkResponse`, `ErrorResponse`, `JsonResponse`
  - intent adapter handler 예제와 연결
- [x] **`use storage;` v0.1**
  - `SnapshotMeta`, `SnapshotRecord`
  - `StorageSave`, `StorageLoad`, `StorageAppendLog`
  - world/session snapshot 예제와 연결
- [x] **`use page;` v0.1**
  - `PageRoute`, `PageAction`, `PageMessage`
  - `MountPage`, `BindAction`, `RenderSection`
  - projection surface / action binder 예제와 연결
- [x] **쇼핑몰 예제를 stdlib 인프라 사용 버전으로 리프트**
  - `pages/` -> `use page;`
  - `api/` -> `use http;`
  - `report/storage` -> `use storage;`

- [ ] **`pgy scaffold project`에 app-infra starter 추가**
  - intent-first layout + `intents/ subjects/ zones/ world.pgy main.pgy`
  - optional `pages/ api/ report/` app adapter starter

## P1.58 — 표준 라이브러리 개선 (2026-04-06 분석)

- [ ] **stdlib page.pgy 실제 렌더링/컴포넌트 시스템으로 확장**
  - 현재: 단순 데이터 구조 + 렌더링 문자열 함수만
  - 목표: 페이지 라이프사이클(마운트/언마운트/업데이트), 컴포넌트 트리, 상태 관리
  - 제안: `Component` abstract base, `mount()`, `render()`, `update()`, `unmount()` 라이프사이클 훅
- [ ] **stdlib storage.pgy WriteFile 추상화**
  - 현재: `WriteFile` 내장 함수 직접 호출 → 플랫폼 의존성
  - 목표: Slot/Device 인터페이스로 분리 (`StorageDevice` ability)
  - 제안: `ability StorageDevice { Write(path, data) -> Result<Void, Error>; Read(path) -> Result<String, Error> }`
- [ ] **stdlib 전반 Result<T, Error> 패턴 활용**
  - 현재: `WriteFile`, `ReadFile` 실패 시 크래시 가능성
  - 목표: 모든 I/O 연산이 `Result<T, Error>` 반환
  - 제안: `?` 연산자와 조합해 에러 전파 자동화
- [ ] **datetime.pgy 메서드 일관성 개선**
  - 현재: `export class LocalDate` + `export func SameDate()` 혼재
  - 제안: 메서드 일관성 (`a.SameDate(b)` vs `SameDate(a, b)`) — 하나만 남기거나 둘 다 문서화

## IR 파이프라인

- [x] **DIR code layer 시작**
  - declaration graph
  - intent participant/step edge
  - role/ability completeness edge
- [x] **RIR code layer 시작**
  - explicit resource/projection/authority/capability/intent-policy fact
  - explicit resource op
  - scope-level normalized state summary
  - HIR-enriched branch/join `flow-block[...]` lattice summary
- [x] **MIR code layer 시작**
  - block/instruction skeleton
  - phi materialization
  - block-local SSA rename
  - instruction-level `def/use` 시작
  - rollback/invalidation exceptional CFG 시작
- [ ] **RIR lattice propagation 심화**
  - relation/effect/zone/world handle merge는 시작됨, conditional handle invalidation과 world-handoff lattice를 더 밀기
  - conditional authority/projection invalidation fact 확장
- [ ] **MIR full SSA / flow merge**
  - block-level version map은 시작됨, rename을 full def-use chain/liveness 수준으로 확장
  - cleanup convergence root는 시작됨, MIR-level `RIR-flow` merge와 cleanup convergence policy를 더 고도화
- [ ] **MIR DCE 확장 (statement-level)**
  - dead DEF/PHI 제거를 넘어 side-effect-free STMT/unused call 제거
  - 현재는 pure query builtin (`Has*`, `ChannelLength/Capacity/Space/Full/Closed`)만 안전 제거 시작
  - `unused pure let initializer` 제거는 source-local/runtime-backed storage와 충돌해 다시 보류
  - dead identifier-assign 제거는 loop/phi/live-out 오판이 남아 있어 계속 보수 보류
  - 다음 reopen 조건: value summary의 block-boundary / phi provenance를 이용해 loop-carried DEF와 진짜 dead local DEF를 분리
  - user call purity는 아직 보수적으로 side-effect 있다고 간주
  - RESOURCE_OP/CLEANUP_EDGE/abort/IO 등 side-effect 보존 규칙 명시
  - RPO 기반 liveness와 결합해 제거 정확도 개선
## P2.0 — Backend MIR 기반 전환 ✅ 완료

- [x] **emit_program()을 HIR 기반 → MIR 기반으로 전환**
  - **완료**: `emit_func_decl_from_mir_named()` 완전 구현
  - **결과**: MIR routine → SSA locals + CFG → C 코드 생성
  - **지원 기능**:
    - Intent compensation (cleanup blocks)
    - SSA versioned locals (`_pgy_ssa_name_N`)
    - PHI 노드 복사 (join block 진입)
    - BRANCH → if/else gotos
    - RESOURCE_OP → 런타임 함수 호출
  - **테스트**: 428 passed, 0 failed (기존 403 passed, 5 failed)
  - **아키텍처**:
    ```
    Domain IR:   Intent Recover → policy exclusive → step Heal → zone main → participant unit
    Resource IR: IntentBegin I1 → ConflictCheck exclusive → BindZone main → CallAction Recover
    MIR:         bb0: conflict_check(unit) → br !r0, bb_fail, bb1
                 bb1: call recover(unit) → call sync_projection(main, unit)
                 bb_commit: intent_commit(I1) → ret true
                 bb_fail: intent_abort(I1) → ret false
    ```

## P2.1 — LLVM 백엔드 MIR 기반 전환 ✅ 완료

- [x] **LLVM 백엔드 MIR 기반 전환 완료**
  - `src/codegen/llvm_pipeline.c`: MIR routine → LLVM IR 직접 생성
  - `src/codegen/llvm_mir_emit.c`: `llvm_emit_func_from_mir()` 완전 구현
  - SSA locals, PHI nodes, branch terminators, intent compensation 모두 지원
  - 기대 효과 달성: LLVM 최적화 패스 완전 활용, C/LLVM 백엔드 아키텍처 통일
  - C/LLVM 둘 다 MIR 기반으로 통일 → IR 투자 ROI 실현

## P1.55 — 언어 기능 확장

### 기반 타입 시스템
- [x] **태그드 유니언 (enum with data)** — `enum Shape { Circle(Int), Rect(Int, Int) }` 데이터를 가진 enum
  - 완료: variant payload 파싱, variant 생성자 타입 추론, C tagged union / LLVM discriminated struct, LLVM tagged-union regression 및 예제 실행
- [x] **Option<T> / None** — "상자가 비어있을 수 있다"를 타입으로 표현. `-1` sentinel 제거
  - 완료: `Option<T>` constructed type, `Some/None`, `IsSome/IsNone/UnwrapOption`, C/LLVM lowering
  - 완료: `match opt { case Some(v): ... case None: ... }` destructuring
- [x] **디스트럭처링 (SecureSlot)** — `let (slot, token) = ClaimSecureSlot<Int>(lvl)` 패턴 바인딩
  - 완료 (2026-04-19): 파서 `ClaimSlot`/`ClaimSecureSlot` 뒤의 `<T>`를 더 이상 버리지 않고 `AST_CALL.generic_args`에 첨부 (일반 call-site 제네릭 인프라), 시맨틱이 destructuring에서 이 generic arg로 SYMBOL_SLOT + SYMBOL_TOKEN 쌍 등록, MIR emit이 `PgyToken_T token; PgySecureSlot_T slot = pgy_claim_secure_T(&token);` 출력, `transpiler_find_local_type_name_in_block`이 바인딩별 `SecureSlot<T>`/`Token<T>` 반환해 MIR header의 타입 예약 정리, SSA 맵에 self-mapping 등록으로 emission contract 통과
  - 파일: `src/parser/ast.h`, `src/parser/ast.c`, `src/parser/parser.h`, `src/parser/parser_expr.c` (제네릭 인자 보존), `src/semantic/type_checker.c` (destructuring 시맨틱), `src/codegen/transpiler_emitters_base_a.inc` (MIR-level claim emit + ssa map 등록)
  - 회귀: `src/test_transpile.c` "let (slot, token) = ClaimSecureSlot<T>(lvl) emits paired claim"
  - SecureSlot MIR auto-Read + claim 토큰 emit 연관 버그 수정 (2026-04-19): (a) SSA-aware identifier 경로가 `suppress_slot_auto_read` 무시하던 버그로 `pgy_secure_write_Int(&pgy_read_Int(&slot),...)` 같은 잘못된 C 출력 — `!ctx->suppress_slot_auto_read` 가드 추가 + Secure 경로에서 `pgy_secure_read_*` 분기. (b) MIR DCE가 `AST_LET_DECL`을 부작용 없음으로 판정해 제거하던 버그 — `mir_stmt_has_side_effect`에 추가. (c) `transpiler_emit_mir_resource_op` Claim 룰이 SecureSlot에도 `pgy_claim_secure_T()`만 emit하고 토큰은 생략하던 버그 — `PgyToken_T anchor_token;` + `= pgy_claim_secure_T(&anchor_token)` 방식으로 수정. (d) `Token<T>`도 "claim shape"로 인식해 MIR header pre-decl 건너뛰도록 `transpiler_type_name_is_claim_shape` 도입 (slot-like와는 구별 — auto-Read는 여전히 Slot 전용). 결과: destructuring + 비-destructuring SecureSlot 모두 E2E 동작 (`Write/Read/Release` 포함)
  - 파일: `src/compiler/mir.c` (DCE), `src/codegen/transpiler_expr_emitters.inc` (suppress 가드), `src/codegen/transpiler_emitters_base_a.inc` (claim_shape 분리), `src/codegen/transpiler_emitters_base_b.inc` (MIR header 체크), `src/codegen/transpiler_helpers.inc` (claim 토큰 emit), `src/parser/parser_decl.c` (class-body destructuring 에러 메시지)
  - 미처리: LLVM 백엔드 SecureSlot destructuring (LLVM은 이미 "requires explicit annotation" 에러 — 별도 세션), class-body destructuring (`private let (slot, token) = ClaimSecureSlot()`는 명확한 에러 메시지로만 처리 — 별도 세션)
- [x] **튜플 반환 타입 + 디스트럭처링** — `func f() -> (Int, String)` 및 `let (n, s) = f()` 지원
  - 완료 (2026-04-19): Type 인프라에 `TYPE_KIND_TUPLE` 활성화 (union에 `tuple.elements/element_count` 필드 + `type_create_tuple`/`type_is_tuple`/`type_tuple_arity`/`type_tuple_get_element`), AST_TYPE에 `tuple_elements` 필드로 `(T, U, ...)` 표현, `AST_TUPLE_LITERAL` 신규 노드로 `(a, b, ...)` 표현식 지원
  - 파서: `parse_type()`에 `LPAREN` 분기로 튜플 타입 구문 처리 (단일 `(T)`는 기존 `T`로 환원, 빈 `()`는 `Void`, 2개 이상일 때만 튜플), `parser_parse_primary`의 괄호 표현식 경로에 콤마 감지 시 튜플 리터럴로 분기
  - 시맨틱: `resolve_type_node`에 tuple 분기 추가 → `type_create_tuple` 반환, `type_check_expression`에 `AST_TUPLE_LITERAL` 케이스로 요소 타입 수집, `AST_LET_DESTRUCTURE`에서 RHS가 tuple이면 arity 검증 + positional element 타입 할당
  - C 백엔드: `append_type_name`이 튜플을 `(T, U)`로 렌더, `pergyra_type_to_c`가 `(Int, String)` → `PgyTuple_Int_String_t`로 매핑 (depth-tracking 파서), `ensure_tuple_specialization_to`가 `typedef struct { T0 f0; T1 f1; ... } PgyTuple_<suffix>_t;`를 ctx->out에 중복 없이 방출, `emit_expression(AST_TUPLE_LITERAL)`이 compound literal `((PgyTuple_T_U_t){.f0=..., .f1=...})` emit, AST_LET_DESTRUCTURE MIR 경로/기본 경로 둘 다 tuple 분기로 `.f0/.f1/...` 필드 추출
  - LLVM 백엔드: `ast_type_to_llvm`이 tuple AST_TYPE → literal anonymous struct `{T0, T1, ...}`, `llvm_emit_expression(AST_TUPLE_LITERAL)`이 `LLVMGetUndef + InsertValue` 체인으로 집계값 구성, `llvm_emit_let_destructure`가 struct 필드 개수 + 첫 필드 비포인터 heuristic으로 tuple 판정 후 `ExtractValue` per-binding
  - 회귀: `tests/cases/backend_compare/destructure_tuple_retun/main.pgy` (C/LLVM 동일: `42/hello/7/11/true`), `compare_backends.sh` case 등록, `test-semantic 1653 passed`, `test-transpile 584 passed`
  - 파일: `src/semantic/type_system.{h,c}`, `src/parser/ast.{h,c}`, `src/parser/parser_decl.c`, `src/parser/parser_expr.c`, `src/semantic/type_checker.{c,_helpers.inc}`, `src/codegen/transpiler.h`, `src/codegen/transpiler_helpers_core_b.inc`, `src/codegen/transpiler_expr_emitters.inc`, `src/codegen/transpiler_emitters_base_{a,b}.inc`, `src/codegen/llvm_backend.c`, `src/codegen/llvm_expr.c`, `src/codegen/llvm_stmt.c`, `src/codegen/llvm_pipeline.c`
  - 후속 수정 (destructure + if 지원): `transpiler_register_with_alias_bindings_in_block`의 Claim-only 제한 제거 — 모든 destructuring 바인딩(array/slice/tuple/일반 call)의 이름을 self-mapping으로 precheck ssa_map에 등록. 실제 emit 경로는 여전히 `<name>.1` 버전드 이름을 MIR emit 시점에 ssa_map에 넣어서 사용 (self-map은 verifier 통과용 가드일 뿐). 결과: `let (a, b, flag) = f(); if flag { ... } else { ... }` 같은 패턴이 array/tuple 둘 다 C/LLVM에서 동작. 파일: `src/codegen/transpiler_emitters_base_a.inc` (register_with_alias_bindings_in_block)
- [ ] **sealed ability** — 구현 가능한 role을 제한 (`sealed ability Combatable` → 같은 모듈 내 role만 impl 가능)
- [x] **문자열 보간** — `f"값은 {x}"` → `StringConcat(...)` series로 lowering
  - 완료: lexer에서 `f"..."` → `TOKEN_INTERPOLATED_STRING`
  - 완료: parser에서 `{expr}` 파싱, `ToString(expr)` + `+` concatenation으로 분해
  - 완료: 기존 `"${expr}"` 레거시 문법도 호환 유지
  - 완료: 베타 stable subset을 `"..."`, `"""..."""`, `"${expr}"`, `f"{expr}"`, escaped f-string brace로 문서화
  - 완료: unmatched interpolation brace는 보간하지 않고 literal text로 보존하도록 parser 회귀 추가
  - beta-out-of-scope: nested brace matching, format specifier, multiline interpolation, custom interpolation protocol

### 에러 처리
- [x] **`?` 연산자** — `Result<T>` 에러 자동 전파. `let val = riskyFunc()?;` → 에러 시 즉시 반환
  - 완료: 시맨틱 검증, C early-retun lowering, LLVM `Result<T>` 레이아웃/unwrap/early-retun lowering, `pipe_and_try.pgy` C/LLVM 실행 검증
  - LLVM try.err 재구성 버그 수정 (2026-04-19): `let val = Validate(x)?;` 패턴에서 let_decl이 `current_ret_type`을 LHS var 타입(i32)으로 잠시 덮어쓰고 있어, `?`의 try.err 블록이 함수 retun 타입 struct 대신 i32로 판정 → `unreachable` emit → 런타임 crash. `ctx->current_func_decl`에서 AST 반환 타입을 재조회해 복구 + Err 값 재구성 (src_err → dst_err 정수/포인터 강제 변환 포함)
  - 파일: `src/codegen/llvm_expr_core.inc`
  - 회귀: `tests/cases/backend_compare/try_operator_result/main.pgy` (C/LLVM 동일), `examples/pipe_and_try.pgy`

### 편의 문법
- [x] **파이프 연산자** — `data |> Transform |> Validate |> Persist` 단방향 데이터 흐름
- [x] **defer** — `defer Release(s)` 스코프 종료 시 자동 실행
- [x] **`let` 타입 추론** — initializer 기반 기본 추론은 현재 구현됨
  - 완료: annotation이 없을 때 initializer 타입으로 추론
  - 남음: 문서/표면 예시를 더 공격적으로 타입 추론 중심으로 정리할지 결정

### 제네릭 클래스
- [x] **제네릭 클래스** — `class Pair<T>` 문법 + 시맨틱 + C 코드젠 (단형화). 예제: `examples/generic_class.pgy`

### Slot 소유권 모델
- [x] **`own`/`ref` 소유권 모델 확정 및 구현** — move 기본, 함수 시그니처에 명시
  - 완료: `own`/`ref` 키워드 (렉서/파서/AST), Slot 대입 시 move 시맨틱, Clone() 명시적 복사
  - `func Upload(own tex: Slot<Texture>)` → 소유권 이전, 원본 무효
  - `func Render(ref tex: Slot<Texture>)` → 빌림, 원본 유효
  - 문서화: `docs/22_ownership_model.md`

### Slot 표면 문법 개선 (P0 우선순위)
- [x] **암묵적 Read + 대입 기반 Write** — Slot의 기본 사용 표면을 일반 변수처럼
  - 완료: 읽기 문맥에서 `Slot<T>` auto-read
  - 완료: `slot = expr` → `Write(slot, expr)` lowering
  - 유지: `Release(slot)`는 계속 명시적

### Slot 최적화 (P0 우선순위)
- [x] **스택 할당 최적화** — 스코프를 벗어나지 않는 Slot은 malloc 대신 alloca
  - 완료: `slot_analyze_escape_flags()` (slot_analyzer.c)
  - 완료: LLVM 백엔드에서 `slot_escapes == false` 시 alloca 생성 (llvm_stmt.c:145-146)
  - 완료: escape analysis로 non-escaping slot 자동 스택 할당

### View 범위 부여 (리뷰 필요 — 미결정)
- [ ] **View에 바이트/인덱스 범위 부여** — 실제 사용 사례 만들어보고 결정
  - 안 A: Slice 기반 — `SliceOf(buf, 0, 1024)` → Slot의 "창문"
  - 안 B: View에 범위 부여 — `ViewRead(buf, offset, length)`
  - **미결정 — 파일 I/O, 네트워크 버퍼, GPU 텍스처 사례를 만들어보고 결정**

### 병렬/채널
- [x] **select 실체화** — 여러 채널 중 먼저 준비된 것을 처리

### 언어 완성도 Tier 1 — 범용 필수
- [x] **for-in 컬렉션 루프** — `for item in array { }` 배열/컬렉션 순회
  - 완료: Array<T>/Slice<T> 특수화 (index loop lowering), 시맨틱 element type 추론
  - 남음: ability 기반 Iterable<T> 프로토콜 (Tier 2)
- [x] **StringSplit / StringJoin** — 문자열 분리/결합 빌트인 실체화
  - 완료: `Split(s, delim) → Array<String>`, `Join(arr, sep) → String`
- [x] **ToInt / ToFloat** — 문자열→숫자 변환 빌트인
- [x] **기본 Math 빌트인** — Sqrt, Pow, Floor, Ceil, Random 추가 (기존 Abs/Min/Max + 신규 5개)
- [x] **ArraySort / ArrayMap / ArrayFilter / ArrayReverse** — 고차 함수 기반 컬렉션 연산
  - 완료: ArraySort(arr) → qsort, ArrayMap(arr, fn) → 새 배열, ArrayFilter(arr, fn) → 조건 필터, ArrayReverse(arr) → 뒤집기
  - fn은 함수 이름 또는 람다 (C 함수 포인터로 lowering)
- [x] **디스트럭처링** — `let (a, b, c) = expr` 배열/컬렉션 positional 바인딩
  - 완료: Array<T> → 인덱스 기반 추출 (`result.data[0]`, `result.data[1]`, ...)
  - MIR 통합 (2026-04-19): MIR DCE가 `AST_LET_DESTRUCTURE` 문을 "부작용 없음"으로 판정해 제거하던 버그 수정 (`mir_stmt_has_side_effect`). 트랜스파일러 MIR emit 루프에서 destructuring을 SSA-renamed 타겟으로 emit, `transpiler_find_local_type_name_in_block`에 AST_LET_DESTRUCTURE 케이스 추가해 로컬 타입 해석 복구
  - LLVM parity (2026-04-19): `llvm_emit_statement`의 AST_LET_DESTRUCTURE 케이스 추가 — 초기화식을 struct 값으로 평가, `ExtractValue(0)`으로 data pointer 추출, 각 바인딩마다 `GEP+Load`로 요소 추출 후 `alloca+store`+`llvm_scope_declare`로 로컬 등록. `llvm_lookup_array_var`로 elem_type 해석
  - 파일: `src/compiler/mir.c`, `src/codegen/transpiler_emitters_base_a.inc` (C 백엔드), `src/codegen/llvm_stmt.c` (LLVM 백엔드)
  - 회귀: `tests/cases/backend_compare/destructure_array/main.pgy` (C/LLVM 동일 출력), `examples/collection_ops.pgy` (hello/world/foo 출력)

### 메타프로그래밍 입장 (결정 완료)
- [x] **TMP 비채택** — 제네릭 monomorphization + ability dispatch로 95% 커버. 문서: `docs/23_metaprogramming_position.md`
- [ ] **향후 코드 생성 필요 시** — 컴파일 타임 플러그인 (proc_macro 모델) 또는 소스 생성기 검토

### 언어 완성도 Tier 2 — 실사용 편의
- [ ] **innate ability** — 같은 모듈 내 role만 impl 허용 (sealed 대신 innate 채택. 문서: `docs/24_visibility_model.md`)
  - 파서 완료, 시맨틱에서 `innate` 키워드 인식 (type_checker_decls.inc 참조)
  - 남음: 모듈 경계 검증 로직 완성
- [x] **제네릭 constraint 시맨틱** — `where T: Comparable` 시맨틱 검증
  - 완료: 파서 + 시맨틱 검증 (type_checker_helpers.inc:1847)
  - 완료: Generic function where-clause constraint validation
- [x] **OR 패턴** — `case 1 | 2 | 3:` match에서
  - 완료: lexer `TOKEN_PATTERN_OR`, parser 파싱, 시맨틱 검증
  - 완료: 리터럴 OR 패턴 지원 (`case 1 | 2 | 3:`)
  - 제한: variant destructuring OR 패턴은 아직 미지원 (`case .Some(v) | .None:`)
- [x] **enum 메서드** — `enum Direction { ... func Name(self) -> String }`
  - 완료: enum body에서 `func` 선언 + `self` 파라미터로 match self 본문 가능, C 컴파일 검증
- [x] **labeled break/continue** — `outer: while { ... break outer; }`
  - 완료: 파서 (`parser.c:1270`), AST (`break_stmt.label`), 시맨틱 (`test_semantic.c:680,714,739`), C 코드젠 (`loop_break_labels[]` + `loop_continue_labels[]`)
  - 검증: outer label break, 알 수 없는 label 거부, continue outer 모두 회귀 테스트 통과
- [x] **Custom error 타입** — `Result<T, E>` where E is user type (현재 String만)
  - 완료 (2026-04-18): 타입명 렌더 `PgyResult_Int_NetError` sanitize, `PGY_RESULT_DEFINE(Int_NetError, int32_t, NetError)` 자동 instantiation (`ensure_result_specialization_to` 신설), 편의 매크로 (`Ok_T_E`, `Err_T_E`, `IsOk_T_E`, `Unwrap_T_E`, `UnwrapOr_T_E`) 자동 생성, Ok/Err builtin이 `ctx->current_retun_type`에서 suffix 추출, match patten Ok/Err 바인딩 `__typeof__` 기반 타입 추론
  - 파일: `src/codegen/transpiler_helpers_core_b.inc` (generic_args_to_c_suffix + ensure_result_specialization_to), `src/codegen/transpiler_expr_emitters.inc` (Ok/Err/Unwrap suffix), `src/codegen/transpiler_emitters_base_b.inc` (match __typeof__), `src/codegen/transpiler.h` (result_specs_*)
  - 회귀: `src/test_semantic.c` "Result<T, E> with enum error type accepts Ok/Err and match destructuring"

### ability 차별화
- [x] **ability ≠ interface 문서화** — ability는 "협업 프로토콜의 자격 조건"이며 슬롯에 부착됨
  - 완료: `docs/24_visibility_model.md`에 `ability ≠ interface` 섹션 추가
  - 정리 내용: ability는 nominal object의 메서드 집합을 직접 모델링하는 interface가 아니라, `requires Ability`, `dyn role slot: Ability`, `zone authority requires Ability`처럼 협업 계약/자격 조건으로 소비되는 surface임을 고정
  - 정리 내용: ability는 subject/role/slot/orchestration contract와 결합되며, 구현 담당은 role impl이고 ability 자체는 "무엇을 구현하라"보다 "어떤 자격으로 참여하라"를 표현한다는 점을 명시

## P1.6 — 자원/오케스트레이션 방향 고정

### 분산 설계 결정 (2026-04-03 확정)
- [x] **RemoteFuture `await` → `Result<T>` 강제** — 원격 자원의 지연/실패를 타입 시스템에서 강제 노출
  - `Future<T>` (로컬) → await → `T` (실패 없음)
  - `RemoteFuture<T>` (원격) → await → `Result<T>` (실패 가능)
  - 시맨틱 체커 + C 코드젠 + 런타임 매크로 구현 완료
  - 테스트: 205 semantic + 141 transpile 통과
- [x] **RemoteFuture에 Claim/Read/Write/Release 차단** — 원격 자원의 동사는 Submit/Await만
  - Read/Write/Release 호출 시 친절한 에러 메시지 출력
  - "RemoteFuture does not support Read(); use 'await' to obtain Result<T>"
- [ ] **원격 Slot은 Claim 없이 Channel 기반 메시지 패싱만** — 분산 락 회피
  - 크로스 World 통신은 `Channel<T>`만 허용
  - 원격 자원에 Claim 동사를 사용하면 컴파일 에러
- [x] **World 경계 = 실패 도메인 경계** — 크로스 World 통신은 Channel만
  - 완료: World 시맨틱 체커 (`type_check_world_decl`, type_checker_decls.inc)
  - 완료: World 코드젠 (C 백엔드, transpiler_helpers.inc)
  - 완료: `HasZoneProjection`, `HasZoneLayer`, `HasZoneState` builtin

### Projection / Domain Query
- [x] **Projection query surface** — `HasProjection(slotName)`으로 relation/effect/zone 문맥에서 object/tobject projection slot의 sync-ready 여부를 질의
  - 완료: semantic + C/LLVM lowering
  - World 내부의 Slot은 로컬 fast path, World 간은 Channel (명시적 비용)

### 스케일링 대응 (레드팀 피드백 기반)
- [ ] **백엔드 역할 컷오프 고정** — C = reference/fallback, LLVM = optimization/mainline
  - 같은 의미론을 두 백엔드에 유지하되, 공격적 최적화와 type-erased fast path는 LLVM에만 집중
  - C 백엔드는 MVP 호환성, 디버깅, 폴백, 부트스트래핑 역할로 제한
  - 새 기능 추가 시 "C에서도 반드시 최적화 경로까지 구현해야 하는가?"를 기본적으로 `아니오`로 둠
- [ ] **매크로 조합 폭발 대응** — C 매크로 monomorphization의 장기 대안
  - 현재: `PGY_SLOT_DEFINE`, `PGY_CHANNEL_DEFINE` 등 타입별 전개 (부트스트래핑 전략)
  - 대안: LLVM 백엔드에서 type-erased 경로 (opaque ptr + vtable) 추가
  - LTO + dead code elimination으로 바이너리 비대화 억제
- [ ] **코드젠 이중화 억제 규칙** — bifurcation trap 방지
  - 동일 기능의 C/LLVM lowering이 영원히 쌍으로 비대해지지 않게 공통 의미론 테스트 우선
  - backend compare / smoke를 계약으로 유지하고, backend-specific fast path는 명시적으로 분리
- [ ] **Async 힙 할당 오버헤드 감소** — 고성능 분산 I/O를 위한 런타임 최적화
  - 현재: `pgy_spawn` + `malloc` per task
  - 대안: Arena allocator 기반 task pool, io_uring/IOCP zero-copy I/O
  - 코루틴 스택은 이미 fiber 기반 (pgy_parallel.h)
  - 단, 언어 코어와 OS 전용 스케줄러를 강결합하지 말 것
- [ ] **BYOS (Bring Your Own Scheduler) 경로 설계** — async 의미론과 스케줄러/I/O 모델 분리
  - 언어는 task/future/channel 의미만 고정
  - 실제 polling/runtime은 플랫폼별 주입 가능 계층으로 분리
- [ ] **ABI 다형성 전략** — 크기가 다른 슬롯 타입의 제네릭 처리
  - 의도적 설계: `Slot<T>` ≠ `SecureSlot<T>` (보안 차원 분리)
  - 다형성 필요 시: `ability` vtable dispatch (Party 시스템에 이미 구현)
  - Boxing 필요 시: `Rc<T>` + ability 조합
  - `Rc<T> + dyn ability`는 explicit high-cost path로 문서화
  - 값 경로(struct), 객체 경로(class), 동적 경로(Rc + dyn ability)를 성능 계약으로 구분

### 기존 항목
- [x] **Slot Protocol 고정** — Claim/Access/Mutate/Transfer/Release 불변 계약
- [x] **Slot/View 계층 마감** — ReadView/WriteView/MoveToken 권한 축소/이전 계층
- [ ] **슬롯을 추상 자원 핸들로 일반화** — 장기적으로 MemorySlot, DeviceSlot, SessionSlot 등 자원 클래스 확장
- [ ] **채널 의미론 강화** — 비동기 제출/대기/수거/후처리 흐름 보강
- [x] **`Future<T>`를 transfer boundary로 고정** — await/recv와 같은 ownership 경계
- [ ] **effect/resource capability 표기 도입** — `local cpu`, `secure device`, `remote` 등 타입/효과 시스템
  - 현재: derived effect mask + spawn/await/channel에서 remote 추론
  - 현재: `/// @effects ...` 선언이 있으면 body derived effect와 mismatch 진단
  - 다음: 시그니처 문법 차원의 선언적 annotation 표면
- [ ] **성능 목표를 orchestration overhead 중심으로 재정의**

## P1.7 — 의미 통일 언어로서의 다음 단계

### 비용 모델 / effect
- [ ] **비용 모델 표면화** — "semantic unity, visible cost" 원칙
  - `local / secure / remote / device` 자원군의 비용 차이를 표면에 드러내기
- [ ] **effect system 2단계** — 선언적 effect 표기, mismatch 진단
  - 부분 완료: structured comment `@effects` 기반 mismatch 진단
  - 부분 완료: source-level `with effects ...` 시그니처 surface
  - 남음: 더 정교한 effect lattice, call-site contract surface

### 상위 계층 모델
- [x] **최종 문맥 계층 / 설계 순서 분리 고정**
  - 조립 계층: `ability -> role -> party -> relation -> effect -> zone -> world`
  - 사용자-facing 설계 순서: `intent -> world -> zone -> subject`
  - 완료: `world`를 최상위 실행/신뢰/실패 경계라는 목표 정의로 문서화
  - 완료: 상위 레이어로 갈수록 덜 구속적이라는 설계 원칙 문서화
  - 완료: `relation`, `effect`, `zone` declaration keyword와 최소 `subject slot` / `object slot` surface를 parser/semantic 표면에 연결
  - 완료: `zone -> relation/effect`, `world -> zone` 최소 조립 slot surface를 parser/semantic에 연결
  - 완료: `relation`, `effect`의 optional `for ...` header로 subject endpoint/target 최소 surface를 연결
  - 완료: `zone`의 `apply effectSlot to targetSlot` 최소 attachment surface를 parser/semantic에 연결
  - 완료: `zone`의 `link relationSlot between left, right` 최소 relation wiring surface를 parser/semantic에 연결
  - 완료: `zone`의 `detach effectSlot from targetSlot`, `unlink relationSlot between left, right` 최소 release surface를 parser/semantic에 연결
  - 완료: `zone`의 `apply/detach`, `link/unlink`를 `effect/relation` declaration contract와 기본 타입/arity 수준으로 연결
  - 완료: `zone` subject shape에 대한 권장 lint 추가
  - 완료: `tobject` keyword를 `struct` 호환 projection alias로 추가
  - 완료: `ToObject(TargetStruct, subjectBinding)` 최소 passive projection surface를 semantic/C backend에 연결
  - 완료: `ToTObject(TargetDto, subjectBinding)` 최소 projection surface를 semantic/C backend에 연결
  - 완료: `relation/effect/zone`에 `tobject slot` surface를 연결
  - 완료: `relation/effect/zone`의 domain slot에 optional initializer를 연결해 `object slot view: View = ToObject(View, subject)` 같은 projection wiring을 직접 표현 가능하게 함
  - 완료: `zone`의 `refresh objectSlot from subjectSlot` surface로 projection 갱신 흐름을 parser/semantic에 연결
  - 완료: `zone`의 `publish dtoSlot from subjectSlot` surface로 tobject projection 갱신 흐름을 parser/semantic에 연결
  - 완료: `zone`의 `maintain effectSlot on targetSlot`, `maintain relationSlot between left, right` surface로 지속 lifecycle rule을 parser/semantic에 연결
  - 완료: `maintain` duplicate/conflict waning (`maintain` + `detach/unlink`) 추가
  - 완료: `zone`의 `authority subjectSlot` surface와 optional `by subjectSlot` authority annotation을 parser/semantic에 연결
  - 완료: `authority subjectSlot requires Ability[, Ability]` ability-gated authority surface를 parser/semantic에 연결
  - 완료: `zone`의 `state name: effect ... on ...` / `state name: relation ... between ..., ...` lifecycle alias surface를 parser/semantic에 연결
  - 완료: `zone`의 `apply/link/detach/unlink/maintain stateName` shorthand를 parser/semantic에 연결
  - 완료: `HasState(stateName)` zone query builtin을 parser/semantic에 연결하고 C backend에서 zone state field query로 lowering
  - 완료: `HasLayer(layerSlot)` zone query builtin을 parser/semantic에 연결하고 C/LLVM backend에서 zone layer field query로 lowering
  - 완료: `HasState(effectState, targetSlot)` / `HasState(relationState, leftSlot, rightSlot)` slot-aware state query를 semantic에 연결
  - 완료: `world`의 `state name: zone zoneSlot`, `activate/deactivate/maintain zoneOrState` lifecycle surface를 parser/semantic에 연결
  - 완료: `HasZone(zoneOrState)` world query builtin을 parser/semantic에 연결하고 C backend에서 world zone-state/active field query로 lowering
  - 완료: C backend가 zone/world마다 sync helper를 생성하고 method 전후에 `refresh`/`publish` projection과 lifecycle flag를 incremental하게 동기화
  - 완료: `relation`, `effect` declaration이 C/LLVM backend에서 struct + method wrapper로 codegen되고 runtime instance constructor/method path가 연결됨
  - 완료: `zone` layer slot이 C/LLVM에서 typed overlay runtime instance로 유지되고 sync가 subject slot을 layer endpoint/target에 바인딩한 뒤 projection sync까지 수행
  - 완료: direct `apply/link/detach/unlink`와 `maintain effect/relation/state`가 C/LLVM zone sync에서 실제 layer/state propagation으로 연결됨
  - 완료: zone embedded overlay projection read (`self.poison.view.hp`, `self.trust.packet.name`)가 LLVM runtime smoke로 검증됨
  - 완료: `world`가 `HasZoneProjection(zoneSlot, projectionSlot)` / `HasZoneLayer(zoneSlot, layerSlot)` / `HasZoneState(zoneSlot, stateName)`로 embedded zone runtime flag를 직접 질의할 수 있음
  - 완료: `ability/role/party/relation/effect/zone/roster/world` 전체 구현
  - 완료: `world`가 `state name: all zoneOrState[, ...]` / `state name: any zoneOrState[, ...]`로 앞서 선언된 zone/state alias를 최소 조합 contract로 합성
  - 남음: richer world-level runtime semantics, 더 깊은 cross-layer propagation policy

### 존재론 모델
- [x] **intent-first 설계 축 / subject-core host 축 분리 고정**
  - 완료: 사용자-facing 설계 순서는 `intent -> world -> zone -> subject`로 문서화
  - 완료: `subject = 상태와 identity를 가진 주체 타입`은 host/naming/lowering 축으로 한정해 문서화
  - 완료: `subject`와 `class`를 서로 다른 nominal flavor로 분리하고 의미론도 1차 분기
  - 완료: legacy host-profile surface를 제거하고 `subject`/`object`/`intent` 중심으로 정리
  - 완료: `entity`는 코어 언어 존재론에 넣지 않고 프레임워크/도메인 용어로 남긴다고 문서화
  - 완료: `object`는 intent를 시작하지 않는 passive state target이라고 문서화
  - 완료: `tobject`는 object의 외부 경계용 축약 투영이라고 문서화
  - 완료: `subject`, `class`, `struct`, `object`, `tobject` declaration flavor를 parser AST에 분리 기록
  - 완료: `subject slot`과 `ToObject` / `ToTObject` source가 `subject` host만 받도록 semantic 분기
  - 완료: `object` keyword alias를 parser/LSP surface에 반영
  - 완료: `object`를 passive state/value 형식으로, `tobject`를 더 좁은 projection/value 형식으로 정리하고 helper method를 허용
  - 완료: `vessel` declaration과 `subject` 내부 `vessel` field surface 추가
  - 완료: `subject` 전용 `action` declaration과 최소 clause (`requires/within/causes/authorized by`) parser/semantic 연결
  - 완료: `subject` 안의 legacy `func` 제거, `action` only 정책으로 승격
  - 완료: `role`/`party`/`authority`를 subject-core host 축으로 더 강하게 제한
  - 완료: C/LLVM method lowering에서 `subject=self-cell`, `class=value self` 1차 분기
  - 완료: legacy host-profile surface를 제거하고 관련 규칙을 `subject`에 통합
  - 완료: `subject` 단일 host surface로 통일
  - 완료: standalone host-profile surface 삭제
  - 완료: object를 effect/relation target으로 semantic/C/LLVM에 연결
  - 완료: domain-local `refresh` / `publish` source를 subject/object까지 확장하고 tobject source는 금지
  - 완료: relation/projection 중심 surface 고정

### 문서 / 스타일 정렬
- [ ] **BSD (Allman) canonical style 전면 고정**
  - 문서/예제/scaffold/formatter 출력은 BSD 기준으로 통일
  - K&R은 parser compatibility로만 남기고 canonical surface로는 취급하지 않음
- [x] **문서 예제 제시 순서 강제**
  - 완료: README entrypoint와 핵심 설계 문서에서 예제 독해 순서를 `intent -> world -> zone -> subject`로 명시
  - 기준 문서: `README.md`, `docs/00_vision.md`, `docs/01_intent_first_design.md`, `docs/22_class_object_model.md`
  - 규칙: `subject`는 core host로 설명하되, 설계의 첫 축으로 가르치지 않음
  - 규칙: compile-order와 teaching-order를 분리해서 명시

### slot 권한 / 자원군 확장
- [ ] **slot 권한 모델 고도화** — 공유 읽기 vs 독점 쓰기, capability narrowing
- [ ] **실제 자원군 확장** — SessionSlot, ChannelSlot, RemoteJob 고도화
- [x] **subject/class/object model 구현 정렬**
  - 완료: subject direct copy/plain value parameter/retun 금지, positional constructor
  - 완료: C/LLVM lowering 1차 분기 (`subject=self-cell`, `class=value self`)
  - 완료: legacy host-profile을 `subject` 규칙으로 통합
  - 완료: `subject` 단일 host surface로 통일
  - 완료: plain/secure `Slot<subject>` local object-cell anchor 지원
  - 완료: `own/ref Slot<subject-host>` / `SecureSlot<subject-host>` 함수 경계 전달을 semantic + C/LLVM backend에 반영
  - 완료: `Box<class>` explicit handle surface (`Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`)
  - 완료: richer object-handle cell propagation

### orchestration 완성도
- [ ] **오케스트레이션 모델 강화** — select 공정성, timeout, cancellation, backpressure
  - 부분 완료: `TryRecv/RecvTimeout -> Option<T>`, `TrySend/SendTimeout -> Bool`
  - 부분 완료: `TrySendStatus/SendTimeoutStatus -> Option<Bool>`로 full/timeout vs closed를 값으로 구분
  - 부분 완료: `ChannelLength/ChannelCapacity/ChannelSpace -> Int`, `ChannelFull/ChannelClosed -> Bool`
  - 부분 완료: `select` round-robin 시작 인덱스 fainess
  - 부분 완료: `Cancel(task)` / `IsCancelled()` cooperative cancellation
  - 부분 완료: spawned descendant cancellation propagation
  - 현재 제한: movable resource channel의 non-blocking/timeout transfer는 미지원
  - 현재 제한: pressure observation은 가능하지만 bounded policy/backpressure protocol은 아직 미구현
  - 현재 제한: preemptive cancellation, blocked thread task interruption, structured cancellation scope/lattice는 미지원
- [x] **async/await runtime 고도화** — POSIX ucontext + Windows Fiber 기반 coroutine
- [ ] **Windows coroutine 검증/고정**

### 툴링 / 표준면
- [ ] **stable stdlib surface 재고정**
- [ ] **툴링 단계 진입** — formatter, LSP 진단 품질
- [x] **ontology-first scaffold 정렬**
  - 완료: `pgy scaffold` help를 `subject/class/object/tobject` 우선 분기로 정렬
  - 완료: `class` scaffold kind 추가
  - 완료: `project/simulator` scaffold가 `subject`가 `class`를 소유하고 `object/tobject`로 투영하는 starter shape를 생성
  - 완료: `project` scaffold가 intent-first layout(`intents/`, `subjects/`, `zones/`, `world.pgy`, `main.pgy`)을 실제로 생성
  - 완료: `pgy new`가 `intent-first` / `class-first` / `projection-first` starter를 선택하게 할지 검토
  - 완료: `pgy new` / scaffold output에 ontology decision guide file 별도 생성 검토
  - 완료: intent-first project guide 문서도 scaffold output에 같이 생성할지 검토
    - `intents/`를 프로젝트 table-of-contents로 설명하는 guide 포함
    - intent declaration이 필요한 subject/zone/ability/effect TODO를 역산하는 workflow 예시 포함
  - 완료: intent runtime follow-up
    - rollback policy를 current reverse-order `compensate` beyond v1로 확장하기
    - intent의 cross-world transfer / identity handoff semantics 설계 및 구현
    - current last-intent typed history를 trace id / stream / multi-instance observability로 확장하기

### 대표 프로그램
- [ ] **대표 애플리케이션 3종** — 이종 자원 파이프라인, secure+device+channel, slot/orchestration 철학 증명

## P1.85 — 게임 프레임워크 계층

- [ ] **게임 프레임워크 라이브러리 경계 고정**
  - 원칙: `entity/object pool`은 언어 코어 기능이 아니라 `use pool;` 같은 게임/앱 라이브러리 계층으로 둔다
  - 원칙: `encounter/tun/state machine`, `strategy/AI`, `content tables`도 동일하게 코어 문법이 아니라 프레임워크 surface로 쌓는다
  - 원칙: 이 계층은 “도메인 라이브러리”보다 “generic patten library + domain injection”으로 정의한다
  - 이유: 코어 언어는 `subject / vessel / object / tobject / relation / effect / zone / world / Slot<T>` 의미론을 유지하고, 대규모 게임 설계는 그 위의 library/DSL 계층으로 올리는 편이 확장성과 설명력이 더 좋다
  - 목표: “게임을 만들 수 있는 코어 언어”와 “게임을 실제로 만드는 프레임워크”를 분리
- [ ] **게임 stdlib/use surface 초안**
  - 후보: `use pool;`, `use fsm;`, `use encounter;`, `use strategy;`, `use tables;`
  - 방향: pool/fsm/strategy/table은 `.pgy` 또는 stdlib 모듈로 제공하고, 언어 키워드로 승격하지 않는다
  - 방향: `Pool<T>`, `StateMachine<TState, TEvent>`, `StrategyTable<TContext, TChoice>`, `WeightedTable<T>`처럼 generic-first naming을 우선한다
  - 방향: GOF 기초 패턴도 inheritance/object graph가 아니라 Pergyra host 기준으로 번역한다
    - `singleton` -> contextual runtime registry / host-local shared state
    - `factory` -> staged template/spec builder
    - `strategy` -> policy card / policy table + function injection
    - `state` -> explicit FSM / transition rule + context application
    - `observer` -> relay bundle / sink spec / report sink / event bus
  - 방향: generic patten library는 static spec/table만이 아니라 function-typed picker/resolver 주입도 기본 표면으로 포함한다
    - 예: `Picker<TInput, TChoice>`
    - 예: `Resolver<TContext, TResult>`
    - 예: `StrategyApply(context, AggressivePolicy)`
  - 현재 상태: `data/card/table` 경로는 안정, custom function injection도 V1 표면이 올라옴
  - 현재 전략 패턴의 안정 단계:
    - `StrategyCard`
    - `StrategyContext`
    - `ApplyStrategy(card, context)`
  - 이번 예제 기준 라이브러리화 후보:
    - `use strategy;`
      - `WeaponCard` / `CombatStrategyCard`
      - `WeaponFactory<TClass>` 또는 `LoadoutTable<TArchetype>`
      - `StrategyTable<TContext, TChoice>`
      - `ActionTextFactory<TContext>` / `EffectTextFactory<TContext>`
    - `use tables;`
      - `SceneChoiceCard`
      - `CompanionEventCard`
      - `BossPhaseCard`
      - `WeightedTable<T>`
      - `ChoiceTable<TState, TOption>`
    - `use encounter;`
      - `EncounterStateMachine<TState, TEvent>`
      - `TunLoop<TActor, TAction>`
      - `BossPhaseMachine<TPhase>`
      - `ResolutionLedger<TSnapshot>`
    - `use report;`
      - transcript accumulator
      - exact report writer
      - stdout/results dual sink
    - `use campaign;`
      - scripted / random / player mode runner
      - input script playback
      - seeded choice resolver
- [ ] **GOF 기초 패턴을 Pergyra식 patten catalog로 정리**
  - 기준 문서: `docs/31_gof_patten_catalog.md`
  - 기준 예제: `examples/patten_library_basics/`
  - 목표: 전통 OOP 패턴 이름을 유지하더라도 실제 구현 shape는 `subject / vessel / shared / spec / card / relay`로 재정의
  - 비목표: inheritance / `super` / hidden callback graph를 패턴 구현의 기본값으로 채택하지 않음
- [ ] **DND/campaign 시나리오를 게임 프레임워크 검증장으로 사용**
  - `dnd_taven_campaign`를 기준으로 pool/fsm/strategy/table이 실제로 충분한지 검증
  - language core 부족이 아니라 framework layer 부족인지 계속 분리해서 기록
  - 지금까지 뽑힌 실제 패턴:
    - 장소/장면 진입 팩토리 (`OpenTavenCampaign`)
    - 게임 상태 머신 (`taven -> floor1 -> floor2 -> floor3 -> dragon -> epilogue`)
    - 선택 해석기 (`scripted` / `random` / `player`)
    - 장면 카드 / 동료 반응 카드 / 보스 페이즈 카드
    - 전투 loadout/strategy 카드
    - transcript-first report writer
  - 다음 목표:
    - 위 패턴들을 `examples/` 전용 코드가 아니라 `use` 라이브러리 후보로 재구성
    - `world.pgy`의 orchestration 양을 줄이고 encounter/strategy/report 계층으로 분리

## P1.8 — 멀티 타겟

- [ ] **공통 UI IR 고정** — Kotlin/Android 개별 백엔드보다 먼저, 모든 플랫폼이 공유하는 scene/projection UI IR을 정의
  - 목적: native / web / mobile이 같은 UI 의미론과 projection 흐름을 공유하게 함
  - 원칙: 기술 기반은 Qt 방향(native shell / render loop), 선언 철학은 WPF식 projection/binding, 최종 정체성은 Pergyra scene/projection UI
  - 범위: `Window`, `Scene`, `Node`, `Layout`, `DrawCommand`, `InputEvent`, `ProjectionBinding`, `DirtyScope`
  - 원칙: `subject`를 직접 화면에 그리지 않고 `object` / `tobject` / projection surface를 UI 소비 표면으로 사용
  - 원칙: `zone` / `world` state와 projection dirty sync가 UI IR의 갱신 계약이 됨
  - 순서: UI IR 고정 → native backend 1개 → JS/web backend 1개 → 그 뒤 mobile shell / Kotlin 필요성 재평가
  - 비목표: 플랫폼별 UI 의미론(Qt widget tree, WPF object model, Android View/Compose semantics)을 코어 언어에 직접 들이지 않음
- [x] **JavaScript 백엔드 직접 경로 거부** — `.pgy → JS` 변환은 베타/dogfood
  경로가 아님
  - 완료: GC + reference-only lowering이 Slot/Zone/Intent/Authority parity를
    흐릴 수 있어 직접 JS backend를 거부하고 C→Emscripten bridge를 1차 경로로
    고정
  - 남음: 필요 시 beta+1에서 JS shim/host import surface만 별도 설계
- [ ] **mobile shell 전략** — Android/iOS는 우선 공통 UI IR consumer로 접근
  - 원칙: 초기 mobile 대응은 JS/web-compatible UI backend 또는 native shell bridge를 우선 검토
  - 남음: Android 전용 Kotlin backend는 공통 UI IR + web/native backend 검증 뒤 필요성을 재평가
- [~] **WebAssembly/WebGL dogfood bridge** — C backend `--emit-c` + optional
  Emscripten을 1차 경로로 사용. native LLVM wasm32 target은 beta+1

## P1.9 — AI-first 인프라 (2026-04-19 positioning 확정)

**맥락**: 경쟁 대상은 C#/Java ↔ Rust 사이 니치이고, 1차 사용자는 frontier LLM(Claude 등)이 주도 + 인간이 리뷰/수정하는 워크플로. "AI가 생성 → 컴파일러/테스트가 검증 → 인간이 리뷰"의 loop이 타이트하게 돌아가는 것이 positioning 핵심.

현재 의도치 않게 갖춰진 AI-friendly 인프라:
- backend-compare 회귀 (C/LLVM 출력 대조) — AI self-verification loop 하네스
- 2000+ test suite + 스모크 체인 — 생성물 즉시 검증 가능한 규모
- Result-first + throw 금지 — AI가 stack trace보다 ErrorCode enum 분기가 쉬움
- 구조화 주석 (WHAT/WHY/ALT/NEXT/EFFECTS/INVARIANTS/RETURNS/THROWS) — prompt-as-code, 의도 보존

부족하고 채워야 할 것:

- [ ] **Language Reference Spec 문서** — 현재 `docs/`는 설계 일지(의사결정 흐름 기록). AI에게 정확한 의미론 제공하려면 "이 언어의 보장"이 한 문서에 정리돼야 함
  - 내용: 타입 시스템 규칙 / Slot 소유권 계약 / effect subsumption / intent rollback 의미 / Result 전파 규칙 / MIR 계약
  - 형태: 단일 파일 (~2000-5000줄), in-context로 한 번에 로드 가능
  - 목적: "Claude가 Pergyra 코드를 새 세션에서 생성할 때 reference로 인용 가능" 수준
  - 현재 `docs/`와 다른 점: 일지는 "왜 이렇게 결정했는가", spec은 "현재 언어가 무엇을 보장하는가"
- [~] **AI-parseable 구조화 에러 메시지** — 현재 진단은 내부자 표현. AI용은 기계 판독 가능한 구조화 필드 필요
  - 현재: `MIR contract breach in Main at line 0: unresolved identifier 'flag' (expected SSA-mapped local)`
  - 목표 형태 (예시):
    ```json
    {
      "severity": "error",
      "stage": "MIR_validation",
      "code": "PGY_MIR_UNRESOLVED_IDENT",
      "location": {"file": "main.pgy", "line": 7, "column": 8},
      "summary": "destructuring binding 'flag' is not SSA-mapped at use site",
      "cause_ir": "a.1 DEF is emitted in block 0 but not propagated to branch-consumer block via ssa_entry_values",
      "fix_source": "ensure destructure binding is referenced within the same block as the destructure, or use let_decl with explicit type to trigger SSA renaming",
      "related_rules": ["MIR.SSA.entry_values", "destructure.binding"]
    }
    ```
  - `--error-format=json` 플래그로 토글, 인간용은 기존 형식 유지
  - 대상: compile, semantic, MIR/LLVM IR 단계 전체
  - 1차 증분 완료 (2026-04-19):
    - `DriverFlags.diag_format` + `--error-format=json|text` CLI 플래그 추가 (`src/pgy_driver.c`, `src/compiler/driver_app.h`)
    - `semantic_result_print_json` — semantic 진단을 JSON 배열로 방출 (severity/stage/location/message 필드, RFC 8259 준수 이스케이프)
    - `driver_emit_single_diag_json` — 단일 에러 JSON 방출 헬퍼 (module_load / backend_c_emit / backend_c_native / backend_llvm_emit / backend_llvm_native 단계 커버)
    - stage 태그: `semantic` / `module_load` / `backend_c_emit` / `backend_c_native` / `backend_llvm_emit` / `backend_llvm_native`
    - 성공 시 `[]` (빈 배열), 실패 시 `[{...}]` — 호출자는 항상 JSON 기대 가능
    - 회귀: `tests/diagnostics_json_smoke.sh` (Python 파서로 shape 검증, 3 케이스: semantic / parse / success)
    - 검증: PowerShell로 3 케이스 모두 정상 동작 확인 (1668 semantic + 601 transpile 회귀 pass)
  - 2차 증분 완료 (2026-04-19):
    - `Diagnostic` 구조체에 `code` 필드 추가 (non-owning `const char*`, 정적 문자열 리터럴 보관) — `src/semantic/type_checker.h`
    - `semantic_error_code` / `semantic_waning_code` 신규 variant — 코드 인자 받아 diagnostic에 실어줌 (레거시 `semantic_error` 는 그대로 NULL 코드로 동작, 단 동일 사이트 중복 emit 시 코드가 있으면 업그레이드)
    - JSON 출력에 `"code"` 필드 선택적 포함 (NULL이면 생략 — 호환성 유지)
    - parser stage 분리: module_load msg가 `"parse error in"`으로 시작하면 `"stage":"parse"`, 그 외 `"module_load"`
    - 초기 코드 부여 사이트 (6종):
      - `PGY_SEM_TYPE_MISMATCH` (assignment)
      - `PGY_SEM_BINOP_TYPE_MISMATCH`
      - `PGY_SEM_UNKNOWN_TYPE`
      - `PGY_SEM_UNDEFINED_SYMBOL` (identifier / member 3 사이트)
      - `PGY_SEM_INFER_COLLECTION` / `PGY_SEM_INFER_GENERIC` / `PGY_SEM_INFER_REQUIRED`
    - smoke test 확장: `code == "PGY_SEM_TYPE_MISMATCH"` 검증 + `stage == "parse"` 검증 (`tests/diagnostics_json_smoke.sh`)
    - 회귀: 1688 semantic + 601 transpile, 0 failed
  - 3차 증분 완료 (2026-04-19):
    - Slot/ownership/parallel/effect 계열 코드 9종 추가:
      - `PGY_SEM_SLOT_RELEASED` (method dispatch 4 사이트 + builtin Read/Write 2 사이트)
      - `PGY_SEM_RELEASE_REQUIRES_OWNER`
      - `PGY_SEM_SLOT_DOUBLE_RELEASE` (method + builtin Release 2 사이트)
      - `PGY_SEM_VIEW_KIND_MISMATCH` (ReadView write / WriteView read)
      - `PGY_SEM_MOVE_TOKEN_MISUSE` (read/write through MoveToken)
      - `PGY_SEM_MOVE_FROM_RELEASED` (let/call/builtin 3 사이트)
      - `PGY_SEM_PARALLEL_SLOT_CONFLICT` (error: mutate-mutate across tasks)
      - `PGY_SEM_PARALLEL_SLOT_RACE_RISK` (waning: read-mutate across tasks)
      - `PGY_SEM_EFFECT_CONFLICT` (waning: effect class 충돌)
    - `docs/72_diagnostic_codes.md` 카탈로그 문서 신규 — 16개 코드 의미/원인/교정 방법, AI 라우팅 가이드, 향후 확장 필드 문서화
    - smoke test 확장: `PGY_SEM_SLOT_RELEASED` 감지 케이스 추가
    - 사용자 기여: `semantic_error_code` / `semantic_waning_code` 선언에 `PGY_PRINTF_LIKE` 속성 추가 (clang/gcc format 경고 체크)
    - 회귀: 1694 semantic + 601 transpile, 0 failed
    - 현재 총 16개 안정 코드, ~25 사이트 커버. 나머지 ~460 사이트는 4차+ 증분 대상
  - 4차 증분 완료 (2026-04-19):
    - `CompilerResult.error_code` / `TranspileResult.error_code` / `LLVMGenResult.error_code` 필드 추가 (모두 owning strdup, destroy에서 free)
    - `TranspilerCtx.backend_error_code` / `LLVMGenCtx.error_code` non-owning `const char *` (정적 literal만)
    - 신규 setter variants: `transpiler_set_backend_error_with_code` / `llvm_set_error_with_code` / `llvm_set_error_at_with_code` (레거시 setter는 code=NULL 경로로 유지)
    - `driver_emit_single_diag_json_with_code(stage, code, message)` — JSON에 code 필드 선택적 포함
    - `driver_route_stage(default_stage, code)` — prefix whitelist (`PGY_SEM_`/`PGY_MIR_`/`PGY_LLVM_`/`PGY_PARSE_`). 모르는 prefix는 default_stage 유지
    - Runner 업데이트: `c_runner.c` (2 사이트) + `llvm_runner.c` (2 사이트) — 기존 호출을 `_with_code` + `driver_route_stage`로 교체
    - MIR/LLVM 코드 5종 신규:
      - `PGY_MIR_UNRESOLVED_LOCAL` — branch terminator의 identifier가 SSA 매핑 없음
      - `PGY_MIR_TOPOLOGY_INVALID` — MIR routine 누락 / kind 불일치 / AST 없음
      - `PGY_MIR_SIGNATURE_UNSUPPORTED` — 지원 안되는 함수 시그니처
      - `PGY_MIR_SSA_LIMIT` — SSA local 4096 초과
      - `PGY_MIR_INTENT_CARRIER_MISSING` — intent step metadata 누락 (C/LLVM 공통, 21 사이트 일괄 업그레이드)
      - `PGY_LLVM_SPEC_LIMIT` — Result\<T,E\> 특수화 한도(MAX_LLVM_RESULT_SPECS=32) 초과
    - 카탈로그 확장: `docs/72_diagnostic_codes.md`에 "MIR Contract" 섹션 5개 엔트리 + "LLVM Backend" 섹션 1개 엔트리
    - smoke test 확장: 33개 Result\<Int, E*\> 특수화로 `PGY_LLVM_SPEC_LIMIT` + `stage=llvm_codegen` 검증 (`tests/diagnostics_json_smoke.sh`)
    - 검증: `[{"severity":"error","stage":"llvm_codegen","code":"PGY_LLVM_SPEC_LIMIT",...}]` end-to-end 확인
    - 회귀: 1694 semantic + 601 transpile, 0 failed (레거시 경로 무손상)
    - 현재 총 22개 안정 코드 (`PGY_SEM_*` 16 + `PGY_MIR_*` 5 + `PGY_LLVM_*` 1), ~50 사이트 커버. `mir_validation` / `llvm_codegen` stage 가 기존 `backend_*_native`와 분리됨
  - 남은 작업 (5차 증분 후보):
    - intent/zone/world / class/ability 관련 `PGY_SEM_*` 코드 점진적 부여 (나머지 ~460 semantic 사이트)
    - LLVM 추가 코드: `PGY_LLVM_TYPE_UNSUPPORTED`, `PGY_LLVM_RUNTIME_MISSING`, `PGY_LLVM_OOM` (개별 사이트 업그레이드)
    - `cause_ir` / `fix_source` 필드 — 현재 message만. MIR/IR 레벨 원인 + 소스 레벨 교정 포인트 분리해 AI가 구분 가능하게
    - parser 레벨 코드 (`PGY_PARSE_*` prefix 예약됨) — parser error 누적형 리팩터 필요
    - `related_rules` 필드 — Language Reference Spec 이후 연결
- [ ] **In-context example corpus 큐레이션** — GitHub에 Pergyra 코드 0개. 훈련 데이터 부재를 in-context examples로 보완
  - `docs/ai_prompt_bundle/` 디렉토리에 몇 개 레벨의 번들 준비:
    - `minimal.md` — 언어 핵심만 (~20KB)
    - `standard.md` — core + stdlib + 5개 패턴 예제 (~100KB)
    - `complete.md` — 위 + 전체 examples + reference spec (~500KB-1MB)
  - 각 번들은 "이 번들만으로 새 세션에서 AI가 Pergyra 코드를 신뢰성 있게 생성 가능한가"를 검증 기준으로
  - 전략적 결정: 1차 audience는 frontier 모델(Claude Opus, Sonnet) 사용자. 소형/저가 모델은 2차
- [ ] **AI iteration-friendly 빌드 툴체인** — 빠른 컴파일 + 기계 판독 출력 + LSP 진단
  - 증분 컴파일 — 현재 단일 TU로 전체 빌드. module 단위 증분으로 전환
  - 테스트 결과 JSON 출력 — 현재 stdout ✓/✗ 형식. AI가 파싱해 다음 액션 결정할 수 있는 JSON 모드
  - LSP 진단 기계 판독 가능 — 위의 구조화 에러 메시지와 공유 포맷
  - backend-compare 실패 시 diff를 구조화 — 현재 unified diff. AI가 "어느 함수의 몇 번째 stdout 라인이 다름"을 바로 인지 가능한 포맷
  - 일부 기반 있음 (`src/lsp/` 디렉토리, `tests/compare_backends.sh` 구조)

**성공 기준**: Frontier 모델이 Pergyra spec bundle을 in-context로 들고, 비자명한 비즈니스 로직 (예: 결제 + 멱등성 + 재시도 정책) 구현을 one-shot에 가깝게 생성할 수 있음. 컴파일/테스트 실패 시 구조화 에러로부터 자기 교정 루프가 ~3회 이내 수렴.

## P2 — 배포 시작 시

- [ ] **문서-구현 동기화** — 테스트 수/기능 범위 일치
- [ ] **SBOM (SPDX) + provenance (SLSA)** — 공급망 투명성
- [ ] **릴리스 아티팩트** — 서명된 바이너리, 체크섬, 설치 스크립트
- [ ] **3rd-party NOTICE** — OpenSSL/LLVM/pthread 라이선스 정리

## IR 파이프라인 재구성

- [x] **컴파일러 계약 고정** — `HIR/DIR/RIR/MIR`, resource lattice, intent compensation, projection sync, authority/capability를 `docs/37_compiler_contracts.md`에 고정

- [~] **DIR (Domain IR)** — declaration graph / intent step graph 시작
  - 완료: `src/compiler/dir.h`, `src/compiler/dir.c`, `src/compiler/dir_collect.c`, `src/compiler/dir_collect_domain.c`, `src/compiler/dir_validate.c`, `pgy --dir`, `test-dir`
- 완료: DIR owner split — `dir.c`는 graph storage / lookup / lower orchestration만 담당하고, node/role/party/world/intent collection, zone/relation/effect projection collection, validation/dump는 별도 TU로 분리됨 (`dir.c` 467 LOC, `dir_collect.c` 546 LOC, `dir_collect_domain.c` 274 LOC, `dir_validate.c` 278 LOC)
- 완료: DIR storage growth debt — node/edge/owned-name/intent/participant/step/name arrays use explicit capacity fields and geometric growth; `tests/perf_contract_smoke.sh` rejects the old `count+1` realloc patten.
  - 완료: intent participant/type edge, step zone/ability/authority/effect edge, step predecessor dependency
  - 완료: role/ability completeness edge, missing-ability-method edge
  - 남음: richer zone/world membership graph
- [~] **RIR (Resource IR)** — slot/resource/authority/lifecycle 의미론 전용 계층
  - 범위: `Slot`, `SecureSlot`, `DeviceSlot`, projection validity, authority, effect/relation lifecycle, intent compensation resource edge
  - 완료: `src/compiler/rir.h`, `src/compiler/rir.c`, `pgy --rir`, `test-rir`
  - 완료: scope별 normalized state summary (`initial_state`, `final_state`, `last_op`, `transition error`)
  - 완료: relation/effect layer slot와 world zone slot도 resource fact로 materialize
  - 출력: 단순 map이 아니라 `Resource Graph + Transfer Ops + Static Ownership Facts`
  - explicit op 정규화:
    - `Claim/Read/Write/Release`
    - `Move/BorrowRead/BorrowWrite`
    - `ProjectRefresh/ProjectPublish`
    - `AttachEffect/DetachEffect`
    - `LinkRelation/UnlinkRelation`
    - `Authorize/AwaitRemote`
    - `CommitIntent/AbortIntent/CompensateIntentStep`
  - state lattice 초안:
    - `Uninit`
    - `Owned`
    - `BorrowedRead`
    - `BorrowedWrite`
    - `Moved`
    - `Released`
    - `Invalid`
    - `Measured`
    - `RemotePending`
  - CFG 의존 branch/join/loop/phi merge는 MIR로 이월
- [~] **MIR (Machine / Execution IR)** — CFG/SSA/liveness/optimization 계층
  - 범위: basic block, explicit instruction, phi, liveness, CFG-dependent resource merge, dead code elimination
  - 완료: `src/compiler/mir.h`, `src/compiler/mir.c`, `pgy --mir`, `test-mir`
  - 완료: HIR CFG -> MIR block bridge
  - 완료: RIR op -> MIR instruction bridge
  - 완료: intent cleanup block skeleton
  - 완료: phi materialization + incoming predecessor value list
  - 완료: block-local SSA rename skeleton
  - 완료: intent cleanup successor edge skeleton
  - 필요: `RIR-flow` merge 정책
  - 필요: richer phi merge policy
  - 필요: cleanup / rollback / detach-invalidation edge 고도화
## Progress Log — 2026-04-24 Parser/Lexer Diagnostic Routing

- 완료: parser/lexer diagnostic routing 1차 gate를 닫았다.
- 구현: `parser_error`는 `PGY_PARSE_SYNTAX`, `parse:unexpected_token`, `check-syntax`를 `Code:` / `Reason:` / `Fix:` 표면으로 출력한다.
- 구현: lexer error token은 `PGY_LEX_INVALID_TOKEN`, `lex:invalid_token`, `remove-or-escape-character`를 같은 표면으로 출력한다.
- 검증: `make parser-lexer-diagnostic-test-smoke`, `make diagnostic-registry-test-smoke`, `make test-parser`.
- 남음: parse/lex diagnostics를 driver JSON diagnostic object로 직접 흘리는 refactor는 별도 Tier 2 작업으로 유지한다.

## UTF-8 Progress Note - 2026-04-25

- `TryRecv` / `RecvTimeout` are now copy-only for the beta surface.
- Ownership-bearing payloads (`QubitSlot`, `Slot<T>`, `SecureSlot<T>`,
  `subject`, boundary-value aggregates, and `Token<T>`) are explicitly rejected.
- Use blocking `<-` receive into a named binding or a plain projection/value
  channel when ownership provenance must cross a channel boundary.

## UTF-8 Progress Note - 2026-04-25 - Cancellation Payload Boundary

- `Cancel(Future<T>)` / `Cancel(RemoteFuture<T>)` are copy-only for beta.
- Ownership-bearing payload futures (`QubitSlot`, `Slot<T>`, `SecureSlot<T>`,
  `subject`, boundary-value aggregates, and `Token<T>`) are explicitly rejected
  until task-boundary cleanup summaries can prove observation/release.

## UTF-8 Progress Note - 2026-04-25 - Channel Close Boundary

- `ChannelClose(Channel<T>)` is copy-only for beta.
- Ownership-bearing queued payload channels (`QubitSlot`, `Slot<T>`,
  `SecureSlot<T>`, `subject`, boundary-value aggregates, and `Token<T>`) are
  explicitly rejected until channel cleanup/backpressure summaries can prove
  drain/release behavior.
## Progress Log - 2026-04-26 - DAG Fallback Seam Cap

- Owner-local resolver files no longer own direct fallback helper seams. They now call
  `semantic_type_resolution_lookup_or_materialize(...)`, which checks DAG
  metadata, materializes stable constructed shells, then falls through to the
  centralized resolver fallback only when imported ability/default/bound/module
  cases still need compatibility materialization.
- `tests/type_resolution_resolver_inventory_smoke.sh` now caps active
  named fallback seams at 0, down from 20. This is still not full DAG
  source-of-truth, but it removes the old fallback helper API and prevents
  owner-local fallback seams from retuning.
- Verified locally: `make type-resolution-resolver-inventory-test-smoke
  type-resolution-dag-test-smoke` and `make test-semantic`.

## Progress Log - 2026-04-27 - DAG Fallback Classification Tightening

- Rechecked DAG closure against the current gates:
  `type-resolution-resolver-inventory-test-smoke` reports owner-local fallback
  seams at 0, while `type-resolution-dag-test-smoke` reports
  `metadata_entries=3498`, `metadata_hits=8380`,
  `materializer_fallbacks=0`.
- The central metadata materializer fallback inventory is now closed at 0.
  Missing-symbol diagnostics, generic-named fallback, and builtin
  constructor-shell/default fallback all stay on metadata-owned paths.
- Constructor-shell provenance, generic default specialization, and
  missing-symbol diagnostics are now expressed without entering the recursive
  materializer. A direct metadata-time reject for bare stable builtin shells was
  tested and rejected because it breaks generic default/multi-bound provenance
  such as `Box<Item>` validation paths; the final closure preserves that
  provenance while keeping central fallback at 0.
- Alias diagnostic inventory no longer calls back into the recursive resolver.
  `type-resolution-dag-test-smoke` gates `alias_diagnostic_resolver_calls==0`;
  the remaining 78 alias entries are repeated alias-cycle diagnostic inventory
  from semantic regression contexts.

## UTF-8 Progress Note - 2026-04-28 - Domain Helper Projection/Overlay Owner Split

- `src/semantic/type_checker_domain_projection.c` now owns projection contract
  diagnostics for domain and zone projection closure. This removes projection
  diagnostic body ownership from the domain helper shell without changing the
  diagnostic wording contract.
- `src/semantic/type_checker_overlay_common.c` now owns overlay
  symbol/shared-field/hosted-method scope setup. The domain helper shell keeps
  only the zone/effect/relation slot helper responsibility.
- `src/semantic/type_checker_decls_domain_helpers.c` is now 972 LOC. With the
  previous stdlib builtin, zone declaration, and intent helper splits, semantic
  production `.c` owners are below the 1,000 LOC hard cap.
- Verified locally: `make test-semantic pgy` remains green at 2357/0, and
  `make semantic-tu-size-test-smoke production-header-size-test-smoke
  inc-sentinel-test-smoke` remains green. The active 1,000+ production `.c`
  owner queue is now parser-only: `ast.c`, `ast_print.c`, and
  `parser_domain.c`.

## UTF-8 Progress Note - 2026-04-28 - Parser Domain Owner Split

- `src/parser/parser_domain_roster.c` now owns roster body parsing,
  `src/parser/parser_domain_world.c` owns world body parsing,
  `src/parser/parser_domain_zone.c` owns zone body parsing, and
  `src/parser/parser_domain_event.c` owns event signature parsing.
- `src/parser/parser_domain_intenal.h` exposes only the domain parser helper
  seam needed by those owners: identifier-keyword matching, child/slot append,
  domain slot parsing, projection sync parsing, and zone participant parsing.
- `src/parser/parser_domain.c` is now 970 LOC. It keeps relation/effect parsing,
  party/ability/role parsing, and the shared domain helper implementations.
- Verified locally: `make test-parser pgy` remains green. The active 1,000+
  production `.c` owner queue is now AST-only: `ast.c` and `ast_print.c`.

## Progress Log - 2026-04-30 C/LLVM Defer Cleanup Parity

- C backend `defer` no longer lowers through a file-scope GCC cleanup helper.
  That helper could not capture local method state such as `self`, which caused
  backend drift on `subject_method_recursion_defer`.
- `src/codegen/transpiler_defer_emit.h` now mirrors the LLVM lexical defer
  stack: block scopes register defer bodies, normal scope exit emits the current
  scope in LIFO order, `retun` emits active defers before leaving, and
  `break`/`continue` emit defers down to the target loop's defer base depth.
- C MIR emission now consumes `AST_DEFER_STMT` directly, opens a MIR function
  defer scope, and emits active defers on MIR retun/fallthrough retuns. This
  closes the previous gap where source-level C lowering was fixed but
  MIR-emitted subject methods still skipped deferred state mutation.
- MIR no longer classifies `AST_DEFER_STMT` as CFG-owned control, and DCE now
  preserves defer statements as side-effecting statements. This closes the
  nested branch defer loss where `if { defer { ... } }` silently disappeared
  from MIR.
- Dynamic `defer` inside runtime-dependent `if`/match/loop control is now an explicit
  beta reject (`PGY_SEM_DEFER_DYNAMIC_CONTROL`) instead of a shared C/LLVM
  wrong-code path. Static control forms remain allowed; dynamic forms must wait
  for a runtime defer stack model rather than pretending lexical lowering is
  sound.
- The transpiler unit test now asserts the new inline lexical cleanup contract
  and rejects the old `__attribute__((cleanup(_pgy_defer_...)))` sentinel path.
- Verified slice gates: `make test-transpile` (`682/0`), `make
  llvm-test-smoke`, `make llvm-test-backend-compare` (`69/69`), `make
  cfg-body-dataflow-test-smoke`, `make air-drift-test-smoke`, and `make
  type-resolution-dag-test-smoke`.
- CI status note: a monolithic `make ci-linux` run exceeded the local 15 minute
  command window, so it is not claimed as a completed full run. The equivalent
  CI target groups were run in slices: `test-all`, tooling/stdlib/module,
  docs/runtime/diagnostics/IR/example gates, AIR nonimpact, LLVM ABI/campaign,
  and backend compare all completed green locally.

## Progress Log - 2026-05-02 Parser Intent Append Capacity Closure

- Intent declaration parsing now tracks geometric capacities for involves,
  values, binding inventory, step inventory, and intent-level `who` defaults.
  This removes the beta-core parser `count+1 realloc` path for intent headers
  and body clauses.
- Intent step parsing now tracks geometric capacities for `who`,
  `authorized by`, `requires`, `on`, and `compensate` clause collections. The
  parser helper seam now takes explicit capacity pointers instead of hiding
  one-element growth behind `intent_append_node` / `parse_intent_name_list`.
- `tests/perf_contract_smoke.sh` now gates these capacity fields and rejects
  regression to count-only append in `parser_intent.c` and
  `parser_intent_step.c`.
- Verified slice probes: direct GCC compile probes for `parser_intent.c`,
  `parser_intent_step.c`, and `ast_domain_constructors.c`.

## Progress Log - 2026-05-02 Parser Domain Local Buffer Capacity Closure

- World composed-state input-name parsing now uses a local geometric capacity
  buffer instead of rebuilding the temporary array on every input.
- Zone grouped slot/layer name parsing now uses a local geometric capacity
  buffer instead of `malloc(count+1) + memcpy + free` for every name.
- `tests/perf_contract_smoke.sh` now rejects count-only append regressions in
  `parser_domain_world.c` and `parser_domain_zone.c`.
- Verified slice probes: direct GCC compile probes for
  `parser_domain_world.c` and `parser_domain_zone.c`.

## Progress Log - 2026-05-02 Parser Projection Buffer Capacity Closure

- Projection target-name parsing now uses a geometric capacity buffer instead of
  rebuilding the target array per refresh/publish/bind target.
- Projection field-map parsing now uses geometric capacity for paired
  target/source field arrays instead of per-entry `malloc(count+1) + memcpy`.
- `tests/perf_contract_smoke.sh` now rejects count-only append regression in
  `parser_domain_projection.c`.
- Verified slice probe: direct GCC compile probe for
  `parser_domain_projection.c`.

## Progress Log - 2026-05-02 Parser Match Capacity Closure

- Match statement case lists and match-case OR-patten lists now carry explicit
  capacities and grow geometrically during parsing.
- `tests/perf_contract_smoke.sh` now gates `patten_capacity` and rejects
  match case / patten regression to count-only append.
- Verified slice probes: direct GCC compile probes for `parser_stmt.c` and
  `ast_constructors.c`.

## Progress Log - 2026-05-02 Parser Enum Method Capacity Closure

- Enum method lists now carry explicit capacity and grow geometrically during
  parsing instead of reallocating to `method_count + 1` per method.
- `tests/perf_contract_smoke.sh` now rejects enum method append regression to
  count-only growth.
- Verified slice probe: direct GCC compile probe for `parser_enum.c`.

## Progress Log - 2026-05-02 Parser Lambda Parameter Capacity Closure

- Lambda parameter lists now carry explicit capacity and use the shared
  expression-list capacity helper. The old count-only `parser_append_expr_node`
  helper was removed from `parser_expr.c`.
- `tests/perf_contract_smoke.sh` now rejects expression append paths that bypass
  the capacity helper.
- Verified slice probes: direct GCC compile probes for `parser_expr.c` and
  `ast_domain_tail_constructors.c`.

## Progress Log - 2026-05-02 Parser Domain Shared Append Capacity Closure

- Shared domain parser append helpers now require explicit capacity pointers:
  `append_domain_slot` and `append_child_node` no longer own hidden `count+1`
  realloc growth.
- Domain-heavy AST payloads now carry capacity fields for party/roster/world,
  relation/effect/zone, role-slot ability lists, zone authority abilities, and
  ability/role/impl/event parser lists.
- Projection sync append now passes the destination refresh capacity through the
  domain parser helper seam, so relation/effect/zone projection entries use the
  same capacity contract.
- The parser `count+1` append scan for the tracked pattens is now empty.
  `tests/perf_contract_smoke.sh` rejects regression in the shared domain
  helpers.
- Verified slice probes: direct GCC compile probes for `parser_domain.c`,
  `parser_domain_relation_effect.c`, `parser_domain_zone.c`,
  `parser_domain_projection.c`, `parser_domain_world.c`,
  `parser_domain_roster.c`, and `parser_domain_event.c`.

## Progress Log - 2026-05-02 Parser Count+1 Append Scan Closure

- Structured comment tags now carry `tag_capacity` and grow geometrically in
  `parser_doc.c`.
- The tracked `count+1` append scan across `src/**/*.c` and `src/**/*.h` is now
  empty for the grep pattens used by the perf/debt audit.
- `tests/perf_contract_smoke.sh` now gates structured-comment tag capacity and
  rejects regression to `tag_count + 1`.
- Verified slice probe: direct GCC compile probe for `parser_doc.c`.

## Progress Log - 2026-05-02 LLVM MIR Inventory Diagnostic Routing

- Added `llvm_set_mir_inventory_missing(...)` as the central diagnostic helper
  for LLVM MIR inventory gaps. It always attaches
  `PGY_CODE_LLVM_MIR_ROUTINE_MISSING`,
  `PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING`, and
  `PGY_FIX_INSPECT_MIR_INVENTORY`.
- Converted the intent entry path and top-level/class method pipeline missing
  inventory errors, including the top-level executable wrapper path, from
  ad-hoc error routing to the central helper.
  These remain honest hard errors; the change closes diagnostic routing drift,
  not the underlying declaration inventory TODO.
- `tests/mir_declaration_inventory_smoke.sh` now gates the helper and rejects
  plain-error regression for these MIR-missing diagnostics.
- Verified slice probes: direct GCC compile probes for `llvm_error.c`,
  `llvm_intent.c`, and `llvm_pipeline.c` with `PGY_LLVM_ENABLED`.

## Progress - 2026-05-02 - LLVM MIR declaration metadata-first close

- LLVM nominal method forward registration now iterates `MIRDeclMethod`
  metadata arrays through `llvm_host_decl_method_metadata(...)` instead of
  walking `class_decl.methods[]` / `enum_decl.methods[]` AST arrays.
- LLVM class-method routine emission now consumes `MIRDeclMethod.has_routine`
  and `MIRDeclMethod.routine_index`. The old local method-routine fallback scan
  was removed so missing links fail through the shared MIR inventory diagnostic.
- `llvm_set_mir_inventory_missing(...)` remains the required path for
  declaration/routine inventory gaps. `mir_declaration_inventory_smoke.sh`
  now gates metadata-first registration and rejects AST method-array loops.
- The legacy `llvm_find_host_decl_methods_in_context(...)` / `llvm_host_decl_methods(...)`
  AST method-array seam is removed. `llvm_find_host_method_decl_in_context(...)`
  is now metadata-only and retuns the method AST only as a diagnostic/codegen
  anchor from `MIRDeclMethod`.
- C backend nominal host-method lookup now also checks `MIRDeclHeader.method_metadata`
  first. When a MIR header exists, `find_nominal_host_method_decl(...)` no
  longer falls back to AST method-array scans for missing methods.
- Zone projection sync and intent-step subject action lookup now delegate to
  the same C backend MIR-aware nominal host-method lookup seam instead of
  open-coding `class_decl.methods[]` scans.
- C hosted method emission now consumes `TranspilerHostedMethodView` routine
  metadata directly. The generic `transpiler_find_mir_method(...)` helper name is
  gone; the only remaining method lookup helper is explicitly scoped as
  `transpiler_find_role_impl_mir_method(...)` for the role-include copy seam.
- The public C backend `transpiler_decl_methods_local(...)` AST method-array
  seam is removed. The remaining AST method-list access is quarantined as a
  static fallback inside `transpiler_decl_host_lookup.c` for MIR-absent paths.
- C class/enum method emission now also chooses its method iteration source
  from `MIRDeclHeader.method_metadata` when MIR is present. AST method arrays
  remain only as the no-MIR fallback/emission anchor.
- Shared C domain method body emission (`emit_hosted_methods_from_mir_or_error_local`)
  now also chooses its active method source from `MIRDeclHeader.method_metadata`
  before falling back to AST method arrays.
- Domain forward declarations for party/roster/relation/effect/zone/world now
  also use `MIRDeclHeader.method_metadata` when MIR is present, with AST arrays
  left as the no-MIR fallback.
- The repeated metadata/fallback selection was folded into
  `TranspilerHostedMethodView`, so C backend hosted-method emitters share one
  policy instead of open-coding MIR header checks in each owner.
- `emit_hosted_methods_from_mir_or_error_local(...)` now accepts a
  `TranspilerHostedMethodView` directly, so the shared body-emission helper no
  longer exposes an AST method-array API.
- Local lightweight gate passed:
  `documentation_quality_smoke`, `perf_contract_smoke`, `source_utf8_smoke`,
  `test_inc_size_smoke`, `air_drift_smoke`,
  `type_resolution_resolver_inventory_smoke`,
  `mir_declaration_inventory_smoke`, and `beta_readiness_checklist_smoke`.

## Progress - 2026-05-02 - Debt Ledger Refresh: CFG/AIR/DAG/MIR Runtime Seams

Closed or narrowed in this slice:

- C/LLVM thread-pool requirement detection now shares one owner:
  `src/codegen/thread_pool_usage.c`. The C backend and LLVM pipeline no longer
  carry duplicate AST/MIR walkers for this feature. The helper prefers
  instruction-carried AST provenance; source-array traversal has been removed
  from this usage decision.
- MIR SSA use-edge collection now prefers instruction-carried provenance for
  `DEF`, `RETURN`, and branch operands. Source statement / source terminator
  arrays remain fallback context, not the first source of use facts.
- MIR value-summary collection now counts slot writes from `MIR_INST_DEF`
  anchors through `mir_instruction_slot_anchor(...)`; it no longer walks
  `block->source_statements` for the write summary.
- C backend pending-use materialization now finds local let declarations from
  block `MIR_INST_DEF.ast` provenance before using the function-wide fallback.
  It no longer scans `block->source_statements` directly.
- C backend source-order scheduling now consumes `MIRInstruction`
  `source_statement_index` metadata instead of walking `block->source_statements`.
  Codegen no longer directly consumes MIR block source statement arrays for
  thread-pool usage, intent-observability usage, pending-use materialization, or
  source-order scheduling.
- Runtime intent exit now uses the active-registry index helper for stable
  active-handle lookup instead of scanning all active entries. The remaining
  linear scans are either free-slot allocation or semantic conflict scans, not
  the stable exit lookup path.
- Hosted-method declaration views in both C and LLVM now reject silent AST
  fallback when `requires_mir_metadata` is set and MIR metadata was not
  available. This makes the remaining MIR declaration bootstrap debt fail
  visibly instead of silently drifting back to AST-carried inventory.

Remaining debt after this slice:

- CFG/MIR is closer to source-of-truth for body facts, but not fully closed.
  MIR lowering still carries HIR source arrays as construction input, and
  codegen still has declaration/routine AST compatibility seams, but C backend
  block-local usage/pending/order facts now consume MIR instruction provenance.
  Ownership/drop/zone/effect consumers are not yet all forced through CFG/MIR facts.
- AIR consumes first-class evidence inventory for the covered facts, but it is
  not yet the verifier for every abstraction boundary. Trace/observability ABI
  evidence now flows through a global `AIREvidenceNode`; remaining AIR closure is
  consumer coverage for deeper effect propagation drift and DAG/module evidence,
  not ad-hoc side checks.
- DAG fallback counters remain zero, and materializer fallback now diagnoses as
  retired compatibility debt. The remaining work is not a numeric fallback
  cleanup; it is removing the recursive-resolver compatibility seam from
  semantic judgement paths and proving intent/zone/generic/module owners all
  consume the DAG-facing APIs.
- MIR/LLVM declaration debt is narrowed but not closed. Hosted method metadata
  is metadata-first and no longer silently falls back when MIR metadata is
  required, but `MIRProgram` still carries AST-backed declaration payloads and
  LLVM can still report missing MIR routines for unmaterialized declaration
  paths.
- Runtime propagation still needs the full transitive frontier scheduler. The
  shared frontier policy and bounded world/zone/projection loops are stronger,
  but the scheduler is not yet the single runtime propagation source of truth.

Local verification for this debt refresh:

- Direct GCC compile probes passed for `thread_pool_usage.c`,
  `transpiler_thread_pool.c`, `llvm_pipeline.c`, `mir.c`, `transpiler.c`, and
  `pgy_runtime_lib.c`.
- Shell gates passed:
  `build_source_inventory_smoke.sh`, `cfg_body_dataflow_smoke.sh`,
  `runtime_intent_observability_contract_smoke.sh`,
  `mir_declaration_inventory_smoke.sh`,
  `type_resolution_resolver_inventory_smoke.sh`, `source_utf8_smoke.sh`, and
  `test_inc_size_smoke.sh`.
- `tests/type_resolution_dag_smoke.sh` was not run locally because it requires
  `SEMANTIC_TEST_BIN`. Full `make` regression was not run in this environment.

## Progress - 2026-05-02 - Thread Pool Usage Fact Tightening

- Shared C/LLVM thread-pool usage detection now treats `await` and
  `task-group` as direct runtime-thread-pool surfaces instead of relying on
  nested fallback traversal.
- MIR instruction scanning now checks `inst->ast`, `inst->expr0`, and
  `inst->expr1` only. Source-only MIR block arrays are no longer consulted for
  thread-pool usage decisions.
- `tests/parallel_core_contract_smoke.sh` now gates the shared owner and both
  backend consumers so C/LLVM cannot reintroduce duplicate thread-pool usage
  walkers.
- The structural AST walk for thread-pool surfaces now lives in
  `src/parser/ast_analysis.c` as `ast_uses_thread_pool_surface(...)`.
  `src/codegen/thread_pool_usage.c` is now an adapter over MIR instruction
  provenance.
- C and LLVM backend entry points no longer re-scan the synthetic
  `__pgy_top_level_exec` AST body. Top-level executable code must appear in the
  MIR routine inventory, so thread-pool detection now has one backend entry
  contract: iterate MIR routines and consume `pgy_mir_routine_uses_thread_pool`.
- Closed in this slice: the source-only block fallback was removed from
  `thread_pool_usage.c`. If a future construct needs the runtime thread pool,
  it must be materialized as MIR instruction-carried provenance.

## Progress - 2026-05-02 - Intent Zone-Authority Compression Slice

- Intent compression now covers the first authority slice: when an
  authority-sensitive step has exactly one `who` participant and that
  participant resolves unambiguously to the current zone's authority subject
  slot, semantic analysis derives `authorized by: <who>` instead of forcing
  duplicate syntax.
- The canonical owner remains the zone/resource layer. The intent step only
  records `derived_authorized_by_from_zone` provenance and then consumes the
  same authorized-by validation path as explicit syntax.
- Pure local-zone declarations still require explicit approval, so toy
  declarative steps keep the existing fix-oriented diagnostic. Derivation is
  limited to authority-sensitive surfaces such as secure helpers, transfers,
  causes/effects, and action-derived authority flows.
- Gate: `intent_compression_contract_smoke.sh` now checks the provenance flag,
  AST print wording, contract-summary wording, authority derivation owner, and
  semantic regression names.

## Progress - 2026-05-02 - AIR Authority Provenance Lift

- The zone-derived `authorized by` provenance now flows through
  `DIRIntentStep.authorized_by_derived_from_zone` into
  `AIRBoundaryNode.authority_from_zone`.
- AIR text/JSON dumps now expose `authority_from_zone`, and strict AIR
  provenance messages include `authority_provenance=zone-derived|explicit|none`
  so LSP/CI consumers can distinguish explicit approval from compressed
  zone-owner inference.
- AIR validation rejects impossible shapes where zone-derived authority is set
  on a non-authority boundary or outside zone/world boundaries.
- AIR cleanup evidence accounting was repaired in the same slice:
  `AIR_EVIDENCE_MIR_CLEANUP` now consumes MIR CFG cleanup successors first,
  while boundary-specific pin evidence stays under
  `AIR_EVIDENCE_MIR_PIN_CLEANUP`. Gate: `make test-air` (`62/0`).

## Progress - 2026-05-02 - DAG Intent Annotation Seam Shrink

- `type_checker_intent_participants.c` now reads intent participant type
  annotations through `semantic_type_resolution_lookup_annotation_or_unknown`,
  not the metadata-first materializer helper.
- `type_checker_intent_transfer.c` now uses the same annotation-only path for
  transfer source/target bindings and step `where` type reads.
- `type_checker_intent_action_contract.c` now uses annotation-only reads for
  inherited action parameter type matching.
- `type_checker_zone_decl_authority.c` now uses annotation-only reads for
  authority subject-slot type validation, closing the zone authority
  subject-slot seam from the active materializer allowlist.
- `type_checker_ability_decl.c` now uses annotation-only reads for abstract
  ability method signature validation. This removes one more signature-only
  owner from the materializer allowlist.
- `type_resolution_resolver_inventory_smoke.sh` now caps materializer helper
  owners at `18` instead of `25`. This is a stricter gate: seven semantic
  owners were removed from the materializer allowlist. `type_checker_projection_path.c`
  now consumes annotation facts for projection field-path type reads, so
  projection diagnostics no longer create type metadata as a side effect.
  `type_checker_ownership_destructure.c` also consumes annotation facts for
  destructuring ownership type reads, keeping that body-safety path read-only
  with respect to DAG metadata creation.
  `type_checker_intent_decl.c`
  remains on the allowlist for now because intent header binding symbols are
  installed before all annotation metadata is safe to consume in the current
  stage order.
- Rechecked the next obvious candidates and kept them on the allowlist:
  `type_checker_world_helpers.c`, `type_checker_func_action_contract.c`, and
  `type_checker_intent_role_fields.c` all still require the materializer seam.
  Converting them to annotation-only reads regresses semantic/DAG smoke because
  effect/action/compressed-intent checks can run before the relevant metadata is
  guaranteed to be materialized. The next DAG closure step is therefore a
  stage-order/materialization prepass fix, not another local helper rename.
- `type_checker_ability_where.c` no longer owns that materializer seam:
  effective ability generic actuals now materialize through
  `collect_effective_generic_arg_types(...)` in
  `type_checker_generic_effective_args.c`.
  Ability where validation consumes the resulting type evidence and otherwise
  stays annotation-only.
- DAG stage signature now installs generic parameter scope while staging class
  and ability signatures. This does not remove another allowlist owner by
  itself, but it closes a real stage-order gap: staged field/method signature
  materialization now sees the same generic parameter vocabulary that the full
  semantic checker later installs.
- Local gates: `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`, and `make test-semantic`.

## Progress - 2026-05-02 - Intent Single-Subject Who Inference Slice

- Intent compression now has its first fail-closed `who` inference rule:
  if an intent step omits `who`, no action/default already supplied it, and the
  enclosing intent has exactly one subject participant, semantic analysis
  derives `who` from that single subject participant.
- The rule deliberately does not infer across multiple subject participants.
  Ambiguous participant sets keep the existing explicit-`who` requirement and
  are covered by a negative semantic regression.
- Provenance is carried end-to-end:
  `ASTIntentStepData.derived_who_from_single_participant` flows through AST
  print/contract summary diagnostics, DIR, AIR, and `pgy.air.graph.v1` JSON as
  `who_from_single_participant`.
- This is not full Intent-Compress. Remaining compressed-default work still
  includes receiver/enclosing-subject `who`, broader `where`, requires/guard
  inference policy, explicit-vs-inferred conflict diagnostics, and example
  migration.
- Test owner cleanup: AIR evidence tests were split at function boundaries into
  `test_air_evidence_part_b.cases.h` and
  `test_air_observability_pin_part_g.cases.h`, restoring the test-case size
  gate without changing AIR behavior.
- Local gates: `make test-semantic` (`2430/0`), `make test-air` (`65/0`),
  `make intent-compression-contract-test-smoke`,
  `make air-json-schema-test-smoke`, `make test-inc-size-test-smoke`, and
  `make source-utf8-test-smoke`.

## Progress - 2026-05-02 - Intent On-Receiver Who Inference Slice

- Intent compression now has a second fail-closed `who` inference rule:
  if a step omits `who` and carries `on: receiver.Action(...)`, semantic
  analysis derives `who` from the receiver only when the receiver is an intent
  subject participant and that subject declares the referenced action.
- Multiple distinct matching receivers do not infer. The step remains explicit
  and existing participant/action diagnostics own the failure. This prevents
  compressed syntax from becoming an authority or effect owner.
- Provenance is carried end-to-end:
  `ASTIntentStepData.derived_who_from_on_receiver` flows through AST print,
  semantic contract summary, DIR, AIR, and `pgy.air.graph.v1` JSON as
  `who_from_on_receiver`.
- The owner is responsibility-named, not size-split:
  `type_checker_intent_on_inference.c` owns on-clause inference, while
  `type_checker_intent_action_contract.c` stays focused on inherited action
  contracts. This keeps all production owners below the 600 LOC review gate.
- This is still not full Intent-Compress. `where`, `using`, `requires`, and
  `authorized by` inference remain separate closure work with explicit
  conflict diagnostics.
- Local gates: `make test-semantic` (`2443/0`), `make test-air` (`65/0`),
  `make intent-compression-contract-test-smoke`,
  `make air-json-schema-test-smoke`, `make test-inc-size-test-smoke`,
  `make source-utf8-test-smoke`, and `make documentation-quality-test-smoke`.

## Progress - 2026-05-02 - Intent On-Receiver Where/Using Inference Slice

- `on: receiver.Action(...)` now supplies a narrow `where` derivation when the
  resolved receiver action declares `within <Zone>` and the step has no
  explicit/local zone. This works even when the step name differs from the
  action name, making the `on` clause a real evidence source instead of only a
  redundant execution clause.
- The existing unique-zone-binding rule then derives `using` from the inferred
  zone when the intent has exactly one binding of that zone type. No new public
  syntax or keyword was added.
- The rule remains fail-closed: conflicting `within` zones across multiple
  `on` calls do not infer, and explicit `where` continues to win.
- `type_checker_intent_on_inference.c` owns the receiver/action evidence path;
  shared subject-action lookup moved to `type_checker_intent_helpers.c` so the
  action-contract owner stays below the 600 LOC review threshold.
- This still does not infer `requires` or `authorized by` from arbitrary `on`
  clauses. `authorized by` remains owned by action/zone authority validation,
  not by intent compression.
- Local gate: `make test-semantic` (`2454/0`).

## Progress - 2026-05-02 - Intent On-Receiver Action Contract Slice

- A step with exactly one resolved `on: receiver.Action(...)` now inherits the
  action contract's `requires` and `causes` clauses when the step has not
  declared those clauses locally.
- `authorized by self` is mapped to the on-call receiver alias and recorded as
  `inherited_authorized_by_from_action`.
- The same evidence path now also maps `authorized by <action-param>` to the
  corresponding single `on` call argument when that argument is a declared
  intent participant identifier. Non-identifier arguments, missing parameter
  bindings, and multiple `on` calls remain explicit.
- Action-derived zone, ability, effect, and authority provenance now reaches DIR/AIR as
  `where_inherited_from_action` / `source_from_action` and
  `requires_inherited_from_action` / `requires_from_action`,
  `causes_inherited_from_action` / `causes_from_action`, and
  `authorized_by_inherited_from_action` / `authority_from_action`. AIR
  diagnostics report the authority side as `authority_provenance=action-inherited`
  instead of flattening it into `explicit`.
- Action-derived authority is now checked through RIR evidence in the parsed
  on-receiver AIR regression: `authority_from_action` must pair with matching
  `AIR_EVIDENCE_RIR_AUTHORITY` / `rir_authority_evidence_name` rather than
  remaining a boundary flag only.
- Multiple or conflicting `on` actions remain fail-closed. The implementation
  does not union ability requirements or merge effect clauses across actions.
- Shared ability-clone append logic moved to `type_checker_intent_helpers.c`,
  keeping `type_checker_intent_on_inference.c` focused on on-clause evidence
  and `type_checker_intent_action_contract.c` focused on step-name action
  inheritance.
- Local gates: `make test-semantic` (`2490/0`), `make test-air` (`66/0`),
  `make intent-compression-contract-test-smoke`,
  `make air-json-schema-test-smoke`, `make test-inc-size-test-smoke`,
  `git diff --check`, `make source-utf8-test-smoke`, and
  `make documentation-quality-test-smoke`.

## Progress Log - 2026-05-03 CFG Loop Snapshot Lifetime Closure

- `for` / `while` flow now restores merged resource state before destroying
  the loop scope. This prevents loop-local snapshot symbols from writing
  through freed scope storage during MIR/transpile lowering.
- `parallel` task flow now restores the entry ownership snapshot before
  destroying each task scope, keeping task-local symbols out of post-scope
  writes while preserving joined conflict analysis.
- Function signature metadata misses now fail closed to `TYPE_UNKNOWN` instead
  of reaching `type_create_function(...)` as `NULL`.
- C backend MIR block lookup now prefers exact `source_statement_index`
  metadata over block-AST name search for preserved let statements.
- C backend preserved-let emission is now named as preserved source-order
  emission (`transpiler_mir_preserved_let_emit.h`) rather than a fallback path,
  and `perf-contract-test-smoke` rejects reintroducing the old fallback-let
  owner/function names.
- LLVM intent success predicate emission now fails closed with
  `PGY_LLVM_TYPE_UNSUPPORTED` instead of silently lowering unsupported success
  expressions to `true`; `perf-contract-test-smoke` rejects the old lossy
  fallback wording.
- LLVM intent authority presence flags now fail closed: missing/null/non-pointer
  zone or participant aliases lower to `false` and flow through the shared
  runtime authority rejection path instead of being treated as present.
- LLVM function registry lookup/declaration now uses declaration terminology:
  `llvm_lookup_or_declare_function(...)` with `decl_type`/`decl_ret_type`.
  The old `lookup_or_create` and `fallback_type` names were false debt signals
  for explicit extenal/runtime declarations, and `perf-contract-test-smoke`
  rejects reintroducing them under `src/codegen`.
- Intent observability usage detection now names the remaining hand-built MIR
  compatibility path as a `legacy_ast_probe`, not an AST fallback. Production
  lowered MIR still consumes routine direct calls and inventory surface facts;
  `perf-contract-test-smoke` rejects the old `allow_ast_fallback` wording.
- Thread-pool surface usage detection uses the same `legacy_ast_probe`
  vocabulary for hand-built MIR fixtures, so codegen no longer advertises
  production AST fallback for surface usage detection.
- LLVM collection calls now fail closed when required List/Set/HashMap
  runtime exports are not registered, using shared backend diagnostics instead
  of treating missing runtime functions as empty `0`/`null` results. HashMap
  raw-export lookup was folded into the required-runtime helper so unsupported
  key metadata is also explicit.
- Queue extended calls now use the same required-runtime helper as List/HashMap
  calls, so missing queue exports no longer silently become empty/zero results.
- LLVM MIR for-in lowering now requires registered `pgy_list_size_raw_export`
  and `pgy_list_get_raw_export` functions via explicit backend diagnostics
  instead of silently lowering a missing export into an empty loop/body.
- LLVM statement-level for-in lowering now uses the same required-runtime
  policy and stops before emitting partial loop IR when the list size/get
  runtime exports are missing.
- LLVM log and intent-observability expression calls now report missing runtime
  exports as backend diagnostics instead of silently ignoring the call or
  redispatching to the generic unknown-function path.
- Link rules now use a shared response-file macro for object lists instead of
  raw `$^` expansion. This closes a Windows/MinGW command-line length failure
  where long object lists were truncated into non-existent paths during
  LLVM-enabled links; `perf-contract-test-smoke` now gates the response-file
  rule.
- AIR boundary evidence now rejects fact-count drift: each boundary evidence
  node must carry exactly one boundary fact. This prevents hand-built or
  JSON-fed evidence from widening HIR/RIR/MIR boundary proofs into ambiguous
  multi-fact claims.
- Local native MinGW gates: `test-semantic` (`2500/0`) and `test-transpile`
  (`710/0`), plus `test-air` (`68/0`). The POSIX Makefile path is still
  blocked locally by Git Bash `Win32 error 5`, so these were run through direct
  object rebuild/link.

## Progress Log - 2026-05-03 Priority Reset and MIR Fact Contract

- Strict beta implementation priority is now:
  CFG/MIR fact contracts, AIR evidence consumption, DAG source-of-truth seams,
  LLVM declaration inventory bootstrap, runtime frontier scheduler, then ABI
  ownership/runtime-none/raw-escape contracts.
- Dogfood/WebGL remains the first beta use path after those contracts are
  stable. Self-hosting remains beta+.
- MIR surface-usage facts are now validator-enforced for instructions that carry
  AST/expression/source payloads. `mir_validate(...)` rejects source payloads
  without `has_surface_usage_facts`, so codegen can consume
  `uses_thread_pool_surface` as a MIR contract for normal lowered MIR instead
  of silently depending on AST rescans.
- AIR evidence inventory now rejects stale legacy boundary summaries when an
  evidence inventory is present. A boundary with `has_hir_*` / `has_rir_*`
  summary flags must have the matching `AIREvidenceNode`; otherwise AIR
  validation fails before drift checking. For real HIR/RIR input, boundary
  evidence nodes must also have the matching summary flag, so the inventory and
  cached boundary summaries cannot drift in either direction. This keeps
  EvidenceNode as the abstraction-boundary source of truth instead of letting
  cached booleans overstate verification coverage.
- The remaining DAG unresolved path is now named as a metadata dead-end
  diagnostic, not a materializer fallback recorder. This is a source-level
  contract change only: the path still increments the same fallback counters if
  reached, but the public intenal seam no longer suggests that recursive
  materialization is an allowed implementation strategy. The trace hook is also
  named `PGY_TYPE_RES_DEAD_END_TRACE` / `[type-res-dead-end]`, keeping developer
  diagnostics aligned with that contract.
- C/LLVM hosted-method declaration views now call their AST-side arrays
  `ast_compat_methods` / `ast_compat_count`, not generic fallback methods. This
  does not remove the AST compatibility path yet, but it makes the remaining
  declaration bootstrap debt explicit and keeps MIR metadata as the named
  primary source. The views now also reject MIR metadata count drift against the
  AST compatibility count instead of silently truncating or extending hosted
  method iteration.
- C backend class/enum/generic hosted-method body emission now consumes the
  `MIRDeclMethod` name and linked `routine_index` through shared helper
  accessors before using AST-method compatibility lookup. This narrows the
  remaining declaration-side debt to body/type payload compatibility instead of
  routine identity discovery.
- C backend routine iteration now has a `TranspilerMIRRoutineInventory` view,
  mirroring the LLVM routine inventory helper. Thread-pool detection, intent
  observability detection, function/intent/method lookup, and view-resource hook
  scanning consume that view instead of directly walking `ctx->mir->routines`.
- DAG/AIR evidence naming now exposes `SemanticResult.type_resolution_metadata_dead_ends`
  as the source-of-truth counter. The older `materializer_fallbacks` field and
  stats label remain only for compatibility with existing smoke/stat parsers.
  `SemanticContext` also owns `type_resolution_metadata_dead_ends` directly now;
  the legacy `type_resolution_metadata_materializer_fallbacks` counter is synced
  from it instead of being the hidden source-of-truth.
  `PGY_TYPE_RES_STATS=1` prints `dead_ends` before the compatibility
  `materializer_fallbacks` mirror, and the DAG smoke now gates both counters
  against each other so future work cannot silently consume the old name as the
  primary source.
- CFG/MIR DEF use-edge collection now consumes the instruction-carried source
  statement (`inst->ast`) only. It no longer reopens block
  `source_statement_inventory` as a fallback for DEF initializer use facts.
- MIR BRANCH/RETURN instructions now carry `source_terminator_kind`, and the
  MIR validator rejects missing/mismatched HIR terminator provenance. This keeps
  CFG terminators as MIR facts instead of backend/source-array context.
- AIR now collects those validated terminator facts as
  `AIR_EVIDENCE_MIR_TERMINATOR` and exposes
  `mir_terminator_evidence_count` in `pgy.air.graph.v1`, making CFG terminator
  provenance part of the AIR evidence inventory rather than MIR-only state.
  Strict AIR also emits a global missing-evidence drift when real MIR input is
  present for boundaries but no MIR terminator evidence was attached.
- Local native MinGW gates: `test-mir` (`35/0`), `test-air` (`71/0`),
  `air-drift-test-smoke`, and `air-json-schema-test-smoke`; plus
  `test-semantic` (`2500/0`), and `test-transpile` (`710/0`).

## Progress Log - 2026-05-04 LLVM Runtime Helper Fail-Closed Sweep

- LLVM runtime declaration terminology now says `lookup_or_declare`, not
  `lookup_or_create` / fallback. This keeps declaration synthesis framed as an
  explicit registry operation rather than an allowed backend fallback.
- Stable LLVM collection, for-in, log, intent-observability, and stdlib
  runtime helper calls now fail closed when their required runtime declaration
  is missing. Missing helpers route through structured LLVM diagnostics with
  `PGY_FIX_INSPECT_MIR_INVENTORY` instead of falling through to generic unknown
  call handling or silently retuning `0`.
- LLVM checked integer division/modulo now also fails closed if the checked
  arithmetic runtime helper is missing. It no longer falls back to raw LLVM
  `sdiv`/`srem`, preserving the panic/runtime-parity contract for divide/mod
  by zero.
- LLVM string concatenation/comparison and numeric string coercion now use the
  same explicit runtime-helper contract. Missing `StringConcat`,
  `pgy_string_equals`, `pgy_int_to_string`, or `pgy_float_to_string` no longer
  falls through into pointer comparison/arithmetic or generic unknown-call
  behavior.
- LLVM `ArrayPush` / `ArraySet` now require their type-specialized
  `pgy_array_*_<suffix>` runtime exports. Missing array exports are backend
  diagnostics instead of silent no-op array mutations.
- LLVM indexed `Array<T>` / `Slice<T>` access now also requires the
  type-specialized `pgy_array_get_*` / `pgy_slice_get_*` runtime exports before
  using the checked collection accessor path. Missing accessors no longer fall
  through to raw aggregate data loads that bypass the runtime bounds-check
  contract.
- The final LLVM generic unknown-call path now reports a structured backend
  diagnostic (`import-or-declare-symbol`) instead of printing a waning and
  retuning `0`. User/extenal functions must be declared in the backend
  function registry before LLVM emission.
- LLVM host declaration metadata lookup now includes role, party, and roster
  declarations. This fixes the exposed role ability operator-overload path:
  `IntMath.Add` can resolve its MIR method routine through declaration
  metadata instead of tripping the MIR-only missing-routine diagnostic.
  The active-inventory compatibility path now mirrors the same host set, so
  party/role/roster owners without a MIR decl header do not regress to a null
  host declaration during LLVM method emission.
- LLVM channel builtin coverage now includes `ChannelClose`, routed through the
  same `Channel<T>` metadata and required-runtime helper path as channel
  readiness/pressure queries. This removes another generic unknown-call escape
  that was only visible once waning+0 fallback was disabled.
- LLVM `Rc<T>` / `Weak<T>` builtins now require their type-specialized runtime
  exports through the shared required-runtime helper. Missing Rc/Weak exports
  are backend diagnostics instead of quiet empty-handle/no-op behavior.
- LLVM `DeviceSlot<T>` read/write/release/submit operations now require their
  type-specialized device runtime exports through the same required-runtime
  helper, so device resource calls cannot silently degrade to `0`.
- LLVM standalone `SecureSlot<T>` read/write/release now fail closed for
  runtime-backed builtin payloads when the secure runtime export is missing.
  User-defined payloads (`SecureSlot<Subject>`, `SecureSlot<Class>`) use the
  module-local structural secure-slot lowering path because those helpers are
  generated per payload rather than provided by the shared runtime registry.
- LLVM method-call style `secureSlot.Write/Read/Release` follows the same
  split: builtin secure payloads require registered runtime exports, while
  user-defined payloads use explicit structural lowering instead of an
  unlabelled direct fallback.
- LLVM `with slot<T>` cleanup and typed slot initializers now use the same
  builtin-vs-custom split. Runtime-backed builtin payloads fail closed if their
  release/write helpers are absent; user-defined payloads use structural
  slot-field lowering instead of silently skipping cleanup or initialization.
- LLVM block-scope slot auto-release now also fails closed for runtime-backed
  builtin payloads when the required release helper is missing. Custom payloads
  keep structural cleanup because those helpers are not shared runtime exports.
- LLVM slot auto-read and assignment-to-slot now use the same helper contract.
  The assignment path also now chooses `pgy_secure_write_*` for secure slots
  instead of probing the plain `pgy_write_*` name first.
- LLVM indexed array assignment now lowers through the checked
  `pgy_array_set_*` runtime helper instead of raw aggregate data-pointer stores.
  Missing element metadata or missing set helpers are backend diagnostics.
- C and LLVM `ArrayPop` now lower through the shared `pgy_array_pop_*` runtime
  export instead of directly mutating the aggregate length field in backend
  emitters. This keeps mutable collection operations on the runtime ABI
  contract path.
- LLVM array-literal and channel initializers now fail closed if
  `pgy_array_new_*`, `pgy_array_push_*`, or `pgy_channel_init_*` is missing.
  The backend no longer declares usable collection variables after skipped
  runtime initialization.
- LLVM expression-level array literals and channel send/receive now also
  require their runtime helper declarations. Missing `pgy_array_push_*`,
  `pgy_channel_send_*`, or `pgy_channel_recv_val_*` is a backend diagnostic
  rather than a silent empty-array/false/zero fallback.
- LLVM `MapKeys` now stops at the required runtime-helper diagnostic instead
  of retuning a null-initialized key array after the helper lookup fails.
- LLVM `ListGet`, `QueuePop`, and `MapGet` now stop at their required
  collection-helper diagnostics instead of retuning a null-initialized temp
  value after the helper lookup fails.
- LLVM event subscribe/unsubscribe/invoke expression lowering now requires the
  generated event function and event storage to be present. Missing event
  inventory is reported as a backend diagnostic instead of a no-op event call.
- LLVM checked `Result` / `Option` unwrap now requires the runtime panic export
  before building the panic branch. Missing panic ABI is reported during backend
  emission instead of silently lowering to a bare `unreachable`.
- LLVM `parallel` blocks now fail closed when `pgy_spawn_export` or
  `pgy_await_export` is absent. The previous sequential fallback hid runtime
  ABI drift by changing concurrency semantics during LLVM lowering.
- LLVM async blocks now fail closed when `pgy_async_spawn_export` or
  `pgy_async_detach_export` is absent. The previous synchronous fallback also
  hid runtime ABI drift by changing scheduling semantics during LLVM lowering.
- LLVM spawn expressions now report missing spawn/malloc/free runtime helpers
  and missing spawn targets as structured diagnostics instead of retuning a
  null task handle.
- LLVM await expressions now require `pgy_await_export` instead of lowering a
  missing await runtime helper to integer zero.
- LLVM top-level pipeline emission now fails closed for missing thread-pool
  init/shutdown exports and missing event initialization inventory. These were
  previously no-op probes that could hide runtime ABI drift at program entry.
- LLVM plain `Slot<T>` builtin/method `Write/Read/Release` now follows the
  same builtin-vs-custom split as secure slots: primitive payloads require
  registered runtime helpers, while user-defined payloads may use structural
  slot-field lowering.
- LLVM secure slot read/write/release paths now share a paired-token
  diagnostic helper. Missing `<slot>_token` bindings report a structured
  backend error instead of retuning `0` from the emission path.
- LLVM secure slot statement cleanup paths now report paired-token diagnostics
  too, so block auto-release and `with SecureSlot<T>` cleanup cannot silently
  skip token-aware release.
- LLVM select/channel lowering now fails closed when `try_recv`, `ready`, or
  `recv_val` helpers are missing. The MIR select readiness path reports the
  same runtime-helper diagnostic instead of retuning `NULL` and letting the
  statement fallback emit an unconditional case body.
- LLVM MIR pin enter/exit now reports structured runtime-helper diagnostics
  (`pgy_pin_*`, `pgy_secure_pin_*`, `pgy_unpin_*`) instead of generic backend
  strings when the pin runtime surface is absent.
- The stdlib scalar/string/file/time LLVM path now consumes
  `llvm_required_runtime_function(...)` for registered runtime helpers. The
  generic unknown-function path remains unchanged for user/extenal calls; the
  fail-closed sweep is intentionally limited to frozen builtin/runtime surface.
- Repeated stdlib scalar/string/file required-runtime calls now funnel through
  `llvm_emit_required_runtime_call_result(...)`, reducing ad-hoc tenary
  fallback sites and keeping new stdlib helpers on the same diagnostic contract
  by default.
- MinGW LLVM-enabled links now use response files for large object inventories,
  avoiding command-line truncation in the current Windows build shape.
- Verified slice probes: LLVM object rebuilds for the touched codegen owners,
  native MinGW `test-parser`, native MinGW `test-semantic` (`2500/0`), and
  `test-transpile` (`717/0`), `llvm-test-smoke`, and
  `perf-contract-test-smoke`.
- Review follow-up gate: `llvm_find_host_decl_in_active_inventory(...)` now
  searches party, role, and roster declarations in the active inventory
  fallback path. `perf-contract-test-smoke` and
  `mir-declaration-inventory-test-smoke` both gate the mirrored host set.
  Local MinGW verification: `test-transpile` (`717/0`), `test-mir` (`41/0`),
  `test-air` (`75/0`), `llvm-test-smoke`, `perf-contract-test-smoke`,
  `mir-declaration-inventory-test-smoke`, and `git diff --check` (line-ending
  wanings only).
- AIR owner cleanup continued: `air_names.c` now owns AIR diagnostic
  formatting and owned-name allocation, while `air_validate_global_evidence.c`
  owns DAG/MIR/RIR propagation/observability global evidence validation.
  `air.c` is back to synthesis/evidence append, and boundary evidence
  validation no longer carries the global evidence matrix inline. Local MinGW
  verification: `test-air` (`76/0`).
- Runtime owner cleanup continued: party dispatch moved from implementation
  header `party_runtime_dispatch.h` into `party_runtime_dispatch.c`, with
  `party_runtime_intenal.h` making the small shared runtime helpers explicit.
  Public `DispatchParallel(...)` ABI is unchanged; the runtime no longer keeps
  the parallel dispatcher body in a header. Local MinGW verification:
  `bin/pgy.exe`, `build-source-inventory-test-smoke`, `source-utf8-test-smoke`,
  and `production-header-size-test-smoke`.
- Parser expression owner cleanup continued: lambda lookahead/body parsing now
  lives in `parser_expr_lambda.c`, and shared expression-list growth lives in
  `parser_expr_util.c`. This keeps `parser_expr.c` focused on precedence,
  postfix, and primary-expression orchestration while preserving the existing
  parser intenal API. Local MinGW verification: `test-parser`.
- LLVM MIR CFG control cleanup continued: range/for-in loop initialization,
  condition, and backedge increment lowering moved into
  `llvm_mir_loop_control.c`. `llvm_mir_cfg_control.c` now keeps match/select
  CFG condition lowering instead of carrying all loop control helpers inline.
  Local MinGW verification: `llvm-test-smoke`.
- C backend intent owner cleanup continued: intent participant type
  classification moved from `transpiler_block_intent_helpers.h` into
  `transpiler_intent_participant.c`, and pointer-self host classification now
  lives in `transpiler_host_self_policy.c` instead of a static implementation
  header. This reduces intent block helper static surface and gives subject /
  vessel / relation / effect / zone / world self-cell policy a single compiled
  owner. Local MinGW verification: `test-transpile` (`717/0`) and
  `build-source-inventory-test-smoke`.
- C backend MIR inventory view cleanup continued:
  `transpiler_inventory_view.h` is now prototype-only, with active MIR routine
  / declaration / exten / top-level-exec queries implemented by
  `transpiler_inventory_view.c`. This removes another implementation header
  from the global `transpiler.h` include surface while keeping the MIR-only
  active-inventory contract unchanged. Local MinGW verification:
  `test-transpile` (`717/0`) and `build-source-inventory-test-smoke`.
- C backend MIR intent metadata cleanup continued:
  `transpiler_mir_inventory_intent_collect.h` is now prototype-only, with
  who/authorized/participant/dispatch metadata collectors implemented by
  `transpiler_mir_inventory_intent_collect.c`. This keeps intent emission
  consuming MIR carrier facts through a compiled owner instead of another
  static collector header. Local MinGW verification: `test-transpile` (`717/0`)
  and `build-source-inventory-test-smoke`.
- C backend type-render state cleanup started: direct `g_type_render_ctx`
  reads/writes outside the type-render seam were replaced with
  `transpiler_type_render_ctx_current/bind/push/restore(...)` and
  `render_type_name_in_ctx(...)`. The global render context still exists, but
  all direct access is now centralized in `transpiler_type_render_helpers.h`,
  which is the next prerequisite for moving the state out of implementation
  headers entirely. Local verification: MinGW `gcc -fsyntax-only
  src/codegen/transpiler.c`; full `test-transpile` was blocked by local
  Git Bash/WSL permission failures, not by compiler diagnostics.
- C backend MIR intent metadata cleanup continued again:
  function/intent MIR routine lookup plus step-name/check/eval/meta-arg
  readers moved out of `transpiler_mir_inventory_intent.h` into the compiled
  `transpiler_mir_inventory_intent_collect.c` owner. The remaining
  `transpiler_mir_inventory_intent.h` surface is now forward-declaration
  emission only, reducing another implementation-header cluster. Local
  verification: MinGW `gcc -fsyntax-only src/codegen/transpiler.c` and
  `gcc -fsyntax-only src/codegen/transpiler_mir_inventory_intent_collect.c`.
- C backend MIR CFG policy cleanup continued: CFG-container classification,
  for-in collection length-field policy, incoming for-in branch lookup, and
  loop-branch lookup moved out of `transpiler_mir_cfg_control_emit.h` into
  `transpiler_mir_cfg_policy.c`. The remaining CFG control header now keeps
  expression-emitting loop condition/body/backedge code only. Local
  verification: MinGW `gcc -fsyntax-only src/codegen/transpiler.c` and
  `gcc -fsyntax-only src/codegen/transpiler_mir_cfg_policy.c`.
- LLVM declaration type-failure propagation tightened again: domain event
  parameter types, domain method/ability/role forward-declaration parameter
  types, and domain struct field types now retun `NULL` after required-type
  diagnostics instead of continuing with `i32` placeholder layouts. The
  affected builders now stop before `LLVMFunctionType(...)` /
  `LLVMStructSetBody(...)` when `ctx->has_error` or the lowered type is
  missing. Local MinGW verification: `perf-contract-test-smoke`,
  `llvm-test-smoke`, and `llvm-test-backend-compare` (`196/196` ABI pipeline,
  `69/69` backend compare).
- LLVM MIR-only domain body cleanup continued: domain method and role method
  body emitters no longer build a partial AST fallback function prelude before
  reporting missing MIR routine inventory. If no MIR routine exists, the LLVM
  path now reports the MIR-only inventory failure immediately and leaves
  function body ownership with MIR. Local MinGW verification:
  `llvm-test-smoke`.
- LLVM domain layout registration tightened: roster party slots and world
  roster/zone slots now require registered class metadata instead of silently
  materializing the field as `i32` when the referenced declaration is missing.
  This keeps domain ABI layout tied to declaration inventory rather than
  placeholder field shapes. Local MinGW verification:
  `perf-contract-test-smoke`, `bin/pgy.exe`, and `llvm-test-smoke`.
- LLVM callable/lambda signature lowering tightened: event-handler callable
  signatures and lambda function signatures now stop when retun/parameter
  type lowering fails instead of building `LLVMFunctionType(...)` with partial
  placeholder types. Callable variable calls and callable arguments also fail
  when their signature cannot be lowered. General LLVM let-bindings now also
  stop when annotated or inferred storage type lowering reports an error,
  rather than allocating with a stale placeholder type. Local MinGW
  verification: `bin/pgy.exe`, `llvm-test-smoke`, and
  `perf-contract-test-smoke`.
- LLVM callable local-fallback signature lowering tightened: the remaining
  current-function event-handler parameter path in call dispatch now stops
  when retun/parameter type metadata cannot be lowered, and non-event
  callable parameter declarations stop before using an invalid declared
  pointer type. Local MinGW verification: `bin/pgy.exe`,
  `perf-contract-test-smoke`, and `llvm-test-smoke`.
- LLVM intent ABI signature lowering tightened: intent entry bindings and
  forward declarations now stop when participant/value type lowering fails,
  and call-dispatch no longer rewrites missing intent forward parameter types
  to `i8ptr`. This keeps intent call ABI shapes tied to concrete type metadata
  instead of pointer fallback recovery. Local MinGW verification:
  `bin/pgy.exe`, `perf-contract-test-smoke`, and `llvm-test-smoke`.
- LLVM AST type-node lowering tightened intenally: event-handler type
  lowering and tuple type lowering now stop when recursive retun/parameter/
  field type lowering fails, and type-name render failures retun `NULL`
  instead of an `i32` placeholder. The final unsupported-AST fallback is still
  intentionally left until the remaining wide callers are null-safe. Local
  MinGW verification: `bin/pgy.exe`, `perf-contract-test-smoke`, and
  `llvm-test-smoke`.
- LLVM collection type-map fallback narrowed: `List<T>`, `Set<T>`,
  `Queue<T>`, and `HashMap<K,V>` missing generic metadata now retuns `NULL`
  instead of an `i32` container shape, and collection let-lowering stops before
  `alloca` / `sizeof` when container or payload type lowering fails. Local
  MinGW verification: `bin/pgy.exe`, `perf-contract-test-smoke`, and
  `llvm-test-smoke`.
- LLVM slot/array/slice type-map fallback narrowed: `Slot<T>`,
  `SecureSlot<T>`, `DeviceSlot<T>`, `Array<T>`, and `Slice<T>` missing generic
  metadata now retuns `NULL` instead of an `i32` aggregate shape, and direct
  array/slot let-lowering callers stop before `alloca` when the aggregate type
  cannot be built. Local MinGW verification: `bin/pgy.exe`,
  `perf-contract-test-smoke`, and `llvm-test-smoke`.
- LLVM Result/Option and MIR type helper fallback narrowed: `Option<T>` inner
  type failures, `Result<T>` payload/layout failures, and
  `llvm_mir_type_from_ast(...)` failures now retun `NULL` instead of
  producing an `i32` placeholder. MIR local alloca preparation now stops when
  the lowered type is missing. Local MinGW verification: `bin/pgy.exe`,
  `perf-contract-test-smoke`, and `llvm-test-smoke`.
- LLVM `pergyra_type_to_llvm(...)` caller guards widened: MIR channel receive,
  MIR for-in body binding, AST for-in lowering, select bound receive,
  Rc payload load/coercion, direct slot/secure-slot reads, await payload
  loading, and intent-zone participant value loads now stop before LLVM
  alloca/load/bitcast when type lowering fails. This prepares the remaining
  final unknown-type fallback for removal. Local MinGW verification:
  `bin/pgy.exe`, `perf-contract-test-smoke`, and `llvm-test-smoke`.
- Removed the final LLVM type-map silent `i32` fallback: unsupported AST type
  nodes and unknown nominal type names now retun `NULL` after structured
  diagnostics instead of manufacturing an `i32` type. Intentional `i32`
  mappings remain only for actual enum/int32 ABI representation. Local MinGW
  verification: `bin/pgy.exe`, `perf-contract-test-smoke`,
  `llvm-test-smoke`, and `llvm-test-backend-compare` (`196/196` ABI
  same-process precheck, `69/69` backend compare).
- Tightened LLVM collection/queue builtin error recovery: receiver/value/type
  diagnostics now leave the expression result `NULL` instead of writing the
  supplied integer/null recovery value through `out`. Successful void-like
  operations still retun their intentional ABI placeholder value. Local MinGW
  verification: `bin/pgy.exe`, `perf-contract-test-smoke`, and
  `llvm-test-smoke`.
- Tightened LLVM scalar math builtin error recovery: `Abs` / `Min` / `Max`
  operand-lowering failures now leave the expression result `NULL` after a
  structured diagnostic instead of retuning `i32 0`. Local MinGW verification:
  `bin/pgy.exe`, `perf-contract-test-smoke`, and `llvm-test-smoke`.
- Tightened LLVM Array builtin failure recovery: `ArrayLength`, `ArrayPush`,
  `ArraySet`, and `ArrayPop` failure paths now leave the expression result
  `NULL` when receiver, element metadata, operand lowering, or runtime export
  lookup fails. Successful mutation/pop calls still retun their intentional
  void-like `i32 0` placeholder. Local MinGW verification: `bin/pgy.exe`,
  `perf-contract-test-smoke`, and `llvm-test-smoke`.
- Tightened LLVM intent observability builtin failure recovery: missing
  runtime exports and argument-lowering failures now leave the expression
  result `NULL` after diagnostics instead of retuning `i32 0`. Successful
  observability calls still retun the runtime result. Local MinGW
  verification: `bin/pgy.exe`, `perf-contract-test-smoke`, and
  `llvm-test-smoke`.
- Tightened LLVM Slot/SecureSlot/DeviceSlot builtin failure recovery:
  argc/receiver/token/runtime-export/value-lowering failures now leave the
  expression result `NULL` after diagnostics instead of retuning `i32 0`.
  Successful write/release-style operations still retun their intentional
  void-like placeholder. Local MinGW verification: `bin/pgy.exe`,
  `perf-contract-test-smoke`, and `llvm-test-smoke`.
- Tightened LLVM Rc/Weak builtin failure recovery: the call owner no longer
  pre-fills `out` with `i32 0`, so arity, metadata, binding, suffix, and
  runtime-export failures remain `NULL` after diagnostics. Successful
  `RcDrop` / `WeakDrop` still retun the intentional void-like placeholder.
  Local MinGW verification: `bin/pgy.exe`, `perf-contract-test-smoke`, and
  `llvm-test-smoke`.
- Tightened LLVM base collection builtin failure recovery: `ListNew` /
  `SetNew` contextual type failures, Set operation element-metadata failures,
  and missing Set runtime exports now leave the expression result `NULL`.
  Successful mutating Set calls still retun their intentional void-like
  placeholder. Local MinGW verification: `bin/pgy.exe`,
  `perf-contract-test-smoke`, and `llvm-test-smoke`.
- Tightened LLVM extended collection/queue runtime-export failure recovery:
  `ListGet` / `ListSize`, `MapGet` / `MapHas` / `MapRemove` / `MapSize` /
  `MapKeys`, and `QueuePop` / `QueueSize` / `QueueEmpty` no longer synthesize
  null/default result values when required runtime exports are missing.
  Local MinGW verification: `bin/pgy.exe`, `perf-contract-test-smoke`, and
  `llvm-test-smoke`.
- Tightened LLVM collection mutation export failure recovery: `ListNew` /
  `SetNew`, `SetAdd` / `SetRemove`, `ListPush` / `ListSet` / `ListRemove`,
  `MapSet`, and `QueuePush` now stop with `NULL` result when required runtime
  exports are absent instead of reporting success with a void-like placeholder.
  Local MinGW verification: `bin/pgy.exe`, `perf-contract-test-smoke`, and
  `llvm-test-smoke`.
- Hardened HIR CFG append capacity growth: predecessor, dominance-frontier,
  dom-tree child, and CFG name arrays now use an overflow-checked shared
  capacity helper before `realloc`. This keeps CFG input construction aligned
  with the beta goal that body safety facts come from trustworthy HIR/MIR CFG
  inventories, not unchecked helper growth.
- Hardened HIR CFG lowering capacity growth and removed duplicate intent CFG
  append helpers: function-body and intent CFG lowerers now share the same
  block/statement append owner, with overflow-checked capacity growth in
  `hir_lower_cfg_blocks.c`. This narrows the CFG construction seam before
  ownership/cleanup/effect consumers rely on the same facts.
- Hardened HIR analysis name collection: signature and direct-call reference
  collection now checks capacity growth before `realloc`, so DAG/CFG
  dependency facts do not rely on unchecked name-array expansion.
- Hardened HIR routine/declaration inventory growth: `HIRDecl` and
  `HIRRoutine` arrays now use overflow-checked capacity growth before
  `realloc`, closing another unchecked seam in the HIR source-of-truth path.
- Narrowed MIR cleanup/intent append duplication: cleanup and intent MIR owners
  now route instruction/block appends through the shared MIR base append
  helpers, and cleanup predecessor-list growth has an explicit overflow guard.
  This keeps cleanup/pin/invalidation CFG edges on the same inventory-growth
  contract as normal MIR blocks.
- Hardened MIR liveness/value-summary growth: DCE name sets and routine
  value-summary inventories now check capacity expansion before `realloc`,
  closing the remaining obvious MIR analysis inventory growth seam.
- Hardened AIR inventory growth: owned-name, evidence-node, and drift
  inventories now share an overflow-checked `air_next_capacity(...)` helper.
  This keeps AIR's verification-layer inventories on the same allocation
  contract as HIR/MIR facts.
