# Generic receiver identity and native-oracle integration regressions

Base: `426cd694c03107d462f47f1050e9a5dab83e6278` (published main/origin/main).
Status: final combined Pergyra-seed candidate passes the bounded executable
and structural integration checks below; scoped publication is pending.

## Counterexamples and ownership

The unchanged inferred member fixture was refused as an invalid identity by
both the prior driver and the published ability-ID candidate. Its receiver
parameter has ten fields, including positive `source_syntax_id`. The common
`DirectMirGenericMemberReceiverParamReady` still required exactly nine. The
constructed member signature consumed that same stale validator.

This is not only a normal-input rejection: published candidate `7E702159`
accepts `receiver-id-missing.json`, exit 0, and emits C (`3b6006`). Removing
the required identity made the old schema appear valid. That artifact is
retained under `.tmp/self_hosted/driver/generic_receiver_identity_inferred_20260906/`
as `prior-receiver-id-missing.c`; it is not accepted evidence for the language.

The repair admits the current exact receiver schema and requires a positive
decimal ID. Both signature consumers use the existing
`MirRoutineHeaderParameterRowsReady` owner for parallel column lengths and
formal name/ID uniqueness, replacing their duplicated column-length checks.
The unused receiver ID is validated before the bounded signature/plan boundary;
it does not become a backend ABI choice or an extra runtime field. No old
nine-field compatibility path, native retry or alternate program-family retry
is retained. The common signature stays at 188 lines, below its unchanged 190 cap.

## Focused execution

Candidate: `.tmp/self_hosted/compiler/generic_receiver_identity_20260906/pgy-self-driver.exe`

- SHA-256: `B9EDBB8F71C41A1FA9C6049AF1373E26A4634224926BD41EA8EEE5400F12B350`.
- Pergyra source graph: `e554a3d9895c01d4d99814feb15f9f67a462256bc8700df89b32ddac18759cf0`.
- Existing Pergyra-seed build/source smoke: PASS (`fdd50b`), test C profile.
- Existing inferred gate: PASS, class output 41 and vessel output 42 through
  C/LLVM, 97 C negatives / 25 LLVM sentinels (`b48807`).
- Existing constructed gate: PASS, output 43 through C/LLVM, 48 C negatives /
  13 LLVM sentinels (`24eea1`).
- Each source MIR is produced once per gate and reused for its projections;
  native/self formal identities are compared through the canonical owner.

Eight receiver-ID mutations cover missing, zero, negative, fractional, string,
null, formal-ID collision and duplicate JSON key. All three host/program shapes
reject them through both backends, exit 1, no artifact, with the exact respective
`direct MIR inferred generic member identity is invalid` or
`direct MIR constructed generic member identity is invalid` diagnostic
(`363e88`). The final gate scripts also assert that exact diagnostic. Renumbering
the unused receiver to a fresh positive ID preserves both emitted artifacts.
These are bounded identity/erasure controls, not general receiver mutation proof.

## Exact published CI failure and repair

Regular CI `33994165910` completed FAILURE, 28/30 on exact base `426cd694`.
Watch `89852` completed exit 1 (`50b53d`); no watcher remains for this run.
There were two failures, not a general platform collapse:

- Linux push job `101381604556`: 22/23 push steps passed. The preparation
  contract failed because the generated language-word fixture inventory had
  not been regenerated after the new ability fixture (`80c133`).
- Linux bootstrap job `101381604622`: native oracle C emission rejected a
  borrowed aggregate alias in the new ability epoch consumer (`363e88`).

The full driver native C emission reproduced that first borrow error locally
and 26 subsequent TextBuilder errors, exit 1, no C artifact (`1dfd3d`).
`MirExpressionIdentityEpochAppendDeclarationMethods` now reads the existing
`routines.declarations` view in place instead of copying it into a local
`declarations` binding. With only this change, the same complete driver native
C emission passed, zero errors / four existing intent-clause warnings
(`52a9ca`). All 26 TextBuilder errors disappeared; they were cascading evidence
in this reproduction, not 26 independently repaired owner leaks.

The existing registry generator regenerated
`docs/semantics/language_word_implementation_inventory.generated.md`.
Only twelve fixture-count cells changed. The keyword registry, generated
Pergyra projection and editor grammar are unchanged. The actual keyword gate
passed: 146 rows, 70 reserved lexer rows, 76 parser selectors (`7fa235`). This
does not adopt keyword removals or change declared support.

## Prevention and limits

The native reference checker must remain enabled. A Pergyra-seed build plus
small source/MIR fixtures did not validate the complete compiler source through
that checker. The existing complete driver native-oracle admission is therefore
a required local integration check for this borrowed-view change before push.
Do not patch TextBuilder consumers or suppress their errors to hide an earlier
invalid binding. After fixture changes, run the keyword registry generator's
check and regenerate owned projections when it reports drift; do not edit a
generated count by hand.

Intermediate combined candidate `55818B2960016B26F5C9A311109723E0D390494621926B47B2630BD224AF460F`
built through the Pergyra seed (`2c6c92`), graph
`429cb74419f3d01c682127e2c6d1d914931d63ff3c3e688c667f5c70e891a30b`.
Native oracle generated C also compiled successfully (`9afe79`). Final inferred
and constructed C/LLVM gates passed (`75204f`, `424af6`), including all exact
identity refusals and erasure controls. The seven-program hard parent passed
(`84c1b2`), with the ability's nine refusals/three runtime controls (`cb6be5`);
canonical and generic specialization identity epochs passed (`6b056c`).

Test-only deduplication fits the existing caps: inferred shell 200/200, inferred
mutations 358/360 and constructed mutations 332/350. The shared parameter-
identity comparison/mutation module is 51 lines with an 80-line structural cap.
The full generated input sets before/after contain the same 244 paths and hashes
(`0b7a41`). Six controls against the actual comparison entrypoints reject changed
IDs, missing IDs and duplicate routines (`eee805`). No fixture or refusal was
removed. SoT/protocol/keyword checks passed (`48d2e7`, `11e722`); only the SoT
gate's descriptive assertion anchor changed, not its owner or status.

The publication-boundary full component inventory then failed (`aee960`) on
the compile-time declaration/literal Log family: 565 LOC against its existing
560 cap. Those three source files matched published HEAD at 265, 220 and 80
lines (`1b05aa`). The bounded follow-up removes duplicated formatted C call
construction in `DirectMirLiteralLogEmitC`: integer temporary/argument spelling
remains variant-specific; the admitted symbol and format have one common
emission path. No admission, ABI fact, LLVM behavior or cap changed. The family
is now 265 + 220 + 75 = 560 lines (`355706`).

The existing projection gate now executes a string literal as well as integers.
Before/after candidates both pass all 28 negatives and exact 7 / 73 / ready
runtime controls (`e772e8`, `86703c`). All twelve emitted C/LLVM artifacts match
byte-for-byte (`b200c2`). The next complete inventory reached its final line
batch, exposing the published MIR producer harness at 321/320 lines (`c8fae2`).
Its ability-test call is now one line like adjacent calls; function, arguments,
order and test coverage are unchanged. A subsequent batch stopped at the role
mutation generator's 108/100 cap (`c080cd`). Compact equivalent dictionary
updates fit 100/100 and preserve all 17 generated inputs byte-for-byte
(`ddf718`); prior gate evidence is preserved in
`.tmp/self_hosted/driver/role_mutation_size_20260906/prior-gate/` (190 files).

The repeated full scans exposed `run_line_cap_checks` stopping at the first
failure. It now collects independent unreadable/oversized-file diagnostics and
fails the batch at the end; invalid request construction still stops immediately.
An actual mixed batch of two oversized files, one missing input and one passing
file failed to expose the later errors before the repair (`4cbdef`). The same
control and all prior checker mechanics pass afterward (`938dfd`). No missing
input or violation becomes success. The complete inventory rerun passed
(`f06a35`), including all 2,332 requested line caps. This inventory is structural
evidence only; the executable gates below own behavioral claims.

Final candidate:
`.tmp/self_hosted/compiler/receiver_literal_integration_20260906/pgy-self-driver.exe`.
SHA-256 `FC669FD242A7FE400BE0450C33BC991DD78B807CB2D949F4FC70D386B58F039E`,
Pergyra source graph
`d1571d6723654bbf4ee1f3fa3ddf12838a775eba89f732e9abd69163b84a8e98` (`2d85d0`).
The Pergyra-seed build/source smoke passed (`575a9d`), as did complete native
oracle C emission/compilation, zero errors/four existing warnings (`6fd83b`).
Final inferred class/vessel and constructed C/LLVM gates passed (`5d6465`,
`7618b5`); canonical/generic identity epochs passed (`263e86`, `1f9300`).
The seven-program hard parent also passed (`1ab2ce`), including nine ability
refusals and three controls (`b2bbce`). Isolated installed-layout role parity
passed all its C/LLVM/source/MIR runtimes and negatives (`30255b`). Its launcher
copy matches native SHA-256 `30F4130B` and sits beside final driver `FC669FD2`;
the ordinary public path selects that sibling without a driver environment override.
SoT/protocol/keyword and CI-profile gates passed (`44232b`, `573bb6`). The three
shared installed binaries retain their original hashes (`b200c2`).
Documentation passed (`521164`); exact 23-path scope, strict UTF-8, whitespace
and the twelve generated fixture-only count changes passed (`5146a1`).
No shared binary installation, current-source gen2/gen3 fixed point, remote CI
green, new SoT status or hard-substitution percentage is claimed. Original
ability MIR, earlier binaries and the enum same-name/guarded-return findings
remain preserved.
