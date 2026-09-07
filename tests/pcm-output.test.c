#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pcm_output.h"
int main(void) {
  uint8_t input[256], output[1024], split[1024];
  for (int i=0; i<256; i++) input[i]=i;
  memset(output, 0x55, sizeof output);
  assert(fi_pcm_expand(input,256,output,1023)==0);
  assert(output[0]==0x55);
  assert(fi_pcm_expand(input,256,output,sizeof output)==1024);
  for (int i=0; i<512; i++) {
    int16_t actual=(int16_t)((uint16_t)output[i*2] | (uint16_t)output[i*2+1]<<8);
    assert(actual==(int8_t)input[i/2]*256);
  }
  assert(fi_pcm_expand(input,137,split,548)==548);
  assert(fi_pcm_expand(input+137,119,split+548,476)==476);
  assert(memcmp(output,split,1024)==0);
  puts("PASS: all signed PCM levels, duration ratio, bounds and packet continuity");
}
