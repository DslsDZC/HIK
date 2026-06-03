#include "hal.h"
#include <stdlib.h>

/* Fixed timestamp for deterministic tests */
static u64 s_test_timestamp = 1000000;

cpu_id_t hal_get_cpu_id(void) { return 0; }
void hal_memory_barrier(void) {}
void hal_read_barrier(void) {}
void hal_write_barrier(void) {}
bool hal_disable_interrupts(void) { return false; }
void hal_enable_interrupts(void) {}
void hal_restore_interrupts(bool state) { (void)state; }
u64 hal_get_timestamp(void) { return s_test_timestamp; }
void hal_udelay(u32 us) { (void)us; }
bool hal_is_kernel_mode(void) { return true; }
void hal_halt(void) {}
void hal_idle(void) {}
void hal_breakpoint(void) {}
u64 hal_get_page_size(void) { return 4096; }
bool hal_supports_io_ports(void) { return false; }
void hal_uart_init(const hal_uart_config_t *config) { (void)config; }
void hal_uart_putc(char c) { (void)c; }
void hal_uart_puts(const char *str) { (void)str; }
bool hal_uart_rx_ready(void) { return false; }
bool hal_uart_tx_ready(void) { return true; }

/* Test helpers */
void test_hal_set_timestamp(u64 ts) { s_test_timestamp = ts; }
