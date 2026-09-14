#!/usr/bin/env bash
set -euo pipefail
SS_ROOT=$(cd "$(dirname "$0")/.." && pwd)
SS_TMP=$(mktemp -d "${TMPDIR:-/tmp}/signal-station-tests.XXXXXX")
trap 'rm -rf "$SS_TMP"' EXIT
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I "$SS_ROOT/src/c" "$SS_ROOT/tests/signal.test.c" -o "$SS_TMP/signal-test"
"$SS_TMP/signal-test"
python3 "$SS_ROOT/tests/survey-contract.test.py"
python3 "$SS_ROOT/tests/collection-contract.test.py"
