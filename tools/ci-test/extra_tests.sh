#!/bin/bash

set -ex

# Go to source root directory
SCRIPT=$(readlink -f "$0")
SRC_DIR=$(dirname "$SCRIPT")/../..
BUILD_DIR=`pwd`
INSTALL_DIR=$BUILD_DIR/install
OUT_OF_TREE_DIR=$BUILD_DIR/alternate

type clang         >/dev/null 2>&1 || { echo >&2 "clang & scan-build are required - please ensure they are installed.  Aborting."; exit 1; }
type scan-build    >/dev/null 2>&1 || { echo >&2 "clang & scan-build are required - please ensure they are installed.  Aborting."; exit 1; }

# Check with clang static analysis - checking jim.c is pretty slow...
$SRC_DIR/tools/ci-test/configure_everything.sh --clang-analyze

set -o pipefail
make 2>&1 | tee analysis.txt

# Clang analysis disables -Werror, so check for warnings
if (grep "warning:" analysis.txt > /dev/null); then
	echo "Warnings issued - abort"
	exit -1
fi


# Check with clang undefined behavior sanitizer
$SRC_DIR/tools/ci-test/configure_everything.sh --clang-check-undefined
set -o pipefail
make test 2>&1 | tee undefined.txt
# Check for errors in the output
if (grep "runtime error:" undefined.txt > /dev/null); then
	echo "Runtime errors - abort"
	exit -1
fi

# Check with clang stack sanitizer
$SRC_DIR/tools/ci-test/configure_everything.sh --clang-check-stack
set -o pipefail
make test 2>&1 | tee stack.txt
# Check for errors in the output
if (grep "runtime error:" stack.txt > /dev/null); then
	echo "Runtime errors - abort"
	exit -1
fi

# Check with clang address sanitizer
$SRC_DIR/tools/ci-test/configure_everything.sh --clang-check-address
set -o pipefail
make test 2>&1 | tee address.txt
# Check for errors in the output
if (grep "runtime error:" address.txt > /dev/null); then
	echo "Runtime errors - abort"
	exit -1
fi

# Check with clang memory sanitizer
$SRC_DIR/tools/ci-test/configure_everything.sh --clang-check-memory
set -o pipefail
make test 2>&1 | tee memory.txt
# Check for errors in the output
if (grep "runtime error:" memory.txt > /dev/null); then
	echo "Runtime errors - abort"
	exit -1
fi

# Check with clang thread sanitizer
$SRC_DIR/tools/ci-test/configure_everything.sh --clang-check-thread
set -o pipefail
make test 2>&1 | tee thread.txt
# Check for errors in the output
if (grep "runtime error:" thread.txt > /dev/null); then
	echo "Runtime errors - abort"
	exit -1
fi


