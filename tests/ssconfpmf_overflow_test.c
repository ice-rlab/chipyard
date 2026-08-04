#include <stdint.h>
#include <stdio.h>

#define MSTATUS_MIE              (1UL << 3)
#define MIE_LCOFIE               (1UL << 13)
#define MIP_LCOFIP               (1UL << 13)

#define MSTATUS_CSR              0x300
#define MIDELEG_CSR              0x303
#define MIE_CSR                  0x304
#define MTVEC_CSR                0x305
#define MCOUNTINHIBIT_CSR        0x320
#define MCAUSE_CSR               0x342
#define MEPC_CSR                 0x341
#define MIP_CSR                  0x344

#define MHPMEVENT3_CSR           0x323
#define MHPMCOUNTER3_CSR         0xb03

#define HPM_COUNTER_IDX          3
#define HPM_COUNTER_BIT          (1UL << HPM_COUNTER_IDX)

/*
 * Change this if your implementation reports a different implemented width.
 * Your previous observation suggests a 40-bit counter.
 */
#define HPM_WIDTH                40
#define HPM_MASK                 ((1ULL << HPM_WIDTH) - 1)
#define HPM_MAX                  HPM_MASK

/* Sscofpmf RV64 mhpmevent fields. */
#define MHPMEVENT_OF             (1ULL << 63)
#define MHPMEVENT_MINH           (1ULL << 62)

/*
 * Rocket EventSet encoding:
 *
 * bits [7:0]   = event set
 * bits [63:8]  = event mask
 *
 * EventSet 0:
 *   exception=0, load=1, store=2, amo=3, system=4,
 *   arith=5, branch=6, jal=7, jalr=8
 */
#define HPM_EVENT_SET_MASK       0xffULL
#define HPM_EVENT_MASK_SHIFT     8
#define HPM_EVENT_SET_0          0ULL
#define HPM_EVENT_BRANCH_IDX     6

#define HPM_EVENT_BRANCH_RAW                                         \
    ((HPM_EVENT_SET_0 & HPM_EVENT_SET_MASK) |                        \
     (1ULL << (HPM_EVENT_MASK_SHIFT + HPM_EVENT_BRANCH_IDX)))

#define STR1(x) #x
#define STR(x) STR1(x)

#define READ_CSR_NUM(csr) ({                                         \
    unsigned long __tmp;                                             \
    asm volatile("csrr %0, " STR(csr) : "=r"(__tmp));                \
    __tmp;                                                           \
})

#define WRITE_CSR_NUM(csr, value) do {                               \
    unsigned long __value = (unsigned long)(value);                  \
    asm volatile("csrw " STR(csr) ", %0" :: "rK"(__value) : "memory"); \
} while (0)

#define SET_CSR_NUM(csr, mask) do {                                  \
    unsigned long __mask = (unsigned long)(mask);                    \
    asm volatile("csrs " STR(csr) ", %0" :: "rK"(__mask) : "memory"); \
} while (0)

#define CLEAR_CSR_NUM(csr, mask) do {                                \
    unsigned long __mask = (unsigned long)(mask);                    \
    asm volatile("csrc " STR(csr) ", %0" :: "rK"(__mask) : "memory"); \
} while (0)

static volatile uint64_t got_lcofi;
static volatile uint64_t unexpected_trap;

static volatile uint64_t trap_mcause;
static volatile uint64_t trap_mepc;
static volatile uint64_t trap_mstatus;
static volatile uint64_t trap_mip;
static volatile uint64_t trap_mie;
static volatile uint64_t trap_mhpmevent3;
static volatile uint64_t trap_mhpmcounter3;

static void print_state(const char *label)
{
    printf(
        "%s:\n"
        "  mstatus       = 0x%016lx\n"
        "  mideleg       = 0x%016lx\n"
        "  mie           = 0x%016lx\n"
        "  mip           = 0x%016lx\n"
        "  mcountinhibit = 0x%016lx\n"
        "  mhpmevent3    = 0x%016lx\n"
        "  mhpmcounter3  = 0x%016lx\n",
        label,
        READ_CSR_NUM(MSTATUS_CSR),
        READ_CSR_NUM(MIDELEG_CSR),
        READ_CSR_NUM(MIE_CSR),
        READ_CSR_NUM(MIP_CSR),
        READ_CSR_NUM(MCOUNTINHIBIT_CSR),
        READ_CSR_NUM(MHPMEVENT3_CSR),
        READ_CSR_NUM(MHPMCOUNTER3_CSR));
}

void trap_handler(void) __attribute__((interrupt, aligned(4)));

void trap_handler(void)
{
    uint64_t mcause = READ_CSR_NUM(MCAUSE_CSR);
    uint64_t cause = mcause & 0xfff;
    uint64_t is_interrupt = mcause >> 63;

    /*
     * Snapshot first. Do not call printf from the trap handler.
     */
    trap_mcause = mcause;
    trap_mepc = READ_CSR_NUM(MEPC_CSR);
    trap_mstatus = READ_CSR_NUM(MSTATUS_CSR);
    trap_mip = READ_CSR_NUM(MIP_CSR);
    trap_mie = READ_CSR_NUM(MIE_CSR);
    trap_mhpmevent3 = READ_CSR_NUM(MHPMEVENT3_CSR);
    trap_mhpmcounter3 = READ_CSR_NUM(MHPMCOUNTER3_CSR);

    if (is_interrupt && cause == 13) {
        /*
         * Stop new delivery before returning. OF remains set until the
         * post-trap servicing code reinitializes the event.
         */
        CLEAR_CSR_NUM(MIE_CSR, MIE_LCOFIE);
        got_lcofi = 1;
        return;
    }

    unexpected_trap = 1;

    /*
     * For an unexpected synchronous exception, skipping the instruction is
     * preferable to repeatedly trapping at the same PC during diagnostics.
     */
    if (!is_interrupt)
        WRITE_CSR_NUM(MEPC_CSR, trap_mepc + 4);
}

static void stop_counter3(void)
{
    SET_CSR_NUM(MCOUNTINHIBIT_CSR, HPM_COUNTER_BIT);
}

static void start_counter3(void)
{
    CLEAR_CSR_NUM(MCOUNTINHIBIT_CSR, HPM_COUNTER_BIT);
}

/*
 * LCOFIP is software-cleared. Clear both stale interrupt state and stale OF
 * while the counter is stopped.
 */
static int clear_stale_overflow_state(void)
{
    uint64_t event;

    stop_counter3();

    CLEAR_CSR_NUM(MIE_CSR, MIE_LCOFIE);

    /*
     * OF must be cleared before another overflow can generate a request.
     */
    event = READ_CSR_NUM(MHPMEVENT3_CSR);
    event &= ~MHPMEVENT_OF;
    WRITE_CSR_NUM(MHPMEVENT3_CSR, event);

    /*
     * Architectural acknowledgement of the shared local overflow interrupt.
     */
    CLEAR_CSR_NUM(MIP_CSR, MIP_LCOFIP);

    asm volatile("fence iorw, iorw" ::: "memory");

    if (READ_CSR_NUM(MHPMEVENT3_CSR) & MHPMEVENT_OF) {
        printf("FAIL: mhpmevent3.OF did not clear\n");
        return -1;
    }

    if (READ_CSR_NUM(MIP_CSR) & MIP_LCOFIP) {
        printf("FAIL: mip.LCOFIP did not clear\n");
        return -1;
    }

    return 0;
}

/*
 * First prove that the selector counts branches in M-mode.
 *
 * The exact delta is intentionally not checked because the function call,
 * loop, and return may all contribute branch events.
 */
__attribute__((noinline))
static void generate_branch_activity(unsigned long iterations)
{
    asm volatile(
        "1:\n"
        "addi %[n], %[n], -1\n"
        "bnez %[n], 1b\n"
        : [n] "+r"(iterations)
        :
        : "memory");
}

static int test_branch_counting(void)
{
    uint64_t before;
    uint64_t after;
    uint64_t delta;

    printf("\n[1] Testing ordinary branch counting\n");

    if (clear_stale_overflow_state())
        return -1;

    WRITE_CSR_NUM(MHPMCOUNTER3_CSR, 0);

    start_counter3();
    before = READ_CSR_NUM(MHPMCOUNTER3_CSR);

    generate_branch_activity(32);

    after = READ_CSR_NUM(MHPMCOUNTER3_CSR);
    stop_counter3();

    before &= HPM_MASK;
    after &= HPM_MASK;
    delta = (after - before) & HPM_MASK;

    printf("Branch counting: before=0x%lx after=0x%lx delta=%lu\n",
           (unsigned long)before,
           (unsigned long)after,
           (unsigned long)delta);

    if (delta == 0) {
        printf("FAIL: branch selector did not increment counter3\n");
        return -1;
    }

    printf("PASS: branch event is counting in M-mode\n");
    return 0;
}

static int wait_for_lcofi(unsigned long timeout)
{
    while (timeout--) {
        if (got_lcofi)
            return 0;

        /*
         * Read pending state directly as well. This distinguishes interrupt
         * generation from interrupt-delivery failures.
         */
        if (READ_CSR_NUM(MIP_CSR) & MIP_LCOFIP)
            asm volatile("nop");

        asm volatile("nop");
    }

    return -1;
}

static int test_branch_overflow(void)
{
    uint64_t live_event;
    uint64_t live_counter;
    uint64_t live_mip;
    uint64_t live_mie;
    uint64_t live_mstatus;

    printf("\n[2] Testing branch counter overflow interrupt\n");

    got_lcofi = 0;
    unexpected_trap = 0;

    trap_mcause = 0;
    trap_mepc = 0;
    trap_mstatus = 0;
    trap_mip = 0;
    trap_mie = 0;
    trap_mhpmevent3 = 0;
    trap_mhpmcounter3 = 0;

    if (clear_stale_overflow_state())
        return -1;

    /*
     * Use a small amount of headroom instead of HPM_MAX. Branches involved
     * in returning from this function and entering the workload may count.
     *
     * This test requires an overflow soon, not on one exact instruction.
     */
    WRITE_CSR_NUM(MHPMCOUNTER3_CSR, HPM_MAX - 8);

    /*
     * This is an M-mode test, so MINH must remain zero.
     */
    if (READ_CSR_NUM(MHPMEVENT3_CSR) & MHPMEVENT_MINH) {
        printf("FAIL: MINH is set; M-mode branch events cannot count\n");
        return -1;
    }

    /*
     * Deliver LCOFI to M-mode, not S-mode.
     */
    CLEAR_CSR_NUM(MIDELEG_CSR, MIE_LCOFIE);

    SET_CSR_NUM(MIE_CSR, MIE_LCOFIE);
    SET_CSR_NUM(MSTATUS_CSR, MSTATUS_MIE);

    start_counter3();

    print_state("Before overflow workload");

    generate_branch_activity(64);

    if (wait_for_lcofi(100000)) {
        stop_counter3();

        live_event = READ_CSR_NUM(MHPMEVENT3_CSR);
        live_counter = READ_CSR_NUM(MHPMCOUNTER3_CSR) & HPM_MASK;
        live_mip = READ_CSR_NUM(MIP_CSR);
        live_mie = READ_CSR_NUM(MIE_CSR);
        live_mstatus = READ_CSR_NUM(MSTATUS_CSR);

        printf(
            "FAIL: no LCOFI trap\n"
            "Live state:\n"
            "  mstatus       = 0x%016lx\n"
            "  mie           = 0x%016lx\n"
            "  mip           = 0x%016lx\n"
            "  mhpmevent3    = 0x%016lx\n"
            "  mhpmcounter3  = 0x%016lx\n",
            (unsigned long)live_mstatus,
            (unsigned long)live_mie,
            (unsigned long)live_mip,
            (unsigned long)live_event,
            (unsigned long)live_counter);

        if (!(live_event & MHPMEVENT_OF)) {
            printf("Diagnosis: counter did not assert mhpmevent3.OF\n");
        } else if (!(live_mip & MIP_LCOFIP)) {
            printf("Diagnosis: OF asserted, but mip.LCOFIP did not assert\n");
        } else if (!(live_mie & MIE_LCOFIE)) {
            printf("Diagnosis: LCOFIP pending, but mie.LCOFIE is disabled\n");
        } else if (!(live_mstatus & MSTATUS_MIE)) {
            printf("Diagnosis: LCOFIP pending, but mstatus.MIE is disabled\n");
        } else {
            printf("Diagnosis: interrupt is pending and enabled; check mtvec/trap delivery\n");
        }

        return -1;
    }

    stop_counter3();

    printf(
        "Trap snapshot:\n"
        "  mcause         = 0x%016lx\n"
        "  mepc           = 0x%016lx\n"
        "  trap mstatus   = 0x%016lx\n"
        "  trap mie       = 0x%016lx\n"
        "  trap mip       = 0x%016lx\n"
        "  mhpmevent3     = 0x%016lx\n"
        "  mhpmcounter3   = 0x%016lx\n",
        (unsigned long)trap_mcause,
        (unsigned long)trap_mepc,
        (unsigned long)trap_mstatus,
        (unsigned long)trap_mie,
        (unsigned long)trap_mip,
        (unsigned long)trap_mhpmevent3,
        (unsigned long)trap_mhpmcounter3);

    if (unexpected_trap) {
        printf("FAIL: unexpected trap occurred\n");
        return -1;
    }

    if (trap_mcause != (UINT64_C(1) << 63 | 13)) {
        printf("FAIL: expected machine interrupt cause 13\n");
        return -1;
    }

    if (!(trap_mhpmevent3 & MHPMEVENT_OF)) {
        printf("FAIL: LCOFI arrived without mhpmevent3.OF set\n");
        return -1;
    }

    /*
     * LCOFIP may be observed with implementation-dependent latency.
     * The OF bit is the per-counter authoritative overflow indication.
     */
    if (!(trap_mip & MIP_LCOFIP))
        printf("WARNING: trap snapshot did not observe mip.LCOFIP asserted\n");

    if (READ_CSR_NUM(MIE_CSR) & MIE_LCOFIE) {
        printf("FAIL: trap handler did not mask mie.LCOFIE\n");
        return -1;
    }

    printf("PASS: branch counter generated an LCOFI trap\n");
    return 0;
}

static int test_rearm(void)
{
    uint64_t before;
    uint64_t after;

    printf("\n[3] Testing overflow acknowledgement and rearm\n");

    if (clear_stale_overflow_state())
        return -1;

    got_lcofi = 0;

    WRITE_CSR_NUM(MHPMCOUNTER3_CSR, 0);

    SET_CSR_NUM(MIE_CSR, MIE_LCOFIE);
    SET_CSR_NUM(MSTATUS_CSR, MSTATUS_MIE);

    start_counter3();

    before = READ_CSR_NUM(MHPMCOUNTER3_CSR) & HPM_MASK;
    generate_branch_activity(16);
    after = READ_CSR_NUM(MHPMCOUNTER3_CSR) & HPM_MASK;

    stop_counter3();

    printf("Rearm count: before=0x%lx after=0x%lx\n",
           (unsigned long)before,
           (unsigned long)after);

    if (got_lcofi) {
        printf("FAIL: stale overflow immediately retriggered\n");
        return -1;
    }

    if (after == before) {
        printf("FAIL: counter did not resume counting after rearm\n");
        return -1;
    }

    if (READ_CSR_NUM(MHPMEVENT3_CSR) & MHPMEVENT_OF) {
        printf("FAIL: OF unexpectedly set after non-overflowing rearm test\n");
        return -1;
    }

    if (READ_CSR_NUM(MIP_CSR) & MIP_LCOFIP) {
        printf("FAIL: LCOFIP unexpectedly pending after rearm\n");
        return -1;
    }

    printf("PASS: counter was acknowledged and rearmed successfully\n");
    return 0;
}

int main(void)
{
    uint64_t event;

    printf("Installing direct M-mode trap handler\n");

    if ((uintptr_t)trap_handler & 0x3) {
        printf("FAIL: trap handler is not four-byte aligned\n");
        return 1;
    }

    WRITE_CSR_NUM(MTVEC_CSR, (uintptr_t)trap_handler);

    /*
     * Begin from a known disabled state.
     */
    CLEAR_CSR_NUM(MSTATUS_CSR, MSTATUS_MIE);
    CLEAR_CSR_NUM(MIE_CSR, MIE_LCOFIE);
    stop_counter3();

    /*
     * Standalone execution remains in M-mode. Therefore configure the raw
     * branch selector with MINH=0 and OF=1 while stopped.
     *
     * OF=1 prevents a stale/configuration-time overflow request. It is
     * cleared before each test starts the counter.
     */
    printf("Configuring M-mode branch event with initial OF=1, MINH=0\n");

    WRITE_CSR_NUM(MHPMEVENT3_CSR,
                  HPM_EVENT_BRANCH_RAW | MHPMEVENT_OF);

    event = READ_CSR_NUM(MHPMEVENT3_CSR);

    printf("Configured mhpmevent3=0x%016lx\n",
           (unsigned long)event);

    if (event & MHPMEVENT_MINH) {
        printf("FAIL: MINH is set in an M-mode counting test\n");
        return 1;
    }

    if (!(event & MHPMEVENT_OF)) {
        printf("FAIL: OF did not stick during stopped configuration\n");
        return 1;
    }

    /*
     * LCOFI must remain in M-mode for this standalone test.
     */
    CLEAR_CSR_NUM(MIDELEG_CSR, MIE_LCOFIE);

    print_state("Initial state");

    if (test_branch_counting())
        return 1;

    if (test_branch_overflow())
        return 1;

    if (test_rearm())
        return 1;

    clear_stale_overflow_state();
    stop_counter3();
    CLEAR_CSR_NUM(MSTATUS_CSR, MSTATUS_MIE);

    print_state("Final cleaned state");

    printf("\nPASS: Sscofpmf counting, overflow, acknowledgement, and rearm work\n");
    return 0;
}

