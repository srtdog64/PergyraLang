#!/usr/bin/env bash
# Owns the receipt mutations that must fail closed.

pgy_selfhost_fixed_point_profile_negative_checks() {
    local root_dir="$1" run_dir="$2" receipt="$3" fixed="$4"
    local boundary
    cp "$receipt" "$run_dir/pristine.receipt"
    printf 'drift\n' >>"$run_dir/gen3.c"
    if pgy_selfhost_fixed_point_profile_validate \
        "$receipt" "$fixed" "$run_dir/driver.pgy" "$run_dir/driver-seed.c" \
        "$run_dir/driver-oracle.c" "$run_dir/seed.mir.json" \
        "$run_dir/oracle.mir.json" "$run_dir/gen2.c" "$run_dir/gen3.c" \
        "$run_dir/gen2.bin" "$run_dir/native-pgy" "$run_dir/codegen-seed"; then
        echo "[self-host-fixed-point-profile-smoke] changed artifact was accepted" >&2
        return 1
    fi
    cp "$run_dir/gen2.c" "$run_dir/gen3.c"
    grep -v '^phase=gen3_emit|' "$run_dir/pristine.receipt" >"$receipt"
    if pgy_selfhost_fixed_point_profile_validate \
        "$receipt" "$fixed" "$run_dir/driver.pgy" "$run_dir/driver-seed.c" \
        "$run_dir/driver-oracle.c" "$run_dir/seed.mir.json" \
        "$run_dir/oracle.mir.json" "$run_dir/gen2.c" "$run_dir/gen3.c" \
        "$run_dir/gen2.bin" "$run_dir/native-pgy" "$run_dir/codegen-seed"; then
        echo "[self-host-fixed-point-profile-smoke] missing phase was accepted" >&2
        return 1
    fi
    awk '
        /^phase=/ && !changed { sub(/wall_ms=[0-9]+/, "wall_ms=bad"); changed=1 }
        { print }
    ' "$run_dir/pristine.receipt" >"$receipt"
    if pgy_selfhost_fixed_point_profile_validate \
        "$receipt" "$fixed" "$run_dir/driver.pgy" "$run_dir/driver-seed.c" \
        "$run_dir/driver-oracle.c" "$run_dir/seed.mir.json" \
        "$run_dir/oracle.mir.json" "$run_dir/gen2.c" "$run_dir/gen3.c" \
        "$run_dir/gen2.bin" "$run_dir/native-pgy" "$run_dir/codegen-seed"; then
        echo "[self-host-fixed-point-profile-smoke] malformed timing was accepted" >&2
        return 1
    fi
    if [[ "${PGY_SELFHOST_FULL_MIR_BULKHEAD:-0}" != "1" ]]; then
        return 0
    fi
    if PGY_SELFHOST_FULL_MIR_BULKHEAD=invalid pgy_selfhost_fixed_point_profile_run \
        full_mir_seed synthetic sh -c 'exit 99'; then
        echo "[self-host-fixed-point-profile-smoke] invalid bulkhead mode executed" >&2
        return 1
    fi
    cp "$run_dir/pristine.receipt" "$receipt"
    boundary="${receipt}.full_mir_oracle.process.json"
    cp "$boundary" "$run_dir/pristine.process.json"
    python3 - "$boundary" <<'PY'
import json
from pathlib import Path
import sys
path = Path(sys.argv[1])
record = json.loads(path.read_text())
record['process_group_drained'] = False
path.write_text(json.dumps(record))
PY
    if pgy_selfhost_fixed_point_profile_validate \
        "$receipt" "$fixed" "$run_dir/driver.pgy" "$run_dir/driver-seed.c" \
        "$run_dir/driver-oracle.c" "$run_dir/seed.mir.json" \
        "$run_dir/oracle.mir.json" "$run_dir/gen2.c" "$run_dir/gen3.c" \
        "$run_dir/gen2.bin" "$run_dir/native-pgy" "$run_dir/codegen-seed"; then
        echo "[self-host-fixed-point-profile-smoke] undrained producer was accepted" >&2
        return 1
    fi
    cp "$run_dir/pristine.process.json" "$boundary"
    grep -v '^process_boundary=' "$run_dir/pristine.receipt" >"$receipt"
    if pgy_selfhost_fixed_point_profile_validate \
        "$receipt" "$fixed" "$run_dir/driver.pgy" "$run_dir/driver-seed.c" \
        "$run_dir/driver-oracle.c" "$run_dir/seed.mir.json" \
        "$run_dir/oracle.mir.json" "$run_dir/gen2.c" "$run_dir/gen3.c" \
        "$run_dir/gen2.bin" "$run_dir/native-pgy" "$run_dir/codegen-seed"; then
        echo "[self-host-fixed-point-profile-smoke] missing process boundary was accepted" >&2
        return 1
    fi
}
