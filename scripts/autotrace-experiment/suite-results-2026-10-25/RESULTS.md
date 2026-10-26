# Seven-workload AutoTrace comparison, 2026-10-25

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
| dhrystone | 24.911 | 28.018 | +12.47% | 4,665.6 | 4,148.3 | +0.000% |
| towers | 6.676 | 7.128 | +6.78% | 4,487.0 | 4,202.1 | +0.000% |
| mm | 56.160 | 58.829 | +4.75% | 4,850.2 | 4,630.1 | +0.000% |
| rsort | 46.452 | 49.410 | +6.37% | 4,712.1 | 4,429.9 | +0.000% |
| mt-matmul | 7.729 | 8.630 | +11.66% | 4,670.6 | 4,182.9 | +0.000% |
| memcpy | 10.032 | 11.936 | +18.97% | 4,516.8 | 3,796.5 | +0.000% |
| qsort | 55.965 | 62.227 | +11.19% | 4,931.1 | 4,434.9 | +0.000% |

## Target performance (baseline / collection)

| Workload | ROI cycles | Retired instructions | CPI | I-cache acquires | Acquire MPKI | Misprediction recoveries |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| dhrystone | 80,605 / 80,605 | 186,020 / 186,020 | 0.43331 / 0.43331 | 17 / 17 | 0.09139 / 0.09139 | 19 / 19 |
| towers | 3,365 / 3,365 | 3,773 / 3,773 | 0.89186 / 0.89186 | 0 / 0 | 0.00000 / 0.00000 | 18 / 18 |
| mm | 129,298 / 129,298 | 197,992 / 197,992 | 0.65305 / 0.65305 | 26 / 26 | 0.13132 / 0.13132 | 30 / 30 |
| rsort | 86,401 / 86,401 | 171,144 / 171,144 | 0.50484 / 0.50484 | 0 / 0 | 0.00000 / 0.00000 | 17 / 17 |
| mt-matmul | 12,429 / 12,429 | 27,951 / 27,951 | 0.44467 / 0.44467 | 6 / 6 | 0.21466 / 0.21466 | 14 / 14 |
| memcpy | 10,547 / 10,547 | 5,515 / 5,515 | 1.91242 / 1.91242 | 1 / 1 | 0.18132 / 0.18132 | 1 / 1 |
| qsort | 201,033 / 201,033 | 123,496 / 123,496 | 1.62785 / 1.62785 | 0 / 0 | 0.00000 / 0.00000 | 10,887 / 10,887 |

## Collection cost (whole run)

| Workload | Records | Stream bytes | CSV bytes | Host cycles blocked | Peak capture occupancy |
| --- | ---: | ---: | ---: | ---: | ---: |
| dhrystone | 90,762 | 5,854,080 | 36,016,439 | 29,218 | 16 / 16 |
| towers | 17,123 | 1,123,520 | 6,819,568 | 7,854 | 16 / 16 |
| mm | 53,069 | 3,665,984 | 21,556,748 | 19,343 | 16 / 16 |
| rsort | 52,932 | 3,964,416 | 22,041,565 | 15,306 | 16 / 16 |
| mt-matmul | 19,770 | 1,339,776 | 7,938,976 | 8,051 | 16 / 16 |
| memcpy | 31,554 | 2,668,416 | 13,638,933 | 15,073 | 16 / 16 |
| qsort | 154,942 | 10,211,200 | 61,718,005 | 42,626 | 16 / 16 |

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

* **dhrystone:** ROI: existing setStats boundaries around 500 Dhrystone iterations and timer reads; wall: simulator process including final drain
* **towers:** ROI: existing setStats boundaries around towers_solve, 7 discs, after preallocation; wall: simulator process including final drain
* **mm:** ROI: all 8 existing mm kernel repetitions, including per-iteration timer reads, excluding printing/verification; wall: simulator process including final drain
* **rsort:** ROI: existing setStats boundaries around sort, 2048 elements, after preallocation; wall: simulator process including final drain
* **mt-matmul:** ROI: matmul and barrier on one hart, 16x16 dataset, excluding printing/verification; wall: simulator process including final drain
* **memcpy:** ROI: existing setStats boundaries around 16000-byte memcpy, after preallocation; wall: simulator process including final drain
* **qsort:** ROI: existing setStats boundaries around sort, 2048 elements, after preallocation; wall: simulator process including final drain

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

## Validation

All **42 simulator executions passed** their correctness checks. Every workload's
ROI cycles, instructions, cache acquires and misprediction recoveries were
identical across all six runs. Each CSV's row count and byte size match the driver
metrics, every final capture queue drained, and baseline produced no trace or
capture backpressure. The independent branch/history checker passed the first
collection trace for every workload; details are in `history-validation.log`.

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
