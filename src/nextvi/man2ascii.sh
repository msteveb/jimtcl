#!/bin/sh
export current_date="$(date +"%b %e, %Y")"
export EXINIT="$(printf '%b' 'c .Dd ${current_date}:>CODE MAP>:.+4,.+18!./gencodemap.sh:wq')"
eval "EXINIT=\"$EXINIT\""
vi -s ./vi.1
mandoc -T ascii ./vi.1 > README
EXINIT="$(printf '%b' '%s/.\010(.)/\\1/g:wq')" vi -s ./README
