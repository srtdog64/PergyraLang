#!/usr/bin/env python3
"""Installed C/LLVM lifetime parity, physical release and unsafe-release controls."""
import copy
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[4]
DRIVER = Path(os.environ['PGY_SELFHOST_PREBUILT_DRIVER']).resolve()
BUILD = Path(os.environ.get('PGY_OWNED_LOCAL_TEST_DIR') or
             tempfile.mkdtemp(prefix='owned-local-return-', dir=ROOT / '.tmp'))
BUILD.mkdir(parents=True, exist_ok=True)
ENV = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
           LSAN_OPTIONS='exitcode=99')
ROWS = []


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(command, name, env=ENV, timeout=45):
    with (BUILD / (name + '.out')).open('wb') as out, (BUILD / (name + '.err')).open('wb') as err:
        process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=out,
                                   stderr=err, start_new_session=True)
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            raise AssertionError(f'{name} exceeded {timeout}s; descendants drained')
    return code, (BUILD / (name + '.out')).read_text(), (BUILD / (name + '.err')).read_text()


def require(command, name, env=ENV, timeout=45):
    code, out, err = run(command, name, env, timeout)
    assert code == 0, (name, code, out[-3000:], err[-6000:])
    return out


CASES = {
    'string_local': ('func Work(n: Int) -> Int { let text = Concat("abcdef", "ghij"); return StringLength(text); }', 200, True),
    'array_local': ('func Work(n: Int) -> Int { let rows: Array<Int> = [n,n+1,n+2]; return rows[0]; }', 190, True),
    'array_return': ('func Make(n: Int) -> Array<Int> { return [n,n+1]; } func Work(n: Int) -> Int { let rows = Make(n); return rows[0]; }', 190, True),
    'string_return': ('func Make(n: Int) -> String { let text = Concat("abcdef", "ghij"); return text; } func Work(n: Int) -> Int { let text = Make(n); return StringLength(text); }', 200, True),
    'record_scalar_member': ('struct Record { text: String; n: Int; } func Work(n: Int) -> Int { let record = Record(Concat("a", "b"), n); return record.n; }', 190, True),
    'record_local': ('struct Record { text: String; } func Work(n: Int) -> Int { let record = Record(Concat("abcdef", "ghij")); return StringLength(record.text); }', 200, True),
    'record_return': ('struct Record { text: String; } func Make(n: Int) -> Record { return Record(Concat("abcdef", "ghij")); } func Work(n: Int) -> Int { let record = Make(n); return StringLength(record.text); }', 200, True),
    'string_branch': ('func Work(n: Int) -> Int { if n % 2 == 0 { let text = Concat("abcdef", "ghij"); return StringLength(text); } return 10; }', 200, True),
    'string_loop': ('func Work(n: Int) -> Int { let sum = 0; let i = 0; while i < 5 { let text = Concat("a", "b"); sum = sum + StringLength(text); i = i + 1; } return sum; }', 200, True),
    'borrowed_literal': ('func Make(n: Int) -> String { return "abcdefghij"; } func Work(n: Int) -> Int { let text = Make(n); return StringLength(text); }', 200, True),
    'borrowed_parameter': ('func Identity(text: String) -> String { return text; } func Work(n: Int) -> Int { let text = Identity("abcdefghij"); return StringLength(text); }', 200, True),
    'nested_concat': ('func Work(n: Int) -> Int { let text = Concat(Concat("ab", "cd"), Concat("ef", "ghij")); return StringLength(text); }', 200, True),
    'array_bool': ('func Work(n: Int) -> Int { let rows: Array<Bool> = [true,false,true]; if rows[2] { return n; } return 0; }', 190, True),
    'fresh_forward': ('func Make(n: Int) -> String { return Concat("abc", "defghij"); } func Forward(n: Int) -> String { return Make(n); } func Work(n: Int) -> Int { let text = Forward(n); return StringLength(text); }', 200, True),
    'independent_fields': ('struct Pair { a: String; b: String; } func Make(n: Int) -> Pair { return Pair(Concat("abc", "defghij"), Concat("klm", "nopqrst")); } func Work(n: Int) -> Int { let pair = Make(n); return StringLength(pair.a) + StringLength(pair.b); }', 400, True),
    'constructor_identity': ('struct A { text: String; } struct B { text: String; } func Work(n: Int) -> Int { let a = A(Concat("a", "b")); let b = B(Concat("c", "d")); return StringLength(a.text) + StringLength(b.text); }', 80, True),
    'qualified_enum': ('enum Number { Value(Int), } func Work(n: Int) -> Int { let value = Number.Value(n); match value { case Value(result): return result; } }', 190, True),
    'mixed_return': ('func Make(n: Int) -> String { if n % 2 == 0 { return "abcdefghij"; } return Concat("abcdef", "ghij"); } func Work(n: Int) -> Int { let text = Make(n); return StringLength(text); }', 200, True),
    'stored_alias': ('struct Record { text: String; } func Work(n: Int) -> Int { let text = Concat("abc", "defghij"); let record = Record(text); return StringLength(text) + StringLength(record.text); }', 400, True),
    'duplicate_return_fields': ('struct Pair { a: String; b: String; } func Make(n: Int) -> Pair { let text = Concat("abc", "defghij"); return Pair(text,text); } func Work(n: Int) -> Int { let pair = Make(n); return StringLength(pair.a) + StringLength(pair.b); }', 400, True),
    'duplicate_return_arrays': ('struct Pair { a: Array<Int>; b: Array<Int>; } func Make(n: Int) -> Pair { let rows: Array<Int> = [n,n+1]; return Pair(rows,rows); } func Work(n: Int) -> Int { let pair = Make(n); return pair.a[0] + pair.b[0]; }', 380, True),
    'borrowed_fresh_identity': ('func Identity(text: String) -> String { return text; } func Work(n: Int) -> Int { let text = Concat("abc", "defghij"); let other = Identity(text); return StringLength(text) + StringLength(other); }', 400, True),
}
# Array descriptor transfer and per-field element rights survive local and
# alias carriers. Each fresh result is replaced repeatedly by the same caller.
for shape, ending in (
    ('direct', 'return Bag(names);'),
    ('local', 'let bag: Bag = Bag(names); return bag;'),
    ('alias', 'let first: Bag = Bag(names); let second: Bag = first; return second;'),
):
    for ownership, value in (('borrowed', '"value"'), ('owned', 'Concat("val", "ue")')):
        CASES['array_string_record_' + shape + '_' + ownership] = (
            'struct Bag { names: Array<String>; } func Build() -> Bag { '
            'let names: Array<String> = []; ArrayPush(names, ' + value + '); ' + ending +
            ' } func Work(n: Int) -> Int { let bag: Bag = Build(); return StringLength(bag.names[0]); }',
            100, True)
CASES['array_string_nested_record'] = (
    'struct Bag { names: Array<String>; } struct Outer { bag: Bag; } '
    'func Build() -> Outer { let names: Array<String> = []; ArrayPush(names, Concat("val", "ue")); '
    'let bag=Bag(names); return Outer(bag); } '
    'func Work(n: Int) -> Int { let outer=Build(); return StringLength(outer.bag.names[0]); }', 100, True)
CASES['array_string_record_replacement'] = (
    'struct Bag { names: Array<String>; } func Build() -> Bag { let names: Array<String> = []; '
    'ArrayPush(names, Concat("val", "ue")); return Bag(names); } '
    'func Work(n: Int) -> Int { let i=0; let total=0; while i<5 { let bag=Build(); '
    'total=total+StringLength(bag.names[0]); i=i+1; } return total; }', 500, True)
CASES['duplicate_return_string_arrays'] = (
    'struct Pair { a: Array<String>; b: Array<String>; } '
    'func Build() -> Pair { let names: Array<String> = []; ArrayPush(names, Concat("val", "ue")); '
    'return Pair(names, names); } '
    'func Work(n: Int) -> Int { let pair=Build(); return StringLength(pair.a[0])+StringLength(pair.b[0]); }',
    200, True)
MAIN = '\nfunc Main() -> Void { let total = 0; let i = 0; while i < 20 { total = total + Work(i); i = i + 1; } Log(total); }\n'
runtime = BUILD / 'runtime.asan.o'
require(['gcc', '-O0', '-g', '-fsanitize=address', '-fno-omit-frame-pointer',
         '-DPGY_LLVM_ENABLED', '-std=c11', '-Isrc', '-Isrc/runtime', '-pthread',
         '-c', 'src/runtime/pgy_runtime_lib.c', '-o', str(runtime)], 'runtime', timeout=60)
for name, (body, expected, leak_free) in CASES.items():
    source = BUILD / (name + '.pgy'); source.write_text(body + MAIN)
    mir = BUILD / (name + '.mir.json')
    output = require([str(DRIVER), '--emit-mir-json-verified', str(source.relative_to(ROOT))], name + '.source')
    document = json.loads(output); mir.write_text(output)
    for backend in ('c', 'llvm'):
        key = name + '.' + backend
        artifact = BUILD / (key + ('.c' if backend == 'c' else '.ll'))
        require([str(DRIVER), '--mir-json-backend=' + backend, str(mir.relative_to(ROOT)),
                 '-o', str(artifact.relative_to(ROOT))], key + '.emit')
        binary = BUILD / (key + '.exe')
        command = ['gcc' if backend == 'c' else 'clang', '-O0', '-g', '-fsanitize=address',
                   '-fno-omit-frame-pointer', '-fwrapv', '-fno-strict-aliasing',
                   '-Isrc', '-Isrc/runtime', '-pthread', str(artifact)]
        if backend == 'llvm': command.append(str(runtime))
        command += ['-lm', '-o', str(binary)]
        require(command, key + '.link')
        env = ENV if leak_free else dict(ENV, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1')
        observed = require([str(binary)], key + '.run', env, timeout=10)
        assert observed == str(expected) + '\n', (key, expected, observed)
        ROWS.append(dict(case=name, backend=backend, gate='ASan+LSan' if leak_free else 'ASan-live-alias',
                         source_sha256=sha(source), mir_sha256=sha(mir), artifact_sha256=sha(artifact),
                         binary_sha256=sha(binary)))
        print(key, ROWS[-1]['gate'], 'PASS', flush=True)
        (BUILD / 'results.json').write_text(json.dumps(ROWS, indent=2) + '\n')

# Target IDs are separately validated from the graph topology digest. A
# self-consistent graph with a missing/crossed declaration must fail closed.
base = json.loads((BUILD / 'constructor_identity.mir.json').read_text())
other = next(node['call_target_syntax_id'] for routine in base['routines']
             for block in routine['blocks'] for instruction in block['instructions']
             for node in (instruction.get('expr0_graph') or {}).get('nodes', [])
             if node['kind'] == 'call' and node['call_target_name'] == 'B')
assert other > 0
for mutation, target in (('missing', 0), ('crossed', other)):
    document = copy.deepcopy(base); changed = 0
    for routine in document['routines']:
        for block in routine['blocks']:
            for instruction in block['instructions']:
                for field in ('expr0_graph', 'expr1_graph'):
                    graph = instruction.get(field)
                    if not graph: continue
                    for node in graph['nodes']:
                        if node['kind'] == 'call' and node['call_target_name'] == 'A':
                            node['call_target_syntax_id'] = target; changed += 1
    assert changed > 0
    mir = BUILD / ('constructor-' + mutation + '.mir.json')
    mir.write_text(json.dumps(document, separators=(',', ':')))
    for backend in ('c', 'llvm'):
        output = BUILD / ('refused-' + mutation + '.' + backend); output.write_text('preserved\n')
        code, stdout, stderr = run([str(DRIVER), '--mir-json-backend=' + backend,
                                   str(mir.relative_to(ROOT)), '-o', str(output.relative_to(ROOT))],
                                  'constructor-' + mutation + '.' + backend)
        assert code != 0 and 'CODEGEN ERROR' in stdout + stderr, (mutation, backend, code, stdout, stderr)
        assert output.read_text() == 'preserved\n', (mutation, backend, 'refusal replaced output')
(BUILD / 'identity.json').write_text(json.dumps(dict(driver=str(DRIVER), driver_sha256=sha(DRIVER),
    runtime_sha256=sha(runtime), lsan_cases=sum(row['gate']=='ASan+LSan' for row in ROWS),
    live_alias_cases=sum(row['gate']=='ASan-live-alias' for row in ROWS), constructor_refusals=4), indent=2)+'\n')
print('Installed C/LLVM local-return cleanup and constructor-identity negatives: PASS')
