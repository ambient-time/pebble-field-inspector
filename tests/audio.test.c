#include <assert.h>
#include <stdio.h>
#include "audio_buffer.h"
int main(void) {
  FiAudio a; uint8_t packet[FI_AUDIO_CHUNK]; uint32_t sequence=1, offset=0, played=0;
  assert(!fi_audio_begin(&a,0)); assert(!fi_audio_begin(&a,FI_AUDIO_LIMIT+1));
  assert(fi_audio_begin(&a,FI_AUDIO_LIMIT));
  while(offset<FI_AUDIO_LIMIT) {
    uint16_t n=FI_AUDIO_LIMIT-offset<FI_AUDIO_CHUNK?FI_AUDIO_LIMIT-offset:FI_AUDIO_CHUNK;
    for(int i=0;i<n;i++) packet[i]=(offset+i)%251;
    FiAudioResult r=fi_audio_accept(&a,sequence,packet,n);
    if(r==FI_AUDIO_OK) {
      assert(fi_audio_accept(&a,sequence,packet,n)==FI_AUDIO_DUPLICATE);
      assert(fi_audio_accept(&a,sequence+2,packet,n)==FI_AUDIO_INVALID);
      offset+=n; sequence++;
    } else assert(r==FI_AUDIO_FULL);
    // A real speaker may accept only part of a buffer, or zero bytes.
    assert(fi_audio_consume(&a,0));
    uint16_t accepted=fi_audio_contiguous(&a); if(accepted>137) accepted=137;
    for(int i=0;i<accepted;i++) assert(a.bytes[a.read+i]==(played+i)%251);
    assert(fi_audio_consume(&a,accepted)); played+=accepted;
    assert(a.count<=FI_AUDIO_CAP);
  }
  assert(fi_audio_end(&a,sequence)==FI_AUDIO_OK);
  assert(fi_audio_end(&a,sequence)==FI_AUDIO_DUPLICATE);
  while(a.count) {
    uint16_t n=fi_audio_contiguous(&a); if(n>79)n=79;
    for(int i=0;i<n;i++) assert(a.bytes[a.read+i]==(played+i)%251);
    assert(fi_audio_consume(&a,n)); played+=n;
  }
  assert(played==FI_AUDIO_LIMIT); assert(!fi_audio_consume(&a,1));
  assert(fi_audio_begin(&a,600)); assert(fi_audio_end(&a,1)==FI_AUDIO_INVALID);
  assert(fi_audio_accept(&a,1,packet,513)==FI_AUDIO_INVALID);
  puts("PASS: 128000 PCM bytes preserved through wraparound, full-buffer backpressure, partial writes, duplicates, out-of-order and truncated streams.");
}
