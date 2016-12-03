// dummy file which defines some symbols to confuse GCC into thinking a 32-bit libc exists
// so that it build the C++ library
.text
.macro DUMMY_FUNC name
.globl \name
\name:
	ret
.size \name, .-\name
.endm

DUMMY_FUNC puts
DUMMY_FUNC printf
DUMMY_FUNC _start
