#include <assert.h>
#include <stdio.h>
#include "signal_home.h"
int main(void) {
 SignalHomePage page={0}; const char *valid="a\tKitchen lights\nb\tPatio 🌿";
 assert(signal_home_page(&page,valid,strlen(valid)+1,0,2)); assert(page.count==2 && !strcmp(page.items[1].id,"b"));
 SignalHomePage before=page;
 const char *bad[]={"a\tOne\na\tTwo","a\tOne\n","a\tOne\tb","a\t","\tOne","a\tOne\nb\tTwo\nc\tThree\nd\tFour\ne\tFive"};
 for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){ assert(!signal_home_page(&page,bad[i],strlen(bad[i])+1,0,1)); assert(!memcmp(&page,&before,sizeof page)); }
 assert(!signal_home_page(&page,valid,strlen(valid),0,1));
 assert(!signal_home_page(&page,valid,strlen(valid)+1,2,2));
 assert(!signal_home_page(&page,valid,strlen(valid)+1,0,0));
 assert(signal_home_page(&page,"",1,0,1)); assert(page.count==0);
 assert(!signal_home_page(&page,"",1,1,2));
 char large[80]; memset(large,'a',79); large[79]=0; assert(!signal_home_id(large));
 assert(!signal_home_string("a\0b",4,65)); assert(!signal_home_string("\xed\xa0\x80",4,65));
 assert(!signal_home_string("\xf0\x9f\x8c",4,65)); assert(signal_home_string("🌿",5,65));
 SignalHomeIntent intent={0}; assert(signal_home_intent(&intent,"favorite","action","intent",220,100));
 assert(signal_home_can_confirm(&intent,219)); assert(!signal_home_can_confirm(&intent,220));
 assert(!signal_home_can_confirm(&intent,99)); // clock backwards cannot extend the bound window
 intent.consumed=true; assert(!signal_home_can_confirm(&intent,120));
 assert(!signal_home_intent(&intent,"favorite","action","intent",221,100));
 assert(!signal_home_intent(&intent,"favorite","action","intent",100,100));
 assert(!signal_home_intent(&intent,"favorite","action","",200,100));
 assert(!signal_home_intent(&intent,"favorite\nother","action","intent",200,100));
 puts("PASS Home page atomic parsing, exact UTF-8/identity bounds and single-use expiring review");
}
