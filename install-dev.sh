rm -rf $1/kernel-include
mkdir -p /usr/bin
mkdir -p $1/lib

cp -r include $1/kernel-include/
cp utils/modcc /usr/bin/$2
cp utils/modld /usr/bin/$3
cp utils/modmake /usr/bin/modmake
cp utils/module.ld $1/lib/module.ld
cp utils/module.rule $1/lib/module.rule

nasm -felf64 utils/module_start.asm -o $1/lib/module_start.o
nasm -felf64 utils/module_end.asm -o $1/lib/module_end.o
chmod +x /usr/bin/modmake
