// Compile the production callback with a small AppMessage transport stub.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MESSAGE_KEY_RequestId 1
#define MESSAGE_KEY_AudioAck 2
typedef int AppMessageResult;
typedef struct { uint32_t value; bool valid; } Tuple;
typedef struct { Tuple id, ack; bool has_id, has_ack; } DictionaryIterator;
static uint32_t s_request_id = 22, s_ack_sequence;
static bool s_ack_pending, s_busy = true;
static unsigned s_failures;
static Tuple *dict_find(DictionaryIterator *iter, int key) {
  if (key == MESSAGE_KEY_RequestId) return iter->has_id ? &iter->id : NULL;
  return iter->has_ack ? &iter->ack : NULL;
}
static bool read_uint(Tuple *tuple, uint32_t *out) {
  if (!tuple || !tuple->valid) return false;
  *out = tuple->value; return true;
}
static bool busy(void) { return s_busy; }
static void fail(const char *message) { (void)message; s_failures++; }

// Extracted from src/c/main.c by audio.test.sh; no mirrored implementation.
#include "outbox_failed.inc"

int main(void) {
  DictionaryIterator message = {.id={21,true}, .ack={17,true}, .has_id=true, .has_ack=true};
  outbox_failed(&message, 1, NULL);
  assert(!s_ack_pending); assert(s_ack_sequence == 0); // Old request cannot create a retry.
  s_ack_pending = true; s_ack_sequence = 3;
  outbox_failed(&message, 1, NULL);
  assert(s_ack_pending); assert(s_ack_sequence == 3); // Nor overwrite a current retry.
  message.has_id = false;
  outbox_failed(&message, 1, NULL);
  assert(s_ack_sequence == 3);
  message.has_id = true; message.id.valid = false;
  outbox_failed(&message, 1, NULL);
  assert(s_ack_sequence == 3);
  message.id.valid = true; message.id.value = s_request_id;
  outbox_failed(&message, 1, NULL);
  assert(s_ack_pending); assert(s_ack_sequence == 17); // Current ACK still retries.
  message.ack.valid = false;
  outbox_failed(&message, 1, NULL);
  assert(s_ack_sequence == 17);
  message.has_ack = false;
  outbox_failed(&message, 1, NULL);
  assert(s_failures == 1); // Current request transport failure still reports an error.
  message.id.value = 21;
  outbox_failed(&message, 1, NULL);
  message.id.value = s_request_id; s_busy = false;
  outbox_failed(&message, 1, NULL);
  assert(s_failures == 1);
  puts("PASS: 8 production outbox-failure cases cover stale, missing, malformed and current request acknowledgements.");
}
