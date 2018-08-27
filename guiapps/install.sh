#! /bin/sh
for name in "$@"
do
	cp $name $DESTDIR/usr/bin/`basename $name`
done

chmod 6755 $DESTDIR/usr/bin/thman
chmod 6755 $DESTDIR/usr/bin/gwmsudo
