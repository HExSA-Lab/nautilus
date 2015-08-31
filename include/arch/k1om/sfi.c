#include <nautilus/nautilus.h>
#include <nautilus/sfi.h>
#include <arch/k1om/k1omsfi.h>

int
__early_init_sfi (struct naut_info * naut)
{
    struct sfi_sys_tbl * sfi = sfi_find_syst();
    if (!sfi) {
        panic("Could not find  SFI table!\n");
        return -1;
    }

    if (sfi_parse_syst(&naut->sys, sfi) != 0) {
        ERROR_PRINT("Problem parsing SFI SYST\n");
        return -1;
    }

    return 0;
}
