/* SSM1 reply parser. Replies are [msb][lsb][data] frames with no checksum
 * and no start marker. We know which address we asked for, so the two
 * address bytes act as the sync marker: a sliding 3-byte window discards
 * one byte at a time until it lines up, which also absorbs lost bytes,
 * inserted bytes, an echo of our own command, and leftovers from an earlier
 * address.
 *
 * A frame's data is only reported once the NEXT frame also starts with the
 * expected address. A frame assembled from misaligned bytes (a lost or
 * inserted byte can slide the next msb into the data slot) is followed by
 * garbage, so it is never reported. Cost: one frame (~17 ms at 1953 baud)
 * of latency. Known limit: if msb == lsb and a data byte equals them, a
 * misaligned window can look like two valid frames. Pure: no IO. */
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
