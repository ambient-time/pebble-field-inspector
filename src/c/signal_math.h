#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
typedef struct { int32_t x, y, z; uint32_t count; int peak; } SignalMotion;
static inline void signal_motion_add(SignalMotion *s, int x, int y, int z, bool vibrating) {
  if (vibrating || s->count >= 1000) return;
  s->x += x; s->y += y; s->z += z; s->count++;
  int a = abs(x); if (abs(y) > a) a = abs(y); if (abs(z) > a) a = abs(z);
  if (a > s->peak) s->peak = a;
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
