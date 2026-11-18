#include <stdint.h>
#include <stdio.h>

#define CTR_ENTRY_BASE 0x200UL

#define SSTATEEN0_CSR 0x10c
#define MSTATEEN0_CSR 0x30c

#define SCTRCTL_CSR 0x14e
#define SCTRDEPTH_CSR 0x15f
#define SISELECT_CSR 0x150
#define SIREG_CSR 0x151
#define SIREG2_CSR 0x152
#define SIREG3_CSR 0x153

#define MSTATUS_CSR 0x300
#define MIE_CSR 0x304
#define MTVEC_CSR 0x305
#define MCOUNTINHIBIT_CSR 0x320
#define MIP_CSR 0x344
#define MCAUSE_CSR 0x342
#define MHPMEVENT3_CSR 0x323
#define MHPMCOUNTER3_CSR 0xb03

#define MSTATUS_MIE (1UL << 3)
#define MIE_LCOFIE (1UL << 13)
#define MIP_LCOFIP (1UL << 13)

#define SCTRCTL_FROZEN_MASK (1UL << 0)
#define SCTRCTL_LCOFIFRZ_MASK (1UL << 12)

#define HPM_WIDTH 40
#define HPM_MAX ((1ULL << HPM_WIDTH) - 1)

#define HPM_EVENT_SET_0 1ULL
#define HPM_EVENT_SET_MASK 0xffULL
#define HPM_EVENT_MASK_SHIFT 8
#define HPM_EVENT_BRANCH_IDX 4

#define MHPMEVENT3_BRANCH_LCOFI                                      \
  ((HPM_EVENT_SET_0 & HPM_EVENT_SET_MASK) |                          \
   (1ULL << (HPM_EVENT_MASK_SHIFT + HPM_EVENT_BRANCH_IDX)) |          \
   (1ULL << 63))

#define STR1(x) #x
#define STR(x) STR1(x)

#define READ_CSR_NUM(csr) ({                          \
  unsigned long __tmp;                                \
  asm volatile("csrr %0, " STR(csr) : "=r"(__tmp));  \
  __tmp;                                              \
})

#define WRITE_CSR_NUM(csr, val)                         \
  do                                                     \
  {                                                      \
    unsigned long __v = (unsigned long)(val);            \
    asm volatile("csrw " STR(csr) ", %0" ::"rK"(__v)); \
  } while (0)

#define SET_CSR_NUM(csr, mask)                          \
  do                                                     \
  {                                                      \
    unsigned long __v = (unsigned long)(mask);           \
    asm volatile("csrs " STR(csr) ", %0" ::"rK"(__v)); \
  } while (0)

#define CLEAR_CSR_NUM(csr, mask)                        \
  do                                                     \
  {                                                      \
    unsigned long __v = (unsigned long)(mask);           \
    asm volatile("csrc " STR(csr) ", %0" ::"rK"(__v)); \
  } while (0)

volatile int got_lcofi = 0;
volatile uint64_t trap_mcause = 0;

static inline void write_siselect(uint64_t x)
{
  asm volatile("csrw 0x150, %0" ::"rK"(x));
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

void trap_handler(void) __attribute__((interrupt));
void trap_handler(void)
{
  uint64_t mcause = READ_CSR_NUM(MCAUSE_CSR);
  uint64_t cause = mcause & 0xfff;

  trap_mcause = mcause;

  if ((mcause >> 63) && cause == 13)
  {
    got_lcofi = 1;

    SET_CSR_NUM(SCTRCTL_CSR, SCTRCTL_FROZEN_MASK);

    CLEAR_CSR_NUM(MIE_CSR, MIE_LCOFIE);
    CLEAR_CSR_NUM(MIP_CSR, MIP_LCOFIP);

    WRITE_CSR_NUM(MHPMEVENT3_CSR, MHPMEVENT3_BRANCH_LCOFI);
    WRITE_CSR_NUM(MHPMCOUNTER3_CSR, 0);
  }
}


static void dump_stateen(const char *tag)
{
  printf("%s: mstateen0=0x%lx sstateen0=0x%lx\n",
         tag,
         READ_CSR_NUM(MSTATEEN0_CSR),
         READ_CSR_NUM(SSTATEEN0_CSR));
}
__attribute__((noinline)) static void exactly_16_cfis(void)
{
  asm volatile(
      ".option push\n"
      ".option norvc\n"

      "jal 1f\n"
      "1:\n"
      "beq x0, x0, 2f\n"
      "2:\n"
      "bne x0, x0, 3f\n"
      "3:\n"
      "jal x0, 4f\n"
      "4:\n"
      "auipc t0, 0\n"
      "addi  t0, t0, 16\n"
      "jalr  x0, 0(t0)\n"
      "nop\n"
      "nop\n"
      "5:\n"

      "jal 6f\n"
      "6:\n"
      "beq x0, x0, 7f\n"
      "7:\n"
      "bne x0, x0, 8f\n"
      "8:\n"
      "jal x0, 9f\n"
      "9:\n"
      "auipc t0, 0\n"
      "addi  t0, t0, 16\n"
      "jalr  x0, 0(t0)\n"
      "nop\n"
      "nop\n"
      "10:\n"

      "jal 11f\n"
      "11:\n"
      "beq x0, x0, 12f\n"
      "12:\n"
      "bne x0, x0, 13f\n"
      "13:\n"
      "jal x0, 14f\n"
      "14:\n"
      "auipc t0, 0\n"
      "addi  t0, t0, 16\n"
      "jalr  x0, 0(t0)\n"
      "nop\n"
      "nop\n"
      "15:\n"

      "jal 16f\n"
      "16:\n"

      ".option pop\n" ::: "t0", "ra", "memory");
}

static void dump_ctr(const char *tag)
{
  uint64_t sctrdepth = READ_CSR_NUM(SCTRDEPTH_CSR);
  uint64_t depth_enc = sctrdepth & 0xf;
  uint64_t ctr_depth = 16ULL << depth_enc;

  printf("%s: sctrctl=0x%lx sctrdepth=0x%lx decoded_depth=%lu\n",
         tag,
         READ_CSR_NUM(SCTRCTL_CSR),
         sctrdepth,
         ctr_depth);

  for (uint64_t i = 0; i < ctr_depth; i++)
  {
    uint64_t idx = CTR_ENTRY_BASE + i;

    write_siselect(idx);

    uint64_t source = read_sireg();
    uint64_t target = read_sireg2();
    uint64_t data = read_sireg3();

    printf("%s: ctr[%3lu] sel=0x%lx source=0x%lx source_v=%lu source_pc=0x%lx "
           "target=0x%lx target_misp=%lu target_pc=0x%lx data=0x%lx\n",
           tag,
           i,
           read_siselect(),
           source,
           source & 1,
           source & ~1ULL,
           target,
           target & 1,
           target & ~1ULL,
           data);
  }
}

static int run_one_round(int round)
{
  got_lcofi = 0;
  trap_mcause = 0;

  printf("\n=== ROUND %d ===\n", round);

  printf("Installing trap handler\n");
  WRITE_CSR_NUM(MTVEC_CSR, (uintptr_t)trap_handler);

  printf("Inhibiting hpmcounter3\n");
  SET_CSR_NUM(MCOUNTINHIBIT_CSR, (1UL << 3));

  printf("Setting CTR lcofifrz and unfreezing CTR\n");
  SET_CSR_NUM(SCTRCTL_CSR, SCTRCTL_LCOFIFRZ_MASK);
  CLEAR_CSR_NUM(SCTRCTL_CSR, SCTRCTL_FROZEN_MASK);
  printf("After CTR setup: sctrctl=0x%lx\n", READ_CSR_NUM(SCTRCTL_CSR));

  printf("Selecting branch overflow event for mhpmcounter3: mhpmevent3=0x%lx\n",
         (unsigned long)MHPMEVENT3_BRANCH_LCOFI);
  WRITE_CSR_NUM(MHPMEVENT3_CSR, MHPMEVENT3_BRANCH_LCOFI);
  printf("After event select: mhpmevent3=0x%lx\n",
         READ_CSR_NUM(MHPMEVENT3_CSR));

  printf("Setting mhpmcounter3 to HPM_MAX=0x%lx\n", (unsigned long)HPM_MAX);
  WRITE_CSR_NUM(MHPMCOUNTER3_CSR, HPM_MAX);
  printf("After counter set: mhpmcounter3=0x%lx mhpmevent3=0x%lx\n",
         READ_CSR_NUM(MHPMCOUNTER3_CSR),
         READ_CSR_NUM(MHPMEVENT3_CSR));

  printf("Enabling mie.LCOFIE\n");
  SET_CSR_NUM(MIE_CSR, MIE_LCOFIE);
  printf("After mie enable: mie=0x%lx mhpmevent3=0x%lx\n",
         READ_CSR_NUM(MIE_CSR),
         READ_CSR_NUM(MHPMEVENT3_CSR));

  printf("Enabling mstatus.MIE\n");
  SET_CSR_NUM(MSTATUS_CSR, MSTATUS_MIE);
  printf("After mstatus enable: mstatus=0x%lx mhpmevent3=0x%lx\n",
         READ_CSR_NUM(MSTATUS_CSR),
         READ_CSR_NUM(MHPMEVENT3_CSR));

  printf("Starting hpmcounter3\n");
  CLEAR_CSR_NUM(MCOUNTINHIBIT_CSR, (1UL << 3));
  printf("After start: mcountinhibit=0x%lx mhpmcounter3=0x%lx mhpmevent3=0x%lx\n",
         READ_CSR_NUM(MCOUNTINHIBIT_CSR),
         READ_CSR_NUM(MHPMCOUNTER3_CSR),
         READ_CSR_NUM(MHPMEVENT3_CSR));

  printf("Triggering branch / CTR events\n");
  exactly_16_cfis();

  printf("After branch: mhpmevent3=0x%lx mhpmcounter3=0x%lx mip=0x%lx mie=0x%lx sctrctl=0x%lx\n",
         READ_CSR_NUM(MHPMEVENT3_CSR),
         READ_CSR_NUM(MHPMCOUNTER3_CSR),
         READ_CSR_NUM(MIP_CSR),
         READ_CSR_NUM(MIE_CSR),
         READ_CSR_NUM(SCTRCTL_CSR));

  printf("Waiting for LCOFI interrupt\n");
  for (volatile int i = 0; i < 100000 && !got_lcofi; i++)
  {
    asm volatile("nop");
  }

  printf("final: mhpmevent3=0x%lx mhpmcounter3=0x%lx mip=0x%lx mie=0x%lx sctrctl=0x%lx got_lcofi=%d mcause=0x%lx\n",
         READ_CSR_NUM(MHPMEVENT3_CSR),
         READ_CSR_NUM(MHPMCOUNTER3_CSR),
         READ_CSR_NUM(MIP_CSR),
         READ_CSR_NUM(MIE_CSR),
         READ_CSR_NUM(SCTRCTL_CSR),
         got_lcofi,
         trap_mcause);

  if (!got_lcofi)
  {
    printf("FAIL round %d: no LCOFI interrupt\n", round);
    return 1;
  }

  printf("PASS round %d: HPM branch overflow interrupt fired\n", round);

  dump_ctr(round == 0 ? "round0" : "round1");

  printf("Resetting LCOFI / CTR state for next round\n");
  SET_CSR_NUM(MCOUNTINHIBIT_CSR, (1UL << 3));
  CLEAR_CSR_NUM(MIE_CSR, MIE_LCOFIE);
  CLEAR_CSR_NUM(MIP_CSR, MIP_LCOFIP);
  WRITE_CSR_NUM(MHPMEVENT3_CSR, MHPMEVENT3_BRANCH_LCOFI);
  WRITE_CSR_NUM(MHPMCOUNTER3_CSR, 0);
  CLEAR_CSR_NUM(SCTRCTL_CSR, SCTRCTL_FROZEN_MASK);

  printf("After reset: mhpmevent3=0x%lx mhpmcounter3=0x%lx mip=0x%lx mie=0x%lx sctrctl=0x%lx\n",
         READ_CSR_NUM(MHPMEVENT3_CSR),
         READ_CSR_NUM(MHPMCOUNTER3_CSR),
         READ_CSR_NUM(MIP_CSR),
         READ_CSR_NUM(MIE_CSR),
         READ_CSR_NUM(SCTRCTL_CSR));

  return 0;
}

int main(void)
{
  dump_stateen("initial");
  
  if (run_one_round(0))
    return 1;
  if (run_one_round(1))
    return 1;

  printf("\nALL CTR LCOFI FREEZE TESTS PASSED\n");
  return 0;
}