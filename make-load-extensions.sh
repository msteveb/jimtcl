#!/bin/sh

exec >$1

shift

echo '#include "jim.h"'
for i in "$@"; do
	name=`echo $i | sed -e 's@.*/\(.*\).c@\1@'`
	echo "extern int Jim_${name}Init(Jim_Interp *interp);"
done
echo "int Jim_InitStaticExtensions(Jim_Interp *interp) {"
for i in "$@"; do
	name=`echo $i | sed -e 's@.*/\(.*\).c@\1@'`
	echo "if (Jim_${name}Init(interp) != JIM_OK) return JIM_ERR;"
done
echo "}"
