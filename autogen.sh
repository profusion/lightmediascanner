#!/bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

origdir=`pwd`

cd "$srcdir"
autoreconf --force --install -I m4/
cd "$origdir"

if [ -z "$NOCONFIGURE" ]; then
    $srcdir/configure $AUTOGEN_CONFIGURE_ARGS "$@" || exit $?
fi
