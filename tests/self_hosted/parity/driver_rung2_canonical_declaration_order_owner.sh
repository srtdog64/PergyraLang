#!/usr/bin/env bash
# Owns canonical MIR declaration-family order during JSON round trips.
# Input declaration order is provenance, not canonical authority.  The role
# fixture deliberately presents native and self-host inputs in different
# cross-family orders; both must project one nominal/role/ability order before
# regenerated routine syntax identity is observed.

pgy_selfhost_verify_driver_rung2_canonical_declaration_order() {
    local backend="$1" base="$2" native_mir="$3" self_mir="$4"
    local native_canonical="$5" self_canonical="$6"
    local native_raw self_raw artifact canonical
    [[ "$base" == "role_operator_dispatch" ]] || return 0

    native_raw="$(tr -d '\r\n' <"$native_mir")"
    self_raw="$(tr -d '\r\n' <"$self_mir")"
    case "$native_raw" in
        *'"kind":"ability"'*'"kind":"role"'*) ;;
        *)
            echo "[self-host-parity:driver-rung2] $backend native role-order adversary drifted" >&2
            exit 1
            ;;
    esac
    case "$self_raw" in
        *'"kind":"role"'*'"kind":"ability"'*) ;;
        *)
            echo "[self-host-parity:driver-rung2] $backend self role-order adversary drifted" >&2
            exit 1
            ;;
    esac

    for artifact in "$native_canonical" "$self_canonical"; do
        canonical="$(tr -d '\r\n' <"$artifact")"
        case "$canonical" in
            *'"decls":[{"kind":"subject"'*'"kind":"role"'*'"kind":"ability"'*) ;;
            *)
                echo "[self-host-parity:driver-rung2] $backend canonical declaration phase drifted: $artifact" >&2
                exit 1
                ;;
        esac
        grep -Eq '"name":"Add","kind":"method","source_syntax_id":[1-9][0-9]*,("receiver_carriage":"[a-z-]+",)?"owner":"IntMath"' \
            "$artifact" || {
            echo "[self-host-parity:driver-rung2] $backend canonical role routine identity missing: $artifact" >&2
            exit 1
        }
    done
}
