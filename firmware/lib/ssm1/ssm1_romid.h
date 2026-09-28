/* ROM ID reply parser. While streaming address 0xFFFE the ECU sends
 * "ff fe xx" frames, and after a ROM ID command the frame is followed by
 * the three ID bytes: "ff fe xx r0 r1 r2". Like ssm1_rx this hunts with a
 * sliding window. It only reports an ID once the same candidate has been
 * seen twice: our own echoed command or a broken frame can look like a
 * candidate, but a wrong one rarely repeats. Pure: no IO. */
#ifndef SSM1_ROMID_H
#define SSM1_ROMID_H

#include <stdbool.h>
#include <stdint.h>

#define SSM1_ROMID_WIN ( 6u )

typedef struct
{
    uint8_t win[SSM1_ROMID_WIN];
    uint8_t len;
    uint32_t cand[2]; /* two most recent distinct candidates */
    uint8_t seen[2];  /* how often each was seen; 0 = slot empty */
} ssm1_romid_t;

void ssm1_romid_init( ssm1_romid_t * p );
/* The last byte had a parity/framing error. */
void ssm1_romid_error( ssm1_romid_t * p );
/* True when an ID is confirmed; *id = r0 | r1 << 8 | r2 << 16. */
bool ssm1_romid_feed(
    ssm1_romid_t * p,
    uint8_t byte,
    uint32_t * id
);

#endif /* SSM1_ROMID_H */
