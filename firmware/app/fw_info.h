/* Firmware identity from VERSION via Zephyr's generated app_version.h. */
#ifndef FW_INFO_H
#define FW_INFO_H

typedef struct
{
    const char * version; /* e.g. "0.1.0"; see firmware/app/VERSION */
} fw_info_t;

extern const fw_info_t fw_info;

#endif /* FW_INFO_H */
