#! /bin/sh
srcdir="`dirname $0`"

echo >Makefile "SRCDIR := $srcdir"
echo >>Makefile "HOST_GCC := $HOST_GCC"
echo >>Makefile "HOST_AS := $HOST_AS"
echo >>Makefile "SYSROOT := $GLIDIX_SYSROOT"
echo >>Makefile "CFLAGS := -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-common -fno-builtin -fno-omit-frame-pointer -I $srcdir/../kernel/include -Wall -Werror -D__KERNEL__"
echo >>Makefile "LDFLAGS := -r -T $srcdir/../kernel/module/module.ld -nostdlib -ffreestanding"

modules=""
initmods=""
noninitmods=""

for modname in `ls ../modconf`
do
	if [ "`cat ../modconf/$modname`" != "disable" ]
	then
		modules="$modules $modname"
	fi
	
	if [ "`cat ../modconf/$modname`" = "initmod" ]
	then
		initmods="$initmods $modname"
	else
		noninitmods="$noninitmods $modname"
	fi
done

echo >>Makefile "MODNAMES := $modules"

for modname in $modules
do
	echo >>Makefile "SRC_$modname := \$(shell find \$(SRCDIR)/$modname -name '*.c')"
	echo >>Makefile "OBJ_$modname := \$(patsubst \$(SRCDIR)/$modname/%.c, $modname/%.o, \$(SRC_$modname))"
done

echo >>Makefile ".PHONY: all install"
echo >>Makefile "all: \$(MODNAMES:=.gkm)"

for modname in $modules
do
	echo >>Makefile "$modname/%.o: \$(SRCDIR)/$modname/%.c"
	echo >>Makefile "	@mkdir -p $modname"
	echo >>Makefile "	\$(HOST_GCC) -c \$< -o \$@ \$(CFLAGS)"
	
	echo >>Makefile "$modname.gkm: \$(OBJ_$modname)"
	echo >>Makefile "	\$(HOST_GCC) -o \$@ \$(LDFLAGS) ../kernel/module_start.o \$^ ../kernel/module_end.o"
done

echo >>Makefile "install:"
echo >>Makefile "	mkdir -p \$(DESTDIR)/etc/modules"

for modname in $noninitmods
do
	if [ "`cat ../modconf/$modname`" != "disable" ]
	then
		echo >>Makefile "	cp $modname.gkm \$(DESTDIR)/etc/modules/$modname.gkm"
	fi
done

echo >>Makefile "	mkdir -p \$(DESTDIR)/lib"
echo >>Makefile "	cp ../kernel/module_start.o \$(DESTDIR)/lib/module_start.o"
echo >>Makefile "	cp ../kernel/module_end.o \$(DESTDIR)/lib/module_end.o"
echo >>Makefile "	cp $srcdir/../kernel/module/module.ld \$(DESTDIR)/lib/module.ld"
