#!/usr/bin/env tclsh

target="$1"
source="$2"

case "$target" in
*.c) ;;
*) echo 1>&2 "Target $target is not a .c file"; exit 1;;
esac
case "$source" in
*.tcl) ;;
*) echo 1>&2 "Source $source is not a .tcl file"; exit 1;;
esac

basename=`basename $source .tcl`

exec >$target

cat <<EOF
#include <jim.h>
int Jim_${basename}Init(Jim_Interp *interp)
{
	return Jim_Eval_Named(interp, 
EOF

# Note: Keep newlines so that line numbers match in error messages
sed -e 's/^[ 	]*#.*//' -e 's@\\@\\\\@g' -e 's@"@\\"@g' -e 's@^\(.*\)$@"\1\\n"@' $source
#sed -e 's@^\(.*\)$@"\1\\n"@' $source

echo ",\"$source\", 1);"
echo "}"
