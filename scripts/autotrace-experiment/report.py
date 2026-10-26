#!/usr/bin/env python3
"""Validate three comparable simulator measurements and compute derived metrics."""

import argparse
import json
import math
import sys


MODES = ("collection", "baseline", "optimized")
COUNTERS = (
    "target_cycles", "retired_instructions", "committed_branches",
    "taken_branches", "branch_mispredictions", "icache_accesses",
    "icache_misses", "frontend_stall_cycles",
)
PRIORITY = ("target_cycles", "retired_instructions", "branch_mispredictions", "icache_misses")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(value, label, positive=False, integer=False):
    require(isinstance(value, (int, float)) and not isinstance(value, bool),
            f"{label}: expected a number")
    require(math.isfinite(value) and (value > 0 if positive else value >= 0),
            f"{label}: expected a finite {'positive' if positive else 'nonnegative'} value")
    require(not integer or isinstance(value, int), f"{label}: expected an integer")


def ratio(numerator, denominator, scale=1):
    return numerator / denominator * scale if denominator else None


def summarize(data):
    runs = data["runs"]
    require(set(runs) == set(MODES), "exactly collection, baseline and optimized runs required")
    definitions = data["counter_definitions"]
    require(set(definitions) == set(COUNTERS), "counter_definitions must describe every counter")
    for key, definition in definitions.items():
        require(definition is None or (isinstance(definition, str) and definition.strip()),
                f"{key}: supply a counter definition or null for unavailable")
    for key in PRIORITY:
        require(definitions[key] is not None, f"priority counter unavailable: {key}")
    available = {key for key, definition in definitions.items() if definition is not None}
    baseline = runs["baseline"]
    for mode, run in runs.items():
        for key in ("hardware_id", "measurement_config_id", "input_sha256", "region", "timing_scope"):
            require(run[key] and run[key] == baseline[key], f"{mode}: mismatched {key}")
        require(run["profiling_enabled"] is (mode == "collection"), f"{mode}: wrong profiling mode")
        require(run["correctness_passed"] is True, f"{mode}: workload correctness not confirmed")
        require(isinstance(run["binary_sha256"], str) and len(run["binary_sha256"]) == 64,
                f"{mode}: binary SHA-256 required")
        require(set(run["counters"]) == available, f"{mode}: counter set differs from definitions")
        for key, value in run["counters"].items():
            number(value, f"{mode}.{key}", positive=key in PRIORITY[:2], integer=True)
        number(run["wall_seconds"], f"{mode}.wall_seconds", positive=True)
        number(run["simulated_cycles"], f"{mode}.simulated_cycles", positive=True, integer=True)
        # Whole-process wall time must use whole-process simulated cycles, not ROI cycles.
        require(run["timing_scope"] == "simulator_process_including_final_drain",
                "wall time must include final trace drain and exclude build/BOLT conversion")
    require(runs["collection"]["binary_sha256"] == baseline["binary_sha256"],
            "collection and baseline must use the identical original ELF")
    require(runs["optimized"]["binary_sha256"] != baseline["binary_sha256"],
            "optimized run must use the rewritten ELF")
    collection = runs["collection"]
    require(isinstance(collection["sampling_settings"], dict) and collection["sampling_settings"],
            "record collection sampling/gating settings")
    overhead = collection["overhead"]
    for key in ("trace_records", "trace_csv_bytes", "trace_stream_bytes", "backpressure"):
        number(overhead[key], key, integer=True)
    require(overhead["backpressure_kind"] in ("host_cycles_blocked", "blocked_token_acceptance_attempts"),
            "backpressure_kind must explicitly identify host cycles or acceptance attempts")
    require(isinstance(overhead["backpressure_definition"], str) and overhead["backpressure_definition"].strip(),
            "backpressure signal and gating definition required")
    for key in ("buffer_occupancy", "buffer_max_occupancy", "sampling_interrupts"):
        require(key in overhead, f"{key}: supply measurement or null if unavailable")
        if overhead[key] is not None:
            number(overhead[key], key, integer=True)
    if overhead["buffer_occupancy"] is not None and overhead["buffer_max_occupancy"] is not None:
        require(overhead["buffer_occupancy"] <= overhead["buffer_max_occupancy"],
                "occupancy exceeds maximum occupancy")
    results = {}
    denominator = data["misprediction_denominator"]
    require(denominator in ("committed_branches", "taken_branches", None),
            "choose a corresponding branch denominator or null when unavailable")
    require(denominator is None or denominator in available, "branch denominator unavailable")
    for mode, run in runs.items():
        c = run["counters"]
        results[mode] = {
            "cpi": ratio(c["target_cycles"], c["retired_instructions"]),
            "icache_mpki": ratio(c["icache_misses"], c["retired_instructions"], 1000),
            "branch_misprediction_rate": ratio(c["branch_mispredictions"], c[denominator]) if denominator else None,
            "simulation_cycles_per_second": run["simulated_cycles"] / run["wall_seconds"],
            "wall_seconds": run["wall_seconds"],
            "counters": c,
        }
    results["cycle_reduction"] = 1 - runs["optimized"]["counters"]["target_cycles"] / baseline["counters"]["target_cycles"]
    results["collection_slowdown"] = collection["wall_seconds"] / baseline["wall_seconds"]
    results["collection_throughput_ratio"] = results["collection"]["simulation_cycles_per_second"] / results["baseline"]["simulation_cycles_per_second"]
    results["collection_overhead"] = overhead
    results["unavailable_counters"] = sorted(set(COUNTERS) - available)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("measurements", help="JSON with counter definitions and three runs")
    args = parser.parse_args()
    try:
        with open(args.measurements) as stream:
            result = summarize(json.load(stream))
        print(json.dumps(result, indent=2, allow_nan=False))
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Invalid experiment: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
