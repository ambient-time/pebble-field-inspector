#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "signal_math.h"
int main(void) {
  SignalMotion m={0};
  signal_motion_add(&m,100,-200,1000,false,100); signal_motion_add(&m,300,0,900,false,200);
  signal_motion_add(&m,9999,9999,9999,true,300);
  assert(m.count==2 && m.x==400 && m.y==-200 && m.z==1900 && m.peak==1000);
  assert(m.received==3 && m.vibration_excluded==1 && m.first_ms==100 && m.last_ms==200);
  assert(signal_motion_variance(&m)==22500);
  for(int i=0;i<2000;i++) signal_motion_add(&m,-32768,32767,0,false,400+i*100);
  assert(m.count==1000 && m.peak==32768 && m.capacity_excluded==1002);
  SignalMotion stationary={0},moving={0},timing={0},extremes={0};
  for(int i=0;i<50;i++) {
    signal_motion_add(&stationary,0,0,1000,false,1000+i*100);
    signal_motion_add(&moving,i%2?-500:500,0,1000,false,1000+i*100);
  }
  assert(stationary.x==moving.x && stationary.peak==moving.peak);
  assert(signal_motion_variance(&stationary)==0 && signal_motion_variance(&moving)==250000);
  signal_motion_add(&timing,1,2,3,false,0);
  signal_motion_add(&timing,1,2,3,false,1000);
  signal_motion_add(&timing,1,2,3,false,1000);
  signal_motion_add(&timing,1,2,3,false,900);
  signal_motion_add(&timing,1,2,3,true,1100);
  signal_motion_add(&timing,1,2,3,false,2000);
  assert(timing.received==6 && timing.count==2 && timing.timestamp_rejected==3 && timing.vibration_excluded==1);
  assert(timing.first_ms==1000 && timing.last_ms==2000 && signal_motion_variance(&timing)==0);
  for(int i=0;i<1000;i++) {
    int v=i%2?-32768:32767;
    signal_motion_add(&extremes,v,v,v,false,1000+i*100);
  }
  assert(signal_motion_variance(&extremes)==3221127168U);
  assert(signal_motion_variance(&(SignalMotion){0})==0);
  char out[8]; signal_utf8_copy(out,sizeof out,"A界😀B"); assert(!strcmp(out,"A界"));
  signal_utf8_copy(out,5,"😀B"); assert(!strcmp(out,"😀"));
  signal_utf8_copy(out,4,"😀"); assert(!strcmp(out,""));
  signal_utf8_copy(out,8,"abc\xe2\x82"); assert(!strcmp(out,"abc"));
  signal_utf8_copy(out,8,"a\x80"); assert(!strcmp(out,"a"));
  puts("PASS bounded motion aggregation and UTF-8 report boundaries");
}
