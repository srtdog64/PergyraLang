#!/usr/bin/env bash
# Owns the exact-input performance receipt for the integrated driver fixed point.
# This is measurement evidence only: it cannot authorize a cache or substitute
# for the semantic fixed-point receipt.

PGY_SELFHOST_PROFILE_OWNER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if ! declare -F pgy_selfhost_driver_receipt_hash_file >/dev/null 2>&1; then
    source "$PGY_SELFHOST_PROFILE_OWNER_DIR/self_host_driver_fixed_point_receipt_owner.sh"
fi

PGY_SELFHOST_PROFILE_ACTIVE=0
PGY_SELFHOST_PROFILE_START_MS=0
PGY_SELFHOST_PROFILE_PHASE_ROWS=""
PGY_SELFHOST_PROFILE_STAGE_ROWS=""
PGY_SELFHOST_PROFILE_RECEIPT=""
PGY_SELFHOST_PROFILE_TIME_BIN=""
PGY_SELFHOST_PROFILE_RESOURCE_SOURCE="shell-clock-only"
pgy_selfhost_fixed_point_profile_error() {
    echo "[self-host-fixed-point-profile] $*" >&2
    return 2
}
pgy_selfhost_fixed_point_profile_now_ms() {
    local raw seconds
    raw="$(date +%s%N 2>/dev/null || true)"
    if [[ "$raw" =~ ^[0-9]{13,}$ ]]; then
        printf '%s\n' "${raw:0:13}"
        return 0
    fi
    seconds="$(date +%s 2>/dev/null || true)"
    [[ "$seconds" =~ ^[0-9]+$ ]] ||
        pgy_selfhost_fixed_point_profile_error "no usable shell clock" || return 2
    printf '%s000\n' "$seconds"
}
pgy_selfhost_fixed_point_profile_begin() {
    local build_dir="$1"
    local receipt="$2"
    local probe="$build_dir/fixed-point-profile.time-probe"
    local candidate

    mkdir -p "$build_dir"
    PGY_SELFHOST_PROFILE_RECEIPT="$receipt"
    PGY_SELFHOST_PROFILE_PHASE_ROWS="${receipt}.phases.tmp"
    PGY_SELFHOST_PROFILE_STAGE_ROWS="${receipt}.stages.tmp"
    : >"$PGY_SELFHOST_PROFILE_PHASE_ROWS"
    : >"$PGY_SELFHOST_PROFILE_STAGE_ROWS"
    PGY_SELFHOST_PROFILE_START_MS="$(pgy_selfhost_fixed_point_profile_now_ms)" || return 2
    PGY_SELFHOST_PROFILE_TIME_BIN=""
    PGY_SELFHOST_PROFILE_RESOURCE_SOURCE="shell-clock-only"

    for candidate in "$(type -P gtime 2>/dev/null || true)" \
        "$(type -P time 2>/dev/null || true)"; do
        [[ -n "$candidate" ]] || continue
        rm -f "$probe"
        if "$candidate" -f 'user_seconds=%U\nsystem_seconds=%S\nmax_rss_kb=%M' \
            -o "$probe" sh -c ':' >/dev/null 2>&1 \
            && grep -Eq '^user_seconds=[0-9]+([.][0-9]+)?$' "$probe" \
            && grep -Eq '^system_seconds=[0-9]+([.][0-9]+)?$' "$probe" \
            && grep -Eq '^max_rss_kb=[0-9]+$' "$probe"; then
            PGY_SELFHOST_PROFILE_TIME_BIN="$candidate"
            PGY_SELFHOST_PROFILE_RESOURCE_SOURCE="gnu-time"
            break
        fi
    done
    rm -f "$probe"
    if [[ "${PGY_SELFHOST_PROFILE_REQUIRE_RESOURCE:-0}" == "1" \
        && "$PGY_SELFHOST_PROFILE_RESOURCE_SOURCE" != "gnu-time" ]]; then
        pgy_selfhost_fixed_point_profile_error \
            "resource metrics required but GNU-compatible time is unavailable"
        return 2
    fi
    command -v tee >/dev/null 2>&1 ||
        pgy_selfhost_fixed_point_profile_error "stage capture requires tee" || return 2
    PGY_SELFHOST_PROFILE_ACTIVE=1
}
pgy_selfhost_fixed_point_profile_seconds_to_ms() {
    awk -v value="$1" 'BEGIN { printf "%.0f", value * 1000.0 }'
}

pgy_selfhost_fixed_point_profile_run() {
    local label="$1"
    local kind="$2"
    shift 2
    if [[ "$PGY_SELFHOST_PROFILE_ACTIVE" != "1" ]]; then
        "$@"
        return $?
    fi

    local start_ms end_ms wall_ms rc
    local user_ms="unavailable" system_ms="unavailable" max_rss_kb="unavailable"
    local resource_file="${PGY_SELFHOST_PROFILE_RECEIPT}.${label}.resource.tmp"
    local user_seconds system_seconds
    start_ms="$(pgy_selfhost_fixed_point_profile_now_ms)" || return 2
    rm -f "$resource_file"
    if [[ -n "$PGY_SELFHOST_PROFILE_TIME_BIN" ]]; then
        if "$PGY_SELFHOST_PROFILE_TIME_BIN" \
            -f 'user_seconds=%U\nsystem_seconds=%S\nmax_rss_kb=%M' \
            -o "$resource_file" "$@"; then
            rc=0
        else
            rc=$?
        fi
        user_seconds="$(sed -n 's/^user_seconds=//p' "$resource_file" | tail -1)"
        system_seconds="$(sed -n 's/^system_seconds=//p' "$resource_file" | tail -1)"
        max_rss_kb="$(sed -n 's/^max_rss_kb=//p' "$resource_file" | tail -1)"
        if [[ "$user_seconds" =~ ^[0-9]+([.][0-9]+)?$ \
            && "$system_seconds" =~ ^[0-9]+([.][0-9]+)?$ \
            && "$max_rss_kb" =~ ^[0-9]+$ ]]; then
            user_ms="$(pgy_selfhost_fixed_point_profile_seconds_to_ms "$user_seconds")"
            system_ms="$(pgy_selfhost_fixed_point_profile_seconds_to_ms "$system_seconds")"
        elif [[ "${PGY_SELFHOST_PROFILE_REQUIRE_RESOURCE:-0}" == "1" ]]; then
            pgy_selfhost_fixed_point_profile_error "resource metrics malformed for $label"
            return 2
        fi
    else
        if "$@"; then rc=0; else rc=$?; fi
    fi
    end_ms="$(pgy_selfhost_fixed_point_profile_now_ms)" || return 2
    wall_ms=$((end_ms - start_ms))
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$label" "$kind" "$wall_ms" "$user_ms" "$system_ms" \
        "$max_rss_kb" "$rc" >>"$PGY_SELFHOST_PROFILE_PHASE_ROWS"
    rm -f "$resource_file"
    return "$rc"
}

pgy_selfhost_fixed_point_profile_is_milestone() {
    local line="$1"
    case "$line" in
        *":routine:"[0-9]*|*":routine-build:"[0-9]*|*":rows:"[0-9]*|\
        *":rows:done:"[0-9]*|*"locals="*|*":intent-build:"[0-9]*|\
        *":intent-append:"[0-9]*) return 1 ;;
    esac
    case "$line" in
        "[driver-pressure-stage] "*|"[semantic-body-type-stage] "*|\
        "[semantic-initializer-stage] "*|"[codegen-view-stage] "*) return 0 ;;
        *) return 1 ;;
    esac
}

pgy_selfhost_fixed_point_profile_capture_stages() {
    local label="$1"
    local capture_start now_ms elapsed_ms line safe_line
    capture_start="$(pgy_selfhost_fixed_point_profile_now_ms)" || return 2
    while IFS= read -r line || [[ -n "$line" ]]; do
        if pgy_selfhost_fixed_point_profile_is_milestone "$line"; then
            now_ms="$(pgy_selfhost_fixed_point_profile_now_ms)" || return 2
            elapsed_ms=$((now_ms - capture_start))
            safe_line="${line//|/_}"
            printf '%s\t%s\t%s\n' "$label" "$elapsed_ms" "$safe_line" \
                >>"$PGY_SELFHOST_PROFILE_STAGE_ROWS"
            printf '[self-host-fixed-point-profile] %s +%sms %s\n' \
                "$label" "$elapsed_ms" "$line"
        fi
    done
}

pgy_selfhost_fixed_point_profile_run_to_files() {
    local root_dir="$1" label="$2" kind="$3" stdout="$4" stderr="$5"
    shift 5
    local run_rc=0
    local -a pipe_status=()
    if [[ "$PGY_SELFHOST_PROFILE_ACTIVE" == "1" \
        && ( "$label" == "full_mir_seed" || "$label" == "full_mir_oracle" ) ]]; then
        set +e
        (cd "$root_dir" && pgy_selfhost_fixed_point_profile_run \
            "$label" "$kind" "$@" 2>"$stderr") |
            tee "$stdout" |
            pgy_selfhost_fixed_point_profile_capture_stages "$label"
        pipe_status=("${PIPESTATUS[@]}")
        set -e
        run_rc="${pipe_status[0]}"
        if [[ "${pipe_status[1]}" -ne 0 || "${pipe_status[2]}" -ne 0 ]]; then
            run_rc=2
        fi
    else
        (cd "$root_dir" && pgy_selfhost_fixed_point_profile_run \
            "$label" "$kind" "$@" >"$stdout" 2>"$stderr") || run_rc=$?
    fi
    return "$run_rc"
}

pgy_selfhost_fixed_point_profile_file_bytes() {
    wc -c <"$1" | tr -d ' '
}

pgy_selfhost_fixed_point_profile_file_lines() {
    wc -l <"$1" | tr -d ' '
}

pgy_selfhost_fixed_point_profile_artifact_row() {
    local name="$1" path="$2"
    printf 'artifact=%s|bytes=%s|lines=%s|sha256=%s\n' \
        "$name" \
        "$(pgy_selfhost_fixed_point_profile_file_bytes "$path")" \
        "$(pgy_selfhost_fixed_point_profile_file_lines "$path")" \
        "$(pgy_selfhost_driver_receipt_hash_file "$path")"
}

pgy_selfhost_fixed_point_profile_required_phases() {
    printf '%s\n' \
        driver_seed_emit driver_seed_compile driver_oracle_emit \
        driver_oracle_compile sample_self sample_oracle bounded_mir_seed \
        bounded_mir_oracle bounded_seed bounded_oracle full_mir_seed \
        full_mir_oracle gen2_emit driver_gen2_compile bounded_gen2 gen3_emit
}

pgy_selfhost_fixed_point_profile_validate() {
    local receipt="$1" fixed_receipt="$2" driver_source="$3"
    local driver_seed_c="$4" driver_oracle_c="$5" mir_seed="$6"
    local mir_oracle="$7" gen2_c="$8" gen3_c="$9" gen2_binary="${10}"
    local native_pgy="${11}" codegen_seed="${12}"
    local phase expected_count actual_count source_graph fixed_hash

    [[ -f "$receipt" ]] || return 1
    [[ "$(grep -Fc 'schema=pgy.selfhost.fixed-point-profile.v1' "$receipt")" -eq 1 ]] || return 1
    expected_count="$(pgy_selfhost_fixed_point_profile_required_phases | wc -l | tr -d ' ')"
    actual_count="$(grep -c '^phase=' "$receipt" || true)"
    [[ "$actual_count" -eq "$expected_count" ]] || return 1
    while IFS= read -r phase; do
        [[ "$(grep -Fc "phase=$phase|" "$receipt")" -eq 1 ]] || return 1
    done < <(pgy_selfhost_fixed_point_profile_required_phases)
    awk -F'|' '
        /^phase=/ {
            if ($3 !~ /^wall_ms=[0-9]+$/ ||
                $4 !~ /^user_ms=([0-9]+|unavailable)$/ ||
                $5 !~ /^system_ms=([0-9]+|unavailable)$/ ||
                $6 !~ /^max_rss_kb=([0-9]+|unavailable)$/ ||
                $7 != "exit_code=0") exit 1
        }
    ' "$receipt" || return 1
    for phase in full_mir_seed full_mir_oracle; do
        grep -F "stage=$phase|" "$receipt" | grep -Fq 'event=[driver-pressure-stage] ast:start' || return 1
        grep -F "stage=$phase|" "$receipt" | grep -Fq 'event=[driver-pressure-stage] json-write:done' || return 1
    done
    source_graph="$(sed -n 's/^source_graph=//p' "$fixed_receipt")"
    [[ -n "$source_graph" ]] || return 1
    grep -Fxq "source_graph=$source_graph" "$receipt" || return 1
    fixed_hash="$(pgy_selfhost_driver_receipt_hash_file "$fixed_receipt")" || return 1
    grep -Fxq "fixed_point_receipt=$fixed_hash" "$receipt" || return 1
    grep -Fxq "tool=native_pgy|sha256=$(pgy_selfhost_driver_receipt_hash_file "$native_pgy")" "$receipt" || return 1
    grep -Fxq "tool=codegen_seed|sha256=$(pgy_selfhost_driver_receipt_hash_file "$codegen_seed")" "$receipt" || return 1
    local spec name path expected
    for spec in \
        "driver_source|$driver_source" "driver_seed_c|$driver_seed_c" \
        "driver_oracle_c|$driver_oracle_c" "mir_seed|$mir_seed" \
        "mir_oracle|$mir_oracle" "gen2_c|$gen2_c" "gen3_c|$gen3_c" \
        "gen2_binary|$gen2_binary"; do
        name="${spec%%|*}"
        path="${spec#*|}"
        expected="$(pgy_selfhost_fixed_point_profile_artifact_row "$name" "$path")" || return 1
        grep -Fxq "$expected" "$receipt" || return 1
    done
    [[ "$(pgy_selfhost_driver_receipt_hash_file "$mir_seed")" == \
        "$(pgy_selfhost_driver_receipt_hash_file "$mir_oracle")" ]] || return 1
    [[ "$(pgy_selfhost_driver_receipt_hash_file "$gen2_c")" == \
        "$(pgy_selfhost_driver_receipt_hash_file "$gen3_c")" ]] || return 1
    if [[ "${PGY_SELFHOST_PROFILE_REQUIRE_RESOURCE:-0}" == "1" ]]; then
        grep -Fxq 'resource_metrics=gnu-time' "$receipt" || return 1
    fi
}

pgy_selfhost_fixed_point_profile_finish() {
    local root_dir="$1" native_pgy="$2" codegen_seed="$3" cc="$4"
    local driver_source="$5" fixed_receipt="$6" driver_seed_c="$7"
    local driver_oracle_c="$8" mir_seed="$9" mir_oracle="${10}"
    local gen2_c="${11}" gen3_c="${12}" gen2_binary="${13}"
    local receipt="$PGY_SELFHOST_PROFILE_RECEIPT" receipt_tmp="${PGY_SELFHOST_PROFILE_RECEIPT}.tmp"
    local end_ms total_ms source_revision source_graph cc_fingerprint
    local phase_count stage_count dominant host_kernel cpu_count
    local label kind wall_ms user_ms system_ms max_rss_kb exit_code
    local stage_label stage_elapsed event

    [[ "$PGY_SELFHOST_PROFILE_ACTIVE" == "1" ]] || return 0
    end_ms="$(pgy_selfhost_fixed_point_profile_now_ms)" || return 2
    total_ms=$((end_ms - PGY_SELFHOST_PROFILE_START_MS))
    source_revision="$(git -C "$root_dir" rev-parse HEAD 2>/dev/null || printf unavailable)"
    source_graph="$(sed -n 's/^source_graph=//p' "$fixed_receipt")"
    [[ -n "$source_graph" ]] ||
        pgy_selfhost_fixed_point_profile_error "fixed-point receipt lacks source graph" || return 2
    cc_fingerprint="$(pgy_selfhost_driver_c_compiler_fingerprint \
        "$cc" "${receipt}.cc-fingerprint.input.tmp")" || return 2
    phase_count="$(wc -l <"$PGY_SELFHOST_PROFILE_PHASE_ROWS" | tr -d ' ')"
    stage_count="$(wc -l <"$PGY_SELFHOST_PROFILE_STAGE_ROWS" | tr -d ' ')"
    dominant="$(awk -F'\t' 'BEGIN { max=-1 } $3+0 > max { max=$3+0; label=$1 } END { print label "|" max }' \
        "$PGY_SELFHOST_PROFILE_PHASE_ROWS")"
    host_kernel="$(uname -srm 2>/dev/null | tr '\t\r\n|' '____')"
    cpu_count="$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf unavailable)"

    rm -f "$receipt_tmp"
    {
        printf '%s\n' \
            'schema=pgy.selfhost.fixed-point-profile.v1' \
            "source_revision=$source_revision" \
            "source_graph=$source_graph" \
            "fixed_point_receipt=$(pgy_selfhost_driver_receipt_hash_file "$fixed_receipt")" \
            "profile_owner=$(pgy_selfhost_driver_receipt_hash_file "${BASH_SOURCE[0]}")" \
            "resource_metrics=$PGY_SELFHOST_PROFILE_RESOURCE_SOURCE" \
            "cc_fingerprint=$cc_fingerprint" \
            "host_kernel=$host_kernel" \
            "host_cpu_count=$cpu_count" \
            "runner_image=${ImageOS:-local}:${ImageVersion:-unknown}" \
            "total_wall_ms=$total_ms" \
            "phase_count=$phase_count" \
            "stage_count=$stage_count" \
            "dominant_phase=${dominant%%|*}" \
            "dominant_wall_ms=${dominant#*|}" \
            'post_seal_retained_bytes=unmeasured' \
            'total_allocations=unmeasured' \
            'expression_graph_visits=unmeasured' \
            'fact_reconstruction_count=unmeasured' \
            'routine_reanalysis_count=unmeasured' \
            "tool=native_pgy|sha256=$(pgy_selfhost_driver_receipt_hash_file "$native_pgy")" \
            "tool=codegen_seed|sha256=$(pgy_selfhost_driver_receipt_hash_file "$codegen_seed")"
        pgy_selfhost_fixed_point_profile_artifact_row driver_source "$driver_source"
        pgy_selfhost_fixed_point_profile_artifact_row driver_seed_c "$driver_seed_c"
        pgy_selfhost_fixed_point_profile_artifact_row driver_oracle_c "$driver_oracle_c"
        pgy_selfhost_fixed_point_profile_artifact_row mir_seed "$mir_seed"
        pgy_selfhost_fixed_point_profile_artifact_row mir_oracle "$mir_oracle"
        pgy_selfhost_fixed_point_profile_artifact_row gen2_c "$gen2_c"
        pgy_selfhost_fixed_point_profile_artifact_row gen3_c "$gen3_c"
        pgy_selfhost_fixed_point_profile_artifact_row gen2_binary "$gen2_binary"
        while IFS=$'\t' read -r label kind wall_ms user_ms system_ms max_rss_kb exit_code; do
            printf 'phase=%s|kind=%s|wall_ms=%s|user_ms=%s|system_ms=%s|max_rss_kb=%s|exit_code=%s\n' \
                "$label" "$kind" "$wall_ms" "$user_ms" "$system_ms" "$max_rss_kb" "$exit_code"
        done <"$PGY_SELFHOST_PROFILE_PHASE_ROWS"
        while IFS=$'\t' read -r stage_label stage_elapsed event; do
            printf 'stage=%s|elapsed_ms=%s|event=%s\n' \
                "$stage_label" "$stage_elapsed" "$event"
        done <"$PGY_SELFHOST_PROFILE_STAGE_ROWS"
    } >"$receipt_tmp" || return 2
    mv -f "$receipt_tmp" "$receipt"
    rm -f "$PGY_SELFHOST_PROFILE_PHASE_ROWS" "$PGY_SELFHOST_PROFILE_STAGE_ROWS" \
        "${receipt}.cc-fingerprint.input.tmp"
    pgy_selfhost_fixed_point_profile_validate \
        "$receipt" "$fixed_receipt" "$driver_source" "$driver_seed_c" \
        "$driver_oracle_c" "$mir_seed" "$mir_oracle" "$gen2_c" "$gen3_c" \
        "$gen2_binary" "$native_pgy" "$codegen_seed" ||
        pgy_selfhost_fixed_point_profile_error "profile receipt validation failed" || return 2
    echo "[self-host-fixed-point-profile] receipt=$receipt total_ms=$total_ms dominant=${dominant%%|*}:${dominant#*|}ms resource=$PGY_SELFHOST_PROFILE_RESOURCE_SOURCE"
}
