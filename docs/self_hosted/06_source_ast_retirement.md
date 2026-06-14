# source_ast Retirement (Phase 2 / Single Source of Truth)

This note records why the backend source_ast readers are safe to retire, how
their deadness is measured and locked, and the staged plan to remove them. It
is the deletion-readiness evidence for the Phase 2 cutover.

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

tests/ast_read_surface_smoke.sh ratchets the source_ast occurrence counts
(codegen 172, compiler 73) so the surface can only shrink, never grow.

tests/source_ast_inventory.sh ranks the remaining readers per file so the
cutover is driven hotspot first. The codegen frontier is 172, of which 73 sit
in four files: transpiler_decl_slot_view.c, transpiler_decl_role_roster_slot_view.c,
llvm_inventory_slot_view.c, llvm_inventory_role_roster_slot_view.c.

## Empirical confirmation probe

src/codegen/source_ast_fallback_probe.h provides a compiled-out fire marker.
The static argument (driver aborts on NULL MIR) is confirmed empirically by
instrumenting the fallback arms and observing zero fires across the corpus,
matching the evidence already gathered for the enum and non_cfg fallbacks.

To apply the probe, in each of the four hotspot files:

1. Add the include:

       #include "source_ast_fallback_probe.h"

2. Immediately before every fallback line that matches

       slot = <prefix>_slot_view_source_ast(view, index);

   inside a public accessor (the arm reached only after the MIR path and the
   requires_mir_metadata guard), insert:

       PGY_SOURCE_AST_FALLBACK_FIRE(__func__);

3. Build with the probe flag and run the corpus:

       make clean && make CFLAGS_EXTRA=-DPGY_PROBE_SOURCE_AST_FALLBACK
       # run the example and parity corpus, capturing stderr
       grep -c '\[source-ast-fallback\]' corpus_stderr.log

   A count of zero confirms the fallbacks never fire. The marker string is
   absent from the release binary because the flag is off by default.

## Two classes of source_ast read

Instrumentation coverage analysis split the codegen source_ast reads into two
classes that retire differently.

The fallback class lives in the four slot and roster view files
(transpiler_decl_slot_view, llvm_inventory_slot_view,
transpiler_decl_role_roster_slot_view, llvm_inventory_role_roster_slot_view).
These are AST-fallback arms reached only after the MIR path and the
requires_mir_metadata guard, so they are dead whenever MIR is present. The probe
is wired into all eighteen of these sites, and the corpus run gives a complete
zero-fire proof for this class. Retirement here is pure deletion.

The provenance class lives in the method and field view files
(transpiler_decl_method_view, transpiler_decl_field_view,
llvm_inventory_field_view, llvm_inventory_host_methods). These reads are
mir_decl_method_source_ast and mir_decl_field_source_ast, the back-pointer the
MIR metadata keeps to the ASTNode it was lowered from. They are not fallbacks
and do not fire conditionally; they are consumed wherever a caller still needs
the origin AST. The probe does not apply. Retiring this class means migrating
each consumer off the back-pointer, after which the source_ast field on the MIR
metadata can be dropped. This is the broader, slower part of the codegen 172.

One special case sits inside the fallback class. The role view accessors
required_ability_count and required_ability call the source_ast helper with no
MIR-first guard, because MIRDeclField carries no ability metadata. These are not
dead fallbacks but un-migrated reads, and they are deliberately left
un-instrumented. They need ability metadata added to MIRDeclField before they
can be retired.

This was verified, not assumed. Stubbing the two ability accessors to their
default return and rebuilding changes emitted output: dyn_test.pgy, which
compiled to 119 lines of C at baseline, instead fails compilation with "cannot
resolve required ability tag for party slot 'Team.fighter' while emitting bind
statement." The reads are consumed by transpiler_role_ability.c and
llvm_domain_role_lookup.c to emit dynamic dispatch, so they are load-bearing.
Removing them before MIRDeclField carries ability metadata is a regression that
breaks every role-ability bind program, not a safe deletion.

## Probe result (recorded)

The probe was wired into the eighteen fallback sites, built with
PGY_PROBE_SOURCE_AST_FALLBACK, and run over the example corpus on both backends.
The C backend ran sixty programs (thirty-three compiling clean) and the LLVM
backend ran sixty programs (thirty-one compiling clean). The fallback marker
fired zero times on either backend. This confirms empirically what the
MIR-or-abort invariant predicts: the eighteen dead-fallback arms are never
reached, so they can be removed. The role required_ability reads remain a
separate un-migrated case as noted above.

## Ability metadata migration (unblocks the role required_ability reads)

The role required_ability reads cannot be deleted until MIRDeclField carries
the ability data. Investigation pinned down exactly what the consumers need, so
the migration is small and mirrors an existing pattern.

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
   string array, for example size_t required_ability_count and
   char **required_ability_type_names, alongside the existing field metadata.

2. Where role-slot MIRDeclField values are populated during lowering, read
   ast_role_slot_required_ability_count and ast_role_slot_required_ability at
   lowering time, take ast_type_name of each ability node, and copy those
   strings into the MIR array. This is the one place the AST is still read, and
   it happens during lowering where the AST is legitimately present.

3. Add accessors mir_decl_field_required_ability_count(field) and
   mir_decl_field_required_ability_type_name(field, index).

4. Repoint the view accessors. required_ability_count returns the MIR count
   MIR-first. Replace the node-returning required_ability with a type-name
   returning path, and update the two consumers to take the string directly
   rather than calling ast_type_name or render_type_name on a node.

5. Free the string array in mir_lifecycle alongside the other MIR-owned
   captures.

After this the required_ability reads are MIR-first like the rest, the
source_ast helper calls in those accessors fall away, and the role-ability bind
codegen keeps working because the type name it needs now travels in MIR. In the
domain-mobility frame this is the step that moves the role ability contract off
its origin AST and into MIR, so the domain stops holding a back-pointer to the
AST for ability data.

## Staged deletion plan

Each step ends with the full build and corpus verification.

Step 1: instrument with the probe, run the corpus, confirm zero fires.

Step 2: remove the AST fallback arms and the `*_slot_view_source_ast` helpers
from the four hotspot files, then the remaining codegen readers. The
requires_mir_metadata branches collapse to the MIR path plus a NULL return.

Step 3: remove the source_ast field from MIRDeclField and MIRDeclHeader once no
codegen reader remains. The compiler plumbing (set on capture, freed in
lifecycle, walked by the provenance shape) empties at this step.

Step 4: lower the ast_read_surface ratchet ceilings to zero and delete the
probe header.
