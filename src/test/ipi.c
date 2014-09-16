#include <nautilus.h>
#include <dev/apic.h>
#include <irq.h>
#include <percpu.h>
#include <spinlock.h>
#include <math.h>

#define PING_VEC 0xf0
#define PONG_VEC 0xf1

#define TRIALS 1000
void ipi_test_setup(void);
void ipi_begin_test(void);


static struct time_struct {
    uint64_t start_cycle;
    uint64_t end_cycle;
    uint64_t sum;
    uint64_t max;
    uint64_t min;
    uint8_t done;
    spinlock_t lock;
} time;


static inline void
time_start (void)
{
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    time.start_cycle = lo | ((uint64_t)(hi) << 32);
}


static inline void
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
    uint32_t id = per_cpu_get(id);
    //printk("PING: running on cpu %u\n", id);
    apic_ipi(apic, 0, PONG_VEC);
    IRQ_HANDLER_END();
    return 0;
}


static int
pong (excp_entry_t * excp, excp_vec_t vec)
{
    time_end();
    while (!(__sync_bool_compare_and_swap((volatile uint8_t*)&(time.done), 0, 1))) {
        asm volatile ("pause");
    }
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
    spinlock_init(&(time.lock));
}


void
ipi_begin_test (void)
{
    struct apic_dev * apic = per_cpu_get(apic);
    int i;
    uint64_t avg;

    memset(&time, 0, sizeof(time));
    time.min = ULONG_MAX;

    /* IPI the remote core (1 for now) */
    printk("Starting IPI PING-PONG...\n");

    for (i = 0; i < TRIALS; i++) {
        uint64_t diff;
        time_start();
        apic_ipi(apic, 1, PING_VEC);

        while (*(volatile uint8_t*)(&time.done) != 1) {
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




