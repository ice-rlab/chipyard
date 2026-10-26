#!/usr/bin/env python3
"""Run a measured workload ELF with AutoTrace disabled/enabled, serially."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import statistics
import subprocess
import time


def sha256(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('simulator', type=Path)
    p.add_argument('binary', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--repeats', type=int, default=3)
    p.add_argument('--workload', default='quicksort')
    p.add_argument('--roi', default='sort plus counter-enable/disable boundary instructions')
    p.add_argument('--max-cycles', type=int, default=1000000)
    p.add_argument('--timeout', type=int, default=300)
    a = p.parse_args()
    if a.repeats < 1:
        p.error('--repeats must be positive')
    simulator, binary = a.simulator.resolve(), a.binary.resolve()
    a.output.mkdir(parents=True, exist_ok=False)
    root = a.output.resolve()
    result = dict(workload=a.workload, simulator=str(simulator), simulator_sha256=sha256(simulator),
                  binary=str(binary), binary_sha256=sha256(binary),
                  scope=f'ROI: {a.roi}; wall: simulator process including final drain',
                  unavailable=['committed_branches', 'taken_branches', 'icache_accesses',
                               'frontend_stall_cycles', 'committed_branch_misprediction_rate'],
                  sampling_settings=dict(period='every enabled event', history_depth=32,
                                         capture_batches=16, stream_queue_entries=6144,
                                         sampling_interrupts=0, trace_scope='whole simulator run'),
                  runs=[])
    summary_path = root / 'summary.json'
    for repeat in range(a.repeats):
        modes = ('baseline', 'collection') if repeat % 2 == 0 else ('collection', 'baseline')
        for mode in modes:
            directory = root / f'{repeat}-{mode}'
            directory.mkdir()
            flags = ['+autotrace-disable'] if mode == 'baseline' else [f'+autotrace-filename-base={directory}/trace']
            command = [str(simulator), '+permissive', '+fesvr-step-size=128',
                       f'+max-cycles={a.max_cycles}', *flags, '+permissive-off', str(binary)]
            start = time.perf_counter()
            with (directory / 'simulator.log').open('w') as log:
                proc = subprocess.run(command, cwd=directory, stdin=subprocess.DEVNULL,
                                      stdout=log, stderr=subprocess.STDOUT, timeout=a.timeout)
            elapsed = time.perf_counter() - start
            log = (directory / 'simulator.log').read_text()
            row = dict(mode=mode, repeat=repeat, command=command,
                       wall_seconds=elapsed, returncode=proc.returncode)
            result['runs'].append(row)
            summary_path.write_text(json.dumps(result, indent=2) + '\n')
            target = re.search(r'(?:QSORT|WORKLOAD)_METRICS (.*)', log)
            cycles = re.search(r'Target Cycles Emulated:\s*(\d+)', log)
            bridge_rows = re.findall(r'AUTOTRACE_METRICS (.*)', log)
            if proc.returncode or not target or not cycles or not bridge_rows or '*** PASSED ***' not in log:
                raise RuntimeError(f'{mode} failed or missing measurements: {directory}/simulator.log')
            row['counters'] = {k: int(v) if v.isdigit() else v for k, v in re.findall(r'(\w+)=(\S+)', target[1])}
            if row['counters'].get('correctness') != 'PASS':
                raise RuntimeError('workload correctness failed')
            row['simulated_cycles'] = int(cycles[1])
            row['simulation_cycles_per_second'] = int(cycles[1]) / elapsed
            row['bridges'] = [{k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', line)} for line in bridge_rows]
            if any(b['enabled'] != (mode == 'collection') for b in row['bridges']):
                raise RuntimeError(f'{mode}: AutoTrace enable state did not match requested mode')
            c = row['counters']
            row['cpi'] = c['target_cycles'] / c['retired_instructions']
            row['icache_acquire_mpki'] = 1000 * c['icache_acquires'] / c['retired_instructions']
            summary_path.write_text(json.dumps(result, indent=2) + '\n')
            print(f'{mode} {repeat}: {elapsed:.3f}s, ROI {c["target_cycles"]} cycles, '
                  f'{row["simulation_cycles_per_second"]:.1f} target cycles/s', flush=True)
    medians = {}
    for mode in ('baseline', 'collection'):
        rows = [r for r in result['runs'] if r['mode'] == mode]
        medians[mode] = {k: statistics.median(r[k] for r in rows)
                         for k in ('wall_seconds', 'simulation_cycles_per_second', 'cpi', 'icache_acquire_mpki')}
        medians[mode]['counters'] = {k: statistics.median(r['counters'][k] for r in rows)
                                   for k in rows[0]['counters'] if k != 'correctness'}
    result['medians'] = medians
    result['collection_slowdown'] = medians['collection']['wall_seconds'] / medians['baseline']['wall_seconds']
    result['collection_roi_cycle_change_fraction'] = medians['collection']['counters']['target_cycles'] / medians['baseline']['counters']['target_cycles'] - 1
    summary_path.write_text(json.dumps(result, indent=2) + '\n')
    print(f'Saved {summary_path}', flush=True)


if __name__ == '__main__':
    main()
