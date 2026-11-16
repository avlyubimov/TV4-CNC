#include "model.h"
#include "strings.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
 model_t m; model_init(&m);
 assert(m.mode==SIMULATION && !m.connected && !m.rpm && !m.feed);
 model_event(&m,EV_STEP); assert(m.x_um==12600);
 model_event(&m,EV_EMERGENCY); model_event(&m,EV_STEP); assert(m.mode==FAULT && m.x_um==12600);
 model_event(&m,EV_CONNECT); assert(m.mode==FAULT);
 model_event(&m,EV_ACK); assert(m.mode==HOLD && !m.emergency);
 model_event(&m,EV_STEP); assert(m.x_um==12600);
 uint8_t b[]={1,255,0,0}; assert(model_receive(&m,b,4,100));
 b[1]=0; assert(model_receive(&m,b,4,200)); /* sequence wrap */
 model_tick(&m,699); assert(m.connected);
 model_tick(&m,700); assert(!m.connected && m.mode==DISCONNECTED);
 model_tick(&m,0xfffffff0); model_event(&m,EV_CONNECT); b[1]=3; assert(model_receive(&m,b,4,0xfffffff0));
 model_tick(&m,484); assert(!m.connected); /* clock wrap, exactly 500 ms */
 for(unsigned len=0;len<10;len++) {
   model_init(&m); model_event(&m,EV_CONNECT);
   assert(!model_receive(&m,NULL,len,0)); assert(m.mode==FAULT);
 }
 for(unsigned i=0;i<256;i++) {
   model_init(&m); model_event(&m,EV_CONNECT);
   uint8_t p[]={1,1,(uint8_t)i,0};
   assert(model_receive(&m,p,4,0)==(i==0));
 }
 model_init(&m); model_event(&m,EV_CONNECT); b[1]=1;
 assert(model_receive(&m,b,4,0)); assert(!model_receive(&m,b,4,1));
 b[0]=2; assert(!model_receive(&m,b,4,2));
 for(int id=0;id<S_COUNT;id++) {
   assert(strlen(tr("ru-RU",id))>0);
   assert(!strcmp(tr("kk-KZ",id),tr("ru-RU",id)));
   assert(!strcmp(tr("en-US",id),tr(NULL,id)));
 }
 model_init(&m); model_event(&m,EV_CONNECT);
 model_tick(&m,500); assert(m.mode==DISCONNECTED); /* missing FIRST heartbeat */
 model_init(&m); m.x_um=1000000; model_event(&m,EV_STEP); assert(m.x_um==1000000);
 model_init(&m);
 model_event(&m,EV_INC_1); model_event(&m,EV_X_MINUS); assert(m.x_um==12499);
 model_event(&m,EV_INC_10); model_event(&m,EV_Z_PLUS); assert(m.z_um==-42490);
 model_event(&m,EV_INC_1000); model_event(&m,EV_Z_MINUS); assert(m.z_um==-43490);
 model_event(&m,EV_INC_100); model_event(&m,EV_STEP); assert(m.x_um==12599);
 model_event(&m,EV_STOP); model_event(&m,EV_Z_PLUS); assert(m.z_um==-43490);
 journal_t journal; journal_init(&journal); journal_entry_t entry;
 for(unsigned i=0;i<70;i++) journal_push(&journal,i,J_STEP,F_NONE);
 assert(journal.count==32 && journal.total==70 && journal.overwritten==38);
 assert(journal_get(&journal,0,&entry) && entry.number==70 && entry.ms==69);
 assert(journal_get(&journal,31,&entry) && entry.number==39);
 assert(!journal_get(&journal,32,&entry) && !journal_get(NULL,0,&entry));
 model_init(&m); model_event(&m,EV_TOUCH_FAULT);
 for(int i=0;i<100;i++) model_event(&m,EV_TOUCH_FAULT);
 assert(m.mode==FAULT && m.fault==F_TOUCH && m.journal.total==2);
 model_event(&m,EV_NEW_SIM); assert(m.mode==FAULT && m.fault==F_TOUCH);
 model_event(&m,EV_ACK); assert(m.mode==HOLD && m.fault==F_NONE && !m.rpm);
 model_event(&m,EV_STEP); assert(m.x_um==12500);
 model_event(&m,EV_NEW_SIM); assert(m.mode==SIMULATION && m.rpm==1250 && m.x_um==25430);
 model_event(&m,EV_DISPLAY_FAULT); assert(m.fault==F_DISPLAY && !m.rpm);
 model_init(&m); model_event(&m,EV_CONNECT); model_tick(&m,500);
 assert(journal_get(&m.journal,0,&entry) && entry.kind==J_TIMEOUT && entry.ms==500);
 int32_t parsed=77;
 assert(tools_parse_mm("-1,250",&parsed) && parsed==-1250);
 assert(tools_parse_mm("+0.001",&parsed) && parsed==1);
 assert(tools_parse_mm("-2147483.648",&parsed) && parsed==INT32_MIN);
 assert(tools_parse_mm("2147483.647",&parsed) && parsed==INT32_MAX);
 const char *bad_numbers[]={"","+","-",".1","1,","1.2345","1e3","NaN"," 1","1 ","1,,2","2147483.648","-2147483.649","1234567890123456"};
 for(unsigned i=0;i<sizeof(bad_numbers)/sizeof(*bad_numbers);i++) {
   parsed=77;assert(!tools_parse_mm(bad_numbers[i],&parsed) && parsed==77);
 }
 tools_t library;tools_init(&library);tool_geometry_t geometry={100,-200,500};
 assert(tools_save(&library,1,&geometry));uint32_t revision=library.revision;
 geometry.radius_um=-1;assert(!tools_save(&library,1,&geometry));
 assert(library.revision==revision && library.geometry[1].radius_um==500);
 assert(!tools_activate(&library,TOOL_COUNT));
 geometry.radius_um=5001;assert(!tools_geometry_valid(&geometry));
 geometry.radius_um=0;geometry.x_um=1000001;assert(!tools_geometry_valid(&geometry));
 model_init(&m);geometry=(tool_geometry_t){100,-200,500};
 int32_t initial_x=m.x_um,initial_z=m.z_um;
 assert(model_save_tool(&m,1,&geometry) && model_activate_tool(&m,1));
 assert(m.tool==2 && m.x_um==initial_x && m.z_um==initial_z && m.rpm==0);
 model_event(&m,EV_TOUCH_FAULT);revision=m.tools.revision;
 assert(!model_activate_tool(&m,0) && !model_save_tool(&m,0,&geometry));
 assert(m.tools.revision==revision && m.tool==2);
 model_event(&m,EV_ACK);model_event(&m,EV_CONNECT);
 assert(!model_activate_tool(&m,0));
 assert(!transport_send(b,4)); puts("State, timeout, invalid frames, RU fallback and inert transport passed");
}
