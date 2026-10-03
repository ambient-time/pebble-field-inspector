"""Keep the mutable publication pointer aligned with the current Store record."""
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
listing = json.loads((ROOT / "store/listing.json").read_text())
current = json.loads((ROOT / "store/publication.json").read_text())
versioned_path = ROOT / f"store/publication-{listing['version']}.json"
assert versioned_path.is_file(), versioned_path
versioned = json.loads(versioned_path.read_text())

assert current == versioned
assert current["version"] == listing["version"]
assert current["appId"] == "37360ca4d9764881bd1d6f4d"
assert current["releaseStatus"] == "Published"
assert current["pbwBytes"] > 0
assert len(current["pbwSHA256"]) == 64
assert current["storePBW"]["url"].endswith(".pbw")
assert current["publicStoreInstallVerified"] is False
assert current["physicalValidation"]
subprocess.run(
    ["git", "cat-file", "-e", f"{current['sourceCommit']}^{{commit}}"],
    cwd=ROOT,
    check=True,
)
assert "EXPERMENTAL" not in listing["description"]
assert "Pebble face" not in listing["description"]
print("PASS current publication pointer, immutable receipt, source commit and listing copy")
