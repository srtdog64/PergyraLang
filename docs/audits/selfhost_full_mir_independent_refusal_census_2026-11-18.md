# Fixed DRV-2 MIR: independent routine refusal census (2026-11-18)

Status: diagnostic inventory, **not admission or memory closure**. The fixed
418,865,243-byte MIR has SHA-256
`e7767d3059972797baf41654ececd912bfb91a0755547afea6336e6af8bbdf19`.
The product source is published `main @ d21aa036dc7599085c61325c45e8954f7113357f`;
[required CI run 44219519773](https://github.com/srtdog64/PergyraLang/actions/runs/44219519773)
passed all 33 executed jobs, including bootstrap and the fixed-point join.
The Pergyra-built v8 driver source used for this scratch diagnostic has the
same compiler-source generation as that commit; its product driver SHA-256 is
`9a06da62ed82fa3d4dbd21699931b70f168ebf5f996ad820395b3d295e92b174`.

## Method and bound

Five scratch-only generated-C instruments (`v8b`, `v8c`, `v8e`, `v8f`, `v8g`)
covered all 9,623 route instances. The route puts `Main` first, then admitted
non-generic routines and any closed generic instances; source JSON itself
contains 9,561 functions, 47 methods and 15 intents. Each trial resets
routine-local carried plans, records the
first reached refusal and continues. A transition refusal is recorded before
returning an empty diagnostic fact. A fatal routine error is caught only in
this diagnostic executable. The scan deliberately stops before GraphPlan
sealing or emission; it cannot publish an accepted C artifact.

The final `v8g` pass reached `INDEPENDENT_CENSUS_REACHED_END routines=9623`.
Its process-boundary receipt reports 14 GiB `RLIMIT_AS`, 1,800-second timeout,
666.55 seconds elapsed, exit 1 from the deliberate diagnostic stop, and a
drained process group. The forbidden C output did not exist. Scratch logs and
receipts live under WSL `/root/mg-main/.tmp/selfhost-memory-20261117/` as
`independent-routine-census-v8{b,c,e,f,g}.*`; they are not tracked evidence.

## First refusal per independent routine

| Reached owner / stage | Distinct routines |
|---|---:|
| Collection ownership transition: owned flow (guard 5) | 28 |
| Collection ownership transition: other guards 4, 6, 8, 17 | 7 |
| Expression admission (builtin, direct call, graph use, left edge, admitted type, leaf operand, literal use) | 25 |
| LocalRef / match binding | 15 |
| Instruction kind, all in late intent routes | 13 |
| Indexed assignment facts | 6 |
| Return ABI | 6 |
| Statement admission | 4 |
| Local inventory | 2 |
| Value type plan | 1 |
| Invalid routine with no owned refusal text, late intents | 2 |
| **Total distinct first-refusal routines** | **109** |

This is a bounded *first-refusal* census. A routine refused before the
collection transition cannot reveal a later transition error in this pass;
the scratch replacement of a failed transition also cannot establish that
the remainder of that routine is valid. The two empty-refusal cases need
diagnostics before their cause can be classified. In particular, zero
ownership refusals, complete direct-MIR admission, emitted output and the
one-process source-to-C memory target have **not** been established.

Representative source-level cases show the chain rather than a file count:
`SemanticAstSignatureTypeTextShape` returns a call-result `Array<String>`
across branches and first hits transition guard 4; its two call-result rows
carry `element_ownership=retired`, which no longer records their prior element
ownership. `SemanticAstCollectionExclusiveAddDep` moves `into.dep_fields`
into a local, appends, then restores the field; it first hits owned-flow guard
5. The 13 instruction-kind refusals occur at intent declarations near the
end of the route. These are distinct fact-owner or emitter obligations, not
permission to skip a guard.

The follow-up scratch subcensus on seven representative guard-5 routines
found the same `OWNED_FLOW_INVALID guard=9` in each: a `retired` row still had
a live terminal state. This is a first-failure grouping, not evidence that
the seven cases share one repair. The inout field-carrier path has a concrete
identity mismatch: its source is a parameter, while the member-restore query
accepted only declared local sources. A first test driver with that query
extended admitted the focused positive, but rejected the existing
`nested_transfer` cleanup control and moved the full-MIR first stop backward
to ordinal 356. A narrower v10 rule repaired that control but moved the
complete first stop to ordinal 1205, a function that drops three array
fields before restoring any. The v11 consumer requires the same-block deep
drop to be the last use or definition of each local before its exact-field
restore. Its focused C/LLVM, existing cleanup and component gates passed;
the complete fixed MIR returned to ordinal 1438 without a C artifact.
The [v11 run record](../truth/evidence/2026-11-18_inout_member_restore_transition.md)
does not claim all guard-5 failures are gone. The active execution plan remains
[`selfhost_owned_value_lifetime_execution_2026-11-18.md`](../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md).
