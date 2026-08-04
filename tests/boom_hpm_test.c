#include <stdint.h>
#include <stdio.h>

#define MCOUNTINHIBIT_CSR 0x320
#define MHPMEVENT3_CSR    0x323
#define MHPMCOUNTER3_CSR  0xb03

#define COUNTER3_BIT      (1UL << 3)

/*
 * BOOM distributed-counter event:
 *
 * EventSet 3, event index 0: "uops dispatched"
 *
 * Rocket/BOOM EventSet encoding:
 *
 *   selector = eventSet | (1 << (8 + eventIndex))
 *
 * Therefore:
 *
 *   selector = 3 | (1 << 8)
 *            = 0x103
 */
#define BOOM_UOPS_DISPATCHED_EVENT 0x103UL

#define STR1(x) #x
#define STR(x) STR1(x)

#define READ_CSR(csr) ({                                      \
    unsigned long value;                                      \
    asm volatile("csrr %0, " STR(csr) : "=r"(value));         \
    value;                                                    \
})

#define WRITE_CSR(csr, value) do {                            \
    unsigned long v = (unsigned long)(value);                 \
    asm volatile("csrw " STR(csr) ", %0"                      \
                 :: "rK"(v) : "memory");                      \
} while (0)

#define SET_CSR(csr, mask) do {                               \
    unsigned long v = (unsigned long)(mask);                  \
    asm volatile("csrs " STR(csr) ", %0"                      \
                 :: "rK"(v) : "memory");                      \
} while (0)

#define CLEAR_CSR(csr, mask) do {                             \
    unsigned long v = (unsigned long)(mask);                  \
    asm volatile("csrc " STR(csr) ", %0"                      \
                 :: "rK"(v) : "memory");                      \
} while (0)

/*
 * Instruction-heavy workload intended to generate a clear number of
 * dispatched micro-operations.
 *
 * The dependency chains help prevent the compiler from removing the work.
 */
__attribute__((noinline))
static uint64_t dispatch_workload(uint64_t iterations)
{
    uint64_t a = 1;
    uint64_t b = 3;
    uint64_t c = 5;
    uint64_t d = 7;

    asm volatile(
        "1:\n"

        "addi %[a], %[a], 1\n"
        "addi %[b], %[b], 2\n"
        "add  %[c], %[c], %[a]\n"
        "xor  %[d], %[d], %[b]\n"

        "add  %[a], %[a], %[d]\n"
        "xor  %[b], %[b], %[c]\n"
        "addi %[c], %[c], 3\n"
        "add  %[d], %[d], %[a]\n"

        "addi %[iterations], %[iterations], -1\n"
        "bnez %[iterations], 1b\n"

        : [iterations] "+r"(iterations),
          [a] "+r"(a),
          [b] "+r"(b),
          [c] "+r"(c),
          [d] "+r"(d)
        :
        : "memory");

    return a ^ b ^ c ^ d;
}

int main(void)
{
    uint64_t programmed_event;
    uint64_t before;
    uint64_t after;
    uint64_t delta;
    uint64_t result;

    printf("Testing BOOM uops-dispatched event\n");
    printf("Expected selector: 0x%lx\n",
           (unsigned long)BOOM_UOPS_DISPATCHED_EVENT);

    /*
     * Stop counter 3 before programming the event.
     */
    SET_CSR(MCOUNTINHIBIT_CSR, COUNTER3_BIT);

    /*
     * Configure mhpmcounter3 to count:
     *
     *   EventSet 3, event 0: uops dispatched
     */
    WRITE_CSR(MHPMEVENT3_CSR, BOOM_UOPS_DISPATCHED_EVENT);

    programmed_event = READ_CSR(MHPMEVENT3_CSR);

    if (programmed_event != BOOM_UOPS_DISPATCHED_EVENT) {
        printf("FAIL: mhpmevent3 readback mismatch\n");
        printf("Expected: 0x%lx\n",
               (unsigned long)BOOM_UOPS_DISPATCHED_EVENT);
        printf("Actual:   0x%lx\n",
               (unsigned long)programmed_event);
        return 1;
    }

    /*
     * Reset the counter while it is inhibited.
     */
    WRITE_CSR(MHPMCOUNTER3_CSR, 0);

    if (READ_CSR(MHPMCOUNTER3_CSR) != 0) {
        printf("FAIL: mhpmcounter3 did not reset to zero\n");
        return 1;
    }

    printf("Configured state:\n");
    printf("  mhpmevent3    = 0x%lx\n",
           READ_CSR(MHPMEVENT3_CSR));
    printf("  mhpmcounter3  = 0x%lx\n",
           READ_CSR(MHPMCOUNTER3_CSR));
    printf("  mcountinhibit = 0x%lx\n",
           READ_CSR(MCOUNTINHIBIT_CSR));

    /*
     * Start counter 3.
     */
    CLEAR_CSR(MCOUNTINHIBIT_CSR, COUNTER3_BIT);

    before = READ_CSR(MHPMCOUNTER3_CSR);

    result = dispatch_workload(10000);

    after = READ_CSR(MHPMCOUNTER3_CSR);

    /*
     * Stop counting before printing or checking the result.
     */
    SET_CSR(MCOUNTINHIBIT_CSR, COUNTER3_BIT);

    delta = after - before;

    printf("Results:\n");
    printf("  before = %lu\n", (unsigned long)before);
    printf("  after  = %lu\n", (unsigned long)after);
    printf("  delta  = %lu\n", (unsigned long)delta);
    printf("  result = 0x%lx\n", (unsigned long)result);

    if (delta == 0) {
        printf("FAIL: uops-dispatched event did not increment\n");
        return 1;
    }

    /*
     * This workload has approximately nine explicit instructions per loop,
     * including the loop branch. The exact count can differ because this
     * event may be multi-incrementing and because surrounding instructions
     * can enter the dispatch stage while the counter is running.
     */
    printf("PASS: uops-dispatched event incremented\n");

    return 0;
}
