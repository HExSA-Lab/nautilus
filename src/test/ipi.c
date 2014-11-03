#include <nautilus.h>
#include <dev/apic.h>
#include <irq.h>
#include <cpu.h>
#include <percpu.h>
#include <atomic.h>
#include <math.h>
#include <naut_assert.h>

#define PING_VEC 0xf0
#define PONG_VEC 0xf1

#define TRIALS 1000
void ipi_test_setup(void);
void ipi_begin_test(cpu_id_t);


static struct time_struct {
    uint64_t start_cycle;
    uint64_t end_cycle;
    uint64_t sum;
    uint64_t max;
    uint64_t min;
    volatile unsigned done;
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


static int
ping (excp_entry_t * excp, excp_vec_t vec)
{
    struct apic_dev * apic = per_cpu_get(apic);
    apic_ipi(apic, 0, PONG_VEC);
    IRQ_HANDLER_END();
    return 0;
}


static int
pong (excp_entry_t * excp, excp_vec_t vec)
{
    time_end();

    //printk("PONG\n");
    //PAUSE_WHILE(atomic_cmpswap(time.done, 0, 1) != 0);
    //time.done = 1;
    //mbarrier();

    while (1) {
        cli();
        if (__sync_bool_compare_and_swap((volatile unsigned*)&(time.done), 0, 1) == 0) {
            sti();
            break;
        }
        sti();
        asm volatile("pause");
    }


    //ASSERT(!irqs_enabled());
    IRQ_HANDLER_END();
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
    uint64_t avg;

    if (cpu >= per_cpu_get(system)->num_cpus) {
        ERROR_PRINT("IPI_TEST: invalid cpu number (%u)\n", cpu);
        return;
    }

    memset(&time, 0, sizeof(time));
    time.min = ULONG_MAX;

    /* IPI the remote core (1 for now) */
    printk("Starting IPI PING-PONG between core %u and core %u \n", my_cpu_id(), cpu);

    for (i = 0; i < TRIALS; i++) {
        uint64_t diff;
        time_start();
        asm volatile ("cli");
        APIC_WRITE(apic, APIC_REG_ICR2, cpu << APIC_ICR2_DST_SHIFT);
        APIC_WRITE(apic, APIC_REG_ICR, PING_VEC | ICR_LEVEL_ASSERT);
        asm volatile ("sti");

        while (1) {
            cli();
            if (*(volatile unsigned*)&(time.done) == 1) {
                sti();
                break;
            }
            sti();
            asm volatile ("pause");
        }

        if (time.end_cycle < time.start_cycle) {
            ERROR_PRINT("Strangeness occured in cycle count\n");
        }

        diff = time.end_cycle - time.start_cycle;
        if (diff > time.max) {
            time.max = diff;
        }
        if (diff < time.min) {
            time.min = diff;
        }

        time.sum += diff; 

        time.end_cycle = 0;
        time.start_cycle = 0;
        time.done = 0;
    }

    avg = time.sum / TRIALS;
    
    printk("PING-PONG complete, IPI latency: %llu cycles, (%u trials, min=%llu, max=%llu)\n",
           avg,
           TRIALS,
           time.min,
           time.max);
}




