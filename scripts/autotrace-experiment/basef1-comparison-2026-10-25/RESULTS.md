# BaseF1 / AutoTrace idle / AutoTrace collection comparison

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
| dhrystone | 25.817 | 25.313 | 28.522 | -1.95% | +12.68% | +10.48% |
| towers | 6.577 | 6.578 | 7.228 | +0.01% | +9.88% | +9.89% |
| mm | 59.580 | 57.329 | 60.430 | -3.78% | +5.41% | +1.43% |
| rsort | 48.108 | 47.561 | 50.008 | -1.14% | +5.15% | +3.95% |
| mt-matmul | 8.231 | 8.128 | 8.932 | -1.24% | +9.89% | +8.52% |
| memcpy | 10.435 | 10.333 | 12.139 | -0.98% | +17.48% | +16.33% |
| qsort | 59.284 | 58.280 | 63.697 | -1.69% | +9.29% | +7.44% |

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
| dhrystone | 25.563–25.916 | 25.062–25.866 | 28.369–28.669 |
| towers | 6.526–6.677 | 6.426–6.626 | 7.178–7.329 |
| mm | 58.528–60.532 | 56.669–58.229 | 59.632–60.882 |
| rsort | 47.296–48.251 | 47.250–48.457 | 49.107–50.413 |
| mt-matmul | 8.179–8.380 | 8.031–8.330 | 8.882–9.032 |
| memcpy | 10.384–10.584 | 10.283–10.734 | 12.039–12.389 |
| qsort | 58.578–59.583 | 57.529–58.329 | 63.385–63.840 |

## Whole-run simulation throughput (target cycles/second)

| Workload | Plain | Idle | Collect |
| --- | ---: | ---: | ---: |
| dhrystone | 4,501.9 | 4,591.5 | 4,075.0 |
| towers | 4,554.0 | 4,553.7 | 4,144.2 |
| mm | 4,571.8 | 4,751.3 | 4,507.5 |
| rsort | 4,549.8 | 4,602.2 | 4,377.0 |
| mt-matmul | 4,385.8 | 4,440.9 | 4,041.3 |
| memcpy | 4,342.4 | 4,385.3 | 3,732.9 |
| qsort | 4,655.0 | 4,735.2 | 4,332.6 |

Throughput divides each run's whole-program simulated cycles by that same run's
wall time, then takes the median. It does not divide ROI cycles by whole-run time.

## Workload-region target counters

| Workload | Setup | Cycles | Retired instructions | CPI | I-cache acquires | Acquire MPKI | Misprediction recoveries |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| dhrystone | plain | 80,605 | 186,020 | 0.43331 | 17 | 0.09139 | 19 |
| dhrystone | idle | 80,605 | 186,020 | 0.43331 | 17 | 0.09139 | 19 |
| dhrystone | collect | 80,605 | 186,020 | 0.43331 | 17 | 0.09139 | 19 |
| towers | plain | 3,365 | 3,773 | 0.89186 | 0 | 0.00000 | 18 |
| towers | idle | 3,365 | 3,773 | 0.89186 | 0 | 0.00000 | 18 |
| towers | collect | 3,365 | 3,773 | 0.89186 | 0 | 0.00000 | 18 |
| mm | plain | 129,298 | 197,992 | 0.65305 | 26 | 0.13132 | 30 |
| mm | idle | 129,298 | 197,992 | 0.65305 | 26 | 0.13132 | 30 |
| mm | collect | 129,298 | 197,992 | 0.65305 | 26 | 0.13132 | 30 |
| rsort | plain | 86,401 | 171,144 | 0.50484 | 0 | 0.00000 | 17 |
| rsort | idle | 86,401 | 171,144 | 0.50484 | 0 | 0.00000 | 17 |
| rsort | collect | 86,401 | 171,144 | 0.50484 | 0 | 0.00000 | 17 |
| mt-matmul | plain | 12,429 | 27,951 | 0.44467 | 6 | 0.21466 | 14 |
| mt-matmul | idle | 12,429 | 27,951 | 0.44467 | 6 | 0.21466 | 14 |
| mt-matmul | collect | 12,429 | 27,951 | 0.44467 | 6 | 0.21466 | 14 |
| memcpy | plain | 10,547 | 5,515 | 1.91242 | 1 | 0.18132 | 1 |
| memcpy | idle | 10,547 | 5,515 | 1.91242 | 1 | 0.18132 | 1 |
| memcpy | collect | 10,547 | 5,515 | 1.91242 | 1 | 0.18132 | 1 |
| qsort | plain | 201,033 | 123,496 | 1.62785 | 0 | 0.00000 | 10,887 |
| qsort | idle | 201,033 | 123,496 | 1.62785 | 0 | 0.00000 | 10,887 |
| qsort | collect | 201,033 | 123,496 | 1.62785 | 0 | 0.00000 | 10,887 |

## Collection counters (whole run, collect mode)

| Workload | Records | Stream bytes | CSV bytes | Host cycles blocked | Peak capture batches |
| --- | ---: | ---: | ---: | ---: | ---: |
| dhrystone | 90,762 | 5,854,080 | 36,016,439 | 29,218 | 16 / 16 |
| towers | 17,123 | 1,123,520 | 6,819,568 | 7,854 | 16 / 16 |
| mm | 53,069 | 3,665,984 | 21,556,748 | 19,343 | 16 / 16 |
| rsort | 52,932 | 3,964,416 | 22,041,565 | 15,306 | 16 / 16 |
| mt-matmul | 19,770 | 1,339,776 | 7,938,976 | 8,051 | 16 / 16 |
| memcpy | 31,554 | 2,668,416 | 13,638,933 | 15,073 | 16 / 16 |
| qsort | 154,942 | 10,211,200 | 61,718,005 | 42,626 | 16 / 16 |

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

* **dhrystone:** ROI: existing setStats boundaries around 500 Dhrystone iterations and timer reads; wall: simulator process including final drain

* **towers:** ROI: existing setStats boundaries around towers_solve, 7 discs, after preallocation; wall: simulator process including final drain

* **mm:** ROI: all 8 existing mm kernel repetitions, including per-iteration timer reads, excluding printing/verification; wall: simulator process including final drain

* **rsort:** ROI: existing setStats boundaries around sort, 2048 elements, after preallocation; wall: simulator process including final drain

* **mt-matmul:** ROI: matmul and barrier on one hart, 16x16 dataset, excluding printing/verification; wall: simulator process including final drain

* **memcpy:** ROI: existing setStats boundaries around 16000-byte memcpy, after preallocation; wall: simulator process including final drain

* **qsort:** ROI: existing setStats boundaries around sort, 2048 elements, after preallocation; wall: simulator process including final drain

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
