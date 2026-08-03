# Installed source-MIR build commit order and retired helper ratchet.

require_text "$BUILD_OWNER" 'DRIVER_SOURCE="src/self_hosted/compiler/driver_bootstrap_main.pgy"'
grep -Fq -- 'DRIVER_SOURCE="src/self_hosted/compiler/driver_rung2_main.pgy"' "$BUILD_OWNER" && fail "installed build returned to the test-only DRV-2 entrypoint"
require_text "$BUILD_OWNER" 'OUTPUT="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"'
require_text "$BUILD_OWNER" 'MSYS2_ARG_CONV_EXCL="$PGY_ARG_CONV_EXCL" "$tmp_output"'
candidate_smoke_line="$(grep -nF -- 'MSYS2_ARG_CONV_EXCL="$PGY_ARG_CONV_EXCL" "$tmp_output"' "$BUILD_OWNER" | cut -d: -f1)"
install_move_line="$(grep -nF -- 'mv -f "$tmp_output" "$OUTPUT"' "$BUILD_OWNER" | cut -d: -f1)"
stamp_line="$(grep -nF -- 'printf '\''%s\n'\'' "$build_key" >"$STAMP"' "$BUILD_OWNER" | cut -d: -f1)"
[[ -n "$candidate_smoke_line" && -n "$install_move_line" && -n "$stamp_line" &&
    "$candidate_smoke_line" -lt "$install_move_line" &&
    "$install_move_line" -lt "$stamp_line" ]] ||
    fail "self-host driver must pass candidate smoke before install/stamp commit"
require_text "$NATIVE_LAUNCHER" 'path_join_dup(directory, "pgy-self-driver")'
retired_file_helpers="$(
    find "$ROOT_DIR/src/self_hosted" -type f -name '*.pgy' \
        -exec grep -lE 'CompileSourceToMirJsonFile(Verified|PressureObserved)\(' {} + || true
)"
[[ -z "$retired_file_helpers" ]] || fail "retired source-MIR file helper definition or call returned"
source_mir_call_files="$(
    find "$ROOT_DIR/src/self_hosted" -type f -name '*.pgy' \
        -exec grep -lE 'CompileSourceToMirJson(Verified|PressureObserved)\(' {} + || true
)"
while IFS= read -r call_file; do
    [[ -n "$call_file" ]] || continue
    [[ "$call_file" == "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy" ]] ||
        fail "legacy source-MIR payload wrapper escaped its owner: ${call_file#"$ROOT_DIR/"}"
done <<<"$source_mir_call_files"
[[ "$(grep -F -c -- 'CompileSourceToMirJsonVerified(' "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy")" -eq 2 ]] ||
    fail "verified source-MIR definition/internal source-to-C consumer inventory drifted"
[[ "$(grep -F -c -- 'CompileSourceToMirJsonPressureObserved(' "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy")" -eq 1 ]] ||
    fail "pressure source-MIR definition inventory drifted"
require_text "$MIR_MANIFEST" 'examples/function_clause_order_minimal.pgy'
require_text "$ROOT_DIR/Makefile" 'function_clause_order_minimal'
