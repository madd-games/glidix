	.file "libglidix.s"
	.text

.globl _exit
.type _exit, @function

_exit:
	xor %rax, %rax
	ud2
	.hword 0
	ret

.size _exit, .-_exit


.globl write
.type write, @function

write:
	xor %rax, %rax
	ud2
	.hword 1
	ret

.size write, .-write


.globl _glidix_exec
.type _glidix_exec, @function

_glidix_exec:
	xor %rax, %rax
	ud2
	.hword 2
	ret

.size _glidix_exec, .-_glidix_exec


.globl read
.type read, @function

read:
	xor %rax, %rax
	ud2
	.hword 3
	ret

.size read, .-read


.globl _glidix_open
.type _glidix_open, @function

_glidix_open:
	xor %rax, %rax
	ud2
	.hword 4
	ret

.size _glidix_open, .-_glidix_open


.globl close
.type close, @function

close:
	xor %rax, %rax
	ud2
	.hword 5
	ret

.size close, .-close


.globl getpid
.type getpid, @function

getpid:
	xor %rax, %rax
	ud2
	.hword 6
	ret

.size getpid, .-getpid


.globl getuid
.type getuid, @function

getuid:
	xor %rax, %rax
	ud2
	.hword 7
	ret

.size getuid, .-getuid


.globl geteuid
.type geteuid, @function

geteuid:
	xor %rax, %rax
	ud2
	.hword 8
	ret

.size geteuid, .-geteuid


.globl _glidix_getsuid
.type _glidix_getsuid, @function

_glidix_getsuid:
	xor %rax, %rax
	ud2
	.hword 9
	ret

.size _glidix_getsuid, .-_glidix_getsuid


.globl getgid
.type getgid, @function

getgid:
	xor %rax, %rax
	ud2
	.hword 10
	ret

.size getgid, .-getgid


.globl getegid
.type getegid, @function

getegid:
	xor %rax, %rax
	ud2
	.hword 11
	ret

.size getegid, .-getegid


.globl _glidix_getsgid
.type _glidix_getsgid, @function

_glidix_getsgid:
	xor %rax, %rax
	ud2
	.hword 12
	ret

.size _glidix_getsgid, .-_glidix_getsgid


.globl _glidix_sighandler
.type _glidix_sighandler, @function

_glidix_sighandler:
	xor %rax, %rax
	ud2
	.hword 13
	ret

.size _glidix_sighandler, .-_glidix_sighandler


.globl _glidix_sigret
.type _glidix_sigret, @function

_glidix_sigret:
	xor %rax, %rax
	ud2
	.hword 14
	ret

.size _glidix_sigret, .-_glidix_sigret


.globl stat
.type stat, @function

stat:
	xor %rax, %rax
	ud2
	.hword 15
	ret

.size stat, .-stat


.globl _glidix_getparsz
.type _glidix_getparsz, @function

_glidix_getparsz:
	xor %rax, %rax
	ud2
	.hword 16
	ret

.size _glidix_getparsz, .-_glidix_getparsz


.globl _glidix_getpars
.type _glidix_getpars, @function

_glidix_getpars:
	xor %rax, %rax
	ud2
	.hword 17
	ret

.size _glidix_getpars, .-_glidix_getpars


.globl raise
.type raise, @function

raise:
	xor %rax, %rax
	ud2
	.hword 18
	ret

.size raise, .-raise


.globl _glidix_geterrno
.type _glidix_geterrno, @function

_glidix_geterrno:
	xor %rax, %rax
	ud2
	.hword 19
	ret

.size _glidix_geterrno, .-_glidix_geterrno


.globl _glidix_seterrno
.type _glidix_seterrno, @function

_glidix_seterrno:
	xor %rax, %rax
	ud2
	.hword 20
	ret

.size _glidix_seterrno, .-_glidix_seterrno


.globl mprotect
.type mprotect, @function

mprotect:
	xor %rax, %rax
	ud2
	.hword 21
	ret

.size mprotect, .-mprotect


.globl lseek
.type lseek, @function

lseek:
	xor %rax, %rax
	ud2
	.hword 22
	ret

.size lseek, .-lseek


.globl _glidix_clone
.type _glidix_clone, @function

_glidix_clone:
	xor %rax, %rax
	ud2
	.hword 23
	ret

.size _glidix_clone, .-_glidix_clone


.globl pause
.type pause, @function

pause:
	xor %rax, %rax
	ud2
	.hword 24
	ret

.size pause, .-pause


.globl kill
.type kill, @function

kill:
	xor %rax, %rax
	ud2
	.hword 26
	ret

.size kill, .-kill


.globl _glidix_insmod
.type _glidix_insmod, @function

_glidix_insmod:
	xor %rax, %rax
	ud2
	.hword 27
	ret

.size _glidix_insmod, .-_glidix_insmod


.globl _glidix_ioctl
.type _glidix_ioctl, @function

_glidix_ioctl:
	xor %rax, %rax
	ud2
	.hword 28
	ret

.size _glidix_ioctl, .-_glidix_ioctl


.globl _glidix_fdopendir
.type _glidix_fdopendir, @function

_glidix_fdopendir:
	xor %rax, %rax
	ud2
	.hword 29
	ret

.size _glidix_fdopendir, .-_glidix_fdopendir


.globl _glidix_diag
.type _glidix_diag, @function

_glidix_diag:
	xor %rax, %rax
	ud2
	.hword 30
	ret

.size _glidix_diag, .-_glidix_diag


.globl _glidix_mount
.type _glidix_mount, @function

_glidix_mount:
	xor %rax, %rax
	ud2
	.hword 31
	ret

.size _glidix_mount, .-_glidix_mount


.globl _glidix_yield
.type _glidix_yield, @function

_glidix_yield:
	xor %rax, %rax
	ud2
	.hword 32
	ret

.size _glidix_yield, .-_glidix_yield


.globl _glidix_time
.type _glidix_time, @function

_glidix_time:
	xor %rax, %rax
	ud2
	.hword 33
	ret

.size _glidix_time, .-_glidix_time


.globl realpath
.type realpath, @function

realpath:
	xor %rax, %rax
	ud2
	.hword 34
	ret

.size realpath, .-realpath


.globl chdir
.type chdir, @function

chdir:
	xor %rax, %rax
	ud2
	.hword 35
	ret

.size chdir, .-chdir


.globl getcwd
.type getcwd, @function

getcwd:
	xor %rax, %rax
	ud2
	.hword 36
	ret

.size getcwd, .-getcwd


.globl fstat
.type fstat, @function

fstat:
	xor %rax, %rax
	ud2
	.hword 37
	ret

.size fstat, .-fstat


.globl chmod
.type chmod, @function

chmod:
	xor %rax, %rax
	ud2
	.hword 38
	ret

.size chmod, .-chmod


.globl fchmod
.type fchmod, @function

fchmod:
	xor %rax, %rax
	ud2
	.hword 39
	ret

.size fchmod, .-fchmod


.globl fsync
.type fsync, @function

fsync:
	xor %rax, %rax
	ud2
	.hword 40
	ret

.size fsync, .-fsync


.globl chown
.type chown, @function

chown:
	xor %rax, %rax
	ud2
	.hword 41
	ret

.size chown, .-chown


.globl fchown
.type fchown, @function

fchown:
	xor %rax, %rax
	ud2
	.hword 42
	ret

.size fchown, .-fchown


.globl mkdir
.type mkdir, @function

mkdir:
	xor %rax, %rax
	ud2
	.hword 43
	ret

.size mkdir, .-mkdir


.globl ftruncate
.type ftruncate, @function

ftruncate:
	xor %rax, %rax
	ud2
	.hword 44
	ret

.size ftruncate, .-ftruncate


.globl unlink
.type unlink, @function

unlink:
	xor %rax, %rax
	ud2
	.hword 45
	ret

.size unlink, .-unlink


.globl dup
.type dup, @function

dup:
	xor %rax, %rax
	ud2
	.hword 46
	ret

.size dup, .-dup


.globl dup2
.type dup2, @function

dup2:
	xor %rax, %rax
	ud2
	.hword 47
	ret

.size dup2, .-dup2


.globl _glidix_seterrnoptr
.type _glidix_seterrnoptr, @function

_glidix_seterrnoptr:
	xor %rax, %rax
	ud2
	.hword 49
	ret

.size _glidix_seterrnoptr, .-_glidix_seterrnoptr


.globl _glidix_geterrnoptr
.type _glidix_geterrnoptr, @function

_glidix_geterrnoptr:
	xor %rax, %rax
	ud2
	.hword 50
	ret

.size _glidix_geterrnoptr, .-_glidix_geterrnoptr


.globl clock
.type clock, @function

clock:
	xor %rax, %rax
	ud2
	.hword 51
	ret

.size clock, .-clock


.globl _glidix_libopen
.type _glidix_libopen, @function

_glidix_libopen:
	xor %rax, %rax
	ud2
	.hword 52
	ret

.size _glidix_libopen, .-_glidix_libopen


.globl _glidix_libclose
.type _glidix_libclose, @function

_glidix_libclose:
	xor %rax, %rax
	ud2
	.hword 53
	ret

.size _glidix_libclose, .-_glidix_libclose


.globl _glidix_pollpid
.type _glidix_pollpid, @function

_glidix_pollpid:
	xor %rax, %rax
	ud2
	.hword 25
	test %rsi, %rsi
	jz .nopass
	mov %edi, (%rsi)
.nopass:
	ret

.size _glidix_pollpid, .-_glidix_pollpid

.globl pipe
.type pipe, @function

pipe:
	xor %rax, %rax
	ud2
	.hword 48
	mov %r8d, (%rdi)
	mov %r9d, 4(%rdi)
	ret
.size pipe, .-pipe
