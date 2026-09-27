/* Ids index the UARTs the board declares; pin routing is the board's
 * job. */
#ifndef HAL_UART_H
#define HAL_UART_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "hal_status.h"

typedef uint8_t hal_uart_id_t;

typedef enum
{
    HAL_UART_PARITY_NONE = 0u,
    HAL_UART_PARITY_EVEN,
    HAL_UART_PARITY_ODD
} hal_uart_parity_t;

typedef struct
{
    uint32_t baud;
    uint8_t data_bits;
    hal_uart_parity_t parity;
    uint8_t stop_bits;
} hal_uart_cfg_t;

/* HAL_ERR_UNSUPPORTED if the chip cannot hit this config (e.g. baud). */
hal_status_t hal_uart_open(
    hal_uart_id_t id,
    const hal_uart_cfg_t * cfg
);

size_t hal_uart_write(
    hal_uart_id_t id,
    const uint8_t * buf,
    size_t len
);

typedef enum
{
    HAL_UART_RX_EMPTY, /* nothing pending */
    HAL_UART_RX_BYTE,  /* *byte holds a received byte */
    HAL_UART_RX_ERROR  /* see below; *byte is untouched */
} hal_uart_rx_t;

/* Non-blocking. Received bytes and receive errors come out in the order
 * they happened, so the caller can tell where the stream is suspect.
 * HAL_UART_RX_ERROR means a parity, framing or overrun error was detected,
 * or the driver had to drop bytes, at this point in the stream. It stands
 * for "the bytes around here cannot be trusted", not for one exact byte. */
hal_uart_rx_t hal_uart_read(
    hal_uart_id_t id,
    uint8_t * byte
);

#endif
