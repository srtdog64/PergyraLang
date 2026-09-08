"""Execute the shared C/LLVM Subject-state transfer kernel, not whole Intent.

Both MIR producers supply valid fixtures. Altered owner facts are checked only
by a valid validator and never emitted or executed. GraphPlan scheduling,
synchronization, authority and compensation are separate required gates.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", required=True)
    parser.add_argument("--driver", required=True)
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--clang", default="clang")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    parent = root / ".tmp/self_hosted/intent_cell_transfer"
    parent.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="run.", dir=parent))
    print(f"[intent-cell-transfer] evidence: {work}", flush=True)

    def relative(path):
        return path.relative_to(root).as_posix()

    def invoke(command, label, expected=0):
        result = subprocess.run(command, cwd=root, capture_output=True, timeout=60)
        (work / f"{label}.out").write_bytes(result.stdout)
        (work / f"{label}.err").write_bytes(result.stderr)
        if result.returncode != expected:
            raise RuntimeError(f"{label}: exit {result.returncode}, expected {expected}; see {work}")
        return result

    probe = work / "intent-cell-transfer-probe.exe"
    probe_c = work / "intent-cell-transfer-probe.c"
    invoke([args.native, "--native-pipeline", "--emit-c",
            "tests/self_hosted/parity/fixture/intent_cell_transfer_probe.pgy",
            "-o", relative(probe_c)], "probe-emit")
    invoke([args.cc, "-std=c11", "-O0", "-g0", "-fwrapv", "-fno-strict-aliasing",
            "-I" + str(root / "src"), "-I" + str(root / "src/runtime"), "-pthread",
            str(probe_c), "-o", str(probe)], "probe-compile")
    checks = 0
    for producer in ("native", "self"):
        for case, expected_slot in (("observed", 0), ("placement_exact", 1), ("placement_unique", 1)):
            label = f"{producer}-{case}"
            source = f"tests/concept_semantics/intent_predicates/header_success_{case}.pgy"
            produced = invoke(([args.native, "--native-pipeline", "--mir-json", source] if producer == "native"
                               else [args.driver, "--emit-mir-json-verified", source]), label + "-produce")
            mir = work / f"{label}.mir.json"
            mir.write_bytes(produced.stdout)
            document = json.loads(produced.stdout)
            target_outputs = {}
            for target in ("c", "llvm"):
                result = invoke([str(probe), relative(mir), target], label + "-" + target + "-project")
                if result.stderr:
                    raise RuntimeError(f"{label}-{target}: unexpected diagnostic output")
                target_outputs[target] = result.stdout.decode()
            c = target_outputs["c"]
            matched = re.search(r"pgy-intent-cell-layout zone=(\d+) subject=(\d+) slot=(\d+)", c)
            if matched is None or int(matched[3]) != expected_slot:
                raise RuntimeError(f"{label}: selected layout disagrees with independent expected slot")
            if matched.group(0) not in target_outputs["llvm"]:
                raise RuntimeError(f"{label}: targets consumed different placement identities")
            zone_type = f"pgy_identity_cell_{matched[1]}"
            subject_type = f"pgy_identity_cell_{matched[2]}"
            zone_decl = next(d for d in document["decls"] if d["name"] == "HeaderZone")
            slot_count = len(zone_decl["fields"])
            # All three source controls deliberately use one-Int Subject state.
            # The two-slot controls must keep both independent storage pointers.
            header = c.split("void pgy_test_place", 1)[0]
            # The reserve type can differ; use its own declared cell type from
            # the emitted header without changing either target kernel.
            field_types = {int(n): cell_type for cell_type, n in re.findall(
                r"(pgy_identity_cell_\d+) \* field_(\d+);", header.split(f"struct {zone_type} {{", 1)[1].split("};", 1)[0])}
            if sorted(field_types) != list(range(slot_count)):
                raise RuntimeError(f"{label}: wrapper cannot identify every declared slot type")
            declarations = "\n".join(f"    {field_types[n]} slot_{n} = {{7 + {n}}};" for n in range(slot_count))
            initializers = ", ".join(f"&slot_{n}" for n in range(slot_count)) + ", 0"
            untouched = "\n".join(f"    if (slot_{n}.field_0 != 7 + {n}) return 20 + {n};"
                                   for n in range(slot_count) if n != expected_slot)
            wrapper = header + "\n#include <stdio.h>\n" + f"void pgy_test_place({zone_type} *, {subject_type} *);\n" + \
                f"void pgy_test_writeback({zone_type} *, {subject_type} *);\nint main(void) {{\n" + declarations + \
                f"\n    {zone_type} zone = {{{initializers}}};\n    {subject_type} participant = {{41}};\n" + \
                f"    {subject_type} *original_slot = zone.field_{expected_slot};\n" + \
                f"    pgy_test_place(&zone, &participant);\n    if (zone.field_{expected_slot} != original_slot || " + \
                f"zone.field_{expected_slot} == &participant || slot_{expected_slot}.field_0 != 41 || participant.field_0 != 41) return 1;\n" + \
                f"    slot_{expected_slot}.field_0 = 88;\n    if (participant.field_0 != 41) return 2;\n" + \
                f"    pgy_test_writeback(&zone, &participant);\n    if (participant.field_0 != 88 || zone.field_{expected_slot} != original_slot) return 3;\n" + \
                f"    participant.field_0 = 99;\n    if (slot_{expected_slot}.field_0 != 88 || zone.pgy_generation != 0) return 4;\n" + \
                untouched + '\n    puts("cell transfer: PASS");\n    return 0;\n}\n'
            wrapper_path = work / f"{label}-wrapper.c"
            wrapper_path.write_text(wrapper, encoding="utf-8")
            for target, suffix in (("c", "c"), ("llvm", "ll")):
                kernel = work / f"{label}-kernel.{suffix}"
                kernel.write_text(target_outputs[target], encoding="utf-8")
                executable = work / f"{label}-{target}.exe"
                compiler = args.cc if target == "c" else args.clang
                invoke([compiler, "-O0", str(kernel), str(wrapper_path), "-o", str(executable)], label + "-" + target + "-compile")
                observed = invoke([str(executable)], label + "-" + target + "-run")
                if observed.stdout.decode().replace("\r\n", "\n") != "cell transfer: PASS\n" or observed.stderr:
                    raise RuntimeError(f"{label}-{target}: transfer/identity observation failed")
                checks += 1
                print(f"[intent-cell-transfer] PASS: {label}-{target}", flush=True)
            for mode in ("missing-slot", "foreign-zone", "crossed-participant", "renamed-slot", "absent-placement"):
                refused = invoke([str(probe), relative(mir), mode], label + "-" + mode, expected=1)
                message = (refused.stdout + refused.stderr).decode(errors="replace")
                if "refused before emission" not in message or "pgy-intent-cell-layout" in message:
                    raise RuntimeError(f"{label}-{mode}: missing owned pre-emission refusal")
                checks += 1
    print(f"[intent-cell-transfer] {checks} checks PASS; kernel only, not GraphPlan execution", flush=True)


if __name__ == "__main__":
    main()
