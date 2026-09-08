"""Validate malformed match MIR without compiling or executing its output."""
import copy
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[2]
    driver = pathlib.Path(sys.argv[1]).resolve()
    work_root = root / ".tmp/concept_semantics/enum_match_mir"
    work_root.mkdir(parents=True, exist_ok=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="run.", dir=work_root))

    def invoke(args):
        return subprocess.run([str(driver), *args], cwd=root, capture_output=True,
                              text=True, encoding="utf-8", errors="replace", timeout=45)

    def relative(path):
        return path.relative_to(root).as_posix()

    seeds = {}
    for name, source in (
        ("payload", "tests/concept_semantics/word_deletion/cases/04_match_enum/orig.pgy"),
        ("plain", "tests/concept_semantics/match/payload_free_nested_valid.pgy"),
    ):
        result = invoke(["--emit-mir-json-verified", source])
        if result.returncode != 0:
            raise RuntimeError(f"valid {name} source admission failed: {result.stdout}{result.stderr}")
        seeds[name] = json.loads(result.stdout)
        seed_path = work / f"{name}.seed.json"
        seed_path.write_text(result.stdout, encoding="utf-8")
        # A negative gate is meaningful only if this exact unmodified seed projects.
        for backend, suffix in (("c", "c"), ("llvm", "ll")):
            control = invoke([f"--mir-json-backend={backend}", relative(seed_path),
                              "-o", relative(work / f"{name}.seed.{suffix}")])
            if control.returncode != 0:
                raise RuntimeError(f"valid {name} {backend} MIR projection failed: {control.stdout}{control.stderr}")

    def area(doc):
        return next(r for r in doc["routines"] if r["name"] == "Area")

    def remove_return(doc):
        blocks = area(doc)["blocks"]
        target = next(b for b in reversed(blocks)
                      if any(i["kind"] == "return" for i in b["instructions"]))
        target["instructions"] = []

    def skip_case(doc):
        branches = [b for b in area(doc)["blocks"]
                    if any(i["kind"] == "branch" for i in b["instructions"])]
        branches[0]["succ_false"] = branches[-1]["id"]

    def true_edge_to_fallthrough(doc):
        branch = next(b for b in reversed(area(doc)["blocks"])
                      if any(i["kind"] == "branch" for i in b["instructions"]))
        branch["succ_true"] = branch["succ_false"]

    def duplicate_missing(doc):
        changed = 0
        for routine in doc["routines"]:
            for block in routine["blocks"]:
                for instruction in block["instructions"]:
                    if instruction.get("match_patterns") == ["South"]:
                        instruction["match_patterns"] = ["East"]
                        changed += 1
        assert changed == 2, "both nested South arms must be changed"

    results = []
    for name, seed, mutate in (
        ("missing-return", "payload", remove_return),
        ("skipped-case-edge", "payload", skip_case),
        ("true-edge-is-not-excluded", "payload", true_edge_to_fallthrough),
        ("duplicate-is-not-coverage", "plain", duplicate_missing),
    ):
        doc = copy.deepcopy(seeds[seed])
        mutate(doc)
        source = work / f"{name}.negative.json"
        source.write_text(json.dumps(doc, separators=(",", ":")), encoding="utf-8")
        for backend, suffix in (("c", "c"), ("llvm", "ll")):
            target = work / f"{name}.{suffix}"
            result = invoke([f"--mir-json-backend={backend}", relative(source), "-o", relative(target)])
            diagnostic = result.stdout + result.stderr
            (work / f"{name}.{backend}.diagnostic").write_text(diagnostic, encoding="utf-8")
            ok = (result.returncode == 1 and not target.exists() and
                  "direct MIR scalar terminal return is invalid" in diagnostic)
            results.append({"case": name, "backend": backend, "passed": ok, "exit": result.returncode})
            print(f"[enum-match-mir] {'PASS' if ok else 'FAIL'}: {name} {backend} owned refusal, no output")
    report = {"driver_sha256": hashlib.sha256(driver.read_bytes()).hexdigest(),
              "results": results, "work": relative(work)}
    (work / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"[enum-match-mir] evidence: {work}")
    return 0 if all(row["passed"] for row in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
