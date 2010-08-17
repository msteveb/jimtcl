#!/bin/sh

exec >$1

shift

echo '#include "jim.h"'
for i in "$@"; do
	name=`echo $i | sed -e 's@.*/\(.*\).c@\1@'`
	echo "extern void Jim_${name}Init(Jim_Interp *interp);"
done
echo "void Jim_InitStaticExtensions(Jim_Interp *interp) {"
for i in "$@"; do
	name=`echo $i | sed -e 's@.*/\(.*\).c@\1@'`
	echo "Jim_${name}Init(interp);"
done
echo "}"
