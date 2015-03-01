/* $Id$ */

/* isgraph( int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <ctype.h>

#ifndef REGTEST

int isgraph( int c )
{
	return isprint(c) && (c != ' ');
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    TESTCASE( isgraph( 'a' ) );
    TESTCASE( isgraph( 'z' ) );
    TESTCASE( isgraph( 'A' ) );
    TESTCASE( isgraph( 'Z' ) );
    TESTCASE( isgraph( '@' ) );
    TESTCASE( ! isgraph( '\t' ) );
    TESTCASE( ! isgraph( '\0' ) );
    TESTCASE( ! isgraph( ' ' ) );
    return TEST_RESULTS;
}

#endif
