#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"

if [[ ! -x "$PGY" ]]; then
    echo "[filesystem-dirwalk] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_require_runnable_binary_here "filesystem-dirwalk" "$PGY"

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_filesystem_dirwalk.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

mkdir -p "$WORK_DIR/tree/a" "$WORK_DIR/tree/b/nested"
printf 'root\n' > "$WORK_DIR/tree/root.txt"
printf 'alpha\n' > "$WORK_DIR/tree/a/alpha.txt"
printf 'zeta\n' > "$WORK_DIR/tree/b/zeta.txt"
printf 'deep\n' > "$WORK_DIR/tree/b/nested/deep.txt"

cat > "$WORK_DIR/dirwalk_probe.pgy" <<'EOF'
func Main() -> Void {
    let files: Array<String> = DirWalk("tree");
    Log(ArrayLength(files));
    if ArrayLength(files) == 4 {
        Log(files[0]);
        Log(files[1]);
        Log(files[2]);
        Log(files[3]);
    }
}
EOF

cat > "$WORK_DIR/expected.txt" <<'EOF'
4
tree/a/alpha.txt
tree/b/nested/deep.txt
tree/b/zeta.txt
tree/root.txt
EOF

normalize_output() {
    tr -d '\r' | awk '
        /^[0-9]+ error\(s\), [0-9]+ warning\(s\)$/ { seen_summary = 1; next }
        /^pgy: compiled/ { next }
        seen_summary && length($0) > 0 { print }
    '
}

run_backend() {
    local backend="$1"
    local source_arg
    local out_bin_arg
    local raw_output
    local actual="$WORK_DIR/${backend}.actual"

    source_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/dirwalk_probe.pgy")"
    out_bin_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/dirwalk_probe_${backend}")"

    set +e
    raw_output="$(cd "$WORK_DIR" && "$PGY" "$source_arg" --backend="$backend" --run -o "$out_bin_arg" 2>&1)"
    local rc=$?
    set -e
    if [[ "$rc" -ne 0 ]]; then
        echo "[filesystem-dirwalk] backend=$backend compiler/run failed (exit=$rc)" >&2
        echo "--- output ---" >&2
        echo "$raw_output" >&2
        echo "--------------" >&2
        exit "$rc"
    fi

    printf '%s\n' "$raw_output" | normalize_output > "$actual"
    if ! diff -u "$WORK_DIR/expected.txt" "$actual" >/dev/null; then
        echo "[filesystem-dirwalk] backend=$backend deterministic walk drift" >&2
        diff -u "$WORK_DIR/expected.txt" "$actual" >&2 || true
        exit 1
    fi

    echo "[filesystem-dirwalk] backend=$backend ok"
}

BACKENDS="${PGY_FILESYSTEM_WALK_BACKENDS:-c llvm}"

for backend in $BACKENDS; do
    run_backend "$backend"
done
