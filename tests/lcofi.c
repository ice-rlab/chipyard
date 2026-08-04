#include <stdint.h>
#include <stdio.h>

#define MSTATUS_MIE       (1UL << 3)
#define MIE_LCOFIE        (1UL << 13)

#define MCOUNTINHIBIT_CSR 0x320
#define MSTATUS_CSR       0x300
#define MIE_CSR           0x304
#define MIP_CSR           0x344
#define MTVEC_CSR         0x305
#define MCAUSE_CSR        0x342

#define MHPMEVENT3_CSR    0x323
#define MHPMCOUNTER3_CSR  0xb03

#define HPM_WIDTH         40
#define HPM_MASK          ((1ULL << HPM_WIDTH) - 1)

#define SAMPLE_PERIOD     100ULL
#define HPM_INIT_VALUE    ((1ULL << HPM_WIDTH) - SAMPLE_PERIOD)

/*
 * Rocket/BOOM EventSets encoding:
 *
 *   bits [7:0] = EventSet number
 *   bits [8+n] = event n within that EventSet
 *
 * Assumption:
 *
 *   EventSet 3:
 *     event 0 = uops dispatched
 *
 * Therefore:
 *
 *   selector = 3 | (1 << 8)
 *            = 0x103
 */
#define HPM_EVENT_SET_MASK       0xffULL
#define HPM_EVENT_MASK_SHIFT     8

#define HPM_UOPS_EVENT_SET       3ULL
#define HPM_UOPS_DISPATCHED_IDX  0

// /*
//  * Rocket/BOOM EventSets encoding:
//  *
//  *   bits [7:0] = EventSet number
//  *   bits [8+n] = event n within that EventSet
//  *
//  * Rocket EventSet 0:
//  *
//  *   event 0 = exception
//  *   event 1 = load
//  *   event 2 = store
//  *   event 3 = amo
//  *   event 4 = system
//  *   event 5 = arith
//  *   event 6 = branch
//  *
//  * Therefore:
//  *
//  *   selector = 0 | (1 << (8 + 6))
//  *            = 1 << 14
//  *            = 0x4000
//  */
// #define HPM_UOPS_EVENT_SET       0ULL
// #define HPM_EVENT_SET_MASK       0xffULL
// #define HPM_EVENT_MASK_SHIFT     8

// #define HPM_BRANCH_EVENT_SET     0ULL
// #define HPM_BRANCH_EVENT_IDX     6

#define HPM_BRANCH_EVENT_SELECTOR \
	(HPM_BRANCH_EVENT_SET | \
	 (1ULL << (HPM_EVENT_MASK_SHIFT + HPM_BRANCH_EVENT_IDX)))


#define HPM_EVENT_UOPS_DISPATCHED \
	((HPM_UOPS_EVENT_SET & HPM_EVENT_SET_MASK) | \
	 (1ULL << (HPM_EVENT_MASK_SHIFT + HPM_UOPS_DISPATCHED_IDX)))

// #define HPM_EVENT_UOPS_DISPATCHED \
// 	((HPM_UOPS_EVENT_SET & HPM_EVENT_SET_MASK) | \
// 	 (1ULL << (HPM_EVENT_MASK_SHIFT + HPM_BRANCH_EVENT_IDX)))

#define STR1(x) #x
#define STR(x) STR1(x)

#define READ_CSR_NUM(csr) ({                              \
	unsigned long __tmp;                              \
	asm volatile("csrr %0, " STR(csr) : "=r"(__tmp)); \
	__tmp;                                             \
})

#define WRITE_CSR_NUM(csr, val) do {                       \
	unsigned long __v = (unsigned long)(val);          \
	asm volatile("csrw " STR(csr) ", %0" :: "rK"(__v));\
} while (0)

#define SET_CSR_NUM(csr, mask) do {                        \
	unsigned long __v = (unsigned long)(mask);         \
	asm volatile("csrs " STR(csr) ", %0" :: "rK"(__v));\
} while (0)

#define CLEAR_CSR_NUM(csr, mask) do {                      \
	unsigned long __v = (unsigned long)(mask);         \
	asm volatile("csrc " STR(csr) ", %0" :: "rK"(__v));\
} while (0)

volatile int got_lcofi;

volatile uint64_t saved_mcause;
volatile uint64_t saved_mip;
volatile uint64_t saved_mhpmevent3;
volatile uint64_t saved_mhpmcounter3;

void trap_handler(void) __attribute__((interrupt));

void trap_handler(void)
{
	uint64_t mcause = READ_CSR_NUM(MCAUSE_CSR);
	uint64_t cause = mcause & 0xfff;

	if ((mcause >> 63) && cause == 13) {
		/*
		 * Save the overflow state before clearing or changing anything.
		 */
		saved_mcause = mcause;
		saved_mip = READ_CSR_NUM(MIP_CSR);
		saved_mhpmevent3 = READ_CSR_NUM(MHPMEVENT3_CSR);
		saved_mhpmcounter3 = READ_CSR_NUM(MHPMCOUNTER3_CSR);

		got_lcofi = 1;

		/*
		 * Prevent repeated interrupts while printing results.
		 */
		CLEAR_CSR_NUM(MIE_CSR, MIE_LCOFIE);
		SET_CSR_NUM(MCOUNTINHIBIT_CSR, 1UL << 3);

		/*
		 * Restore the event selector without its overflow state and reset
		 * the counter. Depending on your OF-bit implementation, writing the
		 * selector may also clear the overflow condition.
		 */
		WRITE_CSR_NUM(MHPMEVENT3_CSR,
			      HPM_EVENT_UOPS_DISPATCHED);
		WRITE_CSR_NUM(MHPMCOUNTER3_CSR, 0);
	}
}

__attribute__((noinline))
static uint64_t generate_uops(void)
{
	volatile uint64_t a = 1;
	volatile uint64_t b = 2;
	volatile uint64_t c = 3;

	/*
	 * Generate substantially more than SAMPLE_PERIOD dispatched uops.
	 * The volatile operands help prevent the compiler from deleting
	 * the loop.
	 */
	for (volatile unsigned long i = 0; i < 1000; i++) {
		a = a + b;
		b = b ^ c;
		c = c + i;
		a = a ^ c;
		b = b + a;
	}

	return a + b + c;
}

int main(void)
{
	uint64_t event_readback;
	uint64_t counter_readback;
	uint64_t result;

	got_lcofi = 0;
	saved_mcause = 0;
	saved_mip = 0;
	saved_mhpmevent3 = 0;
	saved_mhpmcounter3 = 0;

	printf("Installing machine trap handler\n");
	WRITE_CSR_NUM(MTVEC_CSR, (uintptr_t)trap_handler);

	printf("Inhibiting mhpmcounter3\n");
	SET_CSR_NUM(MCOUNTINHIBIT_CSR, 1UL << 3);

	printf("Selecting uops-dispatched event: mhpmevent3=0x%lx\n",
	       (unsigned long)HPM_EVENT_UOPS_DISPATCHED);

	WRITE_CSR_NUM(MHPMEVENT3_CSR, HPM_EVENT_UOPS_DISPATCHED);
	event_readback = READ_CSR_NUM(MHPMEVENT3_CSR);

	printf("After event select: mhpmevent3=0x%lx\n",
	       (unsigned long)event_readback);

	if (event_readback == 0) {
		printf("FAIL: mhpmevent3 rejected the uops-dispatched selector\n");
		printf("Expected selector: 0x%lx\n",
		       (unsigned long)HPM_EVENT_UOPS_DISPATCHED);
		return 1;
	}

	printf("Setting mhpmcounter3 to 0x%lx for period %lu\n",
	       (unsigned long)HPM_INIT_VALUE,
	       (unsigned long)SAMPLE_PERIOD);

	WRITE_CSR_NUM(MHPMCOUNTER3_CSR, HPM_INIT_VALUE);
	counter_readback = READ_CSR_NUM(MHPMCOUNTER3_CSR);

	printf("After counter set: mhpmcounter3=0x%lx mhpmevent3=0x%lx\n",
	       (unsigned long)counter_readback,
	       READ_CSR_NUM(MHPMEVENT3_CSR));

	if ((counter_readback & HPM_MASK) != HPM_INIT_VALUE) {
		printf("FAIL: mhpmcounter3 preload did not stick\n");
		return 1;
	}

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

	printf("Starting mhpmcounter3\n");
	CLEAR_CSR_NUM(MCOUNTINHIBIT_CSR, 1UL << 3);

	printf("Before workload: mcountinhibit=0x%lx "
	       "mhpmcounter3=0x%lx mhpmevent3=0x%lx\n",
	       READ_CSR_NUM(MCOUNTINHIBIT_CSR),
	       READ_CSR_NUM(MHPMCOUNTER3_CSR),
	       READ_CSR_NUM(MHPMEVENT3_CSR));

	printf("Generating dispatched uops\n");
	result = generate_uops();

	printf("After workload: result=%lu mhpmevent3=0x%lx "
	       "mhpmcounter3=0x%lx mip=0x%lx mie=0x%lx\n",
	       (unsigned long)result,
	       READ_CSR_NUM(MHPMEVENT3_CSR),
	       READ_CSR_NUM(MHPMCOUNTER3_CSR),
	       READ_CSR_NUM(MIP_CSR),
	       READ_CSR_NUM(MIE_CSR));

	printf("Waiting for LCOFI interrupt\n");

	for (volatile unsigned long i = 0;
	     i < 100000 && !got_lcofi;
	     i++) {
		asm volatile("nop");
	}

	printf("Saved interrupt state:\n");
	printf("  mcause       = 0x%lx\n",
	       (unsigned long)saved_mcause);
	printf("  mip          = 0x%lx\n",
	       (unsigned long)saved_mip);
	printf("  mhpmevent3   = 0x%lx\n",
	       (unsigned long)saved_mhpmevent3);
	printf("  mhpmcounter3 = 0x%lx\n",
	       (unsigned long)saved_mhpmcounter3);
	printf("  got_lcofi    = %d\n", got_lcofi);

	if (!got_lcofi) {
		printf("FAIL: no uops-dispatched overflow interrupt\n");
		return 1;
	}

	printf("PASS: uops-dispatched overflow interrupt fired\n");
	return 0;
}
