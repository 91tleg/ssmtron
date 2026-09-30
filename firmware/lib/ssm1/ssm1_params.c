#include "ssm1_params.h"

/* Addresses and scalings are for the 1990-94 Legacy ECUs. Verify against a
 * real car before shipping. */

/* Leave thresholds unset until values are verified on a real car. */
#define LIN( n, u, num, den, off, dec )                                        \
    { ( n ),                                                                   \
      ( u ),                                                                   \
      NULL,                                                                    \
      ECU_DEC_LINEAR,                                                          \
      ( num ),                                                                 \
      ( den ),                                                                 \
      ( off ),                                                                 \
      ( dec ),                                                                 \
      NULL,                                                                    \
      NULL,                                                                    \
      0,                                                                       \
      false,                                                                   \
      0,                                                                       \
      0,                                                                       \
      false }
#define FLAGS( n, arr )                                                        \
    { ( n ),                                                                   \
      "",                                                                      \
      NULL,                                                                    \
      ECU_DEC_FLAGS,                                                           \
      1,                                                                       \
      1,                                                                       \
      0,                                                                       \
      0,                                                                       \
      NULL,                                                                    \
      ( arr ),                                                                 \
      ( uint8_t )( sizeof( arr ) / sizeof( ( arr )[0] ) ),                     \
      false,                                                                   \
      0,                                                                       \
      0,                                                                       \
      false }

/* The same, for the bytes whose bits are trouble codes. */
#define CODES( n, arr )                                                        \
    { ( n ),                                                                   \
      "",                                                                      \
      NULL,                                                                    \
      ECU_DEC_FLAGS,                                                           \
      1,                                                                       \
      1,                                                                       \
      0,                                                                       \
      0,                                                                       \
      NULL,                                                                    \
      ( arr ),                                                                 \
      ( uint8_t )( sizeof( arr ) / sizeof( ( arr )[0] ) ),                     \
      false,                                                                   \
      0,                                                                       \
      0,                                                                       \
      true }

/* ---- flag bits --------------------------------------------------------
 */

static const ecu_flag_t sw0_hitachi_n[] = {
    { "Ignition", 0x80u, true },
    { "Auto Trans", 0x40u, true },
    { "Test Mode", 0x20u, false },
    { "Read Mode", 0x10u, false },
    { "Neutral", 0x04u, false },
    { "Park", 0x02u, true },
    { "California", 0x01u, false },
};
static const ecu_flag_t sw0_hitachi_t[] = {
    { "Ignition", 0x80u, true },
    { "Auto Trans", 0x40u, true },
    { "Test Mode", 0x20u, false },
    { "Read Mode", 0x10u, false },
    { "Neutral", 0x04u, false },
    { "Park", 0x02u, false },
    { "California", 0x01u, false },
};
static const ecu_flag_t sw0_jecs1[] = {
    { "Ignition", 0x80u, true },
    { "Auto Trans", 0x40u, true },
    { "Test Mode", 0x20u, true },
    { "Read Mode", 0x10u, true },
    { "Neutral", 0x04u, true },
    { "Park", 0x02u, true },
    { "California", 0x01u, false },
};
static const ecu_flag_t sw0_jecs2[] = {
    { "Ignition", 0x80u, true },
    { "Auto Trans", 0x40u, true },
    { "Test Mode", 0x20u, true },
    { "Read Mode", 0x10u, true },
    { "Neutral", 0x04u, false },
    { "Park", 0x02u, true },
    { "California", 0x01u, false },
};
static const ecu_flag_t sw1_na[] = {
    { "Idle Switch", 0x80u, true },
    { "A/C Switch", 0x40u, true },
    { "A/C Relay", 0x20u, true },
    { "Rad Fan", 0x10u, true },
    { "Fuel Pump", 0x08u, true },
    { "Purge Valve", 0x04u, true },
    { "Pinging", 0x02u, true },
};
static const ecu_flag_t sw1_turbo[] = {
    { "Idle Switch", 0x80u, true },
    { "A/C Switch", 0x40u, true },
    { "A/C Relay", 0x20u, true },
    { "Rad Fan", 0x10u, true },
    { "Fuel Pump", 0x08u, true },
    { "Purge Valve", 0x04u, true },
    { "Pinging", 0x02u, true },
    { "Press Exch", 0x01u, true },
};

/* Trouble code bits: set means the code is active/stored. */
static const ecu_flag_t dtc1[] = {
    { "11 Crank", 0x01u, true },
    { "12 Start Sw", 0x02u, true },
    { "13 Cam", 0x04u, true },
    { "14 Inj #1", 0x08u, true },
    { "15 Inj #2", 0x10u, true },
    { "16 Inj #3", 0x20u, true },
    { "17 Inj #4", 0x40u, true },
};
static const ecu_flag_t dtc2[] = {
    { "21 Temp", 0x01u, true },
    { "22 Knock", 0x02u, true },
    { "23 MAF", 0x04u, true },
    { "24 IAC", 0x08u, true },
    { "31 TPS", 0x10u, true },
    { "32 Oxygen", 0x20u, true },
    { "33 VSS", 0x40u, true },
    { "35 Purge", 0x80u, true },
};
static const ecu_flag_t dtc3_hitachi_n[] = {
    { "41 FuelTrim", 0x01u, true },
    { "42 Idle Sw", 0x02u, true },
    { "45 Baro", 0x10u, true },
    { "49 WrongMAF", 0x20u, true },
    { "51 NeutSw", 0x40u, true },
    { "52 ParkSw", 0x80u, true },
};
static const ecu_flag_t dtc3_turbo[] = {
    { "41 FuelTrim", 0x01u, true },
    { "42 Idle Sw", 0x02u, true },
    { "44 WGC", 0x08u, true },
    { "45 Baro", 0x10u, true },
    { "49 WrongMAF", 0x20u, true },
    { "51 NeutSw", 0x40u, true },
    { "52 ParkSw", 0x80u, true },
};
static const ecu_flag_t dtc3_jecs[] = {
    { "41 FuelTrim", 0x01u, true },
    { "42 Idle Sw", 0x02u, true },
    { "49 WrongMAF", 0x20u, true },
    { "51 NeutSw", 0x40u, true },
    { "52 ParkSw", 0x80u, true },
};

/* ---- coolant lookup (Hitachi ECUs), deg F -----------------------------
 */

static const int16_t coolant_hitachi[256] = {
    401, 387, 372, 360, 345, 331, 318, 303, 291, 284, 277, 270, 264, 257, 250,
    243, 237, 234, 230, 226, 223, 219, 216, 212, 208, 205, 203, 199, 198, 194,
    192, 189, 187, 185, 183, 181, 180, 178, 176, 174, 172, 171, 169, 167, 165,
    163, 162, 160, 158, 156, 154, 153, 153, 151, 149, 147, 147, 145, 144, 144,
    142, 140, 140, 138, 138, 136, 135, 135, 133, 131, 131, 129, 129, 127, 127,
    126, 126, 124, 124, 122, 122, 120, 120, 118, 118, 117, 117, 115, 115, 113,
    113, 111, 111, 109, 109, 108, 108, 106, 106, 104, 104, 102, 102, 100, 100,
    99,  99,  97,  97,  97,  95,  95,  95,  93,  93,  91,  91,  90,  90,  88,
    88,  86,  86,  84,  84,  84,  82,  82,  82,  81,  81,  79,  79,  79,  77,
    77,  77,  75,  75,  73,  73,  73,  72,  72,  72,  70,  70,  68,  68,  68,
    66,  66,  66,  64,  64,  63,  63,  61,  61,  59,  59,  57,  57,  55,  55,
    55,  54,  54,  54,  52,  52,  50,  50,  50,  48,  48,  48,  46,  46,  45,
    45,  43,  43,  41,  41,  39,  39,  37,  37,  37,  36,  36,  36,  34,  34,
    32,  32,  30,  30,  28,  28,  27,  27,  25,  25,  23,  23,  21,  21,  19,
    18,  18,  16,  14,  14,  12,  12,  10,  9,   9,   7,   5,   5,   3,   3,
    1,   0,   -2,  -4,  -6,  -8,  -9,  -9,  -11, -13, -15, -17, -18, -20, -22,
    -24, -27, -31, -33, -36, -40, -42, -45, -47, -47, -47, -47, -47, -47, -47,
    -47
};

/* ---- decoders ---------------------------------------------------------
 */

static const ecu_param_info_t VB = LIN( "System Voltage", "V", 8, 1, 0, 2 );
static const ecu_param_info_t VSP_H =
    LIN( "Vehicle Speed", "mph", 10, 16, 0, 1 );
static const ecu_param_info_t VSP_J =
    LIN( "Vehicle Speed", "mph", 125, 1, 0, 2 );
static const ecu_param_info_t RPM  = LIN( "Engine Speed", "rpm", 25, 1, 0, 0 );
static const ecu_param_info_t TW_H = {
    "Coolant Temp",
    "F",
    NULL,
    ECU_DEC_LOOKUP,
    1,
    1,
    0,
    0,
    coolant_hitachi,
    NULL,
    0,
    false,
    0,
    0,
    false
};
static const ecu_param_info_t TW_J = LIN( "Coolant Temp", "F", 18, 1, -580, 1 );
static const ecu_param_info_t ADVS =
    LIN( "Ignition Timing", "BTDC", 1, 1, 0, 0 );
static const ecu_param_info_t QA   = LIN( "Airflow Signal", "V", 2, 1, 0, 2 );
static const ecu_param_info_t LOAD = LIN( "Load", "", 1, 1, 0, 0 );
static const ecu_param_info_t THV  = LIN( "Throttle Signal", "V", 2, 1, 0, 2 );
static const ecu_param_info_t TPS  = LIN( "Throttle Angle", "deg", 5, 1, 0, 1 );
static const ecu_param_info_t TIM_N =
    LIN( "Inj Pulse Width", "ms", 2048, 1, 0, 3 );
static const ecu_param_info_t TIM_T =
    LIN( "Inj Pulse Width", "ms", 128, 1, 0, 3 );
static const ecu_param_info_t TIM_J =
    LIN( "Inj Pulse Width", "ms", 256, 1, 0, 3 );
static const ecu_param_info_t ISC_H = LIN( "IAC Duty", "%", 5, 1, 0, 1 );
static const ecu_param_info_t ISC_J = LIN( "IAC Duty", "%", 1000, 255, 0, 1 );
static const ecu_param_info_t O2_H  = LIN( "Oxygen Sensor", "V", 1, 1, 0, 2 );
static const ecu_param_info_t O2_J2 = LIN( "Oxygen Sensor", "V", 2, 1, 0, 2 );
static const ecu_param_info_t TRIM =
    LIN( "Fuel Trim", "%", 10000, 128, -10000, 2 );
static const ecu_param_info_t RTRD_H = {
    "Timing Correction",
    "deg",
    NULL,
    ECU_DEC_SIGNED,
    1,
    1,
    0,
    0,
    NULL,
    NULL,
    0,
    false,
    0,
    0,
    false
};
static const ecu_param_info_t RTRD_J =
    LIN( "Timing Correction", "deg", 25, 1, -3200, 2 );
static const ecu_param_info_t WGC =
    LIN( "Boost Control Duty", "%", 10000, 256, 0, 2 );
static const ecu_param_info_t BARO_N =
    LIN( "Barometric Pressure", "torr", -309, 1, 93000, 2 );
static const ecu_param_info_t BARO_T =
    LIN( "Barometric Pressure", "torr", 125, 1, 50000, 2 );
static const ecu_param_info_t BARO_J =
    LIN( "Barometric Pressure", "torr", 8, 1, 0, 0 );
static const ecu_param_info_t MANIP = {
    "Boost/Vacuum",
    "psi",
    "inHg",
    ECU_DEC_BOOST,
    1,
    1,
    0,
    3,
    NULL,
    NULL,
    0,
    false,
    0,
    0,
    false
};
static const ecu_param_info_t FA0_HN = FLAGS( "Input Switches", sw0_hitachi_n );
static const ecu_param_info_t FA0_HT = FLAGS( "Input Switches", sw0_hitachi_t );
static const ecu_param_info_t FA0_J1 = FLAGS( "Input Switches", sw0_jecs1 );
static const ecu_param_info_t FA0_J2 = FLAGS( "Input Switches", sw0_jecs2 );
static const ecu_param_info_t FA1_N  = FLAGS( "I/O Switches", sw1_na );
static const ecu_param_info_t FA1_T  = FLAGS( "I/O Switches", sw1_turbo );
static const ecu_param_info_t U1     = CODES( "Active Codes 1/3", dtc1 );
static const ecu_param_info_t U2     = CODES( "Active Codes 2/3", dtc2 );
static const ecu_param_info_t U3_HN =
    CODES( "Active Codes 3/3", dtc3_hitachi_n );
static const ecu_param_info_t U3_T = CODES( "Active Codes 3/3", dtc3_turbo );
static const ecu_param_info_t U3_J = CODES( "Active Codes 3/3", dtc3_jecs );
static const ecu_param_info_t M1   = CODES( "Stored Codes 1/3", dtc1 );
static const ecu_param_info_t M2   = CODES( "Stored Codes 2/3", dtc2 );
static const ecu_param_info_t M3_HN =
    CODES( "Stored Codes 3/3", dtc3_hitachi_n );
static const ecu_param_info_t M3_T = CODES( "Stored Codes 3/3", dtc3_turbo );
static const ecu_param_info_t M3_J = CODES( "Stored Codes 3/3", dtc3_jecs );

/* ---- profiles ---------------------------------------------------------
 */

/* 91-94 turbo (ROM 3672) */
static const ssm1_param_t turbo[] = {
    { &VB, 0x1404u },     { &VSP_H, 0x154Bu },  { &RPM, 0x140Bu },
    { &TW_H, 0x1405u },   { &ADVS, 0x1489u },   { &QA, 0x1400u },
    { &LOAD, 0x1414u },   { &THV, 0x1487u },    { &TPS, 0x1453u },
    { &TIM_T, 0x15F0u },  { &ISC_H, 0x158Au },  { &O2_H, 0x1403u },
    { &TRIM, 0x1488u },   { &RTRD_H, 0x1530u }, { &WGC, 0x144Du },
    { &BARO_T, 0x1516u }, { &MANIP, 0x00BEu },  { &FA0_HT, 0x15A8u },
    { &FA1_T, 0x15A9u },  { &U1, 0x0047u },     { &U2, 0x0048u },
    { &U3_T, 0x0049u },   { &M1, 0x1664u },     { &M2, 0x1665u },
    { &M3_T, 0x1666u },
};

/* 91 turbo (ROM 3671): alternate pulse width, other stored-code block */
static const ssm1_param_t turbo_early[] = {
    { &VB, 0x1404u },     { &VSP_H, 0x154Bu },  { &RPM, 0x140Bu },
    { &TW_H, 0x1405u },   { &ADVS, 0x1489u },   { &QA, 0x1400u },
    { &LOAD, 0x1414u },   { &THV, 0x1487u },    { &TPS, 0x1453u },
    { &TIM_N, 0x142Au },  { &ISC_H, 0x158Au },  { &O2_H, 0x1403u },
    { &TRIM, 0x1488u },   { &RTRD_H, 0x1530u }, { &WGC, 0x144Du },
    { &BARO_T, 0x1516u }, { &MANIP, 0x00BEu },  { &FA0_HT, 0x15A8u },
    { &FA1_T, 0x15A9u },  { &U1, 0x0047u },     { &U2, 0x0048u },
    { &U3_T, 0x0049u },   { &M1, 0x1604u },     { &M2, 0x1605u },
    { &M3_T, 0x1606u },
};

/* 90-91 5MT NA, Hitachi (ROM 3170) */
static const ssm1_param_t hitachi_na[] = {
    { &VB, 0x1404u },     { &VSP_H, 0x154Bu },  { &RPM, 0x140Bu },
    { &TW_H, 0x1405u },   { &ADVS, 0x1489u },   { &QA, 0x1400u },
    { &LOAD, 0x1414u },   { &THV, 0x1487u },    { &TPS, 0x1453u },
    { &TIM_N, 0x142Au },  { &ISC_H, 0x158Au },  { &O2_H, 0x1403u },
    { &TRIM, 0x1488u },   { &RTRD_H, 0x1530u }, { &BARO_N, 0x140Au },
    { &FA0_HN, 0x15A8u }, { &FA1_N, 0x15A9u },  { &U1, 0x0047u },
    { &U2, 0x0048u },     { &U3_HN, 0x0049u },  { &M1, 0x1604u },
    { &M2, 0x1605u },     { &M3_HN, 0x1606u },
};

/* 90-91 4EAT NA, JECS (ROM 3270) */
static const ssm1_param_t jecs1[] = {
    { &VB, 0x4780u },     { &VSP_J, 0x4781u },  { &RPM, 0x43BCu },
    { &TW_J, 0x4782u },   { &ADVS, 0x43C8u },   { &QA, 0x43ADu },
    { &LOAD, 0x43AAu },   { &THV, 0x4784u },    { &TIM_J, 0x43ABu },
    { &ISC_J, 0x43E3u },  { &O2_H, 0x43CFu },   { &TRIM, 0x43CEu },
    { &RTRD_J, 0x440Du }, { &BARO_J, 0x4787u }, { &FA0_J1, 0x4789u },
    { &FA1_N, 0x478Au },  { &U1, 0x4407u },     { &U2, 0x4406u },
    { &U3_J, 0x4405u },   { &M1, 0x440Au },     { &M2, 0x4409u },
    { &M3_J, 0x4408u },
};

/* 92 NA, JECS (ROM 3272) */
static const ssm1_param_t jecs2[] = {
    { &VB, 0x1335u },     { &VSP_J, 0x1336u },  { &RPM, 0x1338u },
    { &TW_J, 0x1337u },   { &ADVS, 0x1323u },   { &QA, 0x1307u },
    { &LOAD, 0x1305u },   { &THV, 0x1329u },    { &TIM_J, 0x1306u },
    { &ISC_J, 0x1314u },  { &O2_J2, 0x1310u },  { &TRIM, 0x133Eu },
    { &RTRD_J, 0x1328u }, { &BARO_J, 0x1340u }, { &FA0_J2, 0x1343u },
    { &FA1_N, 0x1344u },  { &U1, 0x1348u },     { &U2, 0x1347u },
    { &U3_J, 0x1346u },   { &M1, 0x134Bu },     { &M2, 0x134Au },
    { &M3_J, 0x1349u },
};

/* 93-94 NA, JECS (ROM 3273): as 92 but no barometric pressure */
static const ssm1_param_t jecs3[] = {
    { &VB, 0x1335u },     { &VSP_J, 0x1336u },  { &RPM, 0x1338u },
    { &TW_J, 0x1337u },   { &ADVS, 0x1323u },   { &QA, 0x1307u },
    { &LOAD, 0x1305u },   { &THV, 0x1329u },    { &TIM_J, 0x1306u },
    { &ISC_J, 0x1314u },  { &O2_J2, 0x1310u },  { &TRIM, 0x133Eu },
    { &RTRD_J, 0x1328u }, { &FA0_J2, 0x1343u }, { &FA1_N, 0x1344u },
    { &U1, 0x1348u },     { &U2, 0x1347u },     { &U3_J, 0x1346u },
    { &M1, 0x134Bu },     { &M2, 0x134Au },     { &M3_J, 0x1349u },
};

#define COUNT( a ) ( sizeof( a ) / sizeof( ( a )[0] ) )
#define PROFILE( id, nm, arr, clr, mode )                                      \
    { ( id ), ( nm ), ( arr ), COUNT( arr ), ( clr ), ( mode ) }

static const ssm1_profile_t profiles[] = {
    PROFILE( 0x3671u, "91 Turbo", turbo_early, 0x15F0u, SSM1_CLEAR_INVERT ),
    PROFILE( 0x3672u, "91-94 Turbo", turbo, 0x1650u, SSM1_CLEAR_ZERO ),
    PROFILE( 0x3170u, "90-91 5MT NA", hitachi_na, 0x1600u, SSM1_CLEAR_INVERT ),
    PROFILE( 0x3270u, "90-91 4EAT NA", jecs1, 0x443Fu, SSM1_CLEAR_INVERT ),
    PROFILE( 0x3272u, "92 NA", jecs2, 0x135Eu, SSM1_CLEAR_ZERO ),
    PROFILE( 0x3273u, "93-94 NA", jecs3, 0x135Eu, SSM1_CLEAR_ZERO ),
};

#define PROFILE_COUNT COUNT( profiles )

/* Negative array sizes catch profiles larger than SSM1_PARAM_MAX at
 * compile time. Explicit typedefs avoid token-pasting MISRA deviations. */
/* cppcheck-suppress misra-c2012-2.3 */
typedef char fit_turbo[( COUNT( turbo ) <= SSM1_PARAM_MAX ) ? 1 : -1];
/* cppcheck-suppress misra-c2012-2.3 */
typedef char
    fit_turbo_early[( COUNT( turbo_early ) <= SSM1_PARAM_MAX ) ? 1 : -1];
/* cppcheck-suppress misra-c2012-2.3 */
typedef char fit_hitachi_na[( COUNT( hitachi_na ) <= SSM1_PARAM_MAX ) ? 1 : -1];
/* cppcheck-suppress misra-c2012-2.3 */
typedef char fit_jecs1[( COUNT( jecs1 ) <= SSM1_PARAM_MAX ) ? 1 : -1];
/* cppcheck-suppress misra-c2012-2.3 */
typedef char fit_jecs2[( COUNT( jecs2 ) <= SSM1_PARAM_MAX ) ? 1 : -1];
/* cppcheck-suppress misra-c2012-2.3 */
typedef char fit_jecs3[( COUNT( jecs3 ) <= SSM1_PARAM_MAX ) ? 1 : -1];

const ssm1_profile_t * ssm1_profile_find( uint16_t rom_id )
{
    const ssm1_profile_t * found = NULL;

    for ( size_t i = 0; ( found == NULL ) && ( i < PROFILE_COUNT ); i++ )
    {
        if ( profiles[i].rom_id == rom_id )
        {
            found = &profiles[i];
        }
    }
    return found;
}

/* cppcheck-suppress misra-c2012-8.7 ; public API (ssm1_params.h), called
 * from test_ssm1_params.c, which this scoped MISRA scan does not cover.
 */
size_t ssm1_profile_count( void )
{
    return PROFILE_COUNT;
}

const ssm1_profile_t * ssm1_profile( size_t idx )
{
    return ( idx < PROFILE_COUNT ) ? &profiles[idx] : NULL;
}
