# Self-host MIR carries compiler Clone landings to the direct routes (2026-11-22)

Source: main `309301eeb` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` and `/root/ts3`. Plan: [directive](../../agent_work_directives/selfhost_compiler_clone_carriage_2026-11-22.md),
slice 1 (S5c of the [one hand-off rule decision](../decisions/2026-11-16_one_handoff_rule_for_both_front_ends.md)).

## Before

The self-host semantic analysis selects compiler `Clone` landings
(`compiler_clone_kinds`). Source C copies them; self-host MIR carried only
the stored value of an owned String push or set. The source C and LLVM
routes counted the other landings from the program facts
(`driver/driver_compiler_clone_route_owner.pgy`) and refused the program.
`--mir-json-backend` has no program facts, so it emitted the program without
the copy: `local_copy_source_sorted_landed_valid.pgy` printed `11` on direct
C and LLVM where native prints `31` (the copy `before` shared `items`'
storage and saw the sort).

## Change

- Producer: `mir/clone_site_projection_owner.pgy` lists every landing no
  collection receipt copies, as ascending graph node ids in the program
  facts (`clone_sites`), computed once the routine facts are final. The
  instruction writer emits `expr0_clone_sites` / `expr1_clone_sites` (window
  indices, only when a lane has one) through the same lane-site writer as
  the region sites.
- Admission: a listed node is appended as usual, then a Clone node over it,
  resolved through the explicit Clone signature (String: kind 151, `Array<Int>`
  and `Array<String>`: kind 144); the node's consumer reads the copy. The
  Clone node's literal names the site (`compiler-clone:<lane>:<node>`), which
  the two Clone readiness checks accept. A malformed list, an index past the
  graph or a type with no Clone kind is refused (`clone-site-list`,
  `clone-site-window`, `clone-site-type`).
- Program check: after every routine is admitted, each listed site must have
  its Clone node in an admitted graph of its instruction; otherwise the
  program is refused ("compiler Clone site was copied by no admission").
  The nested-intent shape route, which does not run the admission, does not
  claim a program with a site; the MIR to AST round trip derives the copies
  again.
- Deleted: `driver/driver_compiler_clone_route_owner.pgy`, the landing
  count parameter of the C route chain, the source MIR receipt's
  `compiler_clone_landings` and the source LLVM refusal. The direct-MIR lane
  site reader is shared (`direct_mir_scalar_program_lane_site_owner.pgy`,
  renamed from the region site owner).
- Gate: `selfhost_compiler_clone_carriage_owner.sh` (CI push step).
- Gates that expected the deleted refusal:
  - `direct_mir_array_string_cleanup_fact_owner.sh`: `grown_loop`, a local
    handed again on every loop pass to a callee that grows it, now runs on
    both legs with the copy.
  - The same gate's `member_alias` is refused at the transition admission
    (below). Its MIR with the site removed still reaches the backend's own
    alias guard.
  - `shared_storage_copy_landing_owner.sh`: `inout_hand_off`, an Array
    argument, now runs on direct LLVM. `formal_member_grown`, a record
    argument, is refused by direct MIR. That refusal comes before the
    site: the same MIR without its site is refused the same way.

## Results

Driver: release build of this tree. Previous driver: the `cc27d42c6`
release build.

- New gate: PASS. On the previous driver it fails at the first program:
  its MIR carries no compiler Clone site.
- Runs under LSan, C and LLVM, for the 5 gate programs:
  - all print native's output with no memory error;
  - the copies leak. The two Array landings leak exactly what their
    explicit-Clone versions leak (C 48 and 32 bytes, LLVM 36 and 28);
  - the explicit String versions are refused by the self-host front end
    (`owned_element_escape`), so the String landings have no comparison.
- The two updated gates PASS. Both FAIL on the previous driver.
- Default-route LLVM sweep of the 1,398 programs:
  - equal 529 -> 535; no program left equal;
  - the deleted landing count's 30 refusal records are gone;
  - 8 programs are refused with `clone-site-type` (a record or Map
    landing);
  - non-equal 574 -> 568, first-error clusters 118 -> 119.
- Candidate gates (307 MIR/concat related): 148 fail.
  - One is the cleanup gate, run before its update; it PASSES now.
  - The other 147 also failed on the previous result, 145 with the same
    message.
  - `one_mir_string_builtin_program_projection.sh` now gets past its MIR
    pin and fails at `c rejected display-only`. The previous driver fails
    it there too, and its MIR is byte-equal on both drivers.
  - `completeness_ledger.sh` times out on both, at a different file.
- The 75 scripts behind the CI push make steps match the `cc27d42c6`
  baseline, except:
  - the two updated gates;
  - 6 smokes that failed in the baseline run and pass now (3 native
    ownership, 3 installed semantic).
- The 31 scripts the CI push step runs directly, including the new gate:
  all PASS. On main with the previous driver, the 30 existing ones PASS.
- SoT registry gate PASS (CLOSED=62 BRIDGE=17 ACTIVE=2); gate
  reachability: 889 scripts, 0 undeclared, 0 stale.
- Codegen bootstrap: PASS, gen2 == gen3 (125,506 lines of C).

## Open

- A landing at a binding whose MIR ownership row names a member move
  (`member_alias`, an alias of an inout record member) is refused at the
  transition admission. The row still names the move the copy replaced. A
  single-block variant is refused the same way. The row of a landed
  initializer should be a `clone` row. Open with slice 2.
- Record and Map copies: slices 2 and 3 of the directive.
- A MIR whose site list was deleted cannot be told from one with no landing.
