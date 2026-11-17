"""Execute retirement rights, emitted-C memory checks, and their falsifiers."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import resource
import subprocess
import sys


def main() -> None:
    root, work, native, codegen = map(Path, sys.argv[1:5])
    cc = sys.argv[5]
    root, work, native, codegen = (path.resolve() for path in (root, work, native, codegen))
    if not work.is_relative_to(root / '.tmp'):
        raise SystemExit('retirement test output is outside the owned scratch directory')
    print(f'[string-retirement] evidence={work}', flush=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    environment = os.environ.copy()
    environment['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=86'
    # libasan/liblsan share this exit flag; keep one explicit failure code.
    environment['LSAN_OPTIONS'] = 'exitcode=86'
    rows: list[dict] = []

    def run(label, arguments, *, expect=0, env=None):
        # GNU timeout owns a process group, including compiler grandchildren.
        result = subprocess.run(['timeout', '-k', '5s', '120s', *map(str, arguments)],
                                cwd=root, env=env or environment, capture_output=True)
        (work / f'{label}.stdout').write_bytes(result.stdout)
        (work / f'{label}.stderr').write_bytes(result.stderr)
        rows.append({'check': label, 'exit': result.returncode})
        if expect is not None and result.returncode != expect:
            raise RuntimeError(f'{label}: expected exit {expect}, got {result.returncode}; '
                               f'evidence={work}; {result.stderr[-1800:].decode(errors="replace")}')
        return result

    def relative(path):
        return str(path.relative_to(root))

    def emit(label, source):
        result = run(label + '.emit', [codegen, '--source', relative(source)])
        output = work / f'{label}.c'
        output.write_bytes(result.stdout.replace(b'\r\n', b'\n'))
        return output

    flags = ['-std=c11', '-O0', '-g', '-fwrapv', '-fno-strict-aliasing',
             '-D_POSIX_C_SOURCE=200809L', '-D_XOPEN_SOURCE=700', '-D_DEFAULT_SOURCE',
             '-Isrc', '-Isrc/runtime', '-pthread', '-fsanitize=address', '-fno-omit-frame-pointer']

    def build(label, source):
        binary = work / f'{label}.exe'
        run(label + '.build', [cc, *flags, source, '-lm', '-o', binary])
        return binary

    def memory_fault(label, text, diagnostic, mode):
        source = work / f'{label}.c'
        source.write_text(text)
        result = run(label + '.asan', [build(label, source)], expect=None, env=mode)
        if result.returncode != 86 or diagnostic not in result.stderr:
            raise RuntimeError(f'{label}: the unchecked-retire artifact did not expose {diagnostic!r}')

    fixture = root / 'tests/self_hosted/fixtures'
    plan = work / 'plan.exe'
    run('plan.build', [native, '--native-pipeline',
        'tests/self_hosted/fixtures/string_temporary_retire_plan_probe.pgy', '--backend=c', '-o', plan])
    result = run('plan.run', [plan])
    if result.stdout.strip() != b'STRING TEMPORARY RETIRE PLAN PASS':
        raise RuntimeError('the retire-plan positive and forged-plan checks did not finish')

    for name, leaks in [('loop', True), ('retained', False)]:
        source = fixture / f'string_temporary_retire_{name}.pgy'
        oracle = work / f'{name}.oracle.exe'
        run(name + '.oracle.build', [native, '--native-pipeline', relative(source),
                                      '--backend=c', '-o', oracle])
        reference = run(name + '.oracle.run', [oracle])
        emitted = emit(name, source)
        binary = build(name, emitted)
        mode = environment.copy()
        if not leaks:
            # Roots/user-call results remain unproved: this bridge does not
            # claim their leak freedom. Their post-call reads must be safe.
            mode['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1:exitcode=86'
        actual = run(name + '.asan', [binary], env=mode)
        if actual.stdout != reference.stdout:
            raise RuntimeError(f'{name}: the Pergyra emitter changed runtime output')
        if name == 'loop':
            original = emitted.read_text()
            mutant, removed = re.subn(r'free\(\(void \*\)__pgy_arg_[0-9]+_[0-9]+_*\);', '', original)
            if not removed:
                raise RuntimeError('the loop artifact contains no retirement to falsify')
            omitted = work / 'omitted-retire.c'
            omitted.write_text(mutant)
            leaking = run('omitted-retire.asan', [build('omitted-retire', omitted)], expect=None)
            if leaking.returncode != 86 or b'LeakSanitizer' not in leaking.stderr:
                raise RuntimeError('removing retirements was not detected as a real leak')
            rows.append({'check': 'removed_artifact_retires', 'count': removed})
            duplicated, changed = re.subn(
                r'(free\(\(void \*\)__pgy_arg_[0-9]+_[0-9]+_*\);)',
                r'\1 \1', original, count=1)
            if changed != 1:
                raise RuntimeError('the duplicate-retire mutation did not apply')
            memory_fault('duplicate-retire', duplicated, b'double-free', environment)
            premature, changed = re.subn(
                r'(?P<call>__auto_type\s+__pgy_call_result_[0-9]+_*\s*=\s*'
                r'pgy_concat\((?P<value>__pgy_arg_[0-9]+_0_*),[^)]*\);)\s*'
                r'(?P<retire>free\(\(void \*\)(?P=value)\);)',
                r'\g<retire> \g<call>', original, count=1)
            if changed != 1:
                raise RuntimeError('the premature-retire mutation did not apply')
            memory_fault('premature-retire', premature, b'heap-use-after-free', environment)
        else:
            original = emitted.read_text()
            borrowed, changed = re.subn(
                r'pgy_strlen\("static"\)',
                '({ const char *pgy_borrowed_probe = "static"; '
                'free((void *)pgy_borrowed_probe); pgy_strlen(pgy_borrowed_probe); })',
                original, count=1)
            if changed != 1:
                raise RuntimeError('the borrowed-retire mutation did not apply')
            memory_fault('borrowed-retire', borrowed, b'AddressSanitizer', mode)
            handed_off, changed = re.subn(
                r'(?P<type>\w+)\s+held\s*=\s*(?P<callee>\w+)'
                r'\(pgy_concat\("kept",\s*"-result"\)\);',
                r'\g<type> held = ({ const char *pgy_handoff_probe = pgy_concat("kept", "-result"); '
                r'__auto_type pgy_handoff_result = \g<callee>(pgy_handoff_probe); '
                r'free((void *)pgy_handoff_probe); pgy_handoff_result; });',
                original, count=1)
            if changed != 1:
                raise RuntimeError('the handed-off-retire mutation did not apply')
            memory_fault('handed-off-retire', handed_off, b'heap-use-after-free', mode)

    # Rename a later source binding to the first generated argument temporary.
    # A temporary that checks only its own initializer captures this later use.
    collision = work / 'collision.pgy'
    template = ('func Main() -> Void {\n'
                '    let collision_value: Int = 47;\n'
                '    Log(StringLength(Concat(ToString(1), ToString(collision_value))));\n}\n')
    collision.write_text(template)
    base = emit('collision.base', collision)
    match = re.search(r'__auto_type\s+(__pgy_arg_[0-9]+_0)\s*=\s*\(pgy_tostr\(1(?:LL)?\)', base.read_text())
    if match is None:
        raise RuntimeError('could not select the artifact binding used by the collision falsifier')
    collision.write_text(template.replace('collision_value', match.group(1)))
    renamed = emit('collision.renamed', collision)
    collision_run = run('collision.asan', [build('collision', renamed)])
    if collision_run.stdout.strip() != b'3':
        raise RuntimeError('a generated temporary captured the later source binding')

    record = {'codegen_sha256': hashlib.sha256(codegen.read_bytes()).hexdigest(),
              'native_sha256': hashlib.sha256(native.read_bytes()).hexdigest(), 'checks': rows}
    (work / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')
    print(f'[string-retirement] plans, source output, -O0 ASan/LSan and falsifiers passed; evidence={work}')


if __name__ == '__main__':
    main()
