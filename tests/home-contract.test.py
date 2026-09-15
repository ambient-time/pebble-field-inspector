"""Compile production Home inbox parser with typed Pebble dictionary stubs."""
from pathlib import Path
import re,subprocess,tempfile,json
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'src/c/main.c').read_text()
def fn(name):
 m=re.search(r'^static [^\n]+\b'+name+r'\([^\n]*\) \{',s,re.M); assert m,name
 end=s.find('\nstatic ',m.start()+1); assert end>0
 return s[m.start():end]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "signal_home.h"
typedef enum { VIEW_MENU, VIEW_READER, VIEW_HELP, VIEW_WAIT, VIEW_DICTATION, VIEW_HISTORY, VIEW_REVIEW, VIEW_HOME_LIST, VIEW_HOME_DETAIL, VIEW_HOME_REVIEW, VIEW_HOME_RESULT, VIEW_HOME_HANDOFF } View;
#define TUPLE_CSTRING 1
#define TUPLE_UINT 2
#define TUPLE_INT 3
typedef union { char *cstring; uint32_t uint32; uint16_t uint16; uint8_t uint8; int8_t int8; int16_t int16; int32_t int32; } Value;
typedef struct { int type; size_t length; Value *value; } Tuple;
typedef struct { Tuple fields[40]; Value values[40]; } DictionaryIterator;
static Tuple *dict_find(DictionaryIterator *d,uint32_t key){d->fields[key].value=&d->values[key];return d->fields[key].length?&d->fields[key]:NULL;}
static void number(DictionaryIterator *d,unsigned key,uint32_t n){d->values[key].uint32=n;d->fields[key]=(Tuple){TUPLE_UINT,4,&d->values[key]};}
static void text(DictionaryIterator *d,unsigned key,char *s){d->values[key].cstring=s;d->fields[key]=(Tuple){TUPLE_CSTRING,strlen(s)+1,&d->values[key]};}
static View s_view;
static uint32_t s_request_id,s_home_reply_id,s_ack_id;
static SignalHomePage s_home_page;
static SignalHomeIntent s_home_wire,s_home_intent;
static int s_home_selected,s_scroll,cancels,flushes;
static uint16_t s_home_requested_page;
static char s_kind[16],s_home_text[SIGNAL_HOME_TEXT_CAP];
static bool s_request_pending;
static void *s_timeout_timer;
static void home_cancel(void){cancels++;s_home_intent.consumed=true;}
static void clear_timeout(void){s_timeout_timer=NULL;}
static void home_expired(void *unused){(void)unused;}
static void *app_timer_register(uint32_t ms,void (*f)(void*),void *ctx){(void)f;(void)ctx;assert(ms<=120000);return (void*)1;}
static void flush(void *ctx){(void)ctx;flushes++;}
static void redraw(void){}
'''
keys=json.loads((ROOT/'package.json').read_text())['pebble']['messageKeys']
prefix+='\n'.join('#define MESSAGE_KEY_%s %d'%(k,i) for i,k in enumerate(keys))+'\n'
body=r'''
static DictionaryIterator message(char *mode){DictionaryIterator d={0};number(&d,MESSAGE_KEY_HomeVersion,1);text(&d,MESSAGE_KEY_HomeMode,mode);return d;}
static void reset(char *kind){s_view=VIEW_WAIT;s_request_id=10;s_home_reply_id=0;s_ack_id=0;cancels=0;strcpy(s_kind,kind);s_request_pending=true;memset(&s_home_intent,0,sizeof s_home_intent);memset(&s_home_wire,0,sizeof s_home_wire);strcpy(s_home_wire.favorite,"favorite");strcpy(s_home_wire.action,"action");}
int main(void){
 reset("home-list");s_home_requested_page=1;DictionaryIterator d=message("list");number(&d,MESSAGE_KEY_HomePage,1);number(&d,MESSAGE_KEY_HomePages,2);text(&d,MESSAGE_KEY_HomeItems,"favorite\tKitchen");
 d.fields[MESSAGE_KEY_HomeVersion].type=TUPLE_INT;d.fields[MESSAGE_KEY_HomePage].type=TUPLE_INT;
 home_receive(&d,9);assert(s_view==VIEW_WAIT && !s_ack_id);
 home_receive(&d,10);assert(s_view==VIEW_HOME_LIST && s_home_page.page==1 && s_ack_id==10);
 s_view=VIEW_MENU;s_ack_id=0;home_receive(&d,10);assert(s_view==VIEW_MENU && s_ack_id==10); // Replay ACK cannot reopen page.
 reset("home-open");d=message("detail");text(&d,MESSAGE_KEY_HomeFavorite,"favorite");text(&d,MESSAGE_KEY_ResponseText,"Kitchen light is off.");text(&d,MESSAGE_KEY_HomeAction,"action");
 home_receive(&d,10);assert(s_view==VIEW_HOME_DETAIL && !strcmp(s_home_intent.action,"action"));
 reset("home-open");text(&d,MESSAGE_KEY_HomeFavorite,"other");home_receive(&d,10);assert(s_view==VIEW_HOME_RESULT && cancels==1 && !s_ack_id);
 reset("home-review");d=message("review");text(&d,MESSAGE_KEY_HomeFavorite,"favorite");text(&d,MESSAGE_KEY_HomeAction,"action");text(&d,MESSAGE_KEY_HomeIntent,"immutable-intent");number(&d,MESSAGE_KEY_HomeExpires,(uint32_t)time(NULL)+120);text(&d,MESSAGE_KEY_ResponseText,"Turn Kitchen light on. Exact target and parameters.");
 home_receive(&d,10);assert(s_view==VIEW_HOME_REVIEW && s_timeout_timer && signal_home_can_confirm(&s_home_intent,(uint32_t)time(NULL)));
 reset("home-review");number(&d,MESSAGE_KEY_HomeExpires,(uint32_t)time(NULL)-1);home_receive(&d,10);assert(s_view==VIEW_HOME_RESULT && cancels==1);
 reset("home-review");number(&d,MESSAGE_KEY_HomeExpires,(uint32_t)time(NULL)+121);home_receive(&d,10);assert(s_view==VIEW_HOME_RESULT && !s_ack_id);
 reset("home-review");number(&d,MESSAGE_KEY_HomeExpires,(uint32_t)time(NULL)+100);text(&d,MESSAGE_KEY_HomeAction,"different");home_receive(&d,10);assert(s_view==VIEW_HOME_RESULT);
 reset("home-review");text(&d,MESSAGE_KEY_HomeAction,"action");char huge[902];memset(huge,'x',901);huge[901]=0;text(&d,MESSAGE_KEY_ResponseText,huge);home_receive(&d,10);assert(s_view==VIEW_HOME_RESULT && cancels==1);assert(strstr(s_home_text,"safely"));
 reset("home-review");d=message("result");text(&d,MESSAGE_KEY_HomeFavorite,"favorite");text(&d,MESSAGE_KEY_ResponseText,"Executed under the standing permission.");home_receive(&d,10);assert(s_view==VIEW_HOME_RESULT && s_ack_id==10); // Granted action bypasses unnecessary review.
 reset("home-list");d=message("handoff");s_home_wire.favorite[0]=0;text(&d,MESSAGE_KEY_HomeFavorite,"");text(&d,MESSAGE_KEY_ResponseText,"Review favorites on phone.");home_receive(&d,10);assert(s_view==VIEW_HOME_HANDOFF);
 puts("PASS production Home inbox: scoped replies, replay ACK, exact review, expiry, malformed/oversized rejection, granted result and handoff");
}
'''
with tempfile.TemporaryDirectory(prefix='signal-home-c-') as td:
 p=Path(td)/'test.c';p.write_text(prefix+'\n'.join(fn(n) for n in ['home_string','home_uint','home_receive'])+body)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(ROOT/'src/c'),str(p),'-o',td+'/test'],check=True)
 subprocess.run([td+'/test'],check=True)
