#ifndef HAL_STATUS_H
#define HAL_STATUS_H

typedef enum
{
    HAL_OK = 0u,
    HAL_ERR_INVALID,     /* bad id or argument */
    HAL_ERR_UNSUPPORTED, /* chip cannot do the requested config */
    HAL_ERR_IO,
} hal_status_t;

#endif /* HAL_STATUS_H */
