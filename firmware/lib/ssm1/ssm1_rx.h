/* SSM1 replies are [msb][lsb][data] with no start marker or checksum.
 * A sliding window finds the requested address. Data is reported only
 * after the next frame confirms alignment, adding one frame of latency.
 * Equal address and data bytes can still mimic aligned frames. */
#ifndef SSM1_RX_H
#define SSM1_RX_H

#include <stdbool.h>
#include <stdint.h>

#define SSM1_FRAME_LEN ( 3u )

typedef struct
{
    uint8_t win[SSM1_FRAME_LEN];
    uint8_t len;
    uint8_t hi;
    uint8_t lo;
    uint8_t prev_data; /* last valid frame's data, held until confirmed */
    bool have_prev;
    uint32_t discarded; /* bytes dropped while hunting, for diagnostics */
    uint32_t frames;    /* valid frames seen */
} ssm1_rx_t;

void ssm1_rx_init( ssm1_rx_t * rx );
/* Listen for frames of this address; drops any partial frame and lock. */
void ssm1_rx_expect(
    ssm1_rx_t * rx,
    uint16_t addr
);
/* The last byte had a parity/framing error: drop the partial frame and
 * require re-confirmation. */
void ssm1_rx_error( ssm1_rx_t * rx );
/* True when a frame was confirmed by its successor; *data is its data. */
bool ssm1_rx_feed(
    ssm1_rx_t * rx,
    uint8_t byte,
    uint8_t * data
);

#endif /* SSM1_RX_H */
