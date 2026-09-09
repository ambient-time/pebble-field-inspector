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
assert not any(x in source for x in ['speaker_', 'FiAudio', 'light_enable(true)'])
print('PASS preserved message IDs, text-only build, and sleep episode contract')
