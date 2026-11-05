#include "scheduler.h"
#define US_PER_SECOND 1000000LL
void schedule_init(GeoSchedule *s,int64_t now_us,int64_t wall_epoch,bool has_fact,int64_t last_success_epoch,uint32_t interval_seconds){
 *s=(GeoSchedule){.interval_seconds=interval_seconds,.due_us=now_us+(has_fact?(int64_t)interval_seconds*US_PER_SECOND:0),.last_success_epoch=last_success_epoch,.has_fact=has_fact,.clock_restored=!has_fact};
 schedule_sync_clock(s,now_us,wall_epoch);
}
bool schedule_sync_clock(GeoSchedule *s,int64_t now_us,int64_t wall_epoch){
 if(s->clock_restored||wall_epoch<GEO_VALID_EPOCH)return false;
 if(s->has_fact&&s->last_success_epoch>=GEO_VALID_EPOCH){
  int64_t elapsed=wall_epoch-s->last_success_epoch;if(elapsed<0)elapsed=0;
  int64_t remaining=elapsed>=s->interval_seconds?0:(int64_t)s->interval_seconds-elapsed;
  s->due_us=now_us+remaining*US_PER_SECOND;
 }
 s->clock_restored=true;return true;
}
bool schedule_should_fetch(const GeoSchedule *s,int64_t now_us,bool connected,bool manual_request){
 return connected&&(manual_request||(now_us>=s->due_us&&now_us>=s->retry_after_us));
}
void schedule_succeeded(GeoSchedule *s,int64_t now_us,int64_t wall_epoch){
 s->has_fact=true;s->due_us=now_us+(int64_t)s->interval_seconds*US_PER_SECOND;s->retry_after_us=0;
 s->last_success_epoch=wall_epoch>=GEO_VALID_EPOCH?wall_epoch:0;s->clock_restored=wall_epoch>=GEO_VALID_EPOCH;
}
void schedule_failed(GeoSchedule *s,int64_t now_us){s->retry_after_us=now_us+300LL*US_PER_SECOND;}
uint32_t schedule_remaining(const GeoSchedule *s,int64_t now_us){return s->due_us>now_us?(uint32_t)((s->due_us-now_us+US_PER_SECOND-1)/US_PER_SECOND):0;}
int64_t schedule_legacy_last_success(int64_t saved_next_epoch,uint32_t saved_interval){
 if(!saved_interval||saved_interval>86400)saved_interval=14400;
 return saved_next_epoch>=GEO_VALID_EPOCH+(int64_t)saved_interval?saved_next_epoch-saved_interval:0;
}
