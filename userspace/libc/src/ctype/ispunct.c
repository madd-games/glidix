/* $Id$ */

/* ispunct( int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <ctype.h>

#ifndef REGTEST

int ispunct( int c )
{
    return isgraph(c) && !isalnum(c);
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    TESTCASE( ! ispunct( 'a' ) );
    TESTCASE( ! ispunct( 'z' ) );
    TESTCASE( ! ispunct( 'A' ) );
    TESTCASE( ! ispunct( 'Z' ) );
    TESTCASE( ispunct( '@' ) );
    TESTCASE( ispunct( '.' ) );
    TESTCASE( ! ispunct( '\t' ) );
    TESTCASE( ! ispunct( '\0' ) );
    TESTCASE( ! ispunct( ' ' ) );
    return TEST_RESULTS;
}

#endif
