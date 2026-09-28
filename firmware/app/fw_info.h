/* This unit's exact firmware identity, for support/diagnostics -- so
 * "what version is this" has one place to ask, both for the boot-time
 * log line (see main.c) and for anything later that wants to show it
 * at runtime (e.g. the gauge screen's corner tag). Comes from
 * firmware/app/VERSION via Zephyr's generated app_version.h. */
#ifndef FW_INFO_H
#define FW_INFO_H

typedef struct
{
    const char * version; /* e.g. "0.1.0"; see firmware/app/VERSION */
} fw_info_t;

extern const fw_info_t fw_info;

#endif /* FW_INFO_H */
