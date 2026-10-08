"""Stage 1 envelope admission; not a production backend or a signed issuer.

The finite decision is compared with ProofCarryingIR's extracted checker.
JSON shape, exact input bytes and manifest-only status remain explicit checks
outside that finite theorem. Payload facts are re-read at admission, not trusted
because an envelope names them.
"""

import hashlib
import json

REQUIRED_LAYERS = ("air", "dag", "mir", "abi", "backend")
AIR_REQUIRED = (
    "hir_cfg", "rir_boundary", "rir_authority", "dag_metadata",
    "mir_cleanup", "mir_terminator",
)
MIR_REQUIRED = ("cfg_blocks", "source_shape", "expr0", "cleanup")
CORE_WIDTH = 18


def certificate_core_admits(bits):
    """The 18 typed decisions in ProofCarryingIR.check_bits, in the same order."""
    return (len(bits) == CORE_WIDTH
            and all(type(bit) is bool for bit in bits)
            and all(bits))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _binding_from_digests(source_sha256, air_sha256, mir_sha256):
    payload = {
        "schema": "pgy.proof-input-binding.v1",
        "source_sha256": source_sha256,
        "air_sha256": air_sha256,
        "mir_sha256": mir_sha256,
    }
    return hashlib.sha256(json.dumps(
        payload, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")).hexdigest()


def binding_digest(source_path, air_payload_path, mir_payload_path):
    return _binding_from_digests(
        digest(source_path), digest(air_payload_path), digest(mir_payload_path)
    )


def require(condition, message, errors):
    if not condition:
        errors.append(message)


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON object key: " + key)
        result[key] = value
    return result


def _exact_names(value, expected):
    return (isinstance(value, list)
            and all(isinstance(name, str) for name in value)
            and len(value) == len(expected) and set(value) == set(expected))


def _payload_bits(air, mir, errors):
    require(isinstance(air, dict), "AIR payload must be an object", errors)
    require(isinstance(mir, dict), "MIR payload must be an object", errors)
    if not isinstance(air, dict) or not isinstance(mir, dict):
        return None
    require(air.get("schema") == "pgy.air.graph.v1", "AIR schema mismatch", errors)
    require(mir.get("schema") == "pgy.mir.v1", "MIR schema mismatch", errors)
    summary = air.get("summary")
    evidence = air.get("evidence")
    routines = mir.get("routines")
    require(isinstance(summary, dict), "AIR summary must be an object", errors)
    require(isinstance(evidence, list) and all(isinstance(e, dict) for e in evidence),
            "AIR evidence must be an object array", errors)
    require(isinstance(routines, list) and all(isinstance(r, dict) for r in routines),
            "MIR routines must be an object array", errors)
    if errors:
        return None
    blocks = []
    instructions = []
    for routine in routines:
        rows = routine.get("blocks")
        if not isinstance(rows, list) or not all(isinstance(b, dict) for b in rows):
            errors.append("MIR blocks must be an object array")
            return None
        blocks.extend(rows)
        for block in rows:
            rows_i = block.get("instructions")
            if (not isinstance(rows_i, list)
                    or not all(isinstance(i, dict) for i in rows_i)):
                errors.append("MIR instructions must be an object array")
                return None
            instructions.extend(rows_i)
    for inst in instructions:
        for field in ("source_type", "expr0"):
            value = inst.get(field)
            if value is not None and not isinstance(value, str):
                errors.append("MIR " + field + " must be text or null")
                return None
    # These are bounded presence facts, not a per-routine safety proof. Every
    # matching required evidence row must explicitly report zero fallbacks.
    air_bits = [summary.get("strict_evidence") is True,
                type(summary.get("drift_count")) is int and summary["drift_count"] == 0]
    for kind in AIR_REQUIRED:
        rows = [row for row in evidence if row.get("kind") == kind]
        air_bits.append(bool(rows) and all(
            type(row.get("fallback_count")) is int and row["fallback_count"] == 0
            for row in rows
        ))
    require(any(r.get("kind") == "intent" for r in routines),
            "MIR intent routine missing", errors)
    mir_bits = [bool(blocks), any(bool(i.get("source_type")) for i in instructions),
                any(bool(i.get("expr0")) for i in instructions),
                any(i.get("kind") == "cleanup" for i in instructions)]
    return air_bits, mir_bits


def validate_certificate(cert, source_path, air_payload_path, mir_payload_path, errors):
    """Append explicit admission failures using the smoke's established errors API."""
    if not isinstance(cert, dict):
        errors.append("certificate must be an object")
        return
    try:
        source_bytes = source_path.read_bytes()
        air_bytes = air_payload_path.read_bytes()
        mir_bytes = mir_payload_path.read_bytes()
    except OSError as error:
        errors.append("bound input read failed: " + str(error))
        return
    try:
        air = json.loads(air_bytes, object_pairs_hook=_unique_object)
        mir = json.loads(mir_bytes, object_pairs_hook=_unique_object)
    except (ValueError, UnicodeDecodeError) as error:
        errors.append("bound payload JSON invalid: " + str(error))
        return
    source_sha = hashlib.sha256(source_bytes).hexdigest()
    air_sha = hashlib.sha256(air_bytes).hexdigest()
    mir_sha = hashlib.sha256(mir_bytes).hexdigest()
    require(cert.get("schema") == "pgy.proof-carrying-ir.v1",
            "wrong certificate schema", errors)
    require(cert.get("source") == source_path.as_posix(),
            "certificate source identity drifted", errors)
    require(cert.get("source_digest_sha256") == source_sha,
            "certificate source digest drifted", errors)
    require(cert.get("binding_digest_sha256") ==
            _binding_from_digests(source_sha, air_sha, mir_sha),
            "certificate source/AIR/MIR binding digest drifted", errors)

    policy = cert.get("policy")
    if not isinstance(policy, dict):
        errors.append("certificate policy must be an object")
        return
    require(policy.get("semantic_fallback") == "forbidden",
            "semantic fallback policy must be forbidden", errors)
    require(policy.get("negative_check") == "delete-required-fact+mutate-bound-input",
            "negative-check policy drifted", errors)
    rows = cert.get("layers")
    if (not isinstance(rows, list) or not all(isinstance(row, dict) for row in rows)
            or not all(isinstance(row.get("id"), str) for row in rows)):
        errors.append("certificate layers must be objects with string ids")
        return
    ids = [row["id"] for row in rows]
    require(len(ids) == len(set(ids)), "certificate layer ids are duplicated", errors)
    require(set(ids) == set(REQUIRED_LAYERS), "certificate layer set drifted", errors)
    if errors:
        return
    layers = {row["id"]: row for row in rows}
    require(_exact_names(layers["air"].get("required_evidence"), AIR_REQUIRED),
            "AIR required evidence set drifted", errors)
    require(_exact_names(layers["mir"].get("required_facts"), MIR_REQUIRED),
            "MIR required fact set drifted", errors)
    for kind, schema in (("air", "pgy.air.graph.v1"), ("mir", "pgy.mir.v1"),
                         ("dag", "type-resolution-metadata"),
                         ("abi", "mir-runtime-abi-facts"),
                         ("backend", "backend-consumption-trace")):
        require(layers[kind].get("payload_schema") == schema,
                kind + " payload schema drifted", errors)
    require(layers["air"].get("digest_sha256") == air_sha, "AIR payload digest drifted", errors)
    require(layers["mir"].get("digest_sha256") == mir_sha, "MIR payload digest drifted", errors)
    for kind in ("dag", "abi", "backend"):
        require(layers[kind].get("status") == "manifest-only",
                kind + " layer must be explicit manifest-only", errors)
    facts = _payload_bits(air, mir, errors)
    if facts is None:
        return
    air_bits, mir_bits = facts
    backend_bit = (policy.get("backend_consumption") == "fact-or-fail-closed"
                   and layers["backend"].get("consumption") == "fact-or-fail-closed")
    bits = tuple(kind in layers for kind in REQUIRED_LAYERS) + tuple(air_bits + mir_bits) + (backend_bit,)
    require(certificate_core_admits(bits), "finite certificate facts rejected", errors)
