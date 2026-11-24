#!/bin/bash
# Live TV channel opens with an empty catalog (no account, no add-ons).
#   bash tests/catcanalvazio.sh
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-catcanalvazio-tests"
cc src/catalogo.c tests/catcanalvazio.c tests/catalogo_boundary_stub.c \
  -Isrc -o "$out" -O1 -g -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"$out"
