#!/usr/bin/env python3
"""Compare explicit, pinned allocators using already-admitted algorithm artifacts."""
import argparse
import json
import os
from pathlib import Path
import random
import shutil
import statistics
import subprocess
import sys

from raw_owner_pattern_trials import CASES, MOD, Trial, answers, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True, type=Path)
    parser.add_argument('--dependencies', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--phase-only', action='store_true', help='run only the distinct phase-lifetime shard')
    args = parser.parse_args()
    baseline, dependencies, output = args.baseline.resolve(), args.dependencies.resolve(), args.output.resolve()
    rows = json.loads((baseline / 'preflight.json').read_text())
    if len(rows) != 18 or not all(row.get('san_pass') and row.get('oracle_cases') == 251
                                 and row.get('oracle_failures') == [] for row in rows):
        raise RuntimeError('baseline is not the complete sanitizer/oracle/cleanup pass')
    manifest = json.loads((baseline / 'inputs.json').read_text())
    for path, sha in manifest.items():
        if path != 'harness' and digest(baseline / path) != sha:
            raise RuntimeError('baseline input changed: ' + path)
    expected_dependencies = {'mimalloc': 'b18ab4eef383f655e760881b02eff8af02800879',
                             'tlsf': 'deff9ab509341f264addbd3c8ada533678591905'}
    for name, sha in expected_dependencies.items():
        actual = subprocess.check_output(['git', '-C', str(dependencies / name), 'rev-parse', 'HEAD'], text=True).strip()
        if actual != sha:
            raise RuntimeError('dependency revision differs: ' + name)
        if subprocess.check_output(['git', '-C', str(dependencies / name), 'status', '--porcelain'], text=True).strip():
            raise RuntimeError('dependency has dirty source: ' + name)
    library = dependencies / 'mimalloc-build/libmimalloc.a'
    output.mkdir(parents=True, exist_ok=False)
    for name in ('logs', 'bin'):
        (output / name).mkdir()
    (output / 'src').symlink_to(baseline / 'src', target_is_directory=True)
    shared = Path(__file__).resolve().parent / 'cases/raw_owner_patterns'
    shutil.copytree(shared, output / 'fixtures')
    cpu = json.loads((baseline / 'environment.json').read_text())['cpu']
    os.sched_setaffinity(0, {cpu})
    record = {'baseline': str(baseline), 'baseline_manifest': digest(baseline / 'inputs.json'),
        'harness': digest(Path(__file__)),
        'oracle_process_owner': digest(Path(__file__).with_name('raw_owner_pattern_trials.py')),
        'dependencies': expected_dependencies,
        'mimalloc_library_sha256': digest(library), 'tlsf_pool_reserved_bytes': 16 * 1024 * 1024,
        'cpu': cpu, 'mode': 'phase-only' if args.phase_only else 'complete',
        'initialization_and_final_heap_collection_in_timing': False,
        'external_allocator_validation': 'independent result oracle, observed lifetimes, TLSF integrity; not ASan suballocation proof'}
    record['fixtures'] = {str(path.relative_to(output)): digest(path)
                          for path in (output / 'fixtures').iterdir() if path.is_file()}
    (output / 'inputs.json').write_text(json.dumps(record, indent=2) + '\n')
    trial = Trial(output)
    common = ['clang-18', '-O2', '-g0', '-fwrapv', '-fno-strict-aliasing', '-pthread', '-Isrc', '-Isrc/runtime']
    wrap = ['-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free']
    env = os.environ | {'PGY_PATTERN_ITERATIONS': '503', 'PGY_PATTERN_SEED': '0'}
    observations, releases = [], {}
    for allocator in (() if args.phase_only else ('libc', 'tlsf', 'mimalloc')):
        allocflags = ['-DPATTERN_USE_ALLOCATOR', 'fixtures/allocator_routes.c', *wrap]
        if allocator == 'tlsf':
            allocflags += ['-DPATTERN_TLSF', '-I' + str(dependencies / 'tlsf'), str(dependencies / 'tlsf/tlsf.c')]
        elif allocator == 'mimalloc':
            allocflags += ['-DPATTERN_MIMALLOC', '-I' + str(dependencies / 'mimalloc/include'), str(library), '-lrt', '-latomic']
        control = common + ['fixtures/allocator_controls.c', 'fixtures/bench_support.c', *allocflags,
                            '-o', 'bin/control-' + allocator]
        status, _, error = trial.run('control-build', control)
        if status:
            raise RuntimeError(error)
        status, _, error = trial.run('control-run', [str(output / ('bin/control-' + allocator))])
        if status:
            raise RuntimeError('allocator alignment/zeroing/exhaustion control: ' + allocator + error)
        for case in CASES:
            for backend in ('raw-c', 'c', 'llvm'):
                source = str(baseline / ('fixtures/raw_' + case + '.c' if backend == 'raw-c' else
                                         'emit/' + case + ('.c' if backend == 'c' else '.ll')))
                for mode in ('observe', 'release'):
                    key = case + '/' + backend + '/' + allocator
                    name = key.replace('/', '-') + '-' + mode
                    command = common + [source, 'fixtures/bench_support.c', *allocflags]
                    if mode == 'observe':
                        command += ['-DPATTERN_OBSERVE']
                    if backend == 'llvm':
                        command += [str(baseline / 'emit/runtime-release.o')]
                    command += ['-lm', '-o', 'bin/' + name]
                    status, _, error = trial.run(name + '-build', command)
                    if status:
                        raise RuntimeError(error)
                    if mode == 'observe':
                        status, text, error = trial.run(name, [str(output / 'bin' / name)], env)
                        value = json.loads(text) if status == 0 else None
                        expected = answers(case)
                        oracle = (sum(expected) * 2 + expected[0]) % MOD
                        if value is None or value['checksum'] != oracle or value['live_blocks'] != 0 or value['allocations'] != value['frees']:
                            raise RuntimeError(f'allocator preflight failed: {key}: {text} {error}')
                        observations.append({'case': case, 'backend': backend, 'allocator': allocator, **value})
                    else:
                        releases[key] = str(output / 'bin' / name)
        print(allocator + ': all six algorithms / three implementations passed cleanup and result checks.', flush=True)
    if not args.phase_only:
        (output / 'observations.json').write_text(json.dumps(observations, indent=2) + '\n')
    samples = {key: [] for key in releases}
    order, rng = list(releases), random.Random(20261107)
    for key, executable in releases.items():
        status, _, error = trial.run('warm', [executable], env | {'PGY_PATTERN_ITERATIONS': '251'})
        if status:
            raise RuntimeError(error)
    for repetition in range(0 if args.phase_only else 7):
        rng.shuffle(order)
        for key in order:
            expected = answers(key.split('/')[0])
            n = 12000
            oracle = (sum(expected) * (n // 251) + sum(expected[:n % 251])) % MOD
            status, text, error = trial.run('timing', [releases[key]], env | {'PGY_PATTERN_ITERATIONS': str(n)})
            value = json.loads(text) if status == 0 else None
            if value is None or value['checksum'] != oracle:
                raise RuntimeError('allocator timed result diverged: ' + key + text + error)
            samples[key].append(value['elapsed_ns'])
        print(f'Allocator repetition {repetition + 1}/7 passed.', flush=True)
    timing = {key: {'samples_ns': value, 'iterations': 12000, 'median_ns': statistics.median(value),
                    'min_ns': min(value), 'max_ns': max(value)} for key, value in samples.items()}
    if not args.phase_only:
        (output / 'timing.json').write_text(json.dumps(timing, indent=2) + '\n')

    # A different lifetime pattern: retain a phase of plain bytes, retire all,
    # then repeat. The production region and test-only next-fit are explicit.
    phase_rows, phase_releases = [], {}
    configs = ('libc', 'tlsf', 'mimalloc', 'region', 'cursor-prototype')
    phases = 64
    phase_oracle = sum((i * 37 + phase * 17 + word) % 65536
                       for phase in range(phases) for i in range(4096) for word in range(12)) % MOD
    for config in configs:
        extra = []
        if config in ('libc', 'tlsf', 'mimalloc'):
            extra += ['-DPATTERN_USE_ALLOCATOR', 'fixtures/allocator_routes.c', *wrap]
        if config == 'tlsf':
            extra += ['-DPATTERN_TLSF', '-I' + str(dependencies / 'tlsf'), str(dependencies / 'tlsf/tlsf.c')]
        elif config == 'mimalloc':
            extra += ['-DPATTERN_MIMALLOC', '-I' + str(dependencies / 'mimalloc/include'), str(library), '-lrt', '-latomic']
        elif config == 'region':
            extra += ['-DPHASE_REGION']
        elif config == 'cursor-prototype':
            extra += ['-DPHASE_CURSOR']
        for mode in ('observe', 'release', 'san'):
            if mode == 'san' and config in ('tlsf', 'mimalloc'):
                continue
            name = 'phase-' + config + '-' + mode
            flags = common if mode != 'san' else [arg for arg in common if arg != '-O2'] + ['-O1',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
            command = flags + ['fixtures/phase_allocator.c', 'fixtures/bench_support.c', *extra]
            if mode == 'observe':
                command += ['-DPATTERN_OBSERVE']
                if config in ('region', 'cursor-prototype'):
                    command += wrap
            command += ['-lm', '-o', 'bin/' + name]
            status, _, error = trial.run(name + '-build', command)
            if status:
                raise RuntimeError(error)
            if mode == 'release':
                phase_releases[config] = str(output / 'bin' / name)
            else:
                status, text, error = trial.run(name, [str(output / 'bin' / name)], env |
                    {'PGY_PATTERN_ITERATIONS': str(phases), 'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1',
                     'UBSAN_OPTIONS': 'halt_on_error=1'})
                values = [json.loads(line) for line in text.splitlines()] if status == 0 else []
                if not values or values[0]['checksum'] != phase_oracle:
                    raise RuntimeError('phase result/sanitizer failed: ' + name + text + error)
                if mode == 'observe' and (values[0]['live_blocks'] or values[0]['allocations'] != values[0]['frees']):
                    raise RuntimeError('phase allocation was not retired: ' + name + text)
                if config == 'cursor-prototype' and values[1]['first_phase_blocks'] != values[1]['final_phase_blocks']:
                    raise RuntimeError('prototype failed to reuse the phase blocks')
                phase_rows.append({'config': config, 'mode': mode, 'observation': values[0], 'retained': values[1]})
        print(config + ': phase payload/alignment/stability/retirement passed.', flush=True)
    phase_samples = {key: [] for key in configs}
    for _ in range(7):
        order = list(configs)
        rng.shuffle(order)
        for config in order:
            status, text, error = trial.run('phase-timing', [phase_releases[config]],
                                           env | {'PGY_PATTERN_ITERATIONS': str(phases)})
            values = [json.loads(line) for line in text.splitlines()] if status == 0 else []
            if not values or values[0]['checksum'] != phase_oracle:
                raise RuntimeError('timed phase diverged: ' + config + text + error)
            phase_samples[config].append(values[0]['elapsed_ns'])
    phase_timing = {key: {'samples_ns': value, 'phases': phases, 'objects_per_phase': 4096,
        'payload_bytes': 48, 'median_ns': statistics.median(value), 'min_ns': min(value), 'max_ns': max(value)}
        for key, value in phase_samples.items()}
    (output / 'phase-observations.json').write_text(json.dumps(phase_rows, indent=2) + '\n')
    (output / 'phase-timing.json').write_text(json.dumps(phase_timing, indent=2) + '\n')
    for path, sha in record['fixtures'].items():
        if digest(output / path) != sha:
            raise RuntimeError('frozen allocator fixture changed: ' + path)
    print('PASS: phase lifetime shard.' if args.phase_only else
          'PASS: three real allocators, six algorithms, repeated phase lifetime and explicit capacity control.', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
