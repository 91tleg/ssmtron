#include "ssm1_romid.h"

void ssm1_romid_init( ssm1_romid_t * p )
{
    *p = ( ssm1_romid_t ){ 0 };
}

void ssm1_romid_error( ssm1_romid_t * p )
{
    p->len     = 0;
    p->seen[0] = 0;
    p->seen[1] = 0;
}

/* Our own ROM ID command, echoed back, looks like the candidate 00 ff ff.
 */
#define ECHO_CAND ( 0xFFFF00u )

static bool tally(
    ssm1_romid_t * p,
    uint32_t cand,
    uint32_t * id
)
{
    bool confirmed = false;

    if ( cand != ECHO_CAND )
    {
        uint8_t slot = 2u;

        for ( uint8_t i = 0; i < 2u; i++ )
        {
            if ( ( p->seen[i] != 0u ) && ( p->cand[i] == cand ) )
            {
                slot = i;
            }
        }
        if ( slot == 2u ) /* new: replace the less-seen slot */
        {
            slot = ( uint8_t )( ( p->seen[0] <= p->seen[1] ) ? 0u : 1u );
            p->cand[slot] = cand;
            p->seen[slot] = 0;
        }
        p->seen[slot]++;
        if ( p->seen[slot] >= 2u )
        {
            *id        = cand;
            confirmed  = true;
            p->seen[0] = 0;
            p->seen[1] = 0;
        }
    }
    return confirmed;
}

/* True if the window looks like the stream's own frames (ff fe xx) after a
 * lost or extra byte. A stream with one byte missing or added repeats the
 * same false ID every time that happens, so two sightings would confirm it.
 * A real reply does not hold a second frame header, and its ID is not the
 * bytes just before it turned round. */
static bool looks_like_stream( const uint8_t * w )
{
    bool turned =
        ( ( w[3] == w[1] ) && ( w[4] == w[2] ) && ( w[5] == w[0] ) )
        || ( ( w[3] == w[2] ) && ( w[4] == w[0] ) && ( w[5] == w[1] ) );
    bool header_in_xx = ( w[2] == 0xFFu ) && ( w[3] == 0xFEu );
    bool header_in_id = ( w[4] == 0xFFu ) && ( w[5] == 0xFEu );

    return turned || header_in_xx || header_in_id;
}

bool ssm1_romid_feed(
    ssm1_romid_t * p,
    uint8_t byte,
    uint32_t * id
)
{
    bool got = false;

    p->win[p->len] = byte;
    p->len++;
    if ( p->len == SSM1_ROMID_WIN )
    {
        /* r0 == 0xff is more likely the start of the next header */
        if ( ( p->win[0] == 0xFFu ) && ( p->win[1] == 0xFEu )
             && ( p->win[3] != 0xFFu ) && !looks_like_stream( p->win ) )
        {
            uint32_t cand = ( uint32_t )p->win[3]
                            | ( ( uint32_t )p->win[4] << 8 )
                            | ( ( uint32_t )p->win[5] << 16 );

            got    = tally( p, cand, id );
            p->len = 0;
        }
        else
        {
            for ( uint8_t i = 1; i < SSM1_ROMID_WIN; i++ )
            {
                p->win[i - 1u] = p->win[i];
            }
            p->len = SSM1_ROMID_WIN - 1u;
        }
    }
    return got;
}
