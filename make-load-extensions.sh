#!/bin/sh

exec >$1

shift

exts=$(echo "$@" | sed -e 's@[^ ]*jim-@@g' -e 's@[.]c@@g')

echo '#include "jim.h"'
for name in $exts; do
	echo "extern int Jim_${name}Init(Jim_Interp *interp);"
done
echo "int Jim_InitStaticExtensions(Jim_Interp *interp) {"
for name in $exts; do
	echo "if (Jim_${name}Init(interp) != JIM_OK) return JIM_ERR;"
done
	echo "return JIM_OK;"
echo "}"
