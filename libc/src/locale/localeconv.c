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

#include <locale.h>

struct lconv *localeconv ()
{
	static /*const*/ struct lconv result =
	{
		/* decimal_point */ ".",
		/* thousands_sep */ "",
		/* grouping */ "",
		/* mon_decimal_point */ "",
		/* mon_thousands_sep */ "",
		/* mon_grouping */ "",
		/* positive_sign */ "",
		/* negative_sign */ "",
		/* currency_symbol */ "",
		/* int_curr_symbol */ "",
		/* frac_digits */ ((char)255),
		/* p_cs_precedes */ ((char)255),
		/* p_sign_posn */ ((char)255),
		/* p_sep_by_space */ ((char)255),
		/* n_cs_precedes */ ((char)255),
		/* n_sign_posn */ ((char)255),
		/* n_sep_by_space */ ((char)255),
		/* int_frac_digits */ ((char)255),
		/* int_p_cs_precedes */ ((char)255),
		/* int_p_sign_posn */ ((char)255),
		/* int_p_sep_by_space */ ((char)255),
		/* int_n_cs_precedes */ ((char)255),
		/* int_n_sign_posn */ ((char)255),
		/* int_n_sep_by_space */ ((char)255)
	};

	return &result;
};
