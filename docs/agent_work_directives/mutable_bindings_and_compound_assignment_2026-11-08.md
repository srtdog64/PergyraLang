# Mutable bindings and arithmetic compound assignment

User-authorized extension, 2026-11-08. This lane is independent of the main
lane's frozen V.001 candidate; it does not replace that candidate or its evidence.

## Objective and closure boundary

- Objective: `var` and `let mut` declare the same mutable binding; `let`
  rejects rebinding and writes through its value. Numeric `+=`, `-=`, `*=`, `/=`, `%=` use ordinary checked
  arithmetic and assignment authority. Existing event `+=`/`-=` survive.
- Priority: binding identity, write authority and ownership; single evaluation;
  native/self-host parity; migration; convenience.
- Fact owner: parser declaration mutability -> native Symbol / typed AST arena.
  Assignment admission consumes that exact declaration. Compound operators are
  syntax facts, classified by semantic target type before lowering.
- Last consumers: ordinary MIR assignment/arithmetic and general C/LLVM emitters.
- Forbidden fallback: treating every local as mutable, name-only migration,
  duplicate evaluation of a compound lvalue, unchecked arithmetic, a backend
  guessing source spelling, or weakening a borrow/field/ownership refusal.
- Gates: parser and semantic positive/negative cases, same executable C/LLVM
  executions, self-host parser/semantic and installed-route evidence, migrated
  compiler input. Record exact source/binary identity and omissions separately.
- Falsifiers: direct/conditional/loop/deferred writes to `let`; writable aliases
  passed to a rebinding boundary; immutable fields and read-only borrows;
  effectful compound indices/receivers/RHS; wrong arithmetic types; missing
  typed facts; old event handlers; unchanged ownership refusals.

## Whole-chain plan

1. Native parser already produces `is_mutable`; local binding admission copies
   it to Symbol. Assignment currently never consumes it. Self-host parser
   projects mutable locals as `Let: mut`, typed AST carries mode 0/1, while
   assignment environment currently erases that distinction as `local`.
2. Admit the alias in both parsers without a second semantic representation.
   Preserve contextual `mut`, immutable destructuring and other retired forms.
3. Consume the existing declaration fact at binding-write admission, separately
   from field/element/payload mutation. Audit inout, capture, implicit fields,
   defer and loop paths for missing write authority. Keep parameter modes and
   object/slot/collection authority owned by their existing contracts.
4. Carry compound operator identity through the ordinary assignment chain;
   select the place and load its prior value once, evaluate the RHS once,
   apply existing numeric rules, and store once. Event type classification
   remains semantic, never a name-based backend route.
5. Migrate actual reassigned source declarations to `var`, preserving immutable
   bindings, comments, strings, shadow identities and intentional refusals.
   Keep a declaration-level migration manifest; no global let replacement.
6. Run focused gates before the integration boundary, then repeat an adjacent
   bug audit requested by the user. Include branch/backedge, alias/capture,
   missing-fact, evaluation-order, arithmetic and event negatives.

## Evidence baseline and open obligations

Native `4266252b2562474712a7d462639ccd813b658c4dab6e4ded9edb664ff55535b0`
compiled `let x: Int = 1; x = 2; Log(x);` on C and printed `2`.
The user explicitly authorized fixing this and migrating existing code.
The operator is owned by AST assignment, projected as typed AST mode 0..5 and
semantic assignment `operations`, then carried by MIR instruction `name`
(`assign-add`, `assign-subtract`, `assign-multiply`, `assign-divide`,
`assign-remainder`). Ordinary assignment keeps `assign` / `ssa-def`.
General statement admission must reject unknown operation names; C and LLVM
consume that fact, never parse source text. A compound place is selected and
read before evaluating its RHS. SSA use order follows the same ordering.

Native scalar, field, loop, indexed single-evaluation, nested-assignment and
event execution passed on C and LLVM in the first compound snapshot. Self-host
operation transport and general MIR consumer changes remain open. Existing
event safety checks still own event callback admission. Explicit inout requires
a mutable binding. On 2026-11-08 the user confirmed the existing value-immutability
contract: collection/field writes also require a mutable value root. Identity
hosts and resource handle operations retain their existing authority contracts;
retiring ownership is not mutation of a live value.

The added permission consumers are native assignment places and the existing
collection-mutation admission owner, plus self-host typed local-environment modes,
assignment verdicts and collection-mutation verdicts. A Slice write must also
respect its recorded backing-place permission. These are derived from declaration
identity, never from a variable name scan. Binding permission cannot be erased
by a field projection, view or inout call. Missing permission facts fail closed.
The compiler-resolved migration includes these new writes; prior binding-only
positive fixtures become mutable at their actual declaration. Negative controls
for readonly borrows, fields and invalidated storage keep the original inputs.

The adjacent audit reproduced write-permission loss through Slice rebinding,
reslicing and a returned view. The existing Slice origin owner must retain every
lexically live backing place across those operations. Its consumers are value
write admission, backing retirement/replacement checks and parallel split
admission. Assignment cannot create a new capability or retain a stale split
fact. A returned/projected view without a proved writable origin remains
readable, but a write fails closed; do not infer permission from its `var` name.
Validate direct and nested views, rebindings in branches/loops, read-only
returned views, writable direct views, and the existing disjoint split gate.

The declaration migration is based on compiler-resolved declaration positions,
not identifier spelling. Initial census reached its 300-second budget; after
that migration the next full input completed and exposed additional writes.
Both frozen inputs and their exact edits are recorded under
`.tmp/compound-assignment-20261108/evidence/`. No whole-chain closure is claimed.

## Migration dependency: declaration permission is not source stability

The coherent full input reproduces 52 final ownership errors after declaration
migration. Several collection consumers exclude `var` even when their own
typed history already validates all uses or replacements. Preserve the
declaration's permission and repair those consumers, not the migrated source.

- Empty local String-array receipts: `indexed_string_nonowner_risk` records
  the exact empty birth; its existing sealed history rejects replacement,
  unproved writes and escapes before any receipt can remove a risk.
- Direct aggregate member paths: `collection_member_path_owner` owns typed
  identity, not an ownership verdict. The member-requirement owner proves
  initializer, replacement and returned field elements before cleanup.
- Keep initializer-only carrier/field lineage guards until actual definition
  stability is proved by their existing flow owner. Do not remove every
  mutability guard or infer stability from a lack of root rebinding alone.
- Gates: mutable never-rebound arrays/fields, whole-value and field replacement,
  borrowed elements, retirement and stale use; then the same full compiler
  input. No new source-level Clone or manual drop is a migration repair.

The same chain reaches owned ArraySet replay. Its copy-site selector must use
the current indexed-storage source and exact typed initializer, including the
pending type-check generation; the sealed routine API is not available there.
Copy-site selection does not admit the store: the collection owner must still
prove owned elements on replay. Actual descriptor replacement (including a
branch replacement) must not reuse the initializer receipt. The focused scalar
type contract reproduces the full input's first store error without recompiling
the whole driver. Read-after-store errors are rechecked after that producer is
repaired because a preceding unclassified error prevents String copy selection.

The adjacent while/for backedge falsifier writes an immutable Array on its
second iteration: a Slice is rebound after the first write. Slice origins are
monotone lexical loans, but resource-flow convergence does not observe their
growth. Add those exact live Slice identities and origin-prefix tokens to the
existing flow snapshot, copy/merge/equality lifecycle and loop convergence
bound. Do not restore an older loan set. Cached loop transfer cannot replay
Slice provenance; retain its exported flow facts but re-enter the body when
live Slice loans participate. Verify while/for, continue, local reslices,
safe writable rebindings and straight-line writes before readonly rebinding.

The follow-up boundary audit reproduced default Slice parameters gaining write
permission through a local alias or an inout call. Apply their existing readonly
boundary at the common value-write owner, including aliases. Slice origin rows
may borrow Symbols only while the backing scope outlives the view binding;
reject assignment from an inner owner to an outer view at that producer, before
scope teardown. Existing name-shadow refusal remains unchanged. Verify readonly
default/ref formals, explicit inout mutation, nested local views and scope escape.

The executable self-host body verifier reproduces the same missing Slice write
authority. Its producer inputs are admitted local identities, initializer and
assignment graphs, parameter modes and lexical/loop scope facts. Add a demand
walk over those rows at the existing post-ownership admission boundary; follow
descriptor aliases and every definition that can reach the use, including loop
backedges. Preserve the declaration/field owner for ordinary value permissions.
Returned/projected views with no proved origin remain readable but cannot grant
write permission. Unknown facts cannot become a writable default. Consumers are
indexed stores, collection mutators and inout arguments before MIR production.
Falsifiers are the same native Slice cases, including safe one-iteration and
write-before-rebinding controls. This is semantic parity, not installed-driver
or fixed-point evidence.

The original `nonowner_risk_rebind_clone_landed_valid` input exposes a second
consumer of declaration permission: the empty-array exclusion producer now
records a mutable birth, but its sealed history correctly rejects replacement.
When that exclusion is unavailable, select the existing independent String
landing for its exact typed uses. This is a copy-cost boundary owned by the
loan-copy landing owner, not permission to weaken nonowner history. The next
analysis must rebuild receipts from the inserted Clone before MIR admission;
stale/released sources and forged use identities must still refuse. Measure
the copied sites and run the original nonowner positive/negative inputs.

Full-input replay reaches the builtin-retention registry through
`CompileSourceToC` -> declared result -> `DriverRung2MirProjectionJson` ->
`SelfMirProgramJson` -> `TextBuilderAppend`. Runtime append mutates its local
builder synchronously and copies text; it retains neither argument address.
The registry lists only text ordinal 1, so the no-store consumer conservatively
marks the unlisted builder ordinal 0 as an outward store. Add that measured
missing receiver fact to the existing registry and regenerate its self-host
projection. Keep single-owner/scope/consumption checks separate and unchanged.
The registry renderer must emit `var` for its own changing counters. Verify
registry drift, local-builder C/LLVM execution, borrowed inputs, use after
finish/drop, shadowed callees and the same full compiler input.

Generated Pergyra declarations belong to their renderers. The migration must
change changing locals in the six reached registry renderers, regenerate their
outputs and execute the resulting projections on C/LLVM. Hand editing a
generated declaration is not durable migration evidence.

The typed self-host audit also admitted a growing inout call on the RHS of an
indexed compound store. The selected backing address is live until the store.
The graph owns the target, nested calls, exact local/formal identities and
formal modes; consume those facts at body admission before MIR. A mutation,
consumption or unproved call on overlapping backing cannot run during that
loan. Readonly calls, scalar arguments and disjoint exact places remain valid.
Do not fix this in an emitter or defer lvalue selection until after the RHS.
Falsifiers include direct/formal/member backing, a nested call argument,
readonly and disjoint controls, and a shadowed mutator. This closes the direct
place boundary; descriptor aliases and inferred default-parameter effects must
continue to use the existing ownership owners, not a name-based exemption.
