/* SSM1 command encoders. Commands are always 4 bytes. */
#ifndef SSM1_CMD_H
#define SSM1_CMD_H

#include <stddef.h>
#include <stdint.h>

#define SSM1_CMD_LEN ( 4u )

/* 12 00 00 00 */
size_t ssm1_cmd_stop( uint8_t out[SSM1_CMD_LEN] );
/* 78 msb lsb 00. The 4th byte must be sent but its value is ignored. */
size_t ssm1_cmd_read_ecu(
    uint16_t addr,
    uint8_t out[SSM1_CMD_LEN]
);

/* 00 ff ff 00: ask for the ROM ID. Only answered while the ECU is
 * streaming (we stream address 0xFFFE first). */
size_t ssm1_cmd_romid( uint8_t out[SSM1_CMD_LEN] );
/* aa msb lsb data. Only takes effect while the ECU is streaming. */
size_t ssm1_cmd_write(
    uint16_t addr,
    uint8_t data,
    uint8_t out[SSM1_CMD_LEN]
);

#endif /* SSM1_CMD_H */
