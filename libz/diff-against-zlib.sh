# Usage: sh ./diff-against-zlib.sh /path/to/zlib-1.2.8

for FILE in adler32.c compress.c crc32.c crc32.h deflate.c deflate.h gzclose.c \
            gzguts.h gzlib.c gzread.c gzwrite.c infback.c inffast.c inffast.h \
            inffixed.h inflate.c inflate.h inftrees.c inftrees.h trees.c \
            trees.h uncompr.c zlib.h zutil.c zutil.h FAQ; do
  ${DIFF:=diff} -u "$1/$FILE" "$FILE"
done

${DIFF:=diff} -u "$1/zconf.h.in" "zconf.h"
${DIFF:=diff} -u "$1/zlib.3" "libz.3"
${DIFF:=diff} -u "$1/zlib.pc.in" "z.pc.in"
${DIFF:=diff} -u "$1/zlib.map" "libz.map"
${DIFF:=diff} -uN "$1/gencrc32.c" "gencrc32.c"
${DIFF:=diff} -uN "$1/genfixed.c" "genfixed.c"
${DIFF:=diff} -uN "$1/gentrees.c" "gentrees.c"
