"""Check timestamp provenance using the production C observation formatters.

Inputs are synthetic SDK outcomes, including absent 2 SE heart-rate data.
These host checks do not establish physical sensor or companion behavior.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/c/main.c").read_text()
start = source.index("static void append_observation_ms(")
end = source.index("\n#ifdef PBL_HEALTH", start)
formatters = source[start:end]

program = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static char s_snapshot[1900];
static time_t s_collected_at=1700000000;
static bool selected=true;
static bool enabled(const char *key) { (void)key; return selected; }
''' + formatters + r'''
static void emit(const char *name, const char *value, const char *status, bool date) {
  strcpy(s_snapshot,"[");
  bool steps=date || !strcmp(name,"fresh_zero");
  append_observation(steps?"health.steps":"health.heart_rate",value,steps?"steps":"bpm",status,date?"day":"current",
                     s_collected_at-60,s_collected_at,date);
  strcat(s_snapshot,"]");
  printf("{\"case\":\"%s\",\"snapshot\":%s}\n",name,s_snapshot);
}
int main(void) {
  emit("absent_sensor","null","unavailable",false);
  emit("denied_sensor","null","permission_denied",false);
  emit("unsupported_sensor","null","not_supported",false);
  emit("denied_stale_value","72","permission_denied",false);
  emit("unavailable_stale_value","72","unavailable",false);
  emit("unknown_timestamp","72","timestamp_unknown",false);
  emit("fresh_null","null","fresh",false);
  emit("fresh_zero","0","fresh",false);
  emit("fresh_value","72","fresh",false);
  emit("available_value","72","available",false);
  emit("unavailable_day","null","unavailable",true);
  selected=false;
  emit("source_disabled","72","fresh",false);
  return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="signal-observation-contract-") as tmp:
    c = Path(tmp) / "observations.c"
    binary = Path(tmp) / "observations"
    c.write_text(program)
    subprocess.run([
        "cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", str(c), "-o", str(binary),
    ], check=True)
    result = subprocess.run(
        [str(binary)], check=True, capture_output=True, text=True,
        env={**os.environ, "TZ": "UTC"},
    )
cases = {row["case"]: row["snapshot"] for row in map(json.loads, result.stdout.splitlines())}

assert cases.pop("source_disabled") == []
assert len(cases) == 11
for name, observations in cases.items():
    assert len(observations) == 1, name
    row = observations[0]
    assert row["collectedAt"] == 1700000000000, name
    if name in ("fresh_zero", "fresh_value", "available_value"):
        assert row["measuredAt"] == 1700000000000, name
    else:
        assert "measuredAt" not in row, f"{name} must not invent a measurement timestamp: {row}"
    if name == "unknown_timestamp":
        assert "windowStart" not in row and "windowEnd" not in row, name
    else:
        assert row["windowStart"] == 1699999940000, name
        assert row["windowEnd"] == 1700000000000, name

assert cases["absent_sensor"][0]["value"] is None
assert cases["denied_sensor"][0]["status"] == "permission_denied"
assert cases["fresh_zero"][0]["value"] == 0
assert cases["unknown_timestamp"][0]["value"] == 72
assert cases["unavailable_day"][0]["date"] == "2023-11-14"
print("PASS 12 production observation cases: unavailable/denied data has no measurement time; windows, dates and real zeroes survive")
