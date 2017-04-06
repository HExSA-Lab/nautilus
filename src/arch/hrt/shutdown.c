/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/naut_types.h>
#include <nautilus/cpu.h>
#include <nautilus/acpi.h>
#include <nautilus/shutdown.h>
#include <acpi/acpi.h>
#include <dev/ps2.h>

void 
reboot (void) 
{
    /* resetting A20 should be a bit quicker */
    outb(1, 0x92);

#if 0
    cli();

    tmp = inb(KBD_CMD_REG);
    do {
        if (__CHECK_BIT(tmp, KBD_BIT_KDATA)) {
            inb(KBD_DATA_REG);
        }
    } while (__CHECK_BIT(tmp, KBD_BIT_UDATA));

    outb(KBD_RESET, KBD_CMD_REG);
#endif

    while (1) {
        halt();
    }
}

void
acpi_reboot (void)
{
    /* NOT YET */
}

struct shutdown_info {
    uint32_t  smi_cmd;
    uint8_t   acpi_enable;
    uint8_t   acpi_disable;
    uint32_t  pm1a_cnt;
    uint32_t  pm1b_cnt;
    uint16_t  slp_typa;
    uint16_t  slp_typb;
    uint16_t  slp_en;
    uint16_t  sci_en;
    uint8_t   pm1_cnt_len;
    uint32_t  dsdt;
    uint64_t  xdsdt;

    uint8_t shutdown_ok;
};


static int
try_acpi_shutdown (struct acpi_table_header * hdr, void * arg)
{
    struct shutdown_info * s = (struct shutdown_info *)arg;
    struct acpi_table_fadt * fadt = (struct acpi_table_fadt*)hdr;

    printk("Parsing fadt\n");

    s->dsdt         = fadt->dsdt;
    s->xdsdt        = fadt->Xdsdt;
    s->smi_cmd      = fadt->smi_command;
    s->acpi_enable  = fadt->acpi_enable;
    s->acpi_disable = fadt->acpi_disable;
    s->pm1a_cnt     = fadt->pm1a_control_block;
    s->pm1b_cnt     = fadt->pm1b_control_block;
    s->pm1_cnt_len  = fadt->pm1_control_length;
    s->slp_en       = 1<<13;
    s->sci_en       = 1;

    return 0;
}

static int
get_s5 (struct shutdown_info * s)
{
    struct acpi_table_header * hdr = (struct acpi_table_header*)(uint64_t)s->dsdt;
    uint32_t len = hdr->length;
    unsigned i;
    char * cursor = NULL;

    if (memcmp((void*)(uint64_t)s->dsdt, "DSDT", 4)) {
        printk("NOT REALLY DSDT\n");
    }

    printk("dsdt is at %p, len=%u\n", (void*)hdr, len);
    for (i = 0; i < len; i++) {
        char * tmp = (char*)(uint64_t)s->dsdt + i;
        if (!memcmp(tmp, "_S5_", 4)) {
            s->shutdown_ok = 1;
            printk("found s5\n");
            cursor = tmp;
            break;
        }
    }

    if (!s->shutdown_ok) {
        printk("no S5 found\n");
        return 0;
    }

    if ((*(cursor-1) == 0x08 || ( *(cursor-2) == 0x08 && *(cursor-1) == '\\')) && *(cursor+4) == 0x12) {
        printk("valid AML for shutdown\n");

        cursor += 5;
        cursor += ((*cursor & 0xc0) >> 6) + 2;

        if (*cursor == 0x0a) {
            cursor++;
        }

        s->slp_typa = *cursor << 10;
        cursor++;

        if (*cursor == 0x0a) {
            cursor++;
        }
        s->slp_typb = *cursor << 10;

    }


    return 0;
}

        

/* TODO: fix this */
void
acpi_shutdown (void)
{
    struct shutdown_info s;
    memset(&s, 0, sizeof(s));

    if (acpi_table_parse(ACPI_SIG_FADT, try_acpi_shutdown, &s)) {
        goto out_noacpi;
    } 

    get_s5(&s);

    if (!s.shutdown_ok) {
        goto out_noacpi;
    }


    if (s.sci_en == 0) {
        goto out_noacpi;
    }

    outw(s.slp_typa | s.slp_en, s.pm1a_cnt);
    if (s.pm1b_cnt) {
        outw(s.slp_typb | s.slp_en, (unsigned)s.pm1b_cnt);
    }

out_noacpi:
    printk("System halting.\n");

    while (1) {
        halt();
    }

}
