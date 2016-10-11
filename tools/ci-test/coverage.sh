#!/bin/bash


# Go to source root directory
SCRIPT=$(readlink -f "$0")
SRC_DIR=$(dirname "$SCRIPT")/../..
BUILD_DIR=`pwd`

type lcov    >/dev/null 2>&1 || { echo >&2 "lcov and genhtml are required - please ensure they are installed.  Aborting."; exit 1; }
type genhtml >/dev/null 2>&1 || { echo >&2 "lcov and genhtml are required - please ensure they are installed.  Aborting."; exit 1; }

lcovver="$(lcov --version | cut -d" " -f4)"
lcovver_rc="1.11"
if [ "$(printf "$lcovver_rc\n$lcovver" | sort -V | head -n1)" == "$lcovver_rc" ]; then
	lcov_rc_opt="--rc genhtml_branch_coverage=1"
	genhtml_rc_opt="--rc genhtml_branch_coverage=1"
fi


while [[ $# -gt 0 ]]
do
	key="$1"
	case $key in
		--no-configure)
			NO_CONFIGURE=true
		;;
		--no-test)
			NO_CONFIGURE=true
			NO_TEST=true
		;;
		--open)
	    	OPEN_PAGE=true
	    ;;
		*)
			echo "Unknown option $1"
			echo "options: --no-configure : do not reconfigure"
			echo "         --no-test      : do not run tests - just compile coverage results - implies --no-configure"
			echo "         --open         : open results in web browser"
			exit -1
		;;
	esac
	shift
done

set -ex

# Configure if required
if [ "$NO_CONFIGURE" != "true" ]; then
	$SRC_DIR/tools/ci-test/configure_everything.sh
fi

# Run tests if required
if [ "$NO_CONFIGURE" != "true" ]; then
	# Main tests
	make test

	# Other jimsh commandline options
	./jimsh --help
	./jimsh --version
	echo "puts hello" | ./jimsh -
	./jimsh -e "badcommand" || true
fi

# Compile coverage results
lcov -c $lcov_rc_opt -d . -o lcov.txt
genhtml $genhtml_rc_opt -o coverage_html lcov.txt > genhtml_output.txt

# Open results in web browser if required
if [ "$OPEN_PAGE" == "true" ]; then
	xdg-open coverage_html/index.html
fi

