/* Bench-only hook: not part of the product. See firmware/hil/. */
#ifndef HIL_TEST_H
#define HIL_TEST_H

#if defined( CONFIG_APP_HIL_TEST )
/* Call every loop. Reads the console for a command byte; 'H' hangs the
 * loop forever so a HIL rig can verify the watchdog recovers the unit. */
void hil_test_poll( void );
#else
static inline void hil_test_poll( void )
{
}
#endif

#endif /* HIL_TEST_H */
