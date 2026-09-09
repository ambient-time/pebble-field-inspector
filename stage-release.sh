#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
export PATH="$HOME/.local/bin:$PATH"
if [[ -n "$(git status --porcelain)" ]]; then
  echo "Commit the intended source changes before staging a release." >&2
  exit 1
fi
npm run test:client
pebble sdk activate 4.33.1
pebble clean
pebble build
python3 - <<'PY'
import json
from pathlib import Path
import zipfile

package = json.loads(Path('package.json').read_text())
bundle = Path('build') / (package['name'] + '.pbw')
with zipfile.ZipFile(bundle) as archive:
    assert archive.testzip() is None, 'PBW checksum failure'
    metadata = json.loads(archive.read('appinfo.json'))
    assert metadata['uuid'] == 'e2fd86ec-dfb8-460c-afc1-ebe4d071657a'
    assert metadata['versionLabel'] == package['version']
    assert metadata['watchapp']['watchface'] is False
    for platform in package['pebble']['targetPlatforms']:
        assert archive.getinfo(platform + '/pebble-app.bin').file_size > 0
    assert archive.getinfo('pebble-js-app.js').file_size > 0
print('PBW metadata and all six platform binaries verified')
PY
mkdir -p dist
cp build/pebble-field-inspector.pbw dist/field-inspector.pbw
cp build/pebble-field-inspector.pbw dist/signal-station.pbw
shasum -a 256 build/pebble-js-app.js > dist/JAVASCRIPT_SHA256.txt
git rev-parse HEAD > dist/SOURCE_COMMIT.txt
(cd dist && shasum -a 256 field-inspector.pbw signal-station.pbw > SHA256SUMS.txt && shasum -a 256 -c SHA256SUMS.txt)
echo "Staged dist/signal-station.pbw and compatibility filename"
