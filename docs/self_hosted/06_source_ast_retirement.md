# source_ast Retirement (Phase 2 / Single Source of Truth)

This note records why the backend source_ast readers were safe to retire, how
their deadness is measured and locked, and the remaining compiler-side work to
remove the provenance back-pointers. It is the deletion-readiness evidence for
the Phase 2 cutover.

## Finding

The backend slot and inventory views already resolve declaration data
MIR-first. Every accessor in the four hotspot files follows one shape:

    const MIRDeclField *field = ...metadata(view, index);
    if (field != NULL)
        return mir_decl_field_X(field);        /* MIR metadata path */
    if (view->requires_mir_metadata)
        return NULL;                           /* strict: AST not touched */
    slot = ...slot_view_source_ast(view, index);
    return ast_domain_slot_X(slot);            /* legacy AST fallback */

MIRDeclField already exposes the full accessor set the fallback reconstructs:
name, type, type_name, is_subject_like, is_tobject_like, is_binding_like. The
source_ast reads are therefore not missing metadata. They are fallback arms.

`requires_mir_metadata` resolves to `active_has_mir(ctx)`, which is
`ctx->mir != NULL`. The driver aborts compilation when MIR lowering returns no
program (driver_app.c: `if (mir == NULL) goto cleanup;`), so codegen never
runs with a NULL MIR. The fallback arms are unreachable in production: dead
code, the same shape as the enum variant and non_cfg_body fallbacks already
retired.

## Gates that lock the finding

Three checks keep this true so the deletion stays safe until it happens.

tests/mir_or_abort_invariant_smoke.sh asserts the driver still aborts on a
NULL MIR. This is the single precondition that makes the fallbacks dead; if the
driver is ever changed to degrade to an AST-only path, this fails first.

tests/ast_read_surface_smoke.sh ratchets the source_ast occurrence counts. The
codegen frontier is locked at 0 and the compiler declaration-header payload
boundary is locked at 2, so the surface can only shrink, never grow. The scalar
source-node type/location provenance has been split out of this metric. The ratchet values and
covered files live in tests/ast_read_surface_manifest.txt, which is consumed by
both the shell smoke and the self-hosted ast_read_surface_checker parity rung.
The same manifest now also tracks the AST-returning declaration-header
compatibility boundary: `mir_decl_header_source_decl` is locked at codegen 2
and compiler 1 until that API is removed. Routine source-decl compatibility is
tracked separately as `routine_source_decl_codegen`, locked at 0.

tests/source_ast_inventory.sh ranks the remaining readers per file so the
cutover is driven hotspot first. The codegen frontier is now 0 and the
inventory/slot-view hotspot is also 0. Dead slot-source accessors, shared-field
source accessors, hosted-method source view accessors, the LLVM method body AST
compatibility accessor, and thin C/LLVM routine source aliases are retired.
LLVM generic class method specialization follows the MIRDeclMethod routine
link; C hosted method bodies now pass the linked MIRRoutine directly to the
MIR body emitter instead of recovering the method source declaration. C
class/zone collection-specialization scans also consume the linked MIRRoutine
signature and source-local type facts instead of recovering method bodies. The
remaining C method bridge is limited to lookup compatibility; C overlay
projection invalidation now consumes MIRDeclMethod projection write/call facts
instead of recovering method source declarations. LLVM generic function template registration records MIRRoutine
entries and specializes through that routine.
LLVM MIR body emission consumes routine kind/signature/current-routine metadata
without recovering a source declaration. LLVM intent body emission now starts
from active declaration inventory and rejects MIR intent routines that have body
storage but no declaration inventory row. Routine source declaration checks no
longer appear in codegen; the compiler-owned
`mir_routine_source_decl_of_type` remains compiler-side only. Declaration lookup uses
`mir_decl_header_source_decl`. No backend `.c` file now contains a `source_ast`
read. The remaining 2 occurrences are compiler-owned declaration-header payload
plumbing: the `MIRDeclHeader.source_ast` assignment and accessor. Method and field declaration back-pointers are retired, and the
dead `mir_decl_header_ast_shape` compatibility arm that recomputed header shape
from the origin AST is deleted. The next slice removes the declaration-header
payload boundary, now measured separately as source_decl 2/1. Routine source-decl compatibility is
measured separately at 0 so lookup compatibility cannot grow while
that payload boundary is retired.

src/self_hosted/parity/ast_read_surface_checker_parity.sh runs the same
manifest through a Pergyra-written checker and compares the literal counts
against shell grep. The shell smoke still owns directory coverage; the
self-hosted checker proves the manifest/cap verdict from inside the language,
including a synthetic source_ast growth fixture that must fail.

## Empirical confirmation probe

The compiled-out probe that marked source_ast fallback fires was used during
the deletion phase and has now been removed with the fallback arms. The
recorded result remains part of the evidence: the instrumented C and LLVM
corpus runs observed zero fires, matching the MIR-or-abort invariant.

## Two classes of source_ast read

Instrumentation coverage analysis split the codegen source_ast reads into two
classes that retire differently.

The fallback class lived in the four slot and roster view files
(transpiler_decl_slot_view, llvm_inventory_slot_view,
transpiler_decl_role_roster_slot_view, llvm_inventory_role_roster_slot_view).
These were AST-fallback arms reached only after the MIR path and the
requires_mir_metadata guard, so they were dead whenever MIR was present. The
probe was wired into all eighteen of these sites, and the corpus run gave a
complete zero-fire proof for this class. The unused slot-source accessors and
all remaining backend source_ast readers have now been deleted.

The provenance class lives in the method and field view files
(transpiler_decl_method_view, transpiler_decl_field_view,
llvm_inventory_field_view, llvm_inventory_host_methods). These reads are
mir_decl_method_source_ast and mir_decl_field_source_ast, the back-pointer the
MIR metadata keeps to the ASTNode it was lowered from. They are not fallbacks
and do not fire conditionally; they are consumed wherever a caller still needs
the origin AST. The probe does not apply. Retiring this class meant migrating
each backend consumer off the back-pointer. At the start of this phase the
codegen frontier had 127 source_ast reads. The current ratchet is 0, so all
127 codegen reads have been retired. The compiler-side ratchet is now 50.
Generic, enum, method, and field validation is metadata-owned; method and field
back-pointers are gone; and header shape is no longer recomputed from
`header->source_ast`. The remaining payload boundary is the declaration header
source declaration accessor used by compatibility lookup, plus source-type/
location scalar names.

One special case sat inside the fallback class. The role view accessors
required_ability_count and required_ability originally called the source_ast
helper with no MIR-first guard, because MIRDeclField carried no ability
metadata. This was verified, not assumed: stubbing the two ability accessors to
their default return changed emitted output and broke dyn_test.pgy with "cannot
resolve required ability tag for party slot 'Team.fighter' while emitting bind
statement." The reads were load-bearing, so they were migrated by adding
structured `MIRAbilityRef` metadata to role-slot fields before the source_ast
accessors were retired.

## Probe result (recorded)

The probe was wired into the eighteen fallback sites, built with
PGY_PROBE_SOURCE_AST_FALLBACK, and run over the example corpus on both backends.
The C backend ran sixty programs (thirty-three compiling clean) and the LLVM
backend ran sixty programs (thirty-one compiling clean). The fallback marker
fired zero times on either backend. This confirms empirically what the
MIR-or-abort invariant predicts: the eighteen dead-fallback arms are never
reached, so they can be removed. The role required_ability reads were a
separate load-bearing case and are now migrated through `MIRAbilityRef`.

## Ability metadata migration (role required_ability reads retired)

The role required_ability reads could not be deleted until MIRDeclField carried
the ability data. Investigation pinned down exactly what the consumers needed,
so the migration uses a MIR-owned structured ability reference.

Both consumers reduce the ability AST node to a single string. In
transpiler_role_ability.c the node goes through render_ability_ref_vtable_tag,
which calls render_type_name_in_ctx and then sanitizes the result, so the input
it needs is the ability's type name. In llvm_domain_role_lookup.c the node goes
straight through ast_type_name, again the ability's type name. Neither consumer
needs anything from the node except its type name.

So the data to capture is a list of ability type-name strings per role slot,
which is the same shape as MIRDeclEnumVariant.param_type_names (char **). The
migration steps:

1. In mir_decl.h, add to the role-slot MIRDeclField a count and a MIR-owned
   `MIRAbilityRef` array alongside the existing field metadata.

2. Where role-slot MIRDeclField values are populated during lowering, read
   ast_role_slot_required_ability_count and ast_role_slot_required_ability at
   lowering time, take ast_type_name of each ability node, and copy those
   strings into the MIR array. This is the one place the AST is still read, and
   it happens during lowering where the AST is legitimately present.

3. Add accessors `mir_decl_field_required_ability_count(field)` and
   `mir_decl_field_required_ability_ref(field, index)`.

4. Repoint the view accessors. `required_ability_count` returns the MIR count
   MIR-first. Replace the node-returning `required_ability` with a
   `MIRAbilityRef` returning path, and update consumers to render tags from the
   structured reference.

5. Free the structured reference array in mir_lifecycle alongside the other
   MIR-owned captures.

After this the required_ability reads are MIR-first like the rest, the
source_ast helper calls in those accessors fall away, and the role-ability bind
codegen keeps working because the ability contract now travels in MIR. In the
domain-mobility frame this is the step that moves the role ability contract off
its origin AST and into MIR, so the domain stops holding a back-pointer to the
AST for ability data.

## C ability vtable emission is generic-aware (design finding)

The role ability reads split further than name versus node. Investigation of
the C vtable path found three consumers of the required_ability node:

The two transpiler_role_ability.c consumers reduce the ability to a string. The
first builds a vtable tag through the ctx-NULL render path, where no generic
bindings apply, so the captured type name is sufficient and it can migrate to
required_ability_type_name. The second looks up find_ability_decl by name, but
its tag goes through render_ability_ref_vtable_tag_in_ctx with a live ctx.

The transpiler_domain_nominal_emit.c consumer emits the vtable struct and
typedef through ensure_ability_ref_vtable_decl and ability_ref_vtable_typedef_name.

Both ctx-based paths reach render_effective_ability_ref_vtable_tag, which calls
build_ability_ref_bindings. That reads ast_type_generic_args(ability_ref): an
ability reference can be generic, for example Ability<T>, and the vtable tag and
bindings monomorphize the actual generic arguments under ctx. A captured type
name therefore cannot reproduce the tag for a generic ability; the generic
argument types are part of the identity.

Consequence: only consumer one is safe to migrate on a captured name. The
generic-aware vtable paths need the ability reference's full shape, name plus
generic arguments, not a single string. Retiring them requires representing the
generic ability reference in MIR, either as a structured ability-ref capture
(name plus an ordered list of argument type names, recursively) or by moving the
vtable tag derivation itself behind MIR metadata. This is the deeper tail of the
role ability contract: in the domain-mobility frame, the ability contract is not
just a name but a generic type reference, and moving it off the AST means moving
that generic reference, not flattening it to a name.

Recommended staging: migrate consumer one now for a small reduction; design a
structured generic ability-ref capture for MIR before touching the vtable
consumers; keep the node accessor until that capture exists.

## Structured generic ability-ref capture (design)

This is the representation that moves the generic ability contract off the AST.
It replaces the type-name-only capture for role abilities.

Data model. Add a structured ability reference to the role-slot MIRDeclField:

    typedef struct {
        char   *base_name;              /* ability base type name */
        size_t  generic_arg_count;
        char  **generic_arg_type_names; /* one rendered name per generic arg */
    } MIRAbilityRef;

    /* on MIRDeclField, replacing required_ability_type_names: */
    size_t         required_ability_count;
    MIRAbilityRef *required_abilities;

The argument list is flat here. If an argument is itself a generic type, the
rendered name produced at lowering already carries the nested form as a string
(for example Pair<Int, Bool>), so a flat string array is enough for tag
derivation; a recursive node form is only needed if a later pass must take the
arguments apart again, which the vtable path does not.

Capture point and the lowering versus consumption split. The capture runs in
mir_decl_field_metadata_init_role_slot, at MIR lowering. A subtlety: lowering
cannot run find_ability_decl, which is a codegen function needing ctx, so the
formal-parameter-to-default mapping cannot happen at capture time. Capture
therefore records only what the ability reference itself carries: base_name from
ast_type_name, and the actual generic arguments as written, by walking
ast_type_generic_args and storing ast_type_name of each actual argument type.
Default-argument filling stays at consumption, but the MIR-active consumer now
resolves the ability declaration through `MIRDeclHeader` generic rows instead of
opening the source declaration. For each formal param it uses the captured
actual argument if present or the `MIRDeclGenericParam` default/constraint
otherwise; AST declaration lookup is retained only for the non-MIR compatibility
path. This keeps the cross-declaration resolution on the codegen side that owns
it, and keeps the capture purely local to the ability reference node.

Consumption. The string-based tag renderer takes a `MIRAbilityRef` rather than
`(ability_decl, ability_ref node)`: it concatenates `base_name` with the
captured actual generic argument names, fills omitted arguments from
`MIRDeclGenericParam`, and uses the same separator and sanitize rules. The
remaining ability declaration body emitters still own method signatures/bodies;
role-slot tag and dispatch consumers no longer recover the ability declaration
just to compute generic tags in MIR-active paths.

Caveat to verify. render_type_name_in_ctx is ctx-aware, while lowering capture
uses ast_type_name. These diverge only when an ability argument is an outer type
parameter being monomorphized by the enclosing generic host, for example
role R<T> with slot Ability<T>. The existing name and type_name metadata capture
the same way and are byte-identical on the corpus, which suggests this case does
not arise for role ability slots in practice, but the migration must be verified
byte-identical against a generic-ability program, not only dyn_test, before the
node accessor is removed. If a divergence appears, the host's monomorphization
binding must be threaded into the capture or the arguments kept as a node form.

This migration is complete for the backend source_ast burn-down: the role
ability consumers no longer keep the codegen ratchet above zero. Any further
ability-ref work is now ordinary metadata fidelity work, not a source_ast
retirement blocker.

## Staged deletion plan

Each step ends with the full build and corpus verification.

Step 1: instrument with the probe, run the corpus, confirm zero fires.

Step 2: remove the AST fallback arms and the `*_slot_view_source_ast` helpers
from the four hotspot files, then the remaining codegen readers. This is done:
the current codegen frontier is 0 reads and backend source declaration
compatibility is routed through compiler-owned MIR accessors.

Step 3: remove the source_ast field from MIRDeclField and MIRDeclHeader after
validator drift checks stop reading generic, enum, method, and field facts from
the original AST nodes. MIRDeclField and MIRDeclMethod are done; validation no
longer reopens generic, enum, method, or field origin nodes; and the
`mir_decl_header_ast_shape` compatibility arm is deleted. The remaining payload
part of the compiler tail is `MIRDeclHeader.source_ast` and
`mir_decl_header_source_decl`.

Step 4: lower the compiler ast_read_surface ratchet ceiling to zero after the
payload field is removed. The source-node type/location scalar provenance has
already been split into non-AST names. The codegen ceiling is
already zero and the probe header is deleted.
