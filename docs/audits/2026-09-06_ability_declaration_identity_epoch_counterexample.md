# Ability declaration identity lost at MIR reconstruction

Status: REPAIRED in the isolated 2026-09-06 candidate; publication, shared
installation and full-bootstrap evidence are separate. The original refusal
below remains historical evidence, not the current result.

Observed base: `da818c3df133c23572486872e4114d594f981890`.
This audit is evidence/navigation, not a semantic owner or completion claim.

## Executed repair on base 16b2f894

The semantic signature ID now survives MIR method rows and both serializers.
Canonical rebinding admits declaration-only ability/extern callables; runtime
declarations must agree with their routine ID at the common routine-index
boundary, including direct-MIR routes. Strict method schemas were migrated;
compile-time erasure checks and hashes the signature identity. No target-ID
zeroing, implementation substitution, numeric offset, or name fallback exists.
Raw generic `T` stays raw; its default remains owned by the signature facts.

Current isolated pair:

- `.tmp/self_hosted/compiler/ability_identity_final_20260906/pgy-self-driver.exe`
  SHA-256 `7E702159EC2BA7CF182B521370099782BA936396AF1EA1A2A1DA26520F13D92F`;
- source graph `485cef989a5226acad669b5e318730dbd59f7a075a8ffe1366d48dfe32ea555c`;
- sibling native `pgy.exe` SHA-256
  `30F4130B8856B1EBEB54870619BB1BECEED4AD0B8F959CF5145EEC1765FA03C8`.

Pergyra-seed build and source smoke passed (`c717f0`). The existing hard parent
passed seven selected source/MIR programs (`1022ce`), including unchanged
ability execution 12, nine exact ID refusals and three runtime controls.
The controls cover declaration order, coherent ID rekeying, and same-named
methods on distinct abilities (12/16). Three runtime-ID mutations now refuse
at common machine admission, while six declaration/call mutations refuse at
canonical graph admission; the gate requires the exact applicable diagnostic,
exit 1 and no successful MIR payload.

Further observed gates:

- canonical identity and generic specialization epoch: PASS (`ffcf9e`);
- isolated installed-layout role override: AST/MIR parity, C/LLVM execution,
  three permutations, 14 MIR negatives and source/receiver checks PASS
  (`9d397e`); shared `bin/` was not changed;
- declaration erasure: C/LLVM output 7/73 and 28 negatives PASS (`b736b5`);
- role operator: C/LLVM output 123, six metamorphic controls and 30 negatives,
  plus receiver canonicalization PASS (`b736b5`);
- native MIR unit executable: 162 passed, zero failed (`c00522`). This is the
  unit binary, not every additional smoke attached to `make test-mir`.

The original failing MIR hash remains
`4D3272D17090008C8AB6B354315372B13C6FB2EEDF09F39CAD2FB366A5969602`.
Initial build setup failures (mixed absolute path at artifact `begin-temp`,
then an unconfigured isolated launcher) were not semantic negatives. The
builder subsequently used the same in-repo target through its existing relative
path flow; no IO policy override or shared installation was introduced.

## Broader findings not repaired by this slice

The inferred and constructed generic-member direct-MIR gates remain RED at
`direct MIR ... generic member identity is invalid`. Current failures are
`b7366b` / `76eb8a`; the unchanged pre-repair driver `1AAFC7D7` reproduces both
on the same source programs (`b7b5a3`, `1efd4a`). Original-producer MIR is kept
under `.tmp/self_hosted/driver/ability_identity_prior_inferred/`; current inputs
are under `ability_identity_final_inferred/` and
`ability_identity_final_constructed/` in the same driver workspace root.
These are valid-input bugs, not successful negative tests or proof of full
consumer correctness.
The full parsed old/current carriers compare equal after removing only the
new method-ID column in memory (`518c6a`); no source, stored MIR or compiler
input was changed by that comparison.

Their earlier comparison also equated raw formal IDs from different producer
arenas (self 8/9 versus native 33/34). The existing canonical owner produces
equal complete parameter rows (`2da32e`). The comparison now consumes those
admitted rows without deleting identity keys; raw input still feeds direct
backend execution. That checker repair does not conceal the subsequent real
direct-MIR refusal.
In-memory mechanics (`912aca`) separately changed a canonical formal ID in
each comparator's actual input; both raised the exact parameter-identity
diagnostic. These are comparator controls, not compiler execution evidence.

Regular CI `33987047080` (`91119c45`) and `33989459465` (`16b2f894`) both
completed 30/30 SUCCESS before this local source repair. Platform full remains
the older 10/13 FAILURE; no current-source fixed point or full-matrix green is
claimed. This is an executable regression repair, not new hard-substitution
credit or a SoT registry status promotion.

## Executed evidence

The existing fixture is
`tests/cases/backend_compare/generic_default_ability_bind_dispatch/main.pgy`.
It declares `Bufferable<T=Int>.Put`, implements it through `IntBuffer`, binds
`storage.buffer`, and calls `storage.buffer.Put(7)`. The native C oracle in the
existing driver parent compiles and executes exactly 12.

Both published Platform driver jobs stopped earlier at a stale test expectation:
Linux `101342708201` and Windows `101343906110` expected a free-call diagnostic.
The actual mutated-source refusal is exact exit 1 with
`member_call_arg_type_mismatch`, func `storage.buffer.Put`, expected Int and
actual String. `ast_expression_graph_concrete_scalar_verdict_owner.pgy` owns
that distinction. The corrected child passed locally (`994181`).

The same unchanged Pergyra-built driver was used throughout:

- `.tmp/self_hosted/compiler/enum_local_admission_20260905/pgy-self-driver.exe`
- SHA-256 `1AAFC7D7F031424B75F7BE1737BA088B501A021154204864B1EB286B76642BA6`
- Source graph `6bbeb7206ad331585d44fdc62b9be72a629a3a3ef3165cc2f9cd382a83659f2c`

After the diagnostic correction, the one-fixture parent (`5107`) rejects its
own MIR during `--canonicalize-mir-json`, exit 1:
`CODEGEN ERROR: MIR instruction expression graph is missing or invalid`.
The parent is RED, not complete parity evidence. A separate unmutated source
`--emit-c-verified` route (`493594`) reaches the same refusal; no generated C
program was compiled or executed on that route.

Retained evidence directory:
`.tmp/self_hosted/driver/ability_diagnostic_20260906/`.
The input `generic_default_ability_bind_dispatch_hard.self.mir.json` has SHA-256
`4D3272D17090008C8AB6B354315372B13C6FB2EEDF09F39CAD2FB366A5969602`.
Canonical refusal, valid-source refusal, mutated diagnostic, native MIR and
native output remain beside it. Neither the input nor the driver was modified.

## Source inspection and missing join

The self MIR call node carries `call_target_kind=member`,
`call_target_name=Bufferable_Put`, and producer `call_target_syntax_id=5`.
Its declaration contains the `Bufferable` method name/signature but no method
`source_syntax_id`. The actual routine inventory contains `IntBuffer.Put` at
ID 12 and `Main` at ID 22; the role method's formal IDs are 14 and 15.

`src/self_hosted/mir/declaration_rows_owner.pgy` already receives the ability
method's semantic node ID while emitting its declaration rows, but retains
names, returns, parameters and contracts without that callable identity.
`src/self_hosted/mir_lower/expression_identity_epoch_owner.pgy` builds its exact
producer/canonical map from routines and their formal parameters. The declared
ability method has no runtime routine, so ID 5 has no entry. Its graph rebind
correctly refuses an unknown positive ID. Canonicalization and general MIR-C
both consume that rebind through `expression_graph_fact_owner.pgy`.

This is the source-level explanation for the observed failure; no instrumented
epoch trace or repaired compiler execution is claimed. The named declaration
owner, not a textual call-name guess, must supply the missing identity.

Independent read-only review confirmed two additional constraints:

- `SelfMirDeclarationRows` in `mir/declaration_fact_owner.pgy` has no method-ID
  column. This needs producer carriage, not just another lookup-table entry.
  `IntBuffer.Put` appears in both declarations and routines; reconcile its exact
  identity and register it once. Appending every declaration and routine would
  conflict with the current epoch's one-to-one duplicate rejection.
- The declaration preserves raw parameter `T`; `semantic/ast_signature_fact_owner.pgy`
  resolves its default through `SemanticAstGenericDefaultTypeForName`. The
  identity join must not equate raw `T` with `Int` or rewrite the wire signature
  to conceal this distinction. Preserve module/owner/method attribution and
  let the existing type owner govern that relationship.

## Required continuation at the original observation

Keep one integration rung. Preserve declaration-only callable identity from
the existing semantic signature owner through MIR declaration carriage and the
canonical epoch join. The last consumers are verified canonical MIR and the
general MIR-C graph admission boundary. Do not erase the target ID, map it to
the role implementation, infer numeric offsets, or reparse an AST/name as a
fallback. Keep the existing missing-ID rejection.

Verification must first make this unchanged producer-first parent execute 12,
then reject missing, unknown, duplicate and wrong-owner declaration/call IDs
without an artifact. Include same-named methods on different abilities and
declaration reorder controls so one lexical name cannot become identity.
An exact-revision compiler build is required; the current local candidate does
not contain a fix. Large local rebuilds await the pending storage choice.

No compiler source, SoT registry status, hard-substitution percentage, or
installed binary changed in this audit.
