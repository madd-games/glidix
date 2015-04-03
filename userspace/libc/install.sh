cp out/lib/libc.a $1/lib/libc.a
cp out/lib/libc.so $1/lib/libc.so
cp out/lib/libdl.so $1/lib/libdl.so
cp out/lib/libcrypt.so $1/lib/libcrypt.so
cp out/lib/crt0.o $1/lib/crt0.o
cp out/lib/crti.o $1/lib/crti.o
cp out/lib/crtn.o $1/lib/crtn.o
cp -RT include $1/usr/include
