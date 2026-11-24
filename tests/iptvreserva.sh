#!/bin/bash
# Backup servers of a Live TV account (src/iptvreserva.c). See tests/iptvreserva.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/iptvreserva.c tests/iptvreserva.c \
  -Isrc -o "${TMPDIR:-/tmp}/nuvio-iptvreserva-tests" -O1 -g -Wall -Wextra
"${TMPDIR:-/tmp}/nuvio-iptvreserva-tests"
