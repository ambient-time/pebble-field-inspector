#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define SIGNAL_HISTORY_MINUTES 15
#define SIGNAL_HISTORY_VALUE_CAP 1000

typedef struct {
  uint8_t steps, orientation, light, heart_rate_bpm;
  uint16_t vmc;
  bool invalid;
} SignalMinute;

static inline bool signal_json_advance(size_t capacity, size_t *length, int written) {
  if (written < 0 || (size_t)written >= capacity - *length) return false;
  *length += written;
  return true;
}

static inline bool signal_history_bounds(uint32_t count, int64_t start_ms, int64_t end_ms,
                                         int64_t requested_start_ms, int64_t requested_end_ms) {
  if (count > SIGNAL_HISTORY_MINUTES || requested_start_ms < 0 ||
      requested_end_ms - requested_start_ms != SIGNAL_HISTORY_MINUTES*60000LL ||
      requested_start_ms % 60000 || requested_end_ms % 60000) return false;
  return !count || (start_ms >= requested_start_ms && end_ms <= requested_end_ms &&
    start_ms % 60000 == 0 && end_ms - start_ms == count*60000LL);
}

static inline uint32_t signal_history_valid(const SignalMinute *minutes, uint32_t count) {
  uint32_t valid = 0;
  for (uint32_t i = 0; i < count; i++) if (!minutes[i].invalid) valid++;
  return valid;
}

static inline bool signal_history_format(char *out, size_t capacity, const SignalMinute *minutes,
                                        uint32_t count, int64_t start_ms, int64_t end_ms,
                                        int64_t requested_start_ms, int64_t requested_end_ms) {
  if (!signal_history_bounds(count, start_ms, end_ms, requested_start_ms, requested_end_ms)) return false;
  size_t length = 0;
  if (!capacity) return false;
  // Pebble supports snprintf but does not expose vsnprintf.
#define SIGNAL_HISTORY_APPEND(...) do { \
    if (!signal_json_advance(capacity, &length, \
      snprintf(out + length, capacity - length, __VA_ARGS__))) return false; \
  } while (0)
  SIGNAL_HISTORY_APPEND(
    "{\"schema\":1,\"requested_minutes\":15,\"returned_minutes\":%lu,\"valid_minutes\":%lu,"
    "\"requested_start_ms\":%llu,\"requested_end_ms\":%llu,"
    "\"columns\":[\"steps\",\"vmc\",\"orientation\",\"light\",\"heart_rate_bpm\"],\"minutes\":[",
    (unsigned long)count, (unsigned long)signal_history_valid(minutes, count),
    (unsigned long long)requested_start_ms, (unsigned long long)requested_end_ms);
  for (uint32_t i = 0; i < count; i++) {
    if (i) SIGNAL_HISTORY_APPEND(",");
    if (minutes[i].invalid) {
      SIGNAL_HISTORY_APPEND("null");
    } else {
      char light[8] = "null", hr[8] = "null";
      if (minutes[i].light >= 1 && minutes[i].light <= 4) snprintf(light, sizeof light, "%u", minutes[i].light);
      if (minutes[i].heart_rate_bpm) snprintf(hr, sizeof hr, "%u", minutes[i].heart_rate_bpm);
      SIGNAL_HISTORY_APPEND("[%u,%u,%u,%s,%s]", minutes[i].steps,
        minutes[i].vmc, minutes[i].orientation, light, hr);
    }
  }
  SIGNAL_HISTORY_APPEND("]}");
#undef SIGNAL_HISTORY_APPEND
  return true;
}
