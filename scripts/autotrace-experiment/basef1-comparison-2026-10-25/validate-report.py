#!/usr/bin/env python3
"""Validate fresh three-mode runs, preserve old tests, and report all three costs."""
import csv
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import sys

HERE = Path(__file__).resolve().parent
EXPERIMENT = HERE.parent
ROOT = EXPERIMENT.parents[1]


def digest(path):
    checksum = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            checksum.update(chunk)
    return checksum.hexdigest()


def main():
    path = HERE / 'summary.json'
    data = json.loads(path.read_text())
    assert not data['failures'] and len(data['workloads']) == 7, 'Incomplete or failed suite'
    old = json.loads((HERE / 'original-tests-sha256.json').read_text())
    for name, checksum in old.items():
        assert digest(Path(name)) == checksum, f'Original test changed: {name}'
    for image in data['images'].values():
        assert digest(Path(image['path'])) == image['sha256'], 'Simulator image changed'
        assert digest(Path(image['header'])) == image['header_sha256'], 'Simulator header changed'
    checker = ROOT / 'sims/firesim/sim/autotrace-results/check_quicksort_history.py'
    rows = []
    targets, throughput, costs, ranges = [], [], [], []
    validations = {}
    with (HERE / 'history-validation.log').open('w') as history_log:
        for name, workload in data['workloads'].items():
            assert digest(Path(workload['binary'])) == workload['binary_sha256']
            assert len(workload['runs']) == 3 * data['repeats']
            for mode in ('plain', 'idle', 'collect'):
                assert len([r for r in workload['runs'] if r['mode'] == mode]) == data['repeats']
            for run in workload['runs']:
                assert run['returncode'] == 0 and run['counters']['correctness'] == 'PASS'
                folder = Path(run['log']).parent
                traces = list(folder.glob('trace*.csv'))
                record_count = 0
                for trace in traces:
                    with trace.open(newline='') as stream:
                        record_count += sum(1 for _ in csv.DictReader(stream))
                assert record_count == sum(b['trace_records'] for b in run['bridges'])
                assert sum(t.stat().st_size for t in traces) == sum(b['trace_csv_bytes'] for b in run['bridges'])
                assert all(b['capture_occupancy_final'] == 0 for b in run['bridges'])
                if run['mode'] == 'plain':
                    assert not run['bridges'] and not traces
                elif run['mode'] == 'idle':
                    assert len(run['bridges']) == 1 and not traces
                    assert all(b['enabled'] == b['trace_records'] == b['trace_stream_bytes'] == b['host_cycles_blocked'] == b['capture_max_occupancy'] == 0 for b in run['bridges'])
                else:
                    assert len(run['bridges']) == 1 and traces and all(b['enabled'] == 1 for b in run['bridges'])
            trace = HERE / 'runs' / name / '0-collect/trace0.csv'
            check = subprocess.run([sys.executable, str(checker), str(trace)], text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            history_log.write(name + '\n' + check.stdout + '\n')
            assert check.returncode == 0, (name, check.stdout)
            print(name + ': ' + check.stdout.splitlines()[0], flush=True)
            identical = all(r['counters'] == workload['runs'][0]['counters'] for r in workload['runs'])
            validations[name] = dict(correctness_passed=True, csv_records_and_bytes_match=True,
                                      history_checked=True, final_buffers_empty=True,
                                      all_roi_counters_identical=identical)
            m = workload['medians']
            workload['wall_seconds_ranges'] = {
                mode: dict(min=min(r['wall_seconds'] for r in workload['runs'] if r['mode'] == mode),
                           max=max(r['wall_seconds'] for r in workload['runs'] if r['mode'] == mode))
                for mode in ('plain', 'idle', 'collect')}
            ranges.append('| ' + name + ' | ' + ' | '.join(
                f"{workload['wall_seconds_ranges'][mode]['min']:.3f}–{workload['wall_seconds_ranges'][mode]['max']:.3f}"
                for mode in ('plain', 'idle', 'collect')) + ' |')
            row = dict(workload=name, plain_wall_seconds=m['plain']['wall_seconds'],
                       idle_wall_seconds=m['idle']['wall_seconds'], collect_wall_seconds=m['collect']['wall_seconds'])
            row.update({key + '_percent':100*(value-1) for key,value in workload['slowdowns'].items()})
            rows.append(row)
            for mode in ('plain', 'idle', 'collect'):
                c = m[mode]['counters']
                targets.append(f"| {name} | {mode} | {c['target_cycles']:,} | {c['retired_instructions']:,} | "
                               f"{m[mode]['cpi']:.5f} | {c['icache_acquires']:,} | {m[mode]['icache_acquire_mpki']:.5f} | "
                               f"{c['branch_misprediction_recoveries']:,} |")
            throughput.append('| ' + name + ' | ' + ' | '.join(f"{m[mode]['cycles_per_second']:,.1f}" for mode in ('plain','idle','collect')) + ' |')
            collect = [r for r in workload['runs'] if r['mode'] == 'collect']
            med = lambda key: statistics.median(sum(b[key] for b in r['bridges']) for r in collect)
            peak = max(b['capture_max_occupancy'] for r in collect for b in r['bridges'])
            costs.append(f"| {name} | {med('trace_records'):,.0f} | {med('trace_stream_bytes'):,.0f} | "
                         f"{med('trace_csv_bytes'):,.0f} | {med('host_cycles_blocked'):,.0f} | {peak} / 16 |")
    data['validation'] = dict(workloads=validations, original_test_files_unchanged=len(old),
                              simulator_images_unchanged=True, all_simulations_passed=True,
                              simulation_count=sum(len(w['runs']) for w in data['workloads'].values()))
    data['comparison'] = rows
    data['source_revisions'] = {name:subprocess.check_output(['git','-C',str(ROOT/folder),'rev-parse','HEAD'],text=True).strip()
                                for name,folder in [('chipyard','.'),('boom','generators/boom'),('firesim','sims/firesim')]}
    path.write_text(json.dumps(data, indent=2) + '\n')
    with (HERE / 'comparison.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    report = '''# BaseF1 / AutoTrace idle / AutoTrace collection comparison

Fresh experiment, 2026-10-25, separate from both earlier tests. Local FireSim
Golden Gate / Verilator metasimulation of **Large BOOM v3**, one hart. Three runs
per workload per setup, all serial, with setup order rotated each repetition.

| Setup | Platform config | AutoTrace bridge/history/buffers | Collection |
| --- | --- | --- | --- |
| plain | BaseF1Config | Absent | None |
| idle | AutoTraceF1Config + `+autotrace-disable` | Present | Disabled |
| collect | AutoTraceF1Config | Present | Enabled |

Both simulator images use `FireSimLargeBoomConfig`, the same staged target FIRRTL
and annotations, the same measurement-counter implementation and the default
Verilator compilation settings. BaseF1 disables AutoTrace bridge generation;
the generated header and runtime are checked for absence of a bridge instance.
It still starts from the same annotated target source. This is the requested
platform-config comparison, not a separate source-stripped processor design.

The original, already compiled measured ELF is reused for each workload; its
SHA-256 must match the first seven-workload test and its build manifest. No BOLT
rewriting, software sampling or workload input changes are involved. `mt-matmul`
runs on one hart; qsort is the 2,048-element riscv-tests workload.

## Wall time and the three overhead comparisons

All times are medians of three **fresh runs**. They cover the whole simulator
process, including startup, log output, final drain and file close. Build and
validation time are excluded. Earlier timings are not reused in this table.

| Workload | Plain (s) | Idle (s) | Collect (s) | Idle vs plain | Collect vs idle | Collect vs plain |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
'''
    for row in rows:
        report += (f"| {row['workload']} | {row['plain_wall_seconds']:.3f} | {row['idle_wall_seconds']:.3f} | "
                   f"{row['collect_wall_seconds']:.3f} | {row['idle_over_plain_percent']:+.2f}% | "
                   f"{row['collect_over_idle_percent']:+.2f}% | {row['collect_over_plain_percent']:+.2f}% |\n")
    report += '''
Each overhead is `(mode wall / reference wall - 1) * 100%`. Idle vs plain measures
the metasimulation cost of keeping the generated instrumentation/history present.
Collect vs idle measures incremental collection cost. Collect vs plain measures
total active AutoTrace metasimulation cost relative to BaseF1. These percentages
compose multiplicatively, not by simply adding the two incremental percentages.

Negative idle/plain values are observations of these compiled metasimulators,
not evidence of a BOOM hardware speedup. Generated C++ code layout and host
timing variability can affect wall time even when target counters are identical.

## Observed wall-time ranges (seconds, three samples each)

| Workload | Plain min–max | Idle min–max | Collect min–max |
| --- | ---: | ---: | ---: |
''' + '\n'.join(ranges) + '''

## Whole-run simulation throughput (target cycles/second)

| Workload | Plain | Idle | Collect |
| --- | ---: | ---: | ---: |
''' + '\n'.join(throughput) + '''

Throughput divides each run's whole-program simulated cycles by that same run's
wall time, then takes the median. It does not divide ROI cycles by whole-run time.

## Workload-region target counters

| Workload | Setup | Cycles | Retired instructions | CPI | I-cache acquires | Acquire MPKI | Misprediction recoveries |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
''' + '\n'.join(targets) + '''

## Collection counters (whole run, collect mode)

| Workload | Records | Stream bytes | CSV bytes | Host cycles blocked | Peak capture batches |
| --- | ---: | ---: | ---: | ---: | ---: |
''' + '\n'.join(costs) + '''

Collection counters use medians except occupancy, which is the maximum across
repetitions. Idle exports no trace and has zero capture backpressure/occupancy.
Plain has no AutoTrace bridge: its trace output is absent, and AutoTrace
backpressure/occupancy registers do not exist (not measured zero). Every final
existing capture queue was empty. Sampling interrupts are zero; no software
sampler is linked into these binaries.

## Counter definitions and scope

The measured workload uses `setStats` snapshots, with `mcountinhibit` freezing
counter increments during reads. All setups retain the four same lightweight
PMU measurements: `mcycle`, `minstret`, `mhpmevent3=0x201` for BOOM branch
misprediction recoveries (`b2.mispredict`), and `mhpmevent4=0x102` for instruction
cache acquire/refill transactions. Boundary/call bookkeeping remains included.
CPI is cycles/instructions; acquire MPKI is acquires/instructions*1,000.

Committed/taken branch counts, a corresponding committed-branch misprediction
rate, all I-cache accesses and frontend stall cycles remain unavailable. Acquire
counts are not a measurement of every instruction fetch miss. The same metric
limitations apply to all three setups.

Region boundaries and inputs are unchanged from the first suite:
'''
    for name, workload in data['workloads'].items():
        report += f"\n* **{name}:** {workload['scope']}\n"
    report += '''
AutoTrace captures the whole workload, including setup/warmup, verification and
printing, using 11 observations and three 32-entry branch-resolution histories
on L1D acquire. Capture capacity is 16 pending cycle batches; stream queue depth
is 6,144 entries. History continues tracking while idle collection is disabled.

Backpressure counts **emulated host clock cycles** with an otherwise-ready token
opportunity blocked by a full capture queue:
`hPort.toHost.hValid && hPort.fromHost.hReady && initDone && capture &&
!captures.io.enq.ready`. It does not count physical host CPU cycles or wall time
spent formatting CSV. Occupancy excludes the active serializer batch and
downstream stream queue. Stream records use padded 64-byte beats; CSV bytes
include their text header.

## Validation, preservation and reproduction

All simulations must pass workload correctness checks. Each CSV row count and
byte size must match the driver metrics. Each fresh first collection trace is
checked against the independent branch/history validator; see
`history-validation.log`. The original test files and scripts were hashed before
the new experiment and verified unchanged afterward; see
`original-tests-sha256.json`. Individual ROI equality checks are in `summary.json`.

The runner was interrupted during its first Dhrystone collection attempt. That
incomplete attempt is preserved in a `.interrupted-1` directory and excluded from
the results; the two already completed measurements were retained. The resumed
runner completed the remaining measurements. Interruption time is outside all
per-process wall timers. See `resume_events` and `interrupted_attempts` in the JSON.

`summary.json` retains all commands, image/ELF hashes, per-run counters and timings,
definitions and validation. `comparison.csv` exports the wall-time table. The
`runs/<workload>/<repeat>-<setup>` directories retain all logs and collection CSVs.
The BaseF1 build output is separate from AutoTrace's existing generated directory;
the full build log is `basef1-build.log`.

CSV output uses normal buffered filesystem writes and close/flush; no forced
`fsync` is performed, so wall time does not require all data to reach durable
physical storage before exit.

From Chipyard, after building both images, run the scripts in a new experiment
directory with the same manifest references (the runner refuses an existing
`runs` directory):

```bash
source env.sh
python3 scripts/autotrace-experiment/basef1-comparison-2026-10-25/run-three-modes.py
python3 scripts/autotrace-experiment/basef1-comparison-2026-10-25/validate-report.py
```

This measures local Verilator runtime overhead. It does not measure FPGA area,
FPGA timing closure or FPGA simulation throughput. Separate generated designs
also differ in compiled code layout; three repetitions give observed variability,
not confidence intervals. No simulations were run concurrently or during build.
'''
    (HERE / 'RESULTS.md').write_text(report)
    print('\n'.join(report.splitlines()[:43]))


if __name__ == '__main__':
    main()
