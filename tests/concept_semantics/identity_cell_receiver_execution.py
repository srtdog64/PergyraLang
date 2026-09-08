"""Receiver execution parity and admission-only crossed-identity controls.

Use --shared-only during an owner edit loop; it does not certify the installed
or production public driver. Only fixed valid source controls execute.
"""
import copy
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import tempfile


def graph_digest(graph):
    # Independent test projection of the persisted expression digest contract.
    def integer(value, number):
        return (value * 131 + number + 2) % 268435456

    def string(value, text):
        data = text.encode("utf-8")
        value = (value * 131 + len(data)) % 268435456
        for byte in data:
            value = (value * 131 + byte) % 268435456
        return value

    value = integer(integer(71, graph["root"]), len(graph["nodes"]))
    for node in graph["nodes"]:
        for key in ("kind", "text", "call_target_kind", "call_target_name"):
            value = string(value, node[key])
        value = integer(value, node["runtime_call_abi_id"])
        for key in ("left", "right"):
            value = integer(value, -1 if node[key] is None else node[key])
    return 1073741824 + value


def method(doc):
    return next(r for r in doc["routines"] if r["kind"] == "method" and r["owner"] == "Counter")


def declaration(doc):
    return next(d for d in doc["decls"] if d["name"] == "Counter")


def first_store_graph(doc):
    row = method(doc)["blocks"][0]["instructions"][0]
    return row["expr0_graph" if row["kind"] == "assign" else "expr1_graph"]


def first_call_graph(doc):
    main = next(r for r in doc["routines"] if r["name"] == "Main")
    return next(i["expr0_graph"] for b in main["blocks"] for i in b["instructions"]
                if any(n["call_target_kind"] == "member" for n in i.get("expr0_graph", {}).get("nodes", [])))


def mutate_graph(doc, select, key, value, kind):
    graph = select(doc)
    assert graph_digest(graph) == graph["digest"]
    node = next(n for n in graph["nodes"] if n["kind"] == kind)
    node[key] = value
    graph["digest"] = graph_digest(graph)


def mutations():
    return [
        ("missing-owner", lambda d: method(d).pop("owner")),
        ("crossed-owner", lambda d: method(d).__setitem__("owner", "Other")),
        ("missing-receiver", lambda d: method(d).pop("receiver_carriage")),
        ("value-receiver", lambda d: method(d).__setitem__("receiver_carriage", "value")),
        ("method-as-function", lambda d: method(d).__setitem__("kind", "function")),
        ("omitted-body", lambda d: d["routines"].remove(method(d))),
        ("undeclared-method", lambda d: declaration(d).__setitem__("methods", [])),
        ("missing-self", lambda d: method(d)["params"].pop(0)),
        ("renamed-self", lambda d: method(d)["params"][0].__setitem__("name", "receiver")),
        ("zero-self-id", lambda d: method(d)["params"][0].__setitem__("source_syntax_id", 0)),
        ("typed-self", lambda d: method(d)["params"][0].__setitem__("type", "Int")),
        ("layout-self", lambda d: method(d)["params"][0].__setitem__("abi_layout_id", 42)),
        ("required-layout-self", lambda d: method(d)["params"][0].__setitem__("abi_layout_required", True)),
        ("copyout-self", lambda d: method(d)["params"][0].__setitem__("carriage", "value-result")),
        ("indirect-self", lambda d: method(d)["params"][0].__setitem__("pass", "indirect")),
        ("null-ordinary-parameter", lambda d: method(d)["params"][1].update(type=None, abi_type_name=None)),
        ("duplicate-parameter", lambda d: method(d)["params"].append(copy.deepcopy(method(d)["params"][0]))),
        ("unknown-field-type", lambda d: declaration(d)["fields"][0].__setitem__("type", "UnknownCellField")),
        ("field-not-writable", lambda d: declaration(d)["fields"][0].__setitem__("field_kind", "let")),
        ("crossed-binding-id", lambda d: mutate_graph(d, first_store_graph, "binding_syntax_id",
                                                     method(d)["params"][1]["source_syntax_id"], "leaf")),
        ("crossed-binding-ordinal", lambda d: mutate_graph(d, first_store_graph, "binding_ordinal", 1, "leaf")),
        ("crossed-call-name", lambda d: mutate_graph(d, first_call_graph, "call_target_name", "Other_Bump", "call")),
        ("crossed-call-id", lambda d: mutate_graph(d, first_call_graph, "call_target_syntax_id",
                                                  next(r["source_syntax_id"] for r in d["routines"] if r["owner"] == "Other"), "call")),
    ]


def main():
    root = pathlib.Path(__file__).resolve().parents[2]
    native, driver = (pathlib.Path(x).resolve() for x in sys.argv[1:3])
    shared_only = "--shared-only" in sys.argv[3:]
    parent = root / ".tmp/concept_semantics/identity_cell_receiver"
    parent.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    print(f"[identity-cell-receiver] evidence: {work}", flush=True)
    rows = []
    environment = dict(os.environ, PGY_SELF_DRIVER_BIN=str(driver), PGY_DEBUG_PIPELINE_TIMING="1")
    environment.pop("PGY_NATIVE_PIPELINE", None)

    def run(label, command, timeout=45):
        # Pergyra's ReadFile contract consumes portable repository-relative paths.
        argv = [str(command[0])]
        argv.extend(x.relative_to(root).as_posix() if isinstance(x, pathlib.Path) else str(x) for x in command[1:])
        result = subprocess.run(argv, cwd=root, env=environment,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)
        (work / (label + ".out")).write_bytes(result.stdout)
        (work / (label + ".err")).write_bytes(result.stderr)
        return result

    def check(label, passed):
        rows.append({"case": label, "passed": passed})
        print(f"[identity-cell-receiver] {'PASS' if passed else 'FAIL'}: {label}", flush=True)

    probe_source = root / "tests/self_hosted/parity/fixture/identity_cell_receiver_probe.pgy"
    probe = work / "probe.exe"
    build = run("probe-build", [native, "--native-pipeline", "--backend=c", "--opt=dev", probe_source, "-o", probe], 120)
    if build.returncode:
        print(build.stdout.decode(errors="replace") + build.stderr.decode(errors="replace"))
        return 1
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (native, driver, probe_source, probe)}
    cases = [
        ("authority_effect/action_authority_valid.pgy", "0\n"),
        ("word_deletion/cases/06_action_func/orig.pgy", "2\n"),
        ("authority_effect/identity_cell_receiver_valid.pgy", "4\ntrue\nchanged\n15\n"),
        ("authority_effect/identity_cell_receiver_branch_valid.pgy", "8\n"),
        ("../self_hosted/parity/fixture/zone_subject_cell.pgy", "25\n2\n5\n73\n25\n44\n"),
        ("../self_hosted/parity/fixture/zone_subject_cell_nary.pgy", "123\n789\n"),
        ("../self_hosted/parity/fixture/zone_readonly_call.pgy", "17\n"),
        ("../self_hosted/parity/fixture/zone_readonly_reborrow.pgy", "45\n17\n23\n"),
    ]
    for source_name, expected in cases:
        source = (root / "tests/concept_semantics" / source_name).resolve()
        hashes[str(source)] = hashlib.sha256(source.read_bytes()).hexdigest()
        label = source.stem
        for origin in ("native", "public"):
            if not shared_only or origin == "native":
                for backend in ("c", "llvm"):
                    stem = f"{label}.{origin}.{backend}"
                    exe = work / (stem + ".exe")
                    command = [native, source, f"--backend={backend}", "--opt=dev", "-o", exe]
                    if origin == "native":
                        command.append("--native-pipeline")
                    built = run(stem + ".compile", command)
                    passed = built.returncode == 0 and (origin == "native" or b"[pipeline timing]" not in built.stdout + built.stderr)
                    if passed:
                        result = run(stem + ".run", [exe], 10)
                        passed = result.returncode == 0 and not result.stderr and result.stdout.decode().replace("\r\n", "\n") == expected
                    check(stem, passed)
            command = [native, "--native-pipeline", "--mir-json", source] if origin == "native" else [driver, "--emit-mir-json-verified", source]
            emitted = run(f"{label}.{origin}.mir", command)
            if emitted.returncode:
                check(f"{label}.{origin}.mir", False)
                continue
            document = json.loads(emitted.stdout)
            if label.startswith("zone_readonly_"):
                observed_routine = next(r for r in document["routines"] if r["name"] == "ObserveCounterZone")
                observed_parameter = next(p for p in observed_routine["params"] if p["type"] == "CounterZone")
                check(f"{label}.{origin}.zone-borrow-wire", observed_parameter["carriage"] == "readonly-ref" and
                      observed_parameter["pass"] == "indirect" and
                      not any(i["name"] == "DetachInvalidation" for b in observed_routine["blocks"]
                              for i in b["instructions"]))
                if origin == "native":
                    rir = run(f"{label}.native.rir", [native, "--native-pipeline", "--rir-json", source])
                    rir_ok = False
                    if rir.returncode == 0:
                        scope = next(s for s in json.loads(rir.stdout)["scopes"] if s["name"] == "ObserveCounterZone")
                        fact = next(f for f in scope["facts"] if f["name"] == "area")
                        summary = next(s for s in scope["summaries"] if s["name"] == "area")
                        rir_ok = fact["resource"] == "ZoneHandle" and fact["state"] == "BorrowedRead" and \
                            summary["initial_state"] == summary["final_state"] == "BorrowedRead" and not summary["has_error"]
                    check(f"{label}.native.zone-borrow-rir", rir_ok)
            mir = work / f"{label}.{origin}.json"
            mir.write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
            for backend in ("c", "llvm"):
                stem = f"{label}.{origin}.shared.{backend}"
                projected = run(stem + ".project", [probe, mir, backend])
                passed = projected.returncode == 0
                if passed:
                    code = work / (stem + (".c" if backend == "c" else ".ll"))
                    code.write_bytes(projected.stdout)
                    exe = work / (stem + ".exe")
                    command = ["gcc", "-std=c11", "-O0", "-fwrapv", "-fno-strict-aliasing", code, "-Isrc", "-Isrc/runtime", "-pthread", "-o", exe] if backend == "c" else ["clang", code, "-o", exe]
                    built = run(stem + ".compile", command)
                    passed = built.returncode == 0
                    if passed:
                        result = run(stem + ".run", [exe], 10)
                        passed = result.returncode == 0 and not result.stderr and result.stdout.decode().replace("\r\n", "\n") == expected
                check(stem, passed)
            if label.startswith("zone_readonly_"):
                zone_stem = f"{label}.{origin}"
                normalized = run(f"{zone_stem}.zone-readonly", [probe, mir, "zone-readonly"])
                check(f"{zone_stem}.zone-readonly", normalized.returncode == 0 and
                      b"zone readonly: value carriage and cell return refused" in normalized.stdout)
                for name, edit in (
                    ("value-carriage", lambda p: p.update(carriage="value", **{"pass": "direct"})),
                    ("direct-ref", lambda p: p.update(**{"pass": "direct"})),
                    ("copyout", lambda p: p.update(carriage="value-result")),
                    ("owner-handle", lambda p: p.update(carriage="owner-handle")),
                    ("resource", lambda p: p.update(resource="owner")),
                    ("layout-id", lambda p: p.update(abi_layout_id=42)),
                    ("layout-required", lambda p: p.update(abi_layout_required=True)),
                    ("abi-type", lambda p: p.update(abi_type_name="Counter")),
                    ("zero-id", lambda p: p.update(source_syntax_id=0)),
                ):
                    mutant = copy.deepcopy(document)
                    parameter = next(p for r in mutant["routines"] if r["name"] == "ObserveCounterZone"
                                     for p in r["params"] if p["type"] == "CounterZone")
                    edit(parameter)
                    path = work / f"{zone_stem}.zone-{name}.json"
                    path.write_text(json.dumps(mutant, separators=(",", ":")), encoding="utf-8")
                    rejected = run(f"{zone_stem}.zone-{name}", [probe, path], 30)
                    check(f"{zone_stem}.zone-{name}", rejected.returncode == 1 and
                          bool(rejected.stdout.strip() or rejected.stderr.strip()) and
                          b"receiver GraphPlan admitted" not in rejected.stdout and
                          b"empty or unreadable" not in rejected.stdout + rejected.stderr)
                    if name == "direct-ref" and label == "zone_readonly_call" and not shared_only:
                        for backend in ("c", "llvm"):
                            artifact = work / f"{zone_stem}.refused.{backend}"
                            public = run(f"{zone_stem}.refused.{backend}",
                                         [driver, f"--mir-json-backend={backend}", path, "-o", artifact])
                            check(f"{zone_stem}.no-artifact.{backend}", public.returncode == 1 and
                                  b"ERROR:" in public.stdout + public.stderr and not artifact.exists())
            if label.startswith("zone_subject_cell"):
                zone_stem = f"{label}.{origin}"
                normalized = run(f"{zone_stem}.zone-ownership", [probe, mir, "zone-ownership"])
                check(f"{zone_stem}.zone-ownership", normalized.returncode == 0 and
                      b"zone ownership: shared/borrowed backing and crossed slot kind refused" in normalized.stdout)
                for name, edit in (
                    ("slot-as-field", lambda z: z["fields"][0].update(field_kind="field")),
                    ("slot-wrong-type", lambda z: z["fields"][0].update(type="Int")),
                    ("slot-missing-id", lambda z: z["fields"][0].update(source_syntax_id=0)),
                    ("slot-duplicate-name", lambda z: z["fields"][1].update(name=z["fields"][0]["name"])),
                    ("zone-as-subject", lambda z: z.update(nominal_kind="subject")),
                    ("zone-missing-authorities", lambda z: z.pop("zone_authorities")),
                ):
                    mutant = copy.deepcopy(document)
                    edit(next(d for d in mutant["decls"] if d["name"] == "CounterPair"))
                    path = work / f"{zone_stem}.zone-{name}.json"
                    path.write_text(json.dumps(mutant, separators=(",", ":")), encoding="utf-8")
                    # Admission only; no malformed source/IR gets executed.
                    rejected = run(f"{zone_stem}.zone-{name}", [probe, path], 30)
                    check(f"{zone_stem}.zone-{name}", rejected.returncode == 1 and
                          bool(rejected.stdout.strip() or rejected.stderr.strip()) and
                          b"receiver GraphPlan admitted" not in rejected.stdout and
                          b"empty or unreadable" not in rejected.stdout + rejected.stderr)
            if label != "identity_cell_receiver_valid":
                continue
            # Reordering is a valid identity control, checked before mutants.
            reordered = copy.deepcopy(document)
            reordered["routines"].reverse()
            reordered["decls"].reverse()
            order_path = work / f"{origin}.reordered.json"
            order_path.write_text(json.dumps(reordered), encoding="utf-8")
            admitted = run(f"{origin}.reordered", [probe, order_path])
            check(f"{origin}.reordered", admitted.returncode == 0 and b"receiver GraphPlan admitted" in admitted.stdout)
            for name, edit in mutations():
                mutant = copy.deepcopy(document)
                edit(mutant)
                path = work / f"{origin}.{name}.json"
                path.write_text(json.dumps(mutant, separators=(",", ":")), encoding="utf-8")
                # Admission only: no mutant gets projected, compiled or run.
                rejected = run(f"{origin}.{name}", [probe, path], 30)
                check(f"{origin}.{name}", rejected.returncode == 1 and bool(rejected.stdout.strip() or rejected.stderr.strip())
                      and b"receiver GraphPlan admitted" not in rejected.stdout
                      and b"empty or unreadable" not in rejected.stdout + rejected.stderr)
    readonly_write = root / "tests/self_hosted/parity/fixture/zone_readonly_write_rejected.pgy"
    hashes[str(readonly_write)] = hashlib.sha256(readonly_write.read_bytes()).hexdigest()
    for origin, compiler, flags, diagnostic in (
        ("native", native, ["--native-pipeline", "--mir-json"], b"read-only ref parameter"),
        ("public", driver, ["--emit-mir-json-verified"], b"Code: immutable_field_write"),
    ):
        artifact = work / f"zone-readonly-write.{origin}.json"
        rejected = run(f"zone-readonly-write.{origin}", [compiler, *flags, readonly_write, "-o", artifact])
        check(f"zone-readonly-write.{origin}", rejected.returncode == 1 and
              diagnostic in rejected.stdout + rejected.stderr and not artifact.exists())
    failures = sum(not row["passed"] for row in rows)
    (work / "report.json").write_text(json.dumps({"scope": "shared-only" if shared_only else "production-and-shared",
                                                  "hashes": hashes, "checks": rows, "failures": failures}, indent=2), encoding="utf-8")
    print(f"[identity-cell-receiver] {len(rows)} checks / {failures} failures; evidence: {work}")
    return int(failures != 0)


if __name__ == "__main__":
    raise SystemExit(main())
