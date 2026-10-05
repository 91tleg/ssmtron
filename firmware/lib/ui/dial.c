/* The analog dial: draws it a pixel at a time into one row or one box, with
 * no bitmap stored, and moves its needle. The face is worked out by
 * dial_face.c, the angles by dial_angle.c, the shapes by dial_shape.c and the
 * screws and cover by dial_cover.c. */
#include "lib/ui/dial.h"

#include <string.h>

#include "lib/display/display.h"
#include "lib/ui/dial_angle.h"
#include "lib/ui/dial_cover.h"
#include "lib/ui/dial_face.h"
#include "lib/ui/dial_layout.h"
#include "lib/ui/ui_math.h"
#include "lib/ui/dial_motion.h"
#include "lib/ui/dial_shape.h"
#include "lib/ui/font.h"

/* The ticks, blended over color. dx and dy are from the pivot. */
static uint16_t ticks_layer(
    const dial_cfg_t * cfg,
    const dial_face_t * face,
    int32_t dx,
    int32_t dy,
    uint16_t color
)
{
    int32_t dist_sq  = ( dx * dx ) + ( dy * dy );
    int32_t ring_in  = dial_tick_ring_in( cfg );
    int32_t ring_out = dial_tick_ring_out( cfg );
    uint16_t out     = color;

    if ( ( dist_sq >= ( ring_in * ring_in ) )
         && ( dist_sq <= ( ring_out * ring_out ) ) )
    {
        for ( size_t i = 0; i < face->tick_count; i++ )
        {
            const dial_tick_t * tick = &face->ticks[i];
            uint8_t ink              = dial_radial_line_alpha(
                dx,
                dy,
                tick->sin_m,
                tick->cos_m,
                dial_tick_radius( cfg )
                    - ( tick->major ? DIAL_TICK_MAJOR_LEN
                                                 : DIAL_TICK_MINOR_LEN ),
                dial_tick_radius( cfg ),
                tick->major ? DIAL_TICK_MAJOR_HALF_W : DIAL_TICK_HALF_W
            );

            if ( ink != 0u )
            {
                out = font_blend( cfg->tick_fg, out, ink );
            }
        }
    }
    return out;
}

/* The tick numbers, title and units, blended over color. */
static uint16_t labels_layer(
    const dial_cfg_t * cfg,
    const dial_face_t * face,
    int16_t row,
    int16_t col,
    uint16_t color
)
{
    uint16_t out = color;

    for ( size_t i = 0; i < face->label_count; i++ )
    {
        uint8_t ink = 0u;

        if ( dial_label_pixel( &face->labels[i], row, col, &ink ) )
        {
            out = font_blend( cfg->tick_fg, out, ink );
        }
    }
    return out;
}

/* The cap's colors depend only on panel coordinates. Its thin bevel and
 * bright top face make it sit above the rotating collar and counterweight. */
static uint16_t hub_color(
    int32_t dy,
    int32_t dist_sq
)
{
    int32_t light = 7 - ( ( dy * 5 ) / DIAL_HUB_R );
    int32_t lift  = 3 - ( ( dy * 2 ) / DIAL_HUB_R );
    uint16_t rim;
    uint16_t face;

    light = ui_max( light, 0 );
    light = ( light > ( int32_t )FONT_ALPHA_MAX ) ? ( int32_t )FONT_ALPHA_MAX
                                                  : light;
    rim   = font_blend( 0xFFFEu, 0x9492u, ( uint8_t )light );
    face  = font_blend( 0xFFFFu, 0xF7BEu, ( uint8_t )lift );
    return font_blend(
        face, rim, dial_disc_alpha( dist_sq, DIAL_HUB_INNER_R )
    );
}

/* Light across the blade gives it a shallow ridge. Coverage remains the
 * shape helper's responsibility, so shading cannot enlarge a dirty region. */
static uint16_t needle_color(
    uint16_t base,
    int32_t dx,
    int32_t dy,
    int32_t sin_m,
    int32_t cos_m
)
{
    int32_t across       = ( dx * cos_m ) + ( dy * sin_m );
    int32_t magnitude    = ui_abs( across );
    int32_t amount       = ( magnitude * 4 ) / ( DIAL_NEEDLE_HALF_W * 1000 );
    int32_t normal_light = sin_m + cos_m;
    bool bright          = ( ( across < 0 ) && ( normal_light >= 0 ) )
                  || ( ( across > 0 ) && ( normal_light < 0 ) );
    uint16_t light       = bright ? 0xFFFFu : 0x0000u;
    int32_t illumination = ui_abs( normal_light );

    amount = ui_min( amount, 4 );
    /* Fade through zero as the blade turns across the light direction,
     * rather than swapping its highlight abruptly between the two edges. */
    amount = ( amount * illumination ) / 1414;
    amount = bright ? amount : ( amount / 2 );
    return font_blend( light, base, ( uint8_t )amount );
}

/* A part of the pointer blended over color: ink is how much of the pixel it
 * covers and base its color before shading. Nothing is shaded for a pixel it
 * does not touch. dx and dy are from the pivot. */
static uint16_t needle_layer(
    uint16_t color,
    uint8_t ink,
    uint16_t base,
    int32_t dx,
    int32_t dy,
    int32_t needle_sin,
    int32_t needle_cos
)
{
    uint16_t out = color;

    if ( ink != 0u )
    {
        out = font_blend(
            needle_color( base, dx, dy, needle_sin, needle_cos ), color, ink
        );
    }
    return out;
}

/* A round dial. Draw order, bottom up: face, ticks, numbers, screws, pointer,
 * pale collar and squared counterweight, then the stationary cap. */
static uint16_t round_pixel(
    const dial_cfg_t * cfg,
    const dial_face_t * face,
    int32_t needle_sin,
    int32_t needle_cos,
    int16_t row,
    int16_t col
)
{
    int32_t dx      = ( int32_t )col - DIAL_CX;
    int32_t dy      = ( int32_t )row - DIAL_CY;
    int32_t dist_sq = ( dx * dx ) + ( dy * dy );
    uint16_t color  = cfg->face_bg;
    uint8_t ink;

    color = ticks_layer( cfg, face, dx, dy, color );
    color = labels_layer( cfg, face, row, col, color );
    color = dial_screws( color, dx, dy, false );
    ink   = dial_needle_alpha(
        dx, dy, needle_sin, needle_cos, dial_needle_length( cfg )
    );
    color = needle_layer(
        color, ink, cfg->needle_fg, dx, dy, needle_sin, needle_cos
    );
    ink = dial_radial_line_alpha(
        dx,
        dy,
        needle_sin,
        needle_cos,
        0,
        DIAL_NEEDLE_COLLAR_R,
        DIAL_NEEDLE_HALF_W
    );
    color = needle_layer( color, ink, 0xDEFBu, dx, dy, needle_sin, needle_cos );
    ink   = dial_radial_line_alpha(
        dx, dy, -needle_sin, -needle_cos, 0, DIAL_TAIL_R, DIAL_TAIL_HALF_W
    );
    color = needle_layer( color, ink, 0xDEFBu, dx, dy, needle_sin, needle_cos );
    ink   = dial_disc_alpha( dist_sq, DIAL_HUB_R );
    if ( ink != 0u )
    {
        color = font_blend( hub_color( dy, dist_sq ), color, ink );
    }
    return color;
}

/* The arc style. Draw order, bottom up: face, ticks, numbers, needle, then
 * the cover plate over the needle's base, and the screws on its feet. The
 * cover is the face tinted a little toward the ink. */
static uint16_t arc_pixel(
    const dial_cfg_t * cfg,
    const dial_face_t * face,
    int32_t needle_sin,
    int32_t needle_cos,
    int16_t row,
    int16_t col
)
{
    int32_t dx         = ( int32_t )col - DIAL_CX;
    int32_t dy         = ( int32_t )row - DIAL_CY;
    int32_t from_pivot = dy - dial_pivot_down( cfg );
    uint16_t color     = cfg->face_bg;
    uint8_t ink;

    color = ticks_layer( cfg, face, dx, from_pivot, color );
    color = labels_layer( cfg, face, row, col, color );
    ink   = dial_needle_alpha(
        dx, from_pivot, needle_sin, needle_cos, dial_needle_length( cfg )
    );
    color = needle_layer(
        color, ink, cfg->needle_fg, dx, from_pivot, needle_sin, needle_cos
    );
    color = dial_cover_feet( cfg, color, dx, dy );
    color = dial_screws( color, dx, dy, true );
    if ( dy >= -( int32_t )DIAL_COVER_SHADOW_SPREAD )
    {
        color = dial_cover_body( cfg, color, dx, dy );
    }
    return color;
}

static uint16_t pixel_color(
    const dial_cfg_t * cfg,
    const dial_face_t * face,
    int32_t needle_sin,
    int32_t needle_cos,
    int16_t row,
    int16_t col
)
{
    return dial_is_arc( cfg )
               ? arc_pixel( cfg, face, needle_sin, needle_cos, row, col )
               : round_pixel( cfg, face, needle_sin, needle_cos, row, col );
}

/* Bounding box (x0 <= x < x1) of the needle shape at deg: the forward tip
 * at the needle's length, and on a round dial the counterweight tail
 * reaching DIAL_TAIL_R behind center. Both lie on the same line through the
 * pivot, so covering both endpoints covers the whole shape. The arc style
 * has no tail: its needle starts at the pivot, and the box stops where the
 * needle goes behind the cover, because nothing below that changes. */
static void needle_extent(
    const dial_cfg_t * cfg,
    int16_t deg,
    int16_t * x0,
    int16_t * y0,
    int16_t * x1,
    int16_t * y1
)
{
    int32_t sin_m;
    int32_t cos_m;
    int32_t back_r = dial_is_arc( cfg ) ? 0 : ( int32_t )DIAL_TAIL_R;
    int32_t fwd_x;
    int32_t fwd_y;
    int32_t back_x;
    int32_t back_y;
    int32_t min_x;
    int32_t max_x;
    int32_t min_y;
    int32_t max_y;
    int32_t pivot_y = DIAL_CY + dial_pivot_down( cfg );

    dial_angle_vec( deg, &sin_m, &cos_m );
    fwd_x  = ( dial_needle_length( cfg ) * sin_m ) / 1000;
    fwd_y  = -( dial_needle_length( cfg ) * cos_m ) / 1000;
    back_x = -( back_r * sin_m ) / 1000;
    back_y = ( back_r * cos_m ) / 1000;

    min_x = ui_min( fwd_x, back_x );
    max_x = ui_max( fwd_x, back_x );
    min_y = ui_min( fwd_y, back_y );
    max_y = ui_max( fwd_y, back_y );

    *x0 = ( int16_t )( ( DIAL_CX + min_x ) - DIAL_REGION_MARGIN );
    *y0 = ( int16_t )( ( pivot_y + min_y ) - DIAL_REGION_MARGIN );
    *x1 = ( int16_t )( ( DIAL_CX + max_x ) + DIAL_REGION_MARGIN + 1 );
    *y1 = ( int16_t )( ( pivot_y + max_y ) + DIAL_REGION_MARGIN + 1 );
    if ( dial_is_arc( cfg )
         && ( *y1 > ( DIAL_CY + DIAL_ARC_NEEDLE_SHOWS_TO ) ) )
    {
        *y1 = ( int16_t )( DIAL_CY + DIAL_ARC_NEEDLE_SHOWS_TO );
    }
}

/* Clips a box to the panel; x0/y0 may still exceed x1/y1 afterward if the
 * box fell entirely outside it (the caller checks that). */
static void clamp_box(
    int16_t * x0,
    int16_t * y0,
    int16_t * x1,
    int16_t * y1
)
{
    if ( *x0 < 0 )
    {
        *x0 = 0;
    }
    if ( *y0 < 0 )
    {
        *y0 = 0;
    }
    if ( *x1 > ( int16_t )DIAL_PANEL_W )
    {
        *x1 = ( int16_t )DIAL_PANEL_W;
    }
    if ( *y1 > ( int16_t )DIAL_PANEL_H )
    {
        *y1 = ( int16_t )DIAL_PANEL_H;
    }
}

/* Same pixel_color geometry as draw_frame, restricted to one rectangle and
 * blitted in a single call instead of one blit per row. Ticks/labels are
 * still built fresh (cheap: a couple dozen entries) so a tick or label
 * inside the box is drawn correctly; pixels outside the box are untouched
 * because nothing but the needle moved since the last redraw. */
/* Renders the dial into dst, row by row, for x0 <= x < x1 and y0 <= y < y1. */
static void fill_rect(
    const dial_widget_t * d,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t * dst
)
{
    int32_t needle_sin;
    int32_t needle_cos;
    size_t n = 0u;

    dial_angle_vec( d->displayed_deg, &needle_sin, &needle_cos );
    for ( int16_t row = y0; row < y1; row++ )
    {
        for ( int16_t col = x0; col < x1; col++ )
        {
            dst[n] = pixel_color(
                d->cfg, &d->face, needle_sin, needle_cos, row, col
            );
            n++;
        }
    }
}

/* The pixels of the box or the band being drawn, before they are sent. */
static uint16_t region_buf[DIAL_REGION_PIXELS];

/* One blit for the box around the needle's old and new place. */
static void draw_region(
    const dial_widget_t * d,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1
)
{
    int32_t width  = ( int32_t )x1 - ( int32_t )x0;
    int32_t height = ( int32_t )y1 - ( int32_t )y0;

    fill_rect( d, x0, y0, x1, y1, region_buf );
    ( void )display_blit(
        ( uint16_t )x0,
        ( uint16_t )y0,
        ( uint16_t )width,
        ( uint16_t )height,
        region_buf
    );
}

/* The whole face, in bands of rows. As many rows as the scratch buffer
 * holds go out in one blit, so the panel's window is set a few times for the
 * whole face and not once for every row. */
static void draw_frame( const dial_widget_t * d )
{
    uint32_t buffer_px = DIAL_REGION_PIXELS;
    uint32_t panel_w   = ( uint32_t )DIAL_PANEL_W;
    uint32_t band_rows = buffer_px / panel_w;
    int32_t band       = ( int32_t )band_rows;
    int32_t row        = 0;

    while ( row < ( int32_t )DIAL_PANEL_H )
    {
        int32_t end   = ui_min( row + band, ( int32_t )DIAL_PANEL_H );
        int32_t count = end - row;

        fill_rect(
            d,
            0,
            ( int16_t )row,
            ( int16_t )DIAL_PANEL_W,
            ( int16_t )end,
            region_buf
        );
        ( void )display_blit(
            0u,
            ( uint16_t )row,
            ( uint16_t )DIAL_PANEL_W,
            ( uint16_t )count,
            region_buf
        );
        row = end;
    }
}

bool dial_settled( const dial_widget_t * d )
{
    return dial_motion_reached( &d->motion, d->target_deg );
}

void dial_init(
    dial_widget_t * d,
    const dial_cfg_t * cfg,
    uint32_t now_ms
)
{
    d->cfg = cfg;
    dial_make_face( cfg, &d->face );
    d->displayed_deg = dial_rest_deg( cfg );
    d->target_deg    = d->displayed_deg;
    dial_motion_init( &d->motion, d->displayed_deg, now_ms );
    ( void )display_clear( cfg->face_bg );
    draw_frame( d );
}

/* Only the needle moved: redraw the union of where it was and where it is
 * now, not the whole face (see draw_region). A box bigger than the scratch
 * buffer can hold falls back to a full
 * redraw instead of clipping it. */
static void redraw_needle(
    const dial_widget_t * d,
    int16_t old_deg
)
{
    int16_t ox0;
    int16_t oy0;
    int16_t ox1;
    int16_t oy1;
    int16_t nx0;
    int16_t ny0;
    int16_t nx1;
    int16_t ny1;
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    int32_t width;
    int32_t height;
    uint32_t pixels;

    needle_extent( d->cfg, old_deg, &ox0, &oy0, &ox1, &oy1 );
    needle_extent( d->cfg, d->displayed_deg, &nx0, &ny0, &nx1, &ny1 );
    x0 = ( int16_t )ui_min( ox0, nx0 );
    y0 = ( int16_t )ui_min( oy0, ny0 );
    x1 = ( int16_t )ui_max( ox1, nx1 );
    y1 = ( int16_t )ui_max( oy1, ny1 );
    clamp_box( &x0, &y0, &x1, &y1 );
    width  = ( int32_t )x1 - ( int32_t )x0;
    height = ( int32_t )y1 - ( int32_t )y0;
    pixels = ( uint32_t )width * ( uint32_t )height;

    if ( ( x0 < x1 ) && ( y0 < y1 ) && ( pixels <= DIAL_REGION_PIXELS ) )
    {
        draw_region( d, x0, y0, x1, y1 );
    }
    else
    {
        draw_frame( d );
    }
}

void dial_poll(
    dial_widget_t * d,
    uint32_t now_ms,
    bool has_value,
    int32_t value
)
{
    int16_t old_deg = d->displayed_deg;

    d->target_deg = has_value ? dial_value_to_deg( d->cfg, value )
                              : dial_rest_deg( d->cfg );
    if ( dial_motion_poll(
             &d->motion,
             now_ms,
             d->target_deg,
             d->cfg->max_deg_per_s,
             &d->displayed_deg
         ) )
    {
        redraw_needle( d, old_deg );
    }
}
