#!/bin/bash

set -ex

# Go to source root directory
SCRIPT=$(readlink -f "$0")
SRC_DIR=$(dirname "$SCRIPT")/../..
BUILD_DIR=`pwd`
INSTALL_DIR=$BUILD_DIR/install
OUT_OF_TREE_DIR=$BUILD_DIR/alternate

type valgrind    >/dev/null 2>&1 || { echo >&2 "tclsh, valgrind, asciidoc, lcov and genhtml are required - please ensure they are installed.  Aborting."; exit 1; }
type lcov        >/dev/null 2>&1 || { echo >&2 "tclsh, valgrind, asciidoc, lcov and genhtml are required - please ensure they are installed.  Aborting."; exit 1; }
type genhtml     >/dev/null 2>&1 || { echo >&2 "tclsh, valgrind, asciidoc, lcov and genhtml are required - please ensure they are installed.  Aborting."; exit 1; }
type tclsh       >/dev/null 2>&1 || { echo >&2 "tclsh, valgrind, asciidoc, lcov and genhtml are required - please ensure they are installed.  Aborting."; exit 1; }
type asciidoc    >/dev/null 2>&1 || { echo >&2 "tclsh, valgrind, asciidoc, lcov and genhtml are required - please ensure they are installed.  Aborting."; exit 1; }

# check tree is clean
if [[ $(git -C $SRC_DIR status --ignored -s) ]]; then
	echo "Source tree is not clean (including ignored files) - aborting"
    exit -1
fi

# Create install directory
mkdir -p $INSTALL_DIR

# Configure with tclsh
$SRC_DIR/tools/ci-test/configure_everything.sh --use-tclsh

# Configure by building jimsh0
$SRC_DIR/tools/ci-test/configure_everything.sh --generate-jimsh0 --prefix $INSTALL_DIR

# Build all possible targets
make -j8 install ship Tcl.html readdir.so array.so clock.so file.so interp.so posix.so regexp.so syslog.so readline.so pack.so tclprefix.so sqlite3.so mk.so zlib.so

# Run tests under Valgrind
make -C $SRC_DIR/tests jimsh="valgrind --leak-check=full --show-reachable=yes --error-exitcode=1 --track-origins=yes --suppressions=$SRC_DIR/tools/ci-test/valgrind.supp $BUILD_DIR/jimsh" TOPSRCDIR=$SRC_DIR

# Parse and display code coverage
$SRC_DIR/tools/ci-test/coverage.sh --no-test

## Parse test coverage results without exclusions
#lcov -c --no-markers -d . -o lcov_output_without_exclusions.txt

## Check test coverage is not reduced
# Disabled until a solution compatible with applying
# pull requests out-of-order is determined
#./jimsh $SRC_DIR/tools/ci-test/lcov_parse.tcl

# cleanup
rm -rf install *.gcno *.gcda lcov.txt genhtml_output.txt coverage_html lcov_output_with_exclusions.txt lcov_output_without_exclusions.txt mk/
git -C $SRC_DIR checkout Tcl_shipped.html

# test clean targets
make clean
make distclean

# check tree is clean
if [[ $(git -C $SRC_DIR status --ignored -s) ]]; then
	echo "distclean did not remove everything"
    exit -1
fi

# Check minimal build - checking for cross dependencies
# Also check building out of tree
mkdir $OUT_OF_TREE_DIR
cd $OUT_OF_TREE_DIR && $SRC_DIR/configure && make -j8 test
rm -rf $OUT_OF_TREE_DIR

# Check tests pass in tclsh too - checking compatibility
# Currently disabled until tests are fixed for tclsh compatibility
#make -C tests tcl

