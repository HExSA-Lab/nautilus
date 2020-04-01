void nk_monitor_init();
void nk_monitor_init_ap();

int nk_monitor_entry ();
int nk_monitor_excp_entry ();
int nk_monitor_irq_entry ();
int nk_monitor_panic_entry ();
int nk_monitor_debug_entry ();
void nk_monitor_sync_entry (void *arg);