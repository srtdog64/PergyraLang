#!/usr/bin/env python3
"""Survey buffer success/refusal and cleanup gaps without concealing red cases."""
import argparse
import json
import os
from pathlib import Path
import shutil
import statistics
import sys

from raw_owner_pattern_trials import Trial, asan_ir, digest


def work(seed):
    # Closed form per cell, rather than copying the implementation's loop.
    total = 0
    for cell in range(48):
        turn = ((cell - seed) * 37) % 48  # 13^-1 mod 48
        initial = (seed + 7 * (8 + cell)) % 1024
        first = (initial + turn + seed) % 1024
        total += first + (first + turn + 48 + seed) % 1024
    return total


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    baseline, output = args.baseline.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    for name in ('logs', 'emit', 'bin'):
        (output / name).mkdir()
    for name in ('src', 'stdlib'):
        (output / name).symlink_to(baseline / name, target_is_directory=True)
    shutil.copytree(Path(__file__).resolve().parents[1] / 'cases/raw_owner_patterns', output / 'fixtures')
    manifest = {'native_sha256': digest(baseline / 'native-pgy'),
                'baseline_manifest_sha256': digest(baseline / 'inputs.json'),
                'harness_sha256': digest(Path(__file__)),
                'oracle_process_owner_sha256': digest(Path(__file__).with_name('raw_owner_pattern_trials.py')),
                'fixtures': {str(path.relative_to(output)): digest(path)
                             for path in (output / 'fixtures').iterdir() if path.is_file()}}
    (output / 'inputs.json').write_text(json.dumps(manifest, indent=2) + '\n')
    cpu = json.loads((baseline / 'environment.json').read_text())['cpu']
    os.sched_setaffinity(0, {cpu})
    trial = Trial(output)
    common = ['clang-18', '-g0', '-fwrapv', '-fno-strict-aliasing', '-pthread', '-Isrc', '-Isrc/runtime']
    san = ['-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-DPATTERN_OBSERVE']
    wrap = ['-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free']
    env = os.environ | {'PGY_PATTERN_ITERATIONS': '257', 'PGY_PATTERN_SEED': '0',
                       'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'}
    rows, ready, errors = [], {}, []
    expected = (sum(work(seed) for seed in range(251)) + sum(work(seed) for seed in range(6))) % 1000003
    cases = ('fixed_owner', 'fixed_view', 'grow_then_view', 'borrow_branch_then_growth', 'returned_owner',
             'borrow_last_use_then_growth', 'live_view_growth_negative', 'returned_view')
    for case in cases:
        for backend in ('c', 'llvm'):
            key = case + '-' + backend
            suffix = '.c' if backend == 'c' else '.ll'
            source = 'emit/' + key + suffix
            status, text, error = trial.run(key + '-emit', [str(baseline / 'native-pgy'), '--native-pipeline',
                'fixtures/' + case + '.pgy', '--emit-c' if backend == 'c' else '--emit-llvm', '-o', source])
            row = {'case': case, 'backend': backend, 'admission_status': status, 'diagnostic': error[-2500:]}
            if case in ('borrow_last_use_then_growth', 'live_view_growth_negative'):
                row['intended_refusal'] = status != 0 and 'while Slice' in error and 'is live' in error
                if not row['intended_refusal']:
                    errors.append(key)
            elif status:
                errors.append(key)
            else:
                compiled_source = asan_ir(trial, source, 'emit/' + key + '-asan.ll') if backend == 'llvm' else source
                command = common + san + [compiled_source, 'fixtures/bench_support.c', *wrap]
                if backend == 'llvm':
                    command += [str(baseline / 'emit/runtime-san.o')]
                command += ['-lm', '-o', 'bin/' + key + '-san']
                built, _, build_error = trial.run(key + '-san-build', command)
                if built:
                    raise RuntimeError(build_error)
                invocation_env = env if case in ('fixed_owner', 'fixed_view', 'grow_then_view') else env | {'PGY_PATTERN_ITERATIONS': '1'}
                status, text, error = trial.run(key + '-san', [str(output / 'bin' / (key + '-san'))], invocation_env)
                observation = json.loads(text) if text.strip().startswith('{') else None
                row.update(status=status, observation=observation, runtime_error=error[-2200:])
                answer = expected if case in ('fixed_owner', 'fixed_view', 'grow_then_view') else (5 if case == 'borrow_branch_then_growth' else 1)
                row['result_pass'] = observation is not None and observation['checksum'] == answer
                if not row['result_pass']:
                    errors.append(key)
                if case in ('fixed_owner', 'fixed_view'):
                    row['cleanup_pass'] = (status == 0 and observation is not None and observation['checksum'] == answer
                        and observation['live_blocks'] == 0 and observation['allocations'] == observation['frees'])
                    if not row['cleanup_pass']:
                        errors.append(key)
                else:
                    # Survey any missing cleanup, but never time the leaking route.
                    row['cleanup_gap'] = observation is not None and observation['live_blocks'] != 0
                    row['lsan_detected'] = 'LeakSanitizer' in error
                    if (status != 0 and not row['lsan_detected']) or 'ERROR: AddressSanitizer:' in error or 'runtime error:' in error:
                        errors.append(key)
                if case in ('fixed_owner', 'fixed_view') and row.get('cleanup_pass'):
                    command = common + ['-O2', source, 'fixtures/bench_support.c']
                    if backend == 'llvm':
                        command += [str(baseline / 'emit/runtime-release.o')]
                    command += ['-lm', '-o', 'bin/' + key + '-release']
                    built, _, build_error = trial.run(key + '-release-build', command)
                    if built:
                        raise RuntimeError(build_error)
                    ready[key] = str(output / 'bin' / (key + '-release'))
            rows.append(row)
            print(json.dumps(row), flush=True)
    # Positive controls are not enough: the exact observer/sanitizer path must
    # reject both a real leak and a real post-free read.
    for control in ('leak', 'uaf'):
        flags = ['-DPATTERN_CONTROL_UAF'] if control == 'uaf' else []
        name = 'control-' + control
        built, _, error = trial.run(name + '-build', common + san + flags + ['fixtures/sanitizer_controls.c',
            'fixtures/bench_support.c', *wrap, '-o', 'bin/' + name])
        if built:
            raise RuntimeError(error)
        status, text, error = trial.run(name, [str(output / 'bin' / name)], env)
        caught = status != 0 and ('LeakSanitizer' if control == 'leak' else 'heap-use-after-free') in error
        rows.append({'control': control, 'caught': caught, 'status': status, 'stdout': text, 'error': error[-1500:]})
        if not caught:
            errors.append(name)
    ir_source = asan_ir(trial, 'fixtures/llvm_memory_control.ll', 'emit/llvm-control-asan.ll')
    status, _, error = trial.run('llvm-control-build', common + san + [ir_source, 'fixtures/bench_support.c',
        *wrap, '-o', 'bin/llvm-control'])
    if status:
        raise RuntimeError(error)
    status, text, error = trial.run('llvm-control-run', [str(output / 'bin/llvm-control')], env)
    caught = status != 0 and 'heap-use-after-free' in error
    rows.append({'control': 'llvm-direct-load-uaf', 'caught': caught, 'status': status, 'error': error[-1500:]})
    if not caught:
        errors.append('llvm-direct-load-uaf')
    (output / 'survey.json').write_text(json.dumps(rows, indent=2) + '\n')
    if errors:
        raise RuntimeError('unexpected buffer/control failures: ' + ', '.join(errors))
    raw, _, error = trial.run('raw-build', common + ['-O2', 'fixtures/raw_buffer.c', 'fixtures/bench_support.c',
                                                  '-o', 'bin/raw-buffer'])
    if raw:
        raise RuntimeError(error)
    ready['raw-c'] = str(output / 'bin/raw-buffer')
    n = 200000
    oracle = (sum(work(seed) for seed in range(251)) * (n // 251) + sum(work(seed) for seed in range(n % 251))) % 1000003
    samples = {key: [] for key in ready}
    for _ in range(7):
        for key, executable in ready.items():
            status, text, error = trial.run('timing', [executable], env | {'PGY_PATTERN_ITERATIONS': str(n)})
            value = json.loads(text) if status == 0 else None
            if value is None or value['checksum'] != oracle:
                raise RuntimeError('buffer timing result diverged: ' + key + text + error)
            samples[key].append(value['elapsed_ns'])
    timing = {key: {'iterations': n, 'samples_ns': values, 'median_ns': statistics.median(values),
        'min_ns': min(values), 'max_ns': max(values)} for key, values in samples.items()}
    (output / 'timing.json').write_text(json.dumps(timing, indent=2) + '\n')
    for path, sha in manifest['fixtures'].items():
        if digest(output / path) != sha:
            raise RuntimeError('frozen buffer fixture changed: ' + path)
    print('PASS: buffer survey and deliberate controls; cleanup gaps remain explicitly red and untimed.', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
