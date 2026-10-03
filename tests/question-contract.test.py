"""Exercise the production question parser and pre-paint review measurement."""
from pathlib import Path
import json,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/c/main.c').read_text()
def function(name):
    m=re.search(r'^static [^\n]+\b'+name+r'\([^\n]*\) \{',source,re.M);assert m,name
    return source[m.start():source.find('\nstatic ',m.start()+1)]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "signal_home.h"
#define TUPLE_CSTRING 1
#define TUPLE_UINT 2
#define TUPLE_INT 3
typedef union { char *cstring; uint32_t uint32; uint16_t uint16; uint8_t uint8; int8_t int8; int16_t int16; int32_t int32; } Value;
typedef struct { int type; size_t length; Value *value; } Tuple;
typedef struct { Tuple fields[60]; Value values[60]; } DictionaryIterator;
static Tuple *dict_find(DictionaryIterator *d,uint32_t k){d->fields[k].value=&d->values[k];return d->fields[k].length?&d->fields[k]:NULL;}
static void number(DictionaryIterator *d,unsigned k,uint32_t n){d->values[k].uint32=n;d->fields[k]=(Tuple){TUPLE_UINT,4,&d->values[k]};}
static void text(DictionaryIterator *d,unsigned k,char *s){d->values[k].cstring=s;d->fields[k]=(Tuple){TUPLE_CSTRING,strlen(s)+1,&d->values[k]};}
static int s_view,s_scroll,s_scroll_max,s_question_row,flushes,redraws,refreshes;
static uint32_t s_request_id,s_question_reply_id,s_question_revision,s_question_expires,s_ack_id;
static bool s_request_pending,s_question_read,s_home_review_read,s_question_has_prompt;
static SignalHomePage s_question_systems;
static char s_kind[24],s_question_draft[65],s_question_review[65],s_question_mode[8],s_question_text[901],s_status[160],s_answer[1024];
static void *s_timeout_timer;
static void clear_timeout(void){s_timeout_timer=NULL;}
static void *app_timer_register(uint32_t ms,void(*f)(void*),void *ctx){(void)f;(void)ctx;assert(ms>0 && ms<=120000);return(void*)1;}
static void flush(void *ctx){(void)ctx;flushes++;}
static void redraw(void){redraws++;}
static void question_request(const char *kind){(void)kind;refreshes++;}
typedef struct { int w,h; } GSize;
typedef struct { GSize size; } GRect;
#define GRect(x,y,w,h) ((GRect){{w,h}})
static void *s_body=(void*)1,*s_canvas=(void*)2;
static int measured_height=20;
static GRect layer_get_bounds(void *layer){(void)layer;return GRect(0,0,120,80);}
static int markdown_body(void *ctx,int width,int offset){(void)ctx;(void)width;(void)offset;return measured_height;}
static const char *body_text(void){return s_question_text;}
static int font(void){return 0;}
enum { GTextOverflowModeWordWrap, GTextAlignmentLeft };
static GSize graphics_text_layout_get_content_size(const char *text,int face,GRect r,int mode,int align){(void)text;(void)face;(void)r;(void)mode;(void)align;return(GSize){120,measured_height};}
'''
prefix+=re.search(r'typedef enum \{[^\n]+\} View;',source).group(0)+'\n'
keys=json.loads((root/'package.json').read_text())['pebble']['messageKeys']
prefix+='\n'.join(f'#define MESSAGE_KEY_{k} {i}' for i,k in enumerate(keys))+'\n'
driver=r'''
static DictionaryIterator packet(char *mode){
 DictionaryIterator d={0};number(&d,MESSAGE_KEY_QuestionReviewVersion,1);text(&d,MESSAGE_KEY_QuestionMode,mode);
 text(&d,MESSAGE_KEY_QuestionDraft,"draft");number(&d,MESSAGE_KEY_QuestionRevision,1);text(&d,MESSAGE_KEY_QuestionHomeMode,"none");return d;
}
static void reset(char *kind){s_view=VIEW_WAIT;s_request_id=42;s_question_reply_id=s_ack_id=0;strcpy(s_kind,kind);strcpy(s_question_draft,"draft");s_question_revision=1;s_question_read=false;s_request_pending=true;}
int main(void){
 reset("question-review");DictionaryIterator d=packet("review");text(&d,MESSAGE_KEY_ResponseText,"Full provider/question/exact evidence/Home review.");text(&d,MESSAGE_KEY_QuestionReview,"nonce");number(&d,MESSAGE_KEY_QuestionExpires,(uint32_t)time(NULL)+120);
 question_receive(&d,41);assert(s_view==VIEW_WAIT && !s_ack_id);
 question_receive(&d,42);assert(s_view==VIEW_QUESTION_REVIEW && !s_question_read && s_timeout_timer && s_ack_id==42);
 measured_height=20;measure_body();assert(s_question_read && s_scroll_max==0); // Before either layer paints.
 s_view=VIEW_MENU;s_ack_id=0;question_receive(&d,42);assert(s_view==VIEW_MENU && s_ack_id==42);
 reset("question-review");question_receive(&d,42);measured_height=200;measure_body();assert(!s_question_read && s_scroll_max==120);
 reset("question-review");text(&d,MESSAGE_KEY_QuestionDraft,"other");question_receive(&d,42);assert(s_view==VIEW_QUESTION_PHONE && !s_question_review[0]);
 reset("question-review");text(&d,MESSAGE_KEY_QuestionDraft,"draft");number(&d,MESSAGE_KEY_QuestionExpires,(uint32_t)time(NULL)+121);question_receive(&d,42);assert(s_view==VIEW_QUESTION_PHONE);
 reset("question-review");number(&d,MESSAGE_KEY_QuestionExpires,(uint32_t)time(NULL)+120);char longtext[902];memset(longtext,'a',901);longtext[901]=0;text(&d,MESSAGE_KEY_ResponseText,longtext);question_receive(&d,42);assert(s_view==VIEW_QUESTION_PHONE);
 reset("question-systems");d=packet("systems");number(&d,MESSAGE_KEY_QuestionPage,0);number(&d,MESSAGE_KEY_QuestionPages,1);text(&d,MESSAGE_KEY_QuestionItems,"ha\t[x] Home Assistant\nother\t[ ] Other Home");question_receive(&d,42);assert(s_view==VIEW_QUESTION_SYSTEMS && s_question_systems.count==2);
 reset("question-systems");text(&d,MESSAGE_KEY_QuestionItems,"ha\t[x] Home\nha\t[ ] Duplicate");question_receive(&d,42);assert(s_view==VIEW_QUESTION_PHONE);
 s_view=VIEW_HOME_REVIEW;s_home_review_read=false;measured_height=20;measure_body();assert(s_home_review_read);
 s_view=VIEW_QUESTION_REVIEW;s_question_has_prompt=true;strcpy(s_question_review,"nonce");question_expired(NULL);assert(s_view==VIEW_QUESTION_DRAFT && !s_question_draft[0] && !s_question_review[0] && !s_question_read && !strcmp(s_question_mode,"none"));
 s_question_has_prompt=false;question_expired(NULL);assert(s_view==VIEW_READER && strstr(s_status,"expired"));
 puts("PASS production question inbox: exact bindings, replay ACK, expiry, UTF-8 bounds, systems; short/long review pre-paint measurement");
}
'''
with tempfile.TemporaryDirectory(prefix='signal-question-') as td:
    c=Path(td)/'question.c';c.write_text(prefix+'\n'.join(function(n) for n in ['home_string','home_uint','question_expired','question_receive','measure_body'])+driver)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-I',str(root/'src/c'),str(c),'-o',td+'/test'],check=True)
    subprocess.run([td+'/test'],check=True)
# Paint callbacks must never change consent or scroll bounds.
for name in ['draw_body','draw']:
    assert not re.search(r's_(?:home_review_read|question_read|scroll_max)\s*=',function(name)),name
