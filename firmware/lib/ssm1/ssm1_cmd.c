#include "ssm1_cmd.h"

#define CMD_STOP     ( 0x12u )
#define CMD_READ_ECU ( 0x78u )
#define CMD_WRITE    ( 0xAAu )

size_t ssm1_cmd_stop( uint8_t out[SSM1_CMD_LEN] )
{
    out[0] = CMD_STOP;
    out[1] = 0x00u;
    out[2] = 0x00u;
    out[3] = 0x00u;
    return SSM1_CMD_LEN;
}

size_t ssm1_cmd_read_ecu(
    uint16_t addr,
    uint8_t out[SSM1_CMD_LEN]
)
{
    out[0] = CMD_READ_ECU;
    out[1] = ( uint8_t )( addr >> 8 );
    out[2] = ( uint8_t )( addr & 0xFFu );
    out[3] = 0x00u;
    return SSM1_CMD_LEN;
}

size_t ssm1_cmd_romid( uint8_t out[SSM1_CMD_LEN] )
{
    out[0] = 0x00u;
    out[1] = 0xFFu;
    out[2] = 0xFFu;
    out[3] = 0x00u;
    return SSM1_CMD_LEN;
}

size_t ssm1_cmd_write(
    uint16_t addr,
    uint8_t data,
    uint8_t out[SSM1_CMD_LEN]
)
{
    out[0] = CMD_WRITE;
    out[1] = ( uint8_t )( addr >> 8u );
    out[2] = ( uint8_t )( addr & 0xFFu );
    out[3] = data;
    return SSM1_CMD_LEN;
}
