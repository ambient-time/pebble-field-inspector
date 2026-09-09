#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "signal_math.h"
int main(void) {
  SignalMotion m={0};
  signal_motion_add(&m,100,-200,1000,false); signal_motion_add(&m,300,0,900,false);
  signal_motion_add(&m,9999,9999,9999,true);
  assert(m.count==2 && m.x==400 && m.y==-200 && m.z==1900 && m.peak==1000);
  for(int i=0;i<2000;i++) signal_motion_add(&m,-32768,32767,0,false);
  assert(m.count==1000 && m.peak==32768);
  char out[8]; signal_utf8_copy(out,sizeof out,"A界😀B"); assert(!strcmp(out,"A界"));
  signal_utf8_copy(out,5,"😀B"); assert(!strcmp(out,"😀"));
  signal_utf8_copy(out,4,"😀"); assert(!strcmp(out,""));
  signal_utf8_copy(out,8,"abc\xe2\x82"); assert(!strcmp(out,"abc"));
  signal_utf8_copy(out,8,"a\x80"); assert(!strcmp(out,"a"));
  puts("PASS bounded motion aggregation and UTF-8 report boundaries");
}
