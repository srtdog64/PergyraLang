# ArrayString layout consumer migration — 2026-10-01

Status: private and installed execution green; ABI row CLOSED; scoped delivery ready.
Base HEAD: `62a83a8256bf4fa34878cca8f1f14641736fdeba`.
The session started with 70 pre-existing dirty entries. Their unrelated work is
preserved; only the reached lexical-counter hookup is adopted as the explicit
integration dependency described below. Reports, this note, and the work
directive do not own semantics or registry status.

## Claim and owner

Stable family: `abi.mir_array_string_layout_projection`, unchanged identity.
`DirectMirArrayStorageLayoutContract` owns physical storage;
`DirectMirArrayStringCapturedAbiReady` admits the captured String ABI row.
The change removes remaining supported consumer-local layout reconstruction,
positional empty descriptor initialization, and LLVM storage/element alignment
guesses. C descriptor emission consumes named fields and publishes the owner's
six assertions; LLVM consumes a checked target projection. Slice's own two-field
view and other collection families retain their existing owners.

This is an owner-consumer migration, not a new C-owned compiler-path substitution,
stage-level dogfood, fixed point, target-profile expansion, or deep-lifetime proof.
Alrescha is not a compiler dependency. No TypeSafe network call is involved.

## Reached dependencies and exact refusals

The first private Pergyra-built candidate, SHA-256
`111f187631749e163103e6c1670a8e58446ed18cfa0529d6e715bb160b8ca762`,
built successfully from frozen source in
`.tmp/self_hosted/bridge-array-string-layout.HXnrKG`. It was not installed.
Both it and the old installed driver independently refused current foreach MIR:
the legacy envelope expected zero ownership facts, then rejected terminal
`AST_RETURN_VOID`. Seven-block dispatch incorrectly attributed the failure to
Option match. Both also refused `ToInt("42")`: the canonical signature is
`String -> Int`, but the bounded signature consumer expected `Unknown -> Int`.

The bounded corrections retain one existing fact owner at each boundary:

- Canonical `ToInt` signature is projected exactly; strict registry join remains.
- CFG-only borrowed admission consumes the validated routine index, stable
  declaration IDs and existing semantic origin identity. Every declared String
  array needs its unique live borrowed-literal row; unknown, owned, retired,
  missing or mismatched rows do not become borrowed by type or spelling.
- Program and legacy CFG share one terminal Void-return predicate. Operand,
  successor, uses and physical-ABI absence remain explicit obligations.
  Scalar capture intentionally omits dispatch metadata; source is carried from
  the admitted routine index, not reconstructed from that partial capture.
- The existing Option route requires a carried typed match claim, not block
  count alone. Its strict envelope, certificate, variants and ABI are unchanged.

The native producer's different origin/resource/loop/provenance carriage is
outside this bounded self-producer legacy restoration. No MIR rows are stripped,
no native retry is added, and the generic zero-ownership Option envelope remains
strict. Aggregate-formal String element lifetime remains ACTIVE elsewhere.

## Observed edit-loop evidence

- Static old-read/orphan ratchet and complete scalar-owner cap inventory passed.
- Canonical SoT edge and single-dashboard-owner gates passed at
  `CLOSED=69 BRIDGE=24 ACTIVE=2`; no row was promoted at this point.
- Frozen first-candidate positives: indexed/push/Int-push and independent
  owned/copyout/readonly/SliceCopy/Clone C/LLVM executions matched expected output.
- Reusing four issued MIR bases produced 41 coherent ABI mutations. First
  candidate refused all 82 C/LLVM projections, preserving prior output sentinels
  and original MIR hashes. Sixty refusals named ArrayString ABI, eighteen named
  statement admission, and copyout's missing/cross-family four named unsupported
  three-routine shape. Those four are fail-closed evidence, not ABI-specific
  diagnostic evidence. Negative artifacts were never compiled or run.
- Bootstrap development probe of the corrected current owners executed mixed
  foreach through C/LLVM (`60\nabbccc\n`), and refused fourteen borrowed identity/
  state and terminal-return mutations on both backends with sentinels preserved.
  Evidence: `.tmp/self_hosted/bridge-layout-dev-execution.0hhdCY`.
  Native source emission had zero errors and sixteen existing unreachable
  warnings. This is edit-loop evidence, not an installed self-host result.
- Independent borrowed-reader development probe observed 13/13 expectations,
  including canonically valid empty/mismatched-origin tables rejected by the
  bounded CFG owner. Independent ToInt probe accepted the exact String call and
  rejected wrong argument type/missing callee identity. Neither replaces the
  fresh-driver integration gate.

The fresh typed-source candidate build started at 15:29:44 KST in
`.tmp/self_hosted/bridge-array-string-closure.Bau8ob`, using the existing Pergyra
codegen seed `900518b8a2e21c8de32b4a9778fe32e4664b6d3a52cf4b66548a30ee6420a368`.
It finished at 15:39:29 KST, exit 0, driver SHA-256
`5ECCCFACE84A50F6BFFE3F7D748649B79AEF14A0051A8F3A3D2AB2096AF3FD25`.
Its source graph was
`a7453b1f478151834a59187e041513b78bcd9df9d661c77648762118587012b7`.
Private public ArrayDrop passed in `.tmp/self_hosted/public-array-drop.Iq2NbM`.
Private layout integration executed thirteen unique bases in both backends,
then failed to compile legacy pop C because `pgy_ai` was declared twice.
Evidence: `.tmp/self_hosted/array-string-layout-closure.DQKj7T`.
The candidate was not installed and no registry status changed.

Prefix-pop's existing sealed mode already joins its collection identity and ABI
to foreach storage; its storage declaration is deliberately absent in the
standalone ArrayInt path. The preamble now observes that same mode boundary.
No textual deduplication or typedef guard hides conflicting ABI facts.
The new static ratchet requires this boundary, and pop C must contain exactly
one Int descriptor size assertion.

An independent positive check of the preceding candidate executed LLVM pop and
general-route C/LLVM reverse (`bridge-layout-remaining-positives.DCKEYV`). A
native-built probe of the corrected legacy owner executed pop C/LLVM correctly
(`bridge-layout-remaining-positives.VmQbIY`), with exactly one Int descriptor.
That probe then forced reverse through legacy CFG, which is not reverse's
current production route, and the emitted C failed compilation; it was never
run. No unrelated reverse route repair is counted or attempted here. The fresh
production gate still requires reverse through its actual dispatcher route.
The native probe source compiled with zero errors and sixteen pre-existing
warnings; all 361 comment-excluding scalar owner caps passed.

The new mixed storage fixture first exercises String push/set/index/length,
then Int foreach. The preceding private candidate executes it through actual
legacy C/LLVM, with output `alice/BOB/carol/dave/BOB/4/60` in
`.tmp/self_hosted/bridge-layout-legacy-storage.rQCgFF`. It closes the historical
fixture-name attribution gap without another production implementation track.
The initial ordering (Int foreach before String while) was refused by the
existing String dominance boundary; no rejected artifact ran or admission was
relaxed. Both the supported source and its coherent ABI negatives are now in
the root integration gate. The compiler source freeze is unaffected by this
test-only addition.

The installed native and old driver remain unchanged until fresh private gates
pass. The one integration owner is
`tests/self_hosted/parity/array_string_layout_consumer_closure_owner.sh`:
sixteen unique producer bases, actual general/legacy route attribution, executed
C/LLVM, 102 coherent ABI and 28 reached-route refusals, unchanged input and
preserved rejected artifacts. Adjacent public ArrayDrop needs regression too.

## Final frozen and installed evidence

The final typed-source build in
`.tmp/self_hosted/bridge-array-string-final.2dsiJh` finished with exit 0.
Driver SHA-256:
`35145D3FB9F19DEBEB23F9028528A888FE540A39032F4F68749AFC0E43F8DF33`.
Its prebuild receipt and independently current source graph both report
`95e690deadf4ba13e62f19173d2afcbb73a990eedaedcf0aaf101fbd515f656c`.
Private and default-installed layout gates passed in
`.tmp/self_hosted/array-string-layout-closure.AOPL02` and
`.tmp/self_hosted/array-string-layout-closure.rtohvA`: sixteen unique bases,
thirteen general and three legacy routes, C/LLVM execution, 102 coherent ABI
refusals and 28 reached-route refusals with output sentinels and input hashes
preserved. The legacy storage fixture and pop markers prove their actual leaves
ran, not just a similarly named general-program case.

Public ArrayDrop passed on the private pair in
`.tmp/self_hosted/public-array-drop.IlOM21` and on the installed pair in
`.tmp/self_hosted/public-array-drop.4Fl1Fr`. The installed driver/manifest equal
the verified candidate; the existing native binary remains SHA-256
`A5DFE2CB3258216AB546EA39EEF65839D45100950D7DF7295D756DA9F72B3936`.
Prior driver/manifest are recoverable from the final candidate directory's
`installed-backup/`. The owner-written `installed.output.receipt` validates
artifact identity only; it is not a fixed-point receipt.

All thirteen distinct existing enforcement gates were executed from the stable
registry inventory. Eleven passed unchanged, including owner moves, callee-order
identity, owned return, multiple moves, complete terminal flow and repaired-digest
sealed flow. The latter two include supported consuming exits and missing/stale/
orphan/repeated/lazy/bypass refusals; layout negatives do not substitute for them.

Two stale test contracts were corrected and rerun green:

- String builtins now check the actual task-expression kind owner. The frozen
  producer SHA was updated only after the previous installed driver and the final
  candidate produced identical current MIR. Display-envelope mutation remains a
  byte-equal positive; graph operation text mutation is a separate refusal, as
  already specified by the graph admission owner and the adjacent case/math gate.
  No negative case was discarded or production admission relaxed.
- Readonly forwarding still requires exactly the original Array pointer operand.
  The String value may be its legitimate loaded SSA operand; the previous installed
  driver emitted that same forwarding shape. C/LLVM execution and all existing
  readonly ABI/carriage/resource/type negatives passed.

An independent final source review found no new blocker in the 28 physical
migration files and two orphan deletions, or in the nine reached admission/
dispatch owners plus prefix-pop boundary. Those are read-only findings; the
execution receipts above, not the reviews, own runtime evidence. No supported
lifetime claim, stable ABI owner identity, existing consumer or fallback obligation
is removed to obtain a closure count.

The final independent closure review rechecked all 2,441 source graph hashes
(zero mismatches), private/installed binary and manifest identity, both sets of
32 positive outputs and 130 refusal sentinels, and current static ratchet. It
found no remaining physical ABI blocker. The stable row is now CLOSED with all
29 original consumers retained plus 17 direct physical and four move/query/
cleanup consumers (50 total); all 21 original fallbacks remain with twelve new
physical old-read identities (33 total). The new integration gate is first and
all existing enforcement references remain. SoT edge, 95-row cap/96-row refusal
and the single-dashboard-owner gate passed at `CLOSED=70 BRIDGE=23 ACTIVE=2`.
The closure is physical ABI authority/consumer migration, not support for the
unsupported lifetime frontiers described above or C-owned substitution progress.

The previously existing lexical size counter is also connected to the two
reached component/builtin cap checks as a required integration dependency. New
owners are bounded by code size rather than comments; unrelated pending size-
metric consumers and compiler/runtime comments are not swept into this commit.

The final pre-commit lexical-counter verification executed 31 tests with exit 0
(`.tmp/self_hosted/source_size_count.69wjc4ap`). All 361 scalar-owner code-size
caps, the layout residue ratchet, shell/Python syntax checks and staged diff
whitespace checks also passed. The component inventory stages only its reached
counter integration and ABI inventory/cap changes; its six other pre-existing
hunks remain local. These focused results do not replace the omitted broader
gates listed below.

## Residue deletion and attribution correction

Two functions were definitions-only, with zero source calls and zero test-symbol
anchors in live imported modules: `DirectMirScalarCfgCollectionValueRow` and
`DirectMirScalarProgramArrayStringValueResultParameter`. Only those functions
were removed; their modules and the live per-parameter lookup remain. The static
ratchet rejects either definition returning. See the bounded orphan inventory.

The review fixture map was historical, not route authority: current indexed and
push fixtures actually use the general program route. The existing
`array_pop.pgy` / `one_mir_array_pop_artifact_contract.sh` does identify legacy
String pop (`%pgy.op.7.pop.length`); fresh execution, not the table label, must
prove that the migrated leaf ran.

## Broader omissions and separate blocker

The large component source inventory is not green. Bounded attempts exited 1
after early child checks without an owned failing assertion; the final observed
Makefile target exists in HEAD and the working tree, so it is not recorded as a
missing target. Four unchanged native semantic owners also still exceed the
comment-excluding 600-line cap. No full matrix, fixed point or CI pass is claimed.

The older string-array mutation gate exposed a separate existing issue: current
general C projection accepted a while initializer of `-1`. The old installed
driver also accepted the exact mutation. The accepted negative C artifact was
not executed. The old gate and its expected refusal are unchanged; this ABI
gate does not claim index-range or general C bounds safety. Keep that exact
falsifier for its reached expression/bounds owner rather than weakening it or
silently calling the whole compiler green.
