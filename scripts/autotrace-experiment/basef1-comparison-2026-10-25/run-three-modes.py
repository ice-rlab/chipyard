#!/usr/bin/env python3
"""Fresh plain/idle/collect BOOM metasimulations; never modify earlier test folders."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import statistics
import subprocess
import time

HERE = Path(__file__).resolve().parent
EXPERIMENT = HERE.parent
ROOT = EXPERIMENT.parents[1]
NAMES = ('dhrystone', 'towers', 'mm', 'rsort', 'mt-matmul', 'memcpy', 'qsort')
MODES = ('plain', 'idle', 'collect')


def digest(path):
    checksum = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            checksum.update(chunk)
    return checksum.hexdigest()


def save(path, data):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(data, indent=2) + '\n')
    temporary.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--resume', action='store_true', help='retain completed runs and retry incomplete runs with their files preserved')
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error('positive repeat count required')
    output = HERE / 'runs'
    output.mkdir(exist_ok=args.resume)
    old = json.loads((EXPERIMENT / 'suite-results-2026-10-25/suite-summary.json').read_text())
    build = json.loads((EXPERIMENT / 'suite-build/manifest.json').read_text())
    gen = ROOT / 'sims/firesim/sim/generated-src/f1'
    images = {}
    for platform in ('BaseF1Config', 'AutoTraceF1Config'):
        directory = gen / f'f1-firesim-FireSim-FireSimLargeBoomConfig-{platform}'
        binary = directory / 'VFireSim'
        header = directory / 'FireSim-generated.const.h'
        text = header.read_text()
        has_bridge = 'AUTOTRACEBRIDGEMODULE_struct{' in text
        assert has_bridge == (platform == 'AutoTraceF1Config'), (platform, 'unexpected bridge presence')
        images[platform] = dict(path=str(binary), sha256=digest(binary), header=str(header),
                                header_sha256=digest(header), autotrace_bridge_present=has_bridge)
    assert images['AutoTraceF1Config']['sha256'] == old['workloads']['dhrystone']['simulator_sha256'], 'Existing AutoTrace executable changed'
    data = dict(target='FireSimLargeBoomConfig', execution='local Verilator metasimulation, one hart',
                repeats=args.repeats, modes={
                    'plain': dict(platform='BaseF1Config', bridge_present=False, collecting=False),
                    'idle': dict(platform='AutoTraceF1Config', bridge_present=True, collecting=False),
                    'collect': dict(platform='AutoTraceF1Config', bridge_present=True, collecting=True)},
                images=images, workloads={}, failures=[],
                timing_scope='whole simulator process including startup, log output, final drain and close; excludes build and validation',
                order='Each repetition rotates plain/idle/collect, so each mode occupies each position once in three repetitions',
                sampling=old['workloads']['dhrystone']['sampling_settings'],
                counter_definitions=old['counter_definitions'],
                unavailable=old['workloads']['dhrystone']['unavailable'],
                build_manifest=str(EXPERIMENT / 'suite-build/manifest.json'),
                original_test_hashes=str(HERE / 'original-tests-sha256.json'))
    path = HERE / 'summary.json'
    if args.resume:
        previous = json.loads(path.read_text())
        assert previous['repeats'] == args.repeats
        assert previous['images'] == images, 'Simulator images changed during interruption'
        data = previous
        data['failures'] = []
        data.setdefault('resume_events', []).append(dict(time_unix=time.time(), reason='Resume interrupted runner; retain validated runs and preserve incomplete files'))
    save(path, data)
    for name in NAMES:
        binary = EXPERIMENT / f'suite-build/{name}.riscv'
        sha = digest(binary)
        assert sha == build['workloads'][name]['binary_sha256'] == old['workloads'][name]['binary_sha256'], (name, 'ELF changed')
        workload = data['workloads'].setdefault(name, dict(binary=str(binary), binary_sha256=sha, scope=old['workloads'][name]['scope'], runs=[]))
        assert workload['binary_sha256'] == sha
        if 'medians' in workload:
            print(f'Already complete: {name}', flush=True)
            continue
        print(f'=== {name} ===', flush=True)
        try:
            for repeat in range(args.repeats):
                order = MODES[repeat % 3:] + MODES[:repeat % 3]
                for mode in order:
                    completed = [r for r in workload['runs'] if r['repeat'] == repeat and r['mode'] == mode and 'bridges' in r and 'cpi' in r and r['returncode'] == 0]
                    if completed:
                        assert len(completed) == 1
                        print(f'Retained {name} {mode} {repeat}', flush=True)
                        continue
                    directory = output / name / f'{repeat}-{mode}'
                    if directory.exists():
                        assert args.resume, f'Run directory already exists: {directory}'
                        suffix = 1
                        backup = directory.with_name(directory.name + f'.interrupted-{suffix}')
                        while backup.exists():
                            suffix += 1
                            backup = directory.with_name(directory.name + f'.interrupted-{suffix}')
                        directory.rename(backup)
                        removed = [r for r in workload['runs'] if r['repeat'] == repeat and r['mode'] == mode]
                        workload.setdefault('interrupted_attempts', []).append(dict(mode=mode, repeat=repeat, preserved_directory=str(backup), records=removed))
                        workload['runs'] = [r for r in workload['runs'] if not (r['repeat'] == repeat and r['mode'] == mode)]
                    directory.mkdir(parents=True)
                    platform = data['modes'][mode]['platform']
                    flags = []
                    if mode == 'idle':
                        flags = ['+autotrace-disable']
                    elif mode == 'collect':
                        flags = [f'+autotrace-filename-base={directory}/trace']
                    command = [images[platform]['path'], '+permissive', '+fesvr-step-size=128',
                               '+max-cycles=5000000', *flags, '+permissive-off', str(binary)]
                    start = time.perf_counter()
                    with (directory / 'simulator.log').open('w') as log:
                        proc = subprocess.run(command, cwd=directory, stdin=subprocess.DEVNULL,
                                              stdout=log, stderr=subprocess.STDOUT, timeout=1800)
                    elapsed = time.perf_counter() - start
                    text = (directory / 'simulator.log').read_text()
                    row = dict(mode=mode, repeat=repeat, platform=platform, command=command,
                               wall_seconds=elapsed, returncode=proc.returncode, log=str(directory / 'simulator.log'))
                    workload['runs'].append(row)
                    save(path, data)
                    target = re.findall(r'WORKLOAD_METRICS (.*)', text)
                    cycles = re.search(r'Target Cycles Emulated:\s*(\d+)', text)
                    bridge = re.findall(r'AUTOTRACE_METRICS (.*)', text)
                    assert proc.returncode == 0 and '*** PASSED ***' in text, f'{name}/{mode}: simulation failed'
                    assert len(target) == 1 and cycles, f'{name}/{mode}: missing target measurements'
                    row['counters'] = {k: int(v) if v.isdigit() else v for k, v in re.findall(r'(\w+)=(\S+)', target[0])}
                    c = row['counters']
                    assert c['correctness'] == 'PASS' and c['target_cycles'] > 0 and c['retired_instructions'] > 0
                    row['simulated_cycles'] = int(cycles[1])
                    row['cycles_per_second'] = int(cycles[1]) / elapsed
                    row['cpi'] = c['target_cycles'] / c['retired_instructions']
                    row['icache_acquire_mpki'] = 1000 * c['icache_acquires'] / c['retired_instructions']
                    row['bridges'] = [{k:int(v) for k,v in re.findall(r'(\w+)=(\d+)', line)} for line in bridge]
                    assert len(row['bridges']) == (0 if mode == 'plain' else 1), f'{name}/{mode}: wrong AutoTrace bridge count'
                    assert all(b['enabled'] == (mode == 'collect') for b in row['bridges'])
                    if mode != 'collect':
                        assert not list(directory.glob('trace*.csv'))
                        assert all(b['trace_records'] == b['trace_stream_bytes'] == b['host_cycles_blocked'] == b['capture_max_occupancy'] == 0 for b in row['bridges'])
                    save(path, data)
                    print(f'{name} {mode} {repeat}: {elapsed:.3f}s, ROI {c["target_cycles"]} cycles, {row["cycles_per_second"]:.1f} target cycles/s', flush=True)
            medians = {}
            for mode in MODES:
                rows = [r for r in workload['runs'] if r['mode'] == mode]
                medians[mode] = {k:statistics.median(r[k] for r in rows)
                                 for k in ('wall_seconds', 'cycles_per_second', 'cpi', 'icache_acquire_mpki')}
                medians[mode]['counters'] = {k:statistics.median(r['counters'][k] for r in rows)
                                            for k in rows[0]['counters'] if k != 'correctness'}
            workload['medians'] = medians
            workload['slowdowns'] = dict(idle_over_plain=medians['idle']['wall_seconds']/medians['plain']['wall_seconds'],
                                         collect_over_idle=medians['collect']['wall_seconds']/medians['idle']['wall_seconds'],
                                         collect_over_plain=medians['collect']['wall_seconds']/medians['plain']['wall_seconds'])
            workload['all_roi_counters_identical'] = all(r['counters'] == workload['runs'][0]['counters'] for r in workload['runs'])
            print(f'Completed {name}: {workload["slowdowns"]}', flush=True)
        except Exception as error:
            workload['error'] = str(error)
            data['failures'].append(name)
            print(f'FAILED {name}: {error}', flush=True)
        save(path, data)
    if data['failures']:
        raise SystemExit('Failed workloads: ' + ', '.join(data['failures']))


if __name__ == '__main__':
    main()
