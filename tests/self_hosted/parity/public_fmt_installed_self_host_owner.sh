#!/usr/bin/env bash
# Public fmt must consume one verified Pergyra layout artifact. C retains only
# stdout, comparison, and atomic host publication; native lex/parse/layout is
# forbidden after the installed boundary.
# Registry fallback inventory exercised below: forged token kind accepted;
# valid use or lifecycle surface rejected;
# final source identity fell back after proof failure;
# rollback failure deleted a displaced concurrent edit.

set -euo pipefail

ROOT_DIR="${ROOT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)}"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_DIR="$ROOT_DIR/.tmp/self_hosted/public_fmt_installed_self_host"
COUNT_FILE="$WORK_DIR/count.txt"
FMT_ADAPTER="$ROOT_DIR/src/compiler/fmt.c"
FMT_HANDOFF="$ROOT_DIR/src/compiler/self_host_fmt_driver.c"
PATH_OWNER="$ROOT_DIR/src/compiler/path_utils.c"
LEXER_OWNER="$ROOT_DIR/src/self_hosted/lexer/scan_owner.pgy"
TOKEN_OWNER="$ROOT_DIR/src/self_hosted/lexer/token_owner.pgy"
LAYOUT_OWNER="$ROOT_DIR/src/self_hosted/fmt/layout_owner.pgy"
SESSION_OWNER="$ROOT_DIR/src/self_hosted/fmt/session_owner.pgy"
CLI_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
EXEC_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy"
READ_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy"

fail() {
    echo "[self-host-public-fmt] $*" >&2
    exit 1
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${DRIVER}.exe"; then
    DRIVER="${DRIVER}.exe"
fi
[[ -x "$PGY" ]] || fail "missing public pgy launcher: $PGY"
[[ -x "$DRIVER" ]] || fail "missing installed self-host driver: $DRIVER"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"

mkdir -p "$WORK_DIR"
WORK_TMPDIR="$WORK_DIR"
if pgy_binary_expects_windows_paths "$PGY"; then
    WORK_TMPDIR="$(pgy_path_for_compiler "$PGY" "$WORK_DIR")"
    WORK_TMPDIR="${WORK_TMPDIR//\\//}"
fi
export TMPDIR="$WORK_TMPDIR"
export TEMP="$WORK_TMPDIR"
export TMP="$WORK_TMPDIR"
rm -f "$WORK_DIR"/*.pgy "$WORK_DIR"/*.tmp "$WORK_DIR"/*.out \
    "$WORK_DIR"/*.err "$WORK_DIR"/*.txt "$WORK_DIR"/*.exe
rm -rf "$WORK_DIR"/pgy-* "$WORK_DIR/package-argv"

SOURCE="$WORK_DIR/fmt_case.pgy"
INVALID="$WORK_DIR/fmt_invalid.pgy"
INVALID_LEXEME="$WORK_DIR/fmt_invalid_lexeme.pgy"
UNCLOSED_COMMENT="$WORK_DIR/fmt_unclosed_comment.pgy"
LOSSLESS="$WORK_DIR/fmt_lossless_tokens.pgy"
USE_SURFACE="$WORK_DIR/fmt_use_surface.pgy"
LIFECYCLE_SURFACE="$WORK_DIR/fmt_lifecycle_surface.pgy"
cat >"$SOURCE" <<'EOF'
func Add(a:Int,b:Int)->Int{return a+b;}
func Main()->Void{if(true){Log(Add(1,2));}}
EOF
cat >"$INVALID" <<'EOF'
func Main( -> Void { Log(1); }
EOF
cat >"$INVALID_LEXEME" <<'EOF'
func Main() -> Void { ~ }
EOF
cat >"$UNCLOSED_COMMENT" <<'EOF'
func Main() -> Void {} /*
EOF
cat >"$LOSSLESS" <<'EOF'
/// formatter keeps documentation
func Main() -> Void { let s: String = $"value={1}"; Log(s); }
EOF
ORIGINAL="$(cat "$SOURCE")"
INVALID_ORIGINAL="$(cat "$INVALID")"
INVALID_LEXEME_ORIGINAL="$(cat "$INVALID_LEXEME")"
UNCLOSED_COMMENT_ORIGINAL="$(cat "$UNCLOSED_COMMENT")"
SOURCE_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$SOURCE")"
INVALID_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$INVALID")"
DRIVER_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
INVALID_LEXEME_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$INVALID_LEXEME")"
UNCLOSED_COMMENT_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$UNCLOSED_COMMENT")"
LOSSLESS_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$LOSSLESS")"
cp "$ROOT_DIR/examples/calendar_working/main.pgy" "$USE_SURFACE"
cp "$ROOT_DIR/tests/air_erasure/fixtures/06_lifecycle_branch.pgy" \
    "$LIFECYCLE_SURFACE"
cp "$USE_SURFACE" "$WORK_DIR/fmt_use_surface.original"
cp "$LIFECYCLE_SURFACE" "$WORK_DIR/fmt_lifecycle_surface.original"
USE_SURFACE_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$USE_SURFACE")"
LIFECYCLE_SURFACE_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$LIFECYCLE_SURFACE")"

PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt "$SOURCE_FOR_PGY" >"$WORK_DIR/stdout.out" \
    2>"$WORK_DIR/stdout.err" || fail "installed formatter stdout failed"
cat >"$WORK_DIR/expected.out" <<'EOF'
func Add(a: Int, b: Int) -> Int
{
    return a + b;
}

func Main() -> Void
{
    if (true)
    {
        Log(Add(1, 2));
    }
}
EOF
EXPECTED_STDOUT="$WORK_DIR/expected-stdout.out"
if pgy_binary_expects_windows_paths "$PGY"; then
    awk '{ printf "%s\r\n", $0 }' "$WORK_DIR/expected.out" >"$EXPECTED_STDOUT"
else
    cp "$WORK_DIR/expected.out" "$EXPECTED_STDOUT"
fi
cmp -s "$EXPECTED_STDOUT" "$WORK_DIR/stdout.out" ||
    fail "installed formatter changed the public stdout artifact"
[[ "$(cat "$SOURCE")" == "$ORIGINAL" ]] ||
    fail "stdout mode mutated the source"
[[ ! -e "$SOURCE.fmt.tmp" ]] || fail "stdout mode left a temporary artifact"

PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt "$LOSSLESS_FOR_PGY" >"$WORK_DIR/lossless.out" \
    2>"$WORK_DIR/lossless.err" || fail "lossless token formatting failed"
grep -Fq '/// formatter keeps documentation' "$WORK_DIR/lossless.out" ||
    fail "formatter dropped the exact doc-comment lexeme"
grep -Fq '$"value={1}"' "$WORK_DIR/lossless.out" ||
    fail "formatter dropped the interpolated-string prefix"

for surface_pair in \
    "$USE_SURFACE_FOR_PGY|$USE_SURFACE|fmt_use_surface|use" \
    "$LIFECYCLE_SURFACE_FOR_PGY|$LIFECYCLE_SURFACE|fmt_lifecycle_surface|lifecycle"; do
    IFS='|' read -r surface_for_pgy surface_path surface_stem surface_label \
        <<<"$surface_pair"
    PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
        "$PGY" fmt "$surface_for_pgy" >"$WORK_DIR/$surface_stem.once.pgy" \
        2>"$WORK_DIR/$surface_stem.once.err" ||
        fail "valid $surface_label surface was refused by formatter admission"
    surface_once_for_pgy="$(pgy_path_for_compiler \
        "$PGY" "$WORK_DIR/$surface_stem.once.pgy")"
    PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
        "$PGY" fmt "$surface_once_for_pgy" \
        >"$WORK_DIR/$surface_stem.twice.pgy" \
        2>"$WORK_DIR/$surface_stem.twice.err" ||
        fail "formatted $surface_label surface was not re-admitted"
    cmp -s "$WORK_DIR/$surface_stem.once.pgy" \
        "$WORK_DIR/$surface_stem.twice.pgy" ||
        fail "$surface_label surface formatting is not exactly stable"
    cmp -s "$surface_path" "$WORK_DIR/$surface_stem.original" ||
        fail "$surface_label stdout formatting mutated its source"
done

set +e
PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt "$SOURCE_FOR_PGY" --check >"$WORK_DIR/check-before.out" \
    2>"$WORK_DIR/check-before.err"
check_before_rc=$?
set -e
[[ "$check_before_rc" -ne 0 ]] || fail "unformatted source passed --check"
grep -Fq 'needs formatting' "$WORK_DIR/check-before.err" ||
    fail "--check lost its explicit negative receipt"

PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt "$SOURCE_FOR_PGY" --write >"$WORK_DIR/write-first.out" \
    2>"$WORK_DIR/write-first.err" || fail "first --write failed"
cmp -s "$WORK_DIR/expected.out" "$SOURCE" ||
    fail "--write did not publish the verified artifact"
grep -Fq 'formatted' "$WORK_DIR/write-first.out" ||
    fail "first --write lost its receipt"
PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt "$SOURCE_FOR_PGY" --check ||
    fail "formatted source failed --check"
PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt "$SOURCE_FOR_PGY" --write >"$WORK_DIR/write-second.out" \
    2>"$WORK_DIR/write-second.err" || fail "second --write failed"
grep -Fq 'already formatted' "$WORK_DIR/write-second.out" ||
    fail "second --write lost idempotence receipt"
COMPILED="$WORK_DIR/fmt_case.out"
COMPILED_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$COMPILED")"
PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" "$SOURCE_FOR_PGY" --backend=c -o "$COMPILED_FOR_PGY" \
    >"$WORK_DIR/compile.out" 2>"$WORK_DIR/compile.err" ||
    fail "formatted artifact did not compile through the installed compiler"
if [[ "$PGY" == *.exe && ! -e "$COMPILED" && -e "${COMPILED}.exe" ]]; then
    COMPILED="${COMPILED}.exe"
fi
[[ -e "$COMPILED" ]] || fail "formatted compile produced no binary artifact"

counting_driver="$WORK_DIR/counting-self-fmt-driver"
if [[ "$PGY" == *.exe ]]; then counting_driver="${counting_driver}.exe"; fi
"$CC" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_self_host_fmt_driver.c" \
    -o "$counting_driver"
COUNTING_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$counting_driver")"
COUNT_FILE_FOR_DRIVER="$(pgy_path_for_compiler "$PGY" "$COUNT_FILE")"
PGY_SELF_DRIVER_BIN="$COUNTING_FOR_PGY" \
PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
    "$PGY" fmt "$SOURCE_FOR_PGY" >"$WORK_DIR/counting.out" \
    2>"$WORK_DIR/counting.err" || fail "counting formatter failed"
[[ "$(wc -l <"$COUNT_FILE" | tr -d ' ')" == "1" ]] ||
    fail "one public format request did not invoke exactly one installed owner"
grep -Fq 'func Main() -> Void' "$WORK_DIR/counting.out" ||
    fail "public adapter did not consume the installed artifact"

CONFLICT="$WORK_DIR/fmt_conflict.pgy"
cp "$WORK_DIR/expected.out" "$CONFLICT"
CONFLICT_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$CONFLICT")"
set +e
PGY_SELF_DRIVER_BIN="$COUNTING_FOR_PGY" \
PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
PGY_SELF_DRIVER_MUTATE_SOURCE=1 \
    "$PGY" fmt "$CONFLICT_FOR_PGY" --write \
    >"$WORK_DIR/conflict.out" 2>"$WORK_DIR/conflict.err"
conflict_rc=$?
set -e
[[ "$conflict_rc" -ne 0 ]] || fail "stale formatter artifact overwrote a concurrent edit"
grep -Fq 'source changed while formatting' "$WORK_DIR/conflict.err" ||
    fail "source conflict lost its explicit diagnostic"
grep -Fq 'func ConcurrentEdit() -> Void' "$CONFLICT" ||
    fail "source conflict did not preserve the concurrent edit"
[[ ! -s "$WORK_DIR/conflict.out" ]] ||
    fail "source conflict emitted partial stdout"

for invalid_pair in \
    "$INVALID_LEXEME_FOR_PGY|$INVALID_LEXEME|$INVALID_LEXEME_ORIGINAL|invalid-lexeme" \
    "$UNCLOSED_COMMENT_FOR_PGY|$UNCLOSED_COMMENT|$UNCLOSED_COMMENT_ORIGINAL|unclosed-comment"; do
    IFS='|' read -r invalid_for_pgy invalid_path invalid_original invalid_label \
        <<<"$invalid_pair"
    set +e
    PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
        "$PGY" fmt "$invalid_for_pgy" --write \
        >"$WORK_DIR/$invalid_label.out" 2>"$WORK_DIR/$invalid_label.err"
    invalid_token_rc=$?
    set -e
    [[ "$invalid_token_rc" -ne 0 ]] ||
        fail "$invalid_label was silently deleted into a valid program"
    [[ "$(cat "$invalid_path")" == "$invalid_original" ]] ||
        fail "$invalid_label failure mutated its source"
    [[ ! -s "$WORK_DIR/$invalid_label.out" ]] ||
        fail "$invalid_label failure emitted partial stdout"
done

MISSING_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/missing-driver")"
SENTINEL="$SOURCE.fmt.tmp"
printf '%s\n' 'user-owned-sentinel' >"$SENTINEL"
set +e
PGY_SELF_DRIVER_BIN="$MISSING_FOR_PGY" \
    "$PGY" fmt "$SOURCE_FOR_PGY" --write >"$WORK_DIR/missing.out" \
    2>"$WORK_DIR/missing.err"
missing_rc=$?
PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt "$INVALID_FOR_PGY" --write >"$WORK_DIR/invalid.out" \
    2>"$WORK_DIR/invalid.err"
invalid_rc=$?
set -e
[[ "$missing_rc" -ne 0 ]] || fail "missing installed formatter retried natively"
[[ "$invalid_rc" -ne 0 ]] || fail "invalid source was formatted successfully"
[[ ! -s "$WORK_DIR/missing.out" && ! -s "$WORK_DIR/invalid.out" ]] ||
    fail "failed formatting emitted partial stdout"
grep -Fq 'self-host driver is unavailable' "$WORK_DIR/missing.err" ||
    fail "missing owner lost its explicit diagnostic"
[[ "$(cat "$SENTINEL")" == "user-owned-sentinel" ]] ||
    fail "formatter deleted or replaced a user-owned suffix collision"
cmp -s "$SOURCE" "$WORK_DIR/expected.out" ||
    fail "missing owner mutated the valid source"
[[ "$(cat "$INVALID")" == "$INVALID_ORIGINAL" ]] ||
    fail "invalid formatting mutated its source"
[[ ! -e "$INVALID.fmt.tmp" ]] ||
    fail "failed formatting left a fixed-name temporary artifact"
if find "$WORK_DIR" -maxdepth 1 -type d -name 'pgy-*' -print -quit |
    grep -q .; then
    fail "formatter left a private artifact workspace"
fi

set +e
PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt "$SOURCE_FOR_PGY" "$INVALID_FOR_PGY" --write \
    >"$WORK_DIR/multi-operand.out" 2>"$WORK_DIR/multi-operand.err"
multi_operand_rc=$?
PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt "$SOURCE_FOR_PGY" --bogus \
    >"$WORK_DIR/unknown-option.out" 2>"$WORK_DIR/unknown-option.err"
unknown_option_rc=$?
PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt "$SOURCE_FOR_PGY" --write --check \
    >"$WORK_DIR/conflicting-mode.out" 2>"$WORK_DIR/conflicting-mode.err"
conflicting_mode_rc=$?
set -e
[[ "$multi_operand_rc" -ne 0 && "$unknown_option_rc" -ne 0 &&
   "$conflicting_mode_rc" -ne 0 ]] ||
    fail "formatter argv admission accepted an ambiguous request"
cmp -s "$SOURCE" "$WORK_DIR/expected.out" ||
    fail "ambiguous argv admission mutated the valid source"
[[ "$(cat "$INVALID")" == "$INVALID_ORIGINAL" ]] ||
    fail "ambiguous argv admission mutated another operand"

PACKAGE_DIR="$WORK_DIR/package-argv"
mkdir -p "$PACKAGE_DIR"
(cd "$PACKAGE_DIR" && "$PGY" init fmt-package >/dev/null 2>&1) ||
    fail "could not create formatter package argv fixture"
cat >"$PACKAGE_DIR/main.pgy" <<'EOF'
func Main()->Void{Log(1);}
EOF
cp "$PACKAGE_DIR/main.pgy" "$PACKAGE_DIR/main.original"
set +e
(cd "$PACKAGE_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt --write --check >conflicting-package.out \
    2>conflicting-package.err)
conflicting_package_rc=$?
(cd "$PACKAGE_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
    "$PGY" fmt --write --write >duplicate-package.out \
    2>duplicate-package.err)
duplicate_package_rc=$?
set -e
[[ "$conflicting_package_rc" -ne 0 && "$duplicate_package_rc" -ne 0 ]] ||
    fail "package formatter accepted conflicting or duplicate options"
cmp -s "$PACKAGE_DIR/main.pgy" "$PACKAGE_DIR/main.original" ||
    fail "ambiguous package formatter argv mutated the entry source"

atomic_owner="$WORK_DIR/path-atomic-replace-owner"
if [[ "$PGY" == *.exe ]]; then atomic_owner="${atomic_owner}.exe"; fi
"$CC" -std=c11 -Wall -Wextra -Werror \
    -DPGY_PATH_REPLACE_TEST_HOOKS \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/path_atomic_replace_owner.c" \
    "$ROOT_DIR/src/compiler/path_utils.c" -o "$atomic_owner"
"$atomic_owner" "$WORK_DIR/atomic-dst.pgy" \
    "$WORK_DIR/atomic-tmp.pgy" "$WORK_DIR/atomic-backup.pgy" ||
    fail "atomic publication did not preserve a commit-time concurrent edit"

recovery_owner="$WORK_DIR/fmt-recovery-publication-owner"
if [[ "$PGY" == *.exe ]]; then recovery_owner="${recovery_owner}.exe"; fi
"$CC" -std=c11 -Wall -Wextra -Werror \
    -DPGY_PATH_REPLACE_TEST_HOOKS \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/fmt_recovery_publication_owner.c" \
    "$ROOT_DIR/src/compiler/fmt.c" \
    "$ROOT_DIR/src/compiler/path_utils.c" \
    "$ROOT_DIR/src/compiler/compiler_transient_artifact_workspace.c" \
    -o "$recovery_owner"
set +e
"$recovery_owner" "$WORK_DIR/recovery-source.pgy" \
    >"$WORK_DIR/recovery.out" 2>"$WORK_DIR/recovery.err"
recovery_rc=$?
set -e
[[ "$recovery_rc" -ne 0 ]] ||
    fail "commit-time formatter race reported success"
recovery_dir="$(sed -n "s/.*artifacts preserved at '\([^']*\)'.*/\1/p" \
    "$WORK_DIR/recovery.err")"
[[ -n "$recovery_dir" && -d "$recovery_dir" ]] ||
    fail "commit-time formatter race did not report a preserved workspace"
grep -R -Fq 'func ConcurrentEdit() -> Void {}' "$recovery_dir" ||
    fail "preserved formatter workspace lost the concurrent edit"

if [[ "$PGY" != *.exe ]] && command -v ln >/dev/null 2>&1; then
    MODE_SOURCE="$WORK_DIR/fmt_mode_source.pgy"
    LINK_SOURCE="$WORK_DIR/fmt_mode_link.pgy"
    cp "$WORK_DIR/expected.out" "$MODE_SOURCE"
    printf '%s\n' 'func Mode()->Void{Log(1);}' >"$MODE_SOURCE"
    chmod 600 "$MODE_SOURCE"
    ln -s "$(basename "$MODE_SOURCE")" "$LINK_SOURCE"
    LINK_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$LINK_SOURCE")"
    PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" \
        "$PGY" fmt "$LINK_FOR_PGY" --write \
        >"$WORK_DIR/symlink.out" 2>"$WORK_DIR/symlink.err" ||
        fail "symlink-target formatting failed"
    [[ -L "$LINK_SOURCE" ]] || fail "formatter replaced the source symlink inode"
    if stat -c '%a' "$MODE_SOURCE" >/dev/null 2>&1; then
        mode_bits="$(stat -c '%a' "$MODE_SOURCE")"
    else
        mode_bits="$(stat -f '%Lp' "$MODE_SOURCE")"
    fi
    [[ "$mode_bits" == "600" ]] ||
        fail "atomic replacement changed source permission bits"
fi

[[ "$(grep -Fc 'driver_materialize_self_host_format_artifact(' \
    "$FMT_ADAPTER")" == "1" ]] ||
    fail "C formatter must enter exactly one installed artifact boundary"
for forbidden in lexer_create lexer_next_token parser_parse_program \
    format_source_to_stream fmt_token_needs_space fmt_token_starts_toplevel_decl; do
    ! grep -Fq "$forbidden" "$FMT_ADAPTER" ||
        fail "C formatter retained native policy: $forbidden"
    ! grep -Fq "$forbidden" "$FMT_HANDOFF" ||
        fail "formatter handoff retained native policy: $forbidden"
done
for retired in fmt_io.c fmt_io.h fmt_layout.c fmt_layout.h; do
    [[ ! -e "$ROOT_DIR/src/compiler/$retired" ]] ||
        fail "retired native formatter owner returned: $retired"
done
grep -Fq 'compiler_transient_artifact_workspace_open(' "$FMT_ADAPTER" ||
    fail "formatter does not use a private artifact workspace"
grep -Fq 'driver_self_host_source_identity_path_dup(path)' "$FMT_ADAPTER" ||
    fail "formatter does not commit to the canonical source identity"
grep -Fq 'source changed while formatting' "$FMT_ADAPTER" ||
    fail "formatter does not reject stale source snapshots"
grep -Fq 'pgy_exec_argv_capture_stdout(' "$FMT_HANDOFF" ||
    fail "formatter child diagnostics can leak onto public stdout"
! grep -Fq 'pgy_exec_argv(child_argv, false)' "$FMT_HANDOFF" ||
    fail "formatter restored inherited child stdout"
grep -Fq 'path_replace_file_atomic_if_unchanged(' \
    "$FMT_ADAPTER" || fail "host publication is not canonical and atomic"
! grep -Fq 'remove(dst_path)' "$FMT_ADAPTER" ||
    fail "formatter restored remove-then-rename publication"
! grep -Fq 'remove(dst_path)' "$PATH_OWNER" ||
    fail "atomic replace owner restored remove-then-rename publication"
grep -Fq 'ReplaceFileA(dst_path, tmp_path, backup_path, 0, NULL, NULL)' \
    "$PATH_OWNER" || fail "Windows metadata-preserving replacement is missing"
grep -Fq 'chmod(tmp_path, destination.st_mode & 07777)' "$PATH_OWNER" ||
    fail "POSIX replacement does not preserve permission bits"
grep -Fq 'path_exchange_files_atomic(tmp_path, dst_path)' "$PATH_OWNER" ||
    fail "POSIX commit-time exchange contract is missing"
precheck_line="$(grep -n -m1 \
    'path_file_content_equals(dst_path, expected_content)' \
    "$PATH_OWNER" | cut -d: -f1)"
exchange_line="$(grep -n -m1 \
    'path_exchange_files_atomic(tmp_path, dst_path)' \
    "$PATH_OWNER" | cut -d: -f1)"
[[ -n "$precheck_line" && -n "$exchange_line" &&
    "$precheck_line" -lt "$exchange_line" ]] ||
    fail "publication exposes formatted bytes before rejecting an observed conflict"
grep -Fq 'path_file_content_equals(tmp_path, expected_content)' "$PATH_OWNER" ||
    fail "POSIX publication does not verify the displaced source snapshot"
grep -Fq 'PATH_REPLACE_RECOVERY_REQUIRED' "$FMT_ADAPTER" ||
    fail "commit-time races have no explicit recovery outcome"
grep -Fq 'if (!preserve_workspace)' "$FMT_ADAPTER" ||
    fail "formatter cleanup can delete a preserved concurrent edit"
grep -Fq 'GetFinalPathNameByHandleA(' \
    "$ROOT_DIR/src/compiler/import_resolver_paths.c" ||
    fail "Windows source identity does not resolve the final reparse target"
grep -Fq 'import_resolver_existing_final_identity_path_dup(source_path)' \
    "$ROOT_DIR/src/compiler/self_host_driver.c" ||
    fail "formatter source identity can fall back after final-target proof fails"

grep -Fq 'struct LexerTokenFact {' "$TOKEN_OWNER" ||
    fail "typed lexer token fact owner is missing"
grep -Fq 'source_text: String;' "$TOKEN_OWNER" ||
    fail "typed lexer facts do not preserve exact source lexemes"
grep -Fq 'func LexerTokenFactsReady(' "$TOKEN_OWNER" ||
    fail "typed lexer stream has no completeness admission"
grep -Fq 'func LexerTokenFactExactLexemeReady(' "$TOKEN_OWNER" ||
    fail "typed lexer facts do not cross-check normalized and exact lexemes"
grep -Fq '"DOC_COMMENT", "safe", "Log(1);"' "$TOKEN_OWNER" ||
    fail "typed lexer facts lack the forged doc-comment negative case"
grep -Fq '1, "}", "Log(1);", "Log(1);", 1, 1' "$TOKEN_OWNER" ||
    fail "typed lexer facts lack the forged structural-token negative case"
grep -Fq 'LexerTokenExactLexemeContractReady()' "$LAYOUT_OWNER" ||
    fail "formatter does not execute the exact-lexeme negative contract"
grep -Fq 'lifecycle_transition_indent' \
    "$ROOT_DIR/src/self_hosted/hir/ast_text_inventory_owner.pgy" ||
    fail "lifecycle transition admission is not scoped to its typed parent"
! grep -Fq 'if StringIndexOf(text, ": ") > 0 && StringIndexOf(text, " -> ") > 0' \
    "$ROOT_DIR/src/self_hosted/hir/ast_text_inventory_owner.pgy" ||
    fail "lifecycle transition identity escaped into global text classification"
grep -Fq 'func LexContentFacts(content: String) -> Array<LexerTokenFact>' \
    "$LEXER_OWNER" || fail "lexer scan does not publish typed facts"
grep -Fq 'LexerTokenFactsText(' "$LEXER_OWNER" ||
    fail "public token text is not a typed-fact projection"
grep -Fq 'source_path, LexerScanContentFacts(content, emit_json_receipt)' \
    "$LEXER_OWNER" || fail "public token text bypasses typed lexer facts"
grep -Fq 'FormatSourceFromTokenFacts(LexContentFacts(source))' \
    "$SESSION_OWNER" || fail "formatter does not consume typed token facts"
! grep -Fq 'LexContent(' "$LAYOUT_OWNER" ||
    fail "formatter reparses the token debug serialization"
[[ "$(grep -Fc 'FormatSourceFromTokenFacts(' "$SESSION_OWNER")" == "2" ]] ||
    fail "formatter must prove one output and one stability roundtrip"
grep -Fq 'ParseFormatProgramBuildContent(' "$SESSION_OWNER" ||
    fail "formatted bytes are not parser-admitted before publication"
grep -Fq 'AstTreeArtifactReady(artifact)' "$SESSION_OWNER" ||
    fail "formatter parser admission lacks typed artifact readiness"
parse_line="$(grep -n -m1 'ParseFormatProgramBuildContent(' \
    "$SESSION_OWNER" | cut -d: -f1)"
write_line="$(grep -n -m1 'WriteFile(output_path, formatted)' \
    "$SESSION_OWNER" | cut -d: -f1)"
[[ -n "$parse_line" && -n "$write_line" && "$parse_line" -lt "$write_line" ]] ||
    fail "formatter published output before parser artifact admission"
grep -Fq 'DriverCliFormatSourceArtifact(String, String)' "$CLI_OWNER" ||
    fail "installed CLI request owner lacks formatter identity"
grep -Fq 'case DriverCliFormatSourceArtifact(source_path, output_path):' \
    "$EXEC_OWNER" || fail "installed root does not execute formatter identity"
grep -Fq 'format source request requires installed composition root' \
    "$READ_OWNER" || fail "read-only root does not reject formatter publication"

# CLOSED-registry fallback receipts:
# invalid source character deletion
# lossy interpolated-string prefix
# unadmitted formatter artifact publication
# fixed source-suffix temporary artifact deletion
# stale source overwrite

echo "[self-host-public-fmt] Pergyra token/layout owner substitutes public formatting"
