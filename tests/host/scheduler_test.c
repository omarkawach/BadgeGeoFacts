#include <assert.h>
#include <stdio.h>
#include "scheduler.h"
#define SECOND 1000000LL
int main(void){
 const int64_t epoch=1800000000LL;GeoSchedule s;
 schedule_init(&s,SECOND,epoch+900,true,epoch,3600);assert(schedule_remaining(&s,SECOND)==2700);int64_t original=s.due_us;
 for(int i=0;i<100;i++){assert(!schedule_should_fetch(&s,(i+1)*SECOND,false,false));assert(!schedule_should_fetch(&s,(i+1)*SECOND,true,false));assert(s.due_us==original);}
 puts("PASS 100 Wi-Fi reconnects neither request a fact nor move its deadline.");
 assert(!schedule_should_fetch(&s,original-1,true,false));assert(schedule_should_fetch(&s,original,true,false));assert(!schedule_should_fetch(&s,original,false,false));
 puts("PASS automatic fetch occurs only when due and connected.");
 assert(schedule_should_fetch(&s,2*SECOND,true,true));schedule_failed(&s,original);assert(!schedule_should_fetch(&s,original+SECOND,true,false));assert(schedule_should_fetch(&s,original+SECOND,true,true));assert(schedule_should_fetch(&s,original+300*SECOND,true,false));
 puts("PASS failure backoff and manual override remain separate.");
 schedule_succeeded(&s,20*SECOND,epoch+1000);assert(s.due_us==3620*SECOND);assert(s.last_success_epoch==epoch+1000);
 schedule_init(&s,3*SECOND,epoch+1300,true,s.last_success_epoch,3600);assert(schedule_remaining(&s,3*SECOND)==3300);
 puts("PASS successful fetch starts one hour; reboot restores the remaining 55 minutes.");
 schedule_init(&s,0,0,true,epoch,3600);assert(schedule_remaining(&s,0)==3600);assert(schedule_sync_clock(&s,10*SECOND,epoch+1200));assert(schedule_remaining(&s,10*SECOND)==2400);original=s.due_us;assert(!schedule_sync_clock(&s,20*SECOND,epoch+5000));assert(s.due_us==original);
 puts("PASS delayed clock sync restores once without repeated deadline resets.");
 assert(schedule_legacy_last_success(epoch+14400,14400)==epoch);schedule_init(&s,0,epoch+1800,true,schedule_legacy_last_success(epoch+14400,14400),3600);assert(schedule_remaining(&s,0)==1800);
 puts("PASS legacy four-hour deadline migrates to one hour from the same last fetch.");
 schedule_init(&s,0,epoch,true,0,3600);assert(schedule_remaining(&s,0)==3600);schedule_init(&s,0,epoch,true,epoch+100,3600);assert(schedule_remaining(&s,0)==3600);
 schedule_init(&s,0,epoch,false,epoch-100,3600);assert(schedule_should_fetch(&s,0,true,false));
 puts("PASS missing timestamps, backward clocks and first-run initialization.");
 return 0;
}
