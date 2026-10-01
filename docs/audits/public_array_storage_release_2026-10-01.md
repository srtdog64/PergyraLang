# Public array storage release verification

Status: focused private and installed public pairs PASS (2026-10-01, Asia/Seoul).
Base HEAD: `aed8a904992465b14adf5a1b29d7fcd9789f0a7d`.
Pre-existing unrelated changes remain owned by their existing work. The final
ArrayDrop diagnostic oracle/receipt mapping handed off by main is included in
this release, not silently left as a dependency in the unrelated dirty tree.

## Claim and owners

`ArrayDrop` is a public consuming storage-only release, not an element
destructor. Source semantic admission owns exclusive named storage provenance
and plain-element lifetime. The direct-MIR consumer re-admits the corresponding
typed binding/CFG obligation at its untrusted input boundary. Backends only
materialize an admitted release. Existing expression IDs remain unchanged;
public storage release is ID 146 and `ArraySlice` is still 139.

The contract is `docs/semantics/array_storage_release_contract.md`. Supporting
Pergyra modules are listed in `src/self_hosted/OWNERS.md`; they do not introduce
a second fact-family registry or close the existing aggregate-formal seam.

## Observed evidence

- Native launcher: `.tmp/self_hosted/public-array-drop/native-bin/pgy.exe`,
  SHA-256 `A5DFE2CB3258216AB546EA39EEF65839D45100950D7DF7295D756DA9F72B3936`.
- Native public gate: `.tmp/self_hosted/public-array-drop.YqiRRD`, ten executed
  positives for each C/LLVM backend and 27 diagnostic-specific source refusals
  preserving a pre-existing output artifact.
- Final current-source private pair: `.tmp/self_hosted/public-array-drop.SSSvlx`.
  Default installed pair, with no binary overrides:
  `.tmp/self_hosted/public-array-drop.hkUjts`. Both all-stage runs returned 0:
  native C/LLVM ten positives each, public source-C ten and public LLVM eight,
  27 diagnostic-specific source refusals per lane, four issued MIR inputs
  executed per C/LLVM backend, and five MIR mutations per backend refused at
  storage-lifetime admission (`program_readiness=28`). Rejected outputs retained
  their prior content; the direct consumers did not mutate producer MIR.
- Public release owner inventory, fixed builtin effect registry, and existing
  semantic -> HIR -> MIR collection-ownership fact projection gates passed.
- SoT authority-edge gate passed: 95 authorities, 196 derived carriers,
  `CLOSED=69`, `BRIDGE=24`, `ACTIVE=2`. The single Gate SoT check passed.
- The large self-host component inventory was stopped beyond its static budget.
  Its component-checker, readonly-query, record-shape, constructor-order, and
  source-MIR action checkpoints passed; the whole script is not a green claim.
- The whole semantic-TU size gate failed on four unchanged owners:
  `collection_ownership_fact.c` (873),
  `type_checker_builtins_ownership_nominal.c` (611),
  `type_checker_call_generic_where.c` (655), and
  `type_checker_intent_step_sequence.c` (689), against the 599-line limit.
  Counts exclude comment-only lines. None is edited by this release.

The final driver was emitted through the existing Pergyra codegen seed, not by
native compilation of the driver source. Its installer prebuild receipt and
the source graph independently hashed before installation agree:
`e9523488a43186e46a3de343089673f4cdcebc45746638e93727599bdac64d23`.
Seed SHA-256:
`900518b8a2e21c8de32b4a9778fe32e4664b6d3a52cf4b66548a30ee6420a368`.
Build: `.tmp/self_hosted/public-array-drop/driver-binding-final-build`.
Private and installed driver SHA-256:
`63AF81BA7CB3E84FB0A36732DD8F2C7B0EB1F3DC5761B63EC84E24139BB3DB14`.
The installed native launcher matches the native SHA-256 above.

Existing native launcher, driver, and generated machine manifest were backed
up under `.tmp/self_hosted/public-array-drop/installed-backup-20261001T1251`
before replacement. The manifest hash remains
`0a83b0db5efe3c00c6d9413c63045c4b17aff079781213b280442c588e5a9c19`.
The existing receipt owner generated and validated
`.tmp/self_hosted/public-array-drop/root-installed-driver.receipt`; this is
artifact identity evidence, not fixed-point bootstrap evidence. Compiler builds
used the recorded shared tree, including preserved unrelated comments and cap
work; they are not described as clean-checkout or CI builds.

The emitted-driver compilation retains one discarded-const warning at the
existing `SemanticAstCollectionRequiredOwnedArgumentReady` MapHas boundary.
LLVM compilation reports the existing target-triple override warning. Neither
is silently counted as a warning-free build.

The native-built development probe of `codegen/main.pgy` admitted all ten
source-C controls and refused all 27 negative sources. It falsified and helped
repair three bugs: a later drop/own call overwriting the retirement map before
its argument leaf was checked, Typed-AST inout/ref mode confusion, and duplicate
descriptors in one own+own or own+ref call. The direct-MIR boundary also checks
same-call binding exclusivity. This probe is bootstrap development evidence,
not an installed-driver or self-host substitution claim.

A prior private driver (`7A5AB8D00E857DE0BC744FC8774636030E03786F481513950DD51EED473504FC`)
passed all public positive controls and the isolated MIR stage at
`.tmp/self_hosted/public-array-drop.tzrgro`: four issued inputs executed and all
five mutations refused with `program_readiness=28`, for both C and LLVM.
Its full source gate failed because the public JSON diagnostic mapping lacked
Slice invalidation, private-builtin and reserved-name identities; the new
element verdict also used an unregistered code. The reached receipt owner now
maps those existing identities, and the verdict reuses the existing arity/type
codes. A five-code native-built receipt probe emitted valid owned JSON.
This earlier binary is not the final source-current installation candidate.

## Verification boundaries

The public LLVM/direct-MIR lane does not acquire general logical-record local
`ArrayPush` support from storage release. The executable gate reports eight core
positives there, and ten in native C/LLVM and source-C, with the two populated
record cases attributed only to their actual consumers. The all-stage gate also
projects and executes four producer-issued MIR inputs per backend, then refuses
five lifetime/carriage mutations per backend without replacing prior artifacts.
The unnamed literal receiver is refused by the existing expression-shape
admission before the new lifetime verdict. Its inner
`array_drop_storage_requires_binding` diagnostic is checked; native/self-host
public diagnostic class/wording parity is not claimed for that frontier.

An earlier private driver passed own Int/Bool LLVM execution but falsified the
source checker on a plain index read and exposed a missing direct-C `string.h`
declaration. Those are implementation failures, not accepted exceptions; their
fixes passed the final current-source private and installed pairs.

## Language boundaries, not framework integration

- Borrowed `inout` handoff may rebind a descriptor or retain an alias. Without
  an explicit non-escape/rebind ownership carrier, later public storage release
  is refused. This is a general language rule, not a framework admission rule.
- Fresh general call-result arrays are also unproved; a declared `Array<T>`
  return type alone is not release authority.
- Automatic array scope cleanup, arbitrary resource elements, full backend
  matrices, fixed-point bootstrap, and closure of the 24 BRIDGE rows are not
  claimed by this release.

The user's explicit boundary is preserved: Alrescha is an independent framework,
not a compiler subsystem, dependency, specialized dispatch route, or completion
gate for this change. No external framework code is changed.

The regression boundary is the generic inout-rebind fixture followed by
`ArrayDrop`. Keep that boundary refused until a typed ownership fact can prove
otherwise; a convenient call signature alone cannot authorize storage release.
