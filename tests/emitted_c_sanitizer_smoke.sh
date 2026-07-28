#!/usr/bin/env bash
# Run EMITTED programs under ASan+UBSan.
#
# sanitizer_compile_smoke.sh asks whether the COMPILER commits UB while
# compiling. Nothing asked the other half: does the code the compiler EMITS
# commit UB when it runs. That surface had never been measured, and an
# unanswered safety question is not a pass -- it is an unmeasured one.
#
# Why it matters here specifically. Pergyra lowers to C, so the language-level
# safety claims are delivered THROUGH emitted C; a language-level proof does not
# save you from an out-of-bounds store in the lowering. And backend_compare
# checks the C output against the LLVM output -- a differential oracle, so UB
# present in BOTH lowerings passes parity unnoticed. A sanitizer is an
# INDEPENDENT judge, which is exactly what that comparison lacks.
#
# Two classic emitted-C traps are already defused by construction in
# compiler.c (-fwrapv for signed overflow, -fno-strict-aliasing for slot/witness
# pointer punning), so this gate is aimed at what those do NOT cover:
# out-of-bounds access, use-after-free, null/misaligned pointer use, bad shift
# widths, division by zero, invalid bool/enum loads, pointer overflow.
#
# Mechanism: pgy picks its C compiler from PGY_CC, but its parser keeps only the
# command word and --target= -- extra flags in PGY_CC are DISCARDED. So the
# sanitizer flags ride in on a wrapper script used as PGY_CC, which pgy execs
# for both the compile and the link step.
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
CASES_DIR="$ROOT_DIR/tests/cases/backend_compare"
WORK_DIR="$ROOT_DIR/.tmp/emitted-c-sanitizer"
CASE_LIMIT="${PGY_EMITTED_SAN_CASES:-40}"

# The compiler under test is the ordinary one -- this gate sanitizes the
# EMITTED program, not pgy itself (that is sanitizer_compile_smoke.sh's job).
if [[ ! -x "$PGY" ]]; then
    echo "[emitted-c-sanitizer] no compiler at $PGY" >&2
    echo "  Build one with: make" >&2
    exit 1
fi

if [[ ! -d "$CASES_DIR" ]]; then
    echo "[emitted-c-sanitizer] missing case corpus: $CASES_DIR" >&2
    exit 1
fi

REAL_CC="${PGY_SAN_CC:-${CC:-cc}}"
if ! command -v "$REAL_CC" >/dev/null 2>&1; then
    echo "[emitted-c-sanitizer] FAIL -- no C compiler '$REAL_CC'" >&2
    exit 1
fi

mkdir -p "$WORK_DIR"
WRAPPER="$WORK_DIR/san-cc"

# Fail closed when the toolchain cannot actually sanitize. MinGW ships no
# libasan, so on Windows this gate must report "unmeasured", never green: a
# sanitizer gate that silently compiles without sanitizers is a no-op wearing a
# green badge. Probed with a real link, because -fsanitize can be accepted at
# compile time and only fail when the runtime library is missing.
probe_c="$WORK_DIR/probe.c"
probe_exe="$WORK_DIR/probe.exe"
printf 'int main(void){return 0;}\n' > "$probe_c"
if ! "$REAL_CC" -fsanitize=undefined,address -fno-sanitize-recover=all \
        "$probe_c" -o "$probe_exe" >"$WORK_DIR/probe.log" 2>&1; then
    echo "[emitted-c-sanitizer] FAIL -- '$REAL_CC' cannot link ASan+UBSan;" >&2
    echo "  the emitted-code UB surface would go UNMEASURED behind a green gate." >&2
    echo "  Use Linux/WSL or a clang carrying the sanitizer runtimes." >&2
    sed 's/^/  | /' "$WORK_DIR/probe.log" >&2
    exit 1
fi

cat > "$WRAPPER" <<EOF
#!/usr/bin/env bash
# pgy discards extra flags in PGY_CC, so they ride in here instead.
exec "$REAL_CC" -fsanitize=undefined,address -fno-sanitize-recover=all \\
     -fno-omit-frame-pointer -g "\$@"
EOF
chmod +x "$WRAPPER"

# halt_on_error stops a run from reporting one fault and sailing on. Leaks are
# out of scope: this gate is about UB and memory errors in emitted code, and
# emitted programs are short-lived processes that legitimately exit without
# tearing every arena down.
export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"

SOURCES=()
while IFS= read -r source_path; do
    [[ -n "$source_path" ]] && SOURCES+=("$source_path")
done < <(find "$CASES_DIR" -mindepth 2 -maxdepth 2 -name main.pgy \
         | LC_ALL=C sort | head -n "$CASE_LIMIT")

# Wrapper self-check, before judging any case.
#
# pgy execs PGY_CC directly. On Linux that honours the wrapper's shebang; on
# Windows CreateProcess cannot start a non-.exe script at all, so EVERY case
# would fail to build and the run would look like a broken corpus. Distinguish
# the two by building one case both ways: if plain CC succeeds where the wrapper
# fails, the fault is this gate's mechanism on this platform, not the cases.
if [[ ${#SOURCES[@]} -gt 0 ]]; then
    probe_case="${SOURCES[0]}"
    probe_out="$WORK_DIR/wrapper-selfcheck.exe"
    if PGY_CC="$WRAPPER" "$PGY" "$probe_case" --backend=c -o "$probe_out" \
            >"$WORK_DIR/wrapper-selfcheck.log" 2>&1; then
        :
    elif PGY_CC="$REAL_CC" "$PGY" "$probe_case" --backend=c -o "$probe_out" \
            >>"$WORK_DIR/wrapper-selfcheck.log" 2>&1; then
        echo "[emitted-c-sanitizer] FAIL -- '$REAL_CC' builds this case but the" \
             "sanitizer wrapper cannot be used as PGY_CC here." >&2
        echo "  pgy execs PGY_CC directly, and this platform cannot exec a" >&2
        echo "  shebang script (Windows). The emitted-code UB surface goes" >&2
        echo "  UNMEASURED -- run this gate on Linux/WSL." >&2
        exit 1
    fi
    # Both failed: a corpus/toolchain problem, reported per-case by the loop.
fi

if [[ ${#SOURCES[@]} -eq 0 ]]; then
    echo "[emitted-c-sanitizer] FAIL -- no cases found under $CASES_DIR" >&2
    exit 1
fi

total=$(find "$CASES_DIR" -mindepth 2 -maxdepth 2 -name main.pgy | wc -l | tr -d '[:space:]')
echo "[emitted-c-sanitizer] ${#SOURCES[@]} of $total cases (cc=$REAL_CC, ASan+UBSan)"
if [[ ${#SOURCES[@]} -lt $total ]]; then
    # Never let a bounded run read as full coverage.
    echo "[emitted-c-sanitizer] NOTE: sampled, not exhaustive." \
         "Raise with PGY_EMITTED_SAN_CASES=$total"
fi

faults=0
checked=0
for source in "${SOURCES[@]}"; do
    stem="$(basename "$(dirname "$source")")"
    exe="$WORK_DIR/$stem.exe"
    log="$WORK_DIR/$stem.log"

    # A compile failure is NOT this gate's finding -- the case corpus is gated
    # elsewhere, and reporting it here would blame the sanitizer for unrelated
    # breakage. Skip loudly rather than fail or hide.
    if ! PGY_CC="$WRAPPER" "$PGY" "$source" --backend=c -o "$exe" \
            >"$log" 2>&1; then
        echo "  skip  $stem (did not build under the sanitizer wrapper)"
        continue
    fi

    checked=$((checked + 1))
    "$exe" >>"$log" 2>&1
    # The exit code alone cannot decide this: a case may exit non-zero by
    # design, and there is no expected-output file to compare against. The
    # sanitizer's own diagnostic is the signal.
    if grep -qE 'runtime error:|ERROR: AddressSanitizer|ERROR: UndefinedBehaviorSanitizer' "$log"; then
        echo "  UB    $stem" >&2
        grep -E 'runtime error:|ERROR:' "$log" | head -3 | sed 's/^/          /' >&2
        faults=$((faults + 1))
    fi
done

if [[ "$checked" -eq 0 ]]; then
    echo "[emitted-c-sanitizer] FAIL -- no case ran; nothing was measured." >&2
    exit 1
fi

if [[ "$faults" -gt 0 ]]; then
    echo "[emitted-c-sanitizer] FAIL -- $faults of $checked emitted programs" \
         "committed UB or a memory error. Logs: $WORK_DIR" >&2
    exit 1
fi

echo "[emitted-c-sanitizer] ok ($checked emitted programs ran clean under" \
     "ASan+UBSan -- an independent judge, not a backend-vs-backend comparison)"
