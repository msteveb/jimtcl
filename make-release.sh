#!/bin/sh

version=`sed -n -e 's/AC_INIT[^0]*\([^]]*\).*/\1/p' configure.ac`

if [ `git clean -nqx | wc -l` -ne 0 ]; then
	git clean -nqx
	echo "***: Tree not clean"
	exit 1
fi
if [ `git status | grep modified: | wc -l` -ne 0 ]; then
	git status
	echo "***: Modified files exist"
	exit 1
fi
mkdir jimtcl-$version
rsync --exclude=.git --exclude=jimtcl-$version -a ./ jimtcl-$version/
tar -czf jimtcl-$version.tar.gz jimtcl-$version
rm -rf jimtcl-$version
ls -l jimtcl-$version.tar.gz
