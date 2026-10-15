#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here readonly-map-query "$PGY"
: "${READONLY_MAP_FACT_UNIT:?provide the separately built fact-phase unit; no build fallback}"
PYTHON_BIN="${PYTHON_BIN:-python}"
cd "$ROOT_DIR"
WORK="$(mktemp -d .tmp/readonly-map-query.XXXXXX)"
"$PYTHON_BIN" - "$ROOT_DIR" "$PGY" "$READONLY_MAP_FACT_UNIT" "$WORK" <<'PY'
import hashlib, json, pathlib, subprocess, sys, time


root, native, unit, work = map(pathlib.Path, sys.argv[1:])
root, native, unit, work = (p.resolve() for p in (root, native, unit, work))
if not native.is_file() or not unit.is_file():
    raise SystemExit('explicit native/unit missing; no rebuild fallback')
fixtures = root / 'tests/concept_semantics/hashmap'
positives = {
    'direct': '7\n0\n9\n', 'reverse_declaration': '11\n',
    'multi_hop': '13\n17\n', 'recursive': '19\n19\n', 'record_field': '23\n',
    'snapshot_drop': '1\n2\n29\n', 'snapshot_independent': 'stable\n41\n',
    'host_method': '43\n', 'snapshot_transfer': '1\n',
    'nested_string_operations': '7\ntrue\n1\n1\nfalse\n0\n',
    'nested_int_ref': '11\n11\n1\n4\n1\nfalse\n0\n',
    'string_readers': '1\ntrue\n4\n2\ntrue\n',
    'completed_observe': 'true\n',
    'slot_literal_store': 'literal\n7\n',
    'setter_literal_scalar': 'literal\n7\n',
    'local_string_getter': '9\n',
    'default_getter_scalar_loop': '27\n',
    'numeric_cast_read_loop': '3\n',
    'some_map_local': 'true\n',
    'some_field_local': 'true\n',
    'nominal_local': '17\n',
    'nominal_nested_user_call': 'true\n',
    'nominal_nested_map_read': 'true\n',
    'return_cleanup_order': '1\n1\nonce\ncleanup\n5\n1\n7\nvoid body\nvoid cleanup\n4\n',
}
negative_codes = {
    **{n: ('PGY_SEM_BORROW_ESCAPE', 'borrow_escape') for n in (
        'string_return', 'string_transitive_return', 'string_default_forward',
        'string_aggregate_capture', 'string_array_capture', 'array_return',
        'resource_return', 'ref_set', 'ref_remove', 'lexical_shadow_get',
        'lexical_shadow_keys', 'late_retaining_callee', 'recursive_retaining_callee',
        'nested_string_return', 'nested_string_aggregate', 'nested_array_string_return',
        'completed_observe_string', 'completed_observe_option', 'completed_observe_record',
        'completed_observe_inout', 'completed_observe_default_aggregate',
        'return_provenance_nested_keep', 'return_provenance_alias_keep',
        'return_provenance_alias_helper', 'return_provenance_nested_slot',
        'return_provenance_alias_slot', 'return_provenance_ref_slot_helper',
        'setter_provenance_alias_ref_string', 'setter_provenance_nested_ref_string',
        'setter_provenance_alias_ref_field', 'setter_provenance_nested_default_string',
        'numeric_cast_retaining_operand', 'some_map_keep', 'some_field_slot',
        'some_field_inout', 'some_recursive_output',
        'nominal_string_return', 'nominal_option_return', 'nominal_record_return',
        'nominal_slot', 'nominal_inout', 'nominal_keep', 'deferred_string_return')},
    'setter_provenance_scoped_shadow': ('PGY_SEM_REDECLARATION', 'semantic:scope:duplicate_symbol'),
    'function_shadow_get': ('PGY_SEM_REDECLARATION', 'semantic:function:builtin_name_reserved'),
    'function_shadow_keys': ('PGY_SEM_REDECLARATION', 'semantic:function:builtin_name_reserved'),
    'arity': ('PGY_SEM_BUILTIN_ARGS_INVALID', 'semantic:builtin:signature_mismatch'),
    'key_type': ('PGY_SEM_TYPE_MISMATCH', 'semantic:assignability_check'),
    'receiver_type': ('PGY_SEM_BUILTIN_ARGS_INVALID', 'semantic:builtin:signature_mismatch'),
    'unknown_result_type': ('PGY_SEM_BUILTIN_ARGS_INVALID', 'semantic:builtin:signature_mismatch'),
    'nested_unknown_value': ('PGY_SEM_BUILTIN_ARGS_INVALID', 'semantic:builtin:signature_mismatch'),
    'nested_key_type': ('PGY_SEM_TYPE_MISMATCH', 'semantic:assignability_check'),
    'string_reader_arity': ('PGY_SEM_BUILTIN_ARGS_INVALID', 'semantic:builtin:signature_mismatch'),
    'string_reader_type': ('PGY_SEM_TYPE_MISMATCH', 'semantic:assignability_check'),
    'string_replace_source_type': ('PGY_SEM_TYPE_MISMATCH', 'semantic:assignability_check'),
    'string_replace_search_type': ('PGY_SEM_TYPE_MISMATCH', 'semantic:assignability_check'),
    'string_replace_replacement_type': ('PGY_SEM_TYPE_MISMATCH', 'semantic:assignability_check'),
    'string_reader_shadow': ('PGY_SEM_REDECLARATION', 'semantic:function:builtin_name_reserved'),
}
inputs = [fixtures / f'readonly_map_query_{n}_valid.pgy' for n in positives]
inputs += [fixtures / f'readonly_map_query_{n}_invalid.pgy' for n in negative_codes]
sources = sorted(p for p in (root / 'src').rglob('*') if p.suffix in ('.c', '.h', '.def'))
sources += [root / 'Makefile', root / 'tests/readonly_map_query_smoke.sh',
            root / 'tests/semantic/readonly_map_query_fact_phase_unit.c']
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def capture(paths, name):
    rows = [(p, digest(p)) for p in paths]
    (work / name).write_text(''.join(f'{h}  {p.as_posix()}\n' for p, h in rows))
    return rows
guards = capture(sources, 'source.sha256') + capture(inputs, 'inputs.sha256')
guards += capture([native, unit], 'binaries.sha256')
start = time.monotonic()
failures = []
def run(label, args, intrinsic=120):
    remaining = 300 - (time.monotonic() - start)
    if remaining <= 0: raise SystemExit('focused 300-second budget exhausted')
    proc = subprocess.run([str(a) for a in args], cwd=root, capture_output=True,
                          timeout=min(intrinsic, remaining))
    (work / (label + '.out')).write_bytes(proc.stdout)
    (work / (label + '.err')).write_bytes(proc.stderr)
    return proc
try:
    result = run('fact-phase-unit', [unit], 30)
    if result.returncode != 0 or b'0 failures' not in result.stdout:
        failures.append('fact-phase unit failed')
    for name, expected in positives.items():
        source = fixtures / f'readonly_map_query_{name}_valid.pgy'
        for backend in ('c', 'llvm'):
            executable = work / f'{name}-{backend}.exe'
            result = run(f'{name}-{backend}-compile', [native, '--native-pipeline',
                f'--backend={backend}', '--opt=dev', source, '-o', executable])
            if result.returncode != 0:
                failures.append(f'{name}/{backend}: compile {result.returncode}')
                continue
            result = run(f'{name}-{backend}-run', [executable], 30)
            actual = result.stdout.decode().replace('\r\n', '\n')
            if result.returncode != 0 or actual != expected or result.stderr:
                failures.append(f'{name}/{backend}: output {actual!r}')
    for name, (code, cause) in negative_codes.items():
        # Unsafe inputs are analyzed only: no executable or C/LLVM emission.
        result = run(name + '-negative', [native, '--native-pipeline', '--mir-json',
            '--error-format=json', fixtures / f'readonly_map_query_{name}_invalid.pgy'])
        output = (result.stdout + result.stderr).decode(errors='replace')
        if result.returncode != 1 or '"pgy.mir.v1"' in output or code not in output or cause not in output:
            failures.append(f'{name}: expected {code}/{cause}, actual rc={result.returncode}')
        if name in ('numeric_cast_retaining_operand', 'some_map_keep',
                    'some_field_slot', 'some_field_inout', 'some_recursive_output',
                    'nominal_string_return', 'nominal_option_return', 'nominal_record_return',
                    'nominal_slot', 'nominal_inout', 'nominal_keep', 'deferred_string_return'):
            try:
                diagnostics = json.loads(output)
            except json.JSONDecodeError:
                failures.append(f'{name}: expected structured ownership-only diagnostics')
            else:
                if not isinstance(diagnostics, list):
                    failures.append(f'{name}: invalid diagnostic list')
                else:
                    errors = [row for row in diagnostics if isinstance(row, dict) and row.get('severity') == 'error']
                    if not errors or any(row.get('code') != 'PGY_SEM_BORROW_ESCAPE' or row.get('cause_ir') != 'semantic:borrow_escape' for row in errors):
                        failures.append(f'{name}: retaining falsifier has non-ownership errors')
finally:
    drift = [str(p) for p, before in guards if digest(p) != before]
    (work / 'end-guards.log').write_text('PASS\n' if not drift else '\n'.join(drift))
    if drift: failures.append('source/input/binary generation drift')
(work / 'results.log').write_text('\n'.join(failures) if failures else 'PASS\n')
print(f'[readonly-map-query] receipt {work}; {len(positives)} C/LLVM positives, {len(negative_codes)} analyze-only negatives, direct fact-phase unit')
if failures: raise SystemExit('\n'.join(failures))
PY
