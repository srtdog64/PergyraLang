#!/usr/bin/env bash
# One admitted owner, typed refusals, and fresh extracted finite observations.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
pgy_rocq_require
command -v ocamlopt >/dev/null 2>&1 || { echo 'ocamlopt is required' >&2; exit 1; }
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
receipt="$ROOT_DIR/.tmp/ownership-teardown-authority-2026-10-08"
mkdir -p "$receipt"
{
    rocq --version
    ocamlopt -version
    uname -srv
    sha256sum "$ROOT_DIR/docs/semantics/proofs/OwnershipTeardown.v" \
      "$ROOT_DIR/docs/semantics/proofs/OwnershipTeardownAuthority.v" \
      "$ROOT_DIR/tests/coq/OwnershipTeardownRedteam.v" \
      "$ROOT_DIR/tests/coq/OwnershipTeardownExtraction.v" \
      "$ROOT_DIR/tests/ocaml/ownership_teardown_driver.ml" "$ROOT_DIR/tests/ownership_teardown_redteam_smoke.sh"
} >"$receipt/environment.log"
for input in docs/semantics/proofs/OwnershipTeardown.v \
    docs/semantics/proofs/OwnershipTeardownAuthority.v \
    tests/coq/OwnershipTeardownRedteam.v tests/coq/OwnershipTeardownExtraction.v; do
    cp "$ROOT_DIR/$input" "$work/"
done
PGY_COQ_PROOFS_DIR="$work" PGY_COQ_EXPECTED_AXIOMS='' PGY_ROCQ_EXTRACT_DIR="$work" \
    bash "$ROOT_DIR/tests/coq_kernel_check.sh" >"$receipt/kernel-extraction.log" 2>&1 || {
        tail -n 30 "$receipt/kernel-extraction.log" >&2; exit 1;
    }
tail -n 1 "$receipt/kernel-extraction.log"
mkdir -p "$receipt/extracted"
cp "$work/ownership_teardown.ml" "$work/ownership_teardown.mli" "$receipt/extracted/"
cp "$work/ownership_teardown_authority.ml" "$work/ownership_teardown_authority.mli" "$receipt/extracted/"
cp "$ROOT_DIR/tests/ocaml/ownership_teardown_driver.ml" "$work/"
(cd "$work" && ocamlopt -O2 -o teardown_driver \
    ownership_teardown.mli ownership_teardown.ml \
    ownership_teardown_authority.mli ownership_teardown_authority.ml ownership_teardown_driver.ml)
"$work/teardown_driver" --selftest | tee "$receipt/controls.log"
if "$work/teardown_driver" --invalid >"$work/invalid.log" 2>&1; then
    echo '[teardown-redteam] unknown command appeared green' >&2; exit 1
fi
grep -Fq 'usage:' "$work/invalid.log"
"$work/teardown_driver" --benchmark >"$receipt/cost.jsonl" 2>"$receipt/cost-scope.log"
cat "$receipt/cost-scope.log"

# Isolated mechanical guard mutations. A failing proof is an observed refusal,
# not a timeout, no compiler, or a different shared lane's killed process.
for mutation in unique ancestor incoming children root-alloc root-attach root-drop root-retire root-reset node-read root-check; do
    mutant="$work/$mutation"
    mkdir "$mutant"
    cp "$work/OwnershipTeardownRedteam.v" "$mutant/"
    cp "$work/OwnershipTeardownAuthority.v" "$mutant/"
    case "$mutation" in
        unique) expression='s/NoDup Ul ->/True ->/g' ;;
        ancestor) expression='s/resolves h (p, g) \/\\ ~ in_sub h x p/resolves h (p, g) \/\\ True/' ;;
        incoming) expression='s/then filter (fun e => negb (ent_eqb e (x, k))) (ix t)/then ix t/' ;;
        children) expression='s/then filter (fun c => negb (Nat.eqb c x)) (kd q)/then kd q/' ;;
        root-alloc) expression='/Definition owner_ok_new/,/^  end\./s/root_resolves rs rg (r, g)/root_resolves rs (fun _ => g) (r, g)/' ;;
        root-attach) expression='/Definition attach_ok/,/^  end\./s/root_resolves rs rg (r, g)/root_resolves rs (fun _ => g) (r, g)/' ;;
        root-drop) expression='s/root_resolves (roots s) (root_gen s) (r, g) ->/root_resolves (roots s) (fun _ => g) (r, g) ->/' ;;
        root-retire) expression='s/if Nat.eqb q r then S (root_gen s q) else root_gen s q/root_gen s q/' ;;
        root-reset) expression='/Step s (OpRootNew r)/s/(bound s) (root_gen s)/(bound s) initial_root_generations/' ;;
        node-read) expression='s/if Nat.eqb (s_gen sl) (snd l) then s_node sl else None/s_node sl/' ;;
        root-check) expression='s/inU (roots s) (fst l) \&\& Nat.eqb (root_gen s (fst l)) (snd l)/inU (roots s) (fst l)/' ;;
    esac
    if [ "$mutation" = unique ]; then
        # Keep the existing positive witnesses valid under True; rejection
        # must reach the uniqueness theorem, not fail a constructor call.
        sed -e "$expression" -e '/apply StRelease;/s/exact Hnd/exact I/' \
            -e 's/by (apply StRootDrop; assumption)/by (apply StRootDrop; try assumption; exact I)/' \
            "$work/OwnershipTeardown.v" >"$mutant/OwnershipTeardown.v"
    else
        sed "$expression" "$work/OwnershipTeardown.v" >"$mutant/OwnershipTeardown.v"
    fi
    if cmp -s "$work/OwnershipTeardown.v" "$mutant/OwnershipTeardown.v"; then
        echo "[teardown-redteam] mutation did not change $mutation" >&2; exit 1
    fi
    if PGY_COQ_PROOFS_DIR="$mutant" PGY_COQ_EXPECTED_AXIOMS='' \
        bash "$ROOT_DIR/tests/coq_kernel_check.sh" >"$receipt/mutation-$mutation.log" 2>&1; then
        echo "[teardown-redteam] guard mutation accepted: $mutation" >&2; exit 1
    fi
    grep -Fq 'proof compilation failed' "$receipt/mutation-$mutation.log"
    if [ "$mutation" = unique ]; then
        grep -Fq 'No such assumption' "$receipt/mutation-$mutation.log"
    fi
    case "$mutation" in
        root-alloc|root-attach|root-drop)
            grep -Fq 'root_resolves' "$receipt/mutation-$mutation.log" ;;
    esac
done

# Admission mutations use the real importing boundary, not a second machine.
for mutation in authority-holder unit-completeness unit-loan lease-holder root-consume lease-fresh lease-approval; do
    mutant="$work/$mutation"
    mkdir "$mutant"
    cp "$work/OwnershipTeardown.v" "$work/OwnershipTeardownRedteam.v" "$mutant/"
    case "$mutation" in
        authority-holder) expression='s/Nat.eqb holder (acting_context a)/true/' ;;
        unit-completeness) expression='/Definition valid_unit/,/^Definition UnitExact/s/(seq 0 (bound s))/(seq 0 0)/' ;;
        unit-loan) expression='/Definition unit_quiet/,/^Inductive AuthorityFailure/s/negb (inU Ul (fst (lease_target lease)))/true/' ;;
        lease-holder) expression='s/Nat.eqb (lease_holder lease) (acting_context a)/true/' ;;
        root-consume) expression='/Definition retire/,/^Definition create_root/s/put_right (cleanup_rights a) (fst l) None/cleanup_rights a/' ;;
        lease-fresh) expression='/Definition begin_lease/,/^Definition end_lease/s/(S (next_lease a))/(next_lease a)/' ;;
        lease-approval) expression='/Definition begin_lease/,/^Definition end_lease/s/if target_authorized a (RetireNode l) then/if true then/' ;;
    esac
    sed "$expression" "$work/OwnershipTeardownAuthority.v" >"$mutant/OwnershipTeardownAuthority.v"
    if cmp -s "$work/OwnershipTeardownAuthority.v" "$mutant/OwnershipTeardownAuthority.v"; then
        echo "[teardown-redteam] mutation did not change $mutation" >&2; exit 1
    fi
    if PGY_COQ_PROOFS_DIR="$mutant" PGY_COQ_EXPECTED_AXIOMS='' \
        bash "$ROOT_DIR/tests/coq_kernel_check.sh" >"$receipt/mutation-$mutation.log" 2>&1; then
        echo "[teardown-redteam] admission mutation accepted: $mutation" >&2; exit 1
    fi
    grep -Fq 'proof compilation failed' "$receipt/mutation-$mutation.log"
    if [ "$mutation" = lease-approval ]; then
        read -r approval_start approval_end < <(awk '
            /^Theorem lease_issuance_requires_owner_approval/ { start=NR }
            start && /^Qed\./ { print start, NR; exit }
        ' "$mutant/OwnershipTeardownAuthority.v")
        failure_line="$(sed -nE 's/^File "\.\/OwnershipTeardownAuthority.v", line ([0-9]+),.*/\1/p' \
            "$receipt/mutation-$mutation.log" | head -n 1)"
        if [[ ! "$failure_line" =~ ^[0-9]+$ ]] || \
            (( failure_line < approval_start || failure_line > approval_end )); then
            echo '[teardown-redteam] approval mutation failed outside its semantic issuance theorem' >&2; exit 1
        fi
    fi
done

# Proof-script shape cannot be the only oracle for owner approval: remove the
# same guard from the freshly extracted function and observe the foreign-pin
# request being wrongly accepted by the real executor.
mkdir "$work/lease-approval-extracted"
cp "$work/ownership_teardown.ml" "$work/ownership_teardown.mli" \
    "$work/ownership_teardown_authority.mli" "$ROOT_DIR/tests/ocaml/ownership_teardown_driver.ml" \
    "$work/lease-approval-extracted/"
sed '/^let begin_lease /,/^let end_lease /s/if target_authorized a (RetireNode l)/if true/' \
    "$work/ownership_teardown_authority.ml" >"$work/lease-approval-extracted/ownership_teardown_authority.ml"
if cmp -s "$work/ownership_teardown_authority.ml" "$work/lease-approval-extracted/ownership_teardown_authority.ml"; then
    echo '[teardown-redteam] extracted approval mutation made no change' >&2; exit 1
fi
(cd "$work/lease-approval-extracted" && ocamlopt -O2 -o approval_mutant \
    ownership_teardown.mli ownership_teardown.ml \
    ownership_teardown_authority.mli ownership_teardown_authority.ml ownership_teardown_driver.ml)
if "$work/lease-approval-extracted/approval_mutant" --selftest >"$receipt/mutation-approval-extracted.log" 2>&1; then
    echo '[teardown-redteam] foreign pin accepted after extracted approval mutation' >&2; exit 1
fi
grep -Fq 'authority refusal accepted: foreign pin issuance' "$receipt/mutation-approval-extracted.log"

# Same-value unnecessary client materialization must fail the sharing oracle.
# This is an observer-sensitivity check, not a second semantics implementation.
sed 's/let actual = index_set h ix src k value in/let actual t = (index_set h ix src k value t) @ [] in/' \
    "$ROOT_DIR/tests/ocaml/ownership_teardown_driver.ml" >"$work/copy_mutant.ml"
(cd "$work" && ocamlopt -O2 -o copy_mutant \
    ownership_teardown.mli ownership_teardown.ml \
    ownership_teardown_authority.mli ownership_teardown_authority.ml copy_mutant.ml)
if "$work/copy_mutant" --selftest >"$receipt/mutation-materialization.log" 2>&1; then
    echo '[teardown-redteam] unnecessary client materialization appeared green' >&2; exit 1
fi
grep -Fq 'unrelated field row copied' "$receipt/mutation-materialization.log"

# Equal immutable results must not conceal duplicate functional-heap reads.
mkdir "$work/reread"
sed 's/let sl = h src in/let sl = (ignore (h src); h src) in/' \
    "$work/ownership_teardown.ml" >"$work/reread/ownership_teardown.ml"
if cmp -s "$work/ownership_teardown.ml" "$work/reread/ownership_teardown.ml"; then
    echo '[teardown-redteam] reread mutation did not change extraction' >&2; exit 1
fi
cp "$work/ownership_teardown.mli" "$ROOT_DIR/tests/ocaml/ownership_teardown_driver.ml" "$work/reread/"
cp "$work/ownership_teardown_authority.ml" "$work/ownership_teardown_authority.mli" "$work/reread/"
(cd "$work/reread" && ocamlopt -O2 -o reread_mutant \
    ownership_teardown.mli ownership_teardown.ml \
    ownership_teardown_authority.mli ownership_teardown_authority.ml ownership_teardown_driver.ml)
if "$work/reread/reread_mutant" --selftest >"$receipt/mutation-reread.log" 2>&1; then
    echo '[teardown-redteam] repeated source read appeared green' >&2; exit 1
fi
grep -Fq 'one retirement reread source slot' "$receipt/mutation-reread.log"
echo '[teardown-redteam] PASS: 18 guard/check mutations, 1 extracted approval mutation, 2 cost-oracle mutations, unknown-command refusal; bounded model/OCaml only'
