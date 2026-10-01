/* The power-on sweep with the whole firmware around it: the needle goes to
 * the top of the scale, the bottom and back to rest before the reading shows,
 * and a press of ACTION in the middle of it, which changes the theme and
 * draws the dial again, ends it instead of leaving the needle chasing a
 * target from the wrong place.
 *
 * "run" lets the sweep finish. "theme" taps ACTION while it is under way. */
#include "test/sil/harness.h"

/* What the dial was told, millisecond by millisecond, from now to t. */
typedef struct
{
    int32_t peak;
    int32_t trough;
    bool bottom_after_peak; /* the bottom of the scale came after the top */
    unsigned values_off_reading; /* polls whose value was not the reading */
} watch_t;

static watch_t watch_until(
    uint32_t t,
    watch_t w
)
{
    while ( now < t )
    {
        step();
        if ( dial_has_value )
        {
            if ( dial_value > w.peak )
            {
                w.peak = dial_value;
            }
            if ( dial_value < w.trough )
            {
                w.trough = dial_value;
            }
            if ( ( dial_value == 800 ) && ( w.peak == 1600 ) )
            {
                w.bottom_after_peak = true;
            }
            if ( dial_value != expected( 0 ) )
            {
                w.values_off_reading++;
            }
        }
    }
    return w;
}

static void test_it_goes_to_the_top_the_bottom_and_rests( void )
{
    watch_t w = { 0, 100000, false, 0u };

    boot( ROM_KEY );
    /* The first dial is the voltage arc: 8 to 16 V in hundredths. Its first
     * poll is at rest, the bottom. */
    CHECK( dial_has_value );
    CHECK_EQ( dial_value, 800 );
    w.peak   = 800;
    w.trough = 800;
    w        = watch_until( 4000u, w );
    CHECK_EQ( w.peak, 1600 );           /* the top of the scale */
    CHECK( w.bottom_after_peak );       /* and the bottom, after it */
    CHECK( w.values_off_reading > 0u ); /* it really was sweeping */
    /* Done well within four seconds: the reading is what the dial is given. */
    CHECK( dial_has_value );
    CHECK_EQ( dial_value, expected( 0 ) );
    {
        unsigned off = w.values_off_reading;

        w = watch_until( 6000u, w );
        CHECK_EQ( w.values_off_reading, off ); /* nothing but the reading */
    }
}

static void test_a_theme_change_ends_the_sweep( void )
{
    watch_t w = { 0, 100000, false, 0u };

    boot( ROM_KEY );
    w.peak   = 800;
    w.trough = 800;
    w        = watch_until( 800u, w ); /* well into the first leg */
    CHECK( w.peak > 800 );
    CHECK( w.peak < 1600 ); /* not at the top yet */

    /* A tap on ACTION: the theme changes and the dial is drawn again. */
    button( &action_edges, true );
    run( 40u );
    button( &action_edges, false );
    run( 100u );

    /* From here on the dial is only ever given the reading: the sweep has
     * ended. It does not go on to the top or the bottom. */
    w.values_off_reading = 0u;
    w.peak               = expected( 0 );
    w.trough             = expected( 0 );
    w                    = watch_until( 6000u, w );
    CHECK_EQ( w.values_off_reading, 0u );
    CHECK_EQ( w.peak, expected( 0 ) );
    CHECK_EQ( w.trough, expected( 0 ) );
}

int main(
    int argc,
    char ** argv
)
{
    const char * which = ( argc > 1 ) ? argv[1] : "run";

    if ( strcmp( which, "theme" ) == 0 )
    {
        test_a_theme_change_ends_the_sweep();
    }
    else
    {
        test_it_goes_to_the_top_the_bottom_and_rests();
    }
    return MT_RESULT();
}
