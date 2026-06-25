# Evidence-Driven Guard Amortization

Status: `first compiler slice active`. Owner doc for a Pergyra-specific
optimization family. The active compiler claim is intentionally narrow: plain
`Slot<T>` MIR pin regions with cleanup-edge evidence can lower to one preflight
view in C and LLVM. Secure pin remains a runtime token/capability retain point.

Related:

- `docs/74_slot_pinning_caching.md` - Slot/Pin/Lease model.
- `docs/semantics/14_air_erasure_measurement.md` - erasure/retain measurement.
- `docs/perf_close_to_c.md` - native baseline and string-window measurements.

## 0. Intuition

Pergyra should not chase only zero-cost abstraction. Its stronger local edge is
evidence-amortized abstraction:

> Pay a guard once at the boundary where evidence is established, then consume a
> compact evidence view inside the hot path.

That shape fits `slot`, `pin`, `zone`, `intent`, and `with`: those boundaries
are both safety scopes and optimization scopes. The safety evidence is already
the optimization fact; the backend should not have to rediscover it from lowered
C or LLVM IR.

## 1. What The Backend Can Use

| Evidence | Hot-path fact | Candidate lowering |
| --- | --- | --- |
| unique owner / no alias in region | no per-access owner reload | view pointer + length snapshot |
| generation unchanged in region | generation check is invariant | one preflight guard |
| capability granted by zone/intent | effect-site checks coalesce | boundary bitset check |
| no release/unpin in region | cleanup path is stable | direct load/store/GEP |

The novelty is not any single row. The Pergyra-specific part is that
`zone`/`intent`/`slot` evidence gives one place to prove, name, and retain the
fact.

## 2. Soundness Constraint

Only proven evidence may become a backend assumption. Heuristic evidence stays a
runtime guard.

- proven invariant region: one preflight check plus evidence view;
- unproven or transitioning region: keep per-access guard;
- external boundary, shared mutable state, or cancellation edge: retain runtime
  checks with an explicit retain reason.

This must flow through MIR facts, not AST re-scans or backend guesses.

## 3. Experiment Tracks

### Track A: Safety-Guard Hoisting

Hoist a per-access runtime guard out of a hot loop when evidence proves the
guarded state is invariant over the loop region.

Required proof shape:

1. Same result with per-access guard and preflight evidence view.
2. A generation/state mutation rejects the evidence view.
3. A future MIR pass must emit the optimized path only when it has the invariant
   fact; otherwise it keeps the guard.

### Track B: Domain-State Specialization

Resolve a stable domain state at the boundary and specialize the loop body to
that state. This remains deferred until there is a real dynamic-dispatch target
worth specializing.

## 4. Reproducible Measurement

`benchmarks/perf_guard_amortization.c` is the first checked-in Track A fixture.
It models the exact optimization shape:

- baseline: owner/generation/capability/state checks on every slot-style read;
- no-cache preflight shape: each access rebuilds the evidence view before the
  read;
- optimized cached shape: one preflight check constructs an evidence view, then
  the hot loop reads through the cached view;
- fail-back: a generation change rejects the view (`invalid=-1`).

Representative local Windows/MinGW gate run:

```text
$ make evidence-guard-amortization-test-smoke
[guard-amortization] fixture=slot_read iterations=50000000
[guard-amortization] per_access_guard_avg_s=0.075167
[guard-amortization] repeated_preflight_avg_s=0.146333
[guard-amortization] preflight_view_avg_s=0.021000
[guard-amortization] preflight_over_per_access_ratio=0.280
[guard-amortization] cached_preflight_over_repeated_preflight_ratio=0.144
[guard-amortization] preflight_over_per_access_best_ratio=0.224
[guard-amortization] cached_preflight_over_repeated_preflight_best_ratio=0.118
```

Verdict for Track A: worth pursuing. On this fixture, a one-time preflight view
measured at `0.280x` of the per-access guard path in the representative local
gate run (about `3.57x` faster within this fixture). The explicit cache-effect
metric
compares the cached view against the repeated-preflight no-cache shape; the same
run measured `0.144x` (about `6.94x` faster than rebuilding the evidence view on
each access). The gate records both average ratios and best paired ratios. The
best paired ratios are the pass/fail signal because the fixture now measures
time inside the C benchmark process and treats shell/process scheduling as
ambient noise. This is not yet a whole-language performance claim; it is a
falsification gate showing that the optimization family has a measurable target.

## 5. Compiler Slice: MIR Pin Region

The first implemented slice is deliberately small:

- owner fact: `mir_block_has_pin_guard_amortization_region(...)`;
- proof input: the block is a pin region with source slot, view name, and a
  matching cleanup-edge fact;
- C consumer: `transpiler_emit_mir_plain_pin_preflight_local(...)`;
- LLVM consumer: plain MIR pin enter requires the same fact before inline
  lowering;
- retained runtime path: secure pin still calls the secure runtime ABI because
  token validation is capability evidence, not a plain slot layout invariant.

Generated C for a plain pin now emits a slot pointer local, one null guard, one
occupied guard, and a `PgyPinnedSlotView_*` initializer. It does not call
`pgy_pin_*` / `pgy_unpin_*` for this MIR slice. LLVM emits the corresponding
inline null/occupied preflight and direct pinned-view field stores. If the MIR
fact is absent, LLVM fails closed instead of guessing from source shape.

## 6. Definition Of Done

- A MIR fact names the invariant guard region. (`Slot<T>` pin slice: active)
- C and LLVM consume the same MIR fact. (`Slot<T>` pin slice: active)
- Optimized path has no per-access guard in the hot loop.
- Fallback path remains guarded when the region mutates or evidence is missing.
- The smoke gate records `preflight_over_per_access_ratio`.
- The smoke gate records
  `cached_preflight_over_repeated_preflight_ratio`.
- The smoke gate records and thresholds the paired best ratios:
  `preflight_over_per_access_best_ratio` and
  `cached_preflight_over_repeated_preflight_best_ratio`.
- Docs report measured fixture ratios, not generalized language-speed claims.
