#include <nautilus/nautilus.h>
#include <nautilus/mm.h>
#include <arch/hrt/hrt.h>

int 
arch_early_init (struct naut_info * naut)
{
    return __early_init_hrt(naut);
}
