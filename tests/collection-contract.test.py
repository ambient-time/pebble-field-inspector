"""Exercise production watch collectors with synthetic SDK records on the host.

The C functions are sliced from main.c, not reimplemented here. This proves
serialization and handling of the fake SDK contract, not physical sensor data.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/c/main.c").read_text()


def function(name):
    """Keep the original function body, including every production branch."""
    match = re.search(r"^static [^\n]+\b" + re.escape(name) + r"\(", SOURCE, re.M)
    assert match, name
    start = match.start()
    end = SOURCE.find("\nstatic ", start + 1)
    assert end > start, name
    return SOURCE[start:end].rstrip()


snapshot_cap = re.search(r"^#define SNAPSHOT_CAP (\d+)$", SOURCE, re.M).group(1)
PREFIX = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "signal_math.h"
#include "signal_history.h"
static char s_enabled[900];
static time_t s_collected_at;
static SignalMotion s_motion;
static bool s_sampling;
static int history_calls, accessibility_calls;
static time_t requested_start, requested_end;
static const char *scenario;
typedef struct {
  int16_t x, y, z;
  bool did_vibrate;
  uint64_t timestamp;
} AccelData;
static void cancel_turn(const char *message) {
  fprintf(stderr, "Unexpected collection cancellation: %s\n", message);
  abort();
}
#ifdef PBL_HEALTH
typedef unsigned HealthServiceAccessibilityMask;
enum { HealthServiceAccessibilityMaskAvailable=1, HealthServiceAccessibilityMaskNoPermission=2 };
enum { HealthMetricStepCount=0 };
typedef enum { AmbientLightLevelUnknown=0, AmbientLightLevelVeryDark=1,
  AmbientLightLevelDark=2, AmbientLightLevelLight=3, AmbientLightLevelVeryLight=4 } AmbientLightLevel;
typedef struct {
  uint8_t steps, orientation;
  uint16_t vmc;
  bool is_invalid:1;
  AmbientLightLevel light:3;
  uint8_t padding:4;
  uint8_t heart_rate_bpm;
  uint8_t reserved[6];
} HealthMinuteData;
static HealthServiceAccessibilityMask health_service_metric_accessible(int metric, time_t start, time_t end) {
  assert(metric==HealthMetricStepCount);
  accessibility_calls++;
  requested_start=start; requested_end=end;
  return !strcmp(scenario,"denied") ? HealthServiceAccessibilityMaskNoPermission : HealthServiceAccessibilityMaskAvailable;
}
static uint32_t health_service_get_minute_history(HealthMinuteData *records, uint32_t maximum, time_t *start, time_t *end) {
  history_calls++;
  assert(maximum==15 && *start==requested_start && *end==requested_end);
  for (uint32_t i=0;i<maximum;i++) records[i]=(HealthMinuteData){.steps=255,.vmc=65535,.orientation=255,.light=4,.heart_rate_bpm=255};
  if (!strcmp(scenario,"zero")) { *start=-999; *end=-42; return 0; }
  if (!strcmp(scenario,"invalid_count")) return 16; // Never write beyond the supplied capacity.
  if (!strcmp(scenario,"before_requested")) { *start-=60; *end-=60; return 15; }
  if (!strcmp(scenario,"after_requested")) { *start+=60; *end+=60; return 15; }
  // Observed on Diorite QEMU: the SDK shifts to its first available minute but
  // returns an unfinished current minute beyond the requested exclusive end.
  if (!strcmp(scenario,"qemu_tail")) { *start+=5*60; *end+=60; return 11; }
  if (!strcmp(scenario,"future_only")) { *start=*end; *end+=120; return 2; }
  if (!strcmp(scenario,"unaligned")) { *start+=1; *end+=1; return 15; }
  if (!strcmp(scenario,"wrong_span")) { *end=*start+120; return 3; }
  if (!strcmp(scenario,"partial") || !strcmp(scenario,"all_invalid")) {
    *start+=8*60; *end=*start+3*60;
    records[0]=(HealthMinuteData){0};
    records[1].is_invalid=true;
    records[2]=(HealthMinuteData){.steps=3,.vmc=300,.orientation=16,.light=4,.heart_rate_bpm=72};
    if (!strcmp(scenario,"all_invalid")) for (int i=0;i<3;i++) records[i].is_invalid=true;
    return 3;
  }
  if (!strcmp(scenario,"worst_unknowns")) for (uint32_t i=0;i<maximum;i++) {
    records[i].light=0; records[i].heart_rate_bpm=0;
  }
  return 15;
}
#endif
'''

DRIVER = r'''
static void emit(const char *name) {
  assert(strlen(s_snapshot)+2<sizeof s_snapshot);
  strcat(s_snapshot,"]");
  printf("{\"case\":\"%s\",\"history_calls\":%d,\"accessibility_calls\":%d,"
         "\"requested_start\":%lld,\"requested_end\":%lld,\"packet_bytes\":%lu,\"snapshot\":%s}\n",
         name,history_calls,accessibility_calls,(long long)requested_start,(long long)requested_end,
         (unsigned long)strlen(s_snapshot),s_snapshot);
}
static void reset(const char *name, const char *enabled_keys) {
  scenario=name; strcpy(s_enabled,enabled_keys); strcpy(s_snapshot,"[");
  history_calls=accessibility_calls=0; requested_start=requested_end=0;
  s_collected_at=1789344037; memset(&s_motion,0,sizeof s_motion); s_sampling=false;
}
int main(void) {
  const char *cases[]={"complete","partial","all_invalid","zero","denied","invalid_count",
    "before_requested","after_requested","qemu_tail","future_only","unaligned","wrong_span","worst_unknowns","hour_boundary","day_boundary"};
  for (unsigned i=0;i<sizeof cases/sizeof cases[0];i++) {
    reset(cases[i],"[\"watch.minute_history\"]");
    if (!strcmp(cases[i],"hour_boundary")) s_collected_at=1789347601;
    if (!strcmp(cases[i],"day_boundary")) s_collected_at=1789344000;
    minute_history(); emit(cases[i]);
  }
  reset("history_disabled","[\"watch.motion\"]"); minute_history(); emit(scenario);
  reset("motion_disabled","[\"watch.minute_history\"]"); motion_observation(); emit(scenario);
  reset("motion","[\"watch.motion\"]"); s_sampling=true;
  AccelData samples[]={
    {.x=-500,.z=1000,.timestamp=1789344037025ULL},
    {.x=500,.z=1000,.timestamp=1789344037125ULL},
    {.x=9999,.z=9999,.did_vibrate=true,.timestamp=1789344037225ULL},
    {.x=9999,.timestamp=1789344037125ULL},
    {.x=9999,.timestamp=0},
    {.x=9999,.timestamp=1789344037224ULL}};
  accel(samples,sizeof samples/sizeof samples[0]);
  s_sampling=false; accel(samples,1); // Late callbacks must not change the measurement.
  motion_observation(); emit(scenario);
  reset("motion_empty","[\"watch.motion\"]"); motion_observation(); emit(scenario);
  reset("stationary","[\"watch.motion\"]"); s_sampling=true;
  AccelData still[]={ {.z=1000,.timestamp=1789344037025ULL}, {.z=1000,.timestamp=1789344037125ULL} };
  accel(still,2); motion_observation(); emit(scenario);
  SignalMinute minute={0}; char too_small[8];
  assert(!signal_history_format(too_small,sizeof too_small,&minute,1,60000,120000,0,900000));
  assert(!signal_history_bounds(16,0,960000,0,900000));
  return 0;
}
'''

PROGRAM = PREFIX + f"\nstatic char s_snapshot[{snapshot_cap}];\n" + "\n".join(
    function(name) for name in ("enabled", "accel", "append_observation_ms", "minute_history", "motion_observation")
) + DRIVER


def run_program(folder, health):
    source = folder / ("health.c" if health else "unsupported.c")
    binary = source.with_suffix("")
    source.write_text(PROGRAM)
    command = ["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "src/c")]
    if health:
        command += ["-DPBL_HEALTH"]
    subprocess.run(command + [str(source), "-o", str(binary)], check=True)
    output = subprocess.run([str(binary)], check=True, capture_output=True, text=True).stdout
    return {row["case"]: row for row in map(json.loads, output.splitlines())}


def observation(cases, name):
    row = cases[name]
    assert row["packet_bytes"] < 1900, name
    assert len(row["snapshot"]) == 1, name
    result = row["snapshot"][0]
    assert len(json.dumps(result["value"], separators=(",", ":")).encode()) < 1000, name
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-fixture", action="store_true", help="Explicitly regenerate the checked-in partial-history fixture")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="signal-collection-contract-") as temporary:
        cases = run_program(Path(temporary), True)
        unsupported = run_program(Path(temporary), False)

    for name in ("history_disabled", "motion_disabled"):
        assert cases[name]["snapshot"] == []
        assert cases[name]["history_calls"] == cases[name]["accessibility_calls"] == 0

    full = observation(cases, "complete")
    assert full["status"] == "available" and full["value"]["returned_minutes"] == full["value"]["valid_minutes"] == 15
    assert full["windowEnd"] - full["windowStart"] == 900000
    assert full["measuredAt"] == full["windowEnd"]
    partial = observation(cases, "partial")
    assert partial["value"]["minutes"] == [[0, 0, 0, None, None], None, [3, 300, 16, 4, 72]]
    assert partial["value"]["returned_minutes"] == 3 and partial["value"]["valid_minutes"] == 2
    assert partial["windowStart"] == partial["value"]["requested_start_ms"] + 480000
    assert partial["windowEnd"] - partial["windowStart"] == 180000
    invalid = observation(cases, "all_invalid")
    assert invalid["value"]["minutes"] == [None] * 3 and invalid["value"]["valid_minutes"] == 0
    assert invalid["status"] == "unavailable" and "measuredAt" not in invalid
    assert invalid["windowEnd"] - invalid["windowStart"] == 180000

    for name, retained, omitted in (("after_requested", 14, 1), ("qemu_tail", 10, 1), ("future_only", 0, 2)):
        value = observation(cases, name)
        assert value["value"]["returned_minutes"] == retained, name
        assert len(value["value"]["minutes"]) == retained, name
        assert value["fields"]["reason"] == "history_window_clipped", name
        assert int(value["fields"]["excluded_after_requested_minutes"]) == omitted, name
        assert int(value["fields"]["sdk_returned_minutes"]) == retained + omitted, name
        assert int(value["fields"]["sdk_window_end_s"]) > cases[name]["requested_end"], name
        if retained:
            assert value["windowEnd"] == value["measuredAt"] == value["value"]["requested_end_ms"], name
            assert value["windowEnd"] - value["windowStart"] == retained * 60000, name
            assert value["status"] == "available", name
        else:
            assert not {"windowStart", "windowEnd", "measuredAt"}.intersection(value), name
            assert value["status"] == "unavailable", name

    for name in ("zero", "denied", "invalid_count", "before_requested", "unaligned", "wrong_span"):
        value = observation(cases, name)
        assert value["value"]["returned_minutes"] == value["value"]["valid_minutes"] == 0, name
        assert value["value"]["minutes"] == [], name
        assert not {"windowStart", "windowEnd", "measuredAt"}.intersection(value), name
        assert value["status"] == ("permission_denied" if name == "denied" else "unavailable"), name
        if name not in ("zero", "denied"):
            assert value["fields"]["reason"] == "invalid_history_window", name
    assert cases["denied"]["history_calls"] == 0 and cases["denied"]["accessibility_calls"] == 1
    no_health = observation(unsupported, "complete")
    assert no_health["status"] == "not_supported" and no_health["value"]["minutes"] == []
    assert unsupported["complete"]["history_calls"] == unsupported["complete"]["accessibility_calls"] == 0
    assert not {"windowStart", "windowEnd", "measuredAt"}.intersection(no_health)

    for name, collected in (("complete", 1789344037), ("hour_boundary", 1789347601), ("day_boundary", 1789344000)):
        value = observation(cases, name)
        expected_end = collected - collected % 60
        assert cases[name]["requested_end"] == expected_end
        assert cases[name]["requested_start"] == expected_end - 900
        assert value["value"]["requested_end_ms"] == expected_end * 1000
        assert value["value"]["requested_start_ms"] == (expected_end - 900) * 1000
        assert cases[name]["history_calls"] == 1
    unknowns = observation(cases, "worst_unknowns")
    assert unknowns["value"]["minutes"] == [[255, 65535, 255, None, None]] * 15

    motion = observation(cases, "motion")
    fields = motion["value"]
    assert fields["samples"] == 2 and fields["received_samples"] == 6
    assert fields["vibration_excluded"] == 1 and fields["timestamp_rejected"] == 3 and fields["capacity_excluded"] == 0
    assert fields["mean_x"] == fields["mean_y"] == 0 and fields["mean_z"] == fields["peak_abs_axis"] == 1000
    assert fields["variance_mg2"] == 250000
    assert fields["first_sample_ms"] == motion["windowStart"] == 1789344037025
    assert fields["last_sample_ms"] == motion["windowEnd"] == motion["measuredAt"] == 1789344037125
    assert fields["requested_hz"] == 10 and fields["requested_duration_ms"] == 5000
    assert fields["timing"] == "sdk_epoch_ms"
    still = observation(cases, "stationary")
    assert still["value"]["variance_mg2"] == 0
    assert all(still["value"][key] == fields[key] for key in ("samples", "mean_x", "mean_y", "mean_z", "peak_abs_axis"))
    empty = observation(cases, "motion_empty")
    assert empty["status"] == "unavailable" and empty["value"]["samples"] == 0
    assert not {"windowStart", "windowEnd", "measuredAt"}.intersection(empty)

    fixture = ROOT / "tests/fixtures/watch-minute-history.json"
    if args.write_fixture:
        fixture.write_text(json.dumps([partial], indent=2) + "\n")
        print(f"WROTE C-generated fixture: {fixture}")
    else:
        assert json.loads(fixture.read_text()) == [partial], "Regenerate the fixture only after reviewing the production contract change"
    print("PASS actual C collectors: selected sources, completed UTC minutes, partial/invalid/denied/unsupported history, truthful motion timing and variation")
    print(f"PASS C packet bounds: maximum synthetic snapshot {max(row['packet_bytes'] for row in cases.values())} bytes; all values below 1000 UTF-8 bytes")


if __name__ == "__main__":
    main()
