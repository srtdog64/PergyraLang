#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
pgy_rocq_require
# Fresh CI prefixes must not depend on a compiler borrowed from the image's
# active opam switch. Actual installation and kernel checks run in CI.
installer="$ROOT_DIR/scripts/install_rocq_toolchain.sh"
grep -Fq '"ocaml-base-compiler=$PGY_ROCQ_BOOTSTRAP_OCAML_VERSION"' "$installer"
if grep -Eq 'switch create.*ocaml-system|"ocaml-system[.=]' "$installer"; then
    echo '[rocq-toolchain-selftest] fresh bootstrap reintroduced a system-compiler fallback' >&2
    exit 1
fi
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir "$work/bin"
cat > "$work/bin/rocq" <<'SCRIPT'
#!/usr/bin/env bash
printf 'The Rocq Prover, version %s\n' "$PGY_TEST_ROCQ_VERSION"
SCRIPT
cat > "$work/bin/rocqchk" <<'SCRIPT'
#!/usr/bin/env bash
printf 'The Rocq Proof Checker, version %s\n' "${PGY_TEST_CHECKER_VERSION:-9.3.0}"
SCRIPT
cat > "$work/bin/opam" <<'SCRIPT'
#!/usr/bin/env bash
printf '%s\n' "$PGY_TEST_STDLIB_VERSION"
SCRIPT
chmod +x "$work/bin/rocq" "$work/bin/rocqchk" "$work/bin/opam"
for version in 8.18.0 9.0.1 9.3-rc1 9.3.0+dev ''; do
    if PATH="$work/bin:$PATH" PGY_TEST_ROCQ_VERSION="$version" PGY_TEST_STDLIB_VERSION=9.2.0 \
        bash -c 'source "$1"; pgy_rocq_require' bash "$ROOT_DIR/scripts/rocq_toolchain_owner.sh" >"$work/reject.log" 2>&1; then
        echo "[rocq-toolchain-selftest] accepted non-stable version: $version" >&2
        exit 1
    fi
    grep -Fq 'expected stable' "$work/reject.log"
done
if PATH="$work/bin:$PATH" PGY_TEST_ROCQ_VERSION=9.3.0 PGY_TEST_STDLIB_VERSION=9.1.0 \
    bash -c 'source "$1"; pgy_rocq_require' bash "$ROOT_DIR/scripts/rocq_toolchain_owner.sh" >"$work/stdlib.log" 2>&1; then
    echo '[rocq-toolchain-selftest] accepted wrong Stdlib' >&2; exit 1
fi
grep -Fq 'expected Stdlib' "$work/stdlib.log"
if PATH="$work/bin:$PATH" PGY_TEST_ROCQ_VERSION=9.3.0 PGY_TEST_CHECKER_VERSION=9.0.1 PGY_TEST_STDLIB_VERSION=9.2.0 \
    bash -c 'source "$1"; pgy_rocq_require' bash "$ROOT_DIR/scripts/rocq_toolchain_owner.sh" >"$work/checker.log" 2>&1; then
    echo '[rocq-toolchain-selftest] accepted mismatched kernel checker' >&2; exit 1
fi
grep -Fq 'expected checker' "$work/checker.log"
echo '[rocq-toolchain-selftest] stable pair admitted; five wrong versions, wrong Stdlib and mismatched checker refused'
