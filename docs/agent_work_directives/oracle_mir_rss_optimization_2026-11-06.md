# Oracle MIR producer RSS closure

Status: measured producer investigation; this lane owns only the reached
producer optimization. Main owns carrier-restoration consumption and release.

## Objective and fixed boundary

- Objective: issue the complete canonical MIR within the existing 16 GB CI
  memory boundary, with byte-identical output and preserved safety/refusals.
- Priority: exact semantic input/output and ownership safety, existing owner,
  removal of repeated allocation/copy, then wall time and patch size.
- Entrypoint: native-produced `driver_oracle.exe --emit-mir-json-verified
  src/self_hosted/compiler/driver/driver_bootstrap_main.pgy
  --pressure-owned-full-fixpoint -o ...`.
- Original observation: carrier47 `full_mir_oracle.time`, 21,250,796 KiB RSS,
  exit 0, 225.98 s. The unit is KiB, not a decimal GB measurement.
- Executable owner: `/root/pgy-v001-overlay.ml3fkD/.tmp/v001-driver-carrier47/`.
  Oracle SHA `f2296a5a5e1d0c29ac0251c763f29eb28f1eaf9ab0f48157ddc522874a9d71db`;
  seed SHA `471011a142849749515a9fea5a15b7c43c0687c0050c6fc72069a03dbf742147`.
- Original full MIR: 389,800,975 bytes, both producers SHA
  `5e59c2e1f521f2a21b7bca2e7aa2e9b19f1fed49d5475258949bdd75d174ca8c`.
  Seed peak was 7,725,468 KiB on the same input.
- Runtime attribution separates allocation traffic from retained heap. The
  bounded run's measurements and hashes are recorded in its frozen run folder;
  static clone occurrence counts alone do not establish the bottleneck.
- Last legitimate consumer: complete MIR JSON publication, followed by gen2
  canonical consumption. The producer must not change that MIR contract.
- Forbidden fallback: smaller input, larger memory/time allowance, erased
  provenance/safety checks, guessed ownership, per-program admission, manual
  dropping canonical user values or hand-patched generated C as the solution.

## Whole chain and bounded investigation

Production request -> loaded compiler modules -> AST/semantic facts ->
routine/domain/MIR accumulation -> complete program facts -> JSON projection
and owned text publication -> output file. The native source compiler decides
move/copy/cleanup used by the oracle executable; the oracle runtime performs
those allocations. Seed and oracle are separate binaries, not interchangeable
evidence. The current native compiler's own RSS is a different measurement.

1. Freeze a complete source/input copy, original oracle/seed/generated C and
   their hashes outside main's active build tree.
2. Observe generated allocation call sites with a bounded fixed-size counter
   table linked to a scratch build. It changes no program decision or returned
   allocation, emits only aggregate telemetry, and must reproduce full MIR.
3. Attribute observed bytes to reached owner operations, including their
   callers and lifetime. Select the one actual blocking operation before
   changing production code; update this card with the full repair scope.
4. Run focused behavior and a falsifier for that operation before regenerating
   the whole candidate. No parallel producer implementation or cache project.
5. Re-run the same complete input with identified candidate executable, compare
   exact MIR, record peak RSS/time, and run reached parity/refusal gates.

Edit-loop budgets remain 60 s static / 5 min focused / 30 min integration.
Main's ongoing gen2/gen3 replay is read-only evidence and is not overwritten.

## Selected complete repair boundary

The full stack census identifies two repeated storage-carriage operations:
MIR row mutation/reconstruction and statement transition receipt carriage.
This is a producer operation repair, not relaxed automatic-copy admission.

| Reached operation | Observed reason | Required owner/consumer change |
|---|---|---|
| CFG add/block/successor/graph/match mutation | rebuilding unchanged sibling String arrays | mutate the existing CFG, return that same owner |
| LocalRef row append/primary/graph attachment/append rows | rebuilding earlier String rows | update the existing row owner; retain explicit copies of borrowed Strings |
| Routine add/primary/secondary graph orchestration; local add/scope truncation/termination | reconstructing untouched local String inventories | restore the changed CFG field or mutate the same inventory and return that build |
| Runtime value ABI attachment | reconstructing unrelated instruction and ABI String inventories | update only the admitted nested runtime-value row owner |
| Semantic statement transition | complete seven-column receipt snapshot for each statement | scan keeps one table, transition mutates it inout, result carries only its existing diagnostic contract |

Dependency order: row owners -> routine consumer; transition result/table
contract -> its sole scan consumer. Each error keeps its diagnostic identity;
the scan aborts on failure and never publishes a partial receipt table.

The census also reaches immutable artifact/graph snapshots and final receipt
publication. They are single whole-input projections rather than per-row
rebuilds; this repair does not remove their independent ownership. Runtime
region concatenation and genuine retained String copies are not array-clone
elision targets. Preserve the exclusive-value and ArrayDrop checks unchanged.

Gates: focused imported-owner row content and allocation scaling; machine
projection output/mutation refusals; existing collection copy-field and
receipt falsifiers; complete oracle regeneration by the frozen native emitter;
same original full input with byte-identical MIR and measured peak RSS. An
unchanged safety gate refusing a formerly unsafe alias must still refuse it.
The frozen source includes all existing dirty implementation. Integration
uses no hand-edited generated C and no substitution of the lower-memory seed.

## Retained input and process isolation obligations

The whole-input gate falsified direct mutation without an independent incoming
String store. The previous whole-array copies also detached those Strings from
the borrowed routine input. Preserve that boundary with one owned String copy
per new stored value, including local inventories and runtime-value ABI rows;
never re-copy the preceding rows. A borrowed-input fixture retires its source
Strings before reading the retained row under C/LLVM AddressSanitizer.

The fixed edit boundary is eight existing compiler owners. It adds one private
`inout` receipt-table parameter: the scan owns the descriptor and the transition
really appends to it, so returning a decision must not move/copy that table.
Five machine-projection and seven runtime-value ABI String stores use explicit
`Clone`; the other retained String pushes use the existing owned-copy builtin.
Their authority is borrowed input -> independently retained MIR rows, whose
last consumers are MIR verification/JSON/backend publication. These copies
remain while those lifetimes overlap; remove one only after proving a transfer
that ends the source lifetime. No annotation is added to canonical user input.

The user also requested bulkheads. The existing fixed-point profile execution
owner is the boundary: independent seed/oracle processes, output lifetimes and
process groups; one Linux phase gets 14 GiB virtual-address space and the
existing 30-minute integration ceiling. The 16 GiB CI runner keeps 2 GiB for
the harness/OS. GNU time remains the resource-measurement owner; the guard owns
admission and cancellation, not compiler semantics. Timeout, cancellation and
orphan children must terminate/drain only that phase. A failed bounded run is
never retried uncapped or recorded as a successful artifact. Test allocation
failure, timeout with a grandchild, root exit with a child, interruption and an
unaffected sibling. CI enables this boundary explicitly; Windows keeps its
existing pressure owner. Internal compiler stages may not be separated by
guessing the lifetime of shared typed facts or by reducing the semantic input.
