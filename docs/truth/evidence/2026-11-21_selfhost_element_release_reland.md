# Self-host element release re-landed with Option payload loans (2026-11-21)

Source: main `d9ce69940` plus this change (the reverted `1484fa609` and the
Option payload rule). Trees: WSL `Ubuntu-C-WSL` `/root/ts2` (drivers) and
`/root/ts3` (gates). The change's driver is a release-profile test build.
First landing: [record](2026-11-21_selfhost_element_release_loan_refusal.md).

## Before

`1484fa609` failed [CI 44836830024](https://github.com/srtdog64/PergyraLang/actions/runs/44836830024)
in `self-host-codegen-bootstrap-linux`. The bootstrap reached gen2 == gen3,
then gen2 refused `src/self_hosted/tools/completeness_impact_planner/main.pgy:219`
(`indexed_string_use_after_element_release`). There `impact_id` is
`UnwrapOption(impact_id_opt)` and `impact_id_opt` is
`ImpactPlannerRowField(row, 0)`, a split row's field: it borrows none of
`group_source_values`, yet the loan was unresolved and the owned `ArraySet`
on that array ended it. `c371e2b49` reverted the change. The local check had
covered the canonical DRV-2 input and the corpus sweep, not the 38 tool
programs the bootstrap compiles.

A first fix let every `Option<String>` local carry an unresolved loan and
read it back. On the same tools it refused 9 programs and the canonical
DRV-2 input (`indexed_string_use_after_deep_drop` at
`ast_signature_type_expression_fact_owner.pgy:672`); it was not kept.

## Change

`ast_collection_option_payload_loan_owner.pgy`: `UnwrapOption(o)` takes the
loan of the direct call that initializes `o` when `o` and every String local
that call takes are fixed (no whole-local assignment, no rebinding call; an
`Option<String>` handed to a declared function is not fixed). Any other
`UnwrapOption` stays unresolved. The fixed-local rows ride on the fresh
String result plan.

## Results

- Tool programs (`src/self_hosted/tools/*/main.pgy`, 38): the front-end
  verdict of every one equals the main driver's.
- Canonical DRV-2 producer on one tree with both drivers: exit 0, MIR
  byte-equal (SHA-256 prefix `c03e45697baf024b`).
- `element_release_loan_refusal_owner.sh`: 12 refusals (new: a reassigned
  Option local and a call argument reassigned after the call), 7 admitted
  programs under ASan (new: `UnwrapOption` of a fresh call's Option read
  after another array's set), 1 argument copied where taken. PASS; the main
  driver fails at `pop_index_alias`.
- Default-LLVM sweep, 1,398 programs: equal 526 → 526; the same 3
  `*_landed_valid` programs pass the front end as in the first landing.
- The ownership gates that make targets and push steps reach (43 after
  the role operator and two-routine routes left), in `/root/ts3` with the
  change's driver installed as `bin/pgy-self-driver` and a probe built from
  the changed tree: 36 PASS; the other 7 fail on main the same way
  ([first landing](2026-11-21_selfhost_element_release_loan_refusal.md#ownership-gates)).
