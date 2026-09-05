#!/usr/bin/env python3
"""Make a local emulator-only PBW from a credential-free inspect response.

Only the offline demo payload changes. The production phone protocol, native
watch binary, and app metadata remain the same. Never stage this PBW for release.
"""
import argparse
import base64
import json
import re
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("response", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--pbw", type=Path, default=Path("build/pebble-field-inspector.pbw"))
args = parser.parse_args()
if args.output.resolve() == args.pbw.resolve() or "dist" in args.output.parts:
    parser.error("Use a separate temporary output outside dist.")
result = json.loads(args.response.read_text())
audio = result["audio"]
assert audio["sample_rate"] == 8000 and audio["format"] == "s8"
pcm = base64.b64decode(audio["pcm_base64"], validate=True)
assert 0 < len(pcm) <= 128000
assert isinstance(result["text"], str) and 0 < len(result["text"]) <= 240
# Select only the public response fields. No configuration or token is copied.
fixture = json.dumps({"text": "LOCAL VOICE TEST\n" + result["text"], "audio": audio}, ensure_ascii=True)
with zipfile.ZipFile(args.pbw) as original, zipfile.ZipFile(args.output, "w", zipfile.ZIP_DEFLATED) as output:
    for item in original.infolist():
        data = original.read(item.filename)
        if item.filename == "pebble-js-app.js":
            js = data.decode()
            pattern = r"var audio = new Uint8Array\(8000\);.*?return deliver\(a, cached\);"
            replacement = "var fixture = " + fixture + ";\n cached = {text:fixture.text, audio:decodeAudio(fixture.audio), demo:true};\n return deliver(a, cached);"
            js, count = re.subn(pattern, lambda _: replacement, js, count=1, flags=re.S)
            assert count == 1, "Offline demo code was not found; rebuild and inspect the protocol."
            data = js.encode()
        output.writestr(item, data)
print("Emulator-only fixture PBW:", args.output, "PCM bytes:", len(pcm))
