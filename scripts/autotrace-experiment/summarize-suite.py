#!/usr/bin/env python3
"""Validate paired suite artifacts and write tables with explicit counter scopes."""
import argparse
import csv
import json
from pathlib import Path
import statistics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path)
    args = parser.parse_args()
    root = args.results.resolve()
    suite = json.loads((root / 'suite-summary.json').read_text())
    assert not suite['failures'], suite['failures']
    assert len(suite['workloads']) == 7, 'Incomplete suite'
    rows = []
    target_lines = []
    overhead_lines = []
    validation = {}
    image_hashes = set()
    for name, data in suite['workloads'].items():
        image_hashes.add(data['simulator_sha256'])
        assert len(data['runs']) == 2 * suite['repeats']
        for run in data['runs']:
            assert run['returncode'] == 0 and run['counters']['correctness'] == 'PASS'
            folder = root / name / f"{run['repeat']}-{run['mode']}"
            files = list(folder.glob('trace*.csv'))
            count = 0
            for path in files:
                with path.open(newline='') as stream:
                    count += sum(1 for _ in csv.DictReader(stream))
            assert count == sum(b['trace_records'] for b in run['bridges'])
            assert sum(p.stat().st_size for p in files) == sum(b['trace_csv_bytes'] for b in run['bridges'])
            assert all(b['capture_occupancy_final'] == 0 for b in run['bridges'])
            if run['mode'] == 'baseline':
                assert not files
                assert all(b['host_cycles_blocked'] == b['capture_max_occupancy'] == 0 for b in run['bridges'])
        identical = all(r['counters'] == data['runs'][0]['counters'] for r in data['runs'])
        validation[name] = dict(all_target_counters_identical=identical, csv_records_and_bytes_match=True,
                                correctness_passed=True, samples_per_mode=suite['repeats'])
        b, c = data['medians']['baseline'], data['medians']['collection']
        row = dict(workload=name, baseline_wall_seconds=b['wall_seconds'], collection_wall_seconds=c['wall_seconds'],
                   collection_slowdown=data['collection_slowdown'],
                   collection_wall_increase_percent=100 * (data['collection_slowdown'] - 1),
                   baseline_cycles_per_second=b['simulation_cycles_per_second'],
                   collection_cycles_per_second=c['simulation_cycles_per_second'],
                   roi_cycle_change_percent=100 * data['collection_roi_cycle_change_fraction'])
        rows.append(row)
        bc, cc = b['counters'], c['counters']
        target_lines.append(f"| {name} | {bc['target_cycles']:,} / {cc['target_cycles']:,} | "
                            f"{bc['retired_instructions']:,} / {cc['retired_instructions']:,} | "
                            f"{b['cpi']:.5f} / {c['cpi']:.5f} | "
                            f"{bc['icache_acquires']:,} / {cc['icache_acquires']:,} | "
                            f"{b['icache_acquire_mpki']:.5f} / {c['icache_acquire_mpki']:.5f} | "
                            f"{bc['branch_misprediction_recoveries']:,} / {cc['branch_misprediction_recoveries']:,} |")
        collections = [r for r in data['runs'] if r['mode'] == 'collection']
        med = lambda key: statistics.median(sum(v[key] for v in r['bridges']) for r in collections)
        max_occupancy = max(v['capture_max_occupancy'] for r in collections for v in r['bridges'])
        overhead_lines.append(f"| {name} | {med('trace_records'):,.0f} | {med('trace_stream_bytes'):,.0f} | "
                              f"{med('trace_csv_bytes'):,.0f} | {med('host_cycles_blocked'):,.0f} | {max_occupancy} / 16 |")
    assert len(image_hashes) == 1, 'Simulator changed between workloads'
    suite['validation'] = validation
    suite['comparison'] = rows
    (root / 'suite-summary.json').write_text(json.dumps(suite, indent=2) + '\n')
    with (root / 'comparison.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    report = '''# Seven-workload AutoTrace comparison, 2026-10-25

Local FireSim Golden Gate / Verilator metasimulation of **Large BOOM v3**, using
`FireSimLargeBoomConfig` and `AutoTraceF1Config`, one hart. Three repetitions per
mode, serial execution, alternating pair order. Every pair uses an identical ELF,
input, simulator image, region boundary and measurement configuration; only
AutoTrace collection enable differs. No BOLT optimization is involved.

The requested “my-matmul” is interpreted as the repository's `mt-matmul`.
It runs on one hart here; this is not a multicore scaling experiment. Suite `qsort`
is the riscv-tests 2,048-element benchmark, distinct from the earlier 99-element
standalone quicksort experiment.

## Host performance (medians)

Wall time is monotonic elapsed time around the simulator process, including
startup, normal final trace drain and file close; build time is excluded.
Throughput divides each run's whole-program simulated cycles by that wall time.

| Workload | Baseline wall (s) | Collection wall (s) | Wall increase | Baseline cycles/s | Collection cycles/s | ROI cycle change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
'''
    for row in rows:
        report += (f"| {row['workload']} | {row['baseline_wall_seconds']:.3f} | {row['collection_wall_seconds']:.3f} | "
                   f"{row['collection_wall_increase_percent']:+.2f}% | {row['baseline_cycles_per_second']:,.1f} | "
                   f"{row['collection_cycles_per_second']:,.1f} | {row['roi_cycle_change_percent']:+.3f}% |\n")
    report += '''
## Target performance (baseline / collection)

| Workload | ROI cycles | Retired instructions | CPI | I-cache acquires | Acquire MPKI | Misprediction recoveries |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
''' + '\n'.join(target_lines) + '''

## Collection cost (whole run)

| Workload | Records | Stream bytes | CSV bytes | Host cycles blocked | Peak capture occupancy |
| --- | ---: | ---: | ---: | ---: | ---: |
''' + '\n'.join(overhead_lines) + '''

The table shows median records/bytes/backpressure and the maximum observed queue
occupancy across repetitions. Baseline exported no trace and had zero capture
backpressure or occupancy. Every final capture queue was empty. No software
sampling interrupts are used in either mode.

## Regions and inputs

Scalar RV64GC builds use the upstream default sizes and `PREALLOCATE=1`.
Measurement code is staged in `../suite-build/sources`; upstream sources were not
edited. The build script adds common `setStats` snapshots and emits their totals
only after successful correctness verification. Dhrystone has explicit final-state
checks added. Counter boundary/call bookkeeping is included equally in both modes.

'''
    for name, data in suite['workloads'].items():
        report += f"* **{name}:** {data['scope']}\n"
    report += '''
AutoTrace is enabled for the whole run, including setup, warmup, verification and
metric printing. It uses all 11 existing observations, including three 32-entry
branch-resolution histories on L1D acquire, with every enabled event captured.
Capture depth is 16 target-cycle batches and stream queue depth is 6,144 entries.

## Counter interpretation and limitations

* Cycles and instructions are `mcycle`/`minstret` deltas; `mcountinhibit` freezes
  counters while snapshots are read. The same four PMU counters are used in both
  modes and every workload.
* I-cache acquires use `mhpmevent4=0x102`, BOOM's instruction-cache refill/acquire
  event. They are not all fetch accesses or necessarily every instruction miss.
  Acquire MPKI is acquires / retired instructions * 1,000.
* Misprediction recoveries use `mhpmevent3=0x201` (`b2.mispredict`). They are not
  mispredictions of committed conditional branches. Committed/taken branch counts,
  a corresponding branch error rate, I-cache accesses and frontend stall cycles
  remain unavailable; no values are inferred for those counters.
* Backpressure means **emulated host clock cycles** satisfying
  `hPort.toHost.hValid && hPort.fromHost.hReady && initDone && capture &&
  !captures.io.enq.ready`. It counts opportunities blocked by this capture queue,
  not host CPU time spent decoding CSV or every cause of simulator stalling.
* Queue occupancy counts pending target-cycle batches, excluding the active
  serializer batch and downstream stream queue. CSV bytes include the header;
  stream bytes include 64-byte beat padding.
* Baseline keeps AutoTrace hardware present with capture disabled. This measures
  collection cost, not instrumentation area/frequency cost or FPGA throughput.
  Three repetitions give observed variability, not confidence intervals.

## Artifacts and reproduction

`suite-summary.json` contains individual runs, hashes, flags, derived results and
validation. Each workload directory contains six simulator logs and collection
CSVs. `comparison.csv` exports the host-performance table. The build manifest is
`../suite-build/manifest.json` and includes commands and staged source hashes.

From Chipyard, using the existing matching simulator:

```bash
source env.sh
python3 scripts/autotrace-experiment/build-suite.py
python3 scripts/autotrace-experiment/run-suite.py /tmp/autotrace-seven-workloads
python3 scripts/autotrace-experiment/summarize-suite.py /tmp/autotrace-seven-workloads
```

Build and result directories must be new. If `suite-build` already exists, reuse
its binaries and skip the build step. The simulator build compatibility procedure
and overhead-register implementation are documented in `../RESULTS.md`.
'''
    (root / 'RESULTS.md').write_text(report)
    print(report.split('## Target performance')[0])


if __name__ == '__main__':
    main()
