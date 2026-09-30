/* Connects the HAL UART to the ECU protocol library. */
#ifndef ECU_IO_H
#define ECU_IO_H

#include <stdint.h>

#include "hal/hal_uart.h"

/* Opens the UART with the serial settings the protocol asks for, then
 * starts the protocol (ecu_init). Single instance. */
hal_status_t ecu_io_init( hal_uart_id_t uart );

/* Call every loop: ticks the protocol, feeds it received bytes and receive
 * errors in the order they arrived, and sends whatever it wants sent. */
void ecu_io_poll( uint32_t now_ms );

#endif /* ECU_IO_H */
