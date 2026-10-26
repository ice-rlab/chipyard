#!/usr/bin/env python3
"""Serial paired AutoTrace experiments for the seven staged riscv-tests workloads."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
WORKLOADS = ('dhrystone', 'towers', 'mm', 'rsort', 'mt-matmul', 'memcpy', 'qsort')
REGIONS = {
    'dhrystone': 'existing setStats boundaries around 500 Dhrystone iterations and timer reads',
    'towers': 'existing setStats boundaries around towers_solve, 7 discs, after preallocation',
    'mm': 'all 8 existing mm kernel repetitions, including per-iteration timer reads, excluding printing/verification',
    'rsort': 'existing setStats boundaries around sort, 2048 elements, after preallocation',
    'mt-matmul': 'matmul and barrier on one hart, 16x16 dataset, excluding printing/verification',
    'memcpy': 'existing setStats boundaries around 16000-byte memcpy, after preallocation',
    'qsort': 'existing setStats boundaries around sort, 2048 elements, after preallocation',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--repeats', type=int, default=3)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    simulator = ROOT / 'sims/firesim/sim/generated-src/f1/f1-firesim-FireSim-FireSimLargeBoomConfig-AutoTraceF1Config/VFireSim'
    result = dict(target='FireSimLargeBoomConfig', platform='AutoTraceF1Config',
                  execution='local Verilator metasimulation, one hart', repeats=args.repeats,
                  build_manifest=str(HERE / 'suite-build/manifest.json'), workloads={}, failures=[])
    for name in WORKLOADS:
        print(f'=== {name} ===', flush=True)
        command = [sys.executable, str(HERE / 'run-pair.py'), str(simulator),
                   str(HERE / f'suite-build/{name}.riscv'), str(args.output / name),
                   '--repeats', str(args.repeats), '--workload', name, '--roi', REGIONS[name],
                   '--max-cycles', '5000000', '--timeout', '1800']
        completed = subprocess.run(command)
        if completed.returncode:
            result['failures'].append(name)
        else:
            result['workloads'][name] = json.loads((args.output / name / 'summary.json').read_text())
        (args.output / 'suite-summary.json').write_text(json.dumps(result, indent=2) + '\n')
    if result['failures']:
        raise SystemExit('Failed workloads: ' + ', '.join(result['failures']))


if __name__ == '__main__':
    main()
