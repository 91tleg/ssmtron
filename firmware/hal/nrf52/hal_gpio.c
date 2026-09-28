/* GPIOs come from the board's devicetree: /zephyr,user { hal-gpios = <...>;
 * } */
#include "hal_gpio.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#define USER_NODE           DT_PATH( zephyr_user )
#define HAL_GPIO_COUNT      DT_PROP_LEN( USER_NODE, hal_gpios )
#define GPIO_SPEC( idx, _ ) GPIO_DT_SPEC_GET_BY_IDX( USER_NODE, hal_gpios, idx )

static const struct gpio_dt_spec gpios[] = {
    LISTIFY( HAL_GPIO_COUNT, GPIO_SPEC, (, ) )
};

hal_status_t hal_gpio_open(
    hal_gpio_id_t id,
    hal_gpio_mode_t mode
)
{
    hal_status_t status = HAL_ERR_INVALID;

    if ( id < HAL_GPIO_COUNT )
    {
        /* Matches hal_uart_open: a bad id is the caller's mistake
         * (HAL_ERR_INVALID); a device that is not ready is the
         * hardware's (HAL_ERR_IO). */
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
