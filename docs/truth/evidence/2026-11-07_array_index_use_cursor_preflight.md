# Array indexed-assignment use cursor preflight

Observed in the raw owner/View lane on 2026-11-07. This is intermediate
evidence, not installed-driver cleanup or performance closure.

Main's seed executable was copied read-only into
`/root/pgy-owner-production-20261107/r9-retirement/main-seed-mir/driver_seed.pinned`.
SHA-256: `82d5a060250bb44c9da0ebbcc36d2a8091dd3407adffce3eec1fb1a32af58223`.
All eight original workload source hashes were recorded in `receipt.json` in
that directory. The seed emitted MIR for five; fixed_view, dijkstra and astar
retained the three known source type refusals because this seed predates the
already-delivered Slice consumer patch.

The five emitted MIR inputs all exposed indexed-assignment failures in the
isolated general emitter. `IndexedAssignmentValueUseOffset` reset the RHS
cursor for nonliteral expressions; a literal index also left the validated
receiver use unconsumed. The candidate deletes this expression-shape reset
and consumes the admitted target/index prefix. Existing unique prior-use and
latest-dominating-value checks remain authoritative.

The native-built admission probe after that change has SHA-256
`73fac40b8cfe4efe66c6400451ce93139557377018092726f770b903c44e5cd7`;
its retained path is `r9-retirement/probe-cursor-pinned` under the base above.
The build completed in 264.899 s with 0 errors and 16 existing warnings.
`cursor-eight-admission.json` records acceptance of all five available MIRs.
The original local indexed-assignment mutation owner produced ten mutations;
all ten remained refused. `controls-after-cursor.json` also records acceptance
of the existing positive and fresh/reused/literal/dynamic RHS control.

Foreign-call physical ABI projection was separately compiled and executed for
negative Int, Long, Bool, three arguments and Void: ten C/LLVM executions passed.
Receipts and emitted sources are in `r9-retirement/foreign-execution/`.
Five foreign identity/parameter/ABI/symbol contradictions were refused. The
initial return-type mutation inside an unconstrained Log call was a valid
different MIR program, not a sound falsifier. A typed Long local supplied the
missing constraint; that unchanged positive passed and its Int-return mutation
was refused (`foreign-return-constraint-control.json`).

Automatic retirement was still selected zero times for the five workloads:
their loop phis were incorrectly counted as storage creation/replacement.
The next isolated candidate consumes the existing phi owner's same-LocalRef
proof. Its full source manifest and builder identity are fixed under
`r10-integration/`; the existing `inout_array_storage_mir_contract.sh` is running
in candidate-r10 with the repository's 30-minute integration budget.
This does not turn earlier separate 300-second projection preparation timeouts
into green results. The phi change, actual cleanup, unchanged eight-input
installed C/LLVM matrix and the 1.3x threshold remain unverified here.

Shared promotion still contains only the previously verified Array/Slice lane
patch. Foreign-call, cursor, retirement and native build-cost changes remain
isolated. No staging, commit or push was performed by this lane.
