/* Tiny test harness: no dependencies. */
#ifndef MINITEST_H
#define MINITEST_H

#include <stdio.h>

static unsigned mt_checks;
static unsigned mt_failures;

#define CHECK( cond )                                                      \
    do                                                                     \
    {                                                                      \
        mt_checks++;                                                       \
        if ( !( cond ) )                                                   \
        {                                                                  \
            mt_failures++;                                                 \
            printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond );       \
        }                                                                  \
    } while ( 0 )

#define CHECK_EQ( a, b ) CHECK( ( long long )( a ) == ( long long )( b ) )

/* Prints the summary; evaluates to nonzero if anything failed. */
#define MT_RESULT()                                                        \
    ( printf( "%u checks, %u failures\n", mt_checks, mt_failures ),        \
      mt_failures != 0 )

#endif /* MINITEST_H */
