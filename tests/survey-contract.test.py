"""Host checks of the watch's actual day-window helper and message contract."""
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/c/main.c').read_text()
start = source.index('static time_t day_start(')
end = source.index('\nstatic void next_snapshot(', start)
helper = source[start:end]
program = '''#include <time.h>
#include <assert.h>
#include <stdio.h>
static time_t s_collected_at;
''' + helper + '''
int main(void) {
  struct tm local={0}; local.tm_year=126;local.tm_mon=2;local.tm_mday=9;local.tm_hour=12;local.tm_isdst=-1;
  s_collected_at=mktime(&local);
  assert(day_start(0)-day_start(1)==23*3600);
  for(int i=0;i<8;i++){struct tm d=*localtime(&(time_t){day_start(i)});assert(d.tm_hour==0);assert(d.tm_mday==9-i);}
  local.tm_mon=10;local.tm_mday=2;local.tm_isdst=-1;s_collected_at=mktime(&local);
  assert(day_start(0)-day_start(1)==25*3600);
  puts("PASS today plus seven complete local days across DST");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp) / 'day.c'; c.write_text(program)
    binary = Path(tmp) / 'day'
    subprocess.run(['cc', '-std=c99', '-Wall', '-Werror', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], env={**os.environ, 'TZ': 'America/Los_Angeles'}, check=True)
assert 'stage>=1 && stage<=16' in source
assert 'int ago=(stage-1)/2' in source
assert 'health_service_activities_iterate(HealthActivitySleep' in source
assert 'end-start>=7200' in source
keys = json.loads((root / 'package.json').read_text())['pebble']['messageKeys']
assert keys[:17] == ['RequestType','RequestId','Prompt','SpeakerAvailable','Muted','ResponseText','StatusText','AudioExpected','AudioBegin','AudioChunk','AudioEnd','AudioAck','AudioSequence','Demo','Configured','VoiceEnabled','Volume']
assert not any(x in source for x in ['speaker_', 'FiAudio'])
print('PASS preserved message IDs, text-only build, and sleep episode contract')

# Compile the real button handlers against narrow platform stubs: home shortcuts
# must not become collection/provider actions while reading a saved record.
handlers = source[source.index('static void local_action('):source.index('static void clicks(')]
request_helper = source[source.index('static void request('):source.index('static void retry_flush(')]
program = r'''#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef void *ClickRecognizerRef;
enum { VIEW_MENU, VIEW_READER, VIEW_HELP, VIEW_WAIT, VIEW_DICTATION, VIEW_HISTORY, VIEW_REVIEW };
static int s_view, s_scroll, s_scroll_max=200, requests, asks, cancels, exits;
static bool s_connected, s_bridge_ready, s_ready_pending, s_phone_record;
static char s_status[160], s_kind[16], s_prompt[401];
#define requested s_kind
static unsigned s_request_id=10;
static bool s_request_pending;
static bool busy(void) { return s_view==VIEW_WAIT || s_view==VIEW_DICTATION || s_view==VIEW_REVIEW; }
static void redraw(void) {}
static void flush(void *unused) {}
static void next_request(void) { s_request_id++; }
static void start_timeout(void) { requests++; }
static void signal_utf8_copy(char *out,size_t size,const char *text) { snprintf(out,size,"%s",text); }
static void ask(void) { asks++; }
static void cancel_turn(const char *message) { cancels++;s_view=VIEW_READER; }
static void window_stack_pop(bool animated) { exits++; }
''' + request_helper + handlers + r'''
int main(void) {
  s_connected=s_bridge_ready=true;s_view=VIEW_MENU;
  up_click(NULL,NULL);assert(requests==1 && !strcmp(requested,"capture"));
  back_click(NULL,NULL);assert(cancels==1 && s_view==VIEW_MENU);
  down_click(NULL,NULL);assert(requests==2 && !strcmp(requested,"history"));
  s_view=VIEW_HISTORY;s_scroll=0;down_click(NULL,NULL);assert(s_scroll==36 && requests==2);
  up_click(NULL,NULL);assert(s_scroll==0 && requests==2);
  back_click(NULL,NULL);assert(s_view==VIEW_MENU);
  select_click(NULL,NULL);assert(asks==1);
  select_long(NULL,NULL);assert(s_view==VIEW_HELP);
  back_click(NULL,NULL);back_click(NULL,NULL);assert(exits==1);
  s_bridge_ready=false;up_click(NULL,NULL);assert(requests==2 && s_ready_pending);
  s_connected=s_bridge_ready=true;s_view=VIEW_REVIEW;s_scroll=0;
  select_long(NULL,NULL);assert(asks==1 && requests==2 && s_view==VIEW_REVIEW);
  down_click(NULL,NULL);assert(s_scroll==36 && requests==2);
  s_request_id=77;strcpy(s_prompt,"Reviewed original");select_click(NULL,NULL);
  assert(requests==3 && !s_phone_record && s_request_id==77 && !s_prompt[0] && !strcmp(requested,"confirm-wake"));
  s_view=VIEW_REVIEW;back_click(NULL,NULL);assert(cancels==2 && s_view==VIEW_MENU);
  s_view=VIEW_REVIEW;s_connected=false;select_click(NULL,NULL);assert(requests==3 && cancels==3);
  puts("PASS actual home shortcuts, provider-free local actions, contextual scrolling and Back");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp) / 'buttons.c'; c.write_text(program)
    binary = Path(tmp) / 'buttons'
    subprocess.run(['cc', '-std=c99', '-Wall', '-Werror', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

# Exercise the real batch walker with sparse source selections. Instrument calls
# so an empty batch must advance without nesting another watch stack frame.
walker = source[source.index('static void next_snapshot(void) {'):source.index('static void sample_done(')]
program = r'''#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "signal_math.h"
static void next_snapshot(void);
static int depth, peak, sends;
void __attribute__((no_instrument_function)) __cyg_profile_func_enter(void *fn, void *caller) {
  if (fn == (void *)next_snapshot) { depth++; if(depth>peak)peak=depth; }
}
void __attribute__((no_instrument_function)) __cyg_profile_func_exit(void *fn, void *caller) {
  if (fn == (void *)next_snapshot) depth--;
}
static bool s_collecting, s_snapshot_pending, s_snapshot_complete, s_sampling;
static int s_stage;
static char s_snapshot[1900];
static const char *selection;
static time_t s_collected_at;
static SignalMotion s_motion;
static struct { int magnetic_heading, compass_status; } s_compass;
static unsigned long long s_compass_received_ms;
enum { CompassStatusCalibrated, CompassStatusCalibrating };
#define TRIG_MAX_ANGLE 65536
static bool enabled(const char *key) { return !strcmp(selection,key); }
typedef struct { unsigned charge_percent; bool is_charging; } BatteryChargeState;
static BatteryChargeState battery_state_service_peek(void) { return (BatteryChargeState){50,false}; }
static time_t day_start(int ago) { return s_collected_at-ago*86400; }
static void append_observation(const char *key,const char *value,const char *unit,const char *status,const char *period,time_t start,time_t end,bool date) {
  if(enabled(key))strcat(s_snapshot,"{}");
}
static void append_observation_ms(const char *key,const char *value,const char *unit,const char *status,const char *period,int64_t start,int64_t end,bool measured,const char *fields,bool date) {
  if(enabled(key))strcat(s_snapshot,"{}");
}
static void minute_history(void) { if(enabled("watch.minute_history"))strcat(s_snapshot,"{}"); }
static void motion_observation(void) { if(enabled("watch.motion"))strcat(s_snapshot,"{}"); (void)s_motion; }
static void cancel_turn(const char *message) { assert(false); }
static void flush(void *unused) { sends++; }
''' + walker + r'''
static void run(const char *key, int expected) {
  selection=key;depth=peak=sends=s_stage=0;s_collecting=true;s_snapshot_pending=false;s_sampling=false;
  next_snapshot();
  while(s_collecting) { assert(s_snapshot_pending);s_snapshot_pending=false;next_snapshot(); }
  assert(sends==expected && s_stage==21 && s_snapshot_complete && peak==1);
}
int main(void) {
  run("",1);run("watch.battery",2);run("watch.motion",1);run("health.steps",9);run("watch.minute_history",2);
  selection="watch.motion";s_stage=20;s_collecting=true;s_snapshot_pending=false;s_sampling=true;
  next_snapshot();assert(s_stage==20 && !s_snapshot_pending);
  s_sampling=false;next_snapshot();assert(s_stage==21 && s_snapshot_pending && !s_collecting);
  puts("PASS sparse capture uses one stack frame, preserves batches and waits for sampling");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/'capture.c'; c.write_text(program)
    binary=Path(tmp)/'capture'
    subprocess.run(['cc','-std=c99','-Wall','-Werror','-finstrument-functions','-I',str(root/'src/c'),str(c),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
