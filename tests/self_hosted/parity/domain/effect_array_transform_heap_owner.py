"""Execute the isolated automatic-lifetime case and falsify its retirements."""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import resource
import subprocess
import sys


def main() -> None:
    root, work = (Path(value).resolve() for value in sys.argv[1:3])
    name = sys.argv[3]
    retained_name = sys.argv[4]
    if not work.is_relative_to(root / '.tmp'):
        raise SystemExit('heap evidence must stay in the owned scratch directory')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    environment = dict(os.environ,
        ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=86',
        LSAN_OPTIONS='exitcode=86')
    flags = ['-O0', '-g', '-fsanitize=address', '-fno-omit-frame-pointer',
             '-fwrapv', '-fno-strict-aliasing', '-Isrc', '-Isrc/runtime', '-pthread']
    rows: list[dict] = []

    def run(label: str, arguments, expected=0, run_environment=None):
        result = subprocess.run(['timeout', '-k', '5', '60', *map(str, arguments)],
                                cwd=root, env=run_environment or environment,
                                capture_output=True)
        (work / (label + '.stdout')).write_bytes(result.stdout)
        (work / (label + '.stderr')).write_bytes(result.stderr)
        rows.append({'check': label, 'exit': result.returncode})
        if expected is not None and result.returncode != expected:
            raise RuntimeError(f'{label}: expected {expected}, got {result.returncode}; '
                               f'{result.stderr[-1800:].decode(errors="replace")}')
        return result

    def build_c(label: str, source: Path) -> Path:
        binary = work / (label + '.exe')
        run(label + '.build', ['gcc', '-std=c11', *flags, source, '-lm', '-o', binary])
        return binary

    runtime = work / 'heap.runtime.o'
    run('heap.runtime.build', ['clang', '-std=c11', *flags,
        '-D_POSIX_C_SOURCE=200809L', '-D_XOPEN_SOURCE=700', '-D_DEFAULT_SOURCE',
        '-DPGY_LLVM_ENABLED', '-c', 'src/runtime/pgy_runtime_lib.c', '-o', runtime])
    expected = (work / (name + '.expected')).read_bytes()
    c_source = work / (name + '.c.c')
    c_binary = build_c('heap.c', c_source)
    if run('heap.c.run', [c_binary]).stdout != expected:
        raise RuntimeError('C automatic-lifetime output changed')

    def instrument_llvm(source: str) -> tuple[str, int]:
        lines = source.splitlines(keepends=True)
        definitions = 0
        for index, line in enumerate(lines):
            if line.startswith('define '):
                if not line.rstrip().endswith('{') or 'sanitize_address' in line:
                    raise RuntimeError('cannot instrument the emitted LLVM definition')
                lines[index] = line.rstrip()[:-1] + 'sanitize_address {\n'
                definitions += 1
        if definitions == 0:
            raise RuntimeError('no emitted LLVM definition was instrumented')
        return ''.join(lines), definitions

    original = (work / (name + '.llvm.ll')).read_text()
    instrumented, definitions = instrument_llvm(original)
    checked = work / 'heap.instrumented.ll'
    checked.write_text(instrumented)
    llvm_binary = work / 'heap.llvm.exe'
    run('heap.llvm.build', ['clang', *flags, '-x', 'ir', checked, '-x', 'none', runtime,
                           '-lm', '-o', llvm_binary])
    if run('heap.llvm.run', [llvm_binary]).stdout != expected:
        raise RuntimeError('LLVM automatic-lifetime output changed')

    # An omitted or duplicated actual retirement must be observable at -O0.
    source = c_source.read_text()
    releases = list(re.finditer(r'^    free\(pgy_local_[0-9]+\.data\);\n', source, re.M))
    if not releases:
        raise RuntimeError('no actual array retirement is available to falsify')
    # Earlier frees reset zero-initialized descriptors. The final one retires
    # an allocated array, so duplicating it exercises an actual release right.
    final = releases[-1]
    omitted = re.sub(r'^    free\(pgy_local_[0-9]+\.data\);\n', '', source, flags=re.M)
    duplicated = source[:final.end()] + final.group() + source[final.end():]
    for label, text, diagnostic in (
        ('heap.omitted', omitted, b'LeakSanitizer'),
        ('heap.duplicated', duplicated, b'double-free'),
    ):
        path = work / (label + '.c')
        path.write_text(text)
        result = run(label + '.run', [build_c(label, path)], expected=86)
        if diagnostic not in result.stderr:
            raise RuntimeError(f'{label}: the intended memory fault was not observed')

    # Prove instrumentation of module loads/stores, beyond libc interception.
    fault = checked.read_text()
    entry = re.search(r'^define [^\n]*@main\([^\n]*\) [^\n]*\{\n[^\n]*:\n', fault, re.M)
    if entry is None:
        raise RuntimeError('emitted LLVM entry is unavailable for the raw-store fault')
    fault = fault[:entry.end()] + '  call void @pgy_asan_raw_store_fault()\n' + fault[entry.end():]
    for symbol, declaration in [('malloc', 'declare ptr @malloc(i64)'), ('free', 'declare void @free(ptr)')]:
        if not re.search(r'^declare [^\n]*@' + symbol + r'\(', fault, re.M):
            fault += '\n' + declaration + '\n'
    fault += '''
define internal void @pgy_asan_raw_store_fault() sanitize_address {
entry:
  %heap = call ptr @malloc(i64 8)
  call void @free(ptr %heap)
  store volatile i8 1, ptr %heap, align 1
  ret void
}
'''
    path = work / 'heap.raw-store.ll'
    path.write_text(fault)
    binary = work / 'heap.raw-store.exe'
    run('heap.raw-store.build', ['clang', *flags, '-x', 'ir', path, '-x', 'none', runtime,
                                '-lm', '-o', binary])
    result = run('heap.raw-store.run', [binary], expected=86)
    if b'heap-use-after-free' not in result.stderr:
        raise RuntimeError('LLVM raw store escaped the sanitizer oracle')

    # A returned aggregate still points at its source array. Its cleanup is
    # unproved here; check the reads for early free without requiring a leak
    # verdict at this separate ownership boundary.
    retained_environment = dict(environment, ASAN_OPTIONS=
        'detect_leaks=0:halt_on_error=1:abort_on_error=0:exitcode=86')
    retained_expected = (work / (retained_name + '.expected')).read_bytes()
    retained_c = build_c('heap.retained.c', work / (retained_name + '.c.c'))
    if run('heap.retained.c.run', [retained_c],
           run_environment=retained_environment).stdout != retained_expected:
        raise RuntimeError('retained C array descriptor changed')
    retained_checked, retained_definitions = instrument_llvm(
        (work / (retained_name + '.llvm.ll')).read_text())
    retained_ir = work / 'heap.retained.instrumented.ll'
    retained_ir.write_text(retained_checked)
    retained_llvm = work / 'heap.retained.llvm.exe'
    run('heap.retained.llvm.build', ['clang', *flags, '-x', 'ir', retained_ir,
        '-x', 'none', runtime, '-lm', '-o', retained_llvm])
    if run('heap.retained.llvm.run', [retained_llvm],
           run_environment=retained_environment).stdout != retained_expected:
        raise RuntimeError('retained LLVM array descriptor changed')
    (work / 'heap.results.json').write_text(json.dumps({
        'instrumented_definitions': definitions,
        'retained_instrumented_definitions': retained_definitions,
        'checks': rows}, indent=2) + '\n')
    print('[effect-builtin-heap] PASS: C/LLVM isolated retirement, retained reads; omitted, duplicate and raw-store faults detected')


if __name__ == '__main__':
    main()
