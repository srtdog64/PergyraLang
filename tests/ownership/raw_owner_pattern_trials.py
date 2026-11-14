#!/usr/bin/env python3
"""Bounded Linux owner/View experiment; artifacts and timing are separate evidence.

Requires clang-18 and an explicitly selected native compiler/runtime snapshot.
Creates a new result directory; never edits that snapshot or shared compiler.
"""
import argparse
from collections import OrderedDict, deque
import hashlib
import heapq
import json
import os
from pathlib import Path
import random
import shutil
import signal
import statistics
import subprocess
import sys
import time

CASES = ('radix_sort', 'robin_hood', 'dijkstra', 'astar', 'union_find', 'lru')
MOD = 1000003


def answers(case):
    result = []
    for seed in range(251):
        if case == 'radix_sort':
            values, x = [], seed
            for _ in range(256):
                x = (x * 109 + 89) % 65536
                values.append(x)
            value = sum((i % 31 + 1) * x for i, x in enumerate(sorted(values)))
        elif case == 'robin_hood':
            mapping = {((i * 37 + seed) % 1009 + 1): ((i * 37 + seed) % 1009 + 1) * 3 + seed
                       for i in range(192)}
            for i in range(0, 192, 2):
                del mapping[(i * 37 + seed) % 1009 + 1]
            value = sum((i % 31 + 1) * mapping.get((i * 37 + seed) % 1009 + 1, 0)
                        for i in range(192))
        elif case in ('dijkstra', 'astar'):
            distance = [10**9] * 64
            distance[0], queue = 0, [(0, 0)]
            while queue:
                cost, node = heapq.heappop(queue)
                if cost != distance[node]:
                    continue
                for target in (node + 1 if node % 8 < 7 else -1,
                               node + 8 if node // 8 < 7 else -1,
                               node - 1 if node % 8 > 0 else -1,
                               node - 8 if node // 8 > 0 else -1):
                    if target < 0:
                        continue
                    candidate = cost + (node * 17 + target * 13 + seed) % 9 + 1
                    if candidate < distance[target]:
                        distance[target] = candidate
                        heapq.heappush(queue, (candidate, target))
            value = distance[63]
        elif case == 'union_find':
            graph, x = [set() for _ in range(256)], seed + 1
            for _ in range(512):
                x = (x * 109 + 89) % 65536
                a = x // 256
                x = (x * 109 + 89) % 65536
                b = x // 256
                graph[a].add(b)
                graph[b].add(a)
            unseen, components, sizes = set(range(256)), 0, [0] * 256
            while unseen:
                root = unseen.pop()
                component, pending = {root}, deque([root])
                while pending:
                    for node in graph[pending.popleft()]:
                        if node in unseen:
                            unseen.remove(node)
                            component.add(node)
                            pending.append(node)
                components += 1
                for node in component:
                    sizes[node] = len(component)
            value = components * 1009 + sum((i + 1) * size for i, size in enumerate(sizes))
        else:
            cache, value = OrderedDict(), 0
            for i in range(512):
                key = (i * 37 + seed + i // 17) % 128
                if key in cache:
                    value += cache[key]
                    cache.move_to_end(key, last=False)
                else:
                    if len(cache) == 64:
                        cache.popitem(last=True)
                    cache[key] = key * 3 + seed
                    cache.move_to_end(key, last=False)
            value += sum(rank * (key + item) for rank, (key, item) in enumerate(cache.items(), 1))
        result.append(value % MOD)
    return result


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Trial:
    def __init__(self, output):
        self.output = output
        self.serial = 0

    def run(self, name, argv, env=None, seconds=60):
        self.serial += 1
        log = self.output / 'logs' / f'{self.serial:05d}-{name}'
        with log.with_suffix('.out').open('wb') as stdout, log.with_suffix('.err').open('wb') as stderr:
            process = subprocess.Popen(argv, cwd=self.output, env=env, stdout=stdout, stderr=stderr,
                                       start_new_session=True)
            try:
                status = process.wait(timeout=seconds)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                raise RuntimeError(f'{name}: exceeded {seconds}s; process group killed and drained')
        return status, log.with_suffix('.out').read_text(), log.with_suffix('.err').read_text()


def asan_ir(trial, source, destination):
    # clang's C frontend adds sanitize_address; pre-existing LLVM IR lacks
    # that frontend step. Use LLVM's pass instead of silently linking an
    # uninstrumented generated function against the ASan runtime.
    status, _, error = trial.run('llvm-asan-attributes', ['opt-18', '-passes=forceattrs',
        '-force-attribute=sanitize_address', '-S', source, '-o', destination])
    if status:
        raise RuntimeError(error)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', required=True, type=Path)
    parser.add_argument('--runtime-root', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--timing', action='store_true', help='run seven release samples after all gates pass')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    for directory in ('logs', 'emit', 'bin'):
        (output / directory).mkdir()
    fixture = Path(__file__).resolve().parents[1] / 'cases/raw_owner_patterns'
    shutil.copytree(fixture, output / 'fixtures')
    shutil.copyfile(args.native, output / 'native-pgy')
    (output / 'native-pgy').chmod(0o755)
    for name in ('src', 'stdlib'):
        (output / name).symlink_to(args.runtime_root.resolve() / name, target_is_directory=True)
    manifest = {'native-pgy': digest(output / 'native-pgy')}
    for path in (output / 'fixtures').rglob('*'):
        if path.is_file():
            manifest[str(path.relative_to(output))] = digest(path)
    for path in (output / 'src/runtime').rglob('*'):
        if path.is_file():
            manifest[str(path.relative_to(output))] = digest(path)
    manifest['harness'] = digest(Path(__file__))
    (output / 'inputs.json').write_text(json.dumps(manifest, indent=2) + '\n')
    cpu = max(os.sched_getaffinity(0))
    os.sched_setaffinity(0, {cpu})
    trial = Trial(output)
    environment = os.environ.copy()
    environment.update(PGY_PATTERN_ITERATIONS='503', PGY_PATTERN_SEED='0',
                       ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    common = ['clang-18', '-g0', '-fwrapv', '-fno-strict-aliasing', '-pthread', '-Isrc', '-Isrc/runtime']
    san = ['-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    wrap = ['-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free']
    toolchain = {name: subprocess.check_output([name, '--version'], text=True).splitlines()[0]
                 for name in ('clang-18', 'opt-18', 'python3')}
    (output / 'environment.json').write_text(json.dumps({'cpu': cpu, 'toolchain': toolchain,
        'platform': dict(zip(('system', 'node', 'release', 'version', 'machine'), os.uname())),
        'release_flags': common + ['-O2'],
        'sanitizer_flags': san, 'repetitions': 7}, indent=2) + '\n')
    for mode, flags in (('san', san), ('release', ['-O2'])):
        status, _, error = trial.run('runtime-' + mode, common + flags + ['-DPGY_LLVM_ENABLED', '-c',
            'src/runtime/pgy_runtime_lib.c', '-o', 'emit/runtime-' + mode + '.o'])
        if status:
            raise RuntimeError(error)
    rows, release = [], {}
    for case in CASES:
        expected = answers(case)
        (output / (case + '-answers.json')).write_text(json.dumps(expected) + '\n')
        for backend in ('raw-c', 'c', 'llvm'):
            row = {'case': case, 'backend': backend}
            source = 'fixtures/raw_' + case + '.c'
            if backend != 'raw-c':
                source = 'emit/' + case + ('.c' if backend == 'c' else '.ll')
                status, _, error = trial.run(case + '-' + backend + '-emit', [str(output / 'native-pgy'),
                    '--native-pipeline', 'fixtures/' + case + '.pgy',
                    '--emit-c' if backend == 'c' else '--emit-llvm', '-o', source])
                row['admission_status'] = status
                if status:
                    row['error'] = error[-3000:]
                    rows.append(row)
                    print(json.dumps(row), flush=True)
                    continue
            for mode, flags in (('san', san), ('release', ['-O2'])):
                name = case + '-' + backend + '-' + mode
                compiled_source = asan_ir(trial, source, 'emit/' + case + '-asan.ll') if mode == 'san' and backend == 'llvm' else source
                command = common + flags + [compiled_source, 'fixtures/bench_support.c']
                if mode == 'san':
                    command += ['-DPATTERN_OBSERVE', *wrap]
                if backend == 'llvm':
                    command += ['emit/runtime-' + mode + '.o']
                command += ['-lm', '-o', 'bin/' + name]
                status, _, error = trial.run(name + '-build', command)
                row[mode + '_build_status'] = status
                if status:
                    row['error'] = error[-3000:]
                    break
                if mode == 'san':
                    status, text, error = trial.run(name, [str(output / 'bin' / name)], environment)
                    row['san_status'] = status
                    row['observation'] = json.loads(text) if text.strip().startswith('{') else None
                    row['san_error'] = error[-2500:]
                    if status or row['observation'] is None:
                        break
                    observation = row['observation']
                    oracle = (sum(expected) * 2 + expected[0]) % MOD
                    row['san_pass'] = (observation['checksum'] == oracle and observation['live_blocks'] == 0
                                       and observation['allocations'] == observation['frees'])
                    if not row['san_pass']:
                        break
                else:
                    env = environment | {'PGY_PATTERN_ITERATIONS': '1'}
                    wrong = []
                    for seed, answer in enumerate(expected):
                        status, text, error = trial.run(name + '-oracle', [str(output / 'bin' / name)],
                                                       env | {'PGY_PATTERN_SEED': str(seed)})
                        actual = json.loads(text) if status == 0 else None
                        if status or actual['checksum'] != answer:
                            wrong.append({'seed': seed, 'expected': answer, 'actual': actual, 'error': error[-800:]})
                    row['oracle_cases'] = len(expected)
                    row['oracle_failures'] = wrong
                    if not wrong:
                        release[(case, backend)] = str(output / 'bin' / name)
            rows.append(row)
            print(json.dumps(row), flush=True)
            (output / 'preflight.json').write_text(json.dumps(rows, indent=2) + '\n')
    passed = len(release) == len(CASES) * 3
    if not passed:
        print('FAIL: complete preflight did not pass; no timing performed.', flush=True)
        return 1
    if args.timing:
        counts = {'radix_sort': 12000, 'robin_hood': 12000, 'dijkstra': 12000,
                  'astar': 12000, 'union_find': 12000, 'lru': 12000}
        samples = {case + '/' + backend: [] for case, backend in release}
        order = list(release)
        rng = random.Random(20261107)
        # Warm every implementation before deterministic interleaving.
        for key, executable in release.items():
            status, _, error = trial.run('warm', [executable], environment | {'PGY_PATTERN_ITERATIONS': '251'})
            if status:
                raise RuntimeError(error)
        for repetition in range(7):
            rng.shuffle(order)
            for case, backend in order:
                n = counts[case]
                expected = answers(case)
                oracle = (sum(expected) * (n // 251) + sum(expected[:n % 251])) % MOD
                status, text, error = trial.run('timing', [release[(case, backend)]],
                    environment | {'PGY_PATTERN_ITERATIONS': str(n)})
                value = json.loads(text) if status == 0 else None
                if value is None or value['checksum'] != oracle:
                    raise RuntimeError(f'timing diverged: {case}/{backend}: {text} {error}')
                samples[case + '/' + backend].append(value['elapsed_ns'])
            print(f'Release repetition {repetition + 1}/7 passed.', flush=True)
        summaries = {key: {'iterations': counts[key.split('/')[0]], 'samples_ns': value,
            'median_ns': statistics.median(value), 'min_ns': min(value), 'max_ns': max(value)}
            for key, value in samples.items()}
        (output / 'timing.json').write_text(json.dumps(summaries, indent=2) + '\n')
    for path, sha in manifest.items():
        if path != 'harness' and digest(output / path) != sha:
            raise RuntimeError('frozen input changed: ' + path)
    print('PASS: all six algorithms, C/LLVM/raw C, sanitizer, cleanup and 251 independent seed oracles.', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
