/*
	Glidix Runtime

	Copyright (c) 2014-2015, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>

double strtod(const char *nptr, char **endptr)
{
    double result = 0.0;
    char signedResult = '\0';
    char signedExponent = '\0';
    int decimals = 0;
    bool isExponent = false;
    bool hasExponent = false;
    bool hasResult = false;
    // exponent is logically int but is coded as double so that its eventual
    // overflow detection can be the same as for double result
    double exponent = 0;
    char c;
    for (; '\0' != (c = *str); ++str)
    {
        if ((c >= '0') && (c <= '9'))
        {
            int digit = c - '0';
            if (isExponent)
            {
                exponent = (10 * exponent) + digit;
                hasExponent = true;
            }
            else if (decimals == 0)
            {
                result = (10 * result) + digit;
                hasResult = true;
            }
            else
            {
                result += (double)digit / decimals;
                decimals *= 10;
            }
            continue;
        }
        if (c == '.')
        {
            if (!hasResult)
            {
                // don't allow leading '.'
                break;
            }
            if (isExponent)
            {
                // don't allow decimal places in exponent
                break;
            }
            if (decimals != 0)
            {
                // this is the 2nd time we've found a '.'
                break;
            }
            decimals = 10;
            continue;
        }
        if ((c == '-') || (c == '+'))
        {
            if (isExponent)
            {
                if (signedExponent || (exponent != 0))
                    break;
                else
                    signedExponent = c;
            }
            else
            {
                if (signedResult || (result != 0))
                    break;
                else
                    signedResult = c;
            }
            continue;
        }
        if (c == 'E')
        {
            if (!hasResult)
            {
                // don't allow leading 'E'
                break;
            }
            if (isExponent)
                break;
            else
                isExponent = true;
            continue;
        }
        // else unexpected character
        break;
    }
    if (isExponent && !hasExponent)
    {
        while (*str != 'E')
            --str;
    }
    if (!hasResult && signedResult)
        --str;

    if (endptr)
        *endptr = const_cast<char*>(str);
    for (; exponent != 0; --exponent)
    {
        if (signedExponent == '-')
            result /= 10;
        else
            result *= 10;
    }
    if (signedResult == '-')
    {
        if (result != 0)
            result = -result;
        // else I'm not used to working with double-precision numbers so I
        // was surprised to find my assert for "-0" failing, saying -0 != +0.
    }
    return result;
};
