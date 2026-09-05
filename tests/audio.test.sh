#!/usr/bin/env bash
set -euo pipefail
FI_ROOT=$(cd "$(dirname "$0")/.." && pwd)
FI_TEST_DIR=$(mktemp -d "${TMPDIR:-/tmp}/field-inspector-audio.XXXXXX")
trap 'rm -rf "$FI_TEST_DIR"' EXIT
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I "$FI_ROOT/src/c" "$FI_ROOT/tests/audio.test.c" -o "$FI_TEST_DIR/audio-test"
"$FI_TEST_DIR/audio-test"
