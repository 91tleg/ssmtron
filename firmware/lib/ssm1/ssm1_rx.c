#include "ssm1_rx.h"

void ssm1_rx_init( ssm1_rx_t * rx )
{
    *rx = ( ssm1_rx_t ){ 0 };
}

void ssm1_rx_expect(
    ssm1_rx_t * rx,
    uint16_t addr
)
{
    rx->hi        = ( uint8_t )( addr >> 8 );
    rx->lo        = ( uint8_t )( addr & 0xFFu );
    rx->len       = 0;
    rx->have_prev = false;
}

void ssm1_rx_error( ssm1_rx_t * rx )
{
    rx->len       = 0;
    rx->have_prev = false;
}

bool ssm1_rx_feed(
    ssm1_rx_t * rx,
    uint8_t byte,
    uint8_t * data
)
{
    bool got = false;

    if ( rx->len >= SSM1_FRAME_LEN )
    {
        rx->len = 0; /* unreachable unless state is corrupt */
    }
    rx->win[rx->len] = byte;
    rx->len++;

    if ( rx->len == SSM1_FRAME_LEN )
    {
        if ( ( rx->win[0] == rx->hi ) && ( rx->win[1] == rx->lo ) )
        {
            rx->len = 0;
            rx->frames++;
            if ( rx->have_prev )
            {
                *data = rx->prev_data;
                got   = true;
            }
            rx->prev_data = rx->win[2];
            rx->have_prev = true;
        }
        else
        {
            rx->win[0]    = rx->win[1];
            rx->win[1]    = rx->win[2];
            rx->len       = SSM1_FRAME_LEN - 1u;
            rx->have_prev = false;
            rx->discarded++;
        }
    }
    return got;
}
