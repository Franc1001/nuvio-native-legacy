#!/bin/bash
# Add-ons added by hand on this device (src/addonslocais.c). See tests/addonslocais.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/addonslocais.c tests/addonslocais.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-addonslocais" -O1 -g -Wall -Wextra
"${TMPDIR:-/tmp}/nuvio-addonslocais"
