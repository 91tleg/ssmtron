/* GPIOs come from the board's devicetree: /zephyr,user { hal-gpios = <...>;
 * } */
#include "hal_gpio.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define USER_NODE           DT_PATH( zephyr_user )
#define HAL_GPIO_COUNT      DT_PROP_LEN( USER_NODE, hal_gpios )
#define GPIO_SPEC( idx, _ ) GPIO_DT_SPEC_GET_BY_IDX( USER_NODE, hal_gpios, idx )

static const struct gpio_dt_spec gpios[] = {
    LISTIFY( HAL_GPIO_COUNT, GPIO_SPEC, (, ) )
};

typedef struct
{
    struct gpio_callback cb;
    volatile bool level;
    volatile bool pending;
    volatile uint32_t timestamp_ms;
} watch_t;

static watch_t watches[HAL_GPIO_COUNT];

hal_status_t hal_gpio_open(
    hal_gpio_id_t id,
    hal_gpio_mode_t mode
)
{
    hal_status_t status = HAL_ERR_INVALID;

    if ( id < HAL_GPIO_COUNT )
    {
        if ( !gpio_is_ready_dt( &gpios[id] ) )
        {
            status = HAL_ERR_IO;
        }
        else
        {
            gpio_flags_t flags =
                mode == HAL_GPIO_INPUT ? GPIO_INPUT : GPIO_OUTPUT_INACTIVE;

            status = gpio_pin_configure_dt( &gpios[id], flags ) ? HAL_ERR_IO
                                                                : HAL_OK;
        }
    }
    return status;
}

bool hal_gpio_read( hal_gpio_id_t id )
{
    return ( id < HAL_GPIO_COUNT ) && ( gpio_pin_get_dt( &gpios[id] ) > 0 );
}

void hal_gpio_write(
    hal_gpio_id_t id,
    bool asserted
)
{
    if ( id < HAL_GPIO_COUNT )
    {
        /* hal_gpio_write is fire-and-forget; a failure here has
         * nowhere to go. */
        ( void )gpio_pin_set_dt( &gpios[id], asserted );
    }
}

/* Each callback watches one pin, so the pin mask needs no check. */
static void on_gpio_interrupt(
    const struct device * port,
    struct gpio_callback * cb,
    uint32_t pins
)
{
    ( void )port;
    ( void )pins;
    for ( hal_gpio_id_t id = 0; id < HAL_GPIO_COUNT; id++ )
    {
        if ( &watches[id].cb == cb )
        {
            watches[id].level        = gpio_pin_get_dt( &gpios[id] ) > 0;
            watches[id].timestamp_ms = k_uptime_get_32();
            watches[id].pending      = true;
            break;
        }
    }
}

hal_status_t hal_gpio_watch( hal_gpio_id_t id )
{
    hal_status_t status = HAL_ERR_INVALID;

    if ( id < HAL_GPIO_COUNT )
    {
        gpio_init_callback(
            &watches[id].cb, on_gpio_interrupt, BIT( gpios[id].pin )
        );
        status = ( ( gpio_add_callback( gpios[id].port, &watches[id].cb ) == 0 )
                   && ( gpio_pin_interrupt_configure_dt(
                            &gpios[id], GPIO_INT_EDGE_BOTH
                        )
                        == 0 ) )
                     ? HAL_OK
                     : HAL_ERR_IO;
    }
    return status;
}

bool hal_gpio_take_edge(
    hal_gpio_id_t id,
    bool * level,
    uint32_t * timestamp_ms
)
{
    bool had_edge = false;

    if ( id < HAL_GPIO_COUNT )
    {
        /* Read and clear the latch atomically with respect to the ISR. */
        unsigned int key = irq_lock();

        if ( watches[id].pending )
        {
            *level              = watches[id].level;
            *timestamp_ms       = watches[id].timestamp_ms;
            watches[id].pending = false;
            had_edge            = true;
        }
        irq_unlock( key );
    }
    return had_edge;
}
