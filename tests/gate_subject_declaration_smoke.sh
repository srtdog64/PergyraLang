#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate:
#   a gate declared the wrong compiler for its subject.
#
# PGY_NATIVE_PIPELINE tells a gate to compile in-process instead of delegating
# to the installed self-host driver. That is the right call only when the gate
# asserts a native-pipeline fact, so the declaration has to say which fact --
# otherwise the variable becomes a way to make a red self-host coverage gate
# quiet. This gate checks the two halves of that contract:
#
#   1. every script that declares the variable also states its subject;
#   2. no self-host-subject script declares it at all.
#
# See docs/152_validation_isolation_policy.md.
#
# This gate reads scripts; it does not compile, so it declares nothing itself.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

SUBJECT_MARKER="# Subject of this gate:"
failures=0

report() {
    echo "[gate-subject] $1" >&2
    failures=$((failures + 1))
}

# 1. A declaration without a stated subject is an undocumented opt-out. Only
#    scripts that assign the variable are declarations; a script that merely
#    names it in prose (this one, for instance) is not opting anything out.
first_line() {
    grep -n -- "$2" "$1" | head -1 | cut -d: -f1
}

while IFS= read -r script; do
    if ! grep -Fq -- "$SUBJECT_MARKER" "$script"; then
        report "$script sets PGY_NATIVE_PIPELINE without a '$SUBJECT_MARKER' comment"
        continue
    fi
    # PowerShell harnesses spell the same declaration as an $env: assignment.
    case "$script" in
        *.ps1) export_pattern='^\$env:PGY_NATIVE_PIPELINE = .1.$' ;;
        *)     export_pattern='^export PGY_NATIVE_PIPELINE$' ;;
    esac
    export_line="$(first_line "$script" "$export_pattern" || true)"
    if [[ -z "$export_line" ]]; then
        report "$script sets PGY_NATIVE_PIPELINE without a plain exported declaration line"
        continue
    fi
    # The subject must precede the export, so a reader meets the reason first.
    subject_line="$(first_line "$script" "$SUBJECT_MARKER" || true)"
    if (( subject_line > export_line )); then
        report "$script states its subject after the export; state it first"
    fi
done < <({ grep -rl '^PGY_NATIVE_PIPELINE=' tests scripts --include='*.sh'
           grep -rl '^\$env:PGY_NATIVE_PIPELINE' tests scripts --include='*.ps1'
         } | sort)

# 2. A self-host-subject gate is red when the self-hosted compiler does not
#    cover a surface. That is the signal it exists to give, so it must not opt
#    out. tests/self_hosted/parity holds the bootstrap scaffolding, which is
#    the one declared exception: the driver it would delegate to is the
#    artifact those legs are building.
while IFS= read -r script; do
    case "$script" in
        tests/self_hosted/parity/*) continue ;;
    esac
    if grep -q '^PGY_NATIVE_PIPELINE=' "$script"; then
        report "$script is self-host-subject but opts out of the self-host driver"
    fi
done < <({ ls tests/self_host*.sh 2>/dev/null || true
           find tests/self_hosted -name '*.sh' 2>/dev/null || true; } | sort -u)

if (( failures > 0 )); then
    echo "[gate-subject] $failures violation(s); see docs/152_validation_isolation_policy.md" >&2
    exit 1
fi

declared="$({ grep -rl '^PGY_NATIVE_PIPELINE=' tests scripts --include='*.sh'
               grep -rl '^\$env:PGY_NATIVE_PIPELINE' tests scripts --include='*.ps1'
             } | sort -u | wc -l | tr -d ' ')"
echo "[gate-subject] ok -- $declared native-subject gates declare a subject; no self-host-subject gate opts out"
