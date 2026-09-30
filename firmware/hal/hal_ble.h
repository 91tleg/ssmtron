/* Non-blocking byte pipe to one BLE peer. Poll for connection events.
 * Firmware updates use a separate transport. */
#ifndef HAL_BLE_H
#define HAL_BLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hal_status.h"

typedef struct
{
    const char * name; /* advertised device name */
    /* Six-digit passkey (0..999999) the peer must enter to
     * pair. Data only flows over an encrypted, authenticated link. The
     * caller shows the passkey to the user, and a fresh one is used each
     * time Bluetooth is started. */
    uint32_t passkey;
} hal_ble_cfg_t;

typedef enum
{
    HAL_BLE_EVT_NONE = 0u,
    HAL_BLE_EVT_CONNECTED,   /* a peer connected */
    HAL_BLE_EVT_DISCONNECTED /* the peer is gone; advertising resumes */
} hal_ble_evt_t;

/* HAL_ERR_INVALID for a bad name or passkey.
 * Starts the stack and advertising, connectable by one peer at a time.
 * Advertising resumes by itself after a disconnect. */
hal_status_t hal_ble_open( const hal_ble_cfg_t * cfg );

/* Next pending link event, HAL_BLE_EVT_NONE when there is none. */
hal_ble_evt_t hal_ble_poll_event( void );

/* True while a peer is connected and listening; only then can data be
 * sent. */
bool hal_ble_ready( void );

/* Queues bytes for the peer and returns how many were accepted: 0 when not
 * ready, less than len when the queue is full. Nothing is kept for a peer
 * that is not there. */
size_t hal_ble_write(
    const uint8_t * buf,
    size_t len
);

/* Bytes the peer sent, oldest first; returns how many were copied. */
size_t hal_ble_read(
    uint8_t * buf,
    size_t max
);

#endif /* HAL_BLE_H */
