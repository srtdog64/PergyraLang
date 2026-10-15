#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-focus-recovery "$PGY"
PYTHON_BIN="${PYTHON_BIN:-python3}"
CC_BIN="${PGY_SELFHOST_CC:-gcc}"
CC_PATH="$(command -v "$CC_BIN")"
[[ -n "$CC_PATH" ]] || { echo "missing emitted-C compiler" >&2; exit 1; }
cd "$ROOT_DIR"
mkdir -p .tmp
WORK="$(mktemp -d .tmp/collection-focus-recovery.XXXXXX)"
PYTHON_PLATFORM="$("$PYTHON_BIN" -c 'import sys; print(sys.platform)')"
PYTHON_ROOT="$ROOT_DIR"
PYTHON_PGY="$PGY"
PYTHON_WORK="$(cd "$WORK" && pwd)"
PYTHON_CC="$CC_PATH"
if [[ "$PYTHON_PLATFORM" == win32 ]]; then
    PYTHON_ROOT="$(pgy_path_for_windows_tool "$ROOT_DIR")"
    PYTHON_PGY="$(pgy_path_for_windows_tool "$PGY")"
    PYTHON_WORK="$(pgy_path_for_windows_tool "$(cd "$WORK" && pwd)")"
    PYTHON_CC="$(pgy_path_for_windows_tool "$CC_PATH")"
fi
"$PYTHON_BIN" - "$PYTHON_ROOT" "$PYTHON_PGY" "$PYTHON_WORK" "$PYTHON_CC" <<'PY'
import hashlib, pathlib, subprocess, sys, time, os, signal, re, json
root,native,work,cc=map(pathlib.Path,sys.argv[1:])
probe=root/'tests/self_hosted/fixtures/collection_focus_recovery_probe.pgy'
directory=root/'tests/self_hosted/parity/fixture/collection_field_lifetime'
positives={
'normal':'9\n9\nseed\nrow\n','late_failure':'-1\n9\nseed\nrow\n',
'manual_writeback_control':'-1\n9\nseed\nrow\n','nested_late_failure':'false\n9\nseed\nrow\n',
'branch_early_exit':'false\n1\nfalse\n9\nrow\n',
'eight_field_late_failure':'false\n'+'9\n'*8+'7\nrow\n',
'multi_iteration':'false\n25\nrow\n','scope_break_continue':'false\n17\nrow\n',
'defer_return_capture':'10\n9\n10\ndeferred\n','bare_fallthrough':'9\n17\nrow\n',
'try_option_failure':'7\nfalse\n9\nrow\n','try_result_failure':'8\ntrue\n9\nrow\n',
'partial_branch_repack':('false\n9\n9\nleft-row\nright-row\n'*2)}
negatives={'shared_alias_invalid':'borrow_boundary_escape','root_read_invalid':'move_from_released',
'retaining_alias_invalid':'borrow_boundary_escape','deferred_root_invalid':'move_from_released',
# Current whole-holder borrowing boundary control, not a runtime unsafety claim.
'dead_holder_late_failure':'borrow_boundary_escape'}
positives.update({'map_writeback_plain':'40\nfalse\n','map_writeback_early_return':'40\nfalse\n',
'map_dead_holder_paths':'48\nfalse\n41\n2\n','map_disjoint_backing':'9\n41\n','map_single_reader':'7\n'})
positives['map_bodyful_forward']='7\n'
positives['nominal_bodyful_forward']='1\n'
# These six exact analyze-only codes were observed on both frozen v9 probes.
negatives.update({name:'borrow_boundary_escape' for name in (
'map_root_alias_invalid','map_field_alias_invalid','map_selected_read_invalid',
'map_selected_mutation_invalid','map_selected_return_store_invalid','map_shared_backing_other_field_invalid')})
negatives['map_default_root_extern_retaining_invalid']='borrow_boundary_escape'
originals=['exclusive_inout_exit_before_writeback_negative.pgy','exclusive_inout_repeat_shared_alias_negative.pgy']
inputs=[directory/('focus_recovery_'+name+'.pgy') for name in list(positives)+list(negatives)]+[directory/name for name in originals]
source_paths={name:directory/('focus_recovery_'+name+'.pgy') for name in list(positives)+list(negatives)}
paired_declarations={
    'normal':(b'    let types: Array<String> = rows.types;',),
    'nested_late_failure':(b'    let types: Array<String> = rows.types;',b'    let rows: Rows = outer.rows;'),
    'scope_break_continue':(b'        let types: Array<String> = rows.types;',),
    'try_option_failure':(b'    let types: Array<String> = rows.types;',),
    'defer_return_capture':(b'    let types: Array<String> = rows.types;',),
    'shared_alias_invalid':(b'    let kept: Rows = rows;',b'    let types: Array<String> = rows.types;'),
    'root_read_invalid':(b'    let types: Array<String> = rows.types;',)}
paired_directory=work/'paired-var'
paired_directory.mkdir()
paired_manifest=[]
for baseline,declarations in paired_declarations.items():
    original=source_paths[baseline]
    before=original.read_bytes()
    if re.search(rb'(?m)^\s*import\b',before): raise RuntimeError(baseline+': paired relocation requires import-free source')
    lines=before.splitlines(keepends=True)
    replacements=[]
    for old in declarations:
        positions=[i for i,line in enumerate(lines) if line.rstrip(b'\r\n')==old]
        if len(positions)!=1: raise RuntimeError(baseline+': exact focus declaration premise failed')
        new=old.replace(b'let ',b'var ',1)
        index=positions[0]
        lines[index]=lines[index].replace(old,new,1)
        replacements.append({'line':index+1,'before':old.decode(),'after':new.decode()})
    after=b''.join(lines)
    inverse=after.splitlines(keepends=True)
    for replacement in replacements:
        index=replacement['line']-1
        old=replacement['before'].encode(); new=replacement['after'].encode()
        if inverse[index].rstrip(b'\r\n')!=new: raise RuntimeError(baseline+': paired inverse premise failed')
        inverse[index]=inverse[index].replace(new,old,1)
    if b''.join(inverse)!=before: raise RuntimeError(baseline+': non-spelling paired change')
    name='var_'+baseline
    path=paired_directory/(name+'.pgy')
    path.write_bytes(after)
    source_paths[name]=path
    inputs.append(path)
    paired_manifest.append({'case':name,'baseline':str(original),'source':str(path),'declarations':replacements,
                            'baseline_sha256':hashlib.sha256(before).hexdigest(),'paired_sha256':hashlib.sha256(after).hexdigest()})
paired_spec=work/'paired-var-inputs.json'
paired_spec.write_text(json.dumps(paired_manifest,indent=2)+'\n',encoding='utf-8')
inputs.append(paired_spec)
paired_membership=sorted(p for p in paired_directory.rglob('*') if p.is_file())
positive_cases=dict(positives)
negative_cases=dict(negatives)
for baseline in paired_declarations:
    if baseline in positives: positive_cases['var_'+baseline]=positives[baseline]
    else: negative_cases['var_'+baseline]=negatives[baseline]
def membership():
    return sorted(p for p in (root/'src').rglob('*') if p.is_file() and p.suffix in ('.pgy','.c','.h','.def'))
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
sources=membership()
zone_harness=directory/'focus_recovery_zone_cleanup_harness.c'
paths=sources+inputs+[probe,zone_harness,native,cc,pathlib.Path(sys.executable),root/'Makefile',root/'tests/pgy_binary_path_helpers.sh',
root/'tests/self_hosted/parity/collection_focus_recovery_owner.sh']
guards={p:digest(p) for p in paths}
(work/'inputs.sha256').write_text(''.join(f'{h}  {p}\n' for p,h in guards.items()),encoding='utf-8')
start = time.monotonic()
if sys.platform=='win32':
    import ctypes
    from ctypes import wintypes
    # Official Win32 SDK layouts:
    # https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_limit_information
    # https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_extended_limit_information
    # https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-io_counters
    class BasicLimits(ctypes.Structure):
        _fields_=[('PerProcessUserTimeLimit',ctypes.c_int64),('PerJobUserTimeLimit',ctypes.c_int64),
                  ('LimitFlags',wintypes.DWORD),('MinimumWorkingSetSize',ctypes.c_size_t),
                  ('MaximumWorkingSetSize',ctypes.c_size_t),('ActiveProcessLimit',wintypes.DWORD),
                  ('Affinity',ctypes.c_size_t),('PriorityClass',wintypes.DWORD),('SchedulingClass',wintypes.DWORD)]
    class IoCounters(ctypes.Structure):
        _fields_=[(name,ctypes.c_uint64) for name in ('ReadOperationCount','WriteOperationCount',
                    'OtherOperationCount','ReadTransferCount','WriteTransferCount','OtherTransferCount')]
    class ExtendedLimits(ctypes.Structure):
        _fields_=[('BasicLimitInformation',BasicLimits),('IoInfo',IoCounters),
                  ('ProcessMemoryLimit',ctypes.c_size_t),('JobMemoryLimit',ctypes.c_size_t),
                  ('PeakProcessMemoryUsed',ctypes.c_size_t),('PeakJobMemoryUsed',ctypes.c_size_t)]
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateJobObjectW.argtypes=[ctypes.c_void_p,wintypes.LPCWSTR]; kernel.CreateJobObjectW.restype=wintypes.HANDLE
    kernel.SetInformationJobObject.argtypes=[wintypes.HANDLE,ctypes.c_int,ctypes.c_void_p,wintypes.DWORD]; kernel.SetInformationJobObject.restype=wintypes.BOOL
    kernel.AssignProcessToJobObject.argtypes=[wintypes.HANDLE,wintypes.HANDLE]; kernel.AssignProcessToJobObject.restype=wintypes.BOOL
    kernel.TerminateJobObject.argtypes=[wintypes.HANDLE,wintypes.UINT]; kernel.TerminateJobObject.restype=wintypes.BOOL
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]; kernel.CloseHandle.restype=wintypes.BOOL
    class OwnedJob:
        def __init__(self):
            self.handle=kernel.CreateJobObjectW(None,None)
            if not self.handle: raise ctypes.WinError(ctypes.get_last_error())
            limits=ExtendedLimits()
            limits.BasicLimitInformation.LimitFlags=0x2000 | 0x200 # KILL_ON_JOB_CLOSE | JOB_MEMORY
            limits.JobMemoryLimit=3072*1024*1024
            if not kernel.SetInformationJobObject(self.handle,9,ctypes.byref(limits),ctypes.sizeof(limits)):
                error=ctypes.WinError(ctypes.get_last_error()); self.close(); raise error
        def assign(self,process):
            if not kernel.AssignProcessToJobObject(self.handle,wintypes.HANDLE(int(process._handle))):
                raise ctypes.WinError(ctypes.get_last_error())
        def terminate(self):
            if not kernel.TerminateJobObject(self.handle,1): raise ctypes.WinError(ctypes.get_last_error())
        def close(self):
            if self.handle:
                handle=self.handle; self.handle=None
                if not kernel.CloseHandle(handle): raise ctypes.WinError(ctypes.get_last_error())
    launcher = "import sys; token=sys.stdin.buffer.read(1); token==b'G' or sys.exit(125); import subprocess; sys.exit(subprocess.Popen(sys.argv[1:],stdin=subprocess.DEVNULL).wait())"
def run(label, args, limit, expected_rc=0, compile_step=False):
    remaining = 300 - (time.monotonic()-start)
    if remaining <= 10: raise RuntimeError('focused 300-second budget exhausted (cleanup reserve)')
    job=OwnedJob() if sys.platform=='win32' else None
    command=[str(x) for x in args]
    try:
        # Isolated/no-site startup prevents cwd shadow imports and startup hooks.
        p = subprocess.Popen(([sys.executable,'-I','-S','-c',launcher]+command) if job else command,
                             cwd=root, stdin=subprocess.PIPE if job else subprocess.DEVNULL,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             start_new_session=(sys.platform!='win32'))
    except BaseException:
        if job: job.close()
        raise
    try:
        if job:
            try:
                # Child launch is impossible before assigning this exact process handle.
                job.assign(p)
                p.stdin.write(b'G'); p.stdin.flush(); p.stdin.close(); p.stdin=None
            except BaseException as setup_failure:
                # A failed assignment leaves only the blocked launcher, no children.
                if p.poll() is None: p.terminate()
                try:
                    stdout,stderr=p.communicate(timeout=min(5,max(0.01,300-(time.monotonic()-start))))
                except subprocess.TimeoutExpired as cleanup_failure:
                    raise RuntimeError(label+': blocked launcher cleanup failed') from cleanup_failure
                (work/(label+'.out')).write_bytes(stdout); (work/(label+'.err')).write_bytes(stderr)
                raise RuntimeError(label+': owned Job setup/handshake failed') from setup_failure
        try:
            stdout, stderr = p.communicate(timeout=min(limit,remaining-10))
        except subprocess.TimeoutExpired as failure:
            stdout, stderr = failure.output or b'', failure.stderr or b''
            cleanup_error = None
            try:
                if job:
                    # Handle authority survives the actual parent's or launcher's exit.
                    job.terminate()
                else:
                    try: os.killpg(p.pid, signal.SIGKILL)
                    except ProcessLookupError: pass
                drain_remaining = 300-(time.monotonic()-start)
                if drain_remaining <= 0: raise RuntimeError('no pipe-drain budget remains')
                stdout, stderr = p.communicate(timeout=min(5,drain_remaining))
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as cleanup_failure:
                cleanup_error = str(cleanup_failure)
            finally:
                (work/(label+'.out')).write_bytes(stdout)
                (work/(label+'.err')).write_bytes(stderr)
                (work/(label+'.timeout.log')).write_text(
                    f'timeout launcher_pid={p.pid} launcher_rc={p.poll()} owned_job={job is not None} cleanup_error={cleanup_error}\n',encoding='utf-8')
            if cleanup_error: raise RuntimeError(label+': timeout cleanup failed: '+cleanup_error) from failure
            raise RuntimeError(label+': owned process tree timed out and was terminated') from failure
    finally:
        if job: job.close()
    (work/(label+'.out')).write_bytes(stdout); (work/(label+'.err')).write_bytes(stderr)
    if p.returncode != expected_rc or (stderr and not compile_step): raise RuntimeError(f'{label}: unexpected rc={p.returncode}/stderr')
    return stdout.decode('utf-8').replace('\r\n','\n')

def invoke(tool,name,mode,label,output=None,expected_rc=0):
    source=source_paths[name]
    destination=output or work/(label+'.forbidden.c')
    text=run(label,[tool,source.relative_to(root),mode,destination.relative_to(root)],30,expected_rc)
    if mode not in ('emit','manual-control','ordinary-control') and destination.exists(): raise RuntimeError(label+': forbidden target artifact')
    return text
try:
    tools={}
    for backend in ('c','llvm'):
        tool=work/(backend+'-probe.exe')
        run(backend+'-probe-compile',[native,'--native-pipeline','--backend='+backend,'--opt=dev',probe,'-o',tool],120,compile_step=True)
        guards[tool]=digest(tool); tools[backend]=tool
    for backend,tool in tools.items():
        observed=invoke(tool,'eight_field_late_failure','classifier',backend+'-classifier')
        if observed!='PROBE|classifier-valid|6|bounds2\n': raise RuntimeError(backend+': classifier refusal/restoration oracle drift '+repr(observed))
        current=invoke(tool,'map_single_reader','map-current',backend+'-map-current')
        if current!='PROBE|map-current-valid|17\n': raise RuntimeError(backend+': Map current-fact refusal/restoration oracle drift '+repr(current))
        forwarded=invoke(tool,'map_bodyful_forward','forward-current',backend+'-forward-current')
        if forwarded!='PROBE|forward-current-valid|19\n': raise RuntimeError(backend+': bodyful forwarding current tuple refusal/restoration drift '+repr(forwarded))
        produced=invoke(tool,'nominal_bodyful_forward','nominal-forward-producer',backend+'-nominal-forward-producer')
        if produced!='PROBE|nominal-forward-producer-valid|6\n': raise RuntimeError(backend+': nominal FieldReads producer barrier/restoration drift '+repr(produced))
    for name,code in negative_cases.items():
        for backend,tool in tools.items():
            if name == 'map_default_root_extern_retaining_invalid':
                located=invoke(tool,name,'map-analyze',backend+'-'+name+'-location',expected_rc=2)
                lines=located.splitlines()
                required=('body_ok=false','body_diagnostic=borrow_boundary_escape','body_syntax=29',
                    'body_function=Populate','body_atom=MapSet(values, ToString(index), index)',
                    '- boundary: unproved_exclusive_sink_entry')
                if any(lines.count(row)!=1 for row in required): raise RuntimeError(name+': observed retaining-forward refusal location changed '+repr(located))
            text=invoke(tool,name,'analyze',backend+'-'+name,expected_rc=2)
            if text!='PROBE|body-invalid|'+code+'\n': raise RuntimeError(name+': safety refusal drift: '+repr(text))
    # The original no-writeback source now has a real produced recovery group.
    # Its negative contract is absence/poison of that fact, not a blanket ban.
    original_recovery=originals[0]
    for name in originals[1:]:
        for backend,tool in tools.items():
            output=work/(backend+'-'+name+'.forbidden.c')
            text=run(backend+'-'+name,[tool,(directory/name).relative_to(root),'analyze',output.relative_to(root)],30,2)
            if text!='PROBE|body-invalid|borrow_boundary_escape\n' or output.exists(): raise RuntimeError(name+': original refusal changed')
    original_emitted=[]
    for backend,tool in tools.items():
        source=(directory/original_recovery).relative_to(root)
        forbidden=work/(backend+'-original-recovery.forbidden.c')
        baseline=run(backend+'-original-current',[tool,source,'view',forbidden.relative_to(root)],30)
        if not re.fullmatch(r'PROBE\|view-valid\|[1-9][0-9]*\|[1-9][0-9]*\|[1-9][0-9]*\n',baseline): raise RuntimeError('original recovery baseline failed')
        for route in ('','admitted-'):
            text=run(backend+'-original-'+route+'missing',[tool,source,route+'missing',forbidden.relative_to(root)],30,1)
            reason=('CODEGEN ERROR: codegen semantic body type admission receipt is missing or stale\n' if route
                    else 'CODEGEN ERROR: codegen semantic body type bundle is missing or invalid: collection_focus_recovery_rows\n')
            if text!=reason or forbidden.exists(): raise RuntimeError('original no-receipt refusal changed')
        restored=run(backend+'-original-restored',[tool,source,'view',forbidden.relative_to(root)],30)
        if restored!=baseline: raise RuntimeError('original restored view drift')
        artifact=work/(backend+'-original-recovery.c')
        text=run(backend+'-original-emit',[tool,source,'emit',artifact.relative_to(root)],30)
        if text!='PROBE|emitted-c\n' or not artifact.exists(): raise RuntimeError('original recovered emission failed')
        original_emitted.append(artifact.read_bytes()); guards[artifact]=digest(artifact)
    if original_emitted[0]!=original_emitted[1]: raise RuntimeError('original recovery emitter parity drift')
    poison_sources={m:'eight_field_late_failure' for m in ('missing','stamp','root','field','local','definition','exit','scope','duplicate','selection-missing','selection-value','selection-foreign','omit-active-part','copy-count','read-count','copy-epoch','copy-view-empty','read-map-value')}
    poison_sources['nested-inner-only']='nested_late_failure'
    for mode,name in poison_sources.items():
        for backend,tool in tools.items():
            # A genuine successful deep/current receipt immediately precedes each poison.
            baseline=invoke(tool,name,'view',backend+'-'+mode+'-baseline')
            if not re.fullmatch(r'PROBE\|view-valid\|[1-9][0-9]*\|[1-9][0-9]*\|[1-9][0-9]*\n',baseline): raise RuntimeError(mode+': failed baseline')
            # These original-receipt map seals do not claim a deep Current cut.
            routes=('admitted-',) if mode in ('read-map-value','copy-view-empty') else ('','admitted-')
            for route in routes:
                label=backend+'-'+route+mode
                text=invoke(tool,name,route+mode,label,expected_rc=1)
                reason=('CODEGEN ERROR: codegen semantic body type admission receipt is missing or stale\n' if route
                        else 'CODEGEN ERROR: codegen semantic body type bundle is missing or invalid: collection_focus_recovery_rows\n')
                if text!=reason: raise RuntimeError(label+': wrong refusal '+repr(text))
            restored=invoke(tool,name,'view',backend+'-'+mode+'-restored')
            if restored!=baseline: raise RuntimeError(mode+': restored control drift')
    flags=['-std=c11','-O0','-fwrapv','-fno-strict-aliasing','-I'+str(root/'src'),'-I'+str(root/'src/runtime')]
    if sys.platform=='darwin': flags+=['-D_DARWIN_C_SOURCE','-D_XOPEN_SOURCE=700','-pthread']
    elif sys.platform!='win32': flags+=['-D_POSIX_C_SOURCE=200809L','-D_XOPEN_SOURCE=700','-D_DEFAULT_SOURCE','-pthread']
    original_target=work/'original-recovered.exe'
    run('original-recovered-cc',[cc,*flags,work/'c-original-recovery.c','-o',original_target,'-lm'],120,compile_step=True)
    guards[original_target]=digest(original_target)
    # Main's exact original observable is StringLength("x") == 1, no stdout.
    if run('original-recovered-run',[original_target],30,expected_rc=1)!='': raise RuntimeError('original recovered observable changed')
    baseline_emitted={}
    for name,expected in positive_cases.items():
        emitted=[]
        for backend,tool in tools.items():
            artifact=work/(backend+'-'+name+'.c')
            mode='manual-control' if name in ('manual_writeback_control','map_writeback_plain',
                'map_writeback_early_return','map_dead_holder_paths','map_disjoint_backing','map_single_reader','map_bodyful_forward') else 'emit'
            # This fixture only forwards a nominal value to ArrayLength. Its
            # producer barrier is tested above; no descriptor needs restoration.
            if name == 'nominal_bodyful_forward': mode='ordinary-control'
            text=invoke(tool,name,mode,backend+'-'+name+'-emit',artifact)
            if text!='PROBE|emitted-c\n' or not artifact.is_file() or not artifact.stat().st_size: raise RuntimeError(name+': no admitted emitted C')
            emitted.append(artifact.read_bytes()); guards[artifact]=digest(artifact)
            baseline=name[4:] if name.startswith('var_') else name
            if baseline in paired_declarations and baseline in positives:
                if name.startswith('var_'):
                    if emitted[-1]!=baseline_emitted[(baseline,backend)]: raise RuntimeError(name+': paired emitted bytes/order drift')
                else:
                    baseline_emitted[(name,backend)]=emitted[-1]
        if emitted[0]!=emitted[1]: raise RuntimeError(name+': C/LLVM-built Pergyra emitters differ')
        target=work/(name+'-restored.exe')
        run(name+'-cc',[cc,*flags,work/('c-'+name+'.c'),'-o',target,'-lm'],120,compile_step=True)
        guards[target]=digest(target)
        # Only the actually restored Pergyra-emitted C is executed.
        text=run(name+'-run',[target],30)
        if text!=expected: raise RuntimeError(name+': stdout mismatch '+repr(text))
        if baseline in ('try_option_failure','try_result_failure'):
            threadsafe=work/(name+'-zone-cleanup.exe')
            generated=(work/('c-'+name+'.c')).as_posix()
            run(name+'-zone-cc',[cc,*flags,'-pthread','-DPGY_ZONE_THREADSAFE',
                '-DPGY_FOCUS_GENERATED_C="'+generated+'"',zone_harness,'-o',threadsafe,'-lm'],120,compile_step=True)
            guards[threadsafe]=digest(threadsafe)
            actual=run(name+'-zone-run',[threadsafe],30)
            if actual!=expected+'FOCUS ZONE CLEANUP PASS\n': raise RuntimeError(name+': actual local mutex cleanup mismatch')
    (work/'results.log').write_text('PASS: Pergyra C/LLVM-built emitters, direct/admitted receipt refusals, restored emitted-C execution; original 13 let baselines and 5 var pairs plus 4 Map controls; original 5 and 2 var refusals plus 6 Map refusals. Native target LLVM/MIR recovery OPEN.\n',encoding='utf-8')
finally:
    drift=[str(p) for p,h in guards.items() if not p.exists() or digest(p)!=h]
    if membership()!=sources: drift.append('source membership drift')
    if sorted(p for p in paired_directory.rglob('*') if p.is_file())!=paired_membership: drift.append('paired input membership drift')
    (work/'end-guards.log').write_text('PASS\n' if not drift else '\n'.join(drift),encoding='utf-8')
    if drift: raise RuntimeError('generation drift: '+','.join(drift))
print('[collection-focus-recovery] receipt '+str(work))
PY
