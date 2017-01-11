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

#include <math.h>

#define	DBL_MAX_EXP						1024

/**
 * NOTE: As stated in math.h, this implementation is terrible.
 */

double sin(double x)
{
	const double B = 4/M_PI;
	const double C = -4/(M_PI*M_PI);

	double y = B * x + C * x * fabs(x);

	//  const double Q = 0.775;
	const double P = 0.225;

	y = P * (y * fabs(y) - y) + y;   // Q * y + P * y * abs(y)


	return y;
}

double cos(double x)
{
	return sin(x + M_PI_2);
}

double fabs(double x)
{
	if (x < 0.0) return -x;
	return x;
};

double exp(double x)
{
	int i;
	double sum = 0;
	int fac_so_far = 1;
	double numerator = 1;
	
	for (i=0; i<1000; i++)
	{
		sum += numerator / (double) fac_so_far;
		numerator *= x;
		if (i != 0) fac_so_far *= i;
	};
	
	return sum;
};

static double log_x_plus_1(double x)
{
	int i;
	double numerator = x;
	int denominator = 1;
	double sum = 0;
	
	for (i=1; i<1000; i++)
	{
		sum += numerator / (double) denominator++;
		numerator = -(numerator * x);
	};
	
	return sum;
};

double log(double x)
{
	return log_x_plus_1(x-1);
};

double pow(double x, double y)
{
	return exp(y * log(x));
};

double floor(double x)
{
	return (double) ((int) x);
};

double modf(double x, double *intpart)
{
	double y = floor(x);
	*intpart = y;
	return x - y;
};

double pow2(double x)
{
	return pow(2, x);
};

double ceil(double x)
{
	return floor(x)+1;
};

double frexp(double f, int* p)
{
	register int	k;
	register int	x;
	double		g;

	/*
	 * normalize
	 */

	x = k = DBL_MAX_EXP / 2;
	if (f < 1)
	{
		g = 1.0L / f;
		for (;;)
		{
			k = (k + 1) / 2;
			if (g < pow2(x))
				x -= k;
			else if (k == 1 && g < pow2(x+1))
				break;
			else
				x += k;
		}
		if (g == pow2(x))
			x--;
		x = -x;
	}
	else if (f > 1)
	{
		for (;;)
		{
			k = (k + 1) / 2;
			if (f > pow2(x))
				x += k;
			else if (k == 1 && f > pow2(x-1))
				break;
			else
				x -= k;
		}
		if (f == pow2(x))
			x++;
	}
	else
		x = 1;
	*p = x;

	/*
	 * shift
	 */

	x = -x;
	if (x < 0)
		f /= pow2(-x);
	else if (x < DBL_MAX_EXP)
		f *= pow2(x);
	else
		f = (f * pow2(DBL_MAX_EXP - 1)) * pow2(x - (DBL_MAX_EXP - 1));
	return f;
};

double ldexp(double a, int b)
{
	return a * (double)(1 << b);
};
