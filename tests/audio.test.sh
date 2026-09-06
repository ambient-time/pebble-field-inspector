#!/usr/bin/env bash
set -euo pipefail
FI_ROOT=$(cd "$(dirname "$0")/.." && pwd)
FI_TEST_DIR=$(mktemp -d "${TMPDIR:-/tmp}/field-inspector-audio.XXXXXX")
trap 'rm -rf "$FI_TEST_DIR"' EXIT
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I "$FI_ROOT/src/c" "$FI_ROOT/tests/audio.test.c" -o "$FI_TEST_DIR/audio-test"
"$FI_TEST_DIR/audio-test"
python3 - "$FI_ROOT/src/c/main.c" "$FI_TEST_DIR/outbox_failed.inc" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
start = source.index('static void outbox_failed(')
end = source.index('\nstatic void inbox_dropped(', start)
Path(sys.argv[2]).write_text(source[start:end])
PY
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter -I "$FI_TEST_DIR" "$FI_ROOT/tests/outbox-failed.test.c" -o "$FI_TEST_DIR/outbox-failed-test"
"$FI_TEST_DIR/outbox-failed-test"
