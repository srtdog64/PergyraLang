#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT_DIR/tests/cases/memory_adversarial/manifest.tsv"
MAKEFILE="$ROOT_DIR/Makefile"
WORK_BASE="$ROOT_DIR/.tmp/memory-adversarial-catalog"
mkdir -p "$ROOT_DIR/.tmp"
WORK_DIR="$(mktemp -d "$WORK_BASE.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
IDS="$WORK_DIR/ids.txt"
: >"$IDS"

header="$(awk '!/^#/ { print; exit }' "$MANIFEST")"
header="${header%$'\r'}"
expected_header='id|hazard|surface|expected|oracle|fixture|gate|status'
if [[ "$header" != "$expected_header" ]]; then
    echo "[memory-adversarial] manifest header drifted" >&2
    exit 1
fi

rows=0
closed=0
open=0
partial=0
slot_read=0
slot_write=0
slot_double=0
c_uaf=0
c_race=0

while IFS='|' read -r id hazard surface expected oracle fixture gate status; do
    status="${status%$'\r'}"
    [[ -z "$id" || "$id" == \#* || "$id" == "id" ]] && continue
    rows=$((rows + 1))
    printf '%s\n' "$id" >>"$IDS"

    case "$status" in
        CLOSED) closed=$((closed + 1)) ;;
        PARTIAL) partial=$((partial + 1)) ;;
        OPEN) open=$((open + 1)) ;;
        *)
            echo "[memory-adversarial] unknown status '$status' for $id" >&2
            exit 1
            ;;
    esac
    case "$oracle" in
        SEMANTIC_FACT|MIR_FACT|RUNTIME_GUARD|SANITIZER_WITNESS|NONE) ;;
        *)
            echo "[memory-adversarial] unknown oracle '$oracle' for $id" >&2
            exit 1
            ;;
    esac
    if [[ "$oracle" == "C_EXECUTION_RESULT" ]]; then
        echo "[memory-adversarial] C UB execution cannot own a safety verdict" >&2
        exit 1
    fi

    if [[ "$status" == "CLOSED" || "$status" == "PARTIAL" ]]; then
        if [[ "$fixture" == "-" || ! -f "$ROOT_DIR/$fixture" ]]; then
            echo "[memory-adversarial] $status row $id has no live fixture" >&2
            exit 1
        fi
        if [[ "$gate" == "-" ]] || ! grep -Eq "^${gate}[[:space:]]*:" "$MAKEFILE"; then
            echo "[memory-adversarial] $status row $id has no Makefile gate '$gate'" >&2
            exit 1
        fi
    elif [[ "$fixture" != "-" || "$gate" != "-" || "$oracle" != "NONE" ]]; then
        echo "[memory-adversarial] OPEN row $id must not imply landed evidence" >&2
        exit 1
    fi

    [[ "$id" == "slot_read_after_release" && "$expected" == "REJECT_COMPILE" ]] && slot_read=1
    [[ "$id" == "slot_write_after_release" && "$expected" == "REJECT_COMPILE" ]] && slot_write=1
    [[ "$id" == "slot_double_release" && "$expected" == "REJECT_COMPILE" ]] && slot_double=1
    [[ "$id" == "c_heap_uaf_witness" && "$expected" == "DETECT_WITNESS" ]] && c_uaf=1
    [[ "$id" == "c_pthread_race_witness" && "$expected" == "DETECT_WITNESS" ]] && c_race=1
done <"$MANIFEST"

duplicates="$(sort "$IDS" | uniq -d)"
if [[ -n "$duplicates" ]]; then
    echo "[memory-adversarial] duplicate ids: $duplicates" >&2
    exit 1
fi
if [[ "$rows" -lt 20 || "$closed" -lt 15 || "$open" -lt 5 ]]; then
    echo "[memory-adversarial] corpus coverage shrank: rows=$rows closed=$closed partial=$partial open=$open" >&2
    exit 1
fi
if [[ "$slot_read" -ne 1 || "$slot_write" -ne 1 || "$slot_double" -ne 1 ||
      "$c_uaf" -ne 1 || "$c_race" -ne 1 ]]; then
    echo "[memory-adversarial] foundational UAF/double-release/race witnesses are missing" >&2
    exit 1
fi

grep -Fq 'sanitizers-linux:' "$ROOT_DIR/.github/workflows/ci.yml" || {
    echo "[memory-adversarial] Linux sanitizer CI job is not wired" >&2
    exit 1
}
grep -Fq 'make LLVM_ENABLED=0 PGY_ASAN_CASES=40 test-asan' \
    "$ROOT_DIR/.github/workflows/ci.yml" || {
    echo "[memory-adversarial] Linux sanitizer job does not run the bounded battery" >&2
    exit 1
}
grep -Fq 'tsan-linux:' "$ROOT_DIR/.github/workflows/ci.yml" || {
    echo "[memory-adversarial] Linux TSan CI job is not wired" >&2
    exit 1
}
grep -Fq 'make LLVM_ENABLED=0 test-tsan' "$ROOT_DIR/.github/workflows/ci.yml" || {
    echo "[memory-adversarial] Linux TSan job does not run the concurrency battery" >&2
    exit 1
}

echo "[memory-adversarial] rows=$rows closed=$closed partial=$partial open=$open; UAF/race owners and sanitizer calibration wired"
