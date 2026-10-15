# V1 scope: ownership automation and self-host generality

Status: `ACTIVE` (2026-10-14)
Base revision: `c08dfc6e9` (origin/main)
Decision owner: repository owner

## Decision

V1 is two things, and nothing else:

1. **Ownership automation.** Ownership-based automatic memory management
   compiles the compiler's own canonical program with no manual drops.
2. **Self-host generality.** The installed self-host driver compiles every
   program the native compiler admits, on both C and LLVM.

After V1, ownership errors are found by using the language, not by building
more rules ahead of use. New keywords are out of scope. The language surface is
complete: all 145 registry words are parsed by both the native and the
self-host parser (`language_word_implementation_inventory.generated.md`).

## Finish line

All of the following hold on the same published commit.

1. **Bootstrap.**
   - The installed self-host driver builds from main.
   - gen2 == gen3.
   - Required compiler CI is green, with the bootstrap and installed-driver
     jobs actually executed.
2. **Ownership.** The ownership finish line (`AGENTS.md`, V1 Scope) holds:
   - zero ownership refusals on the fixed DRV-2 input;
   - the unchanged full canonical program is admitted;
   - positive and negative gates are preserved.

   Weakening a safety rule to reach zero does not count.
3. **Generality.**
   - The installed driver compiles the native-admitted corpus
     (`tests/cases/**`, `examples/**`) on C and LLVM, with runtime-equal output
     against the native oracle.
   - No program gets a `CODEGEN ERROR` route refusal.
   - Each backend has one general MIR-consuming emitter, organised by MIR
     instruction kind, expression node kind and ABI layout row. There is no
     per-program-shape admission.
   - The `direct_mir_*` shape owners and the MIR → AST text round trip are
     deleted as the general emitter replaces them.

SoT rows close only as these three steps pass through them. The row count is
not a goal. A `projection.direct_mir_*` row retires with its route, not by
being promoted to `CLOSED`.

## Where the self-host compiler stands (measured 2026-10-14, origin/main)

- **The public launcher routes every plain compile to the self-host driver.**
  This covers C and LLVM, binaries, `--emit-c` and `--emit-llvm`
  (`src/pgy_driver.c`, `src/compiler/driver_self_host_selection_owner.c`).
  The native pipeline runs only with an explicit `--native-pipeline`.
- **The driver is large, and mostly program-shape code.** The driver entry is
  `src/self_hosted/compiler/driver_bootstrap_main.pgy`. Its import closure is
  1,912 files and about 270,000 lines.
  - 1,004 of those files (about 97,000 lines) are `direct_mir_*`
    program-shape owners.
  - The general pipeline, `mir_lower` + `mir` + `codegen`, is about 68,000
    lines.
  - `semantic` is about 62,000 lines.
  - The shape owners are named by program shape, for example
    `direct_mir_scalar_program_bool_two_array_string_two_array_int_value_result_policy_owner.pgy`.
    Every newly admitted shape adds files, so the route grows with the number
    of type and position combinations and does not converge.
- **C path.** The steps are:
  1. `CompileSourceToCVerified` runs source → self-host semantic → MIR JSON.
  2. `CompileMirJsonTextToCForTargetVerifiedObserved` first tries the claimed
     shape emitters (`DriverRung2ScalarCSubstitutionIfClaimed`, then
     `DriverRung2NestedIntentCSubstitutionIfClaimed`).
  3. For any other program it rebuilds an AST tree text from MIR, re-runs
     semantic analysis on it, and emits C through the AST codegen.

  That round trip is the text re-parse that
  `docs/self_hosted/14_target_compiler_world.md` rejects as a handoff.

  **Ownership facts and the AST fallback.** For a program with no
  declarations, `driver_rung2_scalar_c_substitution_owner.pgy` forces the
  direct route whenever a routine carries collection ownership facts:
  rebuilding cleanup from the AST would throw the MIR fact away, and an
  unclaimed program ends in `Die`.

  A program that has any declaration skips the substitution entirely (`decls`
  non-empty, :17) and takes the AST fallback. The fallback drops the MIR
  ownership facts, because no file under `codegen/` reads
  `collection_ownership`. Cleanup then comes from the AST codegen's own
  reasoning, a second authority.

  Either way, the facts ownership automation produces reach C only through
  the shape list or not at all. This is where the two V1 goals meet. The
  detail is in
  [`selfhost_c_path_review_2026-10-14.md`](../audits/selfhost_c_path_review_2026-10-14.md).

  The rule for extending: when a program fails, add the missing MIR
  instruction kind, expression node kind or ABI row to the general emitter.
  Never admit the failing program's shape.
- **LLVM path.** `DriverRung2PublishMirPayloadLlvmArtifactForIdentity` calls
  only the direct backend. A program no shape claims ends in a typed
  `CodegenRouteRefusal` (`CODEGEN ERROR: ...`). Self-host LLVM is therefore
  limited to the claimed shapes.
- **The general emitter is smaller than the shape code it replaces.**
  - **The input mostly exists.** The self-host direct route reads only the
    admitted MIR JSON (`MirMachineLayerAdmittedJsonInput`). For the shapes it
    claims, that is enough: records, arrays, strings, options, enums and
    nested intents.

    The AST fallback's second semantic pass re-derives three inputs that the
    MIR JSON does not carry:
    - zone carriage: the self-host writer hardcodes
      `"resource_flow_symbol_count":0`;
    - DIR intent source facts for intents the MIR plan does not own;
    - declaration rows: rebuilt from analysis rather than read from MIR
      `decls`, which makes a duplicate authority.

    The MIR producer must emit these before the round trip can go. Of the 97k
    direct_mir lines, about 7k are reusable C emission and about 43k are pure
    shape admission (same audit).
  - **Still to measure.** Whether the JSON carries enough for the whole
    native-admitted corpus is measured on the installed driver, not assumed.
  - **The work is bounded.** MIR has 10 instruction kinds, the expression
    graph has 41 expression node kinds, and types arrive as ABI layout rows.
  - **A reference point.** The native oracle's MIR-level emitters are about
    22,000 lines: `transpiler_mir_*` about 13,200 and `llvm_mir_*` about
    8,900.
  - **Pieces to reuse.** The per-operation C/LLVM text inside the `direct_mir_*`
    owners, such as the comparison and integer-wrap projections, can be
    consolidated into per-kind emitters instead of being rewritten.
  - Native in-memory MIR still holds AST pointers (`expr0`, `expr1`, `ast`;
    `src/compiler/mir_types.h`). That limits the native oracle, not the JSON
    contract the self-host reads.
- **Bootstrap.** The bootstrap has been red since 2026-10-02 at the DRV-2
  frontier (`borrow_boundary_escape` / `unproved_exclusive_member_mutation`).
  No installed driver can be built from main, so no generality claim can be
  measured yet.

## Order

Steps 1 and 2 are P0 and P1 in
`compiler_closure_priorities_2026-10-14.md`, which owns their dependency order
and lanes. Generality comes after them, in the slot that document calls the
next production self-host substitution rung (P3). It is defined here as one
general emitter per backend, not as more admitted shapes.

1. **Bootstrap green.** Every later step needs an installed driver to measure
   against.
2. **Ownership finish line**, on the fixed DRV-2 input and the canonical
   program.
3. **Generality, C first.** The AST codegen stays as the reference while the
   general MIR → C emitter grows. Shape owners are deleted family by family
   once the general emitter covers their corpus, and the MIR → AST round trip
   is deleted last.
4. **Generality, LLVM.** Same method. Route refusals go to zero.

## Work split (owner decision, 2026-10-14)

P0 is active. The lanes, in order:

- **GPT lane: P0 implementation.** This lane continues the `focus17`
  candidate under `compiler_closure_priorities_2026-10-14.md`, which owns the
  dependency order, the lanes and the integration gate. Its uncommitted files
  in the shared tree belong to this lane.
- **Claude lane: investigation and verification.** This lane does not edit
  the P0 candidate's files. Its outputs are observations and reviews, not a
  second implementation:
  1. **Full failure census of the fixed DRV-2 input on main.** Instrument a
     scratch copy of the reached verdict owner so the bootstrap reports every
     ownership refusal, not only the first. Record each refusal with its node,
     code, boundary and owner. That gives P0 its real remaining set, replacing
     the historical 97 and the current first error.
  2. **Review of the candidate** as each coherent unit lands. Check it against
     the ownership contract (`docs/semantics/27_ownership_clean.md` §5) and
     the census, and add executable falsifiers where a refusal has none.
  3. **C-path review.** List what in the self-host C path must be fixed or
     refactored for V1 generality:
     - the claimed-shape emitters and their per-operation pieces;
     - the MIR → AST round trip;
     - the route that ownership facts force;
     - the AST codegen.

     Record which parts become the general MIR → C emitter, which are
     deleted, and in what order. This is a plan for after P0; no C-path edit
     starts while P0 is open.

## P0 takeover: copies added and manual drops removed (2026-10-15)

The GPT lane paused, and the owner asked the Claude lane to finish P0
itself (branch `claude/p0-v16`, worktree `pgy-wt/perf`). These are the
source changes outside the checker. AGENTS.md asks each copy or annotation
to name its reason, missing fact, last consumer and removal condition.

**Copies of borrowed text (`Concat("", x)`).** In each case, the String
element of a borrowed array leaves the call: it is stored in another array,
or it goes into a returned fact.

- Missing fact: a lifetime relation between the result and the array it
  borrows from. The formal-use owner keeps such escapes fail-closed "until
  the language has an owned transfer".
- Last consumer: the returned fact, or the destination rows.
- Removal condition: the checker can state "result borrows from argument"
  for a String element.
- Sites:
  - the direct-MIR builtin signature owners (collection clone, ToString,
    ArrayValue, HashMap receiver, record Option);
  - the slice builtin owner and the builtin call fact;
  - the expression admission parameter type;
  - the C and LLVM case-math operands;
  - the C record temporary search (`StringIndexOf` is not yet a registered
    borrowing builtin; the GPT lane's uncommitted registry adds it);
  - the canonical routine ids that zone authority transitions parse
    (`ToInt` is not a registered borrowing builtin either);
  - the payload-enum and direct-call LLVM type projections;
  - the for-each string elements;
  - the generic specialization actuals;
  - the CFG use replacement;
  - the identity-epoch member names;
  - the or-pattern alternatives.

**Manual drops removed.** V1 says no manual drops. The checker refused each
of these, because it cannot prove the dropped value owned.

- `DriverRung2MirProjectionRelease` and its six call sites.
- `MirJsonOwnedInputRelease` and the `retire_owned_source_json` flag. It
  released `admitted.routines.source_json` through a `ref`.
- The function-table release before C emission.
- Six `JsonOwnedFragmentWriteFile` calls whose actual may be borrowed or a
  literal. They now use `CompilerArtifactWrite`.
- Four one-element `ArrayDropOwnedStrings` boxes in the LLVM scalar emitter
  and the intent-execution JSON writer.
- `ArrayDrop(forwarding)` in the collection field-read owner, which the v16
  candidate added. The release verdict counts the `let` that reads an
  element (`ToString(forwarding[i])`) as a second descriptor store. The scan
  census hid this verdict until it reached zero.

Cost to measure before landing: the MIR JSON input now lives until C
emission returns. Measure it on the installed-driver `--mir-json` route
with `--observe-mir-consumer-stages`; the baseline peak was 1.78 GB.

**Real aliasing fixed in source.**

- The string-array plan lookup no longer names the rows it grows.
- The ArrayString and ArrayBool ABI candidates no longer hold the
  value-result rows.
- The C direct-call expression no longer pushes into a copy of its borrowed
  `arguments`.
- `MirExpressionGraphSequenceFromExtendedRows` takes the prefix columns it
  extends, not the whole sequence whose topology columns the caller has
  already copied and grown.
- The MIR expression graph builders gave the topology and the node texts one
  buffer. The iteration and match attaches push to each of them, so one
  buffer grew through two descriptors. The topology now keeps its own text
  rows.
- Three refusal and rebinding paths in the direct-MIR program admission
  returned or reread a descriptor that an inout or by-value growth had
  replaced: the statement admission, the definition route and the
  declaration rows.
- The MIR facts builder now attaches the semantic expression graph after
  the routine appends stop growing the facts. Before, every append carried
  a facts value that shared the analysis' graph.

**Second names removed.** `SelfMirIntentExecutionPlanAppend` bound
`let graphs = facts.cfg.instructions.expr0_graphs` and read it inside
pushes. The name was dead before the return, but the checker cannot follow
a holder into a push argument, so the returned `cfg` counted as shared and
every routine append of the facts refused. The function now reads the path
directly. No copy was added.

**Owned-string pushes into fields.** The v16 focus-recovery facts used
`ArrayPushOwnedString(facts.F, x)` four times. The native transpiler
accepts a field path there, but the self-host C emitter passes the array
`inout` and requires a variable, so gen0 died while emitting the canonical
program. They are now `ArrayPush(facts.F, Concat("", x))`, which makes the
same one copy the builtin made. Removal condition: the self-host emitter
takes an addressable field place for the owned-string builtins.

**Collection ownership rows across MIR.** The driver fixed point is the
first run that lowers the whole canonical program through the self-host MIR
producer since 10-01. That run found two contract gaps. Neither came from
this lane; both were hidden behind the gen1 refusal.

- **The MIR row vocabulary stopped at 09-30.** From 10-02 to 10-05 the
  semantic verdict added new states. They are formal and local string
  transfers, `Retired` ownership after an aggregate release, a named input
  or an owned-result return, owned elements after an owned push, and an
  unknown borrowed literal after an unknown call. The MIR producer check,
  the JSON names and the mir_lower reader each kept the old table, so the
  driver failed on 53 rows. The JSON names had also silently renamed the
  new values to `unknown`.
  - Now one table, `mir/collection_ownership_state_table_owner.pgy`, names
    the admitted (origin, ownership, disposition) triples for both sides of
    the JSON boundary. Its rows are the ones the verdict publishes on the
    canonical program.
  - Every older negative stays refused, including `empty_literal_owned`.
  - A value outside the vocabulary now has no name, so no row can carry
    it under another state's name.
  - The direct backends match states by string. They do not admit the new
    states yet; those programs end in a typed route refusal.
- **An indexed assignment's receipt had no instruction to attach to.** The
  semantic owner gives `a[i] = v` on an empty-literal `Array<String>` a
  shallow-mutation receipt keyed by its statement. MIR never recorded the
  source statement of an assignment instruction, so even a three-line
  program failed. The assignment instruction now carries it.
- **An owned call result named itself as its origin.** The reader proves a
  call-result row by its origin naming a routine that returns
  `Array<String>`. The verdict's owned-result activation switched the origin
  to call-result but kept the binding's own id, so the seed's reader refused
  eight canonical rows at `gen2_emit`. The activation now records the callee
  it already holds.
- **Still refused: an owned call result held live to the end of its scope.**
  The canonical program never holds one. Its owned results are retired by a
  drop or a return. Admitting the state needs the callee's owned-result
  summary in MIR. Without that summary, `call_result_owned_without_summary`
  would pass, so the state stays out of the table.

The full DRV-2 MIR is 374 MB. Its 136k instructions hold 980k expression
graph nodes, about 259 bytes per node in the JSON encoding. No graph repeats.
The native-built runtime's `ReadFile` stops at 64 MiB. The self-host-built
seed, which the bootstrap uses to read this file, does not. The cap and the
encoding are unchanged here.

## Measurements to track

- `direct_mir_*` file and line count inside the driver import closure. It
  should only fall.
- Native-admitted corpus programs that the installed driver compiles on C and
  LLVM with runtime-equal output, against the corpus size.
- `CODEGEN ERROR` route refusals over that corpus. Target 0.
- Ownership refusals on the fixed DRV-2 input. Target 0, with safety gates
  preserved.

  Baseline at `60b9d62bc`, from
  [`p0_ownership_refusal_census_2026-10-14.md`](../audits/p0_ownership_refusal_census_2026-10-14.md):
  - 9 on the gen1 input `codegen/main.pgy`;
  - 97 on the installed-driver input `compiler/driver_bootstrap_main.pgy`.

  All 97 come from the collection ownership scan, at 9 sites and with 13
  boundary kinds. These are upper bounds: later owners in the bundle assembly
  were not reached.

## Out of scope for V1

- New language words.
- Ownership precision beyond what the canonical program needs. Those cases are
  found in use after V1.
- Performance work, unless it blocks a step above (see `AGENTS.md`,
  Closure-Blocking Optimization Policy).
- SoT rows that none of the three steps passes through.
