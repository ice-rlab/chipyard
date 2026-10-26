#!/usr/bin/env python3
"""Stage and instrument seven scalar riscv-tests workloads without editing upstream."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SOURCE = ROOT / 'toolchains/riscv-tools/riscv-tests/benchmarks'
WORKLOADS = ('dhrystone', 'towers', 'mm', 'rsort', 'mt-matmul', 'memcpy', 'qsort')

MEASUREMENT = r'''
static unsigned long measurement_begin[4], measurement_total[4];
static unsigned long measurement_regions;
static int measurement_active;
void setStats(int enable)
{
  write_csr(mcountinhibit, -1);
  if (enable) {
    if (measurement_active) tohost_exit(1338);
    write_csr(mhpmevent3, 0x201);
    write_csr(mhpmevent4, 0x102);
    measurement_begin[0] = read_csr(mcycle);
    measurement_begin[1] = read_csr(minstret);
    measurement_begin[2] = read_csr(mhpmcounter3);
    measurement_begin[3] = read_csr(mhpmcounter4);
    measurement_active = 1;
  } else {
    if (!measurement_active) tohost_exit(1338);
    measurement_total[0] += read_csr(mcycle) - measurement_begin[0];
    measurement_total[1] += read_csr(minstret) - measurement_begin[1];
    measurement_total[2] += read_csr(mhpmcounter3) - measurement_begin[2];
    measurement_total[3] += read_csr(mhpmcounter4) - measurement_begin[3];
    measurement_active = 0;
    ++measurement_regions;
  }
  write_csr(mcountinhibit, 0);
}
'''


def replace_once(path, old, new):
    text = path.read_text()
    assert text.count(old) == 1, (path, old)
    path.write_text(text.replace(old, new))


def main():
    build = HERE / 'suite-build'
    build.mkdir(exist_ok=False)
    sources = build / 'sources'
    sources.mkdir()
    for name in ('common', *WORKLOADS):
        shutil.copytree(SOURCE / name, sources / name)
    common = sources / 'common'
    syscalls = common / 'syscalls.c'
    text = syscalls.read_text()
    start = text.index('void setStats(int enable)')
    end = text.index('void __attribute__((noreturn)) tohost_exit', start)
    text = text[:start] + 'void __attribute__((noreturn)) tohost_exit(uintptr_t code);\n' + MEASUREMENT + '\n' + text[end:]
    old = '  tohost = (code << 1) | 1;'
    new = '''  if (code == 0 && (measurement_active || !measurement_regions)) code = 1338;
  if (code == 0) {
    printf("WORKLOAD_METRICS target_cycles=%lu retired_instructions=%lu "
           "branch_misprediction_recoveries=%lu icache_acquires=%lu correctness=PASS\\n",
           measurement_total[0], measurement_total[1], measurement_total[2], measurement_total[3]);
  }
  tohost = (code << 1) | 1;'''
    assert text.count(old) == 1
    syscalls.write_text(text.replace(old, new))

    mm = sources / 'mm/mm_main.c'
    replace_once(mm, '  for (int i = 0; i < R; i++)', '  setStats(1);\n  for (int i = 0; i < R; i++)')
    replace_once(mm, '  asm volatile("fence");', '  setStats(0);\n  asm volatile("fence");')
    mt = sources / 'mt-matmul/mt-matmul.c'
    replace_once(mt, '   stats(matmul(cid, nc, DIM_SIZE, input1_data, input2_data, results_data); barrier(nc), DIM_SIZE/DIM_SIZE/DIM_SIZE);',
                 '   setStats(1);\n   matmul(cid, nc, DIM_SIZE, input1_data, input2_data, results_data);\n   barrier(nc);\n   setStats(0);')
    dh = sources / 'dhrystone/dhrystone_main.c'
    replace_once(dh, '  return 0;\n}', '''  if (Int_Glob != 5 || Bool_Glob != 1 || Ch_1_Glob != 'A' || Ch_2_Glob != 'B' ||
      Arr_1_Glob[8] != 7 || Arr_2_Glob[8][7] != Number_Of_Runs + 10 ||
      Int_1_Loc != 5 || Int_2_Loc != 13 || Int_3_Loc != 7 || Enum_Loc != Ident_2 ||
      Ptr_Glob->Discr != Ident_1 || Ptr_Glob->variant.var_1.Enum_Comp != Ident_3 ||
      Ptr_Glob->variant.var_1.Int_Comp != 17 || Next_Ptr_Glob->Discr != Ident_1 ||
      Next_Ptr_Glob->variant.var_1.Enum_Comp != Ident_2 || Next_Ptr_Glob->variant.var_1.Int_Comp != 18 ||
      strcmp(Str_1_Loc, "DHRYSTONE PROGRAM, 1'ST STRING") ||
      strcmp(Str_2_Loc, "DHRYSTONE PROGRAM, 2'ND STRING")) return 1;
  return 0;
}''')
    manifest = dict(workloads={}, compiler=str(ROOT / '.conda-env/riscv-tools/bin/riscv64-unknown-elf-gcc'),
                    source_root=str(SOURCE), core_count=1, preallocate=1,
                    notes='Scalar RV64GC; existing default sizes; no CTR/software sampling. mt-matmul runs on one hart. mm ROI includes all 8 repetitions.')
    for name in WORKLOADS:
        directory = sources / name
        binary = build / f'{name}.riscv'
        command = [manifest['compiler'], '-I' + str(SOURCE.parent / 'env'), '-I' + str(common),
                   '-I' + str(directory), '-DPREALLOCATE=1', '-mcmodel=medany', '-static',
                   '-std=gnu99', '-O2', '-ffast-math', '-fno-common', '-fno-builtin-printf',
                   '-fno-tree-loop-distribute-patterns', '-march=rv64gc', '-mabi=lp64d',
                   '-o', str(binary), *map(str, sorted(directory.glob('*.c'))),
                   *map(str, sorted(directory.glob('*.S'))), *map(str, sorted(common.glob('*.c'))),
                   *map(str, sorted(common.glob('*.S'))), '-nostdlib', '-nostartfiles', '-lm', '-lgcc',
                   '-T', str(common / 'test.ld')]
        with (build / f'{name}.build.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        manifest['workloads'][name] = dict(command=command, binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
            staged_sources={str(p.relative_to(sources)):hashlib.sha256(p.read_bytes()).hexdigest()
                            for folder in (directory, common) for p in sorted(folder.iterdir()) if p.is_file()})
        print('Built', name, flush=True)
    (build / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
