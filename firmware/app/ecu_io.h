/* Glue between the HAL's UART and the ECU protocol library. This is the one
 * place that knows both: the HAL knows nothing about protocols, the ECU
 * library nothing about hardware. */
#ifndef ECU_IO_H
#define ECU_IO_H

#include <stdint.h>

#include "hal_uart.h"

/* Opens the UART with the serial settings the protocol asks for, then
 * starts the protocol (ecu_init). Single instance. */
hal_status_t ecu_io_init( hal_uart_id_t uart );

/* Call every loop: ticks the protocol, feeds it received bytes and receive
 * errors in the order they arrived, and sends whatever it wants sent. */
void ecu_io_poll( uint32_t now_ms );

#endif /* ECU_IO_H */
