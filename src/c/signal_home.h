// Signal Station — Luke Steuber. Phone-owned Home protocol, version 1.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define SIGNAL_HOME_VERSION 1
#define SIGNAL_HOME_ID_CAP 65
#define SIGNAL_HOME_LABEL_CAP 101
#define SIGNAL_HOME_PAGE_SIZE 4
#define SIGNAL_HOME_ITEMS_CAP 701
#define SIGNAL_HOME_TEXT_CAP 901
#define SIGNAL_HOME_TTL 120

typedef struct { char id[SIGNAL_HOME_ID_CAP], label[SIGNAL_HOME_LABEL_CAP]; } SignalHomeItem;
typedef struct { SignalHomeItem items[SIGNAL_HOME_PAGE_SIZE]; uint16_t page,pages; uint8_t count; } SignalHomePage;
typedef struct { char favorite[SIGNAL_HOME_ID_CAP],action[SIGNAL_HOME_ID_CAP],intent[SIGNAL_HOME_ID_CAP]; uint32_t expires; bool consumed; } SignalHomeIntent;

// Validate complete UTF-8. Never turn truncated target/review text into an action.
static inline bool signal_home_utf8(const char *s,size_t n) {
  for (size_t i=0;i<n;) {
    unsigned char c=(unsigned char)s[i++];
    if (c<0x80) { if (!c) return false; continue; }
    unsigned need=c>=0xc2 && c<=0xdf?1:c>=0xe0 && c<=0xef?2:c>=0xf0 && c<=0xf4?3:0;
    if (!need || i+need>n) return false;
    unsigned char d=(unsigned char)s[i];
    if ((c==0xe0 && d<0xa0)||(c==0xed && d>=0xa0)||(c==0xf0 && d<0x90)||(c==0xf4 && d>=0x90)) return false;
    while (need--) if (((unsigned char)s[i++] & 0xc0)!=0x80) return false;
  }
  return true;
}
static inline bool signal_home_string(const char *s,size_t wire_length,size_t cap) {
  return s && wire_length>0 && wire_length<=cap && s[wire_length-1]==0 &&
    !memchr(s,0,wire_length-1) && signal_home_utf8(s,wire_length-1);
}
static inline bool signal_home_id(const char *s) {
  size_t n=strlen(s); if (!n || n>=SIGNAL_HOME_ID_CAP || !signal_home_utf8(s,n)) return false;
  for (size_t i=0;i<n;i++) if ((unsigned char)s[i]<0x20 || s[i]==0x7f) return false;
  return true;
}
static inline bool signal_home_page(SignalHomePage *out,const char *items,size_t length,uint16_t page,uint16_t pages) {
  if (!pages || pages>1000 || page>=pages || !signal_home_string(items,length,SIGNAL_HOME_ITEMS_CAP)) return false;
  SignalHomePage next={.page=page,.pages=pages}; const char *p=items;
  while (*p) {
    if (next.count==SIGNAL_HOME_PAGE_SIZE) return false;
    const char *end=strchr(p,'\n'); if (!end) end=p+strlen(p);
    const char *tab=memchr(p,'\t',(size_t)(end-p));
    if (!tab || tab==p || end==tab+1 || tab-p>=SIGNAL_HOME_ID_CAP || end-tab-1>=SIGNAL_HOME_LABEL_CAP || memchr(tab+1,'\t',(size_t)(end-tab-1))) return false;
    SignalHomeItem *item=&next.items[next.count];
    memcpy(item->id,p,(size_t)(tab-p)); memcpy(item->label,tab+1,(size_t)(end-tab-1));
    if (!signal_home_id(item->id)) return false;
    for (const char *q=item->label;*q;q++) if ((unsigned char)*q<0x20 || *q==0x7f) return false;
    for (unsigned i=0;i<next.count;i++) if (!strcmp(item->id,next.items[i].id)) return false;
    next.count++; p=*end?end+1:end;
    if (*end && !*p) return false; // no empty/trailing rows
  }
  if (!next.count && (page || pages!=1)) return false;
  *out=next; return true;
}
static inline bool signal_home_intent(SignalHomeIntent *out,const char *favorite,const char *action,const char *intent,uint32_t expires,uint32_t now) {
  if (!signal_home_id(favorite)||!signal_home_id(action)||!signal_home_id(intent)||expires<=now||expires-now>SIGNAL_HOME_TTL) return false;
  memset(out,0,sizeof *out); strcpy(out->favorite,favorite); strcpy(out->action,action); strcpy(out->intent,intent); out->expires=expires; return true;
}
static inline bool signal_home_can_confirm(const SignalHomeIntent *intent,uint32_t now) {
  return !intent->consumed && intent->intent[0] && intent->expires>now && intent->expires-now<=SIGNAL_HOME_TTL;
}
