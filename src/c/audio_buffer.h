#ifndef FIELD_INSPECTOR_AUDIO_BUFFER_H
#define FIELD_INSPECTOR_AUDIO_BUFFER_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define FI_AUDIO_CAP 8192
#define FI_AUDIO_LIMIT 128000
#define FI_AUDIO_CHUNK 512

typedef struct {
  uint8_t bytes[FI_AUDIO_CAP];
  uint16_t read, count;
  uint32_t received, total, sequence;
  bool ended;
} FiAudio;

typedef enum { FI_AUDIO_INVALID, FI_AUDIO_FULL, FI_AUDIO_OK, FI_AUDIO_DUPLICATE } FiAudioResult;

static inline bool fi_audio_begin(FiAudio *a, uint32_t total) {
  memset(a, 0, sizeof *a);
  if (!total || total > FI_AUDIO_LIMIT) return false;
  a->total = total;
  a->sequence = 1;
  return true;
}
static inline FiAudioResult fi_audio_accept(FiAudio *a, uint32_t sequence, const uint8_t *bytes, uint16_t size) {
  if (a->total && sequence + 1 == a->sequence) return FI_AUDIO_DUPLICATE;
  if (!a->total || a->ended || sequence != a->sequence || !size || size > FI_AUDIO_CHUNK ||
      a->received + size > a->total) return FI_AUDIO_INVALID;
  if (size > FI_AUDIO_CAP - a->count) return FI_AUDIO_FULL;
  for (uint16_t i = 0; i < size; i++) a->bytes[(a->read + a->count + i) % FI_AUDIO_CAP] = bytes[i];
  a->count += size;
  a->received += size;
  a->sequence++;
  return FI_AUDIO_OK;
}
static inline FiAudioResult fi_audio_end(FiAudio *a, uint32_t sequence) {
  if (a->ended && sequence + 1 == a->sequence) return FI_AUDIO_DUPLICATE;
  if (!a->total || sequence != a->sequence || a->received != a->total) return FI_AUDIO_INVALID;
  a->ended = true;
  a->sequence++;
  return FI_AUDIO_OK;
}
static inline uint16_t fi_audio_contiguous(FiAudio *a) {
  uint16_t until_end = FI_AUDIO_CAP - a->read;
  return a->count < until_end ? a->count : until_end;
}
static inline bool fi_audio_consume(FiAudio *a, uint16_t size) {
  if (size > a->count) return false;
  a->read = (a->read + size) % FI_AUDIO_CAP;
  a->count -= size;
  return true;
}
#endif
