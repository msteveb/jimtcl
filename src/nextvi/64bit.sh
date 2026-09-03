#!/bin/sh -e

VI=${VI:-vi}
if ! $VI -? 2>&1 | grep -q 'Nextvi'; then
    echo "Error: $VI is not nextvi" >&2
    echo "Set VI environment variable to point to nextvi binary" >&2
    exit 1
fi

for p in *.c *.h; do
	EXINIT="seq -1:%s/(\<unsigned int\> )/u64 /g"
	EXINIT="${EXINIT}:%s/\(unsigned int\)/(u64)/g"
	EXINIT="${EXINIT}:%s/\(unsigned int\*\)/(u64*) /g"
	EXINIT="${EXINIT}:%s/(\<int\> )|\<int(?=^\[)/s64 /g"
	EXINIT="${EXINIT}:%s/\(int\)/(s64)/g"
	EXINIT="${EXINIT}:%s/\(int\*\)/(s64*)/g"
	EXINIT="${EXINIT}:%s/\<int,\>/s64,/g"
	EXINIT="${EXINIT}:%s/abs\(/labs(/g"
	EXINIT="${EXINIT}:%s/\%d/\%ld/g"
	EXINIT="${EXINIT}:%s/\%\+d/\%+ld/g"
	EXINIT="${EXINIT}:%s/\%x/\%lx/g"
	EXINIT="${EXINIT}:%s/\%o/\%lo/g"
	EXINIT="${EXINIT}:%s/\%u/\%lu/g"
	EXINIT="${EXINIT}:%s/, i, map\[i]\);/, (int)i, map[i]);/g"
	EXINIT="${EXINIT}:%s/s64 signo/int signo/g"
	EXINIT="${EXINIT}:%s/s64 pipefds/int pipefds/g"
	EXINIT="${EXINIT}:%s/void \(\*pipe_old\)\(s64\);/void (*pipe_old)(int);/g"
	EXINIT="${EXINIT}:%s/s64 main\(s64/int main(int/g"
	EXINIT="${EXINIT}:%s/waitpid\(pid, status, 0\);/waitpid(pid, (int*)status, 0);/g"
	EXINIT="${EXINIT}:%s/INT_MAX/INT64_MAX/g"
	EXINIT="${EXINIT}:wq" $VI -sm "$p"
done
EXINIT="$(printf '%b' '1:i #include <stdint.h>\ntypedef uint64_t u64;\ntypedef int64_t s64;:wq')" $VI -sm vi.h
