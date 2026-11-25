# Record Array fields on the general direct-MIR route (2026-11-25)

Source: main `58220e986` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2`. Native reference: `bin/pgy` built from the same source.
Drivers (test profile, `-O0`): base `driver_rl3` (`58220e986` with route
logging only), change `driver_rf1`.

## Route census at `58220e986`

The base driver logged which direct-MIR route served each of the 2,746
corpus programs on default LLVM.

- The three-routine route received 49 programs and ran one,
  `src/self_hosted/mir_lower/fixture/generic_member_record_array_return_flow.pgy`
  (a generic class method returning `Array<Point>`); it refused the other 48.
  Six of the seven fixtures its seven `one_mir_*` gates use are already
  claimed by the general route. Those gates run in no CI job and stop at
  their native/self MIR comparison on main.
- The composite intent route ran one program,
  `examples/composite_intent_orchestration/main.pgy`; the legacy and nested
  intent routes ran none (their gates use their own fixtures).
- Of the 1,200 programs native admits and default LLVM refused, 453 stop in
  the self-host front end (parse or semantic). The largest general-route
  class was `seal-record-collection-abi` (60).

## Before

A record field of type `Array<Int>` or `Array<Bool>` needs that Array's ABI
to lay the record out. The program-wide Array ABI facts were captured only
from the MIR rows of routine parameters, results and definitions, and MIR
carries no row for a record field. A program whose only `Array<Int>` was a
record field was refused at `seal-record-collection-abi`; adding any
`Array<Int>` parameter to the same program made it run.

## Change

- `direct_mir_array_captured_abi_fact_owner.pgy`: the canonical Array ABI row
  is built from the storage layout contract (target profile) and the
  element's runtime constructor and C element type. A captured MIR row is
  admitted only when its identity equals that row; the size, alignment and
  offset literals of the old check are gone.
- `direct_mir_array_{int,bool,string}_abi_fact_owner.pgy`: each element owns
  its canonical row.
- `direct_mir_scalar_program_record_field_array_abi_owner.pgy` (new): when no
  MIR row carries the `Array<Int>` or `Array<Bool>` ABI and a record the
  program uses has a field of that type, the fact is built from the canonical
  row. A fact captured from rows, or refused, is kept unchanged.
  `Array<String>` is not filled: its producer returns the same empty fact for
  "no row" and for a refused row.
- `direct_mir_scalar_program_route_admission_owner.pgy`: the one-routine and
  multi-routine tails were the same code twice; they are one path now
  (353 to 295 lines, cap 300).
- Gate `tests/self_hosted/parity/direct_mir/direct_mir_record_field_array_abi_owner.sh`:
  three fixtures (an `Array<Int>` field alone, an `Array<Bool>` field alone,
  an `Array<Int>` field next to an `Array<Int>` parameter) through
  source-to-MIR and direct C and LLVM, each equal to native's output, the C
  leg under ASan; a malformed `Array<Int>` parameter row next to such a field
  is refused on both backends.

## Results

- Sweep of 2,746 programs against native:
  - Default LLVM: 828 to 855 equal, 0 wrong. 27 programs moved from refused
    to equal and nothing moved out of equal.
  - Four `array_field_detach_*_own_invalid` programs that native refuses
    now run on default LLVM. They already ran on default C. Their direct C
    output has no ASan error. The self-host front end admits an `own`
    Array argument that native refuses as aliased, a front-end difference.
  - Default C (MIR to AST round trip): unchanged, 1,418 equal.
  - `seal-record-collection-abi` refusals: 60 to 27. Of the 27, 23 have an
    `Array<String>` record field.
- New gate PASS with `driver_rf1`. With `driver_rl3` it fails at the first
  fixture: `seal-record-collection-abi`.
- All 128 scripts under `tests/self_hosted/parity/direct_mir/`: 54 pass and
  74 fail. Every failing one also fails with `driver_rl3`, and none of them
  is in a CI job; they are Makefile targets no CI step reaches. Every
  direct-MIR gate CI runs passes.
- Canonical DRV-2 producer: exit 0 on both drivers, MIR byte-equal (sha256
  `cf4711529ca7b01d`), peak 4.70 GB on both (test profile, about 1,500 s).
- Codegen bootstrap: gen2 == gen3 (125,935 lines).
- Component contract (line caps) PASS.

## Not covered

- The general route does not release a record local's Array fields: MIR has
  no drop row for the record. The C output of the three fixtures leaks the
  field storage (60 bytes for the first), as the base driver already did for
  a record it admitted (56 bytes for the third fixture); native frees it.
  This is the S2 record-field obligation in [blocker](../state/blocker.md).
  The gate runs ASan with leak detection off.
- `Array<String>` record fields (most of the remaining 27
  `seal-record-collection-abi` refusals).
