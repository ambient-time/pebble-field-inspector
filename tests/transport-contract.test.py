"""Sanitize production transport functions with hostile synchronous/asynchronous SDK stubs."""
from pathlib import Path
import re, subprocess, tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'src/c/main.c').read_text()
def function(name):
    m=re.search(r'^static [^\n]+\b'+name+r'\([^\n]*\) \{',source,re.M)
    assert m,name
    end=source.find('\nstatic ',m.start()+1)
    return source[m.start():end]

prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "signal_home.h"
#include "signal_transport.h"
enum { VIEW_READER,APP_MSG_OK };
typedef int AppMessageResult;
typedef union { uint32_t uint32; } Value;
typedef struct { Value *value; } Tuple;
typedef struct { int unused; } DictionaryIterator;
static bool s_outbox_busy,s_request_pending,s_ready_pending,s_clear_pending,s_settings_pending,s_snapshot_pending,s_collecting,s_snapshot_complete;
static uint32_t s_request_id,s_cancel_id,s_home_cancel_id,s_ack_id,s_handoff_id,s_outbox_id;
static uint16_t s_home_requested_page;
static int s_outbox_kind,s_view,begins,sends,begin_failure,send_failure,retries;
static SignalTransport s_transport;
static SignalHomeIntent s_home_cancel,s_home_wire;
static char s_kind[24],s_prompt[401],s_snapshot[1900],s_phone_hint[100],s_status[160];
static void *s_outbox_timer;
static void flush(void *unused);
static void *app_timer_register(unsigned ms,void(*f)(void*),void *ctx){(void)ms;(void)f;(void)ctx;retries++;return(void*)1;}
static int app_message_outbox_begin(DictionaryIterator **iter){static DictionaryIterator d;*iter=&d;begins++;return begin_failure?99:APP_MSG_OK;}
static int app_message_outbox_send(void){sends++;return send_failure?99:APP_MSG_OK;}
static Tuple *dict_find(DictionaryIterator *iter,unsigned key){(void)iter;(void)key;return NULL;}
static void dict_write_cstring(DictionaryIterator *iter,unsigned key,const char *value){(void)iter;(void)key;(void)value;}
static void dict_write_uint32(DictionaryIterator *iter,unsigned key,uint32_t value){(void)iter;(void)key;(void)value;}
#define dict_write_uint8 dict_write_uint32
#define dict_write_uint16 dict_write_uint32
static void home_binding(DictionaryIterator *iter,const SignalHomeIntent *i){(void)iter;(void)i;}
static void question_binding(DictionaryIterator *iter){(void)iter;}
static void stop_sampling(void){}
static void clear_timeout(void){}
static void redraw(void){}
static void next_snapshot(void){}
'''
import json
keys=json.loads((root/'package.json').read_text())['pebble']['messageKeys']
prefix+='\n'.join(f'#define MESSAGE_KEY_{k} {i}' for i,k in enumerate(keys))+'\n'
driver=r'''
static void reset(void){
 memset(&s_transport,0,sizeof s_transport);s_outbox_busy=false;s_outbox_timer=NULL;
 s_request_id=20;s_cancel_id=s_home_cancel_id=s_ack_id=s_handoff_id=0;
 s_ready_pending=s_clear_pending=s_settings_pending=s_snapshot_pending=false;s_request_pending=true;
 strcpy(s_kind,"question-send");begins=sends=begin_failure=send_failure=retries=0;
}
int main(void){
 reset();begin_failure=1;for(int i=0;i<1000;i++)flush(NULL);
 assert(begins==4 && sends==0 && retries==3 && s_transport.paused);
 reset();send_failure=1;for(int i=0;i<1000;i++)flush(NULL);
 assert(begins==4 && sends==4 && retries==3 && s_transport.paused);
 reset();for(int i=0;i<4;i++){flush(NULL);assert(s_outbox_busy);outbox_failed(NULL,99,NULL);}
 assert(begins==4 && sends==4 && retries==3 && s_transport.paused);
 flush(NULL);assert(sends==4);signal_transport_resume(&s_transport);s_ready_pending=true;flush(NULL);assert(sends==5);
 outbox_sent(NULL,NULL);assert(!s_ready_pending && !s_outbox_busy);
 reset();s_cancel_id=7;send_failure=1;for(int i=0;i<1000;i++)flush(NULL);
 assert(begins==4 && s_cancel_id==7 && s_transport.paused);
 signal_transport_resume(&s_transport);send_failure=0;flush(NULL);assert(s_outbox_id==7);
 reset();flush(NULL);s_request_id=21;s_request_pending=false;outbox_failed(NULL,99,NULL);
 assert(!s_transport.paused && s_transport.failures==0); // stale callback cannot pause newer work
 puts("PASS actual transport: 1000 immediate failures stop at four; async parity, explicit recovery, cancellation retention, stale envelopes");
}
'''
with tempfile.TemporaryDirectory(prefix='signal-transport-') as td:
    c=Path(td)/'transport.c';c.write_text(prefix+'\n'.join(function(n) for n in ['retry_flush','transport_failed','flush','outbox_sent','outbox_failed'])+driver)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-I',str(root/'src/c'),str(c),'-o',td+'/test'],check=True)
    subprocess.run([td+'/test'],check=True)
