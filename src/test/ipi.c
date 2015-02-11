#include <nautilus/nautilus.h>
#include <dev/apic.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/percpu.h>
#include <nautilus/atomic.h>
#include <nautilus/math.h>
#include <nautilus/naut_assert.h>

#define PING_VEC 0xfb
#define PONG_VEC 0xfa

static volatile uint8_t done = 0;

#define TRIALS 100
void ipi_test_setup(void);
void ipi_begin_test(cpu_id_t);

static uint64_t diffs[TRIALS];

static struct time_struct {
    uint64_t start_cycle;
    uint64_t end_cycle;
    uint64_t sum;
    uint64_t max;
    uint64_t min;
    volatile uint64_t  done;
} time;


static inline void __always_inline
time_start (void)
{
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    time.start_cycle = lo | ((uint64_t)(hi) << 32);
}


static inline void __always_inline
time_end (void)
{
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    time.end_cycle = lo | ((uint64_t)(hi) << 32);
}


int
ping (excp_entry_t * excp, excp_vec_t vec);
int
ping (excp_entry_t * excp, excp_vec_t vec)
{
    struct apic_dev * apic = per_cpu_get(apic);
    //printk("PING\n");
    IRQ_HANDLER_END();
    apic_write(apic, APIC_REG_ICR2, 0 << APIC_ICR2_DST_SHIFT);
    apic_write(apic, APIC_REG_ICR, APIC_DEL_MODE_FIXED | PONG_VEC);
    return 0;
}


static int
pong (excp_entry_t * excp, excp_vec_t vec)
{

    //printk("PONG\n");
    IRQ_HANDLER_END();
    done = 1;
    return 0;
}


void
ipi_test_setup (void)
{
    if (register_int_handler(PING_VEC, ping, NULL) != 0) {
        ERROR_PRINT("Could not register int handler\n");
        return;
    }

    if (register_int_handler(PONG_VEC, pong, NULL) != 0) {
        ERROR_PRINT("Could not register int handler\n");
        return;
    }
}

#define APIC_WRITE(apic, reg, val) \
    *(volatile uint32_t*)(apic->base_addr + (reg)) = (val);

void
ipi_begin_test (cpu_id_t cpu)
{
    struct apic_dev * apic = per_cpu_get(apic);
    int i;
    uint64_t diff = 0;
    uint64_t avg = 0;
    uint64_t max = 0;
    uint64_t min = ULLONG_MAX;
    uint64_t sum = 0;

    /* IPI the remote core (1 for now) */
    printk("Starting IPI PING-PONG between core %u and core %u \n", my_cpu_id(), cpu);

    for (i = 0; i < TRIALS; i++) {
    
        uint64_t end = 0;
        uint64_t start = 0;
        uint64_t end_send = 0;

        apic_write(apic, APIC_REG_ESR, 0);

        start = rdtsc();
        apic_write(apic, APIC_REG_ICR2, 0 | (cpu << APIC_ICR2_DST_SHIFT));
        apic_write(apic, APIC_REG_ICR, 0 | APIC_DEL_MODE_FIXED | PING_VEC);
        end_send = rdtsc();

        while (!done);

        end = rdtsc();

        if (start > end) {
            ERROR_PRINT("Strangeness occured in cycle count\n");
        }

        diffs[i] = end - start;

        if (diffs[i] > max) {
            max = diffs[i];
        }
        if (diffs[i] < min) {
            min = diffs[i];
        }

        sum += diffs[i];

        done = 0;
    }

    avg = sum / TRIALS;
    
    printk("PING-PONG complete, IPI latency: %llu cycles, (%u trials, min=%llu, max=%llu)\n",
           avg,
           TRIALS,
           min,
           max);
}




