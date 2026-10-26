# AutoTrace quicksort comparison

Seven-workload follow-up (2026-10-25): **42 simulations passed** across Dhrystone,
Towers, mm, rsort, mt-matmul, memcpy and riscv-tests qsort. See the
[full suite comparison](suite-results-2026-10-25/RESULTS.md) and
[machine-readable results](suite-results-2026-10-25/suite-summary.json).
Collection increased median wall time by 4.75–18.97%; all measured ROI counters
were identical between modes and repetitions.

Status (2026-10-24): **baseline/collection pair completed**, three runs per mode.
See [RESULTS.md](RESULTS.md) and [raw measurements](results-2026-10-24/summary.json).
Median collection slowdown is 1.0776x; ROI cycles and all four measured target
counters match exactly. This is local Verilator metasimulation, not FPGA timing.
No optimized/BOLT run has been performed.

Chipyard and relevant submodules are aligned with AutoTrace: FireSim
`810c277b1e934d9869ff0139f13e049c02388173` and BOOM
`971dc49aea986913f39012b2533006c3879d54d6`. The local TraceDoctor import edit
was reapplied and retained in the named FireSim stash
`codex-preserve-tracedoctor-before-autotrace`. Other existing edits were preserved.
The bridge/driver now have local overhead measurement changes. The configured
FPGA database and documented BOLT executable remain absent; neither was needed
for this baseline/collection metasimulation experiment.

## Experiment procedure

1. Prepare a matching AutoTrace BOOM simulator/FPGA image and driver, with
   lightweight measurement counters independent of `trace_enable`. Keep this
   exact simulator/image for all three runs. Record source revisions, image hash,
   target configuration, cache configuration, counter selectors and definitions.
2. Build `qsort_riscv_tests-bare` once, without the CTR software sampler. Use its
   existing start/end trigger instructions around `sort(DATA_SIZE, input_data)`
   as the ROI for every measurement counter. Save the ELF and dataset SHA-256.
   Snapshot counters at the triggers; print results after leaving the ROI.
3. Baseline: original ELF, `+autotrace-disable`, measurement counters enabled.
4. Collection: the identical original ELF, AutoTrace enabled. Record branch
   sampling enables/period, history depth, capture queue and stream queue depths,
   trigger settings and included labels. AutoTrace enables are supplied by the
   hardware; it does not generate sampling intervals itself. Software sampling
   interrupts are zero only when the configuration confirms none are used.
5. Convert architectural control-flow records into BOLT input matching that
   exact ELF, then rewrite with BOLT and apply the bare-metal boot trampoline.
   A working AutoTrace-to-BOLT converter is a separate prerequisite: the existing
   `perf/ctr/ctr2bolt.py` consumes CTR uartlog records, not AutoTrace CSV. Do not
   substitute speculative branch resolutions for committed control flow. Verify
   the trace includes the branch targets/fallthrough information the converter
   needs, including indirect control transfers.
6. Optimized: rewritten ELF, `+autotrace-disable`, identical inputs, image,
   measurement counters and semantic trigger boundaries. Confirm sorted output
   for all runs. Repeat runs under comparable host load; retain individual runs
   and report variability. Never include compilation or conversion in run time.

Measure monotonic wall time around the simulator process, including normal final
trace drain and file close. Count simulated cycles over that same process scope
for throughput; use ROI cycles only for target performance. On metasimulation,
this reports metasimulation throughput, not FPGA throughput.

## Counter semantics to implement

All three runs need ROI cycles, retired architectural instructions, committed
conditional branches, corresponding taken branches, branch mispredictions, L1I
accesses and misses, and front-end stall cycles when available. Use integer event
counts (PopCount for multiple commit lanes), not cycles with any event. Document
whether a cache event means a request, refill, or a blocked cycle. Report
unavailable counters explicitly. The existing PMU headers are not proof that the
selected bitstream implements an event.

Choose the misprediction denominator only after matching event definitions. A
resolution-stage misprediction pulse can include speculative work; it cannot
automatically be divided by retired conditional branches. Count committed
mis­predicted conditional branches for that denominator, or report the raw count
and mark the rate unavailable. A CTR record's currently constant mispredict bit
is not a measurement of branch mispredictions.

For the first experiment prioritize cycles, instructions, L1I misses,
mispredictions, throughput, trace bytes and backpressure. The local bridge/driver changes implement these host-domain
counters (they do not depend on targetFire to advance):

* `host_cycles_blocked`: increment each host clock with
  `hPort.toHost.hValid && hPort.fromHost.hReady && initDone && capture &&
  !captures.io.enq.ready`. This counts ready token-acceptance opportunities
  blocked by this capture queue, once per host clock. It is not total time stalled
  anywhere in the simulator and is not additive across bridges.
* Queue occupancy: expose `captures.io.count` and a high-water register updated
  every host clock. Label units **pending target-cycle batches**; the serializer's
  active batch and downstream stream queue are separate storage. Record a final
  snapshot and high-water value, with reset/scope documented.
* Trace records: count completed records in the driver; stream bytes: sum actual
  nonzero `pull` return values (including final drain). CSV bytes: file sizes
  after close. These byte counts differ because stream records are padded to
  64-byte beats, while CSV uses text and includes headers.

Use run-wide scope for collection overhead, including final drain; ROI scope for
target counters. Record both scopes explicitly in the measurement metadata.
Live multiword MMIO counters need a coherent snapshot before host reads.

## Comparison checker

`python3 scripts/autotrace-experiment/report.py measurements.json` validates the
three runs and prints JSON metrics. It does not run simulations or extract
missing counters. The input has:

* `counter_definitions`: a mapping of `target_cycles`, `retired_instructions`,
  `committed_branches`, `taken_branches`, `branch_mispredictions`, `icache_accesses`,
  `icache_misses`, `frontend_stall_cycles` to precise source/selector/scope strings,
  or null when unavailable. The four priority target counters must be available.
* `misprediction_denominator`: `committed_branches`, `taken_branches`, or null.
* `runs`: `collection`, `baseline`, `optimized`. Each contains `hardware_id`,
  `measurement_config_id`, `input_sha256`, `region`, `timing_scope` (the literal
  `simulator_process_including_final_drain`), `binary_sha256`,
  `profiling_enabled`, `correctness_passed`, `wall_seconds`, `simulated_cycles`,
  and `counters` (exactly the available counter names and their ROI deltas).
  IDs should identify immutable configurations/artifacts, not mutable paths.
* The collection run additionally contains nonempty `sampling_settings` and
  `overhead`, with `trace_records`, `trace_csv_bytes`, `trace_stream_bytes`,
  `backpressure`, `backpressure_kind` (`host_cycles_blocked` or
  `blocked_token_acceptance_attempts`), `backpressure_definition`,
  `buffer_occupancy`, `buffer_max_occupancy`, `sampling_interrupts`. The last three
  can be null if unavailable. Include counter scopes in their definitions.

The checker refuses mismatched configurations, original binaries, counter sets,
profiling modes, failed correctness checks and missing priority measurements.
Unavailable rates are null, not zero. Cycle reduction is a fraction; positive
means optimized is faster. Collection slowdown is collection/baseline wall time;
the throughput ratio accounts for differing whole-run simulated cycle counts.
