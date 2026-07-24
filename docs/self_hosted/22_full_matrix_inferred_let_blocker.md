# Full-Matrix Blocker: Inferred-Let MIR ABI Type Fact (2026-07-24)

Status: `RESOLVED` — 아래 Option 1이 `6574f89f` ("Carry inferred let type on
native MIR defs")로 착지. `mir_attach_def_type_name_fact`가
`mir_attach_def_initializer_call_fact` 경유로 모든 def 부착 지점(CFG 매칭,
interleave, non-CFG)에서 소비되므로 단일 owner가 유지된다. 이 문서는 관측
증거와 falsifier 목록의 기록으로 남긴다.

## Observed failure (worktree at `04be5305`, clean HEAD snapshot)

The full unfiltered DRV-2 hard matrix fails at
`src/self_hosted/mir_lower/fixture/random_inferred_let.pgy`:

```text
MIR canonicalization failed: mode=--canonicalize-oracle-mir-json
MIR-LOWER ERROR: local declaration is missing its MIR ABI type fact
```

Minimal reproduction: `let event = Random(1);` — an inferred (unannotated) let.
The focused lanes (body 20 + per-fixture) stay green; only the full matrix
reaches this fixture. Last complete unfiltered matrix remains the previously
recorded one; the current-HEAD full matrix is RED at this row.

## Evidence (both producers, same fixture)

- Native oracle def row: `"abi_type_name": null` — but the same JSON carries
  the fact at routine level: `"source_locals":[{"name":"event","type":"Int"}]`.
- Self producer (`--emit-mir-json-verified`) def row: `"abi_type_name":"Int"`.

So the fact exists in both artifacts; the carriers differ (native = routine
`source_locals` row; self = instruction `abi_type_name`). The `564de5be`
lexical-shadow closure made the instruction-level fact required at
`src/self_hosted/mir_lower/stmt_render.pgy#MirDeclaredLocalTypeFact`, which the
shared render path applies to the oracle-bridge lane as well.

## Why a simple fallback is forbidden

Reading `source_locals` whenever the instruction fact is missing would defeat
the `564de5be` falsifier (its mutation strips only the instruction fact and
must stay fail-closed). Any oracle-side admission must therefore be explicit
to the named `--canonicalize-oracle-mir-json` bridge lane, not a shared
`new ? old` read.

## Fix directions (choose one, with its own objective card)

1. **Native producer carriage (preferred direction):** populate
   `inst->abi_type_name` for `AST_LET_DECL` def instructions from the
   routine's owned source-local type during native MIR lowering
   (`mir_add_def_instruction` in `src/compiler/mir_base_helpers.c` has no type
   today; the association point for `source_type=AST_LET_DECL` is the
   population site). Oracle then satisfies the strict fact; the bridge and
   ratchet stay untouched. Check the native MIR unit suite for null-pinning.
2. **Mode-explicit bridge admission:** thread an oracle-bridge flag from
   `CanonicalizeOracleMirJsonBridge` (`driver_rung2_owner.pgy:439`) through
   `EmitMirProgramTree` → `routine_lower.pgy:221` → `MirDeclaredLocalTypeFact`,
   allowing the bridge lane alone to consume the routine `source_locals` row.
   Keeps native unchanged; adds plumbing through the render chain.

Falsifying fixtures for either: `random_inferred_let` (positive), the
`564de5be` instruction-fact strip (must stay fail-closed), and a new negative
where BOTH carriers are absent (must fail closed in every lane).

## Second full-matrix falsifier — RESOLVED by `73d54a33`
"Canonicalize MIR declaration family order" closed the divergence; the
`role_operator_dispatch` hard lane passes producer-first parity on the
current tree (verified 2026-07-24 after the commit). Original observation:

With `6574f89f` applied the matrix passes `random_inferred_let` and stops at
`role_operator_dispatch`: canonical MIR JSON parity fails on exactly one byte
family — the role-method routine `IntMath.Add` carries
`"source_syntax_id":12` on the oracle-canonicalization side but
`"source_syntax_id":6` on the self-producer side. The bridge re-parses the
rendered compact tree, so its syntax ids are parse-order-fresh; role
declarations evidently render/re-parse with a different node budget than the
original parse. This is the role-method variant of the already-registered
open identity class (`mir.generic_specialization` row: "native and self-host
use different SyntaxNodeId conventions; identity convergence open").

Next objective card: either canonical MIR must remap `source_syntax_id` to a
canonical ordinal on BOTH lanes before comparison, or the compact-tree role
rendering must preserve the original node budget. Falsifier: the
`role_operator_dispatch` mir_json artifact pair (2-line canonical JSON, first
divergence at the `Add` routine header).

## Third falsifier: driver full fixpoint (same 2026-07-24, solo run)

`PGY_SELFHOST_DRIVER_FULL_FIXPOINT=1 driver_bootstrap.sh` now clears the
seed/bounded stages (after `compile_c` learned the shared emitted-C
runtime-header owner — that lane drift is fixed in this tree) and fails at the
`driver_mir_oracle` stage on the full driver source
(`src/self_hosted/compiler/driver_bootstrap_main.pgy`):

```text
MIR assignment target binding type drifted
```

Owner: `src/self_hosted/mir/routine_assignment_owner.pgy` — the self MIR
producer's `build.local_types[local_index]` disagrees with
`input.assignments.target_type_names[binding_row]` for some assignment that
only the driver-scale source reaches. Working hypothesis: interaction between
the `564de5be` lexical-shadow local ordinals and assignment binding-row
lookup (a shadowed name resolving to the wrong local index), or a type
spelling divergence the fixture corpus never exercises. The diagnostic now
carries `target=/local_type=/semantic_type=` (plus `node=` since the follow-up
enrichment) and the rerun named it exactly:

```text
target=base local_type=ParserExpressionFact semantic_type=String
```

Confirmed shape: a shadowed local `base` (one `ParserExpressionFact`, one
`String`) where `SelfMirRoutineLocalIndex` resolves the assignment target by
bare name to the first binding while the semantic `binding_row` identifies the
shadowed one. The `564de5be` per-name binding ordinal exists for declarations;
the assignment lookup in `routine_assignment_owner.pgy` must consume the same
(name, ordinal) identity instead of name-only resolution. Falsifier: any
routine with `let base: A` then a shadowed `let base: B` plus a later
`base = ...` assignment to the inner binding.

## Final F verdict (2026-07-24 evening, isolated runs)

- **This falsifier is CLOSED**: with the semantic routine local inventory SoT
  (`aba64ab1`) on the tree, the isolated oracle stage logs `ast:done` and
  `semantic:done` — the `base` drift no longer reproduces.
- **Shared `.tmp` BUILD_DIR race (operational):** two concurrent
  `driver_bootstrap.sh` sessions clobber each other's artifacts (per-stage
  `rm -f`), and executing `bin/pgy.exe` while the other session relinks it
  fails silently — both symptoms read as empty-`.out/.err` "silent crashes".
  Isolation works and must be used for concurrent verdicts:
  `PGY_SELFHOST_CODEGEN_BUILD_DIR` + `PGY_SELFHOST_DRIVER_BUILD_DIR`.
- **The remaining full-fixpoint blocker is located and measured**: the oracle
  driver dies inside the `verify` pressure stage. A 64MB-stack relink did not
  crash but ballooned to **~12GB RSS in ~30 minutes** (observed live) — the
  already-recorded allocation-amplification debt, now pinned to full-driver
  `verify` rather than codegen emission. Normal 1MB-stack builds die there as
  a silent OOM, hence every empty-artifact failure. Stack size is not the
  cause; disk stayed ~0.4GB (the growth is resident memory). The rung that
  unblocks `PGY_SELFHOST_DRIVER_FULL_FIXPOINT` is an allocation-growth
  closure in the verify path (bounded/streaming verification or arena reuse).
  Repro: emit oracle C (`--emit-c`), link `gcc -x c -std=c11 -I src
  -I src/runtime -pthread`, run `--emit-mir-json-verified <driver source>
  <out> --pressure-owned-full-fixpoint`, watch RSS after `verify:start`.
- **Refined measurement (same evening, two guarded runs on the then-current
  tree with the in-flight normalization slice):** the death point is
  NON-DETERMINISTIC — one run died at semantic-initializer row 7734 after
  4.7 min at peak 3.4GB (no guard involved), another reached
  `call-targets:start` past row 8109 at 12 min. Variable failure row +
  multi-GB footprint + zero diagnostics points at an unchecked allocation
  failure or heap corruption at scale in the emitted C, not monotonic string
  growth alone. The granular `semantic-*-stage row:N` prints bracket the
  failure to the per-row semantic pressure loop; bisecting with those
  anchors (or linking the oracle against ASan on the Linux runner, where
  libasan exists) is the fastest localization.
