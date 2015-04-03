#! /usr/bin/python
import sys, os

proto = """
.globl $(FUNC)
.type $(FUNC), @function

$(FUNC):
	xor %rax, %rax
	ud2
	.hword $(ID)
	ret

.size $(FUNC), .-$(FUNC)
"""

extra = """
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
"""

syscallTable = {
	0:	"_exit",
	1:	"write",
	2:	"_glidix_exec",
	3:	"read",
	4:	"_glidix_open",
	5:	"close",
	6:	"getpid",
	7:	"getuid",
	8:	"geteuid",
	9:	"_glidix_getsuid",
	10:	"getgid",
	11:	"getegid",
	12:	"_glidix_getsgid",
	13:	"_glidix_sighandler",
	14:	"_glidix_sigret",
	15:	"stat",
	16:	"_glidix_getparsz",
	17:	"_glidix_getpars",
	18:	"raise",
	19:	"_glidix_geterrno",
	20:	"_glidix_seterrno",
	21:	"mprotect",
	22:	"lseek",
	23:	"_glidix_clone",
	24:	"pause",
	# 25: see 'extra'
	26:	"kill",
	27:	"_glidix_insmod",
	28:	"_glidix_ioctl",
	29:	"_glidix_fdopendir",
	30:	"_glidix_diag",
	31:	"_glidix_mount",
	32:	"_glidix_yield",
	33:	"_glidix_time",
	34:	"realpath",
	35:	"chdir",
	36:	"getcwd",
	37:	"fstat",
	38:	"chmod",
	39:	"fchmod",
	40:	"fsync",
	41:	"chown",
	42:	"fchown",
	43:	"mkdir",
	44:	"ftruncate",
	45:	"unlink",
	46:	"dup",
	47:	"dup2",
	# 48: see 'extra'
	49:	"_glidix_seterrnoptr",
	50:	"_glidix_geterrnoptr",
	51:	"clock",
	52:	"_glidix_libopen",
	53:	"_glidix_libclose",
	54:	"_glidix_mmap",
	55:	"setuid",
	56:	"setgid",
	57:	"seteuid",
	58:	"setegid",
	59:	"setreuid",
	60:	"setregid"
}

f = open("_libglidix.s", "wb")
f.write("\t.file \"libglidix.s\"\n")
f.write("\t.text\n")

for id_, func in syscallTable.items():
	s = proto.replace("$(FUNC)", func)
	s = s.replace("$(ID)", str(id_))
	f.write("%s\n" % s)

f.write(extra)
f.close()

sys.exit(os.system("%s -c _libglidix.s -o %s" % (sys.argv[1], sys.argv[2])))
