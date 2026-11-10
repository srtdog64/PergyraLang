"""Linux process boundary for one full-MIR producer, never an uncapped retry."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import resource
import signal
import subprocess
import sys
import time

MEMORY_BYTES = 14 * 1024**3
TIMEOUT_SECONDS = 30 * 60
DRAIN_SECONDS = 5


def validate_receipt(path):
    data = Path(path).read_bytes()
    record = json.loads(data)
    valid = (record.get('schema') == 'pgy.full-mir-process-boundary.v1'
             and record.get('memory_limit_kind') == 'RLIMIT_AS'
             and record.get('parent_death_signal') == 'SIGKILL'
             and type(record.get('core_dump_limit_bytes')) is int
             and record['core_dump_limit_bytes'] == 0
             and record.get('launch_signal_handoff') == 'blocked_until_owned'
             and type(record.get('memory_limit_bytes')) is int
             and 32 * 1024**2 <= record['memory_limit_bytes'] <= MEMORY_BYTES
             and type(record.get('timeout_seconds')) in (int, float)
             and 0 < record['timeout_seconds'] <= TIMEOUT_SECONDS
             and type(record.get('exit_code')) is int and record['exit_code'] == 0
             and record.get('reason') == 'completed'
             and record.get('process_group_drained') is True)
    if not valid:
        raise ValueError('full-MIR process boundary receipt is incomplete or failed')
    return hashlib.sha256(data).hexdigest()


class PhaseCancellation(Exception):
    def __init__(self, number):
        self.number = number


def run_phase(command, receipt, memory_bytes=MEMORY_BYTES,
              timeout_seconds=TIMEOUT_SECONDS):
    if sys.platform != 'linux':
        raise RuntimeError('full-MIR process isolation requires Linux')
    if not 32 * 1024**2 <= memory_bytes <= MEMORY_BYTES or not 0 < timeout_seconds <= TIMEOUT_SECONDS:
        raise ValueError('invalid full-MIR process budget')
    libc = ctypes.CDLL(None, use_errno=True)
    # Adopt this phase's orphaned grandchildren so cancellation can reap them.
    if libc.prctl(36, 1, 0, 0, 0) != 0:  # PR_SET_CHILD_SUBREAPER
        number = ctypes.get_errno()
        raise OSError(number, os.strerror(number))

    parent_pid = os.getpid()
    inherited_mask = set()

    def admit_address_space():
        resource.setrlimit(resource.RLIMIT_AS, (memory_bytes, memory_bytes))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        if libc.prctl(1, signal.SIGKILL, 0, 0, 0) != 0:  # PR_SET_PDEATHSIG
            number = ctypes.get_errno()
            raise OSError(number, os.strerror(number))
        if os.getppid() != parent_pid:
            os._exit(125)
        signal.pthread_sigmask(signal.SIG_SETMASK, inherited_mask)

    def cancel(number, frame):
        raise PhaseCancellation(number)

    receipt = Path(receipt)
    started = time.monotonic()
    record = {'schema': 'pgy.full-mir-process-boundary.v1',
              'memory_limit_bytes': memory_bytes, 'memory_limit_kind': 'RLIMIT_AS',
              'parent_death_signal': 'SIGKILL', 'core_dump_limit_bytes': 0,
              'launch_signal_handoff': 'blocked_until_owned',
              'timeout_seconds': timeout_seconds, 'process_group_drained': False}
    process = None
    status, reason = 126, 'owner_failed'
    previous = {}
    # Never replace another phase's receipt or silently reuse its output epoch.
    with receipt.open('x', encoding='utf-8') as output:
        try:
            for number in [signal.SIGINT, signal.SIGTERM]:
                previous[number] = signal.signal(number, cancel)
            # A pending cancellation must not arrive between fork and binding
            # the child's identity to this owner. The child restores its mask.
            inherited_mask = signal.pthread_sigmask(signal.SIG_BLOCK, set(previous))
            try:
                process = subprocess.Popen(command, start_new_session=True,
                                           preexec_fn=admit_address_space)
            finally:
                signal.pthread_sigmask(signal.SIG_SETMASK, inherited_mask)
            record['process_group'] = process.pid
            try:
                child_status = process.wait(timeout=timeout_seconds)
                status = child_status if child_status >= 0 else 128 - child_status
                reason = 'completed' if status == 0 else 'child_failed'
            except subprocess.TimeoutExpired:
                status, reason = 124, 'timeout'
        except PhaseCancellation as interrupted:
            status, reason = 128 + interrupted.number, 'interrupted'
        except (OSError, subprocess.SubprocessError) as failure:
            status, reason = 126, 'launch_failed'
            record['error'] = str(failure)
        finally:
            # Cleanup must not be interrupted a second time, leaving this phase
            # alive after the orchestration consumer has already retired it.
            for number in previous:
                signal.signal(number, signal.SIG_IGN)
            if process is not None:
                try:
                    os.killpg(process.pid, 0)
                except ProcessLookupError:
                    pass
                else:
                    if status == 0:
                        status, reason = 125, 'residual_children'
                    os.killpg(process.pid, signal.SIGKILL)
                drain_until = time.monotonic() + DRAIN_SECONDS
                try:
                    process.wait(timeout=max(0.001, drain_until - time.monotonic()))
                    while time.monotonic() < drain_until:
                        try:
                            adopted, _ = os.waitpid(-process.pid, os.WNOHANG)
                        except ChildProcessError:
                            record['process_group_drained'] = True
                            break
                        if adopted == 0:
                            time.sleep(0.01)
                    if not record['process_group_drained']:
                        status, reason = 125, 'drain_timeout'
                except subprocess.TimeoutExpired:
                    status, reason = 125, 'drain_timeout'
            else:
                record['process_group_drained'] = True
            for number, handler in previous.items():
                signal.signal(number, handler)
            record.update(exit_code=status, reason=reason,
                          wall_seconds=time.monotonic() - started)
            json.dump(record, output, indent=2)
            output.write('\n')
            output.flush()
    if status != 0:
        print(f'[full-mir-process-boundary] {reason}; exit={status}; receipt={receipt}', file=sys.stderr)
    return status


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--receipt')
    mode.add_argument('--validate-receipt')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if args.validate_receipt:
        if command:
            parser.error('receipt validation cannot execute a producer')
        print(validate_receipt(args.validate_receipt))
    else:
        if not command:
            parser.error('a producer command is required')
        sys.exit(run_phase(command, args.receipt))
