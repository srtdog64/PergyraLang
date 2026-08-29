# Owned sequence named value-boundary parity result — 2026-08-30

Status: `LOCAL GREEN — PUBLICATION PENDING`

Exact base: `c3fc46b003d7bb60f371e311b38a0d757672a64c` on
`origin/main`.

This audit records one reached executable source-semantic replacement. It does
not own compiler semantics, registry status, project percentage, remote
publication, or a successor rung.

## Result

`SemanticAstNamedValueBoundaryVerdict` now consumes canonical
`SemanticOwnedSequenceElementType` evidence for `Array<T>`, `List<T>`, and
`Queue<T>`. An unnamed function-returned List or Queue passed through a
non-default value boundary is rejected during semantic body admission instead
of reaching verified MIR publication.

At the base, native rejected the List and Queue probes before C publication,
while installed self-host exited zero and published 8,164- and 8,196-byte MIR
artifacts. The current installed DRV-2 rejects both with stable diagnostic
`named_value_boundary_argument_required` and publishes no MIR.

The verdict does not own constructor-name or type-name exceptions. A direct
call is exempted only when the existing contextual builtin signature owner
returns the exact admitted owned-sequence type. Native therefore retains fresh
`ListNew()` and `QueueNew()` ownership transfer. Installed self-host still
rejects those calls earlier as `List<Unknown>` / `Queue<Unknown>`; this result
does not claim that separate contextual-typing bridge is closed.

## Falsifying evidence

- Native and installed self-host reject function-returned `List<Int>` and
  `Queue<Int>` at an `own` boundary without C/MIR artifacts.
- Named own List/Queue values and unnamed default-mode List/Queue call results
  remain admitted by both pipelines.
- Native fresh List/Queue constructors at matching own boundaries remain
  admitted.
- The canonical owned-sequence shape excludes `Slice<T>`. The broader sequence
  shape still admits Slice, so the two facts cannot silently collapse.
- The verdict consumes the contextual builtin signature owner and contains no
  List/Queue type-name allowlist or backend rejection fallback.

## Observed local gates

- current-source Pergyra-built DRV-2 installation: PASS
- native production `driver_bootstrap_main.pgy` C emission: PASS,
  32,931,450-byte artifact, zero errors, and three pre-existing redundant-`who`
  warnings
- owned-sequence native/self-host rejection, named/default controls, and native
  fresh constructors: PASS
- existing Array family named-boundary regression: PASS
- shared collection call protocol and negative ratchet: PASS
- self-host hard substitution contract: PASS
- SoT authority edge: PASS, 88 authorities and 183 derived carriers,
  `CLOSED=55 BRIDGE=32 ACTIVE=1`
- SoT live adequacy: PASS for live bindings and negative mutations; Coq/Rocq
  was explicitly declared skipped because no prover is installed on this
  runner
- single-owner, likeness `4519/4519`, official 146-row language inventory,
  documentation, and progress checks: PASS
- full self-host component contract: PASS

## Observed broader-gate boundary

The filtered `driver_rung2_body_parity.sh` C leg reached and passed `list_ops`
and `sequence_literal_list_queue`. Its LLVM leg could not start fixture parity
because the native LLVM test-driver build fails at
`driver_source_llvm_intent_execution_owner.pgy:29` on the pre-existing
`self.outcome = Some(...)` contextual Option gap. That line was introduced by
`3e5ad3d6a2daad89e6be1a56e6f4dbd134a79db0` on 2026-08-25 and is unchanged at
this audit's exact base. The new named-boundary owner itself emits standalone C
and LLVM with zero errors and warnings. This result therefore does not claim a
green full LLVM driver-body leg or silently turn it into a skip.

## Remaining boundary

This result replaces a real self-host acceptance path for List/Queue at the
reached semantic boundary. It does not close the registry row or change the 83%
project forecast. Slice call-target completion, self-host contextual typing of
fresh List/Queue constructors, Set/Map/Option/Result/tuple/nominal/resource
classification, and backend collection ABI/lowering remain separate. The SoT
census remains `55 CLOSED / 32 BRIDGE / 1 ACTIVE`.
