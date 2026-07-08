# Compatibility Evolution Checker

## Intent

Consume the PgyCompilerWorld compatibility-evolution owner and prove the seed
breaking-change corpus has coverage for every compatibility surface.
It also proves that the obsolete/migration envelope is field-owned: diagnostic
ID, warning/error/remove versions, replacement, migration URL, and codefix
status must appear in their canonical row positions rather than as loose
substrings. Change kinds are checked against the compatibility owner vocabulary,
so the checker cannot accept a row whose behavior class is invented locally.

## Input Contract

The checker imports `compatibility_evolution_owner.pgy` directly. Shell may
launch the tool but must not own the corpus rows or duplicate the coverage
rules. The consumed source of truth is `CompatibilityEvolutionZone`; this tool
is only a report/check consumer, not a second compatibility policy owner.

## Output Contract

The checker emits one `pgy.selfhost.compatibility-corpus.v1` JSON report with
change counts, per-surface coverage counts, obsolete migration-envelope counts,
change-kind coverage counts, and structured findings.

## Oracle

`tests/self_hosted/parity/compatibility_evolution_checker_parity.sh` compiles
the checker through C and LLVM, compares the report with the committed expected
artifact, and verifies the C/LLVM tool outputs are identical.
The same parity gate runs fail-closed negative modes for malformed change rows
invalid codefix statuses, invalid change kinds, and missing surfaces on C and
LLVM when the LLVM backend is available.
