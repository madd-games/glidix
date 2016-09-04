#! /usr/bin/python
import sys, os

proto = """
.globl $(FUNC)
.type $(FUNC), @function

$(FUNC):
	push %rbp
	mov %rsp, %rbp
	mov $$(ID), %rax
	mov %rcx, %r10
	syscall
	pop %rbp
	ret
.size $(FUNC), .-$(FUNC)
"""

extra = ""

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
	13:	"sigaction",
	14:	"sigprocmask",
	15:	"_glidix_stat",
	16:	"_glidix_getparsz",
	17:	"_glidix_getpars",
	18:	"raise",
	19:	"_glidix_geterrno",
	20:	"_glidix_seterrno",
	21:	"mprotect",
	22:	"lseek",
	23:	"fork",
	24:	"pause",
	25:	"waitpid",
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
	37:	"_glidix_fstat",
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
	48:	"pipe",
	49:	"_glidix_seterrnoptr",
	50:	"_glidix_geterrnoptr",
	51:	"clock",
	52:	"pread",
	53:	"pwrite",
	54:	"mmap",
	55:	"setuid",
	56:	"setgid",
	57:	"seteuid",
	58:	"setegid",
	59:	"setreuid",
	60:	"setregid",
	61:	"_glidix_rmmod",
	62:	"link",
	63:	"_glidix_unmount",
	64:	"_glidix_lstat",
	65:	"symlink",
	66:	"readlink",
	67:	"_glidix_down",
	68:	"sleep",
	69:	"_glidix_utime",
	70:	"umask",
	71:	"_glidix_routetab",
	72:	"socket",
	73:	"bind",
	74:	"sendto",
	75:	"send",
	76:	"recvfrom",
	77:	"recv",
	78:	"_glidix_route_add",
	79:	"_glidix_netconf_stat",
	80:	"_glidix_netconf_getaddrs",
	81:	"getsockname",
	82:	"shutdown",
	83:	"connect",
	84:	"getpeername",
	85:	"_glidix_setsockopt",
	86:	"_glidix_getsockopt",
	87:	"_glidix_netconf_statidx",
	88:	"_glidix_pcistat",
	89:	"getgroups",
	90:	"_glidix_setgroups",
	91:	"uname",
	92:	"_glidix_netconf_addr",
	93:	"_glidix_fcntl_getfd",
	94:	"_glidix_fcntl_setfd",
	95:	"_glidix_unique",
	96:	"isatty",
	97:	"_glidix_bindif",
	98:	"_glidix_route_clear",
	99:	"munmap",
	100:	"_glidix_thsync",
	101:	"getppid",
	102:	"alarm",
	103:	"_glidix_store_and_sleep",
	104:	"_glidix_mqserver",
	105:	"_glidix_mqclient",
	106:	"_glidix_mqsend",
	107:	"_glidix_mqrecv",
	108:	"_glidix_shmalloc",
	109:	"_glidix_shmap",
	110:	"setsid",
	111:	"setpgid",
	112:	"getsid",
	113:	"getpgid",
	114:	"pthread_exit",
	115:	"pthread_create",
	116:	"pthread_self",
	117:	"pthread_join",
	118:	"pthread_detach",
	119:	"pthread_kill",
	120:	"_glidix_kopt",
	121:	"_glidix_sigwait",
	122:	"_glidix_sigsuspend",
	123:	"lockf",
	124:	"_glidix_mcast",
	125:	"_glidix_fpoll",
	126:	"_glidix_oxperm",
	127:	"_glidix_dxperm",
	128:	"_glidix_fsinfo",
	129:	"_glidix_chxperm"
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
