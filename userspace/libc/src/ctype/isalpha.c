/* $Id$ */

/* isalpha( int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <ctype.h>

#ifndef REGTEST

int isalpha( int c )
{
    return isupper(c) || islower(c);
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    TESTCASE( isalpha( 'a' ) );
    TESTCASE( isalpha( 'z' ) );
    TESTCASE( ! isalpha( ' ' ) );
    TESTCASE( ! isalpha( '1' ) );
    TESTCASE( ! isalpha( '@' ) );
    return TEST_RESULTS;
}

#endif
