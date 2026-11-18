#include <stdint.h>
#include <stdio.h>

#define MAR_ENTRY_BASE 0x80000200UL

#define SSTATEEN0_CSR 0x10c
#define MSTATEEN0_CSR 0x30c

#define SMARCTL_CSR    0x5d0
#define SMARDEPTH_CSR  0x5d1
#define SMARSTATUS_CSR 0x5d2

#define MMARCTL_CSR    0x7d0
#define MMARDEPTH_CSR  0x7d1
#define MMARSTATUS_CSR 0x7d2

#define SISELECT_CSR 0x150
#define SIREG_CSR    0x151
#define SIREG2_CSR   0x152
#define SIREG3_CSR   0x153
#define SIREG4_CSR   0x155

// Adjust these masks if your Bundle encoding differs.
#define MARSTATUS_FROZEN_MASK (1UL << 0)

// Assumed ctl encoding:
// bit 0: u enable
// bit 1: s enable
// bit 2: m enable
// bit 3: load inhibit
// bit 4: store inhibit
#define MARCTL_U_MASK        (1UL << 0)
#define MARCTL_S_MASK        (1UL << 1)
#define MARCTL_M_MASK        (1UL << 2)
#define MARCTL_LOADINH_MASK  (1UL << 3)
#define MARCTL_STOREINH_MASK (1UL << 4)

#define MARCTL_ENABLE_ALL_PRIVS \
  (MARCTL_U_MASK | MARCTL_S_MASK | MARCTL_M_MASK)

#define STR1(x) #x
#define STR(x) STR1(x)

#define READ_CSR_NUM(csr) ({                         \
  unsigned long __tmp;                               \
  asm volatile("csrr %0, " STR(csr) : "=r"(__tmp)); \
  __tmp;                                             \
})

#define WRITE_CSR_NUM(csr, val)                        \
  do {                                                 \
    unsigned long __v = (unsigned long)(val);          \
    asm volatile("csrw " STR(csr) ", %0" :: "rK"(__v)); \
  } while (0)

#define SET_CSR_NUM(csr, mask)                         \
  do {                                                 \
    unsigned long __v = (unsigned long)(mask);         \
    asm volatile("csrs " STR(csr) ", %0" :: "rK"(__v)); \
  } while (0)

#define CLEAR_CSR_NUM(csr, mask)                       \
  do {                                                 \
    unsigned long __v = (unsigned long)(mask);         \
    asm volatile("csrc " STR(csr) ", %0" :: "rK"(__v)); \
  } while (0)

static inline void write_siselect(uint64_t x)
{
  asm volatile("csrw 0x150, %0" :: "rK"(x));
}

static inline uint64_t read_siselect(void)
{
  uint64_t x;
  asm volatile("csrr %0, 0x150" : "=r"(x));
  return x;
}

static inline uint64_t read_sireg(void)
{
  uint64_t x;
  asm volatile("csrr %0, 0x151" : "=r"(x));
  return x;
}

static inline uint64_t read_sireg2(void)
{
  uint64_t x;
  asm volatile("csrr %0, 0x152" : "=r"(x));
  return x;
}

static inline uint64_t read_sireg3(void)
{
  uint64_t x;
  asm volatile("csrr %0, 0x153" : "=r"(x));
  return x;
}

static inline uint64_t read_sireg4(void)
{
  uint64_t x;
  asm volatile("csrr %0, 0x155" : "=r"(x));
  return x;
}

static void dump_stateen(const char *tag)
{
  printf("%s: mstateen0=0x%lx sstateen0=0x%lx\n",
         tag,
         READ_CSR_NUM(MSTATEEN0_CSR),
         READ_CSR_NUM(SSTATEEN0_CSR));
}

volatile uint64_t mar_buf[32];

__attribute__((noinline)) static void exactly_16_stores(void)
{
  asm volatile("" ::: "memory");

  mar_buf[0]  = 0x1000;
  mar_buf[1]  = 0x1001;
  mar_buf[2]  = 0x1002;
  mar_buf[3]  = 0x1003;
  mar_buf[4]  = 0x1004;
  mar_buf[5]  = 0x1005;
  mar_buf[6]  = 0x1006;
  mar_buf[7]  = 0x1007;
  mar_buf[8]  = 0x1008;
  mar_buf[9]  = 0x1009;
  mar_buf[10] = 0x100a;
  mar_buf[11] = 0x100b;
  mar_buf[12] = 0x100c;
  mar_buf[13] = 0x100d;
  mar_buf[14] = 0x100e;
  mar_buf[15] = 0x100f;

  asm volatile("" ::: "memory");
}

static void dump_mar(const char *tag)
{
  uint64_t smardepth = READ_CSR_NUM(SMARDEPTH_CSR);
  uint64_t depth_enc = smardepth & 0xf;
  uint64_t mar_depth = 16ULL << depth_enc;

  printf("%s: smarctl=0x%lx smardepth=0x%lx smarstatus=0x%lx decoded_depth=%lu\n",
         tag,
         READ_CSR_NUM(SMARCTL_CSR),
         smardepth,
         READ_CSR_NUM(SMARSTATUS_CSR),
         mar_depth);

  for (uint64_t i = 0; i < mar_depth; i++) {
    uint64_t idx = MAR_ENTRY_BASE + i;

    write_siselect(idx);

    uint64_t addr = read_sireg();
    uint64_t pc   = read_sireg2();
    uint64_t time = read_sireg3();
    uint64_t data = read_sireg4();

    printf("%s: mar[%3lu] sel=0x%lx addr=0x%lx pc=0x%lx time=0x%lx data=0x%lx tpe=0x%lx\n",
           tag,
           i,
           read_siselect(),
           addr,
           pc,
           time,
           data,
           data & 0xf);
  }
}

static int run_one_round(int round)
{
  printf("\n=== MAR ROUND %d ===\n", round);

  printf("Initial MAR state: smarctl=0x%lx smardepth=0x%lx smarstatus=0x%lx\n",
         READ_CSR_NUM(SMARCTL_CSR),
         READ_CSR_NUM(SMARDEPTH_CSR),
         READ_CSR_NUM(SMARSTATUS_CSR));

  printf("Unfreezing MAR\n");
  CLEAR_CSR_NUM(SMARSTATUS_CSR, MARSTATUS_FROZEN_MASK);

  printf("Programming MAR controls: enable U/S/M, enable loads/stores\n");
  WRITE_CSR_NUM(SMARCTL_CSR, MARCTL_ENABLE_ALL_PRIVS);
  WRITE_CSR_NUM(MMARCTL_CSR, MARCTL_ENABLE_ALL_PRIVS);

  printf("After MAR setup: smarctl=0x%lx mmarctl=0x%lx smarstatus=0x%lx\n",
         READ_CSR_NUM(SMARCTL_CSR),
         READ_CSR_NUM(MMARCTL_CSR),
         READ_CSR_NUM(SMARSTATUS_CSR));

  printf("Generating exactly 16 stores\n");
  exactly_16_stores();

  printf("Freezing MAR before dump\n");
  SET_CSR_NUM(SMARSTATUS_CSR, MARSTATUS_FROZEN_MASK);

  printf("After stores: smarctl=0x%lx smardepth=0x%lx smarstatus=0x%lx\n",
         READ_CSR_NUM(SMARCTL_CSR),
         READ_CSR_NUM(SMARDEPTH_CSR),
         READ_CSR_NUM(SMARSTATUS_CSR));

  dump_mar(round == 0 ? "round0" : "round1");

  printf("Unfreezing MAR for next round\n");
  CLEAR_CSR_NUM(SMARSTATUS_CSR, MARSTATUS_FROZEN_MASK);

  return 0;
}

int main(void)
{
  dump_stateen("initial");

  if (run_one_round(0))
    return 1;
  if (run_one_round(1))
    return 1;

  printf("\nALL MAR BASIC FILL/DUMP TESTS PASSED\n");
  return 0;
}