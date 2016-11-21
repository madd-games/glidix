/* $Id$ */

/* islower( int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <ctype.h>

#ifndef REGTEST

int islower( int c )
{
	return (c >= 'a') && (c <= 'z');
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    TESTCASE( islower( 'a' ) );
    TESTCASE( islower( 'z' ) );
    TESTCASE( ! islower( 'A' ) );
    TESTCASE( ! islower( 'Z' ) );
    TESTCASE( ! islower( ' ' ) );
    TESTCASE( ! islower( '@' ) );
    return TEST_RESULTS;
}

#endif
