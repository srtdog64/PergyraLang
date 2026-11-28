# 2026-11-29: `semantic.callable_receiver_carriage` closes by scope

Decided by: the user, on 2026-11-29 ("다음 SoT 2개 더 닫아", closing by
scope decisions and choosing the better long-term option without asking);
recorded by the Claude lane. The user can overturn it.

## Rule

1. Every registered consumer of the row reads the mandatory
   none/value/mutable-identity receiver carriage through exact callable and
   declaration identity. That covers native MIR production, validation and
   JSON, and the self-host semantic, MIR, mir_lower and codegen owners.
2. Native C and LLVM code generation decide pointer self from the MIR
   declaration header's `uses_pointer_self`. They are not consumers of this
   row and are not migrated:
   - Native MIR derives each routine's carriage from that same header field
     (`mir_signature_metadata.c`).
   - `mir_program_fact_validate.c` refuses a routine whose carriage
     disagrees with its declaration owner, and the native unit test "MIR
     validator rejects pointer-self ABI metadata drift" refuses a mutated
     header. The two reads therefore cannot give different answers on an
     admitted program.
   - After V1 the native pipeline is a frozen oracle
     ([decision](2026-11-24_native_pipeline_becomes_frozen_oracle_after_v1.md)).
     Moving its 60-odd pointer-self call sites to the carriage row would
     change no answer and would be deleted with the native pipeline.
3. Source-level ability constraints and generic/default contracts that MIR
   does not carry are not receiver carriage. They move to
   `mir.generic_specialization`, whose active falsifier is already the
   ability declaration erasure.

4. The row's enforcement gates are the ones push CI runs:
   - the carriage admission gate (forged receiver rows refused by
     `mir_lower`), now through the shared MIR lowering tool build;
   - the native `test-mir` pointer-self drift test;
   - the role override replacement gate (role C/LLVM execution, receiver
     runtimes and 14 source negatives).

   Two entries leave the list.
   - `driver_rung2_callable_receiver_carriage_owner.sh` defines a check that
     only the DRV-2 body parity battery calls; it runs nothing on its own.
   - `codegen_role_receiver_admission_owner.sh` first builds the whole
     self-host codegen from source through the installed driver, which took
     17 minutes locally and failed in that build, not in the receiver check.
     With the bootstrap's codegen it passes.

## Reopens when

- the native pipeline stays past V1 as a product compiler rather than an
  oracle; or
- a native code generator decides receiver passing from anything other than
  the declaration header the carriage is derived from.

Replaces: nothing.
