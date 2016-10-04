#!/bin/bash

set -ex

# Try compiling/linking against Metakit
if ! (echo "int main(void){c4_Storage dummy();}" | g++ -o /dev/null -xc++ --include mk4.h -Imk/include -Lmk/lib -lmk4 - >/dev/null  2>&1); then
	# Could not find metakit - download and build it

	METAKIT_VERSION=2.4.9.7
	METAKIT_DIRNAME=metakit-$METAKIT_VERSION
	METAKIT_FILENAME=$METAKIT_DIRNAME.tar.gz
	METAKIT_URL=http://www.equi4.com/pub/mk/$METAKIT_FILENAME

	mkdir -p mk
	cd mk
	wget $METAKIT_URL
	tar -zxvf $METAKIT_FILENAME
	cd $METAKIT_DIRNAME/unix
	./configure --prefix=`pwd`/../..
	make install
fi
