#pragma once
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include "signal_math.h"

// A bounded reading projection, not an HTML renderer. Original replies stay on
// the phone. Code is literal; links keep their labels, with destinations there.
typedef struct { const char *next; char fence; size_t fence_size; } SignalMarkdown;
typedef struct { bool heading, code, quote, rule; unsigned indent; } SignalMarkdownBlock;

static inline void signal_markdown_inline(char *text) {
  char *read=text, *write=text;
  while (*read) {
    if (*read=='\\' && read[1] && ispunct((unsigned char)read[1])) { *write++=read[1]; read+=2; continue; }
    if (*read=='`') {
      size_t n=1; while (read[n]=='`') n++;
      char *end=read+n;
      while (*end && !(end[-1]!='`' && strncmp(end,read,n)==0 && end[n]!='`')) end++;
      if (*end) { read+=n; while (read<end) *write++=*read++; read+=n; continue; }
    }
    if (*read=='[' || (*read=='!' && read[1]=='[')) {
      char *label=read+(*read=='!'?2:1), *end=strchr(label,']');
      if (end && end[1]=='(') {
        char *url=end+2; unsigned depth=1;
        while (*url && depth) { if (*url=='(') depth++; else if (*url==')') depth--; if (depth) url++; }
        if (!depth) { while (label<end) *write++=*label++; read=url+1; continue; }
      }
    }
    if (*read=='*' || *read=='_') {
      char mark=*read; size_t n=1; while (read[n]==mark && n<3) n++;
      // Do not damage snake_case or mathematical multiplication.
      bool boundary=read==text || !isalnum((unsigned char)read[-1]);
      char *end=read+n;
      if (boundary && *end && !isspace((unsigned char)*end)) {
        while (*end && !(strncmp(end,read,n)==0 && !isspace((unsigned char)end[-1]))) end++;
        if (*end) { read+=n; while (read<end) *write++=*read++; read+=n; continue; }
      }
    }
    *write++=*read++;
  }
  *write='\0';
}

static inline bool signal_markdown_next(SignalMarkdown *reader, char *out, size_t cap, SignalMarkdownBlock *block) {
  if (!reader->next || !*reader->next || !cap) return false;
  const char *start=reader->next, *end=strchr(start,'\n');
  size_t length=end?(size_t)(end-start):strlen(start);
  signal_utf8_copy(out, length+1<cap?length+1:cap, start);
  reader->next=end?end+1:start+length;
  *block=(SignalMarkdownBlock){0};
  size_t n=strlen(out); if (n && out[n-1]=='\r') out[--n]='\0';
  char *content=out; while (*content==' ') content++;
  size_t spaces=(size_t)(content-out), fence=0;
  if (spaces<=3 && (*content=='`' || *content=='~')) {
    while (content[fence]==*content) fence++;
    const char *tail=content+fence; while (*tail==' ' || *tail=='\t') tail++;
    if (fence>=3 && (!reader->fence || (reader->fence==*content && fence>=reader->fence_size && !*tail))) {
      if (reader->fence) { reader->fence=0; reader->fence_size=0; }
      else { reader->fence=*content; reader->fence_size=fence; }
      out[0]='\0'; return true;
    }
  }
  block->code=reader->fence!=0;
  if (block->code) return true;
  if (spaces<=3 && *content=='#') {
    size_t hashes=0; while (content[hashes]=='#') hashes++;
    if (hashes<=6 && content[hashes]==' ') { block->heading=true; content+=hashes+1; }
  }
  if (*content=='>') { block->quote=true; content++; if (*content==' ') content++; }
  if ((*content=='-' || *content=='*' || *content=='+') && content[1]==' ') {
    block->indent=spaces>0?12:4; *content='-';
  } else if (isdigit((unsigned char)*content)) {
    char *number=content; while (isdigit((unsigned char)*number)) number++;
    if ((*number=='.' || *number==')') && number[1]==' ') block->indent=spaces>0?12:4;
  }
  if (!block->heading && (*content=='-' || *content=='*' || *content=='_')) {
    char marker=*content; size_t count=0; bool rule=true;
    for (char *p=content;*p;p++) { if (*p==marker) count++; else if (*p!=' ') rule=false; }
    if (rule && count>=3) { block->rule=true; out[0]='\0'; return true; }
  }
  if (content!=out) memmove(out,content,strlen(content)+1);
  // Tables become compact cell rows; their alignment scaffolding is not prose.
  if (strchr(out,'|') && !strchr(out,'`') && !strchr(out,'\\')) {
    bool divider=true; unsigned bars=0, dashes=0;
    for (char *p=out;*p;p++) {
      if (*p=='|') bars++;
      else if (*p=='-') dashes++;
      else if (*p!=':' && *p!=' ') divider=false;
    }
    if (divider && bars && dashes>=3) { out[0]='\0'; return true; }
    if (out[0]=='|' && bars>=2) {
      memmove(out,out+1,strlen(out));
      size_t size=strlen(out); while (size && out[size-1]==' ') out[--size]=0;
      if (size && out[size-1]=='|') out[--size]=0;
      for (char *p=out;*p;p++) if (*p=='|') *p=';';
    }
  }
  signal_markdown_inline(out);
  return true;
}
