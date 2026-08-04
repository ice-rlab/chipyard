#include <stdint.h>
#include <stdio.h>

#define STATEN_BIT (1ULL << 63)

static inline void write_mstateen0(uint64_t x) { asm volatile ("csrw 0x30c, %0" :: "r"(x)); }
static inline void write_mstateen1(uint64_t x) { asm volatile ("csrw 0x30d, %0" :: "r"(x)); }
static inline void write_mstateen2(uint64_t x) { asm volatile ("csrw 0x30e, %0" :: "r"(x)); }
static inline void write_mstateen3(uint64_t x) { asm volatile ("csrw 0x30f, %0" :: "r"(x)); }

static inline uint64_t read_mstateen0(void) { uint64_t x; asm volatile ("csrr %0, 0x30c" : "=r"(x)); return x; }
static inline uint64_t read_mstateen1(void) { uint64_t x; asm volatile ("csrr %0, 0x30d" : "=r"(x)); return x; }
static inline uint64_t read_mstateen2(void) { uint64_t x; asm volatile ("csrr %0, 0x30e" : "=r"(x)); return x; }
static inline uint64_t read_mstateen3(void) { uint64_t x; asm volatile ("csrr %0, 0x30f" : "=r"(x)); return x; }

static inline void write_sstateen0(uint64_t x) { asm volatile ("csrw 0x10c, %0" :: "r"(x)); }
static inline void write_sstateen1(uint64_t x) { asm volatile ("csrw 0x10d, %0" :: "r"(x)); }
static inline void write_sstateen2(uint64_t x) { asm volatile ("csrw 0x10e, %0" :: "r"(x)); }
static inline void write_sstateen3(uint64_t x) { asm volatile ("csrw 0x10f, %0" :: "r"(x)); }

static inline uint64_t read_sstateen0(void) { uint64_t x; asm volatile ("csrr %0, 0x10c" : "=r"(x)); return x; }
static inline uint64_t read_sstateen1(void) { uint64_t x; asm volatile ("csrr %0, 0x10d" : "=r"(x)); return x; }
static inline uint64_t read_sstateen2(void) { uint64_t x; asm volatile ("csrr %0, 0x10e" : "=r"(x)); return x; }
static inline uint64_t read_sstateen3(void) { uint64_t x; asm volatile ("csrr %0, 0x10f" : "=r"(x)); return x; }

static void print_mstateen(const char *tag)
{
  printf("%s\n", tag);
  printf("  mstateen0 = 0x%016lx\n", read_mstateen0());
  printf("  mstateen1 = 0x%016lx\n", read_mstateen1());
  printf("  mstateen2 = 0x%016lx\n", read_mstateen2());
  printf("  mstateen3 = 0x%016lx\n", read_mstateen3());
}

static void print_sstateen(const char *tag)
{
  printf("%s\n", tag);
  printf("  sstateen0 = 0x%016lx\n", read_sstateen0());
  printf("  sstateen1 = 0x%016lx\n", read_sstateen1());
  printf("  sstateen2 = 0x%016lx\n", read_sstateen2());
  printf("  sstateen3 = 0x%016lx\n", read_sstateen3());
}

int main(void)
{
  printf("stateen CSR test start\n");

  print_mstateen("initial mstateen:");
  print_sstateen("initial sstateen:");

  printf("writing mstateen0-3 = STATEN_BIT\n");
  write_mstateen0(STATEN_BIT);
  write_mstateen1(STATEN_BIT);
  write_mstateen2(STATEN_BIT);
  write_mstateen3(STATEN_BIT);

  print_mstateen("after mstateen writes:");

  printf("writing sstateen0-3 = STATEN_BIT\n");
  write_sstateen0(STATEN_BIT);
  write_sstateen1(STATEN_BIT);
  write_sstateen2(STATEN_BIT);
  write_sstateen3(STATEN_BIT);

  print_sstateen("after sstateen writes:");

  printf("writing mstateen0-3 = all ones\n");
  write_mstateen0(~0ULL);
  write_mstateen1(~0ULL);
  write_mstateen2(~0ULL);
  write_mstateen3(~0ULL);

  print_mstateen("after all-ones mstateen writes:");

  printf("writing sstateen0-3 = all ones\n");
  write_sstateen0(~0ULL);
  write_sstateen1(~0ULL);
  write_sstateen2(~0ULL);
  write_sstateen3(~0ULL);

  print_sstateen("after all-ones sstateen writes:");

  printf("stateen CSR test done\n");
  return 0;
}