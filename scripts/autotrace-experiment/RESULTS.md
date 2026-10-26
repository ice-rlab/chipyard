# Quicksort AutoTrace overhead, 2026-10-24

Collection took **7.76% longer** than baseline, with **no change in target ROI
cycles or the measured architectural counters**. Simulation throughput fell
7.20%. These are Verilator metasimulation results on this host, not FPGA results.

Same measured ELF, BOOM configuration, simulator executable, input generation,
counter selectors and ROI boundaries in both modes. The only command difference
was `+autotrace-disable` versus `+autotrace-filename-base=...`. Three runs per mode
were executed serially, alternating pair order; values below use medians.

| Measurement | Baseline, collection disabled | Collection enabled |
| --- | ---: | ---: |
| Sort-region target cycles | 8,149 | 8,149 |
| Sort-region retired instructions | 6,059 | 6,059 |
| CPI | 1.34494 | 1.34494 |
| I-cache acquire/refill transactions in sort region | 7 | 7 |
| I-cache acquire MPKI | 1.15531 | 1.15531 |
| Branch misprediction recoveries in sort region | 399 | 399 |
| Whole-run simulated target cycles | 28,546 | 28,546 |
| Whole-process wall time, seconds | 6.42637 | 6.92513 |
| Throughput, target cycles/second | 4,442.01 | 4,122.09 |
| Exported trace records | 0 | 14,082 |
| Exported stream bytes | 0 | 943,488 |
| Exported CSV bytes, including header | 0 | 5,632,025 |
| Host cycles blocked by capture queue | 0 | 4,415 |
| Capture queue maximum occupancy, pending cycle batches | 0 | 16 / 16 |
| Capture queue occupancy after final drain | 0 | 0 |
| Software sampling interrupts | 0 | 0 |

All six runs passed sorting and checksum checks and reported exactly the same
ROI counters and whole-run simulated cycles. Wall times ranged from 6.374 to
6.504 seconds for baseline and 6.877 to 6.974 seconds for collection.
Three repetitions describe this experiment; they are not statistical confidence
bounds. Every collection run reported identical trace and bridge counters.

## Interpretation and scope

The ROI surrounds `quickSort(arr, 0, 98)` using `mcountinhibit` to freeze the
measurement counters before/after the call. Boundary and call instructions are
included equally in both modes. Input generation, correctness verification and
counter printing are outside the ROI. The original repository algorithm and
99-element `rand() & 1023` input generation are reused in
[quicksort-measured.c](quicksort-measured.c). This is a newly compiled measured
ELF: the exact ELF behind the historical `quicksort.out` was not available.
Its whole-run cycle count should not be compared with that historical log.

AutoTrace capture covers the whole workload, including startup, RNG, verification
and metric printing. It retains the existing 11 observations, including three
32-entry histories triggered by L1D acquire, and records every enabled event.
There is no software sampler. Capture depth is 16 batches, stream queue depth is
6,144 entries. Wall time includes startup and final drain/file close and excludes
all simulator/workload build time. Throughput uses whole-run cycles, not ROI cycles.

Backpressure counts **emulated host clock cycles**, once for each cycle with a
ready token opportunity blocked specifically by the capture queue. The condition
is `hPort.toHost.hValid && hPort.fromHost.hReady && initDone && capture &&
!captures.io.enq.ready`. It is not host CPU cycles or wall time spent formatting
CSV. The occupancy measurement excludes the serializer's active batch and the
downstream stream queue. These overhead counters have whole-run scope.

The I-cache event is BOOM's acquire/refill transaction event, not all cache
accesses or necessarily every instruction fetch miss. The misprediction event
is `b2.mispredict`, a recovery event, not a count of mispredicted retired
conditional branches. Therefore **committed/taken branch counts, a corresponding
branch misprediction rate, I-cache accesses, and front-end stall cycles are
unavailable** from this measurement setup. No values were inferred for them.
No BOLT-optimized binary was run, so optimized cycle reduction is unavailable.
Collection versus baseline ROI cycle difference is zero.

The baseline keeps AutoTrace hardware present with capture disabled. This
experiment measures active collection cost; it does not measure the area,
frequency or idle cost of adding AutoTrace hardware to BOOM.

## Validation and artifacts

[summary.json](results-2026-10-24/summary.json) contains all six commands, hashes,
raw measurements, definitions and derived values. Per-run logs/CSVs are in its
sibling `0-baseline`, `0-collection`, etc. directories. CSV record counts and file
sizes match the driver metrics for every run. The existing history checker passes
on the first fresh collection trace: 14,082 rows, 55 L1D acquires, 165 snapshots,
102 full windows, 3,436 valid history samples, with every field and unused slot
matching the independent branch trace.

## Reproduction

From Chipyard, with the matching simulator already built:

```bash
source env.sh
python3 scripts/autotrace-experiment/run-pair.py \
  sims/firesim/sim/generated-src/f1/f1-firesim-FireSim-FireSimLargeBoomConfig-AutoTraceF1Config/VFireSim \
  scripts/autotrace-experiment/quicksort-measured.riscv \
  /tmp/quicksort-autotrace-repeat
```

The output directory must not exist. Build the measured ELF with the toolchain's
`riscv64-unknown-elf-gcc -O2 -march=rv64gc -mabi=lp64d -specs=htif_nano.specs`,
defining `QUICKSORT_SOURCE` as a quoted absolute path to the parent repository's
`benchmarking/microbenchmarks/benchmarks/quicksort.c`.

The standard simulator build hit a CIRCT `hw.hierpath` export incompatibility.
For this experiment, the original emitted FIRRTL was parsed/exported without
loading external annotations into CIRCT; the original annotation JSON was then
passed directly to Golden Gate. This changes the interchange format while
retaining the annotations. The source-only and hand-normalized legacy routes
were also tried and failed; neither produced the executable used here.

The successful build uses these local changes and options:

```bash
source env.sh
export RISCV="$PWD/.conda-env/riscv-tools"
staging="$PWD/sims/firesim-staging/generated-src/firechip.chip.FireSim.FireSimLargeBoomConfig"
firtool "$staging/firechip.chip.FireSim.FireSimLargeBoomConfig.sfc.fir" \
  --parse-only --disable-annotation-unknown -o /tmp/autotrace-unannotated.mlir
circt-translate /tmp/autotrace-unannotated.mlir --export-firrtl \
  -o "$staging/firechip.chip.FireSim.FireSimLargeBoomConfig.compat.fir"
make -C sims/firesim/sim TARGET_PROJECT=firesim \
  TARGET_PROJECT_MAKEFRAG=../../../generators/firechip/chip/src/main/makefrag/firesim \
  TARGET_CONFIG=FireSimLargeBoomConfig PLATFORM_CONFIG=AutoTraceF1Config \
  USE_CIRCT_PREPROCESSING=0 \
  goldengate_input_firrtl="$staging/firechip.chip.FireSim.FireSimLargeBoomConfig.compat.fir" \
  verilator
```

The original staged FIRRTL/annotation files must first exist (from the standard
build's target elaboration). `USE_CIRCT_PREPROCESSING` defaults to 1, preserving
the existing default build flow. This experiment only exercises the explicit
compatibility route above.
