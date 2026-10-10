# Self-Host Parity Harness

Status: fifteen rung-2 peripheral harnesses, five rung-1 AIR graph consumer
harnesses, one Pergyra-origin fuzz generator harness, plus lexer/parser rung-1,
semantic rung-2, and codegen rung-0..20 compiler-internal harnesses active.

This folder holds oracle comparisons for self-hosted tools and compiler-stage
substitutes. The C compiler and existing shell/C smokes remain the source of
truth until a Pergyra-written component can run and produce deterministic output
that agrees with the C oracle. JSON is used when the owned format is JSON;
semantic verdicts use diagnostic blocks, and codegen rungs use run-stdout.

Hard substitution rungs are parity gates promoted to pass conditions: failure
means the Pergyra substitute, the C oracle surface, or the LLVM oracle leg has a
real source-of-truth problem to close. Bridge scripts may compare artifacts, but
the Pergyra code must not recover hidden semantic facts by parsing older source
payloads.

The parity set currently covers:

- `diagnostic_catalog_checker`
- `stable_subset_section_checker`
- `air_graph_json_validator`
- `air_graph_id_uniqueness`
- `air_graph_node_count_integrity`
- `air_graph_ref_live`
- `air_graph_ref_integrity`
- `air_graph_reachability`
- `ast_read_surface_checker`
- `abi_layout_row_manifest`
- `backend_output_comparator`
- `backend_output_tri_compare` (C/LLVM outputs checked by the Pergyra
  comparator; use `make self-host-backend-tri-compare-extended-test-smoke` for
  the opt-in 29-case C/LLVM closure gate)
- `completeness_impact_planner` (changed-path JSON plus `run_group_plan`
  projection for runner-consumable proof-gate groups; the paired run-group
  runner validates all groups and executes a bounded prefix)
- `module_manifest_resolver`
- `stdlib_dispatch_inventory_checker`
- `doc_link_checker`
- `production_header_size_checker`
- `production_c_size_checker`
- `examples_inventory_checker`
- `runtime_boundary_checker`
- `fuzz_backend_parity_generator` (Pergyra-origin deterministic source corpus
  generator; `make self-host-fuzz-backend-generator-parity-test-smoke` checks
  generator C/LLVM byte-identical corpus output, while
  `make fuzz-backend-parity-test-smoke` additionally runs the generated corpus
  through C/LLVM and treats generated nonzero exits as invariant failures;
  `make fuzz-backend-parity-matrix-test-smoke` repeats that oracle over a
  bounded seed matrix. The generated corpus includes predicate-driven cursor
  updates so branch-style and predicate-value-style state transitions stay
  equivalent across backends)
- `lexer` (rung-1 compiler-internal lexer substitution)
- `parser` (rung-1 compiler-internal parser substitution)
- `semantic` (rung-2 compiler-internal semantic verdict substitution)
- `codegen` (rung-0..20 compiler-internal C-emitter substitution)

`make self-host-preparation-test-smoke` runs the full set. Individual parity
targets may still be used for focused work, but a tool is not considered
current unless its `tests/self_hosted/parity/<tool>_parity.sh` rung passes.

Minimum parity contract for each tool:

The focused `collection_inout_effect_owner.sh` is analyzer-only: supplied
negative programs are not emitted or run. It pairs borrowed default/inout/ref
formal own-entry refusal with readonly alias chains and fresh-definition
controls. Exact-definition borrowed edges propagate real effects to shared
views, not synthetic Unknown bounds; later fresh storage is independent.
Formal identity owner units mutate freshly admitted facts and check final
authority refusal. These checks are not MIR, installed-driver, aggregate-field
release, hard C substitution or registry-closure evidence.

`collection_formal_key_consumption_owner.sh` separately pins formal-use
producer effects/verdicts and observes every returned carrier field and map
entry on native C/LLVM, including partial malformed-input refusals. Its
source-placement ratchet rejects the two historical unconsumed key hoists;
that static check is not behavioral or performance evidence. It runs through
`self-host-collection-formal-key-consumption-test-smoke`, a prerequisite of
the inout-effect target. Admitted borrowed-element statement-target positive
coverage remains OPEN; rejected indexed-mutator source is not forged into
typed facts to count that branch. Whole-input cost, P1, installed-driver and
SoT closure require their separate gates.

`collection_binding_move_prefix_owner.sh` pins seventeen real-source/parent
observations on native C/LLVM: exact scanner triples and matched leaf text,
caller/body diagnostics and source IDs, canonical definition/storage rows,
and completion slots. Its optional `BINDING_MOVE_BASELINE` compares a frozen
pre-edit observation; permanent full-output pins are checked even without
that local receipt. The separate source ratchet requires the empty-map read
guard and leaves completion/lineage updates outside it, rejecting four planted
placement regressions. `--static-only` is placement evidence only;
`--behavior-only` is executable observation only. The default runs both through
`self-host-collection-binding-move-prefix-test-smoke`, reached by the inout-effect
target. Malformed analysis is refused by the validating parent, not sent to a
raw scanner. Probe-skipped invalid body cases are NOT_OBSERVED, not proof of
production call order. Missing-subtree admitted-positive and strict same-SyntaxId
post-insertion later-lane coverage remain OPEN; no forged AST fills those gaps.
This does not establish whole-input cost, installed-driver parity or P1 closure.

`member_indexed_read_permission.sh` runs the reached member transition,
capture and terminal-publication chain on native C/LLVM. It preserves the
original sixteen source controls and adds thirty-five native-calibrated
capture controls: readonly nominal places cannot become owned locals,
constructor/literal/container stores, destructured values, returns or own
actuals. Default/ref calls, actual owned values and fresh factories remain
positive controls. ArraySet consumes the parser-owned Auxiliary value lane;
store calls require canonical builtin identity and the collection protocol's
store policy, not a spelling match. This is bounded capture admission, not
general return-provenance inference or graph/view lifetime implementation.

The same gate checks nine complete publication results, three terminal
transfer refusals and two selected formal-fact corruption cases per backend.
The corruption probes start with real parser/analyzer facts and require both
the capture site and existing signature/artifact admission to refuse the
selected missing row or invalid mode. Publication checks pin early-error
priority, partial carriers, generation and read/escape/nested closure order.
Source-size caps remain unchanged; the responsibility split is not permission
to condense source or weaken a cap. Native, inputs, transitive imports,
executables and observations are hash-bound before and after execution.
Run it with `make self-host-collection-member-permission-test-smoke`; it is a
prerequisite of the inout-effect target, not a second manual gate. Supplied
sources are analyzer inputs; empty-Main native controls do not prove their
payload execution. Full source admission, installed-driver parity, C-path
substitution, SoT closure and exact-candidate CI remain separate obligations.

The selected-site executable also imports the scalar capture-source unit: two
actual implicit Int returns need no nominal backing identity, while Unknown,
missing types and nominal-without-backing fail. Leaf types come from the existing
body environment issuer; this is not an independent arbitrary forged-type
validator. Member receipts are bound to their own imports, so a later event-owner
edit requires a fresh importing pair.

The inout gate consumes pending argument/definition retirement in the scan's
one current-definition stream, not a terminal second scanner. Units 125–128
pin exact pending site, actual generic refusal nonpublication, missing current,
a real foreign current definition and zero syntax. Same-event API checks do not
replace later-event source controls. The structural order ratchet rejects
seventeen planted placement/site/write/fact-overwrite variants; executable
observations own semantics. Unit
scratch and the scan-local map have no physical-reclamation evidence here.

Its formal/member source controls require exact root/declared-field identity,
reject repeated extraction and indexed-target false restoration, and retain
whole-field writeback. Generic source controls consume the real admitted body
type view; unit-projected nominal types alone do not establish that producer.
`collection_member_identity_owner.sh` pairs 43 member identity units (one
baseline and 42 mutations), two constructor-carrier negatives and five invalid
observer mode/arity refusals per backend. Its optional reuse directory must be
inside this checkout's ignored artifact directory with matching native, source,
input and transitive-import hashes, plus an exact two-binary receipt. The inout
gate retains its separate exact six-binary receipt (C/LLVM source, formal and
constructor observers). Neither gate grants
aggregate storage/element lifetime or proves actual Release execution.
Run the focused member gate with
`make self-host-collection-member-identity-test-smoke`; it is also a prerequisite
of `self-host-collection-ownership-semantic-test-smoke`.

The constructor prerequisite keeps descriptor escape distinct from opaque
call Unknown/retirement. Exact current-definition escape bounds forbid deep
release, own forwarding and descriptor mutation, but preserve known Live
element facts for proved readonly indexed calls. Direct Let/shared-storage
lineage inherits the bound; a fresh Clone Assign does not. Source controls
include alias laundering, own-formal direct drop/forwarding, duplicate readonly
descriptor inputs, nested Clone and fresh Assign. Local and own-formal parser
Push/Pop/Set/indexed targets, own-string push and nested/deferred mutation also
consume the exclusive permission boundary. Collected formal effects include
later argument consumption and future opaque/constructor stores in defer;
ordinary mutation before a store and unescaped/fresh storage remain controls.
Nineteen constructor units
check two same-typed physical fields, duplicate/missing identity and callable/
nonconstructor exclusion; five of those units check synthetic vector readiness,
current-definition selection and escape-only closure, not actual producer
identity. Four invalid CLI modes are refused per backend.
`field_ctor_generic_type_held.pgy` and `field_ctor_callable_type_held.pgy`
remain earlier type-admission controls, excluded from permission-loss counts.
This is not exclusive field-entry, aggregate return, MIR receipt, native-oracle
or installed-driver lifetime evidence. Actual callable-table Release remains
the active falsifier; do not remove its three deep drops or writebacks.

- run the existing C or shell oracle against the same input;
- run the Pergyra tool against the same input;
- compile and run the Pergyra tool through both C and LLVM when the current
  compiler build includes LLVM; C-only builds must keep the C leg mandatory and
  emit an explicit LLVM-leg skip;
- compare exit class first;
- compare the owned stable output shape incrementally while the Pergyra tool
  remains a partial implementation.
- compile/run the TestHarness-manifest-projected Pergyra source owner directly;
  scratch directories such as `.tmp/self_hosted/...` are for binaries, logs, and
  comparable artifacts, not copied source trees.
- keep full-corpus probes and campaigns opt-in or bounded by default, so routine
  self-host verification does not accumulate a campaign's worth of scratch
  output.
- use `make clean-scratch` to reset accumulated `.tmp/self_hosted` and
  backend-compare scratch artifacts without touching build outputs. The target
  resets the whole ignored `.tmp` scratch zone, and bootstrap compiler logs must
  stay bounded evidence artifacts rather than unbounded compiler stderr dumps.
- use `make build-resource-report` before broad local CI runs when the machine
  feels stalled. `make clean-local-artifacts` is the explicit heavier reset for
  ignored root-level `build-*` / `bin-*` variants plus `.tmp`; it is not part of
  normal narrow-gate work.
- never leave `.exe`, `.o`, `.d`, or probe artifacts beside
  `src/self_hosted/tools/*/main.pgy`.

Compiler-core self-host migration from this folder is allowed only as a verified
rung with its own intent contract and C/LLVM/Pergyra parity gate.

Fuzz harnesses are intentionally separate from long-running fuzz campaigns.
The parity contract here is deterministic: fixed seed, fixed count, stable
manifest/source output, no executable-output extension ambiguity, and optional
generated-case C/LLVM execution equality. Crash minimization, corpus reduction,
and property-pack mapping remain outside the beta parity set until those
policies are frozen.
