source="$1"

case "$source" in
*.tcl) ;;
*) echo 1>&2 "Source $source is not a .tcl file"; exit 1;;
esac

basename=`basename $source .tcl`

cat <<EOF
#include <jim.h>
int Jim_${basename}Init(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "$basename", "1.0", JIM_ERRMSG))
		return JIM_ERR;

	return Jim_Eval_Named(interp, 
EOF

# Note: Keep newlines so that line numbers match in error messages
sed -e 's/^[ 	]*#.*//' -e 's@\\@\\\\@g' -e 's@"@\\"@g' -e 's@^\(.*\)$@"\1\\n"@' $source
#sed -e 's@^\(.*\)$@"\1\\n"@' $source

echo ",\"$source\", 1);"
echo "}"
