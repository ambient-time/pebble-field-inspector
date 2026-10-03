// Signal Station — Luke Steuber. Retry envelope adapted from Gadget Watch (MIT).
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { int kind; uint32_t id; unsigned failures; bool paused; } SignalTransport;
static inline void signal_transport_resume(SignalTransport *s) { s->paused=false; s->failures=0; }
static inline bool signal_transport_failed(SignalTransport *s,int kind,uint32_t id) {
  if (s->paused) return false;
  if (s->kind!=kind || s->id!=id) s->failures=0;
  s->kind=kind; s->id=id;
  if (++s->failures<4) return true;
  s->paused=true; return false;
}
static inline void signal_transport_sent(SignalTransport *s) { s->failures=0; }
