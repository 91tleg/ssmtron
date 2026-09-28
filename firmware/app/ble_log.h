/* Streams the ECU values over the Bluetooth pipe as CSV lines (see
 * fmt_log_line). Knows the ECU interface and the BLE HAL, nothing else. */
#ifndef BLE_LOG_H
#define BLE_LOG_H

#include <stdint.h>

/* Call every loop. While a peer is listening, sends at most one line per
 * BLE_LOG_PERIOD_MS, for whichever parameter currently has a fresh value.
 */
void ble_log_poll( uint32_t now_ms );

#define BLE_LOG_PERIOD_MS ( 200u )

#endif /* BLE_LOG_H */
