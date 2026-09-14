#include <assert.h>
#include <stdio.h>
#include "signal_markdown.h"
int main(void) {
  char out[1024]; SignalMarkdownBlock block;
  SignalMarkdown reader={.next="# Summary\n- **Strong** and *calm*\n  - nested\n> quoted\n```c\na_b * 2;\n```\n[Download](https://example.com/a(b))\n---\n界😀"};
  assert(signal_markdown_next(&reader,out,sizeof out,&block) && block.heading && !strcmp(out,"Summary"));
  assert(signal_markdown_next(&reader,out,sizeof out,&block) && block.indent==4 && !strcmp(out,"- Strong and calm"));
  assert(signal_markdown_next(&reader,out,sizeof out,&block) && block.indent==12);
  assert(signal_markdown_next(&reader,out,sizeof out,&block) && block.quote && !strcmp(out,"quoted"));
  assert(signal_markdown_next(&reader,out,sizeof out,&block) && !out[0]);
  assert(signal_markdown_next(&reader,out,sizeof out,&block) && block.code && !strcmp(out,"a_b * 2;"));
  assert(signal_markdown_next(&reader,out,sizeof out,&block) && !out[0]);
  assert(signal_markdown_next(&reader,out,sizeof out,&block) && !strcmp(out,"Download"));
  assert(signal_markdown_next(&reader,out,sizeof out,&block) && block.rule);
  assert(signal_markdown_next(&reader,out,sizeof out,&block) && !strcmp(out,"界😀"));
  assert(!signal_markdown_next(&reader,out,sizeof out,&block));
  strcpy(out,"snake_case and `a_b * c` and \\*literal\\* and [unfinished](url");
  signal_markdown_inline(out);
  assert(!strcmp(out,"snake_case and a_b * c and *literal* and [unfinished](url"));
  reader=(SignalMarkdown){.next="界😀\nlast"};
  assert(signal_markdown_next(&reader,out,5,&block) && !strcmp(out,"界"));
  assert(signal_markdown_next(&reader,out,sizeof out,&block) && !strcmp(out,"last"));
  // Every prefix can arrive as a watch excerpt; malformed Markdown stays safe.
  const char *fixture="### **bold** [label](url) `code`\n```\nvalue_x\n```";
  for(size_t i=0;i<=strlen(fixture);i++) {
    char input[100]; memcpy(input,fixture,i); input[i]=0;
    reader=(SignalMarkdown){.next=input};
    while(signal_markdown_next(&reader,out,sizeof out,&block)) {}
  }
  puts("PASS watch Markdown blocks, literal code, links, UTF-8 and truncated input");
}
