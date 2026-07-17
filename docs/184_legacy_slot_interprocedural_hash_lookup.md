# Legacy Slot Interprocedural Hash Lookup

Status: `implemented-rung-3; symbol-sparsification-landed`

Date: 2026-07-15

Related documents:

- `docs/103_cfg_body_dataflow_need.md`
- `docs/125_source_of_truth_spine.md`
- `docs/127_compiler_speed_engineering.md`
- `docs/180_compiler_logical_spine_handles_gates.md`

## 1. Objective Card

| Field | Decision |
| --- | --- |
| Objective | Stop the legacy slot analyzer from rescanning the program root and recursively reopening callee bodies for every named parameter flow query. |
| Priority | Diagnostic byte preservation, one source of truth, self-host latency, then patch size. |
| Fact owner | `SemanticContext::host_decl_index` owns declaration lookup. `FunctionParamFlowSummaryStore` owns `(FunctionId, ParamIndex)` access/escape summaries. |
| Last legitimate consumer | Parameter contract checking plus legacy access/escape provenance in `slot_analyzer.c`, `slot_analyzer_access.c`, and `slot_analyzer_escape.c`. |
| Forbidden fallback | A compiler-owned path must not rescan `AST_PROGRAM`, reopen a callee body transitively, or truncate recursion at a depth limit. |
| Verification | Host-index and function-param-summary gates, semantic `2799/2799`, diagnostic gates, and the fixed self-host `--hir` measurement below. |

This is a consumer migration inside the existing semantic symbol/type fact
family. It does not create a second top-level owner row in the SoT registry.

## 2. Reproduced Cause

The fixed input is:

```powershell
bin\pgy.exe --hir src/self_hosted/codegen/main.pgy
```

The merged source produced 27,325 printed AST lines, 1,226 top-level functions,
and 61 `ref` parameters. Before this change, the process stayed in semantic
analysis for more than 100 seconds and reached about 441 MB working set.
Debugger samples repeatedly showed both call paths below:

```text
semantic_check_param_summary_escapes
  -> semantic_callable_param_escape_summary
  -> slot_analyze_legacy_ast_param_summary_in_program
  -> slot_param_summary_in_program
  -> slot_access_mask_for_named_symbol
  -> slot_analyzer_find_function_decl
  -> linear AST_PROGRAM scan

semantic_run_legacy_slot_resource_analysis
  -> slot_analyze_program
  -> collect_slot_escapes
  -> slot_param_summary_in_program
  -> slot_analyzer_find_function_decl
  -> linear AST_PROGRAM scan
```

The recursive summary walk can reopen callee bodies to depth six. The previous
lookup therefore multiplied recursive AST work by a top-level declaration
scan. The compiler already built an open-addressed hash index with first-match
semantics, but the slot analyzer ignored it.

## 3. Implemented Rung: One Hash Owner

`SlotFunctionLookup` carries the existing `SemanticContext` and the program
root. Compiler-owned entry points always supply the context. In that lane,
`slot_analyzer_find_function_decl(...)` calls
`semantic_host_index_find_decl_by_name(...)` directly and returns its answer.
There is no `new ? old` AST fallback after an indexed miss.

The program-root scan remains only for the public standalone compatibility API
that has no semantic context. This boundary is explicit: `{NULL, program_root}`.
It is not reachable from the normal compiler semantic pipeline.

The hash owner preserves the old first-declaration match rule. If allocating
the hash table fails, the owner may scan its own compact parallel arrays. That
is an implementation fallback inside one owner, not a second AST authority.

## 4. Implemented Rung: Demanded Recursive Summary Owner

`FunctionParamFlowSummaryStore` is scoped to one `SemanticContext` and keyed by
the parser-assigned stable function identity plus parameter index. Each entry
owns the six monotone may-facts used by the compatibility analyzer:

```text
read | write | release | return_escape | call_escape | channel_escape
```

The owner computes a summary only when a consumer demands it. Acyclic callees
complete bottom-up. A demand that reaches a `COMPUTING` entry marks the active
component recursive and returns the current conservative approximation. The
component is then revisited until no may-bit grows. There is no diagnostic-
affecting depth cutoff; allocation, missing stable identity, or non-convergence
emits a structured semantic failure and returns the all-bits conservative
summary. A fixed work budget (`4096` body evaluations per semantic context)
also bounds adversarial recursive demand; exhausting it emits the same
fail-closed diagnostic and returns the all-bits conservative summary rather
than silently accepting an under-approximated fact.

Both access propagation and escape propagation now consume this owner. The
standalone no-context compatibility API may still summarize its one explicitly
provided body, but the compiler pipeline cannot use it for transitive callee
analysis. `function-param-flow-summary-test-smoke` rejects that old read path
and runs a recursive fixture where the summary can grow only on a second pass.

## 5. Measurement

On the same Windows/mingw checkout and source snapshot:

| Build | Wall time to semantic completion | Result |
| --- | ---: | --- |
| Before hash consumer migration | `>100 s` | Did not complete during the observation window. |
| After hash consumer migration | `25.794 s` | Semantic analysis completed on the then-current source snapshot. |
| After demanded summary migration | `1.702 s` | Current source completed semantic analysis and HIR with 0 diagnostics. |

The last run reported `64` summary entries, `64` body evaluations, `367` cache
hits, and `15` recursion hits. This is a checkout-local observation, not a
cross-machine speed claim; the source snapshot also evolved between recorded
rungs. The permanent proof is therefore the bounded recursive fixture and
diagnostic gates, not the ratio alone. The semantic regression suite remains
`2799 passed, 0 failed`.

## 6. Research Basis And Exact Adoption Boundary

The implementation uses the papers as invariant references, not as claims that
Pergyra reproduces their full algorithms or reported speedups.

| Reference | Relevant result | Pergyra adoption |
| --- | --- | --- |
| Schiebel et al., *Scaling Interprocedural Static Data-Flow Analysis to Large C/C++ Applications: An Experience Report*, ECOOP 2024, [DOI](https://doi.org/10.4230/LIPIcs.ECOOP.2024.36) | Engineering the interprocedural implementation and summary machinery produced substantial time and memory reductions at large scale. | Adopted now: make declaration lookup an indexed owner seam and measure a fixed large input. Not claimed: their reported speedup transfers to Pergyra. |
| Stein et al., *Interactive Abstract Interpretation with Demanded Summarization*, ACM TOPLAS 2024, [DOI](https://doi.org/10.1145/3648441) | Compute procedure summaries on demand, track dependencies, and use a recursion fixed point while preserving from-scratch consistency. | Adopted now at the narrow owner boundary: demanded stable-key summaries, explicit computing/complete states, and monotone recursion convergence. Not claimed: the paper's interactive incremental framework. |
| Karakaya and Bodden, *Symbol-Specific Sparsification of Interprocedural Distributive Environment Problems*, ICSE 2024, [DOI](https://doi.org/10.1145/3597503.3639092) | Restrict propagation to symbol-relevant program points to reduce analysis time and memory. | Adopted at the summary owner boundary: binding-aware, function-local parameter rows select relevant source statements before the existing access/escape transfer. Not claimed: their full distributive-environment implementation. |
| Yang et al., *Taming and Dissecting Recursions Through Interprocedural Weak Topological Ordering*, ECOOP 2025, [DOI](https://doi.org/10.4230/LIPIcs.ECOOP.2025.34) | Build targeted interprocedural weak topological orders at recursion boundaries instead of imposing an expensive imprecise whole-program order. | Adopted only as a scheduling invariant: revisit the demanded recursive closure, not the whole program. Pergyra does not claim to implement RecTopo or an interprocedural weak topological order. |

## 7. Landed Symbol-Specific Sparsification

The summary owner now builds one function-local program-point index per
demanded function. Each parameter row contains only source statements with an
identifier reference to that parameter, using the parser-owned
`ast_contains_identifier_ref` walk. The reference index is intentionally an
over-approximation of the legacy name-based transfer, so shadowing cannot
silently change diagnostic bytes. The existing access and escape transfers
consume those rows directly, so the summary no longer re-enters unrelated
statements on every fixed-point pass.

The index is an optimization view owned by `FunctionParamFlowSummaryStore`,
not a second semantic fact serialization. It preserves the same summary mask,
recursive fixed point, and diagnostic paths. Telemetry reports indexed
statement visits and selected program-point count; the permanent fixture gate
requires a strict reduction.

## 8. Permanent Gates

- `slot-analyzer-host-index-test-smoke` requires the compiler path to carry
  `SemanticContext` into the slot analyzer and requires direct use of the host
  index owner.
- The same gate rejects the generic semantic declaration lookup wrapper, which
  may contain an AST compatibility fallback.
- `function-param-flow-summary-test-smoke` requires the stable hash key and
  recursion states, rejects callee-body reopen in both consumers, and proves a
  recursive may-fact needs and receives a bounded fixed-point revisit. It also
  requires `function-param-flow-sparse` telemetry to show fewer selected
  program points than indexed statements.
- `test-semantic` protects semantic and diagnostic behavior.
- Diagnostic registry/JSON/catalog parity gates protect code and rendered
  diagnostic contracts.
- The fixed self-host command above remains the large-input performance witness;
  the portable CI deadline applies to the small recursive falsification fixture.
