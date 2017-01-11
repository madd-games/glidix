/*
	Glidix Runtime

	Copyright (c) 2014-2017, Madd Games.
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

#ifndef _M_H
#define _M_H

/**
 * NOTE:
 * The implementation of the math library, at this point, is ABSOLUTELY TERRIBLE.
 * Replacing the library with a better implementation is a TODO.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define	M_E							2.71828182846
#define	M_LOG2E							1.44269504089
#define	M_LOG10E						0.43429448190
#define	M_LN2							0.69314718056
#define	M_LN10							2.30258509299
#define	M_PI							3.14159265359
#define	M_PI_2							1.57079632679
#define	M_PI_4							0.78539816339
#define	M_1_PI							0.31830988618
#define	M_2_PI							0.63661977236
#define	M_2_SQRTPI						1.12837916710
#define	M_SQRT2							1.41421356237
#define	M_SQRT1_2						0.70710678118

double acos(double);
double asin(double);
double atan(double);
double atan2(double, double);
double ceil(double);
double cos(double);
double cosh(double);
double exp(double);
double fabs(double);
double floor(double);
double fmod(double, double);
double frexp(double, int *);
double ldexp(double, int);
double log(double);
double log10(double);
double modf(double, double *);
double pow(double, double);
double sin(double);
double sinh(double);
double sqrt(double);
double tan(double);
double tanh(double);
double erf(double);
double erfc(double);
double gamma(double);
double hypot(double, double);
double j0(double);
double j1(double);
double jn(int, double);
double lgamma(double);
double y0(double);
double y1(double);
double yn(int, double);
int    isnan(double);
double acosh(double);
double asinh(double);
double atanh(double);
double cbrt(double);
double expm1(double);
int    ilogb(double);
double log1p(double);
double logb(double);
double nextafter(double, double);
double remainder(double, double);
double rint(double);
double scalb(double, double);

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif
