#!/usr/bin/env bash
# Supported native Array/Slice path; no public raw-pointer escape is admitted.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${PGY_BIN:?Identify the fixed native compiler executable}"
: "${PGY_SOURCE_ROOT:?Identify the fixed compiler/runtime source snapshot}"
: "${OPAMROOT:?Use the existing project Rocq toolchain}"
: "${PGY_MIR_READER_BIN:?Identify the Pergyra MIR reader built from the same source snapshot}"
CLANG_BIN="${CLANG_BIN:-clang-18}"
mkdir -p "$ROOT_DIR/.tmp"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/raw-view-memory.XXXXXX")"
fail() { echo "[raw-view-memory] FAIL: $*; evidence: $WORK" >&2; exit 1; }
test -x "$PGY_BIN" && test -f "$PGY_SOURCE_ROOT/src/runtime/pgy_runtime_lib.c" || fail 'missing fixed compiler/runtime'
sha256sum "$PGY_BIN" "$PGY_MIR_READER_BIN" > "$WORK/executable.sha256"
(cd "$PGY_SOURCE_ROOT"; find src stdlib third_party grammar -type f \
  \( -name '*.c' -o -name '*.h' -o -name '*.def' -o -name '*.inc' -o -name '*.pgy' \) -print0 |
  sort -z | xargs -0 sha256sum; sha256sum Makefile) > "$WORK/source.sha256"
cp "$ROOT_DIR/tests/cases/raw_view_ownership/branch_exit.pgy" "$WORK/input.pgy"
cp "$ROOT_DIR/tests/cases/raw_view_ownership/string_cleanup_control.pgy" "$WORK/string-control.pgy"
cp "$ROOT_DIR/tests/cases/raw_view_ownership/observe_alloc.c" "$WORK/observe_alloc.c"
cp "$ROOT_DIR/tests/raw_view_mir_certificate.py" "$ROOT_DIR/tests/raw_view_memory_controls.py" "$WORK/"
cp "$ROOT_DIR/tests/raw_view_reader_controls.py" "$WORK/"
for module in OwnershipCleanCore RawPointerOwnershipClean RawPointerOwnershipTrace RawViewMemoryBridge; do
  cp "$ROOT_DIR/docs/semantics/proofs/$module.v" "$WORK/"
done
sha256sum "$WORK/"*.pgy "$WORK/observe_alloc.c" "$WORK/"*.py "$WORK/"*.v > "$WORK/consumer.sha256"
timeout 45 "$PGY_BIN" --test-native-mir-json-oracle "$WORK/input.pgy" > "$WORK/input.mir.json" 2> "$WORK/mir.err"
timeout 45 "$PGY_BIN" --native-pipeline "$WORK/input.pgy" --emit-c -o "$WORK/program.c" > "$WORK/c.compile.log" 2>&1
timeout 45 "$PGY_BIN" --native-pipeline "$WORK/input.pgy" --emit-llvm -o "$WORK/program.ll" > "$WORK/llvm.compile.log" 2>&1
(cd "$WORK"; timeout 30 "$PGY_MIR_READER_BIN" input.mir.json > reader.ast 2> reader.err) || fail 'Pergyra MIR reader execution'
[[ ! -s "$WORK/reader.err" ]] && grep -Fq 'Function: Work' "$WORK/reader.ast" || fail 'Pergyra MIR reconstruction'
python3 "$WORK/raw_view_reader_controls.py" "$WORK"
for control in missing_abi wrong_abi wrong_binding wrong_kind duplicate_edge missing_def_identity; do
  if (cd "$WORK"; timeout 30 "$PGY_MIR_READER_BIN" "reader-$control.json" > "reader-$control.out" 2> "reader-$control.err"); then
    fail "Pergyra reader accepted $control"
  fi
  grep -Fq 'MIR-LOWER ERROR:' "$WORK/reader-$control.out" || fail "Pergyra reader $control failed for another reason"
done
timeout 45 "$PGY_BIN" --test-native-mir-json-oracle "$WORK/string-control.pgy" > "$WORK/string.mir.json" 2> "$WORK/string.mir.err"
(cd "$WORK"; timeout 30 "$PGY_MIR_READER_BIN" string.mir.json > string.ast 2> string.err) || fail 'existing String cleanup reader'
python3 - "$WORK" <<'PY'
import json,sys,pathlib
work=pathlib.Path(sys.argv[1]); doc=json.loads((work/'string.mir.json').read_text())
rows=[i for r in doc['routines'] for b in r['blocks'] for i in b['instructions'] if i['kind']=='cleanup']
assert len(rows)==1 and rows[0]['collection_cleanup']['kind']==1
rows[0]['collection_cleanup']['kind']=3
(work/'string-kind-three.json').write_text(json.dumps(doc,separators=(',',':')))
PY
if (cd "$WORK"; timeout 30 "$PGY_MIR_READER_BIN" string-kind-three.json > string-kind-three.out 2> string-kind-three.err); then
  fail 'String cleanup gained scalar retirement authority'
fi
grep -Fq '[collection_cleanup]' "$WORK/string-kind-three.out" || fail 'String-kind-three refused for another reason'
python3 "$WORK/raw_view_mir_certificate.py" "$WORK/input.mir.json" "$WORK/RawViewMIRWitness.v" --self-test
PGY_COQ_PROOFS_DIR="$WORK" PGY_COQ_EXPECTED_AXIOMS='' \
  timeout 60 bash "$ROOT_DIR/scripts/run_rocq_toolchain.sh" bash "$ROOT_DIR/tests/coq_kernel_check.sh" > "$WORK/kernel.log" 2>&1 || fail 'Rocq compile/kernel check'
flags=(-O0 -g -std=gnu11 -fwrapv -fno-strict-aliasing -fsanitize=address,undefined -fno-omit-frame-pointer
  -pthread -I"$PGY_SOURCE_ROOT/src/runtime" -I"$PGY_SOURCE_ROOT/src")
timeout 60 "$CLANG_BIN" "${flags[@]}" -DPGY_LLVM_ENABLED -c \
  "$PGY_SOURCE_ROOT/src/runtime/pgy_runtime_lib.c" -o "$WORK/runtime-san.o" > "$WORK/runtime.compile.log" 2>&1
python3 "$WORK/raw_view_memory_controls.py" "$WORK"
for backend in c llvm; do
  suffix=c; extra=()
  if [[ "$backend" == llvm ]]; then suffix=ll; extra=("$WORK/runtime-san.o"); fi
  timeout 45 "$CLANG_BIN" "${flags[@]}" "$WORK/program.$suffix" "${extra[@]}" "$WORK/observe_alloc.c" \
    -Wl,--wrap=malloc,--wrap=free,--wrap=realloc -lm -o "$WORK/$backend" > "$WORK/$backend.link.log" 2>&1
  ASAN_OPTIONS=detect_leaks=1 timeout 10 "$WORK/$backend" > "$WORK/$backend.out" 2> "$WORK/$backend.memory" || fail "$backend physical execution"
  printf '75\n100\n' > "$WORK/expected.out"
  cmp "$WORK/expected.out" "$WORK/$backend.out" || fail "$backend output"
  expected='backing: allocations=2 reallocations=0 frees=2 live=0 freed_bytes=88'
  if [[ "$backend" == llvm ]]; then expected='backing: allocations=2 reallocations=4 frees=2 live=0 freed_bytes=128'; fi
  [[ "$(cat "$WORK/$backend.memory")" == "$expected" ]] || fail "$backend allocation/free observation"
  for control in suppressed premature; do
    timeout 45 "$CLANG_BIN" "${flags[@]}" "$WORK/$control.$suffix" "${extra[@]}" \
      -lm -o "$WORK/$backend-$control" > "$WORK/$backend-$control.link.log" 2>&1
    if ASAN_OPTIONS=detect_leaks=1 timeout 10 "$WORK/$backend-$control" \
      > "$WORK/$backend-$control.out" 2> "$WORK/$backend-$control.memory"; then
      fail "$backend $control control incorrectly passed"
    fi
    reason='LeakSanitizer: detected memory leaks'
    [[ "$control" != premature ]] || reason='AddressSanitizer: heap-use-after-free'
    grep -Fq "$reason" "$WORK/$backend-$control.memory" || fail "$backend $control failed for another reason"
  done
done
sha256sum -c "$WORK/executable.sha256" > "$WORK/executable.recheck"
(cd "$PGY_SOURCE_ROOT"; sha256sum -c "$WORK/source.sha256") > "$WORK/source.recheck"
sha256sum -c "$WORK/consumer.sha256" > "$WORK/consumer.recheck"
echo "[raw-view-memory] PASS: 2 CFG paths, 7 certificate and 7 Pergyra-reader corruptions refused, Rocq kernel, C/LLVM write-through and physical release, leak/UAF controls; evidence: $WORK"
