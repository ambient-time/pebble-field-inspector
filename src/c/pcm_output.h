#ifndef FIELD_INSPECTOR_PCM_OUTPUT_H
#define FIELD_INSPECTOR_PCM_OUTPUT_H
#include <stdint.h>
#include <stddef.h>

// Diagnostic conversion: signed 8 kHz/8-bit to 16 kHz/16-bit little endian.
// Repeat each sample twice. No state or discontinuity at packet boundaries.
static inline size_t fi_pcm_expand(const uint8_t *src, size_t count,
                                   uint8_t *dst, size_t capacity) {
  if (!src || !dst || count > capacity / 4) return 0;
  for (size_t i = 0; i < count; i++) {
    dst[i*4] = dst[i*4+2] = 0;
    dst[i*4+1] = dst[i*4+3] = src[i];
  }
  return count * 4;
}
#endif
