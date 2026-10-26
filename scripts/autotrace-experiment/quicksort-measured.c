// Reuse the repository's quicksort implementation and its 99-element rand input.
// Build with -DQUICKSORT_SOURCE='"/absolute/path/to/benchmarks/quicksort.c"'.
#define main original_unmeasured_main
#include QUICKSORT_SOURCE
#undef main
#include <stdint.h>

#define CSR_READ(csr) ({ uint64_t value; __asm__ volatile("csrr %0, " #csr : "=r"(value) :: "memory"); value; })
#define CSR_WRITE(csr, value) __asm__ volatile("csrw " #csr ", %0" :: "rK"((uint64_t)(value)) : "memory")

int main(void) {
  int arr[N];
  unsigned long sum = 0, sorted_sum = 0;
  for (int i = 0; i < N; ++i) {
    arr[i] = rand() & 1023;
    sum += arr[i];
  }

  // BOOM event sets: low 8 bits select the set, upper bits select the event.
  // set 1 bit 1 = branch misprediction recovery (not committed mispredictions).
  // set 2 bit 0 = instruction-cache acquire/refill transaction.
  CSR_WRITE(mcountinhibit, -1);
  CSR_WRITE(mhpmevent3, 0x201);
  CSR_WRITE(mhpmevent4, 0x102);
  const uint64_t cycles0 = CSR_READ(mcycle);
  const uint64_t inst0 = CSR_READ(minstret);
  const uint64_t mispredict0 = CSR_READ(mhpmcounter3);
  const uint64_t imiss0 = CSR_READ(mhpmcounter4);
  CSR_WRITE(mcountinhibit, 0);
  quickSort(arr, 0, N - 1);
  CSR_WRITE(mcountinhibit, -1);
  const uint64_t cycles = CSR_READ(mcycle) - cycles0;
  const uint64_t instructions = CSR_READ(minstret) - inst0;
  const uint64_t mispredicts = CSR_READ(mhpmcounter3) - mispredict0;
  const uint64_t imisses = CSR_READ(mhpmcounter4) - imiss0;
  CSR_WRITE(mcountinhibit, 0);

  for (int i = 0; i < N; ++i) {
    if (i && arr[i - 1] > arr[i]) return 1;
    sorted_sum += arr[i];
  }
  if (sum != sorted_sum) return 2;
  printf("QSORT_METRICS target_cycles=%lu retired_instructions=%lu "
         "branch_misprediction_recoveries=%lu icache_acquires=%lu correctness=PASS\n",
         (unsigned long)cycles, (unsigned long)instructions,
         (unsigned long)mispredicts, (unsigned long)imisses);
  return 0;
}
