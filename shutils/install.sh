#! /bin/sh
for name in "$@"
do
	cp $name $DESTDIR/usr/bin/`basename $name`
done
