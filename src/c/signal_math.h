#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
typedef struct {
  int32_t x, y, z;
  uint32_t count, received, vibration_excluded, timestamp_rejected, capacity_excluded;
  int peak;
  uint64_t sum_squared, first_ms, last_ms, last_seen_ms;
} SignalMotion;
static inline void signal_motion_add(SignalMotion *s, int x, int y, int z, bool vibrating, uint64_t timestamp_ms) {
  if (s->received < UINT32_MAX) s->received++;
  if (!timestamp_ms || timestamp_ms <= s->last_seen_ms) { if (s->timestamp_rejected < UINT32_MAX) s->timestamp_rejected++; return; }
  s->last_seen_ms = timestamp_ms;
  if (vibrating) { if (s->vibration_excluded < UINT32_MAX) s->vibration_excluded++; return; }
  if (s->count >= 1000) { if (s->capacity_excluded < UINT32_MAX) s->capacity_excluded++; return; }
  if (!s->count) s->first_ms = timestamp_ms;
  s->last_ms = timestamp_ms;
  s->x += x; s->y += y; s->z += z; s->count++;
  s->sum_squared += (int64_t)x*x + (int64_t)y*y + (int64_t)z*z;
  int a = abs(x); if (abs(y) > a) a = abs(y); if (abs(z) > a) a = abs(z);
  if (a > s->peak) s->peak = a;
}
static inline uint32_t signal_motion_variance(const SignalMotion *s) {
  if (!s->count) return 0;
  uint64_t mean_squared = (int64_t)s->x*s->x + (int64_t)s->y*s->y + (int64_t)s->z*s->z;
  return (uint32_t)((s->sum_squared*s->count - mean_squared) / ((uint64_t)s->count*s->count));
}
static inline void signal_utf8_copy(char *out, size_t capacity, const char *in) {
  size_t i = 0;
  if (!capacity) return;
  while (in[i] && i + 1 < capacity) {
    unsigned char c = (unsigned char)in[i];
    size_t n = c < 0x80 ? 1 : (c >= 0xc2 && c <= 0xdf ? 2 : (c >= 0xe0 && c <= 0xef ? 3 : (c >= 0xf0 && c <= 0xf4 ? 4 : 0)));
    if (!n || i + n >= capacity) break;
    bool ok = true;
    for (size_t j = 1; j < n; j++) if (!in[i+j] || ((unsigned char)in[i+j] & 0xc0) != 0x80) { ok = false; break; }
    if (!ok) break;
    for (size_t j = 0; j < n; j++) out[i+j] = in[i+j];
    i += n;
  }
  out[i] = '\0';
}
