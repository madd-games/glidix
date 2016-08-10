#! /bin/sh
mkdir -p $1/usr/bin/
mkdir -p $1/usr/include
mkdir -p $1/usr/libexec/netman/ipv4
mkdir -p $1/usr/libexec/netman/ipv6

cp include/* $1/usr/include/
cp out/ipv4/* $1/usr/libexec/netman/ipv4/
cp out/ipv6/* $1/usr/libexec/netman/ipv6/
cp out/bin/* $1/usr/bin/
